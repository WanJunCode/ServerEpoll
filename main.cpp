#include "EpollService.h"
#include <stdio.h>
#include <signal.h>
#include <memory>
std::shared_ptr<Service> epollServer = std::make_shared<EpollService>(12345);

void my_sig_pro(int sig){
    printf("receive signal [%d]\n",sig);
    if(sig == SIGINT){
        epollServer->stopService();
    }
}

int main(int argc,char *argv[]){

    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = my_sig_pro;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    epollServer->startService();
    return 0;
}