// ENTRENAR LA RED DE VALOR DE LA IA HUMANA POR AUTOJUEGO (rama classic-ia-red; src/ia_red.h, docs/ia_red.md).
// Juega miles de duelos IA contra IA con los rivales reales (rivales.txt + personajes_rivales.txt, mazos dinamicos y
// planes de ia_plan.h), graba las posiciones (al empezar cada turno, desde los dos lados) con su resultado, entrena la
// red y repite por GENERACIONES: la candidata juega contra la campeona (pares de duelos con los mismos mazos y la
// misma semilla, cambiando de lado) y se queda si gana >= 55 %. La generacion 0 juega contra la IA humana sin red
// (los pasos 1-2). Cada campeona se escribe en <salida> (datos/ia_red.txt).
// Procesos en paralelo con fork (el motor tiene banderas estaticas de reentrada: hilos no; procesos si).
// Compilar y correr (contenedor arch-ps5; DESACOPLADO, tarda):
//   g++ -std=c++17 -O3 -march=native -I remake-ps5/src remake-ps5/src/pruebas/entrenar_ia_red.cpp -o /tmp/entrenar_ia_red
//   /tmp/entrenar_ia_red <carpeta ui/ con cartas_ui.txt> remake-ps5/datos/ remake-ps5/datos/ia_red.txt [generaciones]
// Variables: HILOS (10), DUELOS_CLASICA (4000, solo gen 0), DUELOS_HUMANA (1200 por generacion), PARES (150),
//            ESCALA_DATOS (0.35: presupuesto de la busqueda al generar datos) y SIMS_MIN_DATOS (20), EPOCAS (12),
//            OCULTAS1 (64), OCULTAS2 (32), MEZCLA (0.5), HORIZONTE_RED (0), UMBRAL (55), UMBRAL0 (50: la gen 0 contra la humana sin red), L2 (1e-4), INICIAL (un ia_red.txt para
//            seguir desde ahi), GUARDAR_MEJOR (sin campeona, escribe la candidata igual)
#include "ia.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <map>

using namespace ia;

