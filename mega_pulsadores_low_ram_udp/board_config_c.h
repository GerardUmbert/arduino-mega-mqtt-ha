// ===========================================================
// BOARD_CONFIG_C.H — identidad y pines de la UNIDAD C de mega_pulsadores
// Edita este fichero para reflejar los pulsadores que tengas
// cableados físicamente en ESTA unidad. Ver PLACA_A/PLACA_B/PLACA_C en
// el .ino para saber cuál de los tres ficheros (board_config_a.h,
// board_config_b.h o board_config_c.h) se usa al compilar.
//
// Un pin por pulsador. Evita los pines reservados por el shield
// Ethernet:
//   SPI: 50 (MISO), 51 (MOSI), 52 (SCK), 53 (SS)
//   CS del chip Ethernet: normalmente el pin 10
//
// El unique_id de cada pulsador se genera a partir de su número de
// PIN (p. ej. pin 14 → "p14"), no de la posición en esta lista:
// puedes reordenar, insertar o borrar pines libremente sin que ningún
// device trigger ya renombrado en Home Assistant cambie de identidad.
//
// ⚠️ PLACEHOLDER: PINES_BOTONES de abajo es solo de ejemplo — rellénalo
// con el cableado real de esta unidad antes de flashear.
// ===========================================================

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

const char* NOMBRE_PLACA = "Mega Pulsadores C";

// El Mega + shield Ethernet NO trae MAC de fábrica: hay que inventarla.
// Solo debe ser única en tu red local.
// 0x02 en el primer byte = MAC "administrada localmente" (evita
// coincidir por casualidad con una MAC real de fábrica).
// byte[3] = 0x01 identifica la "familia" mega_pulsadores (distinta
// de mega_dispositivos, que usa 0x02) para que nunca choquen entre sí.
// El último byte distingue unidad A (0x00), B (0x01) y C (0x02).
byte mac[] = {0x02, 0x00, 0x00, 0x01, 0x00, 0x02};

// ===========================================================
// IP FIJA de esta unidad.
// Debe estar fuera del rango DHCP de tu router (o reservada para
// esta MAC) para que no choque con otro dispositivo de la red.
// Distinta de la IP de PLACA_A, PLACA_B y de BROKER_ADDR (config.h).
// ===========================================================
const IPAddress IP_ESTATICA(192, 168, 1, 65);

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

// ⚠️ PLACEHOLDER — sustituye por los pines reales de esta unidad.
const uint8_t PINES_BOTONES[] = {
    // TODO: pines reales de esta unidad, p. ej. 2, 3, 4, 5, ...
};

#endif
