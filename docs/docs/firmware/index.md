# Firmware de pulsadores

Existen **dos firmwares distintos** para el rol "leer pulsadores
físicos", que hacen lo mismo desde el punto de vista de Home
Assistant (envían device triggers MQTT), pero usan librerías distintas
internamente:

| | [`mega_pulsadores`](mega-pulsadores.md) | [`mega_pulsadores_low_ram`](mega-pulsadores-low-ram.md) |
|---|---|---|
| Librería de botones | `OneButton` | `AceButton` |
| Límite práctico medido en placa real | 12 estable, 16 falla | 24 estable, 25 falla |
| Pulsaciones soportadas | Las 7: corta, doble, triple, cuádruple, quíntuple, larga, fin de larga | Solo 4: corta, doble, larga, fin de larga |

**No sabes cuál usar?** → [Guía de decisión](decision.md) · Detalle
completo de RAM en [RAM y rendimiento](../reference/ram.md).

Y para el rol "controlar relés" el firmware estable es
[`mega_dispositivos`](mega-dispositivos.md).

## Variante experimental: control directo sin Home Assistant

!!! warning "Prototipo — sin compilar ni probar en placa"
    Versión `0.1.0-udp`. Úsalo solo si quieres probarlo; el resto de
    firmwares son los estables.

Con los firmwares normales, **si Home Assistant (o el broker MQTT) cae,
los pulsadores dejan de mover luces y persianas**: toda la lógica vive
en los blueprints de HA. Hay una variante de cada firmware que añade un
camino directo por UDP entre las placas:

| Rol | Estable | Variante con UDP |
|---|---|---|
| Pulsadores (AceButton) | [`mega_pulsadores_low_ram`](mega-pulsadores-low-ram.md) | [`mega_pulsadores_low_ram_udp`](mega-pulsadores-low-ram-udp.md) |
| Relés | [`mega_dispositivos`](mega-dispositivos.md) | [`mega_dispositivos_udp`](mega-dispositivos-udp.md) |

Hay que flashear **las dos** (pulsadores y relés) para que funcione. La
integración con HA se mantiene intacta: siguen publicándose los
triggers y los estados por MQTT. No existe variante UDP de
`mega_pulsadores` (OneButton).
