/*
 * Yugioh Classic Remaster - IA HUMANA (rama classic-ia-humana). Se incluye DENTRO de namespace ia desde ia.h, entre
 * el Cerebro y el Turno (usa el Cerebro como politica; el Turno la usa a ella).
 *
 * El objetivo: un rival que piense como una persona y no sea predecible. Dos capas:
 *
 *  1. BUSQUEDA QUE IMAGINA JUGADAS (Monte Carlo con determinizacion). En cada decision importante (que invocar o
 *     colocar, si atacar y a quien, si activar o responder) la IA:
 *       - toma las mejores candidatas de la politica de siempre (Cerebro::candidatos / candidatosRespuesta, con el
 *         plan del duelo ya sumado) y descarta las obvias (una sola opcion buena, un letal, pasar sin nada que hacer);
 *       - copia el Duelo y ADIVINA lo oculto del jugador (determinizar): su mano, el orden de su mazo y lo que tiene
 *         boca abajo salen de su lista de cartas menos lo que ya se vio (cementerio, campo boca arriba, desterradas).
 *         Lo boca abajo se sortea con un sesgo segun lo que la IA sabe de el (Lectura, ia_plan.h): si activa de
 *         verdad lo que coloca (trampero) es mas probable que sea trampa; si coloca y no activa (farolero) menos; y
 *         cada ataque propio sin castigo baja la sospecha. Su propio mazo tambien se baraja (no sabe que robara);
 *       - juega cada candidata en la copia y sigue 1-2 turnos con la politica de ia.h (rollout: el Cerebro propio con
 *         un poco mas de vista y un MODELO del jugador hecho con su lectura: ataca mas si siempre ataca...);
 *       - evalua el final con el PLAN del duelo (evaluar: agresivo valora el danno; control y bloqueo la ventaja de
 *         cartas; Kaiba, ademas, tener el monstruo mas fuerte) y elige la mejor en promedio. Rondas con los mismos
 *         numeros al azar para todas (comparacion justa) y las que quedan claramente atras salen (successive halving).
 *     Presupuesto por decision: 30-150 ms (crece con el nivel del rival) y 16-170 simulaciones (nivel, dificultad
 *     adaptable y "vista" del personaje). Nunca traba el juego: el Turno la corre POR PEDAZOS por cuadro (rebanada
 *     de ~4 ms; mientras tanto se ve "El rival esta pensando...") y la prueba la corre de una vez.
 *
 *  2. CAPA HUMANA:
 *       - FAROLES: coloca boca abajo magias que no son trampas para frenar ataques (mas si el jugador ataca con todo
 *         o si suele limpiar las boca abajo: gasta su MST en el farol) y a veces calla una trampa marginal para no
 *         mostrar lo que tiene (con LP de sobra). La busqueda los valora: el modelo del jugador tambien le tiene
 *         miedo a lo boca abajo.
 *       - LECTURA DEL JUGADOR: si ataca con todo, guarda las trampas para el ataque grande y coloca mas trampas; si
 *         coloca y no activa, sospecha farol (determinizacion); si limpia, farolea mas (Lectura::limpia, nueva).
 *       - ERRORES CREIBLES (no ruido al azar): mas en nivel bajo, con la dificultad adaptable y en posiciones con
 *         muchas opciones: no ver un combo raro (fusion, ritual, efecto de monstruo o del cementerio), sobrevalorar
 *         el ataque cuando va ganando, arriesgar cuando va perdiendo, o quedarse con la segunda si estaba cerca.
 *       - TIEMPO DE PENSAR VISIBLE: rapido en lo obvio, una pausa mas larga en lo dificil; lo que ya tardo la
 *         busqueda se descuenta. Corre con el dt del juego (ya escalado por la velocidad de animaciones).
 *       - PERSONALIDAD (Rasgos): cada personaje del anime con lo suyo (Joey arriesga y se equivoca mas, Kaiba es
 *         agresivo y orgulloso, Pegasus e Ishizu leen al jugador, Bakura y Weevil farolean...), sumado al plan.
 *
 * Opciones: "IA: Clasica / Humana" (Memoria::humana en variedad.txt; por defecto Humana).
 *
 *  3. (rama classic-ia-red) LA RED QUE APRENDE (ia_red.h, aprendizaje.h): si hay red ("red", la pone el juego con
 *     "IA que aprende: Si"), el final de cada simulacion se evalua con mezclarRed: el plan del duelo (la personalidad)
 *     mezclado con la probabilidad de ganar que da la red (autojuego + ajuste fino a los duelos de ESTE jugador). Sin
 *     red (opcion en No) todo es como antes.
 */