static int envi(const char* k, int d) { return getenv(k) ? atoi(getenv(k)) : d; }
static float envf(const char* k, float d) { return getenv(k) ? (float)atof(getenv(k)) : d; }
static double ahora()
{
	static const auto t0 = std::chrono::steady_clock::now();
	return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

// ------------------------------------------------------------------------------------------------- en paralelo
// reparte las tareas 0..n-1 entre "hilos" procesos; cada uno devuelve un string por tarea (concatenados)
template <class F> static std::string enParalelo(int hilos, int n, F f)
{
	char plantilla[] = "/tmp/entrenar_ia_red_XXXXXX";
	const char* dir = mkdtemp(plantilla);
	if (!dir) {
		std::string s;
		for (int i = 0; i < n; i++) s += f(i);
		return s;
	}
	std::vector<pid_t> hijos;
	for (int c = 0; c < hilos; c++) {
		const pid_t p = fork();
		if (p == 0) {
			std::string s;
			for (int i = c; i < n; i += hilos) s += f(i);
			const std::string ruta = std::string(dir) + "/" + std::to_string(c);
			FILE* o = fopen(ruta.c_str(), "wb");
			if (o) {
				fwrite(s.data(), 1, s.size(), o);
				fclose(o);
			}
			_exit(0);
		}
		if (p > 0) hijos.push_back(p);
	}
	for (pid_t p : hijos) waitpid(p, nullptr, 0);
	std::string todo;
	for (int c = 0; c < hilos; c++) {
		const std::string ruta = std::string(dir) + "/" + std::to_string(c);
		std::ifstream in(ruta, std::ios::binary);
		std::stringstream ss;
		ss << in.rdbuf();
		todo += ss.str();
		remove(ruta.c_str());
	}
	rmdir(dir);
	return todo;
}

// ------------------------------------------------------------------------------------------------- un duelo
struct Lados {
	bool humana[2]{ false, false };
	const Red* red[2]{ nullptr, nullptr };
	float escala{ 1.0f }, escalaLado[2]{ 1.0f, 1.0f };
};
struct Salida {
	int ganador{ -1 };
	double ms[2]{ 0, 0 }, msMax[2]{ 0, 0 };
	int decisiones[2]{ 0, 0 };
};
// una muestra: 85 bytes de rasgos, el resultado (0 pierde, 1 gana, 2 empate) y el valor del plan (x100, int16)
constexpr int BYTES_MUESTRA = RED_ENTRADAS + 3 + 5;   // (+ el duelo, 4 bytes, y el lado: para TD(lambda))

static Salida jugar(const Base& b, uint32_t s, int k, const Lados& L, std::string* muestras)
{
	const int nivel = 1 + k % NIVEL_MAX;
	Azar az(mezclar(s, 991u));
	std::vector<const Rival*> deEse;
	for (const Rival& x : b.rivales)
		if (x.nivel == nivel) deEse.push_back(&x);
	if (deEse.empty())
		for (const Rival& x : b.rivales) deEse.push_back(&x);
	const Rival& ra = *deEse[(size_t)az.hasta((int)deEse.size())];
	const Rival& rb = *deEse[(size_t)az.hasta((int)deEse.size())];
	// lo que la IA "recuerda" del jugador: variado (como en la PS5 con jugadores distintos)
	Lectura lec[2];
	for (Lectura& x : lec) {
		x.trampas = az.entre(0.15f, 0.7f), x.faroles = az.entre(0.1f, 0.6f), x.ataques = az.entre(0.2f, 0.9f), x.muros = az.entre(0.1f, 0.6f);
		x.limpia = az.entre(0.05f, 0.6f), x.confianza = az.entre(0.0f, 1.0f);
	}
	const MazoRival a = prepararRival(b, ra, s, nullptr), c = prepararRival(b, rb, s ^ 0x55u, nullptr);
	Duelo d(&b);
	d.empezar(a.principal, a.fusion, c.principal, c.fusion, s);
	Turno t0(&d, 0, ra.personalidad(), ra.nivel, s), t1(&d, 1, rb.personalidad(), rb.nivel, s + 1);
	t0.cerebro.usarPlan(a.plan, lec[0], 0, 1);
	t1.cerebro.usarPlan(c.plan, lec[1], 0, 1);
	t0.usarHumana(L.humana[0], ra.clave, s);
	t1.usarHumana(L.humana[1], rb.clave, s + 1);
	t0.humana.red = L.red[0];
	t1.humana.red = L.red[1];
	t0.humana.escala = L.escala * L.escalaLado[0];
	t1.humana.escala = L.escala * L.escalaLado[1];
	t0.humana.msTope = 150.0f * L.escalaLado[0];
	t1.humana.msTope = 150.0f * L.escalaLado[1];
	Turno* t[2] = { &t0, &t1 };
	struct Pos {
		uint8_t q[RED_ENTRADAS];
		int yo;
		float v;
	};
	std::vector<Pos> pos;
	int pasos = 0, turnoVisto = -1;
	while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
		if (muestras && d.turno != turnoVisto) {
			turnoVisto = d.turno;
			for (int yo = 0; yo < 2; yo++) {
				const Cerebro& cb = t[yo]->cerebro;
				const int plan = cb.plan.activo() ? cb.plan.plan : -1;
				Pos p;
				float x[RED_ENTRADAS];
				rasgosRed(d, yo, plan, x);
				aprendizaje::cuantizar(x, p.q);
				p.yo = yo;
				p.v = evaluarHumana(d, yo, plan, cb.p, 0.0f);
				pos.push_back(p);
			}
		}
		t[d.decide()]->paso();
		pasos++;
	}
	Salida o;
	o.ganador = d.ganador;
	for (int i = 0; i < 2; i++) {
		o.ms[i] = t[i]->humana.st.ms;
		o.msMax[i] = t[i]->humana.st.msMax;
		o.decisiones[i] = t[i]->humana.st.decisiones;
	}
	if (muestras && d.ganador >= 0) {
		for (const Pos& p : pos) {
			muestras->append((const char*)p.q, RED_ENTRADAS);
			*muestras += (char)(d.ganador == 2 ? 2 : d.ganador == p.yo ? 1 : 0);
			const int v = std::max(-32000, std::min(32000, (int)std::lround(p.v * 100.0f)));
			*muestras += (char)(v & 0xFF), *muestras += (char)((v >> 8) & 0xFF);
			const uint32_t id = mezclar(s, (uint32_t)k * 2654435761u + 17u);
			for (int b = 0; b < 4; b++) *muestras += (char)((id >> (8 * b)) & 0xFF);
			*muestras += (char)p.yo;
		}
	}
	return o;
}

