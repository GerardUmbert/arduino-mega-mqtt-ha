# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).
La versión aquí debe coincidir con `device.setSoftwareVersion(...)` en
ambos `.ino` — es lo que Home Assistant muestra como versión de firmware
de cada dispositivo.

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
