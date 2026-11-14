/**
 * @file RainGauge.h
 * @brief Driver para pluviómetro de balancín con interrupción no bloqueante y antirrebote.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_RAIN_GAUGE_H
#define SIGATOK_RAIN_GAUGE_H

#include <Arduino.h>
#include "sigatok_config.h"

class RainGauge {
public:
    explicit RainGauge(gpio_num_t pin = SigatokaConfig::PIN_PLUVIOMETRO,
                       float mmPorPulso = SigatokaConfig::PLUVIO_MM_POR_PULSO,
                       uint32_t debounceMs = SigatokaConfig::PLUVIO_DEBOUNCE_TIME_MS);

    /**
     * @brief Inicializa el GPIO y configura la interrupción por flanco de bajada.
     */
    void begin();

    /**
     * @brief Inicia una nueva ventana de medición.
     */
    void iniciarVentana();

    /**
     * @brief Detiene la ventana de medición y calcula la precipitación acumulada en mm.
     * @return Milímetros de lluvia registrados durante el intervalo.
     */
    float finalizarYObtenerPrecipitacion();

    /**
     * @brief Obtiene los pulsos acumulados actualmente en la ventana.
     */
    uint32_t obtenerPulsosActuales() const;

    /**
     * @brief Rutina de Servicio de Interrupción (ISR) estática segura en IRAM.
     */
    static void IRAM_ATTR manejarInterrupcion();

private:
    gpio_num_t _pin;
    float      _mmPorPulso;
    uint32_t   _debounceMs;

    static volatile uint32_t _contadorPulsos;
    static volatile uint64_t _ultimoTiempoPulsoUs;
    static volatile uint64_t _debounceStaticUs;
    static volatile bool     _medicionActiva;
    static portMUX_TYPE      _mux;
};

#endif // SIGATOK_RAIN_GAUGE_H
