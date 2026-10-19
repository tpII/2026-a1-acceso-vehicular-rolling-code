#include <Arduino.h>
#include <SmartRC_CC1101.h>
#include "cc1101_config.h"


//modulation, frequency,  bandwidth, deviation, data rate
namespace radio {

    const Profile PROFILE_2FSK = { Modulation::FSK2, 433.92f, 1.2f, 5.16f, 58.03f};
    const Profile PROFILE_OOK = { Modulation::ASK_OOK, 433.92f, 1.2f, 0.0f, 58.03f};

    // Retorna el perfil de configuración para la modulación especificada
    const Profile& profile_for(Modulation m){
        return (m == Modulation::ASK_OOK) ? PROFILE_OOK : PROFILE_2FSK;
    }

    // Inicializa el módulo CC1101 con los pines especificados y devuelve true si la inicialización fue exitosa, false en caso contrario.
    bool cc1101_init(const Pins& pins) {
        ELECHOUSE_cc1101.setSpiPin(pins.sck, pins.miso, pins.mosi, pins.csn);
        if (pins.gdo0 >= 0)
            ELECHOUSE_cc1101.setGDO0((uint8_t)pins.gdo0);

        ELECHOUSE_cc1101.Init();
        return ELECHOUSE_cc1101.getCC1101();
    }

    void cc1101_configure(const Profile& p){
        ELECHOUSE_cc1101.setCCMode(1); // Establece el modo de operación del CC1101 (1 = modo normal)
        ELECHOUSE_cc1101.setModulation((uint8_t)p.modulation); // Configura la modulación (FSK2 o ASK_OOK)
        ELECHOUSE_cc1101.setMHZ(p.freq_mhz); // Configura la frecuencia de operación en MHz
        ELECHOUSE_cc1101.setDRate(p.drate_kbaud); // Configura la tasa de datos en kbps
        if(p.modulation != Modulation::ASK_OOK) {
            ELECHOUSE_cc1101.setDeviation(p.deviation_khz); // Configura la desviación de frecuencia en kHz (solo para FSK2)
        }
        ELECHOUSE_cc1101.setRxBW(p.rxbw_khz); // Configura el ancho de banda de recepción en kHz
        ELECHOUSE_cc1101.setChannel(CHANNEL); 
        ELECHOUSE_cc1101.setSyncMode(SYNC_MODE);
        ELECHOUSE_cc1101.setCrc(true); // Habilita la verificación de CRC
        ELECHOUSE_cc1101.setSyncWord(SYNC_WORD_H, SYNC_WORD_L); // Configura la palabra de sincronización
    }
    
    // Devuelve la configuración actual del módulo CC1101 en un objeto Readback
    Readback cc1101_readback(){
        Readback r;
        r.freq_mhz = ELECHOUSE_cc1101.getMHZ();
        r.drate_kbaud = ELECHOUSE_cc1101.getDRate();
        r.rxbw_khz = ELECHOUSE_cc1101.getRxBW();
        return r;
    }

}

