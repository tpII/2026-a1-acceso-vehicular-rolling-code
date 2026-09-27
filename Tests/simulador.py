"""
Simula al receptor (ESP32) mandando eventos al backend, para poder
probar el frontend ANTES de tener el firmware real
listo. No reemplaza al ESP32: es solo una herramienta de desarrollo.

Como usarlo (con app.py ya corriendo en otra terminal):
    pip install requests
    python simulador.py
"""
import random
import time

import requests

URL = "http://127.0.0.1:5000/api/eventos"

# (modulacion, resultado, tipo_ataque) — la mezcla de ejemplos que va a mandar
EVENTOS_DE_EJEMPLO = [
    ("2-FSK", "aceptado", None),
    ("2-FSK", "aceptado", None),
    ("ASK/OOK", "aceptado", None),
    ("2-FSK", "rechazado", "replay"),
    ("ASK/OOK", "rechazado", "replay"),
    ("ASK/OOK", "aceptado", None),
]

contador = 16150


def trama_falsa():
    return " ".join(f"{random.randint(0, 255):02X}" for _ in range(8))


def main():
    global contador
    print(f"Mandando eventos a {URL} (Ctrl+C para cortar)")
    while True:
        modulacion, resultado, tipo_ataque = random.choice(EVENTOS_DE_EJEMPLO)
        contador += 1
        
        # Generamos un RSSI falso negativo (ej: -45, -78)
        rssi_simulado = f"-{random.randint(40, 90)}"
        
        body = {
            "id_llavero": "0x01A4",
            "contador": contador,
            "modulacion": modulacion,
            "trama_hex": trama_falsa(),
            "rssi": rssi_simulado,
            "resultado": resultado,
            "tipo_ataque": tipo_ataque,
        }

        try:
            r = requests.post(URL, json=body, timeout=3)
            print(f"POST -> {r.status_code} | {body['modulacion']} | {body['resultado']} | contador={contador}")
        except requests.exceptions.ConnectionError:
            print("No se pudo conectar. ¿Esta corriendo app.py en otra terminal?")
            return

        time.sleep(3)


if __name__ == "__main__":
    main()
