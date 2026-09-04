# Luces: ideas de automatización y brillo futuro (Zigbee)

## Lo que ya existe: `luz_pulsador.yaml`

- **Corta**: toggle on/off.
- **Doble**: enciende y apaga sola a los N minutos (configurable,
  por defecto 5). Reinicia el temporizador si se vuelve a pulsar doble
  antes de que expire.
- **Larga**: toggle de todas las luces del Area de este pulsador
  (además cierra persianas al apagar). Ver tabla completa en el README
  de blueprints.

Este blueprint solo usa corta/doble/larga a propósito, para que
funcione igual tanto en `mega_pulsadores` (OneButton) como en
`mega_pulsadores_low_ram` (AceButton) — este segundo firmware no
soporta triple/cuádruple/quíntuple en absoluto (ver README principal
del repo). El apagado automático usaba originalmente pulsación triple;
se cambió a doble por ese motivo, no por preferencia de UX.

Quedan libres de la agrupación de 7 triggers por botón:
cuádruple, quíntuple y larga_fin (larga sí se usa, larga_fin no). Ideas
para ellos, sin implementar todavía — ninguna requiere cambios de
hardware, pero cuádruple/quíntuple **solo están disponibles en
unidades `mega_pulsadores` (OneButton), no en `mega_pulsadores_low_ram`**:

## Otras ideas dentro de on/off (funcionan con el hardware actual)

- **Cuádruple/quíntuple = escena fija**: p. ej. quíntuple activa una
  `scene.buenas_noches` que apaga todo el piso y cierra persianas. Sin
  caso de uso confirmado en `todo.md` todavía — pendiente de decidir con
  casos reales de uso antes de automatizar. Requiere que ese pulsador
  esté en una unidad `mega_pulsadores` (OneButton), no
  `mega_pulsadores_low_ram`.
- **Larga_fin = "luz de cortesía" temporizada por duración de
  pulsación**: mantener pulsado enciende la luz solo mientras se
  mantiene (por ejemplo, para pasillos de noche) — al soltar (`larga_fin`)
  se apaga. Alternativa al apagado a tiempo fijo de la pulsación doble
  cuando quieres control manual de cuánto dura. Disponible en ambos
  firmwares (`larga_fin` sí lo soporta `AceButton`), pero
  `luz_pulsador.yaml` no lo implementa todavía — habría que añadirlo al
  blueprint.

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
  HA. Más simple, pero pierdes el corte físico por pulsador si Zigbee
  falla. No necesita automatización, solo wiring/configuración.
- **Relé como respaldo, controlado por el pulsador físico** — esto es
  lo que hace `luz_zigbee_respaldo.yaml`: al pulsar el botón físico
  (pulsación corta) pasan dos cosas a la vez — se hace toggle de la
  bombilla Zigbee (el control normal de encender/apagar) y, además, el
  relé se fuerza a ON siempre, esté como esté — el relé NUNCA hace
  toggle ni se apaga desde el pulsador, solo se garantiza que quede
  ON. Sirve como garantía de "esta luz tiene corriente pase lo que
  pase" sin depender de que el relé ya estuviera bien; el toggle real
  de encendido/apagado lo sigue decidiendo la bombilla Zigbee, no el
  relé. **Importante**: nada de esto se dispara solo porque HA o MQTT
  arranquen o se reconecten — el único disparador del relé es el
  pulsador físico. La bombilla Zigbee tiene su propio comportamiento
  de recuperación tras un corte de corriente (normalmente vuelve a su
  último estado por su cuenta); si quieres que HA además le aplique un
  brillo/temperatura por defecto cuando eso pase, eso es la segunda
  parte del blueprint (`adaptar_al_volver`, ver abajo) — dos cosas
  independientes, no una cadena automática "arranca HA → enciende
  luz".

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
4. **Pulsador (device)** y **Subtype del botón**: el pulsador físico de
   esta luz, cuya pulsación corta fuerza el relé a ON.
5. **Adaptar brillo/temperatura cuando la bombilla vuelve a encenderse
   sola**: actívalo solo si quieres forzar un brillo/temperatura
   concreto cuando la bombilla Zigbee se reincorpore por su cuenta tras
   un corte; si lo dejas desactivado, la bombilla se queda con su
   comportamiento propio (normalmente su último estado guardado).
6. **Ajustar temperatura de color según la hora del día**: actívalo si
   quieres el ciclo cálida/fría automático descrito arriba.

Pendiente de concretar cuándo/si se compra la bombilla — el blueprint ya
está listo para cuando exista.
