#include <pigpio.h>
#include <iostream>
#include <atomic>
#include <csignal>

constexpr unsigned int PIN_A = 17;
constexpr unsigned int PIN_B = 27;

constexpr int COUNT_PER_DETENT = 4;
constexpr int DETENT_PER_REV = 50;

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

    // 0 is invalid state or no move, +1 and -1 are step increments
    static const int8_t TRANSITION[16] = {
	0, -1, 1, 0,
	1, 0, 0, -1,
	-1, 0, 0, 1,
	0, 1, -1, 0
    };

    // step increment
    int8_t delta = TRANSITION[index];
    if (delta != 0) position += delta;

    lastState = state;
}


// Ctrl+C handler
void sigintHandler(int) {
    running = false;
}

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        return 1;
    }

    // SIGINT is user interrupt
    signal(SIGINT, sigintHandler);

    gpioSetMode(PIN_A, PI_INPUT);
    gpioSetMode(PIN_B, PI_INPUT);
    gpioSetPullUpDown(PIN_A, PI_PUD_UP);
    gpioSetPullUpDown(PIN_B, PI_PUD_UP);

    // reads initial state
    lastState = (gpioRead(PIN_A <<1) ) | gpioRead(PIN_B);

    // Monitor both rising and falling edges
    gpioSetAlertFunc(PIN_A, cbfA);
    gpioSetAlertFunc(PIN_B, cbfA);

    std::cout << "Encoder reading... Press Ctrl+C to exit\n";

    while (running) {
        // Example: read position in main loop
        int pos = position.load();
        int detents = pos  / COUNT_PER_DETENT;
        double degrees = (detents * 360.0) / DETENT_PER_REV;
        degrees = degrees / 100;

    std::cout << "\rPosition count: " << pos
              << " | Detents: " << detents
              << " | Degrees: " << degrees 
              << "   " << std::flush;
        gpioDelay(50000); // 50 ms delay
    }

    gpioTerminate();
    return 0;
}
