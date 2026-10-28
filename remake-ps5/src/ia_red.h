/*
 * Yugioh Classic Remaster - IA HUMANA, PASO 3: LA RED QUE APRENDE (rama classic-ia-red). Se incluye DENTRO de
 * namespace ia desde ia.h, justo antes de ia_humana.h (la busqueda la usa para evaluar el final de lo que imagina).
 *
 * Una red de VALOR chica (MLP 85 -> 64 -> 32 -> 1, ReLU y sigmoide; ~7,6 mil pesos, 30 KB en memoria) que mira el
 * duelo desde un lado y dice la probabilidad de que ese lado gane. Inferencia en C++ puro, sin dependencias: ~8 mil
 * multiplicaciones (unos microsegundos; la rebanada de la busqueda es de ~4 ms por cuadro).
 *
 *  - rasgosRed: los 85 rasgos de entrada (todos en 0..~2, para guardarlos en un byte: aprendizaje.h). Por cada lado
 *    (primero "yo", despues el otro) 33: LP (y LP bajos), mano, mazo, cementerio, desterradas, extra; monstruos (cuantos,
 *    boca abajo, en ataque, en defensa, los dos ATK mas altos, la suma del ATK en ataque, la DEF mas alta); magias y
 *    trampas boca abajo / boca arriba, campo; la mano por clase (monstruos, magias, trampas, el ATK mas alto, los de
 *    nivel 4 o menos) y por PAPEL (roles_cartas.txt: quita monstruos, quita magias/trampas, castigo, combo/enjambre,
 *    quema, victoria alternativa, robo, bloqueo/muro); y la amenaza (ATK en ataque sobre los LP del otro). Y 19
 *    globales: de quien es el turno, la fase, el turno, el PLAN del duelo (uno de 9 o ninguno) y la ventaja de LP y de
 *    cartas. En la busqueda el estado es una COPIA DETERMINIZADA (ia_humana.h: lo oculto del jugador adivinado), asi
 *    que la red no ve nada que la IA no podria saber.
 *  - Red: pesos, inferencia (logit/valor), lectura y escritura en texto con suma de control (si no cierra, el
 *    archivo no vale: se vuelve a la red base) y el paso de entrenamiento (Adam, entropia cruzada, opcionalmente
 *    atada a una red ANCLA: el ajuste fino del jugador no se aleja de la base; aprendizaje.h).
 *  - mezclarRed: el valor que usa la busqueda = (1 - mezcla) * evaluarHumana (el PLAN y el orgullo: la
 *    personalidad) + mezcla * escala * logit(red). La red no elige jugadas: solo afina lo que la busqueda cree que
 *    vale el final de cada simulacion. Los errores creibles, los faroles y el tiempo de pensar siguen igual.
 *
 * El archivo de pesos: datos/ia_red.txt (lo escribe src/pruebas/entrenar_ia_red.cpp por autojuego; llega al titulo
 * en ui/ como los demas datos) y, en la PS5, el ajustado al jugador en /data/yugioh_classic/ia_red_jugador.txt.
 * Ver docs/ia_red.md.
 */

constexpr int RED_LADO = 33, RED_GLOBAL = 19, RED_ENTRADAS = 2 * RED_LADO + RED_GLOBAL;   // 85
constexpr int RED_MAX_OCULTAS = 128;   // (tope de cada capa oculta: la inferencia usa arreglos en la pila)

