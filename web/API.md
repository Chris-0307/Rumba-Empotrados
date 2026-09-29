# Contrato HTTP de RumBa

El frontend usa un único CGI: `/cgi-bin/robot_api.cgi`.

Las respuestas son JSON y las operaciones que cambian estado usan `POST` con
`application/x-www-form-urlencoded`.

## Autenticación

### POST `action=login`
Campos: `username`, `password`.

Respuesta exitosa:

```json
{"ok":true}
```

El CGI debe crear una cookie de sesión `HttpOnly` y `SameSite=Strict`.

### POST `action=logout`
Invalida la sesión.

Todas las demás operaciones deben responder `401` cuando no exista una sesión válida.

## Estado

### GET `?action=state`

```json
{
  "ok": true,
  "mode": "manual",
  "sensors": {"front": 45.2, "left": 78.3, "right": 61.0},
  "leds": {"power": true, "autonomous": false, "manual": true, "obstacle": false},
  "audio": {"state": "playing", "file": "music.mp3", "volume": 75}
}
```

## Modo

### POST `action=set_mode`
Campo `mode`: `manual` o `autonomous`.

## Movimiento manual

### POST `action=move`
Campos:
- `direction`: `forward`, `backward`, `left`, `right`
- `speed`: `20..100`

### POST `action=stop`
Detiene ambos motores inmediatamente.

El backend debe implementar además un *dead-man timeout*: si no recibe una
renovación/orden válida en un intervalo corto mientras está en movimiento manual,
debe detener los motores aunque el navegador pierda conexión.

## Audio

### GET `?action=audio_list`

```json
{"ok":true,"songs":["song1.mp3","song2.mp3"]}
```

### POST `action=audio_play`
Campo `file`.

### POST `action=audio_pause`

### POST `action=audio_stop`

### POST `action=audio_volume`
Campo `volume`: `0..100`.

## Mapa

### GET `?action=map`

```json
{
  "ok": true,
  "width": 20,
  "height": 20,
  "cells": ["unknown", "visited", "obstacle"],
  "robot": {"x": 10, "y": 8, "heading": "N"}
}
```

`cells` debe contener `width * height` entradas en orden por filas.
