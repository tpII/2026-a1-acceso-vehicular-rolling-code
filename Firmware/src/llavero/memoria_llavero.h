#ifndef MEMORIA_LLAVERO_H
#define MEMORIA_LLAVERO_H

// Datos del llavero que deben sobrevivir al deep sleep y a cortes de energia.
// El ESP8266 no tiene NVS/Preferences: se usa EEPROM (emulada sobre un sector de flash).

#include <stdint.h>
#include "../lib/radio_cc1101/src/cc1101_config.h"

namespace memoria {

    void iniciar();

    // Incrementa el contador, lo graba en flash y lo devuelve.
    // Se graba ANTES de transmitir: si la trama se pierde, el contador nunca se reutiliza
    // (el receptor tolera saltos dentro de su ventana).
    uint32_t siguiente_contador();

    radio::Modulation modulacion();
    void guardar_modulacion(radio::Modulation m);
}

#endif
