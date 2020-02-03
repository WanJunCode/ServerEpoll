#ifndef __MYTIMER_H_
#define __MYTIMER_H_

/***************
高并发场景下的定时器
*****************/

//定时器回调函数
typedef void *(*TimerCallback)(int fd, void *);

typedef enum enFD_TYPE
{
    FD_TIMER,
    FD_SOCKET,
    FD_FILE,
}ENFD_TYPE;

struct STEpollParam
{
    int fd;                  //文件描述符
    ENFD_TYPE enType;        //文件描述符类型
    TimerCallback cb;        //定时器回调函数
    void * pvParam;          //回调函数参数
};

//创建定时器对象
int createTimer(unsigned int uiSec, unsigned int uiNsec);

//设置文件描述符非阻塞
int setNoBlock(int fd);

//创建epoll
int createEpoll();

//添加文件描述符到epoll
int addFdToEpoll(int epfd, STEpollParam *pstParam);

//消息处理
int recvMsg(void *pvParam);

//等待消息
void epollWait(int epfd);

#endif