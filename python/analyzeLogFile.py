import sys

# Specify the line identifiers
line_identifiers = [
    "MasterClock::executeScheduledBatches::Duration: "
]

# Define dictionaries to store data for each line identifier
execute_scheduled_data = {"durations": []}

# Open and read the log file
filename = sys.argv[-1]  # Replace with your actual log file name
with open(filename, "r") as file:
    for line in file:
        for identifier in line_identifiers:
            if line.startswith(identifier):
                duration_str = line[len(identifier):].strip()  # Extract the duration string
                duration, unit = duration_str.split()  # Split the duration and unit
                duration_ms = int(duration)  # Convert duration to integer
                if identifier == line_identifiers[0]:
                    execute_scheduled_data["durations"].append(duration_ms)

# Print extracted data
# print("Execute Scheduled Batches Durations:", execute_scheduled_data["durations"])
# print("Current Division Time Durations:", current_division_data["durations"])
# print("Next Division Time Durations:", next_division_data["durations"])

differencesList = []
data = execute_scheduled_data["durations"]
listLen = len(data)
for idx in range(listLen):
    if idx < listLen - 1:
        maths = data[idx + 1] - data[idx]
        if maths >= 0 and maths <= 10000:
            differencesList.append(maths)
diffListLen = len(differencesList)
if diffListLen > 1000:
    average_positive_difference_start = sum(differencesList[0:int(diffListLen/2)]) / int(diffListLen/2)
    average_positive_difference_end = sum(differencesList[int(diffListLen/2)::]) / int(diffListLen/2)
    print("Durations Start: ", average_positive_difference_start, " (ms)")
    print("Durations End: ", average_positive_difference_end, " (ms)")
else:
    average_positive_difference = sum(differencesList) / diffListLen
    print("Durations: ", average_positive_difference, " (ms)")

