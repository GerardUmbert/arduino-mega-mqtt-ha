# Glosario

`Área` (HA)
:   Agrupación de entidades por habitación en Home Assistant
    (Ajustes → Áreas y zonas). Varios blueprints (`luz_pulsador.yaml`,
    `persiana_pulsador_completo.yaml`) usan el Area del *device* del
    pulsador para saber qué otras luces/persianas afectar en
    pulsaciones "de grupo".

`device trigger`
:   Tipo de entidad de ArduinoHA (`HADeviceTrigger`) que no tiene
    estado propio — solo dispara un evento MQTT una vez, consumido
    como trigger de una automatización en HA. Es lo que usan
    `mega_pulsadores`/`mega_pulsadores_low_ram` para cada tipo de
    pulsación.

`HABILITAR_*`
:   `#define` en el `.ino` de pulsadores que activa/desactiva un tipo
    de evento concreto (p. ej. `HABILITAR_TRIPLE`). Comentar la línea
    lo desactiva; ahorra RAM al no crear ese `HADeviceTrigger`.

`HAButton`
:   Clase de ArduinoHA para crear un botón **con entidad real**, a
    diferencia de `HADeviceTrigger` — visible en Ajustes → Entidades y
    pulsable desde cualquier tarjeta de Lovelace. Usado desde la
    versión 1.8.0 para el "botón virtual" de cada pulsador: al pulsarlo
    en HA, simula un clic corto inyectado en la misma lógica de
    debounce/multiclic que procesa las pulsaciones físicas. Ver
    [`mega_pulsadores`](../firmware/mega-pulsadores.md#boton-virtual-simular-pulsaciones-desde-ha).

`HADeviceTrigger`
:   Clase de ArduinoHA para crear un device trigger. Cada instancia
    representa un tipo de pulsación concreto (`ButtonShortPressType`,
    `ButtonLongPressType`...) para un pulsador concreto.

`OneButton` / `AceButton`
:   Las dos librerías de terceros usadas para leer pulsadores físicos
    (debounce + detección de clics/pulsación larga). Ver
    [Firmware de pulsadores](../firmware/index.md).

`PLACA_A` / `PLACA_B`
:   `#define` al principio de cada `.ino` que decide, en tiempo de
    compilación, la identidad de la unidad física que se está
    flasheando (pines, MAC, IP, nombre en HA). Ver
    [Identificación de unidades A/B](../getting-started/unidades.md).

`skip-worktree`
:   Flag de git (`git update-index --skip-worktree <fichero>`) que
    hace que git ignore cambios futuros en un fichero ya trackeado.
    Usado en los `config.h` para poder rellenarlos con credenciales
    reales sin arriesgarse a subirlas por error. Ver
    [Configuración (config.h)](../getting-started/config.md).

`subtype`
:   Identificador de un pulsador concreto dentro de un device MQTT,
    con formato `pNN` (NN = número de pin, p. ej. `p14`). Es el valor
    que se copia al campo "Subtype del botón" al instanciar un
    blueprint.

`unique_id`
:   Identificador único de una entidad en HA. Para
    luces/persianas/pulsadores de este proyecto, se genera siempre a
    partir del número de **pin**, nunca de la posición en el array de
    configuración — así se pueden reordenar pines sin romper
    automatizaciones ya creadas.
