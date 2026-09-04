# Blueprints de Home Assistant

Plantillas de automatización reutilizables — cada una se instancia una
o varias veces desde Ajustes → Automatizaciones y escenas →
Blueprints → Importar.

## Tabla resumen

| Blueprint | Qué hace | Pulsaciones usadas | Compatible con `low_ram` |
|---|---|---|:---:|
| [`persiana_pulsador`](persiana-pulsador.md) | Mantener pulsado sube/baja, soltar para | larga, fin de larga | ✅ Completa |
| [`persiana_pulsador_completo`](persiana-pulsador-completo.md) | 5 niveles de clic = 5 posiciones/alcances distintos | corta, doble, triple, cuádruple, quíntuple, larga, fin de larga | ⚠️ Parcial |
| [`luz_pulsador`](luz-pulsador.md) | Toggle + apagado automático + toggle de Area | corta, doble, larga | ✅ Completa |
| [`luz_zigbee_respaldo`](luz-zigbee-respaldo.md) | Respaldo de relé para bombilla Zigbee regulable | corta | ✅ Completa |
| [Adaptive Cover](adaptive-cover.md) | Persianas que siguen el sol (integración externa) | — (no usa pulsador) | — |

!!! info "`persiana_pulsador_completo` en `mega_pulsadores_low_ram`"
    Las pulsaciones 1, 2, larga y fin de larga funcionan igual que en
    `mega_pulsadores` — solo las pulsaciones 3, 4 y 5 no se disparan
    (esos tres triggers no existen en `AceButton`). Detalle completo
    en [`mega_pulsadores_low_ram`](../firmware/mega-pulsadores-low-ram.md)
    y en la [guía de decisión](../firmware/decision.md).

## Posición de persianas: nativa desde firmware 1.6.0+

Desde `mega_dispositivos` 1.6.0, cada persiana reporta su propia
posición (0-100%) directamente por MQTT
(`HACover::PositionFeature`), estimada en el propio firmware por
tiempo de relé activo. La tarjeta normal de HA ya muestra el slider de
posición sin necesidad de ningún helper ni blueprint intermedio: usa
`cover.set_cover_position` / `cover.open_cover` / `cover.close_cover`
/ `cover.stop_cover` directamente sobre `cover.persiana_XX_YY`.

!!! note "`persiana_posicion.yaml` (legacy)"
    El blueprint que simulaba esto por HA con 4 helpers `input_number`
    por persiana (para firmwares sin posición real) solo aplica si
    tienes una unidad `mega_dispositivos` en una versión de firmware
    anterior a 1.6.0 sin actualizar. Con firmware 1.6.0+, no lo
    instancies.
