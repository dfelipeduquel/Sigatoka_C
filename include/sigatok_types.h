/**
 * @file sigatok_types.h
 * @brief Estructuras de datos, tramas agroclimáticas y estados del sistema SIGATOK.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @institution Universidad Tecnológica de Pereira (UTP) - Semillero Kilby
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_TYPES_H
#define SIGATOK_TYPES_H

#include <Arduino.h>
#include "sigatok_config.h"

/**
 * @brief Estado operativo del sistema para la máquina de estados.
 */
enum class EstadoOperativo : uint8_t {
    INICIO_WAKEUP,
    MODO_SERVICIO,
    MUESTREO_METEOROLOGICO,
    LECTURA_FOLIAR,
    PROCESAMIENTO_SENALES,
    ALMACENAMIENTO_SD,
    TELEMETRIA_TELEGRAM,
    ACTIVACION_CAMARA,
    PREPARACION_DEEP_SLEEP,
    FALLA_CRITICA
};

/**
 * @brief Trazabilidad del origen de reinicio o despertar del microcontrolador.
 */
enum class CausaDespertar : uint8_t {
    TEMPORIZADOR_DEEP_SLEEP,
    BOTON_SERVICIO_EXT0,
    REINICIO_POWER_ON,
    DESCONOCIDO
};

/**
 * @brief Datos consolidados de variables meteorológicas clásicas.
 */
struct DatosMeteorologicos {
    float lluvia_mm_min        = 0.0f; // Precipitación acumulada en la ventana (mm/min)
    float velocidad_viento_ms  = 0.0f; // Velocidad media de viento (m/s)
    char  direccion_viento[18] = "Fuera de rango"; // Rosa de los vientos (N, NE, E, SE, S, SW, W/N, W, Fuera de rango)
    float temperatura_c        = 0.0f; // Temperatura ambiente (°C)
    float humedad_relativa_pct = 0.0f; // Humedad relativa ambiente (%)
    bool  valido               = false;// Bandera de consistencia de lectura
};

/**
 * @brief Datos agroecológicos especializados en humectación foliar y riesgo de Sigatoka (Fase 3).
 */
struct DatosFoliares {
    int16_t  adc_crudo           = 0;     // Lectura raw del convertidor A/D
    float    voltaje_filtrado    = 0.0f;  // Voltaje tras filtrado EMA (V)
    float    porcentaje_humedad  = 0.0f;  // 0.0% (Seco) a 100.0% (Saturado de rocío/agua)
    bool     hoja_humectada      = false; // Cumple umbral para germinación de conidios/ascosporas
    uint32_t duracion_humeda_min = 0;     // Minutos continuos con presencia de agua libre
};

/**
 * @brief Trama unificada para almacenamiento en MicroSD y envío de telemetría.
 */
struct RegistroCompletoEstacion {
    char fecha[12] = "1970-01-01"; // AAAA-MM-DD
    char hora[10]  = "00:00:00";   // HH:MM:SS
    DatosMeteorologicos meteo;
    DatosFoliares foliar;
    char nombre_imagen[32] = "";   // Reservado Fase 5 (IMG_AAAAMMDD_HHMMSS.JPEG)

    /**
     * @brief Serializa el registro respetando el formato CSV de la fase activa.
     * En Fase 1 mantiene paridad 1:1 con el esquema legacy de MUNDONUEVO.csv.
     */
    String aFormatoCSV() const {
        char buffer[200];
        int escritos = snprintf(buffer, sizeof(buffer),
                                "%s,%s,%.2f,%.3f,%s,%.2f,%.2f",
                                fecha,
                                hora,
                                meteo.lluvia_mm_min,
                                meteo.velocidad_viento_ms,
                                meteo.direccion_viento,
                                meteo.temperatura_c,
                                meteo.humedad_relativa_pct);

#if SIGATOK_ENABLE_LEAF_WETNESS
        escritos += snprintf(buffer + escritos, sizeof(buffer) - escritos,
                             ",%.1f,%u",
                             foliar.porcentaje_humedad,
                             foliar.duracion_humeda_min);
#endif

#if SIGATOK_ENABLE_CAM
        escritos += snprintf(buffer + escritos, sizeof(buffer) - escritos,
                             ",%s",
                             nombre_imagen);
#endif
        (void)escritos;
        return String(buffer);
    }
};

#endif // SIGATOK_TYPES_H
