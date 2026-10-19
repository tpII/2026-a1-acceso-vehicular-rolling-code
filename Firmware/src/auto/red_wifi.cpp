#include "red_wifi.h"
#include <WiFi.h>
#include <SPIFFS.h>

const char* const WIFI_SSID = "Auto_Acceso_Seguro";
const char* const WIFI_PASSWORD = "password123";

namespace RedWiFi {
    AsyncWebServer server(80); // Inicializa el servidor web en el puerto 80
    AsyncWebSocket ws("/ws"); // Inicializa el WebSocket en la ruta "/ws"

    // ======================================
    // MANEJADOR DE EVENTOS DEL WEBSOCKET
    // ======================================
    void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
        if(type == WS_EVT_CONNECT){
            Serial.printf("[WiFi] Cliente WebSocket conectado. IP: %s\n", client->remoteIP().toString().c_str());
        } else if(type == WS_EVT_DISCONNECT){
            Serial.printf("[WiFi] Cliente WebSocket desconectado.");
        }
    }

    void procesarWebSocket(){
        ws.cleanupClients(); // Limpia los clientes desconectados
    }

    void notificarFrontendWebSocket(String jsonPayload) {
        ws.textAll(jsonPayload); // Envía el payload JSON a todos los clientes conectados
    }

    void iniciarServidor() {
        // Configura el servidor web para servir archivos desde SPIFFS
        if (!SPIFFS.begin(true)) {
            Serial.println("[Error] Error al montar SPIFFS");
            return;
        }
        if (!SPIFFS.exists("/index.html")) {
            Serial.println("[Error] No se encontró /index.html en SPIFFS. Cargue el filesystem con: pio run -e auto -t uploadfs");
        }

        // Configura la red WiFi en modo SoftAP
        WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
        Serial.println("[WiFi] Red WiFi iniciada. IP Servidor:");
        Serial.println(WiFi.softAPIP());

        ws.onEvent(onWsEvent); // Asocia el manejador de eventos al WebSocket
        server.addHandler(&ws); // Agrega el WebSocket al servidor web

        server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

        // Inicia el servidor web
        server.begin();
        Serial.println("[Servidor] Servidor web iniciado en el puerto 80");
    }

}