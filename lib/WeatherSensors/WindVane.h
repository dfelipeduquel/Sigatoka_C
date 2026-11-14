/**
 * @file WindVane.h
 * @brief Driver para veleta anemométrica analógica leída vía ADS1115 (I2C) con filtrado EMA.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_WIND_VANE_H
#define SIGATOK_WIND_VANE_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "sigatok_config.h"
#include "EmaFilter.h"

struct RangoDireccion {
    float inferior;
    float superior;
    const char* etiqueta;
};

class WindVane {
public:
    explicit WindVane(Adafruit_ADS1115* ads,
                      uint8_t canal = SigatokaConfig::ADS_CANAL_VELETA,
                      float alphaEma = SigatokaConfig::EMA_ALPHA_VELETA);

    /**
     * @brief Toma una lectura del canal ADC del ADS1115 y actualiza el filtro EMA.
     * @return Tensión filtrada actual (V).
     */
    float muestrear();

    /**
     * @brief Reinicia el historial del filtro EMA de la veleta (inicio de nueva ventana).
     */
    void reiniciarFiltro();

    /**
     * @brief Obtiene la dirección del viento basada en la tensión filtrada por EMA.
     * Si no hubo muestreos previos en la ventana, ejecuta una ráfaga rápida no bloqueante.
     * @param voltajeSalida Puntero opcional para retornar el voltaje filtrado (V).
     * @return Cadena con el cuadrante cardinal ("N", "NE", "E", "SE", "S", "SW", "W/N", "W", "Fuera de rango").
     */
    const char* obtenerDireccion(float* voltajeSalida = nullptr);

    /**
     * @brief Convierte una lectura de tensión en su correspondiente rumbo cardinal.
     */
    static const char* interpretarVoltaje(float voltaje);

private:
    Adafruit_ADS1115* _ads;
    uint8_t           _canal;
    EmaFilter         _filtroEma;
    bool              _tieneMuestras;

    static const RangoDireccion TABLA_RANGOS[8];
};

#endif // SIGATOK_WIND_VANE_H