// ------------------------------------------------------------------------------------------------- personalidad
struct Rasgos {
	const char* clave;
	float riesgo;    // 0..1: busca la varianza (arriesga) al elegir y cuando va perdiendo
	float orgullo;   // 0..1: valora tener el monstruo mas fuerte y no esconderlo (no lo coloca, no lo pone en defensa)
	float lectura;   // x: cuanto lee al jugador
	float farol;     // 0..1: faroles (colocar lo que no es trampa, callar una trampa)
	float error;     // x: errores creibles
	float vista;     // x: simulaciones y tiempo de busqueda
	float pausa;     // x: tiempo de pensar visible
};
static const Rasgos RASGOS_PERSONAJES[] = {
	//  clave              riesgo orgullo lectura farol  error  vista  pausa
	{ "joey",              0.85f, 0.20f, 0.80f, 0.30f, 1.40f, 0.85f, 0.75f },   // arriesga (y a veces le sale)
	{ "kaiba",             0.45f, 1.00f, 0.90f, 0.10f, 0.85f, 1.10f, 0.70f },   // agresivo y orgulloso: no se esconde
	{ "pegasus",           0.30f, 0.30f, 1.80f, 0.55f, 0.65f, 1.20f, 1.10f },   // lee al jugador (el Ojo del Milenio)
	{ "yami_bakura",       0.50f, 0.30f, 1.30f, 0.90f, 0.80f, 1.10f, 1.15f },   // tramposo
	{ "ryou_bakura",       0.30f, 0.00f, 1.00f, 0.60f, 1.10f, 0.90f, 1.10f },
	{ "yugi",              0.55f, 0.30f, 1.30f, 0.35f, 0.60f, 1.25f, 1.00f },   // el corazon de las cartas: ve lejos
	{ "marik",             0.40f, 0.50f, 1.20f, 0.60f, 0.80f, 1.10f, 1.10f },
	{ "yami_marik",        0.65f, 0.60f, 1.20f, 0.55f, 0.80f, 1.10f, 0.95f },   // cruel
	{ "ishizu",            0.25f, 0.20f, 1.70f, 0.40f, 0.70f, 1.20f, 1.25f },   // "ve el futuro"
	{ "mai",               0.55f, 0.50f, 1.00f, 0.30f, 1.00f, 1.00f, 0.85f },
	{ "weevil",            0.35f, 0.20f, 1.10f, 0.95f, 1.10f, 1.00f, 1.00f },   // tramposo
	{ "bandit_keith",      0.55f, 0.30f, 1.00f, 0.90f, 1.10f, 1.00f, 0.90f },
	{ "rex_raptor",        0.80f, 0.40f, 0.70f, 0.10f, 1.30f, 0.85f, 0.75f },
	{ "tea",               0.15f, 0.00f, 1.00f, 0.20f, 1.35f, 0.90f, 1.20f },
	{ "mokuba",            0.40f, 0.30f, 0.80f, 0.30f, 1.50f, 0.80f, 0.90f },
	{ "dartz",             0.30f, 0.50f, 1.50f, 0.50f, 0.60f, 1.25f, 1.20f },
	{ "noah",              0.30f, 0.60f, 1.40f, 0.40f, 0.70f, 1.25f, 1.00f },
	{ "gozaburo",          0.50f, 0.80f, 1.00f, 0.30f, 0.80f, 1.10f, 0.90f },
	{ "valon",             0.85f, 0.40f, 0.80f, 0.20f, 1.30f, 0.90f, 0.75f },
	{ "alister",           0.55f, 0.50f, 1.00f, 0.30f, 1.00f, 1.00f, 0.85f },
	{ "rafael",            0.20f, 0.40f, 1.10f, 0.30f, 0.80f, 1.10f, 1.25f },
	{ "zigfried",          0.40f, 0.60f, 1.10f, 0.85f, 0.90f, 1.05f, 1.00f },
	{ "duke",              0.50f, 0.40f, 1.10f, 0.80f, 1.00f, 1.00f, 0.95f },
	{ "espa_roba",         0.45f, 0.20f, 1.20f, 0.75f, 1.10f, 0.95f, 1.00f },
	{ "solomon",           0.30f, 0.10f, 1.30f, 0.35f, 0.80f, 1.15f, 1.25f },
	{ "arkana",            0.55f, 0.40f, 1.00f, 0.70f, 1.00f, 1.00f, 1.05f },
	{ "lumis_umbra",       0.35f, 0.30f, 1.00f, 0.70f, 1.00f, 1.00f, 1.10f },
	{ "shadi",             0.25f, 0.10f, 1.60f, 0.40f, 0.80f, 1.10f, 1.30f },
	{ "tristan",           0.85f, 0.30f, 0.70f, 0.15f, 1.40f, 0.80f, 0.75f },
	{ "serenity",          0.15f, 0.00f, 0.90f, 0.15f, 1.30f, 0.90f, 1.20f },
	{ "rebecca",           0.35f, 0.40f, 1.30f, 0.30f, 0.75f, 1.20f, 0.95f },
	{ "cinco_grandes",     0.35f, 0.40f, 1.10f, 0.40f, 0.90f, 1.10f, 1.05f },
	{ "leon",              0.30f, 0.20f, 1.00f, 0.40f, 1.00f, 1.00f, 1.10f },
	{ "mana",              0.70f, 0.20f, 0.80f, 0.30f, 1.35f, 0.90f, 0.85f },
	{ "kisara",            0.20f, 0.10f, 1.00f, 0.20f, 1.10f, 1.00f, 1.15f },
	{ "bobasa",            0.80f, 0.20f, 0.80f, 0.40f, 1.30f, 0.85f, 0.80f },
	{ "bonz",              0.30f, 0.00f, 0.90f, 0.50f, 1.20f, 0.90f, 1.15f },
	{ "mako",              0.30f, 0.40f, 1.00f, 0.40f, 1.00f, 1.00f, 1.10f },
	{ "panik",             0.25f, 0.40f, 0.90f, 0.60f, 1.10f, 0.95f, 1.10f },
	{ "hermanos_paradoja", 0.25f, 0.30f, 1.20f, 0.60f, 0.90f, 1.05f, 1.10f },
};
// los rivales genericos: por su personalidad (mismo orden que PERSONALIDADES)
static const Rasgos RASGOS_PERSONALIDAD[] = {
	{ "equilibrado", 0.40f, 0.20f, 1.00f, 0.35f, 1.00f, 1.00f, 1.00f },
	{ "agresivo",    0.60f, 0.40f, 0.90f, 0.20f, 1.05f, 1.00f, 0.85f },
	{ "defensivo",   0.15f, 0.10f, 1.00f, 0.40f, 1.00f, 1.00f, 1.15f },
	{ "farolero",    0.45f, 0.10f, 1.00f, 0.90f, 1.00f, 1.00f, 1.00f },
	{ "paciente",    0.20f, 0.10f, 1.10f, 0.45f, 0.90f, 1.05f, 1.25f },
	{ "temerario",   0.90f, 0.30f, 0.70f, 0.20f, 1.30f, 0.85f, 0.70f },
	{ "calculador",  0.30f, 0.20f, 1.40f, 0.35f, 0.75f, 1.20f, 1.15f },
	{ "tramposo",    0.50f, 0.20f, 1.10f, 0.90f, 1.00f, 1.00f, 0.95f },
	{ "control",     0.25f, 0.30f, 1.20f, 0.60f, 0.85f, 1.10f, 1.15f },
	{ "oscuro",      0.55f, 0.50f, 1.20f, 0.60f, 0.90f, 1.05f, 1.05f },
};
inline Rasgos rasgosDe(const std::string& claveRival, const std::string& clavePersonalidad)
{
	for (const Rasgos& r : RASGOS_PERSONAJES)
		if (claveRival == r.clave) return r;
	for (const Rasgos& r : RASGOS_PERSONALIDAD)
		if (clavePersonalidad == r.clave) return r;
	return RASGOS_PERSONALIDAD[0];
}

