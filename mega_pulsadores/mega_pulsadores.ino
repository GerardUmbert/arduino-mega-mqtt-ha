// ===========================================================
// MEGA_PULSADORES
// Lee pulsadores físicos y envía eventos MQTT (device triggers)
// a Home Assistant: pulsación corta, doble, triple, cuádruple,
// quíntuple, larga y fin de larga (al soltar). Cada uno de los 7 se
// activa/desactiva por separado (ver HABILITAR_CORTA/HABILITAR_DOBLE/
// etc. más abajo — desactivar alguno ahorra RAM, útil si necesitas más
// pulsadores de los que caben con los 7 triggers completos). Por
// defecto: corta, doble, larga y fin de larga activos; triple,
// cuádruple y quíntuple desactivados.
// No controla ningún relé. Solo ENVÍA información.
//
// Además, cada pulsador tiene un HAButton virtual (entidad real y
// pulsable en la UI de HA, a diferencia de los HADeviceTrigger) que
// simula un clic corto en ese pin al pulsarlo desde HA — pulsarlo 3
// veces seguidas rápido se detecta como triple clic igual que con el
// dedo. No simula pulsación larga (ver comentario junto a
// SIMULACION_PULSO_MS más abajo).
//
// Mismo firmware para las 2 unidades físicas (A y B): la identidad
// (pines, MAC, IP, nombre) se decide en TIEMPO DE COMPILACIÓN con
// PLACA_A/PLACA_B (ver más abajo) — no hay jumper físico.
//
// Librerías necesarias (Arduino Library Manager):
//   - ArduinoHA        https://github.com/dawidchyrzynski/arduino-home-assistant
//   - OneButton        https://github.com/mathertel/OneButton
//   - Ethernet (incluida en el IDE si usas shield W5100/W5500)
//
// Para medir RAM real en placa (cuánto cuesta cada pulsador/trigger),
// ver instructions.md en esta misma carpeta. Para una alternativa a
// OneButton más ligera en RAM pero investigada y aparcada (pierde
// soporte de triple/cuádruple/quíntuple clic), ver to_review.md.
// ===========================================================

#include <Ethernet.h>
#include <ArduinoHA.h>
#include <OneButton.h>

// ===========================================================
// CONFIGURACIÓN DE RED
// Copia "config.h.example" como "config.h" en esta misma carpeta
// y rellena tu IP/usuario/password reales. "config.h" está en
// .gitignore, así que tus credenciales no se suben al repositorio.
// ===========================================================
#include "config.h"

// ===========================================================
// IDENTIFICACIÓN DE LA PLACA — ⚠️ CAMBIAR ANTES DE CADA FLASH ⚠️
// Deja SOLO una de las dos líneas descomentada según a qué unidad
// física vayas a subir este firmware. Selecciona a la vez: los pines
// cableados, la MAC, la IP fija y el nombre en Home Assistant
// (todo en board_config_a.h / board_config_b.h). Vuelve a compilar
// y subir tras cambiarla.
// ===========================================================
#define PLACA_A
// #define PLACA_B

#if defined(PLACA_A) && defined(PLACA_B)
    #error "Deja solo una de PLACA_A o PLACA_B descomentada, no las dos."
#elif !defined(PLACA_A) && !defined(PLACA_B)
    #error "Descomenta PLACA_A o PLACA_B para indicar qué unidad es esta."
#endif

#if defined(PLACA_A)
    #include "board_config_a.h"
#elif defined(PLACA_B)
    #include "board_config_b.h"
#endif

