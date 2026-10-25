#ifndef SERVOMOTOR_H
#define SERVOMOTOR_H

#include <Arduino.h>
#include <ESP32Servo.h> // La librería externa queda confinada solo a este archivo

namespace servoSG90 {
    void inicializar();
    void abrirPuerta();
    void cerrarPuerta();
}

#endif