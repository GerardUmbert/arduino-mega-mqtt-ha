// ===========================================================
// MEGA_PULSADORES
// Lee pulsadores físicos y envía eventos MQTT (device triggers)
// a Home Assistant: pulsación corta, doble, triple, cuádruple,
// quíntuple, larga y fin de larga (al soltar).
// No controla ningún relé. Solo ENVÍA información.
//
// Mismo firmware para las 2 unidades físicas (A y B): la identidad
// (pines, MAC, nombre) se decide en TIEMPO DE COMPILACIÓN con
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
// 0x02 en el primer byte = MAC "administrada localmente" (evita
// coincidir por casualidad con una MAC real de fábrica).
// byte[3] = 0x01 identifica la "familia" mega_pulsadores (distinta
// de mega_dispositivos, que usa 0x02) para que nunca choquen entre sí.
// El último byte distingue unidad A (0x00) de B (0x01).
#if defined(PLACA_A)
    byte mac[] = {0x02, 0x00, 0x00, 0x01, 0x00, 0x00};
    const char* NOMBRE_PLACA = "Mega Pulsadores A";
#elif defined(PLACA_B)
    byte mac[] = {0x02, 0x00, 0x00, 0x01, 0x00, 0x01};
    const char* NOMBRE_PLACA = "Mega Pulsadores B";
#endif

EthernetClient client;
HADevice device(mac, sizeof(mac));

// NUM_PULSADORES se calcula a partir de PINES_BOTONES, definido en
// pines_a.h o pines_b.h según PLACA_A/PLACA_B (ver más arriba).
//
// ⚠️ RAM: cada pulsador cuesta aprox. 250 bytes de SRAM (objeto
// OneButton + 7 HADeviceTrigger + punteros + buffer de ID). El Mega
// tiene 8 KB de SRAM total, de los cuales el shield Ethernet y
// ArduinoHA ya reservan una parte antes de llegar aquí. No hay medición
// real todavía de cuántos pulsadores caben con margen — estimación sin
// verificar: unos 20-25 por unidad. Ver "RAM / límite de pulsadores"
// en todo.md antes de cablear muchos más de los que ya hay en
// pines_a.h/pines_b.h.
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

void setup() {
    // Evita que HA confunda entidades/triggers con el mismo ID
    // entre la unidad A y la B (les añade un prefijo único por placa).
    device.enableExtendedUniqueIds();

    device.setName(NOMBRE_PLACA);
    device.setSoftwareVersion("1.2.0");

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

        botones[i]->attachLongPressStop([idx](){
            largaFin[idx]->trigger();
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
