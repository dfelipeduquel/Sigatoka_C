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
#include <vector>
#include "sigatok_config.h"
#include "sigatok_types.h"

class StorageManager {
public:
    StorageManager();

    /**
     * @brief Inicializa los buses de almacenamiento (LittleFS y MicroSD SPI).
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
     * @brief Registra un evento en la bitácora logs.txt (con respaldo en LittleFS).
     * @param marcaTiempo Cadena "YYYY-MM-DD HH:MM:SS".
     * @param mensaje Descripción del evento o alerta.
     */
    bool escribirLog(const String& marcaTiempo, const String& mensaje);

    /**
     * @brief Abre el archivo CSV en modo lectura para streaming HTTP sin saturar la RAM.
     */
    File abrirLecturaCSV();

    /**
     * @brief Abre el archivo de logs en modo lectura para streaming HTTP.
     */
    File abrirLecturaLogs();

    /**
     * @brief Obtiene de forma segura en RAM solo los últimos N registros del CSV.
     */
    bool leerUltimosRegistros(size_t maxRegistros, String& cabeceraOut, std::vector<String>& filasOut);

    /**
     * @brief Borra los datos del CSV preservando la cabecera (exacto a Header_ModoServicio.py).
     */
    bool borrarCSV();

    /**
     * @brief Desmonta ordenadamente la MicroSD y libera el bus SPI antes de Deep Sleep.
     */
    void desmontarSD();

    bool sdDisponible() const;
    bool littleFsDisponible() const;
    static const char* obtenerCabeceraCSV();

private:
    SPIClass _spiSD;
    bool     _sdInicializada;
    bool     _littleFsInicializado;

    void verificarCabeceraCSV();
    void volcarBufferFlashASD();
    void volcarLogsFlashASD();
    void copiarArchivoEnBloques(File& origen, File& destino);
};

#endif // SIGATOK_STORAGE_MANAGER_H
