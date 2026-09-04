// ===========================================================
// MEGA_PULSADORES_LOW_RAM
// Variante de mega_pulsadores que usa AceButton en vez de OneButton —
// mismo comportamiento MQTT/HA para corta, doble, larga y fin de larga,
// pero con mucha menos RAM por pulsador (~18-26 bytes/instancia frente
// a ~90-100 de OneButton, ver mega_pulsadores/to_review.md). A cambio,
// NO PUEDE hacer triple/cuádruple/quíntuple clic — AceButton no tiene
// ningún mecanismo para contar 3+ pulsaciones seguidas, no es una
// opción desactivable como en mega_pulsadores/mega_pulsadores.ino.
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
//   - luz_pulsador.yaml usa corta/triple/larga — el TRIPLE no existe
//     aquí, remapea ese blueprint a doble clic si lo usas en esta
//     unidad (ver mega_pulsadores/to_review.md).
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
// RAM: cada pulsador con AceButton cuesta ~18-26 bytes de SRAM (frente
// a ~90-100 con OneButton, ver mega_pulsadores/to_review.md) — no hay
// todavía medición real en placa para esta variante, solo la
// estimación del research de librerías. Antes de dar por buena una
// cifra de "cuántos caben", medir con freeMemory() (ver
// mega_pulsadores/instructions.md, mismo procedimiento aplicado aquí).
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

// + margen
HAMqtt mqtt(client, device, NUM_PULSADORES * NUM_TRIGGERS_POR_PULSADOR + 2);

// Array estático de AceButton (constructor por defecto + init() para
// configurar pin/id después, mismo patrón que OneButton en
// mega_pulsadores/mega_pulsadores.ino) — confirmado en el ejemplo
// oficial ArrayButtons.ino del propio repo de AceButton.
AceButton botones[NUM_PULSADORES];

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
    device.setSoftwareVersion("1.0.0-low-ram");

    // --- config compartida por todos los pulsadores de esta unidad ---
    ButtonConfig* cfg = ButtonConfig::getSystemButtonConfig();
#ifdef HABILITAR_CORTA
    cfg->setFeature(ButtonConfig::kFeatureClick);
#endif
#ifdef HABILITAR_DOBLE
    cfg->setFeature(ButtonConfig::kFeatureDoubleClick);
#endif
#if defined(HABILITAR_LARGA) || defined(HABILITAR_LARGA_FIN)
    // Un solo feature flag activa tanto el evento de inicio
    // (kEventLongPressed) como el de fin (kEventLongReleased) —
    // AceButton no los separa en dos flags distintos.
    cfg->setFeature(ButtonConfig::kFeatureLongPress);
#endif
    cfg->setEventHandler(handleEvent);
    // Ajustes opcionales de temporización (descomenta y ajusta si los
    // pulsadores van demasiado rápido/lentos para tu gusto):
    // cfg->setDebounceDelay(20);
    // cfg->setClickDelay(200);
    // cfg->setDoubleClickDelay(400);   // ventana para detectar el doble clic
    // cfg->setLongPressDelay(1000);    // tiempo para considerar "larga"

    // --- creamos cada pulsador (ver HABILITAR_* arriba) ---
    for (int i = 0; i < NUM_PULSADORES; i++) {
        snprintf(idBoton[i], sizeof(idBoton[i]), "p%d", PINES_BOTONES[i]);

        pinMode(PINES_BOTONES[i], INPUT_PULLUP);
        // HIGH = nivel en reposo (no pulsado) con INPUT_PULLUP y botón
        // a GND — id = i, para identificar el pulsador en handleEvent().
        botones[i].init(PINES_BOTONES[i], HIGH, i);

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
    // confirmado en el ejemplo oficial de la librería. mqtt.loop() no
    // debería bloquear lo suficiente como para ser un problema aquí.
    for (int i = 0; i < NUM_PULSADORES; i++) {
        botones[i].check();
    }

    static unsigned long ultimoAviso = 0;
    if (!mqtt.isConnected() && millis() - ultimoAviso > 5000) {
        ultimoAviso = millis();
        Serial.println(F("[mqtt] sigue sin conectar, reintentando..."));
    }
}
