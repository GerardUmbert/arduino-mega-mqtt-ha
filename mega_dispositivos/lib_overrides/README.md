# Parche local de ArduinoHA — soporte real de `set_cover_position`

## Por qué existe esto

La librería `ArduinoHA` (`dawidchyrzynski/arduino-home-assistant`, tal
como se instala desde el Library Manager de Arduino) permite que
`HACover` **reporte** su posición (0-100%) a Home Assistant —
`HACover::PositionFeature`, ya usado en `mega_dispositivos.ino` desde
la versión 1.6.0 — pero **no** implementa ningún mecanismo para
**recibir** comandos de posición desde HA. No existe ningún topic
`set_position_topic` en el discovery, ninguna suscripción a él, ni
ningún callback tipo `onPositionCommand` en la clase.

Consecuencia real: `cover.set_cover_position` sobre cualquier
`cover.persiana_XX_YY` de este proyecto **no hacía nada, sin ningún
error visible** — el slider de posición de la tarjeta de HA se movía
en la UI pero no llegaba a mover la persiana. Confirmado leyendo el
código fuente real de la librería (versión 2.1.0, la instalada al
escribir esto) — no es un problema de configuración ni de versión de
firmware.

Ver también `CHANGELOG.md` del repo raíz: hubo un primer intento de
arreglar esto (`mega_dispositivos` v1.8.0) que asumía que la librería
ya tenía este soporte y no compilaba (`'class HACover' has no member
named 'onPositionCommand'`) — se revirtió, y el problema se resolvió
primero con un workaround 100% en Home Assistant
(`home_assistant/blueprints/persiana_ir_a_posicion.yaml`). Esta carpeta
es el intento siguiente: arreglarlo en el firmware de verdad, parcheando
la librería.

## Qué toca el parche exactamente

Dos ficheros de `ArduinoHA`, ambos bajo `src/device-types/`:

- `HACover.h`
- `HACover.cpp`

Cambios, todos marcados con comentarios `PATCHED` en el propio código:

1. Nuevo método público `onPositionCommand(callback)`, con la firma
   `void callback(const int16_t position, HACover* sender)` — paralelo
   a `onCommand()`, que ya existía para open/close/stop.
2. Nueva constante local `HASetPositionTopic` = `"set_position_topic"`
   (clave completa, sin abreviar — la librería original abrevia sus
   topics, p. ej. `pos_t` para posición, pero para no arriesgarse a
   equivocar la abreviatura exacta que usa Home Assistant core sin
   poder verificarla contra su código fuente en el momento de escribir
   esto, se usa el nombre largo, que el esquema MQTT de HA también
   acepta).
3. `buildSerializer()`: si `PositionFeature` está activo, añade
   `set_position_topic` al payload de discovery — sin esto, HA nunca
   sabe que puede enviar comandos de posición para esta entidad y
   `supported_features` no incluye el bit `SET_POSITION`.
4. `onMqttConnected()`: se suscribe también a ese topic (solo si
   `PositionFeature` está activo).
5. `onMqttMessage()` + nuevo método privado `handlePositionCommand()`:
   parsea el payload numérico (usando `HANumeric::fromStr`, el mismo
   mecanismo que ya usa `HANumber` en esta librería para parsear
   comandos numéricos) y llama al callback registrado, con el valor
   ya saturado a 0-100.

**Todo lo demás del fichero es idéntico al original** — mismo
`CoverCommand`/`onCommand()` para open/close/stop, mismos topics de
estado/posición reportada. El parche solo añade, no quita ni cambia
comportamiento existente.

## Cómo instalarlo

1. Instala `ArduinoHA` normalmente desde el Library Manager de Arduino
   IDE (Herramientas → Gestionar bibliotecas → busca "Home Assistant
   Integration" de Dawid Chyrzynski → instalar), si no la tienes ya.
2. Localiza la carpeta donde el IDE la instaló. En Windows suele ser:
   ```
   Documentos\Arduino\libraries\ArduinoHA\src\device-types\
   ```
   (o `home-assistant-integration` según cómo la nombre tu IDE — busca
   la carpeta que contenga `HACover.h`/`HACover.cpp`).
3. Copia los dos ficheros de esta carpeta
   (`mega_dispositivos/lib_overrides/ArduinoHA/src/device-types/`)
   **directamente dentro de** esa carpeta `device-types\` que acabas de
   localizar, sobrescribiendo los `HACover.h`/`HACover.cpp` que ya hay
   ahí — mismo nivel, mismo nombre, se reemplazan sin más.

   ⚠️ **NO los metas en una subcarpeta nueva** (ni `old\`, ni
   `patched\`, ni ninguna otra) dentro de `device-types\`. Los `#include`
   de dentro de `HACover.h`/`.cpp` (p. ej. `#include
   "HABaseDeviceType.h"`) son relativos a esa carpeta `device-types\` —
   si los ficheros quedan un nivel más adentro, esos includes dejan de
   encontrar nada y falla con `fatal error: HABaseDeviceType.h: No such
   file or directory`. Si te ha pasado esto, mueve los dos ficheros un
   nivel hacia arriba (a `device-types\` directamente) y borra la
   subcarpeta vacía.
4. Recompila `mega_dispositivos.ino`. Si compilaba antes, debería
   seguir compilando — el parche es aditivo. Si ves el error `'class
   HACover' has no member named 'onPositionCommand'`, significa que la
   copia no sobrescribió los ficheros correctos (revisa que sea
   exactamente esa carpeta `device-types` la que se está compilando, y
   que los dos ficheros estén ahí mismo, no en una subcarpeta — ver
   aviso del paso 3).

## ⚠️ Qué implica mantener esto

- **No es un fork con actualizaciones automáticas.** Si en el futuro
  actualizas `ArduinoHA` desde el Library Manager, se sobrescribirán
  estos dos ficheros con los originales sin avisar, y `mega_dispositivos`
  dejará de compilar hasta que vuelvas a copiar el parche encima. No
  hay forma de detectar esto automáticamente — revisa este README tras
  cualquier actualización de la librería.
- Parcheado contra la versión **2.1.0** de `ArduinoHA` (`library.properties`
  de la librería instalada al escribir esto). Si actualizas a una
  versión bastante más nueva, revisa que `HACover.cpp`/`.h` no hayan
  cambiado de forma incompatible con este parche antes de copiar
  encima a ciegas — compáralos con el original de esa nueva versión.
- Esto solo afecta a tu instalación local del IDE — no hay forma de
  "instalar" este parche automáticamente vía Library Manager. Cada
  persona que compile `mega_dispositivos.ino` (tú, tu amigo) necesita
  aplicar este mismo paso a mano en su propio `Documentos\Arduino\libraries\`.
