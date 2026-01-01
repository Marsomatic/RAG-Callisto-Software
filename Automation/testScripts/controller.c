#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

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
volatile float target_step_rate = 0; // steps per second
pthread_mutex_t lock;

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

// --- Main ---
int main(void) {
    wiringPiSetupGpio();

    pinMode(STEP_PIN, OUTPUT);
    pinMode(DIR_PIN, OUTPUT);
    pinMode(ENC_A, INPUT);
    pinMode(ENC_B, INPUT);

    wiringPiISR(ENC_A, INT_EDGE_BOTH, &encoderISR);
    wiringPiISR(ENC_B, INT_EDGE_BOTH, &encoderISR);

    pthread_t stepper_thread;
    pthread_mutex_init(&lock, NULL);
    pthread_create(&stepper_thread, NULL, stepperThread, NULL);

    printf("PID Stepper Control Start\n");
    setpoint = 300; // example: move to tick 2000

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000; // 1 ms loop
    long cycleCounter = 0;

    while (1) {
        cycleCounter++;
        if(cycleCounter % 10 == 0){
            setpoint++;
        }
        if(cycleCounter % 5000 == 0){
            setpoint = -100;
        }

        printf("encoder ticks: %d; setpoint: %d; error: %d; step rate: %d\n", encoder_ticks, setpoint, (encoder_ticks-setpoint), target_step_rate);
        pid_update(0.001);  // dt = 1 ms
        nanosleep(&ts, NULL);
    }

    return 0;
}
