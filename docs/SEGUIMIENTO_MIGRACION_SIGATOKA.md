# 🌿 Bitácora y Documento de Seguimiento de Migración — Proyecto SIGATOK
**Estación Agrometeorológica y Monitoreo Foliar para la Prevención de Sigatoka Negra (*Pseudocercospora fijiensis*)**  
**Institución:** Universidad Tecnológica de Pereira (UTP) — Semillero Kilby  
**Investigador / Desarrollador:** Daniel Felipe Duque Lizarralde  
**Repositorio GitHub:** [dfelipeduquel/Sigatoka_C](https://github.com/dfelipeduquel/Sigatoka_C)  
**Ruta Local:** `C:\Users\danie\Developer\Estacion_Sigatoka`

---

## 1. Resumen Ejecutivo y Alcance

El proyecto **SIGATOK** tiene como objetivo el monitoreo agrometeorológico de alta precisión en plantaciones de musáceas (plátano y banano) para modelar las condiciones predisponentes de la **Sigatoka Negra** (*Pseudocercospora fijiensis*). La infección por este hongo fitopatógeno está estrictamente condicionada por:
1. **Lámina de agua libre / Humectación foliar continua** ($\ge 6 - 8$ horas a $25-28^\circ\text{C}$).
2. **Humedad relativa ambiente saturada** ($\ge 90\%$).
3. **Dinámica de viento y precipitación** que dispersan ascosporas y conidios.

Para garantizar robustez en campo, bajo consumo energético (batería/panel solar) y cero congelamiento de ejecución, se ejecuta la migración total desde el prototipo histórico en **MicroPython** hacia una arquitectura modular en **C++17 con PlatformIO y Arduino Framework** sobre microcontroladores **ESP32 DevKit v1 / ESP32-S3**.

---

## 2. Mapa de Arquitectura de Hardware y Pines GPIO (Brújula de Campo vs ESP32-S3)

> **Nota:** En `esp32dev` se respeta al 100% el pinout de campo extraído de `legacy/micropython/main.py` (`include/sigatok_config.h`). Para `esp32s3` se define un mapa seguro sin conflicto con USB/Flash/GPIO inexistentes (`CONFIG_IDF_TARGET_ESP32S3`).

| Subsistema / Sensor | Protocolo / Bus | Pin GPIO ESP32 (Campo) | Parámetros Físicos / Calibración | Tratamiento de Señal |
| :--- | :--- | :--- | :--- | :--- |
| **Bus I2C Principal** | I2C (400 kHz) | `SDA: 27` / `SCL: 26` | ADS1115 (0x48) + TinyRTC DS1307 (0x68) | Pull-ups externos en PCB Kilby |
| **Pluviómetro de Balancín** | Pulso / ISR (`esp_timer`) | `GPIO 23` | $0.30\,\text{mm/pulso}$ | Debounce ISR sin bloqueo ($\ge 150\,\text{ms}$) |
| **Anemómetro de Cazoletas** | Pulso / ISR (`esp_timer`) | `GPIO 22` | $v = f \times 0.188\,\text{m/s}$ | Debounce ($\ge 60\,\text{ms}$) sobre duración real de ventana |
| **Veleta de Dirección** | Analógico vía ADS1115 | `AIN0 (ADS1115)` | $0.0\,\text{V} - 4.1\,\text{V}$ (8 rumbos cardinales) | **Filtro EMA ($\alpha=0.35$)** + muestreo cada $2\,\text{s}$ en ventana |
| **Sensor Humectación Foliar** | Analógico vía ADS1115 | `AIN1 (ADS1115)` | $3.30\,\text{V}$ (seco) a $1.10\,\text{V}$ (saturado) | **Filtro EMA ($\alpha=0.15$)** + acumulador mojado *(Fase 3)* |
| **Microclima (AM2305B / DHT22)** | Unibus Digital | `GPIO 16` | $-40\text{ a }80^\circ\text{C}$, $0\text{ a }100\%\,\text{RH}$ | 1 reintento no bloqueante ($2100\,\text{ms}$) ante fallo CRC |
| **Almacenamiento MicroSD** | SPI Hardware (`VSPI` / `FSPI`) | `SCK: 21, MOSI: 19, MISO: 18, CS: 17` | FAT32 (`/MUNDONUEVO.csv`, `/logs.txt`) | Streaming sin `String` masivos; copia en bloques $512\,\text{B}$ |
| **Memoria Flash Interna** | LittleFS | Flash embebida | `/buffer_pendiente.csv`, `/logs_flash.txt` | Respaldo automático ante falla SD + volcado seguro |
| **Botón Modo Servicio / Wakeup** | Digital / EXT0 | `GPIO 32` | Despertar en nivel bajo (`EXT0`) | ISR desacoplada en servidor web; pulsación $\ge 1\,\text{s}$: salir |
| **Control Nodo ESP32-CAM** | Pulso Digital | `GPIO 14` | Pulso alto $\ge 200\,\text{ms}$ para despertar CAM | Handshake TCP Socket en puerto 8080 *(Fase 5)* |
| **LED Indicador de Estado** | Digital Out | `GPIO 2` | 1 = Ciclo activo / 0 = Deep Sleep | Indicador visual de consumo en campo |

---

## 3. Matriz de Migración: MicroPython Legacy vs. C++17 Modular

| Módulo Legacy (Python) | Módulo C++17 Modular | Estado | Mejoras Técnicas Implementadas |
| :--- | :--- | :--- | :--- |
| `main.py` | `src/main.cpp` | ✅ Migrado | Orquestación por máquina de estados sin `delay()` bloqueante; gestión de Deep Sleep (30 min) y despertar por EXT0/Timer. |
| `Header_sensoresEstacion.py` | `lib/WeatherSensors/` (`RainGauge`, `Anemometer`, `WindVane`) | ✅ Migrado | Conteo atómico en ISR con `portMUX_TYPE`, cálculo de frecuencia de viento exacta en m/s, mapeo cardinal preciso. |
| `Header_sensoresEstacion.py` (Foliar) | `lib/LeafWetness/` (`LeafWetnessSensor`) | ✅ Migrado | Incorporación de **Filtro EMA ($\alpha=0.15$)** para eliminar ruido inductivo/capacitivo y cálculo acumulativo de horas de hoja húmeda en RTC Memory. |
| `ads1x15.py` | Driver Adafruit ADS1X15 + wrapper en C++ | ✅ Migrado | Lecturas de 16 bits sin sobrecarga de heap en MicroPython. |
| `ds1307.py` / `RTC.py` / `Header_tiempo.py` | `lib/TimeManager/` (`TimeManager`) | ✅ Migrado | Sincronización I2C con DS1307 y fallback transparente a RTC interno del ESP32 en caso de batería agotada. |
| `Header_microSD.py` / `sdcard.py` | `lib/StorageManager/` (`StorageManager`) | ✅ Migrado | Sistema híbrido: MicroSD física (SPI) + LittleFS Flash interna como buffer automático ante extracción o error de tarjeta. |
| `Header_ModoServicio.py` | `lib/ServiceMode/` (`ServiceMode`) | ✅ Migrado | Access Point cautivo `ESTACION_METEO_DATOS` en `192.168.4.1` con WebServer HTTP (`/descargar`, `/logs`, `/borrar`, `/salir`). |
| `Header_Telegram.py` | `lib/Telemetry/` (`TelegramNotifier`) | ✅ Migrado | Cliente seguro `WiFiClientSecure` con fragmentación de tramas ($\le 360$ chars) y gestión de reintentos sin bloqueo. |
| `CAM/Header_WiFi_CAM.py` & `Header_WiFi_Principal.py` | `lib/CamInterface/` (`CamInterface`) | ✅ Migrado | Generación de pulso GPIO14 para despertar CAM, conexión WiFi STA y descarga del JPEG binario por socket TCP. |
| - | `lib/SignalProcessing/` (`EmaFilter`) | ✅ Creado | Clase genérica C++ de media móvil exponencial para filtrado digital de sensores analógicos de suelo y follaje. |

---

## 4. Estructura Organizada del Proyecto

```text
C:\Users\danie\Developer\Estacion_Sigatoka/
├── .gitignore                      # Reglas de exclusión para PlatformIO, cachés y secretos
├── platformio.ini                  # Configuración dual (ESP32 DevKit v1 y ESP32-S3) en C++17
├── Sigatoka.code-workspace         # Espacio de trabajo multi-carpeta de VS Code
├── docs/
│   └── SEGUIMIENTO_MIGRACION_SIGATOKA.md # Este documento vivo de seguimiento
├── legacy/
│   └── micropython/
│       ├── ads1x15.py, boot.py, ds1307.py, Header_*.py, main.py, RTC.py, sdcard.py
│       ├── cam/                    # Firmware legacy para ESP32-CAM
│       └── wifi_servidor_mods/     # Modificaciones históricas y simulador HTML
├── include/
│   ├── sigatok_config.h            # Constantes de hardware, calibración, feature flags y pines GPIO
│   └── sigatok_types.h             # Estructuras de datos (RegistroCompletoEstacion, DatosFoliares)
├── lib/
│   ├── SignalProcessing/           # Filtro EMA para acondicionamiento de señal
│   ├── WeatherSensors/             # Drivers pluviómetro, anemómetro y veleta ADS1115
│   ├── LeafWetness/                # Sensor humectación foliar y modelo epidemiológico
│   ├── Microclimate/               # Driver AM2305B / DHT22 no bloqueante
│   ├── TimeManager/                # Gestor I2C DS1307 TinyRTC con fallback interno
│   ├── StorageManager/             # Almacenamiento MicroSD SPI + LittleFS flash buffer
│   ├── ServiceMode/                # AP WiFi y portal web de descarga de datos
│   ├── Telemetry/                  # Notificaciones Telegram Bot seguras
│   └── CamInterface/               # Protocolo TCP y disparo de cámara foliar
└── src/
    └── main.cpp                    # Orquestador del ciclo de campo y Deep Sleep
```

---

## 5. Trazabilidad de Cambios y Mejoras Técnicas

### Versión 1.1.0-cpp — Fase 1: Paridad Fiel 1:1 con Campo (14/Nov/2026)
- **Alineación Total con Brújula Legacy (`legacy/micropython/main.py`):**
  - Feature flags en `include/sigatok_config.h`: `SIGATOK_ENABLE_LEAF_WETNESS=0`, `SIGATOK_ENABLE_TELEGRAM=0`, `SIGATOK_ENABLE_CAM=0` para operación offline fiel a la estación desplegada.
  - CSV de 7 columnas idéntico al archivo en campo (`Date,Time,Precipitation,Wind Velocity,Wind Direction,Temperature,Humidity`) con `char direccion_viento[18]` para evitar truncamiento de `"Fuera de rango"`.
- **Endurecimiento de Drivers y Memoria:**
  - `RainGauge` y `Anemometer`: ISR migradas a `esp_timer_get_time()` con debounce configurable por instancia y cálculo exacto sobre `duracionRealSeg`.
  - `WindVane`: Integración de `EmaFilter` ($\alpha = 0.35$) muestreada cada $2\,\text{s}$ durante la ventana de $60\,\text{s}$ más ráfaga de respaldo.
  - `TimeManager`: Validación `fechaValida()` (`2020..2099`) previa a sincronizar `settimeofday` desde DS1307 con respaldo de RTC interno.
  - `StorageManager`: Corrección de bus SPI (`VSPI` en ESP32 clásico / `FSPI` en ESP32-S3), copia en bloques de $512\,\text{B}$ desde LittleFS (`/buffer_pendiente.csv` y `/logs_flash.txt`) hacia MicroSD, y cierre limpio con `_spiSD.end()`.
  - `ServiceMode`: Streaming HTTP directo con `_servidor.streamFile()` para `/descargar` y `/logs` (cero riesgo de OOM), tabla previa limitada a 5 registros, ruta `/apagar`, y gestión segura de ISR del botón GPIO 32.
- **Validación Dual Exitosa (`pio run -e esp32dev -e esp32s3`):**
  - `esp32dev`: `SUCCESS` en `20.63 s` — RAM: `46,928 bytes (14.3% / 327,680 B)`, Flash: `922,249 bytes (70.4% / 1,310,720 B)`.
  - `esp32s3`: `SUCCESS` en `57.55 s` — RAM: `46,576 bytes (14.2% / 327,680 B)`, Flash: `869,825 bytes (26.0% / 3,342,336 B)`.

### Versión 1.0.0-cpp (10/Nov/2026)
- **Vinculación Git:** Rama `main` vinculada al remoto `https://github.com/dfelipeduquel/Sigatoka_C.git`.
- **Aseguramiento de Archivos Legacy:** Todos los scripts de MicroPython organizados en `legacy/micropython/` sin pérdidas de código previo.
- **Implementación de Reglas Innegociables:**
  - Prohibición total de `delay()` en loops de adquisición (uso de `vTaskDelay` y `millis`).
  - Implementación de **LittleFS** para memoria Flash interna y **SPI** para MicroSD física.
  - **Filtro EMA ($\alpha=0.15$)** integrado en el driver de humectación foliar.
  - Comentarios 100% técnicos en español con rigor agrometeorológico.
- **Compilación Modular:** Configuración de `platformio.ini` con soporte C++17 (`-std=gnu++17`) para ESP32 y ESP32-S3.

---

## 6. Hoja de Ruta Oficial (5 Fases de Despliegue)

1. [x] **Fase 1 — Paridad Fiel 1:1 Offline + Robustez C++ (COMPLETADA - Compilada, pendiente prueba en banco):**
   - Réplica exacta de `main.py` en C++17 sin WiFi cliente, Telegram ni ESP32-CAM.
   - Modo Servicio AP (`ESTACION_METEO_DATOS`, `192.168.4.1`) con descarga por streaming y botón wakeup EXT0.
   - *Siguiente validación física:* Banco de pruebas con ESP32 DevKit v1 + ADS1115 + DS1307 + MicroSD + botón/pulsos.
2. [ ] **Fase 2 — Optimización Energética, Telemetría Interna y Watchdog:**
   - Monitoreo de batería/panel solar (si aplica en PCB), `esp_task_wdt`, ajuste fino de tiempos de estabilización y diagnóstico serial/log avanzado.
3. [ ] **Fase 3 — Sensor de Humectación Foliar (`SIGATOK_ENABLE_LEAF_WETNESS=1`):**
   - Activación del canal `AIN1` del ADS1115 con filtro EMA ($\alpha=0.15$), calibración seco/agua en campo y acumulador de horas de hoja mojada para índice epidemiológico de Sigatoka Negra.
4. [ ] **Fase 4 — Conectividad WiFi Cliente, NTP y Telegram (`SIGATOK_ENABLE_TELEGRAM=1`):**
   - Conexión periódica o bajo demanda, sincronización NTP del RTC y alertas agronómicas vía Bot de Telegram con credenciales desacopladas.
5. [ ] **Fase 5 — Integración Nodo ESP32-CAM (`SIGATOK_ENABLE_CAM=1`):**
   - Pulso de despertar en GPIO 14, servidor/handshake TCP Socket en puerto 8080, recepción/almacenamiento de capturas JPEG en MicroSD y validación conjunta.
