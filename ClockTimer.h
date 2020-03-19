#ifndef CLOCKTIMER_H
#define CLOCKTIMER_H

#include <sys/timerfd.h>
#include <unistd.h>     // close()

typedef void(*TimerCallback)(void *arg);

class ClockTimer{
public:
    ClockTimer(int second,TimerCallback callback=NULL);
    ~ClockTimer();

    int getFd() const;
    TimerCallback getCallback();
    static struct timespec &getNow();

private:
    int fd_;
    int second_;
    struct itimerspec new_value;
    TimerCallback cb_;
};

#endif  // CLOCKTIMER_H