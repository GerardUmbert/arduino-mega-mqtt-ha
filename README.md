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

- `mega_pulsadores/mega_pulsadores.ino` — firmware para las unidades que
  leen pulsadores físicos, usando la librería `OneButton`. No controla
  nada, solo envía eventos. Soporta los 7 tipos de pulsación (corta,
  doble, triple, cuádruple, quíntuple, larga, fin de larga), cada uno
  activable/desactivable por separado.
- `mega_pulsadores/board_config_a.h`, `mega_pulsadores/board_config_b.h` —
  pines, MAC, IP fija y nombre HA de cada una de las 2 unidades físicas.
- `mega_pulsadores_low_ram/mega_pulsadores_low_ram.ino` — **firmware
  alternativo** para el mismo rol, usando `AceButton` en vez de
  `OneButton`: mucha menos RAM por pulsador (~18-26 bytes/instancia
  frente a ~90-100 de OneButton), a cambio de perder soporte de
  triple/cuádruple/quíntuple clic por completo — AceButton no tiene
  ningún mecanismo para contar 3+ pulsaciones seguidas, no es una
  opción desactivable. Solo cubre corta/doble/larga/fin de larga. Ver
  "¿`mega_pulsadores` o `mega_pulsadores_low_ram`?" más abajo antes de
  elegir cuál flashear en una unidad concreta.
- `mega_pulsadores_low_ram/board_config_a.h`,
  `mega_pulsadores_low_ram/board_config_b.h` — **copias independientes**
  de las de `mega_pulsadores/`, no compartidas (Arduino exige que los
  `.h` vivan en la misma carpeta que el `.ino` que los usa). Si cambias
  pines/MAC/IP/nombre en una carpeta, revisa si el mismo cambio aplica
  también en la otra — no se sincronizan solas.
- `mega_dispositivos/mega_dispositivos.ino` — firmware para las 2 unidades
  que controlan relés de luces y persianas. No lee pulsadores, solo recibe
  órdenes.
- `mega_dispositivos/board_config_a.h`, `mega_dispositivos/board_config_b.h`
  — pines, MAC, IP fija y nombre HA de cada una de las 2 unidades físicas.
- `todo.md` — lista de tareas pendientes antes de dar el proyecto por
  terminado (IPs, credenciales, pines reales, automatizaciones...).
- `CHANGELOG.md` — historial de cambios del proyecto, versionado igual
  que `device.setSoftwareVersion(...)` en cada `.ino` (los tres
  versionan de forma independiente).

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

Cada rol usa un único fichero `.ino`, pero la identidad de cada unidad
física (A o B) se decide en **tiempo de compilación**, no con un jumper:
al principio del `.ino` hay un bloque `PLACA_A`/`PLACA_B` — se deja
descomentada SOLO una de las dos líneas según a qué unidad física vayas a
subir ese firmware, se compila y se sube. Para la otra unidad, se cambia
la línea descomentada y se vuelve a subir.

Ese único `#define` selecciona a la vez cuatro cosas:

- Qué fichero de configuración se incluye (`board_config_a.h` o
  `board_config_b.h`), con los pines realmente cableados en esa unidad.
- El último byte de la MAC (distinto en A y B, para no colisionar en la
  red).
- La IP fija de esa unidad (`IP_ESTATICA`, distinta en A y B).
- El nombre del dispositivo en HA (`Mega Pulsadores A/B`, `Mega
  Dispositivos A/B`).

Si por error se compila sin descomentar ninguna línea, o con las dos a la
vez, el propio `.ino` falla la compilación con un `#error` explícito en
vez de subir un firmware con identidad ambigua.

Se usa `device.enableExtendedUniqueIds()` para que HA no confunda entidades
con el mismo ID (p. ej. `p14`) entre la unidad A y la B del mismo rol.

Las MACs se inventan localmente porque el Mega + shield Ethernet no trae
MAC de fábrica (primer byte `0x02` = "administrada localmente"). El byte
`[3]` distingue familia (`0x01` = mega_pulsadores, `0x02` =
mega_dispositivos) para que nunca choquen en la red.

