#include <iostream>
#include <string>
#include <cstring>
#include <winsock.h>
#include<fstream>
#pragma comment(lib, "ws2_32.lib")
using namespace std;
int maxsize = 4096;//传输缓冲区最大长度
unsigned char SYN = 0x1; //SYN = 1 ACK = 0
unsigned char ACK = 0x2;//ACK = 1
unsigned char ACK_SYN = 0x3;//SYN = 1, ACK = 1
unsigned char FIN = 0x4;//FIN = 1
unsigned char FIN_SYN = 0x5;//FIN=1 SYN=1
unsigned char ACK_FIN = 0x6;//FIN=1 SYN=1
unsigned char END = 0x7;//结束标志
double retime = 2000;
int handssuccess = 0;
int handscount = 5;//握手重试次数

struct Head
{
    u_short checksum;//校验和 16位
    u_short datasize;//所包含数据长度 16位
    unsigned char flag;//八位，使用后三位表示FIN ACK SYN
    unsigned char seq;//八位，传输的序列号
    Head()
    {
        checksum = 0;
        datasize = 0;
        flag = 0;
        seq = 0;
    }
};
u_short check(u_short* head, int size)
{
    int count = (size + 1) / 2;//计算循环次数，每次循环计算两个16位的数据
    u_short* buf = (u_short*)malloc(size + 1);//动态分配字符串变量
    memset(buf, 0, size + 1);//数组清空
    memcpy(buf, head, size);//数组赋值
    u_long checkSum = 0;
    while (count--) {
        checkSum += *buf++;//将2个16进制数相加
        if (checkSum & 0xffff0000) {//如果相加结果的高十六位大于一，将十六位置零，并将最低位加一
            checkSum &= 0xffff;
            checkSum++;
        }
    }
    return ~(checkSum & 0xffff);//对最后的结果取反
}
//初始化新的包（不指定seq）
void newbag(Head& head, const unsigned char str, char* buf)
{
    head.flag = str;
    head.checksum = 0;
    head.checksum = check((u_short*)&head, sizeof(head));
    memcpy(buf, &head, sizeof(head));
}
void newbag2(Head& head, const unsigned char str, char* buf, int seq)
{
    head.flag = str; //标志位设为str
    head.datasize = 0; //数据部分为0
    head.seq = (unsigned char)seq; //序列号与当前序列号相同
    head.checksum = 0; //校验和设为0
    head.checksum = check((u_short*)&head, sizeof(head));//重新计算校验和
    memcpy(buf, &head, sizeof(head)); //拷贝到数组
}
void threehands(SOCKET& socket, SOCKADDR_IN& addr)
{
    handssuccess = 0;
    int handscount1 = 0;//记录超时重传次数
    int addrlength = sizeof(addr); //地址族长度
    Head head; //报文头部
    char* buff = new char[sizeof(head)]; //缓存数组
    //第一次握手信息
    if (recvfrom(socket, buff, sizeof(head), 0, (sockaddr*)&addr, &addrlength) == SOCKET_ERROR)
    {//第一次握手接收失败
        cout << "【第一次握手失败】" << endl;
        return;
    }
    memcpy(&head, buff, sizeof(head)); //拷贝到头部
    if (head.flag == SYN && check((u_short*)&head, sizeof(head)) == 0)//校验和和标识符都正确
    {
        cout << "第一次握手成功【SYN】" << endl;
    }
    u_long mode = 1;
    ioctlsocket(socket, FIONBIO, &mode); //设置非阻塞模式
    //第二次握手信息
    head.flag = ACK; //设置ACK=1
    head.checksum = 0;
    head.checksum = check((u_short*)&head, sizeof(head));
    memcpy(buff, &head, sizeof(head));
    if (sendto(socket, buff, sizeof(head), 0, (sockaddr*)&addr, addrlength) == SOCKET_ERROR)
    {//发送报文
        cout << "【第二次握手失败】" << endl;
        return;
    }
    cout << "第二次握手成功【SYN ACK】" << endl;
    clock_t starttime = clock();//记录时间
    u_long imode = 1;

    ioctlsocket(socket, FIONBIO, &imode);//非阻塞模式
    //第三次握手
    while (clock() - starttime > retime)
    {//超时需要重传，等待回复报文
        if (recvfrom(socket, buff, sizeof(head), 0, (sockaddr*)&addr, &addrlength))
        {//收到回复
            cout << "【第三次握手失败】" << endl;
            break;
        }
        handscount1++;
        memcpy(buff, &head, sizeof(head));
        starttime = clock();
        if (handscount1 == handscount) {
            cout << "【等待超时】" << endl;
            return;
        }
    }
    cout << "第三次握手成功【ACK】" << endl;
    handssuccess = 1;
    cout << "【已连接发送端】" << endl;
    cout << "---------------------------------------------------" << endl;
}
//接收文件
int recvfile(SOCKET& socket, SOCKADDR_IN& addr, char* data)
{
    int addrlength = sizeof(addr);
    long int sum = 0;//文件长度，要返回的数据
    Head head;
    char* buf = new char[maxsize + sizeof(head)]; //缓冲数组长度是数据+头部的最大大小
    int seq = 0; //期待序列号清0
    int num = 0; //数据包编号
    while (1)
    {//接收数据包
        int recvlength = recvfrom(socket, buf, sizeof(head) + maxsize, 0, (sockaddr*)&addr, &addrlength);//接收报文长度
        memcpy(&head, buf, sizeof(head));

        if (head.flag == END && check((u_short*)&head, sizeof(head)) == 0)//END标志位，校验和为0，结束
        {
            cout << "【传输成功】" << endl;
            break; //结束跳出while循环
        }

        if (head.flag == unsigned char(0) && check((u_short*)buf, recvlength - sizeof(head)))//校验和不为0且flag是无符号字符
        {
            //判断收到的数据包是否正确
            if (seq != int(head.seq))
            {//seq不相等，数据包接收有误
                cout << "【接收错误，丢包发生】"<<endl;
                //重新发送ACK
                head.flag = SYN; //标志位设为0
                head.datasize = 0; //数据部分为0
                head.checksum = 0; //校验和设为0
                head.checksum = check((u_short*)&head, sizeof(head));//重新计算校验和
                memcpy(buf, &head, sizeof(head)); //拷贝到数组
                sendto(socket, buf, sizeof(head), 0, (sockaddr*)&addr, addrlength);
                //重新发送ACK
                string flag1;
                switch (int(head.flag)) {
                case 1:
                    flag1 = "【SYN】";
                case 2:
                    flag1 = "【ACK】";
                case 3:
                    flag1 = "【SYN ACK】";
                case 4:
                    flag1 = "【FIN】";
                case 5:
                    flag1 = "【FIN SYN】";
                case 6:
                    flag1 = "【FIN ACK】";
                case 7:
                    flag1 = "【END】";
                default:
                    flag1 = "【RESEND】";
                }
                cout << "--------重新发送第" << num - 1 << "个数据包的回复--------" << endl;
                cout << "【重新发送】标志位 = " << flag1 << " 序列号 = " << (int)head.seq << " 校验和 = " << int(head.checksum) << endl; //ACK等于seq
                continue; //丢包
            }

            //接收到数据包正确
            string flag1;
            switch (int(head.flag)) {
            case 1:
                flag1 = "【SYN】";
            case 2:
                flag1 = "【ACK】";
            case 3:
                flag1 = "【SYN ACK】";
            case 4:
                flag1 = "【FIN】";
            case 5:
                flag1 = "【FIN SYN】";
            case 6:
                flag1 = "【FIN ACK】";
            case 7:
                flag1 = "【END】";
            default:
                flag1 = "【RECEIVE】";
            }
            cout << "--------接收第" << num << "个数据包--------" << endl;
            cout << "【接收】标志位 = " << flag1 << " 序列号 = " << int(head.seq) << " 校验和 = " << int(head.checksum) << endl;
            char* bufdata = new char[recvlength - sizeof(head)]; //数组的大小是接收到的报文长度减去头部大小
            memcpy(bufdata, buf + sizeof(head), recvlength - sizeof(head)); //从头部后面开始拷贝，把数据拷贝到缓冲数组
            memcpy(data + sum, bufdata, recvlength - sizeof(head));
            sum = sum + int(head.datasize);

            //初始化首部
            newbag2(head, ACK, buf, seq);
            //发送ACK
            sendto(socket, buf, sizeof(head), 0, (sockaddr*)&addr, addrlength);
            switch (int(head.flag)) {
            case 1:
                flag1 = "【SYN】";
            case 2:
                flag1 = "【ACK】";
            case 3:
                flag1 = "【SYN ACK】";
            case 4:
                flag1 = "【FIN】";
            case 5:
                flag1 = "【FIN SYN】";
            case 6:
                flag1 = "【FIN ACK】";
            case 7:
                flag1 = "【END】";
            }
            cout << "--------发送第" << num++ << "个数据包的回复--------" << endl;
            cout << "【发送】标志位 = " << flag1 << " 序列号 = " << (int)head.seq << " 校验和 = " << int(head.checksum) << endl;
            seq++;//序列号加
            seq %= 256; //超过255要取模
        }
    }
    //发送END信息，结束
    newbag(head, END, buf);
    if (sendto(socket, buf, sizeof(head), 0, (sockaddr*)&addr, addrlength) == SOCKET_ERROR)
    {
        cout << "【发送错误】" << endl;
        return -1; //发送错误
    }
    return sum; //返回接收到的数据包字节总数
}

