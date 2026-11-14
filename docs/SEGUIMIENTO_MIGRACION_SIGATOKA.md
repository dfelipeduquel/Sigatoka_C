# 🌿 Bitácora y Documento de Seguimiento de Migración — Proyecto SIGATOK
**Estación Agrometeorológica y Monitoreo Foliar para la Prevención de Sigatoka Negra (*Pseudocercospora fijiensis*)**  
**Institución:** Universidad Tecnológica de Pereira (UTP) — Semillero Kilby  
**Investigador / Desarrollador:** Daniel Felipe Duque Lizarralde  
**Repositorio GitHub:** [dfelipeduquel/Sigatoka_C](https://github.com/dfelipeduquel/Sigatoka_C)  
**Target:** `esp32dev` (ESP32 DevKit v1 — PlatformIO / Arduino C++17)

---

## 1. Resumen Ejecutivo y Alcance

El proyecto **SIGATOK** tiene como objetivo el monitoreo agrometeorológico de alta precisión en plantaciones de musáceas (plátano y banano) para modelar las condiciones predisponentes de la **Sigatoka Negra** (*Pseudocercospora fijiensis*). La infección por este hongo fitopatógeno está estrictamente condicionada por:
1. **Lámina de agua libre / Humectación foliar continua** ($\ge 6 - 8$ horas a $25-28^\circ\text{C}$).
2. **Humedad relativa ambiente saturada** ($\ge 90\%$).
3. **Dinámica de viento y precipitación** que dispersan ascosporas y conidios.

Para garantizar robustez en campo, bajo consumo energético (batería/panel solar) y cero congelamiento de ejecución, se ejecuta la migración total desde el prototipo histórico en **MicroPython** hacia una arquitectura modular en **C++17 con PlatformIO y Arduino Framework** exclusivamente sobre el microcontrolador de la estación en campo: **ESP32 DevKit v1 (`esp32dev`)**, conservando exactamente los mismos sensores, periféricos y pinout heredados (sin placas nuevas ni sensores adicionales).

---

## 2. Mapa de Arquitectura de Hardware y Pines GPIO (ESP32 DevKit v1 — Brújula de Campo)

> **Regla de Ancla Física:** El único target de compilación es `esp32dev` y su mapa GPIO reproduce el archivo operativo `main.py` del código heredado. Las banderas `SIGATOK_ENABLE_LEAF_WETNESS`, `SIGATOK_ENABLE_TELEGRAM` y `SIGATOK_ENABLE_CAM` permanecen en `0` (deshabilitadas).

| Subsistema / Sensor | Protocolo / Bus | Pin GPIO ESP32 | Parámetros Físicos / Calibración | Tratamiento de Señal / Estado Fase 1 |
| :--- | :--- | :--- | :--- | :--- |
| **Bus I2C Principal** | I2C (400 kHz) | `SDA: 27` / `SCL: 26` | ADS1115 (0x48) + TinyRTC DS1307 (0x68) | Pull-ups externos en PCB Kilby |
| **Pluviómetro de Balancín** | Pulso / ISR (`esp_timer`) | `GPIO 23` | $0.30\,\text{mm/pulso}$ | Debounce ISR sin bloqueo ($\ge 150\,\text{ms}$) |
| **Anemómetro de Cazoletas** | Pulso / ISR (`esp_timer`) | `GPIO 22` | $v = f \times 0.188\,\text{m/s}$ | Debounce ($\ge 60\,\text{ms}$) sobre duración real de ventana |
| **Veleta de Dirección** | Analógico vía ADS1115 | `AIN0 (ADS1115)` | $0.0\,\text{V} - 4.1\,\text{V}$ (8 rumbos cardinales) | Ráfaga EMA ($\alpha=0.35$, 8 muestras / $\sim 80\,\text{ms}$) al cierre de ventana |
| **Microclima (AM2305B / DHT22)** | Unibus Digital | `GPIO 16` | $-40\text{ a }80^\circ\text{C}$, $0\text{ a }100\%\,\text{RH}$ | 1 reintento no bloqueante ($2100\,\text{ms}$) ante fallo CRC |
| **Almacenamiento MicroSD** | SPI Hardware (`VSPI`) | `SCK: 21, MOSI: 19, MISO: 18, CS: 17` | FAT32 (`/MUNDONUEVO.csv`, `/logs.txt`) | Streaming sin `String` masivos; copia en bloques $512\,\text{B}$ |
| **Memoria Flash Interna** | LittleFS | Flash embebida | `/buffer_pendiente.csv`, `/logs_flash.txt` | Respaldo automático ante falla SD + volcado seguro |
| **Botón Modo Servicio / Wakeup** | Digital / EXT0 | `GPIO 32` | Despertar en nivel bajo (`EXT0`) | ISR desacoplada en servidor web; pulsación $\ge 1\,\text{s}$: salir |
| **LED Indicador de Estado** | Digital Out | `GPIO 2` | 1 = Ciclo activo / 0 = Deep Sleep | Indicador visual de consumo en campo |
| **Módulos fuera de Fase 1** | - | `GPIO 14` (CAM) / `AIN1` | `SIGATOK_ENABLE_* = 0` | **No compilados ni activos en Fase 1** |

---

## 3. Matriz de Migración: MicroPython Legacy vs. C++17 Modular

| Módulo Legacy (Python) | Módulo C++17 Modular | Estado | Mejoras Técnicas Implementadas |
| :--- | :--- | :--- | :--- |
| `main.py` | `src/main.cpp` | ✅ Migrado | Orquestación por máquina de estados sin `delay()` bloqueante; gestión de Deep Sleep (30 min) y despertar por EXT0/Timer. |
| `Header_sensoresEstacion.py` | `lib/WeatherSensors/` (`RainGauge`, `Anemometer`, `WindVane`) | ✅ Migrado | Conteo atómico en ISR con `portMUX_TYPE`, cálculo de frecuencia de viento exacta en m/s, mapeo cardinal preciso. |
| `Header_sensoresEstacion.py` (Foliar N/A) | `lib/LeafWetness/` (`LeafWetnessSensor`) | ⏸️ Deshabilitado (`=0`) | Módulo heredado del andamio inicial; no pertenece al hardware actual de campo y permanece apagado (`SIGATOK_ENABLE_LEAF_WETNESS=0`). |
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
Sigatoka_C/
├── .gitignore                      # Reglas de exclusión (PlatformIO, secretos y respaldo legacy local)
├── LICENSE                         # Licencia MIT
├── README.md                       # Documentación principal del repositorio
├── platformio.ini                  # Configuración única para ESP32 DevKit v1 (esp32dev) en C++17
├── Sigatoka.code-workspace         # Espacio de trabajo portátil de VS Code
├── docs/
│   ├── SEGUIMIENTO_MIGRACION_SIGATOKA.md      # Bitácora viva de migración
│   ├── INFORME_CAMBIOS_Y_MEJORAS_FASE1.md     # Informe técnico de cambios y mejoras
│   └── INFORME_TRADUCCION_SIGATOKA_UTP.md     # Informe formal de traducción para el Semillero Kilby
├── include/
│   ├── sigatok_config.h            # Constantes de hardware, calibración, feature flags y pines GPIO
│   └── sigatok_types.h             # Estructuras de datos (RegistroCompletoEstacion, DatosFoliares)
├── lib/
│   ├── SignalProcessing/           # Filtro EMA para acondicionamiento de señal
│   ├── WeatherSensors/             # Drivers pluviómetro, anemómetro y veleta ADS1115
│   ├── LeafWetness/                # Módulo base dormido (deshabilitado: SIGATOK_ENABLE_LEAF_WETNESS=0)
│   ├── Microclimate/               # Driver AM2305B / DHT22 no bloqueante
│   ├── TimeManager/                # Gestor I2C DS1307 TinyRTC con fallback interno
│   ├── StorageManager/             # Almacenamiento MicroSD SPI (VSPI) + LittleFS flash buffer
│   ├── ServiceMode/                # AP WiFi y portal web de descarga de datos
│   ├── Telemetry/                  # Módulo dormido Telegram (deshabilitado: SIGATOK_ENABLE_TELEGRAM=0)
│   └── CamInterface/               # Módulo dormido ESP32-CAM (deshabilitado: SIGATOK_ENABLE_CAM=0)
└── src/
    └── main.cpp                    # Orquestador del ciclo de campo, Watchdog y Deep Sleep
```

---

## 5. Trazabilidad de Cambios y Mejoras Técnicas

### Versión 1.2.0-cpp — Refinamiento Final de Paridad 1:1 y Endurecimiento (14/Nov/2026)
- **Resolución de Discordancias con Legado:**
  - `ServiceMode` duerme **`1 min`** al salir del servidor web/botón (`TIEMPO_DEEP_SLEEP_SERVICIO_MIN = 1`), conservando **`30 min`** en ciclos normales, y registra `"Sleep por servidor"` exactamente como `Header_ModoServicio.py`.
  - `WindVane::obtenerDireccion()` ejecuta una ráfaga rápida no bloqueante de 8 muestras (`~80 ms`) filtradas por EMA ($\alpha=0.35$) al cierre de ventana, eliminando ruido eléctrico sin generar rumbos intermedios fantasma entre minutos.
  - Trazas Serial alineadas con los hitos históricos (`GPIO32 = ...`, `Inicializando microSD...`, `MicroSD detectada correctamente.`, `Pulsos/min`, `Frecuencia`, `Velocidad`, `Registro guardado en microSD.`, `MicroSD desmontada con exito.`, `Wakeup reconfigurado antes de dormir`).
- **Confiabilidad Embebida y Limpieza:**
  - Watchdog de tarea `esp_task_wdt` ($30\,\text{s}$), timeout de bus I2C ($50\,\text{ms}$) y registro en `logs.txt` ante reinicios anómalos (`Brownout` / `WDT` / `Panic`).
  - Eliminación de dependencia sin uso `ArduinoJson` en `platformio.ini`.
  - Informe formal completo para documentación del Semillero creado en `docs/INFORME_TRADUCCION_SIGATOKA_UTP.md`.
- **Validación Final de Compilación (`pio run -e esp32dev`):**
  - `esp32dev`: `SUCCESS` en `64.59 s` — RAM: `46,928 bytes (14.3% / 327,680 B)`, Flash: `923,701 bytes (70.5% / 1,310,720 B)`.

### Versión 1.1.1-cpp — Corrección de Hardware Único (`esp32dev`) (14/Nov/2026)
- **Eliminación Completa de ESP32-S3:**
  - Se removió `[env:esp32s3]` de `platformio.ini` y las ramas condicionales `CONFIG_IDF_TARGET_ESP32S3` de `include/sigatok_config.h` y `lib/StorageManager/StorageManager.cpp`.
  - La compilación y el mapa de pines quedan fijados al 100% en la placa real de campo: **ESP32 DevKit v1 (`esp32dev`)** usando bus `VSPI` para MicroSD y los pines heredados sin alteraciones ni sensores nuevos.
- **Corrección en `TimeManager`:** Se conserva el puntero `TwoWire* _wire` recibido en `begin(&I2CBus)` para que `ajustarFechaHora()` opere siempre sobre el bus I2C de campo (`SDA=27`, `SCL=26`).
- **Informe Detallado:** Documentación exhaustiva de cambios y mejoras generada en `docs/INFORME_CAMBIOS_Y_MEJORAS_FASE1.md`.
- **Validación de Compilación (`pio run -e esp32dev`):**
  - `esp32dev`: `SUCCESS` en `24.67 s` — RAM: `46,928 bytes (14.3% / 327,680 B)`, Flash: `922,501 bytes (70.4% / 1,310,720 B)`.
- **Auditoría Final de Paridad y Limpieza GitHub:**
  - Añadido `rtc_gpio_pullup_en` / `rtc_gpio_deinit` en `GPIO 32` para proteger `EXT0` durante Deep Sleep y corregida la lectura de cabecera en `leerUltimosRegistros()` al respaldar desde LittleFS.
  - Directorio `legacy/` purgado del historial completo de GitHub y conservado exclusivamente en local (excluido en `.gitignore`).

### Versión 1.1.0-cpp — Fase 1: Paridad Fiel 1:1 con Campo (14/Nov/2026)
- **Alineación Total con Brújula Legacy (`main.py`):**
  - Feature flags en `include/sigatok_config.h`: `SIGATOK_ENABLE_LEAF_WETNESS=0`, `SIGATOK_ENABLE_TELEGRAM=0`, `SIGATOK_ENABLE_CAM=0` para operación offline fiel a la estación desplegada.
  - CSV de 7 columnas idéntico al archivo en campo (`Date,Time,Precipitation,Wind Velocity,Wind Direction,Temperature,Humidity`) con `char direccion_viento[18]` para evitar truncamiento de `"Fuera de rango"`.
- **Endurecimiento de Drivers y Memoria:**
  - `RainGauge` y `Anemometer`: ISR migradas a `esp_timer_get_time()` con debounce configurable por instancia y cálculo exacto sobre `duracionRealSeg`.
  - `WindVane`: Integración de `EmaFilter` ($\alpha = 0.35$) muestreada cada $2\,\text{s}$ durante la ventana de $60\,\text{s}$ más ráfaga de respaldo.
  - `TimeManager`: Validación `fechaValida()` (`2020..2099`) previa a sincronizar `settimeofday` desde DS1307 con respaldo de RTC interno.
  - `StorageManager`: Corrección de bus SPI a `VSPI`, copia en bloques de $512\,\text{B}$ desde LittleFS (`/buffer_pendiente.csv` y `/logs_flash.txt`) hacia MicroSD, y cierre limpio con `_spiSD.end()`.
  - `ServiceMode`: Streaming HTTP directo con `_servidor.streamFile()` para `/descargar` y `/logs` (cero riesgo de OOM), tabla previa limitada a 5 registros, ruta `/apagar`, y gestión segura de ISR del botón GPIO 32.

### Versión 1.0.0-cpp (10/Nov/2026)
- **Vinculación Git:** Rama `main` vinculada al remoto `https://github.com/dfelipeduquel/Sigatoka_C.git`.
- **Aseguramiento de Archivos Legacy:** Scripts de MicroPython conservados únicamente como respaldo local en `legacy/` (excluido de GitHub vía `.gitignore`).
- **Implementación de Reglas Innegociables:**
  - Prohibición total de `delay()` en loops de adquisición (uso de `vTaskDelay` y `millis`).
  - Implementación de **LittleFS** para memoria Flash interna y **SPI** para MicroSD física.
  - Comentarios 100% técnicos en español con rigor agrometeorológico.
- **Compilación Modular:** Configuración inicial de `platformio.ini` con soporte C++17 (`-std=gnu++17`). *(Nota: el entorno secundario ESP32-S3 incluido en este andamio inicial fue retirado en v1.1.1-cpp).*

---

## 6. Hoja de Ruta Oficial (Misma Placa ESP32 DevKit v1 y Hardware Existente)

1. [x] **Fase 1 — Paridad Fiel 1:1 Offline en ESP32 DevKit v1 (COMPLETADA - Compilada, pendiente prueba en banco):**
   - Réplica exacta de `main.py` en C++17 sobre `esp32dev` con los mismos 4 sensores de campo, DS1307, MicroSD y Modo Servicio AP (`ESTACION_METEO_DATOS`, `192.168.4.1`).
2. [ ] **Fase 2 — Validación Física en Banco y Endurecimiento Operativo (Misma Placa):**
   - Prueba real de pines, bus I2C (`SDA 27 / SCL 26`), SPI `VSPI`, pulsos GPIO 22/23, EXT0 GPIO 32 y portal web.
   - Incorporación de `esp_task_wdt` (Watchdog de tarea) y ajuste fino de tiempos de estabilización sin agregar sensores ni cambiar placa.
3. [ ] **Fase 3 (Opcional según decisión en campo) — Módulos Legacy Dormidos:**
   - Activación controlada únicamente de los módulos que ya existían en los scripts heredados (`Header_Telegram.py` / `cam/`) solo si la estación física cuenta con cobertura WiFi o nodo CAM operativo. **No se agregan placas ni sensores nuevos.**
