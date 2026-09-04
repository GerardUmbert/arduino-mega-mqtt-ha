# Cómo medir RAM real en mega_pulsadores

`mega_pulsadores.ino` incluye una sonda temporal de RAM libre
(`freeMemory()`, marcada `⚠️ TEMPORAL` en el código) que imprime por
Serial, al final de `setup()`, cuántos bytes de SRAM quedan libres en
ese momento — el punto de mínima RAM libre del programa, con todo ya
creado (pulsadores, triggers, Ethernet, MQTT).

Una sola lectura solo te da un número total, no un desglose. Para
saber qué se está comiendo la RAM (Ethernet/MQTT de base, cada
pulsador, cada tipo de trigger) hacen falta **varias lecturas a
distintas configuraciones** y comparar las diferencias.

## Requisitos

- Placa real (Arduino Mega 2560 + shield Ethernet) conectada por USB.
- Arduino IDE con el Serial Monitor a **9600 baudios** (`Serial.begin(9600)`
  en el `.ino` — si el monitor está a otro baudrate verás basura, no un
  crash real).
- Puedes editar `PINES_BOTONES[]` en `board_config_a.h` (o `board_config_b.h`
  si estás en la unidad B) para probar con menos o más pulsadores de
  los que tengas cableados de verdad — no hace falta que el pin exista
  físicamente para que el firmware arranque, mide RAM igual.

## Qué leer

Después de flashear, abre el Serial Monitor. Busca la línea:

```
[debug] RAM libre: NNN bytes
```

Apunta el número (`NNN`) junto con la configuración exacta que tenías
en ese momento: cuántos pines en `PINES_BOTONES[]` y qué `HABILITAR_*`
estaban activos/comentados en el `.ino`.

## Plan de medición (3-4 ciclos flash-y-lee)

### 1. Coste base (Ethernet + MQTT + ArduinoHA, sin apenas pulsadores)

- `board_config_a.h` → `PINES_BOTONES[] = { 2 };` (un solo pin, para
  que el código siga siendo válido — no puede estar vacío, ver nota
  al final).
- `mega_pulsadores.ino` → deja los `HABILITAR_*` por defecto (corta,
  doble, larga, largaFin activos; triple, cuádruple, quíntuple
  comentados).
- Flashea, lee `RAM libre`. Este número es aprox. el coste fijo de
  Ethernet + PubSubClient + ArduinoHA + 1 pulsador — casi todo "coste
  base", ya que solo hay un pulsador de por medio.

### 2. Coste por pulsador (misma config de triggers, más pines)

- `board_config_a.h` → `PINES_BOTONES[] = { 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 17 };`
  (12 pines — el número que sabemos que arranca bien, ver
  `todo.md` → "RAM / límite de pulsadores").
- Mismos `HABILITAR_*` que el paso 1 (no los toques).
- Flashea, lee `RAM libre`.
- **Cálculo**: `(RAM libre paso 1 − RAM libre paso 2) / (12 − 1)` =
  bytes reales que cuesta cada pulsador adicional, con la config de
  triggers por defecto.

### 3. Coste por tipo de trigger (mismo nº de pulsadores, más triggers)

- `board_config_a.h` → deja los 12 pines del paso 2, sin tocar.
- `mega_pulsadores.ino` → descomenta las 3 líneas que faltan:
  `HABILITAR_TRIPLE`, `HABILITAR_CUADRUPLE`, `HABILITAR_QUINTUPLE`
  (los 7 triggers activos a la vez).
- Flashea, lee `RAM libre`.
- **Cálculo**: `(RAM libre paso 2 − RAM libre paso 3) / 12` = bytes
  reales que cuesta añadir triple+cuádruple+quíntuple a cada pulsador
  (los 3 juntos, no por separado — para aislar uno de los tres habría
  que repetir el paso activando solo uno cada vez).

### 4. (Opcional) Verificar el límite real con la config final

- Una vez tengas los números de arriba, puedes estimar cuántos
  pulsadores caben con el `HABILITAR_*` que vayas a usar de verdad, y
  confirmarlo flasheando esa cantidad exacta antes de cablear la placa
  final. No des por buena una extrapolación sin probarla en placa real
  — ya pasó una vez con la estimación "20-25" (ver `CHANGELOG.md`,
  entrada 1.7.0).

## Después de medir

Con los números reales:

- Actualiza el comentario "⚠️ RAM" en `mega_pulsadores.ino` (cerca de
  `NUM_PULSADORES`) y la sección "RAM / límite de pulsadores" de
  `todo.md` con las cifras medidas en vez de las estimaciones actuales
  (12 arranca / 16 falla, sin desglose).
- Quita la sonda `freeMemory()` del `.ino` (marcada `⚠️ TEMPORAL`) si
  ya no hace falta seguir midiendo — o déjala si prevés volver a tocar
  la configuración de pulsadores/triggers en el futuro y te interesa
  poder remedir rápido.

## Nota: `PINES_BOTONES[]` no puede estar vacío

El firmware asume al menos 1 pulsador — `NUM_PULSADORES` se calcula de
`sizeof(PINES_BOTONES) / sizeof(PINES_BOTONES[0])`, y con el array
vacío el compilador falla igual que pasaba con las persianas en
`mega_dispositivos` (ver `CHANGELOG.md`, entrada 1.6.3 de
`mega_dispositivos`) — ese fix era para arrays de tamaño 0 en otro
sitio del código, pero `PINES_BOTONES[]` en sí mismo declarado vacío
(`{}`) sigue sin ser válido. Usa como mínimo 1 pin para las pruebas.
