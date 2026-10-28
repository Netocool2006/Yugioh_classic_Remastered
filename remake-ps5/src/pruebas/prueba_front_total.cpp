// La bateria de casos de uso DESDE EL FRONT (rama classic-front-total; cu_front_total.h): duelos completos contra cada
// tipo de rival y carta por carta, jugando con el mando sobre el juego sin GPU (cu_juego.h). Escribe la tabla de
// resultados (TSV: id, estado, detalle) para ciclo_completo.sh y el resumen.
// Compilar y correr (contenedor arch-ps5):
//   g++ -std=c++17 -O2 -DPS5_HOST_REFERENCE -I remake-ps5/src -I /work/PS5_ClassicRemaster/external/tinygltf \
//       -I /work/PS5_ClassicRemaster/external/glm remake-ps5/src/pruebas/prueba_front_total.cpp -o /tmp/prueba_front_total
//   /tmp/prueba_front_total <carpeta con ui/> <salida.tsv> [duelos|cartas|todo] [tope de duelos] [tope de cartas] [primera carta]
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#include "cu_front_total.h"
#include "../orientacion_figura.h"   // rama classic-bp-terminar: CU-FT-FIGURA-ORIENTACION*

#include <chrono>
#include <fstream>
#include <iterator>
#include <cstdlib>

using namespace cuft;

static double ahora()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// ------------------------------------------------------------------------------------------------- duelos
static void duelos(Bot& b, int tope)
{
	Juego& g = b.g;
	std::vector<Rival> rivales = listaRivales(g);
	// cada plan de la IA al menos una vez (con el primer personaje que lo saque con el cuadrado)
	for (int pl = 0; pl < ia::NUM_PLANES; pl++) {
		Rival x = rivales[0];
		x.plan = pl;
		x.nombre = std::string("PLAN-") + ia::PLANES[pl].clave;
		rivales.push_back(x);
	}
	fprintf(stderr, "== duelos: %d rivales x 2 (Preguntar antes de activar: Auto y Nunca; tope %d)\n", (int)rivales.size(), tope);
	int n = 0;
	for (int modo : { opciones::PREGUNTAR_AUTO, opciones::PREGUNTAR_NUNCA })
	for (Rival& r : rivales) {
		if (tope >= 0 && n >= tope) break;
		n++;
		opciones::preguntar() = modo;   // (Opciones > Preguntar antes de activar)
		const double t0 = ahora();
		const std::string sufijo = modo == opciones::PREGUNTAR_AUTO ? "-AUTO" : "-NUNCA";
		const std::string id = "CU-FT-DUELO-" + r.nombre + sufijo;
		b.duelo = r.nombre + sufijo;
		bool elegido = elegirRival(b, r);
		if (!elegido && r.plan >= 0) {   // (el plan con otro personaje: el primero que lo tenga)
			for (int s = 0; s < personajes::NUM_SAGAS && !elegido; s++)
				for (int i = 0; i < (int)g.historia.deSaga(s).size() && !elegido; i++) {
					Rival y = r;
					y.saga = s, y.idx = i;
					elegido = elegirRival(b, y);
				}
		}
		if (!elegido) {
			b.caso(id, false, "no se pudo elegir el rival con el mando (pantalla %d, plan %d/%d, torneo %d/%d)", g.ctl.vista.pantalla, g.mazoRivalIA.plan, r.plan, g.mazoRivalIA.torneo,
			       r.torneo);
			continue;
		}
		const std::string plan = g.mazoRivalIA.plan >= 0 ? ia::PLANES[g.mazoRivalIA.plan].clave : "-";
		const Bot::ResDuelo res = b.jugarDuelo(r.nombre + sufijo, 40, 1500.0f);
		b.caso(id, res.empezo, "el duelo no empezo (sorteo)");
		b.caso(id, !res.atasco, "el duelo se atasco (turno %d)", res.turnos);
		fprintf(stderr, "duelo %3d %-48s plan %-11s %s turnos %2d gana %2d botones %4d %s (%.1f s)\n", n, r.nombre.c_str(), plan.c_str(), res.termino ? "fin" : "tope", res.turnos,
		        res.ganador, res.acciones, res.atasco ? "ATASCO" : "", ahora() - t0);
		if (res.atasco) fprintf(stderr, "  guion para reproducir: %s\n", b.botonesDuelo.substr(b.botonesDuelo.size() > 2000 ? b.botonesDuelo.size() - 2000 : 0).c_str());
		// del resultado a elegir rival
		for (int k = 0; k < 8 && g.ctl.vista.pantalla == VD::RESULTADO; k++) b.apretar(PAD_CIRCLE, 0.5f);
	}
	opciones::preguntar() = opciones::PREGUNTAR_AUTO;
}

// ------------------------------------------------------------------------------------------------- carta por carta
static std::vector<std::string> mazoVariado(const ia::Base& base)
{
	// (como prueba_certificacion.cpp: un monstruo de nivel 1-4 de cada tipo y atributo, mas magias y trampas)
	std::vector<std::string> r;
	std::set<std::string> razas, atributos;
	for (const ia::Carta& c : base.cartas) {
		if (!c.monstruo || c.nivel > 4 || c.nivel < 1 || c.subtipo != "normal") continue;
		if (!razas.count(c.raza) || !atributos.count(c.atributo)) {
			razas.insert(c.raza);
			atributos.insert(c.atributo);
			r.push_back(c.id);
		}
	}
	for (const char* id : { "mystical_space_typhoon", "monster_reborn", "dark_hole", "trap_hole", "pot_of_greed", "kuriboh", "sangan", "dark_magician", "summoned_skull",
	                        "baby_dragon", "celtic_guardian", "mystical_elf", "feral_imp", "polymerization", "toon_world", "axe_of_despair", "gather_your_mind" })
		if (base.carta(id)) r.push_back(id);
	while (r.size() < 40) r.push_back(r[r.size() % 7]);
	return r;
}

// el estado del motor que importa (para comparar el front con el motor)
static std::string resumenDuelo(const ia::Duelo& d)
{
	std::string s;
	char b[96];
	for (int lado = 0; lado < 2; lado++) {
		const ia::Lado& x = d.l[(size_t)lado];
		snprintf(b, sizeof(b), "[%d] lp %d mazo %d extra %d | mano:", lado, x.lp, (int)x.mazo.size(), (int)x.extra.size());
		s += b;
		for (const std::string& c : x.mano) s += " " + c;
		s += " | m:";
		for (const ia::Monstruo& m : x.m) {
			snprintf(b, sizeof(b), " %s%s%s(%d/%d)", m.id.c_str(), m.bocaAbajo ? "~" : "", m.defensa ? "d" : "", m.atkEf, m.defEf);
			s += m.vacia() ? std::string(" -") : std::string(b);
		}
		s += " | mt:";
		for (const ia::MagiaTrampa& t : x.mt) s += " " + (t.vacia() ? std::string("-") : t.id + (t.bocaAbajo ? "~" : ""));
		s += " campo " + x.campo.id + " | cem:";
		for (const std::string& c : x.cementerio) s += " " + c;
		s += " | dest:";
		for (const std::string& c : x.desterrados) s += " " + c;
		s += "\n";
	}
	snprintf(b, sizeof(b), "turno %d fase %d activo %d ganador %d", d.turno, (int)d.fase, d.activo, d.ganador);
	return s + b;
}
static std::string primeraDiferencia(const std::string& a, const std::string& b)
{
	size_t k = 0;
	while (k < a.size() && k < b.size() && a[k] == b[k]) k++;
	const size_t i = a.rfind('\n', k) == std::string::npos ? 0 : a.rfind('\n', k) + 1;
	return "front: " + a.substr(i, 160) + " || motor: " + b.substr(std::min(i, b.size()), 160);
}

// el duelo de la prueba de una carta (como el escenario A de la certificacion): turno 3, tu fase principal 1, 3
// monstruos propios (sacrificios) y 3 del rival, cementerios con cartas, el rival sin cartas que pueda activar
static cub::Mesa escenaBase(const ia::Base& base, const std::vector<std::string>& mazo)
{
	cub::Mesa m(&base);
	m.d.l[0].mazo = mazo;
	m.d.l[1].mazo.clear();
	for (int k = 0; k < 30; k++) m.d.l[1].mazo.push_back(k % 2 ? "celtic_guardian" : "battle_ox");
	m.mon(0, "celtic_guardian", true);
	m.mon(0, "battle_ox");
	m.mon(0, "mystical_elf");
	m.mon(1, "baby_dragon");
	m.mon(1, "dark_magician");
	m.mon(1, "giant_soldier_of_stone", true, true);
	m.colocar(1, "pot_of_greed");
	m.d.l[0].cementerio = { "summoned_skull", "mystical_elf", "dark_magician", "pot_of_greed", "trap_hole", "baby_dragon", "feral_imp", "dark_assailant", "aqua_madoor", "lesser_dragon" };
	m.d.l[1].cementerio = { "summoned_skull", "feral_imp", "pot_of_greed", "mirror_force", "celtic_guardian", "hitotsu_me_giant", "fireyarou" };
	m.d.l[0].mano = { "celtic_guardian", "mystical_elf" };
	m.d.l[1].mano = { "vorse_raider", "celtic_guardian" };
	return m;
}

// pone el duelo d en la mesa del juego (como si se hubiera llegado jugando): la mesa sincronizada y nada pendiente
static void inyectar(Bot& b, const ia::Duelo& d)
{
	Juego& g = b.g;
	g.dueloIA = d;
	g.dueloIA.recalcular();
	g.sucesosVistos = g.dueloIA.sucesos.size();
	g.reparto = 0;
	g.ocultas[0] = g.ocultas[1] = 0;
	g.esperaEfecto = 0.0f;
	g.tAtaque = -1.0f;
	g.zonaAtacante = g.zonaObjetivo = -1;
	g.ctl.preparar({}, {}, {});
	g.ctl.vuelo.activo = g.ctl.robo.activo = false;
	g.ctl.enPregunta = g.ctl.armando = g.ctl.eleccionDuelo = false;
	g.ctl.estado = control::NAVEGAR;
	g.interfazDuelo.cambiarLP(false, d.l[0].lp);
	g.interfazDuelo.cambiarLP(true, d.l[1].lp);
	g.sincronizarMesaDuelo();
	g.turnoIA = ia::Turno(&g.dueloIA, 1, ia::PERSONALIDADES[0], 4, 77u);
	b.reiniciarVigia();
}

static int indiceMano(const ia::Duelo& d, const std::string& id)
{
	for (int k = (int)d.l[0].mano.size(); k-- > 0;)
		if (d.l[0].mano[(size_t)k] == id) return k;
	return -1;
}
static bool hubo(const ia::Duelo& d, size_t desde, std::initializer_list<int> tipos, const std::string& carta, int lado = 0)
{
	for (size_t k = desde; k < d.sucesos.size(); k++) {
		const ia::Suceso& s = d.sucesos[k];
		if (s.lado != lado || s.carta != carta) continue;
		for (int t : tipos)
			if (s.tipo == t) return true;
	}
	return false;
}
// las preguntas que quedan: pasar o elegir la primera (lo mismo que hace el bot)
static void pasarTodo(ia::Duelo& d)
{
	for (int k = 0; k < 40 && d.ef.preg.tipo != ia::P_NADA && d.ganador < 0; k++) {
		ia::Jugada j;
		j.tipo = d.ef.preg.tipo == ia::P_ELEGIR ? ia::J_ELEGIR : ia::J_PASAR;
		j.objetivo = 0;
		if (!d.aplicar(j)) break;
	}
}
// la activacion canonica del motor (como las pruebas del motor): objetivos y descartes, los primeros; costes, los de la IA
static ia::Jugada activacionMotor(const ia::Duelo& d, const ia::Lugar& f)
{
	ia::Jugada j = ia::jugadaActivar(f);
	j.carta = ia::cartaEn(d, f);
	const ia::DefCarta* df = ia::defCarta(j.carta);
	if (!df) return j;
	for (int k = 0; k < (int)d.l[0].mano.size() && (int)j.descartes.size() < df->descartes; k++) {
		if (f.donde == ia::EN_MANO && k == f.i) continue;
		if (!ia::descarteVale(d, j.carta, d.l[0].mano[(size_t)k])) continue;
		j.descartes.push_back(k);
	}
	const std::vector<ia::Lugar> objs = ia::objetivosDe(d, 0, *df, f);
	for (int k = 0; k < df->nObj && k < (int)objs.size(); k++) j.objs.push_back(objs[(size_t)k]);
	j.costes = ia::costesIA(d, 0, *df, f);
	return j;
}

static int contar(const std::vector<std::string>& v, const std::string& id) { return (int)std::count(v.begin(), v.end(), id); }

struct Cuentas { int cartas{ 0 }, jugadas{ 0 }, activadas{ 0 }, iguales{ 0 }, sinOcasion{ 0 }, ciclos{ 0 }; std::vector<std::string> sinOcasionIds; };

