// ===========================================================
// MEGA_PULSADORES_LOW_RAM
// Variante de mega_pulsadores que usa AceButton en vez de OneButton —
// mismo comportamiento MQTT/HA para corta, doble, larga y fin de larga,
// pero con mucha menos RAM por pulsador. A cambio, NO PUEDE hacer
// triple/cuádruple/quíntuple clic — AceButton no tiene ningún
// mecanismo para contar 3+ pulsaciones seguidas, no es una opción
// desactivable como en mega_pulsadores/mega_pulsadores.ino.
//
// ⚠️ RAM — probado en placa real (2026-09-05, firmware 1.8.0, con
// botón virtual incluido): 24 pulsadores arrancan y funcionan estables
// (623 bytes libres), 25 ya arranca pero el MQTT se conecta/desconecta
// solo (361 bytes libres, insuficiente para operar con estabilidad).
// Coste real ≈ 263 bytes/pulsador (medido, no solo el tamaño de la
// clase AceButton en sí — incluye también los HADeviceTrigger, el
// HAButton virtual y los buffers de texto de cada pulsador). Detalle
// completo en "RAM / límite de pulsadores" en todo.md. Con OneButton
// (mega_pulsadores/), el límite equivalente está entre 12 y 16 — casi
// el doble de pulsadores caben aquí.
//
// ⚠️ Elige esta unidad en vez de mega_pulsadores/ SOLO si:
//   - Necesitas más pulsadores por unidad de los que caben con
//     OneButton (ver "RAM / límite de pulsadores" en todo.md), Y
//   - Ninguno de esos pulsadores necesita triple/cuádruple/quíntuple
//     clic (revisa qué blueprints vas a instanciar — ver
//     mega_pulsadores/to_review.md para el detalle completo).
// Si tienes dudas, usa mega_pulsadores/ (con OneButton) por defecto —
// esta carpeta es la opción de RAM ajustada, no el firmware normal.
//
// ⚠️ CARPETA DUPLICADA: board_config_a.h, board_config_b.h y
// config.h.example de aquí son COPIAS independientes de las de
// mega_pulsadores/, no las mismas (Arduino IDE exige que los .h vivan
// en la misma carpeta que el .ino). Si cambias pines, MAC, IP o nombre
// en una carpeta, coméntalo y valora si el mismo cambio aplica también
// en la otra — no se sincronizan solas.
//
// No controla ningún relé. Solo ENVÍA información.
//
// Además, cada pulsador tiene un HAButton virtual (entidad real y
// pulsable en la UI de HA) que simula un clic corto en ese pin al
// pulsarlo desde HA — mismo mecanismo que en mega_pulsadores/, pero
// aquí implementado sobreescribiendo ButtonConfig::readButton() en vez
// de OneButton::tick(bool) (AceButton no tiene ese método). No simula
// pulsación larga.
//
// Mismo patrón que mega_pulsadores/: identidad de la placa (pines,
// MAC, IP, nombre) en TIEMPO DE COMPILACIÓN con PLACA_A/PLACA_B — no
// hay jumper físico.
//
// Librerías necesarias (Arduino Library Manager):
//   - ArduinoHA        https://github.com/dawidchyrzynski/arduino-home-assistant
//   - AceButton        https://github.com/bxparks/AceButton
//   - Ethernet (incluida en el IDE si usas shield W5100/W5500)
//
// Para medir RAM real en placa, ver instructions.md en mega_pulsadores/
// (mismo procedimiento, aplica igual aquí).
// ===========================================================

