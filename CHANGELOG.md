# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).
La versión aquí debe coincidir con `device.setSoftwareVersion(...)` en
ambos `.ino` — es lo que Home Assistant muestra como versión de firmware
de cada dispositivo.

## [1.7.0] - 2026-09-04 (mega_pulsadores)

Solo afecta a `mega_pulsadores` (versión de firmware 1.7.0).

### Changed
- ⚠️ **Cambio de comportamiento**: los triggers de pulsación cuádruple
  y quíntuple ahora están DESACTIVADOS por defecto
  (`HABILITAR_CUADRUPLE`/`HABILITAR_QUINTUPLE` comentados al principio
  del `.ino`). Motivo: probado en placa real que 12 pulsadores con los
  7 triggers completos arrancan bien pero 16 ya entra en bucle de
  reinicio por falta de RAM (ver "RAM / límite de pulsadores" en
  `todo.md`); quitar estos dos triggers por defecto deja más margen de pulsadores
  por unidad para el caso común, ya que no hay uso confirmado todavía
  de pulsación 4/5 salvo en `persiana_pulsador_completo.yaml`.
  **Si tienes alguna instancia de `persiana_pulsador_completo.yaml`
  usando pulsación 4 (ajuste fino ±5%) o 5 (todas las persianas de la
  casa), descomenta `HABILITAR_CUADRUPLE`/`HABILITAR_QUINTUPLE` antes
  de flashear esta versión — si no, esas automatizaciones dejan de
  dispararse sin error visible (el trigger deja de existir, sin más).**

### Added
- Los triggers de pulsación cuádruple y quíntuple ahora se pueden
  activar/desactivar por unidad completa (afecta a todos los
  pulsadores de esa unidad, no a uno solo) comentando/descomentando
  `HABILITAR_CUADRUPLE`/`HABILITAR_QUINTUPLE` al principio del `.ino`.
  Desactivar cualquiera de las dos ahorra RAM (deja de crear ese
  `HADeviceTrigger` en cada pulsador), pensado para poder cablear más
  pulsadores por unidad de los que caben con los 7 triggers completos.

## [1.6.3] - 2026-09-04 (mega_pulsadores)

Solo afecta a `mega_pulsadores` (versión de firmware 1.6.3).

### Fixed
- `setup()` adjuntaba los callbacks de `OneButton` (`attachClick`,
  `attachDoubleClick`, `attachMultiClick`, `attachLongPressStart`,
  `attachLongPressStop`) con lambdas que capturaban `idx` por valor
  (`[idx](){...}`). Esa firma no convierte a `callbackFunction` (function
  pointer puro sin captura) y algunas instalaciones de la librería
  `OneButton` no exponen la sobrecarga que sí acepta lambdas con
  captura, dando "no matching function for call to ...attachClick(...)"
  al compilar. Cambiado a la sobrecarga `parameterizedCallbackFunction`
  de `OneButton`: funciones sin captura (`onClick`, `onDoubleClick`,
  etc.) que reciben el índice del pulsador como `void*` en vez de
  capturarlo.

## [1.6.3] - 2026-09-04 (mega_dispositivos)

Solo afecta a `mega_dispositivos` (versión de firmware 1.6.3).

