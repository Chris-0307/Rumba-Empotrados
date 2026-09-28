SUMMARY = "Imagen Linux minima a la medida para el robot aspiradora RumBa"
DESCRIPTION = "core-image-minimal + exactamente lo que el robot necesita: \
biblioteca de hardware, decodificador MP3, ALSA, servidor web, systemd y \
conectividad WiFi. Sin entorno grafico."
LICENSE = "MIT"

require recipes-core/images/core-image-minimal.bb

# ssh para desplegar y depurar en el target. Dropbear pesa ~110 KB
# frente a ~1.5 MB de openssh.
IMAGE_FEATURES += "ssh-server-dropbear"

# --------------------------------------------------------------------
# Paquetes del proyecto
# --------------------------------------------------------------------
ROBOT_PKGS = " \
    librobot \
    robot-web \
    "

# --------------------------------------------------------------------
# Audio: decodificador MP3 + control de volumen
# --------------------------------------------------------------------
# mpg123 viene en oe-core (meta/recipes-multimedia/mpg123) y se compila
# con salida ALSA porque "alsa" esta en DISTRO_FEATURES de poky.
# alsa-utils-amixer se necesita para fijar el volumen maestro.
ROBOT_AUDIO_PKGS = " \
    mpg123 \
    alsa-utils-amixer \
    alsa-utils-aplay \
    alsa-conf \
    "

# --------------------------------------------------------------------
# Hardware
# --------------------------------------------------------------------
# libgpiod-tools: gpioinfo / gpioset / gpioget para depurar los LEDs
#                 desde la consola sin recompilar nada.
# i2c-tools:      i2cdetect para verificar que el ADS1115 responde.
ROBOT_HW_PKGS = " \
    libgpiod \
    libgpiod-tools \
    i2c-tools \
    "

# --------------------------------------------------------------------
# Conectividad y medicion
# --------------------------------------------------------------------
# systemd-analyze es la herramienta con la que se reporta el tiempo de
# arranque exigido por la especificacion.
ROBOT_NET_PKGS = " \
    wpa-supplicant \
    "

ROBOT_METRIC_PKGS = " \
    systemd-analyze \
    "

IMAGE_INSTALL:append = " \
    ${ROBOT_PKGS} \
    ${ROBOT_AUDIO_PKGS} \
    ${ROBOT_HW_PKGS} \
    ${ROBOT_NET_PKGS} \
    ${ROBOT_METRIC_PKGS} \
    "

# Dongle de audio USB (ver yocto/README.md: el PWM de los motores choca
# con la salida analogica de 3.5 mm en la Raspberry Pi).
IMAGE_INSTALL:append:rpi = " kernel-module-snd-usb-audio"

# En QEMU no hay tarjeta de sonido real: se usa snd-dummy del bbappend
# del kernel, y gpio-sim / i2c-stub para simular los perifericos.
IMAGE_INSTALL:append:qemuall = " \
    kernel-module-gpio-sim \
    kernel-module-i2c-stub \
    kernel-module-snd-dummy \
    "

# Presupuesto de la especificacion: rootfs <= 200 MB.
# Si la construccion se pasa, bitbake falla aqui en vez de descubrirlo
# al flashear la tarjeta.
IMAGE_ROOTFS_MAXSIZE = "204800"

# Un poco de holgura para logs y MP3 subidos en caliente.
IMAGE_ROOTFS_EXTRA_SPACE = "32768"

# Limpieza: sin gestor de paquetes en el target, sin documentacion.
# Ahorra decenas de MB.
IMAGE_FEATURES:remove = "package-management"
NO_RECOMMENDATIONS = "1"
