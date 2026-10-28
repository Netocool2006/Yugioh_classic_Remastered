// Prueba de LA RED QUE APRENDE (rama classic-ia-red; src/ia_red.h, src/aprendizaje.h, docs/ia_red.md), sin pantalla.
// Casos (docs/casos_de_uso.md; los da casos_suites.txt si esta prueba termina sin fallas):
//   CU-BE-IA-RED-CARGA       la red base (datos/ia_red.txt) se carga, da probabilidades validas, ida y vuelta del texto
//                            igual y evalua en menos de 1 ms (rasgos + red)
//   CU-BE-IA-RED-CORRUPTO    pesos corruptos, cortados o de otra forma, y duelos corruptos: se descartan (vuelve la base)
//   CU-BE-IA-RED-GRABACION   los duelos se graban con tope (los ultimos 40) y se leen igual
//   CU-BE-IA-RED-AJUSTE      el ajuste fino por rebanadas mejora la prediccion sobre duelos apartados de ESE jugador
//                            (un jugador con estilo propio) y cada rebanada es corta
//   CU-BE-IA-RED-APAGADA     con la opcion en No: sin red, sin grabar, sin ajustar (la IA de antes)
//   CU-BE-IA-RED-FUERZA      humana con red contra humana sin red (pares con los mismos mazos) y contra la clasica
//   CU-BE-IA-RED-TIEMPO      con red dentro del presupuesto por decision
//   CU-BE-IA-RED-DIVERSIDAD  con red sigue jugando distinto entre duelos (la metrica de prueba_ia_humana)
// Compilar y correr (contenedor arch-ps5; minutos):
//   g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_ia_red.cpp -o /tmp/prueba_ia_red &&
//   /tmp/prueba_ia_red <carpeta ui/ con cartas_ui.txt> remake-ps5/datos/ [pares de duelos] [carpeta temporal]
#include "ia.h"

#include <sys/stat.h>

#include <set>

using namespace ia;

