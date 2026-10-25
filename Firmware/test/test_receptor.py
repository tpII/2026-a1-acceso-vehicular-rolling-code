import serial
import time
import pytest
import struct
import os
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

# Configuración del entorno (Reemplazar por el puerto COM del ESP32 Receptor)
PUERTO = os.getenv('RECEPTOR_PORT', 'COM10')
BAUDIOS = 115200

# Credenciales exactas de C++ (El string en C++ agrega un byte nulo \x00 al final)
CLAVE_AES = b"ClaveSecreta123\x00"
ID_ESPERADO = bytes([0xAB, 0xCD, 0xEF, 0x01])

def generar_trama_hex(contador, comando_boton=0x01, id_llavero=ID_ESPERADO):
    """
    Ensambla la trama respetando el plan de proyecto: ID (4) + Cont (4) + Cmd (1) + Pad (7).
    """
    padding = b'\x00' * 7

    # Formato de struct.pack:
    # '<I' = Entero sin signo de 4 bytes (Contador, Little Endian)
    # 'B'  = Entero sin signo de 1 byte (Comando de botón)
    # '7s' = String/Bytes de 7 de longitud (Padding)
    datos_empaquetados = struct.pack('<I B 7s', contador, comando_boton, padding)

    # Trama plana completa de 16 bytes
    trama_plana = id_llavero + datos_empaquetados

    # Cifrado AES-128 ECB para replicar mbedtls
    cipher = Cipher(algorithms.AES(CLAVE_AES), modes.ECB(), backend=default_backend())
    encryptor = cipher.encryptor()
    trama_cifrada = encryptor.update(trama_plana) + encryptor.finalize()

    # Retorna la cadena en formato hexadecimal mayúscula con salto de línea
    return trama_cifrada.hex().upper() + '\n'

@pytest.fixture(scope="module")
def esp32_receptor():
    """Abre la conexión serial con el prototipo alimentado por USB."""
    puerto = serial.Serial(PUERTO, BAUDIOS, timeout=2)
    time.sleep(2) # Espera a que el ESP32 se reinicie al abrir el monitor serie
    puerto.reset_input_buffer()  # descarta mensajes de arranque y logs previos
    puerto.write(b'TEST_RESET:0\n')
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        linea = puerto.readline().decode('utf-8', errors='replace').strip()
        if linea == 'RESULTADO:TEST_RESET_OK':
            break
    else:
        puerto.close()
        pytest.fail('El firmware no respondió TEST_RESET; verificar firmware/puerto serie.')
    yield puerto
    puerto.close()

def enviar_y_leer(esp32, trama):
    esp32.write(trama.encode('utf-8'))
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        linea = esp32.readline().decode('utf-8', errors='replace').strip()
        if linea.startswith('RESULTADO:'):
            return linea
    pytest.fail('Timeout: el firmware no devolvió una línea RESULTADO: por Serial.')

def reiniciar_contador_de_prueba(esp32, ultimo):
    esp32.write(f'TEST_RESET:{ultimo}\n'.encode('ascii'))
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        linea = esp32.readline().decode('utf-8', errors='replace').strip()
        if linea == 'RESULTADO:TEST_RESET_OK':
            return
        if linea == 'RESULTADO:TEST_RESET_INVALIDO':
            pytest.fail('El firmware rechazó el valor inicial del contador de prueba.')
    pytest.fail('Timeout al reiniciar el contador aislado de pruebas.')

# --- SUITE DE PRUEBAS AUTOMATIZADAS ---

def test_01_acceso_secuencial_ok(esp32_receptor):
    reiniciar_contador_de_prueba(esp32_receptor, 0)
    trama = generar_trama_hex(1)
    respuesta = enviar_y_leer(esp32_receptor, trama)
    assert respuesta == "RESULTADO:ACEPTADO"

def test_02_ataque_replay(esp32_receptor):
    reiniciar_contador_de_prueba(esp32_receptor, 1)
    trama = generar_trama_hex(1) # Inyectamos el mismo contador (antiguo)
    respuesta = enviar_y_leer(esp32_receptor, trama)
    assert respuesta == "RESULTADO:RECHAZADO_REPLAY_ATTACK"

def test_03_resincronizacion_en_ventana(esp32_receptor):
    reiniciar_contador_de_prueba(esp32_receptor, 1)
    trama = generar_trama_hex(4) # Contador adelantado por 3 posiciones
    respuesta = enviar_y_leer(esp32_receptor, trama)
    assert respuesta == "RESULTADO:ACEPTADO"

def test_04_contador_fuera_de_ventana_requiere_resincronizacion_fisica(esp32_receptor):
    # TEST_RESET define el último contador aceptado. Con last=23, expected=24.
    reiniciar_contador_de_prueba(esp32_receptor, 23)
    assert enviar_y_leer(esp32_receptor, generar_trama_hex(28)) == "RESULTADO:ACEPTADO"

    # Tras aceptar 28, el próximo esperado es 29. Recibir 36 supone +7,
    # una unidad más allá de la ventana permitida (+0..+6).
    respuesta = enviar_y_leer(esp32_receptor, generar_trama_hex(36))
    assert respuesta == "RESULTADO:REQUIERE_RESINCRONIZACION_FISICA"


def test_05_resincronizacion_fisica_no_modifica_contador(esp32_receptor):
    # last=25 => expected=26. El contador 34 está +8 por delante.
    reiniciar_contador_de_prueba(esp32_receptor, 25)
    assert enviar_y_leer(esp32_receptor, generar_trama_hex(34)) == \
        "RESULTADO:REQUIERE_RESINCRONIZACION_FISICA"

    # Como 34 no se guardó, el 30 sigue dentro de la ventana respecto de 26.
    assert enviar_y_leer(esp32_receptor, generar_trama_hex(30)) == "RESULTADO:ACEPTADO"


def test_06_limite_superior_de_ventana_se_acepta(esp32_receptor):
    # last=25 => expected=26. Diferencia +6: recibido 32, se acepta.
    reiniciar_contador_de_prueba(esp32_receptor, 25)
    assert enviar_y_leer(esp32_receptor, generar_trama_hex(32)) == "RESULTADO:ACEPTADO"
    # El próximo esperado pasa a 33.
    assert enviar_y_leer(esp32_receptor, generar_trama_hex(33)) == "RESULTADO:ACEPTADO"


def test_07_id_invalido_no_consume_contador(esp32_receptor):
    reiniciar_contador_de_prueba(esp32_receptor, 0)
    id_incorrecto = bytes([0xAA, 0xBB, 0xCC, 0xDD])
    respuesta = enviar_y_leer(esp32_receptor, generar_trama_hex(1, id_llavero=id_incorrecto))
    assert respuesta == "RESULTADO:RECHAZADO_ID_INVALIDO"
    # La trama inválida no debe avanzar la ventana.
    assert enviar_y_leer(esp32_receptor, generar_trama_hex(1)) == "RESULTADO:ACEPTADO"


def test_08_comando_invalido_no_consume_contador(esp32_receptor):
    reiniciar_contador_de_prueba(esp32_receptor, 0)
    respuesta = enviar_y_leer(esp32_receptor, generar_trama_hex(1, comando_boton=0x02))
    assert respuesta == "RESULTADO:RECHAZADO_COMANDO_INVALIDO"
    # El comando no permitido tampoco debe avanzar la ventana.
    assert enviar_y_leer(esp32_receptor, generar_trama_hex(1)) == "RESULTADO:ACEPTADO"
