#include "AudioProcessor.h"
#include <cstdio>
#include <cmath>

AudioProcessor::AudioProcessor(bool verbose) :
    verbose(verbose) {
    if (verbose) {
        printf("      AudioProcessor::AudioProcessor::Entered.\n");
    }
}

AudioProcessor::~AudioProcessor() {
}

// void AudioProcessor::clipAudioSamples(Sample* samples, size_t count) {
//     for (size_t i = 0; i < count; i++) {
//         if (samples[i] > 1.0) {
//             samples[i] = 1.0;
//         } else if (samples[i] < -1.0) {
//             samples[i] = -1.0;
//         }
//     }
// }

Sample* AudioProcessor::normalizeAudioSamples(Sample* samples, size_t count, Sample targetPeak) {
    // Find the maximum absolute value in the samples
    Sample maxAbsValue = 0.0;
    for (size_t i = 0; i < count; i++) {
        Sample absValue = std::abs(samples[i]);
        if (absValue > maxAbsValue) {
            maxAbsValue = absValue;
        }
    }
    if (verbose) {
        printf("      AudioProcessor::normalizeAudioSamples:maxAbsValue: %f.\n", maxAbsValue);
    }

    // Calculate the scaling factor based on the target peak value
    Sample scaleFactor = targetPeak / maxAbsValue;
    for (size_t i = 0; i < count; i++) {
        samples[i] *= scaleFactor;
    }
    return samples;
}

// void AudioProcessor::amplifyAudioSamples(Sample* samples, size_t count, Sample amplificationFactor) {
//     for (size_t i = 0; i < count; i++) {
//         samples[i] *= amplificationFactor;

//         // Clip the amplified samples to the valid range
//         if (samples[i] > 1.0) {
//             samples[i] = 1.0;
//         } else if (samples[i] < -1.0) {
//             samples[i] = -1.0;
//         }
//     }
// }
