// ===========================================================
// MEGA_DISPOSITIVOS_UDP
// Copia de mega_dispositivos/ (partida de su versión 1.9.1) que,
// además de recibir órdenes de HA por MQTT, ejecuta las reglas de
// udp_reglas.h cuando llega un paquete UDP de mega_pulsadores_low_ram_udp/
// — así luces y persianas responden sin HA ni broker. El resto del
// comportamiento (HASwitch, HACover, posición, timeout) es idéntico.
// Si arreglas algo en mega_dispositivos/, valora si aplica aquí.
// La ArduinoHA parcheada se toma de mega_dispositivos/lib_overrides/.
//
// MEGA_DISPOSITIVOS
// Recibe comandos MQTT de Home Assistant y activa los relés
// de luces y persianas correspondientes.
// No lee ningún pulsador. Solo RECIBE órdenes y las ejecuta.
//
// Mismo firmware para las distintas unidades físicas (A, B, C...): la
// identidad (pines, MAC, IP, nombre) se decide en TIEMPO DE COMPILACIÓN
// con PLACA_A/PLACA_B/PLACA_C (ver más abajo) — no hay jumper físico.
//
// Librerías necesarias (Arduino Library Manager):
//   - ArduinoHA        https://github.com/dawidchyrzynski/arduino-home-assistant
//   - Ethernet (incluida en el IDE si usas shield W5100/W5500)
//
// ⚠️ IMPORTANTE — ArduinoHA PARCHEADA, no la de Library Manager tal cual:
// la HACover original NO acepta comandos de posición desde HA (solo los
// reporta), así que cover.set_cover_position no hace nada sobre estas
// entidades de fábrica. Antes de compilar, copia
// mega_dispositivos/lib_overrides/ArduinoHA/src/device-types/HACover.h
// y HACover.cpp sobre los mismos ficheros dentro de tu carpeta de
// librerías de ArduinoHA instalada (sobrescribiendo los originales) —
// ver mega_dispositivos/lib_overrides/README.md para el paso a paso y
// el motivo exacto del parche.
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

// ===========================================================
// LÓGICA DE LOS RELÉS — punto único de configuración
// Cambia aquí si tu módulo de relés es activo en LOW o en HIGH;
// el resto del código solo debe usar ACTIVO/INACTIVO, nunca
// HIGH/LOW sueltos.
// ===========================================================
#define ACTIVO   HIGH
#define INACTIVO LOW

struct ParPines { uint8_t subir; uint8_t bajar; };

// ===========================================================
// IDENTIFICACIÓN DE LA PLACA — ⚠️ CAMBIAR ANTES DE CADA FLASH ⚠️
// Deja SOLO una de las tres líneas descomentada según a qué unidad
// física vayas a subir este firmware. Selecciona a la vez: los pines
// cableados, la MAC, la IP fija y el nombre en Home Assistant (todo
// en board_config_a.h / board_config_b.h / board_config_c.h). Vuelve
// a compilar y subir tras cambiarla.
// ===========================================================
#define PLACA_A
// #define PLACA_B
// #define PLACA_C

#if (defined(PLACA_A) + defined(PLACA_B) + defined(PLACA_C)) > 1
    #error "Deja solo una de PLACA_A, PLACA_B o PLACA_C descomentada, no varias."
#elif !defined(PLACA_A) && !defined(PLACA_B) && !defined(PLACA_C)
    #error "Descomenta PLACA_A, PLACA_B o PLACA_C para indicar qué unidad es esta."
#endif

#if defined(PLACA_A)
    #include "board_config_a.h"
#elif defined(PLACA_B)
    #include "board_config_b.h"
#elif defined(PLACA_C)
    #include "board_config_c.h"
#endif

// ===========================================================
// RECEPCIÓN DIRECTA POR UDP (sin broker ni Home Assistant)
// Escucha los paquetes UDP broadcast que envía mega_pulsadores_low_ram_udp
// y ejecuta las reglas de udp_reglas.h. Funciona con HA y/o el broker
// caídos; con HA vivo sigue publicando estados como siempre.
//
// Paquete (5 bytes, mismo formato en mega_pulsadores_low_ram_udp):
//   [0] UDP_MAGIC (0xA5)  [1] id placa 'A'/'B'/'C' (de los PULSADORES)
//   [2] pin del pulsador  [3] evento (UDP_EVT_*)  [4] nº de secuencia
//
// ⚠️ Seguridad: cualquier equipo de tu LAN puede enviar estos paquetes
// y mover luces/persianas. UDP_MAGIC solo filtra tráfico ajeno, NO es
// autenticación. Aíslalo en una VLAN si la red no es de confianza.
// ===========================================================
#define HABILITAR_UDP

