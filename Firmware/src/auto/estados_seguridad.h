#ifndef ESTADOS_SEGURIDAD_H
#define ESTADOS_SEGURIDAD_H

enum EstadoValidacion {
    ACCESO_PERMITIDO, // Contador dentro de la ventana dinámica (+1 a +6)
    RECHAZO_REPLAY_ATTACK, // Contador menor o igual al histórico
    RECHAZO_DESINCRONIZACION, // Salto de contador mayor a +50 (Requiere emparejamiento)
    ALERTA_RESINCRONIZACION, // Salto medio (+7 a +50)
    ERROR_ID_INVALIDO, // Trama pertenece a otro llavero
    ERRO_CRIPTOGRAFICO // Fallo al descifrar AES-128
};


struct EventoSeguridad {
   String tramaHex; // Trama recibida en formato hexadecimal
   uint32_t idllavero; // ID del llavero que envió la trama
   uint32_t contador; // Contador recibido en la trama
   int rssi; // Intensidad de la señal recibida (RSSI)
   String modulacion; // Tipo de modulación utilizada (ASK o FSK)
   EstadoValidacion estado; // Estado de validación de la trama
};

#endif
