#include <Arduino.h>
#include <Preferences.h>
#include "red_wifi.h"
#include "servidor.h"
#include "estados_seguridad.h"
#include "servomotor.h"
#include "../lib/crypto-aes/src/crypto_aes.h"
#include "../lib/radio_cc1101/src/radio_cc1101.h"
#include "../lib/rf/src/protocol_frames.h"
#include "../lib/rf/src/counter_window.h"
#include <ctype.h>
#include <stdlib.h>


const radio::Pins PINS_RF = {
    .sck = 18,
    .miso = 19,
    .mosi = 23,
    .csn = 5,
    .gdo0 = 2
};

const uint8_t ID_ESPERADO[4] = {0xAB, 0xCD, 0xEF, 0x01}; // ID esperado del llavero (4 bytes)

/// true: escucha con Wake-On-Radio (bajo consumo, requisito del proyecto).
// false: recepcion continua, util para depurar el enlace sin el WOR de por medio.
// El WOR usa la interrupcion de GDO0 (PINS_RF.gdo0) y el llavero transmite con preambulo largo.
constexpr bool USAR_WOR = true;

Preferences memoriaNVS;
bool radioDisponible = false;

// Ventana real persistente (se inicializa con el contador recuperado de NVS).
rf::CounterWindow ventanaContador(0);
// Estado aislado para las pruebas por USB: nunca modifica NVS ni acciona el servo.
rf::CounterWindow ventanaPruebas(0);

// Variables globales de estado (leídas por servidor.cpp)
uint32_t ultimo_contador = 0;
String modulacion_actual = "2-FSK";
uint32_t ataques_rechazados = 0;
bool sistema_seguro = true;
String mensaje_estado = "El sistema se encuentra operando correctamente.";
bool resincronizacion_fisica_requerida = false;
uint32_t contador_esperado_resync = 0;
uint32_t contador_recibido_resync = 0;

void escuchar() {
    if (!radioDisponible) return;
    if (USAR_WOR) radio::rx_start_wor();
    else          radio::rx_start();
}

const char* nombreModulacion(radio::Modulation m) {
    return (m == radio::Modulation::ASK_OOK) ? "ASK/OOK" : "2-FSK";
}

// Se llama cuando el llavero confirmo (ACK) el cambio: recien ahora cambia el auto.
void aplicarModulacion(radio::Modulation nueva) {
    radio::apply_profile(radio::profile_for(nueva));
    modulacion_actual = nombreModulacion(nueva);
    memoriaNVS.putUChar("modulacion", (uint8_t)nueva);

    ServidorWeb::registrarEvento("CAMBIO_MODULACION", 0xABCDEF01, ultimo_contador, 0, modulacion_actual, ACCESO_PERMITIDO);
    RedWiFi::notificarFrontendWebSocket("{\"recargar\": true}");
    Serial.printf("[Sync] Modulación cambiada a %s\n", modulacion_actual.c_str());
}

EstadoValidacion estadoWebDesdeFrame(rf::FrameStatus status) {
    switch (status) {
        case rf::FrameStatus::ACCEPTED:
            return ACCESO_PERMITIDO;
        case rf::FrameStatus::REPLAY:
            return RECHAZO_REPLAY_ATTACK;
        case rf::FrameStatus::RESYNC_PHYSICAL:
            return RECHAZO_DESINCRONIZACION;
        case rf::FrameStatus::ID_INVALID:
            return ERROR_ID_INVALIDO;
        case rf::FrameStatus::INVALID_COMMAND:
            return ERROR_COMANDO_INVALIDO;
        case rf::FrameStatus::CRYPTO_ERROR:
        default:
            return ERRO_CRIPTOGRAFICO;
    }
}

