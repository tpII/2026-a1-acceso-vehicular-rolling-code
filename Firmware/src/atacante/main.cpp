// NODO INTERCEPTOR — Arduino UNO R3 + CC1101 + conversor de nivel logico.
//
// Firmware final: a diferencia del modo test, aca ya se maneja el CC1101 de verdad.
// Por pedido del profesor, el interceptor NO descifra la trama (no tiene la clave AES):
// alcanza con demostrar que la puede capturar (16 bytes cifrados) y retransmitir tal cual.
//
// Flujo: el Arduino arranca dormido (sleep_cpu, modo PWR_DOWN) y solo despierta con el
// pulsador de CAPTURA (D2 = INT0, unico pin del UNO que puede despertar ese modo de sueno).
// Al despertar arma el CC1101 con la modulacion que indique el selector (D5) y escucha.
// Al capturar una trama valida (16 bytes, CRC ok) queda lista para reenviarla con el
// pulsador de REPLAY. El pulsador de JAMMING transmite ráfagas de basura mientras se lo
// mantiene presionado, en cualquier momento estando despierto. Presionar CAPTURA de nuevo
// (o pasar 120 s sin actividad) vuelve a dormir el nodo.
//
// GDO0 del CC1101: el UNO solo soporta attachInterrupt() en D2 y D3 (INT0/INT1), asi que
// no se puede usar D9 para una interrupcion como hace el receptor con el WOR. Como este
// nodo ya despierta por boton y despues escucha en recepcion continua (no necesita bajo
// consumo del CC1101 mientras esta despierto), GDO0 no hace falta: se deja sin usar en el
// codigo (Pins.gdo0 = -1), aunque este cableado al conector por si se lo quiere usar despues.

#include <Arduino.h>
#include <avr/sleep.h>
#include "config_control.h"
#include "../../lib/radio_cc1101/src/radio_cc1101.h"

// SCK, MISO, MOSI, CSN = SPI por hardware del UNO (D13, D12, D11, D10). GDO0 sin usar (ver arriba).
const radio::Pins PINS_RF = { 13, 12, 11, 10, -1 };

constexpr unsigned long TIMEOUT_INACTIVIDAD_MS = 120000UL; // 120 s sin actividad -> vuelve a dormir
unsigned long ultimoTiempoActividad = 0;

enum class EstadoInterceptor : uint8_t { DORMIDO, ESCUCHANDO, TRAMA_LISTA };
volatile EstadoInterceptor estadoActual = EstadoInterceptor::DORMIDO;

uint8_t  g_trama_capturada[radio::PAYLOAD_LEN];
bool     g_hay_trama = false;
bool     g_jamming_activo = false;

// ==========================================
// RUTINA DE INTERRUPCION (ISR)
// ==========================================
// Minima: solo marca que hay que despertar. Todo lo demas (CC1101, Serial, etc.) se hace
// despues, ya en el loop() normal, nunca dentro de un ISR.
void despertarISR() {
    sleep_disable();
}

// ==========================================
// CONTROL DE ENERGIA
// ==========================================
void entrarEnSleep() {
    Serial.println(F("[Energia] Entrando en modo Sleep (Power Down)..."));
    delay(50); // tiempo para que el puerto serie termine de imprimir

    radio::sleep(); // el CC1101 tambien baja a bajo consumo, no solo el AVR
    estadoActual = EstadoInterceptor::DORMIDO;

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);

    // Secuencia atomica recomendada para AVR: entre sleep_enable() y la instruccion SLEEP
    // (dentro de sleep_cpu()) no puede colarse una interrupcion que despierte la placa antes
    // de que el micro entre realmente a dormir; si eso pasara, el ISR correria pero igual se
    // ejecutaria la instruccion SLEEP despues, y la placa quedaria dormida sin nadie que la
    // despierte. noInterrupts()/interrupts() evita esa ventana.
    noInterrupts();
    sleep_enable();
    attachInterrupt(digitalPinToInterrupt(perifericos::PIN_BTN_CAPTURA), despertarISR, LOW);
    interrupts();   // en AVR, esta instruccion y la siguiente (dentro de sleep_cpu) son atomicas

    sleep_cpu();
    // --- EL ARDUINO DESPIERTA ACA, TRAS PRESIONAR CAPTURA (D2 en LOW) ---

    detachInterrupt(digitalPinToInterrupt(perifericos::PIN_BTN_CAPTURA));
    ultimoTiempoActividad = millis();
    Serial.println(F("[Energia] Arduino desperto."));
}

// Arma el CC1101 con la modulacion del selector y empieza a escuchar.
void iniciarEscucha() {
    const radio::Modulation mod = perifericos::modulacion_seleccionada();

    Serial.print(F("[RF] Configurando CC1101 en "));
    Serial.println(perifericos::nombre_modulacion(mod));

    if (!radio::begin(PINS_RF, radio::profile_for(mod))) {
        Serial.println(F("[RF] CC1101 no detectado. Volviendo a dormir."));
        entrarEnSleep();
        return;
    }

    radio::rx_start();
    g_hay_trama = false;
    estadoActual = EstadoInterceptor::ESCUCHANDO;
    Serial.println(F("[RF] Escuchando..."));
}

void imprimirTrama(const uint8_t trama[radio::PAYLOAD_LEN]) {
    char hex[radio::PAYLOAD_LEN * 2 + 1];
    for (uint8_t i = 0; i < radio::PAYLOAD_LEN; i++) {
        sprintf(hex + i * 2, "%02X", trama[i]);
    }
    Serial.println(hex);
}

