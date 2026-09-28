# Sistema operativo a la medida con Yocto — Robot RumBa

Guía de la parte de Yocto del Proyecto I (CE-1113). Cubre desde la
instalación en una máquina limpia hasta las evidencias que pide la
especificación, y está escrita para poder trabajar **sin tener la
Raspberry Pi**.

---

## 0. Qué es Yocto, en corto

Yocto no es una distribución de Linux: es una **fábrica de
distribuciones**. Uno declara qué quiere (paquetes, arquitectura,
configuración) y BitBake compila desde código fuente un toolchain
cruzado, un kernel, un rootfs y una imagen lista para flashear.

Las cuatro piezas que hay que tener claras:

| Pieza | Qué es | Dónde vive |
|---|---|---|
| **poky** | La referencia: BitBake + oe-core (las ~2000 recetas base) | `~/yocto-robot/poky` |
| **Capa** (`meta-*`) | Carpeta con recetas y configuración que se suman al build | `meta-raspberrypi`, `meta-oe`, `meta-robot` |
| **Receta** (`.bb`) | Cómo obtener, compilar e instalar **un** paquete | `meta-robot/recipes-robot/librobot/librobot_1.0.bb` |
| **Imagen** (`.bb`) | Qué paquetes van al rootfs final | `meta-robot/recipes-core/images/robot-image.bb` |

Dos reglas que ahorran horas de confusión:

- **Nunca se editan las capas ajenas.** Para cambiar algo de `busybox`
  (que vive en oe-core) se escribe un `busybox_%.bbappend` en la capa
  propia. Es lo que hace este proyecto para habilitar `httpd`.
- **Todo lo del grupo va en `meta-robot/`**, dentro del repo Git. Las
  capas descargadas (poky, meta-raspberrypi, meta-oe) van **fuera** del
  repo, porque pesan varios GB y son código de terceros.

---

## 1. Requisitos del host

### Sistema operativo

Ubuntu 24.04 LTS funciona y es una de las distribuciones soportadas
oficialmente por scarthgap. No hace falta máquina virtual ni cambiar de
distro.

### Paquetes

Ya instalados en el paso previo. Si hay que repetirlo en otra máquina:

```bash
sudo apt update
sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
  chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
  iputils-ping python3-git python3-jinja2 python3-subunit zstd liblz4-tool \
  file locales libacl1 bmap-tools
sudo locale-gen en_US.UTF-8
sudo apt install -y qemu-system-arm qemu-user-static
```

### `/bin/sh` apuntando a bash (recomendado)

Ubuntu apunta `/bin/sh` a `dash`. Poky suele construir igual, pero las
guías de todos los vendors de scarthgap recomiendan cambiarlo porque
algunas recetas asumen bash y fallan con errores crípticos:

```bash
ls -l /bin/sh                  # si dice -> dash:
sudo dpkg-reconfigure dash     # responder NO
```

Es reversible: volver a ejecutar el comando y responder Sí.

### Disco y tiempo

| Recurso | Necesario |
|---|---|
| Espacio libre | **90–120 GB** (con `rm_work`; sin él, 250 GB+) |
| RAM | 8 GB mínimo, 16 GB cómodo |
| Primera construcción | **2 a 6 horas** según CPU e internet |
| Reconstrucciones | 2 a 15 minutos gracias a sstate-cache |

> Si el disco está justo, apuntar `YOCTO_ROOT` a otra partición. Es la
> causa número uno de builds que mueren a mitad de camino.

---

## 2. Instalación

Desde la raíz del repo, en la rama `feature/yocto`:

```bash
chmod +x yocto/setup-yocto.sh
./yocto/setup-yocto.sh
```

El script:

1. Clona **poky** en el tag `yocto-5.0.19` (la misma versión del
   contexto entregado por el profesor).
2. Clona **meta-raspberrypi** y **meta-openembedded** en la rama
   `scarthgap`, dentro de `poky/`.
