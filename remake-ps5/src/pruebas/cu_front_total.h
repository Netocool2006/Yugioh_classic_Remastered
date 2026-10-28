// Casos de uso DESDE EL FRONT (rama classic-front-total): la bateria jugada como un jugador real, con el mando.
// Pedido del usuario: "Realiza todo el set con la bateria de pruebas que ya hemos hecho antes, pero desde el front".
// Motivo: en la PS5, en el primer turno del primer duelo, una carta invocada se desvanecio (solo quedo la casilla), paso
// sola de ataque a defensa, el rival mando una trampa al cementerio sin aviso y no se pudo pasar a M2 ni terminar el
// turno; los 7088 casos de antes (motor y CU-FE) no lo vieron.
//
// Todo pasa por el juego sin GPU de cu_juego.h (control.h + duelo_juego.h + interfaz logica) y SOLO con botones del
// pad (cruz, circulo, cuadrado, triangulo, flechas, OPTIONS). El jugador automatico (Bot) mira lo que mira un jugador
// (la mesa de ctl, el menu abierto, el foco) para decidir, y mueve el foco con las flechas (busca el camino con una
// copia del mando). Lo unico que se arma por fuera son los ESCENARIOS de la prueba carta por carta (como cu_cartas.h
// y la certificacion: las cartas que hay en la mano y en la mesa antes de jugar).
//
// Vigilantes de cada cuadro (Vigia): la mesa = el motor tras cada animacion; ninguna carta cambia de posicion sin un
// suceso que lo explique (en el motor) ni la mesa sin que el motor cambie, y lo que cambia queda en el registro; toda
// activacion del rival se anuncia (aviso o cadena con el nombre); ninguna carta del rival va al cementerio en tu turno
// sin activacion ni causa; con la decision del jugador nunca queda bloqueado (ni una pregunta sin su interfaz); el
// cuadrado siempre pasa de fase.
//
// Grupos (resumen.txt: CU-FT):
//  - CU-FT-DUELO-<rival>[-<plan>|-T<torneo>]: un duelo completo contra ese rival (personajes del modo historia, rivales
//    del duelo libre, cada plan de la IA y cada mazo de torneo), hasta el final o el tope de turnos.
//  - CU-FT-ACCION-*, CU-FT-MESA-*, CU-FT-POSICION-*, CU-FT-RIVAL-*, CU-FT-ATASCO-*, CU-FT-FASE-*, CU-FT-PREGUNTA-*,
//    CU-FT-NAVEGAR, CU-FT-MENU-*: lo que vigilan los duelos (una comprobacion por accion / cuadro).
//  - CU-FT-CARTA-<id>-JUGAR y CU-FT-CARTA-<id>-EFECTO: cada carta jugada desde la mano o la mesa con el mando.
#pragma once

#include "casos_uso.h"
#include "cu_juego.h"
#include "cu_back.h"
#include "cu_cartas.h"

#include <deque>
#include <functional>
#include <map>
#include <queue>
#include <set>

namespace cuft {

using VD = control::VistaDuelo;
using control::Foco;
using control::F_MANO_J;
using control::F_ZONA;

inline const char* nombreBoton(uint32_t pad)
{
	switch (pad) {
	case PAD_CROSS: return "X";
	case PAD_CIRCLE: return "O";
	case PAD_TRIANGLE: return "T";
	case PAD_SQUARE: return "C";
	case PAD_UP: return "arriba";
	case PAD_DOWN: return "abajo";
	case PAD_LEFT: return "izq";
	case PAD_RIGHT: return "der";
	case PAD_OPTIONS: return "options";
	case PAD_L1: return "l1";
	case PAD_R1: return "r1";
	case PAD_L3: return "l3";
	case PAD_TOUCH_PAD: return "tactil";   // (rama classic-md-fases: el engranaje)
	default: return "?";
	}
}

// lo que se juega con el mando: una jugada que el front entrego al motor (para repetirla en una copia del motor)
struct Grabada { ia::Jugada j; bool ok; int lado; };

struct Bot {
	Juego& g;
	uint32_t azar{ 12345u };
	std::string duelo;          // nombre del duelo / carta en curso (para los detalles)
	std::string contexto{ "INICIO" };   // la accion en curso (grupo de CU-FT-MESA-<contexto>)
	std::deque<std::string> botones;    // los ultimos botones (para reproducir una falla)
	std::string botonesDuelo;           // todos los del duelo / carta en curso (guion YUGI_GUION)
	int nBotones{ 0 };
	// ---- modo carta por carta: el bot contesta las preguntas del rival (y graba todo para repetirlo en el motor)
	bool rivalPorBot{ false }, grabar{ false };
	std::vector<Grabada> grabadas;
	float probResponder{ 0.35f };   // en las preguntas del duelo, cuantas veces activa algo
	std::string cartaPreferida;     // (carta por carta: la que se activa si se ofrece)
	// ---- vigia
	struct Mon { std::string id; bool abajo{ false }, def{ false }; int uid{ -1 }; };
	Mon motorAntes[2][ia::ZONAS];
	std::array<control::Celda, control::NUM_ZONAS_MESA> mesaAntes{};
	size_t sucesosAntes{ 0 }, vistosAntes{ 0 };
	bool alDiaAntes{ false };
	float tBloqueado{ 0 }, tSinUI{ 0 }, tRival{ 0 }, tFrames{ 0 };
	bool atascado{ false };
	struct Pendiente { std::string carta; int lado; float hasta; std::string que; size_t k; };
	std::vector<Pendiente> pendientes;   // cambios de posicion que el registro tiene que mostrar
	std::map<std::string, int> fallasPorCaso;

	explicit Bot(Juego& j) : g(j) { conectar(); }

	uint32_t rnd() { return azar = azar * 1664525u + 1013904223u, azar >> 8; }
	float rnd01() { return (float)(rnd() & 0xFFFF) / 65536.0f; }
	ia::Duelo& d() { return g.dueloIA; }

	// ------------------------------------------------------------------------------------------ cuadros
	bool enDuelo() const { return g.ctl.vista.pantalla == VD::DUELO && !g.ctl.vista.sorteo; }
	bool alDia() const
	{
		return g.sucesosVistos >= g.dueloIA.sucesos.size() && !g.hayAtaque() && !g.ctl.vuelo.activo && !g.ctl.robo.activo && g.reparto <= 0 && !g.mesaPendiente &&
		       g.esperaEfecto <= 0.0f;
	}
	// una comprobacion del caso id; si falla, con lo necesario para reproducirla (los ultimos botones). Como cu::caso pero
	// sin escribir cada falla repetida (un atasco se repite miles de veces)
	bool caso(const std::string& id, bool ok, const char* fmt, ...) __attribute__((format(printf, 4, 5)))
	{
		cu::Resultado& r = cu::de(id);
		r.comprobaciones++;
		if (ok) return true;
		char b[700];
		va_list a;
		va_start(a, fmt);
		vsnprintf(b, sizeof(b), fmt, a);
		va_end(a);
		std::string ult;
		for (const std::string& x : botones) ult += (ult.empty() ? "" : ",") + x;
		const int n = ++fallasPorCaso[id];
		if (n <= 3)
			fprintf(stderr, "FALLA-FT %s [%s, turno %d %s, fase %d, contexto %s]: %s | ultimos botones: %s\n", id.c_str(), duelo.c_str(), g.dueloIA.turno,
			        g.dueloIA.activo == 0 ? "jugador" : "rival", (int)g.dueloIA.fase, contexto.c_str(), b, ult.c_str());
		char d[1000];
		snprintf(d, sizeof(d), "%s [%s, turno %d, %s] botones: %s", b, duelo.c_str(), g.dueloIA.turno, contexto.c_str(), ult.substr(ult.size() > 160 ? ult.size() - 160 : 0).c_str());
		if (r.estado == "OK") {
			r.estado = "FALLA";
			r.detalle = d;
		} else if (r.estado == "FALLA" && r.detalle.size() < 400) {
			r.detalle += std::string(" | ") + d;
		}
		return false;
	}
	// las jugadas que el front entrega al motor (todas pasan por aca: ctl.jugar)
	ia::Jugada ultima;
	bool ultimaOk{ true };
	int nJugadas{ 0 };
	void conectar()
	{
		g.ctl.jugar = [this](const ia::Jugada& j) {
			const bool ok = g.jugarJugador(j);
			ultima = j;
			ultimaOk = ok;
			nJugadas++;
			if (grabar) grabadas.push_back({ j, ok, 0 });
			return ok;
		};
	}

