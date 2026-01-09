import matplotlib.pyplot as plt
import matplotlib.animation as animation
import pandas as pd
import random
from datetime import datetime, timedelta

# Simulated CSV file for demonstration
CSV_FILE = "weather_data.csv"

def generate_sample_data():
    """Generates a sample CSV file with time and temperature."""
    start_time = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
    with open(CSV_FILE, "w") as f:
        f.write("timestamp,temperature\n")
        for i in range(1440):  # Simulating 24 hours of data (1440 minutes)
            timestamp = (start_time + timedelta(minutes=i)).strftime("%Y-%m-%d %H:%M:%S")
            temperature = round(15 + random.uniform(-2, 2), 2)  # Random temperature around 15°C
            f.write(f"{timestamp},{temperature}\n")

generate_sample_data()

# Initialize plot
fig, ax = plt.subplots()
ax.set_title("Temperature Over Time")
ax.set_xlabel("Time")
ax.set_ylabel("Temperature (°C)")
ax.grid(True)
line, = ax.plot([], [], label="Temperature", color='r')
ax.legend()

# Fixed x-axis range (entire day)
def update_plot(frame):
    data = pd.read_csv(CSV_FILE, parse_dates=['timestamp'])
    ax.clear()
    ax.set_title("Temperature Over Time")
    ax.set_xlabel("Time")
    ax.set_ylabel("Temperature (°C)")
    ax.grid(True)
    ax.legend(["Temperature"], loc="upper left")

    if len(data) > 0:
        ax.plot(data['timestamp'], data['temperature'], color='r', label="Temperature")
        ax.set_xlim(data['timestamp'].iloc[0].replace(hour=0, minute=0, second=0),
                    data['timestamp'].iloc[0].replace(hour=23, minute=59, second=59))
        ax.legend()

ani = animation.FuncAnimation(fig, update_plot, interval=10000)  # Update every 10s
plt.xticks(rotation=45)
plt.show()
