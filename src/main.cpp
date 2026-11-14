/**
 * @file main.cpp
 * @brief Orquestador principal del firmware C++17 modular para la estación SIGATOK.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @institution Universidad Tecnológica de Pereira (UTP) - Semillero Kilby
 * @author Daniel Felipe Duque Lizarralde
 *
 * Fase 1: Paridad fiel 1:1 con legacy/micropython/main.py en campo.
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <driver/rtc_io.h>

#include "sigatok_config.h"
#include "sigatok_types.h"
#include "TimeManager.h"
#include "StorageManager.h"
#include "RainGauge.h"
#include "Anemometer.h"
#include "WindVane.h"
#include "MicroclimateSensor.h"
#include "ServiceMode.h"

#if SIGATOK_ENABLE_LEAF_WETNESS
#include "LeafWetnessSensor.h"
#endif

#if SIGATOK_ENABLE_TELEGRAM
#include "TelegramNotifier.h"
#endif

#if SIGATOK_ENABLE_CAM
#include "CamInterface.h"
#endif

// =========================================================================
// OBJETOS Y PERIFÉRICOS GLOBALES
// =========================================================================
static TwoWire            I2CBus(0);
static Adafruit_ADS1115   ads1115;
static TimeManager        timeMgr;
static StorageManager     storageMgr;
static RainGauge          rainGauge;
static Anemometer         anemometer;
static WindVane           windVane(&ads1115, SigatokaConfig::ADS_CANAL_VELETA, SigatokaConfig::EMA_ALPHA_VELETA);
static MicroclimateSensor microclimate(SigatokaConfig::PIN_AM2305B, DHT22);
static ServiceMode        serviceMode(&storageMgr, &timeMgr, SigatokaConfig::PIN_BOTON_SERVICIO);

#if SIGATOK_ENABLE_LEAF_WETNESS
static LeafWetnessSensor  leafWetness(&ads1115, SigatokaConfig::ADS_CANAL_HUMECTACION);
RTC_DATA_ATTR static uint32_t duracionHumectacionAcumuladaMin = 0;
#endif

#if SIGATOK_ENABLE_CAM
static CamInterface       camInterface(SigatokaConfig::PIN_CONTROL_CAM);
#endif

RTC_DATA_ATTR static uint32_t contadorCiclos = 0;

// Prototipos de funciones
void inicializarHardware();
bool verificarPuntosDeControl();
bool ejecutarCicloMedicion();
void entrarEnDeepSleep(uint64_t minutosSleep = SigatokaConfig::TIEMPO_DEEP_SLEEP_MIN);
const char* describirCausaReset(esp_reset_reason_t razon);

void setup() {
    Serial.begin(115200);
    // Espera breve no bloqueante para enumeración USB/UART (paridad con utime.sleep_ms(500))
    uint32_t t0 = millis();
    while (millis() - t0 < 500) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Watchdog de tarea (30 s): evita bloqueos indefinidos en I2C/SPI/WiFi sin usar delay()
    esp_task_wdt_init(SigatokaConfig::WATCHDOG_TIMEOUT_SEG, true);
    esp_task_wdt_add(nullptr);

    Serial.println("\n========================================================");
    Serial.println("SIGATOK - Estacion Agrometeorologica (C++17 - ESP32)");
    Serial.println("Semillero Kilby - Universidad Tecnologica de Pereira");
    Serial.println("========================================================");

    pinMode(SigatokaConfig::PIN_LED_ESTADO, OUTPUT);
    digitalWrite(SigatokaConfig::PIN_LED_ESTADO, HIGH);

    inicializarHardware();

    esp_reset_reason_t razonReset = esp_reset_reason();
    if (razonReset == ESP_RST_BROWNOUT || razonReset == ESP_RST_TASK_WDT ||
        razonReset == ESP_RST_WDT || razonReset == ESP_RST_PANIC) {
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(),
                               String("Reinicio anomalo: ") + describirCausaReset(razonReset));
    }

    storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Placa encendida");
    Serial.printf("GPIO32 = %d\n", digitalRead(SigatokaConfig::PIN_BOTON_SERVICIO));

    // 1. Detección de despertar por botón físico (EXT0 en GPIO 32)
    if (serviceMode.despertoPorBoton()) {
        Serial.println("Modo servicio solicitado (EXT0 GPIO 32)");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Desperto por boton");
        serviceMode.ejecutarServidor();
        entrarEnDeepSleep(SigatokaConfig::TIEMPO_DEEP_SLEEP_SERVICIO_MIN);
        return;
    }

    // 2. Checkpoint inicial antes de comenzar mediciones
    if (verificarPuntosDeControl()) {
        entrarEnDeepSleep(SigatokaConfig::TIEMPO_DEEP_SLEEP_SERVICIO_MIN);
        return;
    }

    // 3. Ejecutar ciclo de medición agrometeorológica (paridad 1:1 con main.py)
    if (!ejecutarCicloMedicion()) {
        entrarEnDeepSleep(SigatokaConfig::TIEMPO_DEEP_SLEEP_SERVICIO_MIN);
        return;
    }

    // 4. Desmontar interfaces y entrar en Deep Sleep (30 minutos)
    entrarEnDeepSleep(SigatokaConfig::TIEMPO_DEEP_SLEEP_MIN);
}

void loop() {
    // Ciclo determinista ejecutado por setup() + Deep Sleep; el loop principal no se bloquea.
}

void inicializarHardware() {
    // Bus I2C a 400 kHz en pines legacy: SDA=27, SCL=26 con timeout anti-bloqueo
    I2CBus.begin(SigatokaConfig::PIN_I2C_SDA, SigatokaConfig::PIN_I2C_SCL, SigatokaConfig::I2C_FREQUENCY_HZ);
    I2CBus.setTimeOut(SigatokaConfig::I2C_TIMEOUT_MS);

    // ADS1115 (0x48) con ganancia +/- 4.096 V (igual a gain=1 en ads1x15.py legacy)
    ads1115.setGain(GAIN_ONE);
    if (!ads1115.begin(0x48, &I2CBus)) {
        Serial.println("WARN: ADS1115 no respondio en direccion 0x48.");
    } else {
        Serial.println("ADS1115 16-bit ADC inicializado.");
    }

    // Almacenamiento híbrido: MicroSD (SPI VSPI) + LittleFS Flash interna
    Serial.println("Inicializando microSD...");
    if (storageMgr.begin()) {
        if (storageMgr.sdDisponible()) {
            Serial.println("MicroSD detectada correctamente.");
        } else {
            Serial.println("ERROR: No se detecto la microSD. Respaldo LittleFS activo.");
        }
    } else {
        Serial.println("ERROR: Falla critica de almacenamiento.");
    }

    // TinyRTC DS1307 (0x68) con respaldo transparente al RTC interno del ESP32
    if (timeMgr.begin(&I2CBus)) {
        Serial.printf("TinyRTC sincronizado: %s\n", timeMgr.obtenerCadenaCompleta().c_str());
    } else {
        Serial.println("TinyRTC fallo, uso RTC interno.");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "TinyRTC fallo, uso RTC interno");
    }

    // Sensores y Modo Servicio
    rainGauge.begin();
    anemometer.begin();
    microclimate.begin();
    serviceMode.begin();
    Serial.println("Wakeup GPIO32 configurado");

#if SIGATOK_ENABLE_CAM
    camInterface.begin();
#endif
}

const char* describirCausaReset(esp_reset_reason_t razon) {
    switch (razon) {
        case ESP_RST_BROWNOUT: return "Brownout (caida de tension)";
        case ESP_RST_TASK_WDT: return "Task Watchdog";
        case ESP_RST_WDT:      return "Interrupt/RTC Watchdog";
        case ESP_RST_PANIC:    return "Kernel Panic";
        default:               return "Otro";
    }
}

bool verificarPuntosDeControl() {
    if (serviceMode.solicitudServicioPendiente()) {
        Serial.println("Interrupcion solicitada (checkpoint GPIO 32).");
        serviceMode.ejecutarServidor();
        return true;
    }
    return false;
}

bool ejecutarCicloMedicion() {
    Serial.println("\nInicio de ciclo de medicion (60 s)");

    rainGauge.iniciarVentana();
    anemometer.iniciarVentana();

    uint32_t inicioVentana = millis();

    // Ventana no bloqueante de 60 segundos monitoreando checkpoints del botón y watchdog
    while ((millis() - inicioVentana) < SigatokaConfig::ANEMO_SAMPLE_WINDOW_MS) {
        esp_task_wdt_reset();
        if (verificarPuntosDeControl()) {
            rainGauge.finalizarYObtenerPrecipitacion();
            anemometer.finalizarYObtenerVelocidad(1.0f);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    float duracionRealSeg = static_cast<float>(millis() - inicioVentana) / 1000.0f;

    // 1. Recolección de variables meteorológicas (misma secuencia que EstacionObj.Measure_All())
    float lluvia = rainGauge.finalizarYObtenerPrecipitacion();
    uint32_t pulsosViento = anemometer.obtenerPulsosActuales();
    float vientoVel = anemometer.finalizarYObtenerVelocidad(duracionRealSeg);
    float frecuenciaViento = (duracionRealSeg > 0.0f) ? (static_cast<float>(pulsosViento) / duracionRealSeg) : 0.0f;

    Serial.printf("Pulsos/min: %u\n", pulsosViento);
    Serial.printf("Frecuencia: %.4f Hz\n", frecuenciaViento);
    Serial.printf("Velocidad: %.3f m/s\n", vientoVel);

    float voltajeVeleta = 0.0f;
    const char* vientoDir = windVane.obtenerDireccion(&voltajeVeleta);

    // Lectura AM2305B / DHT22 con un reintento no bloqueante si falla el primer pulso
    float temp = 0.0f;
    float hum = 0.0f;
    bool microOk = microclimate.leer(&temp, &hum);
    if (!microOk) {
        uint32_t tEspera = millis();
        while ((millis() - tEspera) < 2100) {
            esp_task_wdt_reset();
            if (verificarPuntosDeControl()) return false;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        microOk = microclimate.leer(&temp, &hum);
        if (!microOk) {
            temp = 0.0f;
            hum = 0.0f;
            storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Error leyendo AM2305B");
        }
    }

    storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Datos adquiridos");

    if (verificarPuntosDeControl()) return false;

    // 2. Estructuración del registro con marca temporal
    RegistroCompletoEstacion registro;
    timeMgr.obtenerMarcasTiempo(registro.fecha, registro.hora);

    registro.meteo.lluvia_mm_min        = lluvia;
    registro.meteo.velocidad_viento_ms  = vientoVel;
    strncpy(registro.meteo.direccion_viento, vientoDir, sizeof(registro.meteo.direccion_viento) - 1);
    registro.meteo.direccion_viento[sizeof(registro.meteo.direccion_viento) - 1] = '\0';
    registro.meteo.temperatura_c        = temp;
    registro.meteo.humedad_relativa_pct = hum;
    registro.meteo.valido               = microOk;

    Serial.printf("Registro CSV: %s\n", registro.aFormatoCSV().c_str());
    Serial.printf("Veleta V_EMA=%.3fV (%s) | AM2305B=%s\n",
                  voltajeVeleta, vientoDir, microOk ? "OK" : "FALLO");

    if (verificarPuntosDeControl()) return false;

    // 3. Guardado en MicroSD (con failover transparente a LittleFS)
    if (storageMgr.guardarRegistro(registro)) {
        Serial.println("Registro guardado en microSD.");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Registro guardado");
    } else {
        Serial.println("Error guardando en microSD");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Error guardando en microSD");
    }

    contadorCiclos++;
    return true;
}

void entrarEnDeepSleep(uint64_t minutosSleep) {
    Serial.println("Cerrando interfaces y SD...");
    storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Entrando en deep sleep");

    // Desmontar MicroSD para proteger integridad del sistema FAT32
    storageMgr.desmontarSD();
    Serial.println("MicroSD desmontada con exito.");

    // Desactivar watchdog antes de entrar en Deep Sleep
    esp_task_wdt_delete(nullptr);
    esp_task_wdt_deinit();

    // Configurar fuentes de despertar:
    // 1. Temporizador periódico (30 min normal / 1 min al salir de Modo Servicio)
    uint64_t tiempoUs = minutosSleep * 60ULL * 1000000ULL;
    esp_sleep_enable_timer_wakeup(tiempoUs);

    // 2. Botón de servicio físico en GPIO 32 (nivel bajo / EXT0) con pull-up RTC activo en Deep Sleep
    rtc_gpio_pullup_en(SigatokaConfig::PIN_BOTON_SERVICIO);
    rtc_gpio_pulldown_dis(SigatokaConfig::PIN_BOTON_SERVICIO);
    esp_sleep_enable_ext0_wakeup(SigatokaConfig::PIN_BOTON_SERVICIO, 0);

    Serial.println("Wakeup reconfigurado antes de dormir");
    digitalWrite(SigatokaConfig::PIN_LED_ESTADO, LOW);

    Serial.printf("Entrando en Deep Sleep (%llu min)...\n", minutosSleep);
    Serial.flush();

    esp_deep_sleep_start();
}
