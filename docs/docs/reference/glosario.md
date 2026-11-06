# Glosario

`Área` (HA)
:   Agrupación de entidades por habitación en Home Assistant
    (Ajustes → Áreas y zonas). Varios blueprints (`luz_pulsador.yaml`,
    `persiana_pulsador_completo.yaml`) usan el Area del *device* del
    pulsador para saber qué otras luces/persianas afectar en
    pulsaciones "de grupo".

`device trigger`
:   Mecanismo de ArduinoHA (`HADeviceTrigger`) que **no es una
    entidad** y no tiene estado propio — solo dispara un evento MQTT
    una vez, consumido como trigger de una automatización en HA. Es lo
    que usan `mega_pulsadores`/`mega_pulsadores_low_ram` para cada tipo
    de pulsación.

    ⚠️ **No aparece en Ajustes → Entidades**, ni en *Controls* ni en
    *Activity* de la vista del dispositivo. Su única manifestación en
    la UI es el desplegable de disparadores al crear una automatización,
    y hay que llegar por **"By type" → Device**; la ruta "By target"
    solo ofrece disparadores de entidad. Fuente habitual de confusión:
    ver [Solución de problemas](troubleshooting.md).

`HABILITAR_*`
:   `#define` en el `.ino` de pulsadores que activa/desactiva una
    función concreta. Comentar la línea la desactiva. Los hay de tres
    clases:

    - **Por tipo de evento** (p. ej. `HABILITAR_TRIPLE`,
      `HABILITAR_LARGA_FIN`): ahorra RAM al no crear ese
      `HADeviceTrigger`.
    - **`HABILITAR_BOTON_VIRTUAL`** (desde 1.8.6, solo
      `mega_pulsadores_low_ram`): quita los `HAButton` y su andamiaje
      de simulación (~31 bytes/pulsador). No afecta a los pulsadores
      físicos ni a sus device triggers.
    - **`HABILITAR_DEBUG`** (desde 1.8.7, solo
      `mega_pulsadores_low_ram`): quita toda la instrumentación por
      Serial. Además de RAM, evita que cada `Serial.print` bloquee el
      loop, cosa que importa porque `AceButton` necesita `check()` cada
      <5ms.

`HAButton`
:   Clase de ArduinoHA para crear un botón **con entidad real**, a
    diferencia de `HADeviceTrigger` — visible en Ajustes → Entidades y
    pulsable desde cualquier tarjeta de Lovelace. Usado desde la
    versión 1.8.0 para el "botón virtual" de cada pulsador: al pulsarlo
    en HA, simula un clic corto inyectado en la misma lógica de
    debounce/multiclic que procesa las pulsaciones físicas. **Solo
    simula pulsación corta**, nunca larga (el pulso es corto y fijo,
    `SIMULACION_PULSO_MS`). En `mega_pulsadores_low_ram` se puede
    desactivar con `HABILITAR_BOTON_VIRTUAL` desde la 1.8.6. Ver
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

`UDP directo` (prototipo)
:   Camino alternativo entre las placas, sin broker ni Home Assistant,
    que usan `mega_pulsadores_low_ram_udp` y `mega_dispositivos_udp`.
    El pulsador envía un paquete UDP broadcast de 5 bytes por cada
    evento y el receptor lo traduce a una acción con una tabla de
    reglas. Convive con MQTT/HA, que sigue publicando triggers y
    estados. Ver [`mega_pulsadores_low_ram_udp`](../firmware/mega-pulsadores-low-ram-udp.md).

`regla UDP`
:   Una fila de `udp_reglas.h` en `mega_dispositivos_udp`:
    `{placa, pin_pulsador, evento, acción, destino}`. Define qué hace
    una pulsación corta, doble, larga o de fin de larga sobre una luz o
    una persiana concreta. Varias filas pueden compartir pulsador y
    evento. Ver [`mega_dispositivos_udp`](../firmware/mega-dispositivos-udp.md#tabla-de-reglas).

`UDP_MODO`
:   Ajuste de `mega_pulsadores_low_ram_udp`. `UDP_MODO_SOLO_SIN_MQTT`
    (por defecto) envía UDP solo mientras MQTT está caído;
    `UDP_MODO_SIEMPRE` lo envía siempre, con el riesgo de doble acción
    si además hay blueprints de HA sobre ese pulsador.
