"""
Backend del Sistema de Acceso Vehicular Seguro.
Flask + SQLite, pensado para correr localmente en la misma red que
el receptor (ESP32) y la compu donde se abre el frontend.

Como correrlo:
    pip install -r requirements.txt
    python app.py

Por defecto queda escuchando en http://0.0.0.0:5000 (puerto 5000).
Para probarlo sin el ESP32 todavia, usa simulador.py en otra terminal.
"""
import sqlite3
from datetime import datetime, date

from flask import Flask, g, jsonify, request
from flask_cors import CORS
from flask_socketio import SocketIO


DB_PATH = "acceso_vehicular.db"
MODULACIONES_VALIDAS = ("2-FSK", "ASK/OOK")

app = Flask(__name__)
CORS(app)  # el frontend corre en otro origen (otro puerto/archivo), sin esto el navegador bloquea el fetch()

#Inicializa WebSockets permitiendo conexiones externas
socketio = SocketIO(app, cors_allowed_origins="*")

# ---------------------------------------------------------------------
# Base de datos: una conexion por request, se cierra sola al terminar
# ---------------------------------------------------------------------
def get_db():
    if "db" not in g:
        g.db = sqlite3.connect(DB_PATH)
        g.db.row_factory = sqlite3.Row  # permite acceder a las columnas por nombre, ej fila["contador"]
    return g.db


@app.teardown_appcontext
def close_db(exception=None):
    db = g.pop("db", None)
    if db is not None:
        db.close()


def init_db():
    db = sqlite3.connect(DB_PATH)
    db.execute("""
        CREATE TABLE IF NOT EXISTS eventos (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            id_llavero TEXT NOT NULL,
            contador INTEGER NOT NULL,
            modulacion TEXT NOT NULL,
            trama_hex TEXT NOT NULL,
            rssi TEXT,
            resultado TEXT NOT NULL,
            tipo_ataque TEXT
        )
    """)
    db.execute("""
        CREATE TABLE IF NOT EXISTS config (
            clave TEXT PRIMARY KEY,
            valor TEXT
        )
    """)
    db.execute("INSERT OR IGNORE INTO config (clave, valor) VALUES ('modulacion_deseada', '2-FSK')")
    db.commit()
    db.close()


def hex_contador(n):
    return "0x" + format(int(n), "04X")


# ---------------------------------------------------------------------
# Endpoints que consume el FRONTEND (index.html y logs.html)
# ---------------------------------------------------------------------
@app.route("/api/estado")
def api_estado():
    db = get_db()
    ultimo = db.execute("SELECT * FROM eventos ORDER BY id DESC LIMIT 1").fetchone()
    hoy = date.today().strftime("%d/%m/%Y")

    rechazados_hoy = db.execute(
        "SELECT COUNT(*) as c FROM eventos WHERE resultado = 'rechazado' AND timestamp LIKE ?",
        (hoy + "%",),
    ).fetchone()["c"]

    # LÓGICA DE ALERTA: Evaluamos el ÚLTIMO evento en vivo
    if ultimo and ultimo["tipo_ataque"] == "replay":
        seguro = False
        texto_estado = "¡Ataque Detectado!"
        mensaje = "Se detectó y bloqueó un intento de Replay Attack."
    else:
        seguro = True
        texto_estado = "Sistema seguro"
        if rechazados_hoy > 0:
            mensaje = f"Sistema estable. Se han bloqueado {rechazados_hoy} ataques hoy."
        else:
            mensaje = "No se detectaron ataques. El sistema se encuentra operando correctamente."

    return jsonify({
        "sistema_seguro": seguro,
        "texto_estado": texto_estado, # Enviamos el título dinámico
        "mensaje": mensaje,
        "contador_actual": hex_contador(ultimo["contador"]) if ultimo else "0x0000",
        "modulacion_actual": ultimo["modulacion"] if ultimo else "2-FSK",
        "ataques_rechazados_hoy": rechazados_hoy,
    })


