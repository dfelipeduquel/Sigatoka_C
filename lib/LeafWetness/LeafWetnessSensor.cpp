/**
 * @file LeafWetnessSensor.cpp
 * @brief Implementación del sensor de humectación foliar para prevención de Sigatoka.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "LeafWetnessSensor.h"

LeafWetnessSensor::LeafWetnessSensor(Adafruit_ADS1115* ads,
                                     uint8_t canal,
                                     float alphaEma,
                                     float voltajeSeco,
                                     float voltajeSaturado)
    : _ads(ads),
      _canal(canal),
      _filtroEma(alphaEma),
      _voltajeSeco(voltajeSeco),
      _voltajeSaturado(voltajeSaturado) {
    _ultimosDatos = DatosFoliares{};
}

DatosFoliares LeafWetnessSensor::actualizar(uint32_t minutosTranscurridos) {
    if (_ads == nullptr) {
        _ultimosDatos.hoja_humectada = false;
        _ultimosDatos.porcentaje_humedad = 0.0f;
        return _ultimosDatos;
    }

    // 1. Lectura del ADC de 16 bits en el canal de humectación foliar
    int16_t rawBits = _ads->readADC_SingleEnded(_canal);
    float voltajeCrudo = _ads->computeVolts(rawBits);

    // 2. Filtrado de ruido eléctrico mediante Media Móvil Exponencial (EMA)
    float voltajeFiltrado = _filtroEma.filtrar(voltajeCrudo);

    // 3. Normalización y cálculo de porcentaje de lámina de agua libre
    // A menor tensión (o mayor conductividad), mayor presencia de agua líquida sobre la cutícula
    float porcentaje = 0.0f;
    if (_voltajeSeco != _voltajeSaturado) {
        porcentaje = ((_voltajeSeco - voltajeFiltrado) / (_voltajeSeco - _voltajeSaturado)) * 100.0f;
    }

    if (porcentaje < 0.0f)   porcentaje = 0.0f;
    if (porcentaje > 100.0f) porcentaje = 100.0f;

    // 4. Evaluación agronómica del umbral de riesgo de infección (Pseudocercospora fijiensis)
    bool estaMojada = (porcentaje >= SigatokaConfig::UMBRAL_HUMECTACION_ACTIVA);

    if (estaMojada) {
        _ultimosDatos.duracion_humeda_min += minutosTranscurridos;
    } else {
        // Al secarse la lámina foliar por radiación/viento, se reinicia la duración continua
        _ultimosDatos.duracion_humeda_min = 0;
    }

    _ultimosDatos.adc_crudo = rawBits;
    _ultimosDatos.voltaje_filtrado = voltajeFiltrado;
    _ultimosDatos.porcentaje_humedad = porcentaje;
    _ultimosDatos.hoja_humectada = estaMojada;

    return _ultimosDatos;
}

void LeafWetnessSensor::reiniciarDuracionHumectacion() {
    _ultimosDatos.duracion_humeda_min = 0;
}

const DatosFoliares& LeafWetnessSensor::obtenerUltimosDatos() const {
    return _ultimosDatos;
}
