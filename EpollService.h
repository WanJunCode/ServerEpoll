#ifndef EPOLL_SERVER

#include "Service.h"
#include <sys/epoll.h>
#include <netinet/in.h>
#include <map>

class EpollService :public Service
{
private:
    int listenEpollfd;
    int listenfd;
    int port_;
    int backlog_;
    struct epoll_event server_ev;
    struct sockaddr_in svraddr;
    bool service_;

    int pipe_[2];
    pthread_t receiveTrd;
    bool receiveLoopStart;
    std::map<int , clientInfo_t> fd_info;

    int receiveEpollfd;

public:
    explicit EpollService(int port,int backlog = 10);
    ~EpollService();
    void startService() override;
    void stopService() override;

private:
    bool createListen();
    bool reListen();

private:
    bool startReceiveThread();
    static void *ReceiveLoop(void *args);
};

#endif // !EPOLL_SERVER
