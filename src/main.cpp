#include <Arduino.h>
#include "wav_recorder.h"
#include "SnoreDetector.h"
#include <WiFi.h>
#include <time.h>

SnoreDetector snoreDetector;

const char* ssid = "TPLink_F88C";
const char* password = "MartinMatias2006";


const int MAX_ANALISIS = 100;

struct RegistroAnalisis
{
    char archivo[40];
    time_t fechaHora;
    int ronquidos;
    bool esRonquido;
};

RegistroAnalisis registros[MAX_ANALISIS];

int totalRegistros = 0;
int totalRonquidosSesion = 0;

bool sesionFinalizada = false;
time_t inicioSesion = 0;

void conectarWiFi();
String obtenerNombreArchivo();
String obtenerFechaHoraTexto();
time_t obtenerFechaHoraActual();
time_t obtenerFechaHoraDesdeNombre(const char* nombreArchivo);
void guardarRegistro(const char* nombreArchivo, const SnoreAnalysisResult& resultado);
void finalizarSesion();
float calcularHorasSueno(time_t finSesion);
float calcularRonquidosPorHora(float horasSueno);
float calcularNotaSueno(float ronquidosPorHora, float horasSueno);
const char* obtenerDescripcionNota(float nota);

void conectarWiFi()
{
    Serial.print("Conectando al WiFi");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("WiFi conectado");

    configTime(3600, 3600, "pool.ntp.org");

    struct tm timeinfo;

    while (!getLocalTime(&timeinfo))
    {
        Serial.println("Esperando hora...");
        delay(1000);
    }

    Serial.println("Hora sincronizada");
}

time_t obtenerFechaHoraActual()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        return 0;
    }

    return mktime(&timeinfo);
}

String obtenerNombreArchivo()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        return "/audio_sin_hora.wav";
    }

    char nombre[40];
    strftime(nombre, sizeof(nombre), "/%Y-%m-%d_%H-%M-%S.wav", &timeinfo);

    return String(nombre);
}

String obtenerFechaHoraTexto()
{
    struct tm timeinfo;

    if (!getLocalTime(&timeinfo))
    {
        return "FECHA/HORA NO DISPONIBLE";
    }

    char texto[30];
    strftime(texto, sizeof(texto), "%Y-%m-%d %H:%M:%S", &timeinfo);

    return String(texto);
}

time_t obtenerFechaHoraDesdeNombre(const char* nombreArchivo)
{
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;

    const char* p = nombreArchivo;

    if (p[0] == '/')
    {
        p++;
    }

    int leidos = sscanf(
        p,
        "%d-%d-%d_%d-%d-%d.wav",
        &year,
        &month,
        &day,
        &hour,
        &minute,
        &second
    );

    if (leidos != 6)
    {
        return obtenerFechaHoraActual();
    }

    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(timeinfo));

    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min = minute;
    timeinfo.tm_sec = second;

    return mktime(&timeinfo);
}

void guardarRegistro(const char* nombreArchivo, const SnoreAnalysisResult& resultado)
{
    totalRonquidosSesion += resultado.ronquidosDetectados;

    if (totalRegistros >= MAX_ANALISIS)
    {
        Serial.println("Aviso: memoria de registros llena. El resumen usara los primeros registros.");
        return;
    }

    strncpy(registros[totalRegistros].archivo, nombreArchivo, sizeof(registros[totalRegistros].archivo) - 1);
    registros[totalRegistros].archivo[sizeof(registros[totalRegistros].archivo) - 1] = '\0';

    registros[totalRegistros].fechaHora = obtenerFechaHoraDesdeNombre(nombreArchivo);
    registros[totalRegistros].ronquidos = resultado.ronquidosDetectados;
    registros[totalRegistros].esRonquido = resultado.esRonquido;

    totalRegistros++;
}

float calcularHorasSueno(time_t finSesion)
{
    if (inicioSesion == 0 || finSesion <= inicioSesion)
    {
        return 0.0;
    }

    return (float)(finSesion - inicioSesion) / 3600.0;
}

float calcularRonquidosPorHora(float horasSueno)
{
    if (horasSueno <= 0.0)
    {
        return totalRonquidosSesion;
    }

    return totalRonquidosSesion / horasSueno;
}

float calcularNotaSueno(float ronquidosPorHora, float horasSueno)
{
    float notaRonquidos = 10.0;

    if (ronquidosPorHora <= 2.0)
    {
        notaRonquidos = 10.0;
    }
    else if (ronquidosPorHora <= 5.0)
    {
        notaRonquidos = 9.0;
    }
    else if (ronquidosPorHora <= 10.0)
    {
        notaRonquidos = 8.0;
    }
    else if (ronquidosPorHora <= 20.0)
    {
        notaRonquidos = 6.5;
    }
    else if (ronquidosPorHora <= 35.0)
    {
        notaRonquidos = 5.0;
    }
    else if (ronquidosPorHora <= 60.0)
    {
        notaRonquidos = 3.5;
    }
    else
    {
        notaRonquidos = 2.0;
    }

    float notaHoras = 10.0;

    if (horasSueno >= 7.0 && horasSueno <= 9.0)
    {
        notaHoras = 10.0;
    }
    else if (horasSueno >= 6.0 && horasSueno < 7.0)
    {
        notaHoras = 8.0;
    }
    else if (horasSueno > 9.0 && horasSueno <= 10.0)
    {
        notaHoras = 8.0;
    }
    else if (horasSueno >= 5.0 && horasSueno < 6.0)
    {
        notaHoras = 6.0;
    }
    else if (horasSueno > 10.0 && horasSueno <= 11.0)
    {
        notaHoras = 6.0;
    }
    else if (horasSueno >= 4.0 && horasSueno < 5.0)
    {
        notaHoras = 4.0;
    }
    else
    {
        notaHoras = 2.0;
    }

    float notaFinal = (notaRonquidos * 0.70) + (notaHoras * 0.30);

    if (notaFinal > 10.0)
    {
        notaFinal = 10.0;
    }

    if (notaFinal < 0.0)
    {
        notaFinal = 0.0;
    }

    return notaFinal;
}

