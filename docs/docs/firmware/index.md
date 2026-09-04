# Firmware de pulsadores

Existen **dos firmwares distintos** para el rol "leer pulsadores
físicos", que hacen lo mismo desde el punto de vista de Home
Assistant (envían device triggers MQTT), pero usan librerías distintas
internamente:

| | [`mega_pulsadores`](mega-pulsadores.md) | [`mega_pulsadores_low_ram`](mega-pulsadores-low-ram.md) |
|---|---|---|
| Librería de botones | `OneButton` | `AceButton` |
| RAM por pulsador | ~90-100 bytes (fijo) | ~18-26 bytes |
| Pulsaciones soportadas | Las 7 | Solo 4: corta, doble, larga, fin de larga |

**No sabes cuál usar?** → [Guía de decisión](decision.md)

Y para el rol "controlar relés" solo hay un firmware:
[`mega_dispositivos`](mega-dispositivos.md).
