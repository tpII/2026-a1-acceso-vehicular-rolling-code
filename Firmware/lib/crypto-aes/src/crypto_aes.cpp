#include "crypto_aes.h"

#if defined(ESP8266)
    #include <bearssl/bearssl.h>
#else
    #include "mbedtls/aes.h"
#endif

// Debe ser exactamente la misma clave en todos los nodos
const unsigned char CryptoAES::CLAVE_SECRETA[16] = "ClaveSecreta123";

void CryptoAES::encriptar(const uint8_t *entrada, uint8_t *salida) {
    #if defined(ESP8266)
        //un unico bloque de 16 bytes con IV en cero: CBC == ECB
        br_aes_ct_cbcenc_keys ctx;
        br_aes_ct_cbcenc_init(&ctx, CLAVE_SECRETA, 16);
        uint8_t iv[16] = {0};
        memcpy(salida, entrada, 16);
        br_aes_ct_cbcenc_run(&ctx, iv, salida, 16);
    #else
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        
        // Configurar la clave para encriptación
        mbedtls_aes_setkey_enc(&aes, CLAVE_SECRETA, 128);
        
        // Cifrado ECB ideal para bloques fijos de 16 bytes
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, entrada, salida);
        
        mbedtls_aes_free(&aes);
    #endif
}

void CryptoAES::desencriptar(const uint8_t *entrada, uint8_t *salida) {
    #if defined(ESP8266)
        br_aes_ct_cbcdec_keys ctx;
        br_aes_ct_cbcdec_init(&ctx, CLAVE_SECRETA, 16);
        uint8_t iv[16] = {0};
        memcpy(salida, entrada, 16);
        br_aes_ct_cbcdec_run(&ctx, iv, salida, 16);
    #else
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        
        // Configurar la clave para desencriptación
        mbedtls_aes_setkey_dec(&aes, CLAVE_SECRETA, 128);
        
        // Descifrado ECB
        mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, entrada, salida);
        
        mbedtls_aes_free(&aes);
    #endif
}