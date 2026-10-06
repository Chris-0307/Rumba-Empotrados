SUMMARY = "Herramientas ligeras para medir recursos y disponibilidad de la API RumBa"
LICENSE = "CLOSED"
SRC_URI = "file://robot-medir-recursos file://robot-resumir-recursos file://robot-medir-arranque file://robot-boot-metrics.service"
S = "${WORKDIR}"
inherit systemd
RDEPENDS:${PN} = "busybox systemd systemd-extra-utils"
SYSTEMD_SERVICE:${PN} = "robot-boot-metrics.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"
do_install() {
    install -d ${D}${bindir} ${D}${systemd_system_unitdir}
    install -m 0755 ${WORKDIR}/robot-medir-recursos ${WORKDIR}/robot-resumir-recursos ${WORKDIR}/robot-medir-arranque ${D}${bindir}/
    install -m 0644 ${WORKDIR}/robot-boot-metrics.service ${D}${systemd_system_unitdir}/
}
FILES:${PN} += "${systemd_system_unitdir}/robot-boot-metrics.service"
