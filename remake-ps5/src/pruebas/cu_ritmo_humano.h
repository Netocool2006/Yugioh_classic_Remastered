// Rama classic-ps5-ritmo: EL CRONISTA del jugador humano (cu_humano.h). Mide, cuadro a cuadro y con lo que la pantalla
// dibuja (pantalla_sin_gpu.h), cuanto dura de verdad cada cosa de un turno (cartel de turno, carteles de fase, robo, cada
// jugada del rival: pensar + presentarse + pausa antes de la siguiente, ataques) y juzga el robo y el ritmo.
//
// Pedido del usuario (PS5): "el timing o velocidad de juego no es como la del juego de Joey the Passion o la de Steam, es
// demasiado rapida"; "cuando esta a punto de terminar el turno del CPU me das la carta del mazo a mi mano y luego cuando
// ya me toca me la vuelves a dar". Las pruebas no lo veian: nadie miraba la mano ANTES de la animacion del robo.
//
// Oraculos (docs/md_ritmo.md, "Ritmo del turno"):
//  - CU-HU-ROBO-UNA-VEZ: cada carta robada se anima una sola vez y NO aparece en la mano dibujada (la del jugador: su
//    cara; la del rival: un dorso de mas) antes de que empiece su animacion de robo.
//  - CU-HU-ROBO-TRAS-CARTEL: el robo del inicio del turno se anima despues del cartel de turno (MD: EffectTaskTurnChange
//    espera su Timeline y despues viene la Fase de Robo).
//  - CU-HU-RITMO-LECTURA: entre una jugada visible del rival ya presentada (la mesa al dia) y su jugada siguiente pasan
//    al menos ritmoturno::LECTURA_MIN s: el jugador alcanza a ver que hizo.
//  - CU-HU-RITMO-CARTEL-FASE: mientras se ve un cartel de fase el rival no juega (MD: PhaseChangeProcess a los 0.98 s).
#pragma once

#include "cu_humano.h"
#include "../ritmo_turno.h"

namespace hu {

struct Cronista {
	Juego& g;
	pantalla::Pantalla& p;
	Juez& j;
	Cronista(Juego& gg, pantalla::Pantalla& pp, Juez& jj) : g(gg), p(pp), j(jj) {}

	// ---------------------------------------------------------------------------------------- lo que se mide
	struct Marca { float t; std::string que; };
	std::vector<Marca> linea;   // la cronologia (para el informe)
	FILE* log{ nullptr };
	void anotar(const char* fmt, ...) __attribute__((format(printf, 2, 3)))
	{
		char b[300];
		va_list a;
		va_start(a, fmt);
		vsnprintf(b, sizeof(b), fmt, a);
		va_end(a);
		linea.push_back({ j.t, b });
		if (log) fprintf(log, "%8.2f  %s\n", j.t, b);
	}
	// acumulados (s) para la tabla
	struct Suma {
		double s{ 0 };
		int n{ 0 };
		double max{ 0 }, min{ 1e9 };
		void mas(double x) { s += x, n++, max = std::max(max, x), min = std::min(min, x); }
		double media() const { return n ? s / n : 0.0; }
	};
	std::map<std::string, Suma> sumas;
	void medir(const std::string& k, double x) { sumas[k].mas(x); }

	// ---------------------------------------------------------------------------------------- estado del cuadro anterior
	bool primero{ true };
	size_t nSucesos{ 0 }, vistosAntes{ 0 };
	std::map<std::string, int> manoAntes;   // caras en la mano dibujada (cuadro anterior)
	int dorsosAntes{ 0 };
	int serieRobo{ -1 };
	bool roboAntes{ false }, cartelTurnoAntes{ false }, cartelFaseAntes{ false }, ataqueAntes{ false }, alDiaAntes{ false };
	int faseCartel{ -1 };
	float tCartelTurno{ 0 }, tCartelFase{ 0 }, tRobo{ 0 }, tAtaque{ 0 }, tAlDia{ 0 }, tTurno{ -1 };
	int jugadasAntes{ 0 }, turnoJugadas{ -1 };
	int ladoTurno{ -1 };
	// la jugada del rival en curso: cuando se aplico y si ya se presento
	struct JugadaRival { float tAplica{ -1 }, tPresentada{ -1 }; int tipo{ -1 }; bool visible{ false }; size_t s0{ 0 }; };
	JugadaRival ultimaRival;
	float tListoRival{ -1 };   // desde cuando el rival podia jugar (la mesa al dia, decide el rival)

