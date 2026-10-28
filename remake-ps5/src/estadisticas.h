/*
 * Yugioh Classic Remaster (rama classic-extras): estadisticas del jugador. SOLO logica (sin Vulkan).
 * Se registran al terminar cada duelo (cosmeticos_juego.h: registrarEstadisticas, gancho en resultado_juego.h) en
 * estadisticas.txt junto a progreso.txt (guardarAtomico): duelos, victorias/derrotas/empates por rival, racha actual
 * y mejor racha, mazo usado en cada duelo y cartas jugadas por el jugador. Los totales de siempre (victorias y
 * derrotas de progreso.txt, que vienen de antes de esta rama) y las cartas firma los pone el juego al abrir la pantalla.
 * La pantalla (Opciones > Personalizacion > Estadisticas) la dibuja cosmeticos_ui.h.
 */
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "guardado.h"

namespace estadisticas {

struct Rival {
	std::string clave, es, en;
	int v{ 0 }, d{ 0 }, e{ 0 };
	int duelos() const { return v + d + e; }
};

struct Datos {
	std::string ruta;
	int duelos{ 0 }, victorias{ 0 }, derrotas{ 0 }, empates{ 0 };
	int racha{ 0 };        // > 0 victorias seguidas, < 0 derrotas seguidas
	int mejorRacha{ 0 };   // la mayor racha de victorias
	std::vector<Rival> rivales;
	std::vector<std::pair<std::string, int>> mazos, cartas;   // nombre del mazo / id de carta -> veces
	// lo pone el juego al abrir la pantalla
	int victoriasTotal{ 0 }, derrotasTotal{ 0 };   // progreso.txt (incluye lo de antes de registrar por rival)
	// rama classic-en-linea: los duelos en linea van aparte (no tocan los de la IA, ni la racha, ni los rivales)
	int lineaDuelos{ 0 }, lineaVictorias{ 0 }, lineaDerrotas{ 0 }, lineaEmpates{ 0 };
	int firmaLibres{ 0 }, firmaTotal{ 0 };
};
inline Datos& datos()
{
	static Datos d;
	return d;
}

inline std::string limpio(std::string s)
{
	for (char& c : s)
		if (c == '\t' || c == '\n' || c == '\r') c = ' ';
	return s;
}
inline void sumar(std::vector<std::pair<std::string, int>>& v, const std::string& k, int n = 1)
{
	if (k.empty()) return;
	for (auto& p : v)
		if (p.first == k) {
			p.second += n;
			return;
		}
	v.push_back({ k, n });
}
inline const std::pair<std::string, int>* masAlto(const std::vector<std::pair<std::string, int>>& v)
{
	const std::pair<std::string, int>* r = nullptr;
	for (const auto& p : v)
		if (!r || p.second > r->second) r = &p;
	return r;
}

inline void leer(const std::string& ruta)
{
	Datos& d = datos();
	d = Datos{};
	d.ruta = ruta;
	std::ifstream f(ruta);
	std::string l;
	while (std::getline(f, l)) {
		std::vector<std::string> c;
		std::string x;
		std::istringstream ss(l);
		while (std::getline(ss, x, '\t')) c.push_back(x);
		if (c.size() == 2) {
			const int n = atoi(c[1].c_str());
			if (c[0] == "duelos") d.duelos = n;
			if (c[0] == "victorias") d.victorias = n;
			if (c[0] == "derrotas") d.derrotas = n;
			if (c[0] == "empates") d.empates = n;
			if (c[0] == "racha") d.racha = n;
			if (c[0] == "mejor_racha") d.mejorRacha = n;
			if (c[0] == "linea_duelos") d.lineaDuelos = n;
			if (c[0] == "linea_victorias") d.lineaVictorias = n;
			if (c[0] == "linea_derrotas") d.lineaDerrotas = n;
			if (c[0] == "linea_empates") d.lineaEmpates = n;
		} else if (c.size() == 7 && c[0] == "rival") {
			Rival r;
			r.clave = c[1];
			r.es = c[2];
			r.en = c[3];
			r.v = atoi(c[4].c_str());
			r.d = atoi(c[5].c_str());
			r.e = atoi(c[6].c_str());
			d.rivales.push_back(r);
		} else if (c.size() == 3 && (c[0] == "mazo" || c[0] == "carta")) {
			sumar(c[0] == "mazo" ? d.mazos : d.cartas, c[1], atoi(c[2].c_str()));
		}
	}
	fprintf(stderr, "[estadisticas] %d duelos registrados, %zu rivales (%s)\n", d.duelos, d.rivales.size(), ruta.c_str());
}

inline void guardar()
{
	const Datos& d = datos();
	if (d.ruta.empty()) return;
	std::string s;
	guardado::agregar(s, "duelos\t%d\nvictorias\t%d\nderrotas\t%d\nempates\t%d\nracha\t%d\nmejor_racha\t%d\n", d.duelos, d.victorias, d.derrotas, d.empates, d.racha,
	                  d.mejorRacha);
	guardado::agregar(s, "linea_duelos\t%d\nlinea_victorias\t%d\nlinea_derrotas\t%d\nlinea_empates\t%d\n", d.lineaDuelos, d.lineaVictorias, d.lineaDerrotas, d.lineaEmpates);
	for (const Rival& r : d.rivales) guardado::agregar(s, "rival\t%s\t%s\t%s\t%d\t%d\t%d\n", r.clave.c_str(), r.es.c_str(), r.en.c_str(), r.v, r.d, r.e);
	for (const auto& p : d.mazos) guardado::agregar(s, "mazo\t%s\t%d\n", p.first.c_str(), p.second);
	for (const auto& p : d.cartas) guardado::agregar(s, "carta\t%s\t%d\n", p.first.c_str(), p.second);
	guardado::guardarAtomico(d.ruta, s);
}

// al terminar un duelo. ganador: 0 jugador, 1 rival, 2 empate
inline void registrar(const std::string& clave, const std::string& es, const std::string& en, int ganador, const std::string& mazo,
                      const std::vector<std::string>& jugadas)
{
	Datos& d = datos();
	d.duelos++;
	if (ganador == 0) {
		d.victorias++;
		d.racha = d.racha > 0 ? d.racha + 1 : 1;
		d.mejorRacha = std::max(d.mejorRacha, d.racha);
	} else if (ganador == 1) {
		d.derrotas++;
		d.racha = d.racha < 0 ? d.racha - 1 : -1;
	} else {
		d.empates++;
		d.racha = 0;
	}
	if (!clave.empty()) {
		Rival* r = nullptr;
		for (Rival& x : d.rivales)
			if (x.clave == clave) r = &x;
		if (!r) {
			d.rivales.push_back(Rival{});
			r = &d.rivales.back();
			r->clave = limpio(clave);
		}
		r->es = limpio(es);
		r->en = limpio(en);
		(ganador == 0 ? r->v : ganador == 1 ? r->d : r->e)++;
	}
	sumar(d.mazos, limpio(mazo));
	for (const std::string& c : jugadas) sumar(d.cartas, limpio(c));
	guardar();
	fprintf(stderr, "[estadisticas] duelo %d contra %s: %s, racha %d, %zu cartas jugadas, mazo '%s'\n", d.duelos, clave.c_str(),
	        ganador == 0 ? "victoria" : ganador == 1 ? "derrota" : "empate", d.racha, jugadas.size(), mazo.c_str());
}

// rama classic-en-linea: un duelo en linea terminado (ganador: 0 jugador, 1 rival, 2 empate). Los interrumpidos
// (desconexion) no llegan aca: no cuentan
inline void registrarEnLinea(int ganador)
{
	Datos& d = datos();
	d.lineaDuelos++;
	(ganador == 0 ? d.lineaVictorias : ganador == 1 ? d.lineaDerrotas : d.lineaEmpates)++;
	guardar();
	fprintf(stderr, "[estadisticas] duelo en linea %d: %s (%d / %d / %d)\n", d.lineaDuelos, ganador == 0 ? "victoria" : ganador == 1 ? "derrota" : "empate",
	        d.lineaVictorias, d.lineaDerrotas, d.lineaEmpates);
}

}   // namespace estadisticas
