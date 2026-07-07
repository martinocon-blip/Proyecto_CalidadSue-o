#ifndef SNORE_DETECTOR_H
#define SNORE_DETECTOR_H

#include <Arduino.h>
#include <SD.h>

struct SnoreAnalysisResult
{
    bool esRonquido;
    int ronquidosDetectados;
    uint32_t nivelMedio;
    uint32_t nivelMaximo;
};

class SnoreDetector
{
public:
    SnoreDetector();

    SnoreAnalysisResult analizarArchivoWAV(const char* rutaArchivo);

private:
    static const int HEADER_WAV_BYTES = 44;
    static const int BLOCK_SAMPLES = 512;

    int16_t buffer[BLOCK_SAMPLES];

    uint32_t calcularRMS(const int16_t* samples, int count);
    float calcularCrucesPorCero(const int16_t* samples, int count);
};

#endif