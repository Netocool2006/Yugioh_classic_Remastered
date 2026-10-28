// Prueba de la IA HUMANA (rama classic-ia-humana; src/ia_humana.h), sin pantalla:
//  1. IA humana contra IA clasica con los MISMOS mazos: cada par de duelos usa la misma semilla y los dos mazos, y
//     cambia quien juega cada mazo (cada IA juega una vez cada mazo y cada lado). % de victorias, jugadas rechazadas,
//     duelos sin terminar.
//  2. El presupuesto de tiempo: ms medios por decision (todas y las que imaginan), el maximo y las simulaciones.
//  3. La diversidad (la metrica de prueba_variedad): el mismo rival N veces seguidas con la memoria, % de primeros
//     turnos distintos y de "aperturas" (los 2 primeros turnos propios) distintas, clasica frente a humana.
//  4. La capa humana: faroles, trampas calladas/guardadas y errores creibles por nivel.
// Compilar y correr (contenedor arch-ps5; minutos):
//   g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_ia_humana.cpp -o /tmp/prueba_ia_humana &&
//   /tmp/prueba_ia_humana <carpeta ui/ con cartas_ui.txt> remake-ps5/datos/ [pares de duelos] [escala del presupuesto]
#include "ia.h"

#include <map>
#include <set>

using namespace ia;

struct Totales {
	Humana::Estadisticas st;
	int duelos{ 0 }, gana{ 0 }, pierde{ 0 }, empates{ 0 }, sinFin{ 0 }, rechazadas{ 0 };
	std::map<int, std::pair<int, int>> porNivel;   // nivel -> (gana, duelos)
	std::map<int, std::pair<int, int>> errPorNivel;   // nivel -> (errores, decisiones buscadas)
	void sumar(const Humana::Estadisticas& s)
	{
		st.decisiones += s.decisiones, st.buscadas += s.buscadas, st.sims += s.sims, st.errores += s.errores, st.faroles += s.faroles;
		st.ocultas += s.ocultas, st.guardadas += s.guardadas, st.ms += s.ms, st.msBuscadas += s.msBuscadas, st.msMax = std::max(st.msMax, s.msMax);
	}
};

// un duelo: "humanaEn" = el lado que juega la IA humana (-1: ninguno)
static int duelo(const Base& b, const MazoRival& m0, const Rival& r0, const MazoRival& m1, const Rival& r1, uint32_t s, int humanaEn, float escala,
                 Totales& t)
{
	Duelo d(&b);
	d.empezar(m0.principal, m0.fusion, m1.principal, m1.fusion, s);
	Turno t0(&d, 0, r0.personalidad(), r0.nivel, s), t1(&d, 1, r1.personalidad(), r1.nivel, s + 1);
	t0.cerebro.usarPlan(m0.plan, Lectura{}, 0, 1);
	t1.cerebro.usarPlan(m1.plan, Lectura{}, 0, 1);
	t0.usarHumana(humanaEn == 0, r0.clave, s);
	t1.usarHumana(humanaEn == 1, r1.clave, s + 1);
	t0.humana.escala = t1.humana.escala = escala;
	int pasos = 0;
	while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
		(d.decide() == 0 ? t0 : t1).paso();
		pasos++;
	}
	t.duelos++;
	t.sinFin += d.ganador < 0;
	t.rechazadas += t0.rechazadas + t1.rechazadas;
	if (humanaEn >= 0) {
		const Turno& th = humanaEn == 0 ? t0 : t1;
		t.sumar(th.humana.st);
		const int nv = th.cerebro.nivel;
		t.porNivel[nv].second++;
		t.porNivel[nv].first += d.ganador == humanaEn;
		t.errPorNivel[nv].first += th.humana.st.errores;
		t.errPorNivel[nv].second += th.humana.st.buscadas;
		if (d.ganador == humanaEn) t.gana++;
		else if (d.ganador == 1 - humanaEn) t.pierde++;
		else t.empates++;
	}
	return d.ganador;
}

