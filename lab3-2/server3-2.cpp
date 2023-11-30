//操控socket需要使用winsock.h库
#include <winsock.h>
#include <iostream>
#include <fstream>
#include<time.h>
//添加编译命令
#pragma comment(lib,"ws2_32.lib")//把ws2_32.lib 这个库加入到工程文件中,提供了对网络相关API的支持
using namespace std;
int maxlength = 4096;//传输缓冲区最大长度
unsigned char SYN = 0x1; //SYN = 1 ACK = 0
unsigned char ACK = 0x2;//ACK = 1
unsigned char ACK_SYN = 0x3;//SYN = 1, ACK = 1
unsigned char FIN = 0x4;//FIN = 1
unsigned char FIN_SYN = 0x5;//FIN=1 SYN=1
unsigned char ACK_FIN = 0x6;//FIN = 1 ACK = 1
unsigned char END = 0x7;//结束标志
double retime = 2000;
int handscount = 5;//握手重试次数
int handssuccess = 0;//握手是否成功
int sendsuccess = 0;//重传是否成功
int slidewindows = 10;//滑动窗口
int num = 0;//发送的数据包个数
int totalnum = 0;//总数据包个数
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
//初始化新的包（是否指定seq）
void newbag(Head& head, const unsigned char str, char* buf)
{
	head.flag = str;
	head.checksum = check((u_short*)&head, sizeof(head));
	memcpy(buf, &head, sizeof(head));
}
void threehands(SOCKET& socket, SOCKADDR_IN& addr)//三次握手建立连接
{
	handssuccess = 0;
	int length = sizeof(addr); //地址族大小
	//第一次握手
	Head head = Head(); //数据首部
	head.flag = SYN; //标志设为SYN
	head.checksum = check((u_short*)&head, sizeof(head)); //计算校验和
	char* buff = new char[sizeof(head)]; //缓冲数组
	memcpy(buff, &head, sizeof(head));//将首部放入缓冲数组
	if (sendto(socket, buff, sizeof(head), 0, (sockaddr*)&addr, length) == SOCKET_ERROR)
	{//发送失败
		cout << "【第一次握手失败】" << endl;
		return;
	}
	cout << "第一次握手成功【SYN】" << endl;
	clock_t handstime = clock(); //记录发送第一次握手时间
	u_long mode = 1;
	ioctlsocket(socket, FIONBIO, &mode); //设置非阻塞模式
	int handscount1 = 0;//记录超时重传次数
	//第二次握手
	while (recvfrom(socket, buff, sizeof(head), 0, (sockaddr*)&addr, &length) <= 0)
	{//等待接收

		if (clock() - handstime > retime)//超时重传
		{
			memcpy(buff, &head, sizeof(head));//将首部放入缓冲区
			sendto(socket, buff, sizeof(head), 0, (sockaddr*)&addr, length); //再次发送
			handstime = clock(); //计时
			cout << "【连接超时！等待重传……】" << endl;
			handscount1++;
			if (handscount1 == handscount) {
				cout << "【等待超时】" << endl;
				return;
			}
		}
	}
	memcpy(&head, buff, sizeof(head)); //ACK正确且检查校验和无误
	if (head.flag == ACK && check((u_short*)&head, sizeof(head) == 0))
	{
		cout << "第二次握手成功【SYN ACK】" << endl;
		handscount1 = 0;
	}
	else
	{
		cout << "【第二次握手失败】" << endl;
		return;
	}
	//第三次握手
	head.flag = ACK_SYN; //ACK=1 SYN=1
	head.checksum = check((u_short*)&head, sizeof(head)); //计算校验和
	sendto(socket, (char*)&head, sizeof(head), 0, (sockaddr*)&addr, length); //发送握手请求
	bool win = 0; //检验是否连接成功的标志
	while (clock() - handstime <= retime)
	{//等待回应
		if (recvfrom(socket, buff, sizeof(head), 0, (sockaddr*)&addr, &length))
		{//收到报文
			win = 1;
			break;
		}
		//选择重发
		memcpy(buff, &head, sizeof(head));
		sendto(socket, buff, sizeof(head), 0, (sockaddr*)&addr, length);
		handstime = clock();
		handscount1++;
		if (handscount1 == handscount) {
			cout << "【等待超时】" << endl;
			return;
		}
	}
	if (!win)
	{
		cout << "【第三次握手失败】" << endl;
		return;
	}
	cout << "第三次握手成功【ACK】" << endl;
	handssuccess = 1;
	cout << "【连接成功】" << endl;
	cout << "---------------------------------------------------" << endl;

}

