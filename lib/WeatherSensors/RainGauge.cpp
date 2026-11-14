/**
 * @file RainGauge.cpp
 * @brief Implementación del driver para pluviómetro de balancín.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "RainGauge.h"
#include <esp_timer.h>

volatile uint32_t RainGauge::_contadorPulsos = 0;
volatile uint64_t RainGauge::_ultimoTiempoPulsoUs = 0;
volatile uint64_t RainGauge::_debounceStaticUs = SigatokaConfig::PLUVIO_DEBOUNCE_TIME_MS * 1000ULL;
volatile bool     RainGauge::_medicionActiva = false;
portMUX_TYPE      RainGauge::_mux = portMUX_INITIALIZER_UNLOCKED;

RainGauge::RainGauge(gpio_num_t pin, float mmPorPulso, uint32_t debounceMs)
    : _pin(pin), _mmPorPulso(mmPorPulso), _debounceMs(debounceMs) {}

void RainGauge::begin() {
    _debounceStaticUs = static_cast<uint64_t>(_debounceMs) * 1000ULL;
    pinMode(_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pin), RainGauge::manejarInterrupcion, FALLING);
}

void RainGauge::iniciarVentana() {
    uint64_t ahoraUs = static_cast<uint64_t>(esp_timer_get_time());
    portENTER_CRITICAL(&_mux);
    _contadorPulsos = 0;
    _medicionActiva = true;
    // Permitir que el primer pulso válido tras iniciarVentana() se capture inmediatamente
    _ultimoTiempoPulsoUs = (ahoraUs >= _debounceStaticUs) ? (ahoraUs - _debounceStaticUs) : 0;
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

    uint64_t tiempoActualUs = static_cast<uint64_t>(esp_timer_get_time());
    portENTER_CRITICAL_ISR(&_mux);
    // Filtro antirrebote por software en microsegundos seguro dentro de IRAM_ATTR
    if ((tiempoActualUs - _ultimoTiempoPulsoUs) >= _debounceStaticUs) {
        _ultimoTiempoPulsoUs = tiempoActualUs;
        _contadorPulsos++;
    }
    portEXIT_CRITICAL_ISR(&_mux);
}
