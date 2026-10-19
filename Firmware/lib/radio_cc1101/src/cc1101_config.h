#ifndef CC1101_CONFIG_H
#define CC1101_CONFIG_H

#include <stdint.h>

namespace radio {

    // Estructura que representa un perfil de configuración para el módulo CC1101
    enum class Modulation : uint8_t {
        FSK2 = 0,
        ASK_OOK = 2
    };

    struct Profile {
        Modulation modulation; // Tipo de modulación (FSK2 o ASK_OOK)
        float freq_mhz;       // Frecuencia de operación en MHz
        float drate_kbaud;     // Tasa de datos en kbps
        float deviation_khz;   // Desviación de frecuencia en kHz (solo para FSK2)
        float rxbw_khz;        // Ancho de banda de recepción en kHz
    };

    extern const Profile PROFILE_2FSK;
    extern const Profile PROFILE_OOK;

    const Profile& profile_for(Modulation m);

    //Valores comunes a todos los perfiles (mismo sync word en emisor y receptor)
    constexpr uint8_t SYNC_WORD_H = 0xD3; // Parte alta de la palabra de sincronización
    constexpr uint8_t SYNC_WORD_L = 0x91; // Parte baja de la palabra de sincronización
    constexpr uint8_t SYNC_MODE = 0x02;    // Modo de sincronización (0x02 = 16 bits de sincronización)
    constexpr uint8_t CHANNEL = 0; // Canal de operación (0-255)
    constexpr uint8_t PREAMBLE_BYTES = 2; // Número de bytes de preámbulo (2 bytes)

    // Estructura que define el mapeo físico de los pines del CC1101
    struct Pins {
        uint8_t sck, miso, mosi, csn;
        int8_t gdo0;  // Pin GDO0 (Interrupción principal / Estado)
    };
    
    //SPI + reset del chip. false si no detecta el CC1101
    bool cc1101_init(const Pins& pins);

    //La secuencia probada, en este orden:
    //setCCMODE(1) --> setModulation --> setMHZ --> setDRATE --> setDeviation (solo 2-FSK) --> setRxBW --> setChannel(0) --> setSyncMode(2) --> setCrc(1) --> setSyncWord(D3, 91) 
    //El chip debe estar en IDLE (lo garantiza radio::apply_profile)
    void cc1101_configure(const Profile& p);

    struct Readback {
        float freq_mhz;
        float drate_kbaud;
        float rxbw_khz;
    };

    Readback cc1101_readback();
}

#endif