// ------------------------------------------------------------------------------------------------- datos y entrenamiento
struct Datos {
	std::vector<float> X, y, v, res;   // y: el objetivo (res: el resultado; con TD, mezclado con la red)
	std::vector<uint32_t> duelo;
	std::vector<uint8_t> lado;
	int n() const { return (int)y.size(); }
	void quitarViejas(int fuera)
	{
		X.erase(X.begin(), X.begin() + (long)fuera * RED_ENTRADAS);
		y.erase(y.begin(), y.begin() + fuera), v.erase(v.begin(), v.begin() + fuera), res.erase(res.begin(), res.begin() + fuera);
		duelo.erase(duelo.begin(), duelo.begin() + fuera), lado.erase(lado.begin(), lado.begin() + fuera);
	}
	void agregar(const std::string& m)
	{
		const size_t k = m.size() / BYTES_MUESTRA;
		for (size_t i = 0; i < k; i++) {
			const uint8_t* p = (const uint8_t*)m.data() + i * BYTES_MUESTRA;
			const size_t a = X.size();
			X.resize(a + RED_ENTRADAS);
			aprendizaje::descuantizar(p, &X[a]);
			y.push_back(p[RED_ENTRADAS] == 2 ? 0.5f : (float)p[RED_ENTRADAS]);
			v.push_back((float)(int16_t)(p[RED_ENTRADAS + 1] | (p[RED_ENTRADAS + 2] << 8)) / 100.0f);
			res.push_back(y.back());
			uint32_t id = 0;
			for (int b = 0; b < 4; b++) id |= (uint32_t)p[RED_ENTRADAS + 3 + b] << (8 * b);
			duelo.push_back(id);
			lado.push_back(p[RED_ENTRADAS + 7]);
		}
	}
};

// diagnostico: que tan bien predice el resultado la evaluacion del plan (evaluarHumana) sola: sigmoide(a v + b)
static float perdidaPlan(const Datos& D)
{
	float a = 0.2f, b = 0.0f;
	const int n = D.n(), paso = std::max(1, n / 50000);
	for (int it = 0; it < 300; it++) {
		double ga = 0, gb = 0;
		int m = 0;
		for (int i = 0; i < n; i += paso, m++) {
			const float p = Red::sigmoide(a * D.v[(size_t)i] + b);
			ga += (p - D.res[(size_t)i]) * D.v[(size_t)i], gb += p - D.res[(size_t)i];
		}
		a -= 0.05f * (float)(ga / m), b -= 0.5f * (float)(gb / m);
	}
	double s = 0;
	int m = 0;
	for (int i = 0; i < n; i += paso, m++) {
		const float p = Red::sigmoide(a * D.v[(size_t)i] + b), y = D.res[(size_t)i];
		s += -(y * std::log(std::max(1e-7f, p)) + (1 - y) * std::log(std::max(1e-7f, 1 - p)));
	}
	return (float)(s / std::max(1, m));
}

// TD(lambda): el objetivo de cada posicion mezcla el resultado con lo que la red cree de la siguiente posicion del
// mismo lado en el mismo duelo: y_t = (1 - lambda) V(s_t+1) + lambda y_t+1, y al final el resultado
static void objetivosTD(Datos& D, const Red& r, float lambda)
{
	const int n = D.n();
	int i = 0;
	while (i < n) {
		int j = i;
		while (j < n && D.duelo[(size_t)j] == D.duelo[(size_t)i]) j++;
		for (int yo = 0; yo < 2; yo++) {
			int sig = -1;
			for (int k = j - 1; k >= i; k--) {
				if (D.lado[(size_t)k] != yo) continue;
				if (sig < 0) D.y[(size_t)k] = D.res[(size_t)k];
				else D.y[(size_t)k] = (1.0f - lambda) * r.valor(&D.X[(size_t)sig * RED_ENTRADAS]) + lambda * D.y[(size_t)sig];
				sig = k;
			}
		}
		i = j;
	}
}