	void reiniciarVigia()
	{
		for (auto& l : motorAntes)
			for (Mon& m : l) m = Mon{};
		mesaAntes = g.ctl.zonas;
		sucesosAntes = g.dueloIA.sucesos.size();
		vistosAntes = g.sucesosVistos;
		tBloqueado = tSinUI = tRival = 0.0f;
		atascado = false;
		pendientes.clear();
		alDiaAntes = false;
		fotoMotor();
	}
	void fotoMotor()
	{
		for (int lado = 0; lado < 2; lado++)
			for (int i = 0; i < ia::ZONAS; i++) {
				const ia::Monstruo& m = g.dueloIA.l[(size_t)lado].m[(size_t)i];
				motorAntes[lado][i] = m.vacia() ? Mon{} : Mon{ m.id, m.bocaAbajo, m.defensa, m.uid };
			}
	}

	// la mesa (lo que se dibuja) contra el motor: "" si coinciden
	std::string difMesa() const
	{
		const ia::Duelo& du = g.dueloIA;
		const control::Mando& c = g.ctl;
		char b[300];
		for (int lado = 0; lado < 2; lado++) {
			const ia::Lado& x = du.l[(size_t)lado];
			const char* q = lado ? "rival" : "jugador";
			for (int i = 0; i < ia::ZONAS; i++) {
				const control::Celda& k = c.zonas[(size_t)Juego::zonaMesaIA(lado, i)];
				const ia::Monstruo& m = x.m[(size_t)i];
				if (k.carta != m.id || (!m.vacia() && (k.bocaAbajo != m.bocaAbajo || k.defensa != m.defensa))) {
					snprintf(b, sizeof(b), "monstruo %s %d: mesa '%s'%s%s, motor '%s'%s%s", q, i, k.carta.c_str(), k.bocaAbajo ? " boca abajo" : "", k.defensa ? " defensa" : " ataque",
					         m.id.c_str(), m.bocaAbajo ? " boca abajo" : "", m.defensa ? " defensa" : " ataque");
					return b;
				}
				const control::Celda& km = c.zonas[(size_t)Juego::zonaMesaIA(lado, i, true)];
				const ia::MagiaTrampa& mt = x.mt[(size_t)i];
				if (km.carta != mt.id || (!mt.vacia() && km.bocaAbajo != mt.bocaAbajo)) {
					snprintf(b, sizeof(b), "magia/trampa %s %d: mesa '%s'%s, motor '%s'%s", q, i, km.carta.c_str(), km.bocaAbajo ? " boca abajo" : "", mt.id.c_str(),
					         mt.bocaAbajo ? " boca abajo" : "");
					return b;
				}
			}
			const control::Celda& kc = c.zonas[(size_t)control::zonaPorId(lado ? "rival_campo" : "jugador_campo")];
			if (kc.carta != x.campo.id) {
				snprintf(b, sizeof(b), "campo %s: mesa '%s', motor '%s'", q, kc.carta.c_str(), x.campo.id.c_str());
				return b;
			}
			if ((lado ? c.cementerioR : c.cementerioJ) != x.cementerio) {
				snprintf(b, sizeof(b), "cementerio %s: mesa %d cartas, motor %d", q, (int)(lado ? c.cementerioR : c.cementerioJ).size(), (int)x.cementerio.size());
				return b;
			}
			if ((lado ? c.desterradosR : c.desterradosJ) != x.desterrados) {
				snprintf(b, sizeof(b), "desterradas %s: mesa %d, motor %d", q, (int)(lado ? c.desterradosR : c.desterradosJ).size(), (int)x.desterrados.size());
				return b;
			}
			if (g.interfazDuelo.lp(lado == 1) != std::max(0, x.lp)) {
				snprintf(b, sizeof(b), "LP %s: pantalla %d, motor %d", q, g.interfazDuelo.lp(lado == 1), x.lp);
				return b;
			}
		}
		if (c.manoJ != du.l[0].mano) {
			snprintf(b, sizeof(b), "mano del jugador: pantalla %d cartas, motor %d", (int)c.manoJ.size(), (int)du.l[0].mano.size());
			return b;
		}
		if (c.manoR != (int)du.l[1].mano.size()) {
			snprintf(b, sizeof(b), "mano del rival: pantalla %d, motor %d", c.manoR, (int)du.l[1].mano.size());
			return b;
		}
		if (du.ganador < 0 && c.vista.turnoRival != (du.activo == 1)) return "la pantalla no muestra de quien es el turno";
		return "";
	}

	// el turno (lado) en que paso el suceso k
	int turnoDe(size_t k) const
	{
		const auto& su = g.dueloIA.sucesos;
		for (size_t j = k + 1; j-- > 0;)
			if (su[j].tipo == ia::S_TURNO) return su[j].lado;
		return 0;
	}
	// la carta magia/trampa del rival que va al cementerio en tu turno: tiene que haber una causa en ese turno
	bool causaCementerio(size_t k) const
	{
		const auto& su = g.dueloIA.sucesos;
		const ia::Suceso& s = su[k];
		for (size_t j = k; j-- > 0;) {
			const ia::Suceso& x = su[j];
			if (x.tipo == ia::S_TURNO) break;
			if ((x.tipo == ia::S_ACTIVA_MT || x.tipo == ia::S_ACTIVA_EFECTO) && (x.lado == 0 || x.carta == s.carta)) return true;
			if ((x.tipo == ia::S_AL_CEMENTERIO || x.tipo == ia::S_DESTERRADO || x.tipo == ia::S_A_LA_MANO || x.tipo == ia::S_CONTROL) && !x.magia && x.lado == s.lado) return true;   // (su equipo)
			if (x.tipo == ia::S_COMBATE) return true;
		}
		return false;
	}

