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
#define TICKS_PER_REV 60000

// GPIO pins
#define STEP_PIN  13
#define DIR_PIN   5
#define EN_PIN    6
#define ENC_A     17
#define ENC_B     27

// PID parameters
float Kp = 10.0, Ki = 0.0, Kd = 0.0;

// PID state
float integral = 0.0;
float prev_error = 0.0;

// Encoder state
volatile long encoder_ticks = 0;
volatile long setpoint = 0;
static const int8_t TRANSITION[16] = {
    0, -1, 1, 0,
	1, 0, 0, -1,
	-1, 0, 0, 1,
	0, 1, -1, 0
};
uint8_t lastState = 0;

// Motor command
volatile float target_step_rate = 0;
pthread_mutex_t lock;

SpiceDouble ephemeris_t; // ephemeris time past J2000
SpiceDouble obs_lat = 0.790213649;
SpiceDouble obs_lon = 0.239491811;
SpiceDouble obs_alt = 0.2235;

SpiceDouble getEphemerisTime(){
    time_t rawtime = time(NULL);
    char utc_str[80];

    struct tm *utc = gmtime(&rawtime);
    strftime(utc_str, sizeof(utc_str), "%Y-%m-%dT%H:%M:%S", utc);
    str2et_c(utc_str, &ephemeris_t);

    return ephemeris_t;
}

SpiceDouble getHa(){
    SpiceDouble ephemeris_t = getEphemerisTime();

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
    spkpos_c("SUN", ephemeris_t, "J2000", "LT+S", "EARTH", sun_j2000, &lt);

    // Observer vector in J2000
    SpiceDouble xform[3][3]; // transformation matrix
    SpiceDouble obs_j2000[3];
    pxform_c("ITRF93", "J2000", ephemeris_t, xform);
    mxv_c(xform, obs_itrf, obs_j2000);

    // Topocentric Sun vector
    SpiceDouble sun_obs_j2000[3];
    vsub_c(sun_j2000, obs_j2000, sun_obs_j2000); // vector subtraction

    // RA
    SpiceDouble ra  = atan2(sun_obs_j2000[1], sun_obs_j2000[0]);
    if (ra < 0) ra += twopi_c();

    // Greenwich sidereal RA (RA of ITRF x-axis in J2000)
    SpiceDouble x_itrf[3] = {1.0, 0.0, 0.0};
    SpiceDouble x_itrf_j2000[3];
    mxv_c(xform, x_itrf, x_itrf_j2000); // matrix times vector
    SpiceDouble ra_greenwich = atan2(x_itrf_j2000[1], x_itrf_j2000[0]);
    if (ra_greenwich < 0) ra_greenwich += twopi_c();

    // Local Sidereal Time
    SpiceDouble lst = ra_greenwich + obs_lon;
    lst = fmod(lst, twopi_c());
    if (lst < 0) lst += twopi_c();

    // Hour Angle
    SpiceDouble ha = lst - ra;
    while (ha <= -pi_c()) ha += twopi_c();
    while (ha >  pi_c()) ha -= twopi_c();

    return ha;
}

// --- Encoder ISR ---
void encoderISR(void) {
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
    uint8_t index = (lastState << 2) | state;
    int8_t delta = TRANSITION[index];

    if (delta != 0) encoder_ticks += delta;
    lastState = state;
}

// --- Stepper thread ---
void *stepperThread(void *arg) {
    while (1) {
        float step_rate;
        pthread_mutex_lock(&lock);
        step_rate = target_step_rate;
        pthread_mutex_unlock(&lock);

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
    float error = (float)(setpoint - encoder_ticks);

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
}

// --- The antenna knows where it is by knowing where it isnt ---
void *guidanceThread(void *arg){
    SpiceDouble ha;
    SpiceInt setpoint;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 10000000; // 1 s loop
    long cycleCounter = 0;

    while(1){
        cycleCounter++;
        if (cycleCounter > 1000){
            ha = getHa();
            setpoint = ha/twopi_c() * TICKS_PER_REV + TICKS_PER_REV/4;
        }
        pid_update(0.001);
        nanosleep(&ts, NULL)
    }
}

int main(int argc, char **argv){
    wiringPiSetupGpio();

    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(ENC_A, INPUT);
    pinMode(ENC_B, INPUT);

    wiringPiISR(ENC_A, INT_EDGE_BOTH, &encoderISR);
    wiringPiISR(ENC_B, INT_EDGE_BOTH, &encoderISR);

    furnsh_c("/home/matej/cspice/kernels/naif0012.tls");    // leapseconds
    furnsh_c("/home/matej/cspice/kernels/de435.bsp");      // planetary ephemeris
    furnsh_c("/home/matej/cspice/kernels/pck00011.tpc");    // Earth orientation & shape
    furnsh_c("/home/matej/cspice/kernels/earth_000101_260327_251229.bpc"); // earth binary pck

    pthread_t stepper_thread;
    pthread_t guidance_thread;
    pthread_mutex_init(&lock, NULL);
    pthread_create(&stepper_thread, NULL, stepperThread, NULL);
    pthread_create(&guidance_thread, NULL, guidanceThread, NULL);

    kclear_c();

    return 0;
}
