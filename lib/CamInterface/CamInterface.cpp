/**
 * @file CamInterface.cpp
 * @brief Implementación de la comunicación socket TCP con el nodo ESP32-CAM.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "CamInterface.h"
#include <WiFiClient.h>

CamInterface::CamInterface(gpio_num_t pinDisparo) : _pinDisparo(pinDisparo) {}

void CamInterface::begin() {
    pinMode(_pinDisparo, OUTPUT);
    digitalWrite(_pinDisparo, LOW);
}

void CamInterface::despertarCamara(uint32_t duracionPulsoMs) {
    // Generar pulso en flanco de subida para activar wake_on_ext0 en el nodo cámara
    digitalWrite(_pinDisparo, HIGH);
    vTaskDelay(pdMS_TO_TICKS(duracionPulsoMs));
    digitalWrite(_pinDisparo, LOW);
}

bool CamInterface::conectarCamAP(const char* ssid, uint32_t timeoutSegundos) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid);

    uint32_t inicio = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - inicio) >= (timeoutSegundos * 1000)) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return true;
}

bool CamInterface::descargarFoto(uint8_t** bufferSalida, size_t* longitudSalida, 
                                 const char* ipServidor, uint16_t puerto) {
    if (WiFi.status() != WL_CONNECTED || bufferSalida == nullptr || longitudSalida == nullptr) {
        return false;
    }

    WiFiClient client;
    if (!client.connect(ipServidor, puerto)) {
        log_e("No fue posible conectar al socket del ESP32-CAM en %s:%d", ipServidor, puerto);
        return false;
    }

    // 1. Enviar solicitud de captura fotográfica
    client.print("Tomar foto");

    // 2. Esperar confirmación de estado (8 bytes: "correcto" o "error000")
    uint32_t inicioTimeout = millis();
    while (client.available() < 8) {
        if ((millis() - inicioTimeout) > 8000) {
            client.stop();
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    char estado[9] = {0};
    client.readBytes(estado, 8);

    if (strncmp(estado, "correcto", 8) != 0) {
        log_e("Nodo cámara retornó error: %s", estado);
        client.stop();
        return false;
    }

    // 3. Leer longitud de la imagen JPEG (4 bytes en formato uint32_t little-endian)
    inicioTimeout = millis();
    while (client.available() < 4) {
        if ((millis() - inicioTimeout) > 3000) {
            client.stop();
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    uint32_t tamanoFoto = 0;
    client.readBytes(reinterpret_cast<char*>(&tamanoFoto), sizeof(uint32_t));

    if (tamanoFoto == 0 || tamanoFoto > (1024 * 1024)) { // Límite de seguridad: 1 MB
        client.stop();
        return false;
    }

    // 4. Reservar buffer en memoria RAM para la imagen JPEG
    uint8_t* fotoBuffer = reinterpret_cast<uint8_t*>(malloc(tamanoFoto));
    if (fotoBuffer == nullptr) {
        log_e("Memoria RAM insuficiente para alojar la imagen de %u bytes", tamanoFoto);
        client.stop();
        return false;
    }

    size_t bytesRecibidos = 0;
    inicioTimeout = millis();

    while (bytesRecibidos < tamanoFoto && client.connected()) {
        size_t disponibles = client.available();
        if (disponibles > 0) {
            size_t aLeer = (tamanoFoto - bytesRecibidos < disponibles) ? (tamanoFoto - bytesRecibidos) : disponibles;
            size_t leidos = client.readBytes(reinterpret_cast<char*>(fotoBuffer + bytesRecibidos), aLeer);
            bytesRecibidos += leidos;
            inicioTimeout = millis();
        } else {
            if ((millis() - inicioTimeout) > 5000) {
                free(fotoBuffer);
                client.stop();
                return false;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    client.stop();

    if (bytesRecibidos == tamanoFoto) {
        *bufferSalida = fotoBuffer;
        *longitudSalida = tamanoFoto;
        return true;
    }

    free(fotoBuffer);
    return false;
}

void CamInterface::desconectarCamAP() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}
