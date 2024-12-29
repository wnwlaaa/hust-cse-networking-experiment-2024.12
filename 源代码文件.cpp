#pragma once
#include <WinSock2.h>
#include <stdio.h>
#include <WS2tcpip.h>
#include <cstdlib>
#include <time.h>
#include <iostream>
#pragma comment(lib,"ws2_32.lib")
#define DATA_SIZE 512 
//���ݿ��С
#define BUFLEN 255
#define PKT_MAX_RXMT 3 
//����ش�����
#define PKT_RCV_TIMEOUT 3*1000 
//��ȴ�ʱ��
#define SEND_ERR 1
#define FIRST_RECV_ERR 2
#define RECV_ERR 3	
#define FILE_ERR 4
#define CONNECT_ERR 5
#define ERR_PACK 6 
#define PACK_CORR 7
// TFTP�Ĳ�����
#define CMD_RRQ (short)1
#define CMD_WRQ (short)2
#define CMD_DATA (short)3
#define CMD_ACK (short)4
#define CMD_ERROR (short)5
#define CMD_LIST (short)6
#define CMD_HEAD (short)7
#define DEBUG
using namespace std;
// TFTP���ݰ��ṹ��
#pragma warning(disable:4996)
struct tftpPacket
{
	unsigned short cmd;	// ǰ�����ֽڱ�ʾ������
	union	// �м����ֶ�
	{
		unsigned short code;//����ģʽ netascii/octet
		unsigned short block;//���
		char filename[2];	//�ļ�����������Ը��Ǻ�����ڴ棬��Ҫ���������ʹ��
	};
	char data[DATA_SIZE];// ������������ֶ�
};

sockaddr_in serverAddr, clientAddr;//�������Ϳͻ��˵�ip��ַ
SOCKET sock;	// �ͻ���socket
unsigned int addr_len;	// ip��ַ����

double transByte, consumeTime;
FILE* logFp;
char logBuf[512];
time_t rawTime;	//����ʱ��
tm* info;

