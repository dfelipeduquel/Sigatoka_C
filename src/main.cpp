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
void ejecutarCicloMedicion();
void entrarEnDeepSleep();

void setup() {
    Serial.begin(115200);
    // Espera breve no bloqueante para enumeración USB/UART (paridad con utime.sleep_ms(500))
    uint32_t t0 = millis();
    while (millis() - t0 < 500) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    Serial.println("\n========================================================");
    Serial.println("🌿 SIGATOK - Estación Agrometeorológica (C++17 Fase 1)");
    Serial.println("🏛️ Semillero Kilby - Universidad Tecnológica de Pereira");
    Serial.println("========================================================");

    pinMode(SigatokaConfig::PIN_LED_ESTADO, OUTPUT);
    digitalWrite(SigatokaConfig::PIN_LED_ESTADO, HIGH);

    inicializarHardware();

    storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Placa encendida");

    // 1. Detección de despertar por botón físico (EXT0 en GPIO 32)
    if (serviceMode.despertoPorBoton()) {
        Serial.println("⚡ Modo servicio solicitado por EXT0 (GPIO 32).");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Desperto por boton");
        serviceMode.ejecutarServidor();
        entrarEnDeepSleep();
        return;
    }

    // 2. Checkpoint inicial antes de comenzar mediciones
    if (verificarPuntosDeControl()) {
        entrarEnDeepSleep();
        return;
    }

    // 3. Ejecutar ciclo de medición agrometeorológica (paridad 1:1 con main.py)
    ejecutarCicloMedicion();

    // 4. Desmontar interfaces y entrar en Deep Sleep (30 minutos)
    entrarEnDeepSleep();
}

void loop() {
    // Ciclo determinista ejecutado por setup() + Deep Sleep; el loop principal no se bloquea.
}

void inicializarHardware() {
    // Bus I2C a 400 kHz en pines legacy: SDA=27, SCL=26
    I2CBus.begin(SigatokaConfig::PIN_I2C_SDA, SigatokaConfig::PIN_I2C_SCL, SigatokaConfig::I2C_FREQUENCY_HZ);

    // ADS1115 (0x48) con ganancia +/- 4.096 V (igual a gain=1 en ads1x15.py legacy)
    ads1115.setGain(GAIN_ONE);
    if (!ads1115.begin(0x48, &I2CBus)) {
        Serial.println("⚠️ ADS1115 no respondió en dirección 0x48.");
    } else {
        Serial.println("✅ ADS1115 16-bit ADC inicializado.");
    }

    // TinyRTC DS1307 (0x68) con respaldo transparente al RTC interno del ESP32
    if (timeMgr.begin(&I2CBus)) {
        Serial.printf("✅ TinyRTC sincronizado: %s\n", timeMgr.obtenerCadenaCompleta().c_str());
    } else {
        Serial.println("⚠️ TinyRTC no detectado o sin hora válida. Usando RTC interno.");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "TinyRTC fallo, uso RTC interno");
    }

    // Almacenamiento híbrido: MicroSD (SPI) + LittleFS Flash interna
    if (storageMgr.begin()) {
        Serial.printf("✅ Almacenamiento listo (SD: %s, LittleFS: %s)\n",
                      storageMgr.sdDisponible() ? "OK" : "NO",
                      storageMgr.littleFsDisponible() ? "OK" : "NO");
    } else {
        Serial.println("❌ ERROR: Falla crítica de almacenamiento.");
    }

    // Sensores y Modo Servicio
    rainGauge.begin();
    anemometer.begin();
    microclimate.begin();
    serviceMode.begin();

#if SIGATOK_ENABLE_CAM
    camInterface.begin();
#endif

    Serial.println("✅ Periféricos y sensores configurados según ancla de campo.");
}

bool verificarPuntosDeControl() {
    if (serviceMode.solicitudServicioPendiente()) {
        Serial.println("⚠️ Checkpoint: Interrupción de modo servicio solicitada (GPIO 32).");
        serviceMode.ejecutarServidor();
        return true;
    }
    return false;
}