// ===========================================================
// EVENTOS ACTIVOS POR PULSADOR
// Cada uno de los 7 triggers se activa/desactiva por separado, para
// TODOS los pulsadores de esta unidad. Comenta/descomenta según
// necesites — desactivar uno ahorra RAM (cada HADeviceTrigger que no
// se crea es memoria y una entidad MQTT menos), útil si necesitas más
// pulsadores de los que caben con los 7 triggers completos (ver "RAM /
// límite de pulsadores" más abajo y en todo.md).
// ⚠️ Antes de cambiar cualquiera de estas líneas, revisa qué
// blueprints tienes instanciados en HA para los pulsadores de esta
// unidad — un blueprint que espera un trigger que ya no existe
// simplemente deja de dispararse, sin error visible:
//   - persiana_pulsador_completo.yaml usa corta/doble/triple/cuádruple/
//     quíntuple/larga/largaFin (las 5 pulsaciones + larga).
//   - luz_pulsador.yaml usa corta/triple/larga — con el TRIPLE
//     desactivado por defecto, su función de apagado-automático-a-los-
//     N-minutos deja de dispararse.
//   - persiana_pulsador.yaml usa solo larga/largaFin.
// Por defecto: corta, doble, larga y fin de larga activos (caso de uso
// confirmado); triple, cuádruple y quíntuple desactivados (sin caso de
// uso confirmado salvo los blueprints de arriba — actívalos si los
// usas).
// ===========================================================
#define HABILITAR_CORTA
#define HABILITAR_DOBLE
// #define HABILITAR_TRIPLE
// #define HABILITAR_CUADRUPLE
// #define HABILITAR_QUINTUPLE
#define HABILITAR_LARGA
#define HABILITAR_LARGA_FIN

// ===========================================================
// BOTON VIRTUAL POR PULSADOR (HAButton) — ON/OFF
// Coméntalo para NO crear los HAButton. Cada uno es una entidad MQTT
// completa (unique_id, topic de comando y buffer de nombre propios),
// así que desactivarlo ahorra más RAM por pulsador que quitar
// cualquiera de los 7 triggers de arriba — mismo criterio que en
// mega_pulsadores_low_ram (ver su CHANGELOG, entrada donde se añadió
// este flag).
//
// Qué se pierde al desactivarlo: los botones "Press" en Controls del
// dispositivo en HA (simulan un clic corto/doble/triple/etc. desde la
// UI). NO afecta a los pulsadores físicos ni a los HADeviceTrigger.
//
// ACTIVADO por defecto (a diferencia de mega_pulsadores_low_ram, donde
// está desactivado por defecto): aquí el límite de pulsadores por
// unidad es más bajo (12-16, ver comentario junto a NUM_PULSADORES) y
// normalmente sobra margen de RAM para tenerlo activo.
#define HABILITAR_BOTON_VIRTUAL

// ===========================================================
// DEBUG POR SERIAL — ON/OFF
// Coméntalo para compilar sin nada de instrumentación. Quita:
//   - freeMemory() y el "[debug] RAM libre"
//   - el "[boton] ... -> ..." de cada pulsación
// Cada Serial.print bloquea el loop mientras vacía el buffer de 9600
// baudios — con muchos pulsadores y una línea impresa por pulsación,
// la pausa se nota. Mismo criterio que en mega_pulsadores_low_ram.
//
// ACTIVADO por defecto: a diferencia de low_ram, aquí no hay un timing
// tan ajustado (OneButton se lee con tick() normal, sin el requisito
// de <5ms entre llamadas que tiene AceButton), así que el coste de
// mantenerlo activo es menor. Desactívalo si necesitas ese margen.
#define HABILITAR_DEBUG

EthernetClient client;
HADevice device(mac, sizeof(mac));

// NUM_PULSADORES se calcula a partir de PINES_BOTONES, definido en
// board_config_a.h o board_config_b.h según PLACA_A/PLACA_B (ver más arriba).
//
// ⚠️ RAM: cada pulsador cuesta bastante más SRAM de lo que parece a
// simple vista (objeto OneButton + 7 HADeviceTrigger + punteros +
// buffer de ID), y el Mega solo tiene 8 KB en total, de los que el
// shield Ethernet y ArduinoHA ya reservan una parte antes de llegar
// aquí. Probado en placa real: 12 pulsadores arrancan bien, 16 ya
// entra en bucle de reinicio (crashea tan pronto que ni termina de
// imprimir el primer Serial.print de setup()) — el límite real está
// en algún punto entre 12 y 16, muy por debajo de la vieja estimación
// sin verificar de "20-25" que había aquí antes. Si vas a cablear más
// pulsadores de los que ya hay en board_config_a.h/board_config_b.h,
// comprueba en placa real que sigue arrancando — no des por buena
// ninguna cifra sin probarla. Ver "RAM / límite de pulsadores" en
// todo.md.
const int NUM_PULSADORES = sizeof(PINES_BOTONES) / sizeof(PINES_BOTONES[0]);

