# Blueprints de Home Assistant

## `persiana_posicion.yaml`

Añade una posición estimada (0-100%) a una persiana controlada por
`mega_dispositivos`, cuyo firmware **no** reporta posición real (no hay
encoder en los motores — ver `todo.md` del repo). La estimación es
puramente por tiempo: se calibra cuánto tarda la persiana en abrir/cerrar
del todo, y se calcula el tiempo de relé necesario para moverla un % dado.

Cada vez que la persiana llega a abierta o cerrada del todo (ya sea
pedido desde este blueprint, desde la tarjeta normal de HA, o desde un
pulsador físico vía automatización aparte), la posición guardada se
resincroniza a 0/100 exacto, así que el error de estimación no se
acumula indefinidamente entre usos — solo importa la deriva dentro de un
mismo tramo sin pasar por un extremo.

### Helpers necesarios por persiana

Antes de instanciar el blueprint para una persiana (p. ej.
`cover.persiana_38_39`), crea 4 helpers `input_number`
(Ajustes → Dispositivos y servicios → Ayudantes → Crear ayudante →
Número):

| Helper | Min | Max | Paso | Unidad | Para qué |
|---|---|---|---|---|---|
| `input_number.persiana_38_39_posicion` | 0 | 100 | 1 | % | Posición estimada actual (la gestiona el blueprint, no la toques a mano salvo para corregirla manualmente si sabes que está mal). |
| `input_number.persiana_38_39_objetivo` | 0 | 100 | 1 | % | Slider que usas para pedir "ir a X%". Cambiar su valor dispara el movimiento. |
| `input_number.persiana_38_39_tiempo_abrir` | 0 | 60 | 0.5 | s | Segundos que tarda en abrir del todo (calibrado a mano, ver abajo). |
| `input_number.persiana_38_39_tiempo_cerrar` | 0 | 60 | 0.5 | s | Segundos que tarda en cerrar del todo. |

Usa el mismo sufijo de pines que ya usa la entidad `cover` (p. ej.
`_38_39`) para no perder la referencia de a qué persiana pertenece cada
helper.

Alternativa más rápida si tienes varias persianas: añade los helpers
por YAML en vez de crearlos uno a uno desde la UI. Tienen que declararse
bajo la clave `input_number:` de `configuration.yaml` (o un paquete
aparte incluido desde ahí):

```yaml
input_number:
  persiana_38_39_posicion:
    name: Persiana 38/39 - Posición actual
    min: 0
    max: 100
    step: 1
    unit_of_measurement: "%"
  persiana_38_39_objetivo:
    name: Persiana 38/39 - Objetivo
    min: 0
    max: 100
    step: 1
    unit_of_measurement: "%"
  persiana_38_39_tiempo_abrir:
    name: Persiana 38/39 - Tiempo apertura
    min: 0
    max: 60
    step: 0.5
    unit_of_measurement: s
  persiana_38_39_tiempo_cerrar:
    name: Persiana 38/39 - Tiempo cierre
    min: 0
    max: 60
    step: 0.5
    unit_of_measurement: s
```

