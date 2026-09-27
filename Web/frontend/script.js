// ============================================================
// Esta URL debemos cambiarlasi el backend corre en otra
// máquina o puerto (por ejemplo, la IP de la notebook en la demo,
// tipo "http://192.168.0.15:5000").
// ============================================================
const BACKEND_URL = "http://127.0.0.1:5000";

document.addEventListener("DOMContentLoaded", () => {

    // ==========================================
    // LÓGICA PARA LA VISTA: INICIO (index.html)
    // ==========================================
    const btn2Fsk = document.getElementById("btn-2fsk");
    const btnOok = document.getElementById("btn-ook");
    const modIndicatorText = document.getElementById("current-mod-text");
	
    if (btn2Fsk && btnOok) {

        // Pinta el toggle y le avisa al backend cuál quedó activa.
        function activarModulacion(mod) {
            const es2Fsk = mod === "2-FSK";
            btn2Fsk.classList.toggle("active", es2Fsk);
            btnOok.classList.toggle("active", !es2Fsk);
            if (modIndicatorText) modIndicatorText.textContent = mod;

            fetch(`${BACKEND_URL}/api/modulacion`, {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ modulacion: mod }),
            }).catch((err) => console.error("No se pudo avisar el cambio de modulación:", err));
        }

        btn2Fsk.addEventListener("click", () => activarModulacion("2-FSK"));
        btnOok.addEventListener("click", () => activarModulacion("ASK/OOK"));

        // Lógica de alerta visual (Panel Rojo) en actualizarEstado()
        function actualizarEstado() {
            fetch(`${BACKEND_URL}/api/estado`)
                .then((r) => r.json())
                .then((datos) => {
                    const heading = document.getElementById("status-heading");
                    const desc = document.getElementById("status-desc");
                    const contador = document.getElementById("metric-contador");
                    const ataques = document.getElementById("metric-ataques");
 
                    // Utiliza el texto dinámico que manda el backend, o hace un fallback por defecto
					if (heading) heading.textContent = datos.texto_estado || (datos.sistema_seguro ? "Sistema seguro" : "Alerta de seguridad");
                    if (desc) desc.textContent = datos.mensaje;
                    if (contador) contador.textContent = datos.contador_actual;
                    if (ataques) ataques.textContent = datos.ataques_rechazados_hoy;
                    if (modIndicatorText) modIndicatorText.textContent = datos.modulacion_actual;
                    
                    // ALERTA VISUAL: Cambio de color del panel
                    const panelGlobal = document.querySelector(".global-status-panel");
                    if (panelGlobal) {
                        if (!datos.sistema_seguro) {
                            panelGlobal.classList.add("alert-mode");
                        } else {
                            panelGlobal.classList.remove("alert-mode");
                        }
                    }	
                })
                .catch((err) => console.error("No se pudo obtener el estado:", err));
        }

        // Lógica para dibujar las filas en las tablas correctas
        function actualizarTramas(endpoint, contenedorId) {
            const tbody = document.getElementById(contenedorId);
            if (!tbody) return;

            fetch(`${BACKEND_URL}/api/tramas/${endpoint}`)
                .then((r) => r.json())
                .then((tramas) => {
                    tramas.forEach((t) => {
                        const esAceptado = t.resultado === "aceptado";
                        const claseBadge = esAceptado ? "success" : "danger";
                        const textoBadge = esAceptado ? "Acceso permitido" : "Trama rechazada";

                        const fila = document.createElement("tr");
                        fila.innerHTML = `
                            <td>${t.hora}</td>
                            <td class="mono">${t.trama}</td>
                            <td class="mono">${t.rssi} dBm</td>
                            <td><span class="status-text ${claseBadge}">${textoBadge}</span></td>
                        `;
                        tbody.appendChild(fila);
                    });
                })
                .catch((err) => console.error("No se pudieron obtener las tramas:", err));
        }

        // Modifica refrescarInicio para apuntar a los nuevos IDs
        function refrescarInicio() {
            actualizarEstado();
            actualizarTramas("2fsk", "tbody-2fsk");
            actualizarTramas("ook", "tbody-ook");
        }
		
        refrescarInicio();
    }


    // ==========================================
    // LÓGICA PARA LA VISTA: LOGS (logs.html)
    // ==========================================
    const filterInput = document.querySelector(".filter-bar input");
    const tbody = document.getElementById("logs-tbody");

    if (tbody) {

        function aplicarFiltro() {
            if (!filterInput) return;
            const texto = filterInput.value.toLowerCase();
            // Se buscan las filas DE NUEVO cada vez que se llama a esta función,
            // en vez de guardarlas una sola vez al principio, porque cargarLogs()
            // reemplaza el contenido del tbody todo el tiempo.
            tbody.querySelectorAll("tr").forEach((row) => {
                row.style.display = row.textContent.toLowerCase().includes(texto) ? "" : "none";
            });
        }

        function cargarLogs() {
            fetch(`${BACKEND_URL}/api/logs`)
                .then((r) => r.json())
                .then((logs) => {
                    tbody.innerHTML = "";
                    logs.forEach((log) => {
                        const esRechazo = log.resultado === "rechazado";
                        const tr = document.createElement("tr");
                        const [fecha, hora] = log.fecha_hora.split(" ");
                        tr.innerHTML = `
                            <td>
                                <div class="datetime-cell">
                                    <span class="date">${fecha}</span>
                                    <span class="time">${hora}</span>
                                </div>
                            </td>
                            <td class="mono">${log.id_llavero}</td>
                            <td class="mono">${log.contador}</td>
							<td>${log.modulacion}</td>
                            <td class="mono">${log.trama}</td>
                            <td>${log.tipo_ataque}</td>
                            <td><span class="status-text ${esRechazo ? "danger" : "success"}">${esRechazo ? "Trama rechazada" : "Acceso permitido"}</span></td>`;
                        tbody.appendChild(tr);
                    });
                    aplicarFiltro(); // por si ya había texto escrito en el filtro
                })
                .catch((err) => console.error("No se pudieron obtener los logs:", err));
        }

        if (filterInput) filterInput.addEventListener("input", aplicarFiltro);

        cargarLogs();
    }
	
	// ==========================================
    // LÓGICA DE WEBSOCKETS
    // ==========================================
    // Conectarse al túnel de WebSockets
    const socket = io(BACKEND_URL);

    // Escuchar el evento que empuja el backend en app.py
    socket.on("actualizacion_urgente", (data) => {
        if (data.recargar) {
            // Si estamos en Inicio.html, recarga los paneles
            if (document.getElementById("status-heading")) {
                refrescarInicio();
            }
            // Si estamos en Logs.html, recarga la tabla
            if (document.getElementById("logs-tbody")) {
                cargarLogs();
            }
        }
    });

});
