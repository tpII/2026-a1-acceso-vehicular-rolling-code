document.addEventListener("DOMContentLoaded", () => {
    // La API se encuentra en el mismo equipo que sirve la interfaz.
    // Versión actual: servidor Flask, si la web se sirve desde él.
    // Versión autónoma: ESP32, cuando sirve la interfaz y la API.
    const API_BASE = window.location.origin;

    const btn2Fsk = document.getElementById("btn-2fsk");
    const btnOok = document.getElementById("btn-ook");
    const modIndicatorText = document.getElementById("current-mod-text");
    const viewContainer = document.querySelector(".view-container");
    let bannerResync = document.getElementById("resync-alert");
    if (!bannerResync && viewContainer) {
        bannerResync = document.createElement("section");
        bannerResync.id = "resync-alert";
        bannerResync.className = "resync-alert";
        bannerResync.setAttribute("role", "alert");
        bannerResync.setAttribute("aria-live", "assertive");
        bannerResync.hidden = true;
        viewContainer.prepend(bannerResync);
    }
    function actualizarAlertaResync(datos) {
        if (!bannerResync) return;
        const requerida = Boolean(datos.resincronizacion_fisica_requerida);
        bannerResync.hidden = !requerida;
        if (!requerida) return;
        bannerResync.replaceChildren();
        const titulo = document.createElement("strong");
        titulo.textContent = "Resincronización física requerida";
        const detalle = document.createElement("p");
        detalle.textContent = datos.mensaje || "Se rechazó una trama fuera de la ventana. El contador válido no se modificó. Realice el procedimiento físico de resincronización del llavero.";
        bannerResync.append(titulo, detalle);
    }
    const filterInput = document.querySelector(".filter-bar input");
    const logsTbody = document.getElementById("logs-tbody");

    const esInicio = Boolean(document.getElementById("status-heading"));
    const esLogs = Boolean(logsTbody);

    async function apiFetch(ruta, opciones = {}) {
        const respuesta = await fetch(`${API_BASE}${ruta}`, {
            ...opciones,
            headers: {
                ...(opciones.body ? { "Content-Type": "application/json" } : {}),
                ...opciones.headers
            }
        });

        if (!respuesta.ok) {
            throw new Error(`Error HTTP ${respuesta.status} en ${ruta}`);
        }

        return respuesta.json();
    }

    function actualizarBotonesModulacion(mod) {
        if (!btn2Fsk || !btnOok) return;

        const es2Fsk = mod === "2-FSK";
        btn2Fsk.classList.toggle("active", es2Fsk);
        btnOok.classList.toggle("active", !es2Fsk);

        if (modIndicatorText) {
            modIndicatorText.textContent = mod;
        }
    }

    async function cambiarModulacion(mod) {
        if (!["2-FSK", "ASK/OOK"].includes(mod)) return;

        // No marcar el cambio como confirmado hasta recibir respuesta.
        if (btn2Fsk) btn2Fsk.disabled = true;
        if (btnOok) btnOok.disabled = true;

        try {
            const datos = await apiFetch("/api/modulacion", {
                method: "POST",
                body: JSON.stringify({ modulacion: mod })
            });

            // El firmware deberá responder con la modulación efectivamente
            // aplicada, o con un error si no pudo coordinar el cambio.
            if (datos.ok) {
                actualizarBotonesModulacion(
                    datos.modulacion_actual ||
                    datos.modulacion_deseada ||
                    mod
                );
            } else {
                throw new Error(datos.error || "No se pudo cambiar la modulación");
            }
        } catch (error) {
            console.error("Error al cambiar la modulación:", error);
            alert("No se pudo confirmar el cambio de modulación.");
            actualizarEstado();
        } finally {
            if (btn2Fsk) btn2Fsk.disabled = false;
            if (btnOok) btnOok.disabled = false;
        }
    }

    async function actualizarEstado() {
        try {
            const datos = await apiFetch("/api/estado");

            const heading = document.getElementById("status-heading");
            const desc = document.getElementById("status-desc");
            const contador = document.getElementById("metric-contador");
            const ataques = document.getElementById("metric-ataques");
            const panelGlobal = document.querySelector(".global-status-panel");

            if (heading) {
                heading.textContent =
                    datos.texto_estado ||
                    (datos.sistema_seguro ? "Sistema seguro" : "Alerta de seguridad");
            }

            if (desc) desc.textContent = datos.mensaje || "";
            if (contador) contador.textContent = datos.contador_actual ?? "—";
            if (ataques) ataques.textContent = datos.ataques_rechazados_hoy ?? 0;

            if (datos.modulacion_actual) {
                actualizarBotonesModulacion(datos.modulacion_actual);
            }

            if (panelGlobal) {
                panelGlobal.classList.toggle("alert-mode", !datos.sistema_seguro);
            }
        } catch (error) {
            console.error("No se pudo obtener el estado:", error);
        }
    }

    async function actualizarTramas(endpoint, contenedorId) {
        const tbody = document.getElementById(contenedorId);
        if (!tbody) return;

        try {
            const tramas = await apiFetch(`/api/tramas/${endpoint}`);
            tbody.replaceChildren();

            tramas.forEach((trama) => {
                const fila = document.createElement("tr");
                const aceptada = trama.resultado === "aceptado";

                const valores = [
                    trama.hora ?? "—",
                    trama.trama ?? "—",
                    `${trama.rssi ?? "—"} dBm`
                ];

                valores.forEach((valor, indice) => {
                    const celda = document.createElement("td");
                    celda.textContent = valor;
                    if (indice > 0) celda.classList.add("mono");
                    fila.appendChild(celda);
                });

                const celdaResultado = document.createElement("td");
                const badge = document.createElement("span");
                badge.className = `status-text ${aceptada ? "success" : "danger"}`;
                badge.textContent = aceptada ? "Acceso permitido" : "Trama rechazada";
                celdaResultado.appendChild(badge);
                fila.appendChild(celdaResultado);

                tbody.appendChild(fila);
            });
        } catch (error) {
            console.error(`No se pudieron obtener las tramas ${endpoint}:`, error);
        }
    }

    async function refrescarInicio() {
        await Promise.all([
            actualizarEstado(),
            actualizarTramas("2fsk", "tbody-2fsk"),
            actualizarTramas("ook", "tbody-ook")
        ]);
    }

    function aplicarFiltro() {
        if (!filterInput || !logsTbody) return;

        const texto = filterInput.value.toLowerCase();

        logsTbody.querySelectorAll("tr").forEach((fila) => {
            fila.style.display =
                fila.textContent.toLowerCase().includes(texto) ? "" : "none";
        });
    }

    async function cargarLogs() {
        if (!logsTbody) return;

        try {
            const logs = await apiFetch("/api/logs");
            logsTbody.replaceChildren();

            logs.forEach((log) => {
                const fila = document.createElement("tr");
                const [fecha = "—", hora = "—"] =
                    (log.fecha_hora || "").split(" ");

                const fechaCelda = document.createElement("td");
                const contenedorFecha = document.createElement("div");
                contenedorFecha.className = "datetime-cell";

                const fechaTexto = document.createElement("span");
                fechaTexto.className = "date";
                fechaTexto.textContent = fecha;

                const horaTexto = document.createElement("span");
                horaTexto.className = "time";
                horaTexto.textContent = hora;

                contenedorFecha.append(fechaTexto, horaTexto);
                fechaCelda.appendChild(contenedorFecha);
                fila.appendChild(fechaCelda);

                [
                    ["id_llavero", true],
                    ["contador", true],
                    ["modulacion", false],
                    ["trama", true],
                    ["tipo_ataque", false]
                ].forEach(([campo, monoespaciado]) => {
                    const celda = document.createElement("td");
                    celda.textContent = log[campo] ?? "—";
                    if (monoespaciado) celda.classList.add("mono");
                    fila.appendChild(celda);
                });

                const celdaResultado = document.createElement("td");
                const badge = document.createElement("span");
                const rechazado = log.resultado === "rechazado";

                badge.className = `status-text ${rechazado ? "danger" : "success"}`;
                badge.textContent = rechazado ? "Trama rechazada" : "Acceso permitido";
                celdaResultado.appendChild(badge);
                fila.appendChild(celdaResultado);

                logsTbody.appendChild(fila);
            });

            aplicarFiltro();
        } catch (error) {
            console.error("No se pudieron obtener los logs:", error);
        }
    }

    // Botones de modulación
    if (btn2Fsk) {
        btn2Fsk.addEventListener("click", () => cambiarModulacion("2-FSK"));
    }

    if (btnOok) {
        btnOok.addEventListener("click", () => cambiarModulacion("ASK/OOK"));
    }

    if (filterInput) {
        filterInput.addEventListener("input", aplicarFiltro);
    }

    // Actualizaciones en vivo mediante WebSocket nativo.
    // El firmware deberá implementar este mismo endpoint: /ws
    let socket;

    function conectarWebSocket() {
        const protocolo = window.location.protocol === "https:" ? "wss:" : "ws:";
        socket = new WebSocket(`${protocolo}//${window.location.host}/ws`);

        socket.addEventListener("open", () => {
            console.log("WebSocket conectado");
        });

        socket.addEventListener("message", (evento) => {
            try {
                const datos = JSON.parse(evento.data);

                if (datos.recargar) {
                    if (esInicio) refrescarInicio();
                    if (esLogs) cargarLogs();
                }
            } catch (error) {
                console.error("Mensaje WebSocket inválido:", error);
            }
        });

        socket.addEventListener("error", (error) => {
            console.error("Error de WebSocket:", error);
        });

        socket.addEventListener("close", () => {
            console.log("WebSocket desconectado");
            // Sin WebSocket disponible, la carga inicial sigue funcionando.
            // La reconexión se agregará al integrar el firmware.
        });
    }

    if (esInicio) refrescarInicio();
    if (esLogs) cargarLogs();

    conectarWebSocket();
});

