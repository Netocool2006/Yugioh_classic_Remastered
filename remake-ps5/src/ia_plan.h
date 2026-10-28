/*
 * Yugioh Classic Remaster - VARIEDAD del rival (rama classic-variedad). Se incluye DENTRO de namespace ia desde ia.h,
 * justo antes del Cerebro (usa Base, Duelo y el motor de efectos; el Cerebro y el Turno lo usan a el).
 *
 * El problema: contra el mismo rival, la IA de Joey the Passion se volvia predecible. Aqui:
 *  1. PLANES DE JUEGO: al empezar cada duelo el rival elige un plan (aggro, control, bloqueo + quema, enjambre,
 *     combo/OTK, castigo con trampas, molino/robo, tempo con cambio de control, victoria alternativa), al azar
 *     ponderado por su personalidad, por lo que su mazo permite, por lo que uso las ultimas veces (no repite) y, los
 *     calculadores, por como juega el jugador. El plan cambia la personalidad efectiva (agresividad, cautela,
 *     farol...) y suma/resta a cada jugada candidata (ajustarPlan). Si el plan va perdiendo a mitad de duelo (LP y
 *     ventaja de cartas) cambia de plan (revisarPlan) y lo dice ("¡Cambio de estrategia!").
 *  2. MAZOS DINAMICOS: mazoDinamico arma el mazo del duelo desde el POOL del rival (sus 3 mazos de rivales.txt /
 *     personajes_rivales.txt) + su NUCLEO (lo que esta en todos sus mazos y sus cartas unicas: su identidad) + un
 *     PAQUETE del plan (cartas con el papel del plan, del propio pool o de los mazos de rivales de su nivel o menos,
 *     y los combos de ese plan). Siempre valido: 40-42 cartas, limites Goat, rituales con su magia, fusiones con sus
 *     materiales, techo de ATK medio de sus mazos (la dificultad por nivel se mantiene).
 *  3. MEMORIA (Memoria: variedad.txt junto a progreso.txt): por rival, los ultimos planes y el ultimo mazo (se
 *     evitan); del jugador, sus tendencias (Lectura: trampas que de verdad activa, faroles, cuanto ataca, muros), que
 *     se acumulan entre duelos y decaen con el tiempo; y la racha (dificultad adaptable). Solo informacion publica:
 *     lo que el jugador hizo en la mesa (sucesos), nunca su mano ni sus cartas boca abajo.
 *  4. VARIACION TACTICA: un Estilo por duelo (volcar o guardar, colocar las trampas antes o despues de la batalla,
 *     "trampa al reves" dejando el campo vacio a proposito, orden de jugadas) y un humor por turno; combos conocidos
 *     (combos.txt) con bono al jugar sus piezas; horizonte de 2 turnos (Proyecto: guardar/colocar monstruos para el
 *     sacrificio del grande de la mano, juntar materiales de fusion o ritual, retener la trampa clave para el ataque
 *     que importa, no gastar quitamonstruos en monstruos chicos si el plan pide esperar).
 *  5. DIFICULTAD ADAPTABLE: con la racha del jugador (gana seguido: +; pierde seguido: -) baja o sube el ruido de la
 *     decision, los planes se vuelven mas fuertes (encaje con el mazo) o menos castigadores y el paquete del mazo mas
 *     afinado. Acotado (+4..-3) y sin tocar el nivel del rival ni su pool. Se apaga en Ajustes (Memoria::adaptable).
 */

