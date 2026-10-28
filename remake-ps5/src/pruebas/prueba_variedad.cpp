// Prueba de VARIEDAD del rival (rama classic-variedad; src/ia_plan.h), rapida y sin pantalla:
//  1. mazos dinamicos validos (40-60, limites Goat, fusiones con materiales, rituales con su magia) de todos los rivales
//  2. el mismo rival N veces seguidas (con la memoria entre duelos): entropia de planes, % de cartas distintas entre
//     mazos consecutivos, primeras jugadas distintas, cambios de plan a mitad de duelo (frente al sistema de antes)
//  3. la dificultad por nivel se mantiene (el de nivel alto gana mas con planes y mazos dinamicos)
//  4. la lectura del jugador: contra un "trampero" la IA ataca menos a ciegas que contra un "farolero"
//  5. la dificultad adaptable: tras rachas del jugador la IA juega mejor (gana mas) y se puede apagar
//  6. la memoria se guarda y se lee igual
// Compilar y correr (contenedor arch-ps5; segundos):
//   g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_variedad.cpp -o /tmp/prueba_variedad &&
//   /tmp/prueba_variedad <carpeta ui/ con cartas_ui.txt> remake-ps5/datos/ [duelos por rival]
#include "ia.h"

#include <map>
#include <set>

using namespace ia;

struct Resultado {
	int ganador{ -1 }, turnos{ 0 }, ciegos{ 0 }, oportCiegos{ 0 }, limpiezas{ 0 };
	std::string primerTurno;
	Observacion obs;
	std::vector<int> planes;
};

// lado 0: el "jugador" (IA sin plan, mazo fijo); lado 1: el rival con su plan
static Resultado jugar(const Base& b, const MazoRival& mj, const Rival& rj, const MazoRival& mr, const Rival& rr, uint32_t s, const Lectura& lec,
                       float dif, float factorRuido)
{
	Resultado res;
	Duelo d(&b);
	d.empezar(mj.principal, mj.fusion, mr.principal, mr.fusion, s);
	Turno tj(&d, 0, rj.personalidad(), rj.nivel, s), tr(&d, 1, rr.personalidad(), rr.nivel, s + 1);
	tr.cerebro.usarPlan(mr.plan, lec, dif, factorRuido);
	int pasos = 0, turnoPrimero = -1;
	while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
		const bool rival = d.decide() == 1;
		Turno& t = rival ? tr : tj;
		const int bocaAbajo = d.l[0].colocadasMT();
		const Fase f = d.fase;
		t.paso();
		if (rival && d.activo == 1 && t.ultima.tipo != J_FASE && t.ultima.tipo != J_PASAR && t.ultima.tipo != J_ELEGIR) {
			if (turnoPrimero < 0) turnoPrimero = d.turno;
			if (d.turno == turnoPrimero) res.primerTurno += std::string(NOMBRE_JUGADA[t.ultima.tipo]) + ":" + t.ultima.carta + " ";
			if (t.ultima.tipo == J_ATACAR && bocaAbajo > 0) res.ciegos++;
			if (t.ultima.tipo == J_ACTIVAR && papel(&b, t.ultima.carta, 't') && bocaAbajo > 0) res.limpiezas++;
		}
		if (rival && d.activo == 1 && f == F_BATALLA && bocaAbajo > 0 && t.ultima.tipo == J_FASE) res.oportCiegos++;
		pasos++;
	}
	res.ganador = d.ganador;
	res.turnos = d.turno;
	res.obs = tr.cerebro.plan.obs;
	res.obs.mirar(d, 0);
	res.planes = tr.cerebro.plan.planesDuelo;
	return res;
}