// la carta "id" jugada con el mando desde donde le toca; comprueba lo que hizo el motor, la mesa y que el front hizo
// exactamente lo mismo que el motor
static void probarCarta(Bot& b, const ia::Carta& c, const std::vector<std::string>& mazo, Cuentas& cu)
{
	Juego& g = b.g;
	const std::string& id = c.id;
	const std::string cid = "CU-FT-CARTA-" + id;
	const ia::Base& base = g.baseIA;
	b.duelo = id;
	b.botones.clear();
	b.botonesDuelo.clear();
	b.contexto = "CARTA";
	cu.cartas++;
	cub::Mesa m = escenaBase(base, mazo);
	const bool contacto = c.clase == "fusion" && ia::fusionContacto(id);
	if (c.clase == "fusion") {
		auto it = base.fusiones.find(id);
		if (it == base.fusiones.end()) {
			b.caso(cid + "-JUGAR", false, "fusion sin receta");
			return;
		}
		m.d.l[0].extra = { id };
		if (contacto) {
			for (ia::Monstruo& x : m.d.l[0].m) x = ia::Monstruo{};
			for (const std::string& mat : it->second) m.mon(0, mat);
		} else {
			m.mano(0, "polymerization");
			for (const std::string& mat : it->second) m.mano(0, mat);
		}
	} else if (c.clase == "ritual") {
		std::string magia;
		auto it = base.rituales.find(id);
		if (it != base.rituales.end()) magia = it->second;
		if (magia.empty())
			for (const ia::Carta& k : base.cartas)
				if (k.subtipo == "magia_ritual" && base.ritualCon(id, k.id)) magia = k.id;
		if (magia.empty()) {
			b.caso(cid + "-JUGAR", false, "sin magia ritual que la invoque");
			return;
		}
		m.mano(0, magia);
		m.mano(0, id);
		m.mon(0, "battle_ox");   // (los niveles: 4 sacrificios de nivel 4)
	} else {
		if (c.subtipo == "toon") {
			const int w = m.colocar(0, "toon_world");
			m.d.l[0].mt[(size_t)w].bocaAbajo = false;
		}
		if (c.monstruo) {
			cuc::condicionInvocar(m, id);
			if (id == "terrorking_archfiend") m.mon(0, "archfiend_soldier");
		}
		m.mano(0, id);
	}
	m.d.recalcular();
	inyectar(b, m.d);
	b.esperar(0.3f);
	const ia::Duelo antes = g.dueloIA;
	const size_t s0 = g.dueloIA.sucesos.size();
	b.grabadas.clear();
	b.grabar = true;
	b.rivalPorBot = true;
	b.probResponder = 0.0f;
	b.cartaPreferida.clear();
	// donde se juega y que opcion
	Foco foco{ F_MANO_J, 0 };
	std::function<bool(const control::Opcion&)> es;
	const char* que = "";
	bool esperaActivar = false;
	if (c.clase == "fusion") {
		que = "fusion";
		if (contacto) {
			foco = Foco{ F_ZONA, Juego::zonaMesaIA(0, 0) };
			for (int i = 0; i < ia::ZONAS; i++)
				if (!g.dueloIA.l[0].m[(size_t)i].vacia()) {
					foco = Foco{ F_ZONA, Juego::zonaMesaIA(0, i) };
					break;
				}
		} else {
			foco = Foco{ F_MANO_J, indiceMano(g.dueloIA, "polymerization") };
		}
		es = [&](const control::Opcion& o) { return o.accion == control::AC_JUGADA && o.jugada.tipo == ia::J_FUSION && o.jugada.carta == id; };
	} else if (c.clase == "ritual") {
		que = "ritual";
		foco = Foco{ F_MANO_J, indiceMano(g.dueloIA, antes.l[0].mano[antes.l[0].mano.size() - 2]) };
		es = [&](const control::Opcion& o) { return o.accion == control::AC_JUGADA && o.jugada.tipo == ia::J_RITUAL && o.jugada.carta == id; };
	} else {
		foco = Foco{ F_MANO_J, indiceMano(g.dueloIA, id) };
		const std::vector<control::Opcion> ops = g.ctl.opcionesPara(foco);
		auto activa = [&](const std::function<bool(const control::Opcion&)>& f) {
			for (const control::Opcion& o : ops)
				if (o.activa && f(o)) return true;
			return false;
		};
		auto activar = [](const control::Opcion& o) { return o.accion == control::AC_ACTIVAR_EFECTO || o.accion == control::AC_ACTIVAR_MAGIA; };
		if (c.monstruo) {
			auto invocar = [](const control::Opcion& o) { return o.accion == control::AC_INVOCAR; };
			auto toon = [](const control::Opcion& o) { return o.accion == control::AC_TOON; };
			auto colocar = [](const control::Opcion& o) { return o.accion == control::AC_COLOCAR_MONSTRUO; };
			auto especial = [](const control::Opcion& o) { return o.accion == control::AC_ACTIVAR_EFECTO; };
			if (activa(toon)) es = toon, que = "toon";
			else if (activa(invocar)) es = invocar, que = "invocar";
			else if (activa(especial)) es = especial, que = "efecto", esperaActivar = true;
			else if (activa(colocar)) es = colocar, que = "colocar";
			else es = invocar, que = "invocar (desactivada)";
		} else if (c.magia && activa(activar)) {
			es = activar, que = "activar", esperaActivar = true;
		} else {
			es = [](const control::Opcion& o) { return o.accion == control::AC_COLOCAR_MT; }, que = "colocar";
		}
	}
	// paridad con el motor: invocar normal (con los primeros sacrificios) o activar (motivoNoActivar)
	bool frontActiva = false;
	for (const control::Opcion& o : g.ctl.opcionesPara(foco)) frontActiva |= o.activa && es && es(o);
	int r = foco.i < 0 ? 0 : b.accion(foco, es);
	b.asentar();
	const ia::Duelo& d = g.dueloIA;
	bool jugo = false;
	if (strcmp(que, "invocar") == 0 || strcmp(que, "toon") == 0 || strcmp(que, "fusion") == 0 || strcmp(que, "ritual") == 0) jugo = hubo(d, s0, { ia::S_INVOCA }, id);
	else if (strcmp(que, "colocar") == 0) jugo = hubo(d, s0, { ia::S_COLOCA, ia::S_COLOCA_MT }, id);
	else if (esperaActivar) jugo = hubo(d, s0, { ia::S_ACTIVA_MT, ia::S_ACTIVA_EFECTO, ia::S_INVOCA }, id) || hubo(d, s0, { ia::S_INVOCA }, id, 1);   // (Lava Golem: al campo del rival)
	if (strcmp(que, "invocar (desactivada)") == 0) {
		// no se puede invocar normal: el motor tampoco (con los mismos sacrificios), y el menu dio el motivo (r == 2)
		ia::Duelo cp = antes;
		ia::Jugada j;
		j.tipo = ia::J_INVOCAR;
		j.mano = indiceMano(cp, id);
		const int n = ia::motorTributos(cp, 0, &c);
		for (int i = 0; i < ia::ZONAS && (int)j.tributos.size() < n; i++)
			if (!cp.l[0].m[(size_t)i].vacia()) j.tributos.push_back(i);
		const bool motor = cp.aplicar(j);
		b.caso(cid + "-JUGAR", !motor && r == 2, "el menu no deja invocarla (%d) y el motor %s", r, motor ? "si la invoca" : "tampoco");
	} else {
		b.caso(cid + "-JUGAR", r == 1 && jugo, "%s con el mando: menu %d, el motor %s (aviso '%s')", que, r, jugo ? "lo hizo" : "no lo hizo", g.ctl.aviso.c_str());
		if (r == 1 && jugo) cu.jugadas++;
	}
	if (frontActiva || r == 1) {
		const std::string dif = b.difMesa();
		b.caso(cid + "-JUGAR", dif.empty() || !b.alDia(), "la mesa no muestra lo que hizo el motor: %s", dif.c_str());
	}
	// el efecto: el front hizo lo mismo que el motor con las mismas jugadas (repeticion en una copia)
	if (esperaActivar || strcmp(que, "fusion") == 0 || strcmp(que, "ritual") == 0) {
		if (jugo) cu.activadas++;
		ia::Duelo rep = antes;
		bool mismas = true;
		for (const Grabada& x : b.grabadas) mismas &= rep.aplicar(x.j) == x.ok;
		const std::string a = resumenDuelo(g.dueloIA), bb = resumenDuelo(rep);
		if (jugo) b.caso(cid + "-EFECTO", mismas && a == bb, "el front y el motor no terminan igual con las mismas jugadas: %s", primeraDiferencia(a, bb).c_str());
		// y la activacion canonica del motor (objetivos y descartes los primeros) con las mismas elecciones da lo mismo
		if (esperaActivar && jugo && !b.grabadas.empty() && b.grabadas[0].lado == 0) {
			const ia::Lugar f{ 0, ia::EN_MANO, foco.i };
			ia::Duelo can = antes;
			const ia::Jugada jc = activacionMotor(can, f);
			const ia::Jugada& jf = b.grabadas[0].j;
			if (jc.objs == jf.objs && jc.descartes == jf.descartes && jc.costes == jf.costes) {
				const bool okc = can.aplicar(jc);
				pasarTodo(can);
				const std::string cc = resumenDuelo(can);
				// (el front puede haber elegido otra cosa en las preguntas al resolver: solo si no hubo elecciones)
				bool soloPasar = true;
				for (size_t k = 1; k < b.grabadas.size(); k++) soloPasar &= b.grabadas[k].j.tipo == ia::J_PASAR || b.grabadas[k].j.objetivo == 0;
				if (soloPasar) {
					b.caso(cid + "-EFECTO", okc && cc == a, "la activacion del motor (como en su prueba) termina distinto que la del front: %s", primeraDiferencia(a, cc).c_str());
					if (okc && cc == a) cu.iguales++;
				}
			}
		}
	}
	b.grabar = false;
	// (pedido 2) activada desde la mano: las normales, rapidas y rituales terminan en el cementerio; la de campo queda
	// en su zona y cambia el escenario (fondo y tablero)
	if (c.magia && esperaActivar && jugo) {
		const ia::Duelo& d2 = g.dueloIA;
		const bool resolvio = hubo(d2, s0, { ia::S_RESUELVE }, id);
		if (c.subtipo == "magia_normal" || c.subtipo == "magia_rapida" || c.subtipo == "magia_ritual") {
			bool enZona = false;
			for (const ia::MagiaTrampa& x : d2.l[0].mt) enZona |= x.id == id;
			// (las que su efecto deja en la mesa, Swords of Revealing Light o Different Dimension Capsule, siguen ahi)
			const bool otroLado = contar(d2.l[0].desterrados, id) + contar(d2.l[0].mazo, id) + contar(d2.l[0].mano, id) >
			                      contar(antes.l[0].desterrados, id) + contar(antes.l[0].mazo, id) + contar(antes.l[0].mano, id) - 1;
			if (resolvio && !enZona) b.caso(cid + "-EFECTO", contar(d2.l[0].cementerio, id) > contar(antes.l[0].cementerio, id) || otroLado, "la magia %s resuelta salio de la mesa y no termino en el cementerio",
			                                c.subtipo.c_str());
		} else if (c.subtipo == "magia_campo") {
			b.caso(cid + "-EFECTO", d2.l[0].campo.id == id && g.ctl.zonas[(size_t)control::zonaPorId("jugador_campo")].carta == id, "la magia de campo no quedo en su zona (motor '%s')",
			       d2.l[0].campo.id.c_str());
			if (campos::esCampo(id))
				b.caso(cid + "-EFECTO", std::find(g.camposActivados.begin(), g.camposActivados.end(), id) != g.camposActivados.end(), "la magia de campo no cambio el escenario");
		}
	}
	// colocada desde antes (boca abajo en la mesa): activarla desde su zona; la trampa que no se puede en tu turno, en
	// el turno del rival (la IA juega y el juego la ofrece: ataque, invocacion, fin del rival)
	if (!c.monstruo && c.subtipo != "magia_campo") {
		cub::Mesa m2 = escenaBase(base, mazo);
		const int z = m2.colocar(0, id, 1);
		m2.d.recalcular();
		inyectar(b, m2.d);
		b.esperar(0.3f);
		const ia::Duelo antes2 = g.dueloIA;
		const size_t s2 = g.dueloIA.sucesos.size();
		const Foco fz{ F_ZONA, Juego::zonaMesaIA(0, z, true) };
		b.caso(cid + "-COLOCADA", g.ctl.zonas[(size_t)fz.i].carta == id && g.ctl.zonas[(size_t)fz.i].bocaAbajo, "colocada: la mesa no la muestra boca abajo en su zona");
		bool activable = false;
		for (const control::Opcion& o : g.ctl.opcionesPara(fz)) activable |= o.activa && o.accion == control::AC_ACTIVAR_EFECTO;
		b.grabadas.clear();
		b.grabar = true;
		if (activable) {
			const int r2 = b.accion(fz, [](const control::Opcion& o) { return o.accion == control::AC_ACTIVAR_EFECTO; });
			b.asentar();
			const bool act = hubo(g.dueloIA, s2, { ia::S_ACTIVA_MT, ia::S_ACTIVA_EFECTO }, id);
			b.caso(cid + "-EFECTO", r2 == 1 && act, "colocada: 'Activar' activa en el menu y el motor no la activo (%d)", r2);
			if (act) {
				cu.activadas++;
				ia::Duelo rep = antes2;
				bool mismas = true;
				for (const Grabada& x : b.grabadas) mismas &= rep.aplicar(x.j) == x.ok;
				const std::string a = resumenDuelo(g.dueloIA), bb = resumenDuelo(rep);
				b.caso(cid + "-EFECTO", mismas && a == bb, "colocada: el front y el motor no terminan igual: %s", primeraDiferencia(a, bb).c_str());
				// (resuelta y fuera de la mesa: al cementerio; las que su efecto deja en la mesa o equipa, siguen ahi)
				bool sigue = false;
				for (const ia::MagiaTrampa& x : g.dueloIA.l[0].mt) sigue |= x.id == id;
				const ia::Lado& l2 = g.dueloIA.l[0];
				const bool otroLado = contar(l2.desterrados, id) + contar(l2.mazo, id) + contar(l2.mano, id) > contar(antes2.l[0].desterrados, id) + contar(antes2.l[0].mazo, id) + contar(antes2.l[0].mano, id);
				if ((c.subtipo == "magia_normal" || c.subtipo == "magia_rapida" || c.subtipo == "trampa_normal" || c.subtipo == "trampa_contraataque") && hubo(g.dueloIA, s2, { ia::S_RESUELVE }, id) && !sigue)
					b.caso(cid + "-EFECTO", contar(l2.cementerio, id) > contar(antes2.l[0].cementerio, id) || otroLado, "colocada: resuelta (%s) salio de la mesa y no termino en el cementerio",
					       c.subtipo.c_str());
			}
		} else if (!c.trampa) {
			// (magia colocada que ahora no se puede activar: el motor tampoco)
			b.caso(cid + "-EFECTO", !ia::motivoNoActivar(antes2, 0, { 0, ia::EN_MT, z }, false).empty(), "colocada: el motor deja activarla y el menu no");
		} else {
			// el motor tampoco la deja activar ahora
			b.caso(cid + "-EFECTO", !ia::motivoNoActivar(antes2, 0, { 0, ia::EN_MT, z }, false).empty(), "el motor deja activarla colocada y el menu no");
			// el turno del rival: la IA juega y el juego ofrece la trampa cuando corresponde
			cu.ciclos++;
			const int rechazos0 = b.fallasPorCaso["CU-FT-PREGUNTA-RESPUESTA"] + b.fallasPorCaso["CU-FT-ATASCO-UI"];
			b.grabar = false;
			b.rivalPorBot = false;
			b.cartaPreferida = id;
			b.contexto = "CARTA-RIVAL";
			for (int k = 0; k < 8 && g.dueloIA.activo == 0 && g.dueloIA.ganador < 0 && !b.atascado; k++) {
				b.asentar();
				if (b.libre()) b.pasarFase();
			}
			for (float t = 0.0f; t < 120.0f && g.dueloIA.activo == 1 && g.dueloIA.ganador < 0 && !b.atascado; t += 0.1f) {
				if (g.ctl.estado != control::NAVEGAR && g.ctl.estado != control::VOLANDO && !g.ctl.bloqueado) b.resolverUI();
				else b.esperar(0.1f);
			}
			b.asentar();
			const bool act = hubo(g.dueloIA, s2, { ia::S_ACTIVA_MT, ia::S_ACTIVA_EFECTO }, id);
			if (act) {
				cu.activadas++;
				b.caso(cid + "-EFECTO", true, "");
				const std::string dif = b.difMesa();
				b.caso(cid + "-EFECTO", dif.empty() || !b.alDia(), "tras activarla en el turno del rival la mesa no es el motor: %s", dif.c_str());
			} else if (b.fallasPorCaso["CU-FT-PREGUNTA-RESPUESTA"] + b.fallasPorCaso["CU-FT-ATASCO-UI"] > rechazos0) {
				b.caso(cid + "-EFECTO", false, "el juego la ofrece en el turno del rival y rechaza la activacion del jugador (la pregunta vuelve a abrirse y el duelo queda trabado)");
			} else {
				cu.sinOcasion++;
				cu.sinOcasionIds.push_back(id);
			}
			b.cartaPreferida.clear();
		}
		b.grabar = false;
	}
}

static void cartas(Bot& b, int tope, const std::string& desde)
{
	Juego& g = b.g;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	// un duelo de verdad para estar en la pantalla del duelo (despues cada carta se arma en su escenario)
	Rival r{ VD::LIBRE, 0, 0, 1, 0, -1, -1, "cartas" };
	if (!elegirRival(b, r) || !b.resolverInicio(true)) {
		b.caso("CU-FT-CARTA-INICIO", false, "no se pudo empezar el duelo para la prueba carta por carta");
		return;
	}
	b.caso("CU-FT-CARTA-INICIO", true, "");
	Cuentas cu;
	bool empezar = desde.empty();
	const double t0 = ahora();
	for (const ia::Carta& c : g.baseIA.cartas) {
		if (!empezar && c.id == desde) empezar = true;
		if (!empezar) continue;
		if (tope >= 0 && cu.cartas >= tope) break;
		probarCarta(b, c, mazo, cu);
		if (g.ctl.vista.pantalla != VD::DUELO) {   // (la carta termino el duelo: otro)
			Rival r2 = r;
			if (!elegirRival(b, r2) || !b.resolverInicio(true)) break;
		}
		if (cu.cartas % 100 == 0) fprintf(stderr, "cartas: %d (%.0f s)\n", cu.cartas, ahora() - t0);
	}
	// autoprueba de los vigilantes (que no pasen por no mirar): la mesa girada a mano tiene que verse distinta del
	// motor, y un monstruo del motor que cambia de posicion sin suceso tiene que dar falla
	if (g.ctl.vista.pantalla == VD::DUELO) {
		cub::Mesa m = escenaBase(g.baseIA, mazo);
		inyectar(b, m.d);
		b.esperar(0.5f);
		const int z = Juego::zonaMesaIA(0, 1);
		b.caso("CU-FT-VIGIA-AUTOPRUEBA", b.difMesa().empty(), "la mesa recien sincronizada no coincide: %s", b.difMesa().c_str());
		g.ctl.zonas[(size_t)z].defensa = !g.ctl.zonas[(size_t)z].defensa;
		b.caso("CU-FT-VIGIA-AUTOPRUEBA", !b.difMesa().empty(), "una carta girada en la mesa y no en el motor no se detecta");
		g.ctl.zonas[(size_t)z].defensa = !g.ctl.zonas[(size_t)z].defensa;
		const int antes = cu::de("CU-FT-POSICION-MOTOR").comprobaciones, fallas = b.fallasPorCaso["CU-FT-POSICION-MOTOR"];
		const std::string estado0 = cu::de("CU-FT-POSICION-MOTOR").estado, detalle0 = cu::de("CU-FT-POSICION-MOTOR").detalle;
		g.dueloIA.l[0].m[1].defensa = !g.dueloIA.l[0].m[1].defensa;
		b.vigilar();
		const bool detecto = b.fallasPorCaso["CU-FT-POSICION-MOTOR"] > fallas;
		// (se deshace la falla provocada: era la autoprueba)
		cu::Resultado& r = cu::de("CU-FT-POSICION-MOTOR");
		r.estado = estado0, r.detalle = detalle0;
		b.fallasPorCaso["CU-FT-POSICION-MOTOR"] = fallas;
		b.caso("CU-FT-VIGIA-AUTOPRUEBA", detecto && r.comprobaciones > antes, "un cambio de posicion del motor sin suceso no se detecta");
	}
	fprintf(stderr, "CARTAS DESDE EL FRONT: %d cartas, %d jugadas con el mando, %d activadas, %d iguales a la activacion del motor, %d ciclos de turno, %d sin ocasion de activarse (%.0f s)\n",
	        cu.cartas, cu.jugadas, cu.activadas, cu.iguales, cu.ciclos, cu.sinOcasion, ahora() - t0);
	std::string s;
	for (const std::string& x : cu.sinOcasionIds) s += x + " ";
	fprintf(stderr, "sin ocasion: %s\n", s.c_str());
}