#include <Ethernet.h>
#include <ArduinoHA.h>
#include <AceButton.h>
using namespace ace_button;

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
// A diferencia de mega_pulsadores/ (OneButton), aquí NO hay
// HABILITAR_TRIPLE/HABILITAR_CUADRUPLE/HABILITAR_QUINTUPLE — AceButton
// no soporta esos eventos en absoluto, no es una opción que se pueda
// activar. Solo quedan los 4 que sí cubre: corta, doble, larga y fin
// de larga. Cada uno se puede desactivar igual que en mega_pulsadores/
// si no lo necesitas (ahorra aún más RAM), pero no se puede añadir
// ninguno nuevo.
// ⚠️ Antes de cambiar cualquiera de estas líneas, revisa qué
// blueprints tienes instanciados en HA para los pulsadores de esta
// unidad — un blueprint que espera un trigger que ya no existe
// simplemente deja de dispararse, sin error visible:
//   - persiana_pulsador.yaml usa solo larga/largaFin — compatible.
//   - luz_pulsador.yaml usa corta/doble/larga (el pulsación triple
//     original se remapeó a doble en la propia definición del
//     blueprint, ver su CHANGELOG.md — ya funciona en ambos firmwares
//     sin tocar nada) — compatible.
//   - persiana_pulsador_completo.yaml usa las 5 pulsaciones —
//     INCOMPATIBLE con esta unidad, no lo instancies aquí.
// Por defecto: los 4 activos (mismo comportamiento por defecto que
// mega_pulsadores/ para estos cuatro eventos).
// ===========================================================
#define HABILITAR_CORTA
#define HABILITAR_DOBLE
#define HABILITAR_LARGA
#define HABILITAR_LARGA_FIN

// ===========================================================
// BOTON VIRTUAL POR PULSADOR (HAButton) — ON/OFF
// Coméntalo para NO crear los HAButton. Es la palanca de RAM más
// grande que tiene este firmware: cada HAButton es una entidad MQTT
// completa (con su unique_id, su topic de comando y su buffer de
// nombre), así que desactivarlo libera bastante más por pulsador que
// quitar cualquiera de los 4 triggers de arriba — la vía a mirar
// primero si necesitas pasar del límite de pulsadores (ver "RAM /
// límite de pulsadores" en todo.md).
//
// ⚠️ Qué se pierde EXACTAMENTE al desactivarlo: los botones "Press"
// que salen en Controls dentro del dispositivo en HA, que sirven para
// SIMULAR una pulsación corta desde la UI/automatizaciones (pulsas en
// HA y el firmware inyecta un clic en ese pin). NO afecta para nada a
// los pulsadores físicos de pared ni a los HADeviceTrigger: corta,
// doble, larga y fin de larga siguen funcionando igual, que es lo que
// usan los blueprints y las automatizaciones por device trigger.
//
// Al desactivarlo también desaparece todo el andamiaje de simulación
// (la subclase ButtonConfigConSimulacion, el array de temporizadores y
// su limpieza en loop()), así que los pulsadores pasan a leerse con el
// ButtonConfig normal de AceButton.
#define HABILITAR_BOTON_VIRTUAL

// ===========================================================
// DEBUG POR SERIAL — ON/OFF
// Coméntalo para compilar sin nada de instrumentación. Quita:
//   - freeMemory() y el "[debug] RAM libre"
//   - el retorno de setBufferSize() y su "[debug] setBufferSize(...)"
//   - el contador de entidades y los "[debug] entidades/NUM_PULSADORES"
//   - el "[publicado]/[FALLO MQTT]" de cada pulsación
// Lo que se gana no es solo la RAM del contador y los bool: cada
// Serial.print BLOQUEA el loop mientras vacía el buffer de 9600 baudios,
// y AceButton necesita check() cada <5ms para que el debounce y la
// detección de multiclic funcionen bien (documentado en AceButton.h).
// Con 28 pulsadores y una línea impresa por pulsación, esa pausa se
// nota. Déjalo activado mientras diagnostiques; apágalo en producción.
#define HABILITAR_DEBUG

EthernetClient client;
HADevice device(mac, sizeof(mac));

