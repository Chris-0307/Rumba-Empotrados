# Aplicación web RumBa

Interfaz web responsive para operar el robot desde una computadora o teléfono
conectado a la misma red que la Raspberry Pi.

## Arquitectura actual

```text
Navegador
   |
   | HTTP / JSON
   v
robot_server_api :8080
   |
   v
robot_adapter
   |
   +--> simulación, si ROBOT_SIMULATE=1
   |
   +--> hardware/controlador, cuando se complete la integración
```

La interfaz ya no usa `/cgi-bin/robot_api.cgi`. El JavaScript llama directamente
las rutas `/api/...` del servidor de la Raspberry.

## Funciones del frontend

- Prueba de conexión con `GET /api/status`.
- Inicio y cierre de sesión con token Bearer.
- Cambio entre modo automático y manual.
- Control direccional y parada al soltar el botón.
- Ajuste de velocidad manual.
- Lectura del sensor frontal.
- Estado de sistema, modo y obstáculo.
- Lista de canciones, play, pausa, siguiente y volumen.
- Visualización de la grilla 2D del recorrido.
- Intento de parada cuando se abandona la página durante movimiento manual.

## Prueba desde una PC

Dentro de `web/www`:

```bash
python -m http.server 5500
```

Abrir:

```text
http://localhost:5500
```

En la Raspberry, para probar sin tocar hardware:

```bash
ROBOT_ALLOWED_ORIGIN=http://localhost:5500 \
ROBOT_SIMULATE=1 \
/home/root/robot_server_api /home/root/robot.db 8080
```

En la pantalla de login se escribe la IP de la Raspberry y el puerto `8080`.
Primero se puede usar el botón **Probar conexión con Raspberry** y luego iniciar
sesión con un usuario ya registrado en `robot.db`.

## Archivos web

```text
web/
├── API.md
├── README.md
└── www/
    ├── index.html
    ├── css/app.css
    └── js/app.js
```

El contrato de rutas que usa el frontend está resumido en `API.md`.

## Nota de integración

Mientras `ROBOT_SIMULATE=1`, sensores, movimiento, mapa y audio usan los datos del
simulador del `robot_adapter`. Cuando se quite la simulación, el adaptador debe
estar conectado a `librobot.so`, navegación, mapa y audio reales antes de activar
los motores.
