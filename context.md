# Proyecto: Domótica con Arduino Mega + Home Assistant (MQTT)

## Resumen del proyecto

Sistema de domótica doméstica basado en 4 Arduino Mega repartidos en 2 roles,
que se comunican con Home Assistant vía MQTT usando la librería ArduinoHA.
Home Assistant actúa como "cerebro": recibe eventos de los pulsadores y,
mediante automatizaciones, envía comandos a los relés que controlan luces
y persianas. Los Arduinos nunca se comunican directamente entre sí.

```
[mega_pulsadores A/B] --MQTT--> [Home Assistant] --MQTT--> [mega_dispositivos A/B]
      (pulsadores)              (automatizaciones)              (relés)
```

## Hardware

- **4x Arduino Mega 2560**, repartidos en 2 roles con 2 unidades cada uno:
  - `mega_pulsadores` **A** y **B** — leen pulsadores físicos, no controlan nada.
  - `mega_dispositivos` **A** y **B** — controlan relés (luces/persianas), no leen pulsadores.
- Cada Mega lleva un **shield Ethernet** (W5100/W5500) para la conexión MQTT.
- Sin expansores I2C (MCP23017): se descartaron a propósito porque la carga
  ya está repartida entre 2 unidades por rol, y los pines nativos del Mega
  (54 digitales, menos los que usa el shield Ethernet) son suficientes.
- Pines reservados por el shield Ethernet en TODOS los sketches:
  - SPI: 50 (MISO), 51 (MOSI), 52 (SCK), 53 (SS)
  - CS del chip Ethernet: normalmente el pin 10

## Identificación de unidades A/B (mismo .ino, distinta compilación por unidad)

Cada rol (`mega_pulsadores`, `mega_dispositivos`) usa **un único fichero
`.ino`**, pero la identidad de cada unidad física (pines, MAC, IP fija,
nombre en HA) se decide en **tiempo de compilación**, no con jumper:

- Al principio del `.ino`, un bloque `#define PLACA_A` / `#define PLACA_B`
  (uno comentado, el otro no) selecciona la unidad. Antes de compilar y
  subir, se deja descomentada SOLO la línea de la unidad física a la que
  se va a flashear.
- Ese `#define` selecciona a la vez qué fichero de configuración se
  incluye (`board_config_a.h` o `board_config_b.h`, con `#include`
  condicional) — y ese fichero trae consigo los pines, el último byte de
  la MAC (distinto en A/B), la IP fija (`IP_ESTATICA`, distinta en A/B) y
  el nombre del dispositivo en HA (`Mega Pulsadores A/B`, `Mega
  Dispositivos A/B`).
- Si no se descomenta ninguna línea, o se descomentan las dos a la vez, el
  `.ino` falla la compilación con `#error` en vez de subir un firmware con
  identidad ambigua.
- Se usa `device.enableExtendedUniqueIds()` para que HA no confunda
  entidades con el mismo ID (p. ej. `p14`) entre la unidad A y la B
  del mismo rol.

Motivo del cambio (antes había un jumper físico en pin 40 leído en
runtime): al pasar los pines cableados a ficheros separados por unidad
(`board_config_a.h`/`board_config_b.h`, ver más abajo), ya había que
editar/recompilar por unidad de todos modos — el jumper se volvió
redundante. Ahora es imposible flashear el mismo binario a A y B sin
querer (habría que recompilar cambiando el `#define` a propósito), a
cambio de perder la "red de seguridad" runtime que un jumper daba si
alguien confundía las placas.

MACs: como el Mega + shield Ethernet no trae MAC de fábrica, se inventan
localmente (primer byte `0x02` = "administrada localmente"). Se usa el
byte `[3]` para distinguir familia (`0x01` = mega_pulsadores,
`0x02` = mega_dispositivos) y no colisionar en la red.

IP: cada unidad usa IP fija (`Ethernet.begin(mac, IP_ESTATICA)`), no
DHCP — `IP_ESTATICA` vive junto a la MAC en su `board_config_*.h`, debe
quedar fuera del rango DHCP del router (o reservada para esa MAC) y ser
distinta entre A y B.

## Pines, MAC e IP por unidad (`board_config_a.h` / `board_config_b.h`)

`PINES_BOTONES` (mega_pulsadores) y `PINES_LUCES`/`PINES_PERSIANAS`
(mega_dispositivos), junto con `mac[]`, `IP_ESTATICA` y `NOMBRE_PLACA`,
ya no viven dentro del `.ino`: cada sketch tiene `board_config_a.h` y
`board_config_b.h`, uno por unidad física, incluido condicionalmente
según `PLACA_A`/`PLACA_B`. Motivo: separar la identidad/wiring de cada
unidad (que cambia por unidad y con el tiempo, según lo que se cablee o
la IP que se reserve) de la lógica del firmware (que es igual en A y B),
y dejar claro que A y B pueden tener listas de pines completamente
distintas — tanto en cantidad como en tipo de dispositivo (p. ej. una
unidad `mega_dispositivos` solo con luces y la otra con luces +
persianas mezcladas) — sin que eso rompa nada del resto del código.

## Librerías usadas

- **ArduinoHA** — integración con Home Assistant vía MQTT (discovery automático).
  - Repo: https://github.com/dawidchyrzynski/arduino-home-assistant
  - Docs: https://dawidchyrzynski.github.io/arduino-home-assistant/
  - Ejemplo pulsador corto/largo (base de partida):
    https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/examples/button/button.ino
  - Ejemplo device trigger (corta/larga con JC_Button, punto de partida real):
    https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/examples/multi-state-button/multi-state-button.ino
  - Ejemplo cover (persianas):
    https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/examples/cover/cover.ino
  - Ejemplo switch múltiple (luces):
    https://github.com/dawidchyrzynski/arduino-home-assistant/blob/main/examples/multi-switch/multi-switch.ino
