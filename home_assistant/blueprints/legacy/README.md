# Blueprints en desuso

## `persiana_posicion.yaml`

Simulaba posición (0-100%) para una persiana de `mega_dispositivos` con
firmware **anterior a 1.6.0**, que no reportaba posición real — usaba 4
helpers `input_number` por persiana y estimaba el movimiento por tiempo
desde el lado de Home Assistant.

Desde `mega_dispositivos` 1.6.0, la posición es nativa (el propio
firmware la calcula y la reporta por MQTT vía `HACover::PositionFeature`
— ver `README.md` del repo raíz). Con firmware 1.6.0+, no instancies
este blueprint: usa `cover.set_cover_position` directamente sobre la
entidad `cover.*`, como hacen ahora `persiana_pulsador_completo.yaml` y
la integración Adaptive Cover (ver `../README.md`).

Solo relevante si alguna unidad `mega_dispositivos` sigue en una versión
de firmware anterior a 1.6.0 sin actualizar.
