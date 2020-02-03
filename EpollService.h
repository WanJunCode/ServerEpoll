#ifndef EPOLL_SERVER

#include "Service.h"
#include <sys/epoll.h>
#include <netinet/in.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <map>

class ClockTimer{
    typedef void(*TimerCallback)(void *arg);
public:
    ClockTimer(int second,TimerCallback callback=NULL){
        second_ = second;
        fd_ = timerfd_create(CLOCK_REALTIME,0);
        cb_ = callback;

        timespec &now = getNow();
        new_value.it_value.tv_sec = now.tv_sec + second;
        new_value.it_value.tv_nsec = now.tv_nsec;
        new_value.it_interval.tv_sec = second;     //之后的定时间隔
        timerfd_settime(fd_, TFD_TIMER_ABSTIME, &new_value, NULL);
    }

    ~ClockTimer(){
        close(fd_);
    }

    int getFd() const{
        return fd_;
    }

    TimerCallback getCallback() const{
        return cb_;
    }
private:

    static struct timespec &getNow(){
        static struct timespec now;
        clock_gettime(CLOCK_REALTIME,&now);
        return now;
    }

private:
    int fd_;
    int second_;
    struct itimerspec new_value;
    TimerCallback cb_;
};

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
    std::map<int , ClockTimer *>   timerMap; 

    int receiveEpollfd;         // epoll fd for receive loop thread
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
