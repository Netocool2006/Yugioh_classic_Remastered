// Prueba de los MAZOS DE TORNEO (rama classic-mazos-torneo; datos/mazos_torneo.txt, mazos_torneo.py, ia_plan.h), sin
// pantalla:
//  1. carga: todos los mazos de torneo se leen, cada uno va a algun rival y solo a genericos de nivel 7-8 o personajes
//     de nivel 5 o mas (los niveles bajos no cambian)
//  2. legalidad: 40 cartas, max. 3 copias y limites Goat (nada prohibido), sin fusiones en el principal, fusiones con
//     sus materiales y Polymerization o con Metamorphosis y un monstruo de su nivel, rituales con su magia, monstruos
//     que no se invocan normal solo si el mazo sabe sacarlos (Toon World, Horus LV6, Release Restraint, Envoy/Sorcerer)
//  3. el cargador los respeta: prepararRival los da TAL CUAL (mismas cartas en el mismo orden, sin reequilibrar ni
//     diluir), con la probabilidad esperada, nunca el mismo dos duelos seguidos, y nunca a quien no tiene
//  4. la IA los juega: duelos IA (clasica) contra IA con cada mazo de torneo contra los mazos tematicos de nivel 8
//     (los genericos de nivel 8 con su mazo dinamico): 0 jugadas ilegales, activa sus cartas clave, y % de victorias
//     (los de torneo tienen que ganar en mayoria)
//  5. la IA humana tambien los juega sin jugadas ilegales (1 duelo por mazo)
// Compilar y correr (contenedor arch-ps5; 1-3 minutos):
//   g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_torneo.cpp -o /tmp/prueba_torneo &&
//   /tmp/prueba_torneo <carpeta ui/ con cartas_ui.txt> remake-ps5/datos/ [duelos por mazo] [humana 1|0]
#include "ia.h"

#include <map>
#include <set>

using namespace ia;

struct ResDuelo {
	int ganador{ -1 }, turnos{ 0 }, rechazadas{ 0 };
	std::set<std::string> claves;   // cartas clave que el lado del torneo jugo (invocar, activar, efecto)
};

