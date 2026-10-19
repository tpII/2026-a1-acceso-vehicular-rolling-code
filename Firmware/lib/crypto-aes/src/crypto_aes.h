#ifndef CRYPTO_AES_H
#define CRYPTO_AES_H

#include <Arduino.h>

class CryptoAES {
public:
    // La clave secreta de 16 bytes (128 bits) compartida entre llavero y auto
    static const unsigned char CLAVE_SECRETA[16];

    static void encriptar(const uint8_t *entrada, uint8_t *salida);
    static void desencriptar(const uint8_t *entrada, uint8_t *salida);
};

#endif