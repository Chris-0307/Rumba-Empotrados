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

# Sistema de construcción

La biblioteca utiliza CMake como sistema de construcción.

Se requiere:

CMake 3.16 o superior.
Compilador compatible con C11.
GNU Make u otro generador soportado por CMake.

# Compilación

Desde el directorio robot-library:

mkdir build
cd build
cmake ..
cmake --build .

CMake compilará los archivos fuente y generará la biblioteca dinámica:

librobot.so

Dependiendo del sistema también se pueden generar enlaces simbólicos
similares a:

librobot.so
librobot.so.1
librobot.so.1.0.0