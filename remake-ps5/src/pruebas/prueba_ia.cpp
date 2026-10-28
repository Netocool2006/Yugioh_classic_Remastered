// Prueba rapida del rival (src/ia.h) sin pantalla: valida los 120 mazos de rivales.txt (40-60, copias, prohibidas y
// limitadas Goat) y juega muchos duelos IA contra IA solo con la logica, para ver que nunca se cuelga, que ninguna
// jugada es ilegal, que los resultados varian y que el nivel alto le gana mas al bajo.
// Compilar y correr (contenedor arch-ps5 o cualquier g++ con C++17; segundos):
//   g++ -std=c++17 -O2 -Wall -Wextra -I remake-ps5/src remake-ps5/src/pruebas/prueba_ia.cpp -o /tmp/prueba_ia &&
//   /tmp/prueba_ia <carpeta con cartas_ui.txt y cartas_classic.txt>/ remake-ps5/datos/rivales.txt [duelos]
#include "ia.h"

#include <map>
#include <set>

using namespace ia;

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_ia <carpeta ui/> <rivales.txt> [duelos]\n");
		return 2;
	}
	const int N = argc > 3 ? atoi(argv[3]) : 800;
	Base base;
	if (!base.leerCartas(argv[1]) || !base.leerRivales(argv[2])) return 1;
	int fallos = 0;

	// 1) los mazos
	int mazos = 0;
	for (const Rival& r : base.rivales) {
		for (size_t v = 0; v < r.principal.size(); v++) {
			mazos++;
			const auto& m = r.principal[v];
			const auto f = v < r.fusion.size() ? r.fusion[v] : std::vector<std::string>{};
			std::map<std::string, int> n;
			for (auto& id : m) n[id]++;
			for (auto& id : f) n[id]++;
			bool ok = m.size() >= 40 && m.size() <= 60 && f.size() <= 15;
			for (auto& kv : n) {
				const Carta* c = base.carta(kv.first);
				if (!c || kv.second > std::min(3, c->limite)) {
					fprintf(stderr, "FALLA %s v%d: %s x%d (limite %d)\n", r.clave.c_str(), (int)v, kv.first.c_str(), kv.second, c ? c->limite : -1);
					ok = false;
				}
			}
			for (auto& id : m)
				if (base.carta(id) && base.carta(id)->clase == "fusion") ok = false;
			if (!ok) fallos++;
		}
	}
	fprintf(stderr, "mazos: %d de %d rivales (fallos %d)\n", mazos, (int)base.rivales.size(), fallos);

	// 2) los duelos
	Azar azar(12345);
	int sinFin = 0, rechazadas = 0, totalTurnos = 0, maxTurnos = 0;
	std::map<std::string, int> jugadas;
	std::map<int, std::pair<int, int>> porDif;   // nivel A - nivel B -> (gana A, duelos)
	std::map<std::string, std::pair<int, int>> porPers;
	std::set<std::string> distintos;
	for (int k = 0; k < N; k++) {
		const uint32_t s = azar.siguiente();
		const Rival& ra = base.rivales[(size_t)azar.hasta((int)base.rivales.size())];
		const Rival& rb = base.rivales[(size_t)azar.hasta((int)base.rivales.size())];
		MazoRival a = base.mazoDe(ra, s), b = base.mazoDe(rb, s ^ 0x55u);
		Duelo d(&base);
		d.empezar(a.principal, a.fusion, b.principal, b.fusion, s);
		Turno ta(&d, 0, ra.personalidad(), ra.nivel, s), tb(&d, 1, rb.personalidad(), rb.nivel, s + 1);
		int pasos = 0;
		while (d.ganador < 0 && pasos < 20000 && d.turno < 400) {
			Turno& t = d.decide() == 0 ? ta : tb;   // (rama classic-efectos: el que decide puede ser el que responde)
			t.paso();
			if (t.ultima.tipo != J_FASE) jugadas[NOMBRE_JUGADA[t.ultima.tipo]]++;
			pasos++;
		}
		if (d.ganador < 0) sinFin++;
		rechazadas += ta.rechazadas + tb.rechazadas;
		totalTurnos += d.turno;
		maxTurnos = std::max(maxTurnos, d.turno);
		distintos.insert(std::to_string(d.ganador) + "/" + std::to_string(d.turno) + "/" + std::to_string(d.l[0].lp) + "/" + std::to_string(d.l[1].lp));
		if (d.ganador == 0 || d.ganador == 1) {
			const int dif = ra.nivel - rb.nivel;
			auto& e = porDif[dif];
			e.first += d.ganador == 0;
			e.second++;
			porPers[ra.clavePersonalidad].first += d.ganador == 0;
			porPers[ra.clavePersonalidad].second++;
			porPers[rb.clavePersonalidad].first += d.ganador == 1;
			porPers[rb.clavePersonalidad].second++;
		}
	}
	fprintf(stderr, "duelos: %d, sin terminar: %d, jugadas rechazadas: %d, turnos medios %.1f (max %d), resultados distintos: %d\n", N, sinFin,
	        rechazadas, (double)totalTurnos / N, maxTurnos, (int)distintos.size());
	fprintf(stderr, "jugadas:");
	for (auto& kv : jugadas) fprintf(stderr, " %s=%d", kv.first.c_str(), kv.second);
	fprintf(stderr, "\nvictorias del primero segun la diferencia de nivel (A - B):\n");
	for (auto& kv : porDif) fprintf(stderr, "  %+d: %3d%% de %d\n", kv.first, kv.second.second ? 100 * kv.second.first / kv.second.second : 0, kv.second.second);
	fprintf(stderr, "victorias por personalidad:\n");
	for (auto& kv : porPers) fprintf(stderr, "  %-12s %3d%% de %d\n", kv.first.c_str(), kv.second.second ? 100 * kv.second.first / kv.second.second : 0, kv.second.second);

	// la progresion: el de mas nivel debe ganar mas (diferencia >= 3)
	int ga = 0, gn = 0;
	for (auto& kv : porDif)
		if (kv.first >= 3) {
			ga += kv.second.first;
			gn += kv.second.second;
		}
	if (sinFin > N / 50) fallos++, fprintf(stderr, "FALLA: demasiados duelos sin terminar\n");
	if (rechazadas) fallos++, fprintf(stderr, "FALLA: jugadas ilegales\n");
	if ((int)distintos.size() < N / 4) fallos++, fprintf(stderr, "FALLA: poca variedad\n");
	if (gn > 20 && ga * 100 / gn < 60) fallos++, fprintf(stderr, "FALLA: el nivel alto no gana lo suficiente (%d%%)\n", ga * 100 / gn);
	fprintf(stderr, "%s (%d fallos)\n", fallos ? "PRUEBA CON FALLOS" : "PRUEBA OK", fallos);
	return fallos ? 1 : 0;
}
