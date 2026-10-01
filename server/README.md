# Robot API para Raspberry Pi 4

Servidor HTTP en C para la interfaz del robot. Usa SQLite para usuarios, sesiones y catálogo de canciones; OpenSSL para derivar contraseñas con scrypt y generar tokens aleatorios. El servidor **se ejecuta directamente** en el puerto 8080: no se instala dentro de BusyBox `httpd` ni de Apache. Si ya hay un proceso en 8080, detenlo o elige otro puerto.

## Estado de la integración

El recorrido es HTTP → server.c → robot_adapter.c → control_logic.c → librobot.
`control_logic.c` es la única fuente del modo; no contiene GPIO ni depende de libgpiod directamente.

- Al iniciar: inicializa librobot, detiene motores y selecciona manual (LED manual encendido y automático apagado).
- Al cambiar modo: detiene primero, apaga ambos LEDs y enciende el seleccionado. Solo confirma el modo si las llamadas funcionan.
- Automático selecciona el modo y su LED, y rechaza movimientos manuales con HTTP 409. No ejecuta navegación todavía. Stop funciona en ambos modos.
- Un fallo al parar, cambiar LEDs o mover deja el modo `unknown`; se bloquea movimiento hasta seleccionar un modo correctamente. Un fallo puede impedir apagar físicamente un LED: la API no confirma el cambio.
- SIGINT/SIGTERM solicita salida, parada y liberación de librobot. El servidor sigue siendo secuencial; una petición en curso puede retrasar la salida. No hay watchdog de movimiento ni parada por desconexión del cliente.
- Sensores, mapa y audio mantienen la implementación anterior: simulados con ROBOT_SIMULATE=1 y no disponibles en hardware. /api/indicators puede informar system:error aunque el cambio de LEDs funcione.
- Las velocidades de la API (0–100) se pasan sin conversión a librobot. El encabezado no documenta la escala: verificar en la implementación de librobot antes de probar motores.

## Compilar en la PC para simulación

En una terminal SIN activar el SDK, con CMake y paquetes de desarrollo OpenSSL y SQLite:

```bash
cmake -S . -B build-host -DROBOT_WITH_HARDWARE=OFF
cmake --build build-host
ROBOT_SIMULATE=1 ./build-host/robot_server ./robot.db 8080
```

Esta compilación exige ROBOT_SIMULATE=1 al ejecutar. No enlaza librobot.

## Compilar para Raspberry con el SDK

En una terminal nueva:

```bash
source /opt/poky/5.0.19-rumba/environment-setup-cortexa7t2hf-neon-vfpv4-poky-linux-gnueabi
cd ~/proyectos/Empotrados/robot-api
cmake -S . -B build-arm-control -DROBOT_WITH_HARDWARE=ON -DCMAKE_TOOLCHAIN_FILE="$OE_CMAKE_TOOLCHAIN_FILE"
cmake --build build-arm-control
file build-arm-control/robot_server
$READELF -d build-arm-control/robot_server | grep NEEDED
scp build-arm-control/robot_server root@IP_RASPBERRY:/home/root/
```

Usar un directorio nuevo para no reutilizar el cache de CMake incluido en el ZIP original. Ajustar la ruta del SDK si fue instalado en otro lugar. Debe aparecer `librobot.so.1` entre las dependencias.
El SDK necesita librobot-dev, sqlite3-dev y openssl-dev, y la imagen requiere sus bibliotecas de ejecución.
Si OE_CMAKE_TOOLCHAIN_FILE no está definido, compilar directamente después de activar el SDK:

```bash
$CC $CFLAGS -std=c11 -Wall -Wextra -Wpedantic -DROBOT_WITH_HARDWARE=1 -Iinclude src/server.c src/robot_adapter.c src/control_logic.c -o robot_server $LDFLAGS -lrobot -lsqlite3 -lcrypto
```

En la Raspberry, con la versión correspondiente de librobot instalada:

```bash
unset ROBOT_SIMULATE
/home/root/robot_server /home/root/robot.db 8080
```

El proceso debe tener acceso al hardware. Si falla la inicialización, imprime el error y sale. Ctrl+C solicita parada y liberación de recursos.

## Probar el cambio de modo desde la PC

Primero registrar/iniciar sesión como se describe abajo y copiar el token:

```bash
API=http://IP_RASPBERRY:8080
TOKEN=TOKEN_DEL_LOGIN
curl -i -X PUT "$API/api/mode" -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d '{"mode":"automatic"}'
curl -i -H "Authorization: Bearer $TOKEN" "$API/api/mode"
curl -i -X POST "$API/api/move" -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d '{"direction":"forward","speed":30}'
curl -i -X POST "$API/api/move" -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d '{"direction":"stop","speed":0}'
curl -i -X PUT "$API/api/mode" -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d '{"mode":"manual"}'
```

Esperado: automático enciende su LED; movimiento forward devuelve 409; stop devuelve 200; manual intercambia los LEDs.

## Validación disponible

Se probó control_logic con un doble de librobot: modo inicial, parada antes del cambio, LEDs, bloqueo manual, stop, argumentos inválidos, fallos de hardware y simulación sin llamadas al hardware. También se comprobó su sintaxis contra el robot_hw.h proporcionado. No se compiló el servidor completo ni se probó hardware ARM en el entorno de preparación (faltan CMake y cabeceras SQLite).
Para repetir las pruebas en la PC, ajustar la ruta del encabezado original:

