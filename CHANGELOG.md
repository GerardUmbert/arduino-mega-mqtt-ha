# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).
La versión de cada entrada debe coincidir con
`device.setSoftwareVersion(...)` en el `.ino` correspondiente
(`mega_pulsadores`, `mega_pulsadores_low_ram` o `mega_dispositivos`) —
es lo que Home Assistant muestra como versión de firmware de cada
dispositivo.

Cada `.ino` versiona de forma **independiente** — un cambio en uno no
implica tocar la versión de los otros dos.

## Tags de git

Cada bump de versión de un `.ino`, y cada cambio relevante en un
blueprint, se marca con un tag de git — formato `<carpeta>/vX.Y.Z`:

- Firmware: `mega_pulsadores/vX.Y.Z`, `mega_pulsadores_low_ram/vX.Y.Z`,
  `mega_dispositivos/vX.Y.Z` — el número debe coincidir con el
  `device.setSoftwareVersion(...)` de ese `.ino` en el commit tageado.
- Blueprints (`home_assistant/blueprints/*.yaml`, que no llevan número
  de versión dentro del propio fichero): `blueprints/<nombre>/vX.Y.Z`
  (p. ej. `blueprints/luz_pulsador/v1.1.0`) — empieza en `v1.0.0` la
  primera vez que se tageé cada blueprint.

## [1.8.2] - 2026-09-05 (mega_pulsadores_low_ram)

### Fixed
- **`larga (fin)` nunca se disparaba** al soltar tras una pulsación
  larga, confirmado en placa real: `AceButton::checkReleased()` solo
  despacha `kEventLongReleased` si además de `kFeatureLongPress` está
  activo `kFeatureSuppressAfterLongPress` (confirmado leyendo el código
  fuente) — sin ese flag, el release tras una pulsación larga se
  despacha como `kEventReleased` genérico, que este firmware no
  gestiona. Arreglo: se separó el `#ifdef HABILITAR_LARGA_FIN` de
  `HABILITAR_LARGA` y se añadió el flag que faltaba.
- **Corta y doble nunca se detectaban con pulsaciones de dedo
  normales**, confirmado en placa real con medición directa de
  `digitalRead()` (pulsaciones reales de ~220-290ms): el `kClickDelay`
  por defecto de AceButton es 200ms, y `AceButton::checkClicked()`
  descarta el evento EN SILENCIO en cuanto `elapsedTime >= getClickDelay()`
  — cualquier pulsación de dedo normal ya caía por encima del umbral.
  Arreglo: `cfg->setClickDelay(400)`, con margen de sobra frente al
  umbral de pulsación larga (1000ms).
- **Doble clic salía siempre como "corta" seguido de "doble"**, nunca
  solo "doble", por rápido que se hiciera el doble toque: sin
  `kFeatureSuppressClickBeforeDoubleClick`, `AceButton::checkClicked()`
  despacha el `kEventClicked` del primer toque de inmediato y solo
  añade el `kEventDoubleClicked` después si llega un segundo toque a
  tiempo — nunca sustituye al primero (confirmado leyendo el código
  fuente, `AceButton.cpp`). Arreglo: se activa ese flag junto con
  `kFeatureDoubleClick`, que pospone el "corta" hasta confirmar que no
  hay doble, y lo suprime del todo si lo hay. Efecto secundario
  esperado y documentado por la propia librería: el "corta" ahora
  tarda ~`getClickDelay() + getDoubleClickDelay()` (≈800ms con la
  config actual) en aparecer tras soltar, para poder esperar un
  posible segundo toque.

## [1.8.1] - 2026-09-05 (mega_pulsadores_low_ram)