- **OneButton** — detección real de pulsación corta/doble/triple/cuádruple/
  quíntuple/larga. Repo: https://github.com/mathertel/OneButton
  - Importante no confundir: `ArduinoHA` (clase `HADeviceTrigger`) YA
    define en su enum `TriggerType` los 8 tipos built-in
    (`ButtonShortPressType`, `ButtonShortReleaseType`, `ButtonLongPressType`,
    `ButtonLongReleaseType`, `ButtonDoublePressType`, `ButtonTriplePressType`,
    `ButtonQuadruplePressType`, `ButtonQuintuplePressType`) — esto siempre
    ha estado en la librería. Pero esos valores son solo el NOMBRE del
    evento que se publica por MQTT discovery; ArduinoHA no lee pines ni
    cuenta clics por sí misma.
  - El ejemplo oficial de device trigger (`multi-state-button.ino`) usa
    `JC_Button` para la detección física, que SOLO distingue corta y larga
    (no cuenta multi-click), así que ese ejemplo deja sin usar
    double/triple/quadruple/quintuple aunque el enum ya los tuviera.
  - Por eso se sustituyó `JC_Button` por `OneButton`: sí detecta
    multi-click (vía `attachMultiClick` + conteo exacto de clics) además
    de pulsación larga, con debounce incluido.

## Arquitectura de entidades en Home Assistant

- **mega_pulsadores (A y B)**: no crean entidades de estado, solo
  `HADeviceTrigger` (device triggers) — aparecen en HA como "Device" en
  el trigger de una automatización, no como entidad con estado.
  - Por cada pulsador (`p14`, `p27`, ... el nombre lleva el
    número de pin, no la posición en el array) hay hasta 7 triggers
    posibles: `ButtonShortPressType`, `ButtonDoublePressType`,
    `ButtonTriplePressType`, `ButtonQuadruplePressType`,
    `ButtonQuintuplePressType`, `ButtonLongPressType`,
    `ButtonLongReleaseType` (en ese orden en el código: 1-2-3-4-5
    pulsaciones primero, larga/fin de larga aparte al final).
    `ButtonLongReleaseType` se dispara al soltar una pulsación larga —
    pensado para automatizaciones "mantener pulsado para mover / soltar
    para parar" (p. ej. persianas controladas desde un pulsador físico
    normal vía automatización en HA, no cableado directo). Cada uno de
    los 7 se activa/desactiva por separado con `HABILITAR_CORTA`/
    `HABILITAR_DOBLE`/etc. en `mega_pulsadores.ino` (por defecto:
    corta/doble/larga/largaFin activos, triple/cuádruple/quíntuple
    desactivados — ahorra RAM, ver "RAM / límite de pulsadores" en
    `todo.md`). El enum de ArduinoHA también define
    `ButtonShortReleaseType`, que este proyecto no usa.
- **mega_dispositivos (A y B)**: crean entidades reales:
  - `HASwitch` por luz (`luz_22`, `luz_30`, ... nombre = número de pin) —
    on/off.
  - `HACover` por persiana (`persiana_38_39`, `persiana_41_42`, ...
    nombre = pin de "subir" seguido del pin de "bajar" del par, en ese
    orden siempre) — soporta
    `CommandOpen` / `CommandClose` / `CommandStop`. Parar = poner los dos
    relés (subir/bajar) a LOW simultáneamente.

## Estado actual

- [x] Arquitectura general definida (2 roles x 2 unidades, sin MCP23017).
- [x] Sketch base de `mega_pulsadores` con 7 tipos de pulsación (corta,
      doble, triple, cuádruple, quíntuple, larga, fin de larga).
- [x] Sketch base de `mega_dispositivos` con luces + persianas + stop.
- [x] Mosquitto ya instalado y funcionando en Home Assistant.

Para la lista de tareas pendientes (configuración de red, pines reales,
identidad A/B al compilar, automatizaciones en HA, ajustes de timing,
etc.) ver [todo.md](todo.md) — es la única fuente de verdad para
pendientes, para evitar tener dos listas que se puedan desincronizar.

## HVAC / termostato (futuro, no diseñado aún)

Instalación de un amigo, aún sin detalles exactos de cableado. Hay un
único termostato en la casa con:
- Interruptor físico que elige modo calor/frío.
- Ajuste de temperatura que dispara parar/continuar al alcanzarla.
- Modo calor → suelo radiante; modo frío → AC de techo/conductos.
- Probablemente controlado por relés también, "AFAIK" (usuario no está
  seguro de los detalles exactos, es la instalación de un tercero).

No se ha decidido ninguna arquitectura todavía — deliberadamente, porque
faltan datos clave (si el termostato decide on/off por sí mismo o es un
contacto seco simple, si el interruptor de modo actúa antes o después de
esa decisión, si esto se integrará con HA o se queda físico/analógico, y
si comparte Mega con `mega_dispositivos` o merece rol propio). Ver
preguntas abiertas en [todo.md](todo.md).

## Notas de seguridad/hardware a mantener

- Relés de persiana: nunca activar subir y bajar a la vez. Protección
  actual = `RETARDO_INVERSION_MS` (200ms) en software entre apagar un
  sentido y encender el otro. Si el módulo de relés ya tiene interlock
  por hardware, este retardo se puede reducir.
- Comprobar si los módulos de relé son activos en HIGH o en LOW antes de
  tocar la lógica de `digitalWrite` (los sketches actuales asumen activo
  en HIGH).