	// despues de cada cuadro: los vigilantes
	void vigilar()
	{
		const float dt = g.frameTimer;
		tFrames += dt;
		if (!enDuelo()) {
			sucesosAntes = g.dueloIA.sucesos.size();
			vistosAntes = g.sucesosVistos;
			mesaAntes = g.ctl.zonas;
			fotoMotor();
			tBloqueado = tSinUI = tRival = 0.0f;
			return;
		}
		ia::Duelo& du = g.dueloIA;
		const auto& su = du.sucesos;
		if (su.size() < sucesosAntes || g.sucesosVistos < vistosAntes) reiniciarVigia();   // (otro duelo)
		// 1) el motor: un monstruo (el mismo) que cambia de posicion tiene un suceso que lo explique en este cuadro
		for (int lado = 0; lado < 2; lado++)
			for (int i = 0; i < ia::ZONAS; i++) {
				const ia::Monstruo& m = du.l[(size_t)lado].m[(size_t)i];
				const Mon& a = motorAntes[lado][i];
				if (m.vacia() || a.id != m.id || a.uid != m.uid || (a.def == m.defensa && a.abajo == m.bocaAbajo)) continue;
				bool causa = false;
				size_t kc = su.size();
				for (size_t k = sucesosAntes; k < su.size(); k++) {
					const ia::Suceso& s = su[k];
					if (s.magia) continue;
					if ((s.tipo == ia::S_POSICION || s.tipo == ia::S_VOLTEA || s.tipo == ia::S_COLOCA || s.tipo == ia::S_INVOCA || s.tipo == ia::S_CONTROL) &&
					    ((s.lado == lado && s.zona == i) || s.carta == m.id))
						causa = true, kc = k;
				}
				caso("CU-FT-POSICION-MOTOR", causa, "el motor cambio %s (%s %d) de %s%s a %s%s sin suceso que lo explique", m.id.c_str(), lado ? "rival" : "jugador", i,
				     a.abajo ? "boca abajo " : "", a.def ? "defensa" : "ataque", m.bocaAbajo ? "boca abajo " : "", m.defensa ? "defensa" : "ataque");
				if (causa) pendientes.push_back({ m.id, lado, -1.0f, m.bocaAbajo != a.abajo ? "volteo" : "posicion", kc });
			}
		// 2) la mesa: la misma carta que cambia de posicion en pantalla, solo si el motor la tiene asi
		for (int z = 0; z < control::NUM_ZONAS_MESA; z++) {
			if (!control::esTipo(z, "monstruo")) continue;
			const control::Celda& k = g.ctl.zonas[(size_t)z];
			const control::Celda& a = mesaAntes[(size_t)z];
			if (k.vacia() || a.carta != k.carta || (a.defensa == k.defensa && a.bocaAbajo == k.bocaAbajo)) continue;
			// (el boca abajo atacado se da vuelta en la mesa durante el ataque aunque el motor ya lo mando al cementerio)
			if (g.hayAtaque() && (z == g.zonaAtacante || z == g.zonaObjetivo) && !k.bocaAbajo && a.bocaAbajo) continue;
			const int lado = control::ZONAS_MESA[z].rival ? 1 : 0;
			int i = -1;
			for (int q = 0; q < ia::ZONAS; q++)
				if (Juego::zonaMesaIA(lado, q) == z) i = q;
			const ia::Monstruo& m = du.l[(size_t)lado].m[(size_t)(i < 0 ? 0 : i)];
			// (rama classic-arreglo-visor: la mesa presenta los sucesos en orden. Si el cambio es el de un suceso presentado
			// en este cuadro y el motor ya saco esa carta de la zona con un suceso posterior todavia sin presentar (Book of
			// Moon del rival y enseguida su ataque la destruye), la mesa va bien: la igualdad final la mira el paso 5)
			bool presentado = false, igualAlSuceso = false;
			for (size_t q = vistosAntes; q < g.sucesosVistos && q < su.size(); q++) {
				const ia::Suceso& s = su[q];
				if (!s.magia && (s.tipo == ia::S_POSICION || s.tipo == ia::S_VOLTEA || s.tipo == ia::S_COLOCA || s.tipo == ia::S_INVOCA || s.tipo == ia::S_CONTROL) &&
				    ((s.lado == lado && s.zona == i) || s.carta == k.carta)) {
					presentado = true;
					// (la mesa quedo exactamente como dice ese suceso: S_POSICION valor 0 ataque, 1 defensa, 2 boca abajo;
					// S_VOLTEA boca arriba)
					if (s.tipo == ia::S_POSICION) igualAlSuceso = k.defensa == (s.valor >= 1) && k.bocaAbajo == (s.valor == 2);
					if (s.tipo == ia::S_VOLTEA) igualAlSuceso = !k.bocaAbajo;
				}
			}
			if (presentado && i >= 0 && m.id != k.carta && g.sucesosVistos < su.size()) continue;
			// (rama classic-arreglo-ciclo-md: con el ritmo de MD la mesa presenta cada paso de una cadena o de un combate:
			// Book of Moon y despues Enemy Controller sobre el mismo monstruo, o un cambio de posicion y el ataque que lo
			// destruye. El paso intermedio va bien si la mesa muestra justo lo de ese suceso y el motor sigue con sucesos
			// sin presentar o el ataque animado; la igualdad final la mira el paso 5)
			if (presentado && igualAlSuceso && i >= 0 && (g.sucesosVistos < su.size() || g.hayAtaque())) continue;
			caso("CU-FT-POSICION-MESA", i >= 0 && m.id == k.carta && m.defensa == k.defensa && m.bocaAbajo == k.bocaAbajo,
			     "la mesa paso %s (%s) de %s%s a %s%s y el motor tiene ahi '%s' %s%s", k.carta.c_str(), control::ZONAS_MESA[z].id, a.bocaAbajo ? "boca abajo " : "",
			     a.defensa ? "defensa" : "ataque", k.bocaAbajo ? "boca abajo " : "", k.defensa ? "defensa" : "ataque", m.id.c_str(), m.bocaAbajo ? "boca abajo " : "", m.defensa ? "defensa" : "ataque");
		}
		// 3) lo presentado en este cuadro: activaciones del rival anunciadas, cartas del rival al cementerio con causa
		for (size_t k = vistosAntes; k < g.sucesosVistos && k < su.size(); k++) {
			const ia::Suceso& s = su[k];
			if ((s.tipo == ia::S_ACTIVA_MT || s.tipo == ia::S_ACTIVA_EFECTO) && s.lado == 1 && du.efectos) {
				const std::string nom = g.interfazDuelo.nombreCarta(s.carta);
				bool visto = g.ctl.aviso.find(nom) != std::string::npos && g.ctl.tAviso < 0.5f;
				for (const cadena::Eslabon& e : g.consulta.pila.v) visto |= e.carta == s.carta && e.lado == 1;
				caso("CU-FT-RIVAL-AVISO", visto, "el rival activo %s (%s) y no se ve: aviso '%s'", s.carta.c_str(), s.tipo == ia::S_ACTIVA_MT ? "magia/trampa" : "efecto",
				     g.ctl.aviso.c_str());
			}
			if (s.tipo == ia::S_AL_CEMENTERIO && s.lado == 1 && s.magia && turnoDe(k) == 0)
				caso("CU-FT-RIVAL-CEMENTERIO", causaCementerio(k) || (g.ctl.aviso.find(g.interfazDuelo.nombreCarta(s.carta)) != std::string::npos && g.ctl.tAviso < 0.5f), "en tu turno %s del rival se fue al cementerio sin activarse ni causa visible", s.carta.c_str());
		}
		// 4) los cambios de posicion quedan en el registro (lo que el jugador puede consultar con L3)
		for (size_t p = 0; p < pendientes.size();) {
			bool ok = false;
			for (const registro::Linea& l : g.consulta.reg.lineas)
				ok |= l.carta == pendientes[p].carta && (l.tipo == ia::S_POSICION || l.tipo == ia::S_VOLTEA || l.tipo == ia::S_COLOCA || l.tipo == ia::S_INVOCA || l.tipo == ia::S_CONTROL);
			if (ok) {
				pendientes.erase(pendientes.begin() + (long)p);
				continue;
			}
			// (el plazo corre desde que la pantalla presento ese suceso: 3 s)
			if (pendientes[p].hasta < 0.0f && g.sucesosVistos > pendientes[p].k) pendientes[p].hasta = tFrames + 3.0f;
			if (pendientes[p].hasta >= 0.0f && tFrames > pendientes[p].hasta) {
				caso("CU-FT-POSICION-VISIBLE", false, "el cambio de %s de %s no aparece en el registro", pendientes[p].que.c_str(), pendientes[p].carta.c_str());
				pendientes.erase(pendientes.begin() + (long)p);
				continue;
			}
			p++;
		}
		// 5) la mesa = el motor cada vez que termina de presentar
		const bool ad = alDia();
		if (ad && !alDiaAntes && du.ganador < 0) {
			const std::string dif = difMesa();
			caso("CU-FT-MESA-" + contexto, dif.empty(), "%s", dif.c_str());
		}
		alDiaAntes = ad;
		// 6) atascos: la decision es del jugador y la pantalla no lo deja jugar; una pregunta sin interfaz; el rival no juega
		if (du.ganador < 0 && !g.pausa.abierta) {
			const bool jugador = du.decide() == 0;
			tBloqueado = jugador && g.ctl.bloqueado ? tBloqueado + dt : 0.0f;
			if (tBloqueado > 20.0f && !atascado) {
				atascado = true;
				caso("CU-FT-ATASCO-JUGADOR", false, "20 s con la decision del jugador y la mesa bloqueada (%s; sucesos %d/%d, ataque %d, vuelo %d, robo %d, espera %.2f, ocupado %d)",
				     g.bloqueoAntes.c_str(), (int)g.sucesosVistos, (int)su.size(), (int)g.hayAtaque(), (int)g.ctl.vuelo.activo, (int)g.ctl.robo.activo, g.esperaEfecto,
				     (int)du.ocupado());
			}
			const bool preg = jugador && du.ef.preg.tipo != ia::P_NADA && du.ef.preg.lado == 0 && !g.ctl.bloqueado;
			tSinUI = preg && g.ctl.estado == control::NAVEGAR ? tSinUI + dt : 0.0f;
			if (tSinUI > 3.0f && !atascado) {
				atascado = true;
				caso("CU-FT-PREGUNTA-SIN-UI", false, "el motor pregunta al jugador (tipo %d, carta %s) y no hay menu ni eleccion en pantalla", (int)du.ef.preg.tipo,
				     du.ef.preg.carta.c_str());
			}
			tRival = !jugador ? tRival + dt : 0.0f;
			if (tRival > 150.0f && !atascado) {
				atascado = true;
				caso("CU-FT-ATASCO-RIVAL", false, "el rival tiene la decision hace 150 s y no juega (pregunta %d, %s)", (int)du.ef.preg.tipo, g.bloqueoAntes.c_str());
			}
		}
		// 7) la pila de la cadena en pantalla (para las pruebas de cadenas: cuantos eslabones, numeros, bajadas)
		{
			const int vivos = g.consulta.pila.vivos();
			if ((int)g.consulta.pila.v.size() > pilaMax) {
				pilaMax = (int)g.consulta.pila.v.size();
				pilaNums.clear();
				for (const cadena::Eslabon& e : g.consulta.pila.v) pilaNums += std::to_string(e.n) + (e.lado ? "r" : "j") + " ";
			}
			if (vivos < pilaVivosAntes) pilaBajadas++;
			pilaVivosAntes = vivos;
		}
		sucesosAntes = su.size();
		vistosAntes = g.sucesosVistos;
		mesaAntes = g.ctl.zonas;
		fotoMotor();
	}
	int pilaMax{ 0 }, pilaBajadas{ 0 }, pilaVivosAntes{ 0 };
	std::string pilaNums;
	void reiniciarPila() { pilaMax = pilaBajadas = pilaVivosAntes = 0, pilaNums.clear(); }

