#include "radio_cc1101.h"
#include <Arduino.h>
#include <SmartRC_CC1101.h>
#include <string.h>

namespace radio {

    namespace {
        constexpr uint8_t MARC_MASK = 0x1F;
        constexpr uint8_t MARC_IDLE = 0X01;
        constexpr uint8_t MARC_TXFIFO_UNDERFLOW = 0X16;

        constexpr uint8_t RXBYTES_COUNT_MASK = 0X7F;
        constexpr uint8_t RXBYTES_OVERFLOW = 0X80;
        constexpr uint8_t LENGTH_BYTES = 1;
        constexpr uint8_t STATUS_BYTES = 2;
        constexpr uint8_t CRC_OK_BIT = 0X80;
        constexpr uint8_t FIFO_PACKET_BYTES = LENGTH_BYTES + PAYLOAD_LEN + STATUS_BYTES;
        
        constexpr uint8_t SYNC_BYTES = 2;
        constexpr uint8_t CRC_BYTES = 2;

        Pins g_pins = {0, 0, 0, 0, -1};
        Profile g_profile = PROFILE_2FSK;
        bool g_rx_armed = false;
        bool g_tx_active = false;

        uint8_t marc_state(){
            return ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & MARC_MASK;
        }

        //Luego de STX/SRX desde IDLE el chip calibra (1 ms aprox.) antes de cambiar de estado.
        bool wait_leave_idle(){
            const uint32_t t0 = millis();
            while (marc_state() == MARC_IDLE) {
                if (millis() - t0 > 10) return false;
            }
            return true;
        }

        // RXBYTES puede leerse mal mientras entran datos: se repite hasta tener 2 lecturas iguales
        uint8_t rxbytes_stable() {
            uint8_t a = ELECHOUSE_cc1101.SpiReadStatus(CC1101_RXBYTES);
            for (uint8_t i = 0; i < 3; i++) {
                const uint8_t b = ELECHOUSE_cc1101.SpiReadStatus(CC1101_RXBYTES);
                if(a == b) return a;
                a = b;
            }
            return a;
        }

        int16_t rssi_dbm_from_raw(uint8_t raw) {
            int16_t v = raw;
            if(v >= 128) v -= 256;
            return (int16_t)(v / 2 - 74); //Se usa la misma formula que getRssi() de SmartRC.
        }
    } // fin de namespace

    bool begin(const Pins& pins, const Profile& p) {
        g_pins = pins;
        g_rx_armed = false;
        g_tx_active = false;

        if (!cc1101_init(pins)) return false;
        cc1101_configure(p);
        g_profile = p;
        return true;
    }

    bool apply_profile(const Profile& p) {
        idle(); // la configuracion se escribe con IDLE y con FIFOs vacios.
        cc1101_configure(p);
        g_profile = p;
        return true;
    }

    const Profile& current_profile() { return g_profile; }
    Modulation current_modulation() { return g_profile.modulation; }

    uint16_t airtime_ms(const Profile& p) {
        const uint16_t bytes = PREAMBLE_BYTES + SYNC_BYTES + LENGTH_BYTES + PAYLOAD_LEN + CRC_BYTES;
        return (uint16_t)((bytes * 8.0f) / p.drate_kbaud + 0.5f); //kbaud = bits por ms
    }

    void idle() {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
        g_rx_armed = false;
        g_tx_active = false;
    }

    void sleep() {
        idle();
        ELECHOUSE_cc1101.goSleep();
    }

    bool wake() {
        return begin(g_pins, g_profile);
    }

    bool tx_start(const uint8_t frame[PAYLOAD_LEN]) {
        uint8_t buf[PAYLOAD_LEN];
        memcpy(buf, frame, PAYLOAD_LEN);
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
        ELECHOUSE_cc1101.SpiWriteReg(CC1101_TXFIFO, PAYLOAD_LEN); //byte de longitud (igual que en SendData)
        ELECHOUSE_cc1101.SpiWriteBurstReg(CC1101_TXFIFO, buf, PAYLOAD_LEN);
        ELECHOUSE_cc1101.SpiStrobe(CC1101_STX);
        g_rx_armed = false;

        // Si el chip no transmitio
        if (!wait_leave_idle()) {
            idle();
            return false;
        }
        g_tx_active = true;
        return true;
    }

