#include "EpollService.h"
#include "Tool.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> // inet_ntoa
#include <errno.h>
#include <pthread.h>
#include <assert.h>

EpollService::EpollService(int port, int backlog, bool oneshut)
    : port_(port),
      backlog_(backlog),
      service_(false),
      receiveLoopStart(false),
      timerLoopStart(false),
      oneshut_(oneshut)
{
    listenEpollfd = epoll_create(1024);
    receiveEpollfd = epoll_create(1024);
    timerEpollfd = epoll_create(1024);

    // 为监听fd设置处理函数，接受来自键盘的输入事件
    stdin_ = STDIN_FILENO;
    epoll_event stdin_ev;
    stdin_ev.data.fd = stdin_;
    stdin_ev.events = EPOLLIN;
    epoll_ctl(listenEpollfd, EPOLL_CTL_ADD, stdin_, &stdin_ev);
    
    // create pipe for transpotr client message
    pipe(pipe_);

    if(false == createListen()){
        printf("EpollService create listener failure.\n");
        exit(EXIT_FAILURE);
    }
}

EpollService::~EpollService()
{
    if(receiveLoopStart == true){
        printf("join receive thread\n");
        if(0 !=pthread_join(receiveTrd,NULL)){
            printf("join reveive thread fail\n");
        }
    }
    
    if(timerLoopStart == true){
        printf("join timer thread\n");
        if(0 != pthread_join(timerTrd,NULL)){
            printf("join timer thread fail\n");
        }
    }

    for(auto iter = fd_info.begin();iter != fd_info.end();iter++){
        close(iter->first);
        printf("close [%d]\n",iter->first);
    }

    // delete ClockTimer
    for(auto iter = timerMap.begin();iter != timerMap.end();iter++){
        delete iter->second;
        printf("delete timer\n");
    }

    //关闭监听描述字
    if (listenfd > 0){
        close(listenfd);
    }

    //关闭创建的epoll
    if (listenEpollfd > 0){
        close(listenEpollfd);
    }
}

// 创建新的listenfd套接字并监听新的连接
// 如果创建失败会关闭 listenEpollfd
bool EpollService::createListen()
{
    assert(listenEpollfd > 0);// 监听 epoll fd 必须可用
    //创建监听 tcp socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0){
        printf("create Listen socket fail\n");
        close(listenEpollfd);
        return false;
    }

    //把监听socket设置为非阻塞方式
    setnonblocking(listenfd);
    //设置监听socket为端口重用
    setreuseaddr(listenfd);
    server_ev.data.fd = listenfd;
    //使用 ET边沿触发模式处理监听事件(相比水平模式更加复杂但是高效)
    server_ev.events = EPOLLIN | EPOLLET;
    if (0 != epoll_ctl(listenEpollfd, EPOLL_CTL_ADD, listenfd, &server_ev)){
        printf("createListen, epoll_ctl fail: listenEpollfd [%d] listenfd [%d]\n", listenEpollfd, listenfd);
        close(listenfd);
        close(listenEpollfd);
        return false;
    }

    bzero(&svraddr, sizeof(svraddr));
    svraddr.sin_family = AF_INET;
    svraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    svraddr.sin_port = htons(port_);
    bind(listenfd, (sockaddr *)&svraddr, sizeof(svraddr));
    //监听,准备接收连接
    if (0 != listen(listenfd, backlog_)){
        printf("createListen, listen fail\n");
        close(listenfd);
        close(listenEpollfd);
        return false;
    }
    return true;
}

