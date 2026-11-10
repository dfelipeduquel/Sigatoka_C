/**
 * @file main.cpp
 * @brief Orquestador principal del firmware C++ modular para la estación SIGATOK.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @institution Universidad Tecnológica de Pereira (UTP) - Semillero Kilby
 * @author Daniel Felipe Duque Lizarralde
 */

#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>

#include "sigatok_config.h"
#include "sigatok_types.h"
#include "TimeManager.h"
#include "StorageManager.h"
#include "RainGauge.h"
#include "Anemometer.h"
#include "WindVane.h"
#include "LeafWetnessSensor.h"
#include "MicroclimateSensor.h"
#include "ServiceMode.h"
#include "TelegramNotifier.h"
#include "CamInterface.h"

// =========================================================================
// OBJETOS Y PERIFÉRICOS GLOBALES
// =========================================================================
static TwoWire            I2CBus(0);
static Adafruit_ADS1115   ads1115;
static TimeManager        timeMgr;
static StorageManager     storageMgr;
static RainGauge          rainGauge;
static Anemometer         anemometer;
static WindVane           windVane(&ads1115, SigatokaConfig::ADS_CANAL_VELETA);
static LeafWetnessSensor  leafWetness(&ads1115, SigatokaConfig::ADS_CANAL_HUMECTACION);
static MicroclimateSensor microclimate(SigatokaConfig::PIN_AM2305B, DHT22);
static ServiceMode        serviceMode(&storageMgr, &timeMgr, SigatokaConfig::PIN_BOTON_SERVICIO);
static CamInterface       camInterface(SigatokaConfig::PIN_CONTROL_CAM);

// Telemetría Telegram (credenciales legacy de la estación)
static const char* TELEGRAM_BOT_TOKEN = "8494983222:AAExrG_avdsqAH6HYWSFYuM36Q7CfNHozY8";
static const int64_t TELEGRAM_CHAT_ID = -5086398969;
static TelegramNotifier   telegram(TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID);

// RTC Memory para preservar la duración acumulada de mojado foliar a través de Deep Sleep
RTC_DATA_ATTR static uint32_t duracionHumectacionAcumuladaMin = 0;
RTC_DATA_ATTR static uint32_t contadorCiclos = 0;

// Prototipos de funciones
void inicializarHardware();
void verificarPuntosDeControl();
void ejecutarCicloMedicion();
void entrarEnDeepSleep();

void setup() {
    Serial.begin(115200);
    // Espera no bloqueante para el arranque de la consola si está disponible
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0 < 1000)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    Serial.println("\n========================================================");
    Serial.println("🌿 SIGATOK - Estación Agrometeorológica (Firmware C++17)");
    Serial.println("🏛️ Semillero Kilby - Universidad Tecnológica de Pereira");
    Serial.println("========================================================");

    pinMode(SigatokaConfig::PIN_LED_ESTADO, OUTPUT);
    digitalWrite(SigatokaConfig::PIN_LED_ESTADO, HIGH);

    inicializarHardware();

    // 1. Detección de causa de despertar: ¿Fue pulsado el botón de servicio?
    if (serviceMode.despertoPorBoton()) {
        Serial.println("⚡ Despertar por EXT0: Solicitud de MODO SERVICIO activa.");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Despertar por Boton - Entrando a Modo Servicio");
        serviceMode.ejecutarServidor();
        entrarEnDeepSleep();
        return;
    }

    // 2. Ejecutar ciclo estándar de monitoreo agroclimático
    ejecutarCicloMedicion();

    // 3. Apagado y entrada en sueño profundo
    entrarEnDeepSleep();
}

void loop() {
    // El bucle principal permanece inactivo: el microcontrolador gestiona la secuencia en setup() y duerme
}

void inicializarHardware() {
    // Bus I2C a 400 kHz en pines 27 (SDA) y 26 (SCL)
    I2CBus.begin(SigatokaConfig::PIN_I2C_SDA, SigatokaConfig::PIN_I2C_SCL, SigatokaConfig::I2C_FREQUENCY_HZ);

    // Inicializar ADS1115 con ganancia +/- 4.096V (1 bit = 0.125mV)
    ads1115.setGain(GAIN_ONE);
    if (!ads1115.begin(0x48, &I2CBus)) {
        Serial.println("⚠️ ADS1115 no respondió en dirección 0x48.");
    } else {
        Serial.println("✅ ADS1115 16-bit ADC inicializado.");
    }

    // Inicializar RTC DS1307 / TinyRTC
    if (timeMgr.begin(&I2CBus)) {
        Serial.printf("✅ TinyRTC sincronizado: %s\n", timeMgr.obtenerCadenaCompleta().c_str());
    } else {
        Serial.println("⚠️ TinyRTC no detectado. Utilizando RTC interno del ESP32.");
    }

    // Inicializar almacenamiento (MicroSD SPI + LittleFS)
    if (storageMgr.begin()) {
        Serial.printf("✅ Almacenamiento listo (SD: %s, LittleFS: %s)\n",
                      storageMgr.sdDisponible() ? "OK" : "NO",
                      storageMgr.littleFsDisponible() ? "OK" : "NO");
    } else {
        Serial.println("❌ ERROR: Falla crítica de almacenamiento.");
    }

    // Inicializar sensores
    rainGauge.begin();
    anemometer.begin();
    microclimate.begin();
    serviceMode.begin();
    camInterface.begin();

    Serial.println("✅ Todos los drivers de sensores y GPIOs configurados.");
}