// ------------------------------------------------------------------------------------------------- determinizar
// Adivina lo oculto del lado "otro" visto desde "yo" (en una COPIA del duelo): su mano, el orden de su mazo y lo que
// tiene boca abajo salen del conjunto de sus cartas no vistas (mano + mazo + boca abajo: lo que queda de su lista de
// cartas tras quitar lo que ya se vio), con lo boca abajo sesgado por la lectura. Su propio mazo se baraja
inline void determinizar(Duelo& s, int yo, Azar& a, const Lectura& lec, float lecturaK, int sinCastigo)
{
	const int otro = 1 - yo;
	Lado& y = s.l[(size_t)otro];
	// con una cadena o una eleccion a medias, la mano del otro puede estar referida por indice: se deja como esta
	const bool manoLibre = s.ef.cadena.empty() && s.ef.disparos.empty() && !s.ef.resolviendo && s.ef.preg.tipo != P_ELEGIR;
	std::vector<std::string> pool = y.mazo;
	if (manoLibre) pool.insert(pool.end(), y.mano.begin(), y.mano.end());
	std::vector<int> zMT, zM;
	for (int z = 0; z < ZONAS; z++) {
		const MagiaTrampa& c = y.mt[(size_t)z];
		if (!c.vacia() && c.bocaAbajo) zMT.push_back(z), pool.push_back(c.id);
		const Monstruo& m = y.m[(size_t)z];
		if (!m.vacia() && m.bocaAbajo && !m.visto && !m.token && (m.duenno < 0 || m.duenno == otro)) zM.push_back(z), pool.push_back(m.id);
	}
	barajar(pool, a);
	const float k = std::max(0.0f, std::min(1.5f, (0.3f + lec.confianza) * lecturaK));
	const float pesoTrampa = std::max(0.3f, std::min(3.0f, 1.0f + k * (2.2f * (lec.trampas - 0.35f) - 1.6f * (lec.faroles - 0.30f)))) *
	                         std::pow(0.85f, (float)std::min(sinCastigo, 6));
	auto sacar = [&](const std::function<float(const Carta*)>& peso) -> std::string {
		float t = 0.0f;
		for (const std::string& id : pool) t += peso(s.carta(id));
		if (t <= 0.0f) return {};
		float r = a.uno() * t;
		for (size_t i = 0; i < pool.size(); i++) {
			r -= peso(s.carta(pool[i]));
			if (r <= 0.0f) {
				std::string id = pool[i];
				pool.erase(pool.begin() + (long)i);
				return id;
			}
		}
		return {};
	};
	for (int z : zMT) {
		std::string id = sacar([&](const Carta* c) { return !c || c->monstruo ? 0.0f : c->trampa ? pesoTrampa : 1.0f; });
		if (id.empty()) {   // (no deberia pasar: la de verdad esta en el conjunto)
			id = y.mt[(size_t)z].id;
			auto it = std::find(pool.begin(), pool.end(), id);
			if (it != pool.end()) pool.erase(it);
		}
		y.mt[(size_t)z].id = id;
	}
	for (int z : zM) {
		std::string id = sacar([&](const Carta* c) {
			return !c || !(c->clase == "normal" || c->clase == "efecto") ? 0.0f : c->nivel <= 4 ? 1.0f : 0.25f;
		});
		if (id.empty()) {
			id = y.m[(size_t)z].id;
			auto it = std::find(pool.begin(), pool.end(), id);
			if (it != pool.end()) pool.erase(it);
		}
		y.m[(size_t)z].id = id;
	}
	if (manoLibre) {
		const size_t n = std::min(y.mano.size(), pool.size());
		y.mano.assign(pool.begin(), pool.begin() + (long)n);
		y.mazo.assign(pool.begin() + (long)n, pool.end());
	} else {
		y.mazo = pool;
	}
	barajar(s.l[(size_t)yo].mazo, a);
	s.recalcular();
}