// rama classic-pulido: guardado atomico de variedad.txt. Este archivo se incluye DENTRO de namespace ia: se sale un
// momento para incluir guardado.h a nivel global (sin tocar ia.h)
}   // namespace ia
#include "guardado.h"
namespace ia {

// ------------------------------------------------------------------------------------------------- planes
enum PlanJuego { PL_AGRESIVO = 0, PL_CONTROL, PL_BLOQUEO, PL_ENJAMBRE, PL_COMBO, PL_CASTIGO, PL_MOLINO, PL_TEMPO, PL_ALTERNATIVA, NUM_PLANES };
struct DefPlan {
	const char* clave;
	const char* principales;   // papeles (roles_cartas.txt) que el plan busca
	const char* secundarios;
	float dAgr, dCaut, dFarol, dRiesgo, dPac, dUmbral;   // cambios a la personalidad
	int monstruos;                                       // monstruos del mazo (aprox.)
	float bonoAtaque;                                    // a cada ataque
};
static const DefPlan PLANES[NUM_PLANES] = {
	//  clave          princ  sec    agr    caut   farol  riesgo pac    umbral  mons  ataque
	{ "agresivo",     "g",   "kxe",  0.30f, -0.20f, -0.15f, 0.25f, -0.30f, -200.0f, 22,  120.0f },
	{ "control",      "dt",  "nr",  -0.05f,  0.10f,  0.00f, -0.10f, 0.20f,  100.0f, 17,    0.0f },
	{ "bloqueo",      "bqw", "a",   -0.35f,  0.20f,  0.20f, -0.20f, 0.35f,  250.0f, 16,  -60.0f },
	{ "enjambre",     "e",   "gr",   0.15f, -0.10f, -0.10f,  0.10f, -0.10f, -50.0f, 23,   60.0f },
	{ "combo",        "k",   "ro",   0.05f,  0.00f,  0.00f,  0.10f, 0.20f,    0.0f, 19,   20.0f },
	{ "castigo",      "c",   "nw",  -0.20f,  0.00f,  0.30f, -0.05f, 0.20f,  150.0f, 17,  -30.0f },
	{ "molino",       "mo",  "wb",  -0.20f,  0.10f,  0.10f, -0.10f, 0.30f,  150.0f, 17,  -40.0f },
	{ "tempo",        "x",   "dg",   0.15f, -0.05f,  0.00f,  0.15f, -0.05f, -50.0f, 19,   80.0f },
	{ "alternativa",  "a",   "bow", -0.40f,  0.20f,  0.20f, -0.30f, 0.40f,  300.0f, 17, -100.0f },
};
inline int planPorClave(const std::string& k)
{
	for (int i = 0; i < NUM_PLANES; i++)
		if (k == PLANES[i].clave) return i;
	return -1;
}
// afinidad de cada personalidad con cada plan (mismo orden que PERSONALIDADES y PLANES)
static const float AFINIDAD[][NUM_PLANES] = {
	//               agr   ctrl  bloq  enj   combo cast  mol   tempo alt
	/*equilibrado*/ { 1.0f, 1.0f, 0.8f, 1.0f, 1.0f, 1.0f, 0.7f, 1.0f, 0.6f },
	/*agresivo*/    { 3.0f, 0.6f, 0.3f, 2.0f, 1.2f, 0.5f, 0.3f, 1.5f, 0.2f },
	/*defensivo*/   { 0.4f, 1.5f, 2.5f, 0.7f, 0.8f, 2.0f, 1.0f, 0.6f, 1.2f },
	/*farolero*/    { 0.8f, 1.0f, 1.3f, 0.8f, 0.8f, 2.5f, 1.0f, 1.3f, 0.7f },
	/*paciente*/    { 0.5f, 1.5f, 2.0f, 0.8f, 1.5f, 1.2f, 1.5f, 0.8f, 2.0f },
	/*temerario*/   { 3.0f, 0.4f, 0.2f, 1.8f, 1.5f, 0.4f, 0.3f, 2.0f, 0.3f },
	/*calculador*/  { 1.0f, 2.0f, 1.0f, 1.0f, 1.5f, 1.3f, 1.2f, 1.5f, 1.0f },
	/*tramposo*/    { 0.8f, 1.0f, 1.5f, 0.8f, 0.8f, 2.0f, 1.5f, 2.5f, 1.0f },
	/*control*/     { 0.5f, 2.5f, 2.0f, 0.6f, 1.0f, 1.5f, 1.5f, 1.2f, 1.3f },
	/*oscuro*/      { 1.5f, 1.5f, 1.0f, 1.0f, 2.0f, 1.2f, 1.0f, 1.5f, 1.0f },
};
inline int indicePersonalidad(const std::string& clave)
{
	for (int i = 0; i < NUM_PERSONALIDADES; i++)
		if (clave == PERSONALIDADES[i].clave) return i;
	return 0;
}
// los que leen al jugador para contrarrestarlo (el resto lo hace poco)
inline float contrarresta(const std::string& pers)
{
	return pers == "calculador" || pers == "control" || pers == "oscuro" || pers == "paciente" || pers == "tramposo" ? 1.0f : 0.35f;
}

// ------------------------------------------------------------------------------------------------- papeles y combos
inline bool papel(const Base* b, const std::string& id, char r)
{
	if (!b) return false;
	auto it = b->papeles.find(id);
	return it != b->papeles.end() && it->second.find(r) != std::string::npos;
}
// 2 por cada papel principal del plan, 1 por cada secundario
inline int puntajePapel(const Base* b, const std::string& id, int plan)
{
	if (!b || plan < 0) return 0;
	auto it = b->papeles.find(id);
	if (it == b->papeles.end()) return 0;
	int s = 0;
	for (char r : it->second) {
		if (strchr(PLANES[plan].principales, r)) s += 2;
		else if (strchr(PLANES[plan].secundarios, r)) s += 1;
	}
	// combos del plan: sus piezas literales tambien son del plan
	for (const Combo& c : b->combos)
		if (planPorClave(c.plan) == plan && !c.torneo)
			for (const std::string& pz : c.piezas)
				if (pz == id) return s + 2;
	return s;
}
inline bool piezaEs(const Base* b, const std::string& pieza, const std::string& id)
{
	if (pieza.empty() || id.empty() || !b) return false;
	if (pieza[0] == '@') return pieza.size() > 1 && papel(b, id, pieza[1]);
	const Carta* c = b->carta(id);
	if (pieza[0] == '%') return c && c->subtipo == pieza.substr(1);
	if (pieza[0] == '&') return c && c->monstruo && c->raza == pieza.substr(1);
	return pieza == id;
}

// ------------------------------------------------------------------------------------------------- lectura del jugador
// Tendencias (0..1). Lo publico: lo que hizo en la mesa (sucesos), nunca su mano ni lo que hay boca abajo
struct Lectura {
	float trampas{ 0.35f };   // activa de verdad lo que coloca (responde a los ataques)
	float faroles{ 0.30f };   // coloca cartas que luego no activa
	float ataques{ 0.50f };   // ataca siempre que puede
	float muros{ 0.30f };     // coloca monstruos en defensa (muros)
	float limpia{ 0.25f };    // (rama classic-ia-humana) destruye las magias/trampas boca abajo del rival (MST, Tormenta...)
	float confianza{ 0.0f };  // 0 = no se sabe nada (primeros duelos)
	int duelos{ 0 };
	// efecto concreto: cuanto miedo dan sus cartas boca abajo (trampero: mas; farolero: menos)
	float factorMiedo() const
	{
		const float k = std::min(1.0f, 0.35f + confianza);
		return std::max(0.25f, std::min(2.2f, 1.0f + k * (1.1f * (trampas - 0.35f) - 1.2f * (faroles - 0.30f))));
	}
};
// lo que se ve en el duelo en curso (contadores sobre Duelo::sucesos)
struct Observacion {
	int turnos{ 0 }, colocadas{ 0 }, usadas{ 0 }, ataques{ 0 }, puestos{ 0 }, invocados{ 0 };
	int limpiezas{ 0 };   // (rama classic-ia-humana) quitamagias/trampas que activo (papel 't')
	size_t visto{ 0 };
	int turnoDe{ -1 };
	void mirar(const Duelo& d, int jug)
	{
		if (visto > d.sucesos.size()) *this = Observacion{};
		for (; visto < d.sucesos.size(); visto++) {
			const Suceso& s = d.sucesos[visto];
			if (s.tipo == S_TURNO) {
				turnoDe = s.lado;
				if (s.lado == jug) turnos++;
			}
			if (s.lado != jug) continue;
			if (s.tipo == S_COLOCA_MT) colocadas++;
			else if (s.tipo == S_ACTIVA_MT && (turnoDe != jug || esTrampa(d, s.carta))) usadas++;
			if (s.tipo == S_ACTIVA_MT && papel(d.base, s.carta, 't')) limpiezas++;
			else if (s.tipo == S_ATAQUE) ataques++;
			else if (s.tipo == S_COLOCA) puestos++;
			else if (s.tipo == S_INVOCA) invocados++;
		}
	}
	// los valores de este duelo (solo los que hay datos para medir: -1 = sin dato)
	float vTrampas() const { return turnos >= 2 ? std::min(1.0f, 0.9f * (float)usadas / (float)turnos) : -1.0f; }
	float vFaroles() const { return colocadas >= 2 ? std::max(0.0f, std::min(1.0f, 1.0f - (float)usadas / (float)colocadas)) : -1.0f; }
	float vAtaques() const { return turnos >= 2 ? std::min(1.0f, (float)ataques / (float)std::max(1, turnos - 1)) : -1.0f; }
	float vMuros() const { return puestos + invocados >= 2 ? (float)puestos / (float)(puestos + invocados) : -1.0f; }
	float vLimpia() const { return turnos >= 3 ? std::min(1.0f, 3.0f * (float)limpiezas / (float)turnos) : -1.0f; }
	// la lectura de este momento: lo recordado mezclado con lo visto hoy
	Lectura mezclar(const Lectura& mem) const
	{
		Lectura r = mem;
		const float w = std::min(0.5f, 0.06f * (float)turnos) * (1.0f - 0.4f * mem.confianza);   // (lo recordado pesa)
		auto mx = [&](float& v, float o) {
			if (o >= 0.0f) v = v * (1.0f - w) + o * w;
		};
		mx(r.trampas, vTrampas());
		mx(r.faroles, vFaroles());
		mx(r.ataques, vAtaques());
		mx(r.muros, vMuros());
		mx(r.limpia, vLimpia());
		r.confianza = std::min(1.0f, mem.confianza + 0.08f * (float)turnos);
		return r;
	}
};

// ------------------------------------------------------------------------------------------------- estilo y estado
// variacion tactica de ESTE duelo (sale de la semilla): la misma situacion se juega distinto de un duelo a otro
struct Estilo {
	float volcar{ 0.0f };      // -1 guarda recursos .. +1 lo vuelca todo
	bool mtAntes{ false };     // coloca las trampas antes de la batalla (si no, despues)
	bool cebo{ false };        // "trampa al reves": con trampas puestas deja el campo vacio a proposito
	float ordenInvocar{ 0.0f };// + invoca primero, - activa magias primero
	float atrevido{ 0.0f };    // -1..1 ataques dudosos
	static Estilo alAzar(Azar& a, int plan)
	{
		Estilo e;
		e.volcar = a.entre(-1.0f, 1.0f);
		e.mtAntes = a.uno() < 0.35f;
		e.cebo = (plan == PL_CASTIGO || plan == PL_BLOQUEO || plan == PL_ALTERNATIVA) && a.uno() < 0.5f;
		e.ordenInvocar = a.entre(-1.0f, 1.0f);
		e.atrevido = a.entre(-1.0f, 1.0f);
		return e;
	}
};
// lo que el Cerebro sabe de su plan (ia.h: Cerebro::plan)
struct EstadoPlan {
	int plan{ -1 };                 // -1: sin plan (la IA de antes)
	int inicial{ -1 }, cambios{ 0 }, turnosPlan{ 0 };
	float marcadorInicio{ 0.0f };
	Personalidad base{ PERSONALIDADES[0] };
	Estilo estilo;
	Lectura memoria;                // lo recordado del jugador
	Lectura lectura;                // lo recordado + lo visto hoy
	Observacion obs;
	float factorRuido{ 1.0f };      // dificultad adaptable
	float dificultad{ 0.0f };       // -3..+4 (racha)
	std::array<float, 16> humor{};  // por tipo de jugada, cambia cada turno
	std::vector<int> planesDuelo;   // los planes que uso en este duelo (el primero y los cambios)
	bool activo() const { return plan >= 0; }
};
inline Personalidad mezclaPlan(const Personalidad& b, int plan, const Estilo& e)
{
	if (plan < 0) return b;
	const DefPlan& P = PLANES[plan];
	auto c01 = [](float v) { return std::max(0.0f, std::min(1.0f, v)); };
	Personalidad p = b;
	p.agresividad = c01(b.agresividad + P.dAgr + 0.08f * e.atrevido);
	p.cautela = c01(b.cautela + P.dCaut);
	p.farol = c01(b.farol + P.dFarol);
	p.riesgo = c01(b.riesgo + P.dRiesgo + 0.08f * e.atrevido);
	p.paciencia = c01(b.paciencia + P.dPac - 0.15f * e.volcar);
	p.umbral = b.umbral + P.dUmbral - 60.0f * e.atrevido;
	return p;
}

// ------------------------------------------------------------------------------------------------- elegir el plan
// cuanto encaja el plan con estas cartas (0..~1)
inline float encaje(const Base& b, const std::vector<std::string>& cartas, int plan)
{
	if (cartas.empty()) return 0.0f;
	if (plan == PL_ALTERNATIVA) {   // solo si hay como ganar de otra forma
		int ex = 0;
		bool otra = false;
		for (const std::string& id : cartas) {
			ex += id.find("forbidden_one") != std::string::npos;
			otra |= id == "final_countdown" || id == "destiny_board";
		}
		if (ex < 3 && !otra) return 0.0f;
	}
	float s = 0.0f;
	for (const std::string& id : cartas) s += (float)puntajePapel(&b, id, plan);
	return s / (2.0f * (float)cartas.size());
}
inline std::vector<std::string> poolDe(const Rival& r)
{
	std::vector<std::string> p;
	for (const auto& v : r.principal) p.insert(p.end(), v.begin(), v.end());
	return p;
}
// pesos de cada plan para este rival (pool o mazo actual), con la memoria de los ultimos planes y la lectura
inline std::array<float, NUM_PLANES> pesosPlanes(const Base& b, const std::string& pers, const std::vector<std::string>& cartas,
                                                 const std::vector<int>& recientes, const Lectura& lec, float dif)
{
	std::array<float, NUM_PLANES> w{};
	const int ip = std::min(indicePersonalidad(pers), (int)(sizeof(AFINIDAD) / sizeof(AFINIDAD[0])) - 1);
	const float k = contrarresta(pers) * lec.confianza;
	for (int pl = 0; pl < NUM_PLANES; pl++) {
		const float e = encaje(b, cartas, pl);
		if (pl == PL_ALTERNATIVA && e <= 0.0f) continue;
		float v = AFINIDAD[ip][pl] * std::pow(0.25f + 4.0f * e, 1.0f + 0.25f * std::max(0.0f, dif));   // dificultad: planes mas fuertes
		// contrarrestar al jugador (solo lo que vio en la mesa)
		auto f = [](float x) { return std::max(0.2f, x); };
		if (pl == PL_CASTIGO) v *= f(1.0f + 1.6f * k * (lec.ataques - 0.5f));
		if (pl == PL_BLOQUEO) v *= f(1.0f + 1.0f * k * (lec.ataques - 0.5f) + 0.8f * k * (lec.muros - 0.4f));
		if (pl == PL_COMBO || pl == PL_TEMPO) v *= f(1.0f + 1.6f * k * (lec.muros - 0.4f) + (pl == PL_TEMPO ? 0.8f * k * (lec.faroles - 0.4f) : 0.0f));
		if (pl == PL_AGRESIVO) v *= f(1.0f - 0.8f * k * (lec.muros - 0.4f) - 0.8f * k * (lec.trampas - 0.4f) + 1.4f * k * (lec.faroles - 0.4f));
		if (pl == PL_CONTROL) v *= f(1.0f + 2.0f * k * (lec.trampas - 0.4f));
		// dificultad baja: planes menos castigadores
		if (dif < 0.0f && (pl == PL_CASTIGO || pl == PL_CONTROL || pl == PL_BLOQUEO || pl == PL_ALTERNATIVA)) v *= std::max(0.4f, 1.0f + 0.18f * dif);
		// no repetir lo reciente
		static const float REP[] = { 0.08f, 0.35f, 0.7f };
		for (size_t i = 0; i < recientes.size() && i < 3; i++)
			if (recientes[i] == pl) v *= REP[i];
		w[(size_t)pl] = std::max(0.0f, v);
	}
	return w;
}
inline int sortearPlan(const std::array<float, NUM_PLANES>& w, Azar& a, int salvo = -1)
{
	float t = 0.0f;
	for (int i = 0; i < NUM_PLANES; i++) t += i == salvo ? 0.0f : w[(size_t)i];
	if (t <= 0.0f) return salvo == PL_AGRESIVO ? PL_CONTROL : PL_AGRESIVO;
	float r = a.uno() * t;
	for (int i = 0; i < NUM_PLANES; i++) {
		if (i == salvo) continue;
		r -= w[(size_t)i];
		if (r <= 0.0f) return i;
	}
	return salvo == 0 ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------- memoria
// variedad.txt (junto a progreso.txt): lineas
//   adaptable 1|0          humana 1|0       racha N      aprende 1|0 (rama classic-ia-red: "IA que aprende")
//   jugador trampas faroles ataques muros confianza duelos [limpia]   (limpia: rama classic-ia-humana; opcional)
//   plan <rival> p1 p2 p3  (el mas reciente primero)      mazo <rival> id id id ...  (el ultimo)
struct Memoria {
	struct DeRival { std::vector<int> planes; std::vector<std::string> mazo; };
	std::unordered_map<std::string, DeRival> rivales;
	Lectura jugador;
	int racha{ 0 };          // + victorias seguidas del jugador, - derrotas seguidas
	bool adaptable{ true };  // dificultad adaptable (Ajustes)
	bool humana{ true };     // rama classic-ia-humana: "IA: Clasica / Humana" (Opciones; ia_humana.h)
	bool aprende{ true };    // rama classic-ia-red: "IA que aprende: Si / No" (Opciones; ia_red.h, aprendizaje.h)
	std::string ruta;

	// (rama classic-ia-humana: si el jugador pierde seguido baja como mucho un poco, -1; nunca a modo principiante)
	float dificultad() const { return adaptable ? (float)std::max(-1, std::min(4, racha)) : 0.0f; }
	// el ruido de la decision: menos si el jugador gana seguido (acotado: el nivel del rival manda)
	float factorRuido() const { return std::max(0.45f, std::min(1.1f, 1.0f - (dificultad() < 0.0f ? 0.08f : 0.14f) * dificultad())); }

	void leer(const std::string& r)
	{
		ruta = r;
		std::ifstream f(r);
		std::string l;
		while (f && std::getline(f, l)) {
			auto p = palabras(l);
			if (p.size() >= 2 && p[0] == "adaptable") adaptable = p[1] != "0";
			else if (p.size() >= 2 && p[0] == "humana") humana = p[1] != "0";
			else if (p.size() >= 2 && p[0] == "aprende") aprende = p[1] != "0";   // (rama classic-ia-red)
			else if (p.size() >= 2 && p[0] == "racha") racha = std::max(-20, std::min(20, atoi(p[1].c_str())));
			else if (p.size() >= 7 && p[0] == "jugador") {
				jugador.trampas = (float)atof(p[1].c_str());
				jugador.faroles = (float)atof(p[2].c_str());
				jugador.ataques = (float)atof(p[3].c_str());
				jugador.muros = (float)atof(p[4].c_str());
				jugador.confianza = (float)atof(p[5].c_str());
				jugador.duelos = atoi(p[6].c_str());
				if (p.size() >= 8) jugador.limpia = (float)atof(p[7].c_str());   // (rama classic-ia-humana)
			} else if (p.size() >= 2 && p[0] == "plan") {
				auto& d = rivales[p[1]];
				d.planes.clear();
				for (size_t i = 2; i < p.size(); i++)
					if (planPorClave(p[i]) >= 0) d.planes.push_back(planPorClave(p[i]));
			} else if (p.size() >= 2 && p[0] == "mazo") {
				rivales[p[1]].mazo.assign(p.begin() + 2, p.end());
			}
		}
	}
	bool guardar() const
	{
		if (ruta.empty()) return false;
		std::string s;   // (rama classic-pulido: guardado atomico, guardado.h)
		guardado::agregar(s, "adaptable %d\nhumana %d\nracha %d\njugador %.3f %.3f %.3f %.3f %.3f %d %.3f\n", adaptable ? 1 : 0, humana ? 1 : 0, racha,
		                  jugador.trampas, jugador.faroles, jugador.ataques, jugador.muros, jugador.confianza, jugador.duelos, jugador.limpia);
		guardado::agregar(s, "aprende %d\n", aprende ? 1 : 0);   // (rama classic-ia-red)
		for (const auto& kv : rivales) {
			s += "plan " + kv.first;
			for (int pl : kv.second.planes) s += std::string(" ") + PLANES[pl].clave;
			s += "\nmazo " + kv.first;
			for (const std::string& id : kv.second.mazo) s += " " + id;
			s += "\n";
		}
		return guardado::guardarAtomico(ruta, s);
	}
	// al terminar un duelo: el plan y el mazo usados, lo aprendido del jugador (decae: lo viejo pesa menos) y la racha
	void registrar(const std::string& rival, const std::vector<int>& planes, const std::vector<std::string>& mazo, const Observacion& o, int ganador)
	{
		auto& d = rivales[rival];
		for (int k = (int)planes.size() - 1; k >= 0; k--) {
			const int pl = planes[(size_t)k];
			if (pl < 0) continue;
			d.planes.erase(std::remove(d.planes.begin(), d.planes.end(), pl), d.planes.end());
			d.planes.insert(d.planes.begin(), pl);
		}
		if (d.planes.size() > 4) d.planes.resize(4);
		d.mazo = mazo;
		aprender(o);
		if (ganador == 0) racha = racha > 0 ? racha + 1 : 1;
		else if (ganador == 1) racha = racha < 0 ? racha - 1 : -1;
	}
	void aprender(const Observacion& o)
	{
		Lectura& j = jugador;
		const Lectura neutro;
		// decaer hacia lo neutro (lo de hace muchos duelos se olvida)
		j.trampas += 0.12f * (neutro.trampas - j.trampas);
		j.faroles += 0.12f * (neutro.faroles - j.faroles);
		j.ataques += 0.12f * (neutro.ataques - j.ataques);
		j.muros += 0.12f * (neutro.muros - j.muros);
		j.limpia += 0.12f * (neutro.limpia - j.limpia);
		j.confianza *= 0.92f;
		if (o.turnos >= 3) {
			const float a = 0.35f;
			auto mx = [&](float& v, float x) {
				if (x >= 0.0f) v += a * (x - v);
			};
			mx(j.trampas, o.vTrampas());
			mx(j.faroles, o.vFaroles());
			mx(j.ataques, o.vAtaques());
			mx(j.muros, o.vMuros());
			mx(j.limpia, o.vLimpia());
			j.confianza = std::min(1.0f, j.confianza + 0.25f);
			j.duelos++;
		}
	}
};

// ------------------------------------------------------------------------------------------------- mazos dinamicos
// indice de todos los mazos cargados: el nivel minimo de rival en el que aparece cada carta y en cuantos rivales esta
struct IndicePool {
	std::unordered_map<std::string, int> nivelMin, rivalesCon;
};
inline const IndicePool& indicePool(const Base& b)
{
	static const Base* de = nullptr;
	static size_t n = 0;
	static IndicePool ip;
	if (de == &b && n == b.rivales.size()) return ip;
	de = &b;
	n = b.rivales.size();
	ip = IndicePool{};
	for (const Rival& r : b.rivales) {
		std::unordered_map<std::string, int> visto;
		for (const auto& v : r.principal)
			for (const std::string& id : v) {
				auto it = ip.nivelMin.find(id);
				ip.nivelMin[id] = it == ip.nivelMin.end() ? r.nivel : std::min(it->second, r.nivel);
				visto[id] = 1;
			}
		for (auto& kv : visto) ip.rivalesCon[kv.first]++;
	}
	return ip;
}
inline float atkMedio(const Base& b, const std::vector<std::string>& m)
{
	int s = 0, n = 0;
	for (const std::string& id : m)
		if (const Carta* c = b.carta(id))
			if (c->monstruo && c->clase != "fusion") s += c->atk, n++;
	return n ? (float)s / (float)n : 0.0f;
}

// el mazo del duelo: nucleo + paquete del plan + pool, siempre valido. "ultimo": el mazo de la vez anterior (se evita);
// afinado: la dificultad adaptable (+: paquete mas grande y cartas mas fuertes)
inline MazoRival mazoDinamico(const Base& b, const Rival& r, int plan, uint32_t semilla, const std::vector<std::string>& ultimo, float afinado)
{
	MazoRival m;
	m.rival = &r;
	m.plan = plan;
	if (r.principal.empty()) return m;
	Azar a(mezclar(semilla, 0xD1CEu));
	a.siguiente();
	const IndicePool& ip = indicePool(b);
	const int nv = (int)r.principal.size();
	std::unordered_map<std::string, int> maxCop, minCop, freq, enV0;
	float atkTope = 0.0f;
	int tribTope = 0;
	for (int v = 0; v < nv; v++) {
		std::unordered_map<std::string, int> c;
		for (const std::string& id : r.principal[(size_t)v]) c[id]++;
		for (auto& kv : c) {
			maxCop[kv.first] = std::max(maxCop[kv.first], kv.second);
			auto it = minCop.find(kv.first);
			minCop[kv.first] = it == minCop.end() ? kv.second : std::min(it->second, kv.second);
			freq[kv.first]++;
			if (v == 0) enV0[kv.first] = kv.second;
		}
		atkTope = std::max(atkTope, atkMedio(b, r.principal[(size_t)v]));
		int t = 0;
		for (const std::string& id : r.principal[(size_t)v])
			if (const Carta* cc = b.carta(id)) t += cc->monstruo && cc->clase != "ritual" && cc->nivel >= 5;
		tribTope = std::max(tribTope, t);
	}
	std::unordered_map<std::string, int> ult;
	for (const std::string& id : ultimo) ult[id]++;

	std::unordered_map<std::string, int> cnt;
	std::vector<std::string>& main = m.principal;
	std::unordered_map<std::string, int> nucleo;
	auto lim = [&](const std::string& id) {
		const Carta* c = b.carta(id);
		return c ? std::min(3, c->limite) : 0;
	};
	auto esMon = [&](const std::string& id) {
		const Carta* c = b.carta(id);
		return c && c->monstruo;
	};
	auto grande = [&](const std::string& id) {
		const Carta* c = b.carta(id);
		return c && c->monstruo && c->clase != "ritual" && c->nivel >= 5;
	};
	int nMon = 0, nTrib = 0;
	auto add = [&](const std::string& id) {
		const Carta* c = b.carta(id);
		if (!c || c->clase == "fusion" || cnt[id] >= lim(id) || main.size() >= 42) return false;
		if (grande(id) && nTrib >= tribTope + 1) return false;
		cnt[id]++;
		main.push_back(id);
		nMon += c->monstruo;
		nTrib += grande(id);
		return true;
	};
	const int tamano = 40 + (a.uno() < 0.2f ? 1 + a.hasta(2) : 0);
	// 1) nucleo: lo que esta en todos sus mazos y sus cartas unicas de su mazo de siempre (la identidad)
	std::vector<std::pair<float, std::string>> nuc;
	for (auto& kv : freq) {
		const bool todos = nv >= 2 && kv.second == nv;
		const bool unica = enV0.count(kv.first) && ip.rivalesCon.count(kv.first) && ip.rivalesCon.at(kv.first) == 1;
		if (todos || unica) nuc.push_back({ a.uno() + (unica ? 1.0f : 0.0f), kv.first });
	}
	std::sort(nuc.begin(), nuc.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
	int nNuc = 0;
	for (auto& n : nuc) {
		if (nNuc >= 14) break;
		const int copias = std::min(2, std::max(1, minCop[n.second]));
		for (int k = 0; k < copias && nNuc < 14; k++)
			if (add(n.second)) nNuc++, nucleo[n.second]++;
	}
	// 2) paquete del plan: del pool (pesa mas) o de los mazos de rivales de su nivel o menos
	if (plan >= 0) {
		const int topeNivel = r.nivel + (afinado > 1.5f ? 1 : 0);
		std::vector<std::pair<float, std::string>> paq;
		for (auto& kv : ip.nivelMin) {
			const bool enPool = freq.count(kv.first) > 0;
			if (!enPool && kv.second > topeNivel) continue;
			if (!enPool && (int)ip.rivalesCon.at(kv.first) == 1) continue;   // (las cartas unicas de otro: no)
			const int s = puntajePapel(&b, kv.first, plan);
			if (s <= 0) continue;
			const Carta* c = b.carta(kv.first);
			if (!c || (c->monstruo && c->clase == "ritual")) continue;   // (rituales: solo con su magia, abajo)
			float w = (float)s * (enPool ? 3.0f : 1.0f) * (ult.count(kv.first) ? 0.5f : 1.0f) * (0.35f + a.uno());
			w *= 1.0f + 0.1f * afinado * std::min(3.0f, (float)ip.rivalesCon.at(kv.first) / 4.0f);   // (afinado: lo que usan mas rivales)
			paq.push_back({ w, kv.first });
		}
		std::sort(paq.begin(), paq.end(), [](const auto& x, const auto& y) { return x.first > y.first || (x.first == y.first && x.second < y.second); });
		const int K = 7 + a.hasta(4) + (int)std::max(0.0f, afinado);
		int puestos = 0;
		for (auto& pq : paq) {
			if (puestos >= K) break;
			if (add(pq.second)) {
				puestos++;
				if (freq.count(pq.second) && maxCop[pq.second] >= 2 && a.uno() < 0.4f && add(pq.second)) puestos++;
			}
		}
		// combos del plan: entran juntos si el pool o el nivel los permite
		for (const Combo& c : b.combos) {
			if (planPorClave(c.plan) != plan || c.torneo) continue;
			int lit = 0, hay = 0;
			for (const std::string& pz : c.piezas) {
				if (pz[0] == '@' || pz[0] == '%' || pz[0] == '&') continue;
				lit++;
				auto it = ip.nivelMin.find(pz);
				hay += freq.count(pz) || (it != ip.nivelMin.end() && it->second <= topeNivel && ip.rivalesCon.at(pz) > 1);
			}
			if (lit == 0 || hay * 2 < lit || a.uno() < 0.3f) continue;
			for (const std::string& pz : c.piezas) {
				if (pz[0] == '@' || pz[0] == '%' || pz[0] == '&') continue;
				auto it = ip.nivelMin.find(pz);
				if (freq.count(pz) || (it != ip.nivelMin.end() && it->second <= topeNivel && ip.rivalesCon.at(pz) > 1)) add(pz);
			}
		}
	}
	// 3) monstruos hasta el objetivo del plan, luego magias/trampas, del pool (frecuencia, papel, distinto al ultimo)
	const int objMon = std::max(14, std::min(26, (plan >= 0 ? PLANES[plan].monstruos : 19) + a.hasta(5) - 2));
	auto llenar = [&](bool monstruos, int hasta) {
		std::vector<std::pair<float, std::string>> c;
		for (auto& kv : maxCop) {
			const Carta* cc = b.carta(kv.first);
			if (!cc || cc->monstruo != monstruos || (cc->monstruo && cc->clase == "ritual")) continue;
			float w = (float)freq[kv.first] * (r.personaje && enV0.count(kv.first) ? 1.5f : 1.0f) * (1.0f + 0.6f * (float)puntajePapel(&b, kv.first, plan));
			w *= (ult.count(kv.first) ? 0.55f : 1.0f) * (0.3f + a.uno());
			c.push_back({ w, kv.first });
		}
		std::sort(c.begin(), c.end(), [](const auto& x, const auto& y) { return x.first > y.first || (x.first == y.first && x.second < y.second); });
		for (int vuelta = 0; vuelta < 3; vuelta++)
			for (auto& x : c) {
				const int ya = monstruos ? nMon : (int)main.size() - nMon;
				if (ya >= hasta || (int)main.size() >= tamano) return;
				if (cnt[x.second] >= maxCop[x.second]) continue;
				if (vuelta == 0 || a.uno() < 0.5f) add(x.second);
			}
	};
	llenar(true, objMon);
	// rituales del pool: su monstruo y su magia juntos (el plan combo los quiere mas)
	for (auto& kv : maxCop) {
		auto it = b.rituales.find(kv.first);
		if (it == b.rituales.end()) continue;
		const float pr = plan == PL_COMBO ? 0.9f : 0.5f;
		if (a.uno() < pr) {
			add(kv.first);
			add(it->second);
		}
	}
	llenar(false, tamano - objMon);
	llenar(true, 60);   // (si faltan magias en el pool)
	// 4) arreglos (en este orden, para que nada deshaga lo anterior)
	auto quitar = [&](const std::string& id) {
		auto it = std::find(main.begin(), main.end(), id);
		if (it == main.end()) return;
		main.erase(it);
		cnt[id]--;
		nMon -= esMon(id);
		nTrib -= grande(id);
	};
	// relleno seguro: monstruos chicos y magias/trampas del pool que no piden pareja (ni rituales, ni Toons, ni Polymerization)
	auto seguro = [&](const std::string& id) {
		const Carta* c = b.carta(id);
		return c && c->clase != "ritual" && c->subtipo != "toon" && c->subtipo != "magia_ritual" && id != "polymerization" && id != "toon_world" &&
		       !(c->monstruo && c->nivel > 4);
	};
	auto rellenar = [&]() {
		std::vector<std::string> s;
		for (auto& kv : maxCop)
			if (seguro(kv.first)) s.push_back(kv.first);
		std::sort(s.begin(), s.end());
		for (int vuelta = 0; vuelta < 400 && (int)main.size() < 40 && !s.empty(); vuelta++) add(s[(size_t)a.hasta((int)s.size())]);
	};
	// 4a) techo del ATK medio de sus mazos: la dificultad por nivel no sube
	for (int vuelta = 0; vuelta < 40 && atkMedio(b, main) > atkTope + 60.0f + 70.0f * afinado; vuelta++) {
		std::string peor;
		int pa = -1;
		for (const std::string& id : main) {
			const Carta* c = b.carta(id);
			if (c && c->monstruo && c->nivel <= 4 && !nucleo.count(id) && c->atk > pa) pa = c->atk, peor = id;
		}
		if (peor.empty()) break;
		quitar(peor);
		std::string mejor;
		int ma = 99999;
		for (auto& kv : maxCop) {
			const Carta* c = b.carta(kv.first);
			if (c && c->monstruo && seguro(kv.first) && c->atk < pa && cnt[kv.first] < lim(kv.first) && c->atk < ma) {
				if (a.uno() < 0.5f || mejor.empty()) ma = c->atk, mejor = kv.first;
			}
		}
		if (mejor.empty() || !add(mejor)) break;
	}
	rellenar();
	// 4b) rituales con su magia (o fuera) y magia ritual sin monstruo: fuera
	for (auto& kv : b.rituales) {
		if (cnt[kv.first] > 0 && cnt[kv.second] == 0 && !add(kv.second))
			while (cnt[kv.first] > 0) quitar(kv.first);
	}
	for (auto& kv : b.rituales) {
		bool alguno = false;
		for (auto& kv2 : b.rituales) alguno |= kv2.second == kv.second && cnt[kv2.first] > 0;
		if (!alguno)
			while (cnt[kv.second] > 0) quitar(kv.second);
	}
	// 4c) Toons con Toon World (o fuera)
	bool toons = false;
	for (auto& kv : cnt)
		if (kv.second > 0)
			if (const Carta* c = b.carta(kv.first)) toons |= c->subtipo == "toon" && c->monstruo;
	if (toons && cnt["toon_world"] == 0 && !add("toon_world"))
		for (auto it = cnt.begin(); it != cnt.end(); ++it) {
			const Carta* c = b.carta(it->first);
			if (c && c->subtipo == "toon" && c->monstruo)
				while (it->second > 0) quitar(it->first);
		}
	rellenar();
	// 4d) fusiones: las de sus mazos y cualquier receta cuyos materiales esten todos en el principal; con Polymerization
	std::vector<std::string> cand;
	for (const auto& v : r.fusion) cand.insert(cand.end(), v.begin(), v.end());
	for (auto& kv : b.fusiones) cand.push_back(kv.first);
	std::sort(cand.begin(), cand.end());
	cand.erase(std::unique(cand.begin(), cand.end()), cand.end());
	auto materiales = [&]() {
		for (const std::string& f : cand) {
			if (m.fusion.size() >= 15) break;
			auto it = b.fusiones.find(f);
			if (it == b.fusiones.end() || lim(f) <= 0 || std::find(m.fusion.begin(), m.fusion.end(), f) != m.fusion.end()) continue;
			bool ok = true;
			for (const std::string& mat : it->second) ok &= cnt[mat] > 0;
			bool deSusMazos = false;
			for (const auto& v : r.fusion) deSusMazos |= std::find(v.begin(), v.end(), f) != v.end();
			const Carta* cf = b.carta(f);
			if (ok && (deSusMazos || (cf && (float)cf->atk <= atkTope + 1200.0f))) m.fusion.push_back(f);
		}
	};
	materiales();
	auto esMaterial = [&](const std::string& id) {
		for (const std::string& f : m.fusion)
			for (const std::string& mat : b.fusiones.at(f))
				if (mat == id) return true;
		return false;
	};
	if (!m.fusion.empty() && cnt["polymerization"] == 0 && freq.count("polymerization")) {
		if (!add("polymerization")) {   // (mazo lleno: sale una carta que no es material ni nucleo)
			for (int k = (int)main.size() - 1; k >= 0; k--) {
				const std::string id = main[(size_t)k];
				if (!esMaterial(id) && !nucleo.count(id) && seguro(id)) {
					quitar(id);
					break;
				}
			}
			add("polymerization");
		}
	}
	if (cnt["polymerization"] == 0) m.fusion.clear();
	if (m.fusion.empty())
		while (cnt["polymerization"] > 0) quitar("polymerization");
	rellenar();
	// 4e) a 40 como minimo (no deberia faltar: si no, su mazo de siempre)
	if ((int)main.size() < 40) {
		m.principal = r.principal[0];
		m.fusion = r.fusion.empty() ? std::vector<std::string>{} : r.fusion[0];
		cnt.clear();
		for (const std::string& id : main) cnt[id]++;
	}
	std::sort(main.begin(), main.end());
	// la variante mas parecida (para su nombre en la pantalla de elegir rival)
	int mejorV = 0, mejorS = -1;
	for (int v = 0; v < nv; v++) {
		std::unordered_map<std::string, int> c2 = cnt;
		int s = 0;
		for (const std::string& id : r.principal[(size_t)v])
			if (c2[id] > 0) c2[id]--, s++;
		if (s > mejorS) mejorS = s, mejorV = v;
	}
	m.variante = mejorV;
	return m;
}

// todo junto: el plan del duelo (pesos + memoria + lectura + dificultad) y su mazo dinamico. Sin papeles cargados
// (roles_cartas.txt) vuelve al mazo de siempre (Base::mazoDe) sin plan
// rama classic-mazos-torneo: probabilidad de que el rival saque uno de sus mazos de torneo (si tiene). Genericos de
// nivel 7-8: 40 %; personajes: 30 % (40 % desde el nivel 7). Nunca el mismo mazo de torneo dos duelos seguidos
inline float probTorneo(const Rival& r)
{
	if (r.torneo.empty()) return 0.0f;
	return r.personaje ? (r.nivel >= 7 ? 0.40f : 0.30f) : 0.40f;
}
inline bool mismoMazo(std::vector<std::string> a, std::vector<std::string> b)
{
	std::sort(a.begin(), a.end());
	std::sort(b.begin(), b.end());
	return a == b;
}
// el mazo de torneo de este duelo (indice en b.torneos) o -1
inline int elegirTorneo(const Base& b, const Rival& r, uint32_t semilla, const std::vector<std::string>& ultimo)
{
	if (r.torneo.empty()) return -1;
	Azar a(mezclar(semilla, 0x70E0u));
	a.siguiente();
	if (a.uno() >= probTorneo(r)) return -1;
	std::vector<int> cand;
	for (int k : r.torneo)
		if (k >= 0 && k < (int)b.torneos.size() && b.torneos[(size_t)k].principal.size() >= 40 && !mismoMazo(b.torneos[(size_t)k].principal, ultimo))
			cand.push_back(k);
	return cand.empty() ? -1 : cand[(size_t)a.hasta((int)cand.size())];
}
// el mazo de torneo tal cual (sin reequilibrar ni diluir); el plan: casi siempre el suyo, a veces otro que su mazo
// permita (el rival sigue siendo impredecible y puede cambiar de plan a mitad de duelo)
inline MazoRival mazoTorneo(const Base& b, const Rival& r, int k, uint32_t semilla, const std::vector<int>& recientes, const Lectura& lec, float dif)
{
	const MazoTorneo& t = b.torneos[(size_t)k];
	MazoRival m;
	m.rival = &r;
	m.torneo = k;
	m.principal = t.principal;
	m.fusion = t.fusion;
	m.variante = 0;
	if (b.papeles.empty()) return m;
	Azar a(mezclar(semilla, 0x70E1u));
	a.siguiente();
	auto w = pesosPlanes(b, r.clavePersonalidad, t.principal, recientes, lec, dif);
	const int suyo = planPorClave(t.plan);
	if (suyo >= 0) w[(size_t)suyo] = (w[(size_t)suyo] + 1.0f) * 6.0f;
	m.plan = sortearPlan(w, a);
	return m;
}

inline MazoRival prepararRival(const Base& b, const Rival& r, uint32_t semilla, const Memoria* mem)
{
	std::vector<int> recientes;
	std::vector<std::string> ultimo;
	Lectura lec;
	float dif = 0.0f;
	if (mem) {
		auto it = mem->rivales.find(r.clave);
		if (it != mem->rivales.end()) recientes = it->second.planes, ultimo = it->second.mazo;
		lec = mem->jugador;
		dif = mem->dificultad();
	}
	const int t = elegirTorneo(b, r, semilla, ultimo);   // (rama classic-mazos-torneo: tal cual)
	if (t >= 0) return mazoTorneo(b, r, t, semilla, recientes, lec, dif);
	if (b.papeles.empty()) return b.mazoDe(r, semilla);
	Azar a(mezclar(semilla, 0x91A4u));
	a.siguiente();
	const int plan = sortearPlan(pesosPlanes(b, r.clavePersonalidad, poolDe(r), recientes, lec, dif), a);
	return mazoDinamico(b, r, plan, semilla, ultimo, dif);
}

// ------------------------------------------------------------------------------------------------- en el duelo
// el marcador del lado: LP y ventaja de cartas (lo publico + lo propio)
inline float marcador(const Duelo& d, int yo)
{
	auto cartas = [&](int l) {
		const Lado& x = d.l[(size_t)l];
		int n = (int)x.mano.size() + x.monstruos();
		for (const MagiaTrampa& c : x.mt) n += !c.vacia();
		return n;
	};
	return (float)(d.l[(size_t)yo].lp - d.l[(size_t)(1 - yo)].lp) / 1000.0f + 0.6f * (float)(cartas(yo) - cartas(1 - yo));
}
// las cartas que el lado sabe que tiene (su mazo es suyo: no es informacion oculta del rival)
inline std::vector<std::string> cartasPropias(const Duelo& d, int yo)
{
	const Lado& x = d.l[(size_t)yo];
	std::vector<std::string> c = x.mazo;
	c.insert(c.end(), x.mano.begin(), x.mano.end());
	return c;
}
// al empezar cada turno propio: si el plan va perdiendo, cambia (como haria una persona). true si cambio
inline bool revisarPlan(const Duelo& d, int yo, EstadoPlan& ep, Azar& a)
{
	if (!ep.activo() || !d.base) return false;
	ep.turnosPlan++;
	const float mk = marcador(d, yo);
	if (ep.turnosPlan == 1) ep.marcadorInicio = mk;
	const int otro = 1 - yo;
	int nuevo = -1;
	// rematar: el otro esta muy bajo y el plan no es de pegar
	if (d.l[(size_t)otro].lp <= 1500 && ep.plan != PL_AGRESIVO && ep.plan != PL_ENJAMBRE && ep.cambios < 3) nuevo = PL_AGRESIVO;
	// el plan no funciona: pierde por bastante y empeoro desde que lo empezo
	else if (ep.turnosPlan >= 2 && ep.cambios < 2 && mk < -2.5f && mk < ep.marcadorInicio - 1.5f) {
		auto w = pesosPlanes(*d.base, ep.base.clave, cartasPropias(d, yo), ep.planesDuelo, ep.lectura, ep.dificultad);
		nuevo = sortearPlan(w, a, ep.plan);
	}
	if (nuevo < 0 || nuevo == ep.plan) return false;
	ep.plan = nuevo;
	ep.cambios++;
	ep.turnosPlan = 0;
	ep.planesDuelo.push_back(nuevo);
	ep.estilo = Estilo::alAzar(a, nuevo);
	return true;
}

// proyecto a 2 turnos: lo que la mano y el campo permiten preparar para el turno siguiente
struct Proyecto {
	int faltanTributos{ 0 };          // para el grande de la mano: monstruos que faltan en el campo (1 = el que coloque hoy)
	std::string grande;
	std::vector<std::string> materiales;   // de la fusion o el ritual que se esta juntando (no gastar ni descartar)
	bool fusionCasi{ false }, ritualCasi{ false };
};
inline Proyecto proyecto(const Duelo& d, int yo)
{
	Proyecto p;
	const Lado& x = d.l[(size_t)yo];
	const int propios = x.monstruos();
	int mejorAtk = -1;
	for (const std::string& id : x.mano) {
		const Carta* c = d.carta(id);
		if (!c || !(c->clase == "normal" || c->clase == "efecto") || c->noInvocable) continue;
		const int nt = Duelo::tributosPara(c);
		if (nt > propios && nt - propios <= 1 && c->atk > mejorAtk) {
			mejorAtk = c->atk;
			p.grande = id;
			p.faltanTributos = nt - propios;
		}
	}
	if (!d.base) return p;
	const bool poly = std::find(x.mano.begin(), x.mano.end(), "polymerization") != x.mano.end();
	if (poly)
		for (const std::string& f : x.extra) {
			auto it = d.base->fusiones.find(f);
			if (it == d.base->fusiones.end()) continue;
			int tiene = 0;
			for (const std::string& mat : it->second) {
				bool h = std::find(x.mano.begin(), x.mano.end(), mat) != x.mano.end();
				for (const Monstruo& mm : x.m) h |= mm.id == mat;
				tiene += h;
			}
			if (tiene >= 1 && tiene + 1 >= (int)it->second.size()) {
				p.fusionCasi = true;
				p.materiales.insert(p.materiales.end(), it->second.begin(), it->second.end());
				p.materiales.push_back("polymerization");
			}
		}
	for (const std::string& id : x.mano) {
		auto it = d.base->rituales.find(id);
		if (it != d.base->rituales.end() && std::find(x.mano.begin(), x.mano.end(), it->second) != x.mano.end()) {
			p.ritualCasi = true;
			p.materiales.push_back(id);
			p.materiales.push_back(it->second);
		}
	}
	return p;
}

// lo que vale NO gastar esta carta ahora (descartar, pagarla como coste): piezas del proyecto, del plan y de combos
// rama classic-mazos-torneo: Exodia. Una pieza (y cuantas piezas propias quedan vivas: mano + mazo; las del
// cementerio o desterradas ya no cuentan). Con 4 o mas vivas el lado juega a juntarlas: no las invoca ni las descarta
inline bool piezaExodia(const std::string& id) { return id.find("forbidden_one") != std::string::npos; }
inline int piezasExodiaVivas(const Duelo& d, int yo)
{
	const Lado& x = d.l[(size_t)yo];
	std::vector<std::string> p;
	for (const auto* v : { &x.mano, &x.mazo })
		for (const std::string& id : *v)
			if (piezaExodia(id) && std::find(p.begin(), p.end(), id) == p.end()) p.push_back(id);
	return (int)p.size();
}

inline float valorGuardar(const Duelo& d, int yo, const std::string& id, const EstadoPlan& ep)
{
	if (!ep.activo()) return 0.0f;
	float v = 60.0f * (float)puntajePapel(d.base, id, ep.plan);
	if (piezaExodia(id) && piezasExodiaVivas(d, yo) >= 4) v += 3000.0f;   // (rama classic-mazos-torneo)
	const Proyecto p = proyecto(d, yo);
	if (std::find(p.materiales.begin(), p.materiales.end(), id) != p.materiales.end()) v += 400.0f;
	if (id == p.grande) v += 300.0f;
	return v;
}

// piezas de combo disponibles: mano + campo propio boca arriba
inline int piezasCombo(const Duelo& d, int yo, const Combo& c)
{
	const Lado& x = d.l[(size_t)yo];
	int n = 0;
	for (const std::string& pz : c.piezas) {
		bool h = false;
		for (const std::string& id : x.mano) h |= piezaEs(d.base, pz, id);
		for (const Monstruo& mm : x.m) h |= !mm.vacia() && !mm.bocaAbajo && piezaEs(d.base, pz, mm.id);
		for (const MagiaTrampa& mt : x.mt) h |= !mt.vacia() && piezaEs(d.base, pz, mt.id);
		n += h;
	}
	return n;
}

// el bono del plan a cada jugada candidata (se suma antes del ruido)
inline void ajustarPlan(const Duelo& d, int yo, std::vector<Jugada>& c, const EstadoPlan& ep)
{
	if (!ep.activo() || !d.base) return;
	const DefPlan& P = PLANES[ep.plan];
	const Lado& x = d.l[(size_t)yo];
	const int otro = 1 - yo;
	const Lado& y = d.l[(size_t)otro];
	const Proyecto pr = proyecto(d, yo);
	const float pac = std::max(0.0f, std::min(1.0f, ep.base.paciencia + P.dPac));
	const Lectura& lec = ep.lectura;
	const bool m1 = d.fase == F_PRINCIPAL1;
	int trampasPuestas = 0;
	for (const MagiaTrampa& mt : x.mt) trampasPuestas += !mt.vacia() && mt.bocaAbajo && esTrampa(d, mt.id);
	for (Jugada& j : c) {
		float b = ep.humor[(size_t)j.tipo % ep.humor.size()];
		const std::string& id = j.carta;
		const bool conCarta = !id.empty() && j.tipo != J_ATACAR && j.tipo != J_POSICION && j.tipo != J_FASE;
		// papeles del plan (colocar una magia normal no: se activa)
		if (conCarta && !(j.tipo == J_COLOCAR_MT && !esTrampa(d, id) && subtipoDe(d, id) != "magia_rapida")) b += 85.0f * (float)puntajePapel(d.base, id, ep.plan);
		// combos conocidos: jugar una pieza cuando las demas estan a mano
		if (conCarta)
			for (const Combo& cb : d.base->combos) {
				int idx = -1;
				for (size_t k = 0; k < cb.piezas.size() && idx < 0; k++)
					if (piezaEs(d.base, cb.piezas[k], id)) idx = (int)k;
				if (idx < 0) continue;
				if (piezasCombo(d, yo, cb) >= cb.minimo) b += 180.0f + (planPorClave(cb.plan) == ep.plan ? 170.0f : 0.0f) + (idx == 0 ? 60.0f : 0.0f);
			}
		switch (j.tipo) {
		case J_ATACAR: {
			b += P.bonoAtaque;
			const Monstruo& a = x.m[(size_t)std::max(0, std::min(ZONAS - 1, j.zona))];
			// lectura del jugador: con sus cartas boca abajo, el trampero da miedo (no arriesga a sus monstruos buenos:
			// limpia antes o ataca con lo que puede perder) y al farolero ya se le conoce (ataca sin miedo)
			if (y.colocadasMT() > 0) {
				const float k = 0.4f + 0.6f * lec.confianza;
				const float valor = 450.0f + 0.5f * (float)d.atkM(a);
				b += k * (-2.2f * std::max(0.0f, lec.trampas - 0.35f) * valor - 150.0f * (lec.trampas - 0.35f) + 320.0f * (lec.faroles - 0.30f));
			}
			if (ep.plan == PL_TEMPO && a.duenno >= 0 && a.duenno != yo) b += 200.0f;   // el prestado ataca seguro
			if ((ep.plan == PL_ALTERNATIVA || ep.plan == PL_BLOQUEO) && j.puntos < 200.0f) b -= 150.0f;   // solo lo seguro
			break;
		}
		case J_INVOCAR:
		case J_COLOCAR: {
			const Carta* cc = d.carta(id);
			// (rama classic-mazos-torneo) Exodia: las piezas se juntan en la mano, no se juegan al campo
			if (piezaExodia(id) && piezasExodiaVivas(d, yo) >= 4) b -= 2500.0f;
			// tempo: sacrificar el monstruo prestado (Change of Heart + sacrificio)
			if (ep.plan == PL_TEMPO)
				for (int z : j.tributos)
					if (z >= 0 && z < ZONAS && x.m[(size_t)z].duenno >= 0 && x.m[(size_t)z].duenno != yo) b += 300.0f;
			if (ep.plan == PL_ENJAMBRE) b += j.tributos.empty() ? 80.0f : -100.0f * (float)j.tributos.size();
			if (j.tipo == J_COLOCAR && (ep.plan == PL_BLOQUEO || ep.plan == PL_ALTERNATIVA || ep.plan == PL_MOLINO)) b += 120.0f;
			// horizonte de 2 turnos: un monstruo chico hoy es el sacrificio del grande manana (mejor boca abajo)
			if (pr.faltanTributos == 1 && cc && cc->nivel <= 4 && j.tributos.empty() && id != pr.grande) b += (j.tipo == J_COLOCAR ? 220.0f : 120.0f) * (0.4f + pac);
			// juntar materiales de la fusion en el campo
			if (pr.fusionCasi && std::find(pr.materiales.begin(), pr.materiales.end(), id) != pr.materiales.end()) b += 150.0f;
			// trampa al reves: con trampas puestas, el campo vacio invita a atacar
			if (ep.estilo.cebo && trampasPuestas >= 2 && x.monstruos() == 0 && j.tipo == J_INVOCAR) b -= 250.0f;
			// no sobreextenderse contra muchas cartas boca abajo si el plan es de esperar
			if ((ep.plan == PL_CONTROL || ep.plan == PL_CASTIGO) && x.monstruos() >= 2 && y.colocadasMT() >= 2) b -= 150.0f * pac;
			b += 50.0f * ep.estilo.ordenInvocar;
			break;
		}
		case J_COLOCAR_MT:
			if (ep.plan == PL_CASTIGO && esTrampa(d, id)) b += 150.0f;
			if (ep.estilo.mtAntes && m1) b += 90.0f;   // (en la principal 1 la base resta 40)
			break;
		case J_FUSION:
		case J_RITUAL:
			if (ep.plan == PL_COMBO) b += 200.0f;
			break;
		case J_ACTIVAR: {
			b -= 50.0f * ep.estilo.ordenInvocar;
			// (rama classic-mazos-torneo) con el mazo casi vacio no se roba de mas (los de robo: Exodia, Biblioteca)
			if (papel(d.base, id, 'o') && x.mazo.size() <= 6) b -= 300.0f * (float)(7 - (int)x.mazo.size());
			// jugador trampero: limpiar sus boca abajo antes de atacar (MST, Plumero, Tormenta)
			if (papel(d.base, id, 't') && y.colocadasMT() > 0 && m1) b += 320.0f * std::max(0.0f, lec.trampas - 0.2f) * (0.5f + lec.confianza);
			// esperar: no gastar el quitamonstruos en un monstruo chico si el plan pide paciencia
			if (papel(d.base, id, 'd') && (ep.plan == PL_BLOQUEO || ep.plan == PL_CONTROL || ep.plan == PL_ALTERNATIVA)) {
				int amen = 0;
				for (const Monstruo& mm : y.m)
					if (!mm.vacia()) amen = std::max(amen, mm.bocaAbajo && !mm.visto ? 1200 : d.atkM(mm));
				if (amen < 1600) b -= 180.0f * pac;
			}
			break;
		}
		case J_FASE:
			// del principal 1 a la batalla: los planes de esperar no tienen apuro
			break;
		default: break;
		}
		j.puntos += b;
	}
}

// la ventana de respuesta: cuanto sube el umbral para activar esta carta ahora (retener la trampa clave)
inline float ajusteRespuesta(const Duelo& d, int yo, const Lugar& f, const EstadoPlan& ep)
{
	if (!ep.activo()) return 0.0f;
	const std::string id = cartaEn(d, f);
	float u = 0.0f;
	if (ep.plan == PL_CASTIGO) u -= 100.0f;   // castiga a gusto
	// (rama classic-mazos-torneo) con el mazo casi vacio no se roba de mas (Jar of Greed, Good Goblin Housekeeping...)
	const size_t quedan = d.l[(size_t)yo].mazo.size();
	if (papel(d.base, id, 'o') && quedan <= 6) u += 300.0f * (float)(7 - (int)quedan);
	const bool clave = papel(d.base, id, 'c') || papel(d.base, id, 'n');
	if (clave && d.ef.ataque.hay && d.ef.ataque.lado != yo) {
		const Monstruo& a = d.l[(size_t)d.ef.ataque.lado].m[(size_t)std::max(0, std::min(ZONAS - 1, d.ef.ataque.zona))];
		int atacantes = 0;
		for (const Monstruo& mm : d.l[(size_t)d.ef.ataque.lado].m) atacantes += !mm.vacia() && !mm.bocaAbajo && !mm.defensa;
		const float pac = std::max(0.0f, std::min(1.0f, ep.base.paciencia + PLANES[ep.plan].dPac));
		// un ataque chico y solo, con LP de sobra: guardarla para el que importa
		if (d.atkM(a) < 1400 && atacantes <= 1 && d.l[(size_t)yo].lp > 3000) u += 250.0f * pac;
	}
	return u;
}
