// ===========================================================
// MEGA_PULSADORES
// Lee pulsadores físicos y envía eventos MQTT (device triggers)
// a Home Assistant: pulsación corta, doble, triple, cuádruple,
// quíntuple, larga y fin de larga (al soltar).
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

EthernetClient client;
HADevice device(mac, sizeof(mac));

// NUM_PULSADORES se calcula a partir de PINES_BOTONES, definido en
// board_config_a.h o board_config_b.h según PLACA_A/PLACA_B (ver más arriba).
//
// ⚠️ RAM: cada pulsador cuesta aprox. 250 bytes de SRAM (objeto
// OneButton + 7 HADeviceTrigger + punteros + buffer de ID). El Mega
// tiene 8 KB de SRAM total, de los cuales el shield Ethernet y
// ArduinoHA ya reservan una parte antes de llegar aquí. No hay medición
// real todavía de cuántos pulsadores caben con margen — estimación sin
// verificar: unos 20-25 por unidad. Ver "RAM / límite de pulsadores"
// en todo.md antes de cablear muchos más de los que ya hay en
// board_config_a.h/board_config_b.h.
const int NUM_PULSADORES = sizeof(PINES_BOTONES) / sizeof(PINES_BOTONES[0]);

// 7 triggers por pulsador (corta, doble, triple, cuádruple, quíntuple,
// larga, fin de larga) + margen
HAMqtt mqtt(client, device, NUM_PULSADORES * 7 + 2);

OneButton* botones[NUM_PULSADORES];

// Orden deliberado: corta -> doble -> triple -> cuádruple -> quíntuple
// (progresión 1-2-3-4-5 pulsaciones), y larga/fin de larga aparte, al
// final, como caso especial.
HADeviceTrigger* corta[NUM_PULSADORES];
HADeviceTrigger* doble[NUM_PULSADORES];
HADeviceTrigger* triple[NUM_PULSADORES];
HADeviceTrigger* cuadruple[NUM_PULSADORES];
HADeviceTrigger* quintuple[NUM_PULSADORES];
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
    device.setSoftwareVersion("1.5.1");

    // --- creamos cada pulsador y sus 7 triggers ---
    for (int i = 0; i < NUM_PULSADORES; i++) {
        snprintf(idBoton[i], sizeof(idBoton[i]), "boton_%d", PINES_BOTONES[i]);

        botones[i] = new OneButton(PINES_BOTONES[i], true); // true = INPUT_PULLUP, activo en LOW

        corta[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonShortPressType,     idBoton[i]);
        doble[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonDoublePressType,    idBoton[i]);
        triple[i]    = new HADeviceTrigger(HADeviceTrigger::ButtonTriplePressType,    idBoton[i]);
        cuadruple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuadruplePressType, idBoton[i]);
        quintuple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuintuplePressType, idBoton[i]);
        larga[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonLongPressType,      idBoton[i]);
        largaFin[i]  = new HADeviceTrigger(HADeviceTrigger::ButtonLongReleaseType,    idBoton[i]);

        int idx = i; // captura por VALOR: evita que todas las lambdas
                     // acaben apuntando al último valor de "i" del bucle

        botones[i]->attachClick([idx](){
            corta[idx]->trigger();
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.println(F(" -> corta"));
        });

        botones[i]->attachDoubleClick([idx](){
            doble[idx]->trigger();
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.println(F(" -> doble"));
        });

        // OneButton no tiene "attachTripleClick"/"attachQuadrupleClick"/
        // "attachQuintupleClick" propios: se usa attachMultiClick (una
        // sola vez) y se filtra el número exacto de clics detectados.
        botones[i]->attachMultiClick([idx](){
            int clics = botones[idx]->getNumberClicks();
            switch (clics) {
                case 3: triple[idx]->trigger();    break;
                case 4: cuadruple[idx]->trigger(); break;
                case 5: quintuple[idx]->trigger(); break;
            }
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.print(F(" -> multiclick x"));
            Serial.println(clics);
        });

        botones[i]->attachLongPressStart([idx](){
            larga[idx]->trigger();
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.println(F(" -> larga (inicio)"));
        });

        botones[i]->attachLongPressStop([idx](){
            largaFin[idx]->trigger();
            Serial.print(F("[boton] "));
            Serial.print(idBoton[idx]);
            Serial.println(F(" -> larga (fin)"));
        });

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
