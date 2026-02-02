#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

// Simple test to create a dummy UART for testing
int create_dummy_uart(const char* path) {
    // Create a pipe to simulate UART
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return -1;
    }
    
    // For testing, we'll just create a file
    int fd = open("/tmp/test_uart", O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0) {
        perror("open test_uart");
        return -1;
    }
    
    printf("Created test UART file: /tmp/test_uart\n");
    return fd;
}

int main() {
    printf("=== UART Baudrate Test ===\n");
    
    // Test different baudrates
    int test_baudrates[] = {9600, 38400, 115200, 230400, 460800, 420000};
    int num_rates = sizeof(test_baudrates) / sizeof(test_baudrates[0]);
    
    printf("Testing baudrate support...\n");
    
    for (int i = 0; i < num_rates; i++) {
        int baudrate = test_baudrates[i];
        printf("Testing %d baud: ", baudrate);
        
        // Try to open a real UART if available, otherwise create test
        int fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd < 0) {
            printf("(no real UART, simulation only) ");
            fd = open("/tmp/test_uart", O_CREAT | O_RDWR | O_TRUNC, 0666);
            if (fd < 0) {
                printf("FAILED - cannot create test file\n");
                continue;
            }
        }
        
        // Test termios configuration
        struct termios tty;
        if (tcgetattr(fd, &tty) == 0) {
            // Test custom baudrate setup
            struct serial_struct ser;
            if (ioctl(fd, TIOCGSERIAL, &ser) == 0) {
                ser.flags &= ~ASYNC_SPD_MASK;
                ser.flags |= ASYNC_SPD_CUST;
                ser.custom_divisor = ser.baud_base / baudrate;
                
                if (ioctl(fd, TIOCSSERIAL, &ser) == 0) {
                    if (cfsetispeed(&tty, B38400) == 0 && cfsetospeed(&tty, B38400) == 0) {
                        if (tcsetattr(fd, TCSANOW, &tty) == 0) {
                            printf("SUCCESS (custom: divisor=%d)\n", ser.custom_divisor);
                        } else {
                            printf("FAILED (tcsetattr)\n");
                        }
                    } else {
                        printf("FAILED (cfsetispeed/cfsetospeed)\n");
                    }
                } else {
                    printf("FAILED (TIOCSSERIAL)\n");
                }
            } else {
                printf("FAILED (TIOCGSERIAL)\n");
            }
        } else {
            printf("FAILED (tcgetattr)\n");
        }
        
        close(fd);
    }
    
    printf("\n=== Testing CRSF Application ===\n");
    printf("Run the main application with:\n");
    printf("  sudo ./build/src/rv1106_ipc/drone\n");
    printf("\nThe application will:\n");
    printf("1. Try to set 420000 baud on /dev/ttyS3\n");
    printf("2. Fall back to custom baudrate if possible\n");
    printf("3. Fall back to standard baudrate if custom fails\n");
    printf("4. Handle errors gracefully and attempt reconnection\n");
    
    return 0;
}