#define UDP_PUERTO             47800   // el mismo en los pulsadores
#define UDP_MAGIC              0xA5
#define UDP_MAX_PLACAS         3       // 'A', 'B', 'C'
#define UDP_VENTANA_DUPLICADO_MS 500   // misma secuencia dentro de este margen = repetición
#define UDP_TIMEOUT_CONEXION_MQTT_MS 150 // ver nota en mega_pulsadores_low_ram_udp

// Eventos (los manda el pulsador)
#define UDP_EVT_CORTA          1
#define UDP_EVT_DOBLE          2
#define UDP_EVT_LARGA          3
#define UDP_EVT_LARGA_FIN      4

// Acciones (las elige la tabla de reglas). "destino" es:
//   - ACC_LUZ_*:      el número de PIN de la luz (de PINES_LUCES)
//   - ACC_PERSIANA_*: el pin de SUBIR de la persiana (de PINES_PERSIANAS)
#define ACC_LUZ_TOGGLE         1   // enciende si estaba apagada y viceversa
#define ACC_LUZ_ON             2
#define ACC_LUZ_OFF            3
#define ACC_PERSIANA_ABRIR     4   // sube hasta el tope (o hasta recibir PARAR)
#define ACC_PERSIANA_CERRAR    5
#define ACC_PERSIANA_PARAR     6
#define ACC_PERSIANA_TOGGLE    7   // en movimiento -> para; parada -> arranca hacia el lado con más recorrido
#define ACC_PERSIANA_ALTERNAR  8   // arranca en sentido contrario al último (si ya se mueve, invierte).
                                   // Pensado para "larga = mover" + "larga_fin = ACC_PERSIANA_PARAR"

struct ReglaUdp {
    char    placa;    // pulsadores: 'A', 'B' o 'C'. 0 = fin de tabla
    uint8_t pin;      // pin del pulsador en esa placa
    uint8_t evento;   // UDP_EVT_*
    uint8_t accion;   // ACC_*
    uint8_t destino;  // pin de luz / pin "subir" de persiana, en ESTA placa
};
#define FIN_REGLAS { 0, 0, 0, 0, 0 }

// Tabla de reglas de ESTA unidad (una por PLACA_x de dispositivos)
#include "udp_reglas.h"

EthernetUDP udp;
uint8_t       udpUltimaSecuencia[UDP_MAX_PLACAS];
unsigned long udpUltimoMs[UDP_MAX_PLACAS]; // 0 = aún no se ha recibido nada

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
char      idLuz[NUM_LUCES][8];    // buffers de texto: deben vivir todo el programa
char      nombreLuz[NUM_LUCES][10]; // "Luz " + hasta 3 dígitos + '\0'

HACover*  persianas[NUM_PERSIANAS];
char      idPersiana[NUM_PERSIANAS][17];
char      nombrePersiana[NUM_PERSIANAS][20]; // "Persiana " + "XX/YY" + '\0'

// 0 = parada. Distinto de 0 = en movimiento desde ese momento (millis()),
// para poder pararla sola si se pasa de TIEMPO_MAX_MOVIMIENTO_MS y para
// estimar la posición recorrida por tiempo.
unsigned long inicioMovimiento[NUM_PERSIANAS];

// Sentido del movimiento en curso (solo válido mientras inicioMovimiento[i] != 0).
bool subiendo[NUM_PERSIANAS];

// Posición estimada 0-100 (0 = cerrada del todo, 100 = abierta del todo).
// Sin encoder no hay forma de saberla de verdad al arrancar: se asume
// 100 (abierta) hasta que el usuario mueva la persiana a un extremo real,
// momento en el que se resincroniza sola a 0 o 100.
int16_t posicionActual[NUM_PERSIANAS];

// Última vez que se publicó la posición por MQTT mientras se movía
// (evita saturar el broker publicando en cada vuelta de loop()).
unsigned long ultimaPublicacionPosicion[NUM_PERSIANAS];
#define INTERVALO_PUBLICAR_POSICION_MS 500

// Objetivo de un comando "ir a posición X%" (onCoverPositionCommand).
// -1 = sin objetivo: el movimiento en curso (si lo hay) viene de OPEN/CLOSE
// y debe llegar hasta el extremo 0/100 como siempre. 0-100 = parar sola en
// loop() al alcanzar ese valor en vez de seguir hasta el extremo.
int16_t posicionObjetivo[NUM_PERSIANAS];

