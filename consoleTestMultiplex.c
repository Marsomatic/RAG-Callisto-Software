#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

/* =======================
   GLOBAL STATE
   ======================= */

typedef enum {
    ST_IDLE,
    ST_AUTOMATIC,
    ST_MANUAL,
    ST_EXIT
} SystemState;

volatile SystemState current_state = ST_IDLE;
volatile int system_running = 1;

/* =======================
   NON-BLOCKING STDIN
   ======================= */

void make_stdin_nonblocking(void){
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

/* =======================
   CONSOLE THREAD
   ======================= */

void *consoleThread(void *arg){
    char buf[128];

    printf("Console ready.\n");
    printf("Commands: auto | manual | stop | status | quit\n");

    while(system_running){
        memset(buf, 0, sizeof(buf));

        int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (n > 0){
            buf[strcspn(buf, "\n")] = 0; // strip newline

            if (!strcmp(buf, "auto")){
                current_state = ST_AUTOMATIC;
                printf("[CMD] Automatic mode\n");
            }
            else if (!strcmp(buf, "manual")){
                current_state = ST_MANUAL;
                printf("[CMD] Manual mode\n");
            }
            else if (!strcmp(buf, "stop")){
                current_state = ST_IDLE;
                printf("[CMD] Stop\n");
            }
            else if (!strcmp(buf, "status")){
                printf("[STATUS] Current state = %d\n", current_state);
            }
            else if (!strcmp(buf, "quit")){
                current_state = ST_EXIT;
                system_running = 0;
                printf("[CMD] Exit\n");
            }
            else {
                printf("[CMD] Unknown command: %s\n", buf);
            }
        }

        usleep(50000); // 50 ms
    }
    return NULL;
}

/* =======================
   MAIN FSM LOOP
   ======================= */

int main(void){
    pthread_t console_thread;

    make_stdin_nonblocking();
    pthread_create(&console_thread, NULL, consoleThread, NULL);

    printf("Main loop running.\n");

    while(system_running){
        switch(current_state){

            case ST_IDLE:
                // do nothing
                break;

            case ST_AUTOMATIC:
                printf("[FSM] Automatic running...\n");
                sleep(1);
                break;

            case ST_MANUAL:
                printf("[FSM] Manual running...\n");
                sleep(1);
                break;

            case ST_EXIT:
                system_running = 0;
                break;
        }

        usleep(100000); // 100 ms FSM tick
    }

    pthread_join(console_thread, NULL);
    printf("Clean exit.\n");
    return 0;
}

