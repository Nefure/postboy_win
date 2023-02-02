#pragma once

#include <winsock2.h>
//Ws2tcpip.h 头文件包含 WinSock 2 Protocol-Specific TCP/IP 的附件文档中引入的定义，其中包含用于检索 IP 地址的较新的函数和结构
#include <ws2tcpip.h>
#include<vector>
#include<qstring.h>
#include<atomic>

#pragma comment(lib, "Ws2_32.lib")

//status变量
//开始关闭,in:0
#define CLOSING_SEARCHER -3
//正在关闭,in:-3
#define CLOSING_SERVER -2
//已关闭,in:-2
#define CLOSED -1
//正常工作 , in : -1 , 1 , 2
#define REFRESHED 0

//readable变量
//正在更新,in: 0 , 此时仅搜索线程可修改状态
#define WRITING 1
//正在被某线程读,in:0，仅读线程可修改状态
#define READING 2
//普通状态
#define AVIALIABLE 0

struct Host
{
    unsigned long addr = 0;

    unsigned long long lastAweak = 0ll;

    QString name = "";
};

struct Msg
{
#define SEND 11
#define REQUEST  45
    //服务端发送表示拒绝，并将原因写入data
#define EMPTY 14
#define FINISHED 80
    int type = EMPTY;
    std::atomic<Msg*> next = NULL;
    QByteArray data;

    Msg(){}

    Msg(int _type, const char* _data, int len):type(_type),next(NULL),data(_data,len)
    {
    }
    ~Msg()
    {
        Msg* nextMsg = next.load();
        Msg* msg = nextMsg;
        if (msg && next.compare_exchange_strong(msg,NULL))
        {
            delete nextMsg;
        }
    }
};

struct Msgbox
{
    unsigned long addr = 0;
    std::atomic<Msg*> list = NULL;
    std::atomic<Msgbox*> next = NULL;
    ~Msgbox()
    {
        Msg* head = list.load();
        Msg* msg = head;
        if (msg && list.compare_exchange_strong(msg,NULL))
        {
            delete head;
        }
        Msgbox* nextbox = next.load();
        Msgbox* box = nextbox;
        if (box && next.compare_exchange_strong(box,NULL))
        {
            delete nextbox;
        }
    }
};

class Diplomat
{

    //广播目标套接字信息
    SOCKADDR_IN addr_dst;
    SOCKET udpSocket_sender;

    std::atomic_int status;

    std::atomic_int rwFlag;

    std::vector<Host> hosts;

    Host toHostInfo(const char* msg,const unsigned long & ip);

    static void search(Diplomat* diplomat, SOCKET udpSocket_receiver);

    static void serve(Diplomat* diplomat,SOCKET serverSocket);

    static void doserve(Diplomat* diplomat,SOCKET clientSocket, Msgbox* msgbox, std::atomic_int* limiter);

public:

    Msgbox firstbox;

    std::vector<Host> hostCopy;

    Diplomat();

    ~Diplomat();

    bool read()
    {
        int tmp = AVIALIABLE;
        return rwFlag.compare_exchange_strong(tmp,READING);
    }

    void readFinish()
    {
        int tmp = READING;
        rwFlag.compare_exchange_strong(tmp,AVIALIABLE);
    }

    bool refresh()
    {
        int tmp = AVIALIABLE;
        return rwFlag.compare_exchange_strong(tmp, WRITING);
    }

    int getRwFlag()
    {
        return rwFlag.load();
    }

    bool checkMsg(const char* msg);

    void getIpv4Str(char* dst, unsigned long ip);

    void toHostName(QString& res, const QString& name, const unsigned long& ip)
    {
        res.append(name);
        res.append(":[");
        char tmp[INET_ADDRSTRLEN];
        getIpv4Str(tmp, ip);
        res.append(tmp);
        res.append("]");
    }

    unsigned long getIp()
    {
        char hostName[256];
        if (!gethostname(hostName, sizeof(hostName)))
        {
            hostent* host = gethostbyname(hostName);

            return *(unsigned long*)(*host->h_addr_list);
        }
        return 0;
    }

    /**
    * 监听传到端口的消息
    */
    const char* listenStart(u_short udp_port = htons(8080), u_short tcp_port = htons(8081));
    /**
    * 停止监听端口
    */
    void shutdownlisten();
    /**
    * 向其他主机广播自己的消息
    */
    const char* callOut(const QString& name);
    /**
    * 刷新主机列表
    */
    void refreshHosts();
    /**
    * 向目标主机请求指定资源
    */
    void requestf(unsigned long ip,QString path, QString to,std::atomic_int* proccess);
    /**
    * 发送对应资源
    */
    const char* sendf(unsigned long ip,QString& path);

    const char* prepareTcp(unsigned long ip, SOCKET& socket);
};

