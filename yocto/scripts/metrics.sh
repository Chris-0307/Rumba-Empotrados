#!/bin/sh
#
# metrics.sh - Recolecta las metricas de eficiencia de recursos que exige
#              la especificacion del Proyecto I (seccion "Eficiencia de
#              recursos").
#
# Se ejecuta EN EL TARGET (Raspberry Pi o QEMU), no en el host:
#
#   scp yocto/scripts/metrics.sh root@<ip>:/tmp/
#   ssh root@<ip> 'sh /tmp/metrics.sh' | tee docs/metricas-target.txt
#
# Para que las cifras sean representativas, corerlo con el robot en
# operacion normal: navegacion autonoma + audio + servidor web activos.
#
set -u

line() { echo "------------------------------------------------------------"; }

echo "============================================================"
echo " Metricas del robot RumBa"
echo " Fecha  : $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo " Kernel : $(uname -srm)"
echo "============================================================"

# ------------------------------------------------------------------
# 1. Tamano del sistema de archivos raiz
#    Presupuesto de la especificacion: <= 200 MB
# ------------------------------------------------------------------
line
echo "1. TAMANO DEL ROOTFS"
line
echo "[df: espacio ocupado en la particion raiz]"
df -h / 2>/dev/null

echo
echo "[du: suma real de archivos, sin cruzar a /proc /sys /dev]"
du -shx / 2>/dev/null

echo
echo "[10 directorios mas pesados]"
du -shx /* 2>/dev/null | sort -rh | head -10

# ------------------------------------------------------------------
# 2. Tiempo de arranque
#    Presupuesto de la especificacion: <= 15 s hasta el servicio
#    de control operativo.
# ------------------------------------------------------------------
line
echo "2. TIEMPO DE ARRANQUE"
line
if command -v systemd-analyze >/dev/null 2>&1; then
    echo "[total: firmware + bootloader + kernel + userspace]"
    systemd-analyze time 2>/dev/null

    echo
    echo "[unidades mas lentas]"
    systemd-analyze blame 2>/dev/null | head -15

    echo
    echo "[cadena critica hasta el servidor web del robot]"
    systemd-analyze critical-chain robot-httpd.service 2>/dev/null

    echo
    echo "[instante exacto en que el servicio quedo activo]"
    systemctl show robot-httpd.service \
        -p ActiveEnterTimestampMonotonic \
        -p ActiveState -p SubState 2>/dev/null
else
    echo "systemd-analyze no esta instalado."
    echo "Agregar 'systemd-analyze' a IMAGE_INSTALL en robot-image.bb."
fi

echo
echo "[uptime del kernel al momento de medir]"
cat /proc/uptime

# ------------------------------------------------------------------
# 3. Memoria RAM
# ------------------------------------------------------------------
line
echo "3. USO DE MEMORIA"
line
free -m 2>/dev/null || head -5 /proc/meminfo

echo
echo "[detalle de /proc/meminfo]"
grep -E 'MemTotal|MemFree|MemAvailable|Buffers|^Cached|Shmem' /proc/meminfo

echo
echo "[5 procesos con mas RSS]"
ps -eo rss,comm 2>/dev/null | sort -rn | head -6 \
    || ps -o rss,comm 2>/dev/null | sort -rn | head -6

# ------------------------------------------------------------------
# 4. CPU
# ------------------------------------------------------------------
line
echo "4. USO DE CPU"
line
echo "[carga promedio 1/5/15 min]"
cat /proc/loadavg

echo
echo "[muestra de top: la segunda iteracion es la valida,"
echo " la primera siempre reporta promedios desde el arranque]"
top -b -n 2 -d 2 2>/dev/null | tail -20

if command -v systemd-cgtop >/dev/null 2>&1; then
    echo
    echo "[consumo por unidad systemd]"
    systemd-cgtop -b -n 1 2>/dev/null | head -12
fi

# ------------------------------------------------------------------
# 5. Estado de los servicios del proyecto
# ------------------------------------------------------------------
line
echo "5. SERVICIOS DEL PROYECTO"
line
for svc in robot-httpd.service robotd.service; do
    printf '%-24s %s\n' "$svc" \
        "$(systemctl is-active "$svc" 2>/dev/null || echo 'no instalado')"
done

line
echo "Fin. Copiar esta salida a docs/metricas-target.txt para el README."
line