	struct Robo {
		size_t k;
		int lado;
		std::string carta;
		float tMotor;
		bool inicioTurno;
		int manoAntes;          // cuantas de esa carta (o dorsos) habia en la mano dibujada antes de que el motor robara
		int animaciones{ 0 };
		bool juzgado{ false }, temprana{ false };
	};
	std::vector<Robo> robos;

	void reiniciar()
	{
		primero = true;
		robos.clear();
		ultimaRival = JugadaRival{};
		tListoRival = -1;
		ladoTurno = -1;
		tTurno = -1;
	}

	// caras de la mano del jugador y dorsos de la del rival en el cuadro
	void leerManos(std::map<std::string, int>& caras, int& dorsos) const
	{
		caras.clear();
		dorsos = 0;
		for (const pantalla::Carta& k : p.cuadro.cartas) {
			if (k.en3D || k.alfa < 0.5f || k.cx() < 500.0f) continue;
			if (k.cy() > 880.0f && !k.id.empty()) caras[k.id]++;
			if (k.cy() < 140.0f && k.id.empty()) dorsos++;
		}
	}
	static bool visibleJugada(int tipo)
	{
		return tipo != ia::J_FASE && tipo != ia::J_PASAR && tipo != ia::J_ELEGIR && tipo >= 0;
	}

	// ---------------------------------------------------------------------------------------- cada cuadro
	void cuadro()
	{
		if (!j.enDuelo()) {
			primero = true;
			return;
		}
		const ia::Duelo& d = g.dueloIA;
		std::map<std::string, int> caras;
		int dorsos = 0;
		leerManos(caras, dorsos);
		if (primero || d.sucesos.size() < nSucesos) {   // (otro duelo / otra escena)
			primero = false;
			nSucesos = vistosAntes = d.sucesos.size();
			robos.clear();
			manoAntes = caras;
			dorsosAntes = dorsos;
			serieRobo = g.ctl.robo.serie;
			jugadasAntes = g.turnoIA.jugadasEsteTurno;
			turnoJugadas = g.turnoIA.turnoVisto;
			ultimaRival = JugadaRival{};
			tListoRival = -1;
			return;
		}
		// los robos nuevos del motor (la mano de ANTES: la del cuadro anterior)
		for (size_t k = nSucesos; k < d.sucesos.size(); k++) {
			const ia::Suceso& s = d.sucesos[k];
			if (s.tipo != ia::S_ROBO) continue;
			if (s.lado == 0 && s.carta.empty()) continue;
			bool inicio = false;
			for (size_t q = k; q-- > 0 && q + 4 > k;)
				if (d.sucesos[q].tipo == ia::S_TURNO) inicio = d.sucesos[q].lado == s.lado;
			Robo r{ k, s.lado, s.carta, j.t, inicio, s.lado == 0 ? manoAntes[s.carta] : dorsosAntes };
			robos.push_back(r);
		}
		nSucesos = d.sucesos.size();
		// la animacion del robo empieza: a que robo pertenece
		if (g.ctl.robo.serie != serieRobo) {
			serieRobo = g.ctl.robo.serie;
			const int lado = g.ctl.robo.rival ? 1 : 0;
			Robo* r = nullptr;
			for (Robo& x : robos)
				if (x.lado == lado && (lado == 1 || x.carta == g.ctl.robo.carta) && x.animaciones == 0) {
					r = &x;
					break;
				}
			if (!r)   // (ninguno sin animar: es otra animacion de uno ya animado)
				for (Robo& x : robos)
					if (x.lado == lado && (lado == 1 || x.carta == g.ctl.robo.carta)) r = &x;
			if (r && g.reparto <= 0) {
				r->animaciones++;
				const bool cartel = g.ctl.cartelTurnoVisible() || g.ctl.cartelInicioVisible();
				anotar("robo %s '%s' (anima %d, %s turno%s)", lado ? "rival" : "jugador", r->carta.c_str(), r->animaciones, r->inicioTurno ? "inicio de" : "en el",
				       cartel ? ", CON EL CARTEL DE TURNO" : "");
				if (r->animaciones > 1)
					j.falla("ROBO-UNA-VEZ", "la carta robada %s del %s se animo %d veces al robar", r->carta.c_str(), lado ? "rival" : "jugador", r->animaciones);
				else if (!r->temprana)
					j.bien("ROBO-UNA-VEZ");
				if (r->inicioTurno) {
					if (cartel) j.falla("ROBO-TRAS-CARTEL", "el robo del inicio del turno del %s (%s) se anima con el cartel de turno a la vista", lado ? "rival" : "jugador", r->carta.c_str());
					else j.bien("ROBO-TRAS-CARTEL");
				}
			}
		}
		// la carta robada ya esta en la mano dibujada y su animacion no empezo: se va a ver dos veces
		for (Robo& r : robos) {
			if (r.animaciones > 0 || r.temprana || g.reparto > 0) continue;
			// (el rival: solo el robo del inicio del turno; en los robos de efecto su mano cambia por otros sucesos a la vez,
			// descartes, Card Destruction, y los dorsos no dicen cual es cual)
			if (r.lado == 1 && !r.inicioTurno) continue;
			const int ahora = r.lado == 0 ? caras[r.carta] : dorsos;
			// (se juzga hasta que se presenta el robo)
			if (g.sucesosVistos > r.k) continue;
			if (ahora > r.manoAntes) {
				r.temprana = true;
				j.falla("ROBO-UNA-VEZ", "la carta robada %s del %s ya esta en la mano dibujada (%d -> %d) antes de su animacion de robo (suceso %d sin presentar%s): se ve llegar dos veces",
				        r.lado == 0 ? r.carta.c_str() : "(dorso)", r.lado ? "rival" : "jugador", r.manoAntes, ahora, (int)r.k,
				        g.ctl.cartelTurnoVisible() ? ", con el cartel de turno" : "");
				anotar("ROBO TEMPRANO %s '%s'", r.lado ? "rival" : "jugador", r.carta.c_str());
			}
		}
		// los robos ya presentados se olvidan a los 8 s
		robos.erase(std::remove_if(robos.begin(), robos.end(), [&](const Robo& r) { return g.sucesosVistos > r.k && j.t - r.tMotor > 8.0f; }), robos.end());
		manoAntes = caras;
		dorsosAntes = dorsos;

		cronologia();
	}

