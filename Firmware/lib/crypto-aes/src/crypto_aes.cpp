#include "crypto_aes.h"
#include "mbedtls/aes.h"

// Debe ser exactamente la misma clave en todos los nodos
const unsigned char CryptoAES::CLAVE_SECRETA[16] = "ClaveSecreta123";

void CryptoAES::encriptar(const uint8_t *entrada, uint8_t *salida) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Configurar la clave para encriptación
    mbedtls_aes_setkey_enc(&aes, CLAVE_SECRETA, 128);
    
    // Cifrado ECB ideal para bloques fijos de 16 bytes
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, entrada, salida);
    
    mbedtls_aes_free(&aes);
}

void CryptoAES::desencriptar(const uint8_t *entrada, uint8_t *salida) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    
    // Configurar la clave para desencriptación
    mbedtls_aes_setkey_dec(&aes, CLAVE_SECRETA, 128);
    
    // Descifrado ECB
    mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, entrada, salida);
    
    mbedtls_aes_free(&aes);
}