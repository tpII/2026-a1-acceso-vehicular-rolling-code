#include "servidor.h"

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
            json += "\"resincronizacion_fisica_requerida\":" + String(resincronizacion_fisica_requerida ? "true" : "false") + ",";
            json += "\"contador_esperado_resync\":" + String(contador_esperado_resync) + ",";
            json += "\"contador_recibido_resync\":" + String(contador_recibido_resync) + ",";
            json += "\"contador_actual\":" + String(ultimo_contador) + ",";
            json += "\"ataques_rechazados_hoy\":" + String(ataques_rechazados) + ",";
            json += "\"modulacion_actual\":\"" + modulacion_actual + "\",";
            json += "\"modulacion_pendiente\":";
            json += Sincronizacion::hay_pendiente()
                    ? "\"" + String(Sincronizacion::pendiente() == radio::Modulation::ASK_OOK ? "ASK/OOK" : "2-FSK") + "\""
                    : String("null");
            json += "}";
            request->send(200, "application/json", json);
        });

        // 2. Endpoint: Cambiar Modulación
        RedWiFi::server.on("/api/modulacion", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
            [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
                String body = "";
                for(size_t i=0; i<len; i++) body += (char)data[i];

                // El cambio NO se aplica acá: queda pendiente hasta que el llavero lo confirme por RF.
                String deseada = "";
                radio::Modulation pedida = radio::Modulation::FSK2;
                if (body.indexOf("2-FSK") > 0)         { deseada = "2-FSK";   pedida = radio::Modulation::FSK2; }
                else if (body.indexOf("ASK/OOK") > 0)  { deseada = "ASK/OOK"; pedida = radio::Modulation::ASK_OOK; }

                if (deseada == "") {
                    request->send(400, "application/json", "{\"ok\":false, \"error\":\"Modulación inválida\"}");
                } else if (deseada == modulacion_actual) {
                    Sincronizacion::cancelar();   // volvió a pedir la vigente: se descarta lo pendiente
                    request->send(200, "application/json", "{\"ok\":true, \"pendiente\":false, \"modulacion_actual\":\"" + modulacion_actual + "\"}");
                } else {
                    Sincronizacion::solicitar(pedida);
                    registrarEvento("CAMBIO_SOLICITADO", 0, 0, 0, modulacion_actual, ACCESO_PERMITIDO);
                    request->send(202, "application/json", "{\"ok\":true, \"pendiente\":true, \"modulacion_actual\":\"" + modulacion_actual + "\", \"modulacion_deseada\":\"" + deseada + "\"}");
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
                                    (ev.estado == RECHAZO_DESINCRONIZACION) ? "Resincronización física requerida" :
                                    (ev.estado == ALERTA_RESINCRONIZACION) ? "Alerta de sincronización" : "Ninguno";

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