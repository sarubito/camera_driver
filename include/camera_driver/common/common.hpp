#ifndef COMMON_HPP
#define COMMON_HPP

#include <cerrno>
#include <sys/ioctl.h>

// エラーが発生した場合、エラーと扱わずに再度ioctlを呼び出すための関数
int xioctl(int fd, long int request, void *arg);

#endif // COMMON_HPP