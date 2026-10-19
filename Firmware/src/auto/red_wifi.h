#ifndef RED_WIFI_H
#define RED_WIFI_H  

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// ======================================
// CONFIGURACIÓN DE LA RED WIFI (SoftAP)
// ======================================
extern const char* const WIFI_SSID; // Nombre de la red WiFi
extern const char* const WIFI_PASSWORD; // Contraseña de la red WiFi

namespace RedWiFi{
    extern AsyncWebServer server; // Servidor web asíncrono para manejar las solicitudes HTTP

    void iniciarServidor(); // Función para iniciar el servidor web
    void procesarWebSocket(); // Función para procesar las solicitudes del WebSocket

    void notificarFrontendWebSocket(String jsonPayload); // Comunicacion directa ESP32 <-> Navegador Web (Frontend)
}

#endif