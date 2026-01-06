#include <wiringPi.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#define switchPin 23
#define MAX_COUNT 5

//Only accept a state if it remains the same for N consecutive reads.

bool state;

bool debounce_read(bool raw)
{
    static int cnt = 0;
    static bool state = false;

    if (raw && cnt < MAX_COUNT) cnt++;
    else if (!raw && cnt > 0) cnt--;

    if (cnt == MAX_COUNT) state = true;
    else if (cnt == 0) state = false;

    return state;
}

int main(){
    wiringPiSetupGpio();
    pinMode(switchPin, INPUT);

    while(1){
        state = debounce_read((bool)digitalRead(switchPin));
        printf("Switch state: %d\r", state); 
        usleep(1000);
    }
    return 0;
}
