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

## Posición de persianas: se reporta nativa (1.6.0+), pero NO se puede comandar

Desde `mega_dispositivos` 1.6.0, cada persiana reporta su propia
posición (0-100%) directamente por MQTT (`HACover::PositionFeature`),
estimada en el propio firmware por tiempo de relé activo. El slider de
posición se ve en la tarjeta, y `cover.open_cover` / `cover.close_cover`
/ `cover.stop_cover` funcionan directamente sobre `cover.persiana_XX_YY`.

!!! warning "`cover.set_cover_position` no hace nada sobre estas entidades"
    La librería `ArduinoHA` que usa `mega_dispositivos` solo permite
    *reportar* posición, no *recibir* comandos de posición desde HA —
    no es un problema de configuración ni de versión de firmware, es
    una limitación de la librería (confirmado intentando añadir
    soporte en la v1.8.0, revertido — ver `CHANGELOG.md` del repo raíz).
    Para "ir a X%" de verdad, usa
    [`persiana_ir_a_posicion.yaml`](persiana-pulsador-completo.md), un
    script que vigila la posición ya reportada y para el movimiento al
    cruzarla.

!!! note "`persiana_posicion.yaml` (legacy)"
    El blueprint que simulaba posición por HA con 4 helpers
    `input_number` por persiana, pensado para firmwares anteriores a
    1.6.0 sin reporte de posición real, ya no hace falta: usa
    `persiana_ir_a_posicion.yaml` en su lugar.