```bash
cc -std=c11 -Wall -Wextra -Wpedantic -DROBOT_WITH_HARDWARE=1 -Iinclude -I/RUTA/A/HEADER_LIBROBOT src/control_logic.c tests/control_logic_test.c -o /tmp/control_logic_test
/tmp/control_logic_test
```

## Contrato de la API

Salvo registro, login y status, todas las rutas requieren `Authorization: Bearer TOKEN`. Enviar JSON con `Content-Type: application/json`.

| Sección | Método y ruta | Cuerpo / respuesta principal |
| --- | --- | --- |
| Conectividad | `GET /api/status` | `{"status":"ok"}` (solo confirma servidor) |
| Registro | `POST /api/auth/register` | `{"username":"felipe","password":"clave-de-12+"}` |
| Login | `POST /api/auth/login` | mismas credenciales → `{"token":"...","expires_in":86400}` |
| Perfil | `GET /api/profile` | `{"id":1,"username":"felipe"}` |
| Salir | `POST /api/auth/logout` | invalida el token actual |
| Modo | `GET /api/mode` | `{"mode":"manual"}` |
| Modo | `PUT /api/mode` | `{"mode":"manual"}` o `{"mode":"automatic"}` |
| Sensores | `GET /api/sensors` | `front_cm`, `left_cm`, `right_cm` |
| Indicadores | `GET /api/indicators` | `system`, `mode`, `obstacle` (`true`, `false` o `null`) |
| Mover/parar | `POST /api/move` | `{"direction":"forward","speed":40}`. Direcciones: `forward`, `backward`, `left`, `right`, `stop`; para `stop` envía `speed:0` |
| Mapa | `GET /api/map` | `width`, `height`, `robot:{x,y,direction}`, `cells` con `unknown`, `visited`, `obstacle` |
| Canciones | `GET /api/audio/songs` | array de `{id,title}` |
| Audio | `GET /api/audio/state` | `state`, `song_id`, `volume` |
| Reproducir | `POST /api/audio/play` | `{"song_id":1}` |
| Pausar | `POST /api/audio/pause` | `{}` |
| Siguiente | `POST /api/audio/next` | `{}`; vuelve al primer tema al terminar la lista |
| Volumen | `PUT /api/audio/volume` | `{"volume":50}` (0–100) |

`robot` es una **capa visual superpuesta** a la matriz; no sustituye el valor `visited` o `obstacle` de la celda. El umbral provisional de `obstacle` es `<20 cm` en cualquiera de los tres sensores; configúrenlo con su lógica de navegación antes de usarlo como condición de movimiento.

## Prueba desde otra PC

```bash
API=http://IP_RASPBERRY:8080
curl -i "$API/api/status"
curl -i -X POST "$API/api/auth/register" -H 'Content-Type: application/json' -d '{"username":"felipe","password":"una-clave-larga-123"}'
curl -i -X POST "$API/api/auth/login" -H 'Content-Type: application/json' -d '{"username":"felipe","password":"una-clave-larga-123"}'
```

Copia el `token` devuelto por login y ejecuta:

```bash
TOKEN=PEGA_AQUI_EL_TOKEN
curl -i -H "Authorization: Bearer $TOKEN" "$API/api/sensors"
curl -i -H "Authorization: Bearer $TOKEN" "$API/api/map"
curl -i -X POST "$API/api/move" -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/json' -d '{"direction":"stop","speed":0}'
```

La tabla `songs` comienza vacía. Para insertar un catálogo de prueba en la PC, con el servidor detenido:

```bash
python3 - <<'PY'
import sqlite3
with sqlite3.connect('robot.db') as db:
    db.execute('INSERT INTO songs(title,file_path) VALUES (?,?)', ('Canción de prueba', '/home/root/music/prueba.mp3'))
PY
```

El `file_path` no se muestra al cliente; el adaptador de audio real deberá buscar la ruta por `song_id` antes de reproducir. Esta implementación de ejemplo solo pasa el ID.

## Conectar el hardware

Edita únicamente `src/robot_adapter.c` y enlaza las bibliotecas del proyecto en `CMakeLists.txt`. Confirma nombres y firmas reales en sus archivos `.h` antes de hacerlo:

- `robot_adapter_sensors`: leer tres distancias en cm; devolver `0` solo si las lecturas son válidas.
- `robot_adapter_move`: traducir dirección y velocidad a la API del controlador; `stop` debe llevar velocidad cero. Acordar semántica de pulsación continua y tiempo máximo sin comandos antes de activar motores.
- `robot_adapter_mode`: arrancar/detener el proceso automático con transición segura.
- `robot_adapter_audio`: resolver el ID en SQLite, abrir/reproducir/pausar y ajustar volumen en el reproductor.
- `robot_adapter_map`: serializar el estado real del mapa con dimensiones y coordenadas coherentes.

Si la interfaz web está en otro origen (por ejemplo, `http://IP_RASPBERRY:80` mientras la API escucha en `:8080`), configura el origen exacto antes de iniciar: `ROBOT_ALLOWED_ORIGIN=http://IP_RASPBERRY ROBOT_SIMULATE=1 ./robot_server robot.db 8080`. Admite un solo origen y responde la solicitud CORS `OPTIONS`. Si sirves interfaz y API mediante un proxy en el mismo origen, no necesitas esta variable.

El servidor es secuencial, con límite de tamaño y espera de 5 s por cliente; suficiente para la primera integración en red local. No incluye TLS, límite de intentos de login ni supervisión de movimientos. Mantén la API en una red confiable durante las pruebas y configura HTTPS y controles de acceso antes de exponerla a Internet. En la integración real, un controlador debe imponer parada por pérdida de comunicación; el servidor HTTP por sí solo no la garantiza.
