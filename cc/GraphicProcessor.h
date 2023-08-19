#ifndef GRAPHICPROCESSOR_H
#define GRAPHICPROCESSOR_H

#include <cstddef>
#include <chrono>

typedef float Sample;

class GraphicProcessor {
    public:
        explicit GraphicProcessor(bool verbose);
        ~GraphicProcessor();
    private:
        bool verbose;
};

#endif // AUDIOPROCESSOR_H
