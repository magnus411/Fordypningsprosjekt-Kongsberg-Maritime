import struct
import sys
import os
import datetime
import matplotlib.pyplot as plt

"""
This script is used to read occupancy metrics from binary files and plot the buffer occupancy over time.
"""
def read_occupancy_metrics(file_path):
    samples = []
    struct_format = 'iqq'  # 'i' for int, 'q' for long long (tv_sec and tv_nsec)
    struct_size = struct.calcsize(struct_format)
    print(f"Reading {file_path} with struct format '{struct_format}' and size {struct_size} bytes per sample.")

    with open(file_path, 'rb') as f:
        sample_count = 0
        while True:
            bytes_read = f.read(struct_size)
            if not bytes_read or len(bytes_read) < struct_size:
                break
            data = struct.unpack(struct_format, bytes_read)
            occupancy_value = data[0]  # Occupancy percentage
            timestamp_sec = data[1]
            timestamp_nsec = data[2]
            timestamp = timestamp_sec + timestamp_nsec / 1e9
            samples.append((timestamp, occupancy_value))
            sample_count += 1
        print(f"Read {sample_count} samples from {file_path}")
    return samples

def plot_occupancy(samples):
    if not samples:
        print("No occupancy data to plot.")
        return

    timestamps = [datetime.datetime.fromtimestamp(ts) for ts, _ in samples]
    values = [value for _, value in samples]

    plt.figure(figsize=(12, 6))
    plt.plot(timestamps, values, label='Buffer Occupancy')
    plt.title('Buffer Occupancy Over Time')
    plt.xlabel('Time')
    plt.ylabel('Occupancy (%)')
    plt.legend()
    plt.grid(True)
    plt.show()

def main():
    file_name = 'occupancy_metrics.sdb'

    if not os.path.exists(file_name):
        print(f"File {file_name} not found.")
        sys.exit(1)

    samples = read_occupancy_metrics(file_name)

    plot_occupancy(samples)

if __name__ == "__main__":
    main()
