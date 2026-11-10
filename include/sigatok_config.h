/**
 * @file config.h
 * @brief Configuración global de hardware, pines GPIO y parámetros agrometeorológicos.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @institution Universidad Tecnológica de Pereira (UTP) - Semillero Kilby
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_CONFIG_H
#define SIGATOK_CONFIG_H

#include <Arduino.h>

namespace SigatokaConfig {

    // =========================================================================
    // 1. ASIGNACIÓN DE PINES GPIO (ESP32 DevKit v1 / PCB Sigatoka)
    // =========================================================================

    // Bus I2C (ADS1115 y RTC DS1307 / TinyRTC)
    constexpr gpio_num_t PIN_I2C_SDA        = GPIO_NUM_27;
    constexpr gpio_num_t PIN_I2C_SCL        = GPIO_NUM_26;
    constexpr uint32_t   I2C_FREQUENCY_HZ   = 400000;      // 400 kHz Fast-Mode

    // Bus SPI (MicroSD Card)
    constexpr gpio_num_t PIN_SPI_SCK        = GPIO_NUM_21;
    constexpr gpio_num_t PIN_SPI_MOSI       = GPIO_NUM_19;
    constexpr gpio_num_t PIN_SPI_MISO       = GPIO_NUM_18;
    constexpr gpio_num_t PIN_SPI_CS_SD      = GPIO_NUM_17;

    // Sensores Meteorológicos de Pulsos (Interrupciones)
    constexpr gpio_num_t PIN_PLUVIOMETRO    = GPIO_NUM_23; // Interruptor Reed de balancín
    constexpr gpio_num_t PIN_ANEMOMETRO     = GPIO_NUM_22; // Interruptor Reed de cazoletas

    // Sensores de Microclima y Estado
    constexpr gpio_num_t PIN_AM2305B        = GPIO_NUM_16; // Termo-higrómetro digital
    constexpr gpio_num_t PIN_LED_ESTADO     = GPIO_NUM_2;  // LED onboard indicador de ciclo

    // Control de Interrupciones y Modo Servicio
    constexpr gpio_num_t PIN_BOTON_SERVICIO = GPIO_NUM_32; // Botón físico (Wakeup EXT0 / Interrupción)
    constexpr gpio_num_t PIN_CONTROL_CAM    = GPIO_NUM_14; // Señal de activación para ESP32-CAM

    // =========================================================================
    // 2. PARÁMETROS FÍSICOS Y FACTORES DE CALIBRACIÓN
    // =========================================================================

    // Pluviometría
    constexpr float    PLUVIO_MM_POR_PULSO       = 0.30f;  // Cada basculación equivale a 0.3 mm de precipitación
    constexpr uint32_t PLUVIO_DEBOUNCE_TIME_MS   = 150;    // Antirrebote software para balancín (ms)

    // Anemometría
    constexpr float    ANEMO_FACTOR_VELOCIDAD    = 0.188f; // Factor empírico: v (m/s) = (pulsos/60s) * 0.188
    constexpr uint32_t ANEMO_DEBOUNCE_TIME_MS    = 60;     // Antirrebote software para cazoletas (ms)
    constexpr uint32_t ANEMO_SAMPLE_WINDOW_MS    = 60000;  // Ventana de integración: 60 segundos (1 minuto)

    // Humectación Foliar (Sensor Clave Sigatoka)
    constexpr uint8_t  ADS_CANAL_VELETA          = 0;      // AIN0 en ADS1115
    constexpr uint8_t  ADS_CANAL_HUMECTACION     = 1;      // AIN1 en ADS1115
    constexpr float    EMA_ALPHA_FOLIAR          = 0.15f;  // Coeficiente de suavizado exponencial (0.0 < alpha <= 1.0)
    constexpr float    VOLTAJE_HOJA_SECA         = 3.30f;  // Voltaje representativo de hoja completamente seca
    constexpr float    VOLTAJE_HOJA_SATURADA     = 1.10f;  // Voltaje representativo de lámina libre de agua
    constexpr float    UMBRAL_HUMECTACION_ACTIVA = 70.0f;  // % mínimo de humectación para considerar espora en riesgo

    // =========================================================================
    // 3. ENERGÍA Y GESTIÓN DE CICLOS (DEEP SLEEP)
    // =========================================================================
    constexpr uint64_t TIEMPO_DEEP_SLEEP_MIN     = 30;     // Intervalo estándar entre mediciones de campo (minutos)
    constexpr uint64_t TIEMPO_DEEP_SLEEP_US      = TIEMPO_DEEP_SLEEP_MIN * 60ULL * 1000000ULL;

    // Modo Servicio (Access Point Web)
    constexpr const char* AP_SSID_SERVICIO       = "ESTACION_METEO_DATOS";
    constexpr const char* AP_PASS_SERVICIO       = "";      // Red abierta de servicio técnico
    constexpr uint32_t    SERVICIO_TIMEOUT_MS    = 10 * 60 * 1000; // 10 minutos de inactividad
    constexpr uint32_t    PULSACION_SALIR_MS     = 1000;    // Pulsación larga para salir de servicio (ms)

    // Rutas del Sistema de Archivos MicroSD y LittleFS
    constexpr const char* RUTA_CSV_SD            = "/MUNDONUEVO.csv";
    constexpr const char* RUTA_LOG_SD            = "/logs.txt";
    constexpr const char* RUTA_BUFFER_FLASH      = "/buffer_sigatoka.json";

} // namespace SigatokaConfig

#endif // SIGATOK_CONFIG_H
