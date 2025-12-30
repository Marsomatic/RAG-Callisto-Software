import numpy as np
import matplotlib.pyplot as plt

# Generate some example data
time_utc = np.linspace(0, 24, 100)  # Time from 0 to 24 hours

temperature = (
    np.piecewise(time_utc,
                 [time_utc < 6, (time_utc >= 6) & (time_utc < 15), time_utc >= 15],
                 [lambda x: np.sin(x) - 1, lambda x: np.sin(x) * 5 + 2, lambda x: -np.sin(x) * 5 + 5])
)

# Create plot
plt.figure(figsize=(8, 6))
plt.plot(time_utc, temperature, 'r-', label=f'Temperature {temperature[-1]:.1f} °C on: 2025-02-08 at: 22:15:02 UT')

# Labels and title
plt.xlabel("Time [UT]")
plt.ylabel("Temperature [°C]")
plt.title("Weather data in Freienbach, Switzerland")
plt.legend()

# Customize x-axis
plt.xlim(0, 24)
plt.xticks(np.arange(0, 25, 3))  # Ticks from 0 to 24 every 3 hours
plt.grid(True)

# Show plot
plt.show()