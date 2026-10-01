# Contrato HTTP usado por la interfaz RumBa

La interfaz ya no usa CGI. Se conecta directamente al servidor `robot_server_api`,
que normalmente escucha en el puerto `8080` de la Raspberry Pi.

Durante desarrollo la web puede servirse desde una PC, por ejemplo con:

```bash
python -m http.server 5500
```

En ese caso el servidor de la Raspberry debe permitir ese origen con
`ROBOT_ALLOWED_ORIGIN`, por ejemplo:

```bash
ROBOT_ALLOWED_ORIGIN=http://localhost:5500 ROBOT_SIMULATE=1 \
/home/root/robot_server_api /home/root/robot.db 8080
```

Todas las rutas, excepto `status`, `register` y `login`, requieren:

```text
Authorization: Bearer TOKEN
```

Las operaciones con cuerpo usan JSON.

## Conectividad

### GET `/api/status`

```json
{"status":"ok"}
```

## Autenticación

### POST `/api/auth/register`

```json
{"username":"prueba","password":"una-clave-larga-123"}
```

### POST `/api/auth/login`

```json
{"username":"prueba","password":"una-clave-larga-123"}
```

Respuesta:

```json
{"token":"...","expires_in":86400}
```

### POST `/api/auth/logout`

Invalida el token actual.

## Modo

### GET `/api/mode`

```json
{"mode":"manual"}
```

### PUT `/api/mode`

```json
{"mode":"manual"}
```

o:

```json
{"mode":"automatic"}
```

## Sensores e indicadores

### GET `/api/sensors`

```json
{"front_cm":48.0,"left_cm":72.0,"right_cm":35.0}
```

La interfaz actual muestra la lectura frontal del HC-SR04.

### GET `/api/indicators`

```json
{"system":"functional","mode":"manual","obstacle":false}
```

## Movimiento

### POST `/api/move`

```json
{"direction":"forward","speed":40}
```

Direcciones permitidas:

- `forward`
- `backward`
- `left`
- `right`
- `stop`

Para detener:

```json
{"direction":"stop","speed":0}
```

Los movimientos distintos de `stop` requieren que el modo sea `manual`.

## Mapa

### GET `/api/map`

```json
{
  "width":4,
  "height":3,
  "robot":{"x":1,"y":1,"direction":"north"},
  "cells":[
    ["unknown","unknown","unknown","unknown"],
    ["visited","visited","obstacle","unknown"],
    ["unknown","unknown","unknown","unknown"]
  ]
}
```

## Audio

### GET `/api/audio/songs`

```json
[
  {"id":1,"title":"Cancion de prueba"}
]
```

### GET `/api/audio/state`

```json
{"state":"playing","song_id":1,"volume":75}
```

### POST `/api/audio/play`

```json
{"song_id":1}
```

### POST `/api/audio/pause`

```json
{}
```

### POST `/api/audio/next`

```json
{}
```

### PUT `/api/audio/volume`

```json
{"volume":75}
```

Actualmente el servidor no tiene una ruta `/api/audio/stop`, por eso la interfaz
usa los botones Reproducir, Pausar y Siguiente.