@app.route("/api/tramas/<modulacion>")
def api_tramas(modulacion):
    # la url usa "2fsk" / "ook" para no pelear con la barra de "ASK/OOK"
    mod_map = {"2fsk": "2-FSK", "ook": "ASK/OOK"}
    mod_real = mod_map.get(modulacion.lower())
    if not mod_real:
        return jsonify({"error": "modulacion invalida, usar 2fsk O ASK/OOK"}), 400

    db = get_db()
    filas = db.execute(
        "SELECT timestamp, trama_hex, rssi, resultado FROM eventos WHERE modulacion = ? ORDER BY id DESC LIMIT 10",
        (mod_real,),
    ).fetchall()

    return jsonify([
        {
            "hora": f["timestamp"].split(" ")[-1],
            "trama": f["trama_hex"],
            "rssi": f["rssi"] or "-50",
            "resultado": f["resultado"]
        }
        for f in filas
    ])


@app.route("/api/logs")
def api_logs():
    db = get_db()
    filas = db.execute("SELECT * FROM eventos ORDER BY id DESC LIMIT 200").fetchall()
    return jsonify([
        {
            "fecha_hora": f["timestamp"],
            "id_llavero": f["id_llavero"],
            "contador": hex_contador(f["contador"]),
            "modulacion": f["modulacion"],
            "trama": f["trama_hex"],
            "tipo_ataque": f["tipo_ataque"] or "Ninguno",
            "resultado": f["resultado"],
        }
        for f in filas
    ])


@app.route("/api/modulacion", methods=["GET", "POST"])
def api_modulacion():
    db = get_db()
    if request.method == "POST":
        body = request.get_json(silent=True) or {}
        valor = body.get("modulacion")
        if valor not in MODULACIONES_VALIDAS:
            return jsonify({"error": "modulacion invalida, usar '2-FSK' o 'ASK/OOK'"}), 400
        db.execute("UPDATE config SET valor = ? WHERE clave = 'modulacion_deseada'", (valor,))
        db.commit()
        return jsonify({"ok": True, "modulacion_deseada": valor})

    fila = db.execute("SELECT valor FROM config WHERE clave = 'modulacion_deseada'").fetchone()
    return jsonify({"modulacion_deseada": fila["valor"] if fila else "2-FSK"})


# ---------------------------------------------------------------------
# Endpoint que va a usar el RECEPTOR (ESP32): aca llegan los eventos reales
# ---------------------------------------------------------------------
@app.route("/api/eventos", methods=["POST"])
def api_nuevo_evento():
    """
    El firmware del receptor hace un POST aca cada vez que procesa una
    trama (la haya aceptado o rechazado). Body esperado (JSON):

    {
        "id_llavero": "0x01A4",
        "contador": 16170,
        "modulacion": "2-FSK",
        "trama_hex": "A3 F4 9C 2E 7B 1D 4F 20",
        "rssi": "-50",
        "resultado": "aceptado",   // o "rechazado"
        "tipo_ataque": null        // o "replay"
    }
    """
    datos = request.get_json(silent=True)
    if not datos:
        return jsonify({"error": "body invalido, se esperaba JSON"}), 400

    requeridos = ["id_llavero", "contador", "modulacion", "trama_hex", "rssi", "resultado"]
    faltantes = [c for c in requeridos if c not in datos]
    if faltantes:
        return jsonify({"error": f"faltan campos: {', '.join(faltantes)}"}), 400

    if datos["modulacion"] not in MODULACIONES_VALIDAS:
        return jsonify({"error": "modulacion invalida"}), 400
    if datos["resultado"] not in ("aceptado", "rechazado"):
        return jsonify({"error": "resultado invalido, usar 'aceptado' o 'rechazado'"}), 400

    db = get_db()
    db.execute(
        """INSERT INTO eventos
           (timestamp, id_llavero, contador, modulacion, trama_hex, rssi, resultado, tipo_ataque)
           VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
        (
            datetime.now().strftime("%d/%m/%Y %H:%M:%S"),
            datos["id_llavero"],
            datos["contador"],
            datos["modulacion"],
            datos["trama_hex"],
            datos["rssi"],
            datos["resultado"],
            datos.get("tipo_ataque"),
        ),
    )
    db.commit()
    #Envia una señal de actualización a todos los navegadores conectados
    socketio.emit('actualizacion_urgente', {"recargar": True})
    return jsonify({"ok": True}), 201


if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=5000, debug=True)
