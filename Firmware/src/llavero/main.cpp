
// LLAVERO — WeMOS D1 R2 (ESP8266) + CC1101 + pulsador entre RST y GND.
//
// Ciclo de vida: el pulsador resetea la placa -> setup() corre en cada pulsado ->
// transmite -> escucha ordenes del auto -> vuelve a dormir hasta el proximo pulsado.
// loop() es una maquina de estados: ENVIAR -> ESCUCHAR -> (CONFIRMAR) -> DORMIR.

#include <Arduino.h>
#include <ESP8266WiFi.h>
// Registro del generador de numeros aleatorios por hardware del ESP8266.
// Arduino.h normalmente ya lo define; esto cubre el caso en que no.
#ifndef RANDOM_REG32
#define RANDOM_REG32 (*(volatile uint32_t*)0x3FF20E44)
#endif

#include "memoria_llavero.h"
#include "../lib/crypto-aes/src/crypto_aes.h"
#include "../lib/radio_cc1101/src/radio_cc1101.h"
#include "../lib/rf/src/protocol_frames.h"

// Pines hacia el CC1101 (numeros GPIO). GDO0 no se usa: la capa radio consulta MARCSTATE.
// CUIDADO: GPIO15 (CSN) debe estar en LOW al arrancar; no hay que usar pull-up externo.
const radio::Pins PINS_RF = { 14 /*SCK*/, 12 /*MISO*/, 13 /*MOSI*/, 15 /*CSN*/, -1 /*GDO0*/ };

const uint8_t ID_LLAVERO[rf::ID_SIZE] = {0xAB, 0xCD, 0xEF, 0x01};   // igual a ID_ESPERADO del auto

// Margen que se le da al auto para validar la trama y empezar a contestar.
// La ventana real de escucha = este margen + lo que tarda la respuesta en el aire.
constexpr uint16_t T_REACCION_AUTO_MS = 50;

enum class Estado : uint8_t { ENVIAR, ESCUCHAR, CONFIRMAR, DORMIR };

Estado   estado       = Estado::ENVIAR;
bool     radio_ok     = false;
uint32_t contador     = 0;     // contador de esta pulsacion
uint8_t  orden        = 0;     // CMD_SET_* recibido del auto
uint32_t t_escucha_ms = 0;
uint32_t ventana_ms   = 0;

// Arma [ID | contador | comando | padding aleatorio] y lo cifra con AES-128.
void armar_trama(uint8_t comando, uint8_t cifrada[rf::FRAME_SIZE]) {
    rf::Frame f;
    memcpy(f.id, ID_LLAVERO, rf::ID_SIZE);
    f.counter = contador;
    f.command = comando;
    for (uint8_t i = 0; i < rf::PADDING_SIZE; i++) f.padding[i] = (uint8_t)RANDOM_REG32; // RNG por hardware

    uint8_t plana[rf::FRAME_SIZE];
    rf::frame_serialize(f, plana);
    CryptoAES::encriptar(plana, cifrada);
}

// true si la trama es una orden de cambio de modulacion legitima para ESTA pulsacion.
bool es_orden_valida(const uint8_t cifrada[rf::FRAME_SIZE], uint8_t& comando) {
    uint8_t plana[rf::FRAME_SIZE];
    CryptoAES::desencriptar(cifrada, plana);

    rf::Frame f;
    rf::frame_parse(plana, f);

    if (!rf::frame_id_equals(f, ID_LLAVERO)) return false;
    if (f.counter != contador) return false;   // debe responder a la trama que acabo de mandar
    if (f.command != rf::CMD_SET_FSK2 && f.command != rf::CMD_SET_OOK) return false;

    comando = f.command;
    return true;
}

void setup() {
    Serial.begin(115200);

    memoria::iniciar();
    contador = memoria::siguiente_contador();

    radio_ok = radio::begin(PINS_RF, radio::profile_for(memoria::modulacion()));
    if (!radio_ok) {
        Serial.println("[LLAVERO] CC1101 no detectado");
        estado = Estado::DORMIR;
    }
}

void loop() {
    switch (estado) {

    case Estado::ENVIAR: {
        uint8_t trama[rf::FRAME_SIZE];
        armar_trama(rf::CMD_OPEN, trama);

        const bool ok = radio::send_blocking(trama);
        Serial.printf("[LLAVERO] TX contador=%lu %s\n", (unsigned long)contador, ok ? "OK" : "FALLO");

        if (ok) {
            ventana_ms = radio::airtime_ms(radio::current_profile()) + T_REACCION_AUTO_MS;
            radio::rx_start();
            t_escucha_ms = millis();
            estado = Estado::ESCUCHAR;
        } else {
            estado = Estado::DORMIR;
        }
        break;
    }

    case Estado::ESCUCHAR: {
        uint8_t buf[rf::FRAME_SIZE];
        const radio::RxResult r = radio::rx_poll(buf);

        if (r == radio::RxResult::FRAME && es_orden_valida(buf, orden)) {
            estado = Estado::CONFIRMAR;      // el chip queda en IDLE, listo para transmitir
            break;
        }
        if (r != radio::RxResult::NONE) radio::rx_start();   // trama ajena / CRC malo: seguir escuchando

        if (millis() - t_escucha_ms >= ventana_ms) estado = Estado::DORMIR;
        break;
    }

    case Estado::CONFIRMAR: {
        // ACK en la modulacion ACTUAL (el auto todavia escucha en ella); recien despues se cambia.
        uint8_t ack[rf::FRAME_SIZE];
        armar_trama(orden | rf::CMD_ACK_FLAG, ack);

        if (radio::send_blocking(ack)) {
            const radio::Modulation nueva = (orden == rf::CMD_SET_OOK) ? radio::Modulation::ASK_OOK
                                                                       : radio::Modulation::FSK2;
            memoria::guardar_modulacion(nueva);
            Serial.printf("[LLAVERO] ACK enviado, nueva modulacion=%u\n", (unsigned)nueva);
        } else {
            Serial.println("[LLAVERO] ACK fallo: se mantiene la modulacion");
        }
        estado = Estado::DORMIR;
        break;
    }

    case Estado::DORMIR:
        Serial.flush();
        if (radio_ok) radio::sleep();                  // CC1101 a SLEEP
        ESP.deepSleep(0, WAKE_RF_DISABLED);            // 0 = hasta el proximo pulso en RST
        delay(100);                                    // deepSleep es asincrono
        break;
    }
}
