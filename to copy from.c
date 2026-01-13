

//a slight mod of the automatic guidance. we dont need RA calculation, we take it from the console
void *manualGuidanceThread(void *arg){
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
        pid_loop(PID_PERIOD / 1000.0);
        nanosleep(&ts, NULL);
        cycleCounter++;
    }
}



void manualControl(){
  //stop - to stop press q or s or whatever
  //pomak na manji RA
  //pomak na veći RA
  //pomak na određenu poziciju
      //zelis li upisivati u RA obliku ili kut
  
  
  printf("Starting Manual Control\n");
  wiringPiSetupGpio();

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT);

  wiringPiISR(ENC_A, INT_EDGE_BOTH, &encoderISR);
  wiringPiISR(ENC_B, INT_EDGE_BOTH, &encoderISR);


  return;
}



/*trebamo pogledat kako se rade daemons in C
navodno nije komplicirano
odi na stack i kopiraj sve sta se tamo nalazi lol
trebta cemo napisat zasebni program za daemon
treba pogledat multiplexing sa poll funckijom
*/