// lado "lt" juega el mazo de torneo
static ResDuelo jugar(const Base& b, const MazoRival& m0, const Rival& r0, const MazoRival& m1, const Rival& r1, uint32_t s, int lt,
                      const std::vector<std::string>& claves, bool humana)
{
	ResDuelo res;
	Duelo d(&b);
	d.empezar(m0.principal, m0.fusion, m1.principal, m1.fusion, s);
	Turno t0(&d, 0, r0.personalidad(), 8, s), t1(&d, 1, r1.personalidad(), 8, s + 1);
	t0.cerebro.usarPlan(m0.plan, Lectura{}, 0, 1);
	t1.cerebro.usarPlan(m1.plan, Lectura{}, 0, 1);
	if (humana) (lt == 0 ? t0 : t1).usarHumana(true, (lt == 0 ? r0 : r1).clave, s + 3);
	int pasos = 0;
	size_t visto = 0;
	while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
		(d.decide() == 0 ? t0 : t1).paso();
		pasos++;
		for (; visto < d.sucesos.size(); visto++) {
			const Suceso& x = d.sucesos[visto];
			if (x.lado != lt || (x.tipo != S_INVOCA && x.tipo != S_ACTIVA_MT && x.tipo != S_ACTIVA_EFECTO)) continue;
			if (std::find(claves.begin(), claves.end(), x.carta) != claves.end()) res.claves.insert(x.carta);
		}
	}
	res.ganador = d.ganador;
	res.turnos = d.turno;
	if (getenv("DEPURA")) {   // (afinado: como termino el duelo del lado del torneo)
		const Lado& x = d.l[(size_t)lt];
		std::string pz;
		for (const char* donde : { "mano", "cementerio", "desterrado", "mazo" }) {
			const auto& v = strcmp(donde, "mano") == 0 ? x.mano : strcmp(donde, "cementerio") == 0 ? x.cementerio : strcmp(donde, "mazo") == 0 ? x.mazo : x.desterrados;
			int k = 0;
			for (auto& id : v) k += id.find("forbidden_one") != std::string::npos;
			pz += std::string(donde) + "=" + std::to_string(k) + " ";
		}
		printf("  torneo=%d gana %d turno %d LP %d/%d mazo %d/%d exodia: %s cementerio:", lt, d.ganador, d.turno, x.lp, d.l[(size_t)(1 - lt)].lp, (int)x.mazo.size(),
		       (int)d.l[(size_t)(1 - lt)].mazo.size(), pz.c_str());
		for (auto& id : x.cementerio) printf(" %s", id.c_str());
		printf("\n");
	}
	res.rechazadas = t0.rechazadas + t1.rechazadas;
	return res;
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_torneo <carpeta ui/> <carpeta datos/> [duelos por mazo] [humana 1|0]\n");
		return 2;
	}
	const int N = argc > 3 ? atoi(argv[3]) : 20;
	const bool conHumana = argc > 4 ? atoi(argv[4]) != 0 : true;
	const std::string solo = argc > 5 ? argv[5] : "";   // (afinado: solo ese mazo en los duelos)
	const std::string datos = argv[2];
	Base b;
	if (!b.leerCartas(argv[1]) || !b.leerRivales(datos + "rivales.txt")) return 1;
	b.leerRivales(datos + "personajes_rivales.txt");
	if (!b.leerVariedad(datos)) return 1;   // (lee tambien mazos_torneo.txt)
	int fallos = 0;
	auto falla = [&](const char* fmt, auto... a) {
		fallos++;
		fprintf(stdout, "FALLA ");
		fprintf(stdout, fmt, a...);
		fprintf(stdout, "\n");
	};

	// 1) carga y asignacion
	std::map<int, std::vector<const Rival*>> duennos;
	for (const Rival& r : b.rivales)
		for (int k : r.torneo) {
			duennos[k].push_back(&r);
			// (rama classic-torneo-extras: Joey, nivel 4, lleva Gearfried y Equipos por decision del usuario)
			const bool ok = r.personaje ? r.nivel >= 5 || r.clave == "joey" : r.nivel >= 7;
			if (!ok) falla("mazo de torneo %s en %s (nivel %d): solo genericos 7-8, personajes 5+ y Joey", b.torneos[(size_t)k].clave.c_str(), r.clave.c_str(), r.nivel);
		}
	if (b.torneos.size() < 20) falla("solo %d mazos de torneo (se esperan 21)", (int)b.torneos.size());
	for (size_t k = 0; k < b.torneos.size(); k++) {
		if (duennos[(int)k].empty()) falla("el mazo %s no va a ningun rival", b.torneos[k].clave.c_str());
		if (planPorClave(b.torneos[k].plan) < 0) falla("el mazo %s tiene un plan desconocido: %s", b.torneos[k].clave.c_str(), b.torneos[k].plan.c_str());
	}
	int conTorneo = 0;
	for (const Rival& r : b.rivales) conTorneo += !r.torneo.empty();
	printf("carga: %d mazos de torneo, %d rivales/personajes con mazo de torneo\n", (int)b.torneos.size(), conTorneo);

	// 2) legalidad
	static const std::map<std::string, std::string> ESPECIAL = {   // no se invocan normal: lo que el mazo necesita para sacarlos
		{ "black_luster_soldier_envoy_of_the_beginning", "" }, { "chaos_sorcerer", "" }, { "chaos_emperor_dragon_envoy_of_the_end", "" },
		{ "horus_the_black_flame_dragon_lv8", "horus_the_black_flame_dragon_lv6" }, { "gearfried_the_swordmaster", "release_restraint" },
		{ "yamato_no_kami", "" },   // (rama classic-torneo-extras: desterrando un Spirit del cementerio)
	};
	int legales = 0;
	for (const MazoTorneo& t : b.torneos) {
		const int antes = fallos;
		std::map<std::string, int> n;
		for (auto& id : t.principal) n[id]++;
		for (auto& id : t.fusion) n[id]++;
		if (t.principal.size() != 40) falla("%s: %d cartas en el principal", t.clave.c_str(), (int)t.principal.size());
		if (t.fusion.size() > 15) falla("%s: %d fusiones", t.clave.c_str(), (int)t.fusion.size());
		std::set<int> niveles;
		for (auto& kv : n) {
			const Carta* c = b.carta(kv.first);
			if (!c) {
				falla("%s: carta desconocida %s", t.clave.c_str(), kv.first.c_str());
				continue;
			}
			if (c->limite <= 0) falla("%s: %s esta prohibida", t.clave.c_str(), kv.first.c_str());
			if (kv.second > std::min(3, c->limite)) falla("%s: %s x%d (limite %d)", t.clave.c_str(), kv.first.c_str(), kv.second, c->limite);
		}
		for (auto& id : t.principal) {
			const Carta* c = b.carta(id);
			if (!c) continue;
			if (c->clase == "fusion") falla("%s: fusion en el principal: %s", t.clave.c_str(), id.c_str());
			if (c->monstruo) niveles.insert(c->nivel);
			if (c->monstruo && c->subtipo == "ritual") {
				bool magia = false;
				for (auto& kv : n) magia |= b.ritualCon(id, kv.first);
				if (!magia) falla("%s: ritual %s sin su magia", t.clave.c_str(), id.c_str());
			}
			if (c->subtipo == "toon" && c->monstruo && !n.count("toon_world")) falla("%s: Toon %s sin Toon World", t.clave.c_str(), id.c_str());
			if (c->noInvocable && c->subtipo != "toon") {
				auto it = ESPECIAL.find(id);
				if (it == ESPECIAL.end()) falla("%s: %s no se invoca normal y el mazo no sabe sacarlo", t.clave.c_str(), id.c_str());
				else if (!it->second.empty() && !n.count(it->second)) falla("%s: %s sin %s", t.clave.c_str(), id.c_str(), it->second.c_str());
			}
		}
		if (n.count("scapegoat")) niveles.insert(1);   // (fichas de nivel 1)
		for (auto& f : t.fusion) {
			const Carta* c = b.carta(f);
			if (!c || c->clase != "fusion") {
				falla("%s: %s en el de fusion no es fusion", t.clave.c_str(), f.c_str());
				continue;
			}
			bool conPoly = n.count("polymerization") > 0 && b.fusiones.count(f);
			if (conPoly)
				for (auto& mat : b.fusiones.at(f)) conPoly &= n.count(mat) > 0;
			const bool conMeta = n.count("metamorphosis") > 0 && niveles.count(c->nivel);
			if (!conPoly && !conMeta) falla("%s: fusion %s sin materiales+Polymerization ni Metamorphosis", t.clave.c_str(), f.c_str());
		}
		if (n.count("polymerization") && t.fusion.empty()) falla("%s: Polymerization sin fusiones", t.clave.c_str());
		if (t.claves.empty()) falla("%s: sin cartas clave", t.clave.c_str());
		for (auto& k : t.claves)
			if (!n.count(k)) falla("%s: la carta clave %s no esta en el mazo", t.clave.c_str(), k.c_str());
		legales += fallos == antes;
	}
	printf("legalidad: %d de %d mazos legales\n", legales, (int)b.torneos.size());

	// 3) el cargador los respeta
	int tal = 0, salen = 0, repetidos = 0, ajenos = 0, pruebas = 0;
	for (const Rival& r : b.rivales) {
		const int M = 300;
		int k = 0;
		for (int s = 1; s <= M; s++) {
			const MazoRival m = prepararRival(b, r, mezclar(0x7041u, (uint32_t)s), nullptr);
			if (m.torneo < 0) continue;
			if (r.torneo.empty()) {
				ajenos++;
				continue;
			}
			k++;
			const MazoTorneo& t = b.torneos[(size_t)m.torneo];
			if (m.principal == t.principal && m.fusion == t.fusion) tal++;
			else falla("%s: el mazo de torneo %s no salio tal cual", r.clave.c_str(), t.clave.c_str());
			if (std::find(r.torneo.begin(), r.torneo.end(), m.torneo) == r.torneo.end()) falla("%s: salio %s, que no es suyo", r.clave.c_str(), t.clave.c_str());
			if (m.plan < 0) falla("%s: mazo de torneo sin plan", r.clave.c_str());
			// con la memoria del duelo anterior (ese mismo mazo): no se repite
			Memoria mem;
			mem.rivales[r.clave].mazo = t.principal;
			const MazoRival m2 = prepararRival(b, r, mezclar(0x7041u, (uint32_t)s), &mem);
			repetidos += m2.torneo == m.torneo;
		}
		salen += k;
		if (!r.torneo.empty()) {
			pruebas++;
			const float p = (float)k / (float)M, esperada = probTorneo(r);
			if (std::fabs(p - esperada) > 0.08f) falla("%s: sale su mazo de torneo el %.0f%% (se espera %.0f%%)", r.clave.c_str(), 100 * p, 100 * esperada);
		}
	}
	if (ajenos) falla("%d mazos de torneo salieron a rivales sin mazo de torneo", ajenos);
	if (repetidos) falla("%d veces el mismo mazo de torneo dos duelos seguidos", repetidos);
	printf("cargador: %d rivales con torneo probados, %d mazos de torneo sacados, %d tal cual, %d repetidos, %d a rivales ajenos\n", pruebas, salen, tal,
	       repetidos, ajenos);

	// 4) duelos contra los tematicos de nivel 8
	std::vector<const Rival*> n8;
	for (const Rival& r : b.rivales)
		if (!r.personaje && r.nivel == 8) n8.push_back(&r);
	if (n8.empty()) {
		falla("no hay rivales genericos de nivel 8");
		printf("PRUEBA CON FALLOS (%d fallos)\n", fallos);
		return 1;
	}
	int gTot = 0, dTot = 0, sinFin = 0, rech = 0;
	printf("duelos: %d por mazo contra los tematicos de nivel 8 (IA clasica los dos lados, nivel 8)\n", N);
	printf("| mazo | plan | gana | % | cartas clave jugadas (duelos) |\n|---|---|---:|---:|---|\n");
	for (size_t k = 0; k < b.torneos.size(); k++) {
		const MazoTorneo& t = b.torneos[k];
		if (!solo.empty() && t.clave != solo) continue;
		const Rival* duenno = duennos[(int)k].empty() ? n8[0] : duennos[(int)k][0];
		int g = 0, dd = 0, turnos = 0;
		std::map<std::string, int> usadas;
		int conClave = 0;
		for (int i = 0; i < N; i++) {
			const uint32_t s = mezclar(0x70A3u + (uint32_t)k * 977u, (uint32_t)i);
			const Rival& rt = *n8[(size_t)i % n8.size()];
			// el tematico: su mazo dinamico con su plan (nunca uno de torneo)
			Azar a(mezclar(s, 9));
			a.siguiente();
			const int plan = sortearPlan(pesosPlanes(b, rt.clavePersonalidad, poolDe(rt), {}, Lectura{}, 0), a);
			const MazoRival mt = mazoDinamico(b, rt, plan, s, {}, 0);
			MazoRival mz = mazoTorneo(b, *duenno, (int)k, s, {}, Lectura{}, 0);
			const int lt = i % 2;
			const ResDuelo r = lt == 0 ? jugar(b, mz, *duenno, mt, rt, s, 0, t.claves, false) : jugar(b, mt, rt, mz, *duenno, s, 1, t.claves, false);
			rech += r.rechazadas;
			turnos += r.turnos;
			if (r.ganador < 0) sinFin++;
			else {
				dd++;
				g += r.ganador == lt;
			}
			for (auto& c : r.claves) usadas[c]++;
			conClave += !r.claves.empty();
		}
		gTot += g;
		dTot += dd;
		std::string u;
		for (auto& kv : usadas) u += kv.first + " " + std::to_string(kv.second) + ", ";
		if (!u.empty()) u.resize(u.size() - 2);
		printf("| %s | %s | %d/%d | %d%% | %s |\n", t.clave.c_str(), t.plan.c_str(), g, dd, dd ? 100 * g / dd : 0, u.empty() ? "-" : u.c_str());
		if (!solo.empty()) printf("  turnos medios %.1f\n", (double)turnos / std::max(1, N));
		if (conClave == 0) falla("%s: la IA no jugo ninguna de sus cartas clave en %d duelos", t.clave.c_str(), N);
	}
	const int pct = dTot ? 100 * gTot / dTot : 0;
	printf("TOTAL torneo contra tematicos de nivel 8: %d/%d = %d%% (sin terminar %d, jugadas ilegales %d)\n", gTot, dTot, pct, sinFin, rech);
	if (rech) falla("%d jugadas ilegales", rech);
	if (sinFin > (int)b.torneos.size() * N / 20) falla("%d duelos sin terminar", sinFin);
	if (dTot && pct <= 50) falla("los mazos de torneo no ganan en mayoria a los tematicos de nivel 8 (%d%%)", pct);

	// 5) la IA humana con cada mazo de torneo (sin jugadas ilegales)
	if (conHumana) {
		int rh = 0, gh = 0, dh = 0;
		for (size_t k = 0; k < b.torneos.size(); k++) {
			const Rival* duenno = duennos[(int)k].empty() ? n8[0] : duennos[(int)k][0];
			const Rival& rt = *n8[k % n8.size()];
			const uint32_t s = mezclar(0x4D7Au, (uint32_t)k);
			const MazoRival mt = mazoDinamico(b, rt, PL_AGRESIVO, s, {}, 0);
			const MazoRival mz = mazoTorneo(b, *duenno, (int)k, s, {}, Lectura{}, 0);
			const ResDuelo r = jugar(b, mz, *duenno, mt, rt, s, 0, b.torneos[k].claves, true);
			rh += r.rechazadas;
			dh += r.ganador >= 0;
			gh += r.ganador == 0;
		}
		printf("IA humana con los mazos de torneo: %d duelos, gana %d de %d terminados, jugadas ilegales %d\n", (int)b.torneos.size(), gh, dh, rh);
		if (rh) falla("IA humana: %d jugadas ilegales", rh);
	}

	printf("%s (%d fallos)\n", fallos ? "PRUEBA CON FALLOS" : "PRUEBA OK", fallos);
	return fallos ? 1 : 0;
}
