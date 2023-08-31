import sys
import numpy as np
from scipy.stats import linregress, norm
import matplotlib.pyplot as plt
import argparse

def process_data(data, analysisType):
    lines = data.strip().split('\n')

    total_time = 0
    totals = []

    for line in lines:
        parts = line.split()
        duration = int(parts[1])
        if analysisType == "kp":
            if "KP" in parts[-1]:
                totals.append(total_time)
                total_time = duration
            else:
                total_time += duration
        elif analysisType == "pp":
            if "RunTimeTasks" in parts[-1]:
                totals.append(duration)
        elif analysisType == "pd":
            if "PD_time" in parts[-1]:
                totals.append(duration)

    # Add the last total if there's one remaining
    if analysisType == "kp":
        if total_time > 0:
            totals.append(total_time)

    return totals

def main():
    parser = argparse.ArgumentParser(description="Setup Config for Drum Machine.")
    parser.add_argument("-t", "--type", required=True,
                    help="Analytic Type (KeyPad (kp), Processing Durations (pd), or Program Prcoesses (pp))")
    parser.add_argument("-f", "--file", required=True, help="Input file pathh")
    args = parser.parse_args()
    file_path = args.file
    analysisType = args.type
    if type(analysisType) == str:
        analysisType = analysisType.lower()
    else:
        sys.exit(f"Incorrect format for Type: {type(analysisType)}.")
    analysisTypes = ["pp", "pd", "kp"]
    
    if analysisType in analysisTypes:
        pass
    else:
        sys.exit((f"Unrecognized Type value: {analysisType}"))

    
    try:
        with open(file_path, 'r') as file:
            data = file.read()
            totals = process_data(data, analysisType)
    except FileNotFoundError:
        print("File not found!")

        # Create a time index for the data points
    if analysisType == "kp":
        totals = totals[1:-1]
    print("List of total times for each set of 'KP' values:", totals)
    time_index = np.arange(len(totals))

    # Perform linear regression
    slope, intercept, _, _, _ = linregress(time_index, totals)

    # Calculate the regression line
    regression_line = slope * time_index + intercept

    # Create the plot
    plt.figure(figsize=(10, 6))
    plt.plot(time_index, totals, marker='o', label='totals')
    plt.plot(time_index, regression_line, color='red', label='Linear Regression')
    plt.xlabel('Time Index')
    plt.ylabel('totals Value')
    plt.title('totals with Linear Regression')
    plt.legend()
    plt.grid()
    plt.show()

    mean = np.mean(totals)
    std_dev = np.std(totals)

    # Create a range of x values for the bell curve
    x = np.linspace(min(totals), max(totals), 100)

    # Calculate the corresponding y values for the bell curve
    y = norm.pdf(x, mean, std_dev)

    # Create the plot
    plt.figure(figsize=(10, 6))
    plt.hist(totals, bins=15, density=True, alpha=0.6, color='blue', label='Data Distribution')
    plt.plot(x, y, 'r', label='Bell Curve')
    plt.axvline(mean, color='k', linestyle='dashed', linewidth=1, label='Mean')
    plt.xlabel('Data Value')
    plt.ylabel('Frequency')
    plt.title('Data Distribution with Bell Curve')
    plt.legend()
    plt.grid()
    plt.show()


if __name__ == "__main__":
    main()
