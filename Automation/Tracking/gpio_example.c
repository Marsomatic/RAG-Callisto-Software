#include <pigpio.h>
#include <stdio.h>
#include <stdbool.h> 

int main(void)
{
    if (gpioInitialise() < 0) return 1; // Init failed

    int dir = 17; // physical 11
    bool dirstate = 0;
    int step = 27; // physical 13
    bool stepstate = 0;
    int en = 22; // physical 15

    gpioSetMode(dir, PI_OUTPUT);
    gpioSetMode(step, PI_OUTPUT);
    gpioSetMode(en, PI_OUTPUT);

    gpioWrite(en, 1); // Enable
    printf("Enable HIGH\n");
    gpioWrite(dir, 1);
    dirstate = !dirstate;

    printf("Stepping\n");
    for (size_t i = 0; i < 100; i++){
        gpioWrite(step, stepstate);
        stepstate = !stepstate;
        gpioDelay(100000);
    }

    gpioWrite(en, 0); // Set LOW
    printf("Enable LOW\n");

    gpioTerminate();
    return 0;
}
