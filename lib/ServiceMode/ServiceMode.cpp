/**
 * @file ServiceMode.cpp
 * @brief Implementación del portal web de servicio con streaming seguro desde SD/LittleFS.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "ServiceMode.h"
#include <esp_sleep.h>
#include <vector>

volatile bool ServiceMode::_solicitudServicio = false;
portMUX_TYPE  ServiceMode::_muxBoton = portMUX_INITIALIZER_UNLOCKED;

ServiceMode::ServiceMode(StorageManager* storage, TimeManager* timeMgr, gpio_num_t pinBoton)
    : _storage(storage), _timeMgr(timeMgr), _pinBoton(pinBoton), _servidor(80), _servidorActivo(false) {}

void ServiceMode::begin() {
    pinMode(_pinBoton, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(_pinBoton), ServiceMode::manejarInterrupcionBoton, FALLING);

    // Habilitar despertar por nivel bajo en el pin del botón (EXT0, igual a legacy wake_on_ext0)
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
    _servidor.on("/apagar", HTTP_GET, [this]() { manejarSalir(); });
    _servidor.on("/salir", HTTP_GET, [this]() { manejarSalir(); });
}

void ServiceMode::manejarRaiz() {
    String fechaHora = (_timeMgr != nullptr) ? _timeMgr->obtenerCadenaCompleta() : "Desconocida";

    String html = "<!DOCTYPE html><html lang='es'><head><meta charset='UTF-8'>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                  "<title>SIGATOK - Estación Meteorológica</title>"
                  "<style>"
                  "body{font-family:Arial,sans-serif;background:#0d1117;color:#c9d1d9;padding:16px;text-align:center;}"
                  ".card{background:#161b22;border:1px solid #30363d;border-radius:12px;padding:20px;max-width:600px;margin:auto;box-shadow:0 4px 12px rgba(0,0,0,0.5);}"
                  "h1{color:#2ea043;margin-bottom:6px;}p{font-size:14px;color:#8b949e;}"
                  ".btn{display:block;width:92%;margin:10px auto;padding:13px;background:#238636;color:#ffffff;text-decoration:none;border-radius:8px;font-weight:bold;font-size:15px;border:none;cursor:pointer;}"
                  ".btn:hover{background:#2ea043;}.btn-danger{background:#da3633;}.btn-danger:hover{background:#f85149;}"
                  ".status{margin:14px 0;padding:10px;background:#21262d;border-radius:6px;font-family:monospace;font-size:13px;}"
                  ".table-wrap{overflow-x:auto;margin:14px 0;background:#0b1120;border-radius:8px;padding:8px;}"
                  "table{width:100%;border-collapse:collapse;font-size:12px;}"
                  "th,td{padding:6px;border-bottom:1px solid #21262d;text-align:center;}"
                  "th{color:#2ea043;}"
                  "</style></head><body>"
                  "<div class='card'>"
                  "<h1>🌿 SIGATOK UTP</h1>"
                  "<p>Estación Agrometeorológica — Modo Servicio</p>"
                  "<div class='status'>Fecha/Hora: <strong>" + fechaHora + "</strong> | SD: <strong>" +
                  ((_storage != nullptr && _storage->sdDisponible()) ? "OK" : "Respaldo Flash") +
                  "</strong></div>";

    if (_storage != nullptr) {
        String cabecera;
        std::vector<String> filas;
        if (_storage->leerUltimosRegistros(5, cabecera, filas) && !filas.empty()) {
            html += "<div class='table-wrap'><table><tr>";
            int inicio = 0;
            while (inicio <= static_cast<int>(cabecera.length())) {
                int coma = cabecera.indexOf(',', inicio);
                String col = (coma == -1) ? cabecera.substring(inicio) : cabecera.substring(inicio, coma);
                html += "<th>" + col + "</th>";
                if (coma == -1) break;
                inicio = coma + 1;
            }
            html += "</tr>";
            for (const auto& fila : filas) {
                html += "<tr>";
                int pos = 0;
                while (pos <= static_cast<int>(fila.length())) {
                    int coma = fila.indexOf(',', pos);
                    String celda = (coma == -1) ? fila.substring(pos) : fila.substring(pos, coma);
                    html += "<td>" + celda + "</td>";
                    if (coma == -1) break;
                    pos = coma + 1;
                }
                html += "</tr>";
            }
            html += "</table></div>";
        }
    }

    html += "<a class='btn' href='/descargar'>📥 Descargar CSV (MUNDONUEVO.csv)</a>"
            "<a class='btn' href='/logs'>📋 Descargar Logs (logs.txt)</a>"
            "<a class='btn btn-danger' href='/borrar' onclick=\"return confirm('¿Seguro de purgar el CSV preservando la cabecera?');\">🗑️ Borrar CSV</a>"
            "<a class='btn' style='background:#388bfd;' href='/apagar'>💤 Apagar Estación (Deep Sleep)</a>"
            "<p>También puede mantener presionado el botón físico (&ge; 1 s) para salir.</p>"
            "</div></body></html>";

    _servidor.send(200, "text/html", html);
}

void ServiceMode::manejarDescargarCSV() {
    if (_storage == nullptr) {
        _servidor.send(500, "text/plain", "Error: StorageManager no vinculado.");
        return;
    }

    File archivo = _storage->abrirLecturaCSV();
    if (!archivo) {
        _servidor.send(404, "text/plain", "Error: MUNDONUEVO.csv no encontrado.");
        return;
    }

    // Streaming directo por bloques desde SD/Flash para evitar saturación de RAM (OOM)
    _servidor.sendHeader("Content-Disposition", "attachment; filename=MUNDONUEVO.csv");
    _servidor.streamFile(archivo, "text/csv");
    archivo.close();

    if (_timeMgr != nullptr) {
        _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "CSV descargado");
    }
}

void ServiceMode::manejarDescargarLogs() {
    if (_storage == nullptr) {
        _servidor.send(500, "text/plain", "Error: StorageManager no vinculado.");
        return;
    }

    File archivo = _storage->abrirLecturaLogs();
    if (!archivo) {
        _servidor.send(404, "text/plain", "No hay registros disponibles en logs.txt.");
        return;
    }

    _servidor.sendHeader("Content-Disposition", "attachment; filename=logs.txt");
    _servidor.streamFile(archivo, "text/plain");
    archivo.close();

    if (_timeMgr != nullptr) {
        _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Logs descargados");
    }
}

void ServiceMode::manejarBorrarCSV() {
    if (_storage != nullptr && _storage->borrarCSV()) {
        if (_timeMgr != nullptr) {
            _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "CSV borrado");
        }
        _servidor.send(200, "text/html",
                       "<html><body style='background:#0d1117;color:white;text-align:center;padding:50px;font-family:Arial;'>"
                       "<h2>CSV eliminado (cabecera preservada)</h2><a style='color:#58a6ff;' href='/'>Volver</a></body></html>");
    } else {
        _servidor.send(500, "text/plain", "Fallo al purgar archivo CSV.");
    }
}

void ServiceMode::manejarSalir() {
    if (_storage != nullptr && _timeMgr != nullptr) {
        _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Apagado solicitado desde web");
    }
    _servidor.send(200, "text/html",
                   "<html><body style='background:#0d1117;color:white;text-align:center;padding:50px;font-family:Arial;'>"
                   "<h2>La estación entrará en Deep Sleep</h2></body></html>");
    // Breve espera no bloqueante para permitir que la respuesta HTTP salga por el socket
    uint32_t t0 = millis();
    while (millis() - t0 < 150) {
        _servidor.handleClient();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    _servidorActivo = false;
}

void ServiceMode::ejecutarServidor() {
    // Desacoplar ISR temporalmente para que las pulsaciones de control no queden latentes
    detachInterrupt(digitalPinToInterrupt(_pinBoton));
    portENTER_CRITICAL(&_muxBoton);
    _solicitudServicio = false;
    portEXIT_CRITICAL(&_muxBoton);

    if (_storage != nullptr && _timeMgr != nullptr) {
        _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Entro en modo servicio");
    }

    // Configuración del Access Point WiFi (192.168.4.1 / red abierta igual que legacy)
    WiFi.mode(WIFI_AP);
    IPAddress ipAP(192, 168, 4, 1);
    IPAddress netmask(255, 255, 255, 0);
    WiFi.softAPConfig(ipAP, ipAP, netmask);
    if (strlen(SigatokaConfig::AP_PASS_SERVICIO) == 0) {
        WiFi.softAP(SigatokaConfig::AP_SSID_SERVICIO);
    } else {
        WiFi.softAP(SigatokaConfig::AP_SSID_SERVICIO, SigatokaConfig::AP_PASS_SERVICIO);
    }

    configurarRutasWeb();
    _servidor.begin();
    _servidorActivo = true;

    uint32_t inicioServicio = millis();

    // Esperar liberación inicial del botón + antirrebote de entrada
    while (digitalRead(_pinBoton) == LOW) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelay(pdMS_TO_TICKS(200));

    while (_servidorActivo) {
        _servidor.handleClient();

        // 1. Timeout por tiempo máximo de servicio (10 minutos)
        if ((millis() - inicioServicio) >= SigatokaConfig::SERVICIO_TIMEOUT_MS) {
            if (_storage != nullptr && _timeMgr != nullptr) {
                _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Salida modo servicio por timeout");
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
                    _storage->escribirLog(_timeMgr->obtenerCadenaCompleta(), "Salio de modo servicio por boton");
                }
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // Cede tiempo al watchdog y FreeRTOS
    }

    _servidor.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    // Esperar liberación total y estabilización del pin antes de rearmar EXT0/ISR
    while (digitalRead(_pinBoton) == LOW) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    portENTER_CRITICAL(&_muxBoton);
    _solicitudServicio = false;
    portEXIT_CRITICAL(&_muxBoton);
    attachInterrupt(digitalPinToInterrupt(_pinBoton), ServiceMode::manejarInterrupcionBoton, FALLING);
}
