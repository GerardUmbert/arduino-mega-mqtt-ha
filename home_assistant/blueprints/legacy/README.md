# Blueprints en desuso

## `persiana_posicion.yaml`

Simulaba posición (0-100%) para una persiana de `mega_dispositivos` con
firmware **anterior a 1.6.0**, que no reportaba posición real — usaba 4
helpers `input_number` por persiana y estimaba el movimiento por tiempo
desde el lado de Home Assistant.

Desde `mega_dispositivos` 1.6.0, la posición se reporta de forma nativa
(el propio firmware la calcula y la publica por MQTT vía
`HACover::PositionFeature`), y desde la 1.8.0 también se puede comandar
con `cover.set_cover_position` directamente (ver `../README.md`,
sección "Posición de persianas"). Con firmware 1.8.0+ y el parche de
`mega_dispositivos/lib_overrides/` instalado, no instancies este
blueprint: usa `cover.set_cover_position` directamente sobre la
entidad `cover.*`.

Solo relevante si alguna unidad `mega_dispositivos` sigue en una versión
de firmware anterior a 1.6.0 sin actualizar (sin reporte de posición
en absoluto).

## `persiana_ir_a_posicion.yaml`

Script-workaround usado entre el momento en que se detectó que
`cover.set_cover_position` no hacía nada sobre estas entidades (la
librería `ArduinoHA` de fábrica no acepta comandos de posición, solo
los reporta) y el momento en que se resolvió de verdad parcheando esa
librería (`mega_dispositivos` 1.8.0, ver
`mega_dispositivos/lib_overrides/README.md`).

Movía la persiana con `open_cover`/`close_cover` y vigilaba
`current_position` (que el firmware sí reportaba desde la 1.6.0) para
parar con `stop_cover` al cruzar el objetivo — sin tocar el firmware,
100% desde Home Assistant.

Con `mega_dispositivos` 1.8.0+ y el parche de `lib_overrides/`
instalado, no hace falta: `cover.set_cover_position` ya funciona de
forma nativa. Solo relevante si alguna unidad sigue en firmware
anterior a 1.8.0, o sin el parche aplicado.
