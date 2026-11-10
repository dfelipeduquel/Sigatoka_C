/**
 * @file LeafWetnessSensor.h
 * @brief Driver de Humectación Foliar con filtrado EMA y modelo de duración de hoja mojada.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_LEAF_WETNESS_SENSOR_H
#define SIGATOK_LEAF_WETNESS_SENSOR_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "sigatok_config.h"
#include "sigatok_types.h"
#include "EmaFilter.h"

class LeafWetnessSensor {
public:
    explicit LeafWetnessSensor(Adafruit_ADS1115* ads,
                               uint8_t canal = SigatokaConfig::ADS_CANAL_HUMECTACION,
                               float alphaEma = SigatokaConfig::EMA_ALPHA_FOLIAR,
                               float voltajeSeco = SigatokaConfig::VOLTAJE_HOJA_SECA,
                               float voltajeSaturado = SigatokaConfig::VOLTAJE_HOJA_SATURADA);

    /**
     * @brief Realiza la lectura del ADC, filtra el ruido eléctrico y calcula las métricas foliares.
     * @param minutosTranscurridos Minutos transcurridos desde el ciclo anterior para acumular duración de mojado.
     * @return Estructura DatosFoliares con los valores procesados.
     */
    DatosFoliares actualizar(uint32_t minutosTranscurridos = 0);

    /**
     * @brief Restablece el contador acumulado de horas/minutos de hoja mojada.
     */
    void reiniciarDuracionHumectacion();

    /**
     * @brief Retorna el valor actual de los datos foliares sin realizar nueva conversión.
     */
    const DatosFoliares& obtenerUltimosDatos() const;

private:
    Adafruit_ADS1115* _ads;
    uint8_t           _canal;
    EmaFilter         _filtroEma;
    float             _voltajeSeco;
    float             _voltajeSaturado;
    DatosFoliares     _ultimosDatos;
};

#endif // SIGATOK_LEAF_WETNESS_SENSOR_H
