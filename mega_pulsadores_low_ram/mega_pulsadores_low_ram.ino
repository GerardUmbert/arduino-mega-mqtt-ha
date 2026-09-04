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

// + 1 por el HAButton virtual de cada pulsador, + margen
HAMqtt mqtt(client, device, NUM_PULSADORES * (NUM_TRIGGERS_POR_PULSADOR + 1) + 2);

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
char idBotonVirtual[NUM_PULSADORES][5];

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
extern char* __brkval;
extern char __bss_end;
int freeMemory() {
    char top;
    return &top - (__brkval ? __brkval : &__bss_end);
}

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
        case AceButton::kEventClicked:
            corta[idx]->trigger();
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.println(F(" -> corta"));
            break;
#endif
#ifdef HABILITAR_DOBLE
        case AceButton::kEventDoubleClicked:
            doble[idx]->trigger();
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.println(F(" -> doble"));
            break;
#endif
#ifdef HABILITAR_LARGA
        case AceButton::kEventLongPressed:
            larga[idx]->trigger();
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.println(F(" -> larga (inicio)"));
            break;
#endif
#ifdef HABILITAR_LARGA_FIN
        case AceButton::kEventLongReleased:
            largaFin[idx]->trigger();
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.println(F(" -> larga (fin)"));
            break;
#endif
    }
}

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
    device.setSoftwareVersion("1.8.1");

    // --- config compartida por todos los pulsadores de esta unidad ---
    // configConSimulacion en vez de getSystemButtonConfig(): añade el
    // punto de inyección para el botón virtual (ver su definición más
    // arriba) sin cambiar nada más del comportamiento normal.
    ButtonConfig* cfg = &configConSimulacion;
#ifdef HABILITAR_CORTA
    cfg->setFeature(ButtonConfig::kFeatureClick);
#endif
#ifdef HABILITAR_DOBLE
    cfg->setFeature(ButtonConfig::kFeatureDoubleClick);
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
    // Resto de ajustes opcionales de temporización (descomenta y
    // ajusta si los pulsadores van demasiado rápido/lentos):
    // cfg->setDebounceDelay(20);
    // cfg->setDoubleClickDelay(400);   // ventana para detectar el doble clic
    // cfg->setLongPressDelay(1000);    // tiempo para considerar "larga"

    // --- creamos cada pulsador (ver HABILITAR_* arriba) ---
    for (int i = 0; i < NUM_PULSADORES; i++) {
        snprintf(idBoton[i], sizeof(idBoton[i]), "p%d", PINES_BOTONES[i]);
        snprintf(idBotonVirtual[i], sizeof(idBotonVirtual[i]), "v%s", idBoton[i]);

        pinMode(PINES_BOTONES[i], INPUT_PULLUP);
        // HIGH = nivel en reposo (no pulsado) con INPUT_PULLUP y botón
        // a GND — id = i, para identificar el pulsador en handleEvent().
        // &configConSimulacion explícito: sin esto, AceButton usaría
        // ButtonConfig::getSystemButtonConfig() por defecto (una
        // instancia DISTINTA a la nuestra) y el botón virtual no
        // tendría ningún efecto sobre la lectura real de este pulsador.
        botones[i].init(&configConSimulacion, PINES_BOTONES[i], HIGH, i);

#ifdef HABILITAR_CORTA
        corta[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonShortPressType,     idBoton[i]);
#endif
#ifdef HABILITAR_DOBLE
        doble[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonDoublePressType,    idBoton[i]);
#endif
#ifdef HABILITAR_LARGA
        larga[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonLongPressType,      idBoton[i]);
#endif
#ifdef HABILITAR_LARGA_FIN
        largaFin[i]  = new HADeviceTrigger(HADeviceTrigger::ButtonLongReleaseType,    idBoton[i]);
#endif

        // Botón virtual: entidad real y pulsable en la UI de HA (a
        // diferencia de los HADeviceTrigger de arriba).
        botonVirtual[i] = new HAButton(idBotonVirtual[i]);
        botonVirtual[i]->setName(idBoton[i]);
        botonVirtual[i]->onCommand(onBotonVirtual);
    }

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

    // ⚠️ TEMPORAL — DEBUG DE RAM: quitar junto con freeMemory() de más
    // arriba cuando ya no haga falta medir. Se imprime al final de
    // setup() a propósito: es el punto de mínima RAM libre del
    // programa (todo ya creado — pulsadores, triggers, Ethernet, MQTT).
    Serial.print(F("[debug] RAM libre: "));
    Serial.print(freeMemory());
    Serial.println(F(" bytes"));
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

    static unsigned long ultimoAviso = 0;
    if (!mqtt.isConnected() && millis() - ultimoAviso > 5000) {
        ultimoAviso = millis();
        Serial.println(F("[mqtt] sigue sin conectar, reintentando..."));
    }
}