Tras guardar, recarga los helpers (Ajustes → Sistema → Repara e
identifica → ⋮ → Recargar configuración de YAML, sección "Helpers de
input_number") o reinicia HA — no hace falta reiniciar todo el sistema,
con recargar la configuración YAML basta.

### Cómo calibrar `tiempo_abrir` / `tiempo_cerrar`

1. Cierra la persiana del todo manualmente desde HA (botón cerrar) y
   espera a que pare.
2. Pulsa abrir y cronometra hasta que se detenga sola en el tope
   superior. Repite 2-3 veces y usa el promedio.
3. Repite lo mismo para el cierre (desde abierta del todo hasta cerrada).
4. Escribe esos valores en `tiempo_abrir` / `tiempo_cerrar`. Si con el
   uso real ves que la estimación se desvía mucho, ajusta el número —
   no hace falta tocar el blueprint.

### Instanciar el blueprint

Ajustes → Automatizaciones y escenas → Blueprints → importar
`persiana_posicion.yaml` → Crear automatización, y rellenar los 5
inputs (la entidad `cover` y los 4 helpers) para esa persiana en
concreto. Repetir una vez por cada persiana — cada una es una
automatización independiente con sus propios helpers.

### Dashboard

Añade el slider de `input_number.persiana_38_39_objetivo` a la tarjeta
de esa persiana en Lovelace (p. ej. con una tarjeta "Entities" o
"Tile") — así tienes un control de posición aproximada sin que
`mega_dispositivos` necesite reportar `PositionFeature` real.

## `persiana_pulsador.yaml`

Implementa el patrón de `todo.md` → "Persianas controladas desde un
pulsador físico normal": mantener pulsado un botón de la agrupación de 4
sube o baja la persiana, soltar para. Un botón de la agrupación se asigna
a subir, otro a bajar — cada uno es una instancia separada de este
blueprint.

Usa los triggers `button_long_press` (empezar a mantener pulsado) y
`button_long_release` (soltar) que expone `mega_pulsadores` — ver
`mega_pulsadores.ino:127-128` (`ButtonLongPressType` /
`ButtonLongReleaseType`).

### Instanciar el blueprint

Por cada botón de la agrupación que vaya a mover esta persiana (normalmente
2: uno para abrir, otro para cerrar):

1. Ajustes → Automatizaciones y escenas → Blueprints → importar
   `persiana_pulsador.yaml` → Crear automatización.
2. **Pulsador (device)**: elige el device MQTT correspondiente ("Mega
   Pulsadores A" o "B", según en cuál esté cableado el botón físico).
3. **Subtype del botón**: el `object_id` del botón tal cual lo genera el
   firmware, con el número de pin — p. ej. `boton_14` (ver
   `mega_pulsadores.ino`, el nombre sale de `PINES_BOTONES`). Para verlo
   exacto, es más fiable elegir el trigger desde la UI de "Añadir
   disparador" → Device → seleccionar el device y el evento
   "Button Long Press" del botón concreto, y copiar el subtype que HA
   rellena solo.
4. **Persiana**: la entidad `cover` a mover (p. ej. `cover.persiana_38_39`).
5. **Acción al mantener pulsado**: "Abrir / subir" o "Cerrar / bajar"
   según qué botón de la agrupación sea este.

Repite para el segundo botón (el de la acción contraria) sobre la misma
persiana.

### Combinarlo con `persiana_posicion.yaml`

Los dos blueprints pueden convivir sobre la misma persiana sin pisarse:
`persiana_pulsador.yaml` solo llama a `cover.open_cover` /
`close_cover` / `stop_cover`, nunca toca los helpers de posición
directamente. `persiana_posicion.yaml` escucha el estado `open`/`closed`
de la entidad `cover` (venga de donde venga el comando) y resincroniza
`posicion` a 0/100 cuando corresponda — así un recorrido completo hecho
desde el pulsador también deja la posición estimada correcta para la
próxima vez que uses el slider `objetivo`.

Ten en cuenta que si sueltas el pulsador a medio recorrido (sin llegar al
tope), la posición estimada NO se actualiza con ese movimiento — se
queda como estaba hasta el siguiente recorrido completo. Es una
limitación conocida y aceptada del enfoque por tiempo (ver discusión en
el historial del proyecto): el error no se acumula sin límite, pero
tampoco es exacto entre resyncs.

## `persiana_pulsador_completo.yaml`

Alternativa a `persiana_pulsador.yaml` que aprovecha los 5 niveles de
pulsación del botón en vez de solo mantener pulsado. Un botón de la
agrupación de 4 hace SIEMPRE de "subir", otro SIEMPRE de "bajar", para
todas las persianas de esa agrupación:

| Pulsaciones | Botón subir | Botón bajar |
|---|---|---|
| 1 | esta persiana → 100% | esta persiana → 0% |
| 2 | persianas de la misma Area → 100% cada una | ídem → 0% cada una |
| 3 | esta persiana → 50% | esta persiana → 50% |
| 4 | esta persiana → posición actual + 5% | esta persiana → posición actual − 5% |
| 5 | TODAS las persianas de la casa → 100% | TODAS → 0% |
| larga / fin | subir mientras se mantiene, parar al soltar | bajar mientras se mantiene, parar al soltar |

**Dependencia dura**: 1/2/3/4/5 no mueven la persiana directamente —
escriben en el helper `..._objetivo` de cada persiana afectada, y es
`persiana_posicion.yaml` quien la mueve de verdad. Por eso **toda**
persiana que pueda verse afectada (incluidas las de la Area en doble
pulsación, o todas las de la casa en quíntuple) necesita ya su propia
instancia de `persiana_posicion.yaml`, con helpers nombrados
`input_number.<object_id_de_la_cover>_objetivo` /
`..._posicion` — el blueprint deriva esos nombres del `entity_id` de
cada `cover.*`, no los pides a mano uno a uno. Si el nombrado no
coincide para alguna persiana del grupo, esa persiana en concreto
simplemente no se mueve (sin error visible) — revisa el nombrado si ves
alguna que se queda descolgada.

La pulsación larga sí sigue llamando a `cover.*` directamente (igual
que `persiana_pulsador.yaml`), pero al soltar, si la persiana no llegó
a un extremo, estima cuánto se movió a partir de cuánto tiempo estuvo
mantenida y actualiza `posicion` directamente — cierra el hueco que
`persiana_pulsador.yaml` deja en pulsaciones largas parciales.

### Instanciar el blueprint

Dos instancias por persiana (una por botón subir, otra por bajar) — o
más si varias persianas comparten el mismo par de botones subir/bajar:

1. Ajustes → Automatizaciones y escenas → Blueprints → importar
   `persiana_pulsador_completo.yaml` → Crear automatización.
2. **Pulsador (device)** y **Subtype del botón**: igual que en los
   otros blueprints de pulsador.
3. **Persiana controlada por este botón**: la entidad `cover` concreta
   (para 1/3/4/larga; 2/5 se calculan solas a partir de esta).
4. **Dirección**: Subir o Bajar, según qué botón sea este.
5. **Helper de tiempo de apertura/cierre total**: los mismos helpers
   `tiempo_abrir`/`tiempo_cerrar` que ya usa la instancia de
   `persiana_posicion.yaml` de esta persiana.
6. **Helper de inicio de pulsación larga**: crea un `input_datetime`
   nuevo y dedicado a esta instancia concreta (uno por botón, no
   compartido) — Ajustes → Ayudantes → Crear ayudante → Fecha y hora,
   con la opción de hora activada.

## `luz_pulsador.yaml`

Conecta un pulsador físico con una luz (`switch.*`, on/off simple — las
luces actuales no tienen control de brillo, ver `luces_notas.md`):

- Pulsación corta: toggle on/off.
- Pulsación triple: enciende y apaga sola pasados N minutos
  (configurable, 5 por defecto). Volver a pulsar triple antes de que
  expire reinicia el temporizador.

### Instanciar el blueprint

Una instancia por cada luz que quieras controlar así:

1. Ajustes → Automatizaciones y escenas → Blueprints → importar
   `luz_pulsador.yaml` → Crear automatización.
2. **Pulsador (device)** y **Subtype del botón**: igual que en
   `persiana_pulsador.yaml` — elige el device MQTT del pulsador y copia
   el subtype (p. ej. `boton_22`) desde la UI al añadir el disparador.
3. **Luz**: la entidad `switch` a controlar (p. ej. `switch.luz_22`).
4. **Minutos hasta el apagado automático**: ajusta si 5 minutos no es lo
   que quieres para esa luz en concreto.

Ver `luces_notas.md` para más ideas de automatización (doble, cuádruple,
quíntuple, larga).

## `luz_zigbee_respaldo.yaml`

Para una luz cuyo brillo/temperatura de color se controla por una
bombilla Zigbee (no por `mega_dispositivos`, pensado para una IKEA
TRÅDFRI WW/CW regulable). Hace tres cosas independientes:

- **Pulsador físico: toggle de la bombilla + relé a ON siempre**: al
  pulsar el botón (pulsación corta) pasan dos cosas — se hace toggle
  de la bombilla Zigbee (control normal del día a día, enciende/apaga)
  y, a la vez, el relé se fuerza a ON siempre, esté como esté — el
  relé nunca hace toggle ni se apaga desde el pulsador, solo se
  garantiza que esté ON. Así, si el relé se hubiera quedado en OFF por
  lo que sea, pulsar el botón garantiza que la bombilla tiene corriente
  en vez de depender de que el relé ya estuviera bien. Nada de esto
  pasa solo porque HA o MQTT arrancan o se reconectan — el único
  disparador es el pulsador físico.
- **Adaptar la bombilla cuando ella misma vuelve a encenderse**
  (opcional): la bombilla Zigbee ya tiene su propio comportamiento de
  recuperación tras un corte (normalmente vuelve a su último estado).
  Cuando HA detecta que esa bombilla pasa a `on` por su cuenta,
  opcionalmente le aplica un brillo/temperatura de color por defecto en
  vez de dejar lo que la bombilla decidiera por sí sola.
- **Temperatura de color por hora del día** (opcional): cada 15
  minutos ajusta la temperatura de color según la posición del sol
  (`sun.sun`) — cálida de noche, más neutra a mediodía. Solo actúa si
  la bombilla ya está encendida, nunca la enciende/apaga. El rango por
  defecto (250-454 mireds) coincide con el rango real de fábrica de la
  IKEA TRÅDFRI WW/CW.

No requiere cambios en `mega_dispositivos.ino`. Ver `luces_notas.md`
para el detalle completo.

### Instanciar el blueprint

Una instancia por cada luz Zigbee con relé de respaldo:

1. Ajustes → Automatizaciones y escenas → Blueprints → importar
   `luz_zigbee_respaldo.yaml` → Crear automatización.
2. **Relé**: el `switch.*` de `mega_dispositivos` de esa luz.
3. **Bombilla Zigbee**: la entidad `light.*` real.
4. **Pulsador (device) — toggle bombilla + fuerza relé a ON** y
   **Subtype del botón**: el pulsador físico de esta luz — igual que
   en `luz_pulsador.yaml`, elige el device MQTT y copia el subtype
   (p. ej. `boton_22`) desde la UI al añadir el disparador.
5. **Adaptar brillo/temperatura cuando la bombilla vuelve a encenderse
   sola**: opcional, ver `luces_notas.md`.
6. **Ajustar temperatura de color según la hora del día**: opcional,
   activa el ciclo automático cálida/fría. Ajusta
   `temp_calida_mireds`/`temp_fria_mireds` si tu bombilla tiene un
   rango distinto al de la IKEA TRÅDFRI WW/CW.
