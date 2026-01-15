#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>

#define CONSOLE_PERIOD 1000 //in usec

char programVersion[] = "V1.0.";
char programDate[] = "12.01.2026";

/* ======================= GLOBAL STATE ======================= */

typedef enum {
    ST_IDLE,
    ST_AUTOMATIC,
    ST_MANUAL,
    ST_QUIT
} SystemState;

volatile SystemState current_state = ST_IDLE;
volatile int system_running = 1;
volatile long encoder_ticks = 12345;

/* ======================= NON-BLOCKING STDIN ======================= */

void make_stdin_nonblocking(void){
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

/* ======================= UTIL FUNCTIONS ======================= */

void readFromFile(const char* fileName){
    FILE *file;
    char buffer[256];
    char next[256];

    file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }

    /* Read first line */
    if (fgets(buffer, sizeof(buffer), file) != NULL) {

        /* Read remaining lines */
        while (fgets(next, sizeof(next), file) != NULL) {
            printf("%s", buffer);
            strcpy(buffer, next);
        }

        /* Handle last line: remove trailing newline if present */
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }

        printf("%s", buffer);
    }

    fflush(stdout);
    fclose(file);
}

// Writes a string to a file (appends if it exists)
void writeToFile(const char *filename, char *stringToWrite) {
    FILE *filePointer = fopen(filename, "a");  // open for appending
    if (filePointer == NULL) {
        perror("Error opening file");
        return;
    }

    if (fprintf(filePointer, "%s", stringToWrite) < 0) {  // write the string
        perror("Error writing to file");
    }

    fclose(filePointer);
}

char *stringConcatenate(const char *s1, const char *s2) {
    //strlen does not include \0
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    //we have to add +1 for the \0 at the end
    char *result = malloc(len1 + len2 + 1);
    if (!result) return NULL;

    //this memcopy DOES NOT include s1's \0
    memcpy(result, s1, len1); 
    // this copy includes the \0, making it the \0 of the concat'd string
    memcpy(result + len1, s2, len2 + 1);
    return result;
}

void writeLog(const char *filename){
    char encoder_str[32];      // buffer to hold the string
    long temp = encoder_ticks; // Make a snapshot of the volatile variable to be safe

    // Convert to string
    snprintf(encoder_str, sizeof(encoder_str), "%ld", temp);
    char text[] = "The last known posiiton in encoder ticks is: ";
    char *output = stringConcatenate(text, encoder_str);
    
    writeToFile(filename, output);
    free(output); //you have to free memory because you malloced it in string concatenate!!!!
    writeToFile(filename, "\n");
    writeToFile(filename, "You performed a graceful shutdown at TIME\n\n");

    return;
}

/* ======================= CONSOLE THREAD ====================== */

void *consoleThread(void *arg){
    char buf[128];
    const char* helpFile = "help.txt";

    printf("Console is ready.\n");
    printf("\nCommands: auto | manual | stop | status | quit | help | clear\n\n");

    while(system_running){
        memset(buf, 0, sizeof(buf));

        int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (n > 0){
            buf[strcspn(buf, "\n")] = 0; // strip newline

            if (!strcmp(buf, "auto")){
                if(current_state == ST_AUTOMATIC){
			        printf("already in automatic tracking mode\n");
			        continue;
		        }
		        current_state = ST_AUTOMATIC;
                printf("[CMD] Automatic mode pocetak automatskog\n");
            }

            else if (!strcmp(buf, "manual")){
                if(current_state == ST_MANUAL){
			        printf("already in manual control mode\n");
			        continue;
		        }
		        current_state = ST_MANUAL;
                printf("[CMD] Manual  mode -  pocetak manualnog\n");
            }

            else if (!strcmp(buf, "stop")){
		        if(current_state == ST_AUTOMATIC){
			        current_state = ST_IDLE;
			        printf("Automatic control stopped.\nUse these commands to continue: auto | manual | stop | status | quit\n");
		        }

		        if(current_state == ST_MANUAL){
			        current_state = ST_IDLE;
			        printf("Manual control stopped.\nUse these commands to continue: auto | manual | stop | status | quit\n");
		        }

		        if(current_state == ST_IDLE){
                	printf("[CMD] Stop\n");
			        printf("Previous state/command is stopped. Use these commands to continue: auto | manual | stop | status | quit\n");
		        }
            }

            else if (!strcmp(buf, "status")){
	    	    if(current_state == ST_AUTOMATIC) printf("\n[STATUS] Current state is automatic tracking\n\n");
	    	    if(current_state == ST_MANUAL) printf("\n[STATUS] Current state is manual tracking\n\n");
		        if(current_state == ST_IDLE) printf("\n[STATUS] Current state is idle state.\n\n");
            }

            else if (!strcmp(buf, "quit")){
                current_state = ST_QUIT;
                system_running = 0;
                printf("[CMD] Quit\n");
            }

            else if (!strcmp(buf, "help")){
                system("@cls||clear");
                readFromFile(helpFile);
                printf("\n\n");
            }
            
            else if (!strcmp(buf, "clear")){
                system("@cls||clear");
                printf("Commands: auto | manual | stop | status | quit | help | clear\n\n");     
            }

            else {
                printf("[CMD] Unknown command: %s\n", buf);
            }
        }
    }
    return NULL;
}

/* ======================= MAIN FSM LOOP ======================= */
int main(void){
    int idlePrintCounter = 1; // used to print idle state only once

    pthread_t console_thread;

    make_stdin_nonblocking();
    pthread_create(&console_thread, NULL, consoleThread, NULL);

    printf("\nAntenna automation software\n");
    printf("Callisto Station Visnjan\n");
    printf("Version %s\n", programVersion);
    printf("Authors: Matej Markovic, Marko Radolovic\n");
    printf("Visnjan, %s\n \n", programDate);
    printf("Control program has been started.\n");


    while(system_running){
        switch(current_state){

            case ST_IDLE:
                if(idlePrintCounter){
		        printf("The program is in idle state.\n\n"); // do nothing
		        //so i can print only once
		        idlePrintCounter = 0;
		        }
                break;

            case ST_AUTOMATIC:
                printf("[FSM] Automatic running...\n");
                sleep(1);  //change sleep with automaticControl()
		        idlePrintCounter = 1;
                break;

            case ST_MANUAL:
                printf("[FSM] Manual running...\n");
                sleep(1); //change sleep with manualControl()
		        idlePrintCounter = 1;
                break;

            case ST_QUIT:
		        idlePrintCounter = 1;
                system_running = 0;
                break;
        }

        usleep(100 * CONSOLE_PERIOD); // 100 ms FSM tick
    }

    pthread_join(console_thread, NULL);

    writeLog("shutdownLog.txt");

    printf("You have performed a graceful shutdown. The control program shall now terminate.\n");
    return 0;
}

/*trebamo pogledat kako se rade daemons in C
navodno nije komplicirano
odi na stack i kopiraj sve sta se tamo nalazi lol
trebta cemo napisat zasebni program za daemon
treba pogledat multiplexing sa poll funckijom
*/

