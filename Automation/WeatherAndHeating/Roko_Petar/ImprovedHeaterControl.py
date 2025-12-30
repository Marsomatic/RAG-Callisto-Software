#!/usr/bin/env python3
import os
import csv
import math
from datetime import datetime
from smbus2 import SMBus
import bme280
import RPi.GPIO as g

# ==============================
# Settings
# ==============================
BASE_DIR = "/var/www/callisto/enclosurelogs"  # Log folder base
TEMP_DIR = os.path.join(BASE_DIR, "temp")   # Folder for heater state file
STATE_FILE = os.path.join(TEMP_DIR, "heater_state.txt")
PORT = 1
ADDRESS = 0x76
GPIO_PIN = 21

# ==============================
# Helper functions
# ==============================
def getDewPoint(airTemperature, relativeHumidity):
    """Compute the dew point in °C."""
    A = 17.625
    B = 243.04
    alpha = ((A * airTemperature) / (B + airTemperature)) + math.log(relativeHumidity / 100.0)
    return (B * alpha) / (A - alpha)

def read_previous_state():
    """Read the previous heater state from file."""
    if os.path.exists(STATE_FILE):
        with open(STATE_FILE, "r") as f:
            return f.read().strip()
    return None

def save_current_state(state):
    """Save the current heater state to file, creating temp folder if missing."""
    if not os.path.exists(TEMP_DIR):
        os.makedirs(TEMP_DIR)
    with open(STATE_FILE, "w" ) as f:
        f.write(state)

def write_log_entry(heater_status):
    """Write a log entry in YYYY/MM/DD folder structure."""
    now = datetime.now()
    year_folder = os.path.join(BASE_DIR, now.strftime("%Y"))
    month_folder = os.path.join(year_folder, now.strftime("%m"))
    day_folder = os.path.join(month_folder, now.strftime("%d"))

    # Create the folder structure if missing
    os.makedirs(day_folder, exist_ok=True)

    file_path = os.path.join(day_folder, f"{now.strftime('%Y-%m-%d')}_HeaterLog.csv")

    # Write header if file doesn't exist
    if not os.path.isfile(file_path):
        with open(file_path, mode="w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["Timestamp", "Heater state"])

    # Append log entry
    with open(file_path, mode="a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([now.strftime("%Y-%m-%d %H:%M:%S"), heater_status])

# ==============================
# Main execution
# ==============================
if __name__ == "__main__":
    # Initialize GPIO
    g.setmode(g.BOARD)
    g.setup(GPIO_PIN, g.OUT)

    # Read sensor
    bus = SMBus(PORT)
    calibration_params = bme280.load_calibration_params(bus, ADDRESS)
    bmeData = bme280.sample(bus, ADDRESS, calibration_params)

    airTemperature = bmeData.temperature
    relativeHumidity = bmeData.humidity
    dewPoint = getDewPoint(airTemperature, relativeHumidity)

    # Heater decision
    previous_state = read_previous_state()
    current_state = previous_state  # Default to no change

    if relativeHumidity >= 90 or airTemperature <= dewPoint + 2:
        # Turn ON
        g.output(GPIO_PIN, 0)
        current_state = "ON"
    elif relativeHumidity < 90 and airTemperature >= dewPoint + 3:
        # Turn OFF
        g.output(GPIO_PIN, 1)
        g.cleanup()
        current_state = "OFF"

    # Log only if state changes
    if current_state != previous_state:
        save_current_state(current_state)
        if current_state is not None:
            write_log_entry(f"The heater is {current_state.lower()}")

    # Done