// ------------------------------------------------------------------------------------------------- combate (pedido 1)
// Cada monstruo, con el mando: ataca a un monstruo en ataque y a uno boca abajo en defensa (se voltea; rebote si la DEF
// es mayor, perforacion si la carta perfora), y recibe el ataque del rival en ataque, en defensa boca arriba y en
// defensa boca abajo (se voltea). Comprueba: los LP (la regla de siempre con monstruos normales; con efectos, lo mismo
// que el motor con las mismas jugadas), el volteo, que los destruidos van al cementerio de su duenno, la mesa, el
// contador y el visor del cementerio (con el mando: X en la zona del cementerio).
struct ResCombate { int danioA{ 0 }, danioD{ 0 }; bool muereA{ false }, muereD{ false }; };
static ResCombate reglaCombate(int atk, int otro, bool defensa, bool perfora)
{
	ResCombate r;
	if (!defensa) {
		if (atk > otro) r.muereD = true, r.danioD = atk - otro;
		else if (atk < otro) r.muereA = true, r.danioA = otro - atk;
		else if (atk > 0) r.muereA = r.muereD = true;
	} else {
		if (atk > otro) r.muereD = true, r.danioD = perfora ? atk - otro : 0;
		else if (atk < otro) r.danioA = otro - atk;
	}
	return r;
}
// el visor del cementerio de un lado con el mando (flechas hasta la zona, X, contar, circulo)
static void mirarCementerio(Bot& b, const std::string& id, int lado)
{
	Juego& g = b.g;
	if (g.ctl.estado != control::NAVEGAR) return;
	const Foco f{ F_ZONA, control::zonaPorId(lado ? "rival_cementerio" : "jugador_cementerio") };
	if (!b.irA(f)) {
		b.caso(id, false, "no se llega con las flechas al cementerio %s", lado ? "del rival" : "propio");
		return;
	}
	b.apretar(PAD_CROSS, 0.2f);
	const int n = (int)g.dueloIA.l[(size_t)lado].cementerio.size();
	if (n == 0) {
		if (g.consulta.vis.abierto) b.apretar(PAD_CIRCLE, 0.2f);
		return;
	}
	b.caso(id, g.consulta.vis.abierto && g.consulta.vis.pestana == visores::P_CEMENTERIO && g.consulta.vis.lado == lado && g.consulta.filas(g.ctl) == n,
	       "el visor del cementerio %s muestra %d cartas y el motor tiene %d", lado ? "del rival" : "propio", g.consulta.vis.abierto ? g.consulta.filas(g.ctl) : -1, n);
	if (g.consulta.vis.abierto) b.apretar(PAD_CIRCLE, 0.2f);
}
// hasta que termina el combate (o el duelo), contestando lo que se pregunte
static void esperarFinCombate(Bot& b, float maximo = 25.0f)
{
	Juego& g = b.g;
	for (float t = 0.0f; t < maximo && !b.atascado; t += g.frameTimer) {
		const ia::Duelo& d = g.dueloIA;
		if (d.ganador >= 0 && b.alDia()) return;
		if (b.alDia() && d.ef.preg.tipo == ia::P_NADA && d.ef.cadena.empty() && !d.ocupado() && g.ctl.estado == control::NAVEGAR) {
			b.esperar(0.1f);
			if (b.alDia() && g.dueloIA.ef.preg.tipo == ia::P_NADA) return;
		}
		if (g.ctl.estado != control::NAVEGAR && g.ctl.estado != control::VOLANDO && !g.ctl.bloqueado) b.resolverUI(8.0f);
		else b.cuadro(0);
	}
}

static void combateCarta(Bot& b, const ia::Carta& c, const std::vector<std::string>& mazo, int& subcasos)
{
	Juego& g = b.g;
	const ia::Base& base = g.baseIA;
	const std::string& id = c.id;
	const std::string cid = "CU-FT-COMBATE-" + id;
	const bool normal = c.clase == "normal";
	const bool perfora = ia::motorPerfora(id);
	static const char* const NOMBRE[] = { "ataca a uno en ataque", "ataca a uno boca abajo en defensa", "recibe el ataque en defensa boca arriba",
		                                  "recibe el ataque boca abajo en defensa", "recibe el ataque en ataque" };
	for (int modo = 0; modo < 5; modo++) {
		b.duelo = id + " (" + NOMBRE[modo] + ")";
		b.contexto = "COMBATE";
		b.botones.clear();
		cub::Mesa m(&base, ia::F_BATALLA);
		m.d.l[0].mazo = mazo;
		m.d.l[1].mazo.assign(30, "celtic_guardian");
		const bool ataca = modo < 2;
		m.d.activo = ataca ? 0 : 1;
		// la carta y su rival (monstruos normales de valores conocidos)
		const int zc = m.mon(0, id, modo == 2 || modo == 3, modo == 3);
		std::string otro = modo == 0 ? "celtic_guardian" : modo == 1 ? (perfora ? "battle_ox" : "labyrinth_wall") : "summoned_skull";
		const int zo = m.mon(1, otro, modo == 1, modo == 1);
		if (zc < 0 || zo < 0) continue;   // (la carta no puede estar asi: Berserk Gorilla nunca en defensa, Jowgen sin monstruos del rival...)
		m.d.recalcular();
		inyectar(b, m.d);
		b.esperar(0.2f);
		const ia::Duelo antes = g.dueloIA;
		const size_t s0 = antes.sucesos.size();
		b.grabadas.clear();
		b.grabar = true;
		b.rivalPorBot = true;
		b.probResponder = 0.0f;
		b.cartaPreferida.clear();
		bool declarado = false;
		if (ataca) {
			const Foco f{ F_ZONA, Juego::zonaMesaIA(0, zc) };
			bool activa = false;
			std::string motivo;
			for (const control::Opcion& o : g.ctl.opcionesPara(f))
				if (o.accion == control::AC_ATACAR) activa = o.activa, motivo = o.motivo;
			const bool motor = antes.puedeAtacar(0, zc) && control::Mando::hayBlanco(antes, zc);
			if (!activa) {
				// no puede atacar (su efecto, Dioses que no atacan el turno que llegan...): el motor tampoco
				b.caso(cid, !motor, "%s: el menu no deja atacar ('%s') y el motor si", NOMBRE[modo], motivo.c_str());
				b.grabar = false;
				continue;
			}
			declarado = b.accion(f, [](const control::Opcion& o) { return o.accion == control::AC_ATACAR; }) == 1;
		} else {
			ia::Jugada j;
			j.tipo = ia::J_ATACAR;
			j.zona = zo;
			j.objetivo = zc;
			const bool ok = g.dueloIA.aplicar(j);
			b.grabadas.push_back({ j, ok, 1 });
			declarado = ok;
			if (!ok) {   // (no se puede atacar a la carta: su efecto; no hay combate que mirar)
				b.caso(cid, true, "");
				b.grabar = false;
				continue;
			}
		}
		esperarFinCombate(b);
		subcasos++;
		const ia::Duelo& d = g.dueloIA;
		const bool combatio = hubo(d, s0, { ia::S_ATAQUE, ia::S_COMBATE, ia::S_ATAQUE_NEGADO }, id, ataca ? 0 : 1) || [&] {
			for (size_t k = s0; k < d.sucesos.size(); k++)
				if (d.sucesos[k].tipo == ia::S_ATAQUE || d.sucesos[k].tipo == ia::S_COMBATE) return true;
			return false;
		}();
		if (!b.caso(cid, declarado && combatio, "%s: el ataque no se hizo con el mando (menu %d)", NOMBRE[modo], (int)declarado)) {
			b.grabar = false;
			continue;
		}
		// el volteo: el boca abajo atacado se da vuelta
		if (modo == 1 || modo == 3) {
			const std::string atacado = modo == 1 ? otro : id;
			bool volteo = false;
			for (size_t k = s0; k < d.sucesos.size(); k++) volteo |= (d.sucesos[k].tipo == ia::S_VOLTEA || d.sucesos[k].tipo == ia::S_POSICION) && d.sucesos[k].carta == atacado;   // (Gaap: boca arriba en ataque)
			bool negado = false;
			for (size_t k = s0; k < d.sucesos.size(); k++) negado |= d.sucesos[k].tipo == ia::S_ATAQUE_NEGADO;
			bool efecto = false;
			for (size_t k = s0; k < d.sucesos.size(); k++) efecto |= d.sucesos[k].tipo == ia::S_ACTIVA_EFECTO || d.sucesos[k].tipo == ia::S_ACTIVA_MT;
			// (o quedo boca arriba por otra via: Gaap pone las de defensa en ataque)
			const ia::Monstruo& fin = d.l[modo == 1 ? 1 : 0].m[(size_t)(modo == 1 ? zo : zc)];
			volteo |= fin.vacia() ? false : !fin.bocaAbajo;
			std::string vistos;
			for (size_t k = s0; k < d.sucesos.size() && vistos.size() < 300; k++) vistos += std::string(ia::NOMBRE_SUCESO[d.sucesos[k].tipo]) + ":" + d.sucesos[k].carta + " ";
			const bool estabaAbajo = antes.l[modo == 1 ? 1 : 0].m[(size_t)(modo == 1 ? zo : zc)].bocaAbajo;   // (Gaap ya la puso boca arriba al llegar)
			if (!negado && !efecto && estabaAbajo)
				b.caso(cid, volteo, "%s: el monstruo boca abajo (%s) no se volteo al ser atacado (al final: %s%s; sucesos: %s)", NOMBRE[modo], atacado.c_str(), fin.vacia() ? "vacia" : fin.id.c_str(),
				       fin.bocaAbajo ? " boca abajo" : "", vistos.c_str());
		}
		// los LP: con normales (sin efectos en el medio), la regla; si no, lo mismo que el motor con las mismas jugadas
		bool efectos = false;
		for (size_t k = s0; k < d.sucesos.size(); k++)
			efectos |= d.sucesos[k].tipo == ia::S_ACTIVA_EFECTO || d.sucesos[k].tipo == ia::S_ACTIVA_MT || d.sucesos[k].tipo == ia::S_NEGADO || d.sucesos[k].tipo == ia::S_ATAQUE_NEGADO;
		const ia::Monstruo& ma = antes.l[ataca ? 0 : 1].m[(size_t)(ataca ? zc : zo)];
		const ia::Monstruo& mo = antes.l[ataca ? 1 : 0].m[(size_t)(ataca ? zo : zc)];
		if ((normal || (perfora && modo == 1)) && !efectos) {
			const ResCombate e = reglaCombate(ma.atkEf, mo.defensa ? mo.defEf : mo.atkEf, mo.defensa, perfora && ataca);
			const int la = ataca ? 0 : 1, lo = 1 - la;
			const int dA = antes.l[(size_t)la].lp - d.l[(size_t)la].lp, dD = antes.l[(size_t)lo].lp - d.l[(size_t)lo].lp;
			b.caso(cid, dA == e.danioA && dD == e.danioD, "%s (%d contra %s %d): danno al atacante %d (esperado %d), al defensor %d (esperado %d)", NOMBRE[modo], ma.atkEf,
			       mo.defensa ? "DEF" : "ATK", mo.defensa ? mo.defEf : mo.atkEf, dA, e.danioA, dD, e.danioD);
			const bool vivoA = !d.l[(size_t)la].m[(size_t)(ataca ? zc : zo)].vacia(), vivoD = !d.l[(size_t)lo].m[(size_t)(ataca ? zo : zc)].vacia();
			b.caso(cid, vivoA == !e.muereA && vivoD == !e.muereD, "%s: destruidos atacante %d (esperado %d), defensor %d (esperado %d)", NOMBRE[modo], (int)!vivoA, (int)e.muereA,
			       (int)!vivoD, (int)e.muereD);
		} else {
			ia::Duelo rep = antes;
			bool mismas = true;
			for (const Grabada& x : b.grabadas) mismas &= rep.aplicar(x.j) == x.ok;
			const std::string a = resumenDuelo(d), bb = resumenDuelo(rep);
			b.caso(cid, mismas && a == bb, "%s: el front y el motor no terminan igual: %s", NOMBRE[modo], primeraDiferencia(a, bb).c_str());
		}
		// los destruidos van al cementerio de su duenno; la pantalla (LP, cementerios) = el motor; el visor con el mando
		for (int lado = 0; lado < 2; lado++) {
			const std::string carta = lado == 0 ? id : otro;
			const int z = lado == 0 ? zc : zo;
			if (!d.l[(size_t)lado].m[(size_t)z].vacia()) continue;
			const bool enSuCementerio = contar(d.l[(size_t)lado].cementerio, carta) > contar(antes.l[(size_t)lado].cementerio, carta);
			const bool enOtro = id != otro && contar(d.l[(size_t)(1 - lado)].cementerio, carta) > contar(antes.l[(size_t)(1 - lado)].cementerio, carta);
			const bool fuera = contar(d.l[(size_t)lado].desterrados, carta) + contar(d.l[(size_t)lado].mano, carta) + contar(d.l[(size_t)lado].mazo, carta) + contar(d.l[(size_t)lado].extra, carta) >
			                   contar(antes.l[(size_t)lado].desterrados, carta) + contar(antes.l[(size_t)lado].mano, carta) + contar(antes.l[(size_t)lado].mazo, carta) +
			                       contar(antes.l[(size_t)lado].extra, carta);
			// (si no esta en su cementerio, un efecto lo llevo a otro lado: el campo de alguno, la mano, el mazo, desterrado)
			bool enCampo = false;
			for (const ia::Lado& x : d.l)
				for (const ia::Monstruo& mm : x.m) enCampo |= mm.id == carta;
			for (const ia::Lado& x : d.l)
				for (const ia::MagiaTrampa& mm : x.mt) enCampo |= mm.id == carta;
			b.caso(cid, (enSuCementerio || fuera || enCampo) && !(enOtro && !enSuCementerio), "%s: %s destruido termino en el cementerio del otro jugador o desaparecio (%s)", NOMBRE[modo],
			       carta.c_str(), lado ? "rival" : "jugador");
		}
		const std::string dif = b.difMesa();
		b.caso(cid, dif.empty() || !b.alDia(), "%s: la mesa no es el motor despues del combate: %s", NOMBRE[modo], dif.c_str());
		b.grabar = false;
		for (int lado = 0; lado < 2; lado++)
			if (d.l[(size_t)lado].cementerio.size() != antes.l[(size_t)lado].cementerio.size()) mirarCementerio(b, cid, lado);
	}
}

static void combates(Bot& b, int tope)
{
	Juego& g = b.g;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	if (g.ctl.vista.pantalla != VD::DUELO) {
		Rival r{ VD::LIBRE, 0, 0, 1, 0, -1, -1, "combate" };
		if (!elegirRival(b, r) || !b.resolverInicio(true)) {
			b.caso("CU-FT-COMBATE-INICIO", false, "no se pudo empezar un duelo");
			return;
		}
	}
	int n = 0, sub = 0;
	const double t0 = ahora();
	for (const ia::Carta& c : g.baseIA.cartas) {
		if (!c.monstruo) continue;
		if (tope >= 0 && n >= tope) break;
		n++;
		combateCarta(b, c, mazo, sub);
		if (g.ctl.vista.pantalla != VD::DUELO) {
			for (int k = 0; k < 8 && g.ctl.vista.pantalla == VD::RESULTADO; k++) b.apretar(PAD_CIRCLE, 0.5f);
			Rival r{ VD::LIBRE, 0, 0, 1, 0, -1, -1, "combate" };
			if (!elegirRival(b, r) || !b.resolverInicio(true)) break;
		}
		if (n % 200 == 0) fprintf(stderr, "combates: %d monstruos (%.0f s)\n", n, ahora() - t0);
	}
	fprintf(stderr, "COMBATES DESDE EL FRONT: %d monstruos, %d combates (%.0f s)\n", n, sub, ahora() - t0);
}

