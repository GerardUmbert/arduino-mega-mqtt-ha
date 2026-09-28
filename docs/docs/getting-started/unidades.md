# Identificación de unidades A/B/C

Cada rol usa un único fichero `.ino` (o, para pulsadores, uno de los
dos firmwares posibles), pero la identidad de cada unidad física (A, B
o C) se decide en **tiempo de compilación**, no con un jumper físico.

Al principio del `.ino` hay un bloque `PLACA_A`/`PLACA_B`/`PLACA_C`:

```cpp
#define PLACA_A
// #define PLACA_B
// #define PLACA_C
```

Se deja descomentada **SOLO una** de las tres líneas según a qué unidad
física vayas a subir ese firmware, se compila y se sube. Para otra
unidad, se cambia la línea descomentada y se vuelve a subir.

```mermaid
flowchart LR
    D["#define PLACA_A"] --> A1["board_config_a.h"]
    D --> A2["MAC …0x00"]
    D --> A3["IP_ESTATICA de A"]
    D --> A4["Nombre: 'Mega Pulsadores A'"]

    E["#define PLACA_B"] --> B1["board_config_b.h"]
    E --> B2["MAC …0x01"]
    E --> B3["IP_ESTATICA de B"]
    E --> B4["Nombre: 'Mega Pulsadores B'"]

    F["#define PLACA_C"] --> C1["board_config_c.h"]
    F --> C2["MAC …0x02"]
    F --> C3["IP_ESTATICA de C"]
    F --> C4["Nombre: 'Mega Pulsadores C'"]
```

Ese único `#define` selecciona a la vez **cuatro cosas**:

- Qué fichero de configuración se incluye (`board_config_a.h`,
  `board_config_b.h` o `board_config_c.h`), con los pines realmente
  cableados en esa unidad.
- El último byte de la MAC (distinto en cada unidad, para no colisionar
  en la red).
- La IP fija de esa unidad (`IP_ESTATICA`, distinta en cada unidad).
- El nombre del dispositivo en HA (`Mega Pulsadores A/B/C`, `Mega
  Dispositivos A/B/C`).

!!! success "Protección contra errores"
    Si por error se compila sin descomentar ninguna línea, o con varias
    a la vez, el propio `.ino` falla la compilación con un `#error`
    explícito en vez de subir un firmware con identidad ambigua.

## Direcciones MAC

Las MACs se inventan localmente porque el Mega + shield Ethernet no
trae MAC de fábrica (primer byte `0x02` = "administrada localmente").
El byte `[3]` distingue familia (`0x01` = mega_pulsadores, `0x02` =
mega_dispositivos) para que nunca choquen en la red, y el último byte
distingue unidad A (`0x00`) de B (`0x01`) de C (`0x02`).

Se usa además `device.enableExtendedUniqueIds()` para que HA no
confunda entidades con el mismo ID (p. ej. `p14`) entre unidades del
mismo rol.
