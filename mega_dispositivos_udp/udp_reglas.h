// ===========================================================
// UDP_REGLAS.H — qué hace cada pulsador (por UDP, sin HA)
//
// Una fila = "cuando el pulsador PIN de la placa de pulsadores PLACA
// manda EVENTO, haz ACCION sobre DESTINO". Una misma combinación
// pulsador+evento puede tener VARIAS filas (todas se ejecutan).
//
//   { placa, pin_pulsador, UDP_EVT_*, ACC_*, destino }
//
//   placa    'A', 'B' o 'C': la letra PLACA_x de mega_pulsadores_low_ram_udp
//            (NO la de esta unidad de dispositivos)
//   pin      el mismo número que el "pNN" de ese pulsador en HA
//   destino  luces:     el PIN de la luz (de PINES_LUCES de esta unidad)
//            persianas: el pin de SUBIR de la persiana (de PINES_PERSIANAS)
//            Si el destino no existe en esta unidad la fila se ignora, así
//            que cada unidad puede tener solo las filas que le tocan.
//
// Eventos que existen (AceButton no tiene triple/cuádruple/quíntuple):
//   UDP_EVT_CORTA      un toque
//   UDP_EVT_DOBLE      dos toques seguidos
//   UDP_EVT_LARGA      al empezar la pulsación larga (~800 ms sujeto)
//   UDP_EVT_LARGA_FIN  al soltar tras una pulsación larga
//
// Acciones: ACC_LUZ_TOGGLE / ACC_LUZ_ON / ACC_LUZ_OFF
//           ACC_PERSIANA_ABRIR / _CERRAR / _PARAR / _TOGGLE / _ALTERNAR
//
// Patrones habituales:
//   - Luz con un pulsador:   corta -> ACC_LUZ_TOGGLE
//   - "Apagar todo":         doble -> varias filas ACC_LUZ_OFF
//   - Persiana "mantener":   larga -> ACC_PERSIANA_ALTERNAR,
//                            larga_fin -> ACC_PERSIANA_PARAR
//   - Persiana de un toque:  corta -> ACC_PERSIANA_TOGGLE
//
// La tabla vive en flash (PROGMEM), no gasta RAM. Termina SIEMPRE con
// FIN_REGLAS. Rellena las tablas con TU cableado: los ejemplos de abajo
// están comentados y los pines son inventados.
//
// ⚠️ Si activas el modo UDP_MODO_SIEMPRE en los pulsadores y además
// tienes blueprints de HA para esos mismos pulsadores, ambos actuarán.
// ===========================================================

#ifndef UDP_REGLAS_H
#define UDP_REGLAS_H

#if defined(PLACA_A)
const ReglaUdp REGLAS_UDP[] PROGMEM = {
    // { 'A', 14, UDP_EVT_CORTA,     ACC_LUZ_TOGGLE,        22 },
    // { 'A', 14, UDP_EVT_DOBLE,     ACC_LUZ_OFF,           22 },
    // { 'A', 14, UDP_EVT_DOBLE,     ACC_LUZ_OFF,           23 },
    // { 'A', 15, UDP_EVT_LARGA,     ACC_PERSIANA_ALTERNAR, 38 },
    // { 'A', 15, UDP_EVT_LARGA_FIN, ACC_PERSIANA_PARAR,    38 },
    FIN_REGLAS
};
#elif defined(PLACA_B)
const ReglaUdp REGLAS_UDP[] PROGMEM = {
    FIN_REGLAS
};
#elif defined(PLACA_C)
const ReglaUdp REGLAS_UDP[] PROGMEM = {
    FIN_REGLAS
};
#endif

#endif