	// antes de cada cuadro (carta por carta): el bot contesta por el rival (pasar / la primera opcion) y lo graba
	void contestarRival()
	{
		ia::Duelo& du = g.dueloIA;
		if (!rivalPorBot || !enDuelo() || du.ganador >= 0 || du.decide() != 1 || du.ef.preg.tipo == ia::P_NADA) return;
		ia::Jugada j;
		j.tipo = du.ef.preg.tipo == ia::P_ELEGIR ? ia::J_ELEGIR : ia::J_PASAR;
		j.objetivo = 0;
		// (cadenas: el rival responde con la carta pedida si se puede)
		if (!rivalActiva.empty() && du.ef.preg.tipo == ia::P_RESPUESTA && (rivalVentana < 0 || (int)du.ef.ventana.tipo == rivalVentana))
			for (const ia::Lugar& f : ia::activables(du, 1, true))
				if (ia::cartaEn(du, f) == rivalActiva) {
					j = activacionDe(du, 1, f);
					rivalActiva.clear();
					break;
				}
		const bool ok = du.aplicar(j);
		if (grabar) grabadas.push_back({ j, ok, 1 });
	}
	std::string rivalActiva;   // (cadenas) la carta con la que el rival responde la proxima vez que pueda
	int rivalVentana{ -1 };    // (rama classic-bp-terminar) si >= 0, rivalActiva solo en esa ventana del motor (ia::V_...)
	// la activacion como en las pruebas del motor: objetivos y descartes, los primeros; costes, los de la IA
	static ia::Jugada activacionDe(const ia::Duelo& d, int lado, const ia::Lugar& f)
	{
		ia::Jugada j = ia::jugadaActivar(f);
		j.carta = ia::cartaEn(d, f);
		const ia::DefCarta* df = ia::defCarta(j.carta);
		if (!df) return j;
		for (int k = 0; k < (int)d.l[(size_t)lado].mano.size() && (int)j.descartes.size() < df->descartes; k++) {
			if (f.donde == ia::EN_MANO && k == f.i) continue;
			if (!ia::descarteVale(d, j.carta, d.l[(size_t)lado].mano[(size_t)k])) continue;
			j.descartes.push_back(k);
		}
		const std::vector<ia::Lugar> objs = ia::objetivosDe(d, lado, *df, f);
		for (int k = 0; k < df->nObj && k < (int)objs.size(); k++) j.objs.push_back(objs[(size_t)k]);
		j.costes = ia::costesIA(d, lado, *df, f);
		return j;
	}
	// un gatillo (L2/R2) o el stick derecho, como en el mando (analogicos)
	void analogico(float& eje, float valor)
	{
		botones.push_back(&eje == &g.mando.l2 ? "l2" : &eje == &g.mando.r2 ? "r2" : "stick_der");
		if (botones.size() > 30) botones.pop_front();
		for (int k = 0; k < (int)(0.1f / g.frameTimer); k++) {
			eje = valor;
			cuadro(0);
		}
		eje = 0.0f;
		esperar(0.15f);
	}

	// (rama classic-en-linea) dos juegos a la vez, cada uno con su bot en su hilo: "antes" espera el turno del cuadro y
	// "despues" se lo pasa al otro (prueba_red_front.cpp); sin ganchos, como siempre
	std::function<void()> antesDeCuadro, despuesDeCuadro;
	void cuadro(uint32_t pad)
	{
		if (antesDeCuadro) antesDeCuadro();
		contestarRival();
		g.cuadro(pad);
		vigilar();
		if (despuesDeCuadro) despuesDeCuadro();
	}
	void esperar(float s)
	{
		for (int k = 0; k < (int)(s / g.frameTimer); k++) cuadro(0);
	}
	void apretar(uint32_t pad, float espera = 0.2f)
	{
		botones.push_back(nombreBoton(pad));
		if (botones.size() > 30) botones.pop_front();
		botonesDuelo += std::string(botonesDuelo.empty() ? "" : ",") + nombreBoton(pad);
		nBotones++;
		for (int k = 0; k < (int)(0.1f / g.frameTimer); k++) cuadro(pad);
		esperar(espera);
	}
	bool esperarQue(float s, const std::function<bool()>& f)
	{
		for (float t = 0.0f; t < s; t += g.frameTimer) {
			if (f()) return true;
			cuadro(0);
		}
		return f();
	}

	// ------------------------------------------------------------------------------------------ navegar con flechas
	// a donde lleva una flecha desde el foco f (en una copia del mando: el de verdad no se toca)
	Foco vecino(const Foco& f, uint32_t b) const
	{
		control::Mando m = g.ctl;
		m.jugar = nullptr;
		m.optionsPendiente = false;   // (rama classic-md-fases: sin el selector de fases ni la fase elegida)
		m.selFases.abierto = false;
		m.faseObjetivo = -1;
		m.log = false;
		m.foco = f;
		m.avanzar(0.0f, b);
		return m.foco;
	}
	bool irA(const Foco& meta)
	{
		for (int intento = 0; intento < 3; intento++) {
			if (g.ctl.foco == meta) return true;
			// camino mas corto (BFS) por las cuatro flechas
			static const uint32_t FL[4] = { control::B_ARRIBA, control::B_ABAJO, control::B_IZQ, control::B_DER };
			static const uint32_t PAD[4] = { PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT };
			std::map<std::pair<int, int>, std::pair<std::pair<int, int>, int>> previo;
			std::queue<Foco> q;
			q.push(g.ctl.foco);
			previo[{ g.ctl.foco.tipo, g.ctl.foco.i }] = { { -9, -9 }, -1 };
			bool llego = false;
			while (!q.empty() && !llego) {
				const Foco f = q.front();
				q.pop();
				for (int k = 0; k < 4; k++) {
					const Foco n = vecino(f, FL[k]);
					if (previo.count({ n.tipo, n.i })) continue;
					previo[{ n.tipo, n.i }] = { { f.tipo, f.i }, k };
					if (n == meta) {
						llego = true;
						break;
					}
					q.push(n);
				}
			}
			if (!llego) return false;
			std::vector<int> pasos;
			for (std::pair<int, int> x{ meta.tipo, meta.i }; previo[x].second >= 0; x = previo[x].first) pasos.push_back(previo[x].second);
			std::reverse(pasos.begin(), pasos.end());
			for (int k : pasos) apretar(PAD[k], 0.05f);
		}
		return g.ctl.foco == meta;
	}

	// en un menu abierto: llevar el cursor a la opcion k y apretar X
	bool elegirOpcion(int k)
	{
		if (g.ctl.estado != control::MENU || k < 0 || k >= (int)g.ctl.opciones.size()) return false;
		const int n = (int)g.ctl.opciones.size();
		for (int v = 0; v < 2 * n && g.ctl.opcion != k; v++) {
			const int abajo = (k - g.ctl.opcion + n) % n;
			apretar(abajo <= n - abajo ? PAD_DOWN : PAD_UP, 0.05f);
		}
		if (g.ctl.opcion != k) return false;
		apretar(PAD_CROSS, 0.15f);
		return true;
	}

