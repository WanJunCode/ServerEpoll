#include "Tool.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>

void setnonblocking(int sock) 
{
    int opts;
    opts = fcntl(sock,F_GETFL);
    if (opts < 0)
    {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts|O_NONBLOCK;
    if (fcntl(sock, F_SETFL, opts) < 0)
    {
        perror("fcntl(sock,SETFL,opts)");         
        exit(1);
    }
}

void setreuseaddr(int sock)
{
    int opt;
    opt = 1;    
    if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(&opt)) < 0)     
    {         
        perror("setsockopt");         
        exit(1);     
    }  
}

void sig_pro(int signum)
{
    printf("sig_pro, recv signal: [%d]",signum);
    if (signum == SIGQUIT)
    {
        printf("receive signal SIGQUIT\n");
    }
}
