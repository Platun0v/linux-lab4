#include "platmod.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>


int ioctl_set_msg(int file_desc, int stack_size) {
    int ret_val;

    ret_val = ioctl(file_desc, IOCTL_SET_STACK_SIZE, stack_size);

    if (ret_val < 0) {
        printf("ioctl_set_msg failed:%d\n", ret_val);
    }

    return ret_val;
}

int main(void) {
    int file_desc, ret_val, stack_size;

    file_desc = open(DEVICE_PATH, O_RDWR);
    if (file_desc < 0) {
        printf("Can't open device file: %s, error:%d\n", DEVICE_PATH, file_desc);
        exit(EXIT_FAILURE);
    }

    printf("Enter stack size: ");
    scanf("%d", &stack_size);

    ret_val = ioctl_set_msg(file_desc, stack_size);
    if (ret_val) {
        close(file_desc);
        exit(EXIT_FAILURE);
    }

    close(file_desc);
    return 0;
}