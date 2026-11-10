/**
 * @file TelegramNotifier.cpp
 * @brief Implementación del cliente de telemetría segura Telegram.
 * @project SIGATOK - Estación Agrometeorológica y Monitoreo Foliar
 * @author Daniel Felipe Duque Lizarralde
 */

#include "TelegramNotifier.h"

TelegramNotifier::TelegramNotifier(const char* botToken, int64_t chatId)
    : _botToken(botToken), _chatId(chatId) {
    _clientSecure.setInsecure(); // Optimización de RAM: evita cargar cadena pesada de certificados SSL en flash
}

bool TelegramNotifier::conectarWiFi(const char* ssid, const char* password, uint32_t timeoutSegundos) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    uint32_t inicio = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((millis() - inicio) >= (timeoutSegundos * 1000)) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    return true;
}

void TelegramNotifier::desconectarWiFi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

String TelegramNotifier::urlEncode(const String& texto) {
    String codificado = "";
    char c;
    for (size_t i = 0; i < texto.length(); i++) {
        c = texto.charAt(i);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            codificado += c;
        } else if (c == ' ') {
            codificado += "%20";
        } else if (c == '\n') {
            codificado += "%0A";
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (uint8_t)c);
            codificado += hex;
        }
    }
    return codificado;
}

bool TelegramNotifier::enviarFragmento(const String& fragmento, uint8_t reintentos) {
    HTTPClient http;
    String url = "https://api.telegram.org/bot" + String(_botToken) + "/sendMessage";
    String postBody = "chat_id=" + String(_chatId) + "&text=" + urlEncode(fragmento);

    for (uint8_t i = 0; i < reintentos; i++) {
        if (http.begin(_clientSecure, url)) {
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            http.setTimeout(10000);

            int codigoHTTP = http.POST(postBody);
            http.end();

            if (codigoHTTP == HTTP_CODE_OK) {
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(300 + i * 200));
    }
    return false;
}

bool TelegramNotifier::enviarMensaje(const String& mensaje, uint8_t reintentos) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    constexpr size_t LONGITUD_FRAGMENTO = 360;
    size_t longitudTotal = mensaje.length();
    size_t offset = 0;

    while (offset < longitudTotal) {
        size_t len = (longitudTotal - offset > LONGITUD_FRAGMENTO) ? LONGITUD_FRAGMENTO : (longitudTotal - offset);
        String fragmento = mensaje.substring(offset, offset + len);

        if (!enviarFragmento(fragmento, reintentos)) {
            return false;
        }
        offset += len;
    }

    return true;
}
