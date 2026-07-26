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

### Varias persianas a la vez

Si tienes que dar de alta muchas persianas (p. ej. 7), escribir los 4
helpers a mano por cada una es repetitivo. En vez de copiar el bloque
YAML de arriba 7 veces, genera los 28 helpers con un script a partir de
una lista de nombres — usa el `object_id` que le vayas a dar a cada
`cover` en HA (el nombre final, no el sufijo de pines: si vas a
renombrar las persianas a algo más legible, usa ya ese nombre aquí para
no tener que rehacer esto después):

```python
persianas = ["salon", "cocina", "dormitorio_1", "dormitorio_2", "estudio", "pasillo", "terraza"]

for p in persianas:
    print(f"""  {p}_posicion:
    name: Persiana {p.replace('_', ' ').title()} - Posición actual
    min: 0
    max: 100
    step: 1
    unit_of_measurement: "%"
  {p}_objetivo:
    name: Persiana {p.replace('_', ' ').title()} - Objetivo
    min: 0
    max: 100
    step: 1
    unit_of_measurement: "%"
  {p}_tiempo_abrir:
    name: Persiana {p.replace('_', ' ').title()} - Tiempo apertura
    min: 0
    max: 60
    step: 0.5
    unit_of_measurement: s
  {p}_tiempo_cerrar:
    name: Persiana {p.replace('_', ' ').title()} - Tiempo cierre
    min: 0
    max: 60
    step: 0.5
    unit_of_measurement: s""")
```

Pega la salida bajo `input_number:` en `configuration.yaml`. Cambiar el
nombre de una persiana más adelante solo implica renombrar sus 4
entradas (buscar/reemplazar el slug antiguo por el nuevo) — no afecta al
resto.

### Instanciar el blueprint en varias persianas por YAML

Si tu instalación gestiona las automatizaciones en modo YAML (no solo
UI), puedes instanciar el blueprint para varias persianas escribiendo
directamente bajo `automation:` en vez de repetir el asistente de la UI
7 veces:

```yaml
automation:
  - alias: Persiana Salón - posición
    use_blueprint:
      path: persiana_posicion.yaml
      input:
        cover_entity: cover.salon
        posicion_helper: input_number.salon_posicion
        objetivo_helper: input_number.salon_objetivo
        tiempo_abrir_helper: input_number.salon_tiempo_abrir
        tiempo_cerrar_helper: input_number.salon_tiempo_cerrar
  - alias: Persiana Cocina - posición
    use_blueprint:
      path: persiana_posicion.yaml
      input:
        cover_entity: cover.cocina
        posicion_helper: input_number.cocina_posicion
        objetivo_helper: input_number.cocina_objetivo
        tiempo_abrir_helper: input_number.cocina_tiempo_abrir
        tiempo_cerrar_helper: input_number.cocina_tiempo_cerrar
  # ... repetir una entrada por persiana
```

`path` es relativo a `<config>/blueprints/automation/<usuario_o_carpeta>/`
— ajústalo a donde tengas copiado `persiana_posicion.yaml` dentro de
`blueprints/automation/`. Sigue habiendo una entrada por persiana (HA no
soporta instanciar un blueprint en bucle desde YAML), pero es mucho menos
manual que repetir el asistente gráfico 7 veces, y además queda en
control de versiones junto con el resto de la config.

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

## Persianas que se ajustan solas según el sol: Adaptive Cover (HACS)

No es un blueprint de este repo, sino una **integración externa de HA**
(no Arduino/firmware) que calcula la posición óptima de cada persiana
para bloquear el sol directo, a partir de azimut/elevación del sol
(`sun.sun`) y la orientación de la fachada donde está esa persiana.
Encaja con `persiana_posicion.yaml` sin pisarlo: Adaptive Cover escribe
en el mismo helper `input_number.<cover>_objetivo` que ya usan
`persiana_pulsador_completo.yaml` (pulsaciones 1/2/3/4/5) y el slider
manual — es solo otro "escritor" más del objetivo, igual que ellos.

- Repo: https://github.com/basbruss/adaptive-cover
- Se instala vía HACS (Ajustes → HACS → Integraciones → buscar
  "Adaptive Cover" → instalar → reiniciar HA), luego se añade como
  integración normal (Ajustes → Dispositivos y servicios → Añadir
  integración → "Adaptive Cover").

