#ifndef AUDIOPROCESSOR_H
#define AUDIOPROCESSOR_H

#include <cstddef>
#include <chrono>

typedef float Sample;

class AudioProcessor {
    public:
        explicit AudioProcessor(bool verbose);
        ~AudioProcessor();

        // Helper function to clip audio samples to the valid range [-1.0, 1.0]
        // void clipAudioSamples(Sample* samples, size_t count);

        // Helper function to normalize audio samples to the valid range [-1.0, 1.0]
        Sample* normalizeAudioSamples(Sample* samples, size_t count, Sample targetPeak);
        // void amplifyAudioSamples(Sample* samples, size_t count, Sample amplificationFactor);
    private:
        bool verbose;
};

#endif // AUDIOPROCESSOR_H
