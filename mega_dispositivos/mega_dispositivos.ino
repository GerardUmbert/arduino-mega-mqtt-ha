// ===========================================================
// MEGA_DISPOSITIVOS
// Recibe comandos MQTT de Home Assistant y activa los relés
// de luces y persianas correspondientes.
// No lee ningún pulsador. Solo RECIBE órdenes y las ejecuta.
//
// Mismo firmware para las 2 unidades físicas (A y B): la identidad
// (pines, MAC, IP, nombre) se decide en TIEMPO DE COMPILACIÓN con
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

// ===========================================================
// PINES RESERVADOS POR EL SHIELD ETHERNET — NO USAR
//   SPI: 50 (MISO), 51 (MOSI), 52 (SCK), 53 (SS)
//   CS del chip Ethernet: normalmente el pin 10
// ===========================================================

// PINES_LUCES y PINES_PERSIANAS están definidos en board_config_a.h o
// board_config_b.h según PLACA_A/PLACA_B (ver más arriba) — edita esos
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

// Tiempo máximo que se deja un relé de persiana energizado sin parar.
// Protege el motor si HA no envía STOP (fallo de red, tarjeta de HA
// cerrada a medio camino, etc.): pasado este tiempo se para sola.
// Ajusta al tiempo real de recorrido de tus persianas + margen.
unsigned long TIEMPO_MAX_MOVIMIENTO_MS = 20000;

// ===========================================================
// OBJETOS HA — se crean dinámicamente en setup(), no a mano
// ===========================================================
HAMqtt mqtt(client, device, NUM_LUCES + NUM_PERSIANAS + 2);

HASwitch* luces[NUM_LUCES];
char      idLuz[NUM_LUCES][8]; // buffers de texto: deben vivir todo el programa

HACover*  persianas[NUM_PERSIANAS];
char      idPersiana[NUM_PERSIANAS][17];

// 0 = parada. Distinto de 0 = en movimiento desde ese momento (millis()),
// para poder pararla sola si se pasa de TIEMPO_MAX_MOVIMIENTO_MS y para
// estimar la posición recorrida por tiempo.
unsigned long inicioMovimiento[NUM_PERSIANAS] = {0};

// Sentido del movimiento en curso (solo válido mientras inicioMovimiento[i] != 0).
bool subiendo[NUM_PERSIANAS] = {false};

// Posición estimada 0-100 (0 = cerrada del todo, 100 = abierta del todo).
// Sin encoder no hay forma de saberla de verdad al arrancar: se asume
// 100 (abierta) hasta que el usuario mueva la persiana a un extremo real,
// momento en el que se resincroniza sola a 0 o 100.
int16_t posicionActual[NUM_PERSIANAS];

// Última vez que se publicó la posición por MQTT mientras se movía
// (evita saturar el broker publicando en cada vuelta de loop()).
unsigned long ultimaPublicacionPosicion[NUM_PERSIANAS] = {0};
#define INTERVALO_PUBLICAR_POSICION_MS 500

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
// Posición estimada mientras se mueve, a partir del tiempo transcurrido
// desde que arrancó y el tiempo de recorrido completo calibrado para esa
// persiana y ese sentido. Se satura a 0/100 por si el tiempo transcurrido
// se pasa del calibrado (más vale reportar el extremo que un valor fuera
// de rango).
int16_t posicionEnCurso(int i) {
    if (inicioMovimiento[i] == 0) return posicionActual[i];

    unsigned long transcurrido = millis() - inicioMovimiento[i];
    unsigned long total = subiendo[i] ? TIEMPOS_PERSIANAS[i].subida_ms
                                       : TIEMPOS_PERSIANAS[i].bajada_ms;
    if (total == 0) return posicionActual[i];

    long avance = (long)(100UL * transcurrido / total);
    long estimado = subiendo[i] ? posicionActual[i] + avance
                                 : posicionActual[i] - avance;
    if (estimado > 100) estimado = 100;
    if (estimado < 0) estimado = 0;
    return (int16_t)estimado;
}

void pararPersiana(int i, const __FlashStringHelper* motivo) {
    posicionActual[i] = posicionEnCurso(i);
    digitalWrite(PINES_PERSIANAS[i].subir, LOW);
    digitalWrite(PINES_PERSIANAS[i].bajar, LOW);
    inicioMovimiento[i] = 0;
    persianas[i]->setPosition(posicionActual[i]);
    persianas[i]->setState(HACover::StateStopped);
    Serial.print(F("[persiana] "));
    Serial.print(idPersiana[i]);
    Serial.print(F(" -> STOP ("));
    Serial.print(motivo);
    Serial.print(F(") pos="));
    Serial.println(posicionActual[i]);
}