    TxStatus tx_poll() {
        if (!g_tx_active) return TxStatus::NOT_ACTIVE;

        const uint8_t s = marc_state();
        if (s == MARC_TXFIFO_UNDERFLOW ) {
            idle();
            return TxStatus::FAILED;
        }

        if (s == MARC_IDLE) {
            g_tx_active = false; //Luego de TX el chip vuelve solo a IDLE
            return TxStatus::DONE;
        }

        return TxStatus::BUSY;
    }

    bool send_blocking(const uint8_t frame[PAYLOAD_LEN], uint16_t timeout_ms) {
        if (timeout_ms == 0) {
            const uint16_t t = airtime_ms(g_profile);
            timeout_ms = (uint16_t)(t + t / 2 + 50);
        }

        if (!tx_start(frame)) return false;

        const uint32_t t0 = millis();

        for (;;) {
            const TxStatus s = tx_poll();
            if (s == TxStatus::DONE) return true;
            if (s == TxStatus::BUSY) return false;
            if (millis() - t0 > timeout_ms) {
                idle();
                return false;
            }
            delay(1);
        }
    }

    void rx_start() {
        ELECHOUSE_cc1101.setSidle();
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
        ELECHOUSE_cc1101.SpiStrobe(CC1101_SRX);
        wait_leave_idle();
        g_rx_armed = true;
        g_tx_active = false;
    }

    RxResult rx_poll(uint8_t out[PAYLOAD_LEN], int16_t* rssi_dbm) {
        const uint8_t rxb = rxbytes_stable();
        const uint8_t count = rxb & RXBYTES_COUNT_MASK;

        if (rxb & RXBYTES_OVERFLOW) {
            idle();
            return RxResult::FIFO_OVERFLOW;
        }

        //Un paquete de 16 bytes ocupa 19 en la FIFO: logitud + 16 + RSSI + CRC
        if (count >= FIFO_PACKET_BYTES) {
            const uint8_t len = ELECHOUSE_cc1101.SpiReadReg(CC1101_RXFIFO);
            if (len != PAYLOAD_LEN) {
                idle();
                return RxResult::BAD_LENGTH;
            }
            uint8_t status[STATUS_BYTES];
            ELECHOUSE_cc1101.SpiReadBurstReg(CC1101_RXFIFO, out, PAYLOAD_LEN);
            ELECHOUSE_cc1101.SpiReadBurstReg(CC1101_RXFIFO, status, STATUS_BYTES);
            idle(); //luego de un paquete el chip queda en idle/
            if (rssi_dbm) *rssi_dbm = rssi_dbm_from_raw(status[0]);
            return (status[1] & CRC_OK_BIT) ? RxResult::FRAME : RxResult::BAD_CRC;
        }

        //Si estaba escuchando y el chip quedo en IDLE sin 19 bytes, ya no esta recibiendo:
        //O no llego nada (se rearma solo) o termino un paquete de otra longitud.
        if (g_rx_armed && marc_state() == MARC_IDLE) {
            if (count == 0) {
                rx_start();
                return RxResult::NONE;
            }
            idle();
            return RxResult::BAD_LENGTH;
        } 
        return RxResult::NONE;
    }

    void rx_start_wor() {
        // 1. Pasar a IDLE y vaciar la FIFO de recepción para evitar basura previa
        ELECHOUSE_cc1101.SpiStrobe(0x36); // Comando SIDLE
        ELECHOUSE_cc1101.SpiStrobe(0x3A); // Comando SFRX

        // 2. Configurar registros exclusivos de Wake-On-Radio
        // WORCTRL (0x20): Enciende el oscilador RC interno (requerido para el temporizador WOR)
        ELECHOUSE_cc1101.SpiWriteReg(0x20, 0x78); 
        
        // MCSM2 (0x18): Configurar el ciclo de trabajo de recepción (RX_TIME)
        ELECHOUSE_cc1101.SpiWriteReg(0x18, 0x00); 
        
        // 3. Sincronizar el reloj
        // SWORRST (0x3C): Resetea el RTC al valor del Evento 1
        ELECHOUSE_cc1101.SpiStrobe(0x3C); 

        // 4. Iniciar la secuencia WOR
        // SWOR (0x38): Activa el polling automático en lugar de la recepción continua
        ELECHOUSE_cc1101.SpiStrobe(0x38); 
    }
}