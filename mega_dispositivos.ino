// ===========================================================
// MEGA_DISPOSITIVOS
// Recibe comandos MQTT de Home Assistant y activa los relés
// de luces y persianas correspondientes.
// No lee ningún pulsador. Solo RECIBE órdenes y las ejecuta.
//
// Mismo firmware para las 2 unidades físicas (A y B):
// la identidad se resuelve sola con un jumper (ver más abajo).
//
// Librerías necesarias (Arduino Library Manager):
//   - ArduinoHA        https://github.com/dawidchyrzynski/arduino-home-assistant
//   - Ethernet (incluida en el IDE si usas shield W5100/W5500)
// ===========================================================

#include <Ethernet.h>
#include <ArduinoHA.h>

// ===========================================================
// CONFIGURACIÓN DE RED — EDITAR SEGÚN TU INSTALACIÓN
// ===========================================================

// IP de tu Home Assistant / servidor Mosquitto.
// No se autodetecta: reserva esta IP en tu router (DHCP estático)
// para que nunca cambie, y ponla aquí.
#define BROKER_ADDR IPAddress(192, 168, 1, 50)

#define MQTT_USER "usuario_mqtt"
#define MQTT_PASS "password_mqtt"

// ===========================================================
// IDENTIFICACIÓN DE LA PLACA (para subir el MISMO firmware
// a las 2 unidades de mega_dispositivos sin tocar código)
// ===========================================================
// Pin usado como jumper de identidad.
// Unidad A: pin AL AIRE (sin conectar nada).
// Unidad B: pin puenteado con un cable a cualquier GND del Mega.
#define PIN_ID_PLACA 40

byte deviceId = 0; // se calcula en setup(): 0 = unidad A, 1 = unidad B

// El Mega + shield Ethernet NO trae MAC de fábrica: hay que inventarla.
// Solo debe ser única en tu red local.
// 0x02 en el primer byte = MAC "administrada localmente".
// byte[3] = 0x02 identifica la "familia" mega_dispositivos (distinta
// de mega_pulsadores, que usa 0x01) para que nunca choquen entre sí.
// El último byte se sobreescribe solo en setup() según el jumper.
byte mac[] = {0x02, 0x00, 0x00, 0x02, 0x00, 0x00};

EthernetClient client;
HADevice device(mac, sizeof(mac));

// ===========================================================
// PINES RESERVADOS POR EL SHIELD ETHERNET — NO USAR
//   SPI: 50 (MISO), 51 (MOSI), 52 (SCK), 53 (SS)
//   CS del chip Ethernet: normalmente el pin 10
// ===========================================================

// ===========================================================
// LUCES — ⚠️ EDITAR SEGÚN LO QUE TENGAS CABLEADO EN ESTA UNIDAD ⚠️
// Un pin por luz (activa el relé correspondiente).
// ===========================================================
const uint8_t PINES_LUCES[] = {
    22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37
    // añade/quita pines según las luces que controle ESTA unidad
};
const int NUM_LUCES = sizeof(PINES_LUCES) / sizeof(PINES_LUCES[0]);

// ===========================================================
// PERSIANAS — ⚠️ TAMBIÉN EDITABLE ⚠️
// Cada persiana usa 2 pines: relé de "subir" y relé de "bajar".
// Nunca deben ir a HIGH los dos a la vez (protección por software
// más abajo, en RETARDO_INVERSION_MS).
// ===========================================================
struct ParPines { uint8_t subir; uint8_t bajar; };

const ParPines PINES_PERSIANAS[] = {
    {38, 39}, {41, 42}, {43, 44}, {45, 46},
    {47, 48}, {49, A0},  {A1, A2}, {A3, A4}
    // añade/quita pares según las persianas que controle ESTA unidad
    // (A0-A15 también funcionan como pines digitales normales en el Mega)
};
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
char      idLuz[NUM_LUCES][10]; // buffers de texto: deben vivir todo el programa

HACover*  persianas[NUM_PERSIANAS];
char      idPersiana[NUM_PERSIANAS][14];

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
            } else if (cmd == HACover::CommandClose) {
                digitalWrite(pinSubir, LOW);
                delay(RETARDO_INVERSION_MS);
                digitalWrite(pinBajar, HIGH);
                sender->setState(HACover::StateClosing);
            } else if (cmd == HACover::CommandStop) {
                // Parar = apagar los dos relés a la vez.
                digitalWrite(pinSubir, LOW);
                digitalWrite(pinBajar, LOW);
                sender->setState(HACover::StateStopped);
            }
            return;
        }
    }
}

void setup() {
    // --- resolvemos qué unidad somos (A o B) ---
    pinMode(PIN_ID_PLACA, INPUT_PULLUP);
    deviceId = (digitalRead(PIN_ID_PLACA) == LOW) ? 1 : 0;
    mac[5] = deviceId; // MAC distinta para cada unidad

    // Evita que HA confunda entidades con el mismo ID entre unidad A y B
    device.enableExtendedUniqueIds();

    char nombre[26];
    snprintf(nombre, sizeof(nombre), "Mega Dispositivos %c", deviceId == 0 ? 'A' : 'B');
    device.setName(nombre);
    device.setSoftwareVersion("1.0.0");

    // --- luces: se crean y configuran en bucle ---
    for (int i = 0; i < NUM_LUCES; i++) {
        pinMode(PINES_LUCES[i], OUTPUT);
        digitalWrite(PINES_LUCES[i], LOW); // arrancan apagadas

        snprintf(idLuz[i], sizeof(idLuz[i]), "luz_%02d", i + 1);
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

        snprintf(idPersiana[i], sizeof(idPersiana[i]), "persiana_%02d", i + 1);
        persianas[i] = new HACover(idPersiana[i]);
        persianas[i]->onCommand(onCoverCommand);
    }

    Ethernet.begin(mac);
    mqtt.begin(BROKER_ADDR, MQTT_USER, MQTT_PASS);
}

void loop() {
    Ethernet.maintain();
    mqtt.loop();
}
