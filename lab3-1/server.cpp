#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <cstring>
#include <ctime>
#include <io.h>
#include <iomanip>
#include <process.h>
#include <sys/types.h>
#include <vector>

using namespace std;

#pragma comment(lib,"ws2_32.lib")

//WSA&socket变量
WSADATA wsaData;
int sock_error;
SOCKADDR_IN RecvAddr;
SOCKADDR_IN SenderAddr;
SOCKET Sender;
int socketlen = sizeof(SOCKADDR);
// 数据包变量
const int maxlength = 32768 - 256;// 2^15
char* bufSend, * bufRecv;
int Seq = 0;
int resendmax = 10;    //重传最大次数为10
int resendnow = 0;
int datalength=0;
int Gram_length;
string input;

//计时变量
clock_t FileStartTime, FileEndTime;
struct timeval timeout;

bool checkSeq();
bool checkACK();
bool checkRecvFIN();
bool checkFIN();
bool checkSYN();
void setFIN();
void setSYN();
USHORT check(char* buf, int len);
char* set_udp_data(SOCKADDR_IN* source, SOCKADDR_IN* target, char* data, string name);
void updatecheck();
void endconnect();
int failconnect();
void sendfail();

bool checkSeq()
{
    return (Seq == int(USHORT((unsigned char)bufRecv[8])));
}
bool checkACK()
{
    return bufRecv[9] & 0x01;
}
bool checkRecvFIN()
{
    return ((Seq + 1) == int(USHORT((unsigned char)bufRecv[8])));
}
bool checkFIN()
{
    return bufRecv[9] & 0x04;
}
//对于SYN的应答要检查两项
bool checkSYN()
{
    return ((Seq + 1) == int(USHORT((unsigned char)bufRecv[8]))) && (bufRecv[9] & 0x02);
}   
void setFIN()
{
    bufSend[9] |= 0x04; updatecheck();
}
void setSYN()
{
    bufSend[9] |= 0x02; updatecheck();
}
USHORT check(char* buf, int len)
{
    if (len % 2) 
        len++;
    unsigned char* ubuf = (unsigned char*)buf;
    register ULONG sum = 0;
    while (len)
    {
        USHORT temp = USHORT(*ubuf << 8) + USHORT(*(ubuf + 1));
        sum += temp;
        if (sum & 0xFFFF0000)
        {
            sum &= 0xFFFF;
            sum++;
        }
        ubuf += 2;
        len -= 2;
    }
    return ~(sum & 0xFFFF);
}
char* set_udp_data(SOCKADDR_IN* source, SOCKADDR_IN* target, char* data, string name)
{
    USHORT sport = source->sin_port;
    USHORT tport = target->sin_port;
    int len = 12 + datalength + name.length();
    if (len % 2)
        Gram_length = len+1;
    else
        Gram_length = len;
    bufSend = new char[Gram_length];
    bufSend[0] = sport >> 8;
    bufSend[1] = sport & 0xFF;//源端口
    bufSend[2] = tport >> 8;
    bufSend[3] = tport & 0xFF;//目的端口
    bufSend[4] = len >> 8;
    bufSend[5] = len & 0xFF;//总长度
    bufSend[6] = 0;
    bufSend[7] = 0;         //检验和
    bufSend[8] = Seq & 0xFF;//序列号
    bufSend[9] = 0;         //标志位
    bufSend[10] = name.length() >> 8;
    bufSend[11] = name.length() & 0xFF;//文件名长度
    int location = 12;
    for (int i = 0; i < name.length(); i++)
        bufSend[location++] = name[i];
    for (int i = 0; i < datalength; i++)
        bufSend[location++] = data[i];
    if (len % 2) 
        bufSend[len] = 0;
    USHORT c = check(bufSend, len);
    bufSend[6] = c >> 8;
    bufSend[7] = c & 0xFF;
    return bufSend;
}
void updatecheck()
{
    USHORT CheckSum = (USHORT(((unsigned char)bufSend[6]) << 8) + USHORT((unsigned char)bufSend[7]));
    register ULONG sum = (~CheckSum & 0xFFFF) + USHORT((unsigned char)bufSend[9]);
    if (sum & 0xFFFF0000)
        CheckSum = (sum & 0xFFFF) + 1;
    else
        CheckSum = sum & 0xFFFF;
    CheckSum = ~CheckSum;
    bufSend[6] = CheckSum >> 8;
    bufSend[7] = CheckSum & 0xFF;
}
void endconnect()
{
    //delete[] bufSend;
    datalength = 0;
    bufSend = set_udp_data(&RecvAddr, &SenderAddr, (char*)"", "");
    setFIN();
    sock_error = sendto(Sender, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, socketlen);
    bufRecv = new char[maxlength];
    sock_error = recvfrom(Sender, bufRecv, maxlength, 0, (SOCKADDR*)&SenderAddr, &socketlen);
    if (checkFIN())
    {
        cout << "中断连接" << endl << "程序即将关闭..." << endl;
        return;
    }
}
int failconnect()
{
    cout << "wait..." << endl;
    resendnow++;
    if (resendnow == resendmax)
    {
        cout << "failed to connect" << endl << "The program would shut down, wait..." << endl;
        Sleep(1000);
        return 1;
    }
    return 0;
}
void sendfail()
{
    delete[] bufSend;
    datalength = 0;
    bufSend = set_udp_data(&RecvAddr, &SenderAddr, (char*)"", "");
    setFIN();
    sock_error = sendto(Sender, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, socketlen);
    bufRecv = new char[maxlength];
    sock_error = recvfrom(Sender, bufRecv, maxlength, 0, (SOCKADDR*)&SenderAddr, &socketlen);
    if (checkFIN())
    {
        cout << "发送失败" << endl << "程序即将关闭..." << endl;
        return;
    }
}


