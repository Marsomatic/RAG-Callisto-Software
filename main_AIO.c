#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <math.h>

#include <wiringPi.h>
#include "SpiceUsr.h"

#define CONSOLE_PERIOD 1000 //in microseconds
#define TICKS_PER_REV 5000.0f // encoder ticks per revolution of output shaft(encoder ticks * gearbox ratio)
#define PID_PERIOD 1.0f // in milliseconds
#define PI 3.14159265358979323846
#define TARGET_POSITION_UPDATE_MULTIPLIER 5000

// GPIO pins
#define STEP_PIN  13
#define DIR_PIN   5
#define EN_PIN    6
#define ENC_A     27
#define ENC_B     17

char programVersion[] = "V1.0.";
char programDate[] = "12.01.2026.";

// Encoder state
volatile long encoder_ticks = 0;
volatile long setpoint = 0;

uint8_t lastState = 0;

// Motor command
volatile float target_step_rate = 0;
pthread_mutex_t lock;

/* ======================= GLOBAL STATE ======================= */

typedef enum {
    ST_IDLE,
    ST_AUTOMATIC,
    ST_MANUAL,
    ST_QUIT
} SystemState;

volatile SystemState current_state = ST_IDLE;
volatile int system_running = 1;
volatile int control_running = 0;

/* ======================= NON-BLOCKING STDIN ======================= */