void procesarValidacion(uint32_t contador_recibido, String tramaHexStr, int rssi_dbm, bool por_rf) {
    // Una sola implementación de la ventana para el camino RF y la persistencia real.
    const uint32_t esperado_antes_de_validar = ventanaContador.expected();
    const rf::FrameStatus estado_frame = ventanaContador.check(contador_recibido);
    const EstadoValidacion resultado_evaluacion = estadoWebDesdeFrame(estado_frame);
    const bool aceptado = rf::frame_status_accepted(estado_frame);

    // El llavero escucha brevemente después de transmitir; solo se responde a una trama aceptada.
    if (por_rf && aceptado) {
        Sincronizacion::enviar_orden(ID_ESPERADO, contador_recibido);
    }

    if (aceptado) {
        resincronizacion_fisica_requerida = false;
        ultimo_contador = ventanaContador.last();
        memoriaNVS.putUInt("ultimo_cnt", ultimo_contador);
        sistema_seguro = true;
        mensaje_estado = "Operando con normalidad.";
        servoSG90::abrirPuerta();
    } else {
        sistema_seguro = false;
        switch (estado_frame) {
            case rf::FrameStatus::REPLAY:
                mensaje_estado = "Se ha bloqueado un intento de Replay Attack.";
                ataques_rechazados++;
                break;
            case rf::FrameStatus::RESYNC_PHYSICAL:
                resincronizacion_fisica_requerida = true;
                contador_esperado_resync = esperado_antes_de_validar;
                contador_recibido_resync = contador_recibido;
                mensaje_estado = "Se requiere resincronización física. Esperado: " + String(contador_esperado_resync) +
                                 "; recibido: " + String(contador_recibido_resync) +
                                 ". Acceso bloqueado; el contador no fue actualizado.";
                Serial.printf("[SEGURIDAD] Resincronización física requerida: esperado=%lu recibido=%lu; contador sin cambios.\n",
                              (unsigned long)contador_esperado_resync, (unsigned long)contador_recibido_resync);
                break;
            default:
                mensaje_estado = "Trama rechazada por validación.";
                break;
        }
    }

    ServidorWeb::registrarEvento(tramaHexStr, 0xABCDEF01, contador_recibido, rssi_dbm, modulacion_actual, resultado_evaluacion);
    if (estado_frame == rf::FrameStatus::RESYNC_PHYSICAL) {
        RedWiFi::notificarFrontendWebSocket("{\"recargar\":true,\"alerta\":\"RESINCRONIZACION_FISICA\"}");
    } else {
        RedWiFi::notificarFrontendWebSocket("{\"recargar\":true}");
    }
    Serial.println("[Sistema] Evento RF procesado.");
}

// Trama de 16 bytes recibida por RF (CRC de hardware ya verificado).
void manejarTramaRF(const uint8_t* buffer_rx, int16_t rssi_dbm) {
    // Convertir la trama cruda a Hexadecimal para los logs
    String tramaHexStr = "";
    for (int i = 0; i < 16; i++) {
        char hex[3];
        sprintf(hex, "%02X", buffer_rx[i]);
        tramaHexStr += hex;
    }

    // Descifrado AES-128 en modo ECB
    uint8_t trama_descifrada[16];
    CryptoAES::desencriptar(buffer_rx, trama_descifrada);

    rf::Frame f;
    rf::frame_parse(trama_descifrada, f);

    // Validar ID antes de procesar nada más
    if (!rf::frame_id_equals(f, ID_ESPERADO)) {
        ServidorWeb::registrarEvento(tramaHexStr, 0x00000000, f.counter, rssi_dbm, modulacion_actual, ERROR_ID_INVALIDO);
        RedWiFi::notificarFrontendWebSocket("{\"recargar\": true}");
        return;
    }

    // ACK del llavero: no pasa por la ventana del contador (repite el contador de su OPEN).
    if (f.command & rf::CMD_ACK_FLAG) {
        radio::Modulation nueva;
        if (Sincronizacion::confirmar_ack(f, ID_ESPERADO, nueva)) aplicarModulacion(nueva);
        return;
    }

    if (f.command != rf::CMD_OPEN) {
        ServidorWeb::registrarEvento(tramaHexStr, rf::frame_id_as_u32(f), f.counter,
                                     rssi_dbm, modulacion_actual, ERROR_COMANDO_INVALIDO);
        Serial.printf("[RF] Comando no permitido 0x%02X ignorado\n", f.command);
        return;
    }

    procesarValidacion(f.counter, tramaHexStr, rssi_dbm, true);
}

static bool esHexadecimal32(const String& input) {
    if (input.length() != 32) return false;
    for (size_t i = 0; i < input.length(); ++i) {
        if (!isxdigit(static_cast<unsigned char>(input[i]))) return false;
    }
    return true;
}