void EpollService::startService()
{
    service_ = true;

    // start receive thread loop
    if(startReceiveThread()){
        receiveLoopStart = true;
    }else{
        printf("start receive thread fail\n");
        exit(EXIT_FAILURE);
    }

    // start timer thread loop
    if(startTimerThread()){
        timerLoopStart = true;
    }else{
        printf("start timer thread fail\n");
        exit(EXIT_FAILURE);
    }

    printf("start epoll server service\n");
    const int MAXEVENTS = 5;                //最大事件数(监听事件、键盘输入事件)
    struct epoll_event events[MAXEVENTS];   //监听事件数组
    socklen_t clilen = sizeof(struct sockaddr);
    struct sockaddr_in cliaddr;
    const char *defaultResponse = "this is message from epoll server";
    // listen fd main loop 只处理监听事件和键盘输入事件
    while (service_)
    {
        int nfds = epoll_wait(listenEpollfd, events, MAXEVENTS, -1);
        for (int i = 0; i < nfds && service_; ++i)
        {
            if (events[i].data.fd == listenfd) //是本监听socket上的事件
            {
                printf("AcceptThread, events: [%d]\n", events[i].events);
                if (events[i].events & EPOLLIN) //有新的客户端连接到来
                {
                    // 在ET模式下需要使用loop accept一直到EAGAIN
                    do{
                        // 获得新连接的套接字 connfd， 套接字相关信息存储在 cliaddr
                        int connfd = accept(listenfd, (sockaddr *)&cliaddr, &clilen);
                        if (connfd > 0){
                            printf("AcceptThread, accept: [%d],connect: [%s]:[%d]\n", connfd, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
                            // 传递
                            static pipemsg_t msg;
                            msg.op = 0;
                            msg.fd = connfd;
                            msg.addr = cliaddr;
                            write(pipe_[1],&msg,sizeof(msg));
                            write(connfd, defaultResponse, strlen(defaultResponse));
                        }else{
                            printf("AcceptThread, accept fail: connfd = [%d]\n", connfd);
                            if (errno == EAGAIN) //没有连接需要接收了, ET模式下需要一直读取带 EAGAIN
                            {
                                break;
                            }
                            else if (errno == EINTR) //可能被中断信号打断,经过验证对非阻塞socket并未收到此错误,应该可以省掉该步判断
                            {
                                ;
                            }
                            else //其它情况可以认为该套接字出现错误,应该关闭后重新监听
                            {
                                // 如果重启失败，则退出
                                if(false == reListen()){
                                    service_ = false;
                                    return;//退出监听事件循环，等待其他线程结束后关闭程序
                                }
                            }
                        }
                    } while (service_);
                    // end loop accept

                }else if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP){
                    printf("listenfd error , need to restart\n");
                    if(false == reListen()){
                        service_ = false;
                        return;
                    }
                }
            }// end listenfd if

            // 处理标准输入事件
            if(events[i].data.fd == stdin_){
                char buffer[80];
                bzero(buffer,sizeof(buffer));
                int length = read(stdin_,buffer,sizeof(buffer));
                buffer[length-1] = '\0';
                if(strncmp(buffer,"over",4) == 0){
                    service_ = false;
                    printf("from stdin read over\n");
                    stopService();
                }else if(strncmp(buffer,"list",4)==0){
                    printf("list begin\n");
                    for(auto iter=fd_info.begin();iter!=fd_info.end();++iter){
                        printf("infomation: [%d] [%s]:[%d]\n",iter->first,inet_ntoa(iter->second.addr.sin_addr),ntohs(iter->second.addr.sin_port));
                    }
                    printf("list end\n");
                }else if(strncmp(buffer,"time",4)==0){
                    printf("set timer\n");
                    // 添加定时器到　timerEpollfd
                    ClockTimer *timer = new ClockTimer(2);
                    timerMap[timer->getFd()] = timer;//add to map
                    epoll_event timerEv;
                    timerEv.data.fd = timer->getFd();
                    timerEv.events = EPOLLIN;
                    int ret = epoll_ctl(timerEpollfd,EPOLL_CTL_ADD,timer->getFd(),&timerEv);//add to timer epoll
                }
            }// end stdin if

        }// end for
    }// end service while
}

void EpollService::stopService(){
    if(receiveLoopStart){
        printf("stop service pthread cancel\n");
        pthread_cancel(receiveTrd);
    }

    if(timerLoopStart){
        printf("stop timer loop cancel\n");
        pthread_cancel(timerTrd);
    }
}

// listenfd 出错，需要重新创建监听套接字
bool EpollService::reListen()
{
    close(listenfd);
    epoll_ctl(listenEpollfd, EPOLL_CTL_DEL, listenfd, &server_ev);
    return createListen();
}

bool EpollService::startReceiveThread(){
    //创建线程时采用的参数
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);                 //设置绑定的线程,以获取较高的响应速度
    printf("start receive thread\n");

    epoll_event readPipeEV;
    readPipeEV.data.fd = pipe_[0];
    // read pipe need ET mode ???
    readPipeEV.events = EPOLLIN;
    // add read pipe on receiveEpollfd
    int ret = epoll_ctl(receiveEpollfd,EPOLL_CTL_ADD,pipe_[0],&readPipeEV);
    if(ret != 0){
        printf("add epollin event on read pipe fail\n");
        return false;
    }
    // use static method for pthread_routine
    if(0 !=pthread_create(&receiveTrd,&attr,ReceiveLoop,this)){
        printf("create receive thread fail\n");
        return false;
    }
    return true;
}