void setup() {
    Serial.begin(115200);
    delay(50);

    perifericos::iniciar();

    Serial.println(F("\n[INTERCEPTOR] Arranca dormido. Presiona CAPTURA para despertarlo."));
    entrarEnSleep(); // arranca dormido por defecto
}

void loop() {
    // Timeout de inactividad: aplica a cualquier estado despierto (ESCUCHANDO o TRAMA_LISTA).
    if (estadoActual != EstadoInterceptor::DORMIDO &&
        millis() - ultimoTiempoActividad > TIMEOUT_INACTIVIDAD_MS) {
        Serial.println(F("[Timeout] 120 segundos sin actividad."));
        g_hay_trama = false;
        entrarEnSleep();
        return;
    }

    // CAPTURA: presionarlo de nuevo, estando despierto, vuelve a dormir el nodo.
    const perifericos::Edge captura = perifericos::leer_captura();
    if (estadoActual == EstadoInterceptor::DORMIDO) {
        // Mientras esta dormido, CAPTURA se atiende via interrupcion (ver entrarEnSleep).
        iniciarEscucha();
        return;
    }
    if (captura == perifericos::Edge::PRESIONADO) {
        g_hay_trama = false;
        entrarEnSleep();
        return;
    }

    // JAMMING: disponible en cualquier estado despierto. Mientras esta mantenido, transmite
    // rafagas de basura una atras de otra (no es una portadora continua real: la libreria
    // solo expone transmision por paquetes, y el preambulo largo -24 bytes, pensado para el
    // Wake-On-Radio del receptor- hace que cada rafaga tarde unos 300 ms. Es un jamming
    // "controlado", a los fines de la demostracion, no un jammer de RF de verdad).
    switch (perifericos::leer_jamming()) {
    case perifericos::Edge::PRESIONADO:
        g_jamming_activo = true;
        Serial.println(F("[JAMMING] Iniciando..."));
        break;
    case perifericos::Edge::SOLTADO:
        g_jamming_activo = false;
        Serial.println(F("[JAMMING] Detenido."));
        radio::idle();
        if (estadoActual == EstadoInterceptor::ESCUCHANDO) radio::rx_start();
        break;
    case perifericos::Edge::NINGUNO:
        break;
    }

    if (g_jamming_activo) {
        uint8_t basura[radio::PAYLOAD_LEN];
        for (uint8_t i = 0; i < radio::PAYLOAD_LEN; i++) basura[i] = (uint8_t)random(256);
        radio::send_blocking(basura);
        ultimoTiempoActividad = millis();

        // Revisa el boton entre rafaga y rafaga para soltar rapido, sin esperar a la proxima vuelta.
        if (perifericos::leer_jamming() == perifericos::Edge::SOLTADO) {
            g_jamming_activo = false;
            Serial.println(F("[JAMMING] Detenido."));
            radio::idle();
            if (estadoActual == EstadoInterceptor::ESCUCHANDO) radio::rx_start();
        }
        return; // mientras hay jamming, no se atiende captura ni replay
    }

    // Maquina de estados: captura (una sola vez por ciclo despierto) y replay.
    switch (estadoActual) {

    case EstadoInterceptor::ESCUCHANDO: {
        // El selector tambien se puede mover ya despierto: si cambia, se reconfigura el CC1101.
        radio::Modulation nueva_mod;
        if (perifericos::cambio_modulacion(nueva_mod)) {
            Serial.print(F("[RF] Selector cambiado, reconfigurando a "));
            Serial.println(perifericos::nombre_modulacion(nueva_mod));
            radio::apply_profile(radio::profile_for(nueva_mod));
            radio::rx_start();
        }

        uint8_t trama[radio::PAYLOAD_LEN];
        const radio::RxResult r = radio::rx_poll(trama);

        if (r == radio::RxResult::FRAME) {
            memcpy(g_trama_capturada, trama, radio::PAYLOAD_LEN);
            g_hay_trama = true;
            ultimoTiempoActividad = millis();

            Serial.print(F("[RF] Trama capturada: "));
            imprimirTrama(g_trama_capturada);
            // LED: digitalWrite(PIN_LED_TRAMA_LISTA, HIGH);

            estadoActual = EstadoInterceptor::TRAMA_LISTA;
        } else if (r == radio::RxResult::BAD_CRC || r == radio::RxResult::BAD_LENGTH ||
                   r == radio::RxResult::FIFO_OVERFLOW) {
            radio::rx_start(); // trama ajena / ruido: seguir escuchando
        }
        break;
    }

    case EstadoInterceptor::TRAMA_LISTA:
        // Deja de escuchar nuevas tramas mientras haya una lista: asi el replay siempre
        // retransmite exactamente lo ultimo capturado. Para capturar otra, hay que dormir
        // (CAPTURA) y volver a despertar.
        if (g_hay_trama && perifericos::leer_replay() == perifericos::Edge::PRESIONADO) {
            Serial.println(F("[Ataque] Ejecutando Replay Attack (TX)..."));
            const bool ok = radio::send_blocking(g_trama_capturada);
            Serial.println(ok ? F("[Ataque] Transmision OK") : F("[Ataque] Fallo la transmision"));
            ultimoTiempoActividad = millis();
        }
        break;

    case EstadoInterceptor::DORMIDO:
        break; // nunca deberia llegar aca (se atiende mas arriba), por las dudas no hace nada
    }
}
