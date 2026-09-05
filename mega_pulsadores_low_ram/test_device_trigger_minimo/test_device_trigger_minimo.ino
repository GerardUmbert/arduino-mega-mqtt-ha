// ===========================================================
// TEST MÍNIMO — HADeviceTrigger, calcado del ejemplo OFICIAL de
// ArduinoHA (examples/multi-state-button/multi-state-button.ino del
// propio repo github.com/dawidchyrzynski/arduino-home-assistant).
//
// Objetivo: aislar si el bug de "el discovery de HADeviceTrigger
// (homeassistant/device_automation/.../config) nunca aparece en HA ni
// en MQTT Explorer" viene del firmware completo
// (mega_pulsadores_low_ram.ino, con AceButton + N pulsadores + botón
// virtual) o de la instalación de HA/broker en sí — ver
// mega_pulsadores/to_review.md y CHANGELOG.md (entradas 1.8.2-1.8.4)
// para el contexto completo de esta investigación.
//
// Este sketch NO usa AceButton, NO usa botón virtual, NO usa arrays ni
// new() — solo 1 pin, 2 HADeviceTrigger (corta/larga) como variables
// GLOBALES, exactamente como el ejemplo oficial. Si esto SÍ aparece en
// HA como trigger de tipo Dispositivo (Ajustes → Automatizaciones →
// Añadir trigger → Dispositivo → busca "TestDeviceTrigger"), el
// problema está en algo específico del .ino completo (posiblemente el
// patrón `new HADeviceTrigger(...)` dentro de un bucle, o la cantidad
// de objetos). Si esto TAMPOCO aparece, el problema es de la
// instalación de HA/broker, no de este firmware.
//
// ⚠️ ESTE SKETCH ES SOLO PARA DIAGNÓSTICO — no está pensado para
// quedarse instalado en ninguna unidad real, ni lleva número de
// versión en CHANGELOG.md. Bórralo (o dile a Claude que lo borre)
// cuando ya no lo necesites.
// ===========================================================

#include <Ethernet.h>
#include <ArduinoHA.h>

// ⚠️ RELLENA ESTOS 5 VALORES A MANO con tus datos reales antes de
// compilar — este test NO usa config.h/board_config_a.h para no
// duplicar tus credenciales en un fichero nuevo trackeado por git.
// Copia los valores literales que ya tengas en tu
// mega_pulsadores_low_ram/config.h y board_config_a.h reales.
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // usa una MAC libre en tu red, distinta a la de tus placas reales
IPAddress IP_ESTATICA(192, 168, 1, 199);            // una IP libre en tu red, distinta a la de tus placas reales
IPAddress IP_GATEWAY(192, 168, 1, 1);               // tu gateway real
IPAddress IP_SUBNET(255, 255, 255, 0);
IPAddress BROKER_ADDR(192, 168, 1, 10);             // tu broker MQTT real
const char* MQTT_USER = "";                          // tu usuario MQTT real
const char* MQTT_PASS = "";                          // tu password MQTT real

#define BUTTON_PIN 22   // el mismo pin que ya usas para p22, para que
                        // el resultado sea directamente comparable
#define BUTTON_NAME "testp22"

EthernetClient client;
HADevice device(mac, sizeof(mac));
HAMqtt mqtt(client, device);

// Igual que el ejemplo oficial: variables GLOBALES, sin new(), sin
// arrays — la forma más simple posible de usar HADeviceTrigger.
HADeviceTrigger shortPressTrigger(HADeviceTrigger::ButtonShortPressType, BUTTON_NAME);
HADeviceTrigger longPressTrigger(HADeviceTrigger::ButtonLongPressType, BUTTON_NAME);

bool holdingBtn = false;
unsigned long pressedSince = 0;
int lastState = HIGH;

void onMqttConnected() {
    Serial.println(F("[mqtt] conectado al broker"));
}

void onMqttDisconnected() {
    Serial.println(F("[mqtt] desconectado del broker"));
}

void setup() {
    Serial.begin(9600);
    Serial.println();
    Serial.println(F("[boot] TEST MINIMO HADeviceTrigger"));

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    device.setName("TestDeviceTrigger");
    device.setSoftwareVersion("0.0.1-test");

    mqtt.onConnected(onMqttConnected);
    mqtt.onDisconnected(onMqttDisconnected);

    Serial.println(F("[boot] iniciando Ethernet (IP fija)..."));
    Ethernet.begin(mac, IP_ESTATICA, IP_GATEWAY, IP_GATEWAY, IP_SUBNET);

    Serial.print(F("[boot] IP asignada: "));
    Serial.println(Ethernet.localIP());

    Serial.println(F("[boot] conectando a MQTT..."));
    mqtt.begin(BROKER_ADDR, MQTT_USER, MQTT_PASS);
}

void loop() {
    mqtt.loop();

    int state = digitalRead(BUTTON_PIN);

    if (state == LOW && lastState == HIGH) {
        // flanco de bajada: empieza la pulsación
        pressedSince = millis();
        holdingBtn = false;
    } else if (state == LOW && !holdingBtn && (millis() - pressedSince) > 3000) {
        // llevamos >3s pulsado: pulsación larga
        longPressTrigger.trigger();
        Serial.println(F("[boton] -> larga"));
        holdingBtn = true;
    } else if (state == HIGH && lastState == LOW) {
        // flanco de subida: se soltó
        if (!holdingBtn) {
            shortPressTrigger.trigger();
            Serial.println(F("[boton] -> corta"));
        }
        holdingBtn = false;
    }

    lastState = state;

    static unsigned long ultimoAviso = 0;
    if (!mqtt.isConnected() && millis() - ultimoAviso > 5000) {
        ultimoAviso = millis();
        Serial.println(F("[mqtt] sigue sin conectar, reintentando..."));
    }
}