// entrena (desde "inicio" o de cero) con el 90 % y mide en el 10 % (las muestras de cada duelo van juntas: se parte por
// bloques de 64 para que el 10 % no comparta duelos con el resto casi nunca)
static Red entrenar(const Datos& D, const Red* inicio, int epocas, uint32_t semilla, float& perdVal, float& aciertoVal)
{
	Red r;
	if (inicio && inicio->valida()) r = *inicio;
	else r.iniciar(envi("OCULTAS1", 64), envi("OCULTAS2", 32), semilla);
	const int n = D.n();
	std::vector<int> tr, va;
	for (int i = 0; i < n; i++) ((D.duelo[(size_t)i] % 10u) == 7u ? va : tr).push_back(i);   // (validacion: duelos enteros apartados)
	Entrenador opt;
	opt.lr = inicio ? 5e-4f : 1.5e-3f;
	opt.l2 = envf("L2", 1e-4f);
	Azar a(semilla);
	const int lote = 128;
	std::vector<float> xb((size_t)lote * RED_ENTRADAS), yb((size_t)lote);
	std::vector<float> Xv((size_t)va.size() * RED_ENTRADAS), yv(va.size());
	for (size_t k = 0; k < va.size(); k++) {
		std::copy(&D.X[(size_t)va[k] * RED_ENTRADAS], &D.X[(size_t)va[k] * RED_ENTRADAS] + RED_ENTRADAS, &Xv[k * RED_ENTRADAS]);
		yv[k] = D.y[(size_t)va[k]];
	}
	Red mejor = r;
	float mejorVal = perdidaRed(r, Xv.data(), yv.data(), (int)yv.size());
	int sinMejora = 0;
	for (int e = 0; e < epocas; e++) {
		barajar(tr, a);
		double suma = 0;
		int lotes = 0;
		for (size_t i = 0; i + lote <= tr.size(); i += lote) {
			for (int k = 0; k < lote; k++) {
				const int j = tr[i + (size_t)k];
				std::copy(&D.X[(size_t)j * RED_ENTRADAS], &D.X[(size_t)j * RED_ENTRADAS] + RED_ENTRADAS, &xb[(size_t)k * RED_ENTRADAS]);
				yb[(size_t)k] = D.y[(size_t)j];
			}
			suma += opt.paso(r, xb.data(), yb.data(), lote);
			lotes++;
		}
		if (e == epocas / 2) opt.lr *= 0.4f;
		const float pv = perdidaRed(r, Xv.data(), yv.data(), (int)yv.size());
		fprintf(stderr, "    epoca %d: perdida entrenamiento %.4f, validacion %.4f (%.0f s)\n", e + 1, lotes ? suma / lotes : 0.0, pv, ahora());
		if (pv < mejorVal - 1e-4f) mejorVal = pv, mejor = r, sinMejora = 0;
		else if (++sinMejora >= 3) break;
	}
	perdVal = mejorVal;
	int ok = 0;
	for (size_t k = 0; k < yv.size(); k++) {
		const float p = mejor.valor(&Xv[k * RED_ENTRADAS]);
		ok += (yv[k] == 0.5f) || ((p > 0.5f) == (yv[k] > 0.5f));
	}
	aciertoVal = yv.empty() ? 0.0f : (float)ok / (float)yv.size();
	// la escala: el logit de la red en unidades del plan (mismo desvio que evaluarHumana)
	double sz = 0, sz2 = 0, sv = 0, sv2 = 0;
	const int m = std::min(n, 60000);
	for (int i = 0; i < m; i++) {
		const int j = (int)((long long)i * n / m);
		const float z = std::max(-4.0f, std::min(4.0f, mejor.logit(&D.X[(size_t)j * RED_ENTRADAS])));
		sz += z, sz2 += (double)z * z, sv += D.v[(size_t)j], sv2 += (double)D.v[(size_t)j] * D.v[(size_t)j];
	}
	const double dz = std::sqrt(std::max(1e-6, sz2 / m - (sz / m) * (sz / m))), dv = std::sqrt(std::max(1e-6, sv2 / m - (sv / m) * (sv / m)));
	mejor.escala = (float)std::max(0.3, std::min(20.0, dv / dz));
	mejor.mezcla = envf("MEZCLA", 0.5f);
	return mejor;
}

