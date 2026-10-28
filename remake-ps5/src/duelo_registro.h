/*
 * Yugioh Classic Remaster (rama classic-duelo-ui): REGISTRO DEL DUELO (auditoria vs Master Duel, punto 7).
 *
 * El historial de lo que paso en el duelo: invocaciones, colocaciones, activaciones, ataques, danno, destrucciones,
 * cambios de fase y de turno, con el turno y el jugador de cada linea. Sale de los sucesos que ya emite ia::Duelo
 * (ia::Suceso), en el orden en que la mesa los presenta (duelo_consulta.h los pasa a medida que duelo_juego.h los
 * muestra: el registro nunca adelanta lo que todavia no se vio).
 *
 * Solo logica (sin Vulkan): cada linea guarda los datos (tipo, lado, cartas, valores) y el texto se arma al dibujar
 * en el idioma actual (cambiar ES/EN en caliente tambien cambia el registro). Lo dibuja duelo_consulta_ui.h.
 * Las cartas ocultas del rival (lo que roba, lo que coloca) no se nombran.
 */
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ia.h"
#include "idioma.h"

namespace registro {

// el tipo de linea (de que se habla); los colores y el icono los pone la interfaz
enum Clase { R_TURNO = 0, R_FASE, R_INVOCA, R_ACTIVA, R_ATAQUE, R_DANNO, R_DESTRUYE, R_OTRO };

struct Linea {
	int turno{ 0 }, lado{ 0 };
	int tipo{ 0 };                 // ia::TipoSuceso
	int valor{ 0 }, zona2{ -1 };
	bool magia{ false };
	std::string carta, carta2;     // carta2: el objetivo del ataque ("?" boca abajo, "" directo)
	Clase clase() const
	{
		switch (tipo) {
		case ia::S_TURNO: return R_TURNO;
		case ia::S_FASE: return R_FASE;
		case ia::S_INVOCA: case ia::S_COLOCA: case ia::S_VOLTEA: case ia::S_COLOCA_MT: return R_INVOCA;
		case ia::S_ACTIVA_MT: case ia::S_ACTIVA_EFECTO: case ia::S_NEGADO: case ia::S_RESUELVE: return R_ACTIVA;
		case ia::S_ATAQUE: case ia::S_ATAQUE_NEGADO: return R_ATAQUE;
		case ia::S_DANNO: case ia::S_LP: return R_DANNO;
		case ia::S_AL_CEMENTERIO: case ia::S_DESTERRADO: case ia::S_DESCARTA: return R_DESTRUYE;
		default: return R_OTRO;
		}
	}
};

// los sucesos que van al registro (los demas son internos: el combate es el mismo ataque, la cadena se ve en su pila)
inline bool seAnota(const ia::Suceso& s)
{
	switch (s.tipo) {
	case ia::S_COMBATE: case ia::S_CADENA: case ia::S_MODIFICA: case ia::S_RESUELVE: return false;
	default: return true;
	}
}

inline const char* nombreFase(int f, bool en)
{
	static const char* const ES[] = { "Fase de Robo", "Fase Principal 1", "Fase de Batalla", "Fase Principal 2", "Fase Final" };
	static const char* const EN[] = { "Draw Phase", "Main Phase 1", "Battle Phase", "Main Phase 2", "End Phase" };
	if (f < 0 || f > 4) f = 0;
	return en ? EN[f] : ES[f];
}

struct Registro {
	static constexpr size_t MAX_LINEAS = 600;   // (un duelo largo: las mas viejas se descartan)
	std::vector<Linea> lineas;
	int turno{ 0 };
	int nuevas{ 0 };   // lineas que llegaron desde la ultima vez que se miro (la interfaz las marca)

	void limpiar()
	{
		lineas.clear();
		turno = 0;
		nuevas = 0;
	}

	// objetivo: la carta del monstruo atacado como se ve en la mesa ("?" boca abajo, "" ataque directo)
	void anotar(const ia::Suceso& s, const std::string& objetivo = {})
	{
		if (s.tipo == ia::S_TURNO) turno = s.valor;
		if (!seAnota(s)) return;
		Linea l;
		l.turno = turno;
		l.lado = s.lado;
		l.tipo = s.tipo;
		l.valor = s.valor;
		l.zona2 = s.zona2;
		l.magia = s.magia;
		l.carta = s.carta;
		if (s.tipo == ia::S_ATAQUE) l.carta2 = objetivo;
		// (el robo del rival y lo que coloca el rival: ocultos)
		if (s.lado == 1 && (s.tipo == ia::S_ROBO || s.tipo == ia::S_COLOCA || s.tipo == ia::S_COLOCA_MT)) l.carta.clear();
		if (s.tipo == ia::S_ROBO && !lineas.empty() && lineas.back().tipo == ia::S_ROBO && lineas.back().lado == s.lado && lineas.back().turno == turno) {
			lineas.back().valor++;   // (varios robos seguidos: una linea "Roba N cartas")
			if (s.lado == 0) lineas.back().carta2 += (lineas.back().carta2.empty() ? "" : "|") + s.carta;
			return;
		}
		if (s.tipo == ia::S_ROBO) {
			l.valor = 1;
			l.carta2 = s.lado == 0 ? s.carta : std::string();
		}
		lineas.push_back(std::move(l));
		nuevas++;
		if (lineas.size() > MAX_LINEAS) lineas.erase(lineas.begin(), lineas.begin() + (lineas.size() - MAX_LINEAS));
	}