// Cuenta cuántos de los 7 triggers están activos (para dimensionar
// HAMqtt más abajo). defined() no se puede usar dentro del cuerpo de
// un #define normal, así que se acumula paso a paso dentro de #if.
#if defined(HABILITAR_CORTA)
    #define _TRIGGERS_ACTIVOS_1 1
#else
    #define _TRIGGERS_ACTIVOS_1 0
#endif
#if defined(HABILITAR_DOBLE)
    #define _TRIGGERS_ACTIVOS_2 (_TRIGGERS_ACTIVOS_1 + 1)
#else
    #define _TRIGGERS_ACTIVOS_2 _TRIGGERS_ACTIVOS_1
#endif
#if defined(HABILITAR_TRIPLE)
    #define _TRIGGERS_ACTIVOS_3 (_TRIGGERS_ACTIVOS_2 + 1)
#else
    #define _TRIGGERS_ACTIVOS_3 _TRIGGERS_ACTIVOS_2
#endif
#if defined(HABILITAR_CUADRUPLE)
    #define _TRIGGERS_ACTIVOS_4 (_TRIGGERS_ACTIVOS_3 + 1)
#else
    #define _TRIGGERS_ACTIVOS_4 _TRIGGERS_ACTIVOS_3
#endif
#if defined(HABILITAR_QUINTUPLE)
    #define _TRIGGERS_ACTIVOS_5 (_TRIGGERS_ACTIVOS_4 + 1)
#else
    #define _TRIGGERS_ACTIVOS_5 _TRIGGERS_ACTIVOS_4
#endif
#if defined(HABILITAR_LARGA)
    #define _TRIGGERS_ACTIVOS_6 (_TRIGGERS_ACTIVOS_5 + 1)
#else
    #define _TRIGGERS_ACTIVOS_6 _TRIGGERS_ACTIVOS_5
#endif
#if defined(HABILITAR_LARGA_FIN)
    #define _TRIGGERS_ACTIVOS_7 (_TRIGGERS_ACTIVOS_6 + 1)
#else
    #define _TRIGGERS_ACTIVOS_7 _TRIGGERS_ACTIVOS_6
#endif
#define NUM_TRIGGERS_POR_PULSADOR _TRIGGERS_ACTIVOS_7

// Entidades MQTT por pulsador: sus triggers activos, + 1 por el
// HAButton virtual si está activado (ver HABILITAR_BOTON_VIRTUAL), + margen.
#ifdef HABILITAR_BOTON_VIRTUAL
    #define _ENTIDADES_POR_PULSADOR (NUM_TRIGGERS_POR_PULSADOR + 1)
#else
    #define _ENTIDADES_POR_PULSADOR NUM_TRIGGERS_POR_PULSADOR
#endif
HAMqtt mqtt(client, device, NUM_PULSADORES * _ENTIDADES_POR_PULSADOR + 2);

// Array estático (no punteros a objetos con new): OneButton tiene
// constructor por defecto + setup() para configurar el pin después,
// así que no hace falta reservar cada uno en el heap — ahorra el
// overhead de malloc por pulsador (unos pocos bytes cada uno, se nota
// a partir de una docena). HADeviceTrigger no puede hacer lo mismo
// (sus constructores exigen tipo+subtype al crearse, no tiene
// constructor por defecto), así que esos siguen con new/punteros más
// abajo.
OneButton botones[NUM_PULSADORES];

