# Biblioteca de Hardware del Robot

La biblioteca `librobot` proporciona una abstracción para el acceso al
hardware del robot aspiradora.

El servidor web debe utilizar esta biblioteca para controlar los recursos
físicos del sistema, evitando acceder directamente a GPIO, PWM u otros
mecanismos específicos del hardware.

La plataforma objetivo del proyecto es una Raspberry Pi 4 Model B.

## Módulos

La biblioteca encapsula:

- Control de motores mediante PWM.
- Lectura de sensores de proximidad.
- Control de LEDs mediante GPIO.
- Reproducción de audio.

## Estructura

```text
lib/
├── CMakeLists.txt
├── include/
│   └── robot_hw.h
├── src/
│   ├── robot_hw.c
│   ├── motors.c
│   ├── motor_backend.h
│   ├── sensors.c
│   ├── sensor_backend.h
│   ├── leds.c
│   └── audio.c
└── README.md
```

## API pública

La API pública de la biblioteca se encuentra definida en:

```text
include/robot_hw.h
```

Las aplicaciones externas deben utilizar las funciones declaradas en este
archivo para interactuar con el hardware del robot.

---

## Motores

Las velocidades utilizan un rango de `-100` a `100`.

- Valores positivos: movimiento hacia adelante.
- Valores negativos: movimiento hacia atrás.
- Valor `0`: motor detenido.

Ejemplo:

```c
robot_set_motor_speeds(70, 70);
```

También se dispone de las funciones:

```c
robot_move_forward(50);
robot_move_backward(50);
robot_turn_left(50);
robot_turn_right(50);
robot_stop();
```

La API y la lógica de control diferencial están definidas. La integración
final con PWM y el controlador físico de los motores se realizará una vez
se determine el hardware definitivo del robot.

---

## Sensores

La biblioteca proporciona una interfaz para obtener las distancias de los
sensores de proximidad:

```c
robot_get_front_distance();
robot_get_left_distance();
robot_get_right_distance();
```

Las distancias se expresan en centímetros.

La integración física queda pendiente hasta seleccionar los sensores
definitivos y establecer sus conexiones con la Raspberry Pi.

---

## LEDs

El sistema utiliza cuatro LEDs indicadores:

- `ROBOT_LED_POWER`: sistema encendido.
- `ROBOT_LED_AUTONOMOUS`: modo autónomo activo.
- `ROBOT_LED_MANUAL`: modo manual activo.
- `ROBOT_LED_OBSTACLE`: obstáculo detectado.

Los LEDs son controlados utilizando GPIO de la Raspberry Pi.

La API pública utiliza:

```c
robot_led_set(RobotLed led, int state);
```

donde:

- `state = 1`: enciende el LED.
- `state = 0`: apaga el LED.

Ejemplo:

```c
robot_led_set(ROBOT_LED_AUTONOMOUS, 1);
```

La implementación contempla la inicialización, control y liberación de los
GPIO utilizados por los LEDs.

Para evitar números de GPIO distribuidos directamente en el código, la
configuración del hardware se mantiene separada de la lógica de control.

La asignación inicial propuesta utiliza numeración BCM:

| Indicador | GPIO BCM | Pin físico |
| --- | ---: | ---: |
| Sistema encendido | GPIO17 | 11 |
| Modo autónomo | GPIO27 | 13 |
| Modo manual | GPIO22 | 15 |
| Obstáculo | GPIO23 | 16 |

Esta distribución podrá modificarse durante la integración final del
hardware en caso de existir conflictos con motores o sensores.

Cada LED debe conectarse utilizando una resistencia limitadora de corriente.

Ejemplo de conexión:

```text
GPIO ---- Resistencia ---- LED ---- GND
```

---

## Audio

La API proporciona las siguientes operaciones:

```c
robot_audio_play("song.mp3");
robot_audio_pause();
robot_audio_stop();
robot_audio_set_volume(75);
```

El volumen utiliza un rango de `0` a `100`.

La implementación física y reproducción definitiva de audio será incorporada
posteriormente.

---

## Códigos de retorno

La biblioteca define los siguientes códigos de retorno:

```c
ROBOT_OK
ROBOT_ERROR
ROBOT_INVALID_ARGUMENT
ROBOT_NOT_INITIALIZED
```

Estos permiten indicar el resultado de las operaciones realizadas sobre el
hardware.

---

# Sistema de construcción

La biblioteca utiliza CMake como sistema de construcción.

Se requiere:

- CMake 3.16 o superior.
- Compilador compatible con C11.
- GNU Make u otro generador soportado por CMake.
- `libgpiod` para el acceso a GPIO en la implementación de hardware.

## Compilación

Desde el directorio `lib`:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

CMake compilará los archivos fuente y generará la biblioteca dinámica:

```text
librobot.so
```

Dependiendo del sistema también se pueden generar enlaces simbólicos
similares a:

```text
librobot.so
librobot.so.1
librobot.so.1.0.0
```

El directorio `build/` y los archivos compilados no deben almacenarse en
el repositorio Git.

---

## Compilación cruzada

El proyecto CMake no define directamente un compilador ARM específico.

Esto permite utilizar el mismo código con el toolchain proporcionado
posteriormente por Yocto para realizar la compilación cruzada destinada a
la Raspberry Pi 4 Model B.

La configuración específica del compilador, arquitectura y sysroot será
proporcionada por el entorno de compilación cruzada.

---

## Estado actual

### Biblioteca y CMake

- API pública definida.
- Biblioteca dinámica configurada mediante CMake.
- Estructura modular para motores, sensores, LEDs y audio.

### Motores

- API de movimiento definida.
- Control diferencial definido.
- Integración física con PWM y driver de motores pendiente.

### Sensores

- API de sensores definida.
- Interfaces para sensor frontal, izquierdo y derecho definidas.
- Integración con sensores físicos pendiente.

### LEDs

- API para los cuatro indicadores definida.
- Validación de identificador de LED y estado implementada.
- Diseño de control GPIO definido para Raspberry Pi 4.
- Integración y prueba física con LEDs pendiente.

### Audio

- API de reproducción definida.
- Implementación definitiva pendiente.