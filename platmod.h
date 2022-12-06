#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

#define MAJOR_NUM 100

#define IOCTL_SET_STACK_SIZE _IOW(MAJOR_NUM, 0, int)

#define DEVICE_FILE_NAME "platmod"
#define DEVICE_PATH "/dev/platmod"

#endif