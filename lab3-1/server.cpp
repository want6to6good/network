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

//WSA&socket����
WSADATA wsaData;
int sock_error;
SOCKADDR_IN RecvAddr;
SOCKADDR_IN SenderAddr;
SOCKET Sender;
int socketlen = sizeof(SOCKADDR);
// ���ݰ�����
const int maxlength = 32768 - 256;// 2^15
char* bufSend, * bufRecv;
int Seq = 0;
int resendmax = 10;    //�ش�������Ϊ10
int resendnow = 0;
int datalength=0;
int Gram_length;
string input;

//��ʱ����
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
//����SYN��Ӧ��Ҫ�������
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
    bufSend[1] = sport & 0xFF;//Դ�˿�
    bufSend[2] = tport >> 8;
    bufSend[3] = tport & 0xFF;//Ŀ�Ķ˿�
    bufSend[4] = len >> 8;
    bufSend[5] = len & 0xFF;//�ܳ���
    bufSend[6] = 0;
    bufSend[7] = 0;         //�����
    bufSend[8] = Seq & 0xFF;//���к�
    bufSend[9] = 0;         //��־λ
    bufSend[10] = name.length() >> 8;
    bufSend[11] = name.length() & 0xFF;//�ļ�������
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
        cout << "�ж�����" << endl << "���򼴽��ر�..." << endl;
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
        cout << "����ʧ��" << endl << "���򼴽��ر�..." << endl;
        return;
    }
}


int main()
{
    timeout.tv_sec = 1000 * 20;     //�趨�ȴ��ظ�ʱ��Ϊ20��
    timeout.tv_usec = 0;
    // ������ɶ��׽��ֵİ�
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "��ʼ��WSAʧ��" << endl;
        return -1;
    }
    cout << "WSA ����" << endl;
    Sender = socket(AF_INET, SOCK_DGRAM, 0);
    if (Sender == INVALID_SOCKET)
    {
        cout << "��socketʧ��" << endl;
        return -1;
    }
    setsockopt(Sender, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    RecvAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(8887);
    sock_error = bind(Sender, (SOCKADDR*)&RecvAddr, sizeof(SOCKADDR));
    if (sock_error != 0)
    {
        cout << "��socketʧ��" << endl;
        return -1;
    }
    cout << "��socket�ɹ�" << endl;
    cout << "���շ�ip��ַΪ127.0.0.1���˿�Ϊ6665" << endl;
    SenderAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    SenderAddr.sin_family = AF_INET;
    SenderAddr.sin_port = htons(6665);
    cout << "���ڷ���SYN���� ..." << endl;
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

    cout << "���ӳɹ�" << endl;
    cout << "����ش�����Ϊ10 " << endl;
    cout << "���ȴ�ʱ��Ϊ20s" << endl;
    Seq = 0;
    while (true)
    {
        resendnow = 0;
        cout << "�����ļ��������롮exit'�ж�����:";
        cin >> input;
        FileStartTime = clock();
        if (input == "exit")
        {
            endconnect();
            system("pause");
            return 0;
        }
        // ���ļ���������ܳ��ȣ�����֮���ѭ���ж�
        cout << "�����ļ�: " << input << endl;
        ifstream is(input.c_str(), ifstream::in | ios::binary);
        is.seekg(0, is.end);
        int File_length = is.tellg();
        is.seekg(0, is.beg);
        is.close();
        datalength = File_length;
        //��ʼѭ����������
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
                // �������ݰ��Ĵ���...
                sock_error = sendto(Sender, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, sizeof(SOCKADDR));
                cout << "�������: " << location << "/" << File_length << endl;
                if (sock_error != Gram_length)
                {
                    cout << "�ļ�����ʧ�ܣ��ش���" << endl;
                    resendnow++;
                    if (resendnow == resendmax)         // �ﵽ����ش���������FIN����
                    {
                        sendfail();
                        return 0;
                    }
                    continue;
                }
                else
                    cout << "�ļ����ڷ�����" << endl;
                //Sleep(100);
                bufRecv = new char[maxlength];
                fd_set readfds;
                FD_ZERO(&readfds);
                FD_SET(Sender, &readfds);
                // ���ó�ʱΪ2��
                struct timeval tv;
                tv.tv_sec = 2;
                tv.tv_usec = 0;
                int result = select(Sender + 1, &readfds, NULL, NULL, &tv);
                if (result == SOCKET_ERROR) 
                    cout << 'a' << endl;
                else if (result == 0) // ��ʱ��������һ��ѭ��
                {
                    cout << "��ʱ" << endl;
                    Seq = (Seq + 1) % 2;   
                    continue;
                }
                else 
                {
                    if (FD_ISSET(Sender, &readfds)) 
                    {
                        // �յ����ݣ�ִ��recvfrom
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
                            cout << "�ļ�����ʧ�ܣ����ش� " << endl;
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
                        if (!CheckSum && checkSeq() && checkACK() && !checkFIN())       //�ж����������ж�
                        {
                            Seq = (Seq + 1) % 2;             //����ת��
                            isSend = true;
                            cout << "���ͳɹ� " << endl;
                        }
                        else
                        {
                            if (CheckSum)
                                cout << "У�������" << endl;
                            if (!checkSeq())
                                cout << "���Ͷ���Ҫ���к�: " << Seq << endl;
                            if (!checkACK())
                                cout << "��������. " << endl;
                            if (checkFIN())           //�յ��Է��رձ��ģ�ֱ���˳�
                            {
                                cout << " ���ն����ߣ�ֹͣ���� ..." << " ���򼴽��ر�... " << endl;
                                return -1;
                            }
                            cout << "ACKqȷ��ʧ�� " << endl;
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
                        cout << "���к�:" << Seq << " ACK:" << checkACK() << " У���;" << CheckSum << endl;
                        delete[] bufRecv;
                        delete[] bufSend;
                    }
                }
            }
            location += paklength;
        }
        FileEndTime = clock();
        cout << "�ļ��������" << endl;
        cout << "����ʱ: " << (FileEndTime - FileStartTime) * 1.0 / CLOCKS_PER_SEC <<"s" << endl;
        cout << "�ļ���������: " << File_length * 1.0 / (FileEndTime - FileStartTime) <<"KB/s" << endl;
    }
    // close
    sock_error = closesocket(Sender);
    sock_error = WSACleanup();
    return 0;
}