// ===========================================================
// BOTÓN VIRTUAL POR PULSADOR (simular pulsaciones desde HA)
// Un HAButton por pulsador — a diferencia de HADeviceTrigger, este SÍ
// es una entidad visible y pulsable en la UI de HA (aparece como un
// botón normal en Ajustes → Entidades y en cualquier tarjeta). Al
// pulsarlo en HA, el firmware inyecta UN clic corto en la máquina de
// estados de OneButton de ese pulsador — usando OneButton::tick(bool),
// que es el método oficial de la librería para alimentar el estado sin
// leer el pin físico (documentado: "no digital input pin is checked
// because the current level is given by the parameter"). Así, pulsar
// el botón de HA 3 veces seguidas rápido se detecta como un TRIPLE
// clic exactamente igual que 3 pulsaciones físicas reales — toda la
// lógica de debounce/multiclic/temporización sigue siendo la misma,
// no se duplica nada.
//
// Limitación deliberada: el pulso simulado es corto y fijo
// (SIMULACION_PULSO_MS), pensado para corta/doble/triple/cuádruple/
// quíntuple. No sirve para simular una pulsación LARGA (que necesita
// mantener el pin activo un tiempo variable) — eso queda fuera de esta
// primera versión.
#ifdef HABILITAR_BOTON_VIRTUAL
#define SIMULACION_PULSO_MS 90

HAButton* botonVirtual[NUM_PULSADORES];

// Por pulsador: 0 = sin simulación en curso. Si no es 0, es el
// millis() en el que hay que soltar el pulso simulado (ver loop()).
unsigned long simulacionSoltarEn[NUM_PULSADORES];
#endif

// Orden deliberado: corta -> doble -> triple -> cuádruple -> quíntuple
// (progresión 1-2-3-4-5 pulsaciones), y larga/fin de larga aparte, al
// final, como caso especial.
#ifdef HABILITAR_CORTA
HADeviceTrigger* corta[NUM_PULSADORES];
#endif
#ifdef HABILITAR_DOBLE
HADeviceTrigger* doble[NUM_PULSADORES];
#endif
#ifdef HABILITAR_TRIPLE
HADeviceTrigger* triple[NUM_PULSADORES];
#endif
#ifdef HABILITAR_CUADRUPLE
HADeviceTrigger* cuadruple[NUM_PULSADORES];
#endif
#ifdef HABILITAR_QUINTUPLE
HADeviceTrigger* quintuple[NUM_PULSADORES];
#endif
#ifdef HABILITAR_LARGA
HADeviceTrigger* larga[NUM_PULSADORES];
#endif
// Se dispara al SOLTAR una pulsación larga. Imprescindible para
// automatizaciones "mantener pulsado para mover / soltar para parar"
// (p. ej. persianas): "larga" = empezar a subir, "larga_fin" = parar.
#ifdef HABILITAR_LARGA_FIN
HADeviceTrigger* largaFin[NUM_PULSADORES];
#endif

// Buffers de texto para los IDs. Deben ser globales (viven todo el
// programa) porque HADeviceTrigger se queda con el puntero al texto,
// no con una copia. Formato "pNN" (antes "boton_NN") — más corto,
// ahorra RAM; el pin es uint8_t (máx. 2 dígitos en un Mega), así que
// "p" + 2 dígitos + '\0' caben en 4 bytes.
// ⚠️ Cambia el subtype que ve HA: cualquier automatización ya
// instanciada desde un blueprint con el "Subtype del botón" antiguo
// ("boton_NN") hay que volver a seleccionarla desde la UI (el nuevo
// subtype "pNN" no coincide con el guardado).
char idBoton[NUM_PULSADORES][4];

// unique_id del HAButton virtual de cada pulsador — "v" + idBoton[i]
// (ej. "vp14"). Un buffer aparte de idBoton porque HADeviceTrigger y
// HAButton son cosas distintas en ArduinoHA (trigger vs. entidad real)
// y conviene que sus identificadores no se confundan a simple vista.
// Solo existe si el botón virtual está activo.
#ifdef HABILITAR_BOTON_VIRTUAL
char idBotonVirtual[NUM_PULSADORES][5];
#endif

