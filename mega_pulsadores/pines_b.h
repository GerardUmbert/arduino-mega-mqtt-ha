// ===========================================================
// PINES_B.H — pines cableados en la UNIDAD B de mega_pulsadores
// Edita este fichero para reflejar los pulsadores que tengas
// cableados físicamente en ESTA unidad. Ver PLACA_A/PLACA_B en el
// .ino para saber cuál de los dos ficheros (pines_a.h o pines_b.h)
// se usa al compilar.
//
// Un pin por pulsador. Evita los pines reservados por el shield
// Ethernet:
//   SPI: 50 (MISO), 51 (MOSI), 52 (SCK), 53 (SS)
//   CS del chip Ethernet: normalmente el pin 10
//
// El unique_id de cada pulsador se genera a partir de su número de
// PIN (p. ej. pin 14 → "boton_14"), no de la posición en esta lista:
// puedes reordenar, insertar o borrar pines libremente sin que ningún
// device trigger ya renombrado en Home Assistant cambie de identidad.
// ===========================================================

#ifndef PINES_H
#define PINES_H

const uint8_t PINES_BOTONES[] = {
    2, 3, 4, 5, 6, 7, 8, 9,
    14, 15, 16, 17, 18, 19,
    24, 25, 26, 27, 28, 29
    // añade más pines aquí si tienes más pulsadores en esta unidad
};

#endif
