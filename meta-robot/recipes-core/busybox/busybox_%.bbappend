# Habilita el applet httpd de busybox, que no viene activo en el
# defconfig de oe-core. Asi el servidor web no agrega ningun paquete
# nuevo a la imagen: reutiliza el binario de busybox que ya esta ahi.

FILESEXTRAPATHS:prepend := "${THISDIR}/busybox:"

SRC_URI += "file://httpd.cfg"
