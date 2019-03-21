#include "EpollService.h"
#include "Tool.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> // inet_ntoa
#include <errno.h>
#include <pthread.h>

EpollService::EpollService(int port, int backlog)
    : port_(port),
      backlog_(backlog),
      service_(false),
      receiveLoopStart(false)
{
    stdin_ = STDIN_FILENO;
    listenEpollfd = epoll_create(1024);
    receiveEpollfd = epoll_create(1024);

    epoll_event stdin_ev;
    stdin_ev.data.fd = stdin_;
    stdin_ev.events = EPOLLIN;
    epoll_ctl(listenEpollfd,EPOLL_CTL_ADD,stdin_,&stdin_ev);
    // create pipe for transpotr client message
    pipe(pipe_);
    createListen();
}

EpollService::~EpollService()
{
    if(receiveLoopStart == true){
        printf("join receive thread\n");
        if(0 !=pthread_join(receiveTrd,NULL)){
            printf("join reveive thread fail\n");
        }
    }

    for(auto iter = fd_info.begin();iter != fd_info.end();iter++){
        close(iter->first);
        printf("close [%d]\n",iter->first);
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

bool EpollService::createListen()
{
    //创建监听socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0){
        printf("createListen, socket fail: [%d]\n", listenEpollfd);
        close(listenEpollfd);
        return false;
    }
    //把监听socket设置为非阻塞方式
    setnonblocking(listenfd);
    //设置监听socket为端口重用
    setreuseaddr(listenfd);
    server_ev.data.fd = listenfd;
    // 使用 边沿触发模式处理监听事件
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
}

void EpollService::startService()
{
    service_ = true;

    // start receive thread loop
    if(startReceiveThread() == false){
        receiveLoopStart = false;
        printf("start receive thread fail\n");
        return;
    }else{
        receiveLoopStart = true;
    }

    printf("start epoll server service\n");
    const int MAXEVENTS = 1024;           //最大事件数
    struct epoll_event events[MAXEVENTS]; //监听事件数组
    socklen_t clilen;
    int nfds;
    const char *response = "this is message from epoll server";
    while (service_)
    {
        nfds = epoll_wait(listenEpollfd, events, MAXEVENTS, -1);
        for (int i = 0; i < nfds && service_; ++i)
        {
            if (events[i].data.fd == listenfd) //是本监听socket上的事件
            {
                printf("AcceptThread, events: [%d]\n", events[i].events);
                if (events[i].events & EPOLLIN) //有连接到来
                {
                    // loop accept
                    do{
                        struct sockaddr_in cliaddr;
                        clilen = sizeof(struct sockaddr);
                        int connfd = accept(listenfd, (sockaddr *)&cliaddr, &clilen);
                        if (connfd > 0)
                        {
                            printf("AcceptThread, accept: [%d],connect: [%s]:[%d]\n", connfd, inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));
                            // 传递
                            static pipemsg_t msg;
                            msg.op = 0;
                            msg.fd = connfd;
                            msg.addr = cliaddr;
                            write(pipe_[1],&msg,sizeof(msg));
                            write(connfd, response, strlen(response));
                        }else{
                            printf("AcceptThread, accept fail: connfd = [%d]\n", connfd);
                            if (errno == EAGAIN) //没有连接需要接收了
                            {
                                break;
                            }
                            else if (errno == EINTR) //可能被中断信号打断,,经过验证对非阻塞socket并未收到此错误,应该可以省掉该步判断
                            {
                                ;
                            }
                            else //其它情况可以认为该描述字出现错误,应该关闭后重新监听
                            {
                                // 如果重启失败，则退出
                                if(false == reListen()){
                                    service_ = false;
                                    return;
                                }
                            }
                        }
                    } while (service_);
                    // end loop accept
                }else if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP){
                    if(false == reListen()){
                        service_ = false;
                        return;
                    }
                }
            }// end if

            if(events[i].data.fd == stdin_){
                char buffer[80];
                bzero(buffer,sizeof(buffer));
                int length = read(stdin_,buffer,sizeof(buffer));
                buffer[length-1] = '\0';
                if(strncmp(buffer,"over",4) == 0){
                    printf("from stdin read over\n");
                    stopService();
                }else if(strncmp(buffer,"list",4)==0){
                    printf("list begin\n");
                    for(auto iter=fd_info.begin();iter!=fd_info.end();++iter){
                        printf("infomation: [%d] [%s]:[%d]\n",iter->first,inet_ntoa(iter->second.addr.sin_addr),ntohs(iter->second.addr.sin_port));
                    }
                    printf("list end\n");
                }
            }
        }// end for
    }
}

