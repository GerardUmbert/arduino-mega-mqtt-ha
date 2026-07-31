// ===========================================================
// MEGA_DISPOSITIVOS
// Recibe comandos MQTT de Home Assistant y activa los relés
// de luces y persianas correspondientes.
// No lee ningún pulsador. Solo RECIBE órdenes y las ejecuta.
//
// Mismo firmware para las 2 unidades físicas (A y B): la identidad
// (pines, MAC, nombre) se decide en TIEMPO DE COMPILACIÓN con
// PLACA_A/PLACA_B (ver más abajo) — no hay jumper físico.
//
// Librerías necesarias (Arduino Library Manager):
//   - ArduinoHA        https://github.com/dawidchyrzynski/arduino-home-assistant
//   - Ethernet (incluida en el IDE si usas shield W5100/W5500)
// ===========================================================

#include <Ethernet.h>
#include <ArduinoHA.h>

// ===========================================================
// CONFIGURACIÓN DE RED
// Copia "config.h.example" como "config.h" en esta misma carpeta
// y rellena tu IP/usuario/password reales. "config.h" está en
// .gitignore, así que tus credenciales no se suben al repositorio.
// ===========================================================
#include "config.h"

struct ParPines { uint8_t subir; uint8_t bajar; };

// ===========================================================
// IDENTIFICACIÓN DE LA PLACA — ⚠️ CAMBIAR ANTES DE CADA FLASH ⚠️
// Deja SOLO una de las dos líneas descomentada según a qué unidad
// física vayas a subir este firmware. Selecciona a la vez: los pines
// cableados (pines_a.h / pines_b.h), la MAC y el nombre en Home
// Assistant. Vuelve a compilar y subir tras cambiarla.
// ===========================================================
#define PLACA_A
// #define PLACA_B

#if defined(PLACA_A) && defined(PLACA_B)
    #error "Deja solo una de PLACA_A o PLACA_B descomentada, no las dos."
#elif !defined(PLACA_A) && !defined(PLACA_B)
    #error "Descomenta PLACA_A o PLACA_B para indicar qué unidad es esta."
#endif

#if defined(PLACA_A)
    #include "pines_a.h"
#elif defined(PLACA_B)
    #include "pines_b.h"
#endif

// El Mega + shield Ethernet NO trae MAC de fábrica: hay que inventarla.
// Solo debe ser única en tu red local.
// 0x02 en el primer byte = MAC "administrada localmente".
// byte[3] = 0x02 identifica la "familia" mega_dispositivos (distinta
// de mega_pulsadores, que usa 0x01) para que nunca choquen entre sí.
// El último byte distingue unidad A (0x00) de B (0x01).
#if defined(PLACA_A)
    byte mac[] = {0x02, 0x00, 0x00, 0x02, 0x00, 0x00};
    const char* NOMBRE_PLACA = "Mega Dispositivos A";
#elif defined(PLACA_B)
    byte mac[] = {0x02, 0x00, 0x00, 0x02, 0x00, 0x01};
    const char* NOMBRE_PLACA = "Mega Dispositivos B";
#endif

EthernetClient client;
HADevice device(mac, sizeof(mac));

// ===========================================================
// PINES RESERVADOS POR EL SHIELD ETHERNET — NO USAR
//   SPI: 50 (MISO), 51 (MOSI), 52 (SCK), 53 (SS)
//   CS del chip Ethernet: normalmente el pin 10
// ===========================================================

// PINES_LUCES y PINES_PERSIANAS están definidos en pines_a.h o
// pines_b.h según PLACA_A/PLACA_B (ver más arriba) — edita esos
// ficheros para reflejar lo que tengas cableado en cada unidad.
// El unique_id de cada persiana incluye ambos pines del par, SIEMPRE
// en el orden subir_bajar (p. ej. {subir: 38, bajar: 39} →
// "persiana_38_39"), nunca al revés.
const int NUM_LUCES = sizeof(PINES_LUCES) / sizeof(PINES_LUCES[0]);
const int NUM_PERSIANAS = sizeof(PINES_PERSIANAS) / sizeof(PINES_PERSIANAS[0]);

// Tiempo de seguridad entre apagar un sentido y encender el otro,
// para que el relé no reciba ambas señales casi a la vez (protege el motor).
// Si tus relés/módulo ya tienen interlock por hardware, puedes bajarlo.
#define RETARDO_INVERSION_MS 200

// ===========================================================
// OBJETOS HA — se crean dinámicamente en setup(), no a mano
// ===========================================================
HAMqtt mqtt(client, device, NUM_LUCES + NUM_PERSIANAS + 2);

HASwitch* luces[NUM_LUCES];
char      idLuz[NUM_LUCES][8]; // buffers de texto: deben vivir todo el programa

