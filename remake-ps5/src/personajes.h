/*
 * Yugioh Classic Remaster - los PERSONAJES del anime como rivales del modo historia (rama classic-personajes).
 * SOLO logica: sin Vulkan (el dibujo esta en personajes_ui.h, dentro de interfaz::Interfaz).
 *
 * Datos (assets/ui/, los genera remake-ps5/personajes_gen.py; razonamiento en docs/personajes.md):
 *   personajes.txt           id|nombre_es|nombre_en|saga|nivel|personalidad|entorno|lema_es|lema_en
 *   personajes_rivales.txt   sus mazos (formato de rivales.txt; lo lee ia::Base::leer)
 *   cartas_classic.txt       las cartas firma: id|passcode|limite|subtipo|firma|duenno,duenno
 *   ../personajes/retrato_<id>.png   el retrato (RGBA, ~1400 px de alto; no va al repo)
 * Modo historia: 6 sagas en orden (Reino de los Duelistas -> Memorias del Faraon). Una saga se abre al vencer a
 * todos los personajes de la anterior; dentro de una saga abierta se elige a cualquiera (van de menor a mayor nivel).
 * Progreso: historia.txt junto a progreso.txt (una linea "vencido <id>" por personaje).
 * Cartas firma: cartaDesbloqueada(duennos) = vencido al menos uno de sus duennos (PROPUESTA para el constructor: el
 * jugador las tiene desde que vence a su duenno; hoy mazo.h aun se las da todas).
 */
#pragma once

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "guardado.h"   // rama classic-pulido: guardado atomico

namespace personajes {

struct Saga { const char *id, *es, *en; };
static const Saga SAGAS[] = {
	{ "reino", "Reino de los Duelistas", "Duelist Kingdom" },
	{ "ciudad", "Ciudad Batallas", "Battle City" },
	{ "noah", "El mundo virtual de Noah", "Noah's Virtual World" },
	{ "dragones", "El Despertar de los Dragones", "Waking the Dragons" },
	{ "campeonato", "Gran Campeonato KC", "KC Grand Championship" },
	{ "faraon", "Memorias del Faraón", "Memories of the Pharaoh" },
};
constexpr int NUM_SAGAS = (int)(sizeof(SAGAS) / sizeof(SAGAS[0]));

inline int sagaPorId(const std::string& id)
{
	for (int i = 0; i < NUM_SAGAS; i++)
		if (id == SAGAS[i].id) return i;
	return -1;
}

struct Personaje {
	std::string id, nombreEs, nombreEn, saga, personalidad, entorno, lemaEs, lemaEn;
	int nivel{ 1 }, sagaIdx{ 0 };
	std::vector<std::string> firma;   // sus cartas firma (las que desbloquea)
	const std::string& nombre(bool en) const { return en && !nombreEn.empty() ? nombreEn : nombreEs; }
	const std::string& lema(bool en) const { return en && !lemaEn.empty() ? lemaEn : lemaEs; }
};

inline std::vector<std::string> partirCampos(const std::string& l, char sep)
{
	std::vector<std::string> r;
	std::string x;
	std::istringstream ss(l);
	while (std::getline(ss, x, sep)) r.push_back(x);
	if (!l.empty() && l.back() == sep) r.push_back("");
	return r;
}

class Historia {
public:
	std::vector<Personaje> lista;   // por saga y nivel (el orden del archivo)
	std::set<std::string> vencidos;
	bool finalVisto{ false };   // rama classic-historia: el final de la historia ya se vio (desbloquea el Modo Desafio)

