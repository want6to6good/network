#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <process.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#pragma comment(lib,"ws2_32.lib")
using namespace std;


SOCKET sockSrv = INVALID_SOCKET;		//������׽���
SOCKADDR_IN ClientAddr = { 0 };
SOCKADDR_IN ServerAddr = { 0 };			//����˵�ַ
USHORT uPort = 10010;					//�����������˿�

int ClientAddrLen = sizeof(ClientAddr);
bool is_send = false;
int i = 0;
const int number = 10;					//����û���Ŀ
HANDLE HandleRecv[number] = { NULL };
HANDLE mainhandle;

struct Client
{
	SOCKET sClient;			 //�ͻ����׽���
	char data[128];			 //���ݻ�����
	char userName[16];		 //�ͻ����û���
	int id=0;			     //�ͻ��˱�ǩ
};
Client clients[number] = { 0 };
DWORD WINAPI Send(LPVOID _id)
{
	int id = *(int*)_id;
	//SOCKET client = INVALID_SOCKET;
	char temp[128] = { 0 };
	// ����һ����ʱ�׽��������Ҫת���Ŀͻ����׽���
	//client = clients[id].sClient;
	// ����һ����ʱ�����ݻ�������������Ž��յ�������
	strcpy_s(temp, clients[id].data);
	// ������Դ��������ӵ�ת������Ϣ��
	sprintf_s(clients[id].data, "%s: %s", clients[id].userName, temp);
	if (strlen(temp) != 0 && !is_send)
	{
		int error = 0;
		// ������ݲ�Ϊ���һ�ûת����ת��
		for (int j = 0; j < i; j++)
		{
			if (j != id)
			{
				// ����Լ�֮������пͻ��˷�����Ϣ
				error = send(clients[j].sClient, clients[id].data, sizeof(clients[id].data), 0);
				if (error == SOCKET_ERROR)
				{
					cout << "��Ϣ����ʧ��!" << endl;
					return 1;
				}
			}
		}
	}
	is_send = true;   // ת���ɹ�������״̬Ϊ��ת��
	return 0;
}
DWORD WINAPI Recv(LPVOID _id)
{
	int id = -1;
	SOCKET client = INVALID_SOCKET;
	for (int j = 0; j < i; j++)
	{
		if (*(int*)_id == clients[j].id)
		{
			id = j;
			client = clients[j].sClient;
			break;
		}
	}
	char temp[128] = { 0 };  // ��ʱ���ݻ�����
	while (true)
	{
		memset(temp, 0, sizeof(temp));
		int error = recv(client, temp, sizeof(temp), 0);
		if (error == SOCKET_ERROR)
			continue;
		is_send = false;  // ����ת��״̬Ϊδת��
		strcpy_s(clients[id].data, temp);
		if (strcmp(temp, "exit") == 0)
		{
			closesocket(clients[id].sClient);
			CloseHandle(HandleRecv[id]);
			clients[id].sClient = 0;
			HandleRecv[id] = NULL;
			cout << "'" << clients[id].userName << "'" << "�Ѿ�����" << endl;
			break;  // �����߳�
		}
		else
		{
			cout << clients[id].userName << ": " << temp << endl;
			CreateThread(NULL, 0, Send, &id, 0, NULL); // ����һ��ת���߳�
		}
	}
	return 0;
}
DWORD WINAPI Accept(LPVOID _id)
{
	int mark[10] = { 0 };
	while (1)
	{
		int index = -1;
		for (int j = 0; j < number; j++)
		{
			if (clients[j].id == 0)
			{
				index = j;
				break;
			}
		}
		if (index == -1)
		{
			cout << "�޿��ÿͻ��˲�λ" << endl;
			Sleep(3000);
			continue;
		}
		clients[index].sClient = accept(sockSrv, (SOCKADDR*)&ClientAddr, &ClientAddrLen);
		if (clients[index].sClient == INVALID_SOCKET)
		{
			cout << "���մ���: " << WSAGetLastError() << endl;
			closesocket(sockSrv);
			WSACleanup();
			return -1;
		}
		recv(clients[index].sClient, clients[index].userName, sizeof(clients[index].userName), 0); 
		cout << "�û���" << clients[index].userName << "'" << " �Ѽ�������" << endl;
		clients[index].id = clients[index].sClient;
		if (HandleRecv[index])   //�������ѱ����㣬��ô�͹ص����
			CloseHandle(HandleRecv[index]);
		HandleRecv[index] = CreateThread(NULL, 0, Recv, &clients[index].id, 0, NULL); //����������Ϣ���߳�
		mark[index] = clients[index].id; //��ֹRecv�̶߳�ο���
		i++;
	}
	return 0;
}
int main()
{
	WSADATA wsaData = { 0 };
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "��ʼ��socketʧ��" << endl;
		return 0;
	}
	//�����׽���
	sockSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//���÷�������ַ
	ServerAddr.sin_family = AF_INET;//���ӷ�ʽ
	ServerAddr.sin_port = htons(uPort);//�����������˿�
	ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//�κοͻ��˶����������������
	if (SOCKET_ERROR == bind(sockSrv, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		cout << "��ʧ��" << endl;
		closesocket(sockSrv);
		return 0;
	}
	if (SOCKET_ERROR == listen(sockSrv, 20))
	{
		printf("����ʧ��: %d\n", WSAGetLastError());
		closesocket(sockSrv);
		WSACleanup();
		return 0;
	}
	cout << "�ȴ��ͻ����ӡ���"<<endl;
	mainhandle = CreateThread(NULL, 0, Accept, NULL, 0, 0);
	string input;
	cout << "����exit�ر�ϵͳ " << endl;
	while (true)
	{
		getline(cin,input);
		if (input=="exit")
		{
			cout << "ϵͳ�Ѿ��ر�" << endl;
			CloseHandle(mainhandle);
			for (int j = 0; j < i; j++) //���ιر��׽���
			{
				if (clients[j].sClient != INVALID_SOCKET)
					closesocket(clients[j].sClient);
			}
			closesocket(sockSrv);
			WSACleanup();
			exit(1);
			return 1;
		}
	}
};
