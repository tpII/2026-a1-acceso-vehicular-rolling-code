#include <Arduino.h>
#include <avr/sleep.h>

// ==========================================
// CONFIGURACIÓN DE PINES (Pull-Up interno)
// ==========================================
const int PIN_BTN_WAKE = 2;      // Intercepción y Despertar (Debe ser INT0 en Arduino UNO)
const int PIN_BTN_REPLAY = 4;    // Retransmitir trama capturada
const int PIN_SWITCH_MOD = 5;    // Selector de modulación: 2-FSK o ASK/OOK
// const int PIN_BTN_JAMMING = 6; // Reservado para futuro Jamming
const int PIN_LED = 7;           // Indicador visual de trama lista

// ==========================================
// VARIABLES DE ESTADO Y TIEMPO
// ==========================================
const unsigned long TIMEOUT_INACTIVIDAD = 120000; // 120.000 ms = 120 segundos
unsigned long ultimoTiempoActividad = 0;

enum EstadoInterceptor {
    DORMIDO,
    ESCUCHANDO,
    TRAMA_LISTA
};
volatile EstadoInterceptor estadoActual = DORMIDO;

// ==========================================
// RUTINA DE INTERRUPCIÓN (ISR)
// ==========================================
// Se ejecuta inmediatamente cuando el voltaje en PIN_BTN_WAKE cae a LOW
void despertarISR() {
    sleep_disable(); // Desactiva la protección de bajo consumo
    if (estadoActual == DORMIDO) {
        estadoActual = ESCUCHANDO; // Bloqueo de estado para no re-disparar
    }
}

// ==========================================
// CONTROL DE ENERGÍA
// ==========================================
void entrarEnSleep() {
    Serial.println("[Energía] Entrando en modo Sleep (Power Down)...");
    delay(50); // Tiempo para que el puerto serie termine de imprimir
    
    digitalWrite(PIN_LED, LOW);
    estadoActual = DORMIDO;
    
    // Configurar el modo de sueño más profundo del AVR
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    
    // Habilitar la interrupción externa en el Pin 2 por flanco de bajada o estado BAJO
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_WAKE), despertarISR, LOW);
    
    // Detiene el reloj de la CPU. El código se pausa exactamente en esta línea.
    sleep_cpu(); 
    
    // --- EL ARDUINO DESPIERTA AQUÍ TRAS PRESIONAR D2 ---
    detachInterrupt(digitalPinToInterrupt(PIN_BTN_WAKE)); // Desactivar para evitar rebotes
    ultimoTiempoActividad = millis(); 
    Serial.println("[Energía] Arduino despertó. Iniciando intercepción.");
}

void setup() {
    Serial.begin(115200);

    // Activar resistencias Pull-Up internas (estado en reposo = HIGH)
    pinMode(PIN_BTN_WAKE, INPUT_PULLUP);
    pinMode(PIN_BTN_REPLAY, INPUT_PULLUP);
    pinMode(PIN_SWITCH_MOD, INPUT_PULLUP);
    // pinMode(PIN_BTN_JAMMING, INPUT_PULLUP);
    
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    // Iniciar con la placa dormida por defecto
    entrarEnSleep(); 
}

void loop() {
    // 1. Control de Inactividad (Timeout de 120s)
    if (estadoActual != DORMIDO && (millis() - ultimoTiempoActividad > TIMEOUT_INACTIVIDAD)) {
        Serial.println("[Timeout] 120 segundos sin actividad. Borrando RAM y apagando.");
        // memset(trama_capturada, 0, sizeof(trama_capturada)); // Limpiar la memoria
        entrarEnSleep();
    }

    // 2. Máquina de Estados
    switch (estadoActual) {
        
        case ESCUCHANDO:
            // Leer el Slide Switch (HIGH = Abierto, LOW = Cerrado a GND)
            // bool esOOK = digitalRead(PIN_SWITCH_MOD); 
            
            // Aquí se envían los comandos SPI para despertar y configurar el CC1101
            // CC1101_SetModulation(esOOK ? ASK_OOK : FSK_2);
            // CC1101_SetReceive();
            
            // Simulación de captura exitosa (Reemplazar con la lectura real del pin GDO0)
            // if (CC1101_PacketReceived()) {
                Serial.println("[RF] Trama capturada y almacenada.");
                digitalWrite(PIN_LED, HIGH); // Confirmación visual
                estadoActual = TRAMA_LISTA;
                ultimoTiempoActividad = millis(); // Refrescar timeout
            // }
            break;

        case TRAMA_LISTA:
            // Monitorear el pulsador 2 (Retransmitir)
            if (digitalRead(PIN_BTN_REPLAY) == LOW) {
                delay(50); // Anti-rebote por software
                if (digitalRead(PIN_BTN_REPLAY) == LOW) {
                    Serial.println("[Ataque] Ejecutando Replay Attack (TX)...");
                    
                    // Comandos SPI para pasar el CC1101 a TX y vaciar el buffer
                    // CC1101_TransmitData(trama_capturada);
                    
                    ultimoTiempoActividad = millis(); 
                    
                    // Esperar a que el usuario suelte el botón para evitar ráfagas accidentales
                    while(digitalRead(PIN_BTN_REPLAY) == LOW); 
                }
            }
            break;
            
        case DORMIDO:
            break;
    }
}