// ------------------------------------------------------------------------------------------------- fases y cancelar (5)
static cub::Mesa mesaSimple(const ia::Base& base, const std::vector<std::string>& mazo, int turno = 3)
{
	cub::Mesa m(&base);
	m.d.turno = turno;
	m.d.l[0].mazo = mazo;
	m.d.l[1].mazo.assign(30, "celtic_guardian");
	m.mon(0, "battle_ox");
	m.mon(1, "celtic_guardian");
	m.d.l[0].mano = { "mystical_elf", "dark_hole", "trap_hole" };
	m.d.l[1].mano = { "vorse_raider" };
	m.d.recalcular();
	return m;
}
static void fasesYCancelar(Bot& b)
{
	Juego& g = b.g;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	ia::Duelo& d = g.dueloIA;
	b.rivalPorBot = true;
	b.grabar = false;
	b.duelo = "fases";
	// cancelar: menu de la mano, eleccion de zona, eleccion de objetivo
	inyectar(b, mesaSimple(g.baseIA, mazo).d);
	b.esperar(0.3f);
	const size_t s0 = d.sucesos.size();
	b.irA(Foco{ F_MANO_J, 0 });
	b.apretar(PAD_CROSS, 0.15f);
	b.caso("CU-FT-CANCELAR-MENU", g.ctl.estado == control::MENU, "X en la carta de la mano no abrio su menu");
	b.apretar(PAD_CIRCLE, 0.15f);
	b.caso("CU-FT-CANCELAR-MENU", g.ctl.estado == control::NAVEGAR && d.sucesos.size() == s0, "circulo no cerro el menu sin hacer nada");
	b.accion(Foco{ F_MANO_J, 0 }, [](const control::Opcion& o) { return o.accion == control::AC_INVOCAR && o.activa; });
	// (accion() ya eligio: si quedo en ELEGIR_ZONA la cancelamos; si no, esta invocada; se prueba con la siguiente)
	inyectar(b, mesaSimple(g.baseIA, mazo).d);
	b.esperar(0.3f);
	const size_t s1 = d.sucesos.size();
	b.irA(Foco{ F_MANO_J, 0 });
	b.apretar(PAD_CROSS, 0.15f);
	int k = -1;
	for (int o = 0; o < (int)g.ctl.opciones.size(); o++)
		if (g.ctl.opciones[(size_t)o].accion == control::AC_INVOCAR) k = o;
	b.elegirOpcion(k);
	b.caso("CU-FT-CANCELAR-ZONA", g.ctl.estado == control::ELEGIR_ZONA, "Invocar no paso a elegir la zona (estado %s)", control::NOMBRE_ESTADO[g.ctl.estado]);
	b.apretar(PAD_CIRCLE, 0.15f);
	b.caso("CU-FT-CANCELAR-ZONA", g.ctl.estado == control::MENU, "circulo eligiendo la zona no volvio al menu");
	b.apretar(PAD_CIRCLE, 0.15f);
	b.caso("CU-FT-CANCELAR-ZONA", g.ctl.estado == control::NAVEGAR && d.sucesos.size() == s1 && d.l[0].mano.size() == 3, "arrepentirse de invocar dejo algo hecho");
	// fases: M1 -> BP -> M2 -> fin del turno
	b.pasarFase();
	b.caso("CU-FT-FASES-BP", d.fase == ia::F_BATALLA, "OPTIONS en M1 (turno 3) no fue a la batalla (fase %d)", (int)d.fase);
	// arrepentirse de atacar
	const size_t s2 = d.sucesos.size();
	b.irA(Foco{ F_ZONA, Juego::zonaMesaIA(0, 0) });
	b.apretar(PAD_CROSS, 0.15f);
	k = -1;
	for (int o = 0; o < (int)g.ctl.opciones.size(); o++)
		if (g.ctl.opciones[(size_t)o].accion == control::AC_ATACAR) k = o;
	b.elegirOpcion(k);
	b.caso("CU-FT-CANCELAR-ATAQUE", g.ctl.estado == control::ELEGIR_OBJETIVO, "Atacar no paso a elegir el objetivo (estado %s)", control::NOMBRE_ESTADO[g.ctl.estado]);
	b.apretar(PAD_CIRCLE, 0.15f);
	if (g.ctl.estado == control::MENU) b.apretar(PAD_CIRCLE, 0.15f);
	b.caso("CU-FT-CANCELAR-ATAQUE", g.ctl.estado == control::NAVEGAR && d.sucesos.size() == s2 && !d.l[0].m[0].ataco, "arrepentirse de atacar dejo algo hecho");
	b.pasarFase();
	b.caso("CU-FT-FASES-M2", d.fase == ia::F_PRINCIPAL2, "OPTIONS en la batalla no fue a M2 (fase %d)", (int)d.fase);
	const int t0 = d.turno;
	b.pasarFase();
	b.caso("CU-FT-FASES-FIN-TURNO", d.turno != t0 && d.activo == 1, "OPTIONS en M2 no termino el turno");
	// desde la batalla, terminar el turno sin atacar
	inyectar(b, mesaSimple(g.baseIA, mazo).d);
	b.esperar(0.3f);
	b.pasarFase();
	const int t1 = d.turno;
	for (int v = 0; v < 3 && d.turno == t1; v++) b.pasarFase();
	b.caso("CU-FT-FASES-BP-SIN-ATACAR", d.turno != t1 && d.activo == 1, "desde la batalla no se pudo terminar el turno sin atacar");
	// el primer turno: sin batalla; Finalizar turno (rama classic-md-fases: el motor pasa por M2 y la Fase Final)
	inyectar(b, mesaSimple(g.baseIA, mazo, 1).d);
	b.esperar(0.3f);
	const int tPrimero = d.turno;
	b.pasarFase();
	b.caso("CU-FT-FASES-PRIMER-TURNO", d.turno != tPrimero || d.fase == ia::F_FINAL, "en el primer turno Finalizar turno no termino el turno (fase %d)", (int)d.fase);
	// EP con la mano llena: descartar con el mando y terminar
	{
		cub::Mesa m = mesaSimple(g.baseIA, mazo);
		m.d.fase = ia::F_PRINCIPAL2;
		m.d.l[0].mano.assign(8, "mystical_elf");
		inyectar(b, m.d);
		b.esperar(0.3f);
		const int t2 = d.turno;
		b.elegirFase(2);
		b.esperarQue(4.0f, [&] { return b.libre() || d.turno != t2; });
		b.caso("CU-FT-FASES-EP", d.fase == ia::F_FINAL && d.turno == t2, "con 8 cartas Finalizar turno en M2 no se quedo en la fase final para descartar (fase %d)", (int)d.fase);
		for (int v = 0; v < 4 && d.turno == t2 && (int)d.l[0].mano.size() > ia::limiteMano(d, 0); v++)
			b.accion(Foco{ F_MANO_J, 0 }, [](const control::Opcion& o) { return o.accion == control::AC_JUGADA && o.jugada.tipo == ia::J_DESCARTAR; });
		b.caso("CU-FT-FASES-EP", (int)d.l[0].mano.size() <= ia::limiteMano(d, 0), "no se pudo descartar con el mando hasta el limite (%d cartas)", (int)d.l[0].mano.size());
		for (int v = 0; v < 3 && d.turno == t2; v++) b.pasarFase();
		b.caso("CU-FT-FASES-EP", d.turno != t2, "tras descartar no se pudo terminar el turno");
	}
}

// ------------------------------------------------------------------------------------------------- terminar turno desde la batalla
// rama classic-bp-terminar: en la Fase de Batalla el CIRCULO termina el turno (un solo boton, como en Master Duel). Es un
// atajo: pasa por M2 y por la Fase Final del motor (con sus ventanas) como dos cuadrados; el cuadrado sigue yendo a M2
static bool ayudaTiene(const control::Mando& c, const char* boton, const char* texto)
{
	for (const auto& a : c.ayuda())
		if (a.first == boton && a.second == texto) return true;
	return false;
}
// los S_FASE del lado desde el suceso s0: el orden de las fases por las que paso (p. ej. "3 4 " = M2, FINAL)
static std::string fasesDesde(const ia::Duelo& d, size_t s0, int lado = 0)
{
	std::string r;
	for (size_t k = s0; k < d.sucesos.size(); k++)
		if (d.sucesos[k].tipo == ia::S_FASE && d.sucesos[k].lado == lado) r += std::to_string(d.sucesos[k].valor) + " ";
	return r;
}
// atacar con el monstruo de la zona z del jugador al primer objetivo, con el mando (sin resolver lo que pase despues)
static void atacarConMando(Bot& b, int z, float espera)
{
	Juego& g = b.g;
	b.irA(Foco{ F_ZONA, Juego::zonaMesaIA(0, z) });
	b.apretar(PAD_CROSS, 0.15f);
	int k = -1;
	for (int o = 0; o < (int)g.ctl.opciones.size(); o++)
		if (g.ctl.opciones[(size_t)o].accion == control::AC_ATACAR && g.ctl.opciones[(size_t)o].activa) k = o;
	b.elegirOpcion(k);
	if (g.ctl.estado == control::ELEGIR_OBJETIVO && !g.ctl.validas.empty()) {
		b.irA(Foco{ F_ZONA, g.ctl.validas[0] });
		b.apretar(PAD_CROSS, espera);
	}
}
static bool nuevoDuelo(Bot& b);
// rama classic-md-fases: OPTIONS abre el selector de fases de Master Duel (Iniciar Fase de Batalla / Ir a M2 / Finalizar
// turno, solo las validas activas); la fase elegida pasa por el motor con J_FASE (las mismas ventanas que antes). El
// cuadrado ya no pasa de fase. El engranaje (panel tactil o el foco + X) abre el menu del duelo
static void fasesOptions(Bot& b)
{
	Juego& g = b.g;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	ia::Duelo& d = g.dueloIA;
	const control::Mando::SelectorFases& sf = g.ctl.selFases;
	b.rivalPorBot = true;
	b.grabar = false;
	b.duelo = "fases-options";
	const std::string m2y = std::to_string((int)ia::F_PRINCIPAL2) + " " + std::to_string((int)ia::F_FINAL) + " ";
	auto validas = [&](bool bp, bool m2, bool ep) { return sf.valida[0] == bp && sf.valida[1] == m2 && sf.valida[2] == ep; };
	auto libreEn = [&](const ia::Duelo& base) {
		inyectar(b, base);
		b.esperar(0.3f);
		b.esperarQue(8.0f, [&] { return b.libre(); });
		return b.libre();
	};
	auto aLaBatalla = [&](const ia::Duelo& base) {
		libreEn(base);
		b.elegirFase(0);
		b.esperarQue(8.0f, [&] { return b.libre() && d.fase == ia::F_BATALLA; });
		return d.fase == ia::F_BATALLA && b.libre();
	};
	auto esperarTurno = [&](int t) { return b.esperarQue(25.0f, [&] { return d.turno != t || d.ganador >= 0; }); };
	// 1) abrir en M1 (turno 3): BP y EP activas, M2 apagada; el cursor en BP; la ayuda y la placa
	{
		const bool ok = libreEn(mesaSimple(g.baseIA, mazo).d);
		b.caso("CU-FT-FASES-OPTIONS-ABRIR", ok && d.fase == ia::F_PRINCIPAL1, "no se llego libre a M1 (fase %d)", (int)d.fase);
		b.caso("CU-FT-FASES-OPTIONS-AYUDA", ayudaTiene(g.ctl, "OPTIONS", "Fases") && strcmp(g.ctl.botonFase(), "Iniciar Fase de Batalla") == 0,
		       "la ayuda no ofrece 'OPTIONS Fases' o la placa no dice 'Iniciar Fase de Batalla' ('%s')", g.ctl.botonFase());
		b.apretar(PAD_OPTIONS, 0.2f);
		b.caso("CU-FT-FASES-OPTIONS-ABRIR", sf.abierto && validas(true, false, true) && sf.opcion == 0 && !g.pausa.abierta,
		       "OPTIONS en M1 no abrio el selector con BP y EP (abierto %d, validas %d%d%d, cursor %d, pausa %d)", (int)sf.abierto, (int)sf.valida[0], (int)sf.valida[1],
		       (int)sf.valida[2], sf.opcion, (int)g.pausa.abierta);
		b.caso("CU-FT-FASES-OPTIONS-AYUDA", ayudaTiene(g.ctl, "X", "Confirmar") && ayudaTiene(g.ctl, "O", "Cerrar"), "con el selector abierto la ayuda no dice X Confirmar / O Cerrar");
		b.apretar(PAD_RIGHT, 0.1f);
		b.caso("CU-FT-FASES-OPTIONS-INVALIDAS", sf.opcion == 2, "derecha desde BP no salto la M2 apagada (cursor %d)", sf.opcion);
		b.apretar(PAD_RIGHT, 0.1f);
		b.apretar(PAD_LEFT, 0.1f);
		b.caso("CU-FT-FASES-OPTIONS-INVALIDAS", sf.opcion == 0, "izquierda desde EP no volvio a BP (cursor %d)", sf.opcion);
		b.apretar(PAD_CIRCLE, 0.2f);
		b.caso("CU-FT-FASES-OPTIONS-CANCELAR", !sf.abierto && d.fase == ia::F_PRINCIPAL1 && g.ctl.estado == control::NAVEGAR, "circulo no cerro el selector sin cambiar de fase");
		b.apretar(PAD_OPTIONS, 0.2f);
		const bool abre = sf.abierto;
		b.apretar(PAD_OPTIONS, 0.2f);
		b.caso("CU-FT-FASES-OPTIONS-CANCELAR", abre && !sf.abierto && d.fase == ia::F_PRINCIPAL1 && !g.pausa.abierta, "OPTIONS otra vez no cerro el selector (abre %d, abierto %d)", (int)abre,
		       (int)sf.abierto);
		b.apretar(PAD_SQUARE, 0.3f);
		b.caso("CU-FT-FASES-OPTIONS-CUADRADO", d.fase == ia::F_PRINCIPAL1 && !sf.abierto, "el cuadrado cambio de fase (fase %d)", (int)d.fase);
		// ES / EN
		const int idi = idioma::actual();
		idioma::actual() = idioma::EN;
		const bool en = strcmp(idioma::tr("Iniciar Fase de Batalla"), "Start Battle Phase") == 0 && strcmp(idioma::tr("Ir a M2"), "Go to Main Phase 2") == 0 &&
		                strcmp(idioma::tr("Finalizar turno"), "End Turn") == 0 && strcmp(idioma::tr("TU TURNO"), "YOUR TURN") == 0;
		idioma::actual() = idi;
		b.caso("CU-FT-FASES-OPTIONS-AYUDA", en, "los textos del selector no estan en ingles");
	}
	// 2) M1 -> BP -> M2 -> EP, cada una con el selector
	{
		const bool bp = aLaBatalla(mesaSimple(g.baseIA, mazo).d);
		b.caso("CU-FT-FASES-OPTIONS-BP", bp, "Iniciar Fase de Batalla no fue a la batalla (fase %d)", (int)d.fase);
		b.apretar(PAD_OPTIONS, 0.2f);
		b.caso("CU-FT-FASES-OPTIONS-BP", sf.abierto && validas(false, true, true) && sf.opcion == 1, "en la batalla el selector no ofrece M2 y EP (validas %d%d%d, cursor %d)",
		       (int)sf.valida[0], (int)sf.valida[1], (int)sf.valida[2], sf.opcion);
		b.apretar(PAD_CIRCLE, 0.2f);
		b.elegirFase(1);
		b.esperarQue(10.0f, [&] { return b.libre() && d.fase == ia::F_PRINCIPAL2; });
		b.caso("CU-FT-FASES-OPTIONS-M2", d.fase == ia::F_PRINCIPAL2, "Ir a M2 no fue a M2 (fase %d)", (int)d.fase);
		b.apretar(PAD_OPTIONS, 0.2f);
		b.caso("CU-FT-FASES-OPTIONS-EP", sf.abierto && validas(false, false, true) && sf.opcion == 2, "en M2 el selector no ofrece solo EP (validas %d%d%d)", (int)sf.valida[0],
		       (int)sf.valida[1], (int)sf.valida[2]);
		const int t = d.turno;
		const size_t s0 = d.sucesos.size();
		b.apretar(PAD_CROSS, 0.1f);
		esperarTurno(t);
		b.caso("CU-FT-FASES-OPTIONS-EP", d.turno != t && fasesDesde(d, s0).find(std::to_string((int)ia::F_FINAL)) == 0, "Finalizar turno en M2 no paso por la Fase Final (turno %d, fases '%s')",
		       d.turno, fasesDesde(d, s0).c_str());
	}
	// 3) Finalizar turno desde M1 y desde la batalla: el motor pasa por todas (BP, M2, Fase Final) con sus ventanas
	{
		libreEn(mesaSimple(g.baseIA, mazo).d);
		const int t = d.turno;
		const size_t s0 = d.sucesos.size();
		b.elegirFase(2);
		esperarTurno(t);
		const std::string f = fasesDesde(d, s0);
		b.caso("CU-FT-FASES-OPTIONS-EP-DESDE-M1", d.turno != t && f.find(std::to_string((int)ia::F_FINAL)) != std::string::npos && g.ctl.faseObjetivo < 0,
		       "Finalizar turno en M1 no termino el turno por la Fase Final (turno %d, fases '%s', objetivo %d)", d.turno, f.c_str(), g.ctl.faseObjetivo);
		aLaBatalla(mesaSimple(g.baseIA, mazo).d);
		const int t2 = d.turno;
		const size_t s2 = d.sucesos.size();
		b.elegirFase(2);
		esperarTurno(t2);
		b.caso("CU-FT-FASES-OPTIONS-EP-DESDE-BP", d.turno != t2 && fasesDesde(d, s2).find(m2y) == 0, "Finalizar turno en la batalla no paso por M2 y la Fase Final (fases '%s')",
		       fasesDesde(d, s2).c_str());
	}
	// 4) el primer turno: sin batalla; solo Finalizar turno
	{
		libreEn(mesaSimple(g.baseIA, mazo, 1).d);
		b.apretar(PAD_OPTIONS, 0.2f);
		b.caso("CU-FT-FASES-OPTIONS-PRIMER-TURNO", sf.abierto && validas(false, false, true) && sf.opcion == 2, "en el primer turno el selector ofrece la batalla (validas %d%d%d)",
		       (int)sf.valida[0], (int)sf.valida[1], (int)sf.valida[2]);
		b.apretar(PAD_LEFT, 0.1f);
		b.caso("CU-FT-FASES-OPTIONS-INVALIDAS", sf.opcion == 2, "el cursor se fue a una fase apagada (%d)", sf.opcion);
		b.apretar(PAD_CIRCLE, 0.2f);
	}
	// 5) el rival responde en la Fase Final (su Scapegoat): Finalizar turno desde la batalla le deja su ventana
	{
		cub::Mesa m = mesaSimple(g.baseIA, mazo);
		m.colocar(1, "scapegoat", 1);
		m.d.recalcular();
		aLaBatalla(m.d);
		b.rivalActiva = "scapegoat";
		b.rivalVentana = (int)ia::V_FASE_FINAL;
		const int t = d.turno;
		const int antes = d.l[1].monstruos();
		const size_t s0 = d.sucesos.size();
		b.elegirFase(2);
		esperarTurno(t);
		size_t iFin = 0, iCabra = 0;
		for (size_t k = s0; k < d.sucesos.size(); k++) {
			const ia::Suceso& su = d.sucesos[k];
			if (!iFin && su.tipo == ia::S_FASE && su.lado == 0 && su.valor == (int)ia::F_FINAL) iFin = k;
			if (!iCabra && su.tipo == ia::S_ACTIVA_MT && su.lado == 1 && su.carta == "scapegoat") iCabra = k;
		}
		b.caso("CU-FT-FASES-OPTIONS-RIVAL-EP", iFin > 0 && iCabra > iFin && d.turno != t && d.l[1].monstruos() >= antes + 4,
		       "el rival no respondio en la Fase Final (fase final en %d, scapegoat en %d, turno %d, monstruos %d -> %d)", (int)iFin, (int)iCabra, d.turno, antes, d.l[1].monstruos());
		b.rivalActiva.clear();
		b.rivalVentana = -1;
	}
	// 6) con una pregunta abierta (Mirror Force del rival responde al ataque) OPTIONS no abre el selector
	{
		cub::Mesa m(&g.baseIA, ia::F_BATALLA);
		m.d.turno = 3;
		m.d.l[0].mazo = mazo;
		m.d.l[1].mazo.assign(30, "celtic_guardian");
		m.mon(0, "summoned_skull");
		m.mon(1, "celtic_guardian");
		m.colocar(0, "trap_jammer", 1);
		m.colocar(1, "mirror_force", 1);
		m.d.recalcular();
		inyectar(b, m.d);
		// (rama classic-arreglo-ciclo-md: el ataque cuando la mesa queda libre; con el ritmo de MD 0.8 s no alcanzaban
		// tras el turno del rival del paso 5 y X abria el detalle en vez del menu: no habia ataque ni Mirror Force)
		b.esperar(0.3f);
		b.esperarQue(10.0f, [&] { return b.libre(); });
		b.rivalActiva = "mirror_force";
		const int t = d.turno;
		atacarConMando(b, 0, 0.15f);
		const bool preg = b.esperarQue(15.0f, [&] { return g.ctl.estado == control::MENU && g.ctl.enPregunta; });
		b.caso("CU-FT-FASES-OPTIONS-PREGUNTA", preg, "el ataque con Mirror Force del rival no abrio la pregunta (estado %s)", control::NOMBRE_ESTADO[g.ctl.estado]);
		if (preg) {
			b.apretar(PAD_OPTIONS, 0.2f);
			b.caso("CU-FT-FASES-OPTIONS-PREGUNTA", !sf.abierto && g.ctl.enPregunta && d.turno == t && d.fase == ia::F_BATALLA && !*g.ctl.botonFase(),
			       "con la pregunta abierta OPTIONS abrio el selector o cambio de fase (abierto %d, fase %d)", (int)sf.abierto, (int)d.fase);
			b.apretar(PAD_CIRCLE, 0.2f);   // (No activar)
		}
		b.rivalActiva.clear();
		b.resolverUI();
	}
	// 7) durante un ataque animado OPTIONS no abre el selector ni cambia de fase
	{
		aLaBatalla(mesaSimple(g.baseIA, mazo).d);
		const int t = d.turno;
		atacarConMando(b, 0, 0.05f);
		const bool anim = b.esperarQue(3.0f, [&] { return g.hayAtaque(); });
		b.apretar(PAD_OPTIONS, 0.05f);
		const bool abierto = sf.abierto;
		b.resolverUI();
		b.esperarQue(15.0f, [&] { return b.libre() || d.turno != t; });
		b.caso("CU-FT-FASES-OPTIONS-ATAQUE", anim && !abierto && d.turno == t && d.fase == ia::F_BATALLA && g.ctl.faseObjetivo < 0,
		       "OPTIONS durante el ataque (animacion %d) abrio el selector (%d) o cambio de fase (turno %d -> %d, fase %d)", (int)anim, (int)abierto, t, d.turno, (int)d.fase);
	}
	// 8) en el turno del rival OPTIONS no hace nada
	{
		libreEn(mesaSimple(g.baseIA, mazo).d);
		const int t = d.turno;
		b.elegirFase(2);
		const bool rival = b.esperarQue(15.0f, [&] { return d.activo == 1 && d.turno != t; });
		b.apretar(PAD_OPTIONS, 0.05f);
		b.caso("CU-FT-FASES-OPTIONS-RIVAL", rival && !sf.abierto && !g.pausa.abierta && g.ctl.faseObjetivo < 0,
		       "en el turno del rival OPTIONS abrio algo (rival %d, selector %d, pausa %d, objetivo %d)", (int)rival, (int)sf.abierto, (int)g.pausa.abierta, g.ctl.faseObjetivo);
		b.esperarQue(30.0f, [&] { return d.activo == 0 || d.ganador >= 0; });
	}
}

