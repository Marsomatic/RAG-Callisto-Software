#!/usr/bin/env python3
import os
import csv
import math
from datetime import datetime, time, timedelta
from smbus2 import SMBus
import bme280
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.ticker import FixedLocator, FuncFormatter
import numpy as np

# === SETTINGS ===
BASE_DIR = "/var/www/callisto/enclosurelogs"  # changed from weatherlogs
PORT = 1
ADDRESS = 0x76  # or 0x77 depending on your sensor

# === INITIALIZE SENSOR ===
bus = SMBus(PORT)
calibration_params = bme280.load_calibration_params(bus, ADDRESS)
data = bme280.sample(bus, ADDRESS, calibration_params)
now = datetime.now()

# --- Function: Dew Point ---
def getDewPoint(airTemperature, relativeHumidity):
    A, B = 17.625, 243.04
    alpha = ((A * airTemperature) / (B + airTemperature)) + math.log(relativeHumidity/100.0)
    return (B * alpha) / (A - alpha)

dew_point_c = getDewPoint(data.temperature, data.humidity)

# === CREATE FOLDERS & CSV PATH ===
year_folder = os.path.join(BASE_DIR, now.strftime("%Y"))
month_folder = os.path.join(year_folder, now.strftime("%m"))
day_folder = os.path.join(month_folder, now.strftime("%d"))
os.makedirs(day_folder, exist_ok=True)

csv_file = os.path.join(day_folder, f"{now.strftime('%Y-%m-%d')}_enclosure_data.csv")

# --- Write header if new file ---
if not os.path.isfile(csv_file):
    with open(csv_file, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["Time", "Temperature_C", "Pressure_hPa", "Humidity_%", "DewPoint_C"])

# --- Append new reading ---
with open(csv_file, "a", newline="") as f:
    writer = csv.writer(f)
    writer.writerow([
        now.strftime("%H:%M:%S"),
        round(data.temperature, 2),
        round(data.pressure, 2),
        round(data.humidity, 2),
        round(dew_point_c, 2)
    ])

# === LOAD DATA BACK FOR CHART ===
times, temperatures, humidities, dewpoints, pressures = [], [], [], [], []
with open(csv_file, newline="") as f:
    reader = csv.reader(f)
    header = next(reader, None)
    for row in reader:
        if len(row) < 5:
            continue
        try:
            t = datetime.combine(now.date(), datetime.strptime(row[0], "%H:%M:%S").time())
            times.append(t)
            temperatures.append(float(row[1]))
            pressures.append(float(row[2]))
            humidities.append(float(row[3]))
            dewpoints.append(float(row[4]))
        except ValueError:
            continue

if not times:
    print("No valid data found, skipping chart.")
    exit(0)

# === HELPER FUNCTION TO BREAK ON GAPS ===
def break_on_gaps(times_list, values_list, max_gap_minutes=30):
    times_new, values_new = [times_list[0]], [values_list[0]]
    for i in range(1, len(times_list)):
        gap = (times_list[i] - times_list[i-1]).total_seconds() / 60
        if gap > max_gap_minutes:
            times_new.append(np.nan)
            values_new.append(np.nan)
        times_new.append(times_list[i])
        values_new.append(values_list[i])
    return times_new, values_new

# --- Prepare data with gaps ---
times_t, temps_t = break_on_gaps(times, temperatures)
times_d, dew_d = break_on_gaps(times, dewpoints)
times_h, hum_h = break_on_gaps(times, humidities)
times_p, pres_p = break_on_gaps(times, pressures)

# === PLOT STACKED CHART ===
fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)

# 1. Temp + Dew Point
axes[0].plot(times_t, temps_t, label="Temperature (°C)", color="red")
axes[0].plot(times_d, dew_d, label="Dew Point (°C)", color="green")
axes[0].set_ylabel("Temp / Dew [°C]")
axes[0].legend(loc="upper left")
axes[0].set_title(f"Enclosure Data - {now.strftime('%Y-%m-%d')}")
axes[0].grid(True)

# 2. Humidity
axes[1].plot(times_h, hum_h, label="Humidity (%)", color="blue")
axes[1].set_ylabel("Humidity [%]")
axes[1].legend(loc="upper left")
axes[1].grid(True)

# 3. Pressure
axes[2].plot(times_p, pres_p, label="Pressure (hPa)", color="purple")
axes[2].set_ylabel("Pressure [hPa]")
axes[2].legend(loc="upper left")
axes[2].grid(True)

# === Y-axis adjustments ===
def safe_minmax(values):
    vals = [v for v in values if not np.isnan(v)]
    return min(vals), max(vals)

ymin, ymax = safe_minmax(temps_t + dew_d)
axes[0].set_ylim(min(ymin, 0), ymax + 8)

ymin, ymax = safe_minmax(hum_h)
axes[1].set_ylim(min(ymin, 0), ymax + 8)

# === X-axis (24 hours) ===
midnight_start = datetime.combine(now.date(), time(0, 0))
midnight_next = midnight_start + timedelta(days=1)
for ax in axes:
    ax.set_xlim(midnight_start, midnight_next)

tick_datetimes = [midnight_start + timedelta(hours=h) for h in range(25)]
tick_locs = [mdates.date2num(dt) for dt in tick_datetimes]

def format_time(x, pos):
    dt = mdates.num2date(x)
    if dt.date() == midnight_next.date() and dt.hour == 0 and dt.minute == 0:
        return "24:00"
    return dt.strftime("%H:%M")

for ax in axes:
    ax.xaxis.set_major_locator(FixedLocator(tick_locs))
    ax.xaxis.set_major_formatter(FuncFormatter(format_time))

axes[2].set_xlabel("UTC Time (HH:MM)")

fig.autofmt_xdate()
plt.tight_layout()

# === SAVE PNG ===
chart_file = os.path.join(day_folder, f"enclosure_data_{now.strftime('%Y-%m-%d')}.png")
plt.savefig(chart_file, dpi=150)
plt.close()

print(f"Data logged and chart saved: {chart_file}")
