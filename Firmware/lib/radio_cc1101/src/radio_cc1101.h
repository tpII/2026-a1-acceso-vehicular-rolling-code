#ifndef RADIO_CC1101_H
#define RADIO_CC1101_H

#include <stdint.h>
#include "cc1101_config.h"

namespace radio {
    constexpr uint8_t PAYLOAD_LEN = 16;
    // Inicializa SPI, el chip y aplica el perfil. False si no detecta el CC1101.
    bool begin(const Pins& pins, const Profile& p = PROFILE_2FSK);
    
    bool apply_profile(const Profile& p);
    const Profile& current_profile();
    Modulation current_modulation();

    uint16_t airtime_ms(const Profile& p);
    void idle();
    void sleep();
    bool wake();

    //Transmision
    // NOT_ACTIVE: No se ha iniciado ninguna transmisión
    // BUSY: Se está transmitiendo un frame de datos
    // DONE: La transmisión se completó exitosamente
    // FAILED: La transmisión falló (por ejemplo, debido a un desbordamiento del buffer de transmisión)
    enum class TxStatus : uint8_t { NOT_ACTIVE, BUSY, DONE, FAILED };
    bool tx_start(const uint8_t frame[PAYLOAD_LEN]); // Inicia la transmisión de un frame de datos
    TxStatus tx_poll(); // Consulta el estado de la transmisión en curso
    bool send_blocking(const uint8_t frame[PAYLOAD_LEN], uint32_t timeout_ms = 0); // timeout_ms = 0 -> automatico (1.5 x tiempo en el aire + 50 ms)

    //Recepcion
    //Bad_length: Se recibió un frame con longitud incorrecta (distinta a PAYLOAD_LEN)
    //Bad_crc: Se recibió un frame con CRC incorrecto  
    //Fifo_overflow: Se produjo un desbordamiento del buffer de recepción (FIFO) en el módulo CC1101
    //None: No se ha recibido ningún frame de datos
    enum class RxResult : uint8_t { NONE, FRAME, BAD_CRC, BAD_LENGTH, FIFO_OVERFLOW };

    void rx_start(); // Inicia la recepción de un frame de datos
    //FRAME: payload valido en out y RSSI en dBm (si rssi_dbm != nullptr)
    RxResult rx_poll(uint8_t frame[PAYLOAD_LEN], int16_t* rssi_dbm = nullptr); // Consulta si se ha recibido un frame de datos
    // Función para iniciar la escucha en bajo consumo
    void rx_start_wor();
}

#endif