static void engranaje(Bot& b)
{
	Juego& g = b.g;
	VD& v = g.ctl.vista;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	b.rivalPorBot = true;
	b.grabar = false;
	b.duelo = "engranaje";
	if (v.pantalla != VD::DUELO && !nuevoDuelo(b)) {
		b.caso("CU-FT-ENGRANAJE-TACTIL", false, "no se pudo empezar un duelo");
		return;
	}
	inyectar(b, mesaSimple(g.baseIA, mazo).d);
	b.esperar(0.3f);
	b.esperarQue(8.0f, [&] { return b.libre(); });
	// OPTIONS en el duelo no es la pausa
	b.apretar(PAD_OPTIONS, 0.2f);
	b.caso("CU-FT-ENGRANAJE-OPTIONS", !g.pausa.abierta, "OPTIONS en el duelo abrio el menu del duelo (debe ser el selector de fases)");
	if (g.ctl.selFases.abierto) b.apretar(PAD_CIRCLE, 0.2f);
	// el panel tactil lo abre (con Abandonar) y otro toque lo cierra; no abre Construir mazo
	b.apretar(PAD_TOUCH_PAD, 0.2f);
	const std::vector<int> ops = g.opcionesPausa();
	b.caso("CU-FT-ENGRANAJE-TACTIL", g.pausa.abierta && !g.constructorMazo.abierto && std::find(ops.begin(), ops.end(), (int)Juego::PA_ABANDONAR) != ops.end(),
	       "el panel tactil no abrio el menu del duelo (pausa %d, constructor %d)", (int)g.pausa.abierta, (int)g.constructorMazo.abierto);
	b.apretar(PAD_TOUCH_PAD, 0.2f);
	b.caso("CU-FT-ENGRANAJE-CERRAR", !g.pausa.abierta && v.pantalla == VD::DUELO, "otro toque del panel tactil no cerro el menu del duelo");
	b.apretar(PAD_TOUCH_PAD, 0.2f);
	b.apretar(PAD_CIRCLE, 0.2f);
	b.caso("CU-FT-ENGRANAJE-CERRAR", !g.pausa.abierta && v.pantalla == VD::DUELO, "circulo no cerro el menu del duelo");
	// con el cursor: el foco al engranaje (abajo a la izquierda) y X
	const bool llego = b.irA(Foco{ control::F_ENGRANAJE, -1 });
	b.caso("CU-FT-ENGRANAJE-CURSOR", llego && ayudaTiene(g.ctl, "X", "Menú del duelo"), "el foco no llega al engranaje con las flechas (foco %s)", g.ctl.idFoco().c_str());
	b.apretar(PAD_CROSS, 0.3f);
	b.caso("CU-FT-ENGRANAJE-CURSOR", g.pausa.abierta, "X en el engranaje no abrio el menu del duelo");
	b.apretar(PAD_CIRCLE, 0.2f);
	b.irA(Foco{ F_MANO_J, 0 });
	// rendirse desde el engranaje
	const int derrotas = v.derrotas;
	b.abandonar();
	b.esperar(0.5f);
	b.caso("CU-FT-ENGRANAJE-RENDIRSE", v.pantalla == VD::RESULTADO && v.derrotas == derrotas + 1, "Abandonar desde el engranaje no termino en derrota (pantalla %d)", v.pantalla);
	nuevoDuelo(b);   // (las pruebas que siguen inyectan su mesa en un duelo en curso)
}

// ------------------------------------------------------------------------------------------------- el circulo en los estados anidados
// rama classic-arreglo-visor: el circulo que cierra algo anidado (el detalle abierto desde un visor o desde la mano, el
// detalle sobre un menu, la pausa sobre un visor, el detalle con la pregunta del duelo detras del visor, un visor en el
// turno del rival) vuelve un paso atras: no termina el turno (el atajo de la batalla), no deja pedido el fin del turno
// y no pierde lo que habia debajo
static void circuloAnidado(Bot& b)
{
	Juego& g = b.g;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	ia::Duelo& d = g.dueloIA;
	visores::Consulta& q = g.consulta;
	b.rivalPorBot = true;
	b.grabar = false;
	b.duelo = "circulo-anidado";
	auto conCementerio = [&](cub::Mesa m) {
		m.d.l[0].cementerio = { "dark_magician", "mystical_elf" };
		m.d.l[1].cementerio = { "vorse_raider" };
		m.d.recalcular();
		return m;
	};
	auto aLaBatalla = [&](const ia::Duelo& base) {
		inyectar(b, base);
		b.esperar(0.3f);
		b.pasarFase();
		b.esperarQue(8.0f, [&] { return b.libre(); });
		b.esperar(0.8f);   // (la pausa del circulo: control::Mando::PAUSA_CIRCULO)
		return d.fase == ia::F_BATALLA && b.libre();
	};
	// el circulo "de mas" enseguida no termina el turno ni lo deja pedido
	auto sigueLaBatalla = [&](int t) { return d.turno == t && d.activo == 0 && d.fase == ia::F_BATALLA && g.ctl.faseObjetivo < 0; };
	// 1) en la batalla: L3, el cementerio, X (detalle), circulo (al visor), circulo (cierra), circulo de mas
	{
		const bool bp = aLaBatalla(conCementerio(mesaSimple(g.baseIA, mazo)).d);
		b.caso("CU-FT-VISOR-DETALLE-BP", bp, "no se llego libre a la batalla (fase %d)", (int)d.fase);
		const int t = d.turno;
		b.apretar(PAD_L3, 0.2f);
		b.apretar(PAD_RIGHT, 0.2f);
		const bool cem = q.vis.abierto && q.vis.pestana == visores::P_CEMENTERIO && q.filas(g.ctl) == 2;
		b.caso("CU-FT-VISOR-DETALLE-BP", cem, "L3 y derecha no muestran el cementerio con sus 2 cartas (abierto %d, pestanna %d, filas %d)", (int)q.vis.abierto, q.vis.pestana,
		       q.filas(g.ctl));
		g.ctl.desplDetalle = 3.0f;   // (el detalle anterior quedo desplazado: el nuevo empieza arriba)
		g.ctl.zonaDetalle = Juego::zonaMesaIA(0, 0);
		b.apretar(PAD_CROSS, 0.2f);
		b.caso("CU-FT-VISOR-DETALLE-BP", g.ctl.estado == control::DETALLE && q.vis.abierto && g.ctl.antesDetalle == control::NAVEGAR,
		       "X en el visor no abrio el detalle encima (estado %s)", control::NOMBRE_ESTADO[g.ctl.estado]);
		b.caso("CU-FT-VISOR-DETALLE-LIMPIO", g.ctl.desplDetalle == 0.0f && g.ctl.zonaDetalle == -1,
		       "el detalle desde el visor hereda el desplazamiento (%.1f) o la zona (%d) del anterior", g.ctl.desplDetalle, g.ctl.zonaDetalle);
		b.apretar(PAD_CIRCLE, 0.1f);
		b.caso("CU-FT-VISOR-DETALLE-BP", q.vis.abierto && g.ctl.estado == control::NAVEGAR && sigueLaBatalla(t),
		       "circulo en el detalle no volvio al visor o termino el turno (visor %d, estado %s, turno %d -> %d, fase %d)", (int)q.vis.abierto,
		       control::NOMBRE_ESTADO[g.ctl.estado], t, d.turno, (int)d.fase);
		b.apretar(PAD_CIRCLE, 0.1f);
		b.caso("CU-FT-VISOR-DETALLE-BP", !q.vis.abierto && sigueLaBatalla(t), "el segundo circulo no cerro el visor o termino el turno");
		b.apretar(PAD_CIRCLE, 0.3f);
		b.esperar(0.5f);
		b.caso("CU-FT-VISOR-DETALLE-BP", sigueLaBatalla(t), "el circulo de mas al cerrar el visor termino el turno (turno %d -> %d, fase %d, pedido %d)", t, d.turno, (int)d.fase,
		       g.ctl.faseObjetivo);
	}
	// 2) en la batalla: el detalle desde la mano (triangulo) y el detalle sobre el menu de una carta
	{
		aLaBatalla(conCementerio(mesaSimple(g.baseIA, mazo)).d);
		const int t = d.turno;
		b.irA(Foco{ F_MANO_J, 0 });
		b.esperar(0.8f);
		b.apretar(PAD_TRIANGLE, 0.15f);
		const bool det = g.ctl.estado == control::DETALLE;
		b.apretar(PAD_CIRCLE, 0.1f);
		const bool vuelve = g.ctl.estado == control::NAVEGAR;
		b.apretar(PAD_CIRCLE, 0.3f);
		b.esperar(0.5f);
		b.caso("CU-FT-DETALLE-MANO-BP", det && vuelve && sigueLaBatalla(t), "triangulo en la mano y dos circulos: detalle %d, vuelve %d, turno %d -> %d, fase %d, pedido %d", (int)det,
		       (int)vuelve, t, d.turno, (int)d.fase, g.ctl.faseObjetivo);
		b.irA(Foco{ F_ZONA, Juego::zonaMesaIA(0, 0) });
		b.esperar(0.8f);
		b.apretar(PAD_CROSS, 0.15f);
		const bool menu = g.ctl.estado == control::MENU;
		const size_t nOps = g.ctl.opciones.size();
		b.apretar(PAD_TRIANGLE, 0.15f);
		const bool det2 = g.ctl.estado == control::DETALLE && g.ctl.antesDetalle == control::MENU;
		b.apretar(PAD_CIRCLE, 0.1f);
		const bool alMenu = g.ctl.estado == control::MENU && g.ctl.opciones.size() == nOps;
		b.apretar(PAD_CIRCLE, 0.1f);
		const bool aMesa = g.ctl.estado == control::NAVEGAR;
		b.apretar(PAD_CIRCLE, 0.3f);
		b.esperar(0.5f);
		b.caso("CU-FT-DETALLE-MENU-BP", menu && det2 && alMenu && aMesa && sigueLaBatalla(t),
		       "menu -> detalle -> circulos: menu %d, detalle %d, vuelve al menu %d, a la mesa %d, turno %d -> %d, fase %d, pedido %d", (int)menu, (int)det2, (int)alMenu, (int)aMesa, t,
		       d.turno, (int)d.fase, g.ctl.faseObjetivo);
	}
	// 3) en la batalla: la pausa sobre el visor; el circulo cierra la pausa (el visor sigue), luego el visor
	{
		aLaBatalla(conCementerio(mesaSimple(g.baseIA, mazo)).d);
		const int t = d.turno;
		b.apretar(PAD_L3, 0.2f);
		b.apretar(PAD_TOUCH_PAD, 0.2f);   // (rama classic-md-fases: el engranaje = menu del duelo)
		const bool pausa = g.pausa.abierta && q.vis.abierto;
		b.apretar(PAD_CIRCLE, 0.1f);
		const bool sigueVisor = !g.pausa.abierta && q.vis.abierto && g.ctl.estado == control::NAVEGAR;
		b.apretar(PAD_CIRCLE, 0.1f);
		const bool cerro = !q.vis.abierto;
		b.apretar(PAD_CIRCLE, 0.3f);
		b.esperar(0.5f);
		b.caso("CU-FT-VISOR-PAUSA-BP", pausa && sigueVisor && cerro && sigueLaBatalla(t),
		       "visor -> pausa -> circulos: pausa %d, sigue el visor %d, cerro %d, turno %d -> %d, fase %d, pedido %d", (int)pausa, (int)sigueVisor, (int)cerro, t, d.turno, (int)d.fase,
		       g.ctl.faseObjetivo);
	}
	// 4) la pregunta del duelo detras del visor (el rival responde al ataque con Mirror Force y el duelo pregunta al
	// jugador): el detalle desde el visor vuelve al visor CON la pregunta; cerrar el visor la deja a la vista
	{
		cub::Mesa m(&g.baseIA, ia::F_BATALLA);
		m.d.turno = 3;
		m.d.l[0].mazo = mazo;
		m.d.l[1].mazo.assign(30, "celtic_guardian");
		m.mon(0, "summoned_skull");
		m.mon(1, "celtic_guardian");
		m.colocar(0, "trap_jammer", 1);
		m.colocar(1, "mirror_force", 1);
		m = conCementerio(m);
		inyectar(b, m.d);
		b.esperar(0.8f);
		b.rivalActiva = "mirror_force";
		const int t = d.turno;
		atacarConMando(b, 0, 0.0f);
		b.apretar(PAD_L3, 0.1f);   // (el visor abierto mientras el rival responde)
		const bool porL3 = q.vis.abierto;
		const bool preg = b.esperarQue(15.0f, [&] { return g.ctl.estado == control::MENU && g.ctl.enPregunta; });
		if (preg && !q.vis.abierto) q.abrir(visores::P_REGISTRO, 0);   // (la pregunta llego antes que el L3: el visor encima igual)
		b.caso("CU-FT-VISOR-PREGUNTA", preg && q.vis.abierto, "la pregunta no se abrio con el visor abierto (pregunta %d, visor %d, por L3 %d, estado %s)", (int)preg,
		       (int)q.vis.abierto, (int)porL3, control::NOMBRE_ESTADO[g.ctl.estado]);
		if (preg) {
			const size_t nOps = g.ctl.opciones.size();
			b.apretar(PAD_RIGHT, 0.2f);   // -> cementerio (el visor tiene los botones)
			b.apretar(PAD_CROSS, 0.2f);
			const bool det = g.ctl.estado == control::DETALLE && g.ctl.antesDetalle == control::MENU;
			b.caso("CU-FT-VISOR-PREGUNTA", det, "X en el visor no abrio el detalle sobre la pregunta (estado %s, vuelve a %s)", control::NOMBRE_ESTADO[g.ctl.estado],
			       control::NOMBRE_ESTADO[g.ctl.antesDetalle]);
			b.apretar(PAD_CIRCLE, 0.1f);
			b.caso("CU-FT-VISOR-PREGUNTA",
			       q.vis.abierto && g.ctl.estado == control::MENU && g.ctl.enPregunta && g.ctl.opciones.size() == nOps && d.ef.preg.tipo == ia::P_RESPUESTA && d.ef.preg.lado == 0,
			       "circulo en el detalle no volvio al visor con la pregunta (visor %d, estado %s, pregunta %d, opciones %d / %d)", (int)q.vis.abierto,
			       control::NOMBRE_ESTADO[g.ctl.estado], (int)d.ef.preg.tipo, (int)g.ctl.opciones.size(), (int)nOps);
			b.apretar(PAD_CIRCLE, 0.1f);
			b.caso("CU-FT-VISOR-PREGUNTA", !q.vis.abierto && g.ctl.estado == control::MENU && g.ctl.enPregunta && d.ef.preg.tipo == ia::P_RESPUESTA,
			       "cerrar el visor no dejo la pregunta a la vista (visor %d, estado %s)", (int)q.vis.abierto, control::NOMBRE_ESTADO[g.ctl.estado]);
			b.esperar(0.8f);
			b.apretar(PAD_CIRCLE, 0.2f);   // (en la pregunta el circulo es "No activar")
			b.esperarQue(20.0f, [&] { return b.libre() || d.turno != t; });
			b.caso("CU-FT-VISOR-PREGUNTA", d.turno == t && d.activo == 0 && g.ctl.faseObjetivo < 0,
			       "los circulos con la pregunta detras del visor terminaron el turno (turno %d -> %d, pedido %d)", t, d.turno, g.ctl.faseObjetivo);
		}
		b.rivalActiva.clear();
		b.resolverUI();
	}
	// 5) en el turno del rival: el visor del cementerio del rival, el detalle y los circulos; el turno del rival sigue
	// solo y vuelve al jugador navegando
	{
		inyectar(b, conCementerio(mesaSimple(g.baseIA, mazo)).d);
		b.esperar(0.3f);
		const int t = d.turno;
		for (int k = 0; k < 4 && d.turno == t && d.ganador < 0; k++) b.pasarFase();
		const bool rival = b.esperarQue(5.0f, [&] { return d.activo == 1; });
		b.apretar(PAD_L3, 0.1f);
		b.apretar(PAD_RIGHT, 0.1f);
		b.apretar(PAD_SQUARE, 0.1f);   // -> el del rival
		const bool visor = q.vis.abierto && q.vis.lado == 1 && q.vis.pestana == visores::P_CEMENTERIO;
		b.apretar(PAD_CROSS, 0.1f);
		const bool det = g.ctl.estado == control::DETALLE;
		b.apretar(PAD_CIRCLE, 0.1f);
		const bool vuelve = q.vis.abierto && g.ctl.estado != control::DETALLE;
		b.apretar(PAD_CIRCLE, 0.1f);
		b.apretar(PAD_CIRCLE, 0.1f);
		b.asentar(40.0f);
		b.caso("CU-FT-VISOR-TURNO-RIVAL", rival && visor && det && vuelve, "visor en el turno del rival: rival %d, visor %d, detalle %d, vuelve %d", (int)rival, (int)visor, (int)det,
		       (int)vuelve);
		b.caso("CU-FT-VISOR-TURNO-RIVAL", d.ganador >= 0 || (d.activo == 0 && d.turno > t && b.libre() && !q.vis.abierto),
		       "despues del turno del rival el jugador no quedo navegando (turno %d, activo %d, estado %s, visor %d)", d.turno, d.activo, control::NOMBRE_ESTADO[g.ctl.estado],
		       (int)q.vis.abierto);
	}
}