	// el texto de una linea en el idioma actual. nombre: id de carta -> nombre visible
	static std::string texto(const Linea& l, const std::function<std::string(const std::string&)>& nombre)
	{
		const bool en = idioma::ingles();
		auto N = [&](const std::string& id) { return id.empty() ? std::string(en ? "a card" : "una carta") : nombre ? nombre(id) : id; };
		auto T = [&](const char* es, const char* eng) { return std::string(en ? eng : es); };
		switch (l.tipo) {
		case ia::S_TURNO: return (en ? "Turn " : "Turno ") + std::to_string(l.valor);
		case ia::S_FASE: return nombreFase(l.valor, en);
		case ia::S_ROBO:
			if (l.valor > 1) return (en ? "Draws " : "Roba ") + std::to_string(l.valor) + (en ? " cards" : " cartas");
			return l.carta.empty() ? T("Roba una carta", "Draws a card") : T("Roba ", "Draws ") + N(l.carta);
		case ia::S_INVOCA: return T("Invoca a ", "Summons ") + N(l.carta) + (l.valor ? T(" en defensa", " in defense") : std::string());
		case ia::S_COLOCA: return l.carta.empty() ? T("Coloca un monstruo", "Sets a monster") : T("Coloca ", "Sets ") + N(l.carta);
		case ia::S_VOLTEA: return T("Voltea a ", "Flips ") + N(l.carta) + (l.valor == 1 ? T(" (atacado)", " (attacked)") : std::string());
		case ia::S_POSICION:
			return N(l.carta) + (l.valor == 2 ? T(" queda boca abajo", " is turned face-down") : l.valor == 1 ? T(" pasa a defensa", " switches to defense")
			                                                                                                 : T(" pasa a ataque", " switches to attack"));
		case ia::S_COLOCA_MT: return l.carta.empty() ? T("Coloca una magia o trampa", "Sets a spell or trap") : T("Coloca ", "Sets ") + N(l.carta);
		case ia::S_ACTIVA_MT: return T("Activa ", "Activates ") + N(l.carta) + (l.valor >= 1 ? "  [C" + std::to_string(l.valor) + "]" : std::string());
		case ia::S_ACTIVA_EFECTO:
			return T("Activa el efecto de ", "Activates the effect of ") + N(l.carta) + (l.valor >= 1 ? "  [C" + std::to_string(l.valor) + "]" : std::string());
		case ia::S_AL_CEMENTERIO:
			if (!l.magia && l.valor == 1) return N(l.carta) + T(" es destruido en batalla", " is destroyed by battle");
			return N(l.carta) + T(" va al cementerio", " is sent to the Graveyard");
		case ia::S_ATAQUE:
			if (l.zona2 < 0 || l.carta2.empty()) return N(l.carta) + T(" ataca directamente", " attacks directly");
			return N(l.carta) + T(" ataca a ", " attacks ") + (l.carta2 == "?" ? T("un monstruo boca abajo", "a face-down monster") : N(l.carta2));
		case ia::S_DANNO:
			if (l.zona2 == 1) return T("Paga ", "Pays ") + std::to_string(l.valor) + " LP";
			return T("Recibe ", "Takes ") + std::to_string(l.valor) + T(" de daño", " damage");
		case ia::S_LP: return T("Gana ", "Gains ") + std::to_string(l.valor) + " LP";
		case ia::S_FIN: return T("Fin del duelo", "The duel ends");
		case ia::S_NEGADO: return N(l.carta) + T(": negada", ": negated");
		case ia::S_ATAQUE_NEGADO: return T("El ataque es negado", "The attack is negated") + (l.carta.empty() ? std::string() : " (" + N(l.carta) + ")");
		case ia::S_CONTROL: return T("Toma el control de ", "Takes control of ") + N(l.carta);
		case ia::S_A_LA_MANO: return N(l.carta) + T(" vuelve a la mano", " returns to the hand");
		case ia::S_DESTERRADO: return N(l.carta) + T(" queda desterrada", " is banished");
		case ia::S_DESCARTA: return l.zona2 == 1 ? N(l.carta) + T(" vuelve al mazo", " returns to the Deck") : T("Descarta ", "Discards ") + N(l.carta);
		case ia::S_REVELA: return T("Revela ", "Reveals ") + N(l.carta);
		default: return N(l.carta);
		}
	}
};

}  // namespace registro