void make_stdin_nonblocking(void){
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

/* ======================= CSPICE FUNCTIONS ======================= */

SpiceDouble getEphemerisTime(){
    SpiceDouble ephemeris_time; // ephemeris time past J2000
    time_t rawtime = time(NULL);
    char utc_str[80];

    struct tm *utc = gmtime(&rawtime);
    strftime(utc_str, sizeof(utc_str), "%Y-%m-%dT%H:%M:%S", utc);
    str2et_c(utc_str, &ephemeris_time);
    printf("time: %d;   ", ephemeris_time);

    return ephemeris_time;
}

SpiceDouble getHa(){
    SpiceDouble ephemeris_time = getEphemerisTime();
    printf("time ha: %d", ephemeris_time);

    SpiceDouble obs_lat = 0.790213649;
    SpiceDouble obs_lon = 0.239491811;
    SpiceDouble obs_alt = 0.2235;

    // returns earth radii at different locations to account for ellipsoid shape. Loaded from kernel pck00011.tpc
    SpiceDouble radii[3];
    SpiceInt n;
    bodvrd_c("EARTH", "RADII", 3, &n, radii);
    SpiceDouble equatorial = radii[0];
    SpiceDouble polar      = radii[2];
    SpiceDouble flattening = (equatorial - polar) / equatorial;

    // Observer vector in ITRF93 standard
    SpiceDouble obs_itrf[3];
    georec_c(obs_lon, obs_lat, obs_alt, equatorial, flattening, obs_itrf);

    // Sun position wrt Earth in J2000
    SpiceDouble sun_j2000[3];
    SpiceDouble lt;
    spkpos_c("SUN", ephemeris_time, "J2000", "LT+S", "EARTH", sun_j2000, &lt);

    // Observer vector in J2000
    SpiceDouble xform[3][3]; // transformation matrix
    SpiceDouble obs_j2000[3];
    pxform_c("ITRF93", "J2000", ephemeris_time, xform);
    mxv_c(xform, obs_itrf, obs_j2000);

    // Topocentric Sun vector
    SpiceDouble sun_obs_j2000[3];
    vsub_c(sun_j2000, obs_j2000, sun_obs_j2000); // vector subtraction

    // RA
    SpiceDouble ra  = atan2(sun_obs_j2000[1], sun_obs_j2000[0]);
    if (ra < 0) ra += 2*PI;

    // Greenwich sidereal RA (RA of ITRF x-axis in J2000)
    SpiceDouble x_itrf[3] = {1.0, 0.0, 0.0};
    SpiceDouble x_itrf_j2000[3];
    mxv_c(xform, x_itrf, x_itrf_j2000); // matrix times vector
    SpiceDouble ra_greenwich = atan2(x_itrf_j2000[1], x_itrf_j2000[0]);
    if (ra_greenwich < 0) ra_greenwich += 2*PI;

    // Local Sidereal Time
    SpiceDouble lst = ra_greenwich + obs_lon;
    lst = fmod(lst, 2*PI);
    if (lst < 0) lst += 2*PI;

    // Hour Angle
    SpiceDouble ha = lst - ra;
    while (ha <= -PI) ha += 2*PI;
    while (ha > PI) ha -= 2*PI;

    return ha;
}

/* ======================= UTIL FUNCTIONS ====================== */

void encoderISR(void) {
    static const int8_t TRANSITION[16] = {
    0, -1, 1, 0,
	1, 0, 0, -1,
	-1, 0, 0, 1,
	0, 1, -1, 0
    };
    int a = digitalRead(ENC_A);
    int b = digitalRead(ENC_B);

    // static int last_a = 0, last_b = 0;

    // if (a != last_a || b != last_b) {
    //     if (a == b) encoder_ticks++;
    //     else encoder_ticks--;
    // }

    // last_a = a;
    // last_b = b;

    uint8_t state = (a << 1) | b;
    pthread_mutex_lock(&lock);
    uint8_t index = (lastState << 2) | state;
    int8_t delta = TRANSITION[index];
    if (delta != 0) encoder_ticks += delta;
    if (encoder_ticks > TICKS_PER_REV/2) {
        encoder_ticks -= TICKS_PER_REV;
    } else if (encoder_ticks < -TICKS_PER_REV/2) {
        encoder_ticks += TICKS_PER_REV;
    }
    lastState = state;
    pthread_mutex_unlock(&lock);
}

void pid_loop(float dt) {
    volatile float Kp = 10.0, Ki = 0.0, Kd = 0.0;
    pthread_mutex_lock(&lock);
    float loc_setpoint = (float)setpoint; // steps/sec
    float loc_encoder_ticks = (float)encoder_ticks;
    pthread_mutex_unlock(&lock);

    float prev_error, integral;
    float error = loc_setpoint - loc_encoder_ticks;
    

    integral += error * dt;
    if (integral > 1000) integral = 1000; // anti-windup
    if (integral < -1000) integral = -1000;

    float derivative = (error - prev_error) / dt;
    float output = Kp * error + Ki * integral + Kd * derivative;

    prev_error = error;

    // Update shared step rate
    pthread_mutex_lock(&lock);
    target_step_rate = output; // steps/sec
    pthread_mutex_unlock(&lock);
    printf("encoder_ticks: %f;   error: %f;   output/step_rate:%f   ", loc_encoder_ticks, error, output);
}

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

void writeToFile(const char *filename, char *stringToWrite) {
    // Writes a string to a file (appends if it exists)
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


/* ======================= STEPPER THREAD ====================== */

void *stepperThread(void *arg) {
    printf("Starting stepper thread\n");
    while (1) {
        float step_rate;
        pthread_mutex_lock(&lock);
        step_rate = target_step_rate;
        pthread_mutex_unlock(&lock);
        // printf("%d\n", step_rate);
        if (fabs(step_rate) < 1.0) {
            
            usleep(1000); // idle if rate too low
            continue;
        }

        int dir = (step_rate >= 0) ? HIGH : LOW;
        digitalWrite(DIR_PIN, dir);

        float abs_rate = fabs(step_rate);
        int delay_us = (int)(1000000.0 / (abs_rate * 2.0)); // two edges per step
        if (delay_us < 50) delay_us = 50; // speed limit

        digitalWrite(STEP_PIN, HIGH);
        usleep(delay_us);
        digitalWrite(STEP_PIN, LOW);
        usleep(delay_us);
    }
    return NULL;
}

/* ======================= AUTOMATIC GUIDANCE THREAD ====================== */

// --- The antenna knows where it is by knowing where it isnt ---
void *automaticGuidanceThread(void *arg){
    SpiceDouble ha;
    int loc_setpoint;
    printf("Guidance thread started\n");

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000 * PID_PERIOD; // period in nanoseconds 
    volatile uint32_t cycleCounter = TARGET_POSITION_UPDATE_MULTIPLIER + 1;

    // ha = getHa();
    // int loc_setpoint = (int)(ha/PI * TICKS_PER_REV + TICKS_PER_REV/4.0f);
    // loc_setpoint = (int)(ha/PI * TICKS_PER_REV + TICKS_PER_REV/4.0f);
    // if (loc_setpoint > TICKS_PER_REV/2){
    //     loc_setpoint -= TICKS_PER_REV/2;
    // } else if (loc_setpoint < -TICKS_PER_REV/2){
    //     loc_setpoint += TICKS_PER_REV/2;
    // }
    while(control_running) {
        if (cycleCounter > TARGET_POSITION_UPDATE_MULTIPLIER){
            ha = getHa();
            loc_setpoint = (int)(ha/(2*PI) * TICKS_PER_REV + TICKS_PER_REV/4.0f);
            if (loc_setpoint > TICKS_PER_REV/2){
                loc_setpoint -= TICKS_PER_REV/2;
            } else if (loc_setpoint < -TICKS_PER_REV/2){
                loc_setpoint += TICKS_PER_REV/2;
            }
            cycleCounter = 0;
        }
        pthread_mutex_lock(&lock);
        setpoint = loc_setpoint;
        pthread_mutex_unlock(&lock);

        printf("ha: %f;   setpoint: %d;   cycleCounter: %d\n", ha, loc_setpoint, cycleCounter);
        pid_loop(PID_PERIOD / 1000.0);
        nanosleep(&ts, NULL);
        cycleCounter++;
    }
}

/* ======================= MANUAL CONTROL THREAD ====================== */

  //write a manual control thread
  //stop - to stop press q or s or whatever
  //pomak na manji RA
  //pomak na veći RA
  //pomak na određenu poziciju
  //zelis li upisivati u RA obliku ili kut
  
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
			        //control_running = 0; //if this is uncommented, the threads will be killed and the auto control must be started again
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

/* ======================= AUTOMATIC CONTROL STATE ====================== */

void automaticControl(){
    printf("Starting Automatic Solar Tracking\n");
    wiringPiSetupGpio();

    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(EN_PIN, OUTPUT);
    pinMode(ENC_A, INPUT);
    pinMode(ENC_B, INPUT);

    wiringPiISR(ENC_A, INT_EDGE_BOTH, &encoderISR);
    wiringPiISR(ENC_B, INT_EDGE_BOTH, &encoderISR);

    furnsh_c("/home/kalisto/cspice/kernels/naif0012.tls");    // leapseconds
    furnsh_c("/home/kalisto/cspice/kernels/de435.bsp");      // planetary ephemeris
    furnsh_c("/home/kalisto/cspice/kernels/pck00011.tpc");    // Earth orientation & shape
    furnsh_c("/home/kalisto/cspice/kernels/earth_000101_260327_251229.bpc"); // earth binary pck
    printf("Kernels loaded\n");

    digitalWrite(EN_PIN, 0); //EN = LOW enables the stepper driver. EN = HIGH disables the stepper driver
    printf("Stepper enabled\n");

    control_running = 1; //enables the control threads

    pthread_t stepper_thread;
    pthread_t guidance_thread;
    pthread_mutex_init(&lock, NULL);
    pthread_create(&stepper_thread, NULL, stepperThread, NULL);
    pthread_create(&guidance_thread, NULL, automaticGuidanceThread, NULL);
    printf("Threads created\n");
    pthread_join(stepper_thread, NULL);
    pthread_join(guidance_thread, NULL);
    kclear_c();

    return;
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

    //u fajl zapisat last known position i dal je bio safe shutdown

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
                automaticControl();
		        idlePrintCounter = 1;
                break;

            case ST_MANUAL:
                printf("[FSM] Manual running...\n");
                sleep(1); //change sleep with manualControl()
		        idlePrintCounter = 1;
                break;

            case ST_QUIT:
		        idlePrintCounter = 1;
                control_running = 0;
                system_running = 0;
                break;
        }

        usleep(100 * CONSOLE_PERIOD); // 100 ms FSM tick
    }

    pthread_join(console_thread, NULL);
    control_running = 0;
    //at this moment, all of the threads will be killed
    
    writeLog("shutdownLog.txt");

    printf("You have performed a graceful shutdown. The control program shall now terminate.\n");
    return 0;
}
