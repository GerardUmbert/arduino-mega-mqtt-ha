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

## Posición de persianas: nativa de verdad desde firmware 1.8.0+

Desde `mega_dispositivos` 1.6.0, cada persiana reporta su propia
posición (0-100%) directamente por MQTT (`HACover::PositionFeature`),
estimada en el propio firmware por tiempo de relé activo. Desde la
**1.8.0**, esa posición también se puede **comandar**:
`cover.set_cover_position` mueve la persiana de verdad, igual que
`cover.open_cover` / `cover.close_cover` / `cover.stop_cover`,
directamente sobre `cover.persiana_XX_YY`.

!!! warning "Requiere el parche de ArduinoHA en mega_dispositivos/lib_overrides/"
    La librería `ArduinoHA` de fábrica (Library Manager, sin parchear)
    solo permite *reportar* posición, no *recibir* comandos de
    posición desde HA — sin el parche documentado en
    `mega_dispositivos/lib_overrides/README.md` (repo raíz),
    `cover.set_cover_position` no hace nada sobre estas entidades, sin
    error visible. No es un problema de configuración de HA, es una
    limitación real de la librería original.

!!! note "Blueprints en desuso"
    `persiana_posicion.yaml` (simulaba posición por HA con 4 helpers
    `input_number`, para firmwares anteriores a 1.6.0 sin reporte de
    posición real) y `persiana_ir_a_posicion.yaml` (script-workaround
    usado mientras `set_cover_position` no funcionaba, antes del
    parche de la 1.8.0) quedan ambos en `legacy/` — hoy ninguno hace
    falta, usa `cover.set_cover_position` directamente.
