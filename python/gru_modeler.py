import os
import librosa
import numpy as np
import tensorflow as tf
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Conv1D, MaxPooling1D, Flatten, GRU, Dense, Bidirectional, LSTM
from sklearn.model_selection import train_test_split
import sys

# Path to the directory containing wave files
input_dir = sys.argv[-1]

# List to hold segments of audio data
audio_segments = []

# Define the length of each segment in samples
segment_length = 2048

# Iterate through all wave files in the directory
for filename in os.listdir(input_dir):
    if filename.endswith(".mp3") or filename.endswith(".wav"):
        # Load audio file
        file_path = os.path.join(input_dir, filename)
        audio, _ = librosa.load(file_path, sr=None, mono=False)  # Allow stereo
        
        # Convert stereo to mono if needed
        if audio.shape[0] == 2:
            audio = np.mean(audio, axis=0)
        
        # Break audio into segments
        num_segments = len(audio) // segment_length
        for i in range(num_segments):
            start_idx = i * segment_length
            end_idx = start_idx + segment_length
            segment = audio[start_idx:end_idx]
            audio_segments.append(segment)

# Convert the list of segments into a numpy array
audio_data = np.array(audio_segments)

audio_data = audio_data.reshape(-1, segment_length, 1)
print("Shape of audio_data:", audio_data.shape)

# Split the data into training and validation sets
train_audio_data, val_audio_data = train_test_split(audio_data, test_size=0.25, random_state=42)
print("Shape of train_audio_data:", train_audio_data.shape)  # Print shape of train_audio_data
print("Shape of val_audio_data:", val_audio_data.shape)

# Define the input shape for your GRU model (shape of a single segment)
input_shape = (segment_length, 1)  # Adjust the shape based on your data
print("Input shape for Conv1D:", input_shape)

# Create a Sequential model
model = Sequential()

# Add Conv1D layers for local pattern extraction
model.add(Conv1D(filters=32, kernel_size=3, activation='relu', input_shape=input_shape))
model.add(MaxPooling1D(pool_size=2))
print("Shape after MaxPooling1D:", model.output_shape)

# Add a GRU layer to capture temporal dependencies
model.add(Bidirectional(GRU(units=64, return_sequences=True)))
print("Shape after first GRU layer:", model.output_shape)

# Add another GRU layer for the second pass
model.add(Bidirectional(LSTM(units=64, return_sequences=False))) # Set return_sequences=False for the last GRU
print("Shape after second GRU layer:", model.output_shape)

# Add a Dense layer for output
your_output_dimension = 48000 # Define the number of output units
model.add(Dense(units=your_output_dimension, activation='tanh'))

# Compile the model
model.compile(optimizer='SGD', loss='CategoricalCrossentropy')

# Print a summary of the model architecture
model.summary()

# Define training parameters
your_num_epochs = 7  # Define the number of epochs
your_batch_size = 32   # Define the batch size

# Dummy target data (replace this with your actual target data)
# Generate target data
train_target_data = np.random.rand(len(train_audio_data), your_output_dimension)
val_target_data = np.random.rand(len(val_audio_data), your_output_dimension)


# Train the model
model.fit(x=train_audio_data, y=train_target_data, validation_data=(val_audio_data, val_target_data),
          epochs=your_num_epochs, batch_size=your_batch_size)

model.save('trained_model.h5')