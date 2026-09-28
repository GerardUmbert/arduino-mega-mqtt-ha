// ===========================================================
// BOARD_CONFIG_C.H — identidad y pines de la UNIDAD C de mega_dispositivos
// Edita este fichero para reflejar lo que tengas cableado
// físicamente en ESTA unidad. Ver PLACA_A/PLACA_B/PLACA_C en el .ino
// para saber cuál de los tres ficheros (board_config_a.h,
// board_config_b.h o board_config_c.h) se usa al compilar.
//
// ⚠️ PLACEHOLDER: los pines de abajo (PINES_LUCES, PINES_PERSIANAS,
// TIEMPOS_PERSIANAS) son solo de ejemplo — rellénalos con el cableado
// real de esta unidad antes de flashear.
// ===========================================================

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

const char* NOMBRE_PLACA = "Mega Dispositivos C";

// El Mega + shield Ethernet NO trae MAC de fábrica: hay que inventarla.
// Solo debe ser única en tu red local.
// 0x02 en el primer byte = MAC "administrada localmente".
// byte[3] = 0x02 identifica la "familia" mega_dispositivos (distinta
// de mega_pulsadores, que usa 0x01) para que nunca choquen entre sí.
// El último byte distingue unidad A (0x00), B (0x01) y C (0x02).
byte mac[] = {0x02, 0x00, 0x00, 0x02, 0x00, 0x02};

// ===========================================================
// IP FIJA de esta unidad.
// Debe estar fuera del rango DHCP de tu router (o reservada para
// esta MAC) para que no choque con otro dispositivo de la red.
// Distinta de la IP de PLACA_A, PLACA_B y de BROKER_ADDR (config.h).
// ===========================================================
const IPAddress IP_ESTATICA(192, 168, 1, 64);

// ===========================================================
// GATEWAY y MÁSCARA DE SUBRED.
// Imprescindibles: Ethernet.begin(mac, ip) sin más argumentos NO fija
// el gateway real de tu router (asume uno por defecto que puede no
// coincidir con el tuyo), y sin gateway correcto la placa nunca sale
// de tu red aunque la IP parezca asignada correctamente.
// Pon aquí la IP de tu router/gateway real.
// ===========================================================
const IPAddress IP_GATEWAY(192, 168, 150, 254);
const IPAddress IP_SUBNET(255, 255, 255, 0);

// ===========================================================
// LUCES
// Un pin por luz (activa el relé correspondiente).
//
// El unique_id de cada luz se genera a partir de su número de PIN
// (p. ej. pin 22 → "luz_22"), no de la posición en esta lista: puedes
// reordenar, insertar o borrar pines libremente sin que ninguna
// entidad ya renombrada en Home Assistant cambie de identidad.
//
// ⚠️ PLACEHOLDER — sustituye por los pines reales de esta unidad.
// ===========================================================
const uint8_t PINES_LUCES[] = {
    // TODO: pines reales de esta unidad, p. ej. 22, 23, 24, ...
};

// ===========================================================
// PERSIANAS
// Cada persiana usa 2 pines: relé de "subir" y relé de "bajar".
// Nunca deben ir a HIGH los dos a la vez (protección por software en
// RETARDO_INVERSION_MS, en el .ino).
//
// El unique_id de cada persiana incluye ambos pines del par, SIEMPRE
// en el orden subir_bajar (p. ej. {subir: 38, bajar: 39} →
// "persiana_38_39"), nunca al revés.
//
// ⚠️ PLACEHOLDER — sustituye por los pares reales de esta unidad.
// ===========================================================
const ParPines PINES_PERSIANAS[] = {
    // TODO: pares reales de esta unidad, p. ej. {38, 39}, {41, 42}, ...
};

// ===========================================================
// TIEMPOS DE RECORRIDO — uno por persiana, mismo orden/índice que
// PINES_PERSIANAS. Se usan para estimar la posición (0-100%) por
// tiempo de relé activo, ya que estos motores no tienen encoder.
//
// CALIBRA cada persiana por separado: cronometra cuánto tarda en
// hacer el recorrido COMPLETO de un extremo al otro (con margen, mejor
// pasarse un poco de tiempo que quedarse corto) y pon aquí los
// milisegundos reales. subida y bajada pueden ser distintos si el
// motor no tarda lo mismo en los dos sentidos.
//
// ⚠️ PLACEHOLDER — debe tener el mismo número de entradas que
// PINES_PERSIANAS, mismo orden.
// ===========================================================
struct TiempoRecorrido { unsigned long subida_ms; unsigned long bajada_ms; };
const TiempoRecorrido TIEMPOS_PERSIANAS[] = {
    // TODO: un {subida_ms, bajada_ms} por cada persiana de arriba
};

#endif
