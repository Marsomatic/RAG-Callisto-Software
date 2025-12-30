#include <pigpio.h>
#include <stdio.h>
#include <stdbool.h>

int main(void) {
	if (gpioInitialise() < 0) return 1;
	
	int dir = 5;
	bool dirstate = 0;
	int step = 13;
	bool stepstate = 0;
	int en = 6;
	
	gpioSetMode(dir, PI_OUTPUT);
	gpioSetMode(step, PI_OUTPUT);
	gpioSetMode(en, PI_OUTPUT);
	
	gpioWrite(en, 0);
	printf("Enable\n");
	gpioWrite(dir, 1);

	printf("Stepping 100 steps\n");
	for (size_t i = 1; i < 12800; i++){
		gpioWrite(step, stepstate);
		stepstate = !stepstate;
		gpioDelay(100);
	}
	gpioWrite(en, 1);
	printf("Enable\n");

	gpioTerminate();
	return 0;
}