// NUM_PULSADORES se calcula a partir de PINES_BOTONES, definido en
// board_config_a.h o board_config_b.h según PLACA_A/PLACA_B (ver más arriba).
//
// RAM: el objeto AceButton en sí cuesta ~18-26 bytes (frente a
// ~90-100 de OneButton, ver mega_pulsadores/to_review.md), pero el
// coste real por pulsador (con HADeviceTrigger + HAButton virtual +
// buffers) es mayor — medido en placa real (2026-09-05, firmware
// 1.8.0): ~263 bytes/pulsador, límite práctico 24 pulsadores estable
// (623 bytes libres), 25 ya arranca pero deja el MQTT inestable. Ver
// "RAM / límite de pulsadores" en todo.md para la tabla completa. Si
// cambias la config de triggers activos (HABILITAR_* más abajo) o el
// nº de pulsadores, vuelve a medir con freeMemory() en vez de asumir
// que el límite de 24 se mantiene igual — ver
// mega_pulsadores/instructions.md para el procedimiento.
const int NUM_PULSADORES = sizeof(PINES_BOTONES) / sizeof(PINES_BOTONES[0]);

// Cuenta cuántos de los 4 triggers están activos (para dimensionar
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
#if defined(HABILITAR_LARGA)
    #define _TRIGGERS_ACTIVOS_3 (_TRIGGERS_ACTIVOS_2 + 1)
#else
    #define _TRIGGERS_ACTIVOS_3 _TRIGGERS_ACTIVOS_2
#endif
#if defined(HABILITAR_LARGA_FIN)
    #define _TRIGGERS_ACTIVOS_4 (_TRIGGERS_ACTIVOS_3 + 1)
#else
    #define _TRIGGERS_ACTIVOS_4 _TRIGGERS_ACTIVOS_3
#endif
#define NUM_TRIGGERS_POR_PULSADOR _TRIGGERS_ACTIVOS_4

// Entidades MQTT por pulsador: sus triggers activos, + 1 por el
// HAButton virtual si está activado (ver HABILITAR_BOTON_VIRTUAL), + margen.
#ifdef HABILITAR_BOTON_VIRTUAL
    #define _ENTIDADES_POR_PULSADOR (NUM_TRIGGERS_POR_PULSADOR + 1)
#else
    #define _ENTIDADES_POR_PULSADOR NUM_TRIGGERS_POR_PULSADOR
#endif
HAMqtt mqtt(client, device, NUM_PULSADORES * _ENTIDADES_POR_PULSADOR + 2);

// Array estático de AceButton (constructor por defecto + init() para
// configurar pin/id después, mismo patrón que OneButton en
// mega_pulsadores/mega_pulsadores.ino) — confirmado en el ejemplo
// oficial ArrayButtons.ino del propio repo de AceButton.
AceButton botones[NUM_PULSADORES];

// ===========================================================
// BOTÓN VIRTUAL POR PULSADOR (simular pulsaciones desde HA)
// Un HAButton por pulsador — a diferencia de HADeviceTrigger, este SÍ
// es una entidad visible y pulsable en la UI de HA. AceButton no tiene
// un tick(bool) como OneButton, pero sí un punto de inyección
// equivalente y oficial: ButtonConfig::readButton(pin) es virtual y
// está documentado como "Override to use something other than
// digitalRead()" — así que se sobreescribe con una subclase que
// devuelve un nivel simulado por pin cuando hay una simulación activa,
// y digitalRead() normal en cualquier otro caso. check() (llamado en
// loop()) usa esa función internamente sin saber que es distinta —
// toda la lógica de debounce/multiclic de AceButton sigue intacta.
//
// Limitación deliberada: el pulso simulado es corto y fijo
// (SIMULACION_PULSO_MS), pensado para corta/doble clic. No sirve para
// simular una pulsación LARGA (necesita mantener el nivel activo un
// tiempo variable) — eso queda fuera de esta primera versión.
#ifdef HABILITAR_BOTON_VIRTUAL
#define SIMULACION_PULSO_MS 90