// ------------------------------------------------------------------------------------------------- evaluar
// el final de una simulacion visto por "yo" (en ~cartas): LP (con mas peso cuando quedan pocos), ventaja de cartas y
// fuerza en la mesa, pesados por el plan del duelo y el orgullo
inline float& pesoCartasHumana() { static float v = 1.0f; return v; }   // (afinado de las pruebas)
inline float evaluarHumana(const Duelo& s, int yo, int plan, const Personalidad& p, float orgullo)
{
	const int otro = 1 - yo;
	if (s.ganador == yo) return 30.0f;
	if (s.ganador == otro) return -30.0f;
	if (s.ganador == 2) return 0.0f;
	auto vidas = [](int lp) {
		lp = std::max(0, lp);
		return (float)lp / 1000.0f + 0.8f * (float)std::min(lp, 3000) / 1000.0f;
	};
	auto cartas = [&](int l) {
		const Lado& x = s.l[(size_t)l];
		int n = (int)x.mano.size() + x.monstruos();
		for (const MagiaTrampa& c : x.mt) n += !c.vacia();
		return (float)n;
	};
	int maxAtk[2] = { 0, 0 };
	auto campo = [&](int l) {
		float f = 0.0f;
		for (const Monstruo& m : s.l[(size_t)l].m) {
			if (m.vacia()) continue;
			if (m.bocaAbajo) f += 1000.0f;
			else f += m.defensa ? 0.5f * (float)std::max(s.atkM(m), s.defM(m)) : (float)s.atkM(m);
			if (!m.bocaAbajo) maxAtk[l] = std::max(maxAtk[l], s.atkM(m));
		}
		return f / 1000.0f;
	};
	float wLP = 1.0f, wC = 0.7f, wF = 0.5f;
	switch (plan) {
	case PL_AGRESIVO:
	case PL_ENJAMBRE:
	case PL_TEMPO: wLP = 1.3f, wC = 0.5f, wF = 0.6f; break;
	case PL_COMBO: wLP = 1.1f, wC = 0.6f, wF = 0.7f; break;
	case PL_CONTROL:
	case PL_CASTIGO: wLP = 0.85f, wC = 0.95f, wF = 0.45f; break;
	case PL_BLOQUEO:
	case PL_MOLINO:
	case PL_ALTERNATIVA: wLP = 0.8f, wC = 0.9f, wF = 0.4f; break;
	default: break;
	}
	wLP *= 0.85f + 0.3f * p.agresividad;
	wC *= pesoCartasHumana();
	float v = wLP * (vidas(s.l[(size_t)yo].lp) - vidas(s.l[(size_t)otro].lp)) + wC * (cartas(yo) - cartas(otro)) + wF * (campo(yo) - campo(otro));
	if (plan == PL_MOLINO) v += 0.12f * (float)(30 - (int)s.l[(size_t)otro].mazo.size()) / 3.0f;
	if (plan == PL_ALTERNATIVA) {
		int ex = 0;
		for (const std::string& id : s.l[(size_t)yo].mano) ex += id.find("forbidden_one") != std::string::npos || id == "exodia_the_forbidden_one";
		v += 0.8f * (float)ex;
	}
	if (orgullo > 0.0f) v += orgullo * (maxAtk[yo] > 0 && maxAtk[yo] >= maxAtk[otro] ? 0.6f : -0.3f);
	return v;
}

// ------------------------------------------------------------------------------------------------- la IA humana
class Humana {
public:
	bool activa{ false };
	Rasgos r{ RASGOS_PERSONALIDAD[0] };
	Azar azar;
	float escala{ 1.0f };        // x presupuesto (la prueba lo puede bajar o subir)
	float msTope{ 150.0f };      // el techo de tiempo por decision (PS5: 30-150 ms segun el nivel)
	static inline int depura = 0;   // (pruebas) 1: sin errores creibles, 2: sin capa humana (faroles/lectura), 3: ninguna
	static inline float kPrior = 1.0f;   // (afinado: peso de la politica de siempre en la eleccion)
	static inline int horizonte = 2;                      // (afinado: turnos que se imaginan, contando el actual)
	static inline int nivelSim = 0;                       // (afinado: nivel de los rollouts; 0 = el propio + 3 y 6 el jugador)
	static inline float umbralObvia = 700.0f;             // (afinado: tan por encima de la segunda que no hace falta imaginar)
	// rama classic-ia-red: la red de valor (nullptr: sin red, como antes) y su peso frente al plan (<0: el del archivo)
	const Red* red{ nullptr };
	static inline float mezclaRed = -1.0f;                // (pruebas/entrenamiento: fuerza la mezcla)
	static inline int horizonteRed = 0;                   // (pruebas: turnos que se imaginan con red; 0 = horizonte)
	static inline float priorRed = 0.0f;                  // (rama classic-ia-red: peso de la red en el orden previo de las candidatas)
	static inline float umbralObviaRed = 0.0f;            // (rama classic-ia-red: con red, el umbral de lo obvio; 0 = el de siempre)
	static inline bool mezclaPersonaje = false;            // (rama classic-ia-red: la mezcla segun la lectura del personaje)
	static inline int simsMin = 60;                       // (rama classic-ia-red: el piso de simulaciones y de ms por decision;
	static inline float msMin = 80.0f;                    //  el entrenador lo baja para generar datos mas rapido)
	struct Estadisticas {
		int decisiones{ 0 }, buscadas{ 0 }, sims{ 0 }, errores{ 0 }, faroles{ 0 }, ocultas{ 0 }, guardadas{ 0 }, conRed{ 0 };
		double ms{ 0.0 }, msBuscadas{ 0.0 }, msMax{ 0.0 };
	} st;

	void configurar(bool on, const std::string& claveRival, const std::string& clavePersonalidad, uint32_t semilla)
	{
		activa = on;
		r = rasgosDe(claveRival, clavePersonalidad);
		azar = Azar(mezclar(semilla, 0x4A11u));
		azar.siguiente();
		enCurso = false;
	}
	bool buscando() const { return enCurso; }
	void cancelar() { enCurso = false; }

	// el presupuesto de esta decision (nivel, dificultad adaptable y vista del personaje)
	int simsPresupuesto(const Cerebro& c) const
	{
		// (juega fuerte desde el nivel 1: el nivel sube poco el presupuesto; la dificultad adaptable apenas lo baja)
		const float dif = std::max(-1.0f, c.plan.dificultad);
		const float n = (60.0f + 14.0f * (float)(c.nivel - 1)) * r.vista * (1.0f + 0.06f * dif) * escala;
		return std::max(simsMin, std::min(170, (int)n));
	}
	float msPresupuesto(const Cerebro& c) const
	{
		return std::max(msMin, std::min(msTope, (80.0f + 10.0f * (float)(c.nivel - 1)) * r.vista * std::max(0.5f, escala)));
	}

