

# **Taller de Proyecto II - Grupo A1** 

## **Bitácora** 

**Sistema de Acceso Vehicular Seguro (Rolling code con AES-128)** 

### **Grupo de Desarrollo** 

- Gonzalez Seijas, Juan Blas – 03113/7 

- Lumbreras, Valentin – 02684/7 

- Medina, Alan – 01551/9 


#### **Fecha 27/08/2026** 

- Qué se hizo? 

   - Asignación del tema del proyecto por parte de la cátedra: Sistema de Acceso Vehicular Seguro (Rolling Code con AES-128) 

   - Primera reunión con el docente a cargo (Alan) para discutir la idea general del proyecto: Tiempos, requerimientos, materiales, etc 

- Decisión Tomada / Acuerdos 

   - Utilizar cifrado simétrico AES-128 sobre radiofrecuencia y una interfaz web para el monitoreo de eventos 

- Próximo paso 

   - Iniciar la redacción del plan de Proyecto formal y la estructura de requerimientos y funcionalidades del sistema. 


#### **Fecha 30/08/2026** 

- Qué se hizo? 

   - Elaboración y estructuración **parcial** del documento “Plan de Proyecto” siguiendo las pautas e instrucciones de la cátedra. 

   - Definición del circuito y esquema del sistema: Alimentación, circuito y nodos. 

   - Pensar cómo será la primera estructura del software a desarrollar 

- Próximo paso 

   - Finalizar los archivos entregables, avanzar con el video de presentación del Plan de Proyecto, resolver dudas con el docente Alan. 

 #### **Fecha 05/09/2026** 

- Qué se hizo? 

   -Investigación sobre la sincronización de la modulación entre el llavero y el auto. Se evaluó el uso del protocolo ESP-NOW, que permite una comunicación inalámbrica rápida y eficiente entre placas ESP32.

- Decisión tomada 

   -  Se descartó utilizar ESP-NOW. Al operar en la banda de 2.4 GHz, requiere que el microcontrolador esté activo, lo cual es incompatible con el modo Deep Sleep en el que operará el llavero para optimizar su consumo energético.
   -   Además, su implementación interferiría con los objetivos de la cátedra, que exige resolver el enlace operando estrictamente en la banda de 433,92 MHz configurando los transceptores CC1101.

- Próximo paso 

   - Se continuara investigando otras alternativas para lograr que el emisor y receptor puedan sincronizar su modulación de manera automatica.


#### **Fecha 10/09/2026** 

- Qué se hizo? 

   - Investigación acerca de cómo comunicar e implementar el cambio de modulación, el cual se realiza desde la página web y debe actualizarse en los nodos emisor y receptor. 

   - Reunión con el docente a cargo para discutir las posibles soluciones a la comunicación del cambio de modulación. 

- Decisión tomada 

   - Se optó por comunicar el cambio de modulación a los dispositivos emisor y receptor a través de RF ya que es la forma más eficiente. Las opciones contempladas para implementar son: una ventana de escucha ACK luego de la transmisión o una escucha periódica asincrónica con polling. 

- Próximo paso 

   - Analizar las opciones y elegir la más eficiente. 

   - Finalizar el plan de proyecto y realizar el video de presentación.

 #### **Fecha 12/09/2026** 

- Qué se hizo? 

   - Se analizaron las opciones para la implementación del cambio de modulación, teniendo en cuenta factores como la eficiencia energética, la latencia de respuesta y la complejidad de realización.
   - Se realizaron las diapositivas para la presentación del proyecto y se grabó el video correspondiente.
   - Se entregó el plan de proyecto a traves de Ideas.

- Decisión tomada 

   -  Para comunicar el cambio de modulación se eligó la opción de una ventana de escucha ACK en el emisor luego de la transmisión. 

- Próximo paso 

   - Esperar la entrega de los componentes de hardware para comenzar a implementar el proyecto.

 #### **Fecha 19/09/2026** 

- Qué se hizo? 

   - Reunión de consulta y reestructuración del proyecto en base al hardware específico que suministrará la cátedra.

- Decisión tomada 

   -  Se acordó avanzar prioritariamente con el desarrollo del entorno web y la lógica de software aislada mientras se aguarda la entrega de los componentes físicos.