// Por pulsador: 0 = sin simulación en curso. Si no es 0, es el
// millis() en el que hay que devolver HIGH (soltado) — ver
// ButtonConfigConSimulacion::readButton() y loop().
unsigned long simulacionSoltarEn[NUM_PULSADORES];

class ButtonConfigConSimulacion : public ButtonConfig {
public:
    int readButton(uint8_t pin) override {
        unsigned long ahora = millis();
        for (int i = 0; i < NUM_PULSADORES; i++) {
            // La comprobación de caducidad vive AQUÍ, no en loop() en un
            // paso aparte antes de check() — así el nivel que devuelve
            // esta función es siempre consistente en la MISMA llamada
            // que AceButton usa para decidir press/release, sin ninguna
            // ventana de carrera entre "caducar" y "leer". Antes, loop()
            // caducaba simulacionSoltarEn[i] ANTES de llamar a check(),
            // así que la llamada a check() justo en el instante de
            // caducidad caía en el pin real (posiblemente flotante o con
            // rebote) en vez de en un HIGH limpio — eso generaba
            // transiciones Released→Pressed→Released espurias que
            // corrompían el estado interno de AceButton (kFlagPressed se
            // quedaba mal, provocando triple "larga (inicio)" y demás
            // basura en el log, incluso para el pulsador físico real del
            // mismo pin, que comparte instancia).
            if (PINES_BOTONES[i] == pin && simulacionSoltarEn[i] != 0
                    && ahora < simulacionSoltarEn[i]) {
                return LOW; // "pulsado" simulado, sin tocar el pin real
            }
        }
        return digitalRead(pin);
    }
};
ButtonConfigConSimulacion configConSimulacion;

HAButton* botonVirtual[NUM_PULSADORES];
#endif

// Orden deliberado: corta -> doble -> larga/fin de larga aparte al
// final, como caso especial (mismo criterio que mega_pulsadores/).
#ifdef HABILITAR_CORTA
HADeviceTrigger* corta[NUM_PULSADORES];
#endif
#ifdef HABILITAR_DOBLE
HADeviceTrigger* doble[NUM_PULSADORES];
#endif
#ifdef HABILITAR_LARGA
HADeviceTrigger* larga[NUM_PULSADORES];
#endif
// Se dispara al SOLTAR una pulsación larga. Imprescindible para
// automatizaciones "mantener pulsado para mover / soltar para parar"
// (p. ej. persianas): "larga" = empezar a subir, "larga_fin" = parar.
// AceButton dispara esto automáticamente en cuanto kFeatureLongPress
// está activo (confirmado en su código fuente) — no hace falta ningún
// feature flag aparte para el release.
#ifdef HABILITAR_LARGA_FIN
HADeviceTrigger* largaFin[NUM_PULSADORES];
#endif

// Buffers de texto para los IDs. Deben ser globales (viven todo el
// programa) porque HADeviceTrigger se queda con el puntero al texto,
// no con una copia. Formato "pNN" — mismo formato que mega_pulsadores/
// (ver ese CHANGELOG.md, entrada 1.7.1, para el porqué).
char idBoton[NUM_PULSADORES][4];

// unique_id del HAButton virtual de cada pulsador — "v" + idBoton[i]
// (ej. "vp14"), igual que en mega_pulsadores/mega_pulsadores.ino.
// Solo existe si el botón virtual está activo: sin él son 5 bytes por
// pulsador que no hay por qué reservar.
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
// Misma técnica que en mega_pulsadores/mega_pulsadores.ino — ver
// mega_pulsadores/instructions.md para el procedimiento completo.
#ifdef HABILITAR_DEBUG
extern char* __brkval;
extern char __bss_end;
int freeMemory() {
    char top;
    return &top - (__brkval ? __brkval : &__bss_end);
}
#endif

