// ===========================================================
// PINES_B.H — pines cableados en la UNIDAD B de mega_dispositivos
// Edita este fichero para reflejar lo que tengas cableado
// físicamente en ESTA unidad. Ver PLACA_A/PLACA_B en el .ino para
// saber cuál de los dos ficheros (pines_a.h o pines_b.h) se usa al
// compilar.
// ===========================================================

#ifndef PINES_H
#define PINES_H

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

#endif
