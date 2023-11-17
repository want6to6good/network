#include <fstream>
#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>
#include <cstring>
#include <ctime>
#include <process.h>

using namespace std;

#pragma comment(lib, "ws2_32")

// WSA&socket����
WSADATA wsaData;
int sock_error;
SOCKADDR_IN RecvAddr;
SOCKADDR_IN SenderAddr;
SOCKET receiver;
int socketlen = sizeof(SOCKADDR);
char* bufSend, * bufRecv;
const int MAX_PACLEN = 32768;    // ��Ϊ���ĸ����ȱ�ʾλ����������16λ������2^15-1=65535

int Data_length;      // �����ܳ�
int Gram_length;      // �����ܳ�
int Seq = 0;    // ���к�
int File_length;

//��ʱ����
clock_t lastTime = 0;     // ����ʱ�� ��endTime - startTime��
clock_t timecount = 0;    // ʱ��ļ�����
clock_t startTime = 0, endTime = 0;
clock_t FileStartTime, FileEndTime;
HANDLE ticktime;
int waiting_max = 50;        // ��ʱ���޶�

bool checkSeq();
bool checkACK();
bool checkFIN();
bool checkSYN();
void setNAK();
void setACK();
void setFIN();
void setSYN();
USHORT check(char* buf, int len);
char* set_udp_data(SOCKADDR_IN* source, SOCKADDR_IN* target, char* data, string name);
string FileName();
DWORD WINAPI TimeCheck(LPVOID lparam);
void resettime();
void endconnect();

USHORT check(char* buf, int len)
{
    if (len % 2 == 1) len++;
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
    int len = 12 + Data_length + name.length();
    if (len % 2)
        Gram_length = len+1;
    else
        Gram_length = len;
    bufSend = new char[Gram_length];
    bufSend[0] = sport >> 8;
    bufSend[1] = sport & 0xFF;
    bufSend[2] = tport >> 8;
    bufSend[3] = tport & 0xFF;
    bufSend[4] = len >> 8;
    bufSend[5] = len & 0xFF;
    bufSend[6] = 0;
    bufSend[7] = 0;
    bufSend[8] = Seq & 0xFF;
    bufSend[9] = 0;
    bufSend[10] = name.length() >> 8;
    bufSend[11] = name.length() & 0xFF;
    int phead = 12;
    for (int i = 0; i < name.length(); i++)
        bufSend[phead++] = name[i];
    for (int i = 0; i < Data_length; i++)
        bufSend[phead++] = data[i];
    if (len % 2)
        bufSend[len] = 0;
    USHORT c = check(bufSend, len);
    bufSend[6] = c >> 8;
    bufSend[7] = c & 0xFF;
    return bufSend;
}
string FileName()
{
    string name;
    int len = (int)(USHORT((unsigned char)(bufRecv[10]) << 8) + USHORT((unsigned char)(bufRecv[11])));
    for (int i = 0; i < len; i++)
        name += bufRecv[i + 12];
    return name;
}
void update()
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
bool checkSeq() 
{ 
    return (Seq == int(USHORT((unsigned char)bufRecv[8])));
}
bool checkACK() 
{ 
    return bufRecv[9] & 0x01;
}
bool checkFIN() 
{ 
    return bufRecv[9] & 0x04;
}
bool checkSYN() 
{
    return bufRecv[9] & 0x02;
}
void setNAK() 
{
    bufSend[9] &= 0xFE; update();
}
void setACK()
{
    bufSend[9] |= 0x01; update();
}
void setFIN() {
    bufSend[9] |= 0x04;
    bufSend[8] +=1;
    update();
}
void setSYN() {
    bufSend[9] |= 0x02;
    bufSend[8] = bufRecv[8] + 1;    //�ظ�����ԭʼ���к�+1
    update();
}
void resettime()
{
    startTime = clock();
    timecount = 0;
    TerminateThread(ticktime, 0);
    CloseHandle(ticktime);
    ticktime = CreateThread(NULL, NULL, TimeCheck, LPVOID(&waiting_max), 0, 0);
}
void endconnect()
{
    Data_length = 0;
    bufSend = set_udp_data(&RecvAddr, &SenderAddr, (char*)"", "");
    setFIN();    //��֪�Է��ر�
    sock_error = sendto(receiver, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, sizeof(SOCKADDR));
    cout << "���򼴽��ر�... " << endl;
    Sleep(1000);
    CloseHandle(ticktime);
    delete[] bufRecv;
    delete[] bufSend;
    system("pause");
    exit(0);
}

// ���̣߳� ������ʱ�����ڳ�ʱ�ȴ���
DWORD WINAPI TimeCheck(LPVOID lparam)
{
    int* waiting_max = (int*)lparam;
    while (true)
    {
        endTime = clock();
        lastTime = endTime - startTime;
        if (lastTime / CLOCKS_PER_SEC > timecount)    // CLOCKS_PER_SEC=1000
            timecount = lastTime / CLOCKS_PER_SEC;
        if (lastTime / CLOCKS_PER_SEC == *waiting_max)
            endconnect();
    }
    return 0;
}

