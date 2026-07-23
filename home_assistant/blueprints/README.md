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
TRÅDFRI WW/CW regulable). Hace dos cosas independientes, cada una
activable por separado:

- **Respaldo ante corte**: el relé deja de ser el control normal y pasa
  a ser un corte de seguridad ante fallo de red/apagón. Al arrancar HA
  o recuperar MQTT, fuerza el relé a ON y, opcionalmente, espera a que
  la bombilla Zigbee reaparezca antes de aplicarle un brillo por
  defecto — sin asumir que el relé y la bombilla están listos a la
  vez, porque Zigbee tarda en reunirse a la malla tras un corte.
- **Temperatura de color por hora del día**: si se activa, cada 15
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
4. **Aplicar brillo por defecto al recuperar corriente**: opcional, ver
   `luces_notas.md`.
5. **Ajustar temperatura de color según la hora del día**: opcional,
   activa el ciclo automático cálida/fría. Ajusta
   `temp_calida_mireds`/`temp_fria_mireds` si tu bombilla tiene un
   rango distinto al de la IKEA TRÅDFRI WW/CW.