void imprimirMac() {
    for (uint8_t i = 0; i < sizeof(mac); i++) {
        if (mac[i] < 0x10) Serial.print('0');
        Serial.print(mac[i], HEX);
        if (i < sizeof(mac) - 1) Serial.print(':');
    }
}

void onMqttConnected() {
    Serial.println(F("[mqtt] conectado al broker"));
}

void onMqttDisconnected() {
    Serial.println(F("[mqtt] desconectado del broker"));
}

// ⚠️ TEMPORAL — DEBUG DE RAM (quitar cuando ya no haga falta medir).
// Técnica estándar en AVR: la RAM libre es la distancia entre el final
// del heap (__brkval, o el final de .bss si el heap aún no se ha
// tocado) y la dirección actual del stack pointer. Se llama una vez al
// final de setup(), cuando ya está todo creado (pulsadores, triggers,
// Ethernet, MQTT) — el punto de mínima RAM libre del programa.
// Ver instructions.md (misma carpeta) para el procedimiento completo
// de medición — una sola lectura no basta para saber qué se come la
// RAM (coste fijo de Ethernet/MQTT vs. coste por pulsador vs. coste
// por tipo de trigger), hacen falta varias lecturas comparadas.
#ifdef HABILITAR_DEBUG
extern char* __brkval;
extern char __bss_end;
int freeMemory() {
    char top;
    return &top - (__brkval ? __brkval : &__bss_end);
}
#endif

