// ===========================================================
// BOARD_CONFIG_B.H — identidad y pines de la UNIDAD B de mega_dispositivos
// Edita este fichero para reflejar lo que tengas cableado
// físicamente en ESTA unidad. Ver PLACA_A/PLACA_B en el .ino para
// saber cuál de los dos ficheros (board_config_a.h o board_config_b.h)
// se usa al compilar.
// ===========================================================

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

const char* NOMBRE_PLACA = "Mega Dispositivos B";

// El Mega + shield Ethernet NO trae MAC de fábrica: hay que inventarla.
// Solo debe ser única en tu red local.
// 0x02 en el primer byte = MAC "administrada localmente".
// byte[3] = 0x02 identifica la "familia" mega_dispositivos (distinta
// de mega_pulsadores, que usa 0x01) para que nunca choquen entre sí.
// El último byte distingue unidad A (0x00) de B (0x01).
byte mac[] = {0x02, 0x00, 0x00, 0x02, 0x00, 0x01};

// ===========================================================
// IP FIJA de esta unidad.
// Debe estar fuera del rango DHCP de tu router (o reservada para
// esta MAC) para que no choque con otro dispositivo de la red.
// Distinta de la IP de PLACA_A y de BROKER_ADDR (config.h).
// ===========================================================
const IPAddress IP_ESTATICA(192, 168, 1, 61);

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
// ===========================================================
const uint8_t PINES_LUCES[] = {
    22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35, 36, 37
    // añade más pines aquí si tienes más luces en esta unidad
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
// ===========================================================
const ParPines PINES_PERSIANAS[] = {
    {38, 39}, {41, 42}, {43, 44}, {45, 46},
    {47, 48}, {49, A0},  {A1, A2}, {A3, A4}
    // añade más pares aquí si tienes más persianas en esta unidad
    // (A0-A15 también funcionan como pines digitales normales en el Mega)
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
// ===========================================================
struct TiempoRecorrido { unsigned long subida_ms; unsigned long bajada_ms; };
const TiempoRecorrido TIEMPOS_PERSIANAS[] = {
    {20000, 20000}, {20000, 20000}, {20000, 20000}, {20000, 20000},
    {20000, 20000}, {20000, 20000}, {20000, 20000}, {20000, 20000}
    // un par de valores por cada persiana de arriba, mismo orden
};

#endif
