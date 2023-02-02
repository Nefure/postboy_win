#include "diplomat.h"

#include<qdebug>
#include<thread>
#include<cstring>
#include<iostream>
#include <sys/stat.h>
#include<stdio.h>
#include<qstring.h>

#define  MAGIC_STR "boki"

#define TCP_SERVER_PORT "31415"

#define MAX_CONN 16

Diplomat::Diplomat(): status(CLOSED), hosts(), rwFlag(WRITING)

{

    WSADATA data;

    u_short port = htons(8080);

    //启用windows异步套接字2.2版本
    if (WSAStartup(MAKEWORD(2, 2), &data))
    {
        qDebug() << "udpSocket start failed !" << endl;
    }

    udpSocket_sender = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    //关闭无效udp套接字
    if (INVALID_SOCKET == udpSocket_sender)
    {
        qDebug() << "socket is INVALID_SOCKET !" << endl;
        //功能是终止Winsock 2 DLL (Ws2_32.dll) 的使用
        WSACleanup();
    }
    //设置广播权限
    BOOL bBroadcast = TRUE;
    setsockopt(udpSocket_sender, SOL_SOCKET, SO_BROADCAST, (const char*)&bBroadcast, sizeof(bBroadcast));
    addr_dst.sin_family = AF_INET;
    addr_dst.sin_port = port;
    addr_dst.sin_addr.S_un.S_addr = inet_addr("255.255.255.255");
    status.store(REFRESHED);
}

Diplomat::~Diplomat()
{
    closesocket(udpSocket_sender);

    WSACleanup();
}

// 通过stat结构体 获得文件大小，单位字节
size_t getFileSize(const char* fileName)
{

    if (fileName == NULL)
    {
        return 0;
    }

    // 这是一个存储文件(夹)信息的结构体，其中有文件大小和创建时间、访问时间、修改时间等
    struct stat statbuf;

    // 提供文件名字符串，获得文件属性结构体
    stat(fileName, &statbuf);

    // 获取文件大小
    size_t filesize = statbuf.st_size;

    return filesize;
}

bool isExists(const char* name)
{
    if (FILE* file = fopen(name, "r"))
    {
        fclose(file);
        return true;
    }
    else
    {
        return false;
    }
}

int byteToInt(byte a, byte b, byte c, byte d)
{
    int rt = 0;
    rt |= (a << 24);
    rt |= (b << 16);
    rt |= (c << 8);
    rt |= d;
    return rt;
}

void intToBytes(char* bytes, int val)
{
    bytes[3] = val & 255;
    bytes[2] = (val >> 8) & 255;
    bytes[1] = (val >> 16) & 255;
    bytes[0] = (val >> 24) & 255;
}

int sendMsg(SOCKET& socket, char type, const char* _data, int len)
{
    //编码
    // boki(4) +  长度(4) + type(1) +字符数组（内容）
    QByteArray bytes;
    bytes.append(MAGIC_STR);
    char length[4] = { 0 };
    intToBytes(length, len + 1);
    bytes.append(length, 4);
    bytes.append(type);
    bytes.append(_data, len);

    //发送
    return send(socket, bytes.constData(), bytes.size(), 0);
}

void carryFile(SOCKET socket, Msg* msg)
{
    QString tmpstr(QString::fromUtf8(msg->data));
    QByteArray tmpbytes = tmpstr.toLocal8Bit();
    const char* filePath = tmpbytes.constData();

    FILE* file =  fopen(filePath, "rb");

    if (NULL == file)
    {
        sendMsg(socket, EMPTY, "can't open the file!", 20);
        return;
    }

    size_t total = getFileSize(filePath);


    //发送文件长度：
    int rst = 0, st = 0;

    //处理中文文件名或路径
    tmpstr = QString::number(total, 10);
    tmpbytes = tmpstr.toUtf8();
    sendMsg(socket, SEND, tmpbytes.constData(), tmpbytes.size());

    const int LEN = 5 * 1024 * 1024;
    char* buf = new char[LEN];
    Msg tmp;
    total = 0;
    do
    {
        rst = fread(buf, sizeof(char), LEN, file);
        st = sendMsg(socket, SEND, buf, rst);
        total += rst;
    }
    while (rst == LEN && st > 0);

    fclose(file);
    tmpbytes = QString::number(total).toUtf8();
    sendMsg(socket, FINISHED, tmpbytes.constData(), tmpbytes.size());
    delete []buf;
}