// ------------------------------------------------------------------------------------------------- rasgos
inline float redAcotar(float v, float hi = 2.0f) { return v < 0.0f ? 0.0f : v > hi ? hi : v; }
inline void rasgosLado(const Duelo& s, int a, float* f)
{
	const Lado& L = s.l[(size_t)a];
	const Lado& R = s.l[(size_t)(1 - a)];
	const int lp = std::max(0, L.lp);
	f[0] = redAcotar((float)lp / 8000.0f);
	f[1] = (float)std::min(lp, 3000) / 3000.0f;
	f[2] = (float)std::min(lp, 1000) / 1000.0f;
	f[3] = redAcotar((float)L.mano.size() / 6.0f);
	f[4] = redAcotar((float)L.mazo.size() / 40.0f);
	f[5] = redAcotar((float)L.cementerio.size() / 20.0f);
	f[6] = redAcotar((float)L.desterrados.size() / 10.0f);
	f[7] = redAcotar((float)L.extra.size() / 15.0f);
	int n = 0, abajo = 0, ataque = 0, defArriba = 0, atk1 = 0, atk2 = 0, sumAtk = 0, maxDef = 0;
	for (const Monstruo& m : L.m) {
		if (m.vacia()) continue;
		n++;
		if (m.bocaAbajo) {
			abajo++;
			continue;
		}
		const int at = s.atkM(m), df = s.defM(m);
		if (m.defensa) defArriba++;
		else ataque++, sumAtk += at;
		if (at > atk1) atk2 = atk1, atk1 = at;
		else if (at > atk2) atk2 = at;
		maxDef = std::max(maxDef, df);
	}
	f[8] = (float)n / 5.0f;
	f[9] = (float)abajo / 5.0f;
	f[10] = (float)ataque / 5.0f;
	f[11] = (float)defArriba / 5.0f;
	f[12] = redAcotar((float)atk1 / 3000.0f);
	f[13] = redAcotar((float)atk2 / 3000.0f);
	f[14] = redAcotar((float)sumAtk / 8000.0f);
	f[15] = redAcotar((float)maxDef / 3000.0f);
	int mtAbajo = 0, mtArriba = 0;
	for (const MagiaTrampa& c : L.mt) {
		if (c.vacia()) continue;
		if (c.bocaAbajo) mtAbajo++;
		else mtArriba++;
	}
	f[16] = (float)mtAbajo / 5.0f;
	f[17] = (float)mtArriba / 5.0f;
	f[18] = L.campo.vacia() ? 0.0f : 1.0f;
	int mm = 0, mg = 0, tr = 0, maxAtkMano = 0, bajos = 0;
	int pd = 0, pt = 0, pc = 0, pk = 0, pq = 0, pa = 0, po = 0, pb = 0;
	auto papeles = [&](const std::string& id) {
		if (!s.base) return;
		auto it = s.base->papeles.find(id);
		if (it == s.base->papeles.end()) return;
		const std::string& p = it->second;
		pd += p.find('d') != std::string::npos;
		pt += p.find('t') != std::string::npos;
		pc += p.find('c') != std::string::npos;
		pk += p.find('k') != std::string::npos || p.find('e') != std::string::npos;
		pq += p.find('q') != std::string::npos;
		pa += p.find('a') != std::string::npos;
		po += p.find('o') != std::string::npos;
		pb += p.find('b') != std::string::npos || p.find('w') != std::string::npos;
	};
	for (const std::string& id : L.mano) {
		const Carta* c = s.carta(id);
		if (c) {
			if (c->monstruo) {
				mm++;
				maxAtkMano = std::max(maxAtkMano, c->atk);
				bajos += c->nivel <= 4;
			} else if (c->trampa) {
				tr++;
			} else {
				mg++;
			}
		}
		papeles(id);
	}
	for (const MagiaTrampa& c : L.mt)
		if (!c.vacia() && c.bocaAbajo) papeles(c.id);   // (lo colocado tambien cuenta: castigo, quitamagias...)
	f[19] = redAcotar((float)mm / 6.0f);
	f[20] = redAcotar((float)mg / 6.0f);
	f[21] = redAcotar((float)tr / 6.0f);
	f[22] = redAcotar((float)maxAtkMano / 3000.0f);
	f[23] = redAcotar((float)bajos / 6.0f);
	f[24] = redAcotar((float)pd / 3.0f);
	f[25] = redAcotar((float)pt / 3.0f);
	f[26] = redAcotar((float)pc / 3.0f);
	f[27] = redAcotar((float)pk / 3.0f);
	f[28] = redAcotar((float)pq / 3.0f);
	f[29] = redAcotar((float)pa / 5.0f);
	f[30] = redAcotar((float)po / 3.0f);
	f[31] = redAcotar((float)pb / 3.0f);
	f[32] = redAcotar((float)sumAtk / (float)std::max(100, R.lp));
}
// los 85 rasgos del duelo "s" visto por "yo" con el plan "plan" (-1: sin plan)
inline void rasgosRed(const Duelo& s, int yo, int plan, float* x)
{
	rasgosLado(s, yo, x);
	rasgosLado(s, 1 - yo, x + RED_LADO);
	float* g = x + 2 * RED_LADO;
	for (int i = 0; i < RED_GLOBAL; i++) g[i] = 0.0f;
	g[0] = s.activo == yo ? 1.0f : 0.0f;
	g[1 + std::max(0, std::min(4, (int)s.fase))] = 1.0f;
	g[6] = redAcotar((float)s.turno / 40.0f);
	g[7 + (plan >= 0 && plan < NUM_PLANES ? plan : NUM_PLANES)] = 1.0f;   // 7..16 (el 16: sin plan)
	auto cartas = [&](int l) {
		const Lado& X = s.l[(size_t)l];
		int c = (int)X.mano.size() + X.monstruos();
		for (const MagiaTrampa& m : X.mt) c += !m.vacia();
		return c;
	};
	g[17] = redAcotar(0.5f + (float)(std::max(0, s.l[(size_t)yo].lp) - std::max(0, s.l[(size_t)(1 - yo)].lp)) / 16000.0f, 1.0f);
	g[18] = redAcotar(0.5f + (float)(cartas(yo) - cartas(1 - yo)) / 16.0f, 1.0f);
}

