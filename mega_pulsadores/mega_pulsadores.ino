// ===========================================================
// MEGA_PULSADORES
// Lee pulsadores físicos y envía eventos MQTT (device triggers)
// a Home Assistant: pulsación corta, doble, triple, larga y fin de
// larga (al soltar) siempre; cuádruple y quíntuple son opcionales
// (ver HABILITAR_CUADRUPLE/HABILITAR_QUINTUPLE más abajo — desactivarlas
// ahorra RAM, útil si necesitas más pulsadores de los que caben con los
// 7 triggers completos).
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
// Desactivadas por defecto (ahorran RAM — cada HADeviceTrigger que no
// se crea es memoria y una entidad MQTT menos, útil si necesitas más
// pulsadores de los que caben con los 7 triggers completos, ver "RAM /
// límite de pulsadores" más abajo y en todo.md). Descomenta cualquiera
// de estas dos líneas para crear ese trigger en TODOS los pulsadores
// de esta unidad — ⚠️ si activas alguna, revisa que ningún blueprint
// instanciado dependa de "button_quadruple_press"/"button_quintuple_press"
// antes de dejarla desactivada de nuevo (p. ej.
// persiana_pulsador_completo.yaml, pulsaciones 4/5).
// corta/doble/triple/larga/largaFin siempre están activos — no hay
// caso de uso todavía para desactivarlos.
// ===========================================================
// #define HABILITAR_CUADRUPLE
// #define HABILITAR_QUINTUPLE

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

// Triggers por pulsador: corta, doble, triple, larga y fin de larga
// siempre (5) + cuádruple/quíntuple si están habilitados arriba.
#if defined(HABILITAR_CUADRUPLE) && defined(HABILITAR_QUINTUPLE)
    #define NUM_TRIGGERS_POR_PULSADOR 7
#elif defined(HABILITAR_CUADRUPLE) || defined(HABILITAR_QUINTUPLE)
    #define NUM_TRIGGERS_POR_PULSADOR 6
#else
    #define NUM_TRIGGERS_POR_PULSADOR 5
#endif

// + margen
HAMqtt mqtt(client, device, NUM_PULSADORES * NUM_TRIGGERS_POR_PULSADOR + 2);

OneButton* botones[NUM_PULSADORES];

// Orden deliberado: corta -> doble -> triple -> cuádruple -> quíntuple
// (progresión 1-2-3-4-5 pulsaciones), y larga/fin de larga aparte, al
// final, como caso especial.
HADeviceTrigger* corta[NUM_PULSADORES];
HADeviceTrigger* doble[NUM_PULSADORES];
HADeviceTrigger* triple[NUM_PULSADORES];
#ifdef HABILITAR_CUADRUPLE
HADeviceTrigger* cuadruple[NUM_PULSADORES];
#endif
#ifdef HABILITAR_QUINTUPLE
HADeviceTrigger* quintuple[NUM_PULSADORES];
#endif
HADeviceTrigger* larga[NUM_PULSADORES];
// Se dispara al SOLTAR una pulsación larga. Imprescindible para
// automatizaciones "mantener pulsado para mover / soltar para parar"
// (p. ej. persianas): "larga" = empezar a subir, "larga_fin" = parar.
HADeviceTrigger* largaFin[NUM_PULSADORES];

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

// OneButton::attachXxx() solo acepta funciones sin captura (function
// pointer puro) o su variante parameterizedCallbackFunction(void*) — una
// lambda con captura como [idx](){...} no convierte a ninguna de las dos
// firmas. Usamos la variante con parámetro, pasando el índice del
// pulsador como void* (reinterpret_cast<void*>(idx) / de vuelta a int).
void onClick(void* param) {
    int idx = reinterpret_cast<int>(param);
    corta[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.println(F(" -> corta"));
}

void onDoubleClick(void* param) {
    int idx = reinterpret_cast<int>(param);
    doble[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.println(F(" -> doble"));
}

// OneButton no tiene "attachTripleClick"/"attachQuadrupleClick"/
// "attachQuintupleClick" propios: se usa attachMultiClick (una sola vez)
// y se filtra el número exacto de clics detectados.
void onMultiClick(void* param) {
    int idx = reinterpret_cast<int>(param);
    int clics = botones[idx]->getNumberClicks();
    switch (clics) {
        case 3: triple[idx]->trigger();    break;
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

void onLongPressStart(void* param) {
    int idx = reinterpret_cast<int>(param);
    larga[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.println(F(" -> larga (inicio)"));
}

void onLongPressStop(void* param) {
    int idx = reinterpret_cast<int>(param);
    largaFin[idx]->trigger();
    Serial.print(F("[boton] "));
    Serial.print(idBoton[idx]);
    Serial.println(F(" -> larga (fin)"));
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
    device.setSoftwareVersion("1.7.0");

    // --- creamos cada pulsador y sus triggers (NUM_TRIGGERS_POR_PULSADOR) ---
    for (int i = 0; i < NUM_PULSADORES; i++) {
        snprintf(idBoton[i], sizeof(idBoton[i]), "boton_%d", PINES_BOTONES[i]);

        botones[i] = new OneButton(PINES_BOTONES[i], true); // true = INPUT_PULLUP, activo en LOW

        corta[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonShortPressType,     idBoton[i]);
        doble[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonDoublePressType,    idBoton[i]);
        triple[i]    = new HADeviceTrigger(HADeviceTrigger::ButtonTriplePressType,    idBoton[i]);
#ifdef HABILITAR_CUADRUPLE
        cuadruple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuadruplePressType, idBoton[i]);
#endif
#ifdef HABILITAR_QUINTUPLE
        quintuple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuintuplePressType, idBoton[i]);
#endif
        larga[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonLongPressType,      idBoton[i]);
        largaFin[i]  = new HADeviceTrigger(HADeviceTrigger::ButtonLongReleaseType,    idBoton[i]);

        // void* que se le pasa de vuelta a cada callback (ver onClick() etc.
        // más arriba) para que sepa de qué pulsador se trata — no podemos
        // capturar "i"/"idx" en una lambda porque OneButton exige un
        // function pointer puro o su variante con parámetro void*.
        void* idxParam = reinterpret_cast<void*>(i);

        botones[i]->attachClick(onClick, idxParam);
        botones[i]->attachDoubleClick(onDoubleClick, idxParam);
        botones[i]->attachMultiClick(onMultiClick, idxParam);
        botones[i]->attachLongPressStart(onLongPressStart, idxParam);
        botones[i]->attachLongPressStop(onLongPressStop, idxParam);

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
