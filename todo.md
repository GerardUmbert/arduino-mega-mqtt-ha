# TODO

## HVAC / termostato (pendiente de definir arquitectura)

- [ ] **Necesita más información antes de poder diseñarse** — hay un único
      termostato en la casa (setup de un amigo, detalles exactos aún sin
      confirmar) con:
      - Un interruptor FÍSICO que elige modo calor/frío.
      - Un ajuste de temperatura que dispara parar/continuar al alcanzarla.
      - Modo calor → activa suelo radiante; modo frío → activa el AC de
        techo/conductos.
      - Probablemente también controlado por relés, como luces/persianas.
- [ ] Preguntas a resolver con el dueño de la instalación antes de tocar
      firmware:
      - ¿El termostato tiene su propia electrónica que decide on/off según
        temperatura, o es un contacto seco simple que el Arduino tendría
        que leer y decidir?
      - ¿El interruptor físico de modo calor/frío está cableado ANTES o
        DESPUÉS de la lógica de decisión del termostato? ¿Selecciona qué
        relé recibe la señal de "encender", o cambia el comportamiento del
        propio termostato?
      - ¿Se quiere que Home Assistant controle esto (leer modo/temperatura
        como sensores, decidir cuándo activar relé de suelo radiante o de
        AC mediante automatización, igual que ya se hace con luces/
        persianas), o debe quedarse como sistema físico/analógico
        independiente y el Arduino solo evita interferir?
      - ¿Va a compartir Mega con luces/persianas (mismo `mega_dispositivos`,
        pines nuevos) o merece su propia unidad/rol dedicado?
- [ ] Hasta no responder esas preguntas, NO se ha tomado ninguna decisión
      de diseño ni se ha tocado ningún `.ino` para esto.

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

## Identidad A/B al compilar (PLACA_A / PLACA_B)

- [ ] Decidir físicamente qué unidad de cada rol es A y cuál es B.
- [ ] Antes de subir firmware a cada unidad: en el `.ino` correspondiente,
      dejar descomentada SOLO `#define PLACA_A` o SOLO `#define PLACA_B`
      según la unidad física a la que se va a flashear en ese momento, y
      compilar/subir. Repetir cambiando la línea para la otra unidad.
- [ ] Etiquetar físicamente cada placa (A/B + rol) para no confundirlas al
      reprogramar o hacer mantenimiento — ya no hay jumper que lo delate en
      runtime si te equivocas de unidad al flashear.

## Pines reales cableados e IP fija

- [ ] `mega_pulsadores/board_config_a.h` y `board_config_b.h`
      (`PINES_BOTONES`): ajustar la lista de pines a los pulsadores
      realmente cableados en CADA unidad (A y B tendrán listas distintas
      si no cablean lo mismo).
- [ ] `mega_dispositivos/board_config_a.h` y `board_config_b.h`
      (`PINES_LUCES` y `PINES_PERSIANAS`): idem para luces y pares
      subir/bajar de persianas — cada unidad puede tener una mezcla
      distinta (p. ej. una unidad solo con luces, la otra con luces y
      persianas).
- [ ] Revisar que ningún pin usado choque con los reservados por el shield
      Ethernet (SPI: 50/51/52/53, CS: normalmente 10).
- [ ] `IP_ESTATICA` en los 4 `board_config_*.h`: ahora mismo son
      placeholders (`192.168.1.60`-`.63`) — confirmar que están fuera del
      rango DHCP de tu router (o reservadas para la MAC de cada unidad) y
      ajustar si hace falta.

## RAM / límite de pulsadores por unidad

- [x] **Probado en placa real (2026-09-04):** con 20 pulsadores
      cableados en `PINES_BOTONES`, la placa entraba en bucle de
      reinicio nada más arrancar — crasheaba tan pronto que ni
      terminaba de imprimir el primer `Serial.print` de `setup()`
      (salía "M" y basura, repitiendo sin parar). Iterando con menos
      pulsadores: **12 arrancan bien, 16 ya falla** — el límite real
      está en algún punto entre 12 y 16, muy por debajo de la vieja
      estimación sin verificar de "20-25" que había aquí antes. La
      vieja estimación de ~250 bytes/pulsador se queda corta: el coste
      real por pulsador (objeto `OneButton` + 7 `HADeviceTrigger` +
      punteros + buffer de ID) es mayor de lo que asumía ese cálculo.
- [ ] No se ha acotado más fino que "12 sí, 16 no" — no merece la pena
      seguir bisecando a mano en placa real. Si se necesita un número
      exacto (p. ej. porque el límite real de pulsadores deseados cae
      justo en esa franja 12-16): añadir temporalmente al `setup()` de
      `mega_pulsadores.ino` una comprobación de memoria libre (técnica
      estándar en AVR — función tipo `freeMemory()` que resta el final
      del heap del inicio del stack) que imprima el resultado por
      Serial, y medir con distintos tamaños de `PINES_BOTONES`.
- [ ] Si el número real de pulsadores deseados se acerca al límite
      medido: revisar si compensa (a) recortar a 5 triggers por botón
      (quitar `ButtonQuadruplePressType`/`ButtonQuintuplePressType`, sin
      caso de uso confirmado todavía — ver "Lógica de automatizaciones"
      más abajo), o (b) añadir una tercera unidad `mega_pulsadores`
      (arquitectura ya lo permite: añadir `PLACA_C`, `board_config_c.h` y
      un tercer byte de MAC, mismo patrón que A/B).
- [ ] `mega_dispositivos` no se ha medido — su coste por dispositivo es
      menor (`HASwitch`/`HACover` sin `OneButton` de por medio), pero no
      hay número real todavía tampoco.

