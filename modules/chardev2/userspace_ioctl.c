/*
 * userspace_ioctl.c - the process to use IOCTL's to control the kernel module
 *
 * Until now we could have used cat for input/output. Now, we need to do ioctl's,
 * which requires writing our own proces.
 */

// Device specifics, such as IOCTL numbers and major device file
#include "chardev.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>


// Functions for the ioctl calls
int ioctl_set_msg(int fd, char *message) {
    int ret_val;

    ret_val = ioctl(fd, IOCTL_SET_MSG, message);

    if (ret_val < 0)
        printf("%s failed: %d\n", __func__, ret_val);

    return ret_val;
}


int ioctl_get_msg(int fd) {
    int ret_val;
    char message[100]  ={ 0 };

    /* Warning - this is dangerous because we don't tell the kernel how far
     * it's allowed to write, so it might overflow the buffer. In a real
     * production program, we would have used two ioctl's - one to tell
     * the kernel the buffer length and another to give it the buffer to fill
     */

    ret_val = ioctl(fd, IOCTL_GET_MSG, message);

    if (ret_val < 0)
        printf("%s failed: %d\n", __func__, ret_val);

    printf("%s message: %s\n", __func__, message);

    return ret_val;
}


int ioctl_get_nth_byte(int fd) {
    int i, c;

    printf("get_nth_byte message: ");

    i = 0;
    do {
        c = ioctl(fd, IOCTL_GET_NTH_BYTE, i++);

        if (c < 0) {
            printf("\n %s failed at byte %d.\n", __func__, i);
            return c;
        }

        putchar(c);
    } while (c != 0);

    printf("\n");
    return 0;
}


int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s <message to pass ioctl>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd;
    int ret_val = EXIT_FAILURE;
    char *msg = argv[1];

    if (strlen(msg) > 100) {
        printf("Message \"%s\" exceeds max length of %d.\n", msg, 100);
        return EXIT_FAILURE;
    }

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        printf("Can't open device file: %s, error :%d\n", DEVICE_PATH, fd);
        goto error;
    }

    ret_val = ioctl_set_msg(fd, msg);
    if (ret_val)
        goto error;

    ret_val = ioctl_get_nth_byte(fd);
    if (ret_val)
        goto error;

    ret_val = ioctl_get_msg(fd);
    if (ret_val)
        goto error;
    ret_val = EXIT_SUCCESS;

error:
    close(fd);
    return ret_val;
}

