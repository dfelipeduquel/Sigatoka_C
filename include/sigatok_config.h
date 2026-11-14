/**
 * @file sigatok_config.h
 * @brief Configuración global de hardware, pines GPIO y banderas de despliegue.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @institution Universidad Tecnológica de Pereira (UTP) - Semillero Kilby
 * @author Daniel Felipe Duque Lizarralde
 *
 * ANCLA DE HARDWARE: Pines y constantes físicas alineadas 1:1 con el código
 * heredado operativo en campo (legacy/micropython/main.py).
 */

#ifndef SIGATOK_CONFIG_H
#define SIGATOK_CONFIG_H

#include <Arduino.h>

// =========================================================================
// BANDERAS DE FASE DE DESPLIEGUE (1 = Activo, 0 = Deshabilitado)
// Fase 1: Paridad fiel 1:1 con estación offline en campo.
// =========================================================================
#define SIGATOK_ENABLE_LEAF_WETNESS 0
#define SIGATOK_ENABLE_TELEGRAM     0
#define SIGATOK_ENABLE_CAM          0

namespace SigatokaConfig {

    // =========================================================================
    // 1. ASIGNACIÓN DE PINES GPIO (ESP32 DevKit v1 — Brújula exacta de campo)
    //    Extraídos directamente de legacy/micropython/main.py.
    // =========================================================================

    // Bus I2C (ADS1115 0x48 y TinyRTC DS1307 0x68)
    constexpr gpio_num_t PIN_I2C_SDA        = GPIO_NUM_27;
    constexpr gpio_num_t PIN_I2C_SCL        = GPIO_NUM_26;

    // Bus SPI (MicroSD Card sobre periférico VSPI)
    constexpr gpio_num_t PIN_SPI_SCK        = GPIO_NUM_21;
    constexpr gpio_num_t PIN_SPI_MOSI       = GPIO_NUM_19;
    constexpr gpio_num_t PIN_SPI_MISO       = GPIO_NUM_18;
    constexpr gpio_num_t PIN_SPI_CS_SD      = GPIO_NUM_17;

    // Sensores Meteorológicos de Pulsos (Interrupciones)
    constexpr gpio_num_t PIN_PLUVIOMETRO    = GPIO_NUM_23; // Interruptor Reed de balancín
    constexpr gpio_num_t PIN_ANEMOMETRO     = GPIO_NUM_22; // Interruptor Reed de cazoletas

    // Sensores de Microclima y Estado
    constexpr gpio_num_t PIN_AM2305B        = GPIO_NUM_16; // Termo-higrómetro digital (protocolo DHT22)
    constexpr gpio_num_t PIN_LED_ESTADO     = GPIO_NUM_2;  // LED onboard indicador de ciclo activo

    // Control de Interrupciones y Modo Servicio
    constexpr gpio_num_t PIN_BOTON_SERVICIO = GPIO_NUM_32; // Botón físico (Wakeup EXT0 nivel bajo / ISR)
    constexpr gpio_num_t PIN_CONTROL_CAM    = GPIO_NUM_14; // Reservado legado: señal ESP32-CAM (deshabilitado)

    constexpr uint32_t   I2C_FREQUENCY_HZ   = 400000;      // 400 kHz Fast-Mode

    // =========================================================================
    // 2. PARÁMETROS FÍSICOS Y FACTORES DE CALIBRACIÓN (LEGACY CAMPO)
    // =========================================================================

    // Pluviometría (Header_sensoresEstacion.py)
    constexpr float    PLUVIO_MM_POR_PULSO       = 0.30f;  // 1 pulso = 0.3 mm de precipitación
    constexpr uint32_t PLUVIO_DEBOUNCE_TIME_MS   = 150;    // Antirrebote software para balancín (ms)

    // Anemometría (Header_sensoresEstacion.py)
    constexpr float    ANEMO_FACTOR_VELOCIDAD    = 0.188f; // Factor de campo: v (m/s) = (pulsos/60s) * 0.188
    constexpr uint32_t ANEMO_DEBOUNCE_TIME_MS    = 60;     // Antirrebote software para cazoletas (ms)
    constexpr uint32_t ANEMO_SAMPLE_WINDOW_MS    = 60000;  // Ventana de integración: 60 segundos (1 minuto)

    // Conversión ADC ADS1115 (Veleta AIN0 y futuro Foliar AIN1)
    constexpr uint8_t  ADS_CANAL_VELETA          = 0;      // AIN0 en ADS1115 (Veleta)
    constexpr float    EMA_ALPHA_VELETA          = 0.35f;  // Suavizado EMA para estabilidad de lectura ADC
    constexpr uint8_t  ADS_CANAL_HUMECTACION     = 1;      // AIN1 en ADS1115 (Fase 3: Humectación Foliar)
    constexpr float    EMA_ALPHA_FOLIAR          = 0.15f;  // Coeficiente EMA foliar (Fase 3)
    constexpr float    VOLTAJE_HOJA_SECA         = 3.30f;  // Calibración hoja seca (Fase 3)
    constexpr float    VOLTAJE_HOJA_SATURADA     = 1.10f;  // Calibración hoja saturada (Fase 3)
    constexpr float    UMBRAL_HUMECTACION_ACTIVA = 70.0f;  // % mínimo para riesgo Sigatoka (Fase 3)

    // =========================================================================
    // 3. ENERGÍA Y GESTIÓN DE CICLOS (DEEP SLEEP)
    // =========================================================================
    constexpr uint64_t TIEMPO_DEEP_SLEEP_MIN     = 30;     // SLEEP_MIN = 30 en main.py legacy
    constexpr uint64_t TIEMPO_DEEP_SLEEP_US      = TIEMPO_DEEP_SLEEP_MIN * 60ULL * 1000000ULL;

    // Modo Servicio (Access Point Web - Header_ModoServicio.py)
    constexpr const char* AP_SSID_SERVICIO       = "ESTACION_METEO_DATOS";
    constexpr const char* AP_PASS_SERVICIO       = "";      // Red abierta (authmode=0 en legacy)
    constexpr uint32_t    SERVICIO_TIMEOUT_MS    = 10 * 60 * 1000; // 10 minutos (TIEMPO_MAX_SERVICIO_MS)
    constexpr uint32_t    PULSACION_SALIR_MS     = 1000;    // Pulsación larga >= 1000 ms para salir

    // Rutas del Sistema de Archivos MicroSD y LittleFS
    constexpr const char* RUTA_CSV_SD            = "/MUNDONUEVO.csv";
    constexpr const char* RUTA_LOG_SD            = "/logs.txt";
    constexpr const char* RUTA_BUFFER_FLASH      = "/buffer_pendiente.csv";
    constexpr const char* RUTA_LOG_FLASH         = "/logs_flash.txt";

} // namespace SigatokaConfig

#endif // SIGATOK_CONFIG_H