3. Genera `build/conf/local.conf` y `build/conf/bblayers.conf` a partir
   de las plantillas versionadas en `meta-robot/conf/templates/robot/`,
   sustituyendo las rutas absolutas de la máquina.
4. Crea `downloads/` y `sstate-cache/` compartidos.

Para instalar en otra ruta o para la máquina emulada:

```bash
YOCTO_ROOT=/mnt/datos/yocto ./yocto/setup-yocto.sh
MACHINE=qemuarm ./yocto/setup-yocto.sh
```

### Verificar que quedó bien

```bash
cd ~/yocto-robot
source poky/oe-init-build-env build
bitbake-layers show-layers
```

Deben aparecer seis capas: `core`, `yocto`, `yoctobsp`, `raspberrypi`,
`openembedded-layer` y **`robot`** (esta última apuntando al repo).

---

## 3. Construir la imagen

```bash
cd ~/yocto-robot
source poky/oe-init-build-env build     # hay que hacerlo en CADA terminal nueva
bitbake robot-image 2>&1 | tee ~/robot-image-build.log
```

Resultado en `tmp/deploy/images/raspberrypi4/`:

```
robot-image-raspberrypi4.rootfs.wic.bz2   <- esto se flashea a la microSD
robot-image-raspberrypi4.rootfs.manifest  <- lista exacta de paquetes instalados
robot-image-raspberrypi4.rootfs.tar.bz2   <- rootfs suelto
```

### Objetivos útiles durante el desarrollo

```bash
bitbake librobot                    # compilar solo la biblioteca (rápido)
bitbake -c cleansstate librobot     # forzar recompilación desde cero
bitbake -c devshell librobot        # shell con el entorno cruzado cargado
bitbake -e robot-image | grep ^IMAGE_INSTALL=   # qué va a quedar en la imagen
bitbake -g robot-image              # grafo de dependencias
```

### Iterar sobre el código sin hacer push cada vez

Las recetas traen las fuentes del repo de GitHub, así que por defecto
hay que hacer `git push` para que BitBake vea un cambio. Para trabajar
contra el árbol local:

```bash
devtool modify librobot ~/yocto-robot/workspace/sources/librobot
# editar, y luego:
bitbake librobot
# al terminar:
devtool reset librobot
```

---

## 4. Probar sin Raspberry Pi

Esta es la parte que permite avanzar hoy. La estrategia: construir la
**misma imagen y las mismas recetas** para una máquina QEMU ARM, y
sustituir los periféricos físicos por simuladores del kernel.

### 4.1 Construir para QEMU

Conviene un segundo directorio de build para no invalidar la caché del
de la Raspberry:

```bash
cd ~/yocto-robot
source poky/oe-init-build-env build-qemu
# editar conf/local.conf:  MACHINE ?= "qemuarm"
bitbake robot-image
```

`qemuarm` es ARM de 32 bits, igual que `raspberrypi4`, así que el
toolchain cruzado y el empaquetado son prácticamente idénticos.

### 4.2 Arrancar

```bash
runqemu qemuarm nographic
```

Login: `root`, sin contraseña (por `debug-tweaks`). Para salir:
`Ctrl-A` y luego `x`.

Con la red en modo tap, el target queda en `192.168.7.2` y el host en
`192.168.7.1`, así que la interfaz web se abre desde el navegador del
host en **http://192.168.7.2/**.

### 4.3 Qué se puede validar en QEMU

| Requisito | ¿Se valida? | Cómo |
|---|---|---|
| Compilación cruzada de `librobot.so` | Sí, completo | `file /usr/lib/librobot.so.1.0.0` → ARM |
| Receta `.bb` propia funciona | Sí, completo | La imagen la incluye |
| Servicio systemd arranca solo | Sí, completo | `systemctl status robot-httpd` |
| Reinicio ante fallos | Sí, completo | `kill -9 $(pidof httpd)` y ver que revive |
| Servidor web sirve la interfaz | Sí, completo | Navegador en `http://192.168.7.2/` |
| Tiempo de arranque | Aproximado | `systemd-analyze` (QEMU es más lento que la Pi) |
| Tamaño del rootfs | Sí, casi exacto | `du -shx /` |
| LEDs por GPIO | Sí, simulado | `gpio-sim` crea `/dev/gpiochip0` real |
| Sensores I2C | Parcial | `i2c-stub` responde, pero con datos falsos |
| Reproducción MP3 | Sí, sin sonido | `snd-dummy` acepta el audio y lo descarta |
| Motores por PWM | **No** | El kernel no trae simulador de PWM |