static float entropia(const std::map<int, int>& c, int n)
{
	float h = 0.0f;
	for (auto& kv : c) {
		const float p = (float)kv.second / (float)n;
		if (p > 0) h -= p * std::log2(p);
	}
	return h;
}
// % de cartas distintas entre dos mazos (multiconjunto)
static float distintas(const std::vector<std::string>& a, const std::vector<std::string>& b)
{
	std::map<std::string, int> c;
	for (auto& x : a) c[x]++;
	int comun = 0;
	for (auto& x : b)
		if (c[x] > 0) c[x]--, comun++;
	return 100.0f * (1.0f - (float)comun / (float)std::max(a.size(), b.size()));
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_variedad <carpeta ui/> <carpeta datos/> [duelos por rival]\n");
		return 2;
	}
	const int N = argc > 3 ? atoi(argv[3]) : 16;
	const std::string datos = argv[2];
	Base b;
	if (!b.leerCartas(argv[1]) || !b.leerRivales(datos + "rivales.txt")) return 1;
	b.leerRivales(datos + "personajes_rivales.txt");
	if (!b.leerVariedad(datos)) return 1;
	int fallos = 0;

	// 0) los combos apuntan a cartas que existen
	for (const Combo& c : b.combos) {
		if (planPorClave(c.plan) < 0) fallos++, fprintf(stderr, "FALLA combo %s: plan %s\n", c.clave.c_str(), c.plan.c_str());
		for (auto& pz : c.piezas)
			if (pz[0] != '@' && pz[0] != '%' && pz[0] != '&' && !b.carta(pz)) fallos++, fprintf(stderr, "FALLA combo %s: %s\n", c.clave.c_str(), pz.c_str());
	}

	// 1) mazos dinamicos validos
	int mazos = 0, malos = 0;
	for (const Rival& r : b.rivales)
		for (int pl = 0; pl < NUM_PLANES; pl++)
			for (uint32_t s = 1; s <= 2; s++) {
				const MazoRival m = mazoDinamico(b, r, pl, s * 7919u + (uint32_t)pl, {}, (float)(s == 2 ? 4 : -3));
				mazos++;
				std::map<std::string, int> n;
				for (auto& id : m.principal) n[id]++;
				for (auto& id : m.fusion) n[id]++;
				bool ok = m.principal.size() >= 40 && m.principal.size() <= 60 && m.fusion.size() <= 15;
				for (auto& kv : n) {
					const Carta* c = b.carta(kv.first);
					if (!c || kv.second > std::min(3, c->limite)) ok = false, fprintf(stderr, "    limite: %s x%d\n", kv.first.c_str(), kv.second);
				}
				for (auto& id : m.principal) {
					const Carta* c = b.carta(id);
					if (c && c->clase == "fusion") ok = false;
					auto it = b.rituales.find(id);
					if (it != b.rituales.end() && !n.count(it->second)) ok = false, fprintf(stderr, "    ritual sin magia: %s\n", id.c_str());
				}
				for (auto& f : m.fusion) {
					auto it = b.fusiones.find(f);
					if (it == b.fusiones.end()) continue;
					for (auto& mat : it->second)
						if (!n.count(mat)) ok = false, fprintf(stderr, "    fusion %s sin %s\n", f.c_str(), mat.c_str());
				}
				if (!m.fusion.empty() && !n.count("polymerization")) ok = false;
				if (!ok) {
					malos++;
					if (malos <= 5) fprintf(stderr, "FALLA mazo %s plan %s: %d cartas, %d fusiones\n", r.clave.c_str(), PLANES[pl].clave, (int)m.principal.size(), (int)m.fusion.size());
				}
			}
	fprintf(stderr, "mazos dinamicos: %d armados, %d invalidos\n", mazos, malos);
	if (malos) fallos++;

	// 2) el mismo rival N veces seguidas, con memoria
	const char* const PRUEBA[] = { "joey", "kaiba", "pegasus", "marik", "yugi", "cabra", "bruno", "ceniza" };
	const Rival* jugador = b.rival("rodrigo");
	const MazoRival mj = b.mazoDe(*jugador, 99);
	float sumaH = 0, sumaDist = 0, sumaDistAntes = 0;
	int sumaPrim = 0, sumaCambios = 0, sumaRep = 0, nRiv = 0, sumaPlanes = 0;
	for (const char* clave : PRUEBA) {
		const Rival* r = b.rival(clave);
		if (!r) continue;
		nRiv++;
		Memoria mem;
		std::map<int, int> cuenta;
		std::set<std::string> primeros;
		std::vector<std::string> anterior, anteriorAntes;
		float dist = 0, distAntes = 0;
		int rep = 0, cambios = 0, prevPlan = -1;
		for (int k = 0; k < N; k++) {
			const uint32_t s = mezclar(0xC0FFEEu, (uint32_t)(k * 131 + nRiv));
			const MazoRival mr = prepararRival(b, *r, s, &mem);
			const MazoRival ma = b.mazoDe(*r, s);   // el sistema de antes (3 variantes fijas)
			const Resultado res = jugar(b, mj, *jugador, mr, *r, s, mem.jugador, mem.dificultad(), mem.factorRuido());
			cuenta[mr.plan]++;
			rep += mr.plan == prevPlan;
			prevPlan = mr.plan;
			primeros.insert(res.primerTurno);
			cambios += (int)res.planes.size() - 1;
			if (k > 0) dist += distintas(anterior, mr.principal), distAntes += distintas(anteriorAntes, ma.principal);
			anterior = mr.principal;
			anteriorAntes = ma.principal;
			std::vector<std::string> mazo = mr.principal;
			mem.registrar(r->clave, res.planes, mazo, res.obs, res.ganador);
		}
		const float h = entropia(cuenta, N);
		fprintf(stderr, "  %-10s n%d %-11s planes distintos %d, entropia %.2f bits, repite plan seguido %d/%d, cartas distintas entre mazos seguidos %4.1f%% (antes %4.1f%%), "
		                "primeros turnos distintos %d/%d, cambios de plan a mitad %d\n",
		        r->clave.c_str(), r->nivel, r->clavePersonalidad.c_str(), (int)cuenta.size(), h, rep, N - 1, dist / (N - 1), distAntes / (N - 1), (int)primeros.size(), N, cambios);
		sumaH += h;
		sumaDist += dist / (N - 1);
		sumaDistAntes += distAntes / (N - 1);
		sumaPrim += (int)primeros.size();
		sumaCambios += cambios;
		sumaRep += rep;
		sumaPlanes += (int)cuenta.size();
	}
	fprintf(stderr, "DIVERSIDAD (%d rivales x %d duelos): entropia media %.2f bits (max %.2f), planes distintos medios %.1f, repeticion seguida %d/%d, "
	                "cartas distintas %.1f%% (antes %.1f%%), primeros turnos distintos %.0f%%, cambios de plan %d\n",
	        nRiv, N, sumaH / nRiv, std::log2((float)NUM_PLANES), (float)sumaPlanes / nRiv, sumaRep, nRiv * (N - 1), sumaDist / nRiv, sumaDistAntes / nRiv,
	        100.0f * (float)sumaPrim / (float)(nRiv * N), sumaCambios);
	if (sumaH / nRiv < 1.5f) fallos++, fprintf(stderr, "FALLA: poca variedad de planes\n");
	if (sumaDist / nRiv < 25.0f) fallos++, fprintf(stderr, "FALLA: los mazos se parecen demasiado\n");
	if (sumaRep * 5 > nRiv * (N - 1)) fallos++, fprintf(stderr, "FALLA: repite el plan seguido demasiado\n");

	// 3) dificultad por nivel con planes y mazos dinamicos (los genericos)
	{
		std::vector<const Rival*> gen;
		for (const Rival& r : b.rivales)
			if (!r.personaje) gen.push_back(&r);
		Azar az(4242);
		int ga = 0, gn = 0, sinFin = 0, rech = 0;
		const int D = 240;
		for (int k = 0; k < D; k++) {
			const uint32_t s = az.siguiente();
			const Rival& ra = *gen[(size_t)az.hasta((int)gen.size())];
			const Rival& rb = *gen[(size_t)az.hasta((int)gen.size())];
			const MazoRival a = prepararRival(b, ra, s, nullptr), c = prepararRival(b, rb, s ^ 0x55u, nullptr);
			Duelo d(&b);
			d.empezar(a.principal, a.fusion, c.principal, c.fusion, s);
			Turno ta(&d, 0, ra.personalidad(), ra.nivel, s), tb(&d, 1, rb.personalidad(), rb.nivel, s + 1);
			ta.cerebro.usarPlan(a.plan, Lectura{}, 0, 1);
			tb.cerebro.usarPlan(c.plan, Lectura{}, 0, 1);
			int pasos = 0;
			while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
				(d.decide() == 0 ? ta : tb).paso();
				pasos++;
			}
			sinFin += d.ganador < 0;
			rech += ta.rechazadas + tb.rechazadas;
			const int dif = ra.nivel - rb.nivel;
			if ((d.ganador == 0 || d.ganador == 1) && std::abs(dif) >= 3) {
				gn++;
				ga += (dif > 0) == (d.ganador == 0);
			}
		}
		fprintf(stderr, "NIVEL: %d duelos con planes, sin terminar %d, jugadas rechazadas %d, el de mas nivel (dif >= 3) gana %d%% de %d\n", D, sinFin, rech,
		        gn ? 100 * ga / gn : 0, gn);
		if (gn > 15 && ga * 100 / gn < 60) fallos++, fprintf(stderr, "FALLA: el nivel alto no gana lo suficiente\n");
		if (sinFin > D / 50) fallos++, fprintf(stderr, "FALLA: duelos sin terminar\n");
		if (rech) fallos++, fprintf(stderr, "FALLA: jugadas ilegales\n");
	}

	// 4) lectura del jugador: trampero vs farolero (mismo rival, mismas semillas y mazos)
	{
		Lectura trampero, farolero;
		trampero.trampas = 0.9f, trampero.faroles = 0.05f, trampero.confianza = 1.0f;
		farolero.trampas = 0.05f, farolero.faroles = 0.95f, farolero.confianza = 1.0f;
		const Rival* r = b.rival("pegasus");
		int cT = 0, cF = 0, oT = 0, oF = 0, lT = 0, lF = 0;
		for (int k = 0; k < 40; k++) {
			const uint32_t s = mezclar(777u, (uint32_t)k);
			const MazoRival mr = mazoDinamico(b, *r, k % NUM_PLANES == PL_ALTERNATIVA ? PL_CONTROL : k % NUM_PLANES, s, {}, 0);
			const Resultado a = jugar(b, mj, *jugador, mr, *r, s, trampero, 0, 1), c = jugar(b, mj, *jugador, mr, *r, s, farolero, 0, 1);
			cT += a.ciegos, oT += a.ciegos + a.oportCiegos, lT += a.limpiezas;
			if (getenv("DEP")) fprintf(stderr, "  k%d plan %d: T ciegos %d turnos %d gana %d | F ciegos %d turnos %d gana %d\n", k, mr.plan, a.ciegos, a.turnos, a.ganador, c.ciegos, c.turnos, c.ganador);
			cF += c.ciegos, oF += c.ciegos + c.oportCiegos, lF += c.limpiezas;
		}
		fprintf(stderr, "LECTURA: ataques a ciegas (con cartas boca abajo del jugador): trampero %d (%d%% de las batallas), farolero %d (%d%%); "
		                "limpiezas de magias/trampas antes de atacar: trampero %d, farolero %d\n",
		        cT, oT ? 100 * cT / oT : 0, cF, oF ? 100 * cF / oF : 0, lT, lF);
		if (cF <= cT) fallos++, fprintf(stderr, "FALLA: contra el farolero no ataca mas a ciegas\n");
	}

	// 5) dificultad adaptable
	{
		Memoria m;
		for (int k = 0; k < 5; k++) m.registrar("x", { PL_AGRESIVO }, {}, Observacion{}, 0);
		const float fGana = m.factorRuido(), dGana = m.dificultad();
		Memoria m2;
		for (int k = 0; k < 5; k++) m2.registrar("x", { PL_AGRESIVO }, {}, Observacion{}, 1);
		const float fPierde = m2.factorRuido(), dPierde = m2.dificultad();
		m.adaptable = false;
		fprintf(stderr, "ADAPTABLE: 5 victorias del jugador -> dificultad %+.0f, ruido x%.2f; 5 derrotas -> %+.0f, ruido x%.2f; apagada -> %+.0f\n", dGana, fGana, dPierde,
		        fPierde, m.dificultad());
		if (!(fGana < 1.0f && fPierde > 1.0f && m.dificultad() == 0.0f)) fallos++, fprintf(stderr, "FALLA: la dificultad adaptable no responde\n");
		// el mismo rival con la dificultad alta (racha +4) gana mas que con la baja (-3)
		const Rival* r = b.rival("joey");
		const Rival* fuerte = b.rival("acero");   // (un jugador a su altura: si no, gana siempre)
		const MazoRival mf = b.mazoDe(*fuerte, 7);
		Memoria alta, baja;
		alta.racha = 4, baja.racha = -3;
		int gA = 0, gB = 0;
		const int D = 100;
		for (int k = 0; k < D; k++) {
			const uint32_t s = mezclar(31337u, (uint32_t)k);
			const MazoRival ma = prepararRival(b, *r, s, &alta), mb = prepararRival(b, *r, s, &baja);
			gA += jugar(b, mf, *fuerte, ma, *r, s, Lectura{}, alta.dificultad(), alta.factorRuido()).ganador == 1;
			gB += jugar(b, mf, *fuerte, mb, *r, s, Lectura{}, baja.dificultad(), baja.factorRuido()).ganador == 1;
		}
		fprintf(stderr, "ADAPTABLE: el rival gana %d%% con la racha del jugador +4 y %d%% con -3 (%d duelos cada uno)\n", 100 * gA / D, 100 * gB / D, D);
		if (gA < gB + 10) fallos++, fprintf(stderr, "FALLA: con la dificultad alta no gana mas\n");
	}

	// 6) la memoria se guarda y se lee igual
	{
		Memoria m;
		m.ruta = "/tmp/prueba_variedad_memoria.txt";
		m.racha = -2;
		m.adaptable = false;
		m.jugador.trampas = 0.7f;
		m.rivales["joey"].planes = { PL_COMBO, PL_AGRESIVO };
		m.rivales["joey"].mazo = { "a", "b" };
		m.guardar();
		Memoria n;
		n.leer(m.ruta);
		const bool ok = n.racha == -2 && !n.adaptable && std::fabs(n.jugador.trampas - 0.7f) < 0.01f && n.rivales["joey"].planes.size() == 2 &&
		                n.rivales["joey"].planes[0] == PL_COMBO && n.rivales["joey"].mazo.size() == 2;
		if (!ok) fallos++, fprintf(stderr, "FALLA: la memoria no se lee igual\n");
	}

	fprintf(stderr, "%s (%d fallos)\n", fallos ? "PRUEBA CON FALLOS" : "PRUEBA OK", fallos);
	return fallos ? 1 : 0;
}