HACover*  persianas[NUM_PERSIANAS];
char      idPersiana[NUM_PERSIANAS][17];

// ===========================================================
// CALLBACK LUCES
// Busca qué objeto llamó (sender) y actúa sobre el pin de esa
// misma posición en el array.
// ===========================================================
void onSwitchCommand(bool state, HASwitch* sender) {
    for (int i = 0; i < NUM_LUCES; i++) {
        if (luces[i] == sender) {
            digitalWrite(PINES_LUCES[i], state ? HIGH : LOW);
            sender->setState(state); // confirma el estado a HA
            Serial.print(F("[luz] "));
            Serial.print(idLuz[i]);
            Serial.println(state ? F(" -> ON") : F(" -> OFF"));
            return;
        }
    }
}

// ===========================================================
// CALLBACK PERSIANAS
// Soporta abrir, cerrar y PARAR (los tres botones que HA muestra
// automáticamente en la tarjeta de la persiana).
// ===========================================================
void onCoverCommand(HACover::CoverCommand cmd, HACover* sender) {
    for (int i = 0; i < NUM_PERSIANAS; i++) {
        if (persianas[i] == sender) {
            uint8_t pinSubir = PINES_PERSIANAS[i].subir;
            uint8_t pinBajar = PINES_PERSIANAS[i].bajar;

            if (cmd == HACover::CommandOpen) {
                digitalWrite(pinBajar, LOW);
                delay(RETARDO_INVERSION_MS);
                digitalWrite(pinSubir, HIGH);
                sender->setState(HACover::StateOpening);
                Serial.print(F("[persiana] "));
                Serial.print(idPersiana[i]);
                Serial.println(F(" -> OPEN"));
            } else if (cmd == HACover::CommandClose) {
                digitalWrite(pinSubir, LOW);
                delay(RETARDO_INVERSION_MS);
                digitalWrite(pinBajar, HIGH);
                sender->setState(HACover::StateClosing);
                Serial.print(F("[persiana] "));
                Serial.print(idPersiana[i]);
                Serial.println(F(" -> CLOSE"));
            } else if (cmd == HACover::CommandStop) {
                // Parar = apagar los dos relés a la vez.
                digitalWrite(pinSubir, LOW);
                digitalWrite(pinBajar, LOW);
                sender->setState(HACover::StateStopped);
                Serial.print(F("[persiana] "));
                Serial.print(idPersiana[i]);
                Serial.println(F(" -> STOP"));
            }
            return;
        }
    }
}

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

    // Evita que HA confunda entidades con el mismo ID entre unidad A y B
    device.enableExtendedUniqueIds();

    device.setName(NOMBRE_PLACA);
    device.setSoftwareVersion("1.3.0");

    // --- luces: se crean y configuran en bucle ---
    for (int i = 0; i < NUM_LUCES; i++) {
        pinMode(PINES_LUCES[i], OUTPUT);
        digitalWrite(PINES_LUCES[i], LOW); // arrancan apagadas

        snprintf(idLuz[i], sizeof(idLuz[i]), "luz_%d", PINES_LUCES[i]);
        luces[i] = new HASwitch(idLuz[i]);
        luces[i]->onCommand(onSwitchCommand);
        luces[i]->setIcon("mdi:lightbulb");
    }

    // --- persianas: idem ---
    for (int i = 0; i < NUM_PERSIANAS; i++) {
        pinMode(PINES_PERSIANAS[i].subir, OUTPUT);
        pinMode(PINES_PERSIANAS[i].bajar, OUTPUT);
        digitalWrite(PINES_PERSIANAS[i].subir, LOW);
        digitalWrite(PINES_PERSIANAS[i].bajar, LOW);

        // Orden fijo subir_bajar en el ID, no alfabético ni el que sea menor.
        snprintf(idPersiana[i], sizeof(idPersiana[i]), "persiana_%d_%d", PINES_PERSIANAS[i].subir, PINES_PERSIANAS[i].bajar);
        persianas[i] = new HACover(idPersiana[i]);
        persianas[i]->onCommand(onCoverCommand);
    }

    Serial.println(F("[boot] iniciando Ethernet (DHCP)..."));
    if (Ethernet.begin(mac) == 0) {
        Serial.println(F("[boot] ERROR: fallo DHCP, no se obtuvo IP"));
    } else {
        Serial.print(F("[boot] IP asignada: "));
        Serial.println(Ethernet.localIP());
    }

    mqtt.onConnected(onMqttConnected);
    mqtt.onDisconnected(onMqttDisconnected);

    Serial.println(F("[boot] conectando a MQTT..."));
    mqtt.begin(BROKER_ADDR, MQTT_USER, MQTT_PASS);
}

void loop() {
    Ethernet.maintain();
    mqtt.loop();
}