// ------------------------------------------------------------------------------------------------- la mesa en orden y Atacar sin blanco
// rama classic-arreglo-visor (ciclo 1, CU-FT-POSICION-MESA en Noche la Sombria): en la batalla del rival, Book of Moon
// pone boca abajo al Masked Dragon del jugador y enseguida el ataque lo destruye; el motor ya lo saco de la zona
// mientras la mesa todavia presenta el Book of Moon. La mesa muestra todo en orden (boca abajo, el ataque, el dragon
// que llama Masked Dragon) y termina igual al motor; no hay carta que se quede ni que cambie sola
static void mesaEnOrden(Bot& b)
{
	Juego& g = b.g;
	ia::Duelo& d = g.dueloIA;
	b.rivalPorBot = true;
	b.grabar = false;
	b.duelo = "mesa-en-orden";
	const int pregunta0 = opciones::preguntar();
	opciones::preguntar() = opciones::PREGUNTAR_NUNCA;   // (como en el duelo del ciclo: el jugador pasa solo)
	cub::Mesa m(&g.baseIA, ia::F_BATALLA);
	m.d.turno = 4;
	m.d.activo = 1;
	m.d.l[0].mazo.assign(20, "celtic_guardian");
	m.d.l[0].mazo[3] = "baby_dragon";   // (Masked Dragon llama a un dragon de 1500 o menos)
	m.d.l[1].mazo.assign(20, "celtic_guardian");
	const int zJ = m.mon(0, "masked_dragon");
	const int zR = m.mon(1, "gravekeeper_s_spear_soldier");
	const int libro = m.mano(1, "book_of_moon");
	m.d.recalcular();
	inyectar(b, m.d);
	// el rival, sin cuadros de por medio: Book of Moon sobre el Masked Dragon y el ataque (el jugador pasa)
	auto pasarPreguntas = [&] {
		for (int k = 0; k < 20 && d.ef.preg.tipo == ia::P_RESPUESTA; k++) {
			ia::Jugada p;
			p.tipo = ia::J_PASAR;
			d.aplicar(p);
		}
	};
	ia::Jugada j = Bot::activacionDe(d, 1, ia::Lugar{ 1, ia::EN_MANO, libro });
	j.objs = { ia::Lugar{ 0, ia::EN_MONSTRUO, zJ } };
	const bool activo = d.aplicar(j);
	pasarPreguntas();
	const bool abajo = d.l[0].m[(size_t)zJ].bocaAbajo && d.l[0].m[(size_t)zJ].defensa;
	ia::Jugada a;
	a.tipo = ia::J_ATACAR;
	a.zona = zR;
	a.objetivo = zJ;
	const bool ataco = d.aplicar(a);
	pasarPreguntas();
	b.caso("CU-FT-MESA-EN-ORDEN", activo && abajo && ataco, "no se armo la escena (Book of Moon %d, boca abajo %d, ataque %d)", (int)activo, (int)abajo, (int)ataco);
	const int fallas0 = b.fallasPorCaso["CU-FT-POSICION-MESA"] + b.fallasPorCaso["CU-FT-MESA-RIVAL"] + b.fallasPorCaso["CU-FT-MESA-INICIO"];
	bool vioAbajo = false;
	const int zMesa = Juego::zonaMesaIA(0, zJ);
	for (float t = 0.0f; t < 30.0f && !(b.libre() && d.activo == 0); t += g.frameTimer) {
		if (g.ctl.estado != control::NAVEGAR && g.ctl.estado != control::VOLANDO && !g.ctl.bloqueado) {
			b.resolverUI();   // (Masked Dragon: llamar al dragon)
			continue;
		}
		b.cuadro(0);
		vioAbajo |= g.ctl.zonas[(size_t)zMesa].carta == "masked_dragon" && g.ctl.zonas[(size_t)zMesa].bocaAbajo;
	}
	b.asentar(30.0f);
	const int fallas = b.fallasPorCaso["CU-FT-POSICION-MESA"] + b.fallasPorCaso["CU-FT-MESA-RIVAL"] + b.fallasPorCaso["CU-FT-MESA-INICIO"] - fallas0;
	std::string dif;
	for (int lado = 0; lado < 2; lado++)
		for (int i = 0; i < ia::ZONAS; i++) {
			const control::Celda& c = g.ctl.zonas[(size_t)Juego::zonaMesaIA(lado, i)];
			const ia::Monstruo& mo = d.l[(size_t)lado].m[(size_t)i];
			const std::string enMesa = c.vacia() ? std::string() : c.carta;
			if (enMesa != mo.id && !(lado == 1 && mo.bocaAbajo && enMesa == "?")) dif += " " + std::to_string(lado) + ":" + std::to_string(i) + " mesa '" + enMesa + "' motor '" + mo.id + "'";
		}
	b.caso("CU-FT-MESA-EN-ORDEN", vioAbajo, "la mesa no mostro al Masked Dragon boca abajo antes del ataque (el Book of Moon)");
	b.caso("CU-FT-MESA-EN-ORDEN", dif.empty() && fallas == 0, "la mesa no termino igual al motor:%s (fallas del vigia %d)", dif.c_str(), fallas);
	opciones::preguntar() = pregunta0;
	b.resolverUI();
}

// rama classic-arreglo-visor (ciclo 1, CU-FT-ACCION-ATACAR en Valon / Gearfried): con dos Command Knight del rival y
// nada mas, ninguno se puede atacar y el ataque es directo. El menu ofrecia "Atacar" activo (y daba "No se puede"):
// ahora "Atacar" sale desactivado con el motivo y "Atacar directamente" hace el ataque
static void atacarSinBlanco(Bot& b)
{
	Juego& g = b.g;
	ia::Duelo& d = g.dueloIA;
	b.rivalPorBot = true;
	b.grabar = false;
	b.duelo = "atacar-sin-blanco";
	cub::Mesa m(&g.baseIA, ia::F_BATALLA);
	m.d.turno = 3;
	m.d.l[1].mazo.assign(20, "celtic_guardian");
	const int z = m.mon(0, "summoned_skull");
	m.mon(1, "command_knight");
	m.mon(1, "command_knight");
	m.d.recalcular();
	inyectar(b, m.d);
	b.esperar(0.5f);
	b.irA(Foco{ F_ZONA, Juego::zonaMesaIA(0, z) });
	b.apretar(PAD_CROSS, 0.15f);
	int kAt = -1, kDir = -1;
	for (int o = 0; o < (int)g.ctl.opciones.size(); o++) {
		const control::Opcion& op = g.ctl.opciones[(size_t)o];
		if (op.accion == control::AC_ATACAR) kAt = o;
		if (op.accion == control::AC_JUGADA && op.jugada.tipo == ia::J_ATACAR && op.jugada.objetivo < 0) kDir = o;
	}
	const bool atDesactivado = kAt >= 0 && !g.ctl.opciones[(size_t)kAt].activa && !g.ctl.opciones[(size_t)kAt].motivo.empty();
	b.caso("CU-FT-ATACAR-SIN-BLANCO", g.ctl.estado == control::MENU && atDesactivado && kDir >= 0 && g.ctl.opciones[(size_t)kDir].activa,
	       "con dos Command Knight: 'Atacar' %s y 'Atacar directamente' %s (estado %s)", kAt < 0 ? "no esta" : atDesactivado ? "desactivado" : "ACTIVO",
	       kDir < 0 ? "no esta" : "esta", control::NOMBRE_ESTADO[g.ctl.estado]);
	if (g.ctl.estado != control::MENU || kDir < 0) {
		b.resolverUI();
		return;
	}
	const int lp = d.l[1].lp;
	b.elegirOpcion(kDir);
	b.resolverUI();
	b.asentar(15.0f);
	b.caso("CU-FT-ATACAR-SIN-BLANCO", d.l[1].lp < lp, "'Atacar directamente' no hizo danno (LP del rival %d -> %d, aviso '%s')", lp, d.l[1].lp, g.ctl.aviso.c_str());
}

