#!/bin/sh
#
# qemu-sim-setup.sh - Crea perifericos virtuales en el target emulado
#                     para poder probar librobot.so sin Raspberry Pi.
#
# Se ejecuta EN EL TARGET QEMU:
#
#   scp yocto/scripts/qemu-sim-setup.sh root@192.168.7.2:/tmp/
#   ssh root@192.168.7.2 'sh /tmp/qemu-sim-setup.sh'
#
# Crea:
#   /dev/gpiochip0  -> 54 lineas virtuales (gpio-sim), suficiente para
#                      los GPIO 5,6,16,17,18,19,20,22,23,27 del robot
#   /dev/i2c-0      -> bus I2C emulado con un ADS1115 falso en 0x48
#   tarjeta ALSA    -> snd-dummy, para que mpg123 tenga donde escribir
#
set -u

ok()   { printf '  [ok]   %s\n' "$*"; }
fail() { printf '  [FALLA] %s\n' "$*"; }

echo "=== Perifericos virtuales para pruebas sin hardware ==="

# ------------------------------------------------------------------
# GPIO virtual (gpio-sim)
# ------------------------------------------------------------------
echo
echo "GPIO:"
if ! modprobe gpio-sim 2>/dev/null; then
    fail "no se pudo cargar gpio-sim"
    fail "revisar CONFIG_GPIO_SIM=m en meta-robot/.../robot-sim.cfg"
else
    ok "modulo gpio-sim cargado"

    CFG=/sys/kernel/config/gpio-sim/robot

    # configfs lo monta systemd; si no, montarlo a mano.
    if [ ! -d /sys/kernel/config ]; then
        mount -t configfs none /sys/kernel/config 2>/dev/null
    fi

    if [ -d "$CFG" ]; then
        echo 0 > "$CFG/live" 2>/dev/null
        ok "chip previo desactivado"
    fi

    mkdir -p "$CFG/bank0"
    # La Raspberry Pi 4 expone 54 lineas en gpiochip0; se replica el
    # mismo numero para que los indices BCM de hardware_config.h caigan
    # en la misma posicion.
    echo 54 > "$CFG/bank0/num_lines"
    echo 1  > "$CFG/live"

    if [ -e /dev/gpiochip0 ]; then
        ok "/dev/gpiochip0 creado"
    else
        fail "/dev/gpiochip0 no aparecio"
    fi
fi

# ------------------------------------------------------------------
# I2C virtual (i2c-stub) para el ADS1115
# ------------------------------------------------------------------
echo
echo "I2C:"
if modprobe i2c-stub chip_addr=0x48 2>/dev/null; then
    ok "i2c-stub cargado con dispositivo falso en 0x48"
    modprobe i2c-dev 2>/dev/null && ok "i2c-dev cargado"
else
    fail "no se pudo cargar i2c-stub (CONFIG_I2C_STUB=m)"
fi

# hardware_config.h apunta a /dev/i2c-1 (el bus de la Raspberry).
# En QEMU el stub aparece como /dev/i2c-0, asi que se crea un alias.
if [ -e /dev/i2c-0 ] && [ ! -e /dev/i2c-1 ]; then
    ln -sf /dev/i2c-0 /dev/i2c-1
    ok "/dev/i2c-1 enlazado a /dev/i2c-0 (alias para hardware_config.h)"
fi

# ------------------------------------------------------------------
# Audio virtual (snd-dummy)
# ------------------------------------------------------------------
echo
echo "AUDIO:"
if modprobe snd-dummy 2>/dev/null; then
    ok "snd-dummy cargado"
    aplay -l 2>/dev/null | head -5
else
    fail "no se pudo cargar snd-dummy (CONFIG_SND_DUMMY=m)"
fi

# ------------------------------------------------------------------
# PWM: no hay equivalente virtual en el kernel mainline
# ------------------------------------------------------------------
echo
echo "PWM:"
echo "  El kernel no trae un simulador de PWM, asi que"
echo "  /sys/class/pwm/pwmchip0 no existe en QEMU."
echo "  robot_set_motor_speeds() va a devolver ROBOT_ERROR: es lo"
echo "  esperado. La prueba util aqui es que la biblioteca falle"
echo "  de forma controlada y no se caiga con un segfault."
echo "  Los motores se validan sobre la Raspberry real."

# ------------------------------------------------------------------
# Verificacion rapida
# ------------------------------------------------------------------
echo
echo "=== Verificacion ==="
if command -v gpiodetect >/dev/null 2>&1; then
    echo
    echo "[gpiodetect]"
    gpiodetect
    echo
    echo "[gpioinfo: primeras lineas del chip]"
    gpioinfo gpiochip0 2>/dev/null | head -12
fi

if command -v i2cdetect >/dev/null 2>&1; then
    echo
    echo "[i2cdetect: deberia mostrar 0x48]"
    i2cdetect -y 0 2>/dev/null
fi

echo
echo "Listo. Ahora se puede probar la biblioteca, por ejemplo:"
echo "  gpioget -c gpiochip0 17           # leer GPIO17"
echo "  gpioset -t0 -c gpiochip0 17=1     # mantener GPIO17 en alto (Ctrl-C sale)"
echo
echo "Nota: estas son las herramientas de libgpiod 2.x, cuya sintaxis"
echo "cambio respecto a la 1.x. Si algo no calza, consultar:"
echo "  gpioset --help ; gpioget --help"
