#include "servomotor.h"
#include <Arduino.h>
#include <ESP32Servo.h> // La librería externa queda confinada solo a este archivo

const uint32_t TIEMPO_ABIERTA_MS = 3000;
bool abierta = false;
uint32_t t_apertura = 0;

namespace servoSG90 {
    Servo cerraduraServo;
    const int PIN_SERVO = 4; // Pin GPIO para el servo

    void inicializar() {
        cerraduraServo.attach(PIN_SERVO);
        cerraduraServo.write(0); // Forzamos el estado seguro al arrancar
    }

    void abrirPuerta() {
        cerraduraServo.write(90); // Giro de apertura
        abierta = true;
        t_apertura = millis();              // Tiempo de cortesía para ingresar
    }

    void cerrarPuerta() {
        if(abierta && millis() - t_apertura >= TIEMPO_ABIERTA_MS) {
            cerraduraServo.write(0);
            abierta = false;
        }
       
    }
}