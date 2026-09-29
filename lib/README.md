# Biblioteca de Hardware del Robot

La biblioteca `librobot` proporciona una abstraccion para el acceso al
hardware del robot aspiradora.

El servidor web debe utilizar esta biblioteca para controlar los recursos
fisicos del sistema, evitando acceder directamente a GPIO, PWM u otros
mecanismos especificos del hardware.

La plataforma objetivo es una Raspberry Pi 4 Model B.

## Modulos

La biblioteca encapsula:

- Control de motores mediante PWM.
- Lectura de sensores de proximidad.
- Control de LEDs mediante GPIO.
- Reproduccion de audio.

## Motores

Las velocidades utilizan un rango de `-100` a `100`.

- Valores positivos: movimiento hacia adelante.
- Valores negativos: movimiento hacia atras.
- Valor `0`: motor detenido.

Ejemplo:

```c
robot_set_motor_speeds(70, 70);
```

La logica de control diferencial se encuentra definida. La integracion
fisica con PWM y el controlador de motores queda pendiente hasta definir
el hardware definitivo.

## Sensores

La biblioteca proporciona las siguientes funciones publicas:

```c
robot_get_front_distance();
robot_get_left_distance();
robot_get_right_distance();
```

Las distancias se expresan en centimetros. La integracion fisica queda
pendiente hasta seleccionar los sensores definitivos.

## LEDs

El sistema utiliza cuatro LEDs indicadores:

- `ROBOT_LED_POWER`: sistema encendido.
- `ROBOT_LED_AUTONOMOUS`: modo autonomo activo.
- `ROBOT_LED_MANUAL`: modo manual activo.
- `ROBOT_LED_OBSTACLE`: obstaculo detectado.

La API publica utiliza:

```c
robot_led_set(RobotLed led, int state);
```

`state = 1` enciende el LED y `state = 0` lo apaga.

La implementacion utiliza la interfaz GPIO character device de Linux por
medio de `libgpiod`. Al ejecutar `robot_init()` se solicitan las lineas GPIO
y se enciende el LED de sistema. `robot_cleanup()` apaga los LEDs y libera
los recursos GPIO.

Asignacion inicial de GPIO para Raspberry Pi 4, usando numeracion BCM:

| Indicador | GPIO BCM | Pin fisico |
| --- | ---: | ---: |
| Sistema encendido | GPIO17 | 11 |
| Modo autonomo | GPIO27 | 13 |
| Modo manual | GPIO22 | 15 |
| Obstaculo | GPIO23 | 16 |

Cada LED debe conectarse con una resistencia limitadora de corriente. Una
resistencia de aproximadamente 330 ohmios es adecuada para las pruebas del
prototipo.

```text
GPIO ---- 330 ohm ---- LED ---- GND
```

La asignacion puede modificarse posteriormente en `src/hardware_config.h`
si algun GPIO entra en conflicto con motores o sensores.

## Audio

La API proporciona:

```c
robot_audio_play("song.mp3");
robot_audio_pause();
robot_audio_stop();
robot_audio_set_volume(75);
```

El volumen utiliza un rango de `0` a `100`.

## Sistema de construccion

La biblioteca utiliza CMake como sistema de construccion.

Se requiere:

- CMake 3.16 o superior.
- Compilador compatible con C11.
- `pkg-config`.
- `libgpiod` con sus archivos de desarrollo.

Desde el directorio `lib`:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

CMake genera la biblioteca dinamica `librobot.so`.

## Estado actual

- API publica de hardware definida.
- Proyecto CMake definido para generar `librobot.so`.
- Logica de control diferencial de motores definida.
- Interfaz de lectura de sensores definida.
- Control real de los cuatro LEDs implementado mediante `libgpiod`.
- Integracion fisica de motores y sensores pendiente.
- Implementacion definitiva de audio pendiente.