const char* obtenerDescripcionNota(float nota)
{
    if (nota >= 9.0)
    {
        return "SUENO MUY BUENO";
    }

    if (nota >= 7.0)
    {
        return "SUENO BUENO";
    }

    if (nota >= 5.0)
    {
        return "SUENO REGULAR";
    }

    return "SUENO MALO";
}

void finalizarSesion()
{
    sesionFinalizada = true;

    time_t finSesion = obtenerFechaHoraActual();

    if (finSesion == 0)
    {
        finSesion = inicioSesion;
    }

    float horasSueno = calcularHorasSueno(finSesion);
    float ronquidosPorHora = calcularRonquidosPorHora(horasSueno);
    float nota = calcularNotaSueno(ronquidosPorHora, horasSueno);

    Serial.println();
    Serial.println("==================================");
    Serial.println("SESION FINALIZADA");
    Serial.println("RESUMEN DE CALIDAD DEL SUENO");
    Serial.println("==================================");

    Serial.print("Horas de sueno: ");
    Serial.println(horasSueno, 2);

    Serial.print("Audios analizados: ");
    Serial.println(totalRegistros);

    Serial.print("Ronquidos totales: ");
    Serial.println(totalRonquidosSesion);

    Serial.print("Ronquidos por hora: ");
    Serial.println(ronquidosPorHora, 2);

    Serial.print("Nota de calidad del sueno: ");
    Serial.print(nota, 1);
    Serial.println(" / 10");

    Serial.print("Valoracion: ");
    Serial.println(obtenerDescripcionNota(nota));

    Serial.println("----------------------------------");
    Serial.println("Detalle de audios:");

    if (totalRegistros == 0)
    {
        Serial.println("No se ha analizado ningun audio.");
    }
    else
    {
        for (int i = 0; i < totalRegistros; i++)
        {
            Serial.print(registros[i].archivo);
            Serial.print(" -> ");

            if (registros[i].esRonquido)
            {
                Serial.print("RONQUIDO");
            }
            else
            {
                Serial.print("NO_RONQUIDO");
            }

            Serial.print(" | Ronquidos detectados: ");
            Serial.println(registros[i].ronquidos);
        }
    }

    Serial.println("----------------------------------");
    Serial.println("Microfono detenido. Reinicia la placa para empezar una nueva sesion.");
    Serial.println("==================================");
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("==================================");
    Serial.println("PROYECTO DETECTOR DE RONQUIDOS");
    Serial.println("==================================");

    conectarWiFi();

    inicioSesion = obtenerFechaHoraActual();

    if (!iniciarGrabacion())
    {
        Serial.println("Fallo al iniciar el sistema.");

        while (true)
        {
            delay(1000);
        }
    }

    calibrarRuidoAmbiente();

    Serial.println("----------------------------------");
    Serial.println("Sistema listo");
    Serial.println("Escuchando continuamente...");
    Serial.println("Pulsa F para finalizar la sesion y ver la nota.");
    Serial.println("----------------------------------");
}

void loop()
{
    if (Serial.available())
    {
        char tecla = Serial.read();

        while (Serial.available())
        {
            Serial.read();
        }

        if (tecla == 'f' || tecla == 'F')
        {
            if (!sesionFinalizada)
            {
                finalizarSesion();
            }
        }
    }

    if (sesionFinalizada)
    {
        delay(1000);
        return;
    }

    if (detectarSonido())
    {
        String fechaHora = obtenerFechaHoraTexto();
        String nombreArchivo = obtenerNombreArchivo();

        Serial.println("----------------------------------");
        Serial.print(fechaHora);
        Serial.println(" POSIBLE RONQUIDO DETECTADO");
        Serial.println("REALIZANDO ANALISIS");

        if (grabarWAV(nombreArchivo.c_str(), 7))
        {
            Serial.print("Audio guardado en SD: ");
            Serial.println(nombreArchivo);

            SnoreAnalysisResult resultado = snoreDetector.analizarArchivoWAV(nombreArchivo.c_str());

            guardarRegistro(nombreArchivo.c_str(), resultado);

            Serial.println();
            Serial.println("ANALISIS REALIZADO");

            if (resultado.esRonquido)
            {
                Serial.println("SI ES UN RONQUIDO");
            }
            else
            {
                Serial.println("NO ES UN RONQUIDO");
            }

            Serial.print("RONQUIDOS DETECTADOS EN ESTE AUDIO: ");
            Serial.println(resultado.ronquidosDetectados);

            Serial.print("RONQUIDOS TOTALES DE LA SESION: ");
            Serial.println(totalRonquidosSesion);

            Serial.print("NIVEL MEDIO: ");
            Serial.println(resultado.nivelMedio);

            Serial.print("NIVEL MAXIMO: ");
            Serial.println(resultado.nivelMaximo);
        }
        else
        {
            Serial.println("ERROR GUARDANDO EL AUDIO EN LA SD");
        }

        Serial.println("----------------------------------");

        delay(1000);
    }

    delay(50);
}