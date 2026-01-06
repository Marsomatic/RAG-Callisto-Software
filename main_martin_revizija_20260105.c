/*
Author: Matej Markovic
compile with: gcc -o cspice_test.exe cspice_test.c -I/path/to/cspice/include -L/path/to/cspice/lib -lm -lcspice -lwiringPi
*/

#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <wiringPi.h>
#include "SpiceUsr.h"

// encoder ticks per revolution of output shaft(encoder ticks * gearbox ratio)
#define TICKS_PER_REV 5000.0f
#define PID_PERIOD 1.0f // ms
#define PI 3.14159265358979323846
#define TARGET_POSITION_UPDATE_MULTIPLIER 5000

// GPIO pins
#define STEP_PIN  13
#define DIR_PIN   5
#define EN_PIN    6
#define ENC_A     27
#define ENC_B     17

// Encoder state
volatile long encoder_ticks = 0;
volatile long setpoint = 0;

uint8_t lastState = 0;

// Motor command
volatile float target_step_rate = 0;
pthread_mutex_t lock;

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

// --- Encoder ISR ---
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

// --- Stepper thread ---
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

void *displayThreaed(void *arg){
    while(1) {

    }
}

// --- PID loop ---
void pid_update(float dt) {
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

// --- The antenna knows where it is by knowing where it isnt ---
void *guidanceThread(void *arg){
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
    while(1) {
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
        pid_update(PID_PERIOD / 1000.0);
        nanosleep(&ts, NULL);
        cycleCounter++;
    }
}

int main(int argc, char **argv){
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
    digitalWrite(EN_PIN, 0);
    printf("Stepper enabled\n");
    pthread_t stepper_thread;
    pthread_t guidance_thread;
    pthread_mutex_init(&lock, NULL);
    pthread_create(&stepper_thread, NULL, stepperThread, NULL);
    pthread_create(&guidance_thread, NULL, guidanceThread, NULL);
    printf("Threads created\n");
    pthread_join(stepper_thread, NULL);
    pthread_join(guidance_thread, NULL);
    kclear_c();

    return 0;
}

__________
enum State {
  state1,
  state2,
  state3
}

State conver_str_to_start(char *str){
  if(strcmp(str, "state1") == 0) return state1;
  if(strcmp(str, "state2") == 0) return state2;
  ...

}



int main(int argc, char **argv){
  if(argc != 2){
    printf("Error: Did not provide starting state!!!\n");
    exit(1);
  }
  
  State current_state = convert_str_to_start(argv[1]);
  
  while(1){
    switch(current_state){
      case: state1
        napravi sta treba
        current_state = novi_state
      case state2
        napravi_state treba
        current_state = novi state
      default:
        ako nes ode u kurac
      }
    
    }
  
  }
  
  


  return 0;
}
_______________



trebamo pogledat kako se rade daemons in C
navodno nije komplicirano
odi na stack i kopiraj sve sta se tamo nalazi lol
trebta cemo napisat zasebni program za daemon
treba pogledat multiplexing sa poll funckijom
