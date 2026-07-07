#include "SnoreDetector.h"

SnoreDetector::SnoreDetector()
{
}

uint32_t SnoreDetector::calcularRMS(const int16_t* samples, int count)
{
    if (count <= 0)
    {
        return 0;
    }

    uint64_t suma = 0;

    for (int i = 0; i < count; i++)
    {
        int32_t s = samples[i];
        suma += (uint64_t)(s * s);
    }

    return sqrt((double)suma / count);
}

float SnoreDetector::calcularCrucesPorCero(const int16_t* samples, int count)
{
    if (count <= 1)
    {
        return 1.0;
    }

    int cruces = 0;

    for (int i = 1; i < count; i++)
    {
        if ((samples[i - 1] >= 0 && samples[i] < 0) ||
            (samples[i - 1] < 0 && samples[i] >= 0))
        {
            cruces++;
        }
    }

    return (float)cruces / (float)count;
}

static bool eventoPareceRonquido(int bloquesEvento, uint32_t nivelEventoMax, uint64_t nivelEventoSuma)
{
    const int BLOQUES_MIN_RONQUIDO = 4;
    const int BLOQUES_MAX_RONQUIDO = 45;
    const float FACTOR_PICO_TOS = 2.6;

    if (bloquesEvento < BLOQUES_MIN_RONQUIDO)
    {
        return false;
    }

    if (bloquesEvento > BLOQUES_MAX_RONQUIDO)
    {
        return false;
    }

    if (bloquesEvento <= 0)
    {
        return false;
    }

    uint32_t nivelEventoMedio = nivelEventoSuma / bloquesEvento;

    if (nivelEventoMedio == 0)
    {
        return false;
    }

    bool noPareceTos = nivelEventoMax < (nivelEventoMedio * FACTOR_PICO_TOS);

    return noPareceTos;
}

SnoreAnalysisResult SnoreDetector::analizarArchivoWAV(const char* rutaArchivo)
{
    SnoreAnalysisResult resultado;
    resultado.esRonquido = false;
    resultado.ronquidosDetectados = 0;
    resultado.nivelMedio = 0;
    resultado.nivelMaximo = 0;

    File archivo = SD.open(rutaArchivo, FILE_READ);

    if (!archivo)
    {
        Serial.print("ERROR: No se pudo abrir para analizar: ");
        Serial.println(rutaArchivo);
        return resultado;
    }

    if (archivo.size() <= HEADER_WAV_BYTES)
    {
        Serial.println("ERROR: Archivo WAV demasiado pequeno");
        archivo.close();
        return resultado;
    }

    archivo.seek(HEADER_WAV_BYTES);

    uint64_t sumaNiveles = 0;
    uint32_t bloquesTotales = 0;

    bool dentroEvento = false;
    int bloquesEvento = 0;
    int bloquesSilencio = 0;

    uint32_t nivelEventoMax = 0;
    uint64_t nivelEventoSuma = 0;

    const uint32_t UMBRAL_RMS_MINIMO = 700;
    const float ZCR_MAXIMO_GRAVE = 0.18;
    const int BLOQUES_SILENCIO_FIN = 4;

    while (archivo.available())
    {
        int bytesLeidos = archivo.read((uint8_t*)buffer, sizeof(buffer));
        int muestras = bytesLeidos / sizeof(int16_t);

        if (muestras <= 0)
        {
            break;
        }

        uint32_t rms = calcularRMS(buffer, muestras);
        float zcr = calcularCrucesPorCero(buffer, muestras);

        sumaNiveles += rms;
        bloquesTotales++;

        if (rms > resultado.nivelMaximo)
        {
            resultado.nivelMaximo = rms;
        }

        bool bloqueSospechoso = (rms > UMBRAL_RMS_MINIMO) && (zcr < ZCR_MAXIMO_GRAVE);

        if (bloqueSospechoso)
        {
            if (!dentroEvento)
            {
                dentroEvento = true;
                bloquesEvento = 0;
                bloquesSilencio = 0;
                nivelEventoMax = 0;
                nivelEventoSuma = 0;
            }

            bloquesEvento++;
            bloquesSilencio = 0;
            nivelEventoSuma += rms;

            if (rms > nivelEventoMax)
            {
                nivelEventoMax = rms;
            }
        }
        else
        {
            if (dentroEvento)
            {
                bloquesSilencio++;

                if (bloquesSilencio >= BLOQUES_SILENCIO_FIN)
                {
                    if (eventoPareceRonquido(bloquesEvento, nivelEventoMax, nivelEventoSuma))
                    {
                        resultado.ronquidosDetectados++;
                    }

                    dentroEvento = false;
                    bloquesEvento = 0;
                    bloquesSilencio = 0;
                    nivelEventoMax = 0;
                    nivelEventoSuma = 0;
                }
            }
        }
    }

    if (dentroEvento)
    {
        if (eventoPareceRonquido(bloquesEvento, nivelEventoMax, nivelEventoSuma))
        {
            resultado.ronquidosDetectados++;
        }
    }

    archivo.close();

    if (bloquesTotales > 0)
    {
        resultado.nivelMedio = sumaNiveles / bloquesTotales;
    }

    resultado.esRonquido = resultado.ronquidosDetectados > 0;

    return resultado;
}