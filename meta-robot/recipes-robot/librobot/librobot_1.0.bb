SUMMARY = "Biblioteca de hardware RumBa, sensores, motores, aspiracion y audio"
LICENSE = "CLOSED"
SRC_URI = "file://librobot"
S = "${WORKDIR}/librobot"
inherit cmake pkgconfig
DEPENDS = "libgpiod"
RDEPENDS:${PN} += "libgpiod mpg123 alsa-utils-aplay"
FILES:${PN} += "${datadir}/robot/sounds"
