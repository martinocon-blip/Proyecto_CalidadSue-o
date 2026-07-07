## 1. Arquitectura General del Sistema

El proyecto está diseñado bajo un enfoque modular y síncrono, dividiéndose en tres componentes esenciales que interactúan de forma lineal en el ciclo de vida de la sesión:

1. **Controlador Principal (`MAIN`):** Gestiona el ciclo de vida de la sesión, la conectividad WiFi, la sincronización de la hora global (NTP) y la generación de los reportes estadísticos finales.
2. **Subsistema de Captura de Audio (`wav_recorder`):** Controla el hardware periférico. Se encarga de la inicialización de la tarjeta SD (vía SPI), la configuración del micrófono digital (vía I2S) y el volcado de buffers en archivos estructurados `.wav`.
3. **Motor Analítico de Señal (`SnoreDetector`):** El "cerebro" matemático. Analiza los archivos de audio grabados utilizando métricas de energía y frecuencia para discriminar si un ruido corresponde a un ronquido real o a otro tipo de sonido residual.

---

## 2. Explicación Detallada por Módulo

### 📦 Módulo 1: Grabación de Audio (`wav_recorder.h` / `.cpp`)
Este módulo opera a nivel físico (hardware) y se encarga de la captura cruda de datos acústicos.

* **Configuración del Micrófono (I2S):** Configura el protocolo de audio digital a una frecuencia de muestreo de **16 kHz**. Captura el audio en muestras de 32 bits y luego las reduce a **16 bits** mediante un desplazamiento de bits (`i2s_raw_buffer[i] >> 14`) para optimizar el almacenamiento y el posterior análisis RMS.
* **Calibración Ambiental:** Al encenderse, el sistema mide el ruido de la habitación durante unos segundos (80 muestras consecutivas) para establecer una línea base dinámica llamada `ruidoAmbiente`.
* **Detección de Umbral Inteligente:** Para evitar llenar la tarjeta SD con silencio, la función `detectarSonido()` calcula el volumen promedio instantáneo y activa la grabación solo si este supera el umbral adaptativo calculado como:
  $$\text{Umbral} = \max(\text{ruidoAmbiente} \times 2.5, \, \text{ruidoAmbiente} + 600)$$
* **Escritura WAV:** Al activarse, genera un archivo con una cabecera oficial **RIFF/WAVE** (frecuencia, canales, tasa de bytes), captura audio continuo por **7 segundos**, y reescribe dinámicamente los tamaños de datos antes de cerrar el archivo en la SD.

---

### 🧠 Módulo 2: Detector de Ronquidos (`SnoreDetector.h` / `.cpp`)
Cuando el sistema termina de grabar un fragmento de audio, este módulo abre el archivo de la SD y lo procesa en bloques de **512 muestras** (aproximadamente $32\text{ ms}$ de audio por bloque) utilizando dos herramientas de procesamiento digital de señales en el dominio del tiempo:

1.  **RMS (Root Mean Square - Raíz Cuadrada Media):** Mide la potencia o energía electromecánica del sonido. Sirve para determinar la intensidad del volumen en ese bloque:
    $$\text{RMS} = \sqrt{\frac{1}{N} \sum_{i=0}^{N-1} x_i^2}$$
2.  **ZCR (Zero Crossing Rate - Tasa de Cruces por Cero):** Mide cuántas veces la onda de sonido cruza el eje del valor cero. Los sonidos graves y periódicos (como los ronquidos) tienen un ZCR muy bajo ($\le 0.18$), mientras que los agudos (silbidos, clics, sábanas) tienen un ZCR alto.

#### ⚠️ El Filtro de Eventos (`eventoPareceRonquido`)
Para evitar falsos positivos (por ejemplo, dar por bueno un golpe en la mesa o una tos rápida), el código agrupa los bloques sospechosos (alta energía y baja frecuencia) en un "Evento". Este evento se valida con tres reglas estrictas:
* **Duración Mínima y Máxima:** El evento debe durar entre 4 y 45 bloques de audio (aprox. entre **$128\text{ ms}$ y $1.44\text{ segundos}$**), que es la ventana temporal estándar de un ronquido humano regular.
* **Factor de Pico (Filtro Antitos):** La tos genera un impacto acústico súbito y muy alto (un pico de energía), mientras que el ronquido es más progresivo y constante. Si el nivel máximo del sonido es demasiado elevado en comparación con el promedio del bloque (`FACTOR_PICO_TOS = 2.6`), el algoritmo asume que fue un ruido seco y lo descarta:
  $$\text{noPareceTos} = \text{nivelEventoMax} < (\text{nivelEventoMedio} \times 2.6)$$

---

### ⚙️ Módulo 3: Flujo Principal (`MAIN`)
Coordina las interacciones de los módulos anteriores y gestiona los datos de la sesión del usuario.

* **Sincronización NTP:** Se conecta al WiFi para obtener la hora exacta del servidor `pool.ntp.org`. Gracias a esto, cada archivo guardado lleva por nombre la fecha y hora exacta del suceso (ej. `/2026-07-07_23-15-00.wav`).
* **Memoria de Registros:** Guarda en un arreglo en la memoria RAM (`registros[100]`) el historial de los audios analizados durante la noche, indicando si fueron catalogados como ronquidos y cuántos de ellos ocurrieron internamente en ese fragmento de tiempo.
* **Interfaz de Usuario Pasiva:** El programa monitorea constantemente el puerto Serial. Cuando el usuario se despierta y presiona la letra **'F'** (Finalizar), el bucle se detiene y ejecuta el procesamiento estadístico de la noche.

