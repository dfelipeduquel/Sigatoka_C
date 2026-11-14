# Informe Técnico Detallado de Migración, Cambios y Mejoras — Fase 1 (Proyecto SIGATOK)

**Proyecto:** SIGATOK — Estación Agrometeorológica (UTP - Semillero Kilby)  
**Responsable:** Daniel Felipe Duque Lizarralde  
**Repositorio GitHub:** [dfelipeduquel/Sigatoka_C](https://github.com/dfelipeduquel/Sigatoka_C)  
**Fecha:** 14 de noviembre de 2026  
**Hardware Objetivo Único:** **ESP32 DevKit v1 (`esp32dev`)** — idéntico al hardware desplegado en campo (sin cambios de placa ni adición de nuevos sensores).

---

## 1. Aclaración y Rectificación de Alcance Hardware (ESP32-S3 y Sensores)

Durante el andamiaje inicial del repositorio (commit `f77a4f9`) y la primera entrega de Fase 1 (`bb0a95b`), se había incluido un entorno secundario `[env:esp32s3]` junto con directivas condicionales `#if defined(CONFIG_IDF_TARGET_ESP32S3)` en configuración y almacenamiento.  
Tras la revisión de alcance:

1. **Se eliminó completamente `[env:esp32s3]`** de `platformio.ini` y todas las macros/ramas de ESP32-S3 en `include/sigatok_config.h` y `lib/StorageManager/StorageManager.cpp`.
2. **Placa única de trabajo:** `esp32dev` (ESP32 clásico, 240 MHz, 320 KB RAM, 4 MB Flash), con partición LittleFS habilitada (`board_build.filesystem = littlefs`).
3. **Cero sensores nuevos activos:** En `include/sigatok_config.h` se mantienen explícitamente apagados (`0`):
   - `SIGATOK_ENABLE_LEAF_WETNESS 0` (módulo `lib/LeafWetness/` heredado del andamio inicial no se compila ni instancia en `main.cpp`).
   - `SIGATOK_ENABLE_TELEGRAM 0` (`lib/Telemetry/` deshabilitado en Fase 1 para paridad con `main.py`).
   - `SIGATOK_ENABLE_CAM 0` (`lib/CamInterface/` deshabilitado en Fase 1 porque en `main.py` de campo tampoco está activo).

---

## 2. Verificación 1:1 de Pinout y Constantes Físicas vs. Legado MicroPython

Todos los pines y factores de calibración en `include/sigatok_config.h` se contrastaron directamente contra `legacy/micropython/main.py` y `legacy/micropython/Header_sensoresEstacion.py` (respaldados en el commit `f77a4f9`):

| Subsistema | Pin / Parámetro en MicroPython Legacy | Implementación C++ (`sigatok_config.h`) | Verificación |
| :--- | :--- | :--- | :--- |
| **Bus I2C (ADS1115 + DS1307)** | `SoftI2C(scl=Pin(26), sda=Pin(27), freq=400000)` | `PIN_I2C_SDA = GPIO_NUM_27`, `PIN_I2C_SCL = GPIO_NUM_26`, `400000 Hz` | ✅ Exacto |
| **Bus SPI (MicroSD)** | `SPI(1, sck=21, mosi=19, miso=18)`, `CS=17` | `VSPI` (`SCK=21`, `MOSI=19`, `MISO=18`, `CS=17`) | ✅ Exacto |
| **Pluviómetro** | `Pin(23)`, `debounce_time=150 ms`, `0.3 mm/pulso` | `PIN_PLUVIOMETRO = GPIO_NUM_23`, `150 ms`, `0.30f` | ✅ Exacto |
| **Anemómetro** | `Pin(22)`, `debounce_time=60 ms`, ventana `60000 ms`, `f * 0.188` | `PIN_ANEMOMETRO = GPIO_NUM_22`, `60 ms`, `60000 ms`, `0.188f` | ✅ Exacto |
| **Veleta** | `ADS1115` canal `0` (`gain=1`, `+/- 4.096 V`), 8 rangos de voltaje | `ADS_CANAL_VELETA = 0`, `GAIN_ONE`, misma tabla de 8 rangos + EMA ($\alpha = 0.35$) | ✅ Exacto + EMA |
| **AM2305B / DHT22** | `Pin(16)` (`dht.DHT22`) | `PIN_AM2305B = GPIO_NUM_16`, `DHT22` | ✅ Exacto |
| **LED de Estado** | `Pin(2, Pin.OUT)` | `PIN_LED_ESTADO = GPIO_NUM_2` | ✅ Exacto |
| **Botón Modo Servicio / Wakeup** | `pin_boton=32`, `wake_on_ext0(level=0)`, salir $\ge 1000\,\text{ms}$ | `PIN_BOTON_SERVICIO = GPIO_NUM_32`, `EXT0` nivel `0`, `PULSACION_SALIR_MS = 1000` | ✅ Exacto |
| **Modo Servicio AP** | `ESTACION_METEO_DATOS`, red abierta, `192.168.4.1`, timeout `10 min` | `AP_SSID_SERVICIO = "ESTACION_METEO_DATOS"`, `192.168.4.1`, `SERVICIO_TIMEOUT_MS = 600000` | ✅ Exacto |
| **Deep Sleep** | `SLEEP_MIN = 30` | `TIEMPO_DEEP_SLEEP_MIN = 30` | ✅ Exacto |
| **Rutas SD** | `/sd/MUNDONUEVO.csv`, `/sd/logs.txt` | `RUTA_CSV_SD = "/MUNDONUEVO.csv"`, `RUTA_LOG_SD = "/logs.txt"` | ✅ Exacto |

---

## 3. Trazado Detallado de Cambios y Mejoras Módulo por Módulo

### 3.1. Configuración y Estructuras (`include/sigatok_config.h`, `include/sigatok_types.h`)
- **Qué cambió respecto al legado y al andamio inicial (`f77a4f9`):**
  - Se amplió `DatosMeteorologicos::direccion_viento` de `char[8]` a **`char[18]`** (`"Fuera de rango"` ocupa 14 caracteres + `\0`; con `char[8]` se habría truncado silenciosamente).
  - En `RegistroCompletoEstacion::aFormatoCSV()`, cuando las banderas de fases futuras están en `0`, la trama emitida tiene **7 columnas exactas**: `Fecha,Hora,Lluvia,Velocidad,Dirección,Temperatura,Humedad` (`%s,%s,%.2f,%.3f,%s,%.2f,%.2f`).
  - Se fijaron rutas dedicadas para respaldo en Flash interna: `/buffer_pendiente.csv` y `/logs_flash.txt`.
- **Por qué mejora la estación:** Evita corrupción/truncamiento de cadenas en memoria estática y garantiza que el archivo `/MUNDONUEVO.csv` generado en C++ sea compatible con las mediciones históricas existentes en campo (en `Header_microSD.py` el encabezado declaraba una columna extra `,Image Name` vacía que nunca llenaba `main.py`; ahora cabecera y fila coinciden 1:1 en 7 columnas).

### 3.2. Pluviómetro y Anemómetro (`lib/WeatherSensors/RainGauge.*`, `Anemometer.*`)
- **Qué cambió respecto al legado y al andamio inicial:**
  - Las rutinas `IRAM_ATTR manejarInterrupcion()` dejaron de usar `millis()` (no recomendado dentro de interrupciones en ESP32 Arduino) y pasaron a usar **`esp_timer_get_time()`** (resolución de microsegundos, seguro en IRAM) protegido con secciones críticas `portENTER_CRITICAL_ISR(&_mux)`.
  - El tiempo antirrebote pasó a almacenarse en `_debounceStaticUs` a partir del parámetro de instancia `_debounceMs` (`150 ms` pluviómetro / `60 ms` anemómetro).
  - En `iniciarVentana()`, `_ultimoTiempoPulsoUs` se inicializa desplazado `- _debounceStaticUs` para no descartar el primer pulso válido al arranque de la ventana.
  - En `Anemometer::finalizarYObtenerVelocidad(float duracionVentanaSegundos)`, la frecuencia se divide entre la duración real transcurrida en milisegundos medida en `main.cpp` (`duracionRealSeg`) en vez de asumir $60.0\,\text{s}$ fijos.
- **Por qué mejora la estación:** En MicroPython, `Header_sensoresEstacion.py` dependía de `Timer(2)` y bucles bloqueantes `while self.banderaMedicion: pass` sin protección atómica de contadores. En C++, las lecturas están protegidas por spinlock (`portMUX_TYPE`), libres de condiciones de carrera y con exactitud de microsegundos en campo.

### 3.3. Veleta de Dirección (`lib/WeatherSensors/WindVane.*` + `lib/SignalProcessing/EmaFilter.*`)
- **Qué cambió respecto al legado y al andamio inicial:**
  - Se conservaron intactos los **8 rangos de voltaje** calibrados en `Header_sensoresEstacion.py` (`N`, `NE`, `E`, `SE`, `S`, `SW`, `W/N`, `W` + `"Fuera de rango"`).
  - Se integró el filtro digital **`EmaFilter`** ($\alpha = 0.35$, ecuación $y[n] = \alpha x[n] + (1 - \alpha)y[n-1]$) mediante `muestrear()` cada $2\,\text{s}$ dentro de la ventana de $60\,\text{s}$ (más ráfaga de respaldo de 5 muestras si no hubo muestreos previos).
- **Por qué mejora la estación:** En MicroPython se tomaba **una única lectura instantánea** (`ads1115.read()`) al final del minuto, sensible a picos transitorios o rebotes mecánicos de la veleta en campo. El muestreo periódico filtrado por EMA estabiliza la lectura analógica manteniendo exactamente las mismas fronteras de tensión.

### 3.4. Microclima AM2305B / DHT22 (`lib/Microclimate/MicroclimateSensor.*`)
- **Qué cambió respecto al legado:**
  - Control de intervalo mínimo entre consultas (`INTERVALO_MINIMO_MS = 2000 ms`) y validación `isnan(t) || isnan(h)`.
  - En `main.cpp`, ante un fallo del primer pulso unibus, se ejecuta **un único reintento tras espera no bloqueante de $2100\,\text{ms}$** monitoreando el botón de servicio durante la espera.
- **Por qué mejora la estación:** Evita errores silenciosos por consultar el bus unibus antes de su tiempo de recuperación y mantiene la capacidad de atender inmediatamente el botón de servicio sin bloqueos.

### 3.5. Reloj DS1307 y Respaldo RTC (`lib/TimeManager/TimeManager.*`)
- **Qué cambió respecto al legado y al andamio inicial:**
  - Se agregó el filtro **`fechaValida()`** (`año 2020..2099`, `mes 1..12`, `día 1..31`) antes de aceptar la hora del DS1307 y sincronizar `settimeofday()`.
  - Se añadió el miembro privado `TwoWire* _wire` almacenado en `begin(TwoWire* wire)` para que `ajustarFechaHora()` se comunique por el mismo bus I2C de campo (`SDA=27`, `SCL=26`) y no por una instancia sin configurar.
- **Por qué mejora la estación:** Previene marcas de tiempo corruptas (ej. año 2000 o 2165 cuando falla la batería CR2032 del TinyRTC) y asegura respaldo transparente con el RTC interno del ESP32.

### 3.6. Almacenamiento Híbrido MicroSD SPI + LittleFS (`lib/StorageManager/StorageManager.*`)
- **Qué cambió respecto al legado y al andamio inicial (corrección crítica de hardware):**
  - **Corrección crítica de bus SPI:** En el andamio inicial (`f77a4f9`), `_spiSD` se inicializaba con `FSPI`. En **ESP32 DevKit v1**, `FSPI` (`SPI1`) está asignado internamente a la memoria Flash del módulo; el periférico libre para pines definidos por software (`SCK 21`, `MOSI 19`, `MISO 18`, `CS 17`) es **`VSPI`**. Se corrigió a `SIGATOK_SD_SPI_HOST = VSPI`.
  - **Respaldo automático en Flash interna (LittleFS):** Si la tarjeta MicroSD falta o falla al escribir, los datos CSV y logs se almacenan en `/buffer_pendiente.csv` y `/logs_flash.txt` dentro de LittleFS. Al recuperar la SD en un ciclo posterior, `volcarBufferFlashASD()` y `volcarLogsFlashASD()` copian en **bloques de $512\,\text{B}$** (`copiarArchivoEnBloques`) y limpian el buffer de Flash.
  - **Streaming y vista previa acotada:** Se añadieron `abrirLecturaCSV()`, `abrirLecturaLogs()` y `leerUltimosRegistros()` (ventana deslizante en RAM de solo $N$ filas, evitando cargar todo el archivo).
  - **Cierre seguro:** `desmontarSD()` ejecuta `SD.end()` seguido de `_spiSD.end()` antes del Deep Sleep.
- **Por qué mejora la estación:** En el legado MicroPython, un fallo o desconexión momentánea de la MicroSD hacía perder el registro del ciclo de 30 minutos. Con LittleFS como buffer de respaldo y transferencia por bloques de 512 B, los datos sobreviven hasta el próximo montaje exitoso de la tarjeta.

### 3.7. Modo Servicio WiFi y Portal Web (`lib/ServiceMode/ServiceMode.*`)
- **Qué cambió respecto al legado y al andamio inicial (corrección crítica de RAM):**
  - **Streaming HTTP sin OOM:** En el andamio inicial, `/descargar` y `/logs` cargaban el archivo completo en un objeto `String` en RAM, lo que provocaba agotamiento de memoria (`Out of Memory`) a medida que crecía `MUNDONUEVO.csv`. Se reemplazó por **`_servidor.streamFile()`** directo desde `File` (SD o LittleFS).
  - **Rutas completas del legado:** `/`, `/descargar`, `/logs`, `/borrar` (preservando la cabecera CSV), `/apagar` y `/salir` (con espera no bloqueante de $150\,\text{ms}$ atendiendo `_servidor.handleClient()` para garantizar que el navegador reciba la respuesta HTTP antes de apagar el WiFi).
  - **Vista previa web segura:** La página `/` muestra solo los **últimos 5 registros** del CSV mediante `leerUltimosRegistros(5, ...)`.
  - **Manejo limpio de ISR del botón (`GPIO 32`):** Al entrar a `ejecutarServidor()`, se desacopla temporalmente la interrupción (`detachInterrupt`), se limpian banderas y rebotes al soltar el botón, y al salir se limpia el estado antes de rearmar `attachInterrupt` y `EXT0`.
- **Por qué mejora la estación:** Elimina el riesgo de cuelgue por saturación de heap durante descargas de archivos históricos largos en campo y previene reentradas falsas por rebote mecánico del pulsador físico.

### 3.8. Orquestación Principal (`src/main.cpp`)
- **Qué cambió respecto al legado:**
  - Máquina de flujo determinista sin `delay()` bloqueante (`vTaskDelay` + `millis()`).
  - Mantiene todos los `checkpoint()` equivalentes de `main.py` (inicio, antes/durante/después de la ventana de $60\,\text{s}$, durante reintento AM2305B y antes/después del guardado) para entrar a Modo Servicio de inmediato al pulsar el botón físico.
  - Conserva los mensajes de bitácora del sistema (`"Placa encendida"`, `"Desperto por boton"`, `"Datos adquiridos"`, `"Registro guardado"`, `"Entrando en deep sleep"`).
  - Configura wakeup dual antes de dormir: **Timer ($30\,\text{min}$)** + **`EXT0` en `GPIO 32` nivel bajo (`0`)**, apagando `LED_ESTADO` (`GPIO 2`) y desmontando la SD.

---

## 4. Verificación de Compilación y Trazabilidad Git

- **Comando ejecutado:** `pio run -e esp32dev` (PlatformIO Core, plataforma `espressif32 @ 6.12.0`, framework Arduino, compilador `-std=gnu++17`).
- **Resultado:** `SUCCESS` en `82.37 s`.
  - **RAM utilizada:** `46,928 bytes` (`14.3%` de `327,680 bytes`).
  - **Flash utilizada:** `922,253 bytes` (`70.4%` de `1,310,720 bytes`).
- **Historial de commits relevantes en `origin/main`:**
  - `f77a4f9`: Andamio modular inicial C++17 y respaldo histórico en `legacy/`.
  - `654a897`: Eliminación del directorio `legacy/` en el repositorio remoto (`Delete legacy directory`).
  - `bb0a95b`: Implementación de paridad 1:1 Fase 1, correcciones SPI/ISR/streaming/EMA.
  - *(Actual)*: Limpieza total de referencias `ESP32-S3` (placa única `esp32dev`), corrección `_wire` en `TimeManager` y publicación de este informe en GitHub.
