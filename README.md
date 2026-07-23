# Domótica con Arduino Mega + Home Assistant (MQTT)

Sistema de domótica doméstica basado en 4 Arduino Mega 2560 repartidos en 2
roles, que se comunican con Home Assistant vía MQTT usando la librería
ArduinoHA. Home Assistant actúa como "cerebro": recibe eventos de los
pulsadores y, mediante automatizaciones, envía comandos a los relés que
controlan luces y persianas. Los Arduinos nunca se comunican directamente
entre sí.

```
[mega_pulsadores A/B] --MQTT--> [Home Assistant] --MQTT--> [mega_dispositivos A/B]
      (pulsadores)              (automatizaciones)              (relés)
```

## Estructura del repo

- `mega_pulsadores/mega_pulsadores.ino` — firmware para las 2 unidades que
  leen pulsadores físicos. No controla nada, solo envía eventos.
- `mega_dispositivos/mega_dispositivos.ino` — firmware para las 2 unidades
  que controlan relés de luces y persianas. No lee pulsadores, solo recibe
  órdenes.
- `todo.md` — lista de tareas pendientes antes de dar el proyecto por
  terminado (IPs, credenciales, pines reales, automatizaciones...).
- `CHANGELOG.md` — historial de cambios del proyecto, versionado igual
  que `device.setSoftwareVersion(...)` en ambos `.ino`.

## Hardware

- 4x Arduino Mega 2560, repartidos en 2 roles con 2 unidades cada uno
  (A y B por rol).
- Cada Mega lleva un shield Ethernet (W5100/W5500) para la conexión MQTT.
- Sin expansores I2C (MCP23017): se descartaron a propósito porque la carga
  ya está repartida entre 2 unidades por rol, y los pines nativos del Mega
  (54 digitales, menos los que usa el shield Ethernet) son suficientes.
- Pines reservados por el shield Ethernet en TODOS los sketches:
  - SPI: 50 (MISO), 51 (MOSI), 52 (SCK), 53 (SS)
  - CS del chip Ethernet: normalmente el pin 10

## Identificación de unidades A/B

Cada rol usa un único sketch que se sube sin modificar a sus 2 unidades
físicas. La identidad se resuelve con un jumper en el pin `PIN_ID_PLACA`
(pin 40), configurado como `INPUT_PULLUP`:

- **Unidad A**: pin al aire (sin conectar) → lee HIGH → `deviceId = 0`.
- **Unidad B**: pin puenteado a cualquier GND del Mega → lee LOW →
  `deviceId = 1`.

El nombre del dispositivo en HA (`Mega Pulsadores A/B`, `Mega Dispositivos
A/B`) y el último byte de la MAC se generan solos a partir de `deviceId`.
Se usa `device.enableExtendedUniqueIds()` para que HA no confunda entidades
con el mismo ID (p. ej. `boton_01`) entre la unidad A y la B del mismo rol.

Las MACs se inventan localmente porque el Mega + shield Ethernet no trae
MAC de fábrica (primer byte `0x02` = "administrada localmente"). El byte
`[3]` distingue familia (`0x01` = mega_pulsadores, `0x02` =
mega_dispositivos) para que nunca choquen en la red.

## Librerías usadas y por qué