// ------------------------------------------------------------------------------------------------- la red
struct Red {
	int nE{ 0 }, n1{ 0 }, n2{ 0 };
	std::vector<float> w1, b1, w2, b2, w3;   // w1[n1][nE], w2[n2][n1], w3[n2]
	float b3{ 0.0f };
	float escala{ 2.0f };   // logit -> unidades de evaluarHumana (~cartas): lo calibra el entrenador
	float mezcla{ 0.5f };   // peso de la red frente al plan en el final de cada simulacion
	int generacion{ 0 };
	std::string origen;     // "base" (autojuego) o "jugador" (ajuste fino)

	bool valida() const
	{
		return nE == RED_ENTRADAS && n1 > 0 && n2 > 0 && n1 <= RED_MAX_OCULTAS && n2 <= RED_MAX_OCULTAS && w1.size() == (size_t)n1 * nE && b1.size() == (size_t)n1 &&
		       w2.size() == (size_t)n2 * n1 && b2.size() == (size_t)n2 && w3.size() == (size_t)n2;
	}
	size_t numPesos() const { return w1.size() + b1.size() + w2.size() + b2.size() + w3.size() + 1; }
	void iniciar(int ocultas1, int ocultas2, uint32_t semilla)
	{
		nE = RED_ENTRADAS, n1 = ocultas1, n2 = ocultas2;
		Azar a(semilla);
		auto he = [&](std::vector<float>& w, size_t n, int fanIn) {
			w.resize(n);
			const float k = std::sqrt(2.0f / (float)fanIn);
			for (float& x : w) x = a.normal() * k;
		};
		he(w1, (size_t)n1 * nE, nE);
		he(w2, (size_t)n2 * n1, n1);
		he(w3, (size_t)n2, n2);
		for (float& x : w3) x *= 0.1f;
		b1.assign((size_t)n1, 0.01f);
		b2.assign((size_t)n2, 0.01f);
		b3 = 0.0f;
	}
	// la salida antes de la sigmoide (h1/h2: las activaciones, para el entrenamiento; pueden ser nullptr)
	float logit(const float* x, float* h1 = nullptr, float* h2 = nullptr) const
	{
		float a1[RED_MAX_OCULTAS], a2[RED_MAX_OCULTAS];
		float* p1 = h1 ? h1 : a1;
		float* p2 = h2 ? h2 : a2;
		for (int i = 0; i < n1; i++) {
			const float* w = &w1[(size_t)i * nE];
			float v = b1[(size_t)i];
			for (int j = 0; j < nE; j++) v += w[j] * x[j];
			p1[i] = v > 0.0f ? v : 0.0f;
		}
		for (int i = 0; i < n2; i++) {
			const float* w = &w2[(size_t)i * n1];
			float v = b2[(size_t)i];
			for (int j = 0; j < n1; j++) v += w[j] * p1[j];
			p2[i] = v > 0.0f ? v : 0.0f;
		}
		float o = b3;
		for (int i = 0; i < n2; i++) o += w3[(size_t)i] * p2[i];
		return o;
	}
	static float sigmoide(float z) { return 1.0f / (1.0f + std::exp(-std::max(-30.0f, std::min(30.0f, z)))); }
	float valor(const float* x) const { return sigmoide(logit(x)); }
	float valor(const Duelo& s, int yo, int plan) const
	{
		float x[RED_ENTRADAS];
		rasgosRed(s, yo, plan, x);
		return valor(x);
	}

	// todos los pesos en un orden fijo (para la suma de control, el ancla y el optimizador)
	template <class F> void cadaPeso(F f)
	{
		for (float& v : w1) f(v);
		for (float& v : b1) f(v);
		for (float& v : w2) f(v);
		for (float& v : b2) f(v);
		for (float& v : w3) f(v);
		f(b3);
	}
	template <class F> void cadaPeso(F f) const { const_cast<Red*>(this)->cadaPeso([&](float& v) { f((const float&)v); }); }
	bool finita() const
	{
		bool ok = std::isfinite(escala) && std::isfinite(mezcla);
		cadaPeso([&](const float& v) { ok = ok && std::isfinite(v) && std::fabs(v) < 1e4f; });
		return ok;
	}

