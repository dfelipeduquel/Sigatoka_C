/**
 * @file WindVane.h
 * @brief Driver para veleta anemométrica analógica leída vía ADS1115 (I2C).
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_WIND_VANE_H
#define SIGATOK_WIND_VANE_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "sigatok_config.h"

struct RangoDireccion {
    float inferior;
    float superior;
    const char* etiqueta;
};

class WindVane {
public:
    explicit WindVane(Adafruit_ADS1115* ads, uint8_t canal = SigatokaConfig::ADS_CANAL_VELETA);

    /**
     * @brief Mide la tensión analógica de la veleta y determina la dirección del viento.
     * @param voltajeSalida Puntero opcional para retornar el voltaje medido (V).
     * @return Cadena con el cuadrante cardinal ("N", "NE", "E", "SE", "S", "SW", "W", etc.).
     */
    const char* obtenerDireccion(float* voltajeSalida = nullptr);

    /**
     * @brief Convierte una lectura de tensión en su correspondiente rumbo cardinal.
     */
    static const char* interpretarVoltaje(float voltaje);

private:
    Adafruit_ADS1115* _ads;
    uint8_t           _canal;

    static const RangoDireccion TABLA_RANGOS[8];
};

#endif // SIGATOK_WIND_VANE_H
