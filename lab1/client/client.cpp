#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <process.h>
#include <stdlib.h>
#include <conio.h>
#include<iostream>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

char userName[16] = { 0 };
bool printname = false;// �ж��Ƿ�Ҫ�ڿͻ��˴�ӡ����

SOCKADDR_IN ServerAddr = { 0 };//����˵�ַ
USHORT uPort = 10010;//����˶˿�
SOCKET ClientSocket = INVALID_SOCKET;//�ͻ����׽���

//���������߳�
DWORD WINAPI Recv(LPVOID a)
{
	char bufferRecv[128] = { 0 };
	while (true)
	{
		int error = recv(ClientSocket, bufferRecv, sizeof(bufferRecv), 0);
		if (strlen(bufferRecv) != 0)
				cout << "\r" << bufferRecv << endl<< userName << ": ";
		Sleep(100);
	}
	return 0;
}
int main()
{
	WSADATA wsaData = { 0 };//����׽�����Ϣ
	//��ʼ���׽���
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))//�����ļ��ͳ����
	{
		cout << "��ʼ��socketʧ��" << endl;
		return 0;
	}
	//�����׽���
	ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//���÷�������ַ
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(uPort);//�������˿�
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//��������ַ
	//���ӷ�����
	if (SOCKET_ERROR == connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		cout << "����ʧ��" << endl;
		closesocket(ClientSocket);
		WSACleanup();
		return 0;
	}
	cout << "���ӷ������ɹ�!" << endl;
	cout << "�������û���: ";
	cin.getline(userName, 16);
	cout << "����exit�رճ���" << endl;
	send(ClientSocket, userName, sizeof(userName), 0);
	cout << endl;
	CreateThread(NULL, 0, Recv, &ClientSocket, 0, NULL); //���������߳�
	char bufferSend[128] = { 0 };
	while (1)
	{
		if (printname != true)
		{
			cout << userName << ": ";
			printname = true;
		}
		cin.getline(bufferSend, 128);
		if (strcmp(bufferSend, "exit") == 0)
		{
			cout << "���򼴽��ر�. " << endl;
			Sleep(2000);
			int error = send(ClientSocket, bufferSend, sizeof(bufferSend), 0);
			if (error == SOCKET_ERROR)
				return -1;    //�˳���ǰ�߳�
			return 0;   //�̻߳�ر�
		}
		int error = send(ClientSocket, bufferSend, sizeof(bufferSend), 0);
		if (error == SOCKET_ERROR)
			return -1;    //�˳���ǰ�߳�
		if (error != 0)
			printname = false;
	}
	while(1)    //�����߳�һֱת
		Sleep(10000000);
	closesocket(ClientSocket);
	WSACleanup();
	return 0;
}
