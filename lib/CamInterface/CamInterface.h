/**
 * @file CamInterface.h
 * @brief Interfaz de disparo y adquisición de imágenes foliares desde el nodo ESP32-CAM.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_CAM_INTERFACE_H
#define SIGATOK_CAM_INTERFACE_H

#include <Arduino.h>
#include <WiFi.h>
#include "sigatok_config.h"

class CamInterface {
public:
    explicit CamInterface(gpio_num_t pinDisparo = SigatokaConfig::PIN_CONTROL_CAM);

    /**
     * @brief Configura el pin GPIO de disparo/despertar del nodo cámara.
     */
    void begin();

    /**
     * @brief Envía un pulso en nivel alto para despertar el ESP32-CAM mediante EXT0.
     */
    void despertarCamara(uint32_t duracionPulsoMs = 200);

    /**
     * @brief Conecta al Access Point generado por el ESP32-CAM ("RedESP2").
     */
    bool conectarCamAP(const char* ssid = "RedESP2", uint32_t timeoutSegundos = 15);

    /**
     * @brief Solicita y descarga la imagen JPEG vía socket TCP (puerto 8080).
     * @param bufferSalida Vector dinámico o puntero donde almacenar los bytes JPEG.
     * @param longitudSalida Puntero para retornar el tamaño del archivo descargado.
     * @return true si la imagen fue recibida con éxito y sin corrupción.
     */
    bool descargarFoto(uint8_t** bufferSalida, size_t* longitudSalida, const char* ipServidor = "192.168.4.1", uint16_t puerto = 8080);

    /**
     * @brief Desconecta el enlace WiFi tras la recepción fotográfica.
     */
    void desconectarCamAP();

private:
    gpio_num_t _pinDisparo;
};

#endif // SIGATOK_CAM_INTERFACE_H
