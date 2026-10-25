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
    // Preámbulo largo: el receptor duerme en Wake-On-Radio y despierta cada WOR_PERIODO_MS,
    // así que el preámbulo tiene que durar más que ese período para que lo "pesque" despierto.
    // 24 bytes es el máximo del CC1101 (a 1,2 kBd son 160 ms).
    constexpr uint8_t PREAMBLE_BYTES = 24;  // bytes de preámbulo que se transmiten
    constexpr uint8_t PREAMBLE_CODE  = 7;   // valor de MDMCFG1.NUM_PREAMBLE para 24 bytes (0=2, 1=3, 2=4, 3=6, 4=8, 5=12, 6=16, 7=24)
    // Umbral de calidad de preámbulo (PKTCTRL1.PQT): el sync word solo se acepta tras 4*PQT
    // transiciones de preámbulo. Con PQT > 0 el receptor en WOR sigue escuchando si ve preámbulo
    // cuando vence su ventana de recepción.
    constexpr uint8_t PQT_VALUE = 1;

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