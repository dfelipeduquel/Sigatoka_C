/**
 * @file StorageManager.cpp
 * @brief Implementación del gestor de almacenamiento híbrido MicroSD / LittleFS.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "StorageManager.h"

// Selección segura del periférico SPI según arquitectura objetivo:
// En ESP32 clásico, FSPI (1) pertenece a la Flash interna; VSPI (3) es el bus libre.
// En ESP32-S3, FSPI (0) es el bus SPI2 de propósito general.
#if defined(CONFIG_IDF_TARGET_ESP32S3)
    #define SIGATOK_SD_SPI_HOST FSPI
#else
    #define SIGATOK_SD_SPI_HOST VSPI
#endif

static const char* CABECERA_CSV =
    "Date,Time,Precipitation,Wind Velocity,Wind Direction,Temperature,Humidity"
#if SIGATOK_ENABLE_LEAF_WETNESS
    ",LeafWetness_pct,WetnessDuration_min"
#endif
#if SIGATOK_ENABLE_CAM
    ",Image Name"
#endif
    "\n";

StorageManager::StorageManager()
    : _spiSD(SIGATOK_SD_SPI_HOST), _sdInicializada(false), _littleFsInicializado(false) {}

const char* StorageManager::obtenerCabeceraCSV() {
    return CABECERA_CSV;
}

bool StorageManager::begin() {
    // 1. Inicializar LittleFS en memoria Flash interna (persistencia y buffer de respaldo)
    if (LittleFS.begin(true)) {
        _littleFsInicializado = true;
    } else {
        log_e("Falla al montar LittleFS interno");
    }

    // 2. Inicializar bus SPI físico para MicroSD (pines ancla: SCK=21, MISO=18, MOSI=19, CS=17)
    _spiSD.begin(SigatokaConfig::PIN_SPI_SCK,
                 SigatokaConfig::PIN_SPI_MISO,
                 SigatokaConfig::PIN_SPI_MOSI,
                 SigatokaConfig::PIN_SPI_CS_SD);

    if (SD.begin(SigatokaConfig::PIN_SPI_CS_SD, _spiSD)) {
        _sdInicializada = true;
        verificarCabeceraCSV();
        volcarBufferFlashASD();
        volcarLogsFlashASD();
    } else {
        log_w("MicroSD no detectada o con error de montaje. Operando con buffer LittleFS.");
    }

    return (_sdInicializada || _littleFsInicializado);
}

void StorageManager::verificarCabeceraCSV() {
    if (!_sdInicializada) return;

    bool crearCabecera = !SD.exists(SigatokaConfig::RUTA_CSV_SD);
    if (!crearCabecera) {
        File existente = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_READ);
        if (existente) {
            crearCabecera = (existente.size() == 0);
            existente.close();
        }
    }

    if (crearCabecera) {
        File f = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_WRITE);
        if (f) {
            f.print(CABECERA_CSV);
            f.close();
        }
    }
}

bool StorageManager::guardarRegistro(const RegistroCompletoEstacion& registro) {
    String linea = registro.aFormatoCSV();

    // 1. Intentar guardar directamente en MicroSD
    if (_sdInicializada) {
        File f = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_APPEND);
        if (f) {
            size_t escritos = f.println(linea);
            f.close();
            if (escritos > 0) {
                return true;
            }
        }
        log_w("Fallo escribiendo en MicroSD. Redirigiendo a Flash LittleFS...");
    }

    // 2. Respaldo automático en LittleFS si la SD falló o fue extraída
    if (_littleFsInicializado) {
        File fFlash = LittleFS.open(SigatokaConfig::RUTA_BUFFER_FLASH, FILE_APPEND);
        if (fFlash) {
            size_t escritos = fFlash.println(linea);
            fFlash.close();
            return (escritos > 0);
        }
    }

    return false;
}

bool StorageManager::escribirLog(const String& marcaTiempo, const String& mensaje) {
    String entrada = marcaTiempo + " - " + mensaje;

    if (_sdInicializada) {
        File f = SD.open(SigatokaConfig::RUTA_LOG_SD, FILE_APPEND);
        if (f) {
            size_t escritos = f.println(entrada);
            f.close();
            if (escritos > 0) {
                return true;
            }
        }
    }

    if (_littleFsInicializado) {
        File f = LittleFS.open(SigatokaConfig::RUTA_LOG_FLASH, FILE_APPEND);
        if (f) {
            size_t escritos = f.println(entrada);
            f.close();
            return (escritos > 0);
        }
    }

    return false;
}

File StorageManager::abrirLecturaCSV() {
    if (_sdInicializada && SD.exists(SigatokaConfig::RUTA_CSV_SD)) {
        return SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_READ);
    }
    if (_littleFsInicializado && LittleFS.exists(SigatokaConfig::RUTA_BUFFER_FLASH)) {
        return LittleFS.open(SigatokaConfig::RUTA_BUFFER_FLASH, FILE_READ);
    }
    return File();
}

File StorageManager::abrirLecturaLogs() {
    if (_sdInicializada && SD.exists(SigatokaConfig::RUTA_LOG_SD)) {
        return SD.open(SigatokaConfig::RUTA_LOG_SD, FILE_READ);
    }
    if (_littleFsInicializado && LittleFS.exists(SigatokaConfig::RUTA_LOG_FLASH)) {
        return LittleFS.open(SigatokaConfig::RUTA_LOG_FLASH, FILE_READ);
    }
    return File();
}

bool StorageManager::leerUltimosRegistros(size_t maxRegistros, String& cabeceraOut, std::vector<String>& filasOut) {
    filasOut.clear();
    cabeceraOut = String(CABECERA_CSV);
    cabeceraOut.trim();

    File f = abrirLecturaCSV();
    if (!f) {
        return false;
    }

    if (f.available()) {
        String primera = f.readStringUntil('\n');
        primera.trim();
        if (primera.length() > 0) {
            cabeceraOut = primera;
        }
    }

    while (f.available()) {
        String linea = f.readStringUntil('\n');
        linea.trim();
        if (linea.length() == 0) continue;

        if (filasOut.size() >= maxRegistros) {
            filasOut.erase(filasOut.begin());
        }
        filasOut.push_back(linea);
    }

    f.close();
    return true;
}

bool StorageManager::borrarCSV() {
    bool ok = false;
    if (_sdInicializada) {
        // Preservar el encabezado tal como hace Header_ModoServicio.py
        String encabezado = String(CABECERA_CSV);
        if (SD.exists(SigatokaConfig::RUTA_CSV_SD)) {
            File fLectura = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_READ);
            if (fLectura && fLectura.available()) {
                String primera = fLectura.readStringUntil('\n');
                primera.trim();
                if (primera.length() > 0) {
                    encabezado = primera + "\n";
                }
            }
            if (fLectura) fLectura.close();
            SD.remove(SigatokaConfig::RUTA_CSV_SD);
        }

        File fEscritura = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_WRITE);
        if (fEscritura) {
            fEscritura.print(encabezado);
            fEscritura.close();
            ok = true;
        }
    }

    if (_littleFsInicializado && LittleFS.exists(SigatokaConfig::RUTA_BUFFER_FLASH)) {
        LittleFS.remove(SigatokaConfig::RUTA_BUFFER_FLASH);
        ok = true;
    }

    return ok;
}

void StorageManager::copiarArchivoEnBloques(File& origen, File& destino) {
    uint8_t buffer[512];
    while (origen.available()) {
        size_t leidos = origen.read(buffer, sizeof(buffer));
        if (leidos > 0) {
            destino.write(buffer, leidos);
        } else {
            break;
        }
    }
}

void StorageManager::volcarBufferFlashASD() {
    if (!_sdInicializada || !_littleFsInicializado) return;
    if (!LittleFS.exists(SigatokaConfig::RUTA_BUFFER_FLASH)) return;

    File fFlash = LittleFS.open(SigatokaConfig::RUTA_BUFFER_FLASH, FILE_READ);
    if (!fFlash) return;

    File fSD = SD.open(SigatokaConfig::RUTA_CSV_SD, FILE_APPEND);
    if (fSD) {
        copiarArchivoEnBloques(fFlash, fSD);
        fSD.close();
        fFlash.close();
        LittleFS.remove(SigatokaConfig::RUTA_BUFFER_FLASH);
        log_i("Buffer LittleFS volcado exitosamente a la MicroSD.");
    } else {
        fFlash.close();
    }
}

void StorageManager::volcarLogsFlashASD() {
    if (!_sdInicializada || !_littleFsInicializado) return;
    if (!LittleFS.exists(SigatokaConfig::RUTA_LOG_FLASH)) return;

    File fFlash = LittleFS.open(SigatokaConfig::RUTA_LOG_FLASH, FILE_READ);
    if (!fFlash) return;

    File fSD = SD.open(SigatokaConfig::RUTA_LOG_SD, FILE_APPEND);
    if (fSD) {
        copiarArchivoEnBloques(fFlash, fSD);
        fSD.close();
        fFlash.close();
        LittleFS.remove(SigatokaConfig::RUTA_LOG_FLASH);
        log_i("Logs de LittleFS volcados exitosamente a la MicroSD.");
    } else {
        fFlash.close();
    }
}

void StorageManager::desmontarSD() {
    if (_sdInicializada) {
        SD.end();
        _spiSD.end();
        _sdInicializada = false;
    }
}

bool StorageManager::sdDisponible() const {
    return _sdInicializada;
}

bool StorageManager::littleFsDisponible() const {
    return _littleFsInicializado;
}
