#include "protocol_frames.h"

namespace rf {

    void frame_serialize(const Frame& f, uint8_t out[FRAME_SIZE]) {
        for (uint8_t i = 0; i < ID_SIZE; i++) out[i] = f.id[i];
        out[4] = (uint8_t)(f.counter);
        out[5] = (uint8_t)(f.counter >> 8);
        out[6] = (uint8_t)(f.counter >> 16);
        out[7] = (uint8_t)(f.counter >> 24);
        out[8] = f.command;
        for (uint8_t i = 0; i < PADDING_SIZE; i++) out[9 + i] = f.padding[i];
    }

    void frame_parse(const uint8_t in[FRAME_SIZE], Frame& f) {
        for (uint8_t i = 0; i < ID_SIZE; i++) f.id[i] = in[i];
        f.counter = (uint32_t)in[4] | ((uint32_t)in[5] << 8) |
                    ((uint32_t)in[6] << 16) | ((uint32_t)in[7] << 24);
        f.command = in[8];
        for (uint8_t i = 0; i < PADDING_SIZE; i++) f.padding[i] = in[9 + i];
    }

    bool frame_id_equals(const Frame& f, const uint8_t expected[ID_SIZE]) {
        uint8_t diff = 0;
        for (uint8_t i = 0; i < ID_SIZE; i++) diff |= (uint8_t)(f.id[i] ^ expected[i]);
        return diff == 0;
    }

    uint32_t frame_id_as_u32(const Frame& f) {
        return ((uint32_t)f.id[0] << 24) | ((uint32_t)f.id[1] << 16) |
            ((uint32_t)f.id[2] << 8)  |  (uint32_t)f.id[3];
    }

    const char* frame_status_name(FrameStatus s) {
        switch (s) {
            case FrameStatus::ACCEPTED:              return "ACEPTADO";
            case FrameStatus::ACCEPTED_AFTER_RESYNC: return "ACEPTADO_TRAS_RESINCRONIZACION";
            case FrameStatus::REPLAY:                return "RECHAZADO_REPLAY_ATTACK";
            case FrameStatus::RESYNC_LOGICAL:        return "REQUIERE_RESINCRONIZACION_LOGICA";
            case FrameStatus::RESYNC_PHYSICAL:       return "REQUIERE_RESINCRONIZACION_FISICA";
            case FrameStatus::ID_INVALID:            return "RECHAZADO_ID_INVALIDO";
            case FrameStatus::CRYPTO_ERROR:          return "RECHAZADO_ERROR_CRIPTOGRAFICO";
        }
        return "DESCONOCIDO";
    }

    bool frame_status_accepted(FrameStatus s) {
        return s == FrameStatus::ACCEPTED || s == FrameStatus::ACCEPTED_AFTER_RESYNC;
    }

}  