//initwsa函数用来初始化socket,保证Winsock库初始化成功后才会执行聊天的功能，初始化失败则退出程序
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
//文件传输
void sendfile(SOCKET& socket, SOCKADDR_IN& addr, char* data, int data_len)
{
	num = 0;
	Head head;
	int addrlength = sizeof(addr);//地址的长度
	int bagsum = data_len / maxlength; //数据包总数，等于数据长度/一次发送的字节数
	if (data_len % maxlength) {
		bagsum++; //向上取整
	}
	totalnum = bagsum;
	int base = -1;//发送窗口开始的位置
	int nextseq = 0;//发送窗口结束的位置
	char* buff = new char[sizeof(head)]; //数据缓冲区
	clock_t starttime; //计时

	while(base<bagsum-1)
	{
		int len;
		if (nextseq == bagsum - 1)
		{//最后一个数据包是向上取整的结果，因此数据长度是剩余所有
			len = data_len - (bagsum - 1) * maxlength;
		}
		else
		{//非最后一个数据长度均为maxlength
			len = maxlength;
		}
		//sendbag部分、数据段包发送
		//头部初始化及校验和计算
		//sendsuccess = 0;
		int addrlength = sizeof(addr);
		Head head;
		char* buf = new char[maxlength + sizeof(head)];
		head.datasize = len; //使用传入的data的长度定义头部datasize
		head.seq = unsigned char(nextseq % 256);//序列号
		memcpy(buf, &head, sizeof(head)); //拷贝首部的数据
		memcpy(buf + sizeof(head), data + nextseq * maxlength, sizeof(head) + len); //数据data拷贝到缓冲数组
		head.checksum = check((u_short*)buf, sizeof(head) + len);//计算数据部分的校验和
		memcpy(buf, &head, sizeof(head)); //更新后的头部再次拷贝到缓冲数组
		//发送
		sendto(socket, buf, len + sizeof(head), 0, (sockaddr*)&addr, addrlength);//发送
		string flag1;
		switch (head.flag) {
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
			flag1 = "【SEND】";
			;
		}
		cout << "--------发送第" << num<<"/"<< totalnum << "个数据包--------" << endl;
		cout << "【发送】标志位 = " << flag1 << " 序列号 = " << int(head.seq) << " 校验和 = " << int(head.checksum) << endl;
		//没有超过窗口大小且未发送完，并且不是窗口的结尾
		if (nextseq < base + slidewindows && nextseq != bagsum) //没有处理超过窗口大小的情况
		{
			starttime = clock();//记录发送时间
			nextseq++; //发送窗口后沿向后滑动
		}
		u_long mode = 1;
		ioctlsocket(socket, FIONBIO, &mode); //设置非阻塞模式
		cout << "滑动窗口剩余大小：" << slidewindows - nextseq + base << endl;
		Sleep(50); //休眠50ms等待接收，防止频繁超时重传使得频繁反馈丢包信息
		if (recvfrom(socket, buf, maxlength, 0, (sockaddr*)&addr, &addrlength)) {//接收消息
			memcpy(&head, buf, sizeof(head));//缓冲区接收到信息，读取
			u_short checksum1 = check((u_short*)&head, sizeof(head)); //计算校验和
			//如果校验和不正确或没有收到ACK
			if (int(checksum1) != 0||head.flag != ACK)
			{
				string flag1;
				switch (head.flag) {
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
					flag1 = "【SEND】";
					;
				}
				cout << "--------接收到第" << num++ << "个数据包的回复--------" << endl;
				cout << "【接收】标志位 = " <<flag1 << " 序列号 = " << int(head.seq) << " 校验和 = " << int(head.checksum) << endl;
				cout << "【传输错误丢包重传】" << endl;
				nextseq = base + 1; //回到base+1号即未被确认的最低序号数据包
				cout << "窗口前沿 = " << base << " 窗口后沿 = " << nextseq << endl;
				num--;
				//totalnum++;
				continue; //丢包处理，进入下一轮循环重新发送
			}
			//接收到正确的数据包，输出接收信息
			else
			{
				if (int(head.seq) >= base % 256)
				{//序列号没用完，可以继续使用
					base += int(head.seq) - base % 256;
					string flag1;
					switch (head.flag) {
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
						flag1 = "【SEND】";
						;
					}
					cout << "--------接收第" << num++ << "个数据包的回复--------" << endl;
					cout << "【接收】标志位 = " << flag1 << " 序列号 = " << int(head.seq) << " 校验和 = " << int(head.checksum) << endl;
					cout << "滑动窗口数 = " << slidewindows << " 窗口前沿 = " << base << " 窗口后沿 = " << nextseq << endl;
				}else{//序列号用完了
					if (base % 256 > 256 - slidewindows - 1)
					{
						base += 256 + int(head.seq) - base % 256;
						string flag1;
						switch (head.flag) {
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
							flag1 = "【SEND】";
							;
						}
						cout << "--------接收第" << num++ << "个数据包的回复--------" << endl;
						cout << "【接收】标志位 = " << flag1 << " 序列号 = " << int(head.seq) << " 校验和 = " << int(head.checksum) << endl;
						cout << "滑动窗口数 = " << slidewindows << " 窗口前沿 = " << base << " 窗口后沿 = " << nextseq << endl;
					}
				}
			}
		}
		else
		{//未接收到消息
			if (clock() - starttime > retime)
			{
				nextseq = base + 1; //回退到first+1处
				cout << "【超时重新发送】" << endl;
			}
		}

		mode = 0;
		ioctlsocket(socket, FIONBIO, &mode); //阻塞
	}
		//传输完毕
		buff = new char[sizeof(head)]; //缓冲数组
		newbag(head, END, buff); //调用函数生成ACK=SYN=FIN=1的数据包，表示结束
		sendto(socket, buff, sizeof(head), 0, (sockaddr*)&addr, addrlength);
		clock_t starttime2 = clock();//计时

		//处理超时重传
		while (1)
		{
			u_long mode = 1;
			ioctlsocket(socket, FIONBIO, &mode); //设置非阻塞模式
			//等待接收消息
			while (recvfrom(socket, buff, maxlength, 0, (sockaddr*)&addr, &addrlength) <= 0)
			{
				if (clock() - starttime2 > retime) //超时重传
				{
					char* buf = new char[sizeof(head)]; //缓冲数组
					newbag(head, END, buf); //调用函数生成ACK=SYN=FIN=1的数据包，表示结束
					cout << "【超时等待重传】" << endl;
					sendto(socket, buf, sizeof(head), 0, (sockaddr*)&addr, addrlength); //继续发送相同的数据包
					starttime2 = clock(); //新一轮计时
				}
			}
			memcpy(&head, buff, sizeof(head));//缓冲区接收到信息，读取到首部
			if (head.flag == END)
			{//接收到END口令
				cout << "传输成功!" << endl;
				//sendsuccess = 1;
				break;
			}
		}
		u_long mode = 0;
		ioctlsocket(socket, FIONBIO, &mode);//改回阻塞模式
}
			

