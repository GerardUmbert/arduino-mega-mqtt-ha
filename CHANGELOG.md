# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).
La versión aquí debe coincidir con `device.setSoftwareVersion(...)` en
ambos `.ino` — es lo que Home Assistant muestra como versión de firmware
de cada dispositivo.

## [1.2.0] - Sin publicar

### Changed
- El `unique_id` de cada pulsador/luz/persiana (`boton_XX`, `luz_XX`,
  `persiana_XX_YY`) ahora se genera a partir del número de PIN físico,
  no de la posición del pin dentro del array (`PINES_BOTONES`,
  `PINES_LUCES`, `PINES_PERSIANAS`). Permite reordenar, insertar o
  borrar pines libremente sin romper entidades ya renombradas en Home
  Assistant, y hace trivial mapear un pin físico a su entidad en HA.
  Las persianas incluyen ambos pines del par en el ID, siempre en
  orden subir_bajar (p. ej. `persiana_38_39`).
- La identidad de unidad A/B ya NO se resuelve con un jumper físico en
  runtime (pin 40): ahora se decide en tiempo de compilación con un
  bloque `#define PLACA_A` / `#define PLACA_B` al principio de cada
  `.ino`. Ese `#define` selecciona a la vez el fichero de pines
  (`pines_a.h`/`pines_b.h`), el último byte de la MAC y el nombre del
  dispositivo en HA. Si no se descomenta ninguna línea, o las dos a la
  vez, la compilación falla con `#error` en vez de subir un firmware
  con identidad ambigua.
- Los arrays de pines (`PINES_BOTONES`, `PINES_LUCES`,
  `PINES_PERSIANAS`) se han movido fuera de los `.ino`, a ficheros
  nuevos `pines_a.h`/`pines_b.h` (uno por unidad física, en cada
  carpeta de sketch). Separa la configuración de wiring —que cambia por
  unidad y con el tiempo— de la lógica del firmware, y dos unidades del
  mismo rol ya no necesitan tener la misma cantidad ni el mismo tipo de
  dispositivos cableados.

### Added
- Nuevo trigger `ButtonLongReleaseType` por pulsador en
  `mega_pulsadores` (se dispara al soltar una pulsación larga), pensado
  para automatizaciones "mantener pulsado para mover / soltar para
  parar" — p. ej. controlar una persiana desde un pulsador físico
  normal vía automatización en Home Assistant.
- `mega_pulsadores/pines_a.h`, `mega_pulsadores/pines_b.h`,
  `mega_dispositivos/pines_a.h`, `mega_dispositivos/pines_b.h`.

## [1.1.0] - Sin publicar

### Added
- `README.md` con documentación completa del proyecto (hardware,
  librerías y por qué se eligieron, arquitectura de entidades en HA,
  configuración).
- `todo.md` con la lista de tareas pendientes, como única fuente de
  verdad (antes duplicada también en `context.md`).
- `CLAUDE.md` con reglas de seguridad del proyecto (no subir `config.h`
  con credenciales reales).
- Soporte de pulsación cuádruple y quíntuple en `mega_pulsadores`
  (`ButtonQuadruplePressType`, `ButtonQuintuplePressType`), además de
  corta/doble/triple/larga.
- `config.h.example` en cada sketch como plantilla de configuración de
  red (IP del broker, usuario/password MQTT).
- Comentarios de advertencia en los arrays `PINES_BOTONES`,
  `PINES_LUCES` y `PINES_PERSIANAS`: el `unique_id` de cada entidad se
  genera por posición en el array, no por número de pin — no reordenar
  ni insertar/borrar en medio una vez renombrado en Home Assistant.

### Changed
- Cada sketch movido a su propia carpeta (`mega_pulsadores/`,
  `mega_dispositivos/`) para que el Arduino IDE pueda abrirlos
  directamente.
- `BROKER_ADDR`, `MQTT_USER` y `MQTT_PASS` ya no están hardcodeados en
  los `.ino`: ahora viven en `config.h` (gitignored) y se incluyen con
  `#include "config.h"`.
- `memory.md` renombrado a `context.md`.

## [1.0.0] - 2026-07-23

### Added
- Arquitectura general: 2 roles (`mega_pulsadores`, `mega_dispositivos`)
  x 2 unidades (A/B) cada uno, comunicados con Home Assistant vía MQTT
  (ArduinoHA), sin comunicación directa entre Arduinos.
- Sketch base de `mega_pulsadores`: lectura de pulsadores físicos con
  OneButton, envío de `HADeviceTrigger` (corta/doble/triple/larga).
- Sketch base de `mega_dispositivos`: control de luces (`HASwitch`) y
  persianas (`HACover`) con protección por software contra activar
  subir/bajar a la vez (`RETARDO_INVERSION_MS`).
- Identificación de unidad A/B mediante jumper en `PIN_ID_PLACA` (pin
  40) y generación automática de MAC/nombre de dispositivo.