	// ------------------------------------------------------------------------------------------ el duelo
	bool libre() const
	{
		const ia::Duelo& du = g.dueloIA;
		return enDuelo() && du.ganador < 0 && du.decide() == 0 && du.activo == 0 && !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && !g.pausa.abierta && alDia();
	}
	// contesta lo que el juego le pide al jugador (pregunta, eleccion, costes y objetivos que se estan armando)
	// hasta volver a navegar. Devuelve false si se quedo dando vueltas
	bool resolverUI(float maximo = 20.0f)
	{
		control::Mando& c = g.ctl;
		for (float t = 0.0f; t < maximo;) {
			if (!enDuelo() || g.dueloIA.ganador >= 0) return true;
			const int st = c.estado;
			if (st == control::NAVEGAR) {
				const bool preg = g.dueloIA.decide() == 0 && g.dueloIA.ef.preg.tipo != ia::P_NADA && g.dueloIA.ef.preg.lado == 0;
				if (!preg && alDia()) return true;
				cuadro(0);
				t += g.frameTimer;
				continue;
			}
			if (st == control::VOLANDO) {
				esperar(0.1f), t += 0.1f;
				continue;
			}
			if (st == control::DETALLE) {
				apretar(PAD_CIRCLE), t += 0.3f;
				continue;
			}
			if (st == control::ELEGIR_TRIBUTOS) {
				int z = -1;
				for (int v : c.validas)
					if (z < 0 && std::find(c.tributos.begin(), c.tributos.end(), v) == c.tributos.end()) z = v;
				if (z < 0 || !irA(Foco{ F_ZONA, z })) {
					caso("CU-FT-NAVEGAR", false, "no se llega con las flechas al sacrificio %d", z);
					apretar(PAD_CIRCLE);
					return false;
				}
				apretar(PAD_CROSS), t += 0.3f;
				continue;
			}
			if (st == control::ELEGIR_ZONA) {
				apretar(PAD_CROSS), t += 0.3f;
				continue;
			}
			if (st == control::ELEGIR_OBJETIVO) {
				// el de menos ATK/DEF de los que se pueden atacar
				int mejor = c.validas.empty() ? -1 : c.validas[0];
				if (!irA(Foco{ F_ZONA, mejor })) caso("CU-FT-NAVEGAR", false, "no se llega con las flechas al objetivo %d", mejor);
				apretar(PAD_CROSS), t += 0.3f;
				continue;
			}
			if (st == control::ELEGIR_CARTA) {
				const Foco meta = c.validasFoco.empty() ? c.foco : c.validasFoco[(size_t)(cartaPreferida.empty() ? 0 : 0)];
				if (!irA(meta)) caso("CU-FT-NAVEGAR", false, "no se llega con las flechas a la carta a elegir (%d %d)", meta.tipo, meta.i);
				const int antes = c.estado;
				const size_t n0 = c.validasFoco.size();
				const int nj = nJugadas;
				apretar(PAD_CROSS), t += 0.3f;
				if (nJugadas > nj && !ultimaOk && g.dueloIA.activo == 1) {
					caso("CU-FT-PREGUNTA-RESPUESTA", false, "el juego rechaza la eleccion del jugador (%s) en el turno del rival: la pregunta vuelve a abrirse", ia::NOMBRE_JUGADA[ultima.tipo]);
					atascado = true;
					return false;
				}
				if (c.estado == antes && c.validasFoco.size() == n0 && c.foco == meta && c.estado == control::ELEGIR_CARTA && !alDia()) esperar(0.2f);
				continue;
			}
			if (st == control::MENU) {
				if (c.enPregunta && !c.eleccionDuelo && !c.armando) {
					// la pregunta del duelo: activar algo (a veces) o "No activar"
					int k = -1;
					for (int o = 0; o < (int)c.opciones.size(); o++) {
						const control::Opcion& op = c.opciones[(size_t)o];
						if (op.accion != control::AC_ACTIVAR_EFECTO || !op.activa) continue;
						if (!cartaPreferida.empty() ? op.jugada.carta == cartaPreferida : (k < 0 && rnd01() < probResponder)) k = o;
					}
					if (k < 0) k = (int)c.opciones.size() - 1;   // (No activar)
					if (k >= 0 && c.opciones[(size_t)k].accion == control::AC_ACTIVAR_EFECTO) contexto = "RESPONDER";
					const int n0 = nJugadas;
					const bool pasa = c.opciones[(size_t)k].accion == control::AC_PASAR;
					elegirOpcion(k);
					t += 0.4f;
					// "No activar" (J_PASAR) siempre tiene que valer: si el juego lo rechaza, nadie puede seguir
					if (nJugadas > n0 && !ultimaOk && (pasa || g.dueloIA.activo == 1)) {
						caso("CU-FT-PREGUNTA-RESPUESTA", false, "el juego rechaza la respuesta del jugador (%s, turno de %s, ventana %d): la pregunta vuelve a abrirse y el duelo no sigue", ia::NOMBRE_JUGADA[ultima.tipo],
						     g.dueloIA.activo == 0 ? "el jugador" : "el rival", (int)g.dueloIA.ef.ventana.tipo);
						atascado = true;
						return false;
					}
					continue;
				}
				if (c.armando || c.eleccionDuelo) {   // lista de candidatos: el primero
					int k = 0;
					while (k < (int)c.opciones.size() - 1 && (c.opciones[(size_t)k].accion == control::AC_CANCELAR || !c.opciones[(size_t)k].activa)) k++;
					elegirOpcion(k);
					t += 0.4f;
					if (c.eleccionDuelo && !ultimaOk && g.dueloIA.activo == 1 && (ultima.tipo == ia::J_ELEGIR || ultima.tipo == ia::J_PASAR)) {
						caso("CU-FT-PREGUNTA-RESPUESTA", false, "el juego rechaza la eleccion del jugador (%s, lista) en el turno del rival: la eleccion vuelve a abrirse", ia::NOMBRE_JUGADA[ultima.tipo]);
						atascado = true;
						return false;
					}
					continue;
				}
				apretar(PAD_CIRCLE), t += 0.3f;   // (un menu que quedo abierto)
				continue;
			}
			cuadro(0);
			t += g.frameTimer;
		}
		atascado = true;
		caso("CU-FT-ATASCO-UI", false, "20 s contestando al juego sin volver a navegar (estado %s, pregunta %d)", control::NOMBRE_ESTADO[c.estado], (int)g.dueloIA.ef.preg.tipo);
		return false;
	}
	// hasta que el jugador pueda jugar, el rival tenga la decision o termine el duelo
	void asentar(float maximo = 30.0f)
	{
		for (float t = 0.0f; t < maximo; t += g.frameTimer) {
			if (!enDuelo() || g.dueloIA.ganador >= 0 || atascado) return;
			if (g.ctl.estado != control::NAVEGAR && g.ctl.estado != control::VOLANDO && !g.ctl.bloqueado) {
				resolverUI();
				continue;
			}
			if (alDia() && g.dueloIA.decide() == 0 && !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && g.dueloIA.ef.preg.tipo == ia::P_NADA) return;
			if (alDia() && g.dueloIA.decide() == 1 && rivalPorBot) {
				cuadro(0);
				continue;
			}
			if (alDia() && g.dueloIA.decide() == 1 && !rivalPorBot && g.dueloIA.activo == 1) return;   // (turno del rival)
			cuadro(0);
		}
	}

	// una accion del menu de una carta (mano o zona): flechas hasta la carta, X, la opcion, X y lo que pida despues
	// Devuelve: 0 no se pudo abrir / no estaba la opcion, 1 hecha, 2 opcion desactivada (se vio el motivo)
	int accion(const Foco& f, const std::function<bool(const control::Opcion&)>& es)
	{
		if (!irA(f)) {
			caso("CU-FT-NAVEGAR", false, "no se llega con las flechas a %s", control::Mando::idDe(f).c_str());
			return 0;
		}
		apretar(PAD_CROSS, 0.15f);
		if (g.ctl.estado != control::MENU) {
			if (g.ctl.estado == control::DETALLE) apretar(PAD_CIRCLE);
			return 0;
		}
		int k = -1;
		for (int o = 0; o < (int)g.ctl.opciones.size() && k < 0; o++)
			if (es(g.ctl.opciones[(size_t)o])) k = o;
		if (k < 0) {
			apretar(PAD_CIRCLE);
			return 0;
		}
		const control::Opcion op = g.ctl.opciones[(size_t)k];
		if (!op.activa) {
			g.ctl.aviso.clear();
			elegirOpcion(k);
			caso("CU-FT-MENU-MOTIVO", g.ctl.estado == control::MENU && !g.ctl.aviso.empty(), "X en '%s' desactivada no mostro el motivo (estado %s)", op.texto.c_str(),
			     control::NOMBRE_ESTADO[g.ctl.estado]);
			if (g.ctl.estado == control::MENU) apretar(PAD_CIRCLE);
			return 2;
		}
		elegirOpcion(k);
		resolverUI();
		return 1;
	}

