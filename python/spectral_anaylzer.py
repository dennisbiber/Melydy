import librosa
import librosa.display
import matplotlib.pyplot as plt
import numpy as np
import sys

# Load the audio file
audio_file_path = sys.argv[-1]
y, sr = librosa.load(audio_file_path)

# Compute the spectrogram
D = librosa.amplitude_to_db(librosa.stft(y), ref=np.max)

# Display the spectrogram
plt.figure(figsize=(10, 6))
librosa.display.specshow(D, sr=sr, x_axis='time', y_axis='log', cmap='viridis')
plt.title('Spectrogram')
plt.show()
