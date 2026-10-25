#include <Arduino.h>
#include <EEPROM.h>
#include "memoria_llavero.h"

namespace memoria {

    namespace {
        constexpr uint16_t MAGIC = 0xA1C0;   // distingue flash virgen de datos validos

        struct Datos {
            uint16_t magic;
            uint8_t  modulacion;   // valor numerico de radio::Modulation
            uint8_t  reservado;
            uint32_t contador;
        };

        Datos g_datos;

        void grabar() {
            EEPROM.put(0, g_datos);
            EEPROM.commit();
        }
    }

    void iniciar() {
        EEPROM.begin(sizeof(Datos));
        EEPROM.get(0, g_datos);

        const bool mod_valida = g_datos.modulacion == (uint8_t)radio::Modulation::FSK2 ||
                                g_datos.modulacion == (uint8_t)radio::Modulation::ASK_OOK;
        if (g_datos.magic != MAGIC || !mod_valida) {
            g_datos = Datos{MAGIC, (uint8_t) radio::Modulation::FSK2, 0, 0};
            grabar();
        }
    }

    uint32_t siguiente_contador() {
        g_datos.contador++;
        grabar();
        return g_datos.contador;
    }

    radio::Modulation modulacion() {
        return (radio::Modulation)g_datos.modulacion;
    }

    void guardar_modulacion(radio::Modulation m) {
        g_datos.modulacion = (uint8_t)m;
        grabar();
    }
}
