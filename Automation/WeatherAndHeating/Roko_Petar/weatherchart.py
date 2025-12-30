#!/usr/bin/env python3
import os
import csv
from datetime import datetime
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# === SETTINGS ===
BASE_DIR = "/var/www/callisto/weatherlogs"  # same as your logger

# === FIND TODAY'S CSV ===
now = datetime.now()
month_folder = os.path.join(BASE_DIR, now.strftime("%Y"), now.strftime("%m"))
csv_file = os.path.join(month_folder, f"{now.strftime('%Y-%m-%d')}_weather_readouts.csv")

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
            continue  # Skip incomplete rows
        try:
            # Use EXACT time from file (no timezone conversion, no date math)
            t = datetime.strptime(row[0], "%H:%M:%S")
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

# === CREATE CHART ===
fig, ax1 = plt.subplots(figsize=(10, 6))

ax1.plot(times, temperatures, label="Temperature (°C)", color="red")
ax1.plot(times, humidities, label="Humidity (%)", color="blue")
ax1.plot(times, dewpoints, label="Dew Point (°C)", color="green")
ax1.set_xlabel("Time (HH:MM)")
ax1.set_ylabel("Temperature / Humidity / Dew Point")

# Show times exactly as in CSV
ax1.xaxis.set_major_formatter(mdates.DateFormatter("%H:%M"))
ax1.set_xticks(times)  # force every point from CSV
fig.autofmt_xdate()

# Pressure on second y-axis
ax2 = ax1.twinx()
ax2.plot(times, pressures, label="Pressure (hPa)", color="purple", linestyle="--")
ax2.set_ylabel("Pressure (hPa)")

# Legends for both axes
lines_1, labels_1 = ax1.get_legend_handles_labels()
lines_2, labels_2 = ax2.get_legend_handles_labels()
ax1.legend(lines_1 + lines_2, labels_1 + labels_2, loc="upper left")

plt.title(f"Weather Data - {now.strftime('%Y-%m-%d')}")
plt.grid(True)
plt.tight_layout()

# === SAVE CHART ===
chart_file = csv_file.replace(".csv", "_chart.png")
plt.savefig(chart_file, dpi=150)
plt.close()

print(f"Chart saved: {chart_file}")
