#ifndef EPOLL_SERVER

#include "Service.h"
#include <sys/epoll.h>
#include <netinet/in.h>
#include <map>

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
    bool receiveLoopStart;      // state for starting receive loop thread

    // container for accept clients
    std::map<int , clientInfo_t> fd_info;

    int receiveEpollfd;         // epoll fd for receive loop thread

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
    static void *ReceiveLoop(void *args);
};

#endif // !EPOLL_SERVER