Para levantar los periféricos virtuales dentro del target:

```bash
scp yocto/scripts/qemu-sim-setup.sh root@192.168.7.2:/tmp/
ssh root@192.168.7.2 'sh /tmp/qemu-sim-setup.sh'
```

Después, probar los LEDs sin escribir una línea de código:

```bash
gpiodetect                       # debe listar gpiochip0 con 54 líneas
gpioget -c gpiochip0 17          # leer GPIO17
gpioset -t0 -c gpiochip0 17=1    # mantener GPIO17 alto (Ctrl-C para salir)
```

> Estas son las herramientas de **libgpiod 2.x**, cuya sintaxis cambió
> respecto a la 1.x que aparece en la mayoría de tutoriales viejos. Ante
> la duda: `gpioset --help`.

Y probar audio:

```bash
mpg123 -a null /srv/robot/audio/prueba.mp3
```

---

## 5. Métricas de eficiencia de recursos

La especificación pide medir tamaño de rootfs, tiempo de arranque y uso
de RAM/CPU, y **documentar con qué herramienta** se obtuvo cada uno.

### En el target

```bash
scp yocto/scripts/metrics.sh root@<ip>:/tmp/
ssh root@<ip> 'sh /tmp/metrics.sh' | tee docs/metricas-target.txt
```

El script usa `du -shx /` para el rootfs, `systemd-analyze` para el
arranque, `free -m` más `/proc/meminfo` para RAM, y `top -b -n 2` para
CPU (la segunda iteración es la válida; la primera reporta promedios
desde el arranque).

Hay que correrlo con el robot **en operación normal**: navegación,
audio y servidor web simultáneos. Medir en reposo infla el resultado a
favor y el profesor lo va a notar.

### En el host

`buildhistory` (ya activado en `local.conf`) genera el desglose por
paquete, que es exactamente lo que se necesita para justificar cada
inclusión:

```bash
cat buildhistory/images/raspberrypi4/glibc/robot-image/installed-package-sizes.txt
cat buildhistory/images/raspberrypi4/glibc/robot-image/image-info.txt
```

Y el manifiesto de la imagen lista todo lo instalado:

```bash
wc -l tmp/deploy/images/raspberrypi4/robot-image-raspberrypi4.rootfs.manifest
```

### Presupuesto

| Métrica | Límite | Dónde se controla |
|---|---|---|
| rootfs | 200 MB | `IMAGE_ROOTFS_MAXSIZE` en `robot-image.bb` — el build **falla** si se pasa |
| Arranque | 15 s | `DISABLE_SPLASH`, `BOOT_DELAY=0` en `local.conf` |

---

## 6. Evidencias que pide la especificación

### Fragmento del log de compilación cruzada

```bash
# Ubicar el log
ls tmp/work/*/librobot/*/temp/log.do_compile

# Guardarlo en el repo
mkdir -p docs/evidencias
cp tmp/work/*/librobot/*/temp/log.do_compile docs/evidencias/librobot-log.do_compile
```

Lo que demuestra la compilación cruzada son las líneas del compilador:
`arm-poky-linux-gnueabi-gcc` con `--sysroot=...`, en vez de un `gcc`
del host. Conviene resaltarlas en el README:

```bash
grep -m5 'arm-poky-linux' docs/evidencias/librobot-log.do_compile
```

### Verificación de la arquitectura del binario