// Prueba de integración por USB: valida el mismo formato AES/Frame/CounterWindow,
// pero usa estado aislado para no tocar NVS, el servo ni la ventana real de RF.
static void procesarTramaDePruebaSerial(const String& input) {
    // Reinicia únicamente el estado aislado del banco de pruebas, nunca el contador NVS real.
    if (input.startsWith("TEST_RESET:")) {
        const String valor = input.substring(11);
        if (valor.length() == 0) {
            Serial.println("RESULTADO:TEST_RESET_INVALIDO");
            return;
        }
        for (size_t i = 0; i < valor.length(); ++i) {
            if (!isdigit(static_cast<unsigned char>(valor[i]))) {
                Serial.println("RESULTADO:TEST_RESET_INVALIDO");
                return;
            }
        }
        const unsigned long ultimo = strtoul(valor.c_str(), nullptr, 10);
        if (ultimo > UINT32_MAX) {
            Serial.println("RESULTADO:TEST_RESET_INVALIDO");
            return;
        }
        ventanaPruebas.reset(static_cast<uint32_t>(ultimo));
        Serial.println("RESULTADO:TEST_RESET_OK");
        return;
    }
    if (!esHexadecimal32(input)) {
        Serial.println("RESULTADO:TRAMA_INVALIDA");
        return;
    }

    uint8_t cifrada[rf::FRAME_SIZE];
    for (uint8_t i = 0; i < rf::FRAME_SIZE; ++i) {
        char byteHex[3] = { input[i * 2], input[i * 2 + 1], '\0' };
        cifrada[i] = static_cast<uint8_t>(strtoul(byteHex, nullptr, 16));
    }

    uint8_t plana[rf::FRAME_SIZE];
    CryptoAES::desencriptar(cifrada, plana);
    rf::Frame frame;
    rf::frame_parse(plana, frame);

    if (!rf::frame_id_equals(frame, ID_ESPERADO)) {
        Serial.println("RESULTADO:RECHAZADO_ID_INVALIDO");
        return;
    }
    if (frame.command != rf::CMD_OPEN) {
        Serial.println("RESULTADO:RECHAZADO_COMANDO_INVALIDO");
        return;
    }

    const rf::FrameStatus status = ventanaPruebas.check(frame.counter);
    Serial.printf("RESULTADO:%s\n", rf::frame_status_name(status));
}

void setup() {
    Serial.begin(115200);

    // Inicializamos el servo a través de su módulo
    servoSG90::inicializar();

    memoriaNVS.begin("auto", false);
    ultimo_contador = memoriaNVS.getUInt("ultimo_cnt", 0);
    ventanaContador.reset(ultimo_contador);
    Serial.printf("[NVS] Último contador válido: %lu\n", (unsigned long)ultimo_contador);

    // La modulación vigente sobrevive a un reinicio (el llavero también la guarda)
    uint8_t mod_guardada = memoriaNVS.getUChar("modulacion", (uint8_t)radio::Modulation::FSK2);
    if (mod_guardada != (uint8_t)radio::Modulation::FSK2 && mod_guardada != (uint8_t)radio::Modulation::ASK_OOK) {
        mod_guardada = (uint8_t)radio::Modulation::FSK2;
    }
    modulacion_actual = nombreModulacion((radio::Modulation)mod_guardada);

    // Inicialización modular de red y API
    RedWiFi::iniciarServidor();
    ServidorWeb::inicializarRutas();

    // Inicialización de la nueva capa de RF
    radioDisponible = radio::begin(PINS_RF, radio::profile_for((radio::Modulation)mod_guardada));
    if (!radioDisponible) {
        Serial.println("[RF] ERROR: CC1101 no detectado; radio deshabilitada hasta reiniciar.");
    } else {
        escuchar();
    }
}

void loop() {
    RedWiFi::procesarWebSocket();
    servoSG90::cerrarPuerta();

    // Se venció la espera del ACK: no se cambia nada, el pedido sigue pendiente.
    if (Sincronizacion::tick() == Sincronizacion::Evento::SIN_ACK) {
        ServidorWeb::registrarEvento("CAMBIO_MODULACION_SIN_ACK", 0xABCDEF01, ultimo_contador, 0, modulacion_actual, ALERTA_RESINCRONIZACION);
        RedWiFi::notificarFrontendWebSocket("{\"recargar\": true}");
        Serial.println("[Sync] Sin ACK del llavero: el cambio sigue pendiente");
        escuchar();
    }

    // VÍA RF REAL (Transceptor CC1101)
    // ================================
    if (radioDisponible) {
        uint8_t buffer_rx[radio::PAYLOAD_LEN];
        int16_t rssi_dbm = 0;
        const radio::RxResult r = radio::rx_poll(buffer_rx, &rssi_dbm);

        if (r == radio::RxResult::FRAME) {
            manejarTramaRF(buffer_rx, rssi_dbm);
        }

        // FRAME y errores terminan en IDLE: rearmar. Si hay que recibir ACK,
        // usar RX continuo para no perderlo entre ventanas WOR.
        if (r != radio::RxResult::NONE) {
            if (Sincronizacion::esperando_ack()) radio::rx_start();
            else escuchar();
        }
    }

    // VÍA DE TESTING (Pytest por USB). Solo acepta líneas de 32 caracteres hex.
    // El estado de prueba está aislado del contador persistente y de la puerta real.
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        procesarTramaDePruebaSerial(input);
    }
}
