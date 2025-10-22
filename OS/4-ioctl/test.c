#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define DEVICE "/dev/mycdrv"
#define BUF_SIZE 32
#define MAX_VARBUF 256

#define MYDRBASE 0xF0
#define MYDRVR_RESET    _IO(MYDRBASE, 0)
#define MYDRVR_OFFLINE  _IO(MYDRBASE, 1)
#define MYDRVR_GETSTATE _IOR(MYDRBASE, 2, char[BUF_SIZE])
#define MYDRVR_SETSTATE _IOW(MYDRBASE, 3, char[BUF_SIZE])
#define MYDRVR_VARBUF   _IOWR(MYDRBASE, 4, struct mydrvr_varbuf)

struct mydrvr_varbuf {
    size_t len;
    char buf[MAX_VARBUF];
};

int main() {
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    char buf[BUF_SIZE];

    // Get buffer from kernel
    if (ioctl(fd, MYDRVR_GETSTATE, buf) == 0) {
        printf("Kernel buffer: '%s'\n", buf);
    } else {
        perror("ioctl GETSTATE");
    }

    // Set buffer in kernel
    strncpy(buf, "Hello from user!", BUF_SIZE);
    if (ioctl(fd, MYDRVR_SETSTATE, buf) == 0) {
        printf("Sent buffer to kernel.\n");
    } else {
        perror("ioctl SETSTATE");
    }

    // Get buffer again
    if (ioctl(fd, MYDRVR_GETSTATE, buf) == 0) {
        printf("Kernel buffer (after set): '%s'\n", buf);
    } else {
        perror("ioctl GETSTATE");
    }

    // Reset buffer in kernel
    if (ioctl(fd, MYDRVR_RESET) == 0) {
        printf("Kernel buffer reset.\n");
    } else {
        perror("ioctl RESET");
    }

    // Get buffer after reset
    if (ioctl(fd, MYDRVR_GETSTATE, buf) == 0) {
        printf("Kernel buffer (after reset): '%s'\n", buf);
    } else {
        perror("ioctl GETSTATE");
    }

    // Set buffer to OFFLINE
    if (ioctl(fd, MYDRVR_OFFLINE) == 0) {
        printf("Kernel buffer set to OFFLINE.\n");
    } else {
        perror("ioctl OFFLINE");
    }

    // Get buffer after OFFLINE
    if (ioctl(fd, MYDRVR_GETSTATE, buf) == 0) {
        printf("Kernel buffer (after OFFLINE): '%s'\n", buf);
    } else {
        perror("ioctl GETSTATE");
    }

    // Test variable-length buffer (1.4)
    struct mydrvr_varbuf vb;
    strcpy(vb.buf, "hello variable ioctl!");
    vb.len = strlen(vb.buf);

    if (ioctl(fd, MYDRVR_VARBUF, &vb) == 0) {
        printf("Kernel returned (varbuf): '%.*s'\n", (int)vb.len, vb.buf);
    } else {
        perror("ioctl VARBUF");
    }

    close(fd);
    return 0;

}