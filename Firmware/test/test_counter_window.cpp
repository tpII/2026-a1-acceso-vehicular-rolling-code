// Pruebas nativas de la política Rolling Code, sin Arduino, RF ni hardware.
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../lib/rf/src/counter_window.h"

int main() {
    // Contador esperado 24: el último contador aceptado fue 23.
    rf::CounterWindow w(23);
    assert(w.expected() == 24);
    assert(w.check(28) == rf::FrameStatus::ACCEPTED); // +4 respecto del esperado
    assert(w.last() == 28 && w.expected() == 29);

    // Repetido/antiguo: rechazo sin modificar estado.
    assert(w.check(28) == rf::FrameStatus::REPLAY);
    assert(w.check(27) == rf::FrameStatus::REPLAY);
    assert(w.last() == 28);

    // Límite superior: esperado 29, recibido 35 es +6 y se acepta.
    assert(w.check(35) == rf::FrameStatus::ACCEPTED);
    assert(w.last() == 35 && w.expected() == 36);

    // Fuera por una unidad: esperado 36, recibido 43 es +7; física, sin cambio.
    assert(w.check(43) == rf::FrameStatus::RESYNC_PHYSICAL);
    assert(w.last() == 35 && w.expected() == 36);

    // Ejemplo pedido: esperado 26, recibido 34 es +8; física, sin cambio.
    rf::CounterWindow w2(25);
    assert(w2.expected() == 26);
    assert(w2.check(34) == rf::FrameStatus::RESYNC_PHYSICAL);
    assert(w2.last() == 25 && w2.expected() == 26);

    // Una trama fuera de ventana no deja pendiente; la siguiente dentro de
    // la ventana normal se acepta respecto del último contador válido.
    assert(w2.check(30) == rf::FrameStatus::ACCEPTED); // esperado 26, +4
    assert(w2.last() == 30 && w2.expected() == 31);

    // No hay wraparound implícito al agotar el contador de 32 bits.
    rf::CounterWindow edge(UINT32_MAX - 1);
    assert(edge.check(UINT32_MAX) == rf::FrameStatus::ACCEPTED);
    assert(edge.check(0) == rf::FrameStatus::REPLAY);

    std::cout << "CounterWindow: OK\n";
    return 0;
}

