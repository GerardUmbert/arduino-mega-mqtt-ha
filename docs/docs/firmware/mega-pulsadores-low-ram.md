# `mega_pulsadores_low_ram` (AceButton)

Firmware **alternativo** para el mismo rol que
[`mega_pulsadores`](mega-pulsadores.md), usando la librería
[`AceButton`](https://github.com/bxparks/AceButton) en vez de
`OneButton`: mucha menos RAM por pulsador, a cambio de perder soporte
de triple/cuádruple/quíntuple clic **por completo**.

[:material-github: Ver `mega_pulsadores_low_ram.ino` en GitHub](https://github.com/GerardUmbert/arduino-mega-mqtt-ha/blob/master/mega_pulsadores_low_ram/mega_pulsadores_low_ram.ino){ .md-button }

!!! danger "¿Ya sabes si te conviene este firmware?"
    Si tienes dudas, usa la [guía de decisión](decision.md) antes de
    seguir leyendo esta página.

## Los 4 eventos posibles

`AceButton` **no tiene ningún mecanismo** para contar 3+ pulsaciones
seguidas — no es una opción desactivable como en `mega_pulsadores`, la
librería solo distingue click simple y doble click.

```cpp
#define HABILITAR_CORTA
#define HABILITAR_DOBLE
#define HABILITAR_LARGA
#define HABILITAR_LARGA_FIN
```

| Evento | `#define` | Activo por defecto | Evento AceButton |
|---|---|:---:|---|
| Corta | `HABILITAR_CORTA` | ✅ | `kEventClicked` |
| Doble | `HABILITAR_DOBLE` | ✅ | `kEventDoubleClicked` |
| Triple | — | — | ❌ **no existe** |
| Cuádruple | — | — | ❌ **no existe** |
| Quíntuple | — | — | ❌ **no existe** |
| Larga (inicio) | `HABILITAR_LARGA` | ✅ | `kEventLongPressed` |
| Larga (fin, al soltar) | `HABILITAR_LARGA_FIN` | ✅ | `kEventLongReleased` |

```mermaid
flowchart LR
    Btn(["`Pulsador físico
    (pin digital)`"]) --> AB["`AceButton
    .check()`"]
    AB --> Handler["`handleEvent()
    (1 solo handler global)`"]

    Handler -- "kEventClicked" --> C{"HABILITAR_CORTA?"}
    Handler -- "kEventDoubleClicked" --> D{"HABILITAR_DOBLE?"}
    Handler -- "kEventLongPressed" --> L{"HABILITAR_LARGA?"}
    Handler -- "kEventLongReleased" --> LF{"HABILITAR_LARGA_FIN?"}

    C -- Sí --> T1["`HADeviceTrigger
    ButtonShortPressType`"]
    D -- Sí --> T2["`HADeviceTrigger
    ButtonDoublePressType`"]
    L -- Sí --> T3["`HADeviceTrigger
    ButtonLongPressType`"]
    LF -- Sí --> T4["`HADeviceTrigger
    ButtonLongReleaseType`"]

    T1 & T2 & T3 & T4 --> MQTT[("MQTT → Home Assistant")]
```

## Diferencia clave con OneButton en el código

`AceButton` usa **un único handler global** compartido por todos los
pulsadores (`handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState)`),
en vez de una función distinta por tipo de evento como hace
`OneButton`. La identificación de qué pulsador disparó el evento se
hace con `button->getId()`, en vez del `void* param` que usa
`OneButton`.

## RAM: por qué existe este firmware

| | `OneButton` | `AceButton` |
|---|---|---|
| RAM por instancia (AVR) | ~90-100 bytes, **fijo** — reserva sitio para las 8 callbacks posibles aunque no las uses | ~18-26 bytes |
| Motivo de la diferencia | Todas las variables miembro (8 punteros a función + parámetros) son incondicionales en la clase | Clase base más pequeña |

Ver [RAM y rendimiento](../reference/ram.md) para el desglose completo
y cómo medir tu propia configuración en placa real.

## Compatibilidad con blueprints

| Blueprint | ¿Compatible? | Notas |
|---|:---:|---|
| [`persiana_pulsador`](../blueprints/persiana-pulsador.md) | ✅ Completa | Solo usa larga/fin de larga |
| [`persiana_pulsador_completo`](../blueprints/persiana-pulsador-completo.md) | ⚠️ Parcial | Las pulsaciones 1 (100%/0%), 2 (Area) y larga/fin de larga (subir/bajar/parar) funcionan igual que en `mega_pulsadores`. Las pulsaciones 3 (50%), 4 (±5%) y 5 (toda la casa) **no se disparan nunca** — sus triggers (`button_triple_press`/`quadruple`/`quintuple`) no existen en este firmware, y no hay error visible, simplemente esos botones no hacen nada |
| [`luz_pulsador`](../blueprints/luz-pulsador.md) | ✅ Completa | Usa corta/doble/larga — el apagado automático se remapeó de triple a doble precisamente para que funcionara en ambos firmwares |
| [`luz_zigbee_respaldo`](../blueprints/luz-zigbee-respaldo.md) | ✅ Completa | Solo usa corta |

## Configuración de pines y config.h

Igual que `mega_pulsadores`, pero con ficheros de configuración
**independientes** (Arduino exige que los `.h` vivan en la misma
carpeta que el `.ino`):

- `mega_pulsadores_low_ram/board_config_a.h`,
  `board_config_b.h` — copias de las de `mega_pulsadores/`, no
  compartidas. Si cambias pines/MAC/IP en una carpeta, revisa si el
  mismo cambio aplica también en la otra.
- `mega_pulsadores_low_ram/config.h` — necesita un paso extra al
  crearlo, ver [Configuración (config.h)](../getting-started/config.md#mega_pulsadores_low_ram-un-paso-extra-con-git).
