#!/usr/bin/env python3
import os
import csv
from datetime import datetime, time
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import numpy as np

# === SETTINGS ===
BASE_DIR = "/var/www/callisto/weatherlogs"
TEMP_DIR = os.path.join(BASE_DIR, "temp")
STATE_FILE = os.path.join(TEMP_DIR, "heater_state.txt")

# === FIND TODAY'S CSV ===
now = datetime.now()
year_folder = os.path.join(BASE_DIR, now.strftime("%Y"))
month_folder = os.path.join(year_folder, now.strftime("%m"))
day_folder = os.path.join(month_folder, now.strftime("%d"))

csv_file = os.path.join(day_folder, f"{now.strftime('%Y-%m-%d')}_weather_readouts.csv")
if not os.path.isfile(csv_file):
    print(f"No data file found: {csv_file}")
    exit(1)

# === LOAD DATA ===
times = []
temperatures = []
humidities = []
dewpoints = []
pressures = []

with open(csv_file, newline="") as f:
    reader = csv.reader(f)
    header = next(reader, None)  # Skip header row

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
        except ValueError as e:
            print(f"Skipping row {row} due to error: {e}")
            continue

if not times:
    print("No valid data found in CSV.")
    exit(1)

# === LOAD HEATER STATE FILE ===
heater_times = []
heater_states = []
if os.path.isfile(STATE_FILE):
    with open(STATE_FILE, "r") as f:
        for line in f:
            try:
                ts_str, state_str = line.strip().split(",")
                ts = datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S")
                heater_times.append(ts)
                heater_states.append(1 if state_str.strip().lower() == "on" else 0)
            except ValueError:
                continue

# === FUNCTION TO BREAK LINES AT GAPS ===
def break_on_gaps(times_list, values_list, max_gap_minutes=30):
    times_new = [times_list[0]]
    values_new = [values_list[0]]
    for i in range(1, len(times_list)):
        gap = (times_list[i] - times_list[i-1]).total_seconds() / 60
        if gap > max_gap_minutes:
            times_new.append(np.nan)
            values_new.append(np.nan)
        times_new.append(times_list[i])
        values_new.append(values_list[i])
    return times_new, values_new

# Break gaps for plotting
times_t, temps_t = break_on_gaps(times, temperatures)
times_d, dew_d = break_on_gaps(times, dewpoints)
times_h, hum_h = break_on_gaps(times, humidities)
times_p, pres_p = break_on_gaps(times, pressures)

# === CREATE STACKED PLOTS ===
fig, axes = plt.subplots(4 if heater_times else 3, 1, figsize=(12, 12 if heater_times else 10), sharex=True)

# --- 1. Temperature & Dew Point ---
axes[0].plot(times_t, temps_t, label="Temperature (°C)", color="red")
axes[0].plot(times_d, dew_d, label="Dew Point (°C)", color="green")
axes[0].set_ylabel("Temp / Dew (°C)")
axes[0].legend(loc="upper left")
axes[0].set_title(f"Weather Data - {now.strftime('%Y-%m-%d')}")
axes[0].grid(True)

# --- 2. Humidity ---
axes[1].plot(times_h, hum_h, label="Humidity (%)", color="blue")
axes[1].set_ylabel("Humidity (%)")
axes[1].legend(loc="upper left")
axes[1].grid(True)

# --- 3. Pressure ---
axes[2].plot(times_p, pres_p, label="Pressure (hPa)", color="purple")
axes[2].set_ylabel("Pressure (hPa)")
axes[2].legend(loc="upper left")
axes[2].grid(True)

# --- 4. Heater State (optional) ---
if heater_times:
    axes[3].step(heater_times, heater_states, where="post", label="Heater ON/OFF", color="orange")
    axes[3].set_ylabel("Heater State")
    axes[3].set_yticks([0, 1])
    axes[3].set_yticklabels(["OFF", "ON"])
    axes[3].legend(loc="upper left")
    axes[3].grid(True)

# === FORCE Y-AXIS TO INCLUDE 0 WITH TOP MARGIN ===
# Temperature & Dew Point
y_min_temp = min([v for v in temps_t + dew_d if not np.isnan(v)])
y_max_temp = max([v for v in temps_t + dew_d if not np.isnan(v)])
axes[0].set_ylim(min(y_min_temp, 0), max(y_max_temp + 8, 8))  # slightly bigger top margin

# Humidity
y_min_hum = min([v for v in hum_h if not np.isnan(v)])
y_max_hum = max([v for v in hum_h if not np.isnan(v)])
axes[1].set_ylim(min(y_min_hum, 0), max(y_max_hum + 8, 8))

# Pressure - autoscale

# === FORCE X-AXIS TO 24 HOURS ===
midnight_start = datetime.combine(now.date(), time(0, 0))
midnight_end = datetime.combine(now.date(), time(23, 59))
for ax in axes:
    ax.set_xlim(midnight_start, midnight_end)

# Format X-axis
axes[-1].set_xlabel("Time (HH:MM)")
axes[-1].xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
axes[-1].xaxis.set_major_locator(mdates.HourLocator(interval=1))
fig.autofmt_xdate()

plt.tight_layout()

# === SAVE SINGLE PNG ===
os.makedirs(day_folder, exist_ok=True)
chart_file = os.path.join(day_folder, "weather_chart.png")
plt.savefig(chart_file, dpi=150)
plt.close()

print(f"Chart saved: {chart_file}")
