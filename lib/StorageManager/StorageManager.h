/**
 * @file StorageManager.h
 * @brief Gestor de almacenamiento híbrido: MicroSD (SPI) y LittleFS (Flash interna con respaldo).
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_STORAGE_MANAGER_H
#define SIGATOK_STORAGE_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <LittleFS.h>
#include "sigatok_config.h"
#include "sigatok_types.h"

class StorageManager {
public:
    StorageManager();

    /**
     * @brief Inicializa los buses de almacenamiento (MicroSD y LittleFS).
     * @return true si al menos un sistema de archivos está operativo.
     */
    bool begin();

    /**
     * @brief Guarda un registro en el archivo CSV de la MicroSD (o LittleFS si falla la SD).
     * @param registro Estructura con la trama completa adquirida.
     * @return true si se guardó con éxito.
     */
    bool guardarRegistro(const RegistroCompletoEstacion& registro);

    /**
     * @brief Registra un evento en la bitácora logs.txt.
     * @param marcaTiempo Cadena "YYYY-MM-DD HH:MM:SS".
     * @param mensaje Descripción del evento o alerta.
     */
    bool escribirLog(const String& marcaTiempo, const String& mensaje);

    /**
     * @brief Lee todo el contenido del CSV de datos (para descarga en Modo Servicio).
     */
    String leerCSV();

    /**
     * @brief Lee la bitácora logs.txt.
     */
    String leerLogs();

    /**
     * @brief Borra los datos del CSV preservando la cabecera.
     */
    bool borrarCSV();

    /**
     * @brief Desmonta ordenadamente la MicroSD antes de entrar en Deep Sleep.
     */
    void desmontarSD();

    bool sdDisponible() const;
    bool littleFsDisponible() const;

private:
    SPIClass _spiSD;
    bool     _sdInicializada;
    bool     _littleFsInicializado;

    void verificarCabeceraCSV();
    void volcarBufferFlashASD();
};

#endif // SIGATOK_STORAGE_MANAGER_H
