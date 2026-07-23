# Luces: ideas de automatización y brillo futuro (Zigbee)

## Lo que ya existe: `luz_pulsador.yaml`

- **Corta**: toggle on/off.
- **Triple**: enciende y apaga sola a los N minutos (configurable,
  por defecto 5). Reinicia el temporizador si se vuelve a pulsar triple
  antes de que expire.

Quedan libres de la agrupación de 7 triggers por botón: doble,
cuádruple, quíntuple, larga y larga_fin. Ideas para ellos, sin
implementar todavía — ninguna requiere cambios de hardware:

## Otras ideas dentro de on/off (funcionan con el hardware actual)

- **Doble pulsación = "todas las luces de la habitación off"**: útil en
  habitaciones con varias luces controladas por el mismo pulsador o
  agrupación — automatización que llama a `switch.turn_off` sobre una
  lista de entidades en vez de una sola. Encaja con la mención en
  `todo.md` de "escena / modo todas las luces off" como uso posible de
  cuádruple/quíntuple, aunque aquí se sugiere doble por ser más rápida de
  alcanzar (no necesita mantener el ritmo de 4-5 clics).
- **Cuádruple/quíntuple = escena fija**: p. ej. quíntuple activa una
  `scene.buenas_noches` que apaga todo el piso y cierra persianas. Sin
  caso de uso confirmado en `todo.md` todavía — pendiente de decidir con
  casos reales de uso antes de automatizar.
- **Larga + larga_fin = "luz de cortesía" temporizada por duración de
  pulsación**: mantener pulsado enciende la luz solo mientras se
  mantiene (por ejemplo, para pasillos de noche) — al soltar (`larga_fin`)
  se apaga. Alternativa a la temporización fija de la pulsación triple
  cuando quieres control manual de cuánto dura.

## Brillo vía bombilla Zigbee (no vía relé/firmware): `luz_zigbee_respaldo.yaml`

Si se quiere brillo regulable en alguna luz, la idea NO es meter PWM en
el relé/Mega — es sustituir esa bombilla por una bombilla Zigbee
regulable, y dejar que el brillo se controle 100% por Zigbee
directamente en HA (con el coordinador Zigbee que ya tenga o añada HA,
independiente de este proyecto de Arduino). El relé de
`mega_dispositivos` para esa luz sigue existiendo, pero cambia de
propósito: deja de ser "lo que enciende la luz" y pasa a ser un **corte
de seguridad ante fallo de red/Zigbee**, no el mecanismo normal de
control. **No requiere ningún cambio en `mega_dispositivos.ino`** — el
firmware sigue siendo el `HASwitch` on/off que ya es hoy.

Dos formas de combinarlo, de más a menos redundante:

- **Relé siempre encendido, Zigbee hace el on/off/brillo real**: el
  relé se deja permanentemente cerrado (o se quita del wiring de esa
  luz) y toda la lógica on/off/brillo vive en la bombilla Zigbee vía
  HA. Más simple, pero pierdes el corte físico por MQTT si Zigbee
  falla. No necesita automatización, solo wiring/configuración.
- **Relé como respaldo ante apagón/fallo** — esto es lo que hace
  `luz_zigbee_respaldo.yaml`: al arrancar HA o al recuperar conexión
  MQTT tras un corte, fuerza el relé de esa luz a ON. Opcionalmente
  (input `aplicar_estado_por_defecto`), espera a que la bombilla
  Zigbee vuelva a reportar estado y le aplica un brillo por defecto —
  en vez de asumir que "relé ON" y "bombilla ya controlable desde HA"
  pasan a la vez. Los dispositivos Zigbee tardan en reunirse a la
  malla tras un corte de corriente, no es instantáneo como MQTT; si no
  reaparece dentro de `espera_maxima_zigbee_minutos`, el relé se queda
  en ON de todas formas pero no se le manda brillo. El día a día
  normal (toggle, dimming) sigue siendo 100% Zigbee — el relé no se
  vuelve a tocar salvo en este escenario de recuperación.

### Temperatura de color por hora del día (opcional, mismo blueprint)

Aparte del respaldo, `luz_zigbee_respaldo.yaml` puede además ajustar
sola la temperatura de color de la bombilla a lo largo del día —
cálida (tipo incandescente) de noche, más neutra a mediodía —
calculada a partir de la elevación del sol (`sun.sun`), no de horas
fijas, así que se ajusta sola con las estaciones. Se activa con el
input `ajustar_temperatura_color`, revisa cada 15 minutos, y solo
actúa si la bombilla ya está encendida (nunca la enciende ni apaga por
este motivo). Pensado para una **IKEA TRÅDFRI regulable "WW/CW"**
(blanco cálido/frío, sin RGB) — su rango real de fábrica (~250-454
mireds, ~2200K-4000K) es más estrecho que otras bombillas Zigbee con
soporte de color completo, y los valores por defecto del blueprint ya
están ajustados a ese rango.

### Instanciar el blueprint

Una instancia por cada luz Zigbee que tenga relé de respaldo:

1. Ajustes → Automatizaciones y escenas → Blueprints → importar
   `luz_zigbee_respaldo.yaml` → Crear automatización.
2. **Relé**: el `switch.*` de `mega_dispositivos` para esa luz.
3. **Bombilla Zigbee**: la entidad `light.*` real.
4. **Aplicar brillo por defecto al recuperar corriente**: actívalo solo
   si quieres forzar un brillo concreto tras un corte; si lo dejas
   desactivado, el relé se enciende y la bombilla arranca con su
   comportamiento propio (normalmente su último estado guardado).
5. **Ajustar temperatura de color según la hora del día**: actívalo si
   quieres el ciclo cálida/fría automático descrito arriba.

Pendiente de concretar cuándo/si se compra la bombilla — el blueprint ya
está listo para cuando exista.
