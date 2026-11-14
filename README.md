# SIGATOK — Firmware C++17 (Estación Agrometeorológica)

Firmware modular en **C++17** bajo **PlatformIO (Arduino Framework)** para la estación agrometeorológica autónoma del **Semillero Kilby — Universidad Tecnológica de Pereira (UTP)**, ejecutado sobre **ESP32 DevKit v1 (`esp32dev`)**.

Migrado con paridad funcional 1:1 desde el prototipo operativo en campo, incorporando arquitectura no bloqueante, interrupciones seguras en IRAM, respaldo automático en Flash (`LittleFS`), descarga web por streaming, timeout I2C y Watchdog de tarea.

---

## Hardware y Mapa GPIO (`esp32dev`)

| Subsistema | Bus / Interfaz | Pines ESP32 | Parámetros |
| :--- | :--- | :--- | :--- |
| **ADS1115 + TinyRTC DS1307** | I2C (`400 kHz`) | `SDA: 27`, `SCL: 26` | `0x48` (`GAIN_ONE`), `0x68`, timeout `50 ms` |
| **MicroSD** | SPI (`VSPI`) | `SCK: 21`, `MOSI: 19`, `MISO: 18`, `CS: 17` | FAT32 (`/MUNDONUEVO.csv`, `/logs.txt`) |
| **Pluviómetro** | Pulso (`INPUT_PULLUP`) | `GPIO 23` | `0.30 mm/pulso`, debounce `150 ms` |
| **Anemómetro** | Pulso (`INPUT_PULLUP`) | `GPIO 22` | `v = (N / Δt) × 0.188 m/s`, debounce `60 ms`, ventana `60 s` |
| **Veleta** | Analógico ADC | `AIN0` (ADS1115) | 8 cuadrantes cardinales + ráfaga EMA (`α = 0.35`) |
| **AM2305B (DHT22)** | Unibus Digital | `GPIO 16` | Lectura con 1 reintento no bloqueante (`2100 ms`) |
| **Botón Modo Servicio** | Digital / `EXT0` | `GPIO 32` | Despertar en nivel bajo (`rtc_gpio_pullup_en`) |
| **LED de Estado** | Salida Digital | `GPIO 2` | Alto en ciclo activo / Bajo en Deep Sleep |

---

## Estructura del Proyecto

```text
.
├── platformio.ini                   # Entorno único esp32dev (-std=gnu++17, LittleFS)
├── include/
│   ├── sigatok_config.h             # Pines GPIO, calibración física y banderas de compilación
│   └── sigatok_types.h              # Estructuras meteorológicas y serialización CSV
├── lib/
│   ├── SignalProcessing/            # Filtro EMA (EmaFilter)
│   ├── WeatherSensors/              # RainGauge, Anemometer y WindVane
│   ├── Microclimate/                # MicroclimateSensor (AM2305B / DHT22)
│   ├── TimeManager/                 # DS1307 con validación de rango y fallback RTC interno
│   ├── StorageManager/              # MicroSD (VSPI) + failover/volcado LittleFS en bloques
│   └── ServiceMode/                 # AP WiFi + WebServer con streaming HTTP
├── src/
│   └── main.cpp                     # Ciclo principal, Task Watchdog (30 s) y Deep Sleep
└── docs/
    ├── INFORME_TRADUCCION_SIGATOKA_UTP.md    # Informe técnico formal (Semillero Kilby UTP)
    ├── INFORME_CAMBIOS_Y_MEJORAS_FASE1.md    # Trazabilidad de cambios y mejoras
    └── SEGUIMIENTO_MIGRACION_SIGATOKA.md     # Bitácora de versiones y hoja de ruta
```

*(Los directorios dormidos `lib/LeafWetness`, `lib/Telemetry` y `lib/CamInterface` permanecen deshabilitados en `sigatok_config.h` con `0` y no se compilan en el binario activo).*

---

## Compilación y Carga

Requiere [PlatformIO Core](https://platformio.org/):

```bash
# Compilar firmware
pio run -e esp32dev

# Cargar firmware al ESP32 DevKit v1
pio run -e esp32dev -t upload

# Abrir monitor serial (115200 bps)
pio device monitor -b 115200
```

---

## Operación, CSV y Modo Servicio

- **Ciclo automático:** adquiere variables durante `60 s`, registra la fila en `/MUNDONUEVO.csv` (o `/buffer_pendiente.csv` en `LittleFS` si la SD falla) y entra en Deep Sleep por **`30 min`** (`EXT0` + `Timer`).
- **Formato CSV (`MUNDONUEVO.csv`):**
  ```csv
  Date,Time,Precipitation,Wind Velocity,Wind Direction,Temperature,Humidity
  ```
- **Modo Servicio (despertar o pulsación en `GPIO 32`):**
  - Levanta el Access Point abierto **`ESTACION_METEO_DATOS`** en **`http://192.168.4.1`**.
  - Rutas: `/` (vista previa de últimos 5 registros), `/descargar`, `/logs`, `/borrar`, `/apagar` y `/salir`.
  - Al salir por portal, pulsación larga (`>= 1 s`) o timeout (`10 min`), ejecuta Deep Sleep de **`1 min`** para reanudar el ciclo normal.

---

## Documentación Técnica

- [Informe Técnico de Traducción (Semillero Kilby UTP)](docs/INFORME_TRADUCCION_SIGATOKA_UTP.md)
- [Informe Detallado de Cambios y Mejoras](docs/INFORME_CAMBIOS_Y_MEJORAS_FASE1.md)
- [Bitácora de Migración y Hoja de Ruta](docs/SEGUIMIENTO_MIGRACION_SIGATOKA.md)

---

## Licencia

Distribuido bajo licencia **MIT**. Ver [LICENSE](LICENSE).