#ifdef HABILITAR_CORTA
void onClick(void* param) {
    int idx = reinterpret_cast<int>(param);
#ifdef HABILITAR_DEBUG
    bool ok = corta[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.print(F(" -> corta ["));
    Serial.print(ok ? F("publicado") : F("FALLO MQTT"));
    Serial.println(']');
#else
    corta[idx]->trigger();
#endif
}
#endif

#ifdef HABILITAR_DOBLE
void onDoubleClick(void* param) {
    int idx = reinterpret_cast<int>(param);
#ifdef HABILITAR_DEBUG
    bool ok = doble[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.print(F(" -> doble ["));
    Serial.print(ok ? F("publicado") : F("FALLO MQTT"));
    Serial.println(']');
#else
    doble[idx]->trigger();
#endif
}
#endif

// OneButton no tiene "attachTripleClick"/"attachQuadrupleClick"/
// "attachQuintupleClick" propios: se usa attachMultiClick (una sola vez)
// y se filtra el número exacto de clics detectados. Solo hace falta
// este callback (y el attachMultiClick de setup()) si triple, cuádruple
// o quíntuple están activos.
#if defined(HABILITAR_TRIPLE) || defined(HABILITAR_CUADRUPLE) || defined(HABILITAR_QUINTUPLE)
void onMultiClick(void* param) {
    int idx = reinterpret_cast<int>(param);
    int clics = botones[idx].getNumberClicks();
#ifdef HABILITAR_DEBUG
    bool ok = false;
    switch (clics) {
#ifdef HABILITAR_TRIPLE
        case 3: ok = triple[idx]->trigger();    break;
#endif
#ifdef HABILITAR_CUADRUPLE
        case 4: ok = cuadruple[idx]->trigger(); break;
#endif
#ifdef HABILITAR_QUINTUPLE
        case 5: ok = quintuple[idx]->trigger(); break;
#endif
    }
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.print(F(" -> multiclick x"));
    Serial.print(clics);
    Serial.print(F(" ["));
    Serial.print(ok ? F("publicado") : F("FALLO MQTT"));
    Serial.println(']');
#else
    switch (clics) {
#ifdef HABILITAR_TRIPLE
        case 3: triple[idx]->trigger();    break;
#endif
#ifdef HABILITAR_CUADRUPLE
        case 4: cuadruple[idx]->trigger(); break;
#endif
#ifdef HABILITAR_QUINTUPLE
        case 5: quintuple[idx]->trigger(); break;
#endif
    }
#endif
}
#endif

#ifdef HABILITAR_LARGA
void onLongPressStart(void* param) {
    int idx = reinterpret_cast<int>(param);
#ifdef HABILITAR_DEBUG
    bool ok = larga[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.print(F(" -> larga (inicio) ["));
    Serial.print(ok ? F("publicado") : F("FALLO MQTT"));
    Serial.println(']');
#else
    larga[idx]->trigger();
#endif
}
#endif

#ifdef HABILITAR_LARGA_FIN
void onLongPressStop(void* param) {
    int idx = reinterpret_cast<int>(param);
#ifdef HABILITAR_DEBUG
    bool ok = largaFin[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.print(F(" -> larga (fin) ["));
    Serial.print(ok ? F("publicado") : F("FALLO MQTT"));
    Serial.println(']');
#else
    largaFin[idx]->trigger();
#endif
}
#endif

#ifdef HABILITAR_BOTON_VIRTUAL
// Al pulsar el HAButton virtual de un pulsador en HA: inicia el pulso
// simulado (press). El release se dispara solo, más tarde, desde
// loop() (ver simulacionSoltarEn) — aquí solo se marca cuándo debe
// soltarse.
void onBotonVirtual(HAButton* sender) {
    for (int i = 0; i < NUM_PULSADORES; i++) {
        if (botonVirtual[i] == sender) {
            botones[i].tick(false); // false = activo en LOW = "pulsado"
            simulacionSoltarEn[i] = millis() + SIMULACION_PULSO_MS;
#ifdef HABILITAR_DEBUG
            Serial.print(F("[boton] "));
            Serial.print(idBoton[i]);
            Serial.println(F(" -> pulsación simulada desde HA"));
#endif
            break;
        }
    }
}
#endif

void setup() {
    Serial.begin(9600);
    Serial.println();
    Serial.print(F("[boot] "));
    Serial.println(NOMBRE_PLACA);
    Serial.print(F("[boot] MAC: "));
    imprimirMac();
    Serial.println();

    // Evita que HA confunda entidades/triggers con el mismo ID
    // entre la unidad A y la B (les añade un prefijo único por placa).
    device.enableExtendedUniqueIds();

    device.setName(NOMBRE_PLACA);
    device.setSoftwareVersion("1.9.0");

    // ⚠️ ORDEN CRITICO: setBufferSize() va AQUI, antes de crear ni un
    // solo HADeviceTrigger/HAButton — no después del bucle.
    //
    // Mismo bug que en mega_pulsadores_low_ram (ver su CHANGELOG,
    // entradas 1.8.3/1.8.5): el buffer por defecto de PubSubClient (256
    // bytes) no alcanza para el payload de discovery de un
    // device_automation, así que HA nunca recibe el registro del
    // trigger — sin ningún error visible, y el pulsador sigue
    // detectándose y publicando su evento con normalidad, solo que HA
    // lo descarta por no tener el trigger registrado. Aquí es aún más
    // probable que ocurra: hasta 7 triggers por pulsador (frente a 4 en
    // low_ram), así que el payload puede ser igual de largo o más.
    //
    // setBufferSize() hace un realloc, que necesita un bloque CONTIGUO
    // libre y devuelve false EN SILENCIO si no lo encuentra — por eso
    // tiene que ir antes de crear los objetos del bucle de pulsadores
    // (con new), mientras el heap todavía está intacto.
    //
    // 384 y no 512/1024, mismo cálculo que en low_ram (payload JSON +
    // topic + cabecera MQTT ronda los 260-280 bytes para el trigger más
    // largo). Si NOMBRE_PLACA se alarga mucho, recalcular.
    mqtt.setBufferSize(384);

    // ⚠️ TEMPORAL — DEBUG DIAGNOSTICO: cuenta cada entidad MQTT que se
    // crea, para comparar al final de setup() con el hueco reservado en
    // el constructor de HAMqtt.
#ifdef HABILITAR_DEBUG
    int entidadesCreadas = 0;
#endif

    // --- creamos cada pulsador y sus triggers activos (ver HABILITAR_* arriba) ---
    for (int i = 0; i < NUM_PULSADORES; i++) {
        snprintf(idBoton[i], sizeof(idBoton[i]), "p%d", PINES_BOTONES[i]);
#ifdef HABILITAR_BOTON_VIRTUAL
        snprintf(idBotonVirtual[i], sizeof(idBotonVirtual[i]), "v%s", idBoton[i]);
#endif

        botones[i].setup(PINES_BOTONES[i], INPUT_PULLUP, true); // true = activo en LOW

#ifdef HABILITAR_CORTA
        corta[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonShortPressType,     idBoton[i]);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif
#ifdef HABILITAR_DOBLE
        doble[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonDoublePressType,    idBoton[i]);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif
#ifdef HABILITAR_TRIPLE
        triple[i]    = new HADeviceTrigger(HADeviceTrigger::ButtonTriplePressType,    idBoton[i]);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif
#ifdef HABILITAR_CUADRUPLE
        cuadruple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuadruplePressType, idBoton[i]);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif
#ifdef HABILITAR_QUINTUPLE
        quintuple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuintuplePressType, idBoton[i]);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif
#ifdef HABILITAR_LARGA
        larga[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonLongPressType,      idBoton[i]);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif
#ifdef HABILITAR_LARGA_FIN
        largaFin[i]  = new HADeviceTrigger(HADeviceTrigger::ButtonLongReleaseType,    idBoton[i]);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif

#ifdef HABILITAR_BOTON_VIRTUAL
        // Botón virtual: entidad real y pulsable en la UI de HA (a
        // diferencia de los HADeviceTrigger de arriba). unique_id
        // propio (idBotonVirtual[i], ej. "vp14") para no confundirlo
        // con el subtype de los triggers.
        botonVirtual[i] = new HAButton(idBotonVirtual[i]);
        botonVirtual[i]->setName(idBoton[i]);
        botonVirtual[i]->onCommand(onBotonVirtual);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif

        // void* que se le pasa de vuelta a cada callback (ver onClick() etc.
        // más arriba) para que sepa de qué pulsador se trata — no podemos
        // capturar "i"/"idx" en una lambda porque OneButton exige un
        // function pointer puro o su variante con parámetro void*.
        void* idxParam = reinterpret_cast<void*>(i);

#ifdef HABILITAR_CORTA
        botones[i].attachClick(onClick, idxParam);
#endif
#ifdef HABILITAR_DOBLE
        botones[i].attachDoubleClick(onDoubleClick, idxParam);
#endif
#if defined(HABILITAR_TRIPLE) || defined(HABILITAR_CUADRUPLE) || defined(HABILITAR_QUINTUPLE)
        botones[i].attachMultiClick(onMultiClick, idxParam);
#endif
#ifdef HABILITAR_LARGA
        botones[i].attachLongPressStart(onLongPressStart, idxParam);
#endif
#ifdef HABILITAR_LARGA_FIN
        botones[i].attachLongPressStop(onLongPressStop, idxParam);
#endif

        // Ajustes opcionales de temporización (descomenta y ajusta si
        // los pulsadores van demasiado rápido/lentos para tu gusto):
        // botones[i].setDebounceMs(50);
        // botones[i].setClickMs(400);   // ventana para detectar doble/triple
        // botones[i].setPressMs(1000);  // tiempo para considerar "larga"
    }

#ifdef HABILITAR_DEBUG
    // ⚠️ TEMPORAL — DEBUG DIAGNOSTICO: cuantas entidades MQTT se han
    // creado de verdad vs. el hueco reservado en el constructor de
    // HAMqtt. Si "creadas" supera el "maximo", ArduinoHA descarta en
    // silencio las que no caben y se perderian triggers sin aviso.
    Serial.print(F("[debug] entidades MQTT creadas: "));
    Serial.print(entidadesCreadas);
    Serial.print(F(" / maximo reservado: "));
    Serial.println(NUM_PULSADORES * _ENTIDADES_POR_PULSADOR + 2);
    Serial.print(F("[debug] NUM_PULSADORES="));
    Serial.print(NUM_PULSADORES);
    Serial.print(F(" triggers/pulsador="));
    Serial.println(NUM_TRIGGERS_POR_PULSADOR);
#endif

    Serial.println(F("[boot] iniciando Ethernet (IP fija)..."));
    Ethernet.begin(mac, IP_ESTATICA, IP_GATEWAY, IP_GATEWAY, IP_SUBNET);

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println(F("[boot] ERROR: no se detecta el shield Ethernet"));
    } else if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println(F("[boot] ERROR: sin enlace de red (revisa el cable)"));
    } else {
        Serial.println(F("[boot] Ethernet enlazado correctamente"));
    }

    Serial.print(F("[boot] IP asignada: "));
    Serial.println(Ethernet.localIP());
    Serial.print(F("[boot] Gateway: "));
    Serial.println(Ethernet.gatewayIP());
    Serial.print(F("[boot] Subnet: "));
    Serial.println(Ethernet.subnetMask());
    Serial.print(F("[boot] DNS: "));
    Serial.println(Ethernet.dnsServerIP());

    Serial.println(F("[boot] probando TCP directo al broker (puerto 1883)..."));
    EthernetClient testClient;
    if (testClient.connect(BROKER_ADDR, 1883)) {
        Serial.println(F("[boot] TCP OK: el broker responde en ese puerto"));
        testClient.stop();
    } else {
        Serial.println(F("[boot] TCP FALLO: no se pudo abrir conexion al broker (revisa IP/puerto/firewall)"));
    }

    mqtt.onConnected(onMqttConnected);
    mqtt.onDisconnected(onMqttDisconnected);

    Serial.println(F("[boot] conectando a MQTT..."));
    mqtt.begin(BROKER_ADDR, MQTT_USER, MQTT_PASS);

#ifdef HABILITAR_DEBUG
    // ⚠️ TEMPORAL — DEBUG DE RAM: quitar junto con freeMemory() de más
    // arriba cuando ya no haga falta medir. Se imprime al final de
    // setup() a propósito: es el punto de mínima RAM libre del
    // programa (todo ya creado — pulsadores, triggers, Ethernet, MQTT).
    Serial.print(F("[debug] RAM libre: "));
    Serial.print(freeMemory());
    Serial.println(F(" bytes"));
#endif
}

