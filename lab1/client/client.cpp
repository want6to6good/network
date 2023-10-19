#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <WinSock2.h>
#include <process.h>
#include <stdlib.h>
#include <conio.h>
#include<iostream>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

char userName[16] = { 0 };
bool printname = false;// 判断是否要在客户端打印名字

SOCKADDR_IN ServerAddr = { 0 };//服务端地址
USHORT uPort = 10010;//服务端端口
SOCKET ClientSocket = INVALID_SOCKET;//客户端套接字

//接收数据线程
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
	WSADATA wsaData = { 0 };//存放套接字信息
	//初始化套接字
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))//将库文件和程序绑定
	{
		cout << "初始化socket失败" << endl;
		return 0;
	}
	//创建套接字
	ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//设置服务器地址
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(uPort);//服务器端口
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//服务器地址
	//连接服务器
	if (SOCKET_ERROR == connect(ClientSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)))
	{
		cout << "连接失败" << endl;
		closesocket(ClientSocket);
		WSACleanup();
		return 0;
	}
	cout << "连接服务器成功!" << endl;
	cout << "请输入用户名: ";
	cin.getline(userName, 16);
	cout << "输入exit关闭程序" << endl;
	send(ClientSocket, userName, sizeof(userName), 0);
	cout << endl;
	CreateThread(NULL, 0, Recv, &ClientSocket, 0, NULL); //启动接收线程
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
			cout << "程序即将关闭. " << endl;
			Sleep(2000);
			int error = send(ClientSocket, bufferSend, sizeof(bufferSend), 0);
			if (error == SOCKET_ERROR)
				return -1;    //退出当前线程
			return 0;   //线程会关闭
		}
		int error = send(ClientSocket, bufferSend, sizeof(bufferSend), 0);
		if (error == SOCKET_ERROR)
			return -1;    //退出当前线程
		if (error != 0)
			printname = false;
	}
	while(1)    //让主线程一直转
		Sleep(10000000);
	closesocket(ClientSocket);
	WSACleanup();
	return 0;
}
