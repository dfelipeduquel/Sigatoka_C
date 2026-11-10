/**
 * @file ServiceMode.cpp
 * @brief Implementación del portal web de servicio para descarga y mantenimiento de campo.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "ServiceMode.h"
#include <esp_sleep.h>

volatile bool ServiceMode::_solicitudServicio = false;
portMUX_TYPE  ServiceMode::_muxBoton = portMUX_INITIALIZER_UNLOCKED;

ServiceMode::ServiceMode(StorageManager* storage, TimeManager* timeMgr, gpio_num_t pinBoton)
    : _storage(storage), _timeMgr(timeMgr), _pinBoton(pinBoton), _servidor(80), _servidorActivo(false) {}

void ServiceMode::begin() {
    pinMode(_pinBoton, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pinBoton), ServiceMode::manejarInterrupcionBoton, FALLING);

    // Habilitar despertar por nivel bajo en el pin del botón (EXT0)
    esp_sleep_enable_ext0_wakeup(_pinBoton, 0);
}

bool ServiceMode::despertoPorBoton() {
    esp_sleep_wakeup_cause_t causa = esp_sleep_get_wakeup_cause();
    return (causa == ESP_SLEEP_WAKEUP_EXT0);
}

bool ServiceMode::solicitudServicioPendiente() {
    bool estado = false;
    portENTER_CRITICAL(&_muxBoton);
    estado = _solicitudServicio;
    _solicitudServicio = false;
    portEXIT_CRITICAL(&_muxBoton);
    return estado;
}

void IRAM_ATTR ServiceMode::manejarInterrupcionBoton() {
    portENTER_CRITICAL_ISR(&_muxBoton);
    _solicitudServicio = true;
    portEXIT_CRITICAL_ISR(&_muxBoton);
}

void ServiceMode::configurarRutasWeb() {
    _servidor.on("/", HTTP_GET, [this]() { manejarRaiz(); });
    _servidor.on("/descargar", HTTP_GET, [this]() { manejarDescargarCSV(); });
    _servidor.on("/logs", HTTP_GET, [this]() { manejarDescargarLogs(); });
    _servidor.on("/borrar", HTTP_GET, [this]() { manejarBorrarCSV(); });
    _servidor.on("/salir", HTTP_GET, [this]() { manejarSalir(); });
}

void ServiceMode::manejarRaiz() {
    String fechaHora = (_timeMgr != nullptr) ? _timeMgr->obtenerCadenaCompleta() : "Desconocida";
    String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>SIGATOK - Modo Servicio</title>"
                  "<style>"
                  "body{font-family:Arial,sans-serif;background:#0d1117;color:#c9d1d9;padding:20px;text-align:center;}"
                  ".card{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:24px;max-width:480px;margin:auto;box-shadow:0 4px 12px rgba(0,0,0,0.5);}"
                  "h1{color:#2ea043;margin-bottom:8px;}p{font-size:14px;color:#8b949e;}"
                  ".btn{display:block;width:90%;margin:12px auto;padding:14px;background:#238636;color:#ffffff;text-decoration:none;border-radius:8px;font-weight:bold;font-size:16px;border:none;cursor:pointer;}"
                  ".btn:hover{background:#2ea043;}.btn-danger{background:#da3633;}.btn-danger:hover{background:#f85149;}"
                  ".status{margin:16px 0;padding:10px;background:#21262d;border-radius:6px;font-family:monospace;}"
                  "</style></head><body>"
                  "<div class='card'>"
                  "<h1>🌿 SIGATOK UTP</h1>"
                  "<p>Estación Agrometeorológica y Monitoreo Foliar</p>"
                  "<div class='status'>Fecha/Hora Estación: <br><strong>" + fechaHora + "</strong></div>"
                  "<a class='btn' href='/descargar'>📥 Descargar Datos CSV</a>"
                  "<a class='btn' href='/logs'>📋 Ver Bitácora de Logs</a>"
                  "<a class='btn btn-danger' href='/borrar' onclick=\"return confirm('¿Seguro de purgar el CSV?');\">🗑️ Borrar CSV</a>"
                  "<a class='btn' style='background:#388bfd;' href='/salir'>💤 Salir y Dormir</a>"
                  "</div></body></html>";
    _servidor.send(200, "text/html", html);
}

void ServiceMode::manejarDescargarCSV() {
    if (_storage != nullptr) {
        String csv = _storage->leerCSV();
        _servidor.sendHeader("Content-Disposition", "attachment; filename=MUNDONUEVO.csv");
        _servidor.send(200, "text/csv", csv);
        if (_timeMgr != nullptr) _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "CSV descargado via Web");
    } else {
        _servidor.send(500, "text/plain", "Error: StorageManager no vinculado.");
    }
}

void ServiceMode::manejarDescargarLogs() {
    if (_storage != nullptr) {
        String logs = _storage->leerLogs();
        _servidor.sendHeader("Content-Disposition", "attachment; filename=logs.txt");
        _servidor.send(200, "text/plain", logs);
        if (_timeMgr != nullptr) _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Logs descargados via Web");
    } else {
        _servidor.send(500, "text/plain", "Error: StorageManager no vinculado.");
    }
}

void ServiceMode::manejarBorrarCSV() {
    if (_storage != nullptr && _storage->borrarCSV()) {
        if (_timeMgr != nullptr) _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "CSV purgado via Web");
        _servidor.send(200, "text/html", "<html><body style='background:#0d1117;color:white;text-align:center;padding:50px;'>"
                                          "<h2>CSV Purged Correctamente</h2><a style='color:#58a6ff;' href='/'>Volver al Inicio</a></body></html>");
    } else {
        _servidor.send(500, "text/plain", "Fallo al purgar archivo CSV.");
    }
}

void ServiceMode::manejarSalir() {
    _servidor.send(200, "text/html", "<html><body style='background:#0d1117;color:white;text-align:center;padding:50px;'>"
                                      "<h2>Cerrando AP y entrando a modo ultra bajo consumo...</h2></body></html>");
    _servidorActivo = false;
}

void ServiceMode::ejecutarServidor() {
    if (_storage != nullptr && _timeMgr != nullptr) {
        _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Entrando en Modo Servicio");
    }

    // Configuración del Access Point WiFi
    WiFi.mode(WIFI_AP);
    IPAddress ipAP(192, 168, 4, 1);
    IPAddress netmask(255, 255, 255, 0);
    WiFi.softAPConfig(ipAP, ipAP, netmask);
    WiFi.softAP(SigatokaConfig::AP_SSID_SERVICIO, SigatokaConfig::AP_PASS_SERVICIO);

    configurarRutasWeb();
    _servidor.begin();
    _servidorActivo = true;

    uint32_t inicioServicio = millis();

    // Esperar liberación del botón si continuaba oprimido tras despertar
    while (digitalRead(_pinBoton) == LOW) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    while (_servidorActivo) {
        _servidor.handleClient();

        // 1. Timeout de inactividad
        if ((millis() - inicioServicio) >= SigatokaConfig::SERVICIO_TIMEOUT_MS) {
            if (_storage != nullptr && _timeMgr != nullptr) {
                _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Salida de Modo Servicio por Timeout");
            }
            break;
        }

        // 2. Salida por pulsación larga del botón físico (>= 1000 ms)
        if (digitalRead(_pinBoton) == LOW) {
            uint32_t inicioPulsacion = millis();
            while (digitalRead(_pinBoton) == LOW) {
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            uint32_t duracion = millis() - inicioPulsacion;
            if (duracion >= SigatokaConfig::PULSACION_SALIR_MS) {
                if (_storage != nullptr && _timeMgr != nullptr) {
                    _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Salida de Modo Servicio por pulsacion fisica");
                }
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Cede tiempo al watchdog y FreeRTOS
    }

    _servidor.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
}