void loop() {
    mqtt.loop();
#ifdef HABILITAR_BOTON_VIRTUAL
    unsigned long ahora = millis();
    for (int i = 0; i < NUM_PULSADORES; i++) {
        if (simulacionSoltarEn[i] != 0 && ahora >= simulacionSoltarEn[i]) {
            // Toca soltar el pulso simulado — no leemos el pin real
            // este ciclo, forzamos el "release" vía tick(bool) igual
            // que se forzó el "press" en onBotonVirtual().
            botones[i].tick(true); // true = inactivo (soltado)
            simulacionSoltarEn[i] = 0;
        } else if (simulacionSoltarEn[i] == 0) {
            // Sin simulación en curso para este pulsador: lectura
            // normal del pin físico.
            botones[i].tick();
        }
        // Si hay una simulación en curso pero todavía no toca soltar,
        // no se llama a tick() en absoluto este ciclo — el "press" ya
        // se inyectó una vez en onBotonVirtual() y no hace falta
        // repetirlo.
    }
#else
    // Sin botón virtual no hay nada que simular: lectura normal de
    // todos los pulsadores.
    for (int i = 0; i < NUM_PULSADORES; i++) {
        botones[i].tick();
    }
#endif

    static unsigned long ultimoAviso = 0;
    if (!mqtt.isConnected() && millis() - ultimoAviso > 5000) {
        ultimoAviso = millis();
        Serial.println(F("[mqtt] sigue sin conectar, reintentando..."));
    }
}
