SUMMARY = "Interfaz web del robot RumBa y su servicio systemd"
DESCRIPTION = "Instala el frontend estatico (web/www) en /srv/robot/www, \
la configuracion de busybox httpd y la unidad systemd que arranca el \
servidor al energizar el sistema, con reinicio automatico ante fallos."
SECTION = "console/network"
LICENSE = "CLOSED"

ROBOT_GIT_URI ?= "git://github.com/Chris-0307/Rumba-Empotrados.git;protocol=https;branch=develop"
ROBOT_GIT_REV ?= "${AUTOREV}"

SRC_URI = "${ROBOT_GIT_URI} \
           file://httpd.conf \
           file://robot-httpd.service \
           "
SRCREV = "${ROBOT_GIT_REV}"
PV = "1.0+git"

S = "${WORKDIR}/git"

inherit systemd

# busybox provee el applet httpd (habilitado por el bbappend de esta capa).
RDEPENDS:${PN} += "busybox"

SYSTEMD_SERVICE:${PN} = "robot-httpd.service"
SYSTEMD_AUTO_ENABLE = "enable"

ROBOT_WWWDIR = "/srv/robot/www"

do_install() {
    # Frontend estatico
    install -d ${D}${ROBOT_WWWDIR}
    cp -R --no-dereference --preserve=mode,links ${S}/web/www/. ${D}${ROBOT_WWWDIR}/
    chown -R root:root ${D}${ROBOT_WWWDIR}

    # Carpeta donde robot_api.cgi se instalara cuando exista.
    install -d ${D}${ROBOT_WWWDIR}/cgi-bin

    # Carpeta de MP3 que lista la API de audio.
    install -d ${D}/srv/robot/audio

    # Configuracion de busybox httpd
    install -d ${D}${sysconfdir}
    install -m 0644 ${WORKDIR}/httpd.conf ${D}${sysconfdir}/httpd.conf

    # Unidad systemd
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/robot-httpd.service ${D}${systemd_system_unitdir}/robot-httpd.service
}

FILES:${PN} += " \
    /srv/robot \
    ${sysconfdir}/httpd.conf \
    ${systemd_system_unitdir}/robot-httpd.service \
    "

# El frontend es HTML/CSS/JS: no hay binarios que revisar.
INSANE_SKIP:${PN} += "arch"