```bash
# En el host
file tmp/work/*/librobot/*/image/usr/lib/librobot.so.1.0.0

# En el target
ssh root@<ip> 'file /usr/lib/librobot.so.1.0.0; ldd /usr/lib/librobot.so.1.0.0'
```

Debe decir `ELF 32-bit LSB shared object, ARM, EABI5`.

### Capturas de ejecución en el target

Sirven tanto de QEMU como de la Raspberry. Mínimo recomendado:
`systemctl status robot-httpd`, `gpiodetect`, la interfaz web abierta en
el navegador, y la salida de `metrics.sh`.

---

## 7. Cuando llegue la Raspberry Pi

### Flashear

```bash
cd tmp/deploy/images/raspberrypi4
bzcat robot-image-raspberrypi4.rootfs.wic.bz2 | sudo dd of=/dev/sdX bs=4M status=progress conv=fsync
```

Más rápido y seguro con `bmaptool`, que ya está instalado:

```bash
sudo bmaptool copy robot-image-raspberrypi4.rootfs.wic.bz2 /dev/sdX
```

> **Verificar `/dev/sdX` con `lsblk` antes de ejecutar.** Un dd al disco
> equivocado borra el sistema del host.

### Configurar WiFi

Antes del primer arranque, montar la partición raíz de la microSD y
crear `/etc/wpa_supplicant.conf`:

```
network={
    ssid="NOMBRE_RED"
    psk="CONTRASENA"
}
```

Luego, ya en la Pi: `systemctl enable wpa_supplicant@wlan0`.

### Consola serie

`ENABLE_UART = "1"` ya está en `local.conf`. Con un adaptador USB-TTL en
los pines 8 (TX), 10 (RX) y 6 (GND):

```bash
sudo picocom -b 115200 /dev/ttyUSB0
```

Esto salva la vida cuando la imagen no arranca y no hay HDMI a mano.

### Verificar los periféricos

```bash
gpiodetect                          # gpiochip0 = pinctrl-bcm2711, 54 líneas
i2cdetect -y 1                      # el ADS1115 debe aparecer en 0x48
ls /sys/class/pwm/pwmchip0          # existe si el overlay pwm-2chan cargó
aplay -l                            # tarjetas de audio detectadas
```

---

## 8. Conflicto de hardware: PWM contra audio analógico

**Esto hay que resolverlo antes de comprar componentes.**

El overlay `pwm-2chan` que expone `/sys/class/pwm/pwmchip0` para los
motores (GPIO18 = canal 0, GPIO19 = canal 1) usa el **mismo periférico
PWM del SoC** que alimenta la salida analógica de 3.5 mm. No se pueden
tener los dos a la vez: o hay PWM por hardware en los motores, o hay
audio por el jack.

Alternativas:

| Opción | Costo | Ventajas | Desventajas |
|---|---|---|---|
| **Dongle USB de audio** | ~$5 | No toca GPIO, `snd-usb-audio` ya está en la imagen, amplificado | Ocupa un puerto USB |
| **DAC I2S** (PCM5102, MAX98357A) | ~$8 | Calidad superior, la especificación lo menciona explícitamente | Usa GPIO18/19/21, que chocan con el PWM elegido → habría que mover los motores a GPIO12/13 |
| **PWM por software** para motores | $0 | Libera el PWM de hardware para el audio | Jitter, consumo de CPU, control de velocidad impreciso |

**Recomendación: dongle USB.** Es lo más barato, no toca ningún GPIO y
ya está soportado en la imagen. Si se elige DAC I2S, hay que cambiar
`hardware_config.h` a GPIO12/13 y el overlay a
`dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4`.

Este análisis sirve directamente para el indicador **DI2** del documento
de Diseño (valoración de alternativas considerando factores técnicos y
económicos).

---

## 9. Estructura de la capa

