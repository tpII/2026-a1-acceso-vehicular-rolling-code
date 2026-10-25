#ifndef CONFIG_CONTROL_H
#define CONFIG_CONTROL_H

// Pulsadores y selector de modulacion del nodo interceptor (poncho para Arduino UNO).
//
// Pines:
//   D2 -> pulsador CAPTURA   (INT0: ademas de pulsador normal, despierta a la placa del sleep)
//   D4 -> pulsador REPLAY    (retransmite la ultima trama capturada)
//   D6 -> pulsador JAMMING   (mantenido = jamming activo)
//   D5 -> selector de modulacion (pata central a GND, la otra sin conectar; con pull-up
//         interno: LOW = 2-FSK, HIGH = ASK/OOK)
//   D7 / D8 -> LEDs, reservados, sin montar todavia (ver main.cpp)
//
// Los 3 pulsadores van a GND con INPUT_PULLUP: en reposo leen HIGH, presionados LOW.

#include <stdint.h>
#include "../../lib/radio_cc1101/src/cc1101_config.h"   // radio::Modulation

namespace perifericos {

    // Publico porque main.cpp lo necesita para armar la interrupcion de despertar (INT0).
    constexpr uint8_t PIN_BTN_CAPTURA = 2;

    enum class Edge : uint8_t { NINGUNO, PRESIONADO, SOLTADO };

    // Configura los pinMode de los 3 pulsadores y del selector. Llamar una sola vez en setup().
    void iniciar();

    // Un flanco por vuelta de loop(), ya filtrado el rebote (ver config_perifericos.cpp).
    Edge leer_captura();
    Edge leer_replay();
    Edge leer_jamming();

    // Lectura cruda del selector, sin debounce (para leerla una sola vez al despertar / al
    // configurar el CC1101, donde ya no hace falta filtrar ruido porque se usa una sola vez).
    radio::Modulation modulacion_seleccionada();

    // true una sola vez por cambio de posicion ESTABLE del selector (mismo debounce que los
    // pulsadores, por si el pin tiene ruido). "nueva" solo es valida cuando devuelve true.
    // Util para detectar que el usuario movio el switch mientras el nodo ya estaba despierto.
    bool cambio_modulacion(radio::Modulation& nueva);

    const char* nombre_modulacion(radio::Modulation m);
}

#endif
