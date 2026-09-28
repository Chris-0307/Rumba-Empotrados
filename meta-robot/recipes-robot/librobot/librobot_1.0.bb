SUMMARY = "Biblioteca dinamica de control de hardware del robot RumBa"
DESCRIPTION = "librobot.so encapsula el acceso a motores (PWM sysfs), \
sensores de proximidad (ADS1115 por I2C), LEDs (libgpiod v2) y \
reproduccion de audio MP3 (mpg123 en modo remoto). El servidor web \
del robot usa exclusivamente esta biblioteca para tocar el hardware."
HOMEPAGE = "https://github.com/Chris-0307/Rumba-Empotrados"
SECTION = "libs"

# El repositorio del curso no lleva archivo de licencia todavia.
# Cuando se agregue un LICENSE al repo, cambiar a:
#   LICENSE = "MIT"
#   LIC_FILES_CHKSUM = "file://../LICENSE;md5=<md5 real>"
LICENSE = "CLOSED"

# --------------------------------------------------------------------
# Fuentes
# --------------------------------------------------------------------
# ROBOT_GIT_URI y ROBOT_GIT_REV se definen en conf/local.conf para que
# las dos recetas del proyecto (librobot y robot-web) apunten al mismo
# commit sin duplicar la URL.
ROBOT_GIT_URI ?= "git://github.com/Chris-0307/Rumba-Empotrados.git;protocol=https;branch=develop"
ROBOT_GIT_REV ?= "${AUTOREV}"

SRC_URI = "${ROBOT_GIT_URI}"
SRCREV = "${ROBOT_GIT_REV}"
PV = "1.0+git"

# El CMakeLists.txt de la biblioteca vive en lib/ dentro del repo.
S = "${WORKDIR}/git/lib"

# --------------------------------------------------------------------
# Dependencias
# --------------------------------------------------------------------
# libgpiod: en scarthgap meta-oe trae la 2.1.x, que es la que pide
# el pkg_check_modules(GPIOD REQUIRED "libgpiod>=2.0") del CMakeLists.
DEPENDS = "libgpiod"

# mpg123 se invoca en tiempo de ejecucion (execl de /usr/bin/mpg123 -R)
# desde lib/src/audio.c, asi que es dependencia de RUNTIME, no de build.
RDEPENDS:${PN} += "mpg123"

inherit cmake pkgconfig

# El CMakeLists instala en "lib" e "include" relativos al prefijo,
# que cmake.bbclass fija en ${prefix}=/usr -> /usr/lib y /usr/include.
# No hace falta EXTRA_OECMAKE adicional.

# Deja explicito que este paquete produce una .so versionada.
FILES:${PN} += "${libdir}/librobot.so.*"