bool Diplomat::checkMsg(const char* msg)
{
    const char* tmp = MAGIC_STR;
    for (int i = 0; i < strlen(MAGIC_STR); i++)
    {
        if (msg[i] != tmp[i])
        {
            return false;
        }
    }
    return true;
}

void Diplomat::getIpv4Str(char* dst, unsigned long ip)
{
    dst[0] = 0;
    inet_ntop(AF_INET, &ip, dst, INET_ADDRSTRLEN);
}

Host Diplomat::toHostInfo(const char* msg, const unsigned long& ip)
{
    return {ip, GetTickCount64(), QString::fromUtf8(msg)};
}

void Diplomat::search(Diplomat* diplomate, SOCKET udpSocket_receiver)
{

    const int LEN = 512;
    char msg[LEN] = { 0 };
    SOCKADDR_IN addr_cli {};
    int len = sizeof(addr_cli);

    bool isnew;
    std::atomic_int& status = diplomate->status;
    std::vector<Host>& hosts = diplomate->hosts;
    do
    {
        recvfrom(udpSocket_receiver, msg, LEN, 0, (SOCKADDR*) & addr_cli, &len);

        if (diplomate->checkMsg(msg))
        {
            isnew = true;

            Host tmp = diplomate->toHostInfo(msg + strlen(MAGIC_STR), addr_cli.sin_addr.S_un.S_addr);

            for (int i = 0; i < hosts.size(); i++)
            {
                if (hosts[i].addr == tmp.addr)
                {
                    isnew = false;
                    hosts[i].name = tmp.name;
                    hosts[i].lastAweak = tmp.lastAweak;
                    break;
                }
            }
            if (isnew)
            {
                hosts.push_back(tmp);
            }
        }         //更新主机列表拷贝

        if (diplomate->rwFlag.load() == WRITING)
        {
            std::vector<Host>& dst = diplomate->hostCopy;
            dst.clear();
            auto idx = hosts.begin();
            auto cur = GetTickCount64();
            while (idx != hosts.end())
            {
                if (cur - (*idx).lastAweak < 300000)
                {
                    dst.push_back((*idx));
                }
                idx++;
            }
            hosts.clear();
            hosts.assign(dst.begin(), dst.end());

            diplomate->rwFlag.store(REFRESHED);
        }
    }
    while (CLOSING_SEARCHER != status.load());

    closesocket(udpSocket_receiver);
    status.store(CLOSING_SERVER);
}


void Diplomat::serve(Diplomat* diplomate, SOCKET serverSocket)
{

    //用于限制最大连接数
    std::atomic_int limiter(0);

    SOCKADDR_IN addr {};
    int len = sizeof(addr);
    unsigned long ip;

    Msgbox* idxbox = NULL;
    Msgbox* box = NULL;

    while (CLOSING_SERVER  != diplomate->status.load())
    {
        //达到最大连接后等10ms继续尝试
        if (limiter.load() >= MAX_CONN)
        {
            Sleep(10);
            continue;
        }
        limiter.fetch_add(1);
        SOCKET clientSocket = INVALID_SOCKET;
        clientSocket = accept(serverSocket, (SOCKADDR*) & addr, &len);
        if (INVALID_SOCKET == clientSocket)
        {
            qDebug() << "accept failed! :" << WSAGetLastError() << endl;
            continue;
        }

        //连接成功后，更新对应连接的信箱链表，链表头不存储数据
        ip = addr.sin_addr.S_un.S_addr;
        idxbox = &diplomate->firstbox;

        while (true)
        {
            if (ip == idxbox->addr)
            {
                box = idxbox;
                break;
            }
            if (idxbox->next.load() == NULL)
            {
                box = new Msgbox();
                box->addr = ip;
                idxbox->next.store(box);
                break;
            }
            idxbox = idxbox->next.load();
        }

        std::thread worke(doserve, diplomate, clientSocket, box, &limiter);
        worke.detach();
    }


    closesocket(serverSocket);
}

