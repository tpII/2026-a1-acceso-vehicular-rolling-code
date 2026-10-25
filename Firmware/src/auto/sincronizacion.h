#ifndef SINCRONIZACION_H
#define SINCRONIZACION_H

// Cambio de modulacion coordinado con el llavero (lado receptor / auto).
//
// Flujo: la web pide un cambio -> queda PENDIENTE -> cuando llega una trama CMD_OPEN valida
// el auto contesta con CMD_SET_* (el llavero solo escucha ~200 ms despues de transmitir) ->
// el auto espera el ACK en la modulacion ACTUAL -> recien con el ACK cambia la suya.
// Si el ACK no llega, el auto no cambia y el pedido sigue pendiente (se reintenta en la
// proxima pulsacion).

#include <stdint.h>
#include "../lib/radio_cc1101/src/cc1101_config.h"
#include "../lib/rf/src/protocol_frames.h"

namespace Sincronizacion {

    enum class Evento : uint8_t { NINGUNO, SIN_ACK };

    void solicitar(radio::Modulation m);   // desde la web
    void cancelar();                       // la web volvio a pedir la modulacion actual
    bool hay_pendiente();
    radio::Modulation pendiente();
    bool esperando_ack();

    // Llamar apenas se acepta una trama CMD_OPEN, ANTES de NVS/servo. Si hay un cambio
    // pendiente, transmite la orden y pasa a esperar el ACK.
    void enviar_orden(const uint8_t id[rf::ID_SIZE], uint32_t contador);

    // Trama descifrada con el flag ACK. true si confirma la orden en curso; "nueva" es la
    // modulacion que hay que aplicar ahora.
    bool confirmar_ack(const rf::Frame& f, const uint8_t id[rf::ID_SIZE], radio::Modulation& nueva);

    // Llamar en cada loop(): vence la espera del ACK.
    Evento tick();
}

#endif