	// rama classic-md-fases: el selector de fases con el mando: OPTIONS, el cursor hasta la accion k (0 Iniciar Fase de
	// Batalla, 1 Ir a M2, 2 Finalizar turno) y X. false si no se abrio o la accion no es valida (lo deja cerrado)
	bool elegirFase(int k)
	{
		if (!g.ctl.selFases.abierto) apretar(PAD_OPTIONS, 0.1f);
		if (!g.ctl.selFases.abierto) return false;
		if (k < 0 || k >= control::Mando::SEL_FASES || !g.ctl.selFases.valida[(size_t)k]) {
			apretar(PAD_CIRCLE, 0.1f);
			return false;
		}
		for (int i = 0; i < 4 && g.ctl.selFases.opcion != k; i++) apretar(g.ctl.selFases.opcion < k ? PAD_RIGHT : PAD_LEFT, 0.08f);
		if (g.ctl.selFases.opcion != k) {
			apretar(PAD_CIRCLE, 0.1f);
			return false;
		}
		apretar(PAD_CROSS, 0.1f);
		return true;
	}
	// la accion del selector que lleva a la fase siguiente (la primera valida: como el cuadrado de antes)
	int siguienteAccion() const
	{
		const std::array<bool, control::Mando::SEL_FASES> v = g.ctl.fasesValidas();
		for (int k = 0; k < control::Mando::SEL_FASES; k++)
			if (v[(size_t)k]) return k;
		return -1;
	}
	// pasa a la fase siguiente (o termina el turno) con el selector de fases (OPTIONS) en tiempo acotado
	bool pasarFase()
	{
		ia::Duelo& du = g.dueloIA;
		const int t0 = du.turno;
		const ia::Fase f0 = du.fase;
		const std::string c0 = contexto;
		contexto = f0 == ia::F_PRINCIPAL1 ? "FASE-M1" : f0 == ia::F_BATALLA ? "FASE-BP" : f0 == ia::F_PRINCIPAL2 ? "FASE-M2" : "FASE-EP";
		auto cambio = [&] { return du.ganador >= 0 || du.turno != t0 || du.fase != f0; };
		bool ok = false;
		bool elegida = elegirFase(siguienteAccion());
		float reintento = 0.0f;
		// (8 s con la decision del jugador; lo que el rival responde y se anima en el medio, una cadena con sus carteles,
		// no cuenta, hasta 60 s en total)
		float tJugador = 0.0f;
		for (float t = 0.0f; tJugador < 8.0f && t < 60.0f && !ok; t += 0.1f) {
			if (du.decide() == 0 && alDia()) tJugador += 0.1f;
			if (g.ctl.estado != control::NAVEGAR && g.ctl.estado != control::VOLANDO && !g.ctl.bloqueado) resolverUI(5.0f);
			ok = cambio();
			// (OPTIONS no se abre con el duelo ocupado: se vuelve a intentar cuando el jugador queda libre)
			reintento += 0.1f;
			if (!ok && !elegida && g.ctl.faseObjetivo < 0 && reintento > 0.5f && libre()) {
				reintento = 0.0f;
				elegida = elegirFase(siguienteAccion());
			}
			if (!ok) esperar(0.1f);
		}
		char det[300];
		snprintf(det, sizeof(det), "OPTIONS (selector) en %s (turno %d) no paso de fase (8 s con la decision del jugador): ocupado %d, pregunta %d de %d, cadena %d, pendiente %d, estado %s, bloqueado %d, aviso '%s'",
		         Juego::NOMBRE_FASE_IA[f0], t0, (int)du.ocupado(), (int)du.ef.preg.tipo, du.ef.preg.lado, (int)du.ef.cadena.size(), (int)du.ef.pendiente,
		         control::NOMBRE_ESTADO[g.ctl.estado], (int)g.ctl.bloqueado, g.ctl.aviso.c_str());
		const char* id = f0 == ia::F_PRINCIPAL1 ? "CU-FT-FASE-M1" : f0 == ia::F_BATALLA ? "CU-FT-FASE-BP" : f0 == ia::F_PRINCIPAL2 ? "CU-FT-FASE-M2" : "CU-FT-FASE-EP";
		// (en la fase final con la mano llena se descarta primero: no es atasco)
		const bool descarte = f0 == ia::F_FINAL && (int)du.l[0].mano.size() > ia::limiteMano(du, 0);
		if (!descarte) caso(id, ok, "%s", det);
		// (rama classic-ps5-ritmo: como MD, el duelo espera el cartel de la fase nueva, ~1 s; el jugador sigue cuando la
		// mesa queda libre, si no la X de lo que sigue abria el detalle en plena animacion)
		for (float t = 0.0f; ok && du.turno == t0 && t < 4.0f && !(du.decide() == 0 && alDia()); t += 0.1f) esperar(0.1f);
		contexto = c0;
		return ok;
	}

	// ---- lo que el jugador puede hacer ahora (como lo ve en los menus)
	struct Plan { Foco f; control::Accion a; std::string carta, texto; ia::Jugada j; int peso; };
	std::vector<Plan> posibles(bool batalla)
	{
		std::vector<Plan> r;
		control::Mando& c = g.ctl;
		std::vector<Foco> focos;
		for (int k = 0; k < (int)c.manoJ.size(); k++) focos.push_back(Foco{ F_MANO_J, k });
		for (int i = 0; i < ia::ZONAS; i++) {
			focos.push_back(Foco{ F_ZONA, Juego::zonaMesaIA(0, i) });
			focos.push_back(Foco{ F_ZONA, Juego::zonaMesaIA(0, i, true) });
		}
		focos.push_back(Foco{ F_ZONA, control::zonaPorId("jugador_campo") });
		for (const Foco& f : focos) {
			const std::string carta = c.cartaVisible(f);
			for (const control::Opcion& o : c.opcionesPara(f)) {
				if (!o.activa || o.accion == control::AC_CANCELAR || o.accion == control::AC_PASAR) continue;
				int peso = 0;
				switch (o.accion) {
				case control::AC_ATACAR: peso = batalla ? 10 : 0; break;
				case control::AC_JUGADA: peso = batalla ? (o.jugada.tipo == ia::J_ATACAR ? 9 : 0) : (o.jugada.tipo == ia::J_DESCARTAR ? 0 : 9); break;
				case control::AC_INVOCAR:
				case control::AC_TOON: peso = batalla ? 0 : 8; break;
				case control::AC_ACTIVAR_EFECTO:
				case control::AC_ACTIVAR_MAGIA: peso = batalla ? 2 : 6; break;
				case control::AC_COLOCAR_MONSTRUO: peso = batalla ? 0 : 4; break;
				case control::AC_COLOCAR_MT: peso = batalla ? 0 : 4; break;
				case control::AC_VOLTEAR_MONSTRUO: peso = batalla ? 0 : 5; break;
				case control::AC_POSICION: peso = batalla ? 0 : 2; break;
				default: break;
				}
				if (peso > 0) r.push_back({ f, o.accion, carta, o.texto, o.jugada, peso });
			}
		}
		return r;
	}

	static int indiceZona(int z)
	{
		for (int i = 0; i < ia::ZONAS; i++)
			if (Juego::zonaMesaIA(0, i) == z) return i;
		return -9;
	}
	static const char* nombreAccion(const Plan& p)
	{
		switch (p.a) {
		case control::AC_INVOCAR: return "INVOCAR";
		case control::AC_TOON: return "TOON";
		case control::AC_COLOCAR_MONSTRUO: return "COLOCAR";
		case control::AC_ACTIVAR_EFECTO:
		case control::AC_ACTIVAR_MAGIA: return p.f.tipo == F_MANO_J ? "ACTIVAR-MANO" : "ACTIVAR-MESA";
		case control::AC_COLOCAR_MT: return "COLOCAR-MT";
		case control::AC_VOLTEAR_MONSTRUO: return "VOLTEAR";
		case control::AC_POSICION: return "POSICION";
		case control::AC_ATACAR: return "ATACAR";
		case control::AC_JUGADA:
			return p.j.tipo == ia::J_FUSION ? "FUSION" : p.j.tipo == ia::J_RITUAL ? "RITUAL" : p.j.tipo == ia::J_ATACAR ? "ATAQUE-DIRECTO" : "JUGADA";
		default: return "OTRA";
		}
	}

