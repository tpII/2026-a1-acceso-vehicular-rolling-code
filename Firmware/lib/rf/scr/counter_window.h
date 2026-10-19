#ifndef COUNTER_WINDOW_H
#define COUNTER_WINDOW_H

#include <stdint.h>
#include "protocol_frames.h"

namespace rf {

    constexpr uint32_t WINDOW_ACCEPT = 6;    // +1..+6   -> acepta
    constexpr uint32_t WINDOW_RESYNC = 50;   // +7..+50  -> resincronizacion logica (2 tramas consecutivas)
                                            // > +50    -> resincronizacion fisica (emparejamiento manual)

    // Ventana de aceptacion del contador (lado receptor). Logica pura, con estado.
    //
    // Reglas:
    //  - received <= last                 -> REPLAY (no cambia nada, ni el pendiente)
    //  - 1 <= received-last <= 6          -> ACCEPTED, last = received, se descarta el pendiente
    //  - 7 <= received-last <= 50         -> RESYNC_LOGICAL, la trama queda pendiente (no abre)
    //  - received == pendiente + 1        -> ACCEPTED_AFTER_RESYNC, last = received
    //  - received-last > 50               -> RESYNC_PHYSICAL (no cambia nada)
    //

    class CounterWindow {
    public:
        explicit CounterWindow(uint32_t last = 0)
            : last_(last), pending_(0), has_pending_(false) {}

        FrameStatus check(uint32_t received);

        void reset(uint32_t last);   // restaurar desde NVS o emparejamiento fisico

        uint32_t last() const        { return last_; }
        bool     has_pending() const { return has_pending_; }
        uint32_t pending() const     { return pending_; }

    private:
        uint32_t last_;
        uint32_t pending_;
        bool     has_pending_;
    };

} 
#endif