//关闭连接 三次挥手
void fourbye(SOCKET& socket, SOCKADDR_IN& addr)
{
	int addrlength = sizeof(addr);
	Head head;
	char* buff = new char[sizeof(head)];
	//第一次挥手
	head.flag = FIN;
	head.checksum = 0;//校验和置0
	head.checksum = check((u_short*)&head, sizeof(head));
	memcpy(buff, &head, sizeof(head));
	if (sendto(socket, buff, sizeof(head), 0, (sockaddr*)&addr, addrlength) == SOCKET_ERROR)
	{
		cout << "【第一次挥手失败】" << endl;
		return;
	}
	cout << "第一次挥手【FIN ACK】" << endl;
	clock_t byetime = clock(); //记录发送第一次挥手时间

	u_long mode = 1;
	ioctlsocket(socket, FIONBIO, &mode);	

	//第二次挥手
	while (recvfrom(socket, buff, sizeof(head), 0, (sockaddr*)&addr, &addrlength) <= 0)
	{//等待接收
		if (clock() - byetime > retime)//超时重传
		{
			memcpy(buff, &head, sizeof(head));//将首部放入缓冲区
			sendto(socket, buff, sizeof(head), 0, (sockaddr*)&addr, addrlength);
			byetime = clock();
		}
	}
	//进行校验和检验
	memcpy(&head, buff, sizeof(head));
	if (head.flag == ACK && check((u_short*)&head, sizeof(head) == 0))
	{
		cout << "第二次挥手【FIN ACK】" << endl;
	}
	else
	{
		cout << "【第二次挥手失败】" << endl;
		return;
	}
	//第三次挥手
	head.flag = ACK_FIN;
	head.checksum = check((u_short*)&head, sizeof(head));//计算校验和
	memcpy(buff, &head, sizeof(head));
	if (sendto(socket, (char*)&head, sizeof(head), 0, (sockaddr*)&addr, addrlength) == -1)
	{
		cout << "【第三次挥手失败】" << endl;
		return;
	}
	cout << "第三次挥手【ACK】" << endl;
	cout << "【结束连接】" << endl;
	cout << "---------------------------------------------------" << endl;
}