	// hace el plan con el mando y comprueba lo que hizo el motor y lo que muestra la mesa
	void hacer(const Plan& p)
	{
		ia::Duelo& du = g.dueloIA;
		const size_t s0 = du.sucesos.size();
		const std::string que = nombreAccion(p);
		contexto = que;
		const std::string carta = p.a == control::AC_JUGADA && (p.j.tipo == ia::J_FUSION || p.j.tipo == ia::J_RITUAL) ? p.j.carta : p.carta;
		const int r = accion(p.f, [&](const control::Opcion& o) { return o.accion == p.a && o.texto == p.texto; });
		if (r != 1) {
			caso("CU-FT-ACCION-" + que, false, "la opcion '%s' de %s estaba activa y no se pudo elegir con el mando (%d)", p.texto.c_str(), carta.c_str(), r);
			return;
		}
		asentar();
		if (atascado || du.ganador >= 0) return;
		// lo que el motor tiene que haber hecho (el rival puede responder despues: se mira el suceso, no la mesa final)
		bool hecho = false;
		for (size_t k = s0; k < du.sucesos.size(); k++) {
			const ia::Suceso& s = du.sucesos[k];
			if (s.lado != 0) continue;
			switch (p.a) {
			case control::AC_INVOCAR:
			case control::AC_TOON: hecho |= s.tipo == ia::S_INVOCA && s.carta == carta; break;
			case control::AC_COLOCAR_MONSTRUO: hecho |= s.tipo == ia::S_COLOCA && s.carta == carta; break;
			case control::AC_COLOCAR_MT: hecho |= (s.tipo == ia::S_COLOCA_MT || s.tipo == ia::S_COLOCA) && s.carta == carta; break;
			case control::AC_ACTIVAR_EFECTO:
			case control::AC_ACTIVAR_MAGIA: hecho |= (s.tipo == ia::S_ACTIVA_MT || s.tipo == ia::S_ACTIVA_EFECTO || s.tipo == ia::S_INVOCA) && s.carta == carta; break;
			case control::AC_VOLTEAR_MONSTRUO: hecho |= s.tipo == ia::S_VOLTEA && s.carta == carta; break;
			case control::AC_POSICION: hecho |= s.tipo == ia::S_POSICION && s.carta == carta; break;
			case control::AC_ATACAR: hecho |= (s.tipo == ia::S_ATAQUE || s.tipo == ia::S_ATAQUE_NEGADO) && (s.carta == carta || s.zona == indiceZona(p.f.i)); break;
			case control::AC_JUGADA: hecho |= (s.tipo == ia::S_INVOCA || s.tipo == ia::S_ATAQUE || s.tipo == ia::S_ACTIVA_MT) && (s.carta == carta || s.carta == p.carta); break;
			default: hecho = true;
			}
		}
		std::string vistos;
		for (size_t k = s0; k < du.sucesos.size() && vistos.size() < 200; k++)
			vistos += std::string(ia::NOMBRE_SUCESO[du.sucesos[k].tipo]) + ":" + du.sucesos[k].carta + ":" + std::to_string(du.sucesos[k].lado) + " ";
		std::string jug = std::string(ia::NOMBRE_JUGADA[ultima.tipo]) + " mano " + std::to_string(ultima.mano) + " zona " + std::to_string(ultima.zona) + " obj " +
		                  std::to_string(ultima.objetivo) + (ultimaOk ? " ok" : " rechazada") + " objs";
		for (const ia::Lugar& l : ultima.objs) jug += " " + std::to_string(l.lado) + ":" + std::to_string(l.donde) + ":" + std::to_string(l.i);
		jug += " costes";
		for (const ia::Lugar& l : ultima.costes) jug += " " + std::to_string(l.lado) + ":" + std::to_string(l.donde) + ":" + std::to_string(l.i);
		caso("CU-FT-ACCION-" + que, hecho, "'%s' de %s con el mando: el motor no lo hizo (aviso '%s'; ultima jugada %s; sucesos: %s)", p.texto.c_str(), carta.c_str(), g.ctl.aviso.c_str(),
		     jug.c_str(), vistos.c_str());
		// la invocada boca arriba queda en ATAQUE (salvo que algo lo explique: lo mira el vigia de posicion)
		if (hecho && (p.a == control::AC_INVOCAR || p.a == control::AC_TOON))
			for (int i = 0; i < ia::ZONAS; i++) {
				const ia::Monstruo& m = du.l[0].m[(size_t)i];
				if (m.id != carta || m.bocaAbajo) continue;
				const control::Celda& k = g.ctl.zonas[(size_t)Juego::zonaMesaIA(0, i)];
				caso("CU-FT-ACCION-INVOCAR-POSICION", k.carta == carta && !k.bocaAbajo && k.defensa == m.defensa, "%s invocada: mesa %s%s, motor %s", carta.c_str(),
				     k.carta.empty() ? "vacia " : "", k.defensa ? "defensa" : "ataque", m.defensa ? "defensa" : "ataque");
			}
	}

	// un turno del jugador completo: M1 (acciones), BP (ataques), M2 (alguna), fin. Devuelve false si se atasco
	bool turnoJugador(int maxAcciones = 6)
	{
		ia::Duelo& du = g.dueloIA;
		const int t0 = du.turno;
		std::set<std::string> hechos;
		for (int vuelta = 0; vuelta < 60; vuelta++) {
			asentar();
			if (atascado || !enDuelo() || du.ganador >= 0 || du.turno != t0 || du.activo != 0) return !atascado;
			if (!libre()) {
				esperar(0.2f);
				continue;
			}
			if (du.fase == ia::F_ROBO) {
				esperar(0.3f);
				if (du.fase == ia::F_ROBO && libre()) pasarFase();
				continue;
			}
			if (du.fase == ia::F_FINAL) {
				if ((int)du.l[0].mano.size() > ia::limiteMano(du, 0)) {
					contexto = "DESCARTAR";
					const int r = accion(Foco{ F_MANO_J, 0 }, [](const control::Opcion& o) { return o.accion == control::AC_JUGADA && o.jugada.tipo == ia::J_DESCARTAR; });
					caso("CU-FT-ACCION-DESCARTAR", r == 1, "con la mano llena en la fase final no se pudo descartar con el mando");
					continue;
				}
				if (!pasarFase()) return false;
				continue;
			}
			const bool batalla = du.fase == ia::F_BATALLA;
			std::vector<Plan> ps = posibles(batalla);
			std::vector<Plan> nuevos;
			int total = 0;
			for (const Plan& p : ps) {
				const std::string clave = std::to_string((int)du.fase) + "|" + p.carta + "|" + p.texto + "|" + std::to_string(p.f.tipo) + ":" + std::to_string(p.f.i);
				if (hechos.count(clave)) continue;
				nuevos.push_back(p);
				total += p.peso;
			}
			const int nHechos = (int)hechos.size();
			if (nuevos.empty() || (!batalla && nHechos >= maxAcciones) || (du.fase == ia::F_PRINCIPAL2 && nHechos >= maxAcciones + 2)) {
				if (!pasarFase()) {
					// otra vez (el aviso puede ser "Fase (en espera)")
					esperar(1.0f);
					if (du.turno == t0 && du.activo == 0 && !pasarFase()) return false;
				}
				continue;
			}
			int x = (int)(rnd() % (uint32_t)std::max(1, total));
			const Plan* elegido = &nuevos[0];
			for (const Plan& p : nuevos) {
				x -= p.peso;
				if (x < 0) {
					elegido = &p;
					break;
				}
			}
			const Plan p = *elegido;
			hechos.insert(std::to_string((int)du.fase) + "|" + p.carta + "|" + p.texto + "|" + std::to_string(p.f.tipo) + ":" + std::to_string(p.f.i));
			hacer(p);
		}
		return !atascado;
	}

