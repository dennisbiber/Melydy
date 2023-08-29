import numpy as np
import librosa
import soundfile as sf
from tensorflow.keras.models import load_model
import sys

# Load the trained model
loaded_model = load_model('trained_model.h5')  # Specify the path to your saved model file

# Load an audio file and convert it to the same format used for training
audio_file_path = sys.argv[-1]  # Specify the path to your audio file
segment_length = 2048  # Use the same segment length used during training

# Load the audio file and convert it to stereo
audio, _ = librosa.load(audio_file_path, sr=None, mono=False)

# Prepare the audio as a seed sequence (shape: (segment_length, 2))
seed_sequence = audio[:segment_length]

# Generate a sequence of audio samples
num_samples_to_generate = 100  # Adjust as needed
generated_audio = seed_sequence.copy()
print("Generated Audio Shape: ", generated_audio.shape)

for _ in range(num_samples_to_generate):
    # Predict the next audio sample
    next_sample = loaded_model.predict(generated_audio[-segment_length:][np.newaxis, :, np.newaxis])
    print("next sample shape1: ", next_sample.shape)
    next_sample = next_sample.reshape(-1)

    # Append the predicted sample value to the generated sequence
    generated_audio = np.concatenate((generated_audio, next_sample), axis=0)


# Save the generated audio as a WAV file
output_file_path = 'generated_audio.wav'  # Specify the desired output file path
sf.write(output_file_path, generated_audio, 48000)  # Transpose for stereo
