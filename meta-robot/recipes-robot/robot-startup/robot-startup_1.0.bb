SUMMARY = "Servicios systemd, red Wi-Fi y comprobacion de arranque RumBa"
LICENSE = "CLOSED"
SRC_URI = "file://CMakeLists.txt file://robot-hw-check.c \
           file://robot.service file://robot-wifi.service file://robot-preflight file://robot-wifi-unblock \
           file://robot.env file://10-robot-wlan0.network file://wpa_supplicant.conf"
S = "${WORKDIR}"
inherit cmake systemd
DEPENDS = "librobot"
RDEPENDS:${PN} += "robot-server librobot systemd wpa-supplicant rfkill iproute2 alsa-utils-amixer mpg123 alsa-utils-aplay"
EXTRA_OECMAKE += "-DROBOT_INCLUDE_DIR=${STAGING_INCDIR} -DROBOT_LIBRARY=${STAGING_LIBDIR}/librobot.so"
SYSTEMD_SERVICE:${PN} = "robot-wifi.service robot.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
CONFFILES:${PN} += "${sysconfdir}/robot/robot.env ${sysconfdir}/robot/wpa_supplicant.conf"
do_install:append() {
    if grep -q 'REEMPLAZAR_' ${WORKDIR}/wpa_supplicant.conf; then
        bbfatal "Reemplazar wpa_supplicant.conf de ejemplo con el archivo funcional antes de generar imagen"
    fi
    install -d ${D}${systemd_system_unitdir} ${D}${sysconfdir}/robot \
               ${D}${sysconfdir}/systemd/network ${D}${libexecdir}/robot \
               ${D}${sysconfdir}/systemd/system/multi-user.target.wants
    install -m 0644 ${WORKDIR}/robot.service ${WORKDIR}/robot-wifi.service ${D}${systemd_system_unitdir}/
    install -m 0755 ${WORKDIR}/robot-preflight ${WORKDIR}/robot-wifi-unblock ${D}${libexecdir}/robot/
    install -m 0644 ${WORKDIR}/robot.env ${D}${sysconfdir}/robot/
    install -m 0600 ${WORKDIR}/wpa_supplicant.conf ${D}${sysconfdir}/robot/wpa_supplicant.conf
    install -m 0644 ${WORKDIR}/10-robot-wlan0.network ${D}${sysconfdir}/systemd/network/
    ln -s ${systemd_system_unitdir}/systemd.service ${D}${sysconfdir}/systemd/system/multi-user.target.wants/systemd.service
    # Solo robot-wifi + networkd gestionan wlan0; evitar servicios competidores.
    for unit in connman.service networking.service wpa_supplicant.service wpa_supplicant@wlan0.service wifi-autoconnect.service wifi-autoconnect.sh.service; do
        ln -s /dev/null ${D}${sysconfdir}/systemd/system/$unit
    done
}
FILES:${PN} += "${libexecdir}/robot ${systemd_system_unitdir} ${sysconfdir}/robot ${sysconfdir}/systemd ${sysconfdir}/robot/wpa_supplicant.conf"
