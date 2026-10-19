#include "servidor.h"
#include "red_wifi.h" // Para acceder a RedWiFi::server y notificarFrontendWebSocket

namespace ServidorWeb {

    // ==========================================
    // BUFFER CIRCULAR DE LOGS (Memoria RAM)
    // ==========================================
    const int MAX_LOGS = 15;
    EventoSeguridad historialLogs[MAX_LOGS];
    int indice_log = 0;
    int total_logs = 0;

    void registrarEvento(String tramaHex, uint32_t id, uint32_t contador, int rssi, String modulacion, EstadoValidacion resultado) {
        historialLogs[indice_log] = {tramaHex, id, contador, rssi, modulacion, resultado};
        indice_log = (indice_log + 1) % MAX_LOGS;
        if (total_logs < MAX_LOGS) total_logs++;
    }

    // ==========================================
    // CONFIGURACIÓN DE ENDPOINTS (API REST)
    // ==========================================
    void inicializarRutas() {
        // 1. Endpoint: Estado Global
        RedWiFi::server.on("/api/estado", HTTP_GET, [](AsyncWebServerRequest *request){
            String json = "{";
            json += "\"sistema_seguro\":" + String(sistema_seguro ? "true" : "false") + ",";
            json += "\"texto_estado\":\"" + String(sistema_seguro ? "Sistema seguro" : "Alerta de seguridad") + "\",";
            json += "\"mensaje\":\"" + mensaje_estado + "\",";
            json += "\"contador_actual\":" + String(ultimo_contador) + ",";
            json += "\"ataques_rechazados_hoy\":" + String(ataques_rechazados) + ",";
            json += "\"modulacion_actual\":\"" + modulacion_actual + "\"";
            json += "}";
            request->send(200, "application/json", json);
        });

        // 2. Endpoint: Cambiar Modulación
        RedWiFi::server.on("/api/modulacion", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                String body = "";
                for(size_t i=0; i<len; i++) body += (char)data[i];

                String moduloViejo = modulacion_actual;
                
                if(body.indexOf("2-FSK") > 0) {
                    modulacion_actual = "2-FSK";
                    registrarEvento("CAMBIO_MODULACION", 0, 0, 0, modulacion_actual, ACCESO_PERMITIDO); // Registrar cambio de modulación
                    request->send(200, "application/json", "{\"ok\":true, \"modulacion_actual\":\"2-FSK\"}");
                } else if(body.indexOf("ASK/OOK") > 0) {
                    modulacion_actual = "ASK/OOK";
                    registrarEvento("CAMBIO_MODULACION", 0, 0, 0, modulacion_actual, ACCESO_PERMITIDO); // Registrar cambio de modulación
                    request->send(200, "application/json", "{\"ok\":true, \"modulacion_actual\":\"ASK/OOK\"}");
                } else {
                    request->send(400, "application/json", "{\"ok\":false, \"error\":\"Modulación inválida\"}");
                }
                
                // Forzar recarga visual en todos los navegadores conectados
                RedWiFi::notificarFrontendWebSocket("{\"recargar\": true}");
        });

        // 3. Endpoint: Historial de Logs
        RedWiFi::server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request){
            String json = "[";
            int conteo = 0;
            
            for (int i = 0; i < total_logs; i++) {
                int idx = (indice_log - 1 - i + MAX_LOGS) % MAX_LOGS;
                EventoSeguridad ev = historialLogs[idx];
                
                String strResultado = (ev.estado == ACCESO_PERMITIDO) ? "aceptado" : "rechazado";
                String tipoAtaque = (ev.estado == RECHAZO_REPLAY_ATTACK) ? "Replay Attack" : 
                                    (ev.estado == RECHAZO_DESINCRONIZACION) ? "Desincronización Severa" : "Ninguno";

                json += "{";
                json += "\"fecha_hora\":\"Uptime " + String(millis() / 1000) + "s\",";
                json += "\"id_llavero\":\"0x" + String(ev.idllavero, HEX) + "\",";
                json += "\"contador\":" + String(ev.contador) + ",";
                json += "\"modulacion\":\"" + ev.modulacion + "\",";
                json += "\"trama\":\"" + ev.tramaHex + "\",";
                json += "\"tipo_ataque\":\"" + tipoAtaque + "\",";
                json += "\"resultado\":\"" + strResultado + "\"";
                json += "}";
                
                conteo++;
                if (conteo < total_logs) json += ",";
            }
            json += "]";
            request->send(200, "application/json", json);
        });

        // 4. Endpoints: Tramas recientes
        RedWiFi::server.on("/api/tramas/2fsk", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "application/json", "[]"); 
        });
        RedWiFi::server.on("/api/tramas/ook", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "application/json", "[]"); 
        });
    }
}