	// ---------------------------------------------------------------------------------------- la cronologia
	void cronologia()
	{
		const ia::Duelo& d = g.dueloIA;
		const float t = j.t;
		// los turnos presentados
		for (size_t k = vistosAntes; k < g.sucesosVistos && k < d.sucesos.size(); k++) {
			const ia::Suceso& s = d.sucesos[k];
			if (s.tipo == ia::S_TURNO) {
				if (tTurno >= 0 && ladoTurno >= 0) medir(ladoTurno ? "turno completo del rival" : "turno completo del jugador", t - tTurno);
				tTurno = t;
				ladoTurno = s.lado;
				anotar("== TURNO %d (%s)", s.valor, s.lado ? "rival" : "jugador");
			} else if (s.tipo == ia::S_ATAQUE || s.tipo == ia::S_INVOCA || s.tipo == ia::S_COLOCA || s.tipo == ia::S_COLOCA_MT || s.tipo == ia::S_ACTIVA_MT ||
			           s.tipo == ia::S_ACTIVA_EFECTO || s.tipo == ia::S_DANNO || s.tipo == ia::S_FASE) {
				anotar("  presenta %s %s %s", ia::NOMBRE_SUCESO[s.tipo], s.lado ? "rival" : "jugador", s.carta.c_str());
			}
		}
		vistosAntes = g.sucesosVistos;
		// carteles
		const bool ct = g.ctl.cartelTurnoVisible() || g.ctl.cartelInicioVisible();
		if (ct && !cartelTurnoAntes) tCartelTurno = t;
		if (!ct && cartelTurnoAntes) {
			medir(ladoTurno == 1 ? "cartel de turno del rival" : "cartel de turno del jugador", t - tCartelTurno);
			anotar("  cartel de turno %.2f s", t - tCartelTurno);
		}
		cartelTurnoAntes = ct;
		const bool cf = g.ctl.cartelFaseVisible() && !ct;
		if (cf && (!cartelFaseAntes || faseCartel != g.ctl.fase)) {
			if (cartelFaseAntes) medir("cartel de fase", t - tCartelFase);
			tCartelFase = t;
			faseCartel = g.ctl.fase;
			anotar("  cartel de fase %s", control::NOMBRE_FASE_LARGO[std::max(0, std::min(5, faseCartel))]);
		}
		if (!cf && cartelFaseAntes) {
			medir("cartel de fase", t - tCartelFase);
			anotar("  fin del cartel de fase (%.2f s)", t - tCartelFase);
		}
		cartelFaseAntes = cf;
		// robo
		if (g.ctl.robo.activo && !roboAntes) tRobo = t;
		if (!g.ctl.robo.activo && roboAntes && g.reparto <= 0) medir(g.ctl.robo.rival ? "robo del rival" : "robo del jugador", t - tRobo);
		roboAntes = g.ctl.robo.activo;
		// ataques
		if (g.hayAtaque() && !ataqueAntes) tAtaque = t;
		if (!g.hayAtaque() && ataqueAntes) {
			medir("ataque (animacion)", t - tAtaque);
			anotar("  fin del ataque (%.2f s)", t - tAtaque);
		}
		ataqueAntes = g.hayAtaque();
		// las jugadas del rival
		const bool alDia = j.alDia();
		const bool rivalPuede = alDia && d.decide() == 1 && d.ganador < 0;
		if (rivalPuede && tListoRival < 0) tListoRival = t;
		if (!rivalPuede && !(alDia && d.decide() == 1)) {}
		// (una jugada del rival: cambia su cuenta de jugadas del turno; turnoVisto la reinicia al ver un turno nuevo)
		const int jn = g.turnoIA.jugadasEsteTurno;
		const bool nueva = g.turnoIA.turnoVisto != turnoJugadas ? jn > 0 : jn != jugadasAntes && jn > 0;
		turnoJugadas = g.turnoIA.turnoVisto;
		jugadasAntes = jn;
		if (nueva) {
			const int tipo = (int)g.turnoIA.ultima.tipo;
			const float pensar = tListoRival >= 0 ? t - tListoRival : 0.0f;
			// la pausa de lectura: desde que la jugada visible anterior quedo presentada
			if (ultimaRival.visible && ultimaRival.tPresentada >= 0) {
				const float lectura = t - ultimaRival.tPresentada;
				medir("rival: pausa tras su jugada (lectura)", lectura);
				if (lectura + 1e-3f < ritmoturno::LECTURA_MIN * 0.95f)
					j.falla("RITMO-LECTURA", "el rival jugo %s %.2f s despues de que se presentara su jugada anterior (%s); minimo %.2f s", ia::NOMBRE_JUGADA[tipo], lectura,
					        ia::NOMBRE_JUGADA[ultimaRival.tipo], ritmoturno::LECTURA_MIN);
				else
					j.bien("RITMO-LECTURA");
			}
			if (g.ctl.cartelFaseVisible() && !ct && d.activo == 1) {
				j.falla("RITMO-CARTEL-FASE", "el rival jugo %s con el cartel de fase %s a la vista (%.2f s)", ia::NOMBRE_JUGADA[tipo],
				        control::NOMBRE_FASE_LARGO[std::max(0, std::min(5, g.ctl.fase))], g.ctl.tFase);
			} else if (d.activo == 1) {
				j.bien("RITMO-CARTEL-FASE");
			}
			if (tipo >= 0 && tipo <= ia::J_ELEGIR) medir(std::string("rival piensa: ") + ia::NOMBRE_JUGADA[tipo], pensar);
			anotar("  RIVAL juega %s %s (penso %.2f s)", ia::NOMBRE_JUGADA[tipo], g.turnoIA.ultima.carta.c_str(), pensar);
			ultimaRival = JugadaRival{ t, -1, tipo, visibleJugada(tipo), d.sucesos.size() };
			tListoRival = -1;
			tAlDia = t;
		}
		if (!rivalPuede && !alDia) tListoRival = -1;
		if (alDia && !alDiaAntes && ultimaRival.tAplica >= 0 && ultimaRival.tPresentada < 0) {
			ultimaRival.tPresentada = t;
			if (ultimaRival.tipo >= 0 && ultimaRival.tipo <= ia::J_ELEGIR)
				medir(std::string("rival se presenta: ") + ia::NOMBRE_JUGADA[ultimaRival.tipo], t - ultimaRival.tAplica);
			anotar("  mesa al dia (%.2f s de presentacion)", t - ultimaRival.tAplica);
		}
		alDiaAntes = alDia;
	}

	// la tabla (s de juego a la velocidad elegida)
	std::string tabla() const
	{
		std::string r;
		char b[200];
		snprintf(b, sizeof(b), "%-44s %6s %6s %6s %5s\n", "medida", "media", "min", "max", "n");
		r += b;
		for (const auto& [k, s] : sumas) {
			snprintf(b, sizeof(b), "%-44s %6.2f %6.2f %6.2f %5d\n", k.c_str(), s.media(), s.min, s.max, s.n);
			r += b;
		}
		return r;
	}
};

}   // namespace hu