## Verificación en Home Assistant

- [ ] Confirmar en Ajustes → Dispositivos y servicios → MQTT que las 4 unidades
      aparecen diferenciadas correctamente (Mega Pulsadores A/B, Mega
      Dispositivos A/B) sin IDs duplicados.
- [ ] Confirmar que cada pulsador (`boton_14`, `boton_27`... nombrado por
      pin) aparece con sus 7 triggers (corta/doble/triple/cuádruple/
      quíntuple/larga/fin de larga) y que cada luz/persiana aparece como
      entidad controlable.

## Lógica de automatizaciones

- [ ] Definir el mapeo pulsación → acción para cada botón (p. ej. corta =
      encender/abrir, doble = apagar/cerrar, larga = parar persiana). Decidir
      si cuádruple y quíntuple se usan para algo (p. ej. escenas, modo
      "todas las luces off") o se dejan sin automatización por ahora. Aún
      sin cerrar.
- [ ] Crear las automatizaciones en HA que conectan cada pulsador con su
      luz/persiana. Decidir si se hace a mano desde la UI o generando YAML
      con plantilla para no repetir docenas de automatizaciones iguales.
- [ ] Persianas controladas desde un pulsador físico normal (agrupación de
      4 en una habitación, no cableado directo a mega_dispositivos): crear
      automatización en HA que, al recibir `larga` (long-press-start) de un
      `boton_XX` concreto, llame a `cover.open_cover` (o `close_cover`,
      según el botón) sobre la persiana correspondiente, y al recibir
      `larga_fin` (long-press-release) de ese mismo botón, llame a
      `cover.stop_cover`. Decidir qué pulsador de cada agrupación de 4 se
      asigna a subir y cuál a bajar la persiana de esa habitación.

## Persianas siguiendo el sol (Adaptive Cover)

- [ ] Decidir la orientación real (azimut, grados desde el norte) de
      cada fachada/ventana con persiana — dato necesario para configurar
      la integración Adaptive Cover (HACS) por persiana. Ver
      `home_assistant/blueprints/README.md`.
- [ ] Instalar Adaptive Cover vía HACS y dar de alta una instancia por
      persiana (o por grupo de ventanas con la misma orientación).
- [ ] Crear la automatización puente que vuelca el sensor de posición
      recomendada de Adaptive Cover al helper `..._objetivo` de cada
      persiana (mismo patrón que ya usan `persiana_pulsador_completo.yaml`
      y el slider manual) — no mueve el `cover` directamente.
- [ ] **Importante**: el puente debe respetar un ajuste manual del
      usuario (p. ej. persiana dejada a mano al 10% viendo una peli) y
      NO pisarlo con el cálculo del sol. Añadir a la condición del
      puente una comprobación de la entidad de override manual que
      expone Adaptive Cover (switch/botón tipo "reanudar control
      automático") — confirmar su `entity_id` real al dar de alta cada
      instancia. Ver detalle y YAML de ejemplo en
      `home_assistant/blueprints/README.md`.
- [ ] Opcional: decidir si se quiere resetear ese override manual cada
      mañana (p. ej. 7am) con una automatización aparte por persiana,
      para no quedarse en manual indefinidamente si se olvida devolver
      el control. Solo aplica si la instancia real expone el override
      como `switch` controlable — confirmar, ya que algunas versiones
      de Adaptive Cover lo resetean solas al pasar por un extremo. Ver
      YAML de ejemplo en `home_assistant/blueprints/README.md`.
- [ ] Opcional, pendiente de instalar una estación meteorológica propia
      (viento/temperatura/lluvia, marca/protocolo sin decidir todavía):
      cuando exista, añadir sus sensores como condición extra del
      puente solar (no bajar/subir por sol si está lloviendo o no
      interesa) y una automatización aparte de cierre de seguridad por
      viento fuerte (prioridad sobre cualquier control, incluido el
      override manual). Umbrales de lluvia/viento y qué persianas
      afecta el cierre por viento aún sin decidir. Ver YAML de ejemplo
      en `home_assistant/blueprints/README.md`.

### Otras ideas futuras (sin desarrollar, solo para tener en cuenta)

- [ ] **Calor interior**: con sensor de temperatura interior (o el
      termostato, cuando se defina su arquitectura), priorizar cerrar
      persianas por encima del cálculo de Adaptive Cover si se supera
      un umbral en verano — ahorro energético antes que estético.
- [ ] **Frío + sol directo en invierno**: lo contrario al caso de
      lluvia — con temperatura interior/exterior baja, dejar entrar el
      sol aunque Adaptive Cover normalmente lo bloquearía.
- [ ] **Modo ausente/vacaciones**: con nadie en casa
      (`person.*`/`device_tracker` o un `input_boolean` manual),
      simular presencia moviendo persianas en ciertos momentos en vez
      de dejarlas siempre igual.
- [ ] **Rutina de dormir/despertar**: bajar persianas de dormitorios a
      una hora fija por la noche y subirlas por la mañana — se
      solaparía con Adaptive Cover, pendiente de decidir qué manda.
- [ ] **Generalizar el override manual con presencia+TV**: si hay
      sensor de presencia y la TV del salón está encendida, tratarlo
      como override automático sin que haga falta tocar la persiana a
      mano primero.
- [ ] **Corte de red/MQTT a media subida/bajada**: qué debe pasar con
      una persiana si `mega_dispositivos` pierde conexión mientras se
      estaba moviendo — laguna de seguridad de la arquitectura actual,
      no específica de Adaptive Cover.
- [ ] **Nieve/granizo** (si la estación meteo lo detecta): mismo
      tratamiento que el cierre de seguridad por viento fuerte.

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
