import struct
import sys
import os
import datetime
import matplotlib.pyplot as plt
import numpy as np
""" 
This script is used to read metrics from binary files and calculates throughput over time.
"""
def read_metrics(file_path):
    samples = []
    struct_format = 'iqq'  # 'i' for int, 'q' for long long (tv_sec and tv_nsec)
    struct_size = struct.calcsize(struct_format)

    with open(file_path, 'rb') as f:
        while True:
            bytes_read = f.read(struct_size)
            if not bytes_read or len(bytes_read) < struct_size:
                break
            data = struct.unpack(struct_format, bytes_read)
            metric_value = data[0]
            timestamp_sec = data[1]
            timestamp_nsec = data[2]
            timestamp = timestamp_sec + timestamp_nsec / 1e9
            samples.append((timestamp, metric_value))
    return samples

def calculate_throughput(samples, interval=0.1):
    """
    Calculate throughput over time by binning data into intervals.

    :param samples: List of tuples (timestamp, data_value)
    :param interval: Time interval in seconds for binning
    :return: List of tuples (timestamp, throughput)
    """
    if not samples:
        return []

    start_time = samples[0][0]
    end_time = samples[-1][0]
    num_bins = int((end_time - start_time) / interval) + 1

    bins = [0] * num_bins

    for timestamp, data_value in samples:
        bin_index = int((timestamp - start_time) / interval)
        if 0 <= bin_index < num_bins:
            bins[bin_index] += data_value

    throughputs = []
    for i, total_data in enumerate(bins):
        timestamp = start_time + i * interval
        throughput = total_data / interval  # Bytes per second
        throughputs.append((timestamp, throughput))

    return throughputs


def calculate_throughput2(samples):
    throughputs = []
    for i in range(1, len(samples)):
        delta_time = samples[i][0] - samples[i-1][0]
        if delta_time > 0:
            data_size = samples[i][1]
            throughput = data_size / delta_time  # Bytes per second
            timestamp = samples[i][0]
            throughputs.append((timestamp, throughput))
    return throughputs

def plot_throughputs(throughputs_dict, save_path=None):
    plt.figure(figsize=(12, 6))
    for label, data in throughputs_dict.items():
        if not data:
            continue
        timestamps = [datetime.datetime.fromtimestamp(ts) for ts, _ in data]
        values = [value for _, value in data]
        plt.plot(timestamps, values, label=label)
    plt.title('Throughput Over Time')
    plt.xlabel('Time')
    plt.ylabel('Throughput (Bytes/sec)')
    plt.legend()
    plt.grid(True)
    if save_path:
        plt.savefig(save_path, format='png')
        print(f"Plot saved to {save_path}")
    plt.show()

def calculate_average_throughput(throughputs):
    # Calculate average throughput for a list of throughput data
    if not throughputs:
        return 0
    total = sum(value for _, value in throughputs)
    return total / len(throughputs)

def calculate_overall_throughput(throughputs_list):
    # Synchronize timestamps and calculate min throughput at each time point
    # First, collect all timestamps
    all_timestamps = sorted(set(ts for throughputs in throughputs_list for ts, _ in throughputs))

    # Interpolate throughputs to these timestamps
    interpolated_throughputs = []
    for throughputs in throughputs_list:
        timestamps = [ts for ts, _ in throughputs]
        values = [val for _, val in throughputs]
        interp_values = np.interp(all_timestamps, timestamps, values)
        interpolated_throughputs.append(interp_values)

    # Calculate overall throughput as the minimum throughput at each time point
    overall_throughput_values = np.min(interpolated_throughputs, axis=0)
    overall_throughput = list(zip(all_timestamps, overall_throughput_values))
    return overall_throughput



def main():
    # Define the metric files and labels
    metric_files = {
        'Input Throughput': 'input_throughput_metrics.sdb',
        'Buffer Write Throughput': 'buffer_write_throughput_metrics.sdb',
        'Buffer Read Throughput': 'buffer_read_throughput_metrics.sdb',
        'Output Throughput': 'output_throughput_metrics.sdb'
    }

    # Read and process each metric
    throughputs_dict = {}
    throughputs_list = []
    averages = {}  # Dictionary to store average throughputs for each label
    for label, file_name in metric_files.items():
        if not os.path.exists(file_name):
            print(f"File {file_name} not found. Skipping.")
            continue
        samples = read_metrics(file_name)
        if len(samples) < 2:
            print(f"Not enough data in {file_name} to calculate throughput.")
            continue
        throughputs = calculate_throughput(samples)
        throughputs_dict[label] = throughputs
        throughputs_list.append(throughputs)
        
        # Calculate and store the average throughput
        avg_throughput = calculate_average_throughput(throughputs)
        averages[label] = avg_throughput
        print(f"Average {label}: {avg_throughput:.2f} Bytes/sec")

    # Plot individual throughputs and save the plot
    plot_throughputs(throughputs_dict, save_path="individual_throughputs.png")

    # Calculate and plot overall throughput
    overall_throughput = calculate_overall_throughput(throughputs_list)
    if overall_throughput:
        overall_avg = calculate_average_throughput(overall_throughput)
        print(f"Overall Average Throughput: {overall_avg:.2f} Bytes/sec")
        
        plt.figure(figsize=(12, 6))
        timestamps = [datetime.datetime.fromtimestamp(ts) for ts, _ in overall_throughput]
        values = [value for _, value in overall_throughput]
        plt.plot(timestamps, values, label='Overall Throughput', color='black')
        plt.title('Overall Throughput Over Time')
        plt.xlabel('Time')
        plt.ylabel('Throughput (Bytes/sec)')
        plt.legend()
        plt.grid(True)
        plt.savefig("overall_throughput.png", format='png')
        print("Overall throughput plot saved to overall_throughput.png")
        plt.show()
    else:
        print("Not enough data to calculate overall throughput.")

if __name__ == "__main__":
    main()
