#include "uart_utils.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <asm/termbits.h>
#include <string.h>
#include <errno.h>

int uart_set_custom_speed(int fd, int baudrate) {
    struct termios2 tty;

    if (ioctl(fd, TCGETS2, &tty) != 0) {
        perror("ioctl(TCGETS2)");
        return -1;
    }

    // Set baud rate using BOTHER
    tty.c_cflag &= ~CBAUD;
    tty.c_cflag |= BOTHER;
    tty.c_ispeed = baudrate;
    tty.c_ospeed = baudrate;

    // 8N1 Configuration
    tty.c_cflag &= ~PARENB;   // No parity
    tty.c_cflag &= ~CSTOPB;   // 1 stop bit (if set, then 2 stop bits)
    tty.c_cflag &= ~CSIZE;    // Clear data size bits
    tty.c_cflag |= CS8;       // 8 data bits

    // No Hardware Flow Control
    tty.c_cflag &= ~CRTSCTS;

    // Enable receiver, local line
    tty.c_cflag |= CREAD | CLOCAL;

    // Raw mode - disable all formatting/echoing
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    // Timeout settings (non-blocking read with small timeout if needed)
    // We handle timeouts in the read loop or select/poll, but basic settings:
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; // 0.1s timeout

    if (ioctl(fd, TCSETS2, &tty) != 0) {
        perror("ioctl(TCSETS2)");
        return -1;
    }

    // Verify settings
    if (ioctl(fd, TCGETS2, &tty) != 0) {
        perror("ioctl(TCGETS2) verify");
        return -1;
    }
    
    if (tty.c_ispeed != baudrate || tty.c_ospeed != baudrate) {
        printf("Warning: Requested baud %d, but got %d/%d\n", baudrate, tty.c_ispeed, tty.c_ospeed);
    }

    return 0;
}