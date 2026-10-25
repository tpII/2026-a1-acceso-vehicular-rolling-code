#include "counter_window.h"

namespace rf {

    FrameStatus CounterWindow::check(uint32_t received) {
        // No permitimos wraparound: una vez agotado el contador de 32 bits,
        // se requiere un procedimiento explícito de emparejamiento/reprovisión.
        if (last_ == UINT32_MAX || received <= last_) {
            return FrameStatus::REPLAY;
        }

        const uint32_t expected_counter = last_ + 1;
        const uint32_t ahead = received - expected_counter;

        if (ahead <= WINDOW_ACCEPT) {
            last_ = received;
            return FrameStatus::ACCEPTED;
        }

        // Fuera de la ventana de aceptación: requiere resincronización física.
        // No se guarda el contador recibido ni se abre la puerta.
        return FrameStatus::RESYNC_PHYSICAL;
    }

    void CounterWindow::reset(uint32_t last) {
        last_ = last;
    }

}