void verificarPuntosDeControl() {
    if (serviceMode.solicitudServicioPendiente()) {
        Serial.println("⚠️ Punto de control: Botón de servicio presionado en ejecución.");
        serviceMode.ejecutarServidor();
    }
}

void ejecutarCicloMedicion() {
    Serial.println("\n📊 Iniciando ventana de integración agrometeorológica (60 s)...");
    storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Inicio de ciclo de medicion");

    rainGauge.iniciarVentana();
    anemometer.iniciarVentana();

    // Ventana no bloqueante de 60 segundos
    uint32_t inicioVentana = millis();
    while ((millis() - inicioVentana) < SigatokaConfig::ANEMO_SAMPLE_WINDOW_MS) {
        verificarPuntosDeControl();
        vTaskDelay(pdMS_TO_TICKS(100)); // Cede CPU a FreeRTOS y Watchdog sin bloquear
    }

    // 1. Recolección de variables físicas
    float lluvia = rainGauge.finalizarYObtenerPrecipitacion();
    float vientoVel = anemometer.finalizarYObtenerVelocidad(60.0f);
    float voltajeVeleta = 0.0f;
    const char* vientoDir = windVane.obtenerDireccion(&voltajeVeleta);

    float temp = 0.0f;
    float hum = 0.0f;
    bool microOk = microclimate.leer(&temp, &hum);

    // 2. Procesamiento de Humectación Foliar (Factor Clave Sigatoka)
    // El intervalo entre ciclos es de 30 minutos (más 1 min de ventana activa)
    uint32_t minutosCiclo = (contadorCiclos == 0) ? 1 : (SigatokaConfig::TIEMPO_DEEP_SLEEP_MIN + 1);
    DatosFoliares foliar = leafWetness.actualizar(minutosCiclo);

    // Actualizar variable persistente en RTC Memory
    if (foliar.hoja_humectada) {
        duracionHumectacionAcumuladaMin += minutosCiclo;
    } else {
        duracionHumectacionAcumuladaMin = 0;
    }
    foliar.duracion_humeda_min = duracionHumectacionAcumuladaMin;

    // 3. Estructuración del registro unificado
    RegistroCompletoEstacion registro;
    timeMgr.obtenerMarcasTiempo(registro.fecha, registro.hora);

    registro.meteo.lluvia_mm_min        = lluvia;
    registro.meteo.velocidad_viento_ms  = vientoVel;
    strncpy(registro.meteo.direccion_viento, vientoDir, sizeof(registro.meteo.direccion_viento) - 1);
    registro.meteo.temperatura_c        = temp;
    registro.meteo.humedad_relativa_pct = hum;
    registro.meteo.valido               = microOk;
    registro.foliar                     = foliar;

    Serial.println("\n📈 Registro consolidado:");
    Serial.println(registro.aFormatoCSV());

    // 4. Guardado en MicroSD / LittleFS
    if (storageMgr.guardarRegistro(registro)) {
        Serial.println("💾 Datos guardados exitosamente en MicroSD.");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Registro guardado en MicroSD");
    } else {
        Serial.println("❌ Error guardando registro en almacenamiento.");
        storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Fallo guardando en almacenamiento");
    }

    contadorCiclos++;
}

void entrarEnDeepSleep() {
    Serial.println("\n💤 Preparando sistema para Deep Sleep...");
    storageMgr.escribirLog(timeMgr.obtenerCadenaCompleta(), "Entrando en Deep Sleep");

    // Desmontar MicroSD para proteger tabla FAT32
    storageMgr.desmontarSD();

    // Apagar LED de estado
    digitalWrite(SigatokaConfig::PIN_LED_ESTADO, LOW);

    // Configurar fuentes de despertar:
    // 1. Temporizador periódico (30 minutos)
    esp_sleep_enable_timer_wakeup(SigatokaConfig::TIEMPO_DEEP_SLEEP_US);

    // 2. Botón de servicio físico en GPIO32 (Nivel bajo / EXT0)
    esp_sleep_enable_ext0_wakeup(SigatokaConfig::PIN_BOTON_SERVICIO, 0);

    Serial.printf("🌙 Durmiendo durante %llu minutos. Despertar por RTC o Boton GPIO32.\n", 
                  SigatokaConfig::TIEMPO_DEEP_SLEEP_MIN);
    Serial.flush();

    esp_deep_sleep_start();
}
