# Informe Técnico de Traducción y Optimización de Firmware — Proyecto SIGATOK (MicroPython a C++17)

**Institución:** Universidad Tecnológica de Pereira (UTP) — Semillero de Investigación Kilby  
**Proyecto:** SIGATOK — Estación Agrometeorológica Autónoma  
**Investigador / Desarrollador:** Daniel Felipe Duque Lizarralde  
**Repositorio Oficial:** [github.com/dfelipeduquel/Sigatoka_C](https://github.com/dfelipeduquel/Sigatoka_C)  
**Fecha de Emisión:** 14 de noviembre de 2026  
**Versión de Firmware Validada:** `v1.2.0-cpp` (Target único: `esp32dev`)

---

## 1. Propósito y Alcance de la Migración

El presente documento consolida la traducción completa, auditada y optimizada del firmware operativo de la estación agrometeorológica **SIGATOK** desde **MicroPython** hacia **C++17 modular bajo PlatformIO y Arduino Framework**, ejecutándose sobre exactamente la misma arquitectura física desplegada en campo (**ESP32 DevKit v1** sin cambio de placa ni adición de sensores externos).

El objetivo de investigación en el Semillero Kilby UTP persigue dos metas simultáneas:
1. **Paridad funcional e histórica 1:1:** preservar intactos el esquema de cableado GPIO, las ecuaciones y factores de calibración física, la estructura de archivos CSV (`MUNDONUEVO.csv`) y las bitácoras (`logs.txt`) para garantizar continuidad en series temporales históricas.
2. **Superioridad operativa y determinismo embebido:** eliminar limitaciones estructurales del intérprete MicroPython (picos de heap/Garbage Collector, bucles bloqueantes, ausencias de protección atómica en interrupciones, fallas silenciosas ante extracción de MicroSD o caídas del bus I2C).

---

## 2. Arquitectura Física Ancla (100% Compatible con Estación en Campo)

Todo parámetro físico y asignación de pines en `include/sigatok_config.h` fue extraído y contrastado directamente con el código de producción heredado (`main.py` y `Header_*.py`), sin alterar la PCB existente:

| Subsistema / Sensor | Interfaz / Bus | Pin GPIO ESP32 | Constante Física / Parámetro | Modo de Operación en C++17 |
| :--- | :--- | :--- | :--- | :--- |
| **Microcontrolador** | - | ESP32 DevKit v1 | 240 MHz, 320 KB RAM, 4 MB Flash | Único entorno compilado (`esp32dev`) |
| **Conversor ADC 16-bit** | I2C (`400 kHz`) | `SDA: 27`, `SCL: 26` | Dirección `0x48`, `GAIN_ONE` ($\pm 4.096\,\text{V}$) | Timeout I2C $50\,\text{ms}$ anti-bloqueo |
| **Reloj Externo TinyRTC** | I2C (`400 kHz`) | `SDA: 27`, `SCL: 26` | Dirección `0x68` (DS1307) | Validación `2020..2099` + fallback RTC interno |
| **Pluviómetro de Balancín** | Pulso Digital | `GPIO 23` (`INPUT_PULLUP`) | $0.30\,\text{mm/pulso}$, debounce $\ge 150\,\text{ms}$ | ISR en `IRAM_ATTR` con `esp_timer_get_time()` |
| **Anemómetro de Cazoletas** | Pulso Digital | `GPIO 22` (`INPUT_PULLUP`) | $v = (N/\Delta t) \times 0.188\,\text{m/s}$, debounce $\ge 60\,\text{ms}$ | Ventana $60\,\text{s}$ con cálculo sobre $\Delta t$ real |
| **Veleta de Dirección** | Analógico ADC | `AIN0 (ADS1115)` | 8 rangos de tensión ($0.0\text{–}4.1\,\text{V}$) | Ráfaga 8 muestras ($\sim 80\,\text{ms}$) + Filtro EMA ($\alpha=0.35$) |
| **Termo-higrómetro AM2305B** | Unibus Digital | `GPIO 16` (`DHT22`) | Tasa $\ge 2\,\text{s}$ entre lecturas | 1 reintento no bloqueante ($2100\,\text{ms}$) vigilando botón |
| **Módulo MicroSD** | SPI (`VSPI`) | `SCK: 21, MOSI: 19, MISO: 18, CS: 17` | FAT32 (`/MUNDONUEVO.csv`, `/logs.txt`) | Desmontaje seguro (`SD.end()` + `_spiSD.end()`) |
| **Respaldo Interno Flash** | LittleFS | Flash embebida | `/buffer_pendiente.csv`, `/logs_flash.txt` | Failover automático y volcado en bloques de $512\,\text{B}$ |
| **Botón Modo Servicio / Wakeup** | Digital / RTC | `GPIO 32` (`INPUT_PULLUP`) | Despertar `EXT0` nivel bajo (`0`) | `rtc_gpio_pullup_en` en Deep Sleep; salir $\ge 1\,\text{s}$ |
| **LED Indicador** | Salida Digital | `GPIO 2` | Alto en ciclo activo / Bajo al dormir | Control explícito previo a `esp_deep_sleep_start()` |

> **Nota sobre módulos dormidos heredados:** Los esqueletos heredados `Telemetry` (Telegram), `CamInterface` (ESP32-CAM) y `LeafWetness` permanecen estrictamente deshabilitados mediante banderas de preprocesador (`= 0`) en `include/sigatok_config.h`, por lo cual **no consumen Flash ni RAM ni interfieren con la operación 1:1**.

---

## 3. Comparativa Arquitectónica: MicroPython Legacy vs. C++17 Modular

| Dimensión Técnica | Firmware Heredado (MicroPython) | Firmware Traducido (C++17 / PlatformIO) | Beneficio Cuantitativo / Cualitativo |
| :--- | :--- | :--- | :--- |
| **Modularidad** | Scripts planos acoplados (`Header_*.py`) sin encapsulamiento estricto | Librerías desacopladas en `lib/` (`WeatherSensors`, `Microclimate`, `TimeManager`, `StorageManager`, `ServiceMode`, `SignalProcessing`) | Reutilización limpia, bajo acoplamiento y mantenibilidad para futuras promociones del Semillero. |
| **Concurrencia y Espera** | `utime.sleep_ms()` / `Timer(2)` + bucle ocupado `while self.banderaMedicion: pass` | Temporización cooperativa no bloqueante (`millis()` + `vTaskDelay`) + FreeRTOS | Respuesta inmediata ($\le 100\,\text{ms}$) al botón de servicio incluso en mitad del minuto de adquisición. |
| **Captura de Pulsos (ISR)** | Callback Python ejecutado fuera de IRAM con `ticks_ms()` | `IRAM_ATTR` con reloj de microsegundos `esp_timer_get_time()` y exclusión mutua `portMUX_TYPE` | Cero condiciones de carrera entre núcleo ISR y lectura principal; inmunidad a latencias del GC. |
| **Filtrado de Veleta** | Una lectura puntual única al final del minuto (`ads1115.read()`) | Ráfaga rápida de 8 muestras con filtro **EMA** ($\alpha=0.35$) en `EmaFilter` | Atenúa ruido eléctrico del ADC sin mezclar mediciones históricas de decenas de segundos atrás. |
| **Gestión de Memoria (RAM)** | Lectura completa de CSV y logs a `str` en `/descargar` y `/logs` (riesgo alto de `MemoryError`) | Streaming HTTP por bloques con `_servidor.streamFile()` y vista previa limitada a 5 filas | Descarga estable de archivos históricos extensos manteniendo uso base de RAM en **14.3%**. |
| **Integridad de Datos** | Si la MicroSD fallaba o no estaba montada, se perdía el dato del ciclo | Respaldo transparente en **LittleFS** interno + volcado automático por bloques de $512\,\text{B}$ al recuperar la SD | Cero pérdida de tramas agrometeorológicas ante fallos transitorios de contacto en ranura SD. |
| **Periférico SPI** | `SPI(1)` heredado | Host explícito **`VSPI`** en ESP32 clásico (evitando conflicto con Flash interna en `FSPI`) | Estabilidad eléctrica y compatibilidad total con Arduino-ESP32. |
| **Protección ante Fallas** | Sin Watchdog ni timeout de I2C | **Task Watchdog (`esp_task_wdt`, $30\,\text{s}$)**, timeout I2C ($50\,\text{ms}$) y registro de causa de reinicio | Recuperación automática en campo ante bloqueos físicos o caídas de tensión solar (`Brownout`). |
| **Wakeup `EXT0` en Deep Sleep** | `Pin.PULL_UP` digital sin control explícito de dominio RTC | `rtc_gpio_pullup_en` / `rtc_gpio_pulldown_dis` al dormir + `rtc_gpio_deinit` al despertar | Evita despertares fantasma por pin flotante durante el reposo de 30 minutos. |

---

## 4. Validación de Compatibilidad Histórica (CSV, Logs y Flujo Operativo)

Para asegurar comparabilidad directa entre los históricos recolectados por la estación en MicroPython y los generados por la versión C++17:

1. **Archivo de Datos (`/MUNDONUEVO.csv`):**
   - **Columnas de salida (7 variables):** `Date,Time,Precipitation,Wind Velocity,Wind Direction,Temperature,Humidity`.
   - Si se inserta una tarjeta MicroSD con registros previos de campo, el firmware **preserva el archivo existente** y anexa las nuevas filas al final manteniendo la continuidad de la serie temporal.
   - Si se trata de una tarjeta limpia, C++ escribe una cabecera exacta de 7 columnas acorde a los 7 valores registrados (corrigiendo la octava columna vacía `,Image Name` presente en `Header_microSD.py`).
   - Las marcas de fecha y hora utilizan formato ISO estandarizado (`YYYY-MM-DD,HH:MM:SS`) con cero a la izquierda y precisión fija (`%.2f` mm lluvia, `%.3f` m/s viento, `%.2f` °C temperatura, `%.2f` % HR) para preservar la resolución de pulso único del anemómetro ($0.188 / 60 \approx 0.0031\,\text{m/s}$).
2. **Bitácora de Eventos (`/logs.txt`):**
   - Conserva las mismas etiquetas operativas heredadas (`Placa encendida`, `Desperto por boton`, `TinyRTC fallo, uso RTC interno`, `Datos adquiridos`, `Registro guardado`, `Error guardando en microSD`, `Entrando en deep sleep`, `Entro en modo servicio`, `CSV descargado`, `Logs descargados`, `CSV borrado`, `Apagado solicitado desde web`, `Salida modo servicio por timeout`, `Salio de modo servicio por boton`, `Sleep por servidor`), añadiendo diagnóstico ante fallas puntuales de `AM2305B` o reinicios anómalos.
3. **Ciclos de Deep Sleep:**
   - **Operación normal de medición:** $30\,\text{min}$ (`SLEEP_MIN = 30`).
   - **Salida tras Modo Servicio:** $1\,\text{min}$ (`sleep_min = 1`), garantizando la misma cadencia rápida de reanudación implementada en `Header_ModoServicio.py`.
4. **Interfaz Web Local (`ESTACION_METEO_DATOS` — `http://192.168.4.1`):**
   - Mantiene las rutas originales (`/`, `/descargar`, `/logs`, `/borrar`, `/apagar`, `/salir`) e incorpora vista previa de las últimas 5 lecturas.

---

## 5. Resultados de Compilación Verificados (PlatformIO)

- **Entorno:** `esp32dev` (`platform = espressif32 @ 6.12.0`, `framework = arduino`, estándar `-std=gnu++17`).
- **Dependencias de compilación activas:** `Adafruit ADS1X15 @ 2.6.2`, `DHT sensor library @ 1.4.7`, `Adafruit Unified Sensor @ 1.1.15`, `RTClib @ 2.1.4` y librerías nativas ESP32 (`Wire`, `SPI`, `SD`, `FS`, `LittleFS`, `WiFi`, `WebServer`). *(Se removió `ArduinoJson` al no ser requerido).*
- **Resultado final (`pio run -e esp32dev`):**
  - **Estado:** `SUCCESS` (`64.59 s`, **0 errores, 0 advertencias en código de proyecto**).
  - **Consumo RAM:** `46,928 bytes` de `327,680 bytes` (**14.3%**).
  - **Consumo Flash:** `923,701 bytes` de `1,310,720 bytes` (**70.5%**).

---

## 6. Protocolo Recomendado de Validación Física (Cuando Exista Acceso al Hardware)

Dado que el desarrollo actual se ejecuta de forma documental/modular en el marco del Semillero sin acceso presencial temporal a la estación instalada en campo, el siguiente paso al disponer de una unidad física en laboratorio consiste en ejecutar este protocolo breve sobre placa `esp32dev`:

1. Flashear el firmware mediante `pio run -e esp32dev -t upload -t monitor` (`115200 bps`).
2. Verificar montaje inicial de `VSPI` (MicroSD), `I2C` (`0x48` ADS1115 y `0x68` DS1307) y transición hacia Deep Sleep.
3. Probar extracción simulada de MicroSD durante un ciclo para confirmar escritura en `/buffer_pendiente.csv` (LittleFS) y volcado automático posterior al reinsertar la tarjeta.
4. Inyectar trenes de pulsos conocidos en `GPIO 22` / `GPIO 23` y contrastar la serie resultante de `MUNDONUEVO.csv` frente a la estación testigo en MicroPython.
5. Despertar mediante pulso a nivel bajo en `GPIO 32`, descargar `/descargar` y `/logs` desde `http://192.168.4.1`, y verificar el reingreso tras 1 minuto de Deep Sleep.
