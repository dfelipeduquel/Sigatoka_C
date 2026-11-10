/**
 * @file RainGauge.cpp
 * @brief Implementación del driver para pluviómetro de balancín.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "RainGauge.h"

volatile uint32_t RainGauge::_contadorPulsos = 0;
volatile uint32_t RainGauge::_ultimoTiempoPulsoMs = 0;
volatile bool     RainGauge::_medicionActiva = false;
portMUX_TYPE      RainGauge::_mux = portMUX_INITIALIZER_UNLOCKED;

RainGauge::RainGauge(gpio_num_t pin, float mmPorPulso, uint32_t debounceMs)
    : _pin(pin), _mmPorPulso(mmPorPulso), _debounceMs(debounceMs) {}

void RainGauge::begin() {
    pinMode(_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pin), RainGauge::manejarInterrupcion, FALLING);
}

void RainGauge::iniciarVentana() {
    portENTER_CRITICAL(&_mux);
    _contadorPulsos = 0;
    _medicionActiva = true;
    _ultimoTiempoPulsoMs = millis();
    portEXIT_CRITICAL(&_mux);
}

float RainGauge::finalizarYObtenerPrecipitacion() {
    uint32_t pulsos = 0;
    portENTER_CRITICAL(&_mux);
    _medicionActiva = false;
    pulsos = _contadorPulsos;
    _contadorPulsos = 0;
    portEXIT_CRITICAL(&_mux);

    // Cada pulso del balancín corresponde a 0.3 mm de columna de agua
    return static_cast<float>(pulsos) * _mmPorPulso;
}

uint32_t RainGauge::obtenerPulsosActuales() const {
    uint32_t pulsos = 0;
    portENTER_CRITICAL(&_mux);
    pulsos = _contadorPulsos;
    portEXIT_CRITICAL(&_mux);
    return pulsos;
}

void IRAM_ATTR RainGauge::manejarInterrupcion() {
    if (!_medicionActiva) return;

    uint32_t tiempoActual = millis();
    // Filtro antirrebote por software para contacto mecánico
    if ((tiempoActual - _ultimoTiempoPulsoMs) >= SigatokaConfig::PLUVIO_DEBOUNCE_TIME_MS) {
        _ultimoTiempoPulsoMs = tiempoActual;
        portENTER_CRITICAL_ISR(&_mux);
        _contadorPulsos++;
        portEXIT_CRITICAL_ISR(&_mux);
    }
}
