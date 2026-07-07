#ifndef WAV_RECORDER_H
#define WAV_RECORDER_H

#include <Arduino.h>

bool iniciarGrabacion();
void calibrarRuidoAmbiente();
bool detectarSonido();
bool grabarWAV(const char* nombreArchivo, uint8_t segundos);

#endif