- Próximo paso 

   - Iniciar en un principio con lo que es el desarrollo web y de manera secundaria la estructura de pruebas unitarias.

 #### **Fecha 21/09/2026** 

- Qué se hizo? 

   - Se investigaron y analizaron distintas propuestas para reestructurar el proyecto en base a los materiales asignados por la catedra(2 arduino UNO, 1 esp32, 2 cc1101, 1 servo y 1 fuente alimentación). El principal desafío fue encontrar una solución para implementar el nodo interceptor sin un transceptor cc1101.
   - Documentación: [Análisis de componentes asignados](https://docs.google.com/document/d/149fhd8uxdqXGCKvHql11_JB6_TIA2iZjkLbXKHnm_U8/edit?usp=sharing)

- Decisión tomada 

   - Se decidió utilzar el esp32 para el nodo receptor y los 2 arduinos UNO para los nodos emisor e interceptor.
   - Los 2 transceptores cc1101 se utilizarán en los nodos emisor y receptor.
   - Se encontraron y listaron algunas soluciones para la implementación del nodo interceptor, las cuales se encuentran en este documento [link](https://docs.google.com/document/d/1W8DgCcCBv0kgthI1AKxEsXmWdtwS-Hq3AYD4_JGeEIY/edit?usp=sharing) 

- Próximo paso 

   - Analizar las soluciones para implementar el interceptor y elegir entre ellas.

 #### **Fecha 22/09/2026** 

- Qué se hizo? 
   - Se analizaron y evaluaron distintas tecnologías para el desarrollo de la interfaz, considerando el uso de frameworks como React o Vue.

   - Se maquetaron las vistas principales respetando la estructura de los wireframes, incorporando el panel de estado, métricas y registros de auditoría.

   - Se comenzó con la implementación del frontend de la plataforma web.
 
- Decisión tomada 
   
   - Se decidió utilizar HTML5, CSS3 y JavaScript de forma directa sin implementar frameworks pesados.

   - Esta opción se consideró la más ágil y simple para esta etapa del proyecto, ya que elimina la complejidad de empaquetado.

   - Se acordó que, en caso de disponer de tiempo adicional en el futuro, se evaluará la migración a un framework como React para escalar el proyecto.

- Próximo paso 
   - Comenzar con el desarrollo del backend en Python.

   - Estructurar el servidor local para la recepción de eventos y preparar la comunicación en mediante WebSockets.

<div align="center">
  <img src="./Web/frontend/img/image.png" alt="Captura del panel de inicio">
  <p><em>Figura 1: Vista principal del panel de monitoreo y estado de seguridad.</em></p>
</div>

<div align="center">
  <img src="./Web/frontend/img/image-1.png" alt="Captura del panel de inicio">
  <p><em>Figura 2: Vista de la tabla de registros.</em></p>
</div>

 #### **Fecha 25/09/2026** 
- Qué se hizo?

   -  Implementación completa de la arquitectura del backend local utilizando Python y Flask.

   - Configuración de la base de datos relacional con SQLite para el almacenamiento persistente del historial de eventos,id del llavero, contador, tramas, rssi, contadores y tipos de ataque.

   - Desarrollo de la comunicación bidireccional integrando la librería Flask-SocketIO para conectar el servidor directamente con la interfaz web.

   - Desarrollo del script simulador.py. Este código se encarga de enviar datos crudos (tramas, modulación y la acción del sistema) hacia el backend para testear que la comunicación funcione y ver cómo se llenan las tablas de la interfaz automáticamente.

   - Se rediseñó la estructura de la tabla en la vista de Logs. Se añadieron las columnas "Trama (Hex)" y "Modulación", solucionando la imposibilidad previa de rastrear la trama exacta de un registro pasado.

   - Se rediseñó la estructura de la tabla en la vista de Inicio. Se añadieron las columnas RSSI(dBm) y Resultado para tener mas informacion de esa trama y que paso con ella, si fue aceptada o rechazada.

   - Limpieza de las vistas HTML, borrando todos los datos estáticos de prueba que estaban en los <tbody> para que la página dependa 100% de la información del servidor.

- Decisión tomada

   - Se consideró y evaluó utilizar peticiones HTTP asincrónicas para actualizar los datos de la página web. Sin embargo, se decidió implementar WebSockets porque los cambios visuales en el panel se ven mucho más fluidos. Además, a nivel de red, evita saturar el procesamiento del sistema al eliminar la necesidad de consultar la base de datos constantemente.

   - Se definió un comportamiento visual de alerta en el frontend: cuando el sistema detecta un Replay Attack (inyectado ahora mediante el simulador), la pantalla del dashboard se pone de color rojo para evidenciar la vulneración.

- Próximo paso 
   - Arrancar con el desarrollo del firmware en C++ para el ESP32 receptor usando el entorno PlatformIO.

   - Estructurar la librería mbedtls para el descifrado AES-128 y programar la máquina de estados que va a atajar las cuatro zonas del Rolling Code (Ataque, Tolerancia, Resincronización Lógica y Resincronización Física).

   <div align="center">
   <img src="./Web/frontend/img/pantallaDeInicio.png" alt="Captura del panel de inicio">
   <p><em>Figura 1: Pantalla de inicio con las tablas rediseñadas.</em></p>
   </div>

   <div align="center">
   <img src="./Web/frontend/img/pantallaDeInicioAttackReplay.png" alt="Captura del panel de inicio">
   <p><em>Figura 2: Pantalla de inicio con alerta de replay attack </em></p>
   </div>

   <div align="center">
   <img src="./Web/frontend/img/pantallaDeLogs.png" alt="Captura del panel de logs">
   <p><em>Figura 3: Pantalla de logs con las tablas rediseñadas.</em></p>
   </div>

   <div align="center">
   <img src="./Web/backend/img/servidorWebSocket.png" alt="Captura del servidor">
   <p><em>Figura 4: Servidor en funcionamiento.</em></p>
   </div>

   <div align="center">
   <img src="./Tests/img/simuladorDePruebas.png" alt="Captura del simulador de pruebas">
   <p><em>Figura 5: Simulador de pruebas en funcionamiento.</em></p>
   </div>

#### **Fecha 26/09/2026** 

- Qué se hizo? 

   - En primer lugar se investigaron las distintas plataformas de desarrollo para microcontroladores ESP8266, ESP32 y Arduino (PlatformIO y ArduinoIDE).

   - Luego se desarrollaron los códigos base y las configuraciones iniciales tanto para el nodo emisor (node_emisor) como para el nodo receptor (node_receptor).

   - Se puedo testear y verificar con éxito la compilación del entorno con los códigos iniciales, asegurando el empaquetado exacto de la trama de 16 bytes y las librerías criptográficas, aunque aún resta probar su funcionamiento directo sobre HW físico.

   - Se estructuró e integró la carpeta de firmware dentro del repositorio, creando una rama de trabajo aislada llamada 'Prueba' para resguardar la rama main. Para asi poder probar y testear código de manera independiente.
   
- Decisión tomada 

   - Se optó por PlatformIO ya que resulta ser mucho más flexible que el IDE tradicional de Arduino para manejar múltiples arquitecturas en un mismo workspace.
   - Se optó por desarrollar el código base estructurando los entornos por separado y utilizando un diseño modular para garantizar la sincronización de las comunicaciones y la gestión de ventanas de códigos rodantes.

- Problemas encontrados y solución
   - Incompatibilidad con los includes y headers de la librería CC1101: Inicialmente se pensaba que la librería del transceptor podía manejarse con la inclusión genérica estándar, pero al revisar el sitio oficial de platformIO (https://registry.platformio.org/libraries/lsatan/SmartRC-CC1101-Driver-Lib), se constató que requiere una directiva y un archivo de compatibilidad específicos.
      -  Solución: Se ajustaron los includes en el código utilizando la estructura requerida por el driver oficial:
         - #include <SmartRC_CC1101.h>
         - #include "ELECHOUSE_CC1101_SRC_DRV.h>

- Próximo paso 

   - Recibir los componentes físicos del kit
   - Ensamblar la arquitectura
   - Probar el código inicial compilado directamente en el HW
   - Corregir posibles errores de integración y continuar con el desarrollo y calibración de la lógica de radiofrecuencia ya contando con los nodos.
