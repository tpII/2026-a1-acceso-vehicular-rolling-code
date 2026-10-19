#include <Arduino.h>
#include <Preferences.h>
#include "red_wifi.h"
#include "servidor.h"
#include "estados_seguridad.h"
#include "servomotor.h"
#include "../lib/crypto-aes/src/crypto_aes.h" 
#include "../lib/radio_cc1101/src/radio_cc1101.h"

const radio::Pins PINS_RF = { 
    .sck = 18, 
    .miso = 19,
    .mosi = 23,
    .csn = 5,
    .gdo0 = 2
};

const uint8_t ID_ESPERADO[4] = {0xAB, 0xCD, 0xEF, 0x01}; // ID esperado del llavero (4 bytes)


Preferences memoriaNVS;

// Variables globales de estado (leídas por servidor.cpp)
uint32_t ultimo_contador = 0;
String modulacion_actual = "2-FSK";
uint32_t ataques_rechazados = 0;
bool sistema_seguro = true;
String mensaje_estado = "El sistema se encuentra operando correctamente.";

void procesarValidacion(uint32_t contador_recibido, String tramaHexStr, int rssi_dbm) {
    EstadoValidacion resultado_evaluacion;
            
    if (contador_recibido <= ultimo_contador) {
        resultado_evaluacion = RECHAZO_REPLAY_ATTACK;
        sistema_seguro = false;
        mensaje_estado = "Se ha bloqueado un intento de Replay Attack.";
        ataques_rechazados++;
    } else if (contador_recibido <= ultimo_contador + 6) {
        resultado_evaluacion = ACCESO_PERMITIDO;
        sistema_seguro = true;
        mensaje_estado = "Operando con normalidad.";
        
        ultimo_contador = contador_recibido;
        memoriaNVS.putUInt("ultimo_cnt", ultimo_contador);

        // Delegamos la apertura física al módulo del servomotor
        servoSG90::abrirPuerta();

    } else if (contador_recibido <= ultimo_contador + 50) {
        resultado_evaluacion = ALERTA_RESINCRONIZACION;
        sistema_seguro = false;
        mensaje_estado = "Desfase medio detectado. Requiere resincronización lógica.";
    } else {
        resultado_evaluacion = RECHAZO_DESINCRONIZACION;
        sistema_seguro = false;
        mensaje_estado = "Desfase extremo. Requiere emparejamiento físico.";
    }

    // Delegar el almacenamiento al módulo del servidor
    ServidorWeb::registrarEvento(tramaHexStr, 0xABCDEF01, contador_recibido, rssi_dbm, modulacion_actual, resultado_evaluacion);

    // Notificar cambios a la interfaz en milisegundos
    RedWiFi::notificarFrontendWebSocket("{\"recargar\": true}");
    Serial.println("[Sistema] Evento procesado.");
}

void setup() {
    Serial.begin(115200);
    
    // Inicializamos el servo a través de su módulo
    servoSG90::inicializar(); 

    memoriaNVS.begin("auto", false);
    ultimo_contador = memoriaNVS.getUInt("ultimo_cnt", 0);
    Serial.printf("[NVS] Último contador válido: %d\n", ultimo_contador);

    // Inicialización modular de red y API
    RedWiFi::iniciarServidor();
    ServidorWeb::inicializarRutas();

    // Inicialización de la nueva capa de RF
    radio::begin(PINS_RF);   
    radio::rx_start_wor();   // Inicia el ciclo en modo de bajo consumo
}

void loop() {
    RedWiFi::procesarWebSocket();
    servoSG90::cerrarPuerta();

    // ==========================================
    // 1. VÍA RF REAL (Transceptor CC1101)
    // ==========================================
    uint8_t buffer_rx[16];
    int16_t rssi_dbm = 0;
    
    radio::RxResult r = radio::rx_poll(buffer_rx, &rssi_dbm);
    
    if (r == radio::RxResult::FRAME) {
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

        // Extracción de datos
        uint8_t id_recibido[4];
        uint32_t contador_recibido;
        memcpy(id_recibido, trama_descifrada, 4);
        memcpy(&contador_recibido, trama_descifrada + 4, 4);

        // Validar ID antes de procesar el contador
        if (memcmp(id_recibido, ID_ESPERADO, 4) == 0) {
            procesarValidacion(contador_recibido, tramaHexStr, rssi_dbm);
        } else {
            ServidorWeb::registrarEvento(tramaHexStr, 0x00000000, contador_recibido, rssi_dbm, modulacion_actual, ERROR_ID_INVALIDO);
            RedWiFi::notificarFrontendWebSocket("{\"recargar\": true}");
        }
    }
    
    // Si el estado no es NONE (ej. hubo BAD_CRC, OVERFLOW o se leyó un FRAME), reiniciar la escucha en modo WOR
    if (r != radio::RxResult::NONE) {
        radio::rx_start_wor();
    }

    // ==========================================
    // 2. VÍA DE TESTING (Simulación por Puerto Serie para Pytest)
    // ==========================================
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        // El script de Pytest inyectará tramas de 32 caracteres (16 bytes en Hexadecimal)
        if (input.length() == 32) {
            uint8_t trama_simulada[16];
            for (int i = 0; i < 16; i++) {
                String byteHex = input.substring(i*2, i*2 + 2);
                trama_simulada[i] = (uint8_t) strtol(byteHex.c_str(), NULL, 16);
            }

            uint8_t trama_descifrada[16];
            CryptoAES::desencriptar(trama_simulada, trama_descifrada);

            uint32_t contador_simulado;
            memcpy(&contador_simulado, trama_descifrada + 4, 4);

            procesarValidacion(contador_simulado, input, -65); // RSSI simulado de -65 dBm
        }
    }
}