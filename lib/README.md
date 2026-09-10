# Biblioteca de Hardware del Robot

La biblioteca `lib` proporciona una  abstracción para
el acceso al hardware del robot rumba.

El servidor web debe utilizar esta biblioteca para controlar los
recursos físicos del sistema.

## Módulos

La biblioteca encapsula:

- Control de motores mediante PWM.
- Lectura de sensores de proximidad.
- Control de LEDs.
- Reproducción de audio.

## Motores

Las velocidades utilizan un rango de -100 a 100.

- Valores positivos: movimiento hacia adelante.
- Valores negativos: movimiento hacia atrás.
- Valor 0: motor detenido.

Ejemplo:

```c
robot_set_motor_speeds(70, 70);