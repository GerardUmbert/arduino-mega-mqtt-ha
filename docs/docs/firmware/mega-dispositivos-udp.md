# `mega_dispositivos_udp` (relés + UDP directo)

!!! warning "Prototipo — sin compilar ni probar en placa"
    Versión `0.1.0-udp`. Escrito pero **no compilado ni flasheado
    todavía**. El firmware estable sigue siendo
    [`mega_dispositivos`](mega-dispositivos.md).

Copia de [`mega_dispositivos`](mega-dispositivos.md) (partida de su
versión 1.9.1) que, además de recibir órdenes de HA por MQTT, **escucha
paquetes UDP** de [`mega_pulsadores_low_ram_udp`](mega-pulsadores-low-ram-udp.md)
y ejecuta una **tabla de reglas** sobre luces y persianas. Funciona con
Home Assistant y/o el broker caídos.

El comportamiento por HA (`HASwitch`, `HACover`, posición estimada,
parada de seguridad por timeout, `set_cover_position`) es **el mismo**.
Para que HA y UDP muevan los relés exactamente igual, la lógica se
extrajo a funciones compartidas (`aplicarLuz`, `abrirPersiana`,
`cerrarPersiana`) sin cambiar su contenido.

!!! info "ArduinoHA parcheada"
    Igual que `mega_dispositivos`, necesita la `HACover` parcheada de
    [`mega_dispositivos/lib_overrides/`](https://github.com/GerardUmbert/arduino-mega-mqtt-ha/tree/master/mega_dispositivos/lib_overrides)
    (no se ha duplicado aquí).

## Tabla de reglas

Vive en `udp_reglas.h`, en flash (`PROGMEM`, sin coste de RAM), con una
tabla por unidad (`PLACA_A`/`B`/`C` de **dispositivos**). Una fila:

```cpp
{ placa, pin_pulsador, UDP_EVT_*, ACC_*, destino }
```

| Campo | Significado |
|---|---|
| `placa` | Letra de la placa de **pulsadores** (`'A'`, `'B'`, `'C'`), no la de esta unidad |
| `pin_pulsador` | El mismo número que el `pNN` de ese pulsador en HA |
| `UDP_EVT_*` | `CORTA`, `DOBLE`, `LARGA` (inicio), `LARGA_FIN` (al soltar) |
| `ACC_*` | Acción, ver tabla siguiente |
| `destino` | Luces: el **pin** de la luz. Persianas: el pin de **subir** del par |

La tabla termina siempre con `FIN_REGLAS`. Si el `destino` no existe en
esa unidad la fila se ignora sin error, y **varias filas pueden colgar
del mismo pulsador y evento** (se ejecutan todas).

### Acciones

| Acción | Efecto |
|---|---|
| `ACC_LUZ_TOGGLE` | Enciende si estaba apagada y viceversa (lee el estado real del pin) |
| `ACC_LUZ_ON` / `ACC_LUZ_OFF` | Enciende / apaga |
| `ACC_PERSIANA_ABRIR` / `ACC_PERSIANA_CERRAR` | Mueve hasta el tope o hasta recibir `PARAR` |
| `ACC_PERSIANA_PARAR` | Para |
| `ACC_PERSIANA_TOGGLE` | En movimiento → para. Parada → arranca hacia el lado con más recorrido (cierra si la posición estimada es > 50 %) |
| `ACC_PERSIANA_ALTERNAR` | Arranca en sentido contrario al último (si ya se mueve, invierte). Pensada para «larga = mover» |

## Cómo se mapean las pulsaciones múltiples y la larga

El pulsador solo informa de **qué pasó**; la tabla decide **qué hacer**.
AceButton solo da 4 eventos, y cada uno es una fila distinta:

```cpp
const ReglaUdp REGLAS_UDP[] PROGMEM = {
    // Luz con un pulsador, y "apagar las dos luces" con doble toque
    { 'A', 14, UDP_EVT_CORTA,     ACC_LUZ_TOGGLE,        22 },
    { 'A', 14, UDP_EVT_DOBLE,     ACC_LUZ_OFF,           22 },
    { 'A', 14, UDP_EVT_DOBLE,     ACC_LUZ_OFF,           23 },

    // Persiana "mantener para mover, soltar para parar"
    { 'A', 15, UDP_EVT_LARGA,     ACC_PERSIANA_ALTERNAR, 38 },
    { 'A', 15, UDP_EVT_LARGA_FIN, ACC_PERSIANA_PARAR,    38 },
    FIN_REGLAS
};
```

Es el mismo patrón que usan los blueprints por HA (`larga` = empezar a
mover, `larga_fin` = parar). Triple, cuádruple y quíntuple **no
existen** en este firmware (AceButton no los soporta), así que lo que
hacen `persiana_pulsador_completo` con 3, 4 y 5 toques no tiene
equivalente por UDP.

| Patrón | Filas |
|---|---|
| Luz con un pulsador | `CORTA → ACC_LUZ_TOGGLE` |
| «Apagar todo» | `DOBLE → varias filas ACC_LUZ_OFF` |
| Persiana de un toque | `CORTA → ACC_PERSIANA_TOGGLE` |
| Persiana de mantener | `LARGA → ACC_PERSIANA_ALTERNAR` + `LARGA_FIN → ACC_PERSIANA_PARAR` |

Las tablas vienen **vacías** (solo ejemplos comentados con pines
inventados): hay que rellenarlas con tu cableado real.

## Qué pasa con HA

- **Estado:** si MQTT está caído, `setState()` no publica ni actualiza
  el estado interno de `HASwitch`/`HACover` (devuelve `false` en
  silencio). Por eso el estado real de una luz se lee siempre del pin
  con `digitalRead()`.
- **Al reconectar** MQTT, `sincronizarConHA()` vuelve a publicar a la
  fuerza el estado real de todas las luces y persianas (posición
  incluida), de modo que HA no se queda con el estado de antes de la
  caída.
- **Persianas:** igual que por HA, una parada se pierde si falta el
  `PARAR`; la protección `TIEMPO_MAX_MOVIMIENTO_MS` (20 s) sigue
  aplicando.

## Detalles de recepción

- Se leen como máximo 4 paquetes por vuelta de `loop()`, para no dejar
  sin atender a MQTT ni al control de persianas si llega una ráfaga.
- `procesarUdp()` se llama antes y después de `mqtt.loop()`, porque
  éste puede bloquear mientras reconecta.
- Duplicados: un paquete con la misma secuencia que el anterior de esa
  placa, dentro de 500 ms (`UDP_VENTANA_DUPLICADO_MS`), se descarta.
- Paquetes que no miden 5 bytes, que no empiezan por `0xA5` o con una
  letra de placa fuera de `A..C` se ignoran.
- `UDP_TIMEOUT_CONEXION_MQTT_MS` (150 ms) reduce lo que bloquea cada
  reintento MQTT con el broker caído; requiere Ethernet ≥ 2.0.1.

## Seguridad

!!! danger "Cualquier equipo de la LAN puede mover tus luces y persianas"
    `0xA5` solo filtra tráfico ajeno; **no es autenticación**. Cualquiera
    en tu red local puede enviar un paquete UDP al puerto `47800`. Si la
    red no es de confianza, aísla estas placas en una VLAN.

## Puertos e identificadores

| Dato | Valor |
|---|---|
| Puerto de escucha | `47800` (`UDP_PUERTO`, el mismo en los pulsadores) |
| Marca | `0xA5` (`UDP_MAGIC`) |
| Placas de pulsadores aceptadas | `'A'`, `'B'`, `'C'` (`UDP_MAX_PLACAS = 3`) |

## Configuración y `config.h`

Esta carpeta no trae `config.h`, solo `config.h.example`. Sigue los
pasos de
[Configuración (config.h)](../getting-started/config.md#carpetas-udp-mismo-paso-extra-que-low_ram)
antes de rellenar credenciales reales.