void onCoverCommand(HACover::CoverCommand cmd, HACover* sender) {
    for (int i = 0; i < NUM_PERSIANAS; i++) {
        if (persianas[i] == sender) {
            uint8_t pinSubir = PINES_PERSIANAS[i].subir;
            uint8_t pinBajar = PINES_PERSIANAS[i].bajar;

            if (cmd == HACover::CommandOpen) {
                // Si venía moviéndose (p. ej. cerrando) sin haber parado,
                // primero congela la posición real recorrida hasta ahora
                // — si no, se pierde ese tramo y la posición se desincroniza.
                if (inicioMovimiento[i] != 0) posicionActual[i] = posicionEnCurso(i);
                digitalWrite(pinBajar, LOW);
                delay(RETARDO_INVERSION_MS);
                digitalWrite(pinSubir, HIGH);
                subiendo[i] = true;
                inicioMovimiento[i] = millis();
                sender->setState(HACover::StateOpening);
                Serial.print(F("[persiana] "));
                Serial.print(idPersiana[i]);
                Serial.println(F(" -> OPEN"));
            } else if (cmd == HACover::CommandClose) {
                if (inicioMovimiento[i] != 0) posicionActual[i] = posicionEnCurso(i);
                digitalWrite(pinSubir, LOW);
                delay(RETARDO_INVERSION_MS);
                digitalWrite(pinBajar, HIGH);
                subiendo[i] = false;
                inicioMovimiento[i] = millis();
                sender->setState(HACover::StateClosing);
                Serial.print(F("[persiana] "));
                Serial.print(idPersiana[i]);
                Serial.println(F(" -> CLOSE"));
            } else if (cmd == HACover::CommandStop) {
                pararPersiana(i, F("orden HA"));
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
    device.setSoftwareVersion("1.6.1");

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
        // PositionFeature: sin esto, HA colapsa "stopped" a open/closed
        // (no existe un estado "parada a medias" sin posición real).
        persianas[i] = new HACover(idPersiana[i], HACover::PositionFeature);
        persianas[i]->setDeviceClass("shutter");
        persianas[i]->onCommand(onCoverCommand);

        // Sin encoder no hay forma de saber la posición real al arrancar:
        // se asume abierta del todo hasta que el usuario la lleve a un
        // extremo real y se resincronice sola.
        posicionActual[i] = 100;
        persianas[i]->setPosition(posicionActual[i]);
        persianas[i]->setState(HACover::StateOpen);
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

    static unsigned long ultimoAviso = 0;
    if (!mqtt.isConnected() && millis() - ultimoAviso > 5000) {
        ultimoAviso = millis();
        Serial.println(F("[mqtt] sigue sin conectar, reintentando..."));
    }

    for (int i = 0; i < NUM_PERSIANAS; i++) {
        if (inicioMovimiento[i] == 0) continue;

        // Parada de seguridad: si lleva demasiado tiempo en movimiento
        // (p. ej. HA no llegó a enviar STOP), se para sola para no forzar
        // el motor contra el tope indefinidamente.
        if (millis() - inicioMovimiento[i] > TIEMPO_MAX_MOVIMIENTO_MS) {
            pararPersiana(i, F("timeout seguridad"));
            continue;
        }

        int16_t posicion = posicionEnCurso(i);

        // Llegó sola al extremo hacia el que se estaba moviendo (recorrido
        // completo calibrado): parar y resincronizar a 0/100 exactos, con
        // el estado final correcto (abierta/cerrada, no "stopped").
        // OJO: comprobar solo el extremo de la dirección actual — si no,
        // p. ej. un CLOSE que arranca desde posicionActual=100 lee 100 en
        // la primera vuelta de loop() (aún no ha avanzado) y se confunde
        // con "ya llegó a abierta del todo".
        bool llegoAlExtremo = subiendo[i] ? (posicion >= 100) : (posicion <= 0);
        if (llegoAlExtremo) {
            posicionActual[i] = posicion <= 0 ? 0 : 100;
            digitalWrite(PINES_PERSIANAS[i].subir, LOW);
            digitalWrite(PINES_PERSIANAS[i].bajar, LOW);
            inicioMovimiento[i] = 0;
            persianas[i]->setPosition(posicionActual[i]);
            persianas[i]->setState(posicionActual[i] == 0 ? HACover::StateClosed : HACover::StateOpen);
            Serial.print(F("[persiana] "));
            Serial.print(idPersiana[i]);
            Serial.println(posicionActual[i] == 0 ? F(" -> CLOSED (fin recorrido)") : F(" -> OPEN (fin recorrido)"));
            continue;
        }

        // Mientras se mueve, publica la posición estimada cada cierto
        // intervalo (no en cada vuelta de loop(), para no saturar el broker).
        if (millis() - ultimaPublicacionPosicion[i] > INTERVALO_PUBLICAR_POSICION_MS) {
            ultimaPublicacionPosicion[i] = millis();
            persianas[i]->setPosition(posicion);
        }
    }
}
