#ifndef COTASK_UTIL_H
#define COTASK_UTIL_H

#include <fcntl.h>
#include <sys/eventfd.h>

struct Common
{
    static int32_t GetFD()
    {
        int fd = eventfd(0, 0);
        int opts;
        opts = fcntl(fd, F_GETFL);
        opts = opts | O_NONBLOCK;
        fcntl(fd, F_SETFL, opts);
        return fd;
    }
};

#endif  // COTASK_UTIL_H
