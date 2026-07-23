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

## Identificación de unidades A/B (mismo firmware en las 2 placas del mismo rol)

Cada rol (`mega_pulsadores`, `mega_dispositivos`) usa **un único sketch**
que se sube sin modificar a sus 2 unidades físicas. La identidad se
resuelve con un jumper:

- Pin `PIN_ID_PLACA` (definido como `40` en ambos sketches) configurado
  como `INPUT_PULLUP`.
- **Unidad A**: pin al aire → lee HIGH → `deviceId = 0`.
- **Unidad B**: pin puenteado a GND con un cable → lee LOW → `deviceId = 1`.
- El último byte de la MAC y el nombre del dispositivo en HA
  (`Mega Pulsadores A/B`, `Mega Dispositivos A/B`) se generan solos a
  partir de `deviceId`.
- Se usa `device.enableExtendedUniqueIds()` para que HA no confunda
  entidades con el mismo ID (p. ej. `boton_01`) entre la unidad A y la B
  del mismo rol.

MACs: como el Mega + shield Ethernet no trae MAC de fábrica, se inventan
localmente (primer byte `0x02` = "administrada localmente"). Se usa el
byte `[3]` para distinguir familia (`0x01` = mega_pulsadores,
`0x02` = mega_dispositivos) y no colisionar en la red.

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
- **OneButton** — detección de pulsación corta/doble/triple/larga (JC_Button
  del ejemplo oficial NO sirve para esto, solo corta/larga).
  Repo: https://github.com/mathertel/OneButton

## Arquitectura de entidades en Home Assistant

- **mega_pulsadores (A y B)**: no crean entidades de estado, solo
  `HADeviceTrigger` (device triggers) — aparecen en HA como "Device" en
  el trigger de una automatización, no como entidad con estado.
  - Por cada pulsador (`boton_01`, `boton_02`, ...) hay 4 triggers:
    `ButtonShortPressType`, `ButtonDoublePressType`, `ButtonTriplePressType`,
    `ButtonLongPressType` (en ese orden en el código: 1-2-3 pulsaciones
    primero, larga aparte al final).
- **mega_dispositivos (A y B)**: crean entidades reales:
  - `HASwitch` por luz (`luz_01`, `luz_02`, ...) — on/off.
  - `HACover` por persiana (`persiana_01`, `persiana_02`, ...) — soporta
    `CommandOpen` / `CommandClose` / `CommandStop`. Parar = poner los dos
    relés (subir/bajar) a LOW simultáneamente.

## Configuración de red pendiente de rellenar en los 4 sketches

- `BROKER_ADDR`: IP de Home Assistant / Mosquitto. Debe reservarse por
  DHCP estático en el router para que no cambie. No hay autodetección.
- `MQTT_USER` / `MQTT_PASS`: credenciales del broker Mosquitto.
- `PINES_BOTONES` (mega_pulsadores) y `PINES_LUCES` / `PINES_PERSIANAS`
  (mega_dispositivos): listas de pines reales, distintas en cada una de
  las 2 unidades de cada rol según lo que tengan cableado físicamente.

## Estado actual / pendiente de decidir

- [x] Arquitectura general definida (2 roles x 2 unidades, sin MCP23017).
- [x] Sketch base de `mega_pulsadores` con 4 tipos de pulsación.
- [x] Sketch base de `mega_dispositivos` con luces + persianas + stop.
- [x] Mosquitto ya instalado y funcionando en Home Assistant.
- [ ] Rellenar `BROKER_ADDR`, credenciales MQTT y listas de pines reales
      en las 4 copias del firmware (2 pulsadores + 2 dispositivos).
- [ ] Verificar en HA (Ajustes → Dispositivos y servicios → MQTT) que
      las 4 unidades aparecen correctamente diferenciadas (A/B) sin IDs
      duplicados.
- [ ] Definir el mapeo de pulsaciones por botón → acción sobre
      luz/persiana (p. ej. corta = abrir/encender, doble = cerrar/apagar,
      larga = parar persiana). Aún no cerrado.
- [ ] Crear las automatizaciones en HA que conectan cada pulsador con su
      luz/persiana correspondiente (pendiente decidir: UI manual una a
      una, o YAML con plantilla para no repetir docenas de automatizaciones).
- [ ] Ajustar timings de `OneButton` si los tiempos por defecto (400ms
      para doble/triple, 1000ms para larga) no encajan con el uso real.

## Notas de seguridad/hardware a mantener

- Relés de persiana: nunca activar subir y bajar a la vez. Protección
  actual = `RETARDO_INVERSION_MS` (200ms) en software entre apagar un
  sentido y encender el otro. Si el módulo de relés ya tiene interlock
  por hardware, este retardo se puede reducir.
- Comprobar si los módulos de relé son activos en HIGH o en LOW antes de
  tocar la lógica de `digitalWrite` (los sketches actuales asumen activo
  en HIGH).