```
meta-robot/
├── conf/
│   ├── layer.conf                          Declara la capa y sus dependencias
│   └── templates/robot/                    Plantillas versionadas del build
│       ├── local.conf.sample               MACHINE, systemd, overlays, presupuestos
│       ├── bblayers.conf.sample            Lista de capas
│       └── conf-notes.txt                  Mensaje de bienvenida del build
├── recipes-robot/
│   ├── librobot/librobot_1.0.bb            Biblioteca dinámica (CMake + libgpiod)
│   └── robot-web/
│       ├── robot-web_1.0.bb                Frontend + unidad systemd
│       └── files/
│           ├── httpd.conf                  Configuración de busybox httpd
│           └── robot-httpd.service         Restart=on-failure, arranque automático
├── recipes-core/
│   ├── busybox/
│   │   ├── busybox_%.bbappend              Habilita el applet httpd
│   │   └── busybox/httpd.cfg               Fragmento de configuración
│   └── images/robot-image.bb               La imagen del proyecto
└── recipes-kernel/linux/
    ├── linux-yocto_%.bbappend              Solo QEMU: no afecta la Raspberry
    └── linux-yocto/robot-sim.cfg           gpio-sim, i2c-stub, snd-dummy
```

### Por qué busybox httpd y no lighttpd o nginx

`busybox` ya está en cualquier imagen mínima de Yocto. Habilitar su
applet `httpd` con un fragmento de configuración **no agrega ni un solo
paquete nuevo** al rootfs, y soporta CGI, que es justo lo que pide el
contrato en `web/API.md`. lighttpd traería una capa adicional
(`meta-webserver`) y unos 400 KB extra sin beneficio para este caso.

Esa decisión es material directo para la sección de justificación de
paquetes del README de entrega.

---

## 10. Problemas comunes

| Síntoma | Causa y solución |
|---|---|
| `Layer robot is not compatible with the core layer` | `LAYERSERIES_COMPAT_robot` en `layer.conf` no dice `scarthgap` |
| `Nothing PROVIDES 'librobot'` | La capa no está en `bblayers.conf`. Revisar con `bitbake-layers show-layers` |
| `Fetcher failure ... Unable to fetch URL` | `ROBOT_GIT_URI` mal escrita en `local.conf`, o rama inexistente |
| `do_rootfs: Image size exceeds IMAGE_ROOTFS_MAXSIZE` | Se pasó de 200 MB. Revisar `installed-package-sizes.txt` y quitar lo que sobra |
| `ERROR: ... /bin/sh: syntax error` | `/bin/sh` apunta a dash (ver sección 1) |
| `No space left on device` | Disco lleno. `rm -rf tmp/` y reconstruir: `sstate-cache` recupera casi todo |
| El build se detiene sin error | `BB_DISKMON_DIRS` lo paró por disco bajo. Liberar espacio y relanzar |
| `systemctl: command not found` en el target | `INIT_MANAGER = "systemd"` no quedó. Reconstruir imagen desde cero |

### Cuando algo falla en una receta

```bash
bitbake librobot -c compile -f            # forzar y ver el error
cat tmp/work/*/librobot/*/temp/log.do_compile
bitbake -c devshell librobot              # shell con el entorno cruzado
```

---

## 11. Checklist de entrega

Rubros de la especificación que cubre esta parte:

- [ ] Capa `meta-robot/` con `layer.conf` versionada en el repo
- [ ] Al menos una receta `.bb` propia que integre el software del grupo
- [ ] La receta descarga las fuentes, invoca CMake con la toolchain de
      Yocto, instala los artefactos y declara dependencias
- [ ] `bitbake robot-image` reproduce la imagen desde cero, sin pasos
      manuales
- [ ] Fragmento de `log.do_compile` en `docs/evidencias/`
- [ ] Capturas de la ejecución del binario en el target
- [ ] Biblioteca dinámica `.so` compilada de forma cruzada
- [ ] Unidad systemd propia con `Restart=on-failure` y arranque automático
- [ ] Sin interfaz gráfica local en el sistema embebido
- [ ] Reporte de métricas: rootfs, arranque, RAM y CPU, con herramientas
      documentadas
- [ ] Justificación de cada paquete agregado más allá de la imagen base
- [ ] Diagramas de arquitectura de hardware y software