	// ------------------------------------------------------------------------------------------ pantallas
	void soltarPausa()
	{
		if (g.pausa.abierta) apretar(PAD_OPTIONS, 0.2f);
	}
	// del menu principal (o la portada) a la pantalla de elegir rival en el modo pedido
	bool irARivales(int modo)
	{
		VD& v = g.ctl.vista;
		for (int k = 0; k < 6 && v.pantalla == VD::RESULTADO; k++) apretar(PAD_CIRCLE, 0.4f);
		if (v.pantalla == VD::PORTADA) {
			esperar(1.0f);
			apretar(PAD_CROSS, 0.2f);
			esperar(1.5f);
		}
		if (v.pantalla == VD::DUELO) abandonar();
		for (int k = 0; k < 6 && v.pantalla == VD::RESULTADO; k++) apretar(PAD_CIRCLE, 0.4f);
		if (v.pantalla == VD::ELEGIR_RIVAL && v.modo == modo) return true;
		if (v.pantalla == VD::ELEGIR_RIVAL) {
			apretar(PAD_TRIANGLE, 0.2f);
			return v.modo == modo;
		}
		if (v.pantalla != VD::MENU) return false;
		const std::vector<int> ops = g.accionesMenu();
		for (int k = 0; k < 10 && ops[(size_t)g.menuP.opcion] != Juego::MP_DUELO; k++) apretar(PAD_DOWN, 0.15f);
		apretar(PAD_CROSS, 0.3f);
		if (v.pantalla == VD::ELEGIR_RIVAL && v.modo != modo) apretar(PAD_TRIANGLE, 0.2f);
		return v.pantalla == VD::ELEGIR_RIVAL && v.modo == modo;
	}
	void abandonar()
	{
		if (g.ctl.vista.pantalla != VD::DUELO) return;
		esperar(0.2f);
		if (g.ctl.selFases.abierto) apretar(PAD_CIRCLE, 0.1f);
		apretar(PAD_TOUCH_PAD, 0.2f);   // (rama classic-md-fases: el engranaje = menu del duelo)
		if (!g.pausa.abierta) return;
		const std::vector<int> ops = g.opcionesPausa();
		for (int k = 0; k < 8 && ops[(size_t)g.pausa.opcion] != Juego::PA_ABANDONAR; k++) apretar(PAD_DOWN, 0.12f);
		apretar(PAD_CROSS, 0.3f);
		esperar(1.0f);
	}
	// EL INICIO DEL DUELO (quien empieza), en un solo lugar: la moneda estilo Master Duel (rama classic-moneda). Si gana
	// el jugador, ir primero o segundo segun "quieroPrimero"; si gana el rival, decide la IA. Devuelve si el duelo empezo
	bool resolverInicio(bool quieroPrimero)
	{
		const bool segundo = !quieroPrimero;
		VD& v = g.ctl.vista;
		for (int k = 0; k < 40 && v.pantalla == VD::DUELO && v.sorteo; k++) {
			const moneda::Estado& m = v.moneda;
			if (m.etapa == moneda::ORDEN) {
				esperar(0.4f);
				if (segundo != (m.opcion == 1)) apretar(segundo ? PAD_RIGHT : PAD_LEFT, 0.1f);
				apretar(PAD_CROSS, 0.2f);
			} else if (m.etapa == moneda::RESULTADO) {
				esperar(0.4f);
				apretar(PAD_CROSS, 0.2f);
			} else {
				esperar(1.0f);
			}
		}
		return v.pantalla == VD::DUELO && !v.sorteo;
	}
	// las escenas de dialogo del modo historia: X hasta el duelo
	void pasarDialogos()
	{
		for (int k = 0; k < 80 && historia_dialogos::escena().activa(); k++) apretar(PAD_CROSS, 0.25f);
	}

	// ------------------------------------------------------------------------------------------ un duelo completo
	struct ResDuelo { bool empezo{ false }, termino{ false }, atasco{ false }; int ganador{ -1 }, turnos{ 0 }, acciones{ 0 }; };
	ResDuelo jugarDuelo(const std::string& nombre, int maxTurnos, float maxSegundos)
	{
		ResDuelo r;
		duelo = nombre;
		botonesDuelo.clear();
		botones.clear();
		const int n0 = nBotones;
		VD& v = g.ctl.vista;
		r.empezo = resolverInicio(!(rnd() & 1u));
		if (!r.empezo) return r;
		reiniciarVigia();
		contexto = "REPARTO";
		const float t0 = tFrames;
		while (enDuelo() && g.dueloIA.ganador < 0 && !atascado) {
			if (g.dueloIA.turno > maxTurnos || tFrames - t0 > maxSegundos) break;
			if (g.dueloIA.activo == 0 && g.dueloIA.decide() == 0) {
				if (!turnoJugador()) break;
			} else {
				contexto = "RIVAL";
				// el turno del rival: mirar y contestar si pregunta
				if (g.ctl.estado != control::NAVEGAR && g.ctl.estado != control::VOLANDO && !g.ctl.bloqueado) {
					contexto = "RESPONDER";
					resolverUI();
				} else {
					esperar(0.1f);
				}
			}
		}
		esperar(1.0f);
		r.turnos = g.dueloIA.turno;
		r.ganador = g.dueloIA.ganador;
		r.atasco = atascado;
		r.termino = v.pantalla == VD::RESULTADO || g.dueloIA.ganador >= 0;
		r.acciones = nBotones - n0;
		if (v.pantalla == VD::DUELO) abandonar();
		return r;
	}
};

// ------------------------------------------------------------------------------------------------- los rivales
struct Rival {
	int modo;          // VD::HISTORIA o VD::LIBRE
	int saga, idx;     // historia
	int nivel, ri;     // libre (rivalIdx dentro del nivel)
	int plan{ -1 }, torneo{ -1 };   // lo que se busca con el cuadrado (otra semilla); -1 = el que salga
	std::string nombre;
};

// la pantalla de elegir rival hasta el rival pedido, con flechas (y el cuadrado para su plan o mazo de torneo)
inline bool elegirRival(Bot& b, Rival& r)
{
	Juego& g = b.g;
	VD& v = g.ctl.vista;
	if (!b.irARivales(r.modo)) return false;
	if (r.modo == VD::HISTORIA) {
		for (int k = 0; k < 12 && v.saga != r.saga; k++) b.apretar(v.saga < r.saga ? PAD_DOWN : PAD_UP, 0.1f);
		for (int k = 0; k < 20 && v.idx != r.idx; k++) b.apretar(v.idx < r.idx ? PAD_RIGHT : PAD_LEFT, 0.1f);
		if (v.saga != r.saga || v.idx != r.idx) return false;
	} else {
		for (int k = 0; k < 12 && v.nivel != r.nivel; k++) b.apretar(v.nivel < r.nivel ? PAD_UP : PAD_DOWN, 0.1f);
		for (int k = 0; k < 20 && g.rivalIdx != r.ri; k++) b.apretar(PAD_RIGHT, 0.1f);
		if (v.nivel != r.nivel || g.rivalIdx != r.ri) return false;
	}
	// el plan o el mazo de torneo: el cuadrado sortea otra semilla (otro plan / mazo del mismo rival)
	for (int k = 0; k < 250; k++) {
		const bool planOk = r.plan < 0 || g.mazoRivalIA.plan == r.plan;
		const bool torneoOk = r.torneo < 0 ? (r.plan >= 0 || true) : g.mazoRivalIA.torneo == r.torneo;
		if (planOk && torneoOk) break;
		b.apretar(PAD_SQUARE, 0.05f);
		if (r.modo == VD::LIBRE && g.rivalIdx != r.ri) {   // (en el libre el cuadrado puede cambiar de rival: volver)
			for (int q = 0; q < 20 && g.rivalIdx != r.ri; q++) b.apretar(PAD_RIGHT, 0.05f);
		}
	}
	if (r.plan >= 0 && g.mazoRivalIA.plan != r.plan) return false;
	// (el mazo de torneo lo sortea el rival y la variedad no repite el del duelo anterior: si no sale, se juega con el que
	// salio y se anota; no es una falla del juego)
	if (r.torneo >= 0 && g.mazoRivalIA.torneo != r.torneo) {
		fprintf(stderr, "[front-total] %s: el mazo de torneo no salio en 250 sorteos (sale %d): se juega con ese\n", r.nombre.c_str(), g.mazoRivalIA.torneo);
		r.torneo = g.mazoRivalIA.torneo;
	}
	b.apretar(PAD_CROSS, 0.3f);
	b.pasarDialogos();
	return v.pantalla == VD::DUELO;
}

// todos los rivales: personajes (historia), los del duelo libre, cada plan y cada mazo de torneo
inline std::vector<Rival> listaRivales(Juego& g)
{
	std::vector<Rival> r;
	for (int s = 0; s < personajes::NUM_SAGAS; s++) {
		const std::vector<int> fila = g.historia.deSaga(s);
		for (int i = 0; i < (int)fila.size(); i++) {
			const personajes::Personaje& p = g.historia.lista[(size_t)fila[(size_t)i]];
			Rival x{ VD::HISTORIA, s, i, 0, 0, -1, -1, "H-" + p.id };
			r.push_back(x);
			// sus mazos de torneo
			if (const ia::Rival* ri = g.baseIA.rival(p.id))
				for (int t : ri->torneo) {
					Rival y = x;
					y.torneo = t;
					y.nombre = "H-" + p.id + "-T" + g.baseIA.torneos[(size_t)t].clave;
					r.push_back(y);
				}
		}
	}
	for (int n = 1; n <= ia::NIVEL_MAX; n++) {
		const std::vector<const ia::Rival*> del = g.baseIA.deNivel(n);
		for (int i = 0; i < (int)del.size(); i++) {
			Rival x{ VD::LIBRE, 0, 0, n, i, -1, -1, "L" + std::to_string(n) + "-" + del[(size_t)i]->clave };
			r.push_back(x);
			for (int t : del[(size_t)i]->torneo) {
				Rival y = x;
				y.torneo = t;
				y.nombre = x.nombre + "-T" + g.baseIA.torneos[(size_t)t].clave;
				r.push_back(y);
			}
		}
	}
	return r;
}

}  // namespace cuft
