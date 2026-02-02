#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <asm/termbits.h>  /* struct termios2 */

int main() {
    printf("Testing termios2 availability...\n");
    struct termios2 t2;
    printf("Size of termios2: %zu\n", sizeof(t2));
    printf("BOTHER constant: %d\n", BOTHER);
    return 0;
}