void ejecutarCicloMedicion() {
    Serial.println("\n📊 Inicio de ciclo de medición (ventana de 60 s)...");

    windVane.reiniciarFiltro();
    rainGauge.iniciarVentana();
    anemometer.iniciarVentana();

    uint32_t inicioVentana = millis();
    uint32_t ultimoMuestreoVeleta = 0;

    // Ventana no bloqueante de 60 segundos con muestreo periódico EMA de la veleta y checkpoints
    while ((millis() - inicioVentana) < SigatokaConfig::ANEMO_SAMPLE_WINDOW_MS) {
        if (verificarPuntosDeControl()) {
            // Al salir de modo servicio abortamos la ventana interrumpida para dormir ordenadamente
            rainGauge.finalizarYObtenerPrecipitacion();
            anemometer.finalizarYObtenerVelocidad(1.0f);
            return;
        }

        if ((millis() - ultimoMuestreoVeleta) >= 2000) {
            ultimoMuestreoVeleta = millis();
            windVane.muestrear();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    float duracionRealSeg = static_cast<float>(millis() - inicioVentana) / 1000.0f;

    // 1. Recolección de variables meteorológicas
    float lluvia = rainGauge.finalizarYObtenerPrecipitacion();
    float vientoVel = anemometer.finalizarYObtenerVelocidad(duracionRealSeg);
    float voltajeVeleta = 0.0f;
    const char* vientoDir = windVane.obtenerDireccion(&voltajeVeleta);

    // Lectura AM2305B / DHT22 con un reintento no bloqueante si falla el primer pulso
    float temp = 0.0f;
    float hum = 0.0f;
    bool microOk = microclimate.leer(&temp, &hum);
    if (!microOk) {
        uint32_t tEspera = millis();
        while ((millis() - tEspera) < 2100) {
            if (verificarPuntosDeControl()) return;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        microOk = microclimate.leer(&temp, &hum);
        if (!microOk) {
            temp = 0.0f;
            hum = 0.0f;
        }
    }

    storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Datos adquiridos");

    if (verificarPuntosDeControl()) return;

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

#if SIGATOK_ENABLE_LEAF_WETNESS
    uint32_t minutosCiclo = (contadorCiclos == 0) ? 1 : (SigatokaConfig::TIEMPO_DEEP_SLEEP_MIN + 1);
    DatosFoliares foliar = leafWetness.actualizar(minutosCiclo);
    if (foliar.hoja_humectada) {
        duracionHumectacionAcumuladaMin += minutosCiclo;
    } else {
        duracionHumectacionAcumuladaMin = 0;
    }
    foliar.duracion_humeda_min = duracionHumectacionAcumuladaMin;
    registro.foliar = foliar;
#endif

    Serial.println("\n📈 Registro consolidado (CSV):");
    Serial.println(registro.aFormatoCSV());
    Serial.printf("   [Veleta V_EMA=%.3fV | AM2305B=%s]\n",
                  voltajeVeleta, microOk ? "OK" : "FALLO");

    // 3. Guardado en MicroSD (con failover transparente a LittleFS)
    if (storageMgr.guardarRegistro(registro)) {
        Serial.println("💾 Registro guardado en almacenamiento.");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Registro guardado");
    } else {
        Serial.println("❌ Error guardando registro en almacenamiento.");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Error guardando en microSD");
    }

    contadorCiclos++;
}

void entrarEnDeepSleep() {
    Serial.println("\n💤 Cerrando interfaces y entrando en Deep Sleep...");
    storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Entrando en deep sleep");

    // Desmontar MicroSD para proteger integridad del sistema FAT32
    storageMgr.desmontarSD();
    Serial.println("MicroSD desmontada con éxito.");

    // Apagar LED de estado
    digitalWrite(SigatokaConfig::PIN_LED_ESTADO, LOW);

    // Configurar fuentes de despertar:
    // 1. Temporizador periódico (30 minutos)
    esp_sleep_enable_timer_wakeup(SigatokaConfig::TIEMPO_DEEP_SLEEP_US);

    // 2. Botón de servicio físico en GPIO 32 (nivel bajo / EXT0) con pull-up RTC activo en Deep Sleep
    rtc_gpio_pullup_en(SigatokaConfig::PIN_BOTON_SERVICIO);
    rtc_gpio_pulldown_dis(SigatokaConfig::PIN_BOTON_SERVICIO);
    esp_sleep_enable_ext0_wakeup(SigatokaConfig::PIN_BOTON_SERVICIO, 0);

    Serial.printf("🌙 Durmiendo %llu minutos. Wakeup reconfigurado (Timer + EXT0 GPIO32).\n",
                  SigatokaConfig::TIEMPO_DEEP_SLEEP_MIN);
    Serial.flush();

    esp_deep_sleep_start();
}
