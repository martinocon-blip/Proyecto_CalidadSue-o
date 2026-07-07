#include "wav_recorder.h"

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <driver/i2s.h>

// ---------- Pines SD ----------
#define SD_CS 10
#define SD_MOSI 11
#define SD_SCK 12
#define SD_MISO 13

// ---------- Pines y Config Microfono ----------
#define I2S_WS 4
#define I2S_BCK 5
#define I2S_SD 6
#define I2S_PORT I2S_NUM_0
#define SAMPLING_FREQUENCY 16000
#define BUFFER_SIZE 512

// ---------- Deteccion ----------
#define FACTOR_DETECCION 2.5
#define MARGEN_MINIMO 600

File wavFile;
int32_t i2s_raw_buffer[BUFFER_SIZE];
int16_t wav_buffer[BUFFER_SIZE];

uint32_t ruidoAmbiente = 0;

struct WavHeader
{
    char chunkID[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;
    char format[4] = {'W', 'A', 'V', 'E'};
    char subchunk1ID[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = 1;
    uint32_t sampleRate = SAMPLING_FREQUENCY;
    uint32_t byteRate = SAMPLING_FREQUENCY * 1 * 2;
    uint16_t blockAlign = 1 * 2;
    uint16_t bitsPerSample = 16;
    char subchunk2ID[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size;
};

bool iniciarGrabacion()
{
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    if (!SD.begin(SD_CS))
    {
        Serial.println("ERROR: No se ha podido iniciar la SD");
        return false;
    }

    Serial.println("SD iniciada correctamente.");

    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLING_FREQUENCY,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = BUFFER_SIZE,
        .use_apll = false
    };

    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_SD
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_start(I2S_PORT);

    Serial.println("Microfono I2S configurado correctamente.");

    return true;
}

uint32_t leerNivelSonido()
{
    size_t bytesLeidos = 0;

    esp_err_t resultado = i2s_read(
        I2S_PORT,
        &i2s_raw_buffer,
        sizeof(i2s_raw_buffer),
        &bytesLeidos,
        portMAX_DELAY
    );

    if (resultado != ESP_OK || bytesLeidos == 0)
    {
        return 0;
    }

    int muestras = bytesLeidos / sizeof(int32_t);
    uint64_t suma = 0;

    for (int i = 0; i < muestras; i++)
    {
        int16_t muestra = (int16_t)(i2s_raw_buffer[i] >> 14);
        suma += abs(muestra);
    }

    return suma / muestras;
}

void calibrarRuidoAmbiente()
{
    Serial.println("Calibrando ruido ambiente...");
    Serial.println("Mantener silencio unos segundos.");

    uint64_t suma = 0;
    const int lecturas = 80;

    for (int i = 0; i < lecturas; i++)
    {
        suma += leerNivelSonido();
        delay(20);
    }

    ruidoAmbiente = suma / lecturas;

    Serial.print("Ruido ambiente: ");
    Serial.println(ruidoAmbiente);
}

bool detectarSonido()
{
    uint32_t nivel = leerNivelSonido();

    uint32_t umbralPorFactor = ruidoAmbiente * FACTOR_DETECCION;
    uint32_t umbralPorMargen = ruidoAmbiente + MARGEN_MINIMO;
    uint32_t umbral = max(umbralPorFactor, umbralPorMargen);

    if (nivel > umbral)
    {
        Serial.print("Nivel detectado: ");
        Serial.print(nivel);
        Serial.print(" / Umbral: ");
        Serial.println(umbral);

        return true;
    }

    return false;
}

bool grabarWAV(const char* nombreArchivo, uint8_t segundos)
{
    wavFile = SD.open(nombreArchivo, FILE_WRITE);

    if (!wavFile)
    {
        Serial.print("ERROR: No se ha podido crear ");
        Serial.println(nombreArchivo);
        return false;
    }

    WavHeader header;
    header.chunkSize = 0;
    header.subchunk2Size = 0;

    wavFile.write((uint8_t*)&header, sizeof(WavHeader));

    Serial.print("Grabando ");
    Serial.print(segundos);
    Serial.println(" segundos de audio...");

    unsigned long tiempoInicio = millis();
    unsigned long duracion = segundos * 1000UL;
    uint32_t totalAudioLen = 0;
    size_t bytesLeidos = 0;

    while (millis() - tiempoInicio < duracion)
    {
        esp_err_t resultado = i2s_read(
            I2S_PORT,
            &i2s_raw_buffer,
            sizeof(i2s_raw_buffer),
            &bytesLeidos,
            portMAX_DELAY
        );

        if (resultado == ESP_OK && bytesLeidos > 0)
        {
            int muestras = bytesLeidos / sizeof(int32_t);

            for (int i = 0; i < muestras; i++)
            {
                wav_buffer[i] = (int16_t)(i2s_raw_buffer[i] >> 14);
            }

            size_t bytesAEscribir = muestras * sizeof(int16_t);

            wavFile.write((uint8_t*)wav_buffer, bytesAEscribir);
            totalAudioLen += bytesAEscribir;
        }
    }

    header.chunkSize = totalAudioLen + 36;
    header.subchunk2Size = totalAudioLen;

    wavFile.seek(0);
    wavFile.write((uint8_t*)&header, sizeof(WavHeader));

    wavFile.close();

    Serial.print("Grabacion finalizada y guardada: ");
    Serial.println(nombreArchivo);

    return true;
}
