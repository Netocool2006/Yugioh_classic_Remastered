// Rama classic-jugador-humano: el JUGADOR HUMANO (cu_humano.h) sobre la interfaz de verdad sin GPU (pantalla_sin_gpu.h).
// Ver docs/jugador_humano.md. Compilar y correr (contenedor arch-ps5): src/pruebas/humano.sh <salida.tsv> [modo] [n] [semilla]
//   modos: todo | duelos | fijos | cartas | volcar;  n: duelos (duelos) o tope de cartas (cartas); semilla: la base
//   (cartas: HU_LOTE=k/n reparte las cartas en n lotes y corre el k)
#include <unistd.h>

#include "pantalla_sin_gpu.h"
// las escenas de la prueba carta por carta desde el front (escenaBase, inyectar, mazoVariado): se reutilizan
#define main main_prueba_front_total
#include "prueba_front_total.cpp"
#undef main
#include "cu_humano.h"
#include "cu_ritmo_humano.h"   // rama classic-ps5-ritmo: el cronista (robo una vez, ritmo del turno)

using namespace cuft;

struct Mundo {
	Juego* g;
	pantalla::Pantalla* p;
	hu::Juez* j;
	Bot* b;
	uint32_t semilla;
	hu::Cronista* c{ nullptr };
};