### Fixed
- **Bug real de carrera en el botón virtual**, detectado en placa real:
  el pulsador físico se quedaba "atascado" detectando solo pulsación
  larga (nunca corta), y el botón virtual generaba eventos corruptos
  (varios `larga (inicio)` seguidos para un solo tap, "corta" disparado
  aunque se pulsara varias veces rápido, sin release limpio detectado).
  Causa: `loop()` caducaba la bandera de simulación (`simulacionSoltarEn[i] = 0`)
  en un paso separado ANTES de llamar a `botones[i].check()` — así que
  la llamada a `check()` justo en el instante de caducidad caía en
  `digitalRead()` del pin real (potencialmente flotante o con rebote)
  en vez de en un `HIGH` limpio, generando transiciones
  Released→Pressed→Released espurias que corrompían el estado interno
  de `AceButton` (`kFlagPressed` quedaba mal) — y como el pulsador
  físico comparte la misma instancia `AceButton` que el botón virtual,
  heredaba la corrupción.
  Arreglo: la comprobación de caducidad se mueve dentro de
  `ButtonConfigConSimulacion::readButton()`, en la misma llamada que
  `AceButton` usa para decidir press/release — sin ninguna ventana de
  carrera entre "caducar" y "leer". `loop()` ya no decide nada sobre el
  nivel del pin, solo limpia la bandera como bookkeeping después de
  `check()`.

## [1.8.0] - 2026-09-05 (mega_pulsadores y mega_pulsadores_low_ram)

Afecta a `mega_pulsadores` (1.7.1 → 1.8.0) y `mega_pulsadores_low_ram`
(1.7.1 → 1.8.0) — mismo cambio implementado en ambos firmwares, con
mecanismos distintos por librería.