## ¿`mega_pulsadores` o `mega_pulsadores_low_ram`?

Las dos carpetas hacen lo mismo desde el punto de vista de Home
Assistant (envían device triggers MQTT por cada pulsación), pero usan
librerías distintas para leer el botón físico, con un compromiso RAM ↔
funcionalidad:

| | `mega_pulsadores` (OneButton) | `mega_pulsadores_low_ram` (AceButton) |
|---|---|---|
| RAM por pulsador | ~90-100 bytes (fijo, uses las pulsaciones que uses) | ~18-26 bytes |
| Pulsaciones soportadas | Las 7: corta, doble, triple, cuádruple, quíntuple, larga, fin de larga | Solo 4: corta, doble, larga, fin de larga — **sin mecanismo alguno** para triple/cuádruple/quíntuple, no es una opción desactivable |
| Compatible con `persiana_pulsador_completo.yaml` | Sí (usa los 5 niveles) | **No** — ese blueprint necesita 5 niveles de clic distintos y AceButton solo ofrece 2 (single/double) |
| Compatible con `luz_pulsador.yaml` | Sí | Sí — el blueprint usa corta/doble/larga (el triple original se remapeó a doble, ver su propio historial de cambios) |
| Compatible con `persiana_pulsador.yaml` | Sí | Sí — solo usa larga/fin de larga |

**Cuándo usar cuál:**

- **Por defecto, usa `mega_pulsadores/` (OneButton).** Es el firmware
  con más funcionalidad, y en la mayoría de unidades caben de sobra los
  pulsadores que tengas cableados sin acercarte al límite de RAM (ver
  "RAM / límite de pulsadores" en `todo.md`: 12 pulsadores con los 7
  triggers activos arrancan bien en placa real).
- **Usa `mega_pulsadores_low_ram/` (AceButton) SOLO si** necesitas
  cablear más pulsadores en una unidad de los que caben con OneButton
  Y confirmas que ninguno de esos pulsadores necesita
  `persiana_pulsador_completo.yaml` (triple/cuádruple/quíntuple). Mide
  RAM real antes de decidir — no asumas el límite, ver
  `mega_pulsadores/instructions.md`.
- Si una unidad concreta necesita más pulsadores que OneButton permite
  Y alguno de ellos SÍ necesita triple/cuádruple/quíntuple, la solución
  no es cambiar de librería en esa unidad — es repartir los pulsadores
  entre más unidades físicas (arquitectura ya lo permite: `PLACA_C`,
  `board_config_c.h`, tercer byte de MAC, mismo patrón que A/B — ver
  `todo.md`).