int main()
{
    timeout.tv_sec = 1000 * 20;     //设定等待回复时间为20秒
    timeout.tv_usec = 0;
    // 以下完成对套接字的绑定
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "初始化WSA失败" << endl;
        return -1;
    }
    cout << "WSA 启动" << endl;
    Sender = socket(AF_INET, SOCK_DGRAM, 0);
    if (Sender == INVALID_SOCKET)
    {
        cout << "绑定socket失败" << endl;
        return -1;
    }
    setsockopt(Sender, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    RecvAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(8887);
    sock_error = bind(Sender, (SOCKADDR*)&RecvAddr, sizeof(SOCKADDR));
    if (sock_error != 0)
    {
        cout << "绑定socket失败" << endl;
        return -1;
    }
    cout << "绑定socket成功" << endl;
    cout << "接收方ip地址为127.0.0.1，端口为6665" << endl;
    SenderAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    SenderAddr.sin_family = AF_INET;
    SenderAddr.sin_port = htons(6665);
    cout << "正在发送SYN报文 ..." << endl;
    bufSend = set_udp_data(&RecvAddr, &SenderAddr, (char*)"", "");
    setSYN();
    while (1)
    {
        sock_error = sendto(Sender, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, sizeof(SOCKADDR));
        if (sock_error != Gram_length)
        {
            if (failconnect())
                return 1;
            continue;
        }
        Sleep(1000);
        bufRecv = new char[maxlength];
        sock_error = recvfrom(Sender, bufRecv, maxlength, 0, (SOCKADDR*)&SenderAddr, &socketlen);
        if (sock_error == SOCKET_ERROR || !checkACK() || !checkSYN())
        {
            delete[] bufRecv;
            if (failconnect())
                return 1;
            continue;
        }
        delete[] bufRecv;
        break;
    }

    cout << "连接成功" << endl;
    cout << "最大重传次数为10 " << endl;
    cout << "最大等待时间为20s" << endl;
    Seq = 0;
    while (true)
    {
        resendnow = 0;
        cout << "输入文件名或输入‘exit'中断连接:";
        cin >> input;
        FileStartTime = clock();
        if (input == "exit")
        {
            endconnect();
            system("pause");
            return 0;
        }
        // 打开文件，并输出总长度，用于之后的循环判断
        cout << "发送文件: " << input << endl;
        ifstream is(input.c_str(), ifstream::in | ios::binary);
        is.seekg(0, is.end);
        int File_length = is.tellg();
        is.seekg(0, is.beg);
        is.close();
        datalength = File_length;
        //开始循环读入数据
        int location = 0;
        while (location < File_length)
        {
            int paklength;
            if (File_length - location < maxlength)
                paklength = File_length - location;
            else
                paklength = maxlength;

            ifstream is(input.c_str(), ifstream::in | ios::binary);
            is.seekg(location);
            bufSend = new char[paklength];
            is.read(bufSend, paklength);
            is.close();

            datalength = paklength;
            bool isSend = false;
            while (!isSend)
            {
                ifstream is(input.c_str(), ifstream::in | ios::binary);
                is.seekg(location);
                bufSend = new char[paklength];
                is.read(bufSend, paklength);
                is.close();
                bufSend = set_udp_data(&RecvAddr, &SenderAddr, bufSend, input);
                // 发送数据包的代码...
                sock_error = sendto(Sender, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, sizeof(SOCKADDR));
                cout << "传输进度: " << location << "/" << File_length << endl;
                if (sock_error != Gram_length)
                {
                    cout << "文件发送失败，重传中" << endl;
                    resendnow++;
                    if (resendnow == resendmax)         // 达到最大重传次数则发送FIN报文
                    {
                        sendfail();
                        return 0;
                    }
                    continue;
                }
                else
                    cout << "文件正在发送中" << endl;
                //Sleep(100);
                bufRecv = new char[maxlength];
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(Sender, &readfds);
                // 设置超时为2秒
                struct timeval tv;
                tv.tv_sec = 2;
                tv.tv_usec = 0;
                int result = select(Sender + 1, &readfds, NULL, NULL, &tv);
                if (result == SOCKET_ERROR) 
                    cout << 'a' << endl;
                else if (result == 0) // 超时，继续下一次循环
                {
                    cout << "超时" << endl;
                    Seq = (Seq + 1) % 2;   
                    continue;
                }
                else 
                {
                    if (FD_ISSET(Sender, &readfds)) 
                    {
                        // 收到数据，执行recvfrom
                        sock_error = recvfrom(Sender, bufRecv, maxlength, 0, (SOCKADDR*)&SenderAddr, &socketlen);
                        if (sock_error == SOCKET_ERROR)
                        {
                            delete[] bufRecv;
                            if (failconnect())
                                return 1;
                            continue;
                        }
                        Gram_length = (unsigned int)((USHORT)((unsigned char)bufRecv[4] << 8) + (USHORT)(unsigned char)bufRecv[5]);
                        if (Gram_length % 2)
                            Gram_length += 1;
                        if (sock_error != Gram_length)
                        {
                            cout << "文件接收失败，请重传 " << endl;
                            delete[] bufRecv;
                            resendnow++;
                            if (resendnow == resendmax)
                            {
                                sendfail();
                                return 0;
                            }
                            continue;
                        }
                        USHORT CheckSum = check(bufRecv, sock_error);
                        if (!CheckSum && checkSeq() && checkACK() && !checkFIN())       //有多重条件的判断
                        {
                            Seq = (Seq + 1) % 2;             //报文转换
                            isSend = true;
                            cout << "发送成功 " << endl;
                        }
                        else
                        {
                            if (CheckSum)
                                cout << "校验包出错" << endl;
                            if (!checkSeq())
                                cout << "发送端需要序列号: " << Seq << endl;
                            if (!checkACK())
                                cout << "包出错了. " << endl;
                            if (checkFIN())           //收到对方关闭报文，直接退出
                            {
                                cout << " 接收端离线，停止发送 ..." << " 程序即将关闭... " << endl;
                                return -1;
                            }
                            cout << "ACKq确认失败 " << endl;
                            resendnow++;
                            delete[] bufRecv;
                            if (resendnow == resendmax)
                            {
                                sendfail();
                                return 0;
                            }
                            delete[] bufRecv;
                            continue;
                        }
                        cout << "序列号:" << Seq << " ACK:" << checkACK() << " 校验和;" << CheckSum << endl;
                        delete[] bufRecv;
                        delete[] bufSend;
                    }
                }
            }
            location += paklength;
        }
        FileEndTime = clock();
        cout << "文件传输完毕" << endl;
        cout << "总用时: " << (FileEndTime - FileStartTime) * 1.0 / CLOCKS_PER_SEC <<"s" << endl;
        cout << "文件传输速率: " << File_length * 1.0 / (FileEndTime - FileStartTime) <<"KB/s" << endl;
    }
    // close
    sock_error = closesocket(Sender);
    sock_error = WSACleanup();
    return 0;
}