### Qué pide por persiana al configurarla

- **Orientación de la ventana/fachada** (azimut en grados, 0 = norte,
  90 = este, 180 = sur, 270 = oeste) — el dato clave, tienes que
  saberlo tú por persiana.
- Opcional: alto y ancho de la ventana, distancia a la ventana del
  punto que quieres proteger del sol — mejora la precisión del cálculo
  de sombra, pero no es obligatorio para un primer ajuste.
- Modo **blind** (persiana normal de subir/bajar, tu caso — solo
  calcula cuánto bajarla para cortar el sol directo) frente a modo
  **venetian** (lamas orientables, no aplica aquí).

### Cómo conectarlo a lo que ya tienes

Adaptive Cover puede exponer directamente una entidad `cover` propia
que mueve la persiana real, o (más simple con tu arquitectura actual)
puedes leer su sensor de "posición recomendada" y volcarlo tú al mismo
helper `..._objetivo` que ya usan tus otros blueprints, con una
automatización corta tipo:

```yaml
automation:
  - alias: Persiana Salón - seguir sol (Adaptive Cover)
    trigger:
      - platform: state
        entity_id: sensor.adaptive_cover_salon  # el sensor que cree la integración para esa ventana
    action:
      - service: input_number.set_value
        target:
          entity_id: input_number.salon_objetivo
        data:
          value: "{{ trigger.to_state.state | float(0) }}"
```

Así el cálculo de sol lo hace la integración, pero quien mueve
físicamente el relé sigue siendo `persiana_posicion.yaml` — mismo
mecanismo que ya usas para el pulsador físico y el slider manual, sin
tocar `mega_dispositivos.ino` para nada.

**Por qué disparador de estado y no `time_pattern`**: a diferencia del
ciclo de temperatura de color de `luz_zigbee_respaldo.yaml` (donde el
cálculo con `sun.sun` lo hace la propia automatización cada 15 min),
aquí el cálculo de posición ya lo hace Adaptive Cover internamente y
solo publica un valor nuevo en su sensor cuando cambia — un trigger
`platform: state` sobre ese sensor ya se dispara justo con esa cadencia
sin necesidad de sondear con un intervalo fijo propio.

### Pendiente importante: no pisar un ajuste manual del usuario

Problema a tener en cuenta antes de dar esto por terminado: si dejas la
persiana del salón a mano en, p. ej., 10% (viendo una peli) y luego
Adaptive Cover recalcula y publica 35%, el puente de arriba tal cual la
subiría sin que tú lo pidieras — el sensor no sabe que hubo un ajuste
manual de por medio.

La solución NO es meter en el puente una comprobación tipo "¿el
objetivo actual ya se superó?" (frágil: no distingue un override real
de una coincidencia numérica). Adaptive Cover ya trae detección de
override manual incorporada — al mover tú la persiana (desde la
tarjeta, el pulsador físico, o el propio `objetivo`), la integración lo
detecta y dejaría de forzar su cálculo hasta que se le devuelva el
control (normalmente vía un `switch`/botón que expone la propia
integración, del tipo "reanudar control automático" — el nombre exacto
depende de la versión instalada; revisarlo al dar de alta la instancia
real).

El puente debe respetar esa señal: antes de escribir en `..._objetivo`,
comprobar que esa entidad de override NO está activa, y si lo está, no
tocar nada (el usuario manda hasta que decida devolver el control).
Ejemplo de cómo quedaría la automatización con esa condición añadida:

```yaml
automation:
  - alias: Persiana Salón - seguir sol (Adaptive Cover)
    trigger:
      - platform: state
        entity_id: sensor.adaptive_cover_salon
    condition:
      - condition: state
        entity_id: switch.adaptive_cover_salon_override  # nombre real a confirmar al configurar la instancia
        state: "off"
    action:
      - service: input_number.set_value
        target:
          entity_id: input_number.salon_objetivo
        data:
          value: "{{ trigger.to_state.state | float(0) }}"
```

Pendiente de confirmar el `entity_id` exacto de esa entidad de override
una vez se dé de alta la instancia real de Adaptive Cover para cada
persiana (ver `todo.md`).