const char* recvAndDecode(Msg*& msg, SOCKET clientSocket, char* buf, const int LEN, QByteArray& bytes)
{
    int deta = 0, length = -1;
    const  int MAGIC_LEN = strlen(MAGIC_STR);
    bool uncheck = true;
    INT rst;
    while (true)
    {
        if (uncheck && bytes.size() >= MAGIC_LEN)
        {
            if (bytes.startsWith(MAGIC_STR))
            {
                deta += MAGIC_LEN;
                uncheck = false;
            }
            else
            {
                bytes.clear();
                continue;
            }
        }

        if ((!uncheck) && length == -1 && (bytes.size() >= 4 + deta))
        {
            length = byteToInt(bytes[deta + 0], bytes[deta + 1], bytes[deta + 2], bytes[deta + 3]);
            deta += 4;
        }

        if ((!uncheck) && length != -1 && (length + deta <= bytes.size()))
        {
            const char* data = bytes.constData();
            int type = data[deta++];
            msg = new Msg(type, &data[deta], length - 1);
            bytes.remove(0, deta + length - 1);
            buf[0] = 0;
            return NULL;
        }

        if ((rst = recv(clientSocket, buf, LEN, 0)) < 0)
        {
            return "error when recieve request!";
        }
        if (rst == 0)
        {
            return "conn closed !";
        }
        bytes.append(buf, rst);

    };
    return NULL;
}

void Diplomat::doserve(Diplomat* diplomat, SOCKET clientSocket, Msgbox* msgbox, std::atomic_int* limiter)
{

    //消息格式 boki(4) +  长度(4) + type(1) +字符数组（内容）
    int rst, length = -1, deta = 0;
    bool uncheck = true;
    const int MAGIC_LEN = strlen(MAGIC_STR);
    const int LEN = 2048;
    char buf[LEN] = { 0 };
    QByteArray bytes;
    Msg* tail = msgbox->list.load();
    const char* excep = NULL;
    Msg* msg = NULL;
    while (true)
    {

        excep = recvAndDecode(msg, clientSocket, buf, LEN, bytes);

        if (excep != NULL)
        {
            qDebug() << excep<<"381" << endl;
            break;
        }
        if (msg->type == REQUEST)
        {
            carryFile(clientSocket, msg);
        }
        else if (tail == NULL && msgbox->list.compare_exchange_strong(tail, msg))
        {
            msgbox->list.store(msg);
            tail = msg;
        }
        else
        {
            Msg* tmp;
            do
            {
                while ((tmp = tail->next.load()))
                {
                    tail = tail->next.load();
                }
            }
            while (!tail->next.compare_exchange_weak(tmp, msg));
            tail = msg;
        }

        //服务端仅处理request请求（即请求下载具体文件）
        //其他消息挂到信箱里交给ui线程显示

        deta = 0;
        uncheck = true;
        length = -1;
    } ;

    limiter->fetch_sub(1);
    closesocket(clientSocket);
}

