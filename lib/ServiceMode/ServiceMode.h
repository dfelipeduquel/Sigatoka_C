/**
 * @file ServiceMode.h
 * @brief Servidor Web y Punto de Acceso (AP) para modo de mantenimiento en campo.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_SERVICE_MODE_H
#define SIGATOK_SERVICE_MODE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "sigatok_config.h"
#include "StorageManager.h"
#include "TimeManager.h"

class ServiceMode {
public:
    explicit ServiceMode(StorageManager* storage, TimeManager* timeMgr,
                         gpio_num_t pinBoton = SigatokaConfig::PIN_BOTON_SERVICIO);

    /**
     * @brief Configura el pin del botón y la interrupción EXT0 de despertar.
     */
    void begin();

    /**
     * @brief Verifica si el despertar del microcontrolador fue ocasionado por el botón EXT0.
     */
    bool despertoPorBoton();

    /**
     * @brief Inicia el Access Point y el servidor Web interactivo hasta que el operador decida salir o expire el timeout.
     */
    void ejecutarServidor();

    /**
     * @brief Verifica si se ha presionado el botón para solicitar entrar a servicio durante la ejecución.
     */
    bool solicitudServicioPendiente();

    /**
     * @brief ISR para capturar la pulsación del botón.
     */
    static void IRAM_ATTR manejarInterrupcionBoton();

private:
    StorageManager* _storage;
    TimeManager*    _timeMgr;
    gpio_num_t      _pinBoton;
    WebServer       _servidor;
    bool            _servidorActivo;

    static volatile bool _solicitudServicio;
    static portMUX_TYPE  _muxBoton;

    void configurarRutasWeb();
    void manejarRaiz();
    void manejarDescargarCSV();
    void manejarDescargarLogs();
    void manejarBorrarCSV();
    void manejarSalir();
};

#endif // SIGATOK_SERVICE_MODE_H