// AceButton usa UN solo handler global compartido por todos los
// botones (vía ButtonConfig::setEventHandler), a diferencia de
// OneButton que tenía una función distinta por tipo de evento
// (onClick, onDoubleClick, etc.) — API real confirmada en el repo
// oficial de AceButton (AceButton.h, ButtonConfig.h y el ejemplo
// ArrayButtons.ino). button->getId() identifica qué pulsador disparó
// el evento (equivalente al void* param de OneButton, sin necesitar
// lambdas con captura ni punteros void*).
void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState) {
    uint8_t idx = button->getId();
    switch (eventType) {
#ifdef HABILITAR_CORTA
        case AceButton::kEventClicked: {
            // ⚠️ TEMPORAL — DEBUG DIAGNOSTICO (2026-09-17): trigger()
            // devuelve bool y hasta ahora nadie mira ese valor, así que
            // el Serial.print de abajo se imprime IGUAL aunque la
            // publicación MQTT haya fallado — de ahí el síntoma "lo veo
            // en el monitor serie pero HA no reacciona". Quitar el
            // "-> publicado/FALLO" cuando esté resuelto.
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
            break;
        }
#endif
#ifdef HABILITAR_DOBLE
        case AceButton::kEventDoubleClicked: {
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
            break;
        }
#endif
#ifdef HABILITAR_LARGA
        case AceButton::kEventLongPressed: {
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
            break;
        }
#endif
#ifdef HABILITAR_LARGA_FIN
        case AceButton::kEventLongReleased: {
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
            break;
        }
#endif
    }
}

