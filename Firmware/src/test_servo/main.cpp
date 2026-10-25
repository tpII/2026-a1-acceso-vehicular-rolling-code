#include <Arduino.h>
#include "../auto/servomotor.h"

uint32_t t_ultimo = 0;

void setup() {
    Serial.begin(115200);
    servoSG90::inicializar(); //arranca en 0 grados (cerrado)
    Serial.println("Prueba servo: abre cada 6seg, cierra solo a los 3seg");
}

void loop() {
    servoSG90::cerrarPuerta(); //cierra al pasar los 3seg
    
    if (millis() - t_ultimo >= 6000) {
        t_ultimo = millis();
        Serial.println("Abriendo (90 grados)");
        servoSG90::abrirPuerta();
    }
}