### Fixed
- Los arrays globales `inicioMovimiento`, `subiendo` y
  `ultimaPublicacionPosicion` (tamaño `NUM_PERSIANAS`) llevaban un
  inicializador explícito (`= {0}` / `= {false}`) que el compilador
  rechaza cuando `NUM_PERSIANAS` es 0 ("too many initializers for ...
  [0]") — caso de una unidad `mega_dispositivos` configurada solo con
  luces, sin persianas. Al ser arrays globales ya se inicializan a cero
  por defecto, así que se ha quitado el inicializador explícito: mismo
  comportamiento con persianas > 0, y ahora compila también con 0.

## [1.6.2] - 2026-09-02

Solo afecta a `mega_dispositivos` (versión de firmware 1.6.2).

### Added
- Cada luz y persiana ahora tiene un nombre visible propio en HA (`Luz
  22`, `Persiana 38/39`) en vez del genérico "MQTT Switch"/"MQTT Cover"
  con sufijo numérico automático. El `unique_id` (`luz_22`,
  `persiana_38_39`) no cambia — solo el nombre que se ve, que sigue
  siendo renombrable a mano en HA como siempre.
- `HADeviceTrigger` (usado en `mega_pulsadores` para los pulsadores) no
  tiene equivalente de `setName()` en la librería — los triggers de
  botón se identifican en HA por tipo + subtype dentro del device, no
  como entidad individual, así que no aplica el mismo cambio ahí.

## [1.6.1] - 2026-09-02

Solo afecta a `mega_dispositivos` (versión de firmware 1.6.1).

### Fixed
- El auto-stop al llegar a un extremo (añadido en 1.6.0) comprobaba
  `posicion <= 0 || posicion >= 100` sin mirar hacia qué lado se estaba
  moviendo la persiana. Como `posicionActual` arranca en 100 (asumida
  abierta), un CLOSE recién empezado leía posición 100 en la primera
  vuelta de `loop()` (aún no había transcurrido tiempo suficiente para
  que bajara) y se paraba en seco confundiéndolo con "ya llegó abierta
  del todo" — CLOSE y el falso OPEN aparecían casi instantáneos en el
  log. Ahora solo comprueba el extremo de la dirección en curso (100 si
  está subiendo, 0 si está bajando).

## [1.6.0] - 2026-09-02

Solo afecta a `mega_dispositivos` (versión de firmware 1.6.0). `mega_pulsadores`
no controla persianas, se queda en 1.5.1 — a partir de aquí ambos `.ino`
pueden llevar versiones distintas si un cambio solo afecta a uno de los dos.

### Added
- Las persianas ahora reportan posición real (0-100%) a Home Assistant,
  estimada por tiempo de relé activo (sin encoder, no hay otra forma).
  Cada persiana necesita calibrar su tiempo de recorrido completo en
  `TIEMPOS_PERSIANAS` (`board_config_*.h`), en el mismo orden que
  `PINES_PERSIANAS`. Arranca asumiendo 100% (abierta) hasta que el
  usuario la lleve a un extremo real, momento en el que se resincroniza
  sola a 0/100. Al llegar sola a un extremo por tiempo, se para y pasa a
  `open`/`closed` en vez de quedarse indefinidamente con el relé
  activado.
- Parada de seguridad general a los `TIEMPO_MAX_MOVIMIENTO_MS` (20s por
  defecto, variable global en `mega_dispositivos.ino`): si una persiana
  lleva ese tiempo en movimiento sin llegar a un extremo ni recibir
  STOP, se para sola para no forzar el motor.

### Fixed
- Sin `HACover::PositionFeature`, Home Assistant no tiene un estado real
  de "parada a medias": al recibir `stopped` sin posición conocida,
  colapsa el estado a `open` o `closed` según lo que estuviera haciendo
  justo antes (comportamiento documentado de MQTT Cover, no un fallo del
  firmware) — de ahí que pulsar "stop" pareciera no hacer nada y el
  botón de sentido contrario quedara bloqueado. Ahora que cada `HACover`
  se crea con `PositionFeature` y reporta una posición real, `stopped`
  se refleja correctamente y ambos botones (abrir/cerrar) quedan
  disponibles tras parar a medio recorrido.
- Cambiar de sentido (p. ej. pulsar "abrir" mientras estaba cerrando)
  sin haber parado antes perdía el tramo recorrido hasta ese momento y
  desincronizaba la posición estimada; ahora se congela la posición
  real recorrida antes de invertir el sentido.

## [1.5.1] - 2026-09-02

### Added
- Más diagnóstico por Serial tras `Ethernet.begin()` en ambos `.ino`:
  se imprime también el gateway, la máscara de subred y el DNS
  realmente aplicados (`Ethernet.gatewayIP()`, `subnetMask()`,
  `dnsServerIP()`), y una prueba de conexión TCP directa al broker en
  el puerto 1883 (sin pasar por MQTT) antes de `mqtt.begin()`, para
  distinguir un fallo de red/alcanzabilidad de un fallo del propio
  protocolo MQTT. En `loop()`, si sigue sin conectar, se imprime un
  aviso cada 5 segundos en vez de quedarse en silencio indefinidamente.
  Este diagnóstico ayudó a identificar un caso real de shield Ethernet
  defectuoso (enlace detectado como activo pero sin tráfico real
  saliendo a la red: sin respuesta a ping, sin entrada ARP, TCP
  fallando siempre).

## [1.5.0] - 2026-09-02

### Fixed
- `Ethernet.begin(mac, IP_ESTATICA)` en ambos `.ino` solo fijaba la IP:
  la librería Ethernet asumía un gateway por defecto que no coincidía
  con el router real, así que la placa se quedaba con IP fija pero sin
  ruta de salida — nunca llegaba a conectar con el broker MQTT aunque
  el log por Serial no mostrara ningún error explícito, y el dispositivo
  no aparecía ni en el listado de clientes del router ni en los logs de
  Mosquitto. Ahora cada `board_config_*.h` define `IP_GATEWAY` e
  `IP_SUBNET` junto a `IP_ESTATICA`, y ambos `.ino` llaman a
  `Ethernet.begin(mac, ip, dns, gateway, subnet)` pasando el gateway
  real explícitamente.

### Added
- Diagnóstico adicional por Serial tras `Ethernet.begin()` en ambos
  `.ino`: distingue entre shield Ethernet no detectado
  (`Ethernet.hardwareStatus()`) y cable sin enlace
  (`Ethernet.linkStatus()`), para poder diferenciar un fallo de
  hardware/cableado de un fallo de gateway/red sin necesidad de acceso
  físico a la placa.

## [1.4.0] - 2026-08-02

### Changed
- Ambos `.ino` ya no piden IP por DHCP (`Ethernet.begin(mac)`): ahora
  usan IP fija (`Ethernet.begin(mac, IP_ESTATICA)`), con la IP definida
  junto a la MAC en el `board_config_*.h` de cada unidad. Se elimina la
  rama de error "fallo DHCP" y la llamada `Ethernet.maintain()` en
  `loop()` (solo aplicable a renovación de lease DHCP).
- Los ficheros `pines_a.h`/`pines_b.h` de ambos sketches se renombran a
  `board_config_a.h`/`board_config_b.h`, y pasan a concentrar TODA la
  identidad de cada unidad física: pines cableados, MAC, IP fija y
  nombre en HA (`NOMBRE_PLACA`). Antes la MAC y el nombre vivían en el
  `.ino`, seleccionados con `#if defined(PLACA_A)/(PLACA_B)`; ahora ese
  bloque desaparece del `.ino` y cada `board_config_*.h` define sus
  propios `mac[]` y `NOMBRE_PLACA` directamente, sin condicionales.

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