static int fallos = 0;
static void caso(const char* id, bool ok, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
static void caso(const char* id, bool ok, const char* fmt, ...)
{
	char b[600];
	va_list a;
	va_start(a, fmt);
	vsnprintf(b, sizeof(b), fmt, a);
	va_end(a);
	fprintf(stderr, "%s %s: %s\n", ok ? "  ok   " : "FALLA ", id, b);
	if (!ok) fallos++;
}
static bool existe(const std::string& r)
{
	struct stat st{};
	return stat(r.c_str(), &st) == 0;
}
static std::string leer(const std::string& r)
{
	std::ifstream f(r, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
static double ms(std::chrono::steady_clock::time_point t0)
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// un duelo. jugador: el lado 0 con un estilo fijo (personalidad, rival y nivel) si "jugadorFijo"; ap: graba el duelo
struct Config {
	bool humana[2]{ false, false };
	const Red* red[2]{ nullptr, nullptr };
	const Rival* fijo0{ nullptr };
	const char* persFija{ nullptr };
	int nivel{ -1 };
	float escala{ 1.0f };
};
struct Res {
	int ganador{ -1 }, rechazadas{ 0 };
	Humana::Estadisticas st[2];
};
static Res jugar(const Base& b, uint32_t s, int k, const Config& c, aprendizaje::Aprendiz* ap)
{
	const int nivel = c.nivel > 0 ? c.nivel : 1 + k % NIVEL_MAX;
	Azar az(mezclar(s, 991u));
	std::vector<const Rival*> deEse;
	for (const Rival& x : b.rivales)
		if (x.nivel == nivel) deEse.push_back(&x);
	if (deEse.empty())
		for (const Rival& x : b.rivales) deEse.push_back(&x);
	const Rival& ra = c.fijo0 ? *c.fijo0 : *deEse[(size_t)az.hasta((int)deEse.size())];
	const Rival& rb = *deEse[(size_t)az.hasta((int)deEse.size())];
	const MazoRival a = prepararRival(b, ra, c.fijo0 ? 7u : s, nullptr), m1 = prepararRival(b, rb, s ^ 0x55u, nullptr);
	Duelo d(&b);
	d.empezar(a.principal, a.fusion, m1.principal, m1.fusion, s);
	Turno t0(&d, 0, c.persFija ? personalidadPorClave(c.persFija) : ra.personalidad(), ra.nivel, s), t1(&d, 1, rb.personalidad(), rb.nivel, s + 1);
	t0.cerebro.usarPlan(c.fijo0 ? -1 : a.plan, Lectura{}, 0, 1);
	t1.cerebro.usarPlan(m1.plan, Lectura{}, 0, 1);
	t0.usarHumana(c.humana[0], ra.clave, s);
	t1.usarHumana(c.humana[1], rb.clave, s + 1);
	t0.humana.red = c.red[0];
	t1.humana.red = c.red[1];
	t0.humana.escala = t1.humana.escala = c.escala;
	if (ap) ap->empezarDuelo();
	int pasos = 0;
	while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
		if (ap) ap->mirar(d, t1.cerebro.plan.activo() ? t1.cerebro.plan.plan : -1);
		(d.decide() == 0 ? t0 : t1).paso();
		pasos++;
	}
	if (ap) ap->terminarDuelo(d.ganador);
	Res r;
	r.ganador = d.ganador;
	r.rechazadas = t0.rechazadas + t1.rechazadas;
	r.st[0] = t0.humana.st;
	r.st[1] = t1.humana.st;
	return r;
}

// pares con los mismos mazos: la IA "a" (humana con red ra) contra "b" (humana con rb, o la clasica)
struct Marcador {
	int gana{ 0 }, pierde{ 0 }, otros{ 0 }, rechazadas{ 0 }, decisiones{ 0 }, conRed{ 0 };
	double ms{ 0 }, msMax{ 0 };
	float pct() const { return gana + pierde ? 100.0f * (float)gana / (float)(gana + pierde) : 50.0f; }
};
static Marcador partido(const Base& b, const Red* ra, const Red* rb, bool bHumana, int pares, uint32_t semilla)
{
	Marcador m;
	for (int k = 0; k < pares; k++) {
		const uint32_t s = mezclar(semilla, (uint32_t)k * 7919u + 1u);
		for (int lado = 0; lado < 2; lado++) {
			Config c;
			c.humana[lado] = true;
			c.humana[1 - lado] = bHumana;
			c.red[lado] = ra;
			c.red[1 - lado] = rb;
			const Res r = jugar(b, s, k, c, nullptr);
			if (r.ganador == lado) m.gana++;
			else if (r.ganador == 1 - lado) m.pierde++;
			else m.otros++;
			m.rechazadas += r.rechazadas;
			m.decisiones += r.st[lado].decisiones;
			m.conRed += r.st[lado].conRed;
			m.ms += r.st[lado].ms;
			m.msMax = std::max(m.msMax, r.st[lado].msMax);
		}
	}
	return m;
}

// diversidad (como prueba_ia_humana): el rival con red N veces seguidas con la memoria contra un jugador fijo
static float diversidad(const Base& b, const Red* red, int N)
{
	const char* const PRUEBA[] = { "joey", "kaiba", "pegasus", "yami_bakura", "yugi", "marik" };
	const Rival* jugador = b.rival("rodrigo");
	if (!jugador) jugador = &b.rivales[0];
	const MazoRival mj = b.mazoDe(*jugador, 99);
	int aper = 0, tot = 0, nr = 0;
	for (const char* clave : PRUEBA) {
		const Rival* r = b.rival(clave);
		if (!r) continue;
		nr++;
		Memoria mem;
		std::set<std::string> aperturas;
		for (int k = 0; k < N; k++) {
			const uint32_t s = mezclar(0xC0FFEEu, (uint32_t)(k * 131 + nr));
			const MazoRival mr = prepararRival(b, *r, s, &mem);
			Duelo d(&b);
			d.empezar(mj.principal, mj.fusion, mr.principal, mr.fusion, s);
			Turno tj(&d, 0, jugador->personalidad(), jugador->nivel, s), tr(&d, 1, r->personalidad(), r->nivel, s + 1);
			tr.cerebro.usarPlan(mr.plan, mem.jugador, mem.dificultad(), mem.factorRuido());
			tr.usarHumana(true, r->clave, s + 1);
			tr.humana.red = red;
			std::string ap;
			int turnosPropios = 0, ultimoTurno = -1, pasos = 0;
			while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
				const bool rival = d.decide() == 1;
				Turno& t = rival ? tr : tj;
				t.paso();
				if (rival && d.activo == 1 && t.ultima.tipo != J_FASE && t.ultima.tipo != J_PASAR && t.ultima.tipo != J_ELEGIR) {
					if (d.turno != ultimoTurno) ultimoTurno = d.turno, turnosPropios++;
					if (turnosPropios <= 2) ap += std::string(NOMBRE_JUGADA[t.ultima.tipo]) + ":" + t.ultima.carta + ":" + std::to_string(t.ultima.objetivo) + " ";
				}
				pasos++;
			}
			aperturas.insert(ap);
			EstadoPlan& ep = tr.cerebro.plan;
			ep.obs.mirar(d, 0);
			mem.registrar(r->clave, ep.planesDuelo, mr.principal, ep.obs, d.ganador);
		}
		aper += (int)aperturas.size();
		tot += N;
	}
	return tot ? 100.0f * (float)aper / (float)tot : 0.0f;
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_ia_red <carpeta ui/> <carpeta datos/> [pares de duelos] [carpeta temporal]\n");
		return 2;
	}
	const std::string datos = argv[2];
	const int P = argc > 3 ? atoi(argv[3]) : 20;
	const std::string tmp = (argc > 4 ? std::string(argv[4]) : std::string("/tmp/prueba_ia_red_") + std::to_string((int)getpid())) + "/";
	mkdir(tmp.c_str(), 0777);
	const auto inicio = std::chrono::steady_clock::now();
	Base b;
	if (!b.leerCartas(argv[1]) || !b.leerRivales(datos + "rivales.txt")) return 1;
	b.leerRivales(datos + "personajes_rivales.txt");
	b.leerVariedad(datos);

	// ---- CU-BE-IA-RED-CARGA
	const std::string rutaBase = datos + "ia_red.txt";
	Red base;
	const bool cargo = base.leer(rutaBase);
	caso("CU-BE-IA-RED-CARGA", cargo && base.valida(), "%s: %s (%d-%d-%d, %d pesos, generacion %d, escala %.2f, mezcla %.2f)", rutaBase.c_str(), cargo ? "cargada" : "NO se pudo leer",
	     base.nE, base.n1, base.n2, (int)base.numPesos(), base.generacion, base.escala, base.mezcla);
	if (!cargo) {
		fprintf(stderr, "PRUEBA CON FALLOS (sin red base no hay mas que probar)\n");
		return 1;
	}
	{
		Red r2;
		caso("CU-BE-IA-RED-CARGA", r2.leerTexto(base.texto()) && r2.texto() == base.texto(), "el texto de la red va y vuelve igual");
		// posiciones de duelos de verdad: probabilidades validas y con sentido (el que tiene mas LP y cartas, mas)
		Duelo d(&b);
		const MazoRival m0 = b.mazoDe(b.rivales[0], 1), m1 = b.mazoDe(b.rivales[1], 2);
		d.empezar(m0.principal, m0.fusion, m1.principal, m1.fusion, 5u);
		const float p0 = base.valor(d, 0, -1), p1 = base.valor(d, 1, -1);
		Duelo ventaja = d;
		ventaja.l[1].lp = 600;
		ventaja.l[1].mano.resize(1);
		const float pv = base.valor(ventaja, 0, -1);
		caso("CU-BE-IA-RED-CARGA", p0 > 0.0f && p0 < 1.0f && p1 > 0.0f && p1 < 1.0f && pv > p0 + 0.1f,
		     "al empezar %.3f / %.3f; con el otro a 600 LP y 1 carta %.3f (debe subir)", p0, p1, pv);
		const auto t0 = std::chrono::steady_clock::now();
		float suma = 0.0f;
		const int N = 20000;
		for (int i = 0; i < N; i++) suma += base.valor(ventaja, i & 1, i % 10 - 1);
		const double us = 1000.0 * ms(t0) / N;
		caso("CU-BE-IA-RED-CARGA", us < 1000.0, "rasgos + red: %.2f microsegundos por evaluacion (%.1f)", us, suma);
	}

	// ---- CU-BE-IA-RED-CORRUPTO
	{
		const std::string bueno = base.texto();
		std::string malo = bueno;
		malo[malo.size() / 2] = malo[malo.size() / 2] == '1' ? '2' : '1';
		Red r;
		caso("CU-BE-IA-RED-CORRUPTO", !r.leerTexto(malo), "un digito cambiado se rechaza");
		caso("CU-BE-IA-RED-CORRUPTO", !r.leerTexto(bueno.substr(0, bueno.size() / 2)), "un archivo cortado se rechaza");
		caso("CU-BE-IA-RED-CORRUPTO", !r.leerTexto(""), "un archivo vacio se rechaza");
		// la del jugador corrupta / de otra forma: se usa la base
		const std::string dir = tmp + "corrupto/";
		mkdir(dir.c_str(), 0777);
		guardado::guardarAtomico(dir + "ia_red_jugador.txt", malo);
		guardado::guardarAtomico(dir + "ia_duelos.bin", "YCRD basura");
		aprendizaje::Aprendiz ap;
		ap.preparar(rutaBase, dir);
		caso("CU-BE-IA-RED-CORRUPTO", ap.red() == &ap.base && !ap.hayJugador && ap.st.jugadorCorrupto, "con la red del jugador corrupta se usa la base");
		caso("CU-BE-IA-RED-CORRUPTO", ap.grabados.duelos.empty() && ap.st.duelosCorruptos, "los duelos corruptos se descartan");
		Red otra;
		otra.iniciar(16, 8, 3u);
		otra.guardar(dir + "ia_red_jugador.txt");
		ap.leerJugador();
		caso("CU-BE-IA-RED-CORRUPTO", ap.red() == &ap.base && !ap.hayJugador, "una red del jugador de otra forma (16-8) se rechaza");
		Red buena = base;
		buena.origen = "jugador";
		buena.guardar(dir + "ia_red_jugador.txt");
		ap.leerJugador();
		caso("CU-BE-IA-RED-CORRUPTO", ap.red() == &ap.jugador && ap.hayJugador, "una red del jugador buena se usa");
		aprendizaje::Aprendiz sinBase;
		sinBase.preparar(dir + "no_existe.txt", dir);
		caso("CU-BE-IA-RED-CORRUPTO", sinBase.red() == nullptr, "sin red base, la IA queda sin red (juega como antes)");
		// un duelo grabado con un byte cambiado
		aprendizaje::Archivo ar;
		aprendizaje::DueloGrabado dg;
		dg.ganador = 1;
		dg.agregar(Duelo(&b), 0, -1);
		dg.agregar(Duelo(&b), 1, 2);
		ar.duelos.push_back(dg);
		std::string bin = ar.serializar();
		aprendizaje::Archivo ar2;
		const bool vuelve = ar2.deserializar(bin) && ar2.duelos.size() == 1 && ar2.duelos[0].pos == dg.pos;
		bin[20] ^= 0x40;
		caso("CU-BE-IA-RED-CORRUPTO", vuelve && !ar2.deserializar(bin) && ar2.duelos.empty(), "ia_duelos.bin: ida y vuelta %d y un byte cambiado se rechaza", (int)vuelve);
	}

	// ---- CU-BE-IA-RED-GRABACION y CU-BE-IA-RED-AJUSTE: un JUGADOR con estilo propio (siempre el mismo mazo, y FUERTE:
	// juega con la IA humana) contra rivales variados de nivel 3 con la clasica; 45 duelos grabados (quedan 40), otros
	// 30 apartados para medir. La red base no sabe lo fuerte que es: el ajuste tiene que aprenderlo
	const std::string dirJ = tmp + "jugador/";
	mkdir(dirJ.c_str(), 0777);
	remove((dirJ + "ia_duelos.bin").c_str());
	remove((dirJ + "ia_red_jugador.txt").c_str());
	{
		aprendizaje::Aprendiz ap;
		ap.preparar(rutaBase, dirJ);
		const Rival* fijo = b.rival("rex_raptor");
		if (!fijo) fijo = &b.rivales[3];
		Config c;
		c.fijo0 = fijo;
		c.humana[0] = true;
		c.nivel = 3;
		Humana::simsMin = 16;   // (el jugador piensa rapido: la prueba no se alarga)
		Humana::msMin = 15.0f;
		c.escala = 0.2f;
		const auto tg = std::chrono::steady_clock::now();
		int maxPos = 0;
		for (int k = 0; k < 45; k++) {
			jugar(b, mezclar(0xA11u, (uint32_t)k), k, c, &ap);
			if (!ap.grabados.duelos.empty()) maxPos = std::max(maxPos, ap.grabados.duelos.back().n());
			if (k < 44) ap.aj.pendiente = false;   // (el ajuste, al final)
		}
		aprendizaje::Archivo rel;
		const bool releido = rel.leer(ap.rutaDuelos());
		caso("CU-BE-IA-RED-GRABACION", (int)ap.grabados.duelos.size() == aprendizaje::MAX_DUELOS && releido && rel.duelos.size() == ap.grabados.duelos.size() &&
		                                   rel.serializar() == ap.grabados.serializar(),
		     "45 duelos -> %d grabados (tope %d), releidos %d, %d bytes en disco (%.0f ms)", (int)ap.grabados.duelos.size(), aprendizaje::MAX_DUELOS, (int)rel.duelos.size(),
		     (int)leer(ap.rutaDuelos()).size(), ms(tg));
		caso("CU-BE-IA-RED-GRABACION", maxPos > 4 && maxPos <= aprendizaje::MAX_POS_DUELO, "posiciones por duelo %d (tope %d)", maxPos, aprendizaje::MAX_POS_DUELO);
		// los apartados (los mismos estilos, otras semillas)
		aprendizaje::Aprendiz apMedir;
		apMedir.carpeta = tmp + "apartados/";
		mkdir(apMedir.carpeta.c_str(), 0777);
		apMedir.base = base;
		for (int k = 0; k < 30; k++) {
			jugar(b, mezclar(0xB22u, (uint32_t)k), k, c, &apMedir);
		}
		Humana::simsMin = 60;
		Humana::msMin = 80.0f;
		std::vector<float> Xv, yv;
		apMedir.grabados.conjunto(Xv, yv, 0, 0, false, 1);
		const float antes = perdidaRed(base, Xv.data(), yv.data(), (int)yv.size());
		// el ajuste por rebanadas de 1,5 ms (como en el juego)
		int rebanadas = 0;
		const auto ta = std::chrono::steady_clock::now();
		while (ap.avanzar(1.5)) rebanadas++;
		const double total = ms(ta);
		const Red* r = ap.red();
		const float despues = perdidaRed(*r, Xv.data(), yv.data(), (int)yv.size());
		caso("CU-BE-IA-RED-AJUSTE", ap.hayJugador && r == &ap.jugador && existe(ap.rutaJugador()),
		     "el ajuste deja la red del jugador (perdida de validacion interna %.4f -> %.4f)", ap.st.perdidaBase, ap.st.perdidaNueva);
		caso("CU-BE-IA-RED-AJUSTE", despues < antes - 0.002f, "en %d posiciones apartadas del mismo jugador: perdida base %.4f -> ajustada %.4f", (int)yv.size(), antes, despues);
		caso("CU-BE-IA-RED-AJUSTE", ap.st.msMax < 6.0, "%d rebanadas, %.0f ms en total, la mas larga %.2f ms (pedido 1,5; tope 6) y el guardado %.1f ms", rebanadas, total, ap.st.msMax, ap.st.msGuardar);
		aprendizaje::Aprendiz ap2;
		ap2.preparar(rutaBase, dirJ);
		caso("CU-BE-IA-RED-AJUSTE", ap2.hayJugador && ap2.red() == &ap2.jugador && ap2.jugador.texto() == ap.jugador.texto(), "al volver a abrir se lee la red del jugador");
		ap2.olvidar();
		caso("CU-BE-IA-RED-AJUSTE", !existe(ap2.rutaJugador()) && !existe(ap2.rutaDuelos()) && ap2.red() == &ap2.base, "olvidar borra lo aprendido");
	}

	// ---- CU-BE-IA-RED-APAGADA
	{
		const std::string dir = tmp + "apagada/";
		mkdir(dir.c_str(), 0777);
		aprendizaje::Aprendiz ap;
		ap.preparar(rutaBase, dir);
		ap.activo = false;
		Config c;
		c.humana[1] = true;
		c.red[1] = ap.red();
		const Res r = jugar(b, 99u, 3, c, &ap);
		caso("CU-BE-IA-RED-APAGADA", ap.red() == nullptr && r.st[1].conRed == 0 && r.st[1].buscadas > 0, "apagada: red %p, finales con red %d (de %d decisiones imaginadas)",
		     (const void*)ap.red(), r.st[1].conRed, r.st[1].buscadas);
		caso("CU-BE-IA-RED-APAGADA", !existe(ap.rutaDuelos()) && ap.grabados.duelos.empty() && !ap.avanzar(10.0), "apagada no graba ni ajusta");
		ap.activo = true;
		Config c2 = c;
		c2.red[1] = ap.red();
		const Res r2 = jugar(b, 99u, 3, c2, nullptr);
		caso("CU-BE-IA-RED-APAGADA", r2.st[1].conRed > 0, "encendida, la busqueda usa la red (%d finales)", r2.st[1].conRed);
	}

	// ---- CU-BE-IA-RED-FUERZA / TIEMPO
	{
		const auto tf = std::chrono::steady_clock::now();
		const Marcador sinRed = partido(b, &base, nullptr, true, P, 0x5EEDu);
		const float ic = 100.0f * 1.96f * std::sqrt(0.25f / (float)std::max(1, sinRed.gana + sinRed.pierde));
		fprintf(stderr, "CON RED vs SIN RED: %d pares: gana %d, pierde %d, otros %d -> %.1f%% (+-%.1f) (%.0f s)\n", P, sinRed.gana, sinRed.pierde, sinRed.otros, sinRed.pct(), ic,
		        ms(tf) / 1000.0);
		caso("CU-BE-IA-RED-FUERZA", sinRed.pct() >= 40.0f && sinRed.rechazadas == 0, "con red contra sin red %.1f%% (no debe ser claramente peor), rechazadas %d", sinRed.pct(),
		     sinRed.rechazadas);
		const Marcador clas = partido(b, &base, nullptr, false, P, 0xC1A5u);
		fprintf(stderr, "CON RED vs CLASICA: %d pares: gana %d, pierde %d, otros %d -> %.1f%% (%.0f s)\n", P, clas.gana, clas.pierde, clas.otros, clas.pct(), ms(tf) / 1000.0);
		caso("CU-BE-IA-RED-FUERZA", clas.pct() >= 55.0f && clas.rechazadas == 0, "con red contra la clasica %.1f%% (la humana sin red: ~65%%)", clas.pct());
		const double medio = sinRed.decisiones ? sinRed.ms / sinRed.decisiones : 0.0;
		fprintf(stderr, "TIEMPO con red: %d decisiones, %.2f ms medios, maximo %.1f ms, %d finales evaluados con la red\n", sinRed.decisiones, medio, sinRed.msMax, sinRed.conRed);
		caso("CU-BE-IA-RED-TIEMPO", std::max(sinRed.msMax, clas.msMax) <= 175.0 && sinRed.conRed > 0, "maximo por decision %.1f ms (tope 150 + 25)", std::max(sinRed.msMax, clas.msMax));
	}

	// ---- CU-BE-IA-RED-DIVERSIDAD
	{
		const int N = 10;
		const float sin = diversidad(b, nullptr, N), con = diversidad(b, &base, N);
		fprintf(stderr, "DIVERSIDAD (6 rivales x %d duelos con memoria): aperturas distintas sin red %.0f%%, con red %.0f%%\n", N, sin, con);
		caso("CU-BE-IA-RED-DIVERSIDAD", con + 10.0f >= sin, "con red %.0f%% de aperturas distintas, sin red %.0f%%", con, sin);
	}

	fprintf(stderr, "(%.0f s) %s (%d fallos)\n", ms(inicio) / 1000.0, fallos ? "PRUEBA CON FALLOS" : "PRUEBA OK", fallos);
	return fallos ? 1 : 0;
}