- **[ArduinoHA](https://github.com/dawidchyrzynski/arduino-home-assistant)**
  — integración con Home Assistant vía MQTT con discovery automático. Es la
  base de todo el proyecto: crea las entidades/triggers en HA sin tener que
  escribir el YAML de discovery a mano.
- **[OneButton](https://github.com/mathertel/OneButton)** — detección real
  de pulsaciones (corta/doble/triple/cuádruple/quíntuple/larga) en
  `mega_pulsadores`. Es importante no confundir dos cosas distintas:
  - `ArduinoHA` (la clase `HADeviceTrigger`) **ya define** en su enum
    `TriggerType` los 8 tipos de pulsación built-in: `ButtonShortPressType`,
    `ButtonShortReleaseType`, `ButtonLongPressType`, `ButtonLongReleaseType`,
    `ButtonDoublePressType`, `ButtonTriplePressType`,
    `ButtonQuadruplePressType` y `ButtonQuintuplePressType`. Esto siempre
    ha estado en la librería, no es algo que haya que añadir.
  - Pero esos valores son solo **el nombre del evento que se publica** por
    MQTT discovery — `ArduinoHA` no lee ningún pin ni cuenta clics por sí
    misma. Necesita que algo le diga "esto ha sido un doble/triple/cuádruple
    clic" antes de poder llamar a `->trigger()`.
  - El ejemplo oficial de ArduinoHA para device triggers
    ([multi-state-button.ino](https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/examples/multi-state-button/multi-state-button.ino))
    usa la librería `JC_Button` para esa detección física, pero `JC_Button`
    solo distingue pulsación corta y larga — no cuenta clics múltiples. Por
    eso el ejemplo oficial únicamente usa `ButtonShortPressType` y
    `ButtonLongPressType`, dejando sin usar el resto de tipos que el enum
    ya ofrecía.
  - Como este proyecto necesita también doble/triple/cuádruple/quíntuple
    pulsación, se sustituyó `JC_Button` por `OneButton`, que sí detecta
    multi-click (con `attachMultiClick` + conteo exacto de clics) además
    de pulsación larga, con debounce incluido.
- **Ethernet** — incluida en el IDE de Arduino, para el shield W5100/W5500.

## Arquitectura de entidades en Home Assistant

- **mega_pulsadores (A y B)**: no crean entidades de estado, solo
  `HADeviceTrigger` — aparecen en HA como "Device" en el trigger de una
  automatización, no como entidad con estado. Por cada pulsador
  (`boton_01`, `boton_02`...) hay 6 triggers: `ButtonShortPressType`,
  `ButtonDoublePressType`, `ButtonTriplePressType`,
  `ButtonQuadruplePressType`, `ButtonQuintuplePressType` y
  `ButtonLongPressType`. También existen en el enum de ArduinoHA
  (aunque este proyecto no los usa): `ButtonShortReleaseType` y
  `ButtonLongReleaseType`.
- **mega_dispositivos (A y B)**: crean entidades reales:
  - `HASwitch` por luz (`luz_01`, `luz_02`...) — on/off.
  - `HACover` por persiana (`persiana_01`, `persiana_02`...) — soporta
    abrir/cerrar/parar. Parar = poner los dos relés (subir/bajar) a LOW
    simultáneamente.

## Configuración antes de subir el firmware

### IP y credenciales MQTT (`config.h`)

`BROKER_ADDR`, `MQTT_USER` y `MQTT_PASS` NO están escritos directamente en
los `.ino` — viven en un fichero `config.h` que cada `.ino` incluye con
`#include "config.h"`. Ese fichero está en `.gitignore` y nunca se sube al
repositorio, así que tu IP y tu password reales se quedan solo en tu
ordenador/placa.

Para configurarlo, en `mega_pulsadores/` y en `mega_dispositivos/`:

1. Copia `config.h.example` como `config.h` (mismo directorio).
2. Rellena `BROKER_ADDR`, `MQTT_USER` y `MQTT_PASS` con tus valores reales.

Como las 2 unidades de cada rol (A y B) comparten el mismo broker MQTT,
normalmente usarás el mismo `config.h` en ambas — solo cambia el jumper
del pin 40 para diferenciarlas.

### Pines reales cableados

- `PINES_BOTONES` (mega_pulsadores) / `PINES_LUCES` y `PINES_PERSIANAS`
  (mega_dispositivos): a diferencia de la IP/credenciales, estos SÍ se
  editan directamente en el `.ino`, porque son distintos en cada una de
  las 2 unidades físicas de cada rol según lo que tengan cableado.

⚠️ **El orden de estos arrays importa.** El `unique_id` de cada entidad
(`boton_05`, `luz_05`, `persiana_03`...) se genera solo por la **posición**
del pin en el array, no por el número de pin en sí — `boton_05` es
simplemente "el 5º pin de la lista", sea cual sea ese pin.

Esto significa que el nombre "bonito" que le pongas en Home Assistant
(p. ej. renombrar `boton_05` a "Interruptor Dormitorio 1" o `luz_05` a
"Luz Dormitorio 1") sigue funcionando sin problema aunque renombres cien
veces, **siempre que no reordenes ni insertes/borres pines en medio del
array** una vez que ya hayas subido el firmware y renombrado en HA. Si
insertas un pin nuevo en medio de la lista, todo lo que va detrás se
desplaza una posición y `boton_05`/`luz_05` pasarían a apuntar a un pin
físico distinto del que creías (la entidad que renombraste como
"Dormitorio 1" empezaría a controlar otra cosa).

**Regla práctica: si añades un pulsador, luz o persiana nueva más
adelante, añade su pin siempre al final de la lista correspondiente,
nunca en medio.** Esta misma advertencia está también como comentario
directamente encima de cada array en el código.

Ver [todo.md](todo.md) para la lista completa de tareas pendientes.

## Seguridad / notas de hardware

- Relés de persiana: nunca activar subir y bajar a la vez. Protección
  actual = `RETARDO_INVERSION_MS` (200ms) en software entre apagar un
  sentido y encender el otro. Si el módulo de relés ya tiene interlock por
  hardware, este retardo se puede reducir.
- Comprobar si los módulos de relé son activos en HIGH o en LOW antes de
  tocar la lógica de `digitalWrite` (los sketches actuales asumen activo en
  HIGH).
