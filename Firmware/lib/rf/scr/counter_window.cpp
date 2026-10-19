#include "counter_window.h"

namespace rf {

    FrameStatus CounterWindow::check(uint32_t received) {
        if (received <= last_) {
            return FrameStatus::REPLAY;
        }

        // Segunda trama consecutiva de una resincronizacion logica en curso.
        if (has_pending_ && received == pending_ + 1) {
            last_ = received;
            has_pending_ = false;
            return FrameStatus::ACCEPTED_AFTER_RESYNC;
        }

        const uint32_t delta = received - last_;   // > 0 garantizado

        if (delta <= WINDOW_ACCEPT) {
            last_ = received;
            has_pending_ = false;
            return FrameStatus::ACCEPTED;
        }
        if (delta <= WINDOW_RESYNC) {
            pending_ = received;
            has_pending_ = true;
            return FrameStatus::RESYNC_LOGICAL;
        }
        return FrameStatus::RESYNC_PHYSICAL;
    }

    void CounterWindow::reset(uint32_t last) {
        last_ = last;
        has_pending_ = false;
        pending_ = 0;
    }
}