---

## 3. Algoritmo de Calificación del Sueño

La función `calcularNotaSueno()` evalúa el descanso en base a una nota del **0 al 10**, ponderando dos factores a través de la siguiente ecuación lineal:

$$\text{Nota Final} = (\text{Nota Ronquidos} \times 0.70) + (\text{Nota Horas} \times 0.30)$$

### 📊 Tabla de Puntuación por Ronquidos (70% del peso)
Se calcula dividiendo el total de ronquidos detectados entre las horas totales que duró la sesión (Ronquidos por Hora - RPH).

| Rango de Ronquidos por Hora (RPH) | Nota Asignada | Diagnóstico Estimado |
| :--- | :---: | :--- |
| $\le$ 2.0 | **10.0** | Excelente (Silencioso) |
| 2.1 a 5.0 | **9.0** | Normal / Leve |
| 5.1 a 10.0 | **8.0** | Moderado bajo |
| 10.1 a 20.0 | **6.5** | Moderado alto |
| 20.1 a 35.0 | **5.0** | Intenso |
| 35.1 a 60.0 | **3.5** | Muy Severo |
| $>$ 60.0 | **2.0** | Grave (Sugerencia de Evaluación de Apnea del Sueño) |

### 📊 Tabla de Puntuación por Horas de Sueño (30% del peso)
Premia el mantenimiento del rango saludable recomendado por los organismos internacionales de medicina del sueño.

| Horas de Sueño ($H$) | Nota Asignada | Condición del Rango |
| :--- | :---: | :--- |
| De 7.0 a 9.0 horas | **10.0** | **Rango Óptimo de Descanso** |
| 6.0 a <7.0  ó  >9.0 a 10.0 horas | **8.0** | Desviación Leve (Subóptimo) |
| 5.0 a <6.0  ó  >10.0 a 11.0 horas | **6.0** | Desviación Moderada |
| 4.0 a <5.0 horas | **4.0** | Privación de Sueño Leve |
| Menos de 4.0 horas | **2.0** | Privación de Sueño Crónica |

### 📝 Valoración Cualitativa Final
Dependiendo del resultado de la `Nota Final`, el sistema genera una etiqueta de diagnóstico semántico:
* **$\ge$ 9.0:** `SUENO MUY BUENO`
* **$\ge$ 7.0:** `SUENO BUENO`
* **$\ge$ 5.0:** `SUENO REGULAR`
* **$< 5.0$:** `SUENO MALO`

---

## 4. Diagrama de Flujo de Ejecución

```text
       [ Encendido del Dispositivo ] 
                     │
                     ▼
       [ Conexión WiFi & Servidor NTP ] ──> Sincroniza el reloj interno (RTC)
                     │
                     ▼
       [ Calibración Ambiental ] ────────> Registra el ruido base de la habitación
                     │
                     ▼
    ┌────────> [ Bucle Continuo (loop) ]
    │                │
    │                ├─► [Monitoreo Acústico]: ¿El nivel supera el umbral dinámico?
    │                │         │
    │                │         └─► SÍ: Graba 7 segundos en formato .wav
    │                │         └─► Envía el archivo al analizador DSP (RMS y ZCR)
    │                │         └─► Si cumple los filtros de duración/onda ──> Almacena en RAM
    │                │
    │                └─► [Monitoreo de Teclado]: ¿Se presionó la tecla 'F'?
    │                          │
    │                          └─► SÍ: Cambia estado a 'sesionFinalizada = true'
    │
    ▼
[ Función: finalizarSesion() ]
    │
    ├─► Calcula horas totales y Ronquidos por Hora (RPH).
    ├─► Ejecuta la ponderación matemática (70% filtros / 30% tiempo).
    ├─► Desglosa el historial de audios grabados en la consola Serial.
    └─► Detiene periféricos. (Fin de la ejecución).      │
      ▼
 [Conexión WiFi y NTP] ──> Sincroniza reloj interno del ESP32
      │
      ▼
 [Calibración Silenciosa] ──> Registra el ruido base de la habitación
      │
      ▼
┌─> [Bucle Continuo (Loop)]
│     │
│     ├─ En espera de Sonido: Si el nivel supera el umbral dinámico...
│     │     │
│     │     └─> Graba 7 segundos en la tarjeta SD con nombre de fecha/hora.
│     │     └─> Envía el WAV al analizador DSP (RMS y ZCR).
│     │     └─> Si pasa los filtros, suma el ronquido y guarda el registro en RAM.
│     │
│     └─ Monitoreo de Teclado: Si el usuario presiona 'F'...
│           │
│           └─> Rompe el bucle y ejecuta 'finalizarSesion()'.
│
▼
 [Impresión de Reporte Final] ──> Desglosa estadísticas y da la nota del sueño.
Resumen de Beneficios de este CódigoEficiencia Energética y de Almacenamiento: No graba el silencio. Solo utiliza recursos cuando detecta actividad acústica sospechosa.Análisis Local (Edge Computing): No necesita enviar el audio a internet (nube) para saber si estás roncando; el ESP32 calcula las fórmulas matemáticas (RMS/ZCR) de forma local protegiendo la privacidad.Resiliencia: Si la hora de internet falla por algún motivo, el programa tiene contingencias para seguir nombrando los archivos de forma genérica sin colgarse.# Proyecto_CalidadSue-o