	// carpeta ui con "/" al final
	bool leer(const std::string& carpetaUi)
	{
		lista.clear();
		std::ifstream f(carpetaUi + "personajes.txt");
		std::string l;
		while (std::getline(f, l)) {
			if (!l.empty() && l.back() == '\r') l.pop_back();
			if (l.empty() || l[0] == '#') continue;
			auto p = partirCampos(l, '|');
			if (p.size() < 9 || p[0].empty()) continue;
			Personaje x;
			x.id = p[0];
			x.nombreEs = p[1];
			x.nombreEn = p[2];
			x.saga = p[3];
			x.nivel = std::max(1, atoi(p[4].c_str()));
			x.personalidad = p[5];
			x.entorno = p[6];
			x.lemaEs = p[7];
			x.lemaEn = p[8];
			x.sagaIdx = sagaPorId(x.saga);
			if (x.sagaIdx < 0) continue;
			lista.push_back(x);
		}
		std::stable_sort(lista.begin(), lista.end(), [](const Personaje& a, const Personaje& b) {
			return a.sagaIdx != b.sagaIdx ? a.sagaIdx < b.sagaIdx : a.nivel < b.nivel;
		});
		// cartas firma de cada uno (cartas_classic.txt: ...|firma|duenno,duenno)
		std::ifstream g(carpetaUi + "cartas_classic.txt");
		while (std::getline(g, l)) {
			if (!l.empty() && l.back() == '\r') l.pop_back();
			auto p = partirCampos(l, '|');
			if (p.size() < 6 || p[4] != "firma") continue;
			for (const std::string& d : partirCampos(p[5], ','))
				if (Personaje* x = editar(d)) x->firma.push_back(p[0]);
		}
		fprintf(stderr, "[personajes] %d personajes en %d sagas\n", (int)lista.size(), NUM_SAGAS);
		return !lista.empty();
	}

	// ---- progreso
	void leerProgreso(const std::string& ruta)
	{
		rutaProgreso = ruta;
		vencidos.clear();
		finalVisto = false;
		std::ifstream f(ruta);
		std::string clave, id;
		while (f >> clave >> id) {
			if (clave == "vencido") vencidos.insert(id);
			if (clave == "final") finalVisto = true;
		}
		fprintf(stderr, "[personajes] historia: %d de %d vencidos (%s)\n", (int)vencidos.size(), (int)lista.size(), ruta.c_str());
	}
	void guardar() const
	{
		std::string s;   // (rama classic-pulido: guardado atomico, guardado.h)
		for (const std::string& v : vencidos) s += "vencido " + v + "\n";
		if (finalVisto) s += "final visto\n";   // rama classic-historia
		guardado::guardarAtomico(rutaProgreso, s);
	}
	// true si es la primera vez que lo vence
	bool vencer(const std::string& id)
	{
		if (!buscar(id) || vencidos.count(id)) return false;
		vencidos.insert(id);
		guardar();
		return true;
	}

	// ---- consultas
	const Personaje* buscar(const std::string& id) const
	{
		for (const Personaje& x : lista)
			if (x.id == id) return &x;
		return nullptr;
	}
	bool vencido(const std::string& id) const { return vencidos.count(id) > 0; }
	std::vector<int> deSaga(int s) const
	{
		std::vector<int> r;
		for (int i = 0; i < (int)lista.size(); i++)
			if (lista[(size_t)i].sagaIdx == s) r.push_back(i);
		return r;
	}
	bool sagaCompleta(int s) const
	{
		for (int i : deSaga(s))
			if (!vencido(lista[(size_t)i].id)) return false;
		return true;
	}
	bool sagaAbierta(int s) const { return s <= 0 || (sagaAbierta(s - 1) && sagaCompleta(s - 1)); }
	// la saga en la que va el jugador (la ultima abierta)
	int sagaActual() const
	{
		int s = 0;
		while (s + 1 < NUM_SAGAS && sagaAbierta(s + 1)) s++;
		return s;
	}
	// rama classic-historia: todos vencidos (el final de la historia)
	bool completa() const
	{
		for (const Personaje& x : lista)
			if (!vencido(x.id)) return false;
		return !lista.empty();
	}
	int vencidosEn(int s) const
	{
		int n = 0;
		for (int i : deSaga(s)) n += vencido(lista[(size_t)i].id) ? 1 : 0;
		return n;
	}
	// carta firma: el jugador la tiene si vencio a uno de sus duennos (PROPUESTA para el constructor)
	bool cartaDesbloqueada(const std::vector<std::string>& duennos) const
	{
		for (const std::string& d : duennos)
			if (vencido(d)) return true;
		return false;
	}

private:
	std::string rutaProgreso;
	Personaje* editar(const std::string& id)
	{
		for (Personaje& x : lista)
			if (x.id == id) return &x;
		return nullptr;
	}
};

}  // namespace personajes
