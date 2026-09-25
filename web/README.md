# Aplicación web RumBa

Interfaz web responsive para operar el robot desde un teléfono (incluido Honor/Android)
o desde cualquier navegador conectado a la misma red que la Raspberry Pi.

## Arquitectura propuesta

```text
Teléfono
      |
      | HTTP
      v
   httpd
  |        |
  |        +--> /www (HTML/CSS/JS)
  |
  +--> /cgi-bin/robot_api.cgi
                 |
                 v
              robotd
                 |
                 v
             librobot.so
      +----------+----------+---------+
      |          |          |         |
   motores    sensores     LEDs      audio
```

`httpd` sirve la interfaz. El CGI funciona como puerta de entrada HTTP y un proceso
persistente (`robotd`) debe ser el propietario del estado del robot y de `librobot.so`.
Esto evita inicializar/cerrar GPIO, PWM, sensores y audio en cada petición CGI.

## Funciones ya contempladas por el frontend

- Inicio de sesión.
- Cambio entre modo autónomo y manual.
- Control direccional táctil con parada al soltar.
- Ajuste de velocidad manual.
- Lectura de los tres sensores de distancia.
- Estado de los cuatro LEDs.
- Lista de MP3, play, pausa, stop y volumen.
- Visualización de la grilla 2D del recorrido.
- Parada de emergencia cuando la pestaña se oculta o se abandona durante movimiento manual.

## Uso desde el teléfono

Cuando la Raspberry y el Honor estén conectados a la misma red Wi-Fi, se accede desde
el navegador con:

```text
http://IP_DE_LA_RASPBERRY/
```

No se necesita una aplicación Android nativa; la interfaz fue diseñada para pantalla
táctil y se adapta a móvil.

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

## Backend pendiente

El siguiente bloque es implementar:

1. `robotd`: proceso persistente que enlaza con `librobot.so` y con los módulos de navegación/mapa.
2. `robot_api.cgi`: CGI para `httpd` que autentica y reenvía órdenes a `robotd`.
3. Sesiones y contraseña almacenada como hash, nunca como texto plano.
4. Servicio `systemd` para iniciar automáticamente `robotd` y `httpd`.
5. Receta Yocto para instalar los binarios y `/www`.

El contrato de las operaciones HTTP está definido en `API.md`.
