#!/usr/bin/env python3
import os
import csv
from datetime import datetime, time, timedelta
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.ticker import FixedLocator, FuncFormatter
import numpy as np

# === SETTINGS ===
BASE_DIR = "/var/www/callisto/weatherlogs"

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
fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)

# --- 1. Temperature & Dew Point ---
axes[0].plot(times_t, temps_t, label="Temperature (°C)", color="red")
axes[0].plot(times_d, dew_d, label="Dew Point (°C)", color="green")
axes[0].set_ylabel("Temp / Dew [°C]")
axes[0].legend(loc="upper left")
axes[0].set_title(f"Weather Data - {now.strftime('%Y-%m-%d')}")
axes[0].grid(True)

# --- 2. Humidity ---
axes[1].plot(times_h, hum_h, label="Humidity (%)", color="blue")
axes[1].set_ylabel("Humidity [%]")
axes[1].legend(loc="upper left")
axes[1].grid(True)

# --- 3. Pressure ---
axes[2].plot(times_p, pres_p, label="Pressure (hPa)", color="purple")
axes[2].set_ylabel("Pressure [hPa]")
axes[2].legend(loc="upper left")
axes[2].grid(True)

# === FORCE Y-AXIS TO INCLUDE 0 WITH TOP MARGIN ===
# Temperature & Dew Point
y_min_temp = min([v for v in temps_t + dew_d if not np.isnan(v)])
y_max_temp = max([v for v in temps_t + dew_d if not np.isnan(v)])
axes[0].set_ylim(min(y_min_temp, 0), max(y_max_temp + 8, 8))  # slightly bigger top margin

# Humidity
y_min_hum = min([v for v in hum_h if not np.isnan(v)])
y_max_hum = max([v for v in hum_h if not np.isnan(v)])
axes[1].set_ylim(min(y_min_hum, 0), max(y_max_hum + 8, 8))  # slightly bigger top margin

# === FORCE X-AXIS TO 24 HOURS (with a 24:00 tick) ===
midnight_start = datetime.combine(now.date(), time(0, 0))
midnight_next = midnight_start + timedelta(days=1)  # 24:00 of the same day

# Set x-limits for all subplots
for ax in axes:
    ax.set_xlim(midnight_start, midnight_next)

# Build fixed tick locations every hour, including 24:00, as numeric date values
tick_datetimes = [midnight_start + timedelta(hours=h) for h in range(25)]  # 0..24
tick_locs = [mdates.date2num(dt) for dt in tick_datetimes]

def format_time(x, pos):
    dt = mdates.num2date(x)
    # Label the next day's 00:00 as "24:00"
    if dt.date() == midnight_next.date() and dt.hour == 0 and dt.minute == 0:
        return "24:00"
    return dt.strftime("%H:%M")

# Apply locator/formatter to all subplots (shared x)
for ax in axes:
    ax.xaxis.set_major_locator(FixedLocator(tick_locs))
    ax.xaxis.set_major_formatter(FuncFormatter(format_time))

# Bottom label
axes[2].set_xlabel("UTC Time (HH:MM)")

fig.autofmt_xdate()
plt.tight_layout()

# === SAVE SINGLE PNG ===
os.makedirs(day_folder, exist_ok=True)
chart_file = os.path.join(day_folder, f"weather_chart_{now.strftime('%Y-%m-%d')}.png")
plt.savefig(chart_file, dpi=150)
plt.close()

print(f"Chart saved: {chart_file}")