	// empieza una decision. true: ya esta (era obvia; "out" lista). false: hay que imaginar (trabajar)
	bool empezar(const Duelo& d, Cerebro& c, Jugada& out)
	{
		t0 = std::chrono::steady_clock::now();
		msUsado = 0.0;
		st.decisiones++;
		yo = d.decide();
		// (como Cerebro::decidir) el rival respondio a mis ataques: vuelve el miedo
		if (d.ef.castigos[(size_t)yo] != c.castigosVistos) {
			c.castigosVistos = d.ef.castigos[(size_t)yo];
			c.ataquesSinCastigo = 0;
		}
		if (d.ef.preg.tipo == P_ELEGIR || (d.ef.preg.tipo == P_NADA && d.fase == F_FINAL)) {
			out = c.decidir(d);
			out.pensar *= r.pausa;
			cerrar(false);
			return true;
		}
		respuesta = d.ef.preg.tipo == P_RESPUESTA;
		std::vector<Jugada> v;
		if (respuesta) c.candidatosRespuesta(d, v);
		else c.candidatos(d, v);
		std::vector<float> extra(v.size(), 0.0f);
		if (!(depura & 2)) ajustarHumano(d, c, v, extra);
		std::vector<size_t> orden(v.size());
		for (size_t i = 0; i < v.size(); i++) orden[i] = i;
		std::stable_sort(orden.begin(), orden.end(), [&](size_t x, size_t y) { return v[x].puntos > v[y].puntos; });
		const float mejor = v[orden[0]].puntos;
		const float segunda = v.size() > 1 ? v[orden[1]].puntos : -1e9f;
		int cercanas = 0;
		for (const Jugada& j : v) cercanas += j.puntos >= mejor - 350.0f;
		opciones = (int)v.size();
		// lo obvio: una sola opcion, un letal, una muy por encima del resto o (respuesta) nada que valga la pena
		bool obvia = v.size() == 1 || mejor >= 50000.0f || segunda < mejor - (red && umbralObviaRed > 0.0f ? umbralObviaRed : umbralObvia);
		if (respuesta) {
			float mejorAct = -1e9f;
			for (const Jugada& j : v)
				if (j.tipo != J_PASAR) mejorAct = std::max(mejorAct, j.puntos);
			obvia = obvia || mejorAct < -250.0f;
		}
		if (obvia) {
			// un poco de ruido (menos que la clasica: lo obvio casi no se equivoca)
			size_t m = orden[0];
			if (mejor < 50000.0f) {
				float best = -1e18f;
				for (size_t i = 0; i < v.size(); i++) {
					const float x = v[i].puntos + ((v[i].tipo == J_FASE || v[i].tipo == J_PASAR) ? 0.0f : azar.normal() * 60.0f);
					if (x > best) best = x, m = i;
				}
			}
			out = v[m];
			if (extra[m] > 0.0f && out.tipo == J_COLOCAR_MT) st.faroles++;
			out.pensar = pensarObvia(c, out);
			cerrar(false);
			return true;
		}
		// las candidatas a imaginar: las K mejores y siempre "pasar"
		const int K = std::min(7, 3 + c.nivel / 2);
		cands.clear();
		const float pErr = probError(d, c, cercanas);
		bool quitoMejor = false;
		for (size_t i : orden) {
			const Jugada& j = v[i];
			const bool pasar = j.tipo == J_FASE || j.tipo == J_PASAR;
			if (!pasar && (int)cands.size() >= K) continue;
			// error creible: "no ver el combo raro" (fusion, ritual, un efecto de monstruo o del cementerio)
			const bool rara = j.tipo == J_FUSION || j.tipo == J_RITUAL || j.tipo == J_TOON || (j.tipo == J_ACTIVAR && j.deMonstruo);
			if (rara && !pasar && j.puntos < 50000.0f && azar.uno() < pErr * 0.9f) {
				quitoMejor |= i == orden[0];
				continue;
			}
			Cand cd;
			cd.j = j;
			cd.prior = kPrior * std::max(-1.5f, std::min(0.0f, (j.puntos - mejor) / 1500.0f));
			cd.farol = extra[i] > 0.0f && j.tipo == J_COLOCAR_MT;
			cands.push_back(cd);
		}
		if (quitoMejor) st.errores++;
		if (cands.size() <= 1) {
			out = cands.empty() ? v[orden[0]] : cands[0].j;
			out.pensar = pensarObvia(c, out);
			cerrar(false);
			return true;
		}
		// la raiz de las simulaciones (sin la historia de sucesos ni el gancho del registro) y los modelos
		raiz = d;
		raiz.sucesos.clear();
		raiz.alAplicar = nullptr;
		raiz.log = false;
		mio = c;
		mio.nivel = nivelSim > 0 ? nivelSim : std::min(NIVEL_MAX, std::max(6, c.nivel + 3));   // (se imagina jugando con calma: menos ruido que en vivo)
		mio.valorEfecto = c.valorEfecto;
		const Lectura& lec = c.plan.lectura;
		Personalidad pm = PERSONALIDADES[0];   // el jugador, como lo ve la IA
		pm.agresividad = std::max(0.1f, std::min(1.0f, 0.25f + 0.65f * lec.ataques));
		pm.farol = std::max(0.1f, std::min(1.0f, 0.2f + 0.6f * lec.faroles));
		pm.cautela = std::max(0.1f, std::min(0.9f, 0.6f - 0.3f * (lec.ataques - 0.5f)));
		pm.umbral = 50.0f - 250.0f * (lec.ataques - 0.5f);
		otroModelo = Cerebro(pm, nivelSim > 0 ? nivelSim : 6, mezclar(azar.siguiente(), 77u));
		semillaBase = azar.siguiente();
		ronda = 0;
		cursor = 0;
		simMax = 0.0;
		sims = 0;
		simsMax = simsPresupuesto(c);
		msMax = msPresupuesto(c);
		pErrDecision = pErr;
		marcadorIni = marcador(d, yo);
		if (red && priorRed > 0.0f) ordenarConRed(c);
		enCurso = true;
		return false;
	}

