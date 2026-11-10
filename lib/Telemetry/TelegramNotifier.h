/**
 * @file TelegramNotifier.h
 * @brief Notificador de alertas agroclimáticas vía Telegram Bot API con fragmentación y bajo consumo.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#ifndef SIGATOK_TELEGRAM_NOTIFIER_H
#define SIGATOK_TELEGRAM_NOTIFIER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class TelegramNotifier {
public:
    TelegramNotifier(const char* botToken, int64_t chatId);

    /**
     * @brief Conecta el ESP32 a la red WiFi del campus/finca con timeout no bloqueante.
     * @return true si se obtuvo IP; false si expiró el tiempo de espera.
     */
    bool conectarWiFi(const char* ssid, const char* password, uint32_t timeoutSegundos = 12);

    /**
     * @brief Desconecta y apaga el radio WiFi para ahorro de energía en batería.
     */
    void desconectarWiFi();

    /**
     * @brief Envía un mensaje a Telegram fraccionado en bloques para evitar desbordamiento de heap.
     * @param mensaje Texto del reporte agronómico o alerta de esporas.
     * @param reintentos Cantidad de reintentos por fragmento.
     * @return true si todos los fragmentos fueron confirmados (HTTP 200).
     */
    bool enviarMensaje(const String& mensaje, uint8_t reintentos = 2);

private:
    const char* _botToken;
    int64_t     _chatId;
    WiFiClientSecure _clientSecure;

    bool enviarFragmento(const String& fragmento, uint8_t reintentos);
    static String urlEncode(const String& texto);
};

#endif // SIGATOK_TELEGRAM_NOTIFIER_H