	// ---- archivo de texto: "red 1 <nE> <n1> <n2>", "escala", "mezcla", "generacion", "origen", "pesos" y los pesos (16 por
	// linea), y al final "fin <suma>" (FNV-1a de todo lo anterior: si no cierra, el archivo no vale)
	static uint32_t fnv(const char* p, size_t n, uint32_t h = 2166136261u)
	{
		for (size_t i = 0; i < n; i++) h = (h ^ (uint8_t)p[i]) * 16777619u;
		return h;
	}
	std::string texto() const
	{
		std::string s;
		char b[64];
		snprintf(b, sizeof(b), "red 1 %d %d %d\n", nE, n1, n2);
		s += "# Yugioh Classic Remaster: red de valor de la IA humana (ia_red.h; la escribe entrenar_ia_red.cpp o el ajuste fino)\n";
		s += b;
		snprintf(b, sizeof(b), "escala %.6g\nmezcla %.6g\ngeneracion %d\n", (double)escala, (double)mezcla, generacion);
		s += b;
		s += "origen " + (origen.empty() ? std::string("base") : origen) + "\npesos\n";
		int k = 0;
		cadaPeso([&](const float& v) {
			snprintf(b, sizeof(b), "%.6g", (double)v);
			s += b;
			s += (++k % 16 == 0) ? '\n' : ' ';
		});
		if (s.back() != '\n') s += '\n';
		snprintf(b, sizeof(b), "fin %08x\n", fnv(s.data(), s.size()));
		s += b;
		return s;
	}
	bool leerTexto(const std::string& s)
	{
		*this = Red{};
		const size_t f = s.rfind("\nfin ");
		if (f == std::string::npos) return false;
		const uint32_t suma = (uint32_t)strtoul(s.c_str() + f + 5, nullptr, 16);
		if (fnv(s.data(), f + 1) != suma) return false;
		const std::string cuerpo = s.substr(0, f + 1);
		std::istringstream in(cuerpo);
		std::string l;
		int ver = 0;
		bool pesos = false;
		std::vector<float> todos;
		while (std::getline(in, l)) {
			if (l.empty() || l[0] == '#') continue;
			std::istringstream p(l);
			if (pesos) {
				float v;
				while (p >> v) todos.push_back(v);
				continue;
			}
			std::string k;
			p >> k;
			if (k == "red") p >> ver >> nE >> n1 >> n2;
			else if (k == "escala") p >> escala;
			else if (k == "mezcla") p >> mezcla;
			else if (k == "generacion") p >> generacion;
			else if (k == "origen") p >> origen;
			else if (k == "pesos") pesos = true;
		}
		if (ver != 1 || nE != RED_ENTRADAS || n1 <= 0 || n2 <= 0 || n1 > RED_MAX_OCULTAS || n2 > RED_MAX_OCULTAS) return (*this = Red{}), false;
		w1.resize((size_t)n1 * nE), b1.resize((size_t)n1), w2.resize((size_t)n2 * n1), b2.resize((size_t)n2), w3.resize((size_t)n2);
		if (todos.size() != numPesos()) return (*this = Red{}), false;
		size_t i = 0;
		cadaPeso([&](float& v) { v = todos[i++]; });
		mezcla = std::max(0.0f, std::min(1.0f, mezcla));
		if (!valida() || !finita()) return (*this = Red{}), false;
		return true;
	}
	bool leer(const std::string& ruta)
	{
		std::ifstream f(ruta, std::ios::binary);
		if (!f) return (*this = Red{}), false;
		std::stringstream ss;
		ss << f.rdbuf();
		return leerTexto(ss.str());
	}
	bool guardar(const std::string& ruta) const { return guardado::guardarAtomico(ruta, texto()); }
};

// el valor del final de una simulacion: el del plan (la personalidad) mezclado con la red
inline float mezclarRed(const Red& red, const Duelo& s, int yo, int plan, float vPlan, float mezcla)
{
	const float w = std::max(0.0f, std::min(1.0f, mezcla));
	if (w <= 0.0f) return vPlan;
	float x[RED_ENTRADAS];
	rasgosRed(s, yo, plan, x);
	const float z = std::max(-4.0f, std::min(4.0f, red.logit(x)));
	return (1.0f - w) * vPlan + w * red.escala * z;
}

