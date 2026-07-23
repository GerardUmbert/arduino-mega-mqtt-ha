// ===========================================================
// MEGA_PULSADORES
// Lee pulsadores físicos y envía eventos MQTT (device triggers)
// a Home Assistant: pulsación corta, doble, triple, cuádruple,
// quíntuple y larga.
// No controla ningún relé. Solo ENVÍA información.
//
// Mismo firmware para las 2 unidades físicas (A y B):
// la identidad se resuelve sola con un jumper (ver más abajo).
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
// IDENTIFICACIÓN DE LA PLACA (para subir el MISMO firmware
// a las 2 unidades de mega_pulsadores sin tocar código)
// ===========================================================
// Pin usado como jumper de identidad.
// Unidad A: pin AL AIRE (sin conectar nada).
// Unidad B: pin puenteado con un cable a cualquier GND del Mega.
#define PIN_ID_PLACA 40

byte deviceId = 0; // se calcula en setup(): 0 = unidad A, 1 = unidad B

// El Mega + shield Ethernet NO trae MAC de fábrica: hay que inventarla.
// Solo debe ser única en tu red local.
// 0x02 en el primer byte = MAC "administrada localmente" (evita
// coincidir por casualidad con una MAC real de fábrica).
// byte[3] = 0x01 identifica la "familia" mega_pulsadores (distinta
// de mega_dispositivos, que usa 0x02) para que nunca choquen entre sí.
// El último byte se sobreescribe solo en setup() según el jumper.
byte mac[] = {0x02, 0x00, 0x00, 0x01, 0x00, 0x00};

EthernetClient client;
HADevice device(mac, sizeof(mac));

// ===========================================================
// PULSADORES — ⚠️ ES LO ÚNICO QUE NORMALMENTE DEBES EDITAR ⚠️
// Un pin por pulsador. Añade o quita líneas según los botones
// que tengas cableados físicamente en ESTA unidad (A o B).
// Evita los pines reservados por el shield Ethernet:
//   SPI: 50 (MISO), 51 (MOSI), 52 (SCK), 53 (SS)
//   CS del chip Ethernet: normalmente el pin 10
//
// ⚠️⚠️ IMPORTANTE — EL ORDEN DE ESTE ARRAY IMPORTA ⚠️⚠️
// El unique_id de cada pulsador (boton_01, boton_02...) se genera SOLO
// por la POSICIÓN de cada pin en esta lista, no por el número de pin
// en sí. Ejemplo: boton_05 = el 5º pin de la lista, sea cual sea.
//
// Una vez subido el firmware Y renombrados los device triggers en Home
// Assistant (p. ej. boton_05 → "Interruptor Dormitorio 1"), NO
// reordenes ni insertes/borres pines EN MEDIO de esta lista: todo lo
// que va detrás se desplaza de posición y boton_05 pasaría a ser otro
// pulsador físico distinto. Si necesitas añadir un pulsador nuevo más
// adelante, añade su pin SIEMPRE AL FINAL de la lista, nunca en medio.
// ===========================================================
const uint8_t PINES_BOTONES[] = {
    2, 3, 4, 5, 6, 7, 8, 9,
    14, 15, 16, 17, 18, 19,
    24, 25, 26, 27, 28, 29
    // añade más pines aquí, SIEMPRE AL FINAL, si tienes más pulsadores
};
const int NUM_PULSADORES = sizeof(PINES_BOTONES) / sizeof(PINES_BOTONES[0]);

// 6 triggers por pulsador (corta, doble, triple, cuádruple, quíntuple,
// larga) + margen
HAMqtt mqtt(client, device, NUM_PULSADORES * 6 + 2);

OneButton* botones[NUM_PULSADORES];

// Orden deliberado: corta -> doble -> triple -> cuádruple -> quíntuple
// (progresión 1-2-3-4-5 pulsaciones), y larga aparte, al final, como
// caso especial.
HADeviceTrigger* corta[NUM_PULSADORES];
HADeviceTrigger* doble[NUM_PULSADORES];
HADeviceTrigger* triple[NUM_PULSADORES];
HADeviceTrigger* cuadruple[NUM_PULSADORES];
HADeviceTrigger* quintuple[NUM_PULSADORES];
HADeviceTrigger* larga[NUM_PULSADORES];

// Buffers de texto para los IDs. Deben ser globales (viven todo el
// programa) porque HADeviceTrigger se queda con el puntero al texto,
// no con una copia.
char idBoton[NUM_PULSADORES][12];

void setup() {
    // --- resolvemos qué unidad somos (A o B) ---
    pinMode(PIN_ID_PLACA, INPUT_PULLUP);
    deviceId = (digitalRead(PIN_ID_PLACA) == LOW) ? 1 : 0;
    mac[5] = deviceId; // MAC distinta para cada unidad

    // Evita que HA confunda entidades/triggers con el mismo ID
    // entre la unidad A y la B (les añade un prefijo único por placa).
    device.enableExtendedUniqueIds();

    char nombre[26];
    snprintf(nombre, sizeof(nombre), "Mega Pulsadores %c", deviceId == 0 ? 'A' : 'B');
    device.setName(nombre);
    device.setSoftwareVersion("1.1.0");

    // --- creamos cada pulsador y sus 4 triggers ---
    for (int i = 0; i < NUM_PULSADORES; i++) {
        snprintf(idBoton[i], sizeof(idBoton[i]), "boton_%02d", i + 1);

        botones[i] = new OneButton(PINES_BOTONES[i], true); // true = INPUT_PULLUP, activo en LOW

        corta[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonShortPressType,     idBoton[i]);
        doble[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonDoublePressType,    idBoton[i]);
        triple[i]    = new HADeviceTrigger(HADeviceTrigger::ButtonTriplePressType,    idBoton[i]);
        cuadruple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuadruplePressType, idBoton[i]);
        quintuple[i] = new HADeviceTrigger(HADeviceTrigger::ButtonQuintuplePressType, idBoton[i]);
        larga[i]     = new HADeviceTrigger(HADeviceTrigger::ButtonLongPressType,      idBoton[i]);

        int idx = i; // captura por VALOR: evita que todas las lambdas
                     // acaben apuntando al último valor de "i" del bucle

        botones[i]->attachClick([idx](){
            corta[idx]->trigger();
        });

        botones[i]->attachDoubleClick([idx](){
            doble[idx]->trigger();
        });

        // OneButton no tiene "attachTripleClick"/"attachQuadrupleClick"/
        // "attachQuintupleClick" propios: se usa attachMultiClick (una
        // sola vez) y se filtra el número exacto de clics detectados.
        botones[i]->attachMultiClick([idx](){
            switch (botones[idx]->getNumberClicks()) {
                case 3: triple[idx]->trigger();    break;
                case 4: cuadruple[idx]->trigger(); break;
                case 5: quintuple[idx]->trigger(); break;
            }
        });

        botones[i]->attachLongPressStart([idx](){
            larga[idx]->trigger();
        });

        // Ajustes opcionales de temporización (descomenta y ajusta si
        // los pulsadores van demasiado rápido/lentos para tu gusto):
        // botones[i]->setDebounceMs(50);
        // botones[i]->setClickMs(400);   // ventana para detectar doble/triple
        // botones[i]->setPressMs(1000);  // tiempo para considerar "larga"
    }

    Ethernet.begin(mac);
    mqtt.begin(BROKER_ADDR, MQTT_USER, MQTT_PASS);
}

void loop() {
    Ethernet.maintain();
    mqtt.loop();
    for (int i = 0; i < NUM_PULSADORES; i++) {
        botones[i]->tick();
    }
}
