#include "camera_driver/common/common.hpp"

int xioctl(int fd, long int request, void *arg)
{
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}