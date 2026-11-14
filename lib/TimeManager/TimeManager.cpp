/**
 * @file TimeManager.cpp
 * @brief Implementación del gestor de tiempo RTC externo con respaldo.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "TimeManager.h"
#include <sys/time.h>

namespace {
    inline bool fechaValida(const DateTime& dt) {
        return (dt.year() >= 2020 && dt.year() <= 2099 &&
                dt.month() >= 1 && dt.month() <= 12 &&
                dt.day() >= 1 && dt.day() <= 31);
    }
}

TimeManager::TimeManager() : _wire(&Wire), _disponible(false) {}

bool TimeManager::begin(TwoWire* wire) {
    _wire = (wire != nullptr) ? wire : &Wire;
    if (_rtcDS1307.begin(_wire)) {
        if (_rtcDS1307.isrunning()) {
            DateTime ahora = _rtcDS1307.now();
            if (fechaValida(ahora)) {
                _disponible = true;
                // Sincronizar el reloj del sistema ESP32 con el RTC externo
                struct timeval tv;
                tv.tv_sec = ahora.unixtime();
                tv.tv_usec = 0;
                settimeofday(&tv, nullptr);
                return true;
            }
        }
    }
    _disponible = false;
    return false;
}

void TimeManager::obtenerMarcasTiempo(char* bufferFecha, char* bufferHora) {
    if (_disponible) {
        DateTime ahora = _rtcDS1307.now();
        if (fechaValida(ahora)) {
            snprintf(bufferFecha, 12, "%04d-%02d-%02d", ahora.year(), ahora.month(), ahora.day());
            snprintf(bufferHora, 10, "%02d:%02d:%02d", ahora.hour(), ahora.minute(), ahora.second());
            return;
        }
    }

    // Modo de respaldo con el RTC interno del ESP32
    time_t ahora;
    time(&ahora);
    struct tm infoTiempo;
    localtime_r(&ahora, &infoTiempo);

    snprintf(bufferFecha, 12, "%04d-%02d-%02d",
             infoTiempo.tm_year + 1900, infoTiempo.tm_mon + 1, infoTiempo.tm_mday);
    snprintf(bufferHora, 10, "%02d:%02d:%02d",
             infoTiempo.tm_hour, infoTiempo.tm_min, infoTiempo.tm_sec);
}

String TimeManager::obtenerCadenaCompleta() {
    char fecha[12];
    char hora[10];
    obtenerMarcasTiempo(fecha, hora);
    return String(fecha) + " " + String(hora);
}

void TimeManager::ajustarFechaHora(uint16_t anio, uint8_t mes, uint8_t dia,
                                   uint8_t hora, uint8_t minuto, uint8_t segundo) {
    DateTime nuevaFecha(anio, mes, dia, hora, minuto, segundo);
    if (_rtcDS1307.begin(_wire)) {
        _rtcDS1307.adjust(nuevaFecha);
        _disponible = true;
    }

    struct timeval tv;
    tv.tv_sec = nuevaFecha.unixtime();
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
}

bool TimeManager::rtcExternoDisponible() const {
    return _disponible;
}
