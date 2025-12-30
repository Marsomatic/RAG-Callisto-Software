import smbus2
import bme280
import math
import RPi.GPIO as g
import os
import csv
import signal
import sys

from time import sleep
from datetime import datetime

#BME 280 bus/port definition
port = 1
bus = smbus2.SMBus(port)
bmeAddress= 0x76

#BME 280 calibration parameters
calibrationParameters = bme280.load_calibration_params(bus, bmeAddress)

#Initialisation of GPIO pins
g.setmode(g.BOARD)
g.setup(21, g.OUT)

heaterOn = False
#heater sample period heavily depends on how fast does the box cool down.
#In this case, sampling every 10 min is ok, because to raise temp for 2 deg, it takes 1 hr
heaterSamplePeriod = 600
#heaterCounter has to be equal to heaterSamplePeriod in order to log heater in the first pass.
#Otherwise, the first log happens after heaterSamplePeriod seconds
heaterCounter = 600

'''
example code for heater
g.output(21, 0)
print("heater on")

sleep(300)

g.output(21, 1)
g.cleanup()
print("heater off")
'''

def getDewPoint(airTemperature, relativeHumidity):
    """Compute the dew point in degrees Celsius
    :param airTemperature: current ambient temperature in degrees Celsius
    :type airTemperature: float

    :param relativeHumidity: relative humidity in %
    :type relativeHumidity: float

    :return: the dew point in degrees Celsius
    :rtype: float
    """
    A = 17.625
    B = 243.04

    alpha = ((A * airTemperature) / (B + airTemperature)) + math.log(relativeHumidity/100.0)
    return (B * alpha) / (A - alpha)

def turnOnHeater():
    global heaterOn

    if(heaterOn):
        return
    g.output(21, 0)
    heaterOn = True
    print("heater on")

def turnOffHeater():
    global heaterOn

    if(not heaterOn):
        return
    g.output(21, 1)
    #cleanup serves for the control pin to float, aka not to turn on coil on accident
    g.cleanup()

    heaterOn = False
    print("heater off")

def writeToLog(heaterStatus):
    file_name = "heaterLog.csv"

    # Check if the file exists
    file_exists = os.path.isfile(file_name)
    now = datetime.now()
    
    # Write data to CSV
    with open(file_name, mode='a', newline='') as file:
        writer = csv.writer(file)
        # If the file is new, write the header
        if not file_exists:
            writer.writerow(["Timestamp", "Heater state"])
        # Write the data row
        writer.writerow([now.strftime("%Y-%m-%d %H:%M:%S"), heaterStatus])

def signalHandler(sig, frame):
    turnOffHeater()
    print("You killed the program by pressing Ctrl+C\nThe heater is turned off")
    sys.exit(0)

signal.signal(signal.SIGINT, signalHandler)

while True:
    sleep(1)

    print("BME 280")
    #sampling the sensor
    bmeData = bme280.sample(bus, bmeAddress, calibrationParameters)

    #extracting the parameters
    airTemperature = bmeData.temperature
    relativeHumidity = bmeData.humidity
    airPressure = bmeData.pressure

    print(f"The temperature is: {round(airTemperature,2)}")
    print(f"The air pressure is: {round(airPressure,2)}")
    print(f"The relative humidity is: {round(relativeHumidity,2)}")

    #The heater must be turned on when RH>=90% or when the air temp is <= dew point+2
    #--------------dew point + 3
    #temp must stay here
    #--------------dew point + 2
    #The heater is inherently latching, because if the pin is once pulled high, it stays that way, even when program is killed

    dewPoint = getDewPoint(airTemperature, relativeHumidity)

    if(relativeHumidity >= 90 or airTemperature <= dewPoint+2 ):
        turnOnHeater()
    elif(relativeHumidity < 90 and airTemperature >= dewPoint+3):
        turnOffHeater()

    #calculating the dew point using RH and temp
    print(f"Dew point is: {round(dewPoint,2)}")
    print("==============================")
    if(heaterOn):
        print("The heater is on")
    else:
        print("The heater is off")
    print("\033[F"*7, end='')

    #logging the heater activity every heaterSamplePeriod seconds
    if (heaterCounter == heaterSamplePeriod):
        heaterCounter = 0 
        if(heaterOn):
            writeToLog("The heater is on")
        else:
            writeToLog("The heater is off")
    
    heaterCounter = heaterCounter + 1