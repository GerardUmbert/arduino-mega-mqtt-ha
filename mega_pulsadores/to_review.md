# Cosas pendientes de revisar — mega_pulsadores

## OneButton vs. AceButton (ahorro de RAM real, pero pierde 3/4/5 clics)

Investigado el 2026-09-04. **Decisión tomada: no sustituir OneButton en
`mega_pulsadores/`, sino mantener las dos librerías como firmwares
paralelos** — ver `mega_pulsadores_low_ram/mega_pulsadores_low_ram.ino`
(usa AceButton, mismo `HABILITAR_*`/formato `pNN` que aquí, pero solo
corta/doble/larga/largaFin). Cada unidad física se flashea con el
firmware que le convenga sin comprometer el proyecto entero a una sola
librería. El análisis de abajo sigue siendo la referencia de por qué se
decidió así.

### El problema que resolvería

`OneButton` reserva sitio para las 8 posibles callbacks (click, double,
multi, longPressStart, longPressStop, duringLongPress, idle, pressed)
como variables miembro **incondicionales** de la clase — están ahí
tanto si las usas como si no, `sizeof(OneButton)` no cambia según
cuántos `attachX()` llames. Confirmado leyendo `OneButton.h` del repo
oficial: ronda **90-100 bytes por instancia**, siempre, aunque solo
uses `attachClick()`.

Con la config por defecto actual (`HABILITAR_CORTA`, `HABILITAR_DOBLE`,
`HABILITAR_LARGA`, `HABILITAR_LARGA_FIN` activos; triple/cuádruple/
quíntuple desactivados — ver `mega_pulsadores.ino`), estamos pagando
ese coste fijo de ~90-100 bytes por pulsador aunque solo usemos 4 de
los 7 tipos de evento posibles.

### La alternativa investigada: AceButton

Repo: https://github.com/bxparks/AceButton

### Comparativa de eventos: los 5 candidatos investigados

Los 7 eventos son los que expone `mega_pulsadores.ino` hoy (ver
`HABILITAR_*` en el `.ino`). ✅ = soportado de forma nativa, con
evento/callback dedicado. ⚠️ = posible pero requiere lógica extra
propia (la librería no lo da hecho). ❌ = no soportado, sin mecanismo
alguno para conseguirlo.

| Evento | OneButton (actual) | AceButton | Button2 | Bounce2 | ezButton |
|---|---|---|---|---|---|
| Corta (1 clic) | ✅ `attachClick` | ✅ `kEventClicked` | ✅ `setTapHandler` | ❌ solo debounce | ⚠️ `isPressed()`, sin evento — hay que contarlo a mano |
| Doble clic | ✅ `attachDoubleClick` | ✅ `kEventDoubleClicked` | ✅ `setDoubleClickHandler` | ❌ | ❌ |
| Triple clic | ✅ `attachMultiClick` + `getNumberClicks()==3` | ❌ sin mecanismo para 3+ clics | ⚠️ `getNumberOfClicks()` sin tope, pero el enum de tipos solo nombra hasta `triple_click` | ❌ | ❌ |
| Cuádruple clic | ✅ igual, `==4` | ❌ | ⚠️ igual que triple, sin nombre propio en el enum pero el contador no tiene tope | ❌ | ❌ |
| Quíntuple clic | ✅ igual, `==5` | ❌ | ⚠️ igual | ❌ | ❌ |
| Larga (inicio, con el botón aún pulsado) | ✅ `attachLongPressStart` | ✅ `kEventLongPressed` | ✅ `setLongClickHandler` | ❌ | ⚠️ `isPressed()` + medir tiempo a mano |
| Larga (fin, al soltar tras larga) | ✅ `attachLongPressStop` | ✅ `kEventLongReleased` — distinto del release normal | ❌ solo hay un release genérico, no distingue si veía de una pulsación larga o corta | ❌ | ❌ |
| **Identificación del pulsador sin lambda con captura** | ✅ `void* param` en cada `attachX(callback, param)` | ✅ `button->getId()` desde el puntero que recibe el callback | ✅ `btn.getContext()` desde la instancia | N/A (sin callbacks) | N/A (sin callbacks) |
| **RAM aprox. por instancia (AVR)** | ~90-100 bytes, fijo, uses las callbacks que uses | ~18-26 bytes | ~68-76 bytes (similar a OneButton, no es un ahorro) | ~11 bytes (pero casi no hace nada) | sin datos publicados |

