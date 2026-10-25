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
    bool send_blocking(const uint8_t frame[PAYLOAD_LEN], uint16_t timeout_ms = 0); // timeout_ms = 0 -> automatico (1.5 x tiempo en el aire + 50 ms)

    //Recepcion
    //Bad_length: Se recibió un frame con longitud incorrecta (distinta a PAYLOAD_LEN)
    //Bad_crc: Se recibió un frame con CRC incorrecto  
    //Fifo_overflow: Se produjo un desbordamiento del buffer de recepción (FIFO) en el módulo CC1101
    //None: No se ha recibido ningún frame de datos
    enum class RxResult : uint8_t { NONE, FRAME, BAD_CRC, BAD_LENGTH, FIFO_OVERFLOW };

    void rx_start(); // Inicia la recepción de un frame de datos
    //FRAME: payload valido en out y RSSI en dBm (si rssi_dbm != nullptr)
    // Con la escucha WOR activa y GDO0 configurado, rx_poll no toca el chip por SPI (lo despertaría
    // del sueño): solo lee el FIFO cuando la interrupción de GDO0 avisó que terminó un paquete.
    RxResult rx_poll(uint8_t frame[PAYLOAD_LEN], int16_t* rssi_dbm = nullptr); // Consulta si se ha recibido un frame de datos

    // Escucha en bajo consumo (Wake-On-Radio). El chip duerme y despierta cada WOR_PERIODO_MS
    // para escuchar una ventana corta. Después de cada paquete (o de un evento sin paquete) hay que
    // volver a llamarla. Si no hay pin GDO0 configurado cae en recepción continua.
    //
    // Regla para elegir el período con el preámbulo de 24 bytes: la ventana de escucha es el 12,5 %
    // del período, y el preámbulo tiene que durar al menos 0,875 * período + ~40 ms. A 1,2 kBd el
    // preámbulo dura 160 ms, o sea que el período no debería pasar de ~130 ms.
    constexpr uint16_t WOR_PERIODO_MS = 100;
    void rx_start_wor();
}

#endif