// ===========================================================
// CALLBACK LUCES
// Busca qué objeto llamó (sender) y actúa sobre el pin de esa
// misma posición en el array.
// ===========================================================
// Único sitio que mueve el relé de una luz — lo usan tanto HA (vía
// onSwitchCommand) como el camino directo UDP.
// Si MQTT está caído, setState() NO publica ni actualiza el estado
// interno de HASwitch (devuelve false en silencio) — por eso el estado
// "real" se lee siempre del pin con digitalRead(), y al reconectar
// sincronizarConHA() lo vuelve a publicar a la fuerza.
void aplicarLuz(int i, bool estado) {
    digitalWrite(PINES_LUCES[i], estado ? ACTIVO : INACTIVO);
    luces[i]->setState(estado); // confirma el estado a HA (si hay MQTT)
    Serial.print(F("[luz] "));
    Serial.print(idLuz[i]);
    Serial.println(estado ? F(" -> ON") : F(" -> OFF"));
}

void onSwitchCommand(bool state, HASwitch* sender) {
    for (int i = 0; i < NUM_LUCES; i++) {
        if (luces[i] == sender) {
            aplicarLuz(i, state);
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
    digitalWrite(PINES_PERSIANAS[i].subir, INACTIVO);
    digitalWrite(PINES_PERSIANAS[i].bajar, INACTIVO);
    inicioMovimiento[i] = 0;
    posicionObjetivo[i] = -1;
    persianas[i]->setPosition(posicionActual[i]);
    persianas[i]->setState(HACover::StateStopped);
    Serial.print(F("[persiana] "));
    Serial.print(idPersiana[i]);
    Serial.print(F(" -> STOP ("));
    Serial.print(motivo);
    Serial.print(F(") pos="));
    Serial.println(posicionActual[i]);
}

// Abrir / cerrar una persiana por índice — extraído de onCoverCommand
// sin cambiar su lógica, para que el camino UDP use exactamente lo mismo
// que HA (misma protección de inversión, misma estimación de posición).
void abrirPersiana(int i) {
    // Si venía moviéndose (p. ej. cerrando) sin haber parado,
    // primero congela la posición real recorrida hasta ahora
    // — si no, se pierde ese tramo y la posición se desincroniza.
    if (inicioMovimiento[i] != 0) posicionActual[i] = posicionEnCurso(i);
    posicionObjetivo[i] = -1; // ir al extremo, no a una posición intermedia
    digitalWrite(PINES_PERSIANAS[i].bajar, INACTIVO);
    delay(RETARDO_INVERSION_MS);
    digitalWrite(PINES_PERSIANAS[i].subir, ACTIVO);
    subiendo[i] = true;
    inicioMovimiento[i] = millis();
    persianas[i]->setState(HACover::StateOpening);
    Serial.print(F("[persiana] "));
    Serial.print(idPersiana[i]);
    Serial.println(F(" -> OPEN"));
}

void cerrarPersiana(int i) {
    if (inicioMovimiento[i] != 0) posicionActual[i] = posicionEnCurso(i);
    posicionObjetivo[i] = -1;
    digitalWrite(PINES_PERSIANAS[i].subir, INACTIVO);
    delay(RETARDO_INVERSION_MS);
    digitalWrite(PINES_PERSIANAS[i].bajar, ACTIVO);
    subiendo[i] = false;
    inicioMovimiento[i] = millis();
    persianas[i]->setState(HACover::StateClosing);
    Serial.print(F("[persiana] "));
    Serial.print(idPersiana[i]);
    Serial.println(F(" -> CLOSE"));
}

void onCoverCommand(HACover::CoverCommand cmd, HACover* sender) {
    for (int i = 0; i < NUM_PERSIANAS; i++) {
        if (persianas[i] == sender) {
            if (cmd == HACover::CommandOpen) {
                abrirPersiana(i);
            } else if (cmd == HACover::CommandClose) {
                cerrarPersiana(i);
            } else if (cmd == HACover::CommandStop) {
                pararPersiana(i, F("orden HA"));
            }
            return;
        }
    }
}

// ===========================================================
// CALLBACK "IR A POSICIÓN X%" (requiere la ArduinoHA parcheada,
// ver aviso al principio del fichero)
// Arranca el motor en la dirección que corresponda y deja que loop()
// la pare sola al llegar al objetivo (misma lógica de fin de recorrido
// que usan OPEN/CLOSE, con posicionObjetivo en vez del extremo 0/100).
// ===========================================================
void onCoverPositionCommand(const int16_t position, HACover* sender) {
    for (int i = 0; i < NUM_PERSIANAS; i++) {
        if (persianas[i] == sender) {
            int16_t objetivo = position;
            if (objetivo > 100) objetivo = 100;
            if (objetivo < 0) objetivo = 0;

            if (inicioMovimiento[i] != 0) posicionActual[i] = posicionEnCurso(i);

            if (objetivo == posicionActual[i]) {
                pararPersiana(i, F("posicion ya alcanzada"));
                return;
            }

            uint8_t pinSubir = PINES_PERSIANAS[i].subir;
            uint8_t pinBajar = PINES_PERSIANAS[i].bajar;
            bool haySubir = objetivo > posicionActual[i];

            posicionObjetivo[i] = objetivo;
            if (haySubir) {
                digitalWrite(pinBajar, INACTIVO);
                delay(RETARDO_INVERSION_MS);
                digitalWrite(pinSubir, ACTIVO);
            } else {
                digitalWrite(pinSubir, INACTIVO);
                delay(RETARDO_INVERSION_MS);
                digitalWrite(pinBajar, ACTIVO);
            }
            subiendo[i] = haySubir;
            inicioMovimiento[i] = millis();
            sender->setState(haySubir ? HACover::StateOpening : HACover::StateClosing);
            Serial.print(F("[persiana] "));
            Serial.print(idPersiana[i]);
            Serial.print(F(" -> SET_POSITION "));
            Serial.println(objetivo);
            return;
        }
    }
}

// ===========================================================
// UDP: buscar destino, ejecutar acción, leer paquetes
// ===========================================================
int indiceLuz(uint8_t pin) {
    for (int i = 0; i < NUM_LUCES; i++) {
        if (PINES_LUCES[i] == pin) return i;
    }
    return -1;
}

// Se identifica la persiana por su pin de SUBIR (el primero del par).
int indicePersiana(uint8_t pinSubir) {
    for (int i = 0; i < NUM_PERSIANAS; i++) {
        if (PINES_PERSIANAS[i].subir == pinSubir) return i;
    }
    return -1;
}

void ejecutarAccion(uint8_t accion, uint8_t destino) {
    if (accion >= ACC_LUZ_TOGGLE && accion <= ACC_LUZ_OFF) {
        int i = indiceLuz(destino);
        if (i < 0) return; // la luz no es de esta unidad: se ignora sin error
        bool estaOn = digitalRead(PINES_LUCES[i]) == ACTIVO;
        bool nuevo = (accion == ACC_LUZ_TOGGLE) ? !estaOn : (accion == ACC_LUZ_ON);
        aplicarLuz(i, nuevo);
        return;
    }

    int i = indicePersiana(destino);
    if (i < 0) return;
    bool enMovimiento = inicioMovimiento[i] != 0;
    switch (accion) {
        case ACC_PERSIANA_ABRIR:  abrirPersiana(i);  break;
        case ACC_PERSIANA_CERRAR: cerrarPersiana(i); break;
        case ACC_PERSIANA_PARAR:  pararPersiana(i, F("UDP")); break;
        case ACC_PERSIANA_TOGGLE:
            if (enMovimiento) pararPersiana(i, F("UDP"));
            else if (posicionActual[i] > 50) cerrarPersiana(i);
            else abrirPersiana(i);
            break;
        case ACC_PERSIANA_ALTERNAR: {
            bool sube = enMovimiento ? !subiendo[i] : (posicionActual[i] <= 50);
            if (sube) abrirPersiana(i); else cerrarPersiana(i);
            break;
        }
    }
}

// Recorre TODA la tabla: varias reglas pueden colgar del mismo
// pulsador+evento (p. ej. una doble pulsación que apaga varias luces).
void aplicarReglas(char placa, uint8_t pin, uint8_t evento) {
    for (uint8_t r = 0; ; r++) {
        ReglaUdp regla;
        memcpy_P(&regla, &REGLAS_UDP[r], sizeof(regla));
        if (regla.placa == 0) break;
        if (regla.placa == placa && regla.pin == pin && regla.evento == evento) {
            ejecutarAccion(regla.accion, regla.destino);
        }
    }
}

// Lee como mucho 4 paquetes por vuelta de loop() para no dejar
// sin atender a MQTT ni a las persianas si llega una ráfaga.
void procesarUdp() {
    for (uint8_t n = 0; n < 4; n++) {
        int tam = udp.parsePacket(); // descarta solo lo que sobre del paquete anterior
        if (tam <= 0) return;

        uint8_t p[5];
        int leidos = udp.read(p, sizeof(p));
        if (tam != 5 || leidos != 5 || p[0] != UDP_MAGIC) continue;

        char placa = (char)p[1];
        if (placa < 'A' || placa >= 'A' + UDP_MAX_PLACAS) continue;
        uint8_t k = placa - 'A';

        // El pulsador manda cada paquete 2 veces: la repetición trae la
        // misma secuencia y llega enseguida — se descarta.
        unsigned long ahora = millis();
        if (udpUltimoMs[k] != 0 && p[4] == udpUltimaSecuencia[k]
                && ahora - udpUltimoMs[k] < UDP_VENTANA_DUPLICADO_MS) {
            continue;
        }
        udpUltimaSecuencia[k] = p[4];
        udpUltimoMs[k] = ahora;

        Serial.print(F("[udp] "));
        Serial.print(placa);
        Serial.print(F(" p"));
        Serial.print(p[2]);
        Serial.print(F(" evento="));
        Serial.println(p[3]);
        aplicarReglas(placa, p[2], p[3]);
    }
}

// Tras una caída de MQTT los estados que publica HA pueden estar
// desfasados (las órdenes por UDP no pudieron publicarse). Al reconectar
// se vuelve a publicar el estado real, con force = true.
void sincronizarConHA() {
    for (int i = 0; i < NUM_LUCES; i++) {
        luces[i]->setState(digitalRead(PINES_LUCES[i]) == ACTIVO, true);
    }
    for (int i = 0; i < NUM_PERSIANAS; i++) {
        int16_t pos = posicionEnCurso(i);
        HACover::CoverState estado;
        if (inicioMovimiento[i] != 0) {
            estado = subiendo[i] ? HACover::StateOpening : HACover::StateClosing;
        } else if (pos <= 0) {
            estado = HACover::StateClosed;
        } else if (pos >= 100) {
            estado = HACover::StateOpen;
        } else {
            estado = HACover::StateStopped;
        }
        persianas[i]->setPosition(pos, true);
        persianas[i]->setState(estado, true);
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
    sincronizarConHA();
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

    // Evita que HA confunda entidades con el mismo ID entre unidades
    device.enableExtendedUniqueIds();

    device.setName(NOMBRE_PLACA);
    device.setSoftwareVersion("0.1.0-udp");

    // --- luces: se crean y configuran en bucle ---
    for (int i = 0; i < NUM_LUCES; i++) {
        pinMode(PINES_LUCES[i], OUTPUT);
        digitalWrite(PINES_LUCES[i], INACTIVO); // arrancan apagadas

        snprintf(idLuz[i], sizeof(idLuz[i]), "luz_%d", PINES_LUCES[i]);
        luces[i] = new HASwitch(idLuz[i]);
        luces[i]->onCommand(onSwitchCommand);
        luces[i]->setIcon("mdi:lightbulb");

        // Nombre visible en HA (unique_id/idLuz sigue siendo luz_XX, esto
        // solo cambia lo que se ve — renómbralo luego en HA si quieres).
        snprintf(nombreLuz[i], sizeof(nombreLuz[i]), "Luz %d", PINES_LUCES[i]);
        luces[i]->setName(nombreLuz[i]);
    }

    // --- persianas: idem ---
    for (int i = 0; i < NUM_PERSIANAS; i++) {
        pinMode(PINES_PERSIANAS[i].subir, OUTPUT);
        pinMode(PINES_PERSIANAS[i].bajar, OUTPUT);
        digitalWrite(PINES_PERSIANAS[i].subir, INACTIVO);
        digitalWrite(PINES_PERSIANAS[i].bajar, INACTIVO);

        // Orden fijo subir_bajar en el ID, no alfabético ni el que sea menor.
        snprintf(idPersiana[i], sizeof(idPersiana[i]), "persiana_%d_%d", PINES_PERSIANAS[i].subir, PINES_PERSIANAS[i].bajar);
        // PositionFeature: sin esto, HA colapsa "stopped" a open/closed
        // (no existe un estado "parada a medias" sin posición real).
        persianas[i] = new HACover(idPersiana[i], HACover::PositionFeature);
        persianas[i]->setDeviceClass("shutter");
        persianas[i]->onCommand(onCoverCommand);
        persianas[i]->onPositionCommand(onCoverPositionCommand);

        // Nombre visible en HA (unique_id/idPersiana sigue siendo
        // persiana_XX_YY, esto solo cambia lo que se ve).
        snprintf(nombrePersiana[i], sizeof(nombrePersiana[i]), "Persiana %d/%d", PINES_PERSIANAS[i].subir, PINES_PERSIANAS[i].bajar);
        persianas[i]->setName(nombrePersiana[i]);

        // Sin encoder no hay forma de saber la posición real al arrancar:
        // se asume abierta del todo hasta que el usuario la lleve a un
        // extremo real y se resincronice sola.
        posicionActual[i] = 100;
        posicionObjetivo[i] = -1;
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

    if (udp.begin(UDP_PUERTO)) {
        Serial.print(F("[boot] escuchando UDP en el puerto "));
        Serial.println(UDP_PUERTO);
    } else {
        Serial.println(F("[boot] ERROR: sin socket UDP libre, no habra control directo"));
    }
    // Con el broker caído, cada reintento de conexión bloquea loop() hasta
    // este tiempo (default de Ethernet: 1000 ms), retrasando las órdenes
    // UDP. Requiere Ethernet >= 2.0.1.
    client.setConnectionTimeout(UDP_TIMEOUT_CONEXION_MQTT_MS);

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
    // UDP antes Y después de mqtt.loop(): si MQTT está reconectando,
    // mqtt.loop() puede bloquear; así una orden no espera a la siguiente vuelta.
    procesarUdp();
    mqtt.loop();
    procesarUdp();

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

        // Con objetivo (comando "ir a X%"): parar al alcanzarlo, sin llegar
        // necesariamente a 0/100 ni cambiar a estado Closed/Open (se queda
        // "stopped" a medio camino, igual que un STOP manual).
        if (posicionObjetivo[i] >= 0) {
            bool llegoAlObjetivo = subiendo[i] ? (posicion >= posicionObjetivo[i])
                                                : (posicion <= posicionObjetivo[i]);
            if (llegoAlObjetivo) {
                posicionActual[i] = posicionObjetivo[i];
                posicionObjetivo[i] = -1;
                digitalWrite(PINES_PERSIANAS[i].subir, INACTIVO);
                digitalWrite(PINES_PERSIANAS[i].bajar, INACTIVO);
                inicioMovimiento[i] = 0;
                persianas[i]->setPosition(posicionActual[i]);
                persianas[i]->setState(HACover::StateStopped);
                Serial.print(F("[persiana] "));
                Serial.print(idPersiana[i]);
                Serial.print(F(" -> STOP (posicion alcanzada) pos="));
                Serial.println(posicionActual[i]);
                continue;
            }
        } else {
            // Sin objetivo (OPEN/CLOSE normal): llegó sola al extremo hacia
            // el que se estaba moviendo (recorrido completo calibrado):
            // parar y resincronizar a 0/100 exactos, con el estado final
            // correcto (abierta/cerrada, no "stopped").
            // OJO: comprobar solo el extremo de la dirección actual — si
            // no, p. ej. un CLOSE que arranca desde posicionActual=100 lee
            // 100 en la primera vuelta de loop() (aún no ha avanzado) y se
            // confunde con "ya llegó a abierta del todo".
            bool llegoAlExtremo = subiendo[i] ? (posicion >= 100) : (posicion <= 0);
            if (llegoAlExtremo) {
                posicionActual[i] = posicion <= 0 ? 0 : 100;
                digitalWrite(PINES_PERSIANAS[i].subir, INACTIVO);
                digitalWrite(PINES_PERSIANAS[i].bajar, INACTIVO);
                inicioMovimiento[i] = 0;
                persianas[i]->setPosition(posicionActual[i]);
                persianas[i]->setState(posicionActual[i] == 0 ? HACover::StateClosed : HACover::StateOpen);
                Serial.print(F("[persiana] "));
                Serial.print(idPersiana[i]);
                Serial.println(posicionActual[i] == 0 ? F(" -> CLOSED (fin recorrido)") : F(" -> OPEN (fin recorrido)"));
                continue;
            }
        }

        // Mientras se mueve, publica la posición estimada cada cierto
        // intervalo (no en cada vuelta de loop(), para no saturar el broker).
        if (millis() - ultimaPublicacionPosicion[i] > INTERVALO_PUBLICAR_POSICION_MS) {
            ultimaPublicacionPosicion[i] = millis();
            persianas[i]->setPosition(posicion);
        }
    }
}