// diversidad: el rival (lado 1) contra un jugador fijo (IA clasica), N veces seguidas con la memoria
struct Div {
	float primeros{ 0 }, aperturas{ 0 };
};
static Div diversidad(const Base& b, bool humana, int N)
{
	const char* const PRUEBA[] = { "joey", "kaiba", "pegasus", "yami_bakura", "yugi", "marik" };
	const Rival* jugador = b.rival("rodrigo");
	if (!jugador) jugador = &b.rivales[0];
	const MazoRival mj = b.mazoDe(*jugador, 99);
	int prim = 0, aper = 0, tot = 0;
	int nr = 0;
	for (const char* clave : PRUEBA) {
		const Rival* r = b.rival(clave);
		if (!r) continue;
		nr++;
		Memoria mem;
		std::set<std::string> primeros, aperturas;
		for (int k = 0; k < N; k++) {
			const uint32_t s = mezclar(0xC0FFEEu, (uint32_t)(k * 131 + nr));
			const MazoRival mr = prepararRival(b, *r, s, &mem);
			Duelo d(&b);
			d.empezar(mj.principal, mj.fusion, mr.principal, mr.fusion, s);
			Turno tj(&d, 0, jugador->personalidad(), jugador->nivel, s), tr(&d, 1, r->personalidad(), r->nivel, s + 1);
			tr.cerebro.usarPlan(mr.plan, mem.jugador, mem.dificultad(), mem.factorRuido());
			tr.usarHumana(humana, r->clave, s + 1);
			std::string p1, ap;
			int turnosPropios = 0, ultimoTurno = -1, pasos = 0;
			while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
				const bool rival = d.decide() == 1;
				Turno& t = rival ? tr : tj;
				t.paso();
				if (rival && d.activo == 1 && t.ultima.tipo != J_FASE && t.ultima.tipo != J_PASAR && t.ultima.tipo != J_ELEGIR) {
					if (d.turno != ultimoTurno) ultimoTurno = d.turno, turnosPropios++;
					const std::string x = std::string(NOMBRE_JUGADA[t.ultima.tipo]) + ":" + t.ultima.carta + ":" + std::to_string(t.ultima.objetivo) + " ";
					if (turnosPropios == 1) p1 += x;
					if (turnosPropios <= 2) ap += x;
				}
				pasos++;
			}
			primeros.insert(p1);
			aperturas.insert(ap);
			EstadoPlan& ep = tr.cerebro.plan;
			ep.obs.mirar(d, 0);
			mem.registrar(r->clave, ep.planesDuelo, mr.principal, ep.obs, d.ganador);
		}
		prim += (int)primeros.size();
		aper += (int)aperturas.size();
		tot += N;
	}
	Div dv;
	dv.primeros = tot ? 100.0f * (float)prim / (float)tot : 0.0f;
	dv.aperturas = tot ? 100.0f * (float)aper / (float)tot : 0.0f;
	return dv;
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_ia_humana <carpeta ui/> <carpeta datos/> [pares de duelos] [escala]\n");
		return 2;
	}
	const int P = argc > 3 ? atoi(argv[3]) : 100;
	const float escala = argc > 4 ? (float)atof(argv[4]) : 1.0f;
	const std::string datos = argv[2];
	if (getenv("DEPURA")) Humana::depura = atoi(getenv("DEPURA"));
	if (getenv("KPRIOR")) Humana::kPrior = (float)atof(getenv("KPRIOR"));
	if (getenv("NIVELSIM")) Humana::nivelSim = atoi(getenv("NIVELSIM"));
	if (getenv("OBVIA")) Humana::umbralObvia = (float)atof(getenv("OBVIA"));
	if (getenv("HORIZONTE")) Humana::horizonte = atoi(getenv("HORIZONTE"));
	if (getenv("WCARTAS")) pesoCartasHumana() = (float)atof(getenv("WCARTAS"));
	if (getenv("SIN_DIVERSIDAD")) fprintf(stderr, "(sin la parte de diversidad)\n");
	Base b;
	if (!b.leerCartas(argv[1]) || !b.leerRivales(datos + "rivales.txt")) return 1;
	b.leerRivales(datos + "personajes_rivales.txt");
	b.leerVariedad(datos);
	int fallos = 0;
	const auto inicio = std::chrono::steady_clock::now();

	// 1) humana contra clasica, mismos mazos, cambiando quien juega cada mazo
	Totales t;
	Azar az(20261019u);
	std::map<std::string, std::pair<int, int>> porRival;
	for (int k = 0; k < P; k++) {
		const uint32_t s = az.siguiente();
		// los dos rivales del MISMO nivel (1..8 por turnos): la tabla por nivel compara a las dos IA en igualdad
		const int nivel = 1 + k % NIVEL_MAX;
		std::vector<const Rival*> deEse;
		for (const Rival& x : b.rivales)
			if (x.nivel == nivel) deEse.push_back(&x);
		if (deEse.empty())
			for (const Rival& x : b.rivales) deEse.push_back(&x);
		const Rival& ra = *deEse[(size_t)az.hasta((int)deEse.size())];
		const Rival& rb = *deEse[(size_t)az.hasta((int)deEse.size())];
		const MazoRival a = prepararRival(b, ra, s, nullptr), c = prepararRival(b, rb, s ^ 0x55u, nullptr);
		// mazo A en el lado 0 y B en el 1; la humana juega A, luego la humana juega B (mismo reparto, misma semilla)
		const int g1 = duelo(b, a, ra, c, rb, s, 0, escala, t);
		const int g2 = duelo(b, a, ra, c, rb, s, 1, escala, t);
		porRival[ra.clave].first += g1 == 0, porRival[ra.clave].second++;
		porRival[rb.clave].first += g2 == 1, porRival[rb.clave].second++;
		if ((k + 1) % 25 == 0)
			fprintf(stderr, "  ... %d pares: humana %d - %d clasica (%.0f s)\n", k + 1, t.gana, t.pierde,
			        std::chrono::duration<double>(std::chrono::steady_clock::now() - inicio).count());
	}
	const int decididos = t.gana + t.pierde;
	const float pct = decididos ? 100.0f * (float)t.gana / (float)decididos : 0.0f;
	fprintf(stderr, "HUMANA vs CLASICA: %d duelos (%d pares con los mismos mazos): humana gana %d, clasica %d, empates/sin terminar %d -> %.1f%% de victorias\n",
	        t.duelos, P, t.gana, t.pierde, t.empates, pct);
	fprintf(stderr, "  sin terminar %d, jugadas rechazadas %d\n", t.sinFin, t.rechazadas);
	fprintf(stderr, "  por nivel de la humana:");
	for (auto& kv : t.porNivel) fprintf(stderr, " n%d %d%% (%d)", kv.first, kv.second.second ? 100 * kv.second.first / kv.second.second : 0, kv.second.second);
	fprintf(stderr, "\n");
	const Humana::Estadisticas& st = t.st;
	fprintf(stderr, "TIEMPO: %d decisiones, %d imaginadas (%.0f%%), %.2f ms medios por decision, %.1f ms medios por decision imaginada, maximo %.1f ms, "
	                "%.0f simulaciones por decision imaginada\n",
	        st.decisiones, st.buscadas, st.decisiones ? 100.0 * st.buscadas / st.decisiones : 0.0, st.decisiones ? st.ms / st.decisiones : 0.0,
	        st.buscadas ? st.msBuscadas / st.buscadas : 0.0, st.msMax, st.buscadas ? (double)st.sims / st.buscadas : 0.0);
	fprintf(stderr, "CAPA HUMANA: faroles %d, trampas calladas %d, trampas guardadas para el ataque grande %d, errores creibles %d (%.1f%% de las imaginadas)\n",
	        st.faroles, st.ocultas, st.guardadas, st.errores, st.buscadas ? 100.0 * st.errores / st.buscadas : 0.0);
	fprintf(stderr, "  errores por nivel:");
	for (auto& kv : t.errPorNivel) fprintf(stderr, " n%d %.1f%%", kv.first, kv.second.second ? 100.0 * kv.second.first / kv.second.second : 0.0);
	fprintf(stderr, "\n");
	if (t.rechazadas) fallos++, fprintf(stderr, "FALLA: jugadas ilegales\n");
	if (t.sinFin > t.duelos / 50) fallos++, fprintf(stderr, "FALLA: demasiados duelos sin terminar\n");
	if (decididos >= 40 && pct < 55.0f) fallos++, fprintf(stderr, "FALLA: la humana no gana claramente\n");
	if (st.msMax > 150.0 + 25.0) fallos++, fprintf(stderr, "FALLA: se paso del presupuesto de tiempo\n");

	// 3) diversidad
	const int N = getenv("SIN_DIVERSIDAD") ? 0 : 10;
	const Div dc = diversidad(b, false, N), dh = diversidad(b, true, N);
	fprintf(stderr, "DIVERSIDAD (6 rivales x %d duelos con memoria): primeros turnos distintos clasica %.0f%%, humana %.0f%%; aperturas (2 turnos) distintas "
	                "clasica %.0f%%, humana %.0f%%\n",
	        N, dc.primeros, dh.primeros, dc.aperturas, dh.aperturas);
	if (dh.aperturas + 10.0f < dc.aperturas) fallos++, fprintf(stderr, "FALLA: la humana es mas predecible\n");

	fprintf(stderr, "(%.0f s) %s (%d fallos)\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - inicio).count(),
	        fallos ? "PRUEBA CON FALLOS" : "PRUEBA OK", fallos);
	return fallos ? 1 : 0;
}
