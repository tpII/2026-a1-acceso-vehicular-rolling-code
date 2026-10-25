#ifndef COUNTER_WINDOW_H
#define COUNTER_WINDOW_H

#include <stdint.h>
#include "protocol_frames.h"

namespace rf {

    // Cantidad máxima de unidades que el contador recibido puede adelantar
    // respecto del próximo contador esperado.
    constexpr uint32_t WINDOW_ACCEPT = 6;

    // Política del receptor:
    //  - received <= last: REPLAY (no modifica el estado).
    //  - expected = last + 1.
    //  - expected <= received <= expected + 6: ACCEPTED; last = received.
    //  - received > expected + 6: RESYNC_PHYSICAL; no modifica el estado.
    // La resincronización física debe autorizarse mediante el procedimiento
    // de emparejamiento manual; una trama RF por sí sola nunca adelanta last.
    class CounterWindow {
    public:
        explicit CounterWindow(uint32_t last = 0) : last_(last) {}

        FrameStatus check(uint32_t received);
        void reset(uint32_t last);  // restaurar último contador aceptado desde NVS/test

        uint32_t last() const { return last_; }
        uint32_t expected() const { return last_ == UINT32_MAX ? UINT32_MAX : last_ + 1; }

    private:
        uint32_t last_;
    };

}
#endif
