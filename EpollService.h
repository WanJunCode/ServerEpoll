#ifndef EPOLL_SERVER
#define EPOLL_SERVER

#include "Service.h"
#include "ClockTimer.h"
#include <sys/epoll.h>
#include <netinet/in.h>
#include <map>

// listener  监听者
// 客户端处理器
// 定时器处理器

class EpollService :public Service
{
private:
    int listenEpollfd;          // epoll fd
    int listenfd;               // listener

    int port_;
    int backlog_;
    struct epoll_event server_ev;
    struct sockaddr_in svraddr;
    bool service_;              // state for server

    int pipe_[2];               // pipe for transport pipemsg_t

    pthread_t receiveTrd;       // thread id for accept thread
    pthread_t timerTrd;
    bool receiveLoopStart;      // state for starting receive loop thread
    bool timerLoopStart;        // state for starting timer loop thread
    
    // container for accept clients
    std::map<int , clientInfo_t> fd_info;
    std::map<int , ClockTimer *> timerMap; 

    int receiveEpollfd;         // epoll fd for receive loop thread 接收从管道传输的客户端套接字，接收来自各客户端套接字的数据
    int timerEpollfd;

    int oneshut_;               // if use one shut method on client fd

public:
    explicit EpollService(int port,int backlog = 10,bool oneshut = false);
    ~EpollService();
    void startService() override;
    void stopService() override;

private:
    bool createListen();
    bool reListen();
    bool addClient();
    void readClient(int clientfd);

private:
    bool startReceiveThread();
    bool startTimerThread();
    static void *ReceiveLoop(void *args);
    static void *TimerLoop(void *args);
};

#endif // !EPOLL_SERVER
