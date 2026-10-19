# Firmware del auto

La interfaz web se sirve desde `data/` a través de SPIFFS. Además de cargar el firmware, hay que cargar los archivos del filesystem al ESP32:

```sh
cd Firmware
pio run -e auto -t upload
pio run -e auto -t uploadfs
```

También se puede usar la tarea **PlatformIO: Upload Filesystem Image** para el entorno `auto`. Volvé a cargar el filesystem cada vez que cambien los archivos de `data/`.

Después de conectarte a la red `Auto_Acceso_Seguro`, abrí **http://192.168.4.1/** en el navegador (HTTP, no HTTPS). La IP del punto de acceso también se muestra por Serial al iniciar. Si SPIFFS no contiene `index.html`, el monitor Serial indica que hay que ejecutar `uploadfs`.
