/**
 * @file WindVane.cpp
 * @brief Implementación del driver para veleta de dirección vía ADS1115 con filtro EMA.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "WindVane.h"

// Calibración de cuadrantes 1:1 según mediciones legacy de Header_sensoresEstacion.py
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

WindVane::WindVane(Adafruit_ADS1115* ads, uint8_t canal, float alphaEma)
    : _ads(ads), _canal(canal), _filtroEma(alphaEma), _tieneMuestras(false) {}

const char* WindVane::interpretarVoltaje(float voltaje) {
    for (const auto& rango : TABLA_RANGOS) {
        if (voltaje >= rango.inferior && voltaje < rango.superior) {
            return rango.etiqueta;
        }
    }
    return "Fuera de rango";
}

void WindVane::reiniciarFiltro() {
    _filtroEma.reiniciar(0.0f);
    _tieneMuestras = false;
}

float WindVane::muestrear() {
    if (_ads == nullptr) return 0.0f;

    int16_t adcBits = _ads->readADC_SingleEnded(_canal);
    float voltios = _ads->computeVolts(adcBits);
    float filtrado = _filtroEma.filtrar(voltios);
    _tieneMuestras = true;
    return filtrado;
}

const char* WindVane::obtenerDireccion(float* voltajeSalida) {
    if (_ads == nullptr) {
        if (voltajeSalida) *voltajeSalida = 0.0f;
        return "Fuera de rango";
    }

    // Ráfaga rápida no bloqueante de 8 muestras (~80 ms) filtradas por EMA al cierre de ventana:
    // suprime ruido eléctrico del ADC sin promediar rumbos distintos de 60 segundos atrás.
    reiniciarFiltro();
    for (uint8_t i = 0; i < 8; i++) {
        muestrear();
        if (i < 7) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    float voltiosFiltrados = _filtroEma.obtenerValor();
    if (voltajeSalida) {
        *voltajeSalida = voltiosFiltrados;
    }

    return interpretarVoltaje(voltiosFiltrados);
}
