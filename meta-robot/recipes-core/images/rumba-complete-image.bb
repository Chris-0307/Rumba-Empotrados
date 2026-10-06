SUMMARY = "Imagen RumBa con herramientas de medicion integradas"
# Reutiliza la misma imagen Raspberry que ya se probo.
require recipes-core/images/rpi-test-image.bb
# El bbappend de rpi-test-image no se aplica a este nombre de receta;
# se explicitan sus adiciones conocidas y el arranque del robot.
IMAGE_INSTALL:append = " librobot robot-startup robot-metrics"