void setnonblocking(int sockfd) {//������ģʽ
	unsigned long on = 1;
	ioctlsocket(sockfd, FIONBIO, &on);
}
/*
��������upload
���룺char *filename   �ϴ����ļ���
���أ�bool
���ܣ�ʵ���ļ��ϴ�����
*/
bool upload(char* filename)
{
	int rxmt;//�ش������ı�������
	double spack = 0, epack = 0;//�ɹ����Ͷ�ʧ����������¼
	clock_t start, end;	// ��¼ʱ��
	transByte = 0;	// �����ֽ���
	int time_wait_ack, r_size, choose;	// �ȴ�ʱ��,���հ���С,�����ʽ
	tftpPacket sendPacket, rcv_packet;	// ���Ͱ�,���հ�
	// д�������ݰ�
	sendPacket.cmd = htons(CMD_WRQ);	// д�������
	// ��Linux��Windows������ʱ��Ҫ�õ�htons��htonl�����������������ֽ�˳��ת��Ϊ�����ֽ�˳��
	cout << "please choose the file format(1.netascii  2.octet)" << endl;//���ִ���ģʽ
	cin >> choose;
	if (choose == 1)
		sprintf(sendPacket.filename, "%s%c%s%c", filename, 0, "netascii", 0);
	else
		sprintf(sendPacket.filename, "%s%c%s%c", filename, 0, "octet", 0);
	// �����������ݰ�
	//�������ѭ��
	for (rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt++)
	{
		if (sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*)&serverAddr, addr_len) == SOCKET_ERROR)
		{
			continue;
			//epack++; //�������ۼ�
		}
		else spack++;

		// �ȴ����ջ�Ӧ,���ȴ�3s,ÿ��20msˢ��һ��
		for (time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20)
		{
			// ���Խ���
			r_size = recvfrom(sock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*)&serverAddr, (int*)&addr_len);
			if (r_size > 0 && r_size < 4)
				printf("Bad packet: r_size=%d\n", r_size);	// ���հ��쳣
			if (r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(0))
			{
				break;	// �ɹ��յ�ACK���ҽ��յ��İ�ΪACK����ȷ�Ϻ��뷢�Ͱ��Ŀ����ͬ
				spack++;//�ɹ������ۼ�
			}
			if (r_size == -1)//δ���յ���ȴ�20ms
				Sleep(20);
		}
		if (time_wait_ack < PKT_RCV_TIMEOUT)
			break;	// ���ͳɹ�break��ѭ���������ش�
		else // δ�յ�ACK���ش�
		{
			cout << "ACK drop.." << endl;
			epack++;//�������ۼӣ������ش�
			time(&rawTime);//��ȡʱ��
			info = localtime(&rawTime);
			sprintf(logBuf, "%s WARNING: upload %s, mode: %s, %s\n", asctime(info), filename, choose == 1 ? ("netascii") : ("octet"), "Can't receive ACK, Retransmission");
			//minus
			fwrite(logBuf, 1, strlen(logBuf), logFp);//д����־
			continue;
		}
	}
	if (rxmt >= PKT_MAX_RXMT)//�ش����κ�δ�յ�ACK
	{
		// �ش�����Ҳδ���յ�ACK��ֹ���δ���
		printf("Could not receive from server.\n");
		time(&rawTime);
		info = localtime(&rawTime);
		sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n", asctime(info), filename, choose == 1 ? ("netascii") : ("octet"), "Could not receive from server.");
		fwrite(logBuf, 1, strlen(logBuf), logFp);//д����־
		return false;//ֱ�ӽ���
	}

	FILE* fp = NULL;	// ���ļ�
	if (choose == 1)
		fp = fopen(filename, "r");//asciiʹ��r���ļ�
	else
		fp = fopen(filename, "rb");//������ʹ��rb���ļ�
	if (fp == NULL) {//���ļ�ʧ��
		printf("File not exists!\n");
		time(&rawTime);
		info = localtime(&rawTime);
		sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n", asctime(info), filename, choose == 1 ? ("netascii") : ("octet"), "File not exists!");
		fwrite(logBuf, 1, strlen(logBuf), logFp);
		return false;//��ֲ����
	}
	int s_size = 0;
	unsigned short block = 1;//����
	sendPacket.cmd = htons(CMD_DATA);	// ���ݰ�������
	// �������ݰ�
	start = clock();//��ʼ��ʱ
	do
	{
		memset(sendPacket.data, 0, sizeof(sendPacket.data));
		sendPacket.block = htons(block);	// д�����
		s_size = fread(sendPacket.data, 1, DATA_SIZE, fp);	// ��������
		transByte += s_size;//�����ֽ���
		// ����ش�3��
		for (rxmt = 0; rxmt < PKT_MAX_RXMT; rxmt++)//�ش������ۼ�
		{
			// �������ݰ�
			if (sendto(sock, (char*)&sendPacket, s_size + 4, 0, (struct sockaddr*)&serverAddr, addr_len) == SOCKET_ERROR)
			{
				//epack++;//����ʧ���򻵰����ۼ�
				continue;
				//	epack++;
			}
			else spack++;
			printf("Send the %d block\n", block);
			// �ȴ�ACK,���ȴ�3s,ÿ��20msˢ��һ��
			for (time_wait_ack = 0; time_wait_ack < PKT_RCV_TIMEOUT; time_wait_ack += 20)
			{
				r_size = recvfrom(sock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*)&serverAddr, (int*)&addr_len);
				if (r_size > 0 && r_size < 4)
					printf("Bad packet: r_size=%d\n", r_size);
				if (r_size >= 4 && rcv_packet.cmd == htons(CMD_ACK) && rcv_packet.block == htons(block))
				{
					//���յ�ACK��ACKȷ�Ͽ����뷢�͵Ŀ�����ͬ
					break;//����ɹ����ͣ�����ѭ��
					//spack++;�����ۼ��ˣ���ǰ��ɹ��������ۼ�
				}
				if (r_size == -1)//��δ���յ�ACK���ȴ�20ms�ٳ���
					Sleep(20);
			}
			if (time_wait_ack < PKT_RCV_TIMEOUT)
				break;	// ���ͳɹ�
			else // δ�յ�ACK���ش�
			{
				cout << "ACK drop.." << endl;
				epack++;//�������ۼ�
				time(&rawTime);
				info = localtime(&rawTime);
				sprintf(logBuf, "%s WARNING: upload %s, mode: %s, %s\n", asctime(info), filename, choose == 1 ? ("netascii") : ("octet"), "Can't receive ACK, Retransmission");
				fwrite(logBuf, 1, strlen(logBuf), logFp);
				continue;
			}
		}
		if (rxmt >= PKT_MAX_RXMT)
		{
			// 3���ش�ʧ��
			printf("Could not receive ACK from server.\n");
			fclose(fp);
			time(&rawTime);
			info = localtime(&rawTime);
			sprintf(logBuf, "%s ERROR: upload %s, mode: %s, %s\n", asctime(info), filename, choose == 1 ? ("netascii") : ("octet"), "Could not receive ACK from server.");
			fwrite(logBuf, 1, strlen(logBuf), logFp);
			return false;
		}
		block++;	// ������һ�����ݿ�
	} while (s_size == DATA_SIZE);	// �����ݿ�δװ��ʱ��Ϊʱ���һ�����ݣ�����ѭ��
	end = clock();
	printf("Send file end.\n");
	fclose(fp);
	consumeTime = ((double)(end - start)) / CLK_TCK;//���㴫��ʱ��
	//cout<<
	printf("upload file size: %.2f kB consuming time: %.3f s\n", transByte / 1024, consumeTime);
	printf("upload speed: %.2f kB/s\n", transByte / (1024 * consumeTime));
	//cout << epack << endl << spack << endl;
	printf("drop percent: %.2f%%\n", epack / (epack + spack) * 100);
	time(&rawTime);
	info = localtime(&rawTime);
	sprintf(logBuf, "%s\n INFO: upload %s, mode: %s, size: %.1f kB, consuming time: %.4f s\n", asctime(info), filename, choose == 1 ? ("netascii") : ("octet"), transByte / 1024, consumeTime);
	fwrite(logBuf, 1, strlen(logBuf), logFp);
	return true;
}

