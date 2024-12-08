import sys
from typing import List, Tuple

def analyze_timing_data(filename: str, batch_size: int, total_items: int) -> None:
    """
    Analyze timing data from database batch inserts.
    
    Args:
        filename: Path to file containing timing data
        batch_size: Number of items in each batch
        total_items: Total number of items inserted
    """
    # Read and parse timing data
    times: List[Tuple[float, float]] = []
    with open(filename, 'r') as f:
        for line in f:
            batch_time, total_time = map(float, line.strip().split())
            times.append((batch_time, total_time))
    
    # Remove the last timing entry as it might be incomplete
    times = times[:-1]
    
    if not times:
        print("No complete batch insert times found in the file.")
        return
    
    # Calculate batch statistics
    batch_times = [t[0] for t in times]
    min_batch_time = min(batch_times)
    max_batch_time = max(batch_times)
    avg_batch_time = sum(batch_times) / len(batch_times)
    
    # Calculate items per second for each batch
    batch_items_per_second = [batch_size / t for t in batch_times]
    min_items_per_second = min(batch_items_per_second)
    max_items_per_second = max(batch_items_per_second)
    avg_items_per_second = sum(batch_items_per_second) / len(batch_items_per_second)
    
    # Calculate overall items per second using the last total time
    overall_items_per_second = (len(times) * batch_size) / times[-1][1]
    
    # Print results
    print("\nBatch Insert Times:")
    print(f"Min: {min_batch_time:.6f} seconds")
    print(f"Max: {max_batch_time:.6f} seconds")
    print(f"Avg: {avg_batch_time:.6f} seconds")
    
    print("\nItems per Second (per batch):")
    print(f"Min: {min_items_per_second:.2f} items/s")
    print(f"Max: {max_items_per_second:.2f} items/s")
    print(f"Avg: {avg_items_per_second:.2f} items/s")
    
    print("\nOverall Performance:")
    print(f"Total items/s: {overall_items_per_second:.2f} items/s")

def main():
    if len(sys.argv) != 4:
        print("Usage: python script.py <filename> <batch_size> <total_items>")
        sys.exit(1)
    
    try:
        filename = sys.argv[1]
        batch_size = int(sys.argv[2])
        total_items = int(sys.argv[3])
        
        analyze_timing_data(filename, batch_size, total_items)
    except ValueError:
        print("Error: batch_size and total_items must be integers")
        sys.exit(1)
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {str(e)}")
        sys.exit(1)

if __name__ == "__main__":
    main()
