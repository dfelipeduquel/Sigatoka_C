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

## 2. Mapa de Arquitectura de Hardware y Pines GPIO

| Subsistema / Sensor | Protocolo / Bus | Pin GPIO ESP32 | Parámetros Físicos / Calibración | Tratamiento de Señal |
| :--- | :--- | :--- | :--- | :--- |
| **Bus I2C Principal** | I2C (400 kHz) | `SDA: 27` / `SCL: 26` | ADS1115 (0x48) + TinyRTC DS1307 (0x68) | Pull-ups externos en PCB Kilby |
| **Pluviómetro de Balancín** | Pulso / ISR | `GPIO 23` | $0.30\,\text{mm/pulso}$ | Debounce por software ($\ge 150\,\text{ms}$) |
| **Anemómetro de Cazoletas** | Pulso / ISR | `GPIO 22` | $v = f \times 0.188\,\text{m/s}$ | Debounce ($\ge 60\,\text{ms}$) en ventana 60 s |
| **Veleta de Dirección** | Analógico vía ADS1115 | `AIN0 (ADS1115)` | $0.0\,\text{V} - 4.1\,\text{V}$ (8 rumbos cardinales) | Promedio de lecturas y discriminación angular |
| **Sensor Humectación Foliar** | Analógico vía ADS1115 | `AIN1 (ADS1115)` | $3.30\,\text{V}$ (seco) a $1.10\,\text{V}$ (saturado) | **Filtro EMA ($\alpha=0.15$)** + acumulador mojado |
| **Microclima (AM2305B / DHT22)** | Unibus Digital | `GPIO 16` | $-40\text{ a }80^\circ\text{C}$, $0\text{ a }100\%\,\text{RH}$ | Verificación de paridad CRC y tasa $\ge 2\,\text{s}$ |
| **Almacenamiento MicroSD** | SPI Hardware | `SCK: 21, MOSI: 19, MISO: 18, CS: 17` | FAT32 (`/MUNDONUEVO.csv`, `/logs.txt`) | Apertura/cierre por lote; LittleFS fallback |
| **Memoria Flash Interna** | LittleFS | Flash embebida | Buffer `/buffer_sigatoka.json` | Desgaste controlado, persistencia ante falla SD |
| **Botón Modo Servicio / Wakeup** | Digital / EXT0 | `GPIO 32` | Despertar en nivel bajo (`EXT0`) | Pulsación corta: entrar / larga ($\ge 1\,\text{s}$): salir |
| **Control Nodo ESP32-CAM** | Pulso Digital | `GPIO 14` | Pulso alto $\ge 200\,\text{ms}$ para despertar CAM | Handshake TCP Socket en puerto 8080 |
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
│   ├── config.h                    # Constantes de hardware, calibración, tiempos y pines GPIO
│   └── types.h                     # Estructuras de datos (RegistroCompletoEstacion, DatosFoliares)
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

### Versión 1.0.0-cpp (10/Nov/2026)
- **Vinculación Git:** Rama `main` vinculada al remoto `https://github.com/dfelipeduquel/Sigatoka_C.git`.
- **Aseguramiento de Archivos Legacy:** Todos los scripts de MicroPython organizados en `legacy/micropython/` sin pérdidas de código previo.
- **Implementación de Reglas Innegociables:**
  - Prohibición total de `delay()` en loops de adquisición (uso de `vTaskDelay` y `millis`).
  - Implementación de **LittleFS** para memoria Flash interna y **SPI** para MicroSD física.
  - **Filtro EMA ($\alpha=0.15$)** integrado en el driver de humectación foliar y veleta.
  - Comentarios 100% técnicos en español con rigor agrometeorológico.
- **Compilación Modular:** Configuración de `platformio.ini` con soporte C++17 (`-std=gnu++17`) para ESP32 y ESP32-S3.
- **Validación Exitosa de Construcción:**
  - Plataforma: Espressif 32 (6.12.0) / ESP32 Dev Module.
  - RAM Utilizada: 48,032 bytes (14.7% de 320 KB).
  - Flash Utilizada: 1,055,065 bytes (80.5% de 1.3 MB).
  - Estado: `SUCCESS` (0 errores, 0 advertencias críticas).

---

## 6. Próximos Hitos y Hoja de Ruta de Validación

1. [ ] **Verificación en Banco de Pruebas (Benchtop Test):**
   - Conectar ESP32 DevKit v1 con ADS1115 y TinyRTC en bus I2C (GPIO 26/27).
   - Inyectar tren de pulsos en GPIO 22 y 23 con generador de señales para calibrar ISR de anemómetro y pluviómetro.
2. [ ] **Validación del Filtro EMA en Sensor Foliar:**
   - Pruebas de inmersión y secado controlado en hoja artificial para validar respuesta transitoria con $\alpha = 0.15$.
3. [ ] **Prueba de Modo Servicio en Campo:**
   - Conexión desde smartphone al AP `ESTACION_METEO_DATOS` (192.168.4.1) y descarga directa del archivo `MUNDONUEVO.csv`.
4. [ ] **Enlace con Nodo ESP32-CAM:**
   - Calibrar temporización del pulso de despertar GPIO 14 y flujo de recepción del JPEG binario por socket TCP.
