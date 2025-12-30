#include <Stepper.h>
#define enable 2

const int stepsPerRevolution = 200;  // change this to fit the number of steps per revolution
String command;

// initialize the stepper library on pins 8 through 11:
Stepper myStepper(stepsPerRevolution, 8, 9, 10, 11);

void setup() {
  // set the speed at 60 rpm:
  myStepper.setSpeed(140);
  // initialize the serial port:
  Serial.begin(9600);
  pinMode(enable, INPUT);
}

void loop() {
  Serial.println(Serial.available());
  if (Serial.available()) {
    if (digitalRead(enable)) {
      command = Serial.readStringUntil('\n');
      command.trim();
      // step one revolution  in one direction:
      Serial.println(command.toInt());
      myStepper.step(command.toInt());
    }
  }
}