# TODO

## Configuración de red (`config.h`)

- [ ] Copiar `mega_pulsadores/config.h.example` → `mega_pulsadores/config.h`
      y `mega_dispositivos/config.h.example` → `mega_dispositivos/config.h`
      (ya hecho una vez como plantilla vacía; falta rellenar valores reales).
- [ ] `BROKER_ADDR`: rellenar con la IP real de Home Assistant/Mosquitto en
      ambos `config.h` (ahora mismo están a `0.0.0.0`).
- [ ] Reservar esa IP por DHCP estático en el router para que no cambie nunca.
- [ ] `MQTT_USER` / `MQTT_PASS`: rellenar con las credenciales reales del
      broker Mosquitto en ambos `config.h` (ahora mismo están vacíos).
- [ ] Como las 2 unidades de cada rol comparten broker, normalmente basta un
      único `config.h` por rol (mismo contenido en unidad A y B).

## Jumper de identidad A/B (pin 40)

- [ ] Decidir físicamente qué unidad de cada rol es A y cuál es B.
- [ ] **Unidades A** (una `mega_pulsadores` y una `mega_dispositivos`): dejar el
      pin 40 al aire, sin conectar nada.
- [ ] **Unidades B** (la otra `mega_pulsadores` y la otra `mega_dispositivos`):
      puentear el pin 40 a cualquier GND del Mega con un cable.
- [ ] Etiquetar físicamente cada placa (A/B + rol) para no confundirlas al
      reprogramar o hacer mantenimiento.

## Pines reales cableados

- [ ] `mega_pulsadores/mega_pulsadores.ino:64-69` (`PINES_BOTONES`): ajustar la lista de pines
      a los pulsadores realmente cableados en CADA unidad (A y B tendrán listas
      distintas si no cablean lo mismo).
- [ ] `mega_dispositivos/mega_dispositivos.ino:62-66` (`PINES_LUCES`): idem para luces.
- [ ] `mega_dispositivos/mega_dispositivos.ino:77-82` (`PINES_PERSIANAS`): idem para pares
      subir/bajar de persianas.
- [ ] Revisar que ningún pin usado choque con los reservados por el shield
      Ethernet (SPI: 50/51/52/53, CS: normalmente 10).

## Verificación en Home Assistant

- [ ] Confirmar en Ajustes → Dispositivos y servicios → MQTT que las 4 unidades
      aparecen diferenciadas correctamente (Mega Pulsadores A/B, Mega
      Dispositivos A/B) sin IDs duplicados.
- [ ] Confirmar que cada pulsador (`boton_01`, `boton_02`...) aparece con sus
      6 triggers (corta/doble/triple/cuádruple/quíntuple/larga) y que cada
      luz/persiana aparece como entidad controlable.

## Lógica de automatizaciones

- [ ] Definir el mapeo pulsación → acción para cada botón (p. ej. corta =
      encender/abrir, doble = apagar/cerrar, larga = parar persiana). Decidir
      si cuádruple y quíntuple se usan para algo (p. ej. escenas, modo
      "todas las luces off") o se dejan sin automatización por ahora. Aún
      sin cerrar.
- [ ] Crear las automatizaciones en HA que conectan cada pulsador con su
      luz/persiana. Decidir si se hace a mano desde la UI o generando YAML
      con plantilla para no repetir docenas de automatizaciones iguales.

## Ajustes finos / hardware

- [ ] Comprobar si los módulos de relé (luces y persianas) son activos en
      HIGH o en LOW. El código actual asume activo en HIGH
      (`digitalWrite(..., HIGH)` para encender/activar) — si el módulo real
      es activo en LOW, hay que invertir la lógica en `onSwitchCommand` y
      `onCoverCommand` en `mega_dispositivos/mega_dispositivos.ino`.
- [ ] Confirmar si el módulo de relés de persiana tiene interlock por
      hardware (evita subir+bajar a la vez). Si lo tiene, se puede bajar o
      quitar `RETARDO_INVERSION_MS` (200ms) en `mega_dispositivos/mega_dispositivos.ino:88`.
- [ ] Probar los tiempos por defecto de `OneButton` (400ms para
      doble/triple/cuádruple/quíntuple, 1000ms para larga) con el uso real
      y ajustar con `setDebounceMs`, `setClickMs`, `setPressMs` si hace
      falta (líneas comentadas en `mega_pulsadores/mega_pulsadores.ino:140-142`). Con 5
      niveles de multi-click puede convenir ampliar `setClickMs` para dar
      más margen a pulsar 4 o 5 veces seguidas sin que se corte antes.
