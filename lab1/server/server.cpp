#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <process.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#pragma comment(lib,"ws2_32.lib")
using namespace std;


SOCKET sockSrv = INVALID_SOCKET;		//服务端套接字
SOCKADDR_IN ClientAddr = { 0 };
SOCKADDR_IN ServerAddr = { 0 };			//服务端地址
USHORT uPort = 10010;					//服务器监听端口

int ClientAddrLen = sizeof(ClientAddr);
bool is_send = false;
int i = 0;
const int number = 10;					//最大用户数目
HANDLE HandleRecv[number] = { NULL };
HANDLE mainhandle;

struct Client
{
	SOCKET sClient;			 //客户端套接字
	char data[128];			 //数据缓冲区
	char userName[16];		 //客户端用户名
	int id=0;			     //客户端标签
};
Client clients[number] = { 0 };
DWORD WINAPI Send(LPVOID _id)
{
	int id = *(int*)_id;
	//SOCKET client = INVALID_SOCKET;
	char temp[128] = { 0 };
	// 创建一个临时套接字来存放要转发的客户端套接字
	//client = clients[id].sClient;
	// 创建一个临时的数据缓冲区，用来存放接收到的数据
	strcpy_s(temp, clients[id].data);
	// 将发送源的名字添加到转发的信息里
	sprintf_s(clients[id].data, "%s: %s", clients[id].userName, temp);
	if (strlen(temp) != 0 && !is_send)
	{
		int error = 0;
		// 如果数据不为空且还没转发则转发
		for (int j = 0; j < i; j++)
		{
			if (j != id)
			{
				// 向除自己之外的所有客户端发送信息
				error = send(clients[j].sClient, clients[id].data, sizeof(clients[id].data), 0);
				if (error == SOCKET_ERROR)
				{
					cout << "消息发送失败!" << endl;
					return 1;
				}
			}
		}
	}
	is_send = true;   // 转发成功后设置状态为已转发
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
	char temp[128] = { 0 };  // 临时数据缓冲区
	while (true)
	{
		memset(temp, 0, sizeof(temp));
		int error = recv(client, temp, sizeof(temp), 0);
		if (error == SOCKET_ERROR)
			continue;
		is_send = false;  // 设置转发状态为未转发
		strcpy_s(clients[id].data, temp);
		if (strcmp(temp, "exit") == 0)
		{
			closesocket(clients[id].sClient);
			CloseHandle(HandleRecv[id]);
			clients[id].sClient = 0;
			HandleRecv[id] = NULL;
			cout << "'" << clients[id].userName << "'" << "已经走了" << endl;
			break;  // 结束线程
		}
		else
		{
			cout << clients[id].userName << ": " << temp << endl;
			CreateThread(NULL, 0, Send, &id, 0, NULL); // 开启一个转发线程
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
			cout << "无可用客户端槽位" << endl;
			Sleep(3000);
			continue;
		}
		clients[index].sClient = accept(sockSrv, (SOCKADDR*)&ClientAddr, &ClientAddrLen);
		if (clients[index].sClient == INVALID_SOCKET)
		{
			cout << "接收错误: " << WSAGetLastError() << endl;
			closesocket(sockSrv);
			WSACleanup();
			return -1;
		}
		recv(clients[index].sClient, clients[index].userName, sizeof(clients[index].userName), 0); 
		cout << "用户‘" << clients[index].userName << "'" << " 已加入聊天" << endl;
		clients[index].id = clients[index].sClient;
		if (HandleRecv[index])   //如果句柄已被清零，那么就关掉句柄
			CloseHandle(HandleRecv[index]);
		HandleRecv[index] = CreateThread(NULL, 0, Recv, &clients[index].id, 0, NULL); //开启接收消息的线程
		mark[index] = clients[index].id; //防止Recv线程多次开启
		i++;
	}
	return 0;
}
int main()
{
	WSADATA wsaData = { 0 };
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		cout << "初始化socket失败" << endl;
		return 0;
	}
	//创建套接字
	sockSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//设置服务器地址
	ServerAddr.sin_family = AF_INET;//连接方式
	ServerAddr.sin_port = htons(uPort);//服务器监听端口
	ServerAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);//任何客户端都能连接这个服务器
	if (SOCKET_ERROR == bind(sockSrv, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		cout << "绑定失败" << endl;
		closesocket(sockSrv);
		return 0;
	}
	if (SOCKET_ERROR == listen(sockSrv, 20))
	{
		printf("监听失败: %d\n", WSAGetLastError());
		closesocket(sockSrv);
		WSACleanup();
		return 0;
	}
	cout << "等待客机连接……"<<endl;
	mainhandle = CreateThread(NULL, 0, Accept, NULL, 0, 0);
	string input;
	cout << "输入exit关闭系统 " << endl;
	while (true)
	{
		getline(cin,input);
		if (input=="exit")
		{
			cout << "系统已经关闭" << endl;
			CloseHandle(mainhandle);
			for (int j = 0; j < i; j++) //依次关闭套接字
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