// ------------------------------------------------------------------------------------------------- orientacion de las figuras 3D
// rama classic-bp-terminar: las figuras miran a la pantalla (la camara de juego); solo al atacar se voltean hacia el
// objetivo, atacan y vuelven a mirar a la pantalla. Mide los angulos con la misma cuenta que figuras.h
// (orientacion_figura.h) y comprueba en el fuente que matrizFigura, la entrada clave y la camara la usan
static void orientacionFiguras(Bot& b)
{
	const glm::vec3 ojo = orientacion::ojoJuego();
	auto centro = [](int z) { return glm::vec3(control::ZONAS_MESA[z].x * control::ESCALA_MESA, 0.06f, control::ZONAS_MESA[z].z * control::ESCALA_MESA); };
	std::vector<int> jug, riv;
	for (int z = 0; z < control::NUM_ZONAS_MESA; z++) {
		const std::string id = control::ZONAS_MESA[z].id;
		if (id.find("_monstruo_") == std::string::npos) continue;
		(control::ZONAS_MESA[z].rival ? riv : jug).push_back(z);
	}
	b.caso("CU-FT-FIGURA-ORIENTACION", jug.size() == 5 && riv.size() == 5, "zonas de monstruo: %d del jugador y %d del rival", (int)jug.size(), (int)riv.size());
	const float rad = 3.14159265358979f / 180.0f;
	float peorReposo = 0.0f, peorDefensa = 0.0f, peorAtaque = 0.0f, peorVuelta = 0.0f, peorEntrada = 0.0f, rivalMin = 999.0f;
	for (int esRival = 0; esRival < 2; esRival++) {
		const std::vector<int>& mios = esRival ? riv : jug;
		const std::vector<int>& otros = esRival ? jug : riv;
		for (int z : mios) {
			const glm::vec3 base = centro(z);
			const float g0 = orientacion::giroReposo(base, ojo);
			peorReposo = std::max(peorReposo, orientacion::grados(g0, ojo - base));
			peorDefensa = std::max(peorDefensa, orientacion::grados(g0 + orientacion::GIRA_DEFENSA_BIPEDO * rad, ojo - base));
			if (!esRival)   // el jugador en reposo no mira al rival: de espaldas al monstruo de enfrente (su misma columna)
				for (int o : otros)
					if (fabsf(centro(o).x - base.x) < 0.1f) rivalMin = std::min(rivalMin, orientacion::grados(g0, centro(o) - base));
			for (int o : otros) {
				for (float tg = 0.3f; tg <= 1.2f; tg += 0.1f)   // atacando: de frente al objetivo
					peorAtaque = std::max(peorAtaque, orientacion::grados(orientacion::giroAtaque(g0, base, centro(o), tg), centro(o) - base));
				for (float tg : { 0.0f, orientacion::FIN_MIRA_ATAQUE, 2.0f, 3.0f })   // antes de empezar y al terminar: a la pantalla
					peorVuelta = std::max(peorVuelta, orientacion::grados(orientacion::giroAtaque(g0, base, centro(o), tg), ojo - base));
			}
			// la entrada de una carta clave (rigida, con esqueleto, corta, Dios): al terminar, de frente a la pantalla
			for (int c = 0; c < 2; c++)
				for (int e = 0; e < 2; e++)
					for (int dz = 0; dz < 2; dz++)
						peorEntrada = std::max(peorEntrada, orientacion::grados(g0 + orientacion::giroEntrada(c, e, dz, 1.6f), ojo - base));
		}
	}
	b.caso("CU-FT-FIGURA-ORIENTACION-REPOSO", peorReposo < 0.5f, "en reposo una figura no mira a la pantalla (desvio %.2f grados)", peorReposo);
	b.caso("CU-FT-FIGURA-ORIENTACION-REPOSO", rivalMin > 135.0f && rivalMin < 999.0f, "en reposo una figura del jugador mira hacia el rival (%.1f grados del monstruo de enfrente)", rivalMin);
	b.caso("CU-FT-FIGURA-ORIENTACION-DEFENSA", peorDefensa < orientacion::GIRA_DEFENSA_BIPEDO + 0.5f, "en defensa una figura se aparta %.1f grados de la pantalla", peorDefensa);
	b.caso("CU-FT-FIGURA-ORIENTACION-ATAQUE", peorAtaque < 1.0f, "atacando, el atacante no mira al objetivo (desvio %.2f grados)", peorAtaque);
	b.caso("CU-FT-FIGURA-ORIENTACION-VUELTA", peorVuelta < 0.5f, "antes o despues del ataque la figura no mira a la pantalla (desvio %.2f grados)", peorVuelta);
	b.caso("CU-FT-FIGURA-ORIENTACION-ENTRADA", peorEntrada < 0.5f, "al terminar la entrada de una carta clave la figura no mira a la pantalla (desvio %.2f grados)", peorEntrada);
	// el juego usa esta cuenta: matrizFigura (figuras.h), la entrada clave y la camara del juego
	const std::string fuente = std::string(__FILE__);
	const std::string dir = fuente.substr(0, fuente.find_last_of('/')) + "/../";
	auto leer = [](const std::string& r) {
		std::ifstream f(r);
		return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	};
	const std::string fig = leer(dir + "figuras.h"), ent = leer(dir + "entrada_clave_juego.h"), yr = leer(dir + "yugireload.cpp");
	b.caso("CU-FT-FIGURA-ORIENTACION-FUENTE",
	       fig.find("float giro = orientacion::giroReposo(base, OJO);") != std::string::npos &&
	           // (rama classic-ps5-ataque: haciaOtro es centroZona(otro), o el punto de MD del ataque directo)
	           (fig.find("giro = orientacion::giroAtaque(giro, centroZona(z), centroZona(otro), tg);") != std::string::npos ||
	            (fig.find("giro = orientacion::giroAtaque(giro, centroZona(z), haciaOtro, tg);") != std::string::npos &&
	             fig.find("glm::vec3 haciaOtro = z == zonaAtacante && ataqueDirecto ? destinoAtaqueMundo() : centroZona(otro);") != std::string::npos)),
	       "matrizFigura (figuras.h, %d bytes) no usa la orientacion medida (giroReposo / giroAtaque)", (int)fig.size());
	b.caso("CU-FT-FIGURA-ORIENTACION-FUENTE", ent.find("giro += orientacion::giroEntrada(") != std::string::npos, "la entrada clave no usa orientacion::giroEntrada");
	b.caso("CU-FT-FIGURA-ORIENTACION-FUENTE", yr.find("const glm::vec3 OJO = orientacion::ojoJuego();") != std::string::npos, "la camara del juego (OJO) no es orientacion::ojoJuego()");
}

// ------------------------------------------------------------------------------------------------- rendirse y mazo vacio (6, 7)
static bool nuevoDuelo(Bot& b)
{
	Juego& g = b.g;
	for (int k = 0; k < 8 && g.ctl.vista.pantalla == VD::RESULTADO; k++) b.apretar(PAD_CIRCLE, 0.5f);
	Rival r{ VD::LIBRE, 0, 0, 1, 0, -1, -1, "fin" };
	return elegirRival(b, r) && b.resolverInicio(true);
}
static void rendirseYMazoVacio(Bot& b)
{
	Juego& g = b.g;
	VD& v = g.ctl.vista;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	b.rivalPorBot = false;
	// rendirse desde la pausa
	if (g.ctl.vista.pantalla == VD::DUELO || nuevoDuelo(b)) {
		const int derrotas = v.derrotas;
		b.abandonar();
		b.esperar(0.5f);
		b.caso("CU-FT-RENDIRSE", v.pantalla == VD::RESULTADO && v.ganador == 1 && v.derrotas == derrotas + 1, "Abandonar en la pausa no termino en derrota (pantalla %d, ganador %d)", v.pantalla,
		       v.ganador);
		b.caso("CU-FT-RENDIRSE", resultado::resumen().motivo == resultado::M_RENDICION, "el resultado no dice que te rendiste (motivo %s)", resultado::nombreMotivo(resultado::resumen().motivo));
	}
	// el rival no puede robar: pierde
	if (nuevoDuelo(b)) {
		cub::Mesa m = mesaSimple(g.baseIA, mazo);
		m.d.fase = ia::F_PRINCIPAL2;
		m.d.l[1].mazo.clear();
		inyectar(b, m.d);
		b.esperar(0.3f);
		b.pasarFase();
		b.esperarQue(20.0f, [&] { return v.pantalla == VD::RESULTADO; });
		b.caso("CU-FT-MAZO-VACIO-RIVAL", g.dueloIA.ganador == 0 && v.pantalla == VD::RESULTADO && resultado::resumen().motivo == resultado::M_SIN_MAZO,
		       "el rival sin cartas en el mazo no perdio al robar (ganador %d, pantalla %d, motivo %s)", g.dueloIA.ganador, v.pantalla, resultado::nombreMotivo(resultado::resumen().motivo));
	}
	// el jugador no puede robar: pierde
	if (nuevoDuelo(b)) {
		cub::Mesa m = mesaSimple(g.baseIA, mazo);
		m.d.fase = ia::F_PRINCIPAL2;
		m.d.activo = 1;
		m.d.l[0].mazo.clear();
		inyectar(b, m.d);
		b.esperarQue(90.0f, [&] {
			if (g.ctl.estado != control::NAVEGAR && g.ctl.estado != control::VOLANDO && !g.ctl.bloqueado) b.resolverUI(5.0f);
			return v.pantalla == VD::RESULTADO || b.atascado;
		});
		b.caso("CU-FT-MAZO-VACIO-JUGADOR", g.dueloIA.ganador == 1 && v.pantalla == VD::RESULTADO && resultado::resumen().motivo == resultado::M_SIN_MAZO,
		       "el jugador sin cartas en el mazo no perdio al robar (ganador %d, pantalla %d, motivo %s)", g.dueloIA.ganador, v.pantalla, resultado::nombreMotivo(resultado::resumen().motivo));
	}
	for (int k = 0; k < 8 && v.pantalla == VD::RESULTADO; k++) b.apretar(PAD_CIRCLE, 0.5f);
}

// ------------------------------------------------------------------------------------------------- cadenas (9)
// En tu turno: Raigeki <- Magic Jammer del rival <- Solemn Judgment tuyo (3 eslabones, se resuelven al reves: Solemn
// niega Jammer y Raigeki destruye). En tu batalla: ataque <- Mirror Force del rival <- Trap Jammer tuyo. En el turno
// del rival: su ataque <- tu Mirror Force. La pila se ve numerada (consulta.pila) y se resuelve eslabon por eslabon.
static void cadenas(Bot& b)
{
	Juego& g = b.g;
	ia::Duelo& d = g.dueloIA;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	if (g.ctl.vista.pantalla != VD::DUELO && !nuevoDuelo(b)) return;
	// (la pila la mira el vigia en cada cuadro, tambien mientras el bot contesta: b.pilaMax, pilaNums, pilaBajadas)
	auto vigilarPila = [&](float s, int& maxEslabones, int& bajadas, std::string& numeros) {
		for (float t = 0.0f; t < s && !b.atascado; t += g.frameTimer) {
			if (g.ctl.estado != control::NAVEGAR && g.ctl.estado != control::VOLANDO && !g.ctl.bloqueado) b.resolverUI(8.0f);
			else b.cuadro(0);
			if (b.alDia() && d.ef.cadena.empty() && d.ef.preg.tipo == ia::P_NADA && g.ctl.estado == control::NAVEGAR && g.consulta.pila.vivos() == 0 && t > 1.0f) break;
		}
		maxEslabones = b.pilaMax, bajadas = b.pilaBajadas, numeros = b.pilaNums;
	};
	auto ordenResuelve = [&](size_t s0) {
		std::string r;
		for (size_t k = s0; k < d.sucesos.size(); k++)
			if (d.sucesos[k].tipo == ia::S_RESUELVE) r += d.sucesos[k].carta + " ";
		return r;
	};
	// 1) Raigeki <- Magic Jammer <- Solemn Judgment
	{
		cub::Mesa m(&g.baseIA);
		m.d.l[0].mazo = mazo;
		m.d.l[1].mazo.assign(30, "celtic_guardian");
		m.mon(1, "celtic_guardian");
		m.mon(1, "summoned_skull");
		m.colocar(0, "solemn_judgment", 1);
		m.colocar(1, "magic_jammer", 1);
		m.d.l[0].mano = { "raigeki" };
		m.d.l[1].mano = { "vorse_raider", "battle_ox" };
		m.d.recalcular();
		inyectar(b, m.d);
		b.esperar(0.3f);
		b.rivalPorBot = true;
		b.rivalActiva = "magic_jammer";
		b.cartaPreferida = "solemn_judgment";
		const size_t s0 = d.sucesos.size();
		b.reiniciarPila();
		b.accion(Foco{ F_MANO_J, 0 }, [](const control::Opcion& o) { return (o.accion == control::AC_ACTIVAR_EFECTO || o.accion == control::AC_ACTIVAR_MAGIA) && o.activa; });
		int maxE = 0, bajadas = 0;
		std::string nums;
		vigilarPila(25.0f, maxE, bajadas, nums);
		const std::string orden = ordenResuelve(s0);
		b.caso("CU-FT-CADENA-TRES", maxE >= 3, "la pila en pantalla llego a %d eslabones (esperados 3: Raigeki, Magic Jammer, Solemn); numeros: %s", maxE, nums.c_str());
		b.caso("CU-FT-CADENA-TRES", nums.find("1j 2r 3j") == 0, "los eslabones no se ven numerados 1 (tuyo), 2 (rival), 3 (tuyo): '%s'", nums.c_str());
		b.caso("CU-FT-CADENA-LIFO", orden.find("solemn_judgment") < orden.find("raigeki") && orden.find("raigeki") != std::string::npos,
		       "no se resolvio al reves (ultimo primero): %s", orden.c_str());
		b.caso("CU-FT-CADENA-LIFO", d.l[1].monstruos() == 0, "Raigeki no destruyo (Solemn tenia que negar a Magic Jammer): el rival tiene %d monstruos", d.l[1].monstruos());
		b.caso("CU-FT-CADENA-PASO-A-PASO", bajadas >= 2, "la pila no se resolvio eslabon por eslabon en pantalla (%d bajadas)", bajadas);
		const std::string dif = b.difMesa();
		b.caso("CU-FT-CADENA-TRES", dif.empty() || !b.alDia(), "tras la cadena la mesa no es el motor: %s", dif.c_str());
		b.rivalActiva.clear();
		b.cartaPreferida.clear();
	}
	// 2) tu ataque <- Mirror Force del rival <- Trap Jammer tuyo
	{
		cub::Mesa m(&g.baseIA, ia::F_BATALLA);
		m.d.l[0].mazo = mazo;
		m.d.l[1].mazo.assign(30, "celtic_guardian");
		m.mon(0, "summoned_skull");
		m.mon(1, "celtic_guardian");
		m.colocar(0, "trap_jammer", 1);
		m.colocar(1, "mirror_force", 1);
		m.d.recalcular();
		inyectar(b, m.d);
		b.esperar(0.3f);
		b.rivalPorBot = true;
		b.rivalActiva = "mirror_force";
		b.cartaPreferida = "trap_jammer";
		const size_t s0 = d.sucesos.size();
		b.reiniciarPila();
		b.accion(Foco{ F_ZONA, Juego::zonaMesaIA(0, 0) }, [](const control::Opcion& o) { return o.accion == control::AC_ATACAR && o.activa; });
		int maxE = 0, bajadas = 0;
		std::string nums;
		vigilarPila(25.0f, maxE, bajadas, nums);
		bool jammer = false, mirror = false;
		for (size_t k = s0; k < d.sucesos.size(); k++) {
			jammer |= d.sucesos[k].tipo == ia::S_ACTIVA_MT && d.sucesos[k].carta == "trap_jammer";
			mirror |= d.sucesos[k].tipo == ia::S_ACTIVA_MT && d.sucesos[k].carta == "mirror_force";
		}
		b.caso("CU-FT-CADENA-RESPUESTA", mirror && jammer && maxE >= 2, "Mirror Force del rival (%d) y Trap Jammer tuyo en respuesta (%d): la pila llego a %d", (int)mirror, (int)jammer, maxE);
		b.caso("CU-FT-CADENA-RESPUESTA", !d.l[0].m[0].vacia(), "Trap Jammer no salvo a Summoned Skull de Mirror Force");
		b.rivalActiva.clear();
		b.cartaPreferida.clear();
	}
	// 3) en el turno del rival: su ataque <- tu Mirror Force
	{
		cub::Mesa m(&g.baseIA, ia::F_BATALLA);
		m.d.activo = 1;
		m.d.l[0].mazo = mazo;
		m.d.l[1].mazo.assign(30, "celtic_guardian");
		m.mon(0, "celtic_guardian");
		const int zr = m.mon(1, "summoned_skull");
		m.colocar(0, "mirror_force", 1);
		m.d.recalcular();
		inyectar(b, m.d);
		b.esperar(0.3f);
		b.rivalPorBot = true;
		b.cartaPreferida = "mirror_force";
		const size_t s0 = d.sucesos.size();
		b.reiniciarPila();
		ia::Jugada j;
		j.tipo = ia::J_ATACAR;
		j.zona = zr;
		j.objetivo = 0;
		d.aplicar(j);
		int maxE = 0, bajadas = 0;
		std::string nums;
		vigilarPila(25.0f, maxE, bajadas, nums);
		bool mirror = false;
		for (size_t k = s0; k < d.sucesos.size(); k++) mirror |= d.sucesos[k].tipo == ia::S_ACTIVA_MT && d.sucesos[k].carta == "mirror_force";
		b.caso("CU-FT-CADENA-TURNO-RIVAL", mirror && d.l[1].monstruos() == 0, "en el turno del rival no se pudo responder a su ataque con Mirror Force (activada %d, monstruos del rival %d)",
		       (int)mirror, d.l[1].monstruos());
		b.cartaPreferida.clear();
		b.atascado = false;
	}
}

