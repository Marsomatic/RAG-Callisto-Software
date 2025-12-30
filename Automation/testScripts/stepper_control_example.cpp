#include <pigpio.h>
#include <stdio.h>
#include <stdbool.h>

constexpr unsigned int PIN_A = 17;
constexpr unsigned int PIN_B = 27;

std::atomic<int> position(0); // shared count
volatile bool running = true;
uint8_t lastState = 0;

// Interrupt callback for channel A
void cbfA(int gpio, int level, uint32_t tick) {
    if (level == PI_TIMEOUT) return; // ignore timeouts

    int a = gpioRead(PIN_A);
    int b = gpioRead(PIN_B);
    uint8_t state = (a <<1) | b; // 0b000000ab

    uint8_t index = (lastState <<2) | state; //0b0000[2bit last state][2bit current state]
    
    // 0 is invalid state or no move, +1 and -1 are step_pin increments
    static const int8_t TRANSITION[16] = {
	0, -1, 1, 0,
	1, 0, 0, -1,
	-1, 0, 0, 1,
	0, 1, -1, 0
    };

    // step_pin increment
    int8_t delta = TRANSITION[index];
    if (delta != 0) position += delta;

    lastState = state;
}

// Ctrl+C handler
void sigintHandler(int) {
    running = false;
}

int error(int set, int mes) {
	return set - mes;
}

int main(void) {
	if (gpioInitialise() < 0) { 
		std::cerr << "pigpio init failed\n";
		return 1;
	}

	int dir_pin = 17;
	bool dir = 0;
	int step_pin = 27;
	bool step = 0;
	int en = 22;

    signal(SIGINT, sigintHandler);
	
	// rotary feedback
	gpioSetMode(PIN_A, PI_INPUT);
    gpioSetMode(PIN_B, PI_INPUT);
	gpioSetPullUpDown(PIN_A, PI_PUD_UP);
    gpioSetPullUpDown(PIN_B, PI_PUD_UP);
	// stepper feedforward
	gpioSetMode(dir_pin, PI_OUTPUT);
	gpioSetMode(step_pin, PI_OUTPUT);
	gpioSetMode(en, PI_OUTPUT);
	
	// stepper initialization
	gpioWrite(en, 0);
	printf("Enable\n");
	gpioWrite(dir_pin, 1);
	lastState = (gpioRead(PIN_A <<1) ) | gpioRead(PIN_B);

 	// encoder pin initialization
    gpioSetAlertFunc(PIN_A, cbfA);
    gpioSetAlertFunc(PIN_B, cbfA);

	printf("Stepping 1000 steps\n");
	int pos_set = 1000;
	int pos_mes = position.load();
	int err;

	while(running && err != 0){
		err = error(pos_set, pos_mes);
		err < 0 ? dir = 1 : dir = 0;
		gpioWrite(dir_pin, dir);

		gpioWrite(step_pin, step);
		step = !step;
		gpioDelay(25);
		pos_mes = position.load();
		gpioDelay(75);

		std::cout << "\rPosition count: " << pos_mes << "\nError: " << err << std::flush;
	}
	
	gpioWrite(en, 1);
	printf("Enable\n");

	gpioTerminate();
	return 0;
}