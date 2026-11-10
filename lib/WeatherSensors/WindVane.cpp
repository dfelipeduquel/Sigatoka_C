/**
 * @file WindVane.cpp
 * @brief Implementación del driver para veleta de dirección vía ADS1115.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "WindVane.h"

// Calibración de cuadrantes según mediciones de tensión legacy del Semillero Kilby
const RangoDireccion WindVane::TABLA_RANGOS[8] = {
    {3.700f, 3.940f, "N"},    // N  ~ 3.785 V
    {1.810f, 2.625f, "NE"},   // NE ~ 2.221 V
    {0.000f, 0.675f, "E"},    // E  ~ 0.450 V
    {0.676f, 1.130f, "SE"},   // SE ~ 0.890 V
    {1.131f, 1.800f, "S"},    // S  ~ 1.381 V
    {2.625f, 3.425f, "SW"},   // SW ~ 3.034 V
    {3.425f, 3.945f, "W/N"},  // W/N intermedio
    {3.946f, 4.100f, "W"}     // W ~ 4.096 V / NW
};

WindVane::WindVane(Adafruit_ADS1115* ads, uint8_t canal)
    : _ads(ads), _canal(canal) {}

const char* WindVane::interpretarVoltaje(float voltaje) {
    for (const auto& rango : TABLA_RANGOS) {
        if (voltaje >= rango.inferior && voltaje < rango.superior) {
            return rango.etiqueta;
        }
    }
    return "Fuera de rango";
}

const char* WindVane::obtenerDireccion(float* voltajeSalida) {
    if (_ads == nullptr) {
        if (voltajeSalida) *voltajeSalida = 0.0f;
        return "ERROR_ADC";
    }

    // Lectura del canal configurado en el ADS1115 (16 bits)
    int16_t adcBits = _ads->readADC_SingleEnded(_canal);
    // Conversión a voltios considerando la ganancia configurada (p. ej. GAIN_ONE: +/-4.096V -> 0.125mV/bit)
    float voltios = _ads->computeVolts(adcBits);

    if (voltajeSalida) {
        *voltajeSalida = voltios;
    }

    return interpretarVoltaje(voltios);
}
