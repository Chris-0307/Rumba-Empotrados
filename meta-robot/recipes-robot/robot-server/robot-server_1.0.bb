SUMMARY = "Servidor RumBa con navegacion, ciclo y velocidades independientes"
LICENSE = "CLOSED"
SRC_URI = "file://server file://robot-data"
S = "${WORKDIR}/server"
inherit cmake
DEPENDS = "librobot openssl sqlite3"
RDEPENDS:${PN} += "librobot"
EXTRA_OECMAKE += "-DROBOT_WITH_HARDWARE=ON -DROBOT_INCLUDE_DIR=${STAGING_INCDIR} -DROBOT_LIBRARY=${STAGING_LIBDIR}/librobot.so"
do_install() {
    if [ ! -s ${WORKDIR}/robot-data/robot.db ]; then
        bbfatal "Copiar robot.db del robot, con servidor detenido, a files/robot-data antes de generar imagen"
    fi
    install -d ${D}/home/root
    install -m 0755 ${B}/robot_server ${D}/home/root/robot_server
    install -m 0600 ${WORKDIR}/robot-data/robot.db ${D}/home/root/robot.db
    install -d ${D}/home/root/music
    if [ -d ${WORKDIR}/robot-data/music ]; then
        cp -R ${WORKDIR}/robot-data/music/. ${D}/home/root/music/
    fi
}
FILES:${PN} += "/home/root/robot_server /home/root/robot.db /home/root/music"
CONFFILES:${PN} += "/home/root/robot.db"
