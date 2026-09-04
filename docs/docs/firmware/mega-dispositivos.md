# `mega_dispositivos`

Firmware para las unidades que controlan relés de luces y persianas.
**No lee ningún pulsador — solo recibe órdenes MQTT y las ejecuta.**

[:material-github: Ver `mega_dispositivos.ino` en GitHub](https://github.com/GerardUmbert/ruben_smart_home_arduino/blob/master/mega_dispositivos/mega_dispositivos.ino){ .md-button }

```mermaid
flowchart LR
    HA(("Home Assistant")) -- "switch.turn_on/off" --> SW["HASwitch<br/>(luces)"]
    HA -- "cover.open/close/stop_cover" --> COV["HACover<br/>(persianas)"]
    SW --> RelayL["Relé de luz<br/>(digitalWrite)"]
    COV --> RelaySub["Relé subir"]
    COV --> RelayBaj["Relé bajar"]
```

## Entidades que crea

A diferencia de `mega_pulsadores`/`mega_pulsadores_low_ram` (que solo
envían triggers sin estado), `mega_dispositivos` crea **entidades
reales** en HA:

| Entidad | Nombre (`unique_id`) | Soporta |
|---|---|---|
| `HASwitch` por luz | `luz_22`, `luz_30`... (número de pin) | on/off |
| `HACover` por persiana | `persiana_38_39`, `persiana_41_42`... (pin subir + pin bajar, en ese orden siempre) | abrir/cerrar/parar + posición nativa (firmware 1.6.0+) |

!!! info "Posición nativa de persianas"
    Desde la versión 1.6.0, cada persiana reporta su propia posición
    (0-100%) directamente por MQTT, estimada por tiempo de relé
    activo. La tarjeta normal de HA muestra el slider de posición sin
    necesidad de ningún helper — usa `cover.set_cover_position`
    directamente sobre `cover.persiana_XX_YY`.

## Configuración de pines

```cpp
struct ParPines { uint8_t subir; uint8_t bajar; };
```

Cada persiana se define como un **par de pines** (subir/bajar) en
`board_config_a.h`/`board_config_b.h`. El `unique_id` incluye ambos
pines del par, **siempre en el orden subir_bajar** (p. ej.
`{subir: 38, bajar: 39}` → `persiana_38_39`), nunca al revés.

## Seguridad: interlock entre subir y bajar

Hay un tiempo de seguridad (`RETARDO_INVERSION_MS`) entre apagar un
sentido y encender el otro, para no invertir el sentido de giro del
motor demasiado rápido. Parar = poner los dos relés (subir/bajar) a
LOW simultáneamente.

## Compatible con 0 persianas

Una unidad puede configurarse con `PINES_PERSIANAS[] = {}` (solo
luces, sin persianas) sin problema — corregido en la versión 1.6.3
tras un bug de compilación con arrays de tamaño 0. Ver
[Changelog](../reference/changelog.md).