Duración de pulsación larga en AceButton configurable vía
`ButtonConfig::setLongPressDelay(ms)` (equivalente a
`OneButton::setPressMs()`). El `id` de AceButton se pasa en el
constructor y se lee luego con `getId()` desde el puntero que recibe
el callback (ver fila "Identificación del pulsador" arriba) — no hace
falta `void* param` ni lambdas con captura (que de todas formas no
funcionan aquí, ver el fix de la versión 1.6.3 en `CHANGELOG.md`).

### Por qué NO se ha cambiado todavía

**AceButton no soporta triple/cuádruple/quíntuple clic — no es que
esté desactivado, es que la librería solo distingue single vs. double
click, sin ningún mecanismo para contar 3+ pulsaciones seguidas.**
Confirmado leyendo el código fuente (`AceButton.h`/`ButtonConfig.h`),
no es una limitación de configuración.

Eso afecta de forma distinta a cada blueprint (si algún día se activan
esos triggers, cosa que hoy con
`HABILITAR_TRIPLE`/`HABILITAR_CUADRUPLE`/`HABILITAR_QUINTUPLE`
comentados no pasa):

- **`persiana_pulsador_completo.yaml`** — **sin solución posible con
  AceButton.** Usa corta+doble+triple+cuádruple+quíntuple SIMULTÁNEAMENTE
  como 5 niveles distintos sobre el mismo botón (1/2/3/4/5
  pulsaciones). AceButton solo da 2 slots de clic (single/double) — no
  hay hueco donde remapear los otros 3, perdería el ajuste fino ±5%,
  el "todas las persianas de la Area" y el "todas las persianas de la
  casa" sin remedio.
- **`luz_pulsador.yaml`** — **ya remapeado.** Solo usaba corta + triple
  + larga (ni doble, ni cuádruple, ni quíntuple, ni largaFin —
  comprobado en el propio blueprint), así que el triple clic (apagado
  automático a los N minutos) se remapeó a doble clic directamente en
  la definición del blueprint (`type: button_triple_press` →
  `button_double_press`, ver su propio CHANGELOG.md) — funciona igual
  en ambos firmwares, sin distinguir cuál lo dispara.

### Cuándo reconsiderar dejar de mantener las dos librerías

- Si se confirma con datos reales (ver `instructions.md`, medición con
  `freeMemory()`) que `OneButton` es efectivamente una parte
  significativa del consumo total de RAM por pulsador — hoy no lo
  sabemos, solo tenemos el desglose teórico de arriba, no una medida
  en placa.
- Si se decide definitivamente que ninguna unidad necesitará nunca
  triple/cuádruple/quíntuple (`persiana_pulsador_completo.yaml` deja
  de usarse en cualquier pulsador de cualquier unidad), momento en el
  que `mega_pulsadores/` (OneButton) dejaría de aportar nada frente a
  `mega_pulsadores_low_ram/` y podría retirarse en vez de mantener las
  dos.

## Otras alternativas descartadas (mismo research, 2026-09-04)

- **Bounce2**: solo debounce, sin clic-conteo ni pulsación larga — no
  es un sustituto real, habría que reimplementar todo eso encima.
- **ezButton**: sin API de callbacks documentada (`void*` o similar),
  basada en polling (`isPressed()`/`getCount()`) — no encaja con el
  patrón actual del `.ino`.
- **Button2**: RAM similar o ligeramente mayor que OneButton (no es un
  ahorro), y sin evento dedicado de "release tras pulsación larga"
  (solo un release genérico) — no aporta nada frente a quedarnos con
  OneButton.
