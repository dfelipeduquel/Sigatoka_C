/**
 * @file TimeManager.h
 * @brief Gestor de tiempo para RTC externo (DS1307 / TinyRTC) con sincronización y respaldo de RTC interno.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_TIME_MANAGER_H
#define SIGATOK_TIME_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

class TimeManager {
public:
    TimeManager();

    /**
     * @brief Inicializa la comunicación I2C con el TinyRTC y sincroniza la hora local.
     * @param wire Puntero a la instancia TwoWire configurada (pines 26 y 27).
     * @return true si el RTC externo respondió; false si opera en modo respaldo interno.
     */
    bool begin(TwoWire* wire = &Wire);

    /**
     * @brief Obtiene la fecha y hora formateadas.
     * @param bufferFecha Salida para fecha "YYYY-MM-DD" (mínimo 12 bytes).
     * @param bufferHora Salida para hora "HH:MM:SS" (mínimo 10 bytes).
     */
    void obtenerMarcasTiempo(char* bufferFecha, char* bufferHora);

    /**
     * @brief Obtiene una cadena ISO completa "YYYY-MM-DD HH:MM:SS".
     */
    String obtenerCadenaCompleta();

    /**
     * @brief Ajusta la hora del RTC externo e interno.
     */
    void ajustarFechaHora(uint16_t anio, uint8_t mes, uint8_t dia, uint8_t hora, uint8_t minuto, uint8_t segundo);

    bool rtcExternoDisponible() const;

private:
    RTC_DS1307 _rtcDS1307;
    TwoWire*   _wire;
    bool       _disponible;
};

#endif // SIGATOK_TIME_MANAGER_H