	// imagina durante "ms" como mucho (una simulacion puede pasarse ~1-2 ms). true: termino y "out" esta lista
	bool trabajar(const Duelo& d, Cerebro& c, double ms, Jugada& out)
	{
		(void)d;
		if (!enCurso) return true;
		const auto inicio = std::chrono::steady_clock::now();
		auto transcurrido = [&]() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count(); };
		bool fin = false;
		while (!fin) {
			// una ronda: cada viva una simulacion con la misma semilla (numeros comunes: comparacion justa)
			for (; cursor < cands.size(); cursor++) {
				Cand& cd = cands[cursor];
				if (!cd.viva) continue;
				bool valida = true;
				const double antes = transcurrido();
				const float v = simular(cd.j, mezclar(semillaBase, (uint32_t)ronda * 2654435761u + 13u), c, valida);
				simMax = std::max(simMax, transcurrido() - antes);
				sims++;
				if (!valida) {
					if (++cd.invalidas >= 3 && cd.n == 0) cd.viva = false;
				} else {
					cd.suma += v;
					cd.suma2 += (double)v * v;
					cd.n++;
				}
				// (sin pasarse: si la siguiente simulacion no entra en lo que queda, se termina)
				const double usado = msUsado + transcurrido();
				if (sims >= simsMax || usado + std::max(1.5 * usado / std::max(1, sims), simMax) + 3.0 >= msMax) {   // (3 ms de margen)
					fin = true;
					cursor++;
					break;
				}
				if (transcurrido() >= ms) {
					cursor++;
					msUsado += transcurrido();
					return false;
				}
			}
			if (cursor >= cands.size()) {
				cursor = 0;
				ronda++;
				podar();
				int vivas = 0;
				for (const Cand& cd : cands) vivas += cd.viva && cd.n > 0;
				if (vivas <= 1 && ronda >= 3) fin = true;
			}
		}
		msUsado += transcurrido();
		out = elegir(d, c);
		cerrar(true);
		return true;
	}

