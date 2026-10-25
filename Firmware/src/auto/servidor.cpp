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
        historialLogs[indice_log] = {tramaHex, id, contador, rssi, modulacion, resultado, millis() / 1000};
        indice_log = (indice_log + 1) % MAX_LOGS;
        if (total_logs < MAX_LOGS) total_logs++;
    }

    // El ESP32 no tiene RTC ni acceso a internet (es un Access Point aislado, sin uplink para
    // NTP), asi que no puede saber la fecha/hora real. Lo mas cercano sin agregar un modulo de
    // reloj aparte es mostrar hace cuanto paso cada evento, calculado con el uptime (millis())
    // que tenia el ESP32 en el momento en que se registro.
    static String formatearAntiguedad(uint32_t timestamp_s) {
        const uint32_t ahora_s = millis() / 1000;
        // Resta protegida: por si millis() ya dio la vuelta (desborda a los ~49 dias).
        const uint32_t antiguedad = (ahora_s >= timestamp_s) ? (ahora_s - timestamp_s) : 0;

        if (antiguedad < 60)   return "hace " + String(antiguedad) + "s";
        if (antiguedad < 3600) return "hace " + String(antiguedad / 60) + "m";
        return "hace " + String(antiguedad / 3600) + "h";
    }

    // Genera el JSON de tramas filtrando por modulación ("2-FSK" o "ASK/OOK")
    static String generarJsonTramasPorModulacion(const String& modFiltro) {
        String json = "[";
        bool primero = true;

        for (int i = 0; i < total_logs; i++) {
            int idx = (indice_log - 1 - i + MAX_LOGS) % MAX_LOGS;
            EventoSeguridad ev = historialLogs[idx];

            // Ignorar eventos internos que no son tramas RF de 32 caracteres hex
            if (ev.modulacion != modFiltro || ev.tramaHex.length() != 32) continue;

            if (!primero) json += ",";
            primero = false;

            String strResultado = (ev.estado == ACCESO_PERMITIDO) ? "aceptado" : "rechazado";
            json += "{";
            json += "\"hora\":\"" + formatearAntiguedad(ev.timestamp_s) + "\",";
            json += "\"trama\":\"" + ev.tramaHex + "\",";
            json += "\"rssi\":" + String(ev.rssi) + ",";
            json += "\"resultado\":\"" + strResultado + "\"";
            json += "}";
        }
        json += "]";
        return json;
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

                String deseada = "";
                radio::Modulation pedida = radio::Modulation::FSK2;
                if (body.indexOf("2-FSK") >= 0)        { deseada = "2-FSK";   pedida = radio::Modulation::FSK2; }
                else if (body.indexOf("ASK/OOK") >= 0) { deseada = "ASK/OOK"; pedida = radio::Modulation::ASK_OOK; }

                if (deseada == "") {
                    request->send(400, "application/json", "{\"ok\":false, \"error\":\"Modulación inválida\"}");
                } else if (deseada == modulacion_actual) {
                    Sincronizacion::cancelar();
                    request->send(200, "application/json", "{\"ok\":true, \"pendiente\":false, \"modulacion_actual\":\"" + modulacion_actual + "\", \"modulacion_deseada\":\"" + deseada + "\"}");
                } else {
                    Sincronizacion::solicitar(pedida);
                    registrarEvento("CAMBIO_SOLICITADO", 0, ultimo_contador, 0, deseada, ACCESO_PERMITIDO);
                    request->send(202, "application/json", "{\"ok\":true, \"pendiente\":true, \"modulacion_actual\":\"" + modulacion_actual + "\", \"modulacion_deseada\":\"" + deseada + "\"}");
                }
                
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
                json += "\"fecha_hora\":\"" + formatearAntiguedad(ev.timestamp_s) + "\",";
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

        // 4. Endpoints: Tramas recientes por modulación
        RedWiFi::server.on("/api/tramas/2fsk", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "application/json", generarJsonTramasPorModulacion("2-FSK")); 
        });
        RedWiFi::server.on("/api/tramas/ook", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "application/json", generarJsonTramasPorModulacion("ASK/OOK")); 
        });
    }
}