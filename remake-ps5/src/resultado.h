/*
 * Yugioh Classic Remaster (rama classic-resultado): los datos de la pantalla de resultado (punto 10a de la auditoria).
 * Sin Vulkan: lo llena el juego (resultado_juego.h, dentro de la clase del juego) y lo dibuja la interfaz
 * (resultado_ui.h, dentro de interfaz::Interfaz). Los dos ven el mismo resultado::resumen().
 *  - El MOTIVO del final (LP a 0, sin cartas en el mazo, Exodia, Destiny Board, Final Countdown, rendicion...).
 *  - El resumen del duelo: turnos, danno hecho y recibido, cartas usadas y la carta MVP (la que mas danno hizo).
 *  - Lo desbloqueado: cartas firma nuevas y el siguiente rival de la historia.
 *  - Las opciones: Revancha / Elegir otro rival / Menu principal.
 * Textos ES/EN con idioma::t (no tocan la tabla de idioma.h).
 */
#pragma once

#include <string>
#include <vector>

#include "idioma.h"

namespace resultado {

enum Motivo {
	M_NINGUNO = 0,
	M_LP,            // los LP del perdedor llegaron a 0
	M_LP_AMBOS,      // empate: los dos a 0 a la vez
	M_SIN_MAZO,      // el perdedor no pudo robar
	M_EXODIA,        // las 5 piezas en la mano del ganador
	M_DESTINY,       // Destiny Board con los 4 mensajes
	M_CUENTA_FINAL,  // Final Countdown
	M_RENDICION,     // el jugador abandono (pausa)
	M_EFECTO,        // otro efecto de carta (Last Turn, Self-Destruct Button...)
	M_RENDICION_RIVAL,   // (rama classic-en-linea) el rival remoto se rindio
	M_TIEMPO,            // (rama classic-en-linea) el reloj del turno se agoto 3 veces
};

enum Opcion { O_REVANCHA = 0, O_OTRO_RIVAL, O_MENU, NUM_OPCIONES };

struct Resumen {
	bool activo{ false };
	int ganador{ -1 };          // 0 jugador, 1 rival, 2 empate
	Motivo motivo{ M_NINGUNO };
	int turnos{ 0 };
	int dannoHecho{ 0 }, dannoRecibido{ 0 };
	int cartasUsadas{ 0 }, invocaciones{ 0 }, activaciones{ 0 };
	std::string mvp;            // la carta del jugador que mas danno de batalla hizo
	int mvpDanno{ 0 };
	std::string cartaClave;     // la carta clave o firma que invoco (sono su musica)
	std::vector<std::string> firmaNuevas;   // ids de las cartas firma desbloqueadas en este duelo
	std::string siguienteEs, siguienteEn;   // el siguiente rival de la historia (vacio: no hay)
	bool revancha{ true };      // hay rival para la revancha
	bool enLinea{ false };      // (rama classic-en-linea) duelo en linea: "Buscar otro rival" en lugar de "Elegir otro rival"
	int opcion{ O_REVANCHA };
};

inline Resumen& resumen()
{
	static Resumen r;
	return r;
}

// el motivo, contado desde el jugador
inline const char* textoMotivo(Motivo m, int ganador)
{
	const bool gano = ganador == 0;
	switch (m) {
	case M_LP: return gano ? idioma::t("Los LP del rival llegaron a 0", "Your opponent's LP reached 0") : idioma::t("Tus LP llegaron a 0", "Your LP reached 0");
	case M_LP_AMBOS: return idioma::t("Los LP de los dos llegaron a 0 a la vez", "Both players' LP reached 0 at the same time");
	case M_SIN_MAZO:
		return gano ? idioma::t("El rival no pudo robar: sin cartas en el mazo", "Your opponent could not draw: no cards left in the deck")
		            : idioma::t("No pudiste robar: sin cartas en el mazo", "You could not draw: no cards left in your deck");
	case M_EXODIA:
		return gano ? idioma::t("¡Exodia! Reuniste las 5 piezas del Prohibido", "Exodia! You assembled all 5 pieces of the Forbidden One")
		            : idioma::t("El rival reunió las 5 piezas de Exodia", "Your opponent assembled all 5 pieces of Exodia");
	case M_DESTINY:
		return gano ? idioma::t("Destiny Board: los 4 mensajes completaron FINAL", "Destiny Board: the 4 messages spelled FINAL")
		            : idioma::t("Destiny Board del rival: los 4 mensajes completaron FINAL", "Your opponent's Destiny Board spelled FINAL");
	case M_CUENTA_FINAL:
		return gano ? idioma::t("Final Countdown: la cuenta llegó al final", "Final Countdown: the count reached its end")
		            : idioma::t("Final Countdown del rival: la cuenta llegó al final", "Your opponent's Final Countdown reached its end");
	case M_RENDICION: return idioma::t("Te rendiste", "You surrendered");
	case M_RENDICION_RIVAL: return idioma::t("El rival se rindió", "Your opponent surrendered");
	case M_TIEMPO:
		return gano ? idioma::t("Se agotó el tiempo del rival 3 veces", "Your opponent ran out of time 3 times")
		            : idioma::t("Se agotó tu tiempo 3 veces", "You ran out of time 3 times");
	case M_EFECTO:
		return ganador == 2 ? idioma::t("Empate por el efecto de una carta", "Draw by a card effect")
		       : gano       ? idioma::t("Ganaste por el efecto de una carta", "You won by a card effect")
		                    : idioma::t("El rival ganó por el efecto de una carta", "Your opponent won by a card effect");
	default: return "";
	}
}

inline const char* textoOpcion(int o, bool enLinea = false)
{
	switch (o) {
	case O_REVANCHA: return idioma::t("Revancha", "Rematch");
	case O_OTRO_RIVAL: return enLinea ? idioma::t("Buscar otro rival", "Find another opponent") : idioma::t("Elegir otro rival", "Choose another opponent");
	default: return idioma::t("Menú principal", "Main menu");
	}
}

inline const char* nombreMotivo(Motivo m)   // (klog)
{
	static const char* const N[] = { "ninguno", "lp", "lp_ambos", "sin_mazo", "exodia", "destiny_board", "final_countdown", "rendicion", "efecto" };
	return N[(int)m];
}

}  // namespace resultado
