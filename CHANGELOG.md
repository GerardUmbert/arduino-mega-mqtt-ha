# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).
La versión aquí debe coincidir con `device.setSoftwareVersion(...)` en
ambos `.ino` — es lo que Home Assistant muestra como versión de firmware
de cada dispositivo.

## [ha-blueprints] - 2026-07-23

No es una versión de firmware (no toca ningún `.ino`, por eso no lleva
número de versión sino el nombre del tag de este commit).

### Added
- `home_assistant/blueprints/persiana_posicion.yaml`: blueprint que
  añade una posición estimada (0-100%) a una persiana sin encoder,
  calculando el tiempo de relé necesario a partir de un tiempo de
  apertura/cierre calibrado por persiana. Resincroniza a 0/100 cada vez
  que la persiana llega de verdad a un extremo, venga el movimiento de
  donde venga.
- `home_assistant/blueprints/persiana_pulsador.yaml`: blueprint que
  conecta un pulsador físico (`button_long_press`/`button_long_release`
  de `mega_pulsadores`) con abrir/cerrar/parar de una persiana —
  implementa el patrón "mantener pulsado para mover" descrito en
  `todo.md`.
- `home_assistant/blueprints/README.md`: cómo crear los helpers
  `input_number` necesarios, cómo calibrar los tiempos de recorrido, y
  cómo combinar ambos blueprints sobre la misma persiana.
- `home_assistant/blueprints/luz_pulsador.yaml`: blueprint que conecta
  un pulsador físico con una luz (`switch.*`) — pulsación corta hace
  toggle, pulsación triple enciende y apaga sola pasados N minutos
  (configurable).
- `home_assistant/blueprints/luz_zigbee_respaldo.yaml`: blueprint para
  luces cuyo brillo/temperatura de color se controla por una bombilla
  Zigbee (pensado para IKEA TRÅDFRI WW/CW) en vez de por el relé de
  `mega_dispositivos`. Hace tres cosas independientes: (1) el pulsador
  físico hace toggle de la bombilla (control normal del día a día) y a
  la vez fuerza el relé a ON siempre, sin condición ni toggle — el relé
  nunca se apaga desde el pulsador, solo se garantiza que esté
  encendido; nada de esto pasa solo porque HA o MQTT arranquen o se
  reconecten, el único disparador es el pulsador físico; (2)
  opcionalmente, cuando la bombilla Zigbee vuelve a encenderse por su
  cuenta (su propia recuperación tras un corte), HA le aplica un
  brillo/temperatura por defecto en vez de dejar lo que decida ella
  sola; (3) ajusta sola la temperatura de color según la posición del
  sol (cálida de noche, neutra a mediodía), sin encender/apagar la
  bombilla por su cuenta. Ninguna de las tres cosas
  requiere cambios en `mega_dispositivos.ino`.
- `home_assistant/blueprints/luces_notas.md`: ideas de automatización
  para luces con los triggers libres (doble, cuádruple, quíntuple,
  larga) y detalle del enfoque de brillo/temperatura vía Zigbee en vez
  de PWM en el relé.
- `home_assistant/blueprints/persiana_pulsador_completo.yaml`:
  alternativa a `persiana_pulsador.yaml` que usa los 5 niveles de
  pulsación del botón (no solo mantener pulsado) para un patrón
  subir/bajar completo: 1 pulsación = esta persiana al extremo, 2 =
  todas las persianas de la misma Area al extremo, 3 = esta persiana al
  50%, 4 = ajuste fino ±5% desde la posición actual, 5 = todas las
  persianas de la casa al extremo, larga = mover mientras se mantiene
  (y al soltar sin llegar a un extremo, estima la posición recorrida
  por tiempo mantenido en vez de dejarla desactualizada). Depende de
  que cada persiana afectada tenga ya su propia instancia de
  `persiana_posicion.yaml`, con los helpers de esa persiana nombrados
  siguiendo el convenio `input_number.<object_id>_objetivo`/
  `..._posicion` que el blueprint deriva automáticamente del
  `entity_id` de cada `cover.*`.

## [1.3.0] - 2026-07-31

### Added
- Diagnóstico por Serial (9600 baudios) en ambos `.ino`: al arrancar
  imprime el nombre de la placa y su MAC, el resultado de `Ethernet.begin`
  (IP asignada por DHCP, o error explícito si falla), y los eventos de
  conexión/desconexión MQTT (`mqtt.onConnected`/`onDisconnected`).
  Pensado para diagnosticar en campo si una placa no aparece en la red
  (fallo de boot vs. fallo de DHCP vs. fallo de MQTT) sin depender de
  verla en el listado de clientes del router.
- `mega_dispositivos`: cada comando de luz o persiana ejecutado
  (encender/apagar, abrir/cerrar/parar) se imprime por Serial con el ID
  de la entidad afectada.
- `mega_pulsadores`: cada evento de pulsador detectado (corta, doble,
  multiclick con el número exacto de clics, inicio y fin de pulsación
  larga) se imprime por Serial con el ID del botón.

## [1.2.0] - 2026-07-23

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

## [1.1.0] - 2026-07-23

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