private:
	struct Cand {
		Jugada j;
		float prior{ 0.0f };
		double suma{ 0.0 }, suma2{ 0.0 };
		int n{ 0 }, invalidas{ 0 };
		bool viva{ true }, farol{ false };
		float media() const { return n ? (float)(suma / n) : -1e9f; }
		float desvio() const
		{
			if (n < 2) return 0.0f;
			const double m = suma / n;
			return (float)std::sqrt(std::max(0.0, suma2 / n - m * m));
		}
	};
	std::vector<Cand> cands;
	bool enCurso{ false }, respuesta{ false };
	int yo{ 1 }, ronda{ 0 }, sims{ 0 }, simsMax{ 0 }, opciones{ 0 };
	size_t cursor{ 0 };
	double msUsado{ 0.0 }, msMax{ 0.0 }, simMax{ 0.0 };
	float pErrDecision{ 0.0f }, marcadorIni{ 0.0f };
	uint32_t semillaBase{ 1 };
	std::chrono::steady_clock::time_point t0;
	Duelo raiz;
	Cerebro mio, otroModelo;

	// rama classic-ia-red: el orden previo de las candidatas con la red (una copia determinizada, cada candidata
	// aplicada y valorada sin imaginar mas): suma al "prior" lo que la red ve de diferencia con la media
	void ordenarConRed(const Cerebro& c)
	{
		Duelo s = raiz;
		s.preguntarSiempre = {};
		Azar a(mezclar(semillaBase, 0xBEEFu));
		determinizar(s, yo, a, c.plan.lectura, r.lectura, c.ataquesSinCastigo);
		const int plan = c.plan.activo() ? c.plan.plan : -1;
		std::vector<float> z(cands.size(), 0.0f);
		std::vector<bool> ok(cands.size(), false);
		float suma = 0.0f;
		int n = 0;
		for (size_t i = 0; i < cands.size(); i++) {
			Duelo x = s;
			if (!x.aplicar(cands[i].j)) continue;
			if (x.ganador >= 0) continue;
			float f[RED_ENTRADAS];
			rasgosRed(x, yo, plan, f);
			z[i] = std::max(-4.0f, std::min(4.0f, red->logit(f)));
			ok[i] = true;
			suma += z[i], n++;
		}
		if (n < 2) return;
		const float media = suma / (float)n;
		for (size_t i = 0; i < cands.size(); i++)
			if (ok[i]) cands[i].prior += priorRed * red->escala * std::max(-1.0f, std::min(1.0f, z[i] - media));
	}

	void cerrar(bool buscada)
	{
		enCurso = false;
		const double ms = buscada ? msUsado : std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		st.ms += ms;
		st.msMax = std::max(st.msMax, ms);
		if (buscada) {
			st.buscadas++;
			st.msBuscadas += ms;
			st.sims += sims;
		}
		raiz = Duelo{};   // (no guardar una copia del duelo entre decisiones)
	}

	// la probabilidad de un error creible en esta decision
	float probError(const Duelo& d, const Cerebro& c, int cercanas) const
	{
		(void)d;
		if (depura & 1) return 0.0f;
		// juega fuerte desde el nivel 1 (~1,5 %) y el nivel casi no cambia nada (~1,15 % en el 8); la personalidad
		// (Joey arriesga) y la dificultad adaptable solo lo mueven un poco
		const float base = 0.015f - 0.0005f * (float)(c.nivel - 1);
		const float pers = 1.0f + 0.5f * (r.error - 1.0f);
		const float adapt = std::max(0.8f, std::min(1.1f, c.plan.factorRuido));
		const float p = base * adapt * (1.0f + 0.12f * (float)std::max(0, cercanas - 2)) * pers;
		return std::max(0.003f, std::min(0.04f, p));
	}

	// una simulacion: la copia, lo oculto adivinado, la candidata y 1-2 turnos con la politica de siempre
	float simular(const Jugada& j, uint32_t semilla, const Cerebro& c, bool& valida)
	{
		Duelo s = raiz;
		s.preguntarSiempre = {};   // (rama classic-ciclos: en lo que imagina, sin las preguntas de "Siempre")
		Azar a(semilla);
		a.siguiente();
		determinizar(s, yo, a, c.plan.lectura, r.lectura, c.ataquesSinCastigo);
		s.ef.azar = Azar(mezclar(semilla, 0x5EEDu));
		if (!s.aplicar(j)) {
			valida = false;
			return 0.0f;
		}
		Cerebro cc[2];
		cc[yo] = mio;
		cc[1 - yo] = otroModelo;
		cc[0].azar = Azar(mezclar(semilla, 101u));
		cc[1].azar = Azar(mezclar(semilla, 202u));
		for (Cerebro& x : cc)
			for (float& h : x.plan.humor) h = x.azar.normal() * 45.0f;
		const int fin = raiz.turno + (red && horizonteRed > 0 ? horizonteRed : horizonte);
		int pasos = 0, turnoV = s.turno, jugadas = 0;
		while (s.ganador < 0 && s.turno < fin && pasos < 220) {
			if (s.turno != turnoV) turnoV = s.turno, jugadas = 0;
			const int q = s.decide();
			const bool pregunta = s.ef.preg.tipo != P_NADA;
			Jugada x = cc[q].decidir(s);
			if (++jugadas > 40 && !pregunta) x = Jugada{};
			bool ok = s.aplicar(x);
			if (!ok && x.tipo != J_FASE) {
				x = Jugada{};
				if (pregunta) x.tipo = s.ef.preg.tipo == P_ELEGIR ? J_ELEGIR : J_PASAR, x.objetivo = 0;
				ok = s.aplicar(x);
			}
			if (!ok && s.fase == F_FINAL) {
				Jugada dsc;
				dsc.tipo = J_DESCARTAR;
				dsc.mano = 0;
				ok = s.aplicar(dsc);
			}
			if (!ok) break;   // (atascado: se evalua como esta)
			pasos++;
		}
		const int plan = c.plan.activo() ? c.plan.plan : -1;
		const float v = evaluarHumana(s, yo, plan, c.p, r.orgullo);
		if (!red || s.ganador >= 0) return v;
		st.conRed++;   // (rama classic-ia-red: el final mezclado con la red de valor)
		const float base = mezclaRed >= 0.0f ? mezclaRed : red->mezcla;
		// (por personaje: los que leen al jugador confian mas en la red; los impulsivos, menos)
		const float k = mezclaPersonaje ? std::max(0.6f, std::min(1.25f, 0.7f + 0.3f * r.lectura)) : 1.0f;
		return mezclarRed(*red, s, yo, plan, v, std::min(1.0f, base * k));
	}

	// las que quedan claramente atras salen (al menos 2 vivas)
	void podar()
	{
		if (ronda < 4) return;
		float mejor = -1e9f;
		for (const Cand& cd : cands)
			if (cd.viva && cd.n > 0) mejor = std::max(mejor, cd.media() + cd.prior);
		const float margen = 3.0f / std::sqrt((float)ronda);
		std::vector<Cand*> vivas;
		for (Cand& cd : cands)
			if (cd.viva && cd.n > 0) vivas.push_back(&cd);
		std::sort(vivas.begin(), vivas.end(), [](const Cand* x, const Cand* y) { return x->media() + x->prior > y->media() + y->prior; });
		for (size_t i = 2; i < vivas.size(); i++)
			if (vivas[i]->media() + vivas[i]->prior < mejor - margen) vivas[i]->viva = false;
	}

	// la mejor en promedio, con los errores creibles de una persona
	Jugada elegir(const Duelo& d, const Cerebro& c)
	{
		const float kRiesgo = 0.25f * (r.riesgo - 0.4f);
		int mejor = -1, segunda = -1;
		auto puntaje = [&](const Cand& cd, float kr) { return cd.media() + cd.prior + kr * cd.desvio(); };
		for (int i = 0; i < (int)cands.size(); i++) {
			const Cand& cd = cands[(size_t)i];
			if (cd.n == 0) continue;
			if (mejor < 0 || puntaje(cd, kRiesgo) > puntaje(cands[(size_t)mejor], kRiesgo)) segunda = mejor, mejor = i;
			else if (segunda < 0 || puntaje(cd, kRiesgo) > puntaje(cands[(size_t)segunda], kRiesgo)) segunda = i;
		}
		if (mejor < 0) {   // (ninguna simulacion valida: la politica de siempre)
			Jugada j = cands[0].j;
			j.pensar = pensarObvia(c, j);
			return j;
		}
		int elegida = mejor;
		const float pm = puntaje(cands[(size_t)mejor], kRiesgo);
		const float pErr = pErrDecision;
		const bool letal = cands[(size_t)mejor].j.puntos >= 50000.0f || cands[(size_t)mejor].media() >= 29.0f;
		if (!letal) {
			if (marcadorIni > 2.5f && azar.uno() < pErr * 1.2f) {
				// va ganando: sobrevalora el ataque
				int at = -1;
				for (int i = 0; i < (int)cands.size(); i++)
					if (cands[(size_t)i].n > 0 && cands[(size_t)i].j.tipo == J_ATACAR && (at < 0 || puntaje(cands[(size_t)i], kRiesgo) > puntaje(cands[(size_t)at], kRiesgo))) at = i;
				if (at >= 0 && at != mejor && puntaje(cands[(size_t)at], kRiesgo) >= pm - 1.2f) elegida = at;
			} else if (marcadorIni < -3.0f && azar.uno() < pErr * 1.5f + 0.01f * r.riesgo) {
				// desesperado: arriesga (lo que mejor puede salir, no lo que sale en promedio)
				int ar = mejor;
				for (int i = 0; i < (int)cands.size(); i++)
					if (cands[(size_t)i].n > 0 && puntaje(cands[(size_t)i], 1.2f) > puntaje(cands[(size_t)ar], 1.2f)) ar = i;
				elegida = ar;
			} else if (segunda >= 0 && azar.uno() < pErr * 0.5f && puntaje(cands[(size_t)segunda], kRiesgo) >= pm - 0.7f) {
				elegida = segunda;   // apurado: se queda con la otra que tambien parecia buena
			}
		}
		if (elegida != mejor) st.errores++;
		Cand& e = cands[(size_t)elegida];
		Jugada j = e.j;
		if (e.farol) st.faroles++;
		// lo dificil se piensa mas: cuantas quedaron cerca de la mejor
		int cerca = 0;
		for (const Cand& cd : cands) cerca += cd.n > 0 && puntaje(cd, kRiesgo) >= pm - 0.6f;
		float t = c.p.ritmo * r.pausa * (0.9f + 0.35f * std::log2(1.0f + (float)opciones) + 0.4f * (float)std::max(0, cerca - 1));
		if (azar.uno() < 0.08f) t += 0.8f + 1.2f * azar.uno();
		t *= std::exp(0.2f * azar.normal());
		j.pensar = std::max(0.5f, std::min(3.0f, t));   // (rama classic-md-ritmo: tope de Master Duel, CpuThinkingIcon.MAXTHINKINGTIME = 3 s; antes 5)
		if (!j.frase && cerca >= 3 && azar.uno() < 0.3f) j.frase = "ia.frase.duda";
		(void)d;
		return j;
	}

	float pensarObvia(const Cerebro& c, const Jugada& j)
	{
		float t = c.p.ritmo * r.pausa * (j.tipo == J_FASE || j.tipo == J_PASAR ? 0.35f : 0.5f);
		t *= 0.85f + 0.3f * azar.uno();
		return std::max(0.3f, std::min(2.0f, t));
	}

	// la capa humana sobre los puntos de la politica: faroles, lectura del jugador y orgullo. "extra": lo que sumo el
	// farol (para contarlo)
	void ajustarHumano(const Duelo& d, const Cerebro& c, std::vector<Jugada>& v, std::vector<float>& extra)
	{
		const int otro = 1 - yo;
		const Lado& x = d.l[(size_t)yo];
		const Lado& y = d.l[(size_t)otro];
		const Lectura& lec = c.plan.lectura;
		const float k = std::max(0.0f, std::min(1.5f, (0.35f + lec.confianza) * r.lectura));
		const float farolH = std::max(0.0f, std::min(1.2f, 0.5f * c.p.farol + 0.6f * r.farol));
		int trampasPuestas = 0, miMaxAtk = 0;
		for (const MagiaTrampa& mt : x.mt) trampasPuestas += !mt.vacia() && mt.bocaAbajo && esTrampa(d, mt.id);
		for (const Monstruo& m : x.m)
			if (!m.vacia() && !m.bocaAbajo) miMaxAtk = std::max(miMaxAtk, d.atkM(m));
		int suMaxAtk = 0;
		for (const Monstruo& m : y.m)
			if (!m.vacia()) suMaxAtk = std::max(suMaxAtk, m.bocaAbajo && !m.visto ? 1200 : d.atkM(m));
		for (size_t i = 0; i < v.size(); i++) {
			Jugada& j = v[i];
			if (!respuesta) {
				if (j.tipo == J_COLOCAR_MT && !esTrampa(d, j.carta) && subtipoDe(d, j.carta) != "magia_rapida") {
					// farol: una magia boca abajo que parece trampa
					float b = 0.0f;
					if (trampasPuestas == 0) b += 110.0f;
					if (x.monstruos() == 0 || suMaxAtk > miMaxAtk) b += 120.0f;
					b += 220.0f * (lec.ataques - 0.4f) * k;   // ataca con todo: el farol lo frena
					b += 160.0f * lec.limpia * k;            // limpia las boca abajo: gasta su quitamagias en el farol
					if (x.colocadasMT() >= 3) b -= 250.0f;
					b *= farolH;
					if (b > 0.0f) {
						j.puntos += b;
						extra[i] = b;
						if (b > 120.0f && !j.frase && azar.uno() < 0.35f) j.frase = "ia.frase.farol";
					}
				} else if (j.tipo == J_COLOCAR_MT && esTrampa(d, j.carta) && lec.ataques > 0.55f) {
					j.puntos += 300.0f * (lec.ataques - 0.5f) * k;   // el que ataca siempre cae en las trampas
				}
				if (r.orgullo > 0.0f) {
					const Carta* cc = d.carta(j.carta);
					if (j.tipo == J_COLOCAR && cc && cc->atk >= 1800) j.puntos -= 250.0f * r.orgullo;
					if (j.tipo == J_INVOCAR && cc && cc->atk >= 2000) j.puntos += 120.0f * r.orgullo;
					if (j.tipo == J_POSICION && j.zona >= 0 && j.zona < ZONAS && !x.m[(size_t)j.zona].defensa && d.atkM(x.m[(size_t)j.zona]) >= 1800)
						j.puntos -= 150.0f * r.orgullo;
				}
			} else if (j.tipo == J_ACTIVAR && j.zona >= 0 && j.zona < ZONAS && !j.deMonstruo) {
				const std::string id = j.carta.empty() ? x.mt[(size_t)j.zona].id : j.carta;
				if (!esTrampa(d, id)) continue;
				// el jugador ataca con todo: guardar la trampa para el ataque grande que viene detras
				if (d.ef.ataque.hay && d.ef.ataque.lado == otro && lec.ataques > 0.55f && x.lp > 2500) {
					const Lado& ya = d.l[(size_t)otro];
					const int z = std::max(0, std::min(ZONAS - 1, d.ef.ataque.zona));
					const int este = d.atkM(ya.m[(size_t)z]);
					int mayorPendiente = 0;
					for (int zz = 0; zz < ZONAS; zz++) {
						const Monstruo& m = ya.m[(size_t)zz];
						if (zz != z && !m.vacia() && !m.bocaAbajo && !m.defensa && !m.ataco) mayorPendiente = std::max(mayorPendiente, d.atkM(m));
					}
					if (mayorPendiente > este + 300) {
						j.puntos -= 450.0f * (lec.ataques - 0.5f) * k;
						st.guardadas++;
					}
				}
				// esconder informacion: una trampa marginal con LP de sobra a veces se calla
				if (j.puntos > 0.0f && j.puntos < 220.0f && x.lp > 3500 && azar.uno() < 0.5f * farolH) {
					j.puntos -= 260.0f;
					st.ocultas++;
				}
			}
		}
	}
};
