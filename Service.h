#ifndef WANJUN_SERVICE_H
#define WANJUN_SERVICE_H
#include <netinet/in.h>

class Service{
protected:
#pragma pack(1)
    typedef struct clientInfo{
        sockaddr_in addr;
        unsigned int contime;    //最后连接时间
        unsigned int rcvtime;    //收到数据时间
        unsigned int rcvbyte;    //收到字节个数
        unsigned int sndtime;    //发送数据时间
        unsigned int sndbyte;    //发送字节个数
    } clientInfo_t;
    typedef struct pipemsg{
        unsigned int op;
        int fd;
        sockaddr_in addr;
    } pipemsg_t;
#pragma pack()
    int stdin_;
public:
    Service() {}
    virtual ~Service() {}

    virtual void startService() = 0; 
    virtual void stopService() = 0; 
};

#endif