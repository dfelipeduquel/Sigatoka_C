/**
 * @file Anemometer.cpp
 * @brief Implementación del driver para anemómetro de cazoletas.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "Anemometer.h"
#include <esp_timer.h>

volatile uint32_t Anemometer::_contadorPulsos = 0;
volatile uint64_t Anemometer::_ultimoTiempoPulsoUs = 0;
volatile uint64_t Anemometer::_debounceStaticUs = SigatokaConfig::ANEMO_DEBOUNCE_TIME_MS * 1000ULL;
volatile bool     Anemometer::_medicionActiva = false;
portMUX_TYPE      Anemometer::_mux = portMUX_INITIALIZER_UNLOCKED;

Anemometer::Anemometer(gpio_num_t pin, float factorVelocidad, uint32_t debounceMs)
    : _pin(pin), _factorVelocidad(factorVelocidad), _debounceMs(debounceMs) {}

void Anemometer::begin() {
    _debounceStaticUs = static_cast<uint64_t>(_debounceMs) * 1000ULL;
    pinMode(_pin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pin), Anemometer::manejarInterrupcion, FALLING);
}

void Anemometer::iniciarVentana() {
    uint64_t ahoraUs = static_cast<uint64_t>(esp_timer_get_time());
    portENTER_CRITICAL(&_mux);
    _contadorPulsos = 0;
    _medicionActiva = true;
    _ultimoTiempoPulsoUs = (ahoraUs >= _debounceStaticUs) ? (ahoraUs - _debounceStaticUs) : 0;
    portEXIT_CRITICAL(&_mux);
}

float Anemometer::finalizarYObtenerVelocidad(float duracionVentanaSegundos) {
    uint32_t pulsos = 0;
    portENTER_CRITICAL(&_mux);
    _medicionActiva = false;
    pulsos = _contadorPulsos;
    _contadorPulsos = 0;
    portEXIT_CRITICAL(&_mux);

    if (duracionVentanaSegundos <= 0.0f) {
        duracionVentanaSegundos = 60.0f;
    }

    // Frecuencia en Hertz = pulsos / segundos transcurridos
    float frecuenciaHz = static_cast<float>(pulsos) / duracionVentanaSegundos;

    // Velocidad (m/s) = frecuencia (Hz) * factorVelocidad (0.188 m/s/Hz, ancla legacy)
    return frecuenciaHz * _factorVelocidad;
}

uint32_t Anemometer::obtenerPulsosActuales() const {
    uint32_t pulsos = 0;
    portENTER_CRITICAL(&_mux);
    pulsos = _contadorPulsos;
    portEXIT_CRITICAL(&_mux);
    return pulsos;
}

void IRAM_ATTR Anemometer::manejarInterrupcion() {
    if (!_medicionActiva) return;

    uint64_t tiempoActualUs = static_cast<uint64_t>(esp_timer_get_time());
    portENTER_CRITICAL_ISR(&_mux);
    if ((tiempoActualUs - _ultimoTiempoPulsoUs) >= _debounceStaticUs) {
        _ultimoTiempoPulsoUs = tiempoActualUs;
        _contadorPulsos++;
    }
    portEXIT_CRITICAL_ISR(&_mux);
}