// ------------------------------------------------------------------------------------------------- constructor (8)
// Con el mando, en el juego: MIS MAZOS -> + Nuevo mazo -> nombre con el teclado en pantalla -> editor: tope 60 del
// principal, minimo 40 para guardar, 3 copias, banlist, tope 15 de fusion; guardar; volver a abrirlo (cargar); otro
// juego que lee los mismos datos lo tiene (persiste); borrarlo.
static void escribirTeclado(Bot& b, const std::string& txt)
{
	constructor::Constructor& c = b.g.constructorMazo;
	for (char ch : txt) {
		if (ch == ' ') {
			b.apretar(PAD_TRIANGLE, 0.05f);
			continue;
		}
		const char up = (char)toupper((unsigned char)ch);
		int f = -1, k = -1;
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 10; j++)
				if (constructor::TECLAS[i][j][0] == up && constructor::TECLAS[i][j][1] == 0) f = i, k = j;
		if (f < 0) continue;
		const bool quiereMayus = isupper((unsigned char)ch) != 0;
		if (quiereMayus != c.mayus && isalpha((unsigned char)ch)) b.apretar(PAD_R1, 0.05f);
		for (int v = 0; v < 8 && c.teclaF != f; v++) b.apretar(c.teclaF < f ? PAD_DOWN : PAD_UP, 0.05f);
		for (int v = 0; v < 12 && c.teclaC != k; v++) b.apretar(c.teclaC < k ? PAD_RIGHT : PAD_LEFT, 0.05f);
		b.apretar(PAD_CROSS, 0.05f);
	}
}
static void constructorFT(Bot& b, const std::string& assets)
{
	Juego& g = b.g;
	VD& v = g.ctl.vista;
	constructor::Constructor& c = g.constructorMazo;
	const mazo::Coleccion& col = g.coleccion;
	b.duelo = "constructor";
	for (int k = 0; k < 8 && v.pantalla == VD::RESULTADO; k++) b.apretar(PAD_CIRCLE, 0.5f);
	if (v.pantalla == VD::DUELO) b.abandonar();
	for (int k = 0; k < 8 && v.pantalla == VD::RESULTADO; k++) b.apretar(PAD_CIRCLE, 0.5f);
	for (int k = 0; k < 6 && v.pantalla == VD::ELEGIR_RIVAL; k++) b.apretar(PAD_CIRCLE, 0.4f);
	if (v.pantalla != VD::MENU) {
		b.caso("CU-FT-CONS-ABRIR", false, "no se llego al menu principal (pantalla %d)", v.pantalla);
		return;
	}
	const std::vector<int> ops = g.accionesMenu();
	for (int k = 0; k < 10 && ops[(size_t)g.menuP.opcion] != Juego::MP_CONSTRUIR; k++) b.apretar(PAD_DOWN, 0.15f);
	b.apretar(PAD_CROSS, 0.3f);
	if (!b.caso("CU-FT-CONS-ABRIR", c.abierto && c.pantalla == constructor::P_LISTA, "CONSTRUCCION DE DECK no abrio MIS MAZOS")) return;
	const size_t mazos0 = g.biblioteca.mazos.size();
	// + Nuevo mazo y su nombre con el teclado en pantalla
	for (int k = 0; k < 20 && c.cursorLista != (int)g.biblioteca.mazos.size(); k++) b.apretar(PAD_UP, 0.05f);
	b.apretar(PAD_CROSS, 0.1f);
	b.caso("CU-FT-CONS-NOMBRE", c.capa == constructor::K_TECLADO, "X en + Nuevo mazo no abrio el teclado");
	escribirTeclado(b, "Prueba FT");
	b.caso("CU-FT-CONS-NOMBRE", c.textoTeclado == "Prueba FT", "el teclado escribio '%s'", c.textoTeclado.c_str());
	b.apretar(PAD_OPTIONS, 0.1f);
	b.caso("CU-FT-CONS-NOMBRE", c.pantalla == constructor::P_EDITOR && c.mazo.nombre == "Prueba FT", "no entro al editor del mazo nuevo ('%s')", c.mazo.nombre.c_str());
	// monstruos normales hasta 60, y el 61 no entra
	b.apretar(PAD_L3, 0.05f);
	b.analogico(g.mando.r2, 1.0f);   // (clase rapida: Normal)
	b.caso("CU-FT-CONS-FILTRO", c.filtros.clase == 1, "R2 no paso la clase rapida a Normal (%d)", c.filtros.clase);
	for (int guard = 0; c.mazo.principal.size() < 60 && guard < 600; guard++) {
		const mazo::Carta* k = c.elegida();
		if (k && c.mazo.sePuedeAgregar(col, k->id)) b.apretar(PAD_CROSS, 0.02f);
		else b.apretar(PAD_RIGHT, 0.02f);
	}
	b.caso("CU-FT-CONS-TOPE-60", c.mazo.principal.size() == 60, "no se llego a 60 cartas con el mando (%d)", (int)c.mazo.principal.size());
	for (int guard = 0; guard < 40 && c.elegida() && c.mazo.cuantas(c.elegida()->id) >= 3; guard++) b.apretar(PAD_RIGHT, 0.02f);
	c.aviso.clear();
	b.apretar(PAD_CROSS, 0.05f);
	b.caso("CU-FT-CONS-TOPE-60", c.mazo.principal.size() == 60 && !c.aviso.empty(), "con 60 cartas X agrego otra o no explico por que (%d, aviso '%s')", (int)c.mazo.principal.size(),
	       c.aviso.c_str());
	// 3 copias: la que tiene 3 no entra
	{
		std::string tres;
		for (const std::string& id : c.mazo.principal)
			if (c.mazo.cuantas(id) == 3) tres = id;
		mazo::Mazo m = c.mazo;
		std::string motivo;
		for (size_t q = 0; q < m.principal.size(); q++)
			if (m.principal[q] != tres) {
				m.principal.erase(m.principal.begin() + (long)q);
				break;
			}
		b.caso("CU-FT-CONS-3-COPIAS", !tres.empty() && !m.agregar(col, tres, &motivo) && motivo.find("3") != std::string::npos, "una cuarta copia de %s entro o sin el motivo (%s)", tres.c_str(),
		       motivo.c_str());
	}
	// quitar con cuadrado desde el mazo (stick derecho a la izquierda) hasta 39: no se puede guardar; 40 si
	b.analogico(g.mando.right_x, -1.0f);
	b.caso("CU-FT-CONS-MINIMO-40", c.area == constructor::A_MAZO, "el stick derecho no paso al mazo");
	for (int guard = 0; c.mazo.principal.size() > 39 && guard < 40; guard++) b.apretar(PAD_SQUARE, 0.02f);
	c.aviso.clear();
	b.apretar(PAD_OPTIONS, 0.1f);
	b.caso("CU-FT-CONS-MINIMO-40", c.pantalla == constructor::P_EDITOR && c.aviso.find("No se puede guardar") == 0, "con 39 cartas se guardo (aviso '%s')", c.aviso.c_str());
	// la banlist: una prohibida no entra (por el nombre, con el teclado de busqueda)
	b.analogico(g.mando.right_x, 1.0f);
	{
		const mazo::Carta* prohibida = nullptr;
		for (const mazo::Carta& k : col.cartas)
			if (!k.firma && k.limiteBase == 0 && k.clase != "fusion" && !prohibida && k.nombre.size() > 4) prohibida = &k;
		if (prohibida) {
			std::string motivo;
			mazo::Mazo m = c.mazo;
			b.caso("CU-FT-CONS-BANLIST", !col.permitirProhibidas && !m.agregar(col, prohibida->id, &motivo), "la prohibida %s entro en el mazo", prohibida->id.c_str());
			b.apretar(PAD_TRIANGLE, 0.1f);
			if (c.capa == constructor::K_FILTROS) {
				b.apretar(PAD_CROSS, 0.1f);
				std::string busca;
				for (char ch : prohibida->nombre) {
					if (!isalnum((unsigned char)ch) && ch != ' ') break;
					busca += ch;
				}
				escribirTeclado(b, busca.substr(0, 8));
				b.apretar(PAD_OPTIONS, 0.1f);
				if (c.capa != constructor::K_NADA) b.apretar(PAD_CIRCLE, 0.1f);
				for (int guard = 0; guard < 30 && c.elegida() && c.elegida()->id != prohibida->id; guard++) b.apretar(PAD_RIGHT, 0.02f);
				if (c.elegida() && c.elegida()->id == prohibida->id) {
					const size_t n0 = c.mazo.principal.size();
					c.aviso.clear();
					b.apretar(PAD_CROSS, 0.05f);
					b.caso("CU-FT-CONS-BANLIST", c.mazo.principal.size() == n0 && !c.aviso.empty(), "X en la prohibida %s la agrego o no dijo por que", prohibida->id.c_str());
				}
				b.apretar(PAD_L3, 0.05f);
			}
		}
	}
	// vuelve a 40 y fusiones hasta 15 (la 16 no entra)
	b.apretar(PAD_L3, 0.05f);
	b.analogico(g.mando.r2, 1.0f);   // (Normal)
	for (int guard = 0; c.mazo.principal.size() < 40 && guard < 200; guard++) {
		const mazo::Carta* k = c.elegida();
		if (k && c.mazo.sePuedeAgregar(col, k->id)) b.apretar(PAD_CROSS, 0.02f);
		else b.apretar(PAD_RIGHT, 0.02f);
	}
	b.apretar(PAD_L3, 0.05f);
	for (int k = 0; k < 4; k++) b.analogico(g.mando.r2, 1.0f);
	for (int guard = 0; c.mazo.fusion.size() < 15 && guard < 400; guard++) {
		const mazo::Carta* k = c.elegida();
		if (k && c.mazo.sePuedeAgregar(col, k->id)) b.apretar(PAD_CROSS, 0.02f);
		else b.apretar(PAD_RIGHT, 0.02f);
	}
	b.caso("CU-FT-CONS-TOPE-FUSION", c.mazo.fusion.size() == 15, "no se llego a 15 fusiones (%d)", (int)c.mazo.fusion.size());
	for (int guard = 0; guard < 60 && c.elegida() && c.mazo.cuantas(c.elegida()->id) >= std::min(3, c.elegida()->limite); guard++) b.apretar(PAD_RIGHT, 0.02f);
	c.aviso.clear();
	b.apretar(PAD_CROSS, 0.05f);
	b.caso("CU-FT-CONS-TOPE-FUSION", c.mazo.fusion.size() == 15 && !c.aviso.empty(), "con 15 fusiones X agrego otra o no explico por que (%d)", (int)c.mazo.fusion.size());
	// guardar
	const mazo::Mazo armado = c.mazo;
	b.apretar(PAD_OPTIONS, 0.2f);
	if (c.pantalla == constructor::P_EDITOR && c.capa == constructor::K_NADA) b.apretar(PAD_CIRCLE, 0.2f);
	if (c.capa == constructor::K_PREGUNTA) b.apretar(PAD_CROSS, 0.2f);
	b.caso("CU-FT-CONS-GRABAR", c.pantalla == constructor::P_LISTA && g.biblioteca.mazos.size() == mazos0 + 1 && g.biblioteca.mazos.back().nombre == "Prueba FT" &&
	                                g.biblioteca.mazos.back().principal.size() == 40 && g.biblioteca.mazos.back().fusion.size() == 15,
	       "el mazo no quedo grabado en MIS MAZOS (%d mazos, pantalla %d)", (int)g.biblioteca.mazos.size(), c.pantalla);
	// cargar: abrirlo de nuevo (X > Editar) tiene lo mismo
	const int nuevo = (int)g.biblioteca.mazos.size() - 1;
	for (int k = 0; k < 20 && c.cursorLista != nuevo; k++) b.apretar(c.cursorLista < nuevo ? PAD_DOWN : PAD_UP, 0.05f);
	b.apretar(PAD_CROSS, 0.1f);
	if (c.capa == constructor::K_PREGUNTA) b.apretar(PAD_CROSS, 0.2f);   // (Editar)
	b.caso("CU-FT-CONS-CARGAR", c.pantalla == constructor::P_EDITOR && c.mazo.principal == armado.principal && c.mazo.fusion == armado.fusion, "al abrirlo de nuevo no tiene las mismas cartas");
	b.apretar(PAD_CIRCLE, 0.2f);
	if (c.capa == constructor::K_PREGUNTA) b.apretar(PAD_CIRCLE, 0.2f);
	// persiste: otro juego con los mismos datos (como reiniciar la consola)
	{
		Juego* otro = new Juego;
		otro->preparar(assets);
		bool esta = false;
		for (const mazo::Mazo& m : otro->biblioteca.mazos) esta |= m.nombre == "Prueba FT" && m.principal == armado.principal && m.fusion == armado.fusion;
		b.caso("CU-FT-CONS-PERSISTE", esta, "al reiniciar el juego el mazo grabado no esta (o cambio)");
		delete otro;
	}
	// borrarlo (X > Borrar > confirmar) y que no vuelva al reiniciar
	if (c.pantalla == constructor::P_LISTA) {
		for (int k = 0; k < 20 && c.cursorLista != nuevo; k++) b.apretar(c.cursorLista < nuevo ? PAD_DOWN : PAD_UP, 0.05f);
		b.apretar(PAD_CROSS, 0.1f);
		for (int k = 0; k < 6 && c.capa == constructor::K_PREGUNTA && c.opcion != 4; k++) b.apretar(PAD_DOWN, 0.05f);
		b.apretar(PAD_CROSS, 0.1f);
		b.caso("CU-FT-CONS-BORRAR", c.pregunta == constructor::Q_BORRAR, "Borrar no pregunto antes");
		b.apretar(PAD_UP, 0.05f);
		b.apretar(PAD_CROSS, 0.2f);
		b.caso("CU-FT-CONS-BORRAR", g.biblioteca.mazos.size() == mazos0, "el mazo no se borro (%d mazos)", (int)g.biblioteca.mazos.size());
		Juego* otro = new Juego;
		otro->preparar(assets);
		bool esta = false;
		for (const mazo::Mazo& m : otro->biblioteca.mazos) esta |= m.nombre == "Prueba FT";
		b.caso("CU-FT-CONS-BORRAR", !esta, "el mazo borrado vuelve al reiniciar");
		delete otro;
	}
	for (int k = 0; k < 4 && c.abierto; k++) b.apretar(PAD_CIRCLE, 0.3f);
	b.caso("CU-FT-CONS-ABRIR", !c.abierto && v.pantalla == VD::MENU, "circulo no salio del constructor");
}

#include "cu_ritmo.h"   // rama classic-md-ritmo: CU-FT-ANIM-* (el ritmo de Master Duel)
#include "cu_entradas.h"   // rama classic-md-entradas: CU-FT-ANIM-FUSION/RITUAL-*, CU-FT-ENGRANAJE-MD-*
#include "cu_pregunta.h"   // rama classic-arreglo-ciclo-md: CU-FT-PREGUNTA-FANTASMA-*, la pregunta que no se veia

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_front_total <carpeta con ui/> <salida.tsv> [duelos|cartas|todo] [tope duelos] [tope cartas] [primera carta]\n");
		return 2;
	}
	const std::string assets = std::string(argv[1]) + "/", salida = argv[2];
	const std::string modo = argc > 3 ? argv[3] : "todo";
	const int topeDuelos = argc > 4 ? atoi(argv[4]) : -1, topeCartas = argc > 5 ? atoi(argv[5]) : -1;
	const std::string desde = argc > 6 ? argv[6] : "";
	const std::string datos = assets + "datos_ft/";
	system(("rm -rf '" + datos + "' && mkdir -p '" + datos + "'").c_str());
	setenv("YUGI_MAZO", (datos + "mazo.txt").c_str(), 1);
	for (const char* v : { "YUGI_GUION", "YUGI_IDIOMA", "YUGI_FIGURAS", "YUGI_CAMARA", "YUGI_VELOCIDAD", "YUGI_RESULTADO" }) unsetenv(v);
	idioma::cargar(datos + "idioma.txt");
	Juego* g = new Juego;
	g->preparar(assets);
	if (!g->baseIAlista) {
		fprintf(stderr, "sin datos en %s\n", assets.c_str());
		return 2;
	}
	// todo desbloqueado (como un jugador que ya avanzo): los 8 niveles del duelo libre y todas las sagas
	g->ctl.vista.nivelMax = ia::NIVEL_MAX;
	for (const personajes::Personaje& p : g->historia.lista) g->historia.vencidos.insert(p.id);
	g->actualizarDesbloqueos();
	opciones::preguntar() = opciones::PREGUNTAR_AUTO;
	Bot b(*g);
	b.esperar(0.5f);
	// las cartas de campo con escenario (campos.h: la tabla usa Vulkan, se lee del fuente) piden su escenario en el duelo
	{
		const std::string fuente = std::string(__FILE__);
		campos::idsCampo() = [&] {
			std::vector<std::string> v;
			for (const std::string& id : cuc::leerCampos(fuente.substr(0, fuente.find_last_of('/')) + "/../campos.h")) v.push_back(id);
			return v;
		}();
		fprintf(stderr, "cartas de campo con escenario: %d\n", (int)campos::idsCampo().size());
	}
	if (modo == "duelos" || modo == "todo") duelos(b, topeDuelos);
	if (modo == "cartas" || modo == "todo") cartas(b, topeCartas, desde);
	if (modo == "combate" || modo == "todo") combates(b, topeCartas);
	if (modo == "extra" || modo == "todo") {
		if (g->ctl.vista.pantalla != VD::DUELO) nuevoDuelo(b);
		fasesYCancelar(b);
		fasesOptions(b);   // rama classic-md-fases (antes: terminarDesdeBatalla de classic-bp-terminar)
		preguntaFantasma(b);   // rama classic-arreglo-ciclo-md
		unionUnaVez(b);        // rama classic-arreglo-ciclo-md
		engranaje(b);      // rama classic-md-fases
		circuloAnidado(b);         // rama classic-arreglo-visor
		mesaEnOrden(b);            // rama classic-arreglo-visor
		atacarSinBlanco(b);        // rama classic-arreglo-visor
		orientacionFiguras(b);     // rama classic-bp-terminar
		cadenas(b);
		rendirseYMazoVacio(b);
		constructorFT(b, assets);
	}
	if (modo == "pregunta") {   // (rama classic-arreglo-ciclo-md: solo las fases con OPTIONS y la pregunta fantasma)
		if (g->ctl.vista.pantalla != VD::DUELO) nuevoDuelo(b);
		fasesOptions(b);
		preguntaFantasma(b);
		unionUnaVez(b);
	}
	if (modo == "ritmo" || modo == "extra" || modo == "todo") ritmoMD(b);   // rama classic-md-ritmo
	if (modo == "ritmo" || modo == "extra" || modo == "todo") entradasMD(b);   // rama classic-md-entradas
	fprintf(stderr, "botones apretados: %d\n", b.nBotones);
	return cu::resumen(salida.c_str());
}