void
EpollService::stopService(){
    if(receiveLoopStart){
        printf("stop service pthread cancel\n");
        pthread_cancel(receiveTrd);
    }
    service_ = false;
}

bool 
EpollService::reListen()
{
    //此时说明该描述字已经出错了,需要重新创建和监听
    close(listenfd);
    epoll_ctl(listenEpollfd, EPOLL_CTL_DEL, listenfd, &server_ev);
    return createListen();
}

bool 
EpollService::startReceiveThread(){
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
    // use static method for start_routine
    if(0 !=pthread_create(&receiveTrd,&attr,ReceiveLoop,this)){
        printf("create receive thread fail\n");
        return false;
    }
    return true;
}

// in static method we can use private argument in this class
void *
EpollService::ReceiveLoop(void *args){
    EpollService *server = (EpollService *)args;
    int nfds = 0;
    int ret = 0;
    pipemsg_t msg;
    const int maxEvents = 1024;
    epoll_event events[maxEvents];
    const int bufferSize = 1024;
    char buffer[bufferSize];
    bzero(buffer,bufferSize);
    while(server->service_){
        
        nfds = epoll_wait(server->receiveEpollfd,events,maxEvents,-1);
        if(nfds>0){
            for(int i=0;i<nfds && server->service_;++i){
                if(events[i].data.fd == server->pipe_[0]){
                    // read new client from pipe and add to receiveEpollfd
                    int length = read(server->pipe_[0],&msg,sizeof(msg));
                    if(length == sizeof(msg)){
                        printf("receive thread read [%d] from pipe connection [%s]:[%d]\n",msg.fd,inet_ntoa(msg.addr.sin_addr),ntohs(msg.addr.sin_port));
                        // add to receiveEpollfd
                        epoll_event clientEv;
                        clientEv.data.fd = msg.fd;
                        clientEv.events = EPOLLIN;
                        ret = epoll_ctl(server->receiveEpollfd,EPOLL_CTL_ADD,msg.fd,&clientEv);
                        if(ret != 0){
                            printf("add client [%d] on receive epollfd fail\n",msg.fd);
                            // ignore the later insert into map
                            continue;
                        }
                        // msg => clientInfo_t => std::map
                        static clientInfo_t client_info;
                        client_info.addr = msg.addr;
                        server->fd_info[msg.fd] = client_info;
                    }else{
                        printf("read from pipe and length = [%d]\n",length);
                    }
                }else{
                    // read from client
                    bzero(buffer,bufferSize);
                    int length = read(events[i].data.fd,buffer,bufferSize);
                    if(length>0){
                        printf("receive from client [%d] , message is :[%s]\n",events[i].data.fd,buffer);
                    }else if(length == 0){
                        printf("receive from client but length is [%d], client [%d] close connection\n",length,events[i].data.fd);
                        close(events[i].data.fd);
                        // remove from receiveEpollfd
                        epoll_event clientEv;
                        clientEv.data.fd = msg.fd;
                        clientEv.events = EPOLLIN;
                        epoll_ctl(server->receiveEpollfd,EPOLL_CTL_DEL,events[i].data.fd,&clientEv);
                        server->fd_info.erase(events[i].data.fd);
                    }else{
                        printf("receive from client but length is [%d]\n",length);
                    }
                }

            }// end for loop
        }// end nfds > 0
        else{
            printf("nfds = [%d]\n",nfds);
        }

    }

    return NULL;
}