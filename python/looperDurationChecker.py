import sys

def process_data(data):
    lines = data.strip().split('\n')

    total_time = 0
    totals = []

    for line in lines:
        parts = line.split()
        duration = int(parts[1])
        if "KP" in parts[-1]:
            totals.append(total_time)
            total_time = duration
        else:
            total_time += duration

    # Add the last total if there's one remaining
    if total_time > 0:
        totals.append(total_time)

    return totals

def main():
    file_path = sys.argv[-1]
    
    try:
        with open(file_path, 'r') as file:
            data = file.read()
            totals = process_data(data)
            print("List of total times for each set of 'KP' values:", totals)
    except FileNotFoundError:
        print("File not found!")

if __name__ == "__main__":
    main()
