/**
 * @file MicroclimateSensor.h
 * @brief Driver para sensor termo-higrométrico AM2305B / DHT22 sin retardos bloqueantes.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_MICROCLIMATE_SENSOR_H
#define SIGATOK_MICROCLIMATE_SENSOR_H

#include <Arduino.h>
#include <DHT.h>
#include "sigatok_config.h"

class MicroclimateSensor {
public:
    explicit MicroclimateSensor(gpio_num_t pin = SigatokaConfig::PIN_AM2305B, uint8_t tipo = DHT22);

    /**
     * @brief Inicializa el bus digital del sensor.
     */
    void begin();

    /**
     * @brief Realiza la lectura no bloqueante respetando el intervalo de muestreo.
     * @param temperatura Puntero para almacenar la temperatura en °C.
     * @param humedad Puntero para almacenar la humedad relativa en %.
     * @return true si la lectura fue exitosa y pasó la verificación CRC; false en caso contrario.
     */
    bool leer(float* temperatura, float* humedad);

    float obtenerUltimaTemperatura() const;
    float obtenerUltimaHumedad() const;

private:
    gpio_num_t _pin;
    DHT        _dht;
    uint32_t   _ultimoMuestreoMs;
    float      _ultimaTemperatura;
    float      _ultimaHumedad;
    static constexpr uint32_t INTERVALO_MINIMO_MS = 2000; // DHT22 requiere >= 2s entre muestreos
};

#endif // SIGATOK_MICROCLIMATE_SENSOR_H
