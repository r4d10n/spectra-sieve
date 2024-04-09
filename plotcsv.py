import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import pandas as pd

# Load CSV file
csv_file = 'data.csv'
data = pd.read_csv(csv_file, header=None)

# Convert data to numpy array
data_array = data.to_numpy()

print(data_array.shape)

# Number of frequency bins
num_frequency_bins = data_array.shape[1]

# Initialize plot
fig, ax = plt.subplots()
x = np.arange(num_frequency_bins)
line, = ax.plot(x, data_array[0])

# Define update function for animation
def update(frame):
    line.set_ydata(data_array[frame])
    print(frame)
    return line,

# Create animation
ani = animation.FuncAnimation(fig, update, frames=len(data_array), interval=30, blit=True)

# Show the animation
plt.show()