const char* Diplomat::listenStart(u_short udp_port, u_short)
{
    //udp收听部分初始化

    SOCKET udpSocket_receiver;
    udpSocket_receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (INVALID_SOCKET == udpSocket_receiver)
    {
        return "receiverSocket is INVALID_SOCKET !";
    }
    //广播来源套接字信息
    SOCKADDR_IN addr_src {};
    addr_src.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
    addr_src.sin_family = AF_INET;
    addr_src.sin_port = udp_port;

    if (bind(udpSocket_receiver, (SOCKADDR*)&addr_src, sizeof(SOCKADDR)) == SOCKET_ERROR)
    {
        return "udp receiver: socket_error !";
    }

    //启动搜索线程
    std::thread searcher(search, this, udpSocket_receiver);
    searcher.detach();

    //tcp服务端

    addrinfo* res = NULL, hints;
    ZeroMemory(&hints, sizeof(addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    //这个设置表示会在bind函数里用到，此时如果调getaddrinfo把nodename填NULL，得到的套接字ip会是INADDR_ANY或者IN6ADDR_ANY_INIT
    hints.ai_flags = AI_PASSIVE;

    SOCKET serverSocket = INVALID_SOCKET;
    INT rst = getaddrinfo(NULL, TCP_SERVER_PORT, &hints, &res);
    if (rst)
    {
        return "getaddrinfo failed! ";
    }

    serverSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (INVALID_SOCKET == serverSocket)
    {
        return "init socket failed! ";
    }

    rst = bind(serverSocket, res->ai_addr, (int)res->ai_addrlen);
    if (SOCKET_ERROR == rst)
    {
        closesocket(serverSocket);
        return "bind serverSocket failed! " ;
    }

    //用完拿到的地址释放
    freeaddrinfo(res);

    //监听套接字
    if (SOCKET_ERROR == listen(serverSocket, SOMAXCONN))
    {
        closesocket(serverSocket);
        return "listen failed! " ;
    }

    //启动发件服务
    std::thread server(serve, this, serverSocket);
    server.detach();
    return NULL;
}

void Diplomat::shutdownlisten()
{
    int tmp = REFRESHED;
    status.compare_exchange_strong(tmp, CLOSING_SEARCHER);
}


const char* Diplomat::callOut(const QString& name)
{

    if (status.load() < 0)
    {
        return NULL;
    }
    QByteArray bytes = name.toUtf8();
    char* buf =  bytes.data();
    const int len = strlen(buf) + strlen(MAGIC_STR) + 1;
    char* msg = new char[len];
    strcpy(msg, MAGIC_STR);
    strcat(msg, buf);
    if (sendto(udpSocket_sender, msg, len, 0, (SOCKADDR*)&addr_dst,
               sizeof(addr_dst)) == SOCKET_ERROR)
    {
        return "socket error !";
    }
    delete []msg;
    return NULL;
}

void Diplomat::refreshHosts()
{
    int tmp = AVIALIABLE;
    rwFlag.compare_exchange_strong(tmp, WRITING);
}

void Diplomat::requestf(unsigned long ip, QString path, QString to, std::atomic_int* process)
{
    SOCKET socket;
    const char* excep = prepareTcp(ip, socket);
    if (excep != NULL)
    {
        process->store(-2);
        return;
    }

    std::thread download([ = ]()
    {
        SOCKET connSocket = socket;
        QByteArray tmpbytes = path.toUtf8();
        INT rst = sendMsg(connSocket, REQUEST, tmpbytes.constData(), tmpbytes.size());

        if (SOCKET_ERROR == rst)
        {
            process->store(-2);
            return;
        }
        QString tmpstr = to;
        tmpstr.append(path.mid(path.lastIndexOf('/'))).append(".downloading");
        tmpbytes = tmpstr.toLocal8Bit();
        int cnt = -1;
        while (isExists(tmpbytes.constData()))
        {
            int idx = tmpstr.lastIndexOf('g');
            tmpstr.remove(idx, tmpstr.size() - idx);
            tmpstr.append(QString::number(cnt++));
            tmpbytes = tmpstr.toLocal8Bit();
        }
        FILE* fp =  fopen(tmpbytes.constData(), "wb");
        if (fp == NULL)
        {
            return;
        }

        Msg* msg = NULL;
        const int LEN = 5 * 1024 * 1024 + 1024;
        char* buf = new char[LEN];
        QByteArray bytes;
        const char* excep = recvAndDecode(msg, socket, buf, LEN, bytes);
        if (excep != NULL)
        {
            qDebug() << excep << endl;
            delete[]buf;
            return;
        }

        if (msg->type == EMPTY)
        {
            qDebug() << msg->data << endl;
            delete[]buf;
            delete msg;
            return;
        }

        tmpstr = QString::fromUtf8(msg->data);
        size_t size = tmpstr.toLongLong();

        delete msg;
        msg = NULL;
        size_t has = 0;
        while ((excep = recvAndDecode(msg, socket, buf, LEN, bytes)) == NULL && msg->type != FINISHED)
        {
            fwrite(msg->data.constData(), sizeof(char), msg->data.size(), fp);
            fflush(fp);
            has += msg->data.size();
            process->store((has * 100) / size);
            delete msg;
            msg = NULL;
        }
        fclose(fp);
        delete []buf;

        closesocket(socket);
        if (excep != NULL)
        {
            qDebug() << excep << endl;
            return;
        }
        delete msg;
        cnt = path.lastIndexOf('/');
        QString name = to + path.mid(cnt, path.lastIndexOf(".") - cnt);
        QString suffix = path.mid(path.lastIndexOf('.'));
        QString newname = name + suffix;
        bytes = newname.toLocal8Bit();
        cnt = 0;

        while (isExists(bytes.constData()))
        {
            newname = name;
            newname.append('(').append(QString::number(++cnt)).append(')').append(suffix);
            bytes = newname.toLocal8Bit();
        }

        bool renamed = rename(tmpbytes.constData(), bytes.constData()) == 0;

        if (renamed)
        {
            process->store(-4);
        }
        else
        {
            process->store(-3);
        }
    });

    download.detach();
}

const char* Diplomat::sendf(unsigned long ip, QString& path)
{
    SOCKET connSocket;
    const char* excep = prepareTcp(ip, connSocket);
    if (excep)
    {
        return excep;
    }
    INT rst;

    //连接成功就能和服务端通信了
    QByteArray bytes = path.toUtf8();
    const char* msg = bytes.constData();
    rst = sendMsg(connSocket, SEND, msg, bytes.size());
    if (SOCKET_ERROR == rst)
    {
        excep = "send failed! ";
    }


    closesocket(connSocket);
    return excep;
}

const char* Diplomat::prepareTcp(unsigned long ip, SOCKET& connSocket)
{
    addrinfo* res = NULL,
              * ptr = NULL,
                hints;
    ZeroMemory(&hints, sizeof(hints));

    //含义为‘未指定’，兼容IP v 4与IP v 6的地址族
    hints.ai_family = AF_UNSPEC;
    //使用提供基于连接的有序双向字节流的sock类型
    //此设置将tcp用于当前Internet 地址族（AF_INET 或 AF_INET6）
    hints.ai_socktype = SOCK_STREAM;
    //指定tcp协议
    hints.ai_protocol = IPPROTO_TCP;
    //使用getaddrinfo填写地址信息到res
    char ipstr[INET_ADDRSTRLEN];
    getIpv4Str(ipstr, ip);
    INT rst = getaddrinfo(ipstr, TCP_SERVER_PORT, &hints, &res);
    if (rst)
    {
        return "getaddrinfo failed";
    }
    connSocket = INVALID_SOCKET;
    ptr = res;
    connSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    //检查套接字是否有效
    if (connSocket == INVALID_SOCKET)
    {
        freeaddrinfo(res);
        return "Error at socket()!";
    }


    //连接到服务器,
    //失败时尝试下一个由getaddrinfo填写的地址信息
    addrinfo* tmp = ptr;
    do
    {
        ptr = tmp;
        rst = connect(connSocket, ptr->ai_addr, ptr->ai_addrlen);
    }
    while ((tmp = ptr->ai_next) != NULL && rst == SOCKET_ERROR);
    if (rst == SOCKET_ERROR)
    {
        closesocket(connSocket);
        connSocket = INVALID_SOCKET;
    }

    //拿到的地址信息用完释放
    freeaddrinfo(res);

    if (connSocket == INVALID_SOCKET)
    {
        return "unable to connect !";
    }
    return NULL;
}