#!/usr/bin/env python3
import os
import csv
import math
from datetime import datetime
from smbus2 import SMBus
import bme280  # you need to have the bme280 Python module installed

# Settings
BASE_DIR = "/var/www/callisto/weatherlogs"  # change to your desired base folder
PORT = 1
ADDRESS = 0x76  # or 0x77 depending on your sensor

# Initialize BME280
bus = SMBus(PORT)
calibration_params = bme280.load_calibration_params(bus, ADDRESS)

# Read sensor data
data = bme280.sample(bus, ADDRESS, calibration_params)
now = datetime.now()

# Function to calculate dew point
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

# Calculate dew point
dew_point_c = getDewPoint(data.temperature, data.humidity)

# Create folder paths
year_folder = os.path.join(BASE_DIR, now.strftime("%Y"))
month_folder = os.path.join(year_folder, now.strftime("%m"))
day_folder = os.path.join(month_folder, now.strftime("%d"))
print(day_folder)
os.makedirs(day_folder, exist_ok=True)  # Ensure daily folder exists

# Build file path in daily folder
file_path = os.path.join(
    day_folder, f"{now.strftime('%Y-%m-%d')}_weather_readouts.csv"
)

# If file doesn't exist, create and write header
if not os.path.isfile(file_path):
    with open(file_path, mode="w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Time", "Temperature_C", "Pressure_hPa", "Humidity_%", "DewPoint_C"])

# Append new data row
with open(file_path, mode="a", newline="") as f:
    writer = csv.writer(f)
    writer.writerow([
        now.strftime("%H:%M:%S"),
        round(data.temperature, 2),
        round(data.pressure, 2),
        round(data.humidity, 2),
        round(dew_point_c, 2)
    ])
    