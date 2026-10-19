#ifndef PROTOCOL_FRAMES_H
#define PROTOCOL_FRAMES_H

// Trama del protocolo Rolling Code y resultados de validacion.
// Sin dependencias de Arduino: compila igual en ESP8266, ESP32, AVR y en la PC (tests nativos).

#include <stdint.h>

namespace rf {

    constexpr uint8_t FRAME_SIZE   = 16;   // exactamente un bloque AES-128
    constexpr uint8_t ID_SIZE      = 4;
    constexpr uint8_t PADDING_SIZE = 7;

    // Valores del campo "command". Reservados para despues: ACK y cambio de modulacion.
    constexpr uint8_t CMD_OPEN = 0x01;

    // Layout de la trama ANTES de cifrar (16 bytes):
    //   [0..3]  id       4 bytes crudos (igual que ID_ESPERADO: AB CD EF 01)
    //   [4..7]  counter  uint32 little-endian
    //   [8]     command
    //   [9..15] padding  7 bytes (los rellena el emisor con aleatorios)
    struct Frame {
        uint8_t  id[ID_SIZE];
        uint32_t counter;
        uint8_t  command;
        uint8_t  padding[PADDING_SIZE];
    };

    void     frame_serialize(const Frame& f, uint8_t out[FRAME_SIZE]);
    void     frame_parse(const uint8_t in[FRAME_SIZE], Frame& f);
    bool     frame_id_equals(const Frame& f, const uint8_t expected[ID_SIZE]);
    uint32_t frame_id_as_u32(const Frame& f);   // big-endian: AB CD EF 01 -> 0xABCDEF01 (para mostrar/loguear)

    // Resultado de procesar una trama recibida (unifica WindowResult y EstadoValidacion).
    enum class FrameStatus : uint8_t {
        ACCEPTED,                // contador en +1..+6
        ACCEPTED_AFTER_RESYNC,   // segunda trama consecutiva de una resincronizacion logica
        REPLAY,                  // contador <= ultimo aceptado
        RESYNC_LOGICAL,          // salto +7..+50: queda pendiente, no abre
        RESYNC_PHYSICAL,         // salto > +50: requiere emparejamiento manual
        ID_INVALID,              // la trama pertenece a otro llavero
        CRYPTO_ERROR             // fallo al descifrar / trama mal formada
    };

    // Texto estable (lo usa el protocolo serie de test_receptor.py y los logs).
    const char* frame_status_name(FrameStatus s);
    // true si la trama debe abrir la puerta y persistir el nuevo contador.
    bool frame_status_accepted(FrameStatus s);

    }  

#endif 
