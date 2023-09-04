import sys

# Specify the line identifiers
line_identifiers = [
    "PD_time",
    "RunTimeTasks",
    "KP",
    "BPM",
    "Divisions"
]

# Define dictionaries to store data for each line identifier
processDurations = {"durations": []}
runTimeTask = {"durations": []}
keypadDuration = {"durations": []}
bpm = 0
divisions = 0

# Open and read the log file
filename = sys.argv[-1]  # Replace with your actual log file name
with open(filename, "r") as file:
    for line in file:
        splitLine = line.split()
        if splitLine[0] == "MasterClock::executeScheduledBatches::Duration:":
            duration = splitLine[1]
            units = splitLine[2]
            idTag = splitLine[-1]
            duration_ms = int(duration)  # Convert duration to integer
            if idTag == line_identifiers[0]:
                processDurations["durations"].append(duration_ms)
            elif idTag == line_identifiers[1]:
                runTimeTask["durations"].append(duration_ms)
            elif line_identifiers[2] in idTag:
                keypadDuration["durations"].append(duration_ms)
        else:
            if line_identifiers[3] in splitLine[0]:
                bpm = splitLine[-1]
            elif line_identifiers[4] in splitLine[0]:
                divisions = splitLine[-1]

# Print extracted data
# print("Execute Scheduled Batches Durations:", execute_scheduled_data["durations"])
# print("Current Division Time Durations:", current_division_data["durations"])
# print("Next Division Time Durations:", next_division_data["durations"])
print(f"{line_identifiers[3]}: ", bpm)
print(f"{line_identifiers[4]}: ", divisions)

def fetchAverage(durationData):
    arrayLen = len(durationData["durations"])
    if arrayLen > 0:
        return sum(durationData["durations"]) / arrayLen

avgPD = fetchAverage(processDurations)
avgRunTime = fetchAverage(runTimeTask)
avgKP = fetchAverage(keypadDuration)


print("ProcessDuration: ", avgPD, " (ms)")
print("ProgramDuraiton: ", avgRunTime, " (ms)")
print("KeyPadDuration: ", avgKP, " (ms)")

 