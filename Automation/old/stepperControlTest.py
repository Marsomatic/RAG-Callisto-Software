import RPi.GPIO as g
from constants import SLEEP_TIME
import time

g.setmode(g.BOARD)
absoluteStepperState = [330000, 330000] 

states = [
    [1, 0, 0, 0],
    [0, 0, 1, 0],
    [0, 1, 0, 0],
    [0, 0, 0, 1]
]

motors = [[15, 13, 12, 11],
          [32, 33, 31, 29]]

def initMotors():
    '''
    initializes all control pins from the motors list
    '''
    g.setup(motors[0][0], g.OUT)
    g.setup(motors[0][1], g.OUT)
    g.setup(motors[0][2], g.OUT)
    g.setup(motors[0][3], g.OUT)
    g.setup(motors[1][0], g.OUT)
    g.setup(motors[1][1], g.OUT)
    g.setup(motors[1][2], g.OUT)
    g.setup(motors[1][3], g.OUT)

def cleanup(motors):
    '''
    sets all outputs to the motors to 0 (LOW)
    This is done for example to prevent motors from pulling unnecessarily high current when stationary. The used high transmission ratio is enough to hold the steppers in place in most cases.
    '''
    g.output(motors[0][3], 0)
    g.output(motors[0][2], 0)
    g.output(motors[0][1], 0)
    g.output(motors[0][0], 0)
    g.output(motors[1][3], 0)
    g.output(motors[1][2], 0)
    g.output(motors[1][1], 0)
    g.output(motors[1][0], 0)
    
def moveStepper(motor, steps, dir, absoluteStepperState):
    '''
    moves a stepper a given amount of steps in a given direction
    motor: 0 = ra, 1 = dec
    steps: int number of steps
    dir: -1 = east/north, 1 = west/south
    absoluteStepperState: 
    '''
    
    for i in range(abs(steps)):
        g.output(motors[motor][3], states[absoluteStepperState[motor]%4][0])
        g.output(motors[motor][2], states[absoluteStepperState[motor]%4][1])
        g.output(motors[motor][1], states[absoluteStepperState[motor]%4][2])
        g.output(motors[motor][0], states[absoluteStepperState[motor]%4][3])
        time.sleep(SLEEP_TIME)
        cleanup(motors)
        absoluteStepperState[motor] += dir
    return absoluteStepperState

def manual(raSteps, decSteps):
    '''
    manually moves motors by a given amount of steps (direction is indicated with positive or negative values)
    '''
    try:
        global pointing
        global observer
        global sun
        global loc
        global lastPrint
        global absoluteStepperState

        if raSteps < 0:
            try:
                print(f'brrrrrrrrrrrrrrrrrrrrr {absoluteStepperState}')
                absoluteStepperState = moveStepper(0, raSteps, -1, absoluteStepperState)
            except KeyboardInterrupt:
                cleanup(motors)
                return
        if raSteps > 0:
            try:
                print('brrrrrrrrrrrrrrrrrrrrr')
                absoluteStepperState = moveStepper(0, raSteps, 1, absoluteStepperState)
            except KeyboardInterrupt:
                cleanup(motors)
                return

        if decSteps < 0:
            try:
                print('brrrrrrrrrrrrrrrrrrrrr')
                absoluteStepperState = moveStepper(1, decSteps, -1, absoluteStepperState)
            except KeyboardInterrupt:
                cleanup(motors)
                return
        if decSteps > 0:
            try:
                print('brrrrrrrrrrrrrrrrrrrrr')
                absoluteStepperState = moveStepper(1, decSteps, 1, absoluteStepperState)
            except KeyboardInterrupt:
                cleanup(motors)
                return
    except KeyboardInterrupt:
        cleanup(motors)
        return
   
initMotors()
cleanup(motors) 
manual(1000, 0)
manual(-1000, 0)