bool EpollService::startTimerThread(){
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);                 //设置绑定的线程,以获取较高的响应速度
    printf("start timer thread\n");

    // use static method for start_routine
    if(0 !=pthread_create(&timerTrd,&attr,TimerLoop,this)){
        printf("create timer thread fail\n");
        return false;
    }
    return true;
}

// 在接收线程中使用，接收管道中的客户端套接字
bool EpollService::addClient(){
    pipemsg_t msg;
    int length = read(pipe_[0],&msg,sizeof(msg));
    if(length == sizeof(msg)){
        printf("receive thread read [%d] from pipe connection [%s]:[%d]\n",msg.fd,inet_ntoa(msg.addr.sin_addr),ntohs(msg.addr.sin_port));
        // add to receiveEpollfd
        epoll_event clientEv;
        clientEv.data.fd = msg.fd;
        clientEv.events = EPOLLIN | EPOLLET;
        if(oneshut_){
            clientEv.events |= EPOLLONESHOT;
        }
        int ret = epoll_ctl(receiveEpollfd,EPOLL_CTL_ADD,msg.fd,&clientEv);
        if(ret != 0){
            printf("add client [%d] on receive epollfd fail\n",msg.fd);
            // ignore the late insert into map
            return false;
        }
        setnonblocking(msg.fd);
        // msg => clientInfo_t => std::map
        static clientInfo_t client_info;
        client_info.addr = msg.addr;
        fd_info[msg.fd] = client_info;
        return true;
    }else{
        printf("read from pipe and length = [%d]\n",length);
        return false;
    }
}

void EpollService::readClient(int clientfd){
    const int bufferSize = 10;
    char buffer[bufferSize];
    bzero(buffer,bufferSize);
    pipemsg_t msg;

    // 使用ET模式，需要读干净
    do{
        int length = read(clientfd,buffer,bufferSize);
        if(length>0){
            printf("receive from client [%d] ,length is [%d] message is :[%s]\n",clientfd,length,buffer);
            bzero(buffer,bufferSize);
        }else if(length == 0){
            printf("receive from client but length is [%d], client [%d] close connection\n",length,clientfd);
            close(clientfd);
            // remove from receiveEpollfd
            epoll_event clientEv;
            clientEv.data.fd = clientfd;
            clientEv.events = EPOLLIN | EPOLLET;
            epoll_ctl(receiveEpollfd,EPOLL_CTL_DEL,clientfd,&clientEv);
            fd_info.erase(clientfd);
        }else{
            if (errno == EAGAIN) //没有数据需要读取了
            {
                printf("errno is EAGAIN\n");
                if(oneshut_){
                    // 如果使用了 epoll one shut 模式，需要重新设置
                    epoll_event clientEv;
                    clientEv.data.fd = clientfd;
                    clientEv.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
                    epoll_ctl(receiveEpollfd,EPOLL_CTL_MOD,clientfd,&clientEv);
                    printf("reset one shut\n");
                }
                break;
            }
            else if (errno == EINTR) //可能被中断信号打断,,经过验证对非阻塞socket并未收到此错误,应该可以省掉该步判断
            {
                printf("read break by signal\n");
            }
        }
    }while(service_);
}

// this static method that we can use private argument in this class
void *EpollService::ReceiveLoop(void *args){
    EpollService *server = (EpollService *)args;
    int nfds = 0;
    int ret = 0;
    pipemsg_t msg;
    const int maxEvents = 1024;
    epoll_event events[maxEvents];

    // 接收线程处理两类事件
    // 1 从管道接收主监听线程传输的客户端套接字
    // 2 从各客户端套接字接收传输的数据
    while(server->service_){
        nfds = epoll_wait(server->receiveEpollfd,events,maxEvents,-1);
        for(int i=0;i<nfds && server->service_;++i){
            if(events[i].data.fd == server->pipe_[0]){
                // read new client from pipe and add to receiveEpollfd
                server->addClient();
            }else{
                server->readClient(events[i].data.fd);
            }
        }// end for loop
    }
    return NULL;
}

// 定时器处理线程
void *EpollService::TimerLoop(void *args){
    EpollService *server = (EpollService *)args;
    int nfds = 0;
    const int maxEvents = 1024;
    epoll_event events[maxEvents];
    uint64_t res;

    while(server->service_){
        nfds = epoll_wait(server->timerEpollfd,events,maxEvents,-1);
        for(int i=0;i<nfds && server->service_;++i){
            printf("timer callback\n");
            auto cb = server->timerMap[events[i].data.fd]->getCallback();
            read(events[i].data.fd,&res,sizeof(res));
        }// end for loop
    }
    return NULL;
}