### Opcional: resetear el override manual cada mañana

Para no quedarte en modo manual indefinidamente si un día se te olvida
devolver el control (p. ej. dejaste el salón a mano anoche viendo una
peli y ya no te acuerdas al día siguiente), se puede añadir una
automatización aparte que fuerce el override a `off` todas las mañanas
a una hora fija (p. ej. 07:00), una por persiana/instancia:

```yaml
automation:
  - alias: Persiana Salón - reset override Adaptive Cover (7am)
    trigger:
      - platform: time
        at: "07:00:00"
    action:
      - service: switch.turn_off
        target:
          entity_id: switch.adaptive_cover_salon_override  # mismo entity_id que en la condición del puente
```

**Solo aplica si tu versión de Adaptive Cover expone el override como
un `switch` controlable por servicio** (la mayoría lo hace). Algunas
versiones en cambio resetean el override solo al pasar la persiana por
un extremo (abierta/cerrada del todo) en vez de mantenerlo indefinido —
en ese caso este reset horario no sería necesario. Confirmar cuál de
los dos comportamientos tiene la instancia real antes de dar esto por
implementado (ver `todo.md`); si no aplica, simplemente no crear esta
automatización.

### Opcional: tener en cuenta clima exterior (viento, lluvia, temperatura)

Pendiente de una estación meteorológica propia planeada por el usuario
(sensores de viento, temperatura y lluvia — marca/protocolo aún sin
decidir). Cuando esté instalada, expondrá sus propios `sensor.*` /
`binary_sensor.*` en HA, que se pueden usar para dos casos distintos
sobre el mismo puente de `persiana_posicion.yaml`:

1. **Bloquear el ajuste solar cuando no tiene sentido** (p. ej. está
   nublado/lloviendo y no hay sol directo que cortar, o hace frío y de
   hecho interesa dejar entrar el sol en vez de bloquearlo). Mismo
   mecanismo que el override manual: una condición más en el puente,
   además de comprobar el switch de override.
2. **Forzar cierre de seguridad por viento fuerte**, independiente del
   cálculo solar — esto no es "no tocar la persiana" como los casos
   anteriores, sino "bajarla activamente" para proteger la lona/lama
   física del viento. Conviene como automatización aparte (no dentro
   del puente de Adaptive Cover), con prioridad sobre cualquier otro
   control — incluido el override manual, ya que aquí el motivo es
   proteger el hardware, no una preferencia de usuario.

Ejemplo orientativo (entity_id de la estación meteo son placeholders,
pendientes de la instalación real):

```yaml
automation:
  - alias: Persiana Salón - seguir sol (Adaptive Cover, con clima)
    trigger:
      - platform: state
        entity_id: sensor.adaptive_cover_salon
    condition:
      - condition: state
        entity_id: switch.adaptive_cover_salon_override
        state: "off"
      - condition: numeric_state
        entity_id: sensor.estacion_lluvia_mm_h  # placeholder, nombre real a confirmar
        below: 0.1
    action:
      - service: input_number.set_value
        target:
          entity_id: input_number.salon_objetivo
        data:
          value: "{{ trigger.to_state.state | float(0) }}"

  - alias: Persianas - cierre de seguridad por viento fuerte
    trigger:
      - platform: numeric_state
        entity_id: sensor.estacion_viento_kmh  # placeholder, nombre real a confirmar
        above: 50  # umbral a decidir según la instalación física real
    action:
      - repeat:
          for_each: "{{ states.cover | map(attribute='entity_id') | list }}"
          sequence:
            - service: cover.close_cover
              target:
                entity_id: "{{ repeat.item }}"
```

Sin decidir todavía: umbrales exactos de lluvia/viento, y si el cierre
por viento debe afectar a todas las persianas o solo a las expuestas
(toldos/persianas exteriores) según orientación/instalación física. Ver
`todo.md`.

### Nota

Como cada persiana necesita su propia orientación de fachada, esto se
configura **una instancia de la integración por persiana** (o por
grupo de ventanas con la misma orientación). Pendiente de decidir la
orientación real de cada fachada de la casa antes de dar de alta las
instancias — ver `todo.md`.