// ------------------------------------------------------------------------------------------------- partido
struct Partido {
	int gana{ 0 }, pierde{ 0 }, empates{ 0 };
	double ms{ 0 }, msMax{ 0 };
	int decisiones{ 0 };
	float pct() const { return gana + pierde ? 100.0f * (float)gana / (float)(gana + pierde) : 50.0f; }
};
// la candidata (red "a"; nullptr: sin red) contra la campeona ("b"), "pares" pares de duelos con los mismos mazos
static Partido partido(const Base& b, const Red* ra, const Red* rb, int pares, int hilos, uint32_t semilla, float escala = 1.0f, bool bHumana = true)
{
	const std::string r = enParalelo(hilos, pares, [&](int k) {
		const uint32_t s = mezclar(semilla, (uint32_t)k * 7919u + 1u);
		std::string o;
		for (int lado = 0; lado < 2; lado++) {
			Lados L;
			L.humana[lado] = true;
			L.humana[1 - lado] = bHumana;
			L.red[lado] = ra;
			L.red[1 - lado] = rb;
			L.escala = escala;
			L.escalaLado[lado] = envf("ESCALA_A", 1.0f);
			const Salida x = jugar(b, s, k, L, nullptr);
			char buf[96];
			snprintf(buf, sizeof(buf), "%d %.3f %.3f %d\n", x.ganador < 0 ? -1 : x.ganador == 2 ? 2 : x.ganador == lado ? 1 : 0, x.ms[lado], x.msMax[lado], x.decisiones[lado]);
			o += buf;
		}
		return o;
	});
	Partido p;
	std::istringstream in(r);
	int g, dec;
	double ms, mx;
	while (in >> g >> ms >> mx >> dec) {
		if (g == 1) p.gana++;
		else if (g == 0) p.pierde++;
		else p.empates++;
		p.ms += ms, p.msMax = std::max(p.msMax, mx), p.decisiones += dec;
	}
	return p;
}