#ifdef HABILITAR_BOTON_VIRTUAL
// Al pulsar el HAButton virtual de un pulsador en HA: marca cuándo
// debe volver a HIGH (soltado). Mientras esa marca esté activa,
// ButtonConfigConSimulacion::readButton() devuelve LOW para ese pin en
// vez de leer el pin real — así check() en loop() lo procesa como una
// pulsación real más, sin tocar la lógica de AceButton para nada.
void onBotonVirtual(HAButton* sender) {
    for (int i = 0; i < NUM_PULSADORES; i++) {
        if (botonVirtual[i] == sender) {
            simulacionSoltarEn[i] = millis() + SIMULACION_PULSO_MS;
            Serial.print(F("[boton] "));
            Serial.print(idBoton[i]);
            Serial.println(F(" -> pulsación simulada desde HA"));
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
    device.setSoftwareVersion("1.8.7");

    // ⚠️ ORDEN CRITICO: setBufferSize() va AQUI, antes de crear ni un
    // solo HADeviceTrigger/HAButton — no después del bucle, donde
    // estaba hasta la 1.8.4.
    //
    // Contexto (bug real, medido en placa 2026-09-17): el buffer por
    // defecto de PubSubClient (256 bytes) no alcanza para el payload
    // de discovery de un device_automation, así que los triggers no
    // llegaban nunca a HA (ver CHANGELOG 1.8.3). Pero setBufferSize()
    // hace un realloc, que necesita un bloque CONTIGUO libre, y
    // devuelve false EN SILENCIO si no lo encuentra: PubSubClient se
    // queda con los 256 de siempre y el bug vuelve, invisible.
    //
    // Llamándolo después del bucle, con 24 pulsadores (120 objetos ya
    // creados con new) fallaba tanto con 1024 como con 512 — el heap
    // queda troceado en asignaciones pequeñas y ya no hay hueco
    // contiguo, aunque el total libre parezca suficiente. Aquí arriba
    // el heap está intacto, así que el bloque se consigue entero.
    //
    // 512 y no 1024: los payloads de discovery reales, capturados del
    // topic homeassistant/device_automation/# en placa, rondan los
    // ~250 bytes, así que 512 va sobrado y pide la mitad de memoria
    // contigua — importante en un Mega de 8 KB al subir de pulsadores.
#ifdef HABILITAR_DEBUG
    bool bufOk = mqtt.setBufferSize(512);
    Serial.print(F("[debug] setBufferSize(512) -> "));
    Serial.println(bufOk ? F("OK") : F("FALLO (sigue en 256!)"));
#else
    mqtt.setBufferSize(512);
#endif

    // --- config compartida por todos los pulsadores de esta unidad ---
    // configConSimulacion en vez de getSystemButtonConfig(): añade el
    // punto de inyección para el botón virtual (ver su definición más
    // arriba) sin cambiar nada más del comportamiento normal.
#ifdef HABILITAR_BOTON_VIRTUAL
    ButtonConfig* cfg = &configConSimulacion;
#else
    // Sin botón virtual no hay nada que simular, así que se usa el
    // ButtonConfig normal de AceButton (una sola instancia compartida).
    ButtonConfig* cfg = ButtonConfig::getSystemButtonConfig();
#endif
#ifdef HABILITAR_CORTA
    cfg->setFeature(ButtonConfig::kFeatureClick);
#endif
#ifdef HABILITAR_DOBLE
    // ⚠️ Sin kFeatureSuppressClickBeforeDoubleClick, AceButton despacha
    // el "corta" del primer toque AL INSTANTE (ver AceButton::checkClicked(),
    // que llama a handleEvent(kEventClicked) enseguida salvo que este
    // flag esté activo) y SOLO DESPUÉS, si llega un segundo toque a
    // tiempo, añade el "doble" — nunca sustituye al primer evento. Bug
    // real confirmado en placa (2026-09-05): cualquier doble clic salía
    // siempre como "corta" seguido de "doble", por rápido que se hiciera.
    // Con este flag, el primer clic se pospone (kFlagClickPostponed) y
    // si llega el segundo a tiempo se descarta del todo, dejando pasar
    // solo el doble — ver AceButton::checkDoubleClicked().
    cfg->setFeature(ButtonConfig::kFeatureDoubleClick);
    cfg->setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
#endif
#ifdef HABILITAR_LARGA
    cfg->setFeature(ButtonConfig::kFeatureLongPress);
#endif
#ifdef HABILITAR_LARGA_FIN
    // ⚠️ kEventLongReleased NO se activa solo con kFeatureLongPress —
    // confirmado leyendo el código fuente de AceButton
    // (AceButton::checkReleased()): sin kFeatureSuppressAfterLongPress,
    // el release tras una pulsación larga siempre se despacha como
    // kEventReleased genérico (que este firmware ni siquiera gestiona
    // en el switch de handleEvent()), nunca como kEventLongReleased.
    // Bug real confirmado en placa (2026-09-05): "larga (inicio)" salía
    // siempre correctamente, "larga (fin)" nunca — ni una sola vez,
    // por más que se soltara el pulsador tras la pulsación larga. El
    // comentario que había aquí antes ("un solo flag activa los dos
    // eventos") era incorrecto, sin haberlo verificado bien contra el
    // código fuente real de la librería.
    cfg->setFeature(ButtonConfig::kFeatureLongPress);
    cfg->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
#endif
    cfg->setEventHandler(handleEvent);

    // ⚠️ setClickDelay subido de 200ms (por defecto) a 400ms — bug real
    // confirmado en placa (2026-09-05): con pulsaciones de dedo
    // normales (~220-290ms, medidas con digitalRead directo, sin
    // rebote), AceButton::checkClicked() descarta el evento EN
    // SILENCIO en cuanto elapsedTime >= getClickDelay() — con el
    // valor por defecto de 200ms, cualquier pulsación de dedo normal
    // ya caía por encima del umbral y nunca disparaba ni corta ni
    // doble. 400ms da margen real de sobra para un toque de dedo
    // normal sin acercarse al umbral de pulsación larga (1000ms).
    cfg->setClickDelay(400);
    // Ventana entre soltar el 1er toque y presionar el 2º para que
    // cuente como doble clic (checkDoubleClicked(): now - mLastClickTime).
    // 450ms, explícito en vez de dejar el default (400ms) implícito.
    cfg->setDoubleClickDelay(450);
    // Tiempo sujetando el botón para que cuente como pulsación larga.
    // 800ms (bajado del default de AceButton, 1000ms) a gusto del
    // usuario tras probar en placa real.
    cfg->setLongPressDelay(800);
    // Filtra el rebote eléctrico del contacto mecánico del pulsador —
    // no confirmado como problema real en placa (el test con
    // digitalRead directo no mostró rebote), se deja en el default.
    // cfg->setDebounceDelay(20);

    // ⚠️ TEMPORAL — DEBUG DIAGNOSTICO (2026-09-17): cuenta cada
    // entidad MQTT que se crea, para comparar al final de setup()
    // con el hueco reservado en el constructor de HAMqtt.
#ifdef HABILITAR_DEBUG
    int entidadesCreadas = 0;
#endif

    // --- creamos cada pulsador (ver HABILITAR_* arriba) ---
    for (int i = 0; i < NUM_PULSADORES; i++) {
        snprintf(idBoton[i], sizeof(idBoton[i]), "p%d", PINES_BOTONES[i]);
#ifdef HABILITAR_BOTON_VIRTUAL
        snprintf(idBotonVirtual[i], sizeof(idBotonVirtual[i]), "v%s", idBoton[i]);
#endif

        pinMode(PINES_BOTONES[i], INPUT_PULLUP);
        // HIGH = nivel en reposo (no pulsado) con INPUT_PULLUP y botón
        // a GND — id = i, para identificar el pulsador en handleEvent().
        // &configConSimulacion explícito: sin esto, AceButton usaría
        // ButtonConfig::getSystemButtonConfig() por defecto (una
        // instancia DISTINTA a la nuestra) y el botón virtual no
        // tendría ningún efecto sobre la lectura real de este pulsador.
        botones[i].init(cfg, PINES_BOTONES[i], HIGH, i);

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
        // diferencia de los HADeviceTrigger de arriba).
        botonVirtual[i] = new HAButton(idBotonVirtual[i]);
        botonVirtual[i]->setName(idBoton[i]);
        botonVirtual[i]->onCommand(onBotonVirtual);
#ifdef HABILITAR_DEBUG
        entidadesCreadas++;
#endif
#endif
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

    // check() hay que llamarlo a menudo (cada <5ms lo ideal) para que
    // el debounce por defecto de AceButton (20ms) funcione bien —
    // documentado explícitamente en AceButton.h: check() debe llamarse
    // al menos 2-3 veces durante la ventana de debounce, como mínimo
    // cada 5ms, o la detección de clics/pulsación larga puede fallar.
    // ⚠️ Si mqtt.loop() llega a bloquear más de eso (reconexión TCP,
    // DNS...), la cadencia de check() se resiente y pueden aparecer
    // los mismos síntomas corruptos que el bug de la carrera de más
    // arriba — no confirmado como problema real todavía, pero vigilar
    // si vuelve a pasar algo raro con MQTT desconectado/reconectando.
    for (int i = 0; i < NUM_PULSADORES; i++) {
        botones[i].check();
    }

#ifdef HABILITAR_BOTON_VIRTUAL
    // La caducidad de la simulación ya la decide readButton() en cada
    // llamada (ver ButtonConfigConSimulacion más arriba) — esto de aquí
    // es solo limpieza de la bandera una vez que ya no hace falta,
    // puede pasar en cualquier momento después de check() sin afectar
    // a la detección.
    unsigned long ahora = millis();
    for (int i = 0; i < NUM_PULSADORES; i++) {
        if (simulacionSoltarEn[i] != 0 && ahora >= simulacionSoltarEn[i]) {
            simulacionSoltarEn[i] = 0;
        }
    }
#endif

    static unsigned long ultimoAviso = 0;
    if (!mqtt.isConnected() && millis() - ultimoAviso > 5000) {
        ultimoAviso = millis();
        Serial.println(F("[mqtt] sigue sin conectar, reintentando..."));
    }
}
