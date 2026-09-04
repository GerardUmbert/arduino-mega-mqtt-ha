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
// Mismo firmware para las 2 unidades físicas (A y B): la identidad
// (pines, MAC, IP, nombre) se decide en TIEMPO DE COMPILACIÓN con
// PLACA_A/PLACA_B (ver más abajo) — no hay jumper físico.
//
// Librerías necesarias (Arduino Library Manager):
//   - ArduinoHA        https://github.com/dawidchyrzynski/arduino-home-assistant
//   - OneButton        https://github.com/mathertel/OneButton
//   - Ethernet (incluida en el IDE si usas shield W5100/W5500)
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

// + margen
HAMqtt mqtt(client, device, NUM_PULSADORES * NUM_TRIGGERS_POR_PULSADOR + 2);

OneButton* botones[NUM_PULSADORES];

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
// no con una copia.
char idBoton[NUM_PULSADORES][10];

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
extern char* __brkval;
extern char __bss_end;
int freeMemory() {
    char top;
    return &top - (__brkval ? __brkval : &__bss_end);
}

#ifdef HABILITAR_CORTA
void onClick(void* param) {
    int idx = reinterpret_cast<int>(param);
    corta[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.println(F(" -> corta"));
}
#endif

#ifdef HABILITAR_DOBLE
void onDoubleClick(void* param) {
    int idx = reinterpret_cast<int>(param);
    doble[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.println(F(" -> doble"));
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
    int clics = botones[idx]->getNumberClicks();
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
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.print(F(" -> multiclick x"));
    Serial.println(clics);
}
#endif

#ifdef HABILITAR_LARGA
void onLongPressStart(void* param) {
    int idx = reinterpret_cast<int>(param);
    larga[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.println(F(" -> larga (inicio)"));
}
#endif

#ifdef HABILITAR_LARGA_FIN
void onLongPressStop(void* param) {
    int idx = reinterpret_cast<int>(param);
    largaFin[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.println(F(" -> larga (fin)"));
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
    device.setSoftwareVersion("1.7.0");

    // --- creamos cada pulsador y sus triggers activos (ver HABILITAR_* arriba) ---
    for (int i = 0; i < NUM_PULSADORES; i++) {
        snprintf(idBoton[i], sizeof(idBoton[i]), "boton_%d", PINES_BOTONES[i]);

        botones[i] = new OneButton(PINES_BOTONES[i], true); // true = INPUT_PULLUP, activo en LOW

#ifdef HABILITAR_CORTA
        corta[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonShortPressType,     idBoton[i]);
#endif
#ifdef HABILITAR_DOBLE
        doble[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonDoublePressType,    idBoton[i]);
#endif
#ifdef HABILITAR_TRIPLE
        triple[i]    = new HADeviceTrigger(HADeviceTrigger::ButtonTriplePressType,    idBoton[i]);
#endif
#ifdef HABILITAR_CUADRUPLE
        cuadruple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuadruplePressType, idBoton[i]);
#endif
#ifdef HABILITAR_QUINTUPLE
        quintuple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuintuplePressType, idBoton[i]);
#endif
#ifdef HABILITAR_LARGA
        larga[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonLongPressType,      idBoton[i]);
#endif
#ifdef HABILITAR_LARGA_FIN
        largaFin[i]  = new HADeviceTrigger(HADeviceTrigger::ButtonLongReleaseType,    idBoton[i]);
#endif

        // void* que se le pasa de vuelta a cada callback (ver onClick() etc.
        // más arriba) para que sepa de qué pulsador se trata — no podemos
        // capturar "i"/"idx" en una lambda porque OneButton exige un
        // function pointer puro o su variante con parámetro void*.
        void* idxParam = reinterpret_cast<void*>(i);

#ifdef HABILITAR_CORTA
        botones[i]->attachClick(onClick, idxParam);
#endif
#ifdef HABILITAR_DOBLE
        botones[i]->attachDoubleClick(onDoubleClick, idxParam);
#endif
#if defined(HABILITAR_TRIPLE) || defined(HABILITAR_CUADRUPLE) || defined(HABILITAR_QUINTUPLE)
        botones[i]->attachMultiClick(onMultiClick, idxParam);
#endif
#ifdef HABILITAR_LARGA
        botones[i]->attachLongPressStart(onLongPressStart, idxParam);
#endif
#ifdef HABILITAR_LARGA_FIN
        botones[i]->attachLongPressStop(onLongPressStop, idxParam);
#endif

        // Ajustes opcionales de temporización (descomenta y ajusta si
        // los pulsadores van demasiado rápido/lentos para tu gusto):
        // botones[i]->setDebounceMs(50);
        // botones[i]->setClickMs(400);   // ventana para detectar doble/triple
        // botones[i]->setPressMs(1000);  // tiempo para considerar "larga"
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
    for (int i = 0; i < NUM_PULSADORES; i++) {
        botones[i]->tick();
    }

    static unsigned long ultimoAviso = 0;
    if (!mqtt.isConnected() && millis() - ultimoAviso > 5000) {
        ultimoAviso = millis();
        Serial.println(F("[mqtt] sigue sin conectar, reintentando..."));
    }
}
