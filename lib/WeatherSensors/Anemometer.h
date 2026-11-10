/**
 * @file Anemometer.h
 * @brief Driver para anemómetro de cazoletas por conteo de frecuencia de pulsos.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_ANEMOMETER_H
#define SIGATOK_ANEMOMETER_H

#include <Arduino.h>
#include "sigatok_config.h"

class Anemometer {
public:
    explicit Anemometer(gpio_num_t pin = SigatokaConfig::PIN_ANEMOMETRO,
                        float factorVelocidad = SigatokaConfig::ANEMO_FACTOR_VELOCIDAD,
                        uint32_t debounceMs = SigatokaConfig::ANEMO_DEBOUNCE_TIME_MS);

    /**
     * @brief Configura el pin GPIO y la interrupción de cazoletas.
     */
    void begin();

    /**
     * @brief Inicia una ventana cronometrada de integración de pulsos.
     */
    void iniciarVentana();

    /**
     * @brief Finaliza la ventana de medición y calcula la velocidad media del viento.
     * @param duracionVentanaSegundos Duración real transcurrida de la ventana (por defecto 60 s).
     * @return Velocidad del viento en m/s.
     */
    float finalizarYObtenerVelocidad(float duracionVentanaSegundos = 60.0f);

    /**
     * @brief ISR estática de interrupción.
     */
    static void IRAM_ATTR manejarInterrupcion();

private:
    gpio_num_t _pin;
    float      _factorVelocidad;
    uint32_t   _debounceMs;

    static volatile uint32_t _contadorPulsos;
    static volatile uint32_t _ultimoTiempoPulsoMs;
    static volatile bool     _medicionActiva;
    static portMUX_TYPE      _mux;
};

#endif // SIGATOK_ANEMOMETER_H
