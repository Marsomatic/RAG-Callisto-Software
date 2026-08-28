from ina219 import INA219
from time import sleep
import smbus2
import bme280
import os

SHUNT_OHMS = 0.1

MAX_EXPECTED_AMPS_HEATER = 3
MAX_EXPECTED_AMPS_CALLISTO = 0.3
MAX_EXPECTED_AMPS_LNA = 0.05

#addresses of I2C devices on bus 1 of RPi
bmeAddress= 0x76
callistoAddress = 0x44
heaterAddress = 0x42
lnaAddress = 0x40

#INA219 initialisation
inaCallisto = INA219(SHUNT_OHMS, busnum=1, address=callistoAddress, max_expected_amps=MAX_EXPECTED_AMPS_CALLISTO)
inaCallisto.configure(inaCallisto.RANGE_16V)

inaHeater = INA219(SHUNT_OHMS, busnum=1, address=heaterAddress, max_expected_amps=MAX_EXPECTED_AMPS_HEATER)
inaHeater.configure(inaHeater.RANGE_32V)

inaLNA = INA219(SHUNT_OHMS, busnum=1, address=lnaAddress, max_expected_amps=MAX_EXPECTED_AMPS_LNA)
inaLNA.configure(inaLNA.RANGE_16V, bus_adc=inaLNA.ADC_128SAMP, gain=inaLNA.GAIN_1_40MV)

#BME280 initialisation
#defining port for BME280
port = 1
bus = smbus2.SMBus(port)
calibration_params = bme280.load_calibration_params(bus, bmeAddress)

#5 lines
def read(ina, inaAddress):
    try:
        print(f"Address: {hex(inaAddress)}")
	#1.02 IS A CORRECTION COEFFICIENT FOUND BY COMPARING INA 
	#AND A KNOWN MULTIMETER
        print(f"Bus Voltage {round(ina.voltage(), 3)}V")
        print(f"Bus Current {round(ina.current()*1.02, 3)}mA")
        print(f"Bus Power {round(ina.power(), 3)}mW")
        print("=================")
    except DeviceRangeError as e:
        print("Current overflow")

#5 lines
def readTemp(bus, bmeAddress, calibration_params):
    print(f"Address: {hex(bmeAddress)}")
    bme_data = bme280.sample(bus, bmeAddress, calibration_params)
    print(f"Temperature: {round(bme_data.temperature,3)} deg C")
    print(f"Air pressure: {round(bme_data.pressure,3)} hPa")
    print(f"Relative humidity: {round(bme_data.humidity,3)} %")
    print("=================")



while True:
    
    print("CALLISTO radio spectrometer")
    read(inaCallisto, callistoAddress)

    print("LNA - LNA4ALL low noise amplifier")
    read(inaLNA, lnaAddress)
#from 24 subtract 6 for correct output without lna
    
    print("Heater - two 20W car light bulbs")
    read(inaHeater, heaterAddress)

    print("BME280 temperature sensor")
    readTemp(bus, bmeAddress, calibration_params)
        
    #The function print('\33[<number of rows>A', end='') is used to bring back the cursor to the beginning
    #so the print can "refresh" itself while taking a new measurement
    #parameter <number of rows> shows how many rows does the cursor go back for

    #read() has in itself 5 printable rows
    #readTemp() has  5 printable rows

    #For example, without the LNA attached, we print 1+5 + 1+5 + 1+5, which amounts to 18 rows.


    #print("\033[F"*24, end='')
    print('\33[24A', end='')
    sleep(1)
