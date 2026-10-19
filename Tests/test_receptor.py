import serial
import time
import pytest
import struct
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend

# Configuración del entorno (Reemplazar por el puerto COM del ESP32 Receptor)
PUERTO = 'COM3' 
BAUDIOS = 115200

# Credenciales exactas de C++ (El string en C++ agrega un byte nulo \x00 al final)
CLAVE_AES = b"ClaveSecreta123\x00" 
ID_ESPERADO = bytes([0xAB, 0xCD, 0xEF, 0x01])

def generar_trama_hex(contador, comando_boton=0x01):
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
    trama_plana = ID_ESPERADO + datos_empaquetados
    
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
    yield puerto
    puerto.close()

def enviar_y_leer(esp32, trama):
    esp32.write(trama.encode('utf-8'))
    return esp32.readline().decode('utf-8').strip()

# --- SUITE DE PRUEBAS AUTOMATIZADAS ---

def test_01_acceso_secuencial_ok(esp32_receptor):
    trama = generar_trama_hex(1)
    respuesta = enviar_y_leer(esp32_receptor, trama)
    assert respuesta == "RESULTADO:ACEPTADO"

def test_02_ataque_replay(esp32_receptor):
    trama = generar_trama_hex(1) # Inyectamos el mismo contador (antiguo)
    respuesta = enviar_y_leer(esp32_receptor, trama)
    assert respuesta == "RESULTADO:RECHAZADO_REPLAY_ATTACK"

def test_03_resincronizacion_en_ventana(esp32_receptor):
    trama = generar_trama_hex(4) # Contador adelantado por 3 posiciones
    respuesta = enviar_y_leer(esp32_receptor, trama)
    assert respuesta == "RESULTADO:ACEPTADO"

def test_04_zona2_una_trama_queda_pendiente(esp32_receptor):
    # Receptor quedó con último_contador = 4. Un salto a 54 cae en zona de resincronización lógica.
    trama = generar_trama_hex(54)
    respuesta = enviar_y_leer(esp32_receptor, trama)
    assert respuesta == "RESULTADO:REQUIERE_RESINCRONIZACION_LOGICA"

def test_05_resincronizacion_fisica(esp32_receptor):
    # Salto extremo que exige emparejamiento manual.
    trama = generar_trama_hex(200)
    respuesta = enviar_y_leer(esp32_receptor, trama)
    assert respuesta == "RESULTADO:REQUIERE_RESINCRONIZACION_FISICA"