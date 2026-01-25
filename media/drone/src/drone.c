#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
}

static int init()
{
    return 0;
}

int main(int argc, char *argv[])
{
    /* Setup signal handler for clean exit */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Starting drone app..."); fflush(stdout);
    init();
    printf("ok.\n");

    /* Animation counter */
    int counter = 0;

    printf("Press Ctrl+C to exit\n");

    /* Main loop */
    while(running) {
        counter++;
        if(counter % 100 == 0) {
            printf("Running... counter=%d\n", counter);
        }
        usleep(10000); /* 10ms delay */
    }

    printf("\nCleaning up...\n");

    return 0;
}