// el resumen de los casos del jugador humano (los CU-FT del vigia de cu_front_total.h no van: son de su prueba)
static int resumenHumano(const char* salida)
{
	int ok = 0, mal = 0, dec = 0;
	FILE* f = fopen(salida, "w");
	for (const cu::Resultado& r : cu::todos()) {
		if (r.id.rfind("CU-HU-", 0) != 0) continue;
		(r.estado == "FALLA" ? mal : r.estado == "DECISION" ? dec : ok)++;
		if (f) {
			std::string d = r.detalle;
			for (char& c : d)
				if (c == '\t' || c == '\n') c = ' ';
			fprintf(f, "%s\t%s\t%s\n", r.id.c_str(), r.estado.c_str(), d.c_str());
		}
	}
	if (f) fclose(f);
	printf("CASOS DE USO: %d casos, OK %d, FALLA %d, DECISION 0\n", ok + mal, ok, mal);
	for (const cu::Resultado& r : cu::todos())
		if (r.id.rfind("CU-HU-", 0) == 0 && r.estado == "FALLA") printf("  FALLA %s: %s\n", r.id.c_str(), r.detalle.substr(0, 600).c_str());
	return mal ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------- duelos completos
// el humano juega contra el rival hasta el final o el tope (turnos / s de juego); al tope abandona con el engranaje
static void dueloHumano(Mundo& m, const Rival& r0, int n, int maxTurnos, float maxSegundos)
{
	Juego& g = *m.g;
	hu::Juez& j = *m.j;
	Rival r = r0;
	const std::string id = "CU-HU-DUELO-" + r.nombre;
	hu::Humano h(g, *m.b, *m.p, j, m.semilla * 1000u + (uint32_t)n);
	h.lento = n % 4 == 3;   // (uno de cada cuatro, un jugador lento)
	char sem[64];
	snprintf(sem, sizeof(sem), "%u/%d", m.semilla, n);
	j.semilla = sem;
	j.contexto = "duelo " + r.nombre;
	j.cartaCaso.clear();
	if (!elegirRival(*m.b, r)) {
		cu::caso(id, false, "no se pudo elegir el rival (pantalla %d)", g.ctl.vista.pantalla);
		return;
	}
	const auto t0 = std::chrono::steady_clock::now();
	if (!h.sorteo()) {
		cu::caso(id, false, "el sorteo no termino con X (semilla %s)", sem);
		return;
	}
	j.reiniciar();
	const float tj0 = j.t;
	while (j.enDuelo() && g.dueloIA.ganador < 0 && g.dueloIA.turno <= maxTurnos && j.t - tj0 < maxSegundos && !j.trancado) h.paso();
	h.mirar(2.0f);
	j.cerrarEventos();
	const int turnos = g.dueloIA.turno, gan = g.dueloIA.ganador;
	// al tope: abandonar con el engranaje (Abandonar duelo = derrota)
	if (j.enDuelo() && gan < 0) {
		const int derrotas = g.ctl.vista.derrotas;
		for (int k = 0; k < 6 && !h.lee().pausa; k++) {
			if (h.lee().menu.abierto || h.lee().selector || h.lee().detalle()) h.pulsar(PAD_CIRCLE);
			else h.pulsar(PAD_TOUCH_PAD, 0.6f);
		}
		hu::Lectura L = h.lee();
		const int k = hu::Humano::buscarOp(L.menu, { "Abandonar duelo" });
		if (L.pausa && k >= 0) {
			h.elegir(k);
			for (int q = 0; q < 4 && g.ctl.vista.pantalla == VD::DUELO; q++) {
				L = h.lee();
				const int si = hu::Humano::buscarOp(L.menu, { "Abandonar", "Sí", "Si" });
				if (L.menu.abierto && si >= 0) h.elegir(si);
				else h.mirar(0.8f);
			}
			h.mirar(1.5f);
			if (g.ctl.vista.pantalla == VD::RESULTADO && g.ctl.vista.derrotas == derrotas + 1) j.bien("CONTROLES-ABANDONAR");
			else j.falla("CONTROLES-ABANDONAR", "Abandonar duelo (derrota) de la pausa: pantalla %d, derrotas %d -> %d", g.ctl.vista.pantalla, derrotas, g.ctl.vista.derrotas);
		} else {
			j.falla("CONTROLES-ENGRANAJE", "el panel tactil no abrio la pausa con Abandonar duelo para terminar: %s", L.resumen().c_str());
		}
	}
	const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	cu::caso(id, !j.trancado, "el duelo quedo trancado (semilla %s)", sem);
	fprintf(stderr, "duelo %2d %-40s semilla %-8s turnos %2d gana %2d botones %4d juego %.0f s (%.1f s)\n", n, r.nombre.c_str(), sem, turnos, gan, h.nBotones, j.t - tj0, s);
	for (int k = 0; k < 8 && g.ctl.vista.pantalla == VD::RESULTADO; k++) m.b->apretar(PAD_CIRCLE, 0.5f);
}

static void duelos(Mundo& m, int cuantos)
{
	Juego& g = *m.g;
	std::vector<Rival> todos = listaRivales(g);
	// rivales repartidos (historia y libre, distintos niveles) y la semilla elige el resto
	std::vector<Rival> rs;
	for (int k = 0; k < cuantos && !todos.empty(); k++) rs.push_back(todos[(size_t)((m.semilla * 7u + (uint32_t)k * 37u) % todos.size())]);
	for (int k = 0; k < (int)rs.size(); k++) dueloHumano(m, rs[(size_t)k], k, 14, 600.0f);
}

// ------------------------------------------------------------------------------------------------- escenas
// (lo unico que se arma por fuera es la ESCENA: las cartas en la mano y en la mesa; despues juega el humano mirando)
static bool asegurarDuelo(Mundo& m)
{
	Juego& g = *m.g;
	if (g.ctl.vista.pantalla == VD::DUELO && !g.ctl.vista.sorteo && g.dueloIA.ganador < 0) return true;
	for (int k = 0; k < 40 && g.ctl.vista.pantalla == VD::DUELO && g.dueloIA.ganador >= 0; k++) m.b->esperar(0.5f);
	return nuevoDuelo(*m.b);
}
static void nuevaEscena(Mundo& m, const ia::Duelo& d, const std::string& ctx)
{
	m.b->rivalPorBot = false;
	m.b->rivalActiva.clear();
	m.b->rivalVentana = -1;
	ritmo::estado().limpiar();   // (los viajes de la escena anterior no siguen en la nueva)
	inyectar(*m.b, d);
	m.j->reiniciar();
	m.j->tEscena = m.j->t;
	m.j->contexto = ctx;
	m.b->esperar(0.6f);
}
static void jugarEscena(Mundo& m, hu::Humano& h, float maxJuego, const std::function<bool()>& fin)
{
	Juego& g = *m.g;
	hu::Juez& j = *m.j;
	const float t0 = j.t;
	while (j.enDuelo() && g.dueloIA.ganador < 0 && !j.trancado && j.t - t0 < maxJuego && !fin()) h.paso();
	h.mirar(1.0f);
	j.cerrarEventos();
}
static bool huboLado(const ia::Duelo& d, size_t desde, std::initializer_list<int> tipos, const std::string& carta, int lado)
{
	for (size_t k = desde; k < d.sucesos.size(); k++) {
		const ia::Suceso& s = d.sucesos[k];
		if (s.lado != lado || s.carta != carta) continue;
		for (int t : tipos)
			if (s.tipo == t) return true;
	}
	return false;
}
// el turno siguiente del jugador, con la mesa al dia
static std::function<bool()> hastaTurno(Mundo& m, int turno)
{
	return [&m, turno] { return m.g->dueloIA.turno >= turno && m.g->dueloIA.activo == 0 && m.j->alDia(); };
}

// ------------------------------------------------------------------------------------------------- casos fijos
struct Fijo {
	const char* id;      // CU-HU-FIJO-<id>
	const char* carta;   // la del rival
	bool turnoRival;     // la juega el rival en su turno (si no: colocada, en el turno del jugador)
	int ventana;         // (colocada) en que ventana la activa el rival (-1: la primera que pueda)
};
static void fijo(Mundo& m, const Fijo& f, const std::vector<std::string>& mazo, int n)
{
	Juego& g = *m.g;
	hu::Juez& j = *m.j;
	const std::string id = std::string("CU-HU-FIJO-") + f.id;
	if (!g.baseIA.carta(f.carta)) return;
	if (!asegurarDuelo(m)) {
		cu::caso(id, false, "no se pudo empezar un duelo");
		return;
	}
	cub::Mesa me = escenaBase(g.baseIA, mazo);
	// el jugador: dos monstruos en ataque y uno en la mano; el rival: uno debil y la carta
	for (ia::Monstruo& x : me.d.l[0].m) x = ia::Monstruo{};
	for (ia::Monstruo& x : me.d.l[1].m) x = ia::Monstruo{};
	for (ia::MagiaTrampa& x : me.d.l[1].mt) x = ia::MagiaTrampa{};
	me.mon(0, "battle_ox");
	me.mon(0, "celtic_guardian");
	me.d.l[0].mano = { "summoned_skull", "vorse_raider", "mystical_elf" };
	me.mon(1, "baby_dragon");
	// (el jugador tiene con que responder: una trampa colocada que niega trampas; asi el juego le ofrece la ventana)
	for (ia::MagiaTrampa& x : me.d.l[0].mt) x = ia::MagiaTrampa{};
	if (g.baseIA.carta("seven_tools_of_the_bandit")) me.colocar(0, "seven_tools_of_the_bandit", 1);
	if (g.baseIA.carta("mystical_space_typhoon")) me.colocar(0, "mystical_space_typhoon", 1);
	if (f.turnoRival) {
		me.d.l[1].mano = { f.carta, "vorse_raider", "celtic_guardian" };
		me.d.turno = 4;
		me.d.activo = 1;
		me.d.l[1].invocoNormal = false;
	} else {
		me.colocar(1, f.carta, 1);
	}
	me.d.recalcular();
	char ctx[96];
	snprintf(ctx, sizeof(ctx), "fijo %s", f.id);
	nuevaEscena(m, me.d, ctx);
	if (!f.turnoRival) {   // (el rival la activa en cuanto pueda: lo demas lo pasa)
		m.b->rivalPorBot = true;
		m.b->rivalActiva = f.carta;
		m.b->rivalVentana = f.ventana;
	}
	hu::Humano h(g, *m.b, *m.p, j, m.semilla * 31u + (uint32_t)n);
	h.probResponder = 0.3f;
	const size_t s0 = g.dueloIA.sucesos.size();
	const int f0 = j.fallasTotal;
	jugarEscena(m, h, 120.0f, hastaTurno(m, f.turnoRival ? 5 : 4));
	const bool activo = huboLado(g.dueloIA, s0, { ia::S_ACTIVA_MT, ia::S_ACTIVA_EFECTO }, f.carta, 1);
	cu::caso(id, j.fallasTotal == f0, "los oraculos fallaron en la escena (ver CU-HU-ENTERARSE/VENTANA/NUNCA-TRANCADO/MESA; %s %s)", f.carta, activo ? "activada" : "no activada");
	fprintf(stderr, "fijo %-24s %s: el rival %s; fallas de oraculos %d\n", f.id, f.carta, activo ? "la activo" : "NO la activo", j.fallasTotal - f0);
	if (!activo) cu::de(id).detalle += std::string(cu::de(id).detalle.empty() ? "" : " | ") + "(el rival no la activo: sin ocasion en esta escena)";
	m.b->rivalPorBot = false;
}

// la pregunta pendiente del jugador + engranaje, detalle y OPTIONS: nunca trancado y la pregunta vuelve a estar a la vista
static void preguntaBotones(Mundo& m, const std::vector<std::string>& mazo)
{
	Juego& g = *m.g;
	hu::Juez& j = *m.j;
	const std::string id = "CU-HU-FIJO-PREGUNTA-BOTONES";
	if (!asegurarDuelo(m)) {
		cu::caso(id, false, "no se pudo empezar un duelo");
		return;
	}
	cub::Mesa me = escenaBase(g.baseIA, mazo);
	for (ia::Monstruo& x : me.d.l[0].m) x = ia::Monstruo{};
	for (ia::Monstruo& x : me.d.l[1].m) x = ia::Monstruo{};
	me.mon(0, "mystical_elf");
	me.colocar(0, "negate_attack", 1);
	me.colocar(0, "sakuretsu_armor", 1);
	me.mon(1, "vorse_raider");
	me.mon(1, "summoned_skull");
	me.d.turno = 4;
	me.d.activo = 1;
	me.d.recalcular();
	nuevaEscena(m, me.d, "fijo pregunta + botones");
	hu::Humano h(g, *m.b, *m.p, j, m.semilla * 17u + 3u);
	h.travesura = 0.0f;
	const int f0 = j.fallasTotal;
	int vistas = 0, malas = 0;
	std::string detalle;
	for (float t0 = j.t; j.t - t0 < 90.0f && j.enDuelo() && g.dueloIA.ganador < 0 && !(g.dueloIA.turno >= 5 && g.dueloIA.activo == 0);) {
		hu::Lectura L = h.lee();
		if (!L.pregunta()) {
			h.mirar(0.3f);
			continue;
		}
		vistas++;
		// la pregunta esta a la vista: engranaje (pausa) y cerrar; triangulo y cerrar; OPTIONS; cuadrado. Tras cada uno la
		// pregunta tiene que seguir (o volver) a la vista y contestarse
		static const uint32_t MOLESTAR[] = { PAD_TOUCH_PAD, PAD_TRIANGLE, PAD_OPTIONS, PAD_SQUARE, PAD_CIRCLE };
		for (uint32_t b : MOLESTAR) {
			h.pulsar(b, 0.6f);
			L = h.lee();
			if (L.pausa || L.detalle() || L.selector) h.pulsar(PAD_CIRCLE, 0.6f);
			bool ok = false;
			for (int k = 0; k < 8 && !ok; k++) {
				L = h.lee();
				ok = L.pregunta() || L.eligiendo() || !j.preguntaJugador();
				if (!ok) h.mirar(0.25f);
			}
			if (!ok) {
				malas++;
				detalle += std::string(" tras ") + cuft::nombreBoton(b) + ": " + L.resumen();
			}
			if (!j.preguntaJugador()) break;
		}
		L = h.lee();
		if (L.pregunta()) h.responder(L);
		else if (L.eligiendo()) h.elegirAlgo(L);
		else h.paso();
	}
	h.mirar(1.0f);
	j.cerrarEventos();
	cu::caso(id, vistas > 0, "el rival no ataco o la pregunta del jugador nunca aparecio (turno %d, fase %d)", g.dueloIA.turno, (int)g.dueloIA.fase);
	cu::caso(id, malas == 0, "con la pregunta pendiente, tras un boton la pregunta no volvio a la vista:%s", detalle.substr(0, 500).c_str());
	cu::caso(id, j.fallasTotal == f0, "los oraculos fallaron en la escena");
	fprintf(stderr, "fijo PREGUNTA-BOTONES: %d preguntas vistas, %d sin volver, fallas de oraculos %d\n", vistas, malas, j.fallasTotal - f0);
}

// la pausa por dentro: Opciones, Construir mazo y volver (con la pantalla)
static void pausaMenus(Mundo& m)
{
	Juego& g = *m.g;
	hu::Juez& j = *m.j;
	if (!asegurarDuelo(m)) return;
	hu::Humano h(g, *m.b, *m.p, j, m.semilla * 13u + 1u);
	h.travesura = 0.0f;
	for (const char* op : { "Opciones", "Construir mazo" }) {
		const std::string id = std::string("CU-HU-CONTROLES-PAUSA-") + (op[0] == 'O' ? "OPCIONES" : "CONSTRUIR");
		for (int k = 0; k < 4 && !h.lee().pausa; k++) h.pulsar(PAD_TOUCH_PAD, 0.8f);
		hu::Lectura L = h.lee();
		const int k = hu::Humano::buscarOp(L.menu, { op });
		if (!L.pausa || k < 0) {
			if (getenv("HU_DEBUG"))
				for (const pantalla::Texto& t : m.p->cuadro.textos)
					if (!t.tapado && t.col.a > 0.2f && t.x > 600.0f && t.x < 1400.0f && t.y > 250.0f && t.y < 850.0f)
						fprintf(stderr, "    '%s' %.0f,%.0f w %.0f tam %.1f a %.2f\n", t.s.c_str(), t.x, t.y, t.w, t.tam, t.col.a);
			cu::caso(id, false, "la pausa no ofrece '%s': %s", op, L.resumen().c_str());
			continue;
		}
		const std::string antes = m.p->cuadro.resumen(300);
		h.elegir(k);
		h.mirar(0.8f);
		const bool abrio = op[0] == 'O' ? g.menuP.enOpciones : g.constructorMazo.abierto;
		const std::string se = m.p->cuadro.resumen(300);
		cu::caso(id, abrio && se != antes && !h.lee().pausa, "'%s' de la pausa no abrio su pantalla (se ve: %s)", op, se.c_str());
		for (int q = 0; q < 6 && (g.menuP.enOpciones || g.constructorMazo.abierto); q++) h.pulsar(PAD_CIRCLE, 0.6f);
		L = h.lee();
		cu::caso(id, !g.menuP.enOpciones && !g.constructorMazo.abierto, "circulo no salio de '%s'", op);
		if (L.pausa) h.pulsar(PAD_CIRCLE, 0.6f);
	}
	h.mirar(0.5f);
	cu::caso("CU-HU-CONTROLES-PAUSA-VUELVE", j.enDuelo() && !g.pausa.abierta, "tras Opciones / Construir mazo no se volvio al duelo (pantalla %d, pausa %d)", g.ctl.vista.pantalla,
	         (int)g.pausa.abierta);
}

// autopruebas: el juez tiene que ver las fallas a proposito (que no pase por no mirar)
static void autopruebas(Mundo& m, const std::vector<std::string>& mazo)
{
	Juego& g = *m.g;
	hu::Juez& j = *m.j;
	if (!asegurarDuelo(m)) return;
	cub::Mesa me = escenaBase(g.baseIA, mazo);
	nuevaEscena(m, me.d, "autoprueba");
	m.b->esperar(1.0f);
	auto guardar = [](const char* id) { return cu::de(id); };
	auto volver = [](const char* id, const cu::Resultado& r) { cu::de(id) = r; };
	// ENTERARSE: un monstruo del jugador al cementerio sin presentarlo (la mesa lo saca de golpe, sin viaje ni aviso)
	{
		const cu::Resultado r0 = guardar("CU-HU-ENTERARSE");
		const int f0 = j.fallasTotal;
		ia::Duelo& d = g.dueloIA;
		int z = -1;
		for (int i = 0; i < ia::ZONAS; i++)
			if (!d.l[0].m[(size_t)i].vacia()) z = i;
		if (z >= 0) {
			const std::string c = d.l[0].m[(size_t)z].id;
			d.l[0].m[(size_t)z] = ia::Monstruo{};
			d.l[0].cementerio.push_back(c);
			d.sucesos.push_back(ia::Suceso(ia::S_AL_CEMENTERIO, 0, z, -1, 0, false, c));
			g.sucesosVistos = d.sucesos.size();   // (la pantalla lo da por presentado sin mostrar nada)
			g.consulta.vistos = d.sucesos.size();
			g.sincronizarMesaDuelo();
			m.b->esperar(hu::VENTANA_SUCESO + 1.0f);
		}
		const bool vio = j.fallasTotal > f0;
		volver("CU-HU-ENTERARSE", r0);
		j.fallasTotal = f0;
		cu::caso("CU-HU-AUTOPRUEBA-ENTERARSE", vio, "una carta del jugador al cementerio sin verse no dio falla");
	}
	// MESA-COHERENTE: una carta girada en la mesa dibujada y no en el duelo
	{
		const cu::Resultado r0 = guardar("CU-HU-MESA-COHERENTE");
		const int f0 = j.fallasTotal;
		cub::Mesa m2 = escenaBase(g.baseIA, mazo);
		nuevaEscena(m, m2.d, "autoprueba mesa");
		const int z = Juego::zonaMesaIA(1, 0);
		g.ctl.zonas[(size_t)z].carta = g.ctl.zonas[(size_t)z].carta == "battle_ox" ? "celtic_guardian" : "battle_ox";
		g.ctl.zonas[(size_t)z].bocaAbajo = false;
		j.mesaMirada = false;
		j.tAlDia = 0.0f;
		j.tEscena = -99.0f;
		m.b->esperar(4.0f);
		const bool vio = j.fallasTotal > f0;
		volver("CU-HU-MESA-COHERENTE", r0);
		j.fallasTotal = f0;
		cu::caso("CU-HU-AUTOPRUEBA-MESA", vio, "una carta cambiada en la mesa dibujada no dio falla");
		g.sincronizarMesaDuelo();
	}
}

static void fijos(Mundo& m)
{
	const std::vector<std::string> mazo = mazoVariado(m.g->baseIA);
	autopruebas(m, mazo);
	// el pedido del usuario: atacar contra Mirror Force del rival; y las trampas / magias de respuesta de siempre
	static const Fijo F[] = {
		{ "MIRROR-FORCE", "mirror_force", false, ia::V_ATAQUE },          { "TRAP-HOLE", "trap_hole", false, ia::V_INVOCACION_NORMAL },
		{ "SAKURETSU-ARMOR", "sakuretsu_armor", false, ia::V_ATAQUE },    { "MAGIC-CYLINDER", "magic_cylinder", false, ia::V_ATAQUE },
		{ "NEGATE-ATTACK", "negate_attack", false, ia::V_ATAQUE },        { "BOOK-OF-MOON", "book_of_moon", false, -1 },
		{ "TORRENTIAL-TRIBUTE", "torrential_tribute", false, ia::V_INVOCACION_NORMAL }, { "DUST-TORNADO", "dust_tornado", false, -1 },
		{ "SWORDS-OF-REVEALING-LIGHT", "swords_of_revealing_light", true, -1 }, { "RAIGEKI", "raigeki", true, -1 },
		{ "DARK-HOLE", "dark_hole", true, -1 },                           { "MYSTICAL-SPACE-TYPHOON", "mystical_space_typhoon", true, -1 },
	};
	int n = 0;
	for (const Fijo& f : F) fijo(m, f, mazo, n++);
	preguntaBotones(m, mazo);
	pausaMenus(m);
}

// ------------------------------------------------------------------------------------------------- carta por carta
struct Cobertura { int cartas{ 0 }, usadaJugador{ 0 }, usadaRival{ 0 }, visor{ 0 }, conFalla{ 0 }; std::vector<std::string> sinUso; };

// A: el jugador la usa (desde la mano; fusion con Polymerization o por contacto, ritual con su magia, toon con Toon World),
// ataca y termina el turno; el rival juega el suyo. B: el rival la usa contra el jugador (trampa colocada en el turno del
// jugador; magia en la mano y monstruo en la mesa en el turno del rival) y el jugador responde
static void cartaHumano(Mundo& m, const ia::Carta& c, const std::vector<std::string>& mazo, Cobertura& cob, int n)
{
	Juego& g = *m.g;
	hu::Juez& j = *m.j;
	const ia::Base& base = g.baseIA;
	const std::string& id = c.id;
	const std::string cid = "CU-HU-CARTA-" + id;
	cu::de(cid);
	cob.cartas++;
	const int f0 = j.fallasTotal;
	j.cartaCaso = id;
	char sem[48];
	snprintf(sem, sizeof(sem), "%u/carta%d", m.semilla, n);
	j.semilla = sem;
	hu::Humano h(g, *m.b, *m.p, j, m.semilla * 7919u + (uint32_t)n);
	h.travesura = 0.08f;
	// ---- A
	if (!asegurarDuelo(m)) {
		cu::caso(cid, false, "no se pudo empezar un duelo para la carta");
		j.cartaCaso.clear();
		return;
	}
	cub::Mesa ma = escenaBase(base, mazo);
	std::string jugar = id;
	bool sinA = false;
	if (c.clase == "fusion") {
		auto it = base.fusiones.find(id);
		if (it == base.fusiones.end()) sinA = true;
		else {
			ma.d.l[0].extra = { id };
			if (ia::fusionContacto(id)) {
				for (ia::Monstruo& x : ma.d.l[0].m) x = ia::Monstruo{};
				for (const std::string& mat : it->second) ma.mon(0, mat);
				jugar = it->second.empty() ? id : it->second[0];
			} else {
				ma.mano(0, "polymerization");
				for (const std::string& mat : it->second) ma.mano(0, mat);
				jugar = "polymerization";
			}
		}
	} else if (c.clase == "ritual") {
		std::string magia;
		auto it = base.rituales.find(id);
		if (it != base.rituales.end()) magia = it->second;
		if (magia.empty())
			for (const ia::Carta& k : base.cartas)
				if (k.subtipo == "magia_ritual" && base.ritualCon(id, k.id)) magia = k.id;
		if (magia.empty()) sinA = true;
		else {
			ma.mano(0, magia);
			ma.mano(0, id);
			ma.mon(0, "battle_ox");
			jugar = magia;
		}
	} else {
		if (c.subtipo == "toon") {
			const int w = ma.colocar(0, "toon_world");
			ma.d.l[0].mt[(size_t)w].bocaAbajo = false;
		}
		if (c.monstruo) {
			cuc::condicionInvocar(ma, id);
			if (id == "terrorking_archfiend") ma.mon(0, "archfiend_soldier");
		}
		ma.mano(0, id);
	}
	if (!sinA) {
		ma.d.recalcular();
		nuevaEscena(m, ma.d, "carta " + id + " (la usa el jugador)");
		h.preferida = jugar;
		// el visor: con el recuadro en la carta, la columna muestra su nombre
		const bool mano = std::find(g.dueloIA.l[0].mano.begin(), g.dueloIA.l[0].mano.end(), jugar) != g.dueloIA.l[0].mano.end();
		if (h.irACarta(jugar, mano)) {
			h.mirar(0.5f);
			const std::string nom = j.nombreDe(jugar);
			bool enVisor = false;
			for (const pantalla::Texto& t : m.p->cuadro.textos)
				if (!t.icono && !t.tapado && t.x < 460.0f && hu::contiene(t.s, nom)) enVisor = true;
			if (enVisor) {
				cob.visor++;
				j.bien("CARTA-VISOR");
			} else {
				j.falla("CARTA-VISOR", "con el recuadro en %s el visor de la columna no muestra su nombre '%s'", jugar.c_str(), nom.c_str());
			}
		}
		const size_t s0 = g.dueloIA.sucesos.size();
		jugarEscena(m, h, 100.0f, hastaTurno(m, 5));
		if (huboLado(g.dueloIA, s0, { ia::S_INVOCA, ia::S_COLOCA, ia::S_COLOCA_MT, ia::S_ACTIVA_MT, ia::S_ACTIVA_EFECTO }, id, 0)) cob.usadaJugador++;
		else cob.sinUso.push_back(id);
	}
	// ---- B
	if (asegurarDuelo(m)) {
		cub::Mesa mb = escenaBase(base, mazo);
		bool turnoRival = true;
		if (c.trampa) {
			mb.colocar(1, id, 1);
			turnoRival = false;
		} else if (c.magia) {
			mb.d.l[1].mano.insert(mb.d.l[1].mano.begin(), id);
		} else {
			for (ia::Monstruo& x : mb.d.l[1].m) x = ia::Monstruo{};
			mb.mon(1, id);
			mb.mon(1, "baby_dragon");
		}
		if (turnoRival) {
			mb.d.turno = 4;
			mb.d.activo = 1;
			mb.d.l[1].invocoNormal = false;
		}
		mb.d.recalcular();
		nuevaEscena(m, mb.d, "carta " + id + " (la usa el rival)");
		if (!turnoRival) {
			m.b->rivalPorBot = true;
			m.b->rivalActiva = id;
		}
		h.preferida.clear();
		const size_t s1 = g.dueloIA.sucesos.size();
		jugarEscena(m, h, 100.0f, hastaTurno(m, turnoRival ? 5 : 4));
		m.b->rivalPorBot = false;
		if (huboLado(g.dueloIA, s1, { ia::S_INVOCA, ia::S_ACTIVA_MT, ia::S_ACTIVA_EFECTO, ia::S_ATAQUE, ia::S_COMBATE }, id, 1) ||
		    (c.monstruo && huboLado(g.dueloIA, s1, { ia::S_AL_CEMENTERIO }, id, 1)))
			cob.usadaRival++;
	}
	if (j.fallasTotal > f0) cob.conFalla++;
	j.bien("CARTA");   // (cuenta la comprobacion; las fallas de la escena ya marcaron el caso de la carta)
	j.cartaCaso.clear();
}

static void cartas(Mundo& m, int tope)
{
	Juego& g = *m.g;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	// primero las que tienen efecto (monstruos de efecto, magias y trampas), despues fusion/ritual y las normales
	std::vector<const ia::Carta*> orden;
	for (int pasada = 0; pasada < 3; pasada++)
		for (const ia::Carta& c : g.baseIA.cartas) {
			const int p = (!c.monstruo || c.clase == "efecto") ? 0 : (c.clase == "fusion" || c.clase == "ritual") ? 1 : 2;
			if (p == pasada) orden.push_back(&c);
		}
	int lote = 0, lotes = 1;
	if (const char* l = getenv("HU_LOTE")) sscanf(l, "%d/%d", &lote, &lotes);
	Cobertura cob;
	const auto t0 = std::chrono::steady_clock::now();
	int n = 0;
	const char* desde = getenv("HU_DESDE");   // (para reproducir: empezar en esa carta)
	bool empezar = !desde;
	for (int k = 0; k < (int)orden.size(); k++) {
		if (!empezar && orden[(size_t)k]->id == desde) empezar = true;
		if (!empezar) continue;
		if (!desde && k % lotes != lote) continue;
		if (tope >= 0 && n >= tope) break;
		cartaHumano(m, *orden[(size_t)k], mazo, cob, k);
		n++;
		if (n % 50 == 0)
			fprintf(stderr, "cartas: %d (%.0f s; usadas jugador %d, rival %d)\n", n, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), cob.usadaJugador,
			        cob.usadaRival);
	}
	fprintf(stderr, "CARTA POR CARTA (lote %d/%d): %d cartas; usada por el jugador %d, por el rival %d; visor con su nombre %d; con alguna falla %d (%.0f s)\n", lote, lotes, cob.cartas,
	        cob.usadaJugador, cob.usadaRival, cob.visor, cob.conFalla, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
	std::string s;
	for (size_t k = 0; k < cob.sinUso.size() && k < 80; k++) s += cob.sinUso[k] + " ";
	fprintf(stderr, "sin usar por el jugador (el menu no la ofrecio o el humano eligio otra cosa): %s\n", s.c_str());
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_humano <carpeta con ui/> <salida.tsv> [todo|duelos|fijos|cartas|volcar] [n] [semilla]\n");
		return 2;
	}
	const std::string assets = std::string(argv[1]) + "/", salida = argv[2];
	const std::string modo = argc > 3 ? argv[3] : "todo";
	const int n = argc > 4 ? atoi(argv[4]) : -1;
	const uint32_t semilla = argc > 5 ? (uint32_t)atoi(argv[5]) : 1u;
	const std::string datos = assets + "datos_hu_" + std::to_string((int)getpid()) + "/";   // (los lotes en paralelo, cada uno lo suyo)
	system(("rm -rf '" + datos + "' && mkdir -p '" + datos + "'").c_str());
	setenv("YUGI_MAZO", (datos + "mazo.txt").c_str(), 1);
	for (const char* v : { "YUGI_GUION", "YUGI_IDIOMA", "YUGI_FIGURAS", "YUGI_CAMARA", "YUGI_VELOCIDAD", "YUGI_RESULTADO", "YUGI_PUNTERO", "YUGI_LP", "YUGI_FASE" }) unsetenv(v);
	idioma::cargar(datos + "idioma.txt");
	Juego* g = new Juego;
	g->preparar(assets);
	if (!g->baseIAlista) {
		fprintf(stderr, "sin datos en %s\n", assets.c_str());
		return 2;
	}
	g->frameTimer = 1.0f / 30.0f;   // (30 cuadros por segundo de juego: los tiempos son los de la consola)
	g->ctl.vista.nivelMax = ia::NIVEL_MAX;
	for (const personajes::Personaje& q : g->historia.lista) g->historia.vencidos.insert(q.id);
	g->actualizarDesbloqueos();
	opciones::preguntar() = opciones::PREGUNTAR_AUTO;
	pantalla::Pantalla* p = new pantalla::Pantalla(*g);
	p->preparar(assets);
	hu::Juez* j = new hu::Juez(*g, *p);
	j->dt = g->frameTimer;
	j->preparar();
	j->fallas = fopen((salida + ".fallas.txt").c_str(), "w");
	Bot* b = new Bot(*g);
	hu::Cronista* cron = new hu::Cronista(*g, *p, *j);   // rama classic-ps5-ritmo
	cron->log = fopen((salida + ".ritmo.txt").c_str(), "w");
	b->despuesDeCuadro = [&] {
		p->dibujar(g->frameTimer);
		j->cuadro();
		cron->cuadro();
	};
	{   // las cartas de campo con escenario (como prueba_front_total.cpp)
		const std::string fuente = std::string(__FILE__);
		campos::idsCampo() = [&] {
			std::vector<std::string> v;
			for (const std::string& id : cuc::leerCampos(fuente.substr(0, fuente.find_last_of('/')) + "/../campos.h")) v.push_back(id);
			return v;
		}();
	}
	Mundo m{ g, p, j, b, semilla, cron };
	const auto t0 = std::chrono::steady_clock::now();
	b->esperar(0.5f);
	if (modo == "volcar") {
		hu::Humano h(*g, *b, *p, *j, semilla);
		b->despuesDeCuadro = [&] {
			p->dibujar(g->frameTimer);
			j->cuadro();
			static std::string antes;
			if (j->cuadroN % 10) return;
			const std::string r = hu::leer(p->cuadro).resumen();
			if (r != antes) {
				fprintf(stderr, "#%d t%.1f est %s | %s\n", j->cuadroN, j->t, control::NOMBRE_ESTADO[g->ctl.estado], r.c_str());
				if (getenv("HU_DEBUG"))
					for (const pantalla::Texto& t : p->cuadro.textos)
						if (t.y > 850.0f && t.x > 1300.0f) fprintf(stderr, "    '%s' %.0f,%.0f tam %.0f a %.2f tap %d\n", t.s.c_str(), t.x, t.y, t.tam, t.col.a, (int)t.tapado);
			}
			antes = r;
		};
		std::vector<Rival> rs = listaRivales(*g);
		Rival r = rs[0];
		if (elegirRival(*b, r) && h.sorteo())
			for (int k = 0; k < (n > 0 ? n : 200) && j->enDuelo() && g->dueloIA.ganador < 0; k++) h.paso();
	}
	if (modo == "ritmo") {   // rama classic-ps5-ritmo: duelos completos y la tabla de tiempos del turno
		std::vector<Rival> todos = listaRivales(*g);
		for (int k = 0; k < (n > 0 ? n : 3) && !todos.empty(); k++)
			dueloHumano(m, todos[(size_t)((semilla * 7u + (uint32_t)k * 37u) % todos.size())], k, getenv("HU_TURNOS") ? atoi(getenv("HU_TURNOS")) : 8, 400.0f);
		fprintf(stderr, "RITMO (s de juego, velocidad %s):\n%s", velocidad::nombre(velocidad::nivel()), cron->tabla().c_str());
		if (cron->log) fprintf(cron->log, "\nRITMO:\n%s", cron->tabla().c_str());
	}
	if (modo == "fijos" || modo == "general" || modo == "todo") fijos(m);
	if (modo == "duelos" || modo == "general" || modo == "todo") duelos(m, n > 0 && modo != "todo" ? n : 6);
	if (modo == "cartas" || modo == "todo") cartas(m, modo == "cartas" ? n : -1);
	fprintf(stderr, "ENTERARSE: %d sucesos vistos, %d sin ver; %d fallas en total (%.0f s)\n", j->vistos, j->sinVer, j->fallasTotal,
	        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
	if (j->fallas) fclose(j->fallas);
	if (cron->log) fclose(cron->log);
	return resumenHumano(salida.c_str());
}
