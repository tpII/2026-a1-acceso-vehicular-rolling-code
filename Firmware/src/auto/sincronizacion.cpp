#include <Arduino.h>
#include <string.h>
#include "sincronizacion.h"
#include "../lib/crypto-aes/src/crypto_aes.h"
#include "../lib/radio_cc1101/src/radio_cc1101.h"

namespace Sincronizacion {

    namespace {
        constexpr uint16_t T_GUARDA_MS = 5;            // el llavero necesita unos ms para pasar de TX a RX
        constexpr uint16_t T_REACCION_LLAVERO_MS = 50; // margen para que descifre y empiece a contestar

        bool              g_hay_pendiente = false;
        radio::Modulation g_pendiente = radio::Modulation::FSK2;

        bool     g_esperando = false;
        uint8_t  g_orden = 0;            // CMD_SET_* enviado
        uint32_t g_contador_orden = 0;   // contador de la trama CMD_OPEN a la que se responde
        uint32_t g_t_inicio_ms = 0;
        uint32_t g_ventana_ms = 0;
    }

    void solicitar(radio::Modulation m) { g_pendiente = m; g_hay_pendiente = true; }
    void cancelar()                     { g_hay_pendiente = false; }
    bool hay_pendiente()                { return g_hay_pendiente; }
    radio::Modulation pendiente()       { return g_pendiente; }
    bool esperando_ack()                { return g_esperando; }

    void enviar_orden(const uint8_t id[rf::ID_SIZE], uint32_t contador) {
        if (!g_hay_pendiente) return;

        rf::Frame f;
        memcpy(f.id, id, rf::ID_SIZE);
        f.counter = contador;   // eco del contador del llavero: sirve de anti-replay en la bajada
        f.command = (g_pendiente == radio::Modulation::ASK_OOK) ? rf::CMD_SET_OOK : rf::CMD_SET_FSK2;
        for (uint8_t i = 0; i < rf::PADDING_SIZE; i++) f.padding[i] = (uint8_t)esp_random();

        uint8_t plana[rf::FRAME_SIZE], cifrada[rf::FRAME_SIZE];
        rf::frame_serialize(f, plana);
        CryptoAES::encriptar(plana, cifrada);

        delay(T_GUARDA_MS);
        if (!radio::send_blocking(cifrada)) return;   // main re-arma la escucha

        g_orden = f.command;
        g_contador_orden = contador;
        g_ventana_ms = radio::airtime_ms(radio::current_profile()) + T_REACCION_LLAVERO_MS;
        radio::rx_start();                             // escucha continua (no WOR) para el ACK
        g_t_inicio_ms = millis();
        g_esperando = true;
    }

    bool confirmar_ack(const rf::Frame& f, const uint8_t id[rf::ID_SIZE], radio::Modulation& nueva) {
        if (!g_esperando) return false;
        if (!rf::frame_id_equals(f, id)) return false;
        if (f.counter != g_contador_orden) return false;
        if (f.command != (uint8_t)(g_orden | rf::CMD_ACK_FLAG)) return false;

        nueva = (g_orden == rf::CMD_SET_OOK) ? radio::Modulation::ASK_OOK : radio::Modulation::FSK2;
        g_esperando = false;
        // Si mientras tanto la web pidio otra cosa, el pedido sigue pendiente.
        g_hay_pendiente = (g_pendiente != nueva);
        return true;
    }

    Evento tick() {
        if (g_esperando && millis() - g_t_inicio_ms >= g_ventana_ms) {
            g_esperando = false;
            return Evento::SIN_ACK;
        }
        return Evento::NINGUNO;
    }
}
