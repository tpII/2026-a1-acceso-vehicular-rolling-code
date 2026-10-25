#include <Arduino.h>
#include "config_control.h"

namespace perifericos {
    namespace {
        // PIN_BTN_CAPTURA esta declarado en el .h (publico: main.cpp lo necesita para la interrupcion).
        constexpr uint8_t PIN_BTN_REPLAY    = 4;
        constexpr uint8_t PIN_BTN_JAMMING   = 6;
        constexpr uint8_t PIN_SW_MODULACION = 5;

        constexpr uint32_t DEBOUNCE_MS = 40;

        // Debounce por flanco para un pulsador activo en LOW. Un solo metodo que se llama una
        // vez por vuelta de loop() y devuelve el flanco detectado (o NINGUNO): asi, si dos
        // lugares del codigo preguntan por el mismo boton, no se "roban" el flanco entre si.
        struct Boton {
            uint8_t pin = 0;
            bool estado_anterior = HIGH;
            uint32_t t_cambio = 0;

            void iniciar(uint8_t p) {
                pin = p;
                pinMode(pin, INPUT_PULLUP);
                estado_anterior = digitalRead(pin);
            }

            Edge leer() {
                const bool actual = digitalRead(pin);
                if (actual == estado_anterior || millis() - t_cambio < DEBOUNCE_MS) return Edge::NINGUNO;

                t_cambio = millis();
                const Edge e = (estado_anterior == HIGH && actual == LOW) ? Edge::PRESIONADO : Edge::SOLTADO;
                estado_anterior = actual;
                return e;
            }
        };

        constexpr uint32_t MANTENIDO_DORMIR_MS = 3000; // mantener CAPTURA este tiempo -> dormir

        Boton g_captura, g_replay, g_jamming;
        radio::Modulation g_mod_estable;
        radio::Modulation g_mod_candidata;
        uint32_t g_t_cambio_mod = 0;

        bool     g_captura_mantenido = false;   // true mientras el pin sigue en LOW
        uint32_t g_t_presion_captura = 0;
        bool     g_captura_disparo_dormir = false; // evita disparar DORMIR mas de una vez por pulsacion
    }

    void iniciar() {
        g_captura.iniciar(PIN_BTN_CAPTURA);
        g_replay.iniciar(PIN_BTN_REPLAY);
        g_jamming.iniciar(PIN_BTN_JAMMING);

        // Pata central a GND, la otra sin conectar: sin pull-up esa posicion queda flotando.
        pinMode(PIN_SW_MODULACION, INPUT_PULLUP);
        g_mod_estable = g_mod_candidata = modulacion_seleccionada();
    }

    Edge leer_replay()  { return g_replay.leer(); }
    Edge leer_jamming() { return g_jamming.leer(); }

    AccionCaptura evaluar_captura() {
        const Edge e = g_captura.leer();

        if (e == Edge::PRESIONADO) {
            g_captura_mantenido = true;
            g_t_presion_captura = millis();
            g_captura_disparo_dormir = false;
        } else if (e == Edge::SOLTADO) {
            g_captura_mantenido = false;
            // Si ya disparo DORMIR mientras estaba mantenido, soltarlo despues no hace nada mas.
            if (!g_captura_disparo_dormir) return AccionCaptura::DESCARTAR_Y_RECAPTURAR;
            return AccionCaptura::NINGUNA;
        }

        // Sin esperar a que lo suelten: apenas se cumplen los 3 s mantenido, duerme.
        if (g_captura_mantenido && !g_captura_disparo_dormir &&
            millis() - g_t_presion_captura >= MANTENIDO_DORMIR_MS) {
            g_captura_disparo_dormir = true;
            return AccionCaptura::DORMIR;
        }

        return AccionCaptura::NINGUNA;
    }

    radio::Modulation modulacion_seleccionada() {
        return (digitalRead(PIN_SW_MODULACION) == LOW) ? radio::Modulation::FSK2 : radio::Modulation::ASK_OOK;
    }

    bool cambio_modulacion(radio::Modulation& nueva) {
        const radio::Modulation leida = modulacion_seleccionada();
        if (leida != g_mod_candidata) {
            g_mod_candidata = leida;
            g_t_cambio_mod = millis();
            return false;
        }
        if (leida == g_mod_estable || millis() - g_t_cambio_mod < DEBOUNCE_MS) return false;

        g_mod_estable = leida;
        nueva = leida;
        return true;
    }

    const char* nombre_modulacion(radio::Modulation m) {
        return (m == radio::Modulation::ASK_OOK) ? "ASK/OOK" : "2-FSK";
    }
}