int main(int argc, char** argv)
{
	if (argc < 4) {
		fprintf(stderr, "uso: entrenar_ia_red <carpeta ui/> <carpeta datos/> <salida ia_red.txt> [generaciones]\n");
		return 2;
	}
	const std::string datos = argv[2], salida = argv[3];
	const int G = argc > 4 ? atoi(argv[4]) : 4;
	const int hilos = envi("HILOS", 10), nClasica = envi("DUELOS_CLASICA", 4000), nHumana = envi("DUELOS_HUMANA", 1200), pares = envi("PARES", 150);
	const float escalaDatos = envf("ESCALA_DATOS", 0.35f), umbral = envf("UMBRAL", 55.0f), umbral0 = envf("UMBRAL0", 50.0f);
	const int epocas = envi("EPOCAS", 12);
	const float lambdaTD = envf("LAMBDA", 1.0f);
	const bool contraSinRed = envi("CONTRA_SINRED", 0) != 0;   // (cada candidata contra la humana sin red: se queda la mejor)
	float mejorPct = 0.0f;
	Humana::horizonteRed = envi("HORIZONTE_RED", 0);
	Humana::priorRed = envf("PRIOR_RED", 0.0f);
	Humana::umbralObviaRed = envf("OBVIA_RED", 0.0f);
	Humana::mezclaPersonaje = envi("MEZCLA_PERSONAJE", 0) != 0;
	const int simsMinDatos = envi("SIMS_MIN_DATOS", 20);
	Base b;
	if (!b.leerCartas(argv[1]) || !b.leerRivales(datos + "rivales.txt")) return 1;
	b.leerRivales(datos + "personajes_rivales.txt");
	b.leerVariedad(datos);
	// (los indices estaticos del motor, antes de los fork: cada proceso los hereda ya hechos)
	for (const Rival& r : b.rivales) prepararRival(b, r, 1u, nullptr);
	fprintf(stderr, "entrenar_ia_red: %d rivales, %d procesos, %d generaciones (%.0f s)\n", (int)b.rivales.size(), hilos, G, ahora());
	// solo medir: PARTIDO_SOLO=<red.txt> (o "ninguna") contra CONTRA=<red.txt> | "ninguna" (la humana sin red) | "clasica"
	if (getenv("PARTIDO_SOLO")) {
		Red ra, rb;
		const std::string a = getenv("PARTIDO_SOLO"), c = getenv("CONTRA") ? getenv("CONTRA") : "ninguna";
		bool hayA = a != "ninguna" && ra.leer(a);
		if (a == "azar" || a == "cero") {   // (diagnostico: una red al azar, o una que da 0 siempre: solo cuentan los finales de verdad)
			ra.iniciar(64, 32, 4242u);
			if (a == "cero") std::fill(ra.w3.begin(), ra.w3.end(), 0.0f), ra.b3 = 0.0f;
			ra.escala = 3.0f;
			ra.mezcla = 1.0f;
			hayA = true;
		}
		const bool hayB = c != "ninguna" && c != "clasica" && rb.leer(c);
		if (getenv("MEZCLA_FORZADA")) Humana::mezclaRed = envf("MEZCLA_FORZADA", -1.0f);
		const Partido p = partido(b, hayA ? &ra : nullptr, hayB ? &rb : nullptr, pares, hilos, (uint32_t)envi("SEMILLA", 0xF00D), envf("ESCALA", 1.0f), c != "clasica");
		const float ic = 100.0f * 1.96f * std::sqrt(0.25f / (float)std::max(1, p.gana + p.pierde));
		fprintf(stderr, "PARTIDO %s (mezcla %s, horizonte con red %d) contra %s: gana %d, pierde %d, empates %d -> %.1f%% (+-%.1f); %.2f ms por decision, maximo %.1f ms (%.0f s)\n",
		        hayA ? a.c_str() : "sin red", getenv("MEZCLA_FORZADA") ? getenv("MEZCLA_FORZADA") : "del archivo", Humana::horizonteRed, c.c_str(), p.gana, p.pierde, p.empates,
		        p.pct(), ic, p.decisiones ? p.ms / p.decisiones : 0.0, p.msMax, ahora());
		return 0;
	}

	Datos D;
	Red campeona;
	bool hayCampeona = false;
	if (getenv("INICIAL") && campeona.leer(getenv("INICIAL"))) {
		hayCampeona = true;
		fprintf(stderr, "sigue desde %s (generacion %d)\n", getenv("INICIAL"), campeona.generacion);
	}
	// (DATOS_LEER: las muestras ya generadas, sin jugar la gen 0; DATOS_GUARDAR: guarda las que se generen)
	auto guardarMuestras = [&](const std::string& m) {
		if (!getenv("DATOS_GUARDAR")) return;
		FILE* o = fopen(getenv("DATOS_GUARDAR"), "ab");
		if (o) fwrite(m.data(), 1, m.size(), o), fclose(o);
	};
	bool saltarDatos = false;
	if (getenv("DATOS_LEER")) {
		std::ifstream in(getenv("DATOS_LEER"), std::ios::binary);
		std::stringstream ss;
		ss << in.rdbuf();
		D.agregar(ss.str());
		saltarDatos = D.n() > 0 && !getenv("NO_SALTAR");
		fprintf(stderr, "datos leidos de %s: %d posiciones\n", getenv("DATOS_LEER"), D.n());
	}
	// generacion 0: duelos de la IA clasica (rapidos: volumen) y de la humana sin red
	if (!hayCampeona && nClasica > 0 && !saltarDatos) {
		const std::string m = enParalelo(hilos, nClasica, [&](int k) {
			std::string s;
			jugar(b, mezclar(0xC1A5u, (uint32_t)k), k, Lados{}, &s);
			return s;
		});
		D.agregar(m);
		guardarMuestras(m);
		fprintf(stderr, "datos clasica: %d duelos -> %d posiciones (%.0f s)\n", nClasica, D.n(), ahora());
	}
	for (int gen = hayCampeona ? campeona.generacion + 1 : 0, hechas = 0; hechas < G; gen++, hechas++) {
		const Red* rc = hayCampeona ? &campeona : nullptr;
		const int antes = D.n();
		const std::string m = saltarDatos ? std::string() : enParalelo(hilos, nHumana, [&](int k) {
			std::string s;
			Humana::simsMin = simsMinDatos;   // (en el proceso hijo: datos mas rapidos)
			Humana::msMin = 25.0f;
			Lados L;
			L.humana[0] = L.humana[1] = true;
			L.red[0] = L.red[1] = rc;
			L.escala = escalaDatos;
			jugar(b, mezclar(0x4E7u + (uint32_t)gen * 104729u, (uint32_t)k), k, L, &s);
			return s;
		});
		saltarDatos = false;
		D.agregar(m);
		guardarMuestras(m);
		// (el buffer: como mucho ~900 mil posiciones; salen las mas viejas)
		const int tope = 900000;
		if (D.n() > tope) D.quitarViejas(D.n() - tope);
		fprintf(stderr, "GENERACION %d: datos de la humana %s: %d duelos -> +%d posiciones (total %d) (%.0f s)\n", gen, rc ? "con la campeona" : "sin red", nHumana,
		        D.n() - antes, D.n(), ahora());
		fprintf(stderr, "  (diagnostico: perdida de la evaluacion del plan sola, en logistica: %.4f)\n", perdidaPlan(D));
		float pv = 0, acierto = 0;
		Red cand = entrenar(D, rc, rc ? std::max(3, epocas / 2) : epocas, 1000u + (uint32_t)gen, pv, acierto);
		// TD(lambda) (LAMBDA < 1): los objetivos con la red recien entrenada y otra pasada; despues, el resultado otra vez
		if (lambdaTD < 1.0f) {
			for (int pasada = 0; pasada < 2; pasada++) {
				objetivosTD(D, cand, lambdaTD);
				const Red previa = cand;
				cand = entrenar(D, &previa, std::max(3, epocas / 3), 2000u + (uint32_t)gen * 7u + (uint32_t)pasada, pv, acierto);
			}
			D.y = D.res;
			fprintf(stderr, "  TD(%.2f): perdida (sobre los objetivos TD) %.4f\n", lambdaTD, pv);
		}
		cand.generacion = gen;
		cand.origen = "base";
		fprintf(stderr, "  candidata: perdida de validacion %.4f, acierto %.1f%%, escala %.2f, mezcla %.2f (%.0f s)\n", pv, 100.0f * acierto, cand.escala, cand.mezcla, ahora());
		cand.guardar(salida + ".candidata");
		const Partido p = partido(b, &cand, contraSinRed ? nullptr : rc, pares, hilos, 0xBA77u + (uint32_t)gen * 31u);
		const float pct = p.pct();
		const float ic = 100.0f * 1.96f * std::sqrt(0.25f / (float)std::max(1, p.gana + p.pierde));
		fprintf(stderr, "  PARTIDO gen %d contra %s: candidata gana %d, pierde %d, empates %d -> %.1f%% (+-%.1f); %.2f ms por decision, maximo %.1f ms (%.0f s)\n", gen,
		        rc ? ("la generacion " + std::to_string(campeona.generacion)).c_str() : "la humana sin red", p.gana, p.pierde, p.empates, pct, ic,
		        p.decisiones ? p.ms / p.decisiones : 0.0, p.msMax, ahora());
		const float exigido = contraSinRed ? std::max(umbral0, mejorPct + 0.01f) : rc ? umbral : umbral0;   // (la primera: no ser mas debil que la humana sin red; despues, ganarle a la campeona)
		if (pct >= exigido) {
			campeona = cand;
			hayCampeona = true;
			mejorPct = pct;
			campeona.guardar(salida);
			fprintf(stderr, "  -> se queda: %s (generacion %d)\n", salida.c_str(), gen);
		} else {
			fprintf(stderr, "  -> no llega al %.0f%%: sigue la campeona%s\n", exigido, hayCampeona ? "" : " (ninguna todavia)");
			if (!hayCampeona && getenv("GUARDAR_MEJOR")) cand.guardar(salida);
		}
	}
	fprintf(stderr, "FIN (%.0f s): %s\n", ahora(), hayCampeona ? ("campeona generacion " + std::to_string(campeona.generacion)).c_str() : "sin campeona");
	return 0;
}
