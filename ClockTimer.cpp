#include "ClockTimer.h"

ClockTimer::ClockTimer(int second,TimerCallback callback){
    second_ = second;
    fd_ = timerfd_create(CLOCK_REALTIME,0);
    cb_ = callback;

    timespec &now = getNow();
    new_value.it_value.tv_sec = now.tv_sec + second;                // first callback time
    new_value.it_value.tv_nsec = now.tv_nsec;
    new_value.it_interval.tv_sec = second;                          // interval
    timerfd_settime(fd_, TFD_TIMER_ABSTIME, &new_value, NULL);
}

ClockTimer::~ClockTimer(){
    close(fd_);
}

int ClockTimer::getFd() const{
    return fd_;
}

TimerCallback ClockTimer::getCallback(){
    return cb_;
}

// STATIC
struct timespec &ClockTimer::getNow(){
    static struct timespec now;
    clock_gettime(CLOCK_REALTIME,&now);
    return now;
}