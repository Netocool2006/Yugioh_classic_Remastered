/*
 * Yugioh Classic Remaster (rama classic-ps5-cpu-joey): EL RIVAL ENSENA LA CARTA antes de jugarla desde la mano, como
 * Yu-Gi-Oh! Power of Chaos: Joey the Passion. Decision del usuario: "Como Joey: ensenar la carta".
 *
 * Joey the Passion (Joey_Pc.exe, 30 cuadros/s; decompilado: FUN_0040ad90 = paso 0x19 del duelo, FUN_00418b60 = la
 * carta, FUN_00418ef0 = su dibujo):
 *  - la carta de la mano se da vuelta EN SU LUGAR de la mano (bit 0x8000 de la carta = cara arriba; el volteo corre con
 *    el contador +0x14 de 0 a 0x100 a 0x20 por cuadro: 8 cuadros = 0.267 s, sin agrandarse ni moverse);
 *  - queda en "modo 1" (+0x18 = 1, +0x1c = 0x200 que baja 0x10 por cuadro: 32 cuadros = 1.067 s) dibujada con el marco
 *    de seleccion que parpadea ((+0x1c >> 5) & 1: cambia cada 2 cuadros);
 *  - el motor no sigue hasta que la carta deja de estar ocupada (FUN_0041d050): 8 + 32 = 40 cuadros = 1.33 s; despues
 *    la carta hace su movimiento a la zona.
 * Aca: la carta del rival se levanta en su mano hasta verse entera (nuestra mano del rival esta pegada al borde de
 * arriba), se da vuelta (boca arriba: la cara, derecha para que se lea; boca abajo: no se da vuelta, el dorso, para no
 * revelar lo colocado) y parpadea su marco; el visor de la izquierda muestra la carta (las boca arriba). Despues vuela
 * con el movimiento de Master Duel de siempre. Todo en s de la velocidad Normal (Rapida y R2 lo acortan: el dt del
 * duelo ya va escalado).
 */
#pragma once

namespace cpumuestra {

constexpr float VOLTEO = 8.0f / 30.0f;        // Joey: 8 cuadros (0x100 / 0x20)
constexpr float MUESTRA = 32.0f / 30.0f;      // Joey: 32 cuadros (0x200 / 0x10) con el marco parpadeando
constexpr float TOTAL = VOLTEO + MUESTRA;     // 1.33 s
constexpr float PARPADEO = 2.0f / 30.0f;      // Joey: el marco cambia cada 2 cuadros

}   // namespace cpumuestra
