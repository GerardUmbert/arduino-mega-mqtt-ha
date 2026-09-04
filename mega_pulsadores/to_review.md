# Cosas pendientes de revisar — mega_pulsadores

## Cambiar OneButton por AceButton (ahorro de RAM real, pero pierde 3/4/5 clics)

Investigado el 2026-09-04, sin implementar todavía — decisión pendiente.

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

**Cubre exactamente lo que usamos por defecto hoy:**

| Evento que usamos | Equivalente en AceButton |
|---|---|
| corta | `kEventClicked` |
| doble | `kEventDoubleClicked` |
| larga (inicio) | `kEventLongPressed` — se dispara una vez, con el botón aún pulsado (no espera a soltar) |
| largaFin (soltar tras larga) | `kEventLongReleased` — distinto del release normal, así que sí se puede diferenciar "solté tras pulsación larga" de "solté tras pulsación corta" sin lógica extra |

Duración de pulsación larga configurable vía
`ButtonConfig::setLongPressDelay(ms)` (equivalente a
`OneButton::setPressMs()`).

**Identificación del pulsador sin lambdas con captura**: cada
`AceButton` puede llevar un `id` (constructor opcional, `getId()`
público) — el callback recibe un puntero al `AceButton` que disparó el
evento y puede leer `button->getId()` para saber cuál es, sin
necesitar `void* param` como hace OneButton ni lambdas con captura
(que de todas formas no funcionan aquí, ver el fix de la versión
1.6.3 en `CHANGELOG.md`).

**RAM estimada**: ~18-26 bytes por instancia (frente a ~90-100 de
OneButton) — la clase en sí es más pequeña, no un caso de "activa
menos features y pesa menos" (AceButton también reserva su estado
incondicionalmente, pero esa clase base ya es mucho más compacta).

### Por qué NO se ha cambiado todavía

**AceButton no soporta triple/cuádruple/quíntuple clic — no es que
esté desactivado, es que la librería solo distingue single vs. double
click, sin ningún mecanismo para contar 3+ pulsaciones seguidas.**
Confirmado leyendo el código fuente (`AceButton.h`/`ButtonConfig.h`),
no es una limitación de configuración.

Eso rompería (si algún día se activan esos triggers, cosa que hoy con
`HABILITAR_TRIPLE`/`HABILITAR_CUADRUPLE`/`HABILITAR_QUINTUPLE`
comentados no pasa):

- **`persiana_pulsador_completo.yaml`**: usa las 5 pulsaciones
  (1/2/3/4/5) — sin triple/cuádruple/quíntuple, pierde el ajuste
  fino ±5%, el "todas las persianas de la Area", y el "todas las
  persianas de la casa".
- **`luz_pulsador.yaml`**: usa el triple clic para el apagado
  automático a los N minutos — sin triple, esa función deja de
  existir.

Cambiar a AceButton sería una decisión **permanente en la práctica**:
para volver a tener 3/4/5 clics habría que deshacer el cambio y volver
a OneButton (no es un simple `#define` como con `HABILITAR_TRIPLE`
ahora mismo).

### Cuándo reconsiderar esto

- Si se confirma con datos reales (ver `instructions.md`, medición con
  `freeMemory()`) que `OneButton` es efectivamente una parte
  significativa del consumo total de RAM por pulsador — hoy no lo
  sabemos, solo tenemos el desglose teórico de arriba, no una medida
  en placa.
- Si se decide definitivamente que triple/cuádruple/quíntuple no se
  van a usar nunca en ningún pulsador de ninguna unidad (hoy están
  desactivados por defecto pero siguen siendo una opción con un
  `#define`).
- Si se necesitan más pulsadores por unidad de los que caben con
  `OneButton` y bajar RAM sí es el cuello de botella real (otra vez:
  confirmar con medición antes de asumirlo).

Si se decide seguir adelante, sería una migración con dos añadidos que
otras librerías no tienen ambos a la vez, así que no es un simple
find-and-replace: cambiar el patrón de callbacks (AceButton pasa el
puntero al `AceButton` que disparó el evento a un único
`handleEvent(AceButton*, uint8_t eventType, uint8_t buttonState)`, en
vez de una función distinta por tipo de evento como hace OneButton) y
quitar toda la lógica de multi-clic (`onMultiClick`,
`getNumberClicks()`, los `case 3/4/5` del switch).

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
