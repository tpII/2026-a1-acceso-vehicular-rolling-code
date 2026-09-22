document.addEventListener("DOMContentLoaded", () => {
    
    // ==========================================
    // LÓGICA PARA LA VISTA: INICIO (index.html)
    // ==========================================
    const btn2Fsk = document.getElementById("btn-2fsk");
    const btnOok = document.getElementById("btn-ook");
    const modIndicatorText = document.getElementById("current-mod-text");
    
    if (btn2Fsk && btnOok) {
        btn2Fsk.addEventListener("click", () => {
            btnOok.classList.remove("active");
            btn2Fsk.classList.add("active");
            if (modIndicatorText) modIndicatorText.textContent = "2-FSK";
        });

        btnOok.addEventListener("click", () => {
            btn2Fsk.classList.remove("active");
            btnOok.classList.add("active");
            if (modIndicatorText) modIndicatorText.textContent = "ASK/OOK";
        });
    }
	
	
    // ==========================================
    // LÓGICA PARA LA VISTA: LOGS (logs.html)
    // ==========================================
    const filterInput = document.querySelector(".filter-bar input");
    const tableRows = document.querySelectorAll(".logs-table tbody tr");

    if (filterInput && tableRows.length > 0) {
        filterInput.addEventListener("input", (e) => {
            const textoBusqueda = e.target.value.toLowerCase();

            tableRows.forEach(row => {
                // Captura todo el texto dentro de la fila actual
                const contenidoFila = row.textContent.toLowerCase();
                
                // Si el texto de la fila incluye lo que se buscó, se muestra. Si no, se oculta.
                if (contenidoFila.includes(textoBusqueda)) {
                    row.style.display = "";
                } else {
                    row.style.display = "none";
                }
            });
        });
    }

});