// ------------------------------------------------------------------------------------------------- entrenar
// Adam con entropia cruzada (el resultado: 1 gana "yo", 0 pierde, 0,5 empate) y, si hay ancla, una atadura L2 a sus
// pesos (el ajuste fino del jugador no se aleja de la red base: no olvida lo del autojuego por unos pocos duelos)
struct Entrenador {
	std::vector<float> m, v, g;
	int t{ 0 };
	float lr{ 1e-3f }, b1{ 0.9f }, b2{ 0.999f }, l2{ 1e-5f };
	float ancla{ 0.0f };   // fuerza de la atadura a la red ancla
	bool soloSalida{ false };   // solo la ultima capa (w3, b3): el ajuste fino del jugador (pocos datos: no sobreajusta)

	// un lote: X[n][RED_ENTRADAS], y[n] (0..1). Devuelve la perdida media del lote (antes del paso)
	float paso(Red& r, const float* X, const float* y, int n, const Red* base = nullptr)
	{
		const size_t P = r.numPesos();
		if (g.size() != P) g.assign(P, 0.0f), m.assign(P, 0.0f), v.assign(P, 0.0f), t = 0;
		std::fill(g.begin(), g.end(), 0.0f);
		const int nE = r.nE, n1 = r.n1, n2 = r.n2;
		float* gw1 = g.data();
		float* gb1 = gw1 + (size_t)n1 * nE;
		float* gw2 = gb1 + n1;
		float* gb2 = gw2 + (size_t)n2 * n1;
		float* gw3 = gb2 + n2;
		float* gb3 = gw3 + n2;
		float h1[RED_MAX_OCULTAS], h2[RED_MAX_OCULTAS], d1[RED_MAX_OCULTAS], d2[RED_MAX_OCULTAS];
		double perdida = 0.0;
		for (int k = 0; k < n; k++) {
			const float* x = X + (size_t)k * RED_ENTRADAS;
			const float z = r.logit(x, h1, h2);
			const float p = Red::sigmoide(z);
			const float yy = y[k];
			perdida += -(yy * std::log(std::max(1e-7f, p)) + (1.0f - yy) * std::log(std::max(1e-7f, 1.0f - p)));
			const float dz = (p - yy) / (float)n;
			*gb3 += dz;
			for (int i = 0; i < n2; i++) {
				gw3[i] += dz * h2[i];
				d2[i] = h2[i] > 0.0f ? dz * r.w3[(size_t)i] : 0.0f;
			}
			if (soloSalida) continue;
			for (int j = 0; j < n1; j++) d1[j] = 0.0f;
			for (int i = 0; i < n2; i++) {
				if (d2[i] == 0.0f) continue;
				gb2[i] += d2[i];
				const float* w = &r.w2[(size_t)i * n1];
				float* gw = gw2 + (size_t)i * n1;
				for (int j = 0; j < n1; j++) {
					gw[j] += d2[i] * h1[j];
					d1[j] += d2[i] * w[j];
				}
			}
			for (int j = 0; j < n1; j++) {
				if (h1[j] <= 0.0f || d1[j] == 0.0f) continue;
				gb1[j] += d1[j];
				float* gw = gw1 + (size_t)j * nE;
				for (int e = 0; e < nE; e++) gw[e] += d1[j] * x[e];
			}
		}
		// L2 y atadura al ancla
		size_t i = 0;
		std::vector<float> anc;
		if (base && ancla > 0.0f && base->numPesos() == P) {
			anc.reserve(P);
			base->cadaPeso([&](const float& w) { anc.push_back(w); });
		}
		t++;
		const float c1 = 1.0f - std::pow(b1, (float)t), c2 = 1.0f - std::pow(b2, (float)t);
		const size_t desde = soloSalida ? P - (size_t)n2 - 1 : 0;
		r.cadaPeso([&](float& w) {
			if (i < desde) {
				i++;
				return;
			}
			float gg = g[i] + l2 * w;
			if (!anc.empty()) gg += ancla * (w - anc[i]);
			m[i] = b1 * m[i] + (1.0f - b1) * gg;
			v[i] = b2 * v[i] + (1.0f - b2) * gg * gg;
			w -= lr * (m[i] / c1) / (std::sqrt(v[i] / c2) + 1e-8f);
			i++;
		});
		return n ? (float)(perdida / n) : 0.0f;
	}
};
// la perdida media (entropia cruzada) de la red sobre un conjunto
inline float perdidaRed(const Red& r, const float* X, const float* y, int n)
{
	double s = 0.0;
	for (int k = 0; k < n; k++) {
		const float p = r.valor(X + (size_t)k * RED_ENTRADAS);
		s += -(y[k] * std::log(std::max(1e-7f, p)) + (1.0f - y[k]) * std::log(std::max(1e-7f, 1.0f - p)));
	}
	return n ? (float)(s / n) : 0.0f;
}
