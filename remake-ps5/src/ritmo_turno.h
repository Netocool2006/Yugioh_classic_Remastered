/*
 * Yugioh Classic Remaster (rama classic-ps5-ritmo): EL RITMO DEL TURNO. Solo logica (sin Vulkan).
 *
 * Pedido del usuario (PS5): "el timing o velocidad de juego no es como la del juego de Joey the Passion o la de Steam,
 * es demasiado rapida"; "revisa los juegos que ya estan hechos". Las tablas (antes / Master Duel / Joey the Passion /
 * ahora) estan en docs/md_ritmo.md, "Ritmo del turno".
 *
 *  - Pausa de lectura: cuando una jugada visible del rival ya se presento (la mesa al dia), el rival espera LECTURA_MIN
 *    antes de su jugada siguiente (ademas de lo que piensa), asi el jugador alcanza a ver que hizo.
 *  - Carteles de fase: el duelo espera a que termine el cartel (MD: el evento PhaseChangeProcess del Timeline de fase,
 *    a los 0.98 s); antes el rival seguia jugando debajo del cartel.
 *  - Inicio del turno como Master Duel: cartel de turno -> cartel de la Fase de Robo -> el robo (la carta sale del mazo,
 *    se muestra y va a la mano: DrawCardMotionFirst 0.25 + DrawCardMotion 0.28 + muestra + DrawCardToHandMotion 0.65) ->
 *    Fase de Espera -> Fase Principal 1.
 *  - Robo una sola vez: mientras el robo no se presento, la carta no esta en la mano dibujada (antes, al terminar el turno
 *    del rival el motor ya habia robado y cualquier sincronizacion de la mesa la ponia en la mano; despues, en la fase de
 *    robo, se animaba otra vez).
 * Todo en s de la velocidad Normal: el dt del duelo ya va escalado por la velocidad de animaciones y R2 (velocidad.h).
 */
#pragma once

namespace ritmoturno {

// pausa minima entre una jugada visible del rival ya presentada y la siguiente (Joey the Passion: cada accion de la CPU
// con su pausa; MD: el ritmo lo dan las animaciones, ~0.5-1 s visibles entre acciones)
constexpr float LECTURA_MIN = 0.9f;
// el robo de la Fase de Robo del jugador (MD: EffectTaskWaitInput -> DrawOperation): la carta sale del mazo
// (DrawCardMotionFirst 0.25), viene delante de la camara dandose vuelta (DrawCardMotion 0.28), se queda mostrada (MD
// espera un toque, hasta 10 s; aca una pausa fija de lectura) y va a la mano (DrawCardToHandMotion 0.65). El mazo que
// sube al centro y vuelve (DeckCenterMotion / DeckBackMotion 0.40) no se dibuja
constexpr float ROBO_SALE = 0.25f;
constexpr float ROBO_A_CAMARA = 0.28f;
constexpr float ROBO_MUESTRA = 0.50f;
constexpr float ROBO_A_MANO = 0.65f;
constexpr float ROBO_TURNO = ROBO_SALE + ROBO_A_CAMARA + ROBO_MUESTRA + ROBO_A_MANO;   // 1.68 s
// el robo del rival en su Fase de Robo (MD: un CardMove normal Deck -> Hand, Field2Hand 0.45 s, sin mostrarse)
constexpr float ROBO_TURNO_RIVAL = 0.45f;

}   // namespace ritmoturno