int main() {
	SOCKET s_server;
	SOCKADDR_IN server_addr;//服务端地址 
	cout << "---------------------------------------------------" << endl;
	cout << "||                    发送端                     ||" << endl;
	cout << "---------------------------------------------------" << endl;
	if (!initwsa()) {
		return 0;//初始化失败则退出程序
	}
	char ip[100];
	u_short port;
	int windows;
	int flag = 1;
	while (flag) {
		cout << "请输入滑动窗口大小（4-32）：";
		cin >> windows;
		if (windows <= 32 && windows>=4) {
			break;
		}else {
			cout << "滑动窗口大小有错误，请重新输入！"<<endl;
			continue;
		}
	}
	slidewindows = windows;
	cout << "请输入本机IP地址：";
	cin >> ip;
	cout << "请输入端口号：";
	cin >> port;
	server_addr.sin_family = AF_INET;//用于指定地址族。地址族确定了该地址是 IPv4 还是 IPv6。AF_INET表示 IPv4 地址族，AF_INET6表示 IPv6 地址族
	server_addr.sin_addr.S_un.S_addr = inet_addr(ip);//IP地址
	server_addr.sin_port = htons(port);// htons()将整型变量从主机字节顺序转变成网络字节顺序，填入端口号
	s_server = socket(AF_INET, SOCK_DGRAM, 0);//创建一个基于 IPv4 的数据报套接字，适用于 UDP 协议
	string filename;
	while (filename!="exit")
	{
		//三次握手建立连接
		threehands(s_server, server_addr);
		if (handssuccess == 0){
			cout << "【连接失败即将重试】" << endl;
			continue;
		}
		//传输文件
		cout << "输入文件名为exit时退出程序"<<endl;
		cout << "请输入要传输的文件名：" << endl;
		cin >> filename;

		ifstream in(filename, ifstream::in | ios::binary);//以二进制方式打开文件
		if (!in) {
			if (filename == "exit") {
				cout << "【退出程序】" << endl;
				fourbye(s_server, server_addr);
				return 0;
			}
			cout << "【错误！检查文件是否存在】" << endl;
			return false;
		}
		//传输文件名
		sendfile(s_server, server_addr, (char*)(filename.data()), filename.length());
		/*if (sendsuccess == 0) {
			return 0;
		}*/
		clock_t starttime = clock();
		int bytesRead;
		char* buff = new char[100000000];
		if (in.is_open()) {
			in.read(buff, 100000000);
			bytesRead = in.gcount(); // 获取实际读取的字节数
			// 处理 bytesRead 字节的数据
		}
		else {
			// 文件打开失败的处理
			cout << "【文件读取出错】" << endl;
			return 0;
		}
		//传输文件
		sendfile(s_server, server_addr, buff, bytesRead);
		clock_t end = clock();

		//计算性能
		cout << "【文件名称】=" << filename << endl;
		cout << "【文件大小】 =" << ((double)bytesRead) / 1024 << "KB" << endl;
		cout << "【传输总时间】=" << end - starttime << "ms" << endl;
		cout << "【吞吐率】 =" << ((double)bytesRead) / (double(end - starttime)) << "byte/ms" << endl;

		//四次挥手断开连接
		fourbye(s_server, server_addr);
	}

	WSACleanup();
	return 0;
	
}
