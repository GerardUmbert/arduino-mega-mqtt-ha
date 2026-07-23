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