El análisis completo (comparativa evento por evento con otras
librerías, por qué se descartaron Bounce2/ezButton/Button2) está en
`mega_pulsadores/to_review.md`.

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
- **[AceButton](https://github.com/bxparks/AceButton)** — usada solo en
  `mega_pulsadores_low_ram` (no en `mega_pulsadores`), como alternativa
  a `OneButton` con mucha menos RAM por pulsador (~18-26 bytes/instancia
  frente a ~90-100 de `OneButton`). Detecta corta/doble/larga/fin de
  larga (`kEventClicked`/`kEventDoubleClicked`/`kEventLongPressed`/
  `kEventLongReleased`) con debounce incluido, pero **no tiene ningún
  mecanismo** para triple/cuádruple/quíntuple clic — a diferencia de
  `OneButton`, no es algo que se pueda activar, la librería solo
  distingue single vs. double click. Ver "¿`mega_pulsadores` o
  `mega_pulsadores_low_ram`?" más arriba antes de elegir cuál usar.
- **Ethernet** — incluida en el IDE de Arduino, para el shield W5100/W5500.

## Arquitectura de entidades en Home Assistant

- **mega_pulsadores (A y B)**: no crean entidades de estado, solo
  `HADeviceTrigger` — aparecen en HA como "Device" en el trigger de una
  automatización, no como entidad con estado. Por cada pulsador
  (`p14`, `p27`... el nombre lleva el número de pin) hay hasta 7
  triggers posibles: `ButtonShortPressType`, `ButtonDoublePressType`,
  `ButtonTriplePressType`, `ButtonQuadruplePressType`,
  `ButtonQuintuplePressType`, `ButtonLongPressType` y
  `ButtonLongReleaseType` (se dispara al soltar una pulsación larga —
  útil para automatizaciones "mantener pulsado para mover / soltar para
  parar", p. ej. persianas). Cada uno se activa/desactiva por separado
  con `HABILITAR_CORTA`/`HABILITAR_DOBLE`/etc. en `mega_pulsadores.ino`
  (por defecto: corta/doble/larga/largaFin activos, triple/cuádruple/
  quíntuple desactivados — ahorra RAM, ver "RAM / límite de pulsadores"
  en `todo.md`). También existe en el enum de ArduinoHA (aunque este
  proyecto no lo usa): `ButtonShortReleaseType`.
- **mega_pulsadores_low_ram (A y B)**: igual que `mega_pulsadores`, pero
  solo 4 triggers posibles por pulsador: `ButtonShortPressType`,
  `ButtonDoublePressType`, `ButtonLongPressType` y
  `ButtonLongReleaseType` — sin triple/cuádruple/quíntuple, AceButton no
  los soporta (ver "¿`mega_pulsadores` o `mega_pulsadores_low_ram`?"
  más arriba).
- **mega_dispositivos (A y B)**: crean entidades reales:
  - `HASwitch` por luz (`luz_22`, `luz_30`... nombre = número de pin) —
    on/off.
  - `HACover` por persiana (`persiana_38_39`, `persiana_41_42`... nombre
    = pin de "subir" seguido del pin de "bajar" del par, en ese orden
    siempre) — soporta abrir/cerrar/parar. Parar = poner los dos relés
    (subir/bajar) a LOW simultáneamente.

## Configuración antes de subir el firmware

### IP y credenciales MQTT (`config.h`)

`BROKER_ADDR`, `MQTT_USER` y `MQTT_PASS` NO están escritos directamente en
los `.ino` — viven en un fichero `config.h` que cada `.ino` incluye con
`#include "config.h"`.

`config.h` SÍ viene incluido en el repo (con los valores vacíos/de
ejemplo) para que el proyecto compile nada más clonarlo, sin pasos
adicionales. Para que tus credenciales reales no se suban por error, el
repo tiene marcados ambos `config.h` (`mega_pulsadores/config.h` y
`mega_dispositivos/config.h`) con
`git update-index --skip-worktree`: una vez rellenes tus datos reales,
git deja de detectar cambios en ese fichero y nunca aparecerá en
`git status` ni se subirá en un commit.

Para configurarlo, en `mega_pulsadores/` y en `mega_dispositivos/`:

1. Abre directamente `config.h` (ya existe, con placeholders vacíos).
2. Rellena `BROKER_ADDR`, `MQTT_USER` y `MQTT_PASS` con tus valores reales.

(`config.h.example` es solo la plantilla documentada de referencia —
no hace falta copiarla, `config.h` ya está listo para editar.)

Como las 2 unidades de cada rol (A y B) comparten el mismo broker MQTT,
normalmente usarás el mismo `config.h` en ambas — solo cambia el
`#define PLACA_A`/`PLACA_B` para diferenciarlas.

⚠️ **`mega_pulsadores_low_ram/` es distinto: NO trae `config.h`
todavía, solo `config.h.example`** — no está `skip-worktree`d porque
ese comando solo puede aplicarse a un fichero que ya existe en el
repo, y este `config.h` en concreto no se ha creado nunca (a
diferencia de los otros dos, que sí vienen con placeholders vacíos
trackeados). Si vas a usar este firmware, sigue estos pasos EN ORDEN
antes de rellenar credenciales reales:

1. Copia `config.h.example` como `config.h` en `mega_pulsadores_low_ram/`.
2. **Antes de rellenar nada real**, marca el fichero para que git
   ignore futuros cambios:
   `git update-index --skip-worktree mega_pulsadores_low_ram/config.h`
3. Ahora sí, rellena `BROKER_ADDR`/`MQTT_USER`/`MQTT_PASS` con tus
   valores reales — git ya no detectará ese cambio.

Si rellenas credenciales reales ANTES del paso 2, revisa `git status`
— si `mega_pulsadores_low_ram/config.h` aparece como modificado/nuevo,
NO hagas commit todavía: aplica el `skip-worktree` del paso 2 primero.

### Pines, IP fija y demás identidad por unidad (`board_config_a.h` / `board_config_b.h`)

- `PINES_BOTONES` (`mega_pulsadores/board_config_a.h` /
  `board_config_b.h`) y `PINES_LUCES`/`PINES_PERSIANAS`
  (`mega_dispositivos/board_config_a.h` / `board_config_b.h`): a
  diferencia del broker/credenciales, estos SÍ se editan directamente en
  su fichero, porque son distintos en cada una de las 2 unidades físicas
  de cada rol según lo que tengan cableado. No es necesario que las 2
  unidades de un mismo rol tengan la misma cantidad ni el mismo tipo de
  dispositivos — por ejemplo, nada impide que la unidad A de
  `mega_dispositivos` sea solo luces y la B tenga luces y persianas
  mezcladas.
- `IP_ESTATICA`: IP fija de esa unidad en la red local. Debe quedar
  fuera del rango DHCP de tu router (o reservada para su MAC) y ser
  distinta entre A y B para no colisionar.

El `unique_id` de cada entidad (`p14`, `luz_22`,
`persiana_38_39`...) se genera a partir del **número de pin**, no de la
posición en el array. Puedes reordenar, insertar o borrar pines libremente en
cualquier momento sin que ninguna entidad ya renombrada en Home
Assistant cambie de identidad — y mirando el pin en el propio Arduino
sabes directamente qué entidad es en HA, sin tener que contar
posiciones en una lista.

### Calibración de persianas (`TIEMPOS_PERSIANAS`)

Los motores de persiana no tienen encoder, así que `mega_dispositivos`
estima la posición (0-100%) por tiempo de relé activo. Cada persiana
necesita su propio tiempo de recorrido completo (subida y bajada, pueden
ser distintos) en `TIEMPOS_PERSIANAS` (`mega_dispositivos/board_config_a.h`
/ `board_config_b.h`), en el **mismo orden e índice** que
`PINES_PERSIANAS` — la primera pareja de `TIEMPOS_PERSIANAS` corresponde
a la primera pareja de pines de `PINES_PERSIANAS`, y así sucesivamente.

Por defecto ambos valores están puestos a 20000 ms (20s) como placeholder
en las 8 persianas — hay que sustituirlos por el tiempo real de cada una:

1. Sube o baja la persiana hasta un extremo real conocido.
2. Cronometra cuánto tarda en llegar al otro extremo (mejor pasarse un
   poco de margen que quedarse corto — quedarse corto hace que se pare
   antes de llegar de verdad al tope).
3. Pon ese tiempo en milisegundos en `subida_ms`/`bajada_ms` de esa
   persiana en `TIEMPOS_PERSIANAS`.

Sin esta calibración, la posición reportada a HA no coincide con la
posición real de la persiana, aunque el control abrir/cerrar/parar sigue
funcionando igual.

Ver [todo.md](todo.md) para la lista completa de tareas pendientes.

## Seguridad / notas de hardware

- Relés de persiana: nunca activar subir y bajar a la vez. Protección
  actual = `RETARDO_INVERSION_MS` (200ms) en software entre apagar un
  sentido y encender el otro. Si el módulo de relés ya tiene interlock por
  hardware, este retardo se puede reducir.
- Relés de persiana: si una persiana lleva más de
  `TIEMPO_MAX_MOVIMIENTO_MS` (20s por defecto, variable global en
  `mega_dispositivos.ino`) en movimiento sin llegar a un extremo
  calibrado ni recibir STOP, se para sola — protege el motor si HA no
  llega a enviar la orden de parar (fallo de red, etc.). Ajusta este
  valor si tus persianas tardan más de 20s en un recorrido completo.
- Comprobar si los módulos de relé son activos en HIGH o en LOW antes de
  tocar la lógica de `digitalWrite` (los sketches actuales asumen activo en
  HIGH).