/*
��������download
���룺char* remoteFile char* localFile   remoteFileΪ�������ϵ��ļ��� localFileΪ���غ����õ��ļ���
���أ�bool
���ܣ�ʵ���ļ����ع���
*/
bool download(char* remoteFile, char* localFile)
{
	//char* remoteFile=filename1, char* localFile=filename1;
	FILE* fp = NULL;
	int cnt = 0;
	//int rxmt;//�ش�������������
	double epack = 0, spack = 0;//�ɹ����͵İ��ͻ�����������
	clock_t start, end;	// ��¼ʱ��
	transByte = 0;	// �����ֽ���
	int time_wait_ack, r_size, choose;	// �ȴ�ʱ��,���հ���С��ѡ��ģʽ
	tftpPacket sendPacket, rcv_packet, ack;// ���Ͱ�,���հ�
	int time_wait_data;//�ȴ�ʱ��
	unsigned short block = 1;//����
	// ���������ݰ�
	start = clock();
	sendPacket.cmd = htons(CMD_RRQ);	// ��ȡ������
	cout << "please choose the file format(1.netascii  2.octet)" << endl;//����ģʽ
	cin >> choose;
	if (choose == 1)
		sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "netascii", 0);
	else
		sprintf(sendPacket.filename, "%s%c%s%c", remoteFile, 0, "octet", 0);
	// �����������ݰ�
	if (sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*)&serverAddr, addr_len) == SOCKET_ERROR)
	{
		epack++;
		//continue;
	}
	else spack++;
	// �½�����д���ļ�

	if (choose == 1)
		fp = fopen(localFile, "w");//ASCII��wд���ļ�
	else
		fp = fopen(localFile, "wb");//��������wbд���ļ�
	if (fp == NULL)//���ļ�ʧ��
	{
		printf("Create file \"%s\" error.\n", localFile);
		time(&rawTime);
		info = localtime(&rawTime);
		sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Create file \"%s\" error.\n", asctime(info), remoteFile, localFile, choose == 1 ? ("netascii") : ("octet"), localFile);
		fwrite(logBuf, 1, strlen(logBuf), logFp);
		return false;//ֱ����ֹ����
	}
	// ��������

	sendPacket.cmd = htons(CMD_ACK);
	do {
		for (time_wait_data = 0; time_wait_data < PKT_RCV_TIMEOUT; time_wait_data += 20)
		{
			//���շ��������͵����ݰ�
			r_size = recvfrom(sock, (char*)&rcv_packet, sizeof(tftpPacket), 0, (struct sockaddr*)&serverAddr, (int*)&addr_len);
			if (r_size > 0 && r_size < 4)
				printf("Bad packet: r_size=%d\n", r_size);//����
			if (r_size >= 4 && rcv_packet.cmd == htons(CMD_DATA) && rcv_packet.block == htons(block))
			{//���յ����ݰ������Ұ��Ŀ��һ��
				cnt = 0;
				spack++;//�ۼ�
				printf("DATA: block=%d, data_size=%d\n", ntohs(rcv_packet.block), r_size - 4);
				// ����ack����
				sendPacket.block = rcv_packet.block;	//ack�����������
				sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*)&serverAddr, addr_len);//����ACK
				//д������
				fwrite(rcv_packet.data, 1, r_size - 4, fp);
				break;
			}
			if (r_size == -1)
				Sleep(20);
		}
		if (time_wait_data >= PKT_RCV_TIMEOUT && block == 1)//��ʱ��Ϊ��һ����˵�����ӳ�ʱ����������
		{
			epack++;
			break;
		}
		if (time_wait_data >= PKT_RCV_TIMEOUT && block > 1 && cnt < 3)//��ʱ�Ҳ�Ϊ��һ����
		{
			printf("Datapack drop..\n");
			cnt++;
			epack++;
			sendPacket.block = htons(block - 1);
			sendto(sock, (char*)&sendPacket, sizeof(tftpPacket), 0, (struct sockaddr*)&serverAddr, addr_len);//�����ظ�ACK����������֪δ���յ���
			r_size = DATA_SIZE + 4;
			continue;
		}

		if (time_wait_data >= PKT_RCV_TIMEOUT && cnt == 3)//�ش�����ACK����δ���յ��°���ERROR��ֱ�ӽ���
		{
			printf("Wait for DATA #%d timeout.\n", block);
			fclose(fp);
			time(&rawTime);
			info = localtime(&rawTime);
			sprintf(logBuf, "%s ERROR: download %s as %s, mode: %s, Wait for DATA #%d timeout.\n", asctime(info), remoteFile, localFile, choose == 1 ? ("netascii") : ("octet"), block);
			fwrite(logBuf, 1, strlen(logBuf), logFp);
			return false;
		}
		transByte += (r_size - 4);
		block++;
	} while (r_size == DATA_SIZE + 4);
	end = clock();
	consumeTime = ((double)(end - start)) / CLK_TCK;
	printf("download file size: %.2f kB consuming time: %.3f s\n", transByte / 1024, consumeTime);
	printf("download speed: %.2f kB/s\n", transByte / (1024 * consumeTime));
	printf("drop percent :%.1f%%\n", epack / (epack + spack) * 100);
	fclose(fp);
	time(&rawTime);
	info = localtime(&rawTime);
	sprintf(logBuf, "%s INFO: download %s as %s, mode: %s, size: %.1f kB, consuming time: %.2f s\n", asctime(info), remoteFile, localFile, choose == 1 ? ("netascii") : ("octet"), transByte / 1024, consumeTime);
	fwrite(logBuf, 1, strlen(logBuf), logFp);
	return true;
}
void main() {
	//��ȡʱ��
	time(&rawTime);
	//ת��Ϊ����ʱ��
	info = localtime(&rawTime);
	// ����־�ļ�
	logFp = fopen("tftp.log", "a");
	if (logFp == NULL)
		cout << "open log file failed" << endl;
	printf("��ǰ�ı���ʱ������ڣ�%s", asctime(info));
	char serverIP[20], clientIP[20];
	char buf[BUFLEN];//���ݻ�����
	//�ָ�����
	char token1[BUFLEN];
	char token2[BUFLEN];
	WSADATA wsaData;
	addr_len = sizeof(struct sockaddr_in);
	//��ʼ�� winsock
	int nRC = WSAStartup(0x0101, &wsaData);
	if (nRC)
	{
		printf("�ͻ���winsock����!\n");
		return;
	}
	if (wsaData.wVersion != 0x0101)
	{
		printf("�ͻ���winsock�汾����!\n");
		WSACleanup();
		return;
	}
	printf("�ͻ���winsock��ʼ����� !\n");
	cout << "�����������IP��";
	cin >> serverIP;
	cout << "������ͻ���IP��";
	cin >> clientIP;

	// �����ͻ���socket
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock == INVALID_SOCKET)// ����ʧ��
	{
		cout << "�ͻ���socket����ʧ��!" << endl;
		WSACleanup();
		return;
	}
	printf("�ͻ���socket�����ɹ�!\n");
	// ���ÿͻ��� ��ַ�� �˿� ip
	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(0);//��Ϊ0���Զ�����
	clientAddr.sin_addr.S_un.S_addr = inet_addr(clientIP);
	//socket��
	nRC = bind(sock, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));
	if (nRC == SOCKET_ERROR)
	{
		// ��ʧ��
		printf("�ͻ���socket��ʧ��!\n");
		closesocket(sock);
		WSACleanup();
		return;
	}
	setnonblocking(sock);//ʹ�÷�����ģʽ

	while (1)
	{
		// ���÷����� ��ַ��(ipv4) �˿� ip
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(69);
		serverAddr.sin_addr.S_un.S_addr = inet_addr(serverIP);
		int op;
		//ѡ��Ҫ���еĲ���
		cout << "Please select option��\n\t��1��upload \n\t��2��download \n\t��3��quit\n ";
		cin >> op;
		//if (buf == NULL)
			//continue;
		if (op == 1)//�ϴ��ļ�
		{
			cout << "Please input filename: ";
			cin >> token1;
			if (token1 != NULL)
				upload(token1);
			else
				cout << "please input filename correctly!!!" << endl;
			cout << "****upload****" << endl << endl;
		}
		else if (op == 2)//�����ļ�
		{
			cout << "Please input remote filename: ";
			cin >> token1;

			cout << "Please input local filename: ";
			cin >> token2;
			if (token2 != NULL && token1 != NULL)
				download(token1, token2);
			else
				cout << "please input remote filename or local filename correctly!!!\n" << endl;
			cout << "****download****" << endl << endl;
		}
		else if (op == 3)
		{
			cout << "****exit****" << endl;
			break;
		}
		else
			cout << "Please input number correctly!!!\n";
	}
	fclose(logFp);//������־

}
