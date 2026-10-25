#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <Arduino.h>
#include "estados_seguridad.h"
#include "sincronizacion.h"
#include "red_wifi.h" // Para acceder a RedWiFi::server y notificarFrontendWebSocket

extern uint32_t ultimo_contador; // Variable global para almacenar el último contador recibido
extern String modulacion_actual; // Variable global para almacenar la modulación actual (ASK o FSK)
extern uint32_t ataques_rechazados; // Variable global para almacenar la cantidad de ataques rechazados hoy
extern bool sistema_seguro; // Variable global para indicar si el sistema está en modo seguro (true) o no (false)
extern String mensaje_estado; // Variable global para almacenar el mensaje de estado actual del sistema
extern bool resincronizacion_fisica_requerida;
extern uint32_t contador_esperado_resync;
extern uint32_t contador_recibido_resync;

namespace ServidorWeb{
    void inicializarRutas(); // Función para inicializar las rutas (/api/estado, /api/logs, etc.)
    void registrarEvento(String  tramaHex, uint32_t id, uint32_t contador, int rssi, String modulacion, EstadoValidacion estado); // Función para registrar un evento de seguridad
}

#endif  