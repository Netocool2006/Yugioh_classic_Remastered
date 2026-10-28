/*
 * Yugioh Classic Remaster - reglas del duelo (rama classic-historia). SOLO logica y textos: sin Vulkan (el dibujo,
 * con un esquema de la mesa por pagina, esta en tutorial_ui.h dentro de interfaz::Interfaz).
 *
 * Se abre desde elegir rival (Modo Duelo: Historia y Duelo libre) con R1. Navegacion: cruz izq/der o L1/R1 cambian
 * de pagina, Circulo vuelve. Estado global: tutorial::estado() (lo lleva el juego, lo dibuja la interfaz).
 * Textos ES/EN aqui mismo (pares), en el idioma actual con idioma::ingles().
 */
#pragma once

#include <cstdio>

#include "control.h"
#include "idioma.h"

namespace tutorial {

// el esquema que acompanna a cada pagina (tutorial_ui.h)
enum Esquema { E_MESA = 0, E_FASES, E_INVOCAR, E_POSICIONES, E_MAGIAS, E_CADENA, E_BATALLA, E_FUSION, E_MANDO };

struct Texto { const char *es, *en; const char* v() const { return idioma::ingles() ? en : es; } };

struct Pagina {
	Texto titulo;
	Texto puntos[5];   // (vacios al final: es = nullptr)
	int esquema;
};

static const Pagina PAGINAS[] = {
	{ { "EL DUELO", "THE DUEL" },
	  { { "Cada duelista empieza con 8000 LP (puntos de vida) y una mano de 5 cartas.", "Each duelist starts with 8000 LP (Life Points) and a hand of 5 cards." },
	    { "Ganas si los LP del rival llegan a 0, si no puede robar porque su mazo se acabó, o con victorias especiales como Exodia.",
	      "You win if your opponent's LP reach 0, if they can't draw because their deck ran out, or with special wins like Exodia." },
	    { "Tu lado de la mesa: 5 zonas de monstruo, 5 de magia y trampa, la zona de campo, el mazo, el cementerio y el Extra (fusiones).",
	      "Your side of the table: 5 Monster Zones, 5 Spell & Trap Zones, the Field Zone, the Deck, the Graveyard and the Extra Deck (Fusions)." },
	    { "El rival se sienta enfrente, con la misma mesa al revés.", "Your opponent sits across, with the same table mirrored." },
	    { nullptr, nullptr } },
	  E_MESA },
	{ { "TURNOS Y FASES", "TURNS AND PHASES" },
	  { { "Robo: robas la carta de arriba del mazo (también en el primer turno).", "Draw: draw the top card of your deck (also on the first turn)." },
	    { "Principal 1: invocas monstruos, colocas cartas y activas magias.", "Main 1: summon monsters, set cards and activate Spells." },
	    { "Batalla: tus monstruos en ataque pueden atacar una vez cada uno. En el primer turno del duelo no hay batalla.",
	      "Battle: each of your Attack Position monsters can attack once. There is no battle on the very first turn of the duel." },
	    { "Principal 2: otra oportunidad de jugar cartas después de atacar.", "Main 2: another chance to play cards after attacking." },
	    { "Final: el turno pasa al rival. Si tienes más de 6 cartas en la mano, descartas hasta quedarte con 6.",
	      "End: the turn passes to your opponent. If you hold more than 6 cards, discard down to 6." } },
	  E_FASES },
	{ { "INVOCACIÓN Y TRIBUTOS", "SUMMONING AND TRIBUTES" },
	  { { "Una vez por turno puedes hacer una Invocación Normal o colocar un monstruo boca abajo.",
	      "Once per turn you can Normal Summon or Set a monster face-down." },
	    { "Nivel 1 a 4: sin tributos.", "Level 1 to 4: no Tributes." },
	    { "Nivel 5 y 6: sacrifica 1 monstruo tuyo del campo.", "Level 5 and 6: Tribute 1 of your monsters on the field." },
	    { "Nivel 7 o más: sacrifica 2 monstruos.", "Level 7 or higher: Tribute 2 monsters." },
	    { "Los sacrificados van al cementerio y el monstruo nuevo ocupa su lugar.", "Tributed monsters go to the Graveyard and the new monster takes the field." } },
	  E_INVOCAR },
	{ { "POSICIONES", "BATTLE POSITIONS" },
	  { { "Ataque (vertical): el monstruo puede atacar y usa su ATK.", "Attack (vertical): the monster can attack and uses its ATK." },
	    { "Defensa (horizontal): no ataca y usa su DEF para resistir.", "Defense (horizontal): it can't attack and uses its DEF to resist." },
	    { "Colocado boca abajo: el rival no sabe qué es hasta que lo ataca o lo volteas.",
	      "Set face-down: your opponent doesn't know what it is until it's attacked or flipped." },
	    { "Puedes cambiar la posición de cada monstruo una vez por turno (no el turno en que llegó).",
	      "You can change each monster's position once per turn (not on the turn it arrived)." },
	    { nullptr, nullptr } },
	  E_POSICIONES },
	{ { "MAGIAS Y TRAMPAS", "SPELLS AND TRAPS" },
	  { { "Las magias se activan en tu Fase Principal desde la mano o desde el campo.", "Spells are activated during your Main Phase, from the hand or the field." },
	    { "Las trampas se colocan boca abajo y se pueden activar desde el turno siguiente, incluso en el turno del rival.",
	      "Traps are Set face-down and can be activated starting next turn, even during your opponent's turn." },
	    { "Las de Equipo se quedan junto a un monstruo; las de Campo cambian el escenario y van en su propia zona.",
	      "Equip Spells stay attached to a monster; Field Spells change the scenery and go in their own zone." },
	    { "Las magias Rápidas y las trampas sirven para responder: cuando puedes hacerlo, el juego te pregunta.",
	      "Quick-Play Spells and Traps are for responding: when you can, the game asks you." },
	    { nullptr, nullptr } },
	  E_MAGIAS },
	{ { "LA CADENA", "THE CHAIN" },
	  { { "Cuando se activa una carta, cada duelista puede responder con otra: así se forma una cadena.",
	      "When a card is activated, each duelist can respond with another one: that builds a chain." },
	    { "Cada respuesta es un eslabón nuevo (Cadena 1, Cadena 2, ...).", "Each response is a new link (Chain Link 1, Chain Link 2, ...)." },
	    { "La cadena se resuelve al revés: primero el último eslabón y al final el primero.",
	      "The chain resolves backwards: the last link first and the first link last." },
	    { "Ejemplo: el rival ataca, respondes con Fuerza Espejo y destruye sus monstruos en ataque antes del daño.",
	      "Example: your opponent attacks, you respond with Mirror Force and it destroys their attackers before damage." },
	    { nullptr, nullptr } },
	  E_CADENA },
	{ { "BATALLA Y DAÑO", "BATTLE AND DAMAGE" },
	  { { "Ataque contra ataque: el de menos ATK se destruye y su dueño pierde la diferencia en LP. Si empatan, se destruyen los dos.",
	      "Attack vs. Attack: the one with lower ATK is destroyed and its owner loses the difference in LP. On a tie, both are destroyed." },
	    { "Ataque contra defensa: si el ATK supera la DEF, el defensor se destruye sin daño. Si es menor, el atacante pierde la diferencia.",
	      "Attack vs. Defense: if ATK beats DEF, the defender is destroyed with no damage. If it's lower, the attacker loses the difference." },
	    { "Si el rival no tiene monstruos, atacas directo: pierde todo tu ATK en LP.",
	      "If your opponent has no monsters, you attack directly: they lose your full ATK in LP." },
	    { "Cuidado con las cartas boca abajo: pueden ser trampas.", "Watch out for face-down cards: they may be Traps." },
	    { nullptr, nullptr } },
	  E_BATALLA },
	{ { "FUSIÓN Y RITUAL", "FUSION AND RITUAL" },
	  { { "Fusión: activa Polimerización y manda al cementerio los materiales (de la mano o del campo).",
	      "Fusion: activate Polymerization and send the materials (from hand or field) to the Graveyard." },
	    { "El monstruo de fusión sale del Extra y se invoca al instante.", "The Fusion Monster comes out of the Extra Deck and is summoned at once." },
	    { "Ritual: activa la magia de ritual y sacrifica monstruos cuyos niveles sumen al menos el del monstruo de ritual.",
	      "Ritual: activate the Ritual Spell and Tribute monsters whose Levels add up to at least the Ritual Monster's Level." },
	    { "Las dos invocaciones no gastan tu Invocación Normal del turno.", "Neither summon uses up your Normal Summon for the turn." },
	    { nullptr, nullptr } },
	  E_FUSION },
	{ { "EL MANDO", "THE CONTROLLER" },
	  { { nullptr, nullptr } },
	  E_MANDO },
};
constexpr int NUM_PAGINAS = (int)(sizeof(PAGINAS) / sizeof(PAGINAS[0]));

// los botones del mando en el duelo (pagina E_MANDO): icono (iconoBoton de interfaz.h) y texto
struct BotonAyuda { const char* icono; Texto texto; };
static const BotonAyuda BOTONES[] = {
	{ "CRUZ", { "Moverte por la mano y la mesa; elegir en los menús", "Move around your hand and the table; choose in menus" } },
	{ "X", { "Acciones de la carta (invocar, colocar, activar, atacar) y confirmar", "Card actions (summon, set, activate, attack) and confirm" } },
	{ "O", { "Atrás / cancelar; en una pregunta, no activar", "Back / cancel; when asked, don't activate" } },
	{ "T", { "Ver la carta en grande con su texto", "View the card enlarged with its text" } },
	{ "C", { "Pasar a la fase siguiente", "Go to the next phase" } },
	{ "L1R1", { "Cambiar la cámara (cine, tablero, cenital, lateral, libre)", "Change camera (cinema, board, top-down, side, free)" } },
	{ "STICK_R", { "Girar la cámara; L2 / R2 alejan y acercan; R3 la recentra", "Orbit the camera; L2 / R2 zoom out and in; R3 recenters it" } },
	{ "OPTIONS", { "Pausa: continuar, construir mazo, abandonar o salir", "Pause: continue, build deck, forfeit or quit" } },
	{ "TÁCTIL", { "Construir mazo (fuera del duelo)", "Build deck (outside the duel)" } },
};
constexpr int NUM_BOTONES = (int)(sizeof(BOTONES) / sizeof(BOTONES[0]));

struct Estado {
	bool abierto{ false };
	int pagina{ 0 };
	float t{ 0.0f }, tPagina{ 0.0f };
	int dir{ 0 };   // de donde llego la pagina (-1 / +1, para el deslizamiento)

	void abrir()
	{
		abierto = true;
		pagina = 0;
		t = tPagina = 0.0f;
		dir = 0;
		fprintf(stderr, "[tutorial] reglas abiertas\n");
	}
	void cuadro(float dt)
	{
		if (!abierto) return;
		t += dt;
		tPagina += dt;
	}
	// el mando (con el tutorial abierto se queda con todo); devuelve 0 nada, 1 cambio de pagina, 2 cerrado
	int avanzar(uint32_t p)
	{
		if (!abierto) return 0;
		if (p & control::B_CIRCULO) {
			abierto = false;
			fprintf(stderr, "[tutorial] cerrado\n");
			return 2;
		}
		int d = 0;
		if (p & (control::B_DER | control::B_R1 | control::B_X)) d = 1;
		if (p & (control::B_IZQ | control::B_L1)) d = -1;
		const int antes = pagina;
		pagina = pagina + d < 0 ? 0 : pagina + d >= NUM_PAGINAS ? NUM_PAGINAS - 1 : pagina + d;
		if (pagina == antes) return 0;
		dir = d;
		tPagina = 0.0f;
		return 1;
	}
};

inline Estado& estado()
{
	static Estado e;
	return e;
}

}  // namespace tutorial