//关闭连接 三次挥手
void fourbye(SOCKET& socket, SOCKADDR_IN& addr)
{
    int addrlength = sizeof(addr);
    Head head;
    char* buf = new char[sizeof(head)];

    //第一次挥手
    while (1)
    {
        recvfrom(socket, buf, sizeof(head) + maxsize, 0, (sockaddr*)&addr, &addrlength);//接收报文长度
        memcpy(&head, buf, sizeof(head));
        if (head.flag == FIN && check((u_short*)&head, sizeof(head)) == 0)
        {
            cout << "第一次挥手成功【FIN ACK】" << endl;
            break;
        }
    }

    //第二次挥手
    head.flag = ACK;
    head.checksum = 0;
    head.checksum = check((u_short*)&head, sizeof(head));
    memcpy(buf, &head, sizeof(head));
    if (sendto(socket, buf, sizeof(head), 0, (sockaddr*)&addr, addrlength) == -1)
    {
        cout << "【第二次挥手失败】" << endl;
        return;
    }
    cout << "第二次挥手成功【FIN ACK】" << endl;
    clock_t starttime = clock();//记录第二次挥手发送时间

    //第三次挥手
    while (recvfrom(socket, buf, sizeof(head), 0, (sockaddr*)&addr, &addrlength) <= 0)
    {
        if (clock() - starttime > retime)
        {
            memcpy(buf, &head, sizeof(head));
            if (sendto(socket, buf, sizeof(head), 0, (sockaddr*)&addr, addrlength) == -1)
            {
                cout << "【第三次挥手失败】" << endl;
                return;
            }
        }
    }
    Head head1; //不修改head结构，使用临时头部head1计算
    memcpy(&head1, buf, sizeof(head));
    string flag1;
    if (head1.flag == ACK_FIN && check((u_short*)&head1, sizeof(head1) == 0))
    {

        cout << "第三次挥手成功【ACK】" << endl;
    }
    else
    {
        cout << "【连接关闭失败！传输终止】" << endl;
        return;
    }
    cout << "【结束连接】" << endl;
}
int initwsa() {
    WSADATA wsaDATA;
    if (!WSAStartup(MAKEWORD(2, 2), &wsaDATA)) {
        cout << "||【系统消息】:初始化网络环境成功!!              ||" << endl;
        return 1;
    }
    else {
        cout << "||【系统消息】:初始化网络环境失败!!              ||" << endl;
        return 0;
    }
}
int main() {
    cout << "---------------------------------------------------" << endl;
    cout << "||                    接收端                     ||" << endl;
    cout << "---------------------------------------------------" << endl;
    SOCKET c_client;
    sockaddr_in client_addr;
    // 创建服务器套接字
    if (!initwsa()) {
        return 0;//初始化失败则退出程序
    }
    char ip[100] ;
    u_short port ;
    cout << "请输入本机IP地址：";
    cin >> ip;
    cout << "请输入端口号：";
    cin >> port;
    cout << "【等待连接】" << endl;
    // 配置接收端地址
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);//端口号
    client_addr.sin_addr.s_addr = inet_addr(ip);//IP地址
    c_client = socket(AF_INET, SOCK_DGRAM, 0);//使用数据报套接字，适合用于UDP协议。

    //绑定套接字
    bind(c_client, (sockaddr*)&client_addr, sizeof(client_addr));

         int len = sizeof(client_addr);
         while (1) {
             //建立连接
             threehands(c_client, client_addr);
             if (handssuccess == 0) {
                 cout << "【重新连接】" << endl;
                 continue;
             }

             char* name = new char[20];
             char* data = new char[100000000];
             string file;
             int filenamelength = recvfile(c_client, client_addr, name);
             for (int i = 0; i < filenamelength; i++)
             {
                 file = file + name[i];
             }
             clock_t starttime = clock();
             int datalength = recvfile(c_client, client_addr, data);
             clock_t end = clock();
             //关闭连接
             fourbye(c_client, client_addr);
             ofstream out(file.data(), ofstream::binary); //输出流采用二进制的方法
             streampos startPos = out.tellp(); // 记录初始位置
             int bytesWritten = 0;
             if (out.is_open()) {
                 out.write(data, datalength); // 将数据以二进制方式写入文件
                 streampos endPos = out.tellp(); // 获取写入完成后的位置
                 bytesWritten = endPos - startPos; // 计算写入的总字节数
                 out.close();
             }
             else {
                 // 文件打开失败的处理
                 cout << "【文件打开出错】" << endl;
                 return 0;
             }
             out.close();
             cout << "【文件传输成功】" << endl;
             cout << "【文件名称】=" << file << endl;
             cout << "【文件大小】 =" << ((double)bytesWritten) / 1024 << "KB" << endl;
             cout << "【传输总时间】=" << end - starttime << "ms" << endl;
             cout << "【吞吐率】 =" << ((double)bytesWritten) / (double(end - starttime)) << "byte/ms" << endl;
             cout << "---------------------------------------------------" << endl;
         }
    closesocket(c_client); //关闭套接字
    WSACleanup();
    return 0;

}
