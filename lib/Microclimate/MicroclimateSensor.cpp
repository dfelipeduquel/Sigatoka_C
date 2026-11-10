/**
 * @file MicroclimateSensor.cpp
 * @brief Implementación del sensor termo-higrométrico AM2305B.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "MicroclimateSensor.h"

MicroclimateSensor::MicroclimateSensor(gpio_num_t pin, uint8_t tipo)
    : _pin(pin),
      _dht(pin, tipo),
      _ultimoMuestreoMs(0),
      _ultimaTemperatura(0.0f),
      _ultimaHumedad(0.0f) {}

void MicroclimateSensor::begin() {
    _dht.begin();
    _ultimoMuestreoMs = millis() - INTERVALO_MINIMO_MS; // Permitir lectura inmediata
}

bool MicroclimateSensor::leer(float* temperatura, float* humedad) {
    uint32_t tiempoActual = millis();

    // Evitar consultar el bus DHT antes de cumplir su tiempo de estabilización térmico
    if ((tiempoActual - _ultimoMuestreoMs) < INTERVALO_MINIMO_MS && _ultimoMuestreoMs != 0) {
        if (temperatura) *temperatura = _ultimaTemperatura;
        if (humedad)     *humedad     = _ultimaHumedad;
        return true;
    }

    float t = _dht.readTemperature();
    float h = _dht.readHumidity();

    if (isnan(t) || isnan(h)) {
        return false;
    }

    _ultimaTemperatura = t;
    _ultimaHumedad     = h;
    _ultimoMuestreoMs  = tiempoActual;

    if (temperatura) *temperatura = t;
    if (humedad)     *humedad     = h;

    return true;
}

float MicroclimateSensor::obtenerUltimaTemperatura() const {
    return _ultimaTemperatura;
}

float MicroclimateSensor::obtenerUltimaHumedad() const {
    return _ultimaHumedad;
}
