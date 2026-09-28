# Este bbappend SOLO afecta a las maquinas QEMU (qemuarm, qemuarm64,
# qemux86-64), que son las que usan linux-yocto. La Raspberry Pi usa
# linux-raspberrypi y no se ve afectada.
#
# Agrega gpio-sim, i2c-stub y una tarjeta de sonido falsa para poder
# probar librobot.so en emulacion, sin placa fisica.

FILESEXTRAPATHS:prepend := "${THISDIR}/linux-yocto:"

SRC_URI:append:qemuall = " file://robot-sim.cfg"
