#ifndef PROTOCOL_FRAMES_H
#define PROTOCOL_FRAMES_H

// Trama del protocolo Rolling Code y resultados de validacion.
// Sin dependencias de Arduino: compila igual en ESP8266, ESP32, AVR y en la PC (tests nativos).

#include <stdint.h>

namespace rf {

    constexpr uint8_t FRAME_SIZE   = 16;   // exactamente un bloque AES-128
    constexpr uint8_t ID_SIZE      = 4;
    constexpr uint8_t PADDING_SIZE = 7;

    // Valores del campo "command".
    constexpr uint8_t CMD_OPEN = 0x01; // llavero --> auto: pulsador presionado.
    constexpr uint8_t CMD_SET_FSK2 = 0x10; // auto --> llavero: cambiar a 2-fsk.
    constexpr uint8_t CMD_SET_OOK = 0x11; // auto --> llavero: cambiar a ASK/OOK

    constexpr uint8_t CMD_ACK_FLAG = 0x80; // llavero --> auto: ACK = comando recibido con este bit en 1.

    // Las tramas auto -> llavero y los ACK llevan en "counter" el contador de la trama CMD_OPEN
    // a la que responden: asi una orden grabada de un pulsado anterior no sirve (anti-replay).

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
        ACCEPTED,                // contador esperado o hasta 6 unidades por delante
        REPLAY,                  // contador <= ultimo aceptado
        RESYNC_PHYSICAL,         // fuera de la ventana: requiere emparejamiento manual
        ID_INVALID,              // la trama pertenece a otro llavero
        CRYPTO_ERROR,            // fallo al descifrar / trama mal formada
        INVALID_COMMAND          // comando no permitido para el receptor
    };

    // Texto estable (lo usa el protocolo serie de test_receptor.py y los logs).
    const char* frame_status_name(FrameStatus s);
    // true si la trama debe abrir la puerta y persistir el nuevo contador.
    bool frame_status_accepted(FrameStatus s);

    }

#endif
