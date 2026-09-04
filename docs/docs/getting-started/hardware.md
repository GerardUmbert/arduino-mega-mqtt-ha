# Hardware

- **4x Arduino Mega 2560**, repartidos en 2 roles con 2 unidades cada
  uno (A y B por rol).
- Cada Mega lleva un **shield Ethernet** (W5100/W5500) para la
  conexión MQTT.
- Sin expansores I2C (MCP23017): se descartaron a propósito porque la
  carga ya está repartida entre 2 unidades por rol, y los pines
  nativos del Mega (54 digitales, menos los que usa el shield
  Ethernet) son suficientes.

## Pines reservados por el shield Ethernet

En **todos** los sketches, estos pines están ocupados por el shield y
no se pueden usar para pulsadores/relés:

| Pin | Función |
|---|---|
| 50 | MISO (SPI) |
| 51 | MOSI (SPI) |
| 52 | SCK (SPI) |
| 53 | SS (SPI) |
| 10 | CS del chip Ethernet (normalmente) |

## SRAM disponible

El Mega 2560 tiene **8&nbsp;KB de SRAM totales**. El shield Ethernet y
la librería ArduinoHA reservan una parte fija antes de que tu código
llegue a `setup()` — ver [RAM y rendimiento](../reference/ram.md) para
el desglose real medido en placa.

## Baudrate del Serial Monitor

Todos los `.ino` de este proyecto usan `Serial.begin(9600)`. Si el
Serial Monitor está a otro baudrate (p. ej. 115200), verás texto
basura o caracteres repetidos sin parar — **eso no es un reinicio en
bucle real**, es solo un desajuste de baudrate. Comprueba el selector
de baudrate del monitor antes de asumir que la placa está crasheando.
Ver [Solución de problemas](../reference/troubleshooting.md) para más
síntomas y sus causas reales.
