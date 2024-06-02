import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import pandas as pd
import sys
from collections import deque

# Initialize an empty deque to store incoming data
data_queue = deque()

# Function to read lines from stdin and append to the queue
def read_stdin():
    for line in sys.stdin:
        # Convert the line to a list of floats and append to the deque
        data = list(map(float, line.strip().split(',')))
        data_queue.append(data)
        if len(data_queue) > 100:  # Limit the queue size to the last 100 frames for memory efficiency
            data_queue.popleft()

# Initialize plot
fig, ax = plt.subplots()
line, = ax.plot([], [])

# Set plot labels and title
ax.set_title('Live Data Plot from stdin')
ax.set_xlabel('Frequency Bin')
ax.set_ylabel('Amplitude')

# Initialize the plot limits
ax.set_xlim(0, 100)  # Assume 100 frequency bins for initialization
ax.set_ylim(-1, 1)  # Assume amplitude range from -1 to 1 for initialization

# Function to initialize the plot
def init():
    line.set_data([], [])
    return line,

# Define update function for animation
def update(frame):
    if data_queue:
        data = data_queue.popleft()
        x = np.arange(len(data))
        line.set_data(x, data)
        ax.set_xlim(0, len(data) - 1)
        ax.set_ylim(min(data)-10, max(data)+10)
    return line,

# Create animation
ani = animation.FuncAnimation(fig, update, init_func=init, frames=None, interval=100, blit=True)

# Start reading stdin in a separate thread
import threading
stdin_thread = threading.Thread(target=read_stdin)
stdin_thread.daemon = True
stdin_thread.start()

# Show the animation
plt.show()
