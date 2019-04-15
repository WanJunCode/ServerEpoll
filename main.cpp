#include "EpollService.h"
#include <stdio.h>
#include <signal.h>
#include <memory>

// 使用唯一指针 多态 使用
std::unique_ptr<Service> g_epollServer(new EpollService(12345,10,true));

void my_sig_pro(int sig){
    printf("receive signal [%d]\n",sig);
    if(sig == SIGINT){
        g_epollServer->stopService();
    }
}

void setSignalProcess(){
    struct sigaction sa;
    sa.sa_flags = SA_RESTART;       // 被打断后，结束信号的操作后重新执行
    sa.sa_handler = my_sig_pro;     // 信号处理函数
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

int main(int argc,char *argv[]){

    // 使用信号 关闭 epoll server
    setSignalProcess();

    g_epollServer->startService();
    
    return 0;
}