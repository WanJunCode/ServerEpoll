#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <string.h>

#include "timer.h"

#define EPOOL_SIZE 32000
#define EPOOL_EVENT 32


/********************************************************
   Func Name: createTimer
Date Created: 2018-7-30
 Description: 创建定时器对象
       Input: uiTvSec：设置间隔多少秒
             uiTvUsec：设置间隔多少微秒
      Output: 
      Return: 文件句柄
     Caution: 
*********************************************************/
int createTimer(unsigned int uiSec, unsigned int uiNsec)
{
    int iRet = 0;
    int tfd = 0;
    struct itimerspec timeValue;

    //初始化定时器
    /*
     When the file descriptor is no longer required it should be closed.  
     When all file descriptors associated with the same timer object have been closed, 
     the timer is disarmed and its resources are freed by the kernel.

     意思是该文件句柄还是需要调用close函数关闭的
    */
    tfd = timerfd_create(CLOCK_REALTIME,0);
    if (tfd < 0)
    {
        return -1;
    }

    //设置开启定时器
    /*
    Setting either field of new_value.it_value to a nonzero value arms the timer. 
    Setting both fields of new_value.it_value to zero disarms the timer.
    意思是如果不设置it_interval的值非零，那么即关闭定时器
    */
    timeValue.it_value.tv_sec = 1;
    timeValue.it_value.tv_nsec = 0;

    //设置定时器周期
    timeValue.it_interval.tv_sec = (time_t)uiSec;
    timeValue.it_interval.tv_nsec = (long)uiNsec;

    iRet = timerfd_settime(tfd, 0, &timeValue, NULL);
    if (iRet < 0)
    {
        return -1;
    }

    return tfd;
}

/********************************************************
   Func Name: setNoBlock
Date Created: 2018-7-27
 Description: 设置文件描述符非阻塞
       Input: fd：文件描述符
      Output:         
      Return: error code
     Caution: 
*********************************************************/
int setNoBlock(IN int fd)
{
    int iRet = DEFAULT_ERROR;

    int iOption = -1;

    iOption = fcntl(fd, F_GETFD);
    if(iOption < 0)
    {
        iRet = DEFAULT_ERROR;
        return iRet;
    }

    iOption = iOption | O_NONBLOCK;

    iOption = fcntl(fd,F_SETFD,iOption);
    if(iOption < 0)
    {
        iRet = DEFAULT_ERROR;
        return iRet;
    }

    return RESULT_OK;
}

/********************************************************
   Func Name: createEpoll
Date Created: 2018-7-30
 Description: 创建epoll
       Input: 
      Output:         
      Return: epoll句柄
     Caution: 
*********************************************************/
int createEpoll()
{
    int epfd = 0;

    /*
    When no longer required,
    the file descriptor returned by epoll_create() should be closed byusing close(2).  
    When all file descriptors referring to an epoll instance have been closed, 
    the kernel destroys the instance and releases the associated resources for reuse.

    意思是用完需要调用close函数关闭epoll句柄
    */
    epfd = epoll_create(EPOOL_SIZE);
    if (epfd < 0)
    {
        return -1;
    }

    return epfd;
}

/********************************************************
   Func Name: addFdToEpoll
Date Created: 2018-7-30
 Description: 添加文件描述符到epoll
       Input: 
      Output:         
      Return: error code
     Caution: 
*********************************************************/
int addFdToEpoll(int epfd, STEpollParam *pstParam)
{
    int iRet = DEFAULT_ERROR;
    struct epoll_event ev;
    ev.data.ptr = pstParam;
    ev.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    iRet = epoll_ctl(epfd, EPOLL_CTL_ADD, pstParam->fd, &ev);
    if (iRet < 0)
    {
        return DEFAULT_ERROR;
    }
    return RESULT_OK;
}

/********************************************************
   Func Name: recvMsg
Date Created: 2018-7-30
 Description: 消息处理
       Input: pvParam：参数指针
      Output:         
      Return: 
     Caution: 
*********************************************************/
int recvMsg(void *pvParam)
{
    int iRet = DEFAULT_ERROR;
    STEpollParam * pstParam = NULL;

    if (NULL == pvParam)
    {
        iRet = PARAM_ERROR;
        return iRet;
    }
    pstParam = (STEpollParam *)pvParam;
    switch(pstParam->enType)
    {
    case FD_TIMER:
        pstParam->cb(pstParam->fd, pstParam->pvParam);
        break;
    default:
        break;
    }
    return RESULT_OK;
}

/********************************************************
   Func Name: epollWait
Date Created: 2018-7-30
 Description: 等待消息
       Input: epfd：epoll句柄
      Output:         
      Return: 
     Caution: 
*********************************************************/
void epollWait(int epfd)
{
    int i = 0;
    int nfds = 0;
    struct epoll_event *events = NULL;

    //分配epoll事件内存
    events = (struct epoll_event *)malloc(sizeof(struct epoll_event)*EPOOL_EVENT);
    if (NULL == events)
    {
        return ;
    }
    memset(events, 0, sizeof(struct epoll_event)*EPOOL_EVENT);

    for (;;)
    {
        nfds = epoll_wait(epfd, events, EPOOL_EVENT, -1);
        if (nfds < 0)
        {
            break;
        }
        for (i = 0; i < nfds; i++ )
        {
            //监听读事件
            if (events[i].events & EPOLLIN)
            {
                recvMsg(events[i].data.ptr);
            }
        }
    }

    //关闭epoll
    close(epfd);

    return ;
}