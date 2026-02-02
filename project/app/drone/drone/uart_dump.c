#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <asm/termbits.h>

int configure_uart(int fd, int baudrate) {
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
    tty.c_cflag &= ~CSTOPB;   // 1 stop bit
    tty.c_cflag &= ~CSIZE;    // Clear size bits
    tty.c_cflag |= CS8;       // 8 data bits

    // No Hardware Flow Control
    tty.c_cflag &= ~CRTSCTS;

    // Enable receiver, local line
    tty.c_cflag |= CREAD | CLOCAL;

    // Raw mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;

    if (ioctl(fd, TCSETS2, &tty) != 0) {
        perror("ioctl(TCSETS2)");
        return -1;
    }
    
    printf("Configured UART using termios2/BOTHER at %d baud\n", baudrate);
    return 0;
}

int main(int argc, char *argv[]) {
    char *port = "/dev/ttyS3";
    int baudrate = 420000;

    if (argc > 1) port = argv[1];
    
    printf("Opening %s at %d baud...\n", port, baudrate);

    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("Error opening serial port");
        return 1;
    }

    if (configure_uart(fd, baudrate) != 0) {
        printf("Failed to configure UART\n");
        close(fd);
        return 1;
    }

    printf("Listening for data... (Press Ctrl+C to stop)\n");

    unsigned char buf[256];
    int total_bytes = 0;

    while (1) {
        int n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            total_bytes += n;
            printf("Read %d bytes: ", n);
            for (int i = 0; i < n; i++) {
                printf("%02X ", buf[i]);
            }
            printf("| ASCII: ");
            for (int i = 0; i < n; i++) {
                if (buf[i] >= 32 && buf[i] <= 126) printf("%c", buf[i]);
                else printf(".");
            }
            printf("\n");
        } else if (n < 0) {
            perror("Read error");
            usleep(100000);
        } else {
            // No data, wait a bit
            // printf("."); fflush(stdout);
            usleep(10000);
        }
    }

    close(fd);
    return 0;
}