### Added
- **Botón virtual por pulsador**: un `HAButton` nuevo por cada
  pulsador, visible y pulsable en la UI de Home Assistant (Ajustes →
  Entidades, o en cualquier tarjeta) — a diferencia de
  `HADeviceTrigger`, que no tiene estado ni aparece en la UI. Al
  pulsarlo desde HA, el firmware inyecta un clic corto (~90ms,
  `SIMULACION_PULSO_MS`) directamente en la máquina de estados de
  debounce/multiclic de ese pulsador, sin tocar el pin físico —
  pulsar el botón de HA varias veces seguidas rápido se detecta como
  doble/triple/cuádruple/quíntuple clic exactamente igual que con el
  dedo, porque es la MISMA lógica de detección la que decide, no una
  ruta paralela.
  - En `mega_pulsadores` (OneButton): usa `OneButton::tick(bool)`,
    método público documentado de la librería para alimentar el
    estado sin leer el pin (`digitalRead`).
  - En `mega_pulsadores_low_ram` (AceButton): usa una subclase de
    `ButtonConfig` que sobreescribe `readButton(pin)` (punto de
    inyección oficial de la librería, documentado como "Override to
    use something other than digitalRead()") para devolver un nivel
    simulado mientras haya una simulación activa para ese pin.
  - unique_id del botón virtual: `"v" + pNN` (ej. `vp14`) — buffer
    distinto de `idBoton` para no confundirlo con el subtype de los
    `HADeviceTrigger`.
  - **Limitación deliberada**: el pulso simulado es corto y fijo,
    pensado para corta/doble/triple/cuádruple/quíntuple. No simula
    pulsación larga (necesitaría mantener el nivel activo un tiempo
    variable) — queda fuera de esta primera versión.
  - Coste de RAM: una entidad `HAButton` más por pulsador (además de
    los `HADeviceTrigger` ya existentes) — el tamaño de `HAMqtt` se
    ajustó para reservar sitio para ella (`NUM_TRIGGERS_POR_PULSADOR + 1`
    por pulsador en vez de `NUM_TRIGGERS_POR_PULSADOR`).

Además de estos tags por componente, sigue existiendo un tag de
proyecto entero (`vX.Y.0`, sin subcarpeta) para checkpoints generales
del repositorio (el último es `v1.5.0`) — no sustituye a los tags por
componente, es un nivel aparte.

Nota histórica: entre el 2026-09-04 (commit `d9fbdb5`) y este cambio,
los tres `.ino` compartieron brevemente un único número de versión
(`1.7.1` los tres). Se volvió al versionado independiente el mismo
día — ambos `mega_dispositivos` y `mega_pulsadores_low_ram` retoman
`1.7.1` como punto de partida (no se revierte a su número anterior,
`1.6.3`/`1.0.0-low-ram`, para no generar un salto de versión hacia
atrás), y a partir de aquí cada uno evoluciona por su cuenta.

## [1.7.1] - 2026-09-04 (los 3 firmwares — mega_pulsadores, mega_pulsadores_low_ram, mega_dispositivos)

### Changed
- Unificado el número de versión de los tres `.ino` a `1.7.1` (antes:
  `mega_pulsadores` 1.7.1, `mega_pulsadores_low_ram` 1.0.0-low-ram,
  `mega_dispositivos` 1.6.3, cada uno con su propio historial). Sin
  cambios de comportamiento en ningún firmware — solo el número que
  reporta cada uno a Home Assistant.
- **Revertido el mismo día**: cada `.ino` vuelve a versionar de forma
  independiente (ver nota histórica arriba). Los tres quedan en
  `1.7.1` como nuevo punto de partida común, pero a partir de ahora
  cada uno sube su propio número según sus propios cambios.

## [1.0.0] - 2026-09-04 (mega_pulsadores_low_ram)

Firmware nuevo — primera versión de `mega_pulsadores_low_ram`.

### Added
- Firmware alternativo a `mega_pulsadores` para el mismo rol (leer
  pulsadores físicos y enviar device triggers MQTT a HA), usando
  `AceButton` en vez de `OneButton`: ~18-26 bytes de SRAM por
  pulsador, frente a ~90-100 bytes fijos de `OneButton` (confirmado
  leyendo el código fuente de ambas librerías — no es una diferencia
  de configuración, es el tamaño real de cada clase). A cambio, **no
  soporta triple/cuádruple/quíntuple clic en absoluto** — `AceButton`
  no tiene ningún mecanismo para contar 3+ pulsaciones seguidas, a
  diferencia de `HABILITAR_TRIPLE` etc. en `mega_pulsadores`, que sí
  se pueden activar si hace falta.
- Cubre los mismos 4 eventos que `mega_pulsadores` tiene activados por
  defecto: corta, doble, larga y fin de larga
  (`HABILITAR_CORTA`/`HABILITAR_DOBLE`/`HABILITAR_LARGA`/
  `HABILITAR_LARGA_FIN`, mismo patrón de `#define` que en
  `mega_pulsadores.ino`), mismo formato de subtype `pNN`.
- No sustituye a `mega_pulsadores`: existen como firmwares paralelos,
  cada unidad física se flashea con el que le convenga. Motivo
  (research completo en `mega_pulsadores/to_review.md`):
  `persiana_pulsador_completo.yaml` necesita los 5 niveles de clic a
  la vez, y `AceButton` solo ofrece 2 (single/double) — sustituir
  `OneButton` directamente habría sido una decisión prácticamente
  irreversible para ese blueprint.
- `board_config_a.h`, `board_config_b.h` y `config.h.example` son
  copias independientes de las de `mega_pulsadores/` (Arduino exige
  que los `.h` vivan en la misma carpeta que el `.ino`), no
  compartidas — ver README principal para el aviso de sincronización
  manual.
- ⚠️ **`config.h` de esta carpeta NO está protegido con
  `git update-index --skip-worktree`** como los de `mega_pulsadores/`
  y `mega_dispositivos/`, porque ese comando solo se puede aplicar a
  un fichero que ya exista en el repo y este `config.h` en concreto
  nunca se ha creado. Ver README principal, sección "IP y credenciales
  MQTT", para los pasos exactos antes de rellenar credenciales reales
  aquí.

## [1.7.1] - 2026-09-04 (mega_pulsadores)

Solo afecta a `mega_pulsadores` (versión de firmware 1.7.1).

### Changed
- ⚠️ **Cambio de comportamiento**: el subtype de cada pulsador pasa de
  `"boton_NN"` a `"pNN"` (p. ej. `boton_14` → `p14`), donde NN es el
  número de pin. Motivo: ahorra RAM en `idBoton[NUM_PULSADORES][N]`
  (de 10 a 4 bytes por pulsador — pequeño pero gratis, ver "RAM /
  límite de pulsadores" en `todo.md`). **Cualquier automatización ya
  instanciada desde un blueprint (persiana_pulsador.yaml,
  persiana_pulsador_completo.yaml, luz_pulsador.yaml,
  luz_zigbee_respaldo.yaml) que use el "Subtype del botón" antiguo
  deja de dispararse tras flashear esta versión — hay que volver a
  seleccionar el trigger desde la UI (el subtype guardado ya no
  coincide con el que envía el firmware).** Actualizada toda la
  documentación (`README.md`, `context.md`, `todo.md`, blueprints y su
  README) para reflejar el nuevo formato.
- Los objetos `OneButton` (uno por pulsador) pasan de reservarse en el
  heap (`OneButton* botones[N]; ... new OneButton(pin, true)`) a un
  array estático (`OneButton botones[N];`, configurado después con
  `.setup(pin, INPUT_PULLUP, true)`). Confirmado en el código fuente de
  la librería que `OneButton` tiene constructor por defecto + `setup()`
  para configurar el pin más tarde, así que no hace falta el heap.
  Ahorra el overhead de `malloc` por pulsador (unos pocos bytes cada
  uno, se nota a partir de una docena). Sin cambio de comportamiento —
  es un cambio interno, no afecta a ninguna automatización ni HA.
  `HADeviceTrigger` sigue con `new`/punteros: sus constructores exigen
  tipo+subtype al crearse (sin constructor por defecto ni setter
  posterior), así que un array estático no es viable ahí.

## [1.7.0] - 2026-09-04 (mega_pulsadores)

Solo afecta a `mega_pulsadores` (versión de firmware 1.7.0).

### Added
- Cada uno de los 7 triggers de pulsador (corta, doble, triple,
  cuádruple, quíntuple, larga, fin de larga) ahora se puede activar o
  desactivar por separado, por unidad completa (afecta a todos los
  pulsadores de esa unidad, no a uno solo), comentando/descomentando
  `HABILITAR_CORTA`/`HABILITAR_DOBLE`/`HABILITAR_TRIPLE`/
  `HABILITAR_CUADRUPLE`/`HABILITAR_QUINTUPLE`/`HABILITAR_LARGA`/
  `HABILITAR_LARGA_FIN` al principio del `.ino`. Desactivar cualquiera
  ahorra RAM (deja de crear ese `HADeviceTrigger`, y de asignar el
  array correspondiente, en cada pulsador), pensado para poder cablear
  más pulsadores por unidad de los que caben con los 7 triggers
  completos.

### Changed
- ⚠️ **Cambio de comportamiento**: los triggers de pulsación triple,
  cuádruple y quíntuple ahora están DESACTIVADOS por defecto
  (`HABILITAR_TRIPLE`/`HABILITAR_CUADRUPLE`/`HABILITAR_QUINTUPLE`
  comentados). Corta, doble, larga y fin de larga siguen activos por
  defecto (sin cambio de comportamiento en esos cuatro). Motivo:
  probado en placa real que 12 pulsadores con los 7 triggers completos
  arrancan bien pero 16 ya entra en bucle de reinicio por falta de RAM
  (ver "RAM / límite de pulsadores" en `todo.md`); quitar estos tres
  triggers por defecto deja más margen de pulsadores por unidad para
  el caso común.
  **Si tienes alguna instancia de blueprint que dependa de triple,
  cuádruple o quíntuple, descomenta el `HABILITAR_*` correspondiente
  antes de flashear esta versión — si no, esa automatización deja de
  dispararse sin error visible (el trigger deja de existir, sin más):**
  - `persiana_pulsador_completo.yaml` usa las 5 pulsaciones + larga →
    necesita `HABILITAR_TRIPLE`/`HABILITAR_CUADRUPLE`/`HABILITAR_QUINTUPLE`.
  - `luz_pulsador.yaml` usa corta/triple/larga (el triple es el
    apagado automático a los N minutos) → necesita `HABILITAR_TRIPLE`.

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
