/**
 * @file StorageManager.cpp
 * @brief Implementación del gestor de almacenamiento híbrido MicroSD / LittleFS.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "StorageManager.h"

static const char* CABECERA_CSV = "Fecha,Hora,Lluvia_mm_min,Velocidad_ms,Direccion,Temperatura_C,Humedad_pct,Humectacion_Foliar_pct,Duracion_Humectacion_min\n";

StorageManager::StorageManager()
    : _spiSD(FSPI), _sdInicializada(false), _littleFsInicializado(false) {}

bool StorageManager::begin() {
    // 1. Inicializar LittleFS en memoria Flash interna (primer nivel de persistencia y buffer)
    if (LittleFS.begin(true)) {
        _littleFsInicializado = true;
    } else {
        log_e("Falla al montar LittleFS interno");
    }

    // 2. Inicializar bus SPI para la MicroSD física
    _spiSD.begin(SigatokaConfig::PIN_SPI_SCK,
                 SigatokaConfig::PIN_SPI_MISO,
                 SigatokaConfig::PIN_SPI_MOSI,
                 SigatokaConfig::PIN_SPI_CS_SD);

    if (SD.begin(SigatokaConfig::PIN_SPI_CS_SD, _spiSD)) {
        _sdInicializada = true;
        verificarCabeceraCSV();
        volcarBufferFlashASD();
    } else {
        log_w("MicroSD no detectada o con error de montaje. Operando con buffer LittleFS.");
    }

    return (_sdInicializada || _littleFsInicializado);
}

void StorageManager::verificarCabeceraCSV() {
    if (!_sdInicializada) return;

    if (!SD.exists(SigatokaConfig::RUTA_CSV_SD)) {
        File f = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_WRITE);
        if (f) {
            f.print(CABECERA_CSV);
            f.close();
        }
    }
}

bool StorageManager::guardarRegistro(const RegistroCompletoEstacion& registro) {
    String linea = registro.aFormatoCSV();

    // 1. Intentar guardar en la MicroSD
    if (_sdInicializada) {
        File f = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_APPEND);
        if (f) {
            f.println(linea);
            f.close();
            return true;
        }
        log_w("Fallo escribiendo en MicroSD. Redirigiendo a Flash LittleFS...");
    }

    // 2. Respaldo automático en LittleFS si la SD falló o fue extraída
    if (_littleFsInicializado) {
        File fFlash = LittleFS.open(SigatokaConfig::RUTA_BUFFER_FLASH, FILE_APPEND);
        if (fFlash) {
            fFlash.println(linea);
            fFlash.close();
            return true;
        }
    }

    return false;
}

bool StorageManager::escribirLog(const String& marcaTiempo, const String& mensaje) {
    String entrada = marcaTiempo + " - " + mensaje;

    if (_sdInicializada) {
        File f = SD.open(SigatokaConfig::RUTA_LOG_SD, FILE_APPEND);
        if (f) {
            f.println(entrada);
            f.close();
            return true;
        }
    }

    if (_littleFsInicializado) {
        File f = LittleFS.open("/logs_flash.txt", FILE_APPEND);
        if (f) {
            f.println(entrada);
            f.close();
            return true;
        }
    }

    return false;
}

String StorageManager::leerCSV() {
    if (_sdInicializada && SD.exists(SigatokaConfig::RUTA_CSV_SD)) {
        File f = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_READ);
        if (f) {
            String contenido = f.readString();
            f.close();
            return contenido;
        }
    }
    return "Error: No se pudo leer el archivo CSV de la MicroSD.";
}

String StorageManager::leerLogs() {
    if (_sdInicializada && SD.exists(SigatokaConfig::RUTA_LOG_SD)) {
        File f = SD.open(SigatokaConfig::RUTA_LOG_SD, FILE_READ);
        if (f) {
            String contenido = f.readString();
            f.close();
            return contenido;
        }
    }
    return "No hay registros disponibles en logs.txt.";
}

bool StorageManager::borrarCSV() {
    if (_sdInicializada) {
        if (SD.exists(SigatokaConfig::RUTA_CSV_SD)) {
            SD.remove(SigatokaConfig::RUTA_CSV_SD);
        }
        File f = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_WRITE);
        if (f) {
            f.print(CABECERA_CSV);
            f.close();
            return true;
        }
    }
    return false;
}

void StorageManager::volcarBufferFlashASD() {
    if (!_sdInicializada || !_littleFsInicializado) return;

    if (LittleFS.exists(SigatokaConfig::RUTA_BUFFER_FLASH)) {
        File fFlash = LittleFS.open(SigatokaConfig::RUTA_BUFFER_FLASH, FILE_READ);
        if (fFlash) {
            File fSD = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_APPEND);
            if (fSD) {
                while (fFlash.available()) {
                    fSD.write(fFlash.read());
                }
                fSD.close();
                fFlash.close();
                LittleFS.remove(SigatokaConfig::RUTA_BUFFER_FLASH);
                log_i("Buffer LittleFS volcado exitosamente a la MicroSD.");
                return;
            }
            fFlash.close();
        }
    }
}

void StorageManager::desmontarSD() {
    if (_sdInicializada) {
        SD.end();
        _sdInicializada = false;
    }
}

bool StorageManager::sdDisponible() const {
    return _sdInicializada;
}

bool StorageManager::littleFsDisponible() const {
    return _littleFsInicializado;
}