int main()
{
    // ��������Ϊsocket�İ�
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        cout << "��ʼ��WSAʧ��" << endl;
        return -1;
    }
    cout << "WSA ����" << endl;
    receiver = socket(AF_INET, SOCK_DGRAM, 0);
    RecvAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(6665);
    sock_error = bind(receiver, (SOCKADDR*)&RecvAddr, sizeof(SOCKADDR));
    if (sock_error != 0)
    {
        cout << "��socketʧ��" << endl;
        sock_error = WSACleanup();
        return -1;
    }
    cout << "��socket�ɹ�" << endl;
    SenderAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
    SenderAddr.sin_family = AF_INET;
    SenderAddr.sin_port = htons(8887);

    cout << "���ȴ�����:" << waiting_max << endl;
    // ��Է���������
    startTime = clock();
    ticktime = CreateThread(NULL, NULL, TimeCheck, LPVOID(&waiting_max), 0, 0);
    while (1)
    {
        cout << "�ȴ����� ..." << endl;
        bufRecv = new char[MAX_PACLEN];
        sock_error = recvfrom(receiver, bufRecv, MAX_PACLEN, 0, (SOCKADDR*)&SenderAddr, &socketlen);
        if (sock_error == SOCKET_ERROR || !checkSYN())
        {
            delete[] bufRecv;
            continue;
        }
        Data_length = 0;
        bufSend = set_udp_data(&RecvAddr, &SenderAddr, (char*)"", "");
        setACK();
        setSYN();
        Sleep(100);
        sock_error = sendto(receiver, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, socketlen);
        if (sock_error != Gram_length)
        {
            delete[] bufRecv;
            continue;
        }
        TerminateThread(ticktime, 0);
        CloseHandle(ticktime);
        delete[] bufRecv;
        break;
    }
    cout << "���ӳɹ�" << endl;
    Seq = 0;
    startTime = clock();
    timecount = 0;
    ticktime = CreateThread(NULL, NULL, TimeCheck, LPVOID(&waiting_max), 0, 0);
    FileStartTime = clock();
    while (true)
    {
        bufRecv = new char[MAX_PACLEN];
        sock_error = recvfrom(receiver, bufRecv, MAX_PACLEN, 0, (SOCKADDR*)&SenderAddr, &socketlen);
        //cout << "�յ�" << endl;
        Gram_length = (int)((USHORT)((unsigned char)bufRecv[4] << 8) + (USHORT)(unsigned char)bufRecv[5]);
        if (Gram_length % 2)
            Gram_length += 1;
        if (sock_error != Gram_length)
        {
            delete[] bufRecv;
            /*startTime = clock();
            timecount = 0;
            TerminateThread(ticktime, 0);
            CloseHandle(ticktime);
            ticktime = CreateThread(NULL, NULL, TimeCheck, LPVOID(&waiting_max), 0, 0);*/
            resettime();
            continue;
        }
        USHORT CheckSum = check(bufRecv, Gram_length);  
        Data_length = 0;
       // cout << "��ʼ���" << endl;
        if (!CheckSum && checkSeq())
        {
            if (checkFIN())
                endconnect();
            bufSend = set_udp_data(&RecvAddr, &SenderAddr, (char*)"", "");
            setACK();
            Seq = (Seq + 1) % 2;   //״̬Ǩ��
        }
        else
        {
            if (CheckSum)
                cout << "�����У����: " << CheckSum << " ���Ͷ˰��������� " << endl;
            if (!checkSeq()) 
                cout << "���кŴ���: " << Seq << endl;
            bufSend = set_udp_data(&RecvAddr, &SenderAddr, (char*)"", "");
            setNAK();
            sock_error = sendto(receiver, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, socketlen);
            delete[] bufRecv;
            delete[] bufSend;
            resettime();
            continue;
        }
        cout << "���к�:" << Seq << " ACK:" << checkACK() << " У���;" << CheckSum << endl;
        Sleep(100);
        int randomValue = rand() % 100; // ����0��99�������
        if (randomValue < 5) { 
            cout << "ģ�ⶪ��..." << endl;
            delete[] bufSend;
            delete[] bufRecv;
            continue; // ���������ݰ����������ѭ��
        }
        else
        {
            sendto(receiver, bufSend, Gram_length, 0, (SOCKADDR*)&SenderAddr, socketlen);
            // �յ����ݺ󣬰�����д���ļ�
            Gram_length = (int)((USHORT)((unsigned char)bufRecv[4] << 8) + (USHORT)(unsigned char)bufRecv[5]);
            string filename = FileName();
            ofstream os(filename.c_str(), ios::app | ios::binary);
            os.seekp(0, os.end);
            for (int i = 0; i < Gram_length - filename.length() - 12; i++)
                os << (unsigned char)bufRecv[i + 12 + filename.length()];
            os.close();
            ifstream is(filename.c_str(), ifstream::in | ios::binary);
            is.seekg(0, is.end);
            File_length = is.tellg();
            is.close();
            cout << "�ļ��������:" << File_length <<"B"<< endl;
            if (Gram_length < 32768 - 256)
            {
                FileEndTime = clock();
                cout << "�ļ��������,�������ݾ���д��" << endl;
                cout << "����ʱ: " << (FileEndTime - FileStartTime) * 1.0 / CLOCKS_PER_SEC << "s" << endl;
                cout << "�ļ���������: " << File_length * 1.0 / (FileEndTime - FileStartTime) << "KB/s" << endl;
                FileStartTime = clock();
            }
        }
        delete[] bufSend;
        delete[] bufRecv;
        resettime();
    }
    // close
    CloseHandle(ticktime);
    sock_error = closesocket(receiver);
    sock_error = WSACleanup();
    system("pause");
    return 0;
}