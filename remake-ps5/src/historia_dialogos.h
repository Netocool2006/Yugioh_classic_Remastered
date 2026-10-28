/*
 * Yugioh Classic Remaster - dialogos del modo historia (rama classic-historia). SOLO logica: sin Vulkan (el dibujo
 * esta en historia_dialogos_ui.h, dentro de interfaz::Interfaz).
 *
 * Datos: assets/ui/dialogos.txt (en el repo: datos/dialogos.txt), "id|tipo|texto ES|texto EN":
 *   intro   la escena antes del duelo (2 a 4 lineas, en orden; avanza con X)
 *   pierde  la linea de cierre del rival cuando GANA EL JUGADOR
 *   gana    la linea de cierre del rival cuando PIERDE EL JUGADOR
 *   _final  el final de la historia: "texto" (en orden), "creditos" (en orden) y "desbloqueo" (el premio)
 *
 * API para otras ramas (classic-resultado):
 *   historia_dialogos::dialogoCierre(id, gano)   la linea de cierre del personaje en el idioma actual ("" si no hay)
 *   historia_dialogos::cierreEnResultado()       true (por defecto): historia_dialogos_ui.h dibuja la linea en la
 *                                                pantalla de resultado; la rama que la dibuje a su manera lo pone en false
 *   historia_dialogos::desafioDisponible() / desafioActivo()   el Modo Desafio (premio del final)
 * Escena (estado global, lo lleva el juego y lo dibuja la interfaz): escena().abrir(INTRO, id) / abrirFinal();
 * escena().avanzar(pulsos) devuelve true al terminar (X pasa de linea; Circulo salta la escena).
 */
#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "control.h"
#include "idioma.h"

namespace historia_dialogos {

struct Linea {
	std::string es, en;
	const std::string& v() const { return idioma::ingles() && !en.empty() ? en : es; }
};

struct Personaje {
	std::vector<Linea> intro;
	Linea pierde, gana;
};

class Dialogos {
public:
	std::unordered_map<std::string, Personaje> por;   // por id de personaje
	std::vector<Linea> finalTexto, creditos;
	Linea desbloqueo;

	bool leer(const std::string& carpetaUi)
	{
		por.clear();
		finalTexto.clear();
		creditos.clear();
		std::ifstream f(carpetaUi + "dialogos.txt");
		std::string l;
		int n = 0;
		while (std::getline(f, l)) {
			if (!l.empty() && l.back() == '\r') l.pop_back();
			if (l.empty() || l[0] == '#') continue;
			std::vector<std::string> p;
			size_t a = 0, b;
			while ((b = l.find('|', a)) != std::string::npos) {
				p.push_back(l.substr(a, b - a));
				a = b + 1;
			}
			p.push_back(l.substr(a));
			if (p.size() < 3 || p[0].empty()) continue;
			const Linea x{ p[2], p.size() > 3 ? p[3] : std::string() };
			if (p[0] == "_final") {
				if (p[1] == "texto") finalTexto.push_back(x);
				if (p[1] == "creditos") creditos.push_back(x);
				if (p[1] == "desbloqueo") desbloqueo = x;
			} else if (p[1] == "intro") {
				por[p[0]].intro.push_back(x);
			} else if (p[1] == "pierde") {
				por[p[0]].pierde = x;
			} else if (p[1] == "gana") {
				por[p[0]].gana = x;
			} else {
				continue;
			}
			n++;
		}
		fprintf(stderr, "[dialogos] %d lineas, %d personajes (%s)\n", n, (int)por.size(), (carpetaUi + "dialogos.txt").c_str());
		return n > 0;
	}

	const Personaje* de(const std::string& id) const
	{
		auto it = por.find(id);
		return it == por.end() ? nullptr : &it->second;
	}
};

inline Dialogos& base()
{
	static Dialogos d;
	return d;
}

inline bool& cierreEnResultado()
{
	static bool v = true;
	return v;
}

// Modo Desafio (premio del final): disponible al completar la historia; activo = el jugador lo encendio (L1 en elegir
// rival del modo historia). En un duelo de desafio el jugador empieza con LP_DESAFIO y el rival juega al nivel maximo
constexpr int LP_DESAFIO = 4000;
inline bool& desafioDisponible()
{
	static bool v = false;
	return v;
}
inline bool& desafioActivo()
{
	static bool v = false;
	return v;
}

// la linea de cierre del personaje "id": gano = gano el JUGADOR (el rival dice su linea de derrota)
inline std::string dialogoCierre(const std::string& id, bool gano)
{
	const Personaje* p = base().de(id);
	if (!p) return std::string();
	return (gano ? p->pierde : p->gana).v();
}

// ---- la escena (intro antes del duelo o el final de la historia)
enum Tipo { NINGUNA = 0, INTRO, FINAL };
enum EtapaFinal { F_TEXTO = 0, F_CREDITOS, F_DESBLOQUEO };

struct Escena {
	int tipo{ NINGUNA };
	std::string personaje;   // id (retrato y nombre)
	std::string nombreEs, nombreEn;
	int linea{ 0 };          // INTRO: la linea; FINAL: la linea dentro de la etapa
	int etapa{ F_TEXTO };    // FINAL
	float t{ 0.0f };         // s desde que se abrio
	float tLinea{ 0.0f };    // s en la linea actual (el texto se escribe letra a letra)

	bool activa() const { return tipo != NINGUNA; }
	const std::string& nombre() const { return idioma::ingles() && !nombreEn.empty() ? nombreEn : nombreEs; }

	// INTRO de un personaje; false si no tiene dialogo (se va directo al duelo)
	bool abrir(const std::string& id, const std::string& es, const std::string& en)
	{
		const Personaje* p = base().de(id);
		if (!p || p->intro.empty()) return false;
		tipo = INTRO;
		personaje = id;
		nombreEs = es;
		nombreEn = en;
		linea = 0;
		t = tLinea = 0.0f;
		fprintf(stderr, "[dialogos] intro de %s (%d lineas)\n", id.c_str(), (int)p->intro.size());
		return true;
	}
	// el final de la historia (texto con el retrato del jefe, creditos y lo desbloqueado)
	bool abrirFinal(const std::string& id, const std::string& es, const std::string& en)
	{
		if (base().finalTexto.empty()) return false;
		tipo = FINAL;
		personaje = id;
		nombreEs = es;
		nombreEn = en;
		linea = 0;
		etapa = F_TEXTO;
		t = tLinea = 0.0f;
		fprintf(stderr, "[dialogos] final de la historia\n");
		return true;
	}
	void cerrar() { tipo = NINGUNA; }

	// la linea que se muestra ahora
	const Linea* actual() const
	{
		const Dialogos& d = base();
		if (tipo == INTRO) {
			const Personaje* p = d.de(personaje);
			return p && linea < (int)p->intro.size() ? &p->intro[(size_t)linea] : nullptr;
		}
		if (tipo == FINAL) {
			if (etapa == F_TEXTO) return linea < (int)d.finalTexto.size() ? &d.finalTexto[(size_t)linea] : nullptr;
			if (etapa == F_DESBLOQUEO) return &d.desbloqueo;
		}
		return nullptr;
	}
	int total() const
	{
		if (tipo == INTRO) {
			const Personaje* p = base().de(personaje);
			return p ? (int)p->intro.size() : 0;
		}
		return etapa == F_TEXTO ? (int)base().finalTexto.size() : 1;
	}
	// caracteres visibles de la linea (se escribe a ~55 letras/s)
	size_t visibles() const { return (size_t)(tLinea * 55.0f); }
	bool lineaCompleta() const
	{
		const Linea* l = actual();
		return !l || visibles() >= l->v().size();
	}

	// s que tardan en subir los creditos
	float duracionCreditos() const { return 4.0f + 2.4f * (float)base().creditos.size(); }

	void cuadro(float dt)
	{
		if (!activa()) return;
		t += dt;
		tLinea += dt;
		if (tipo == FINAL && etapa == F_CREDITOS && t > duracionCreditos()) {
			etapa = F_DESBLOQUEO;
			linea = 0;
			tLinea = 0.0f;
		}
	}

	// el mando; devuelve true cuando la escena termino (el juego sigue: duelo o elegir rival)
	bool avanzar(uint32_t p)
	{
		if (!activa()) return true;
		if (tipo == INTRO && (p & control::B_CIRCULO)) {   // saltar la presentacion
			cerrar();
			return true;
		}
		if (tipo == FINAL && etapa == F_CREDITOS) {   // los creditos suben solos; X los adelanta al final
			if ((p & control::B_X) && t > 1.0f) {
				etapa = F_DESBLOQUEO;
				linea = 0;
				tLinea = 0.0f;
			}
			return false;
		}
		if (!(p & control::B_X) || tLinea < 0.15f) return false;
		if (!lineaCompleta()) {   // X con la linea a medias: se completa
			tLinea = 1000.0f;
			return false;
		}
		linea++;
		tLinea = 0.0f;
		if (linea < total()) return false;
		if (tipo == FINAL && etapa == F_TEXTO) {
			etapa = F_CREDITOS;
			linea = 0;
			t = 0.0f;
			return false;
		}
		cerrar();
		return true;
	}
};

inline Escena& escena()
{
	static Escena e;
	return e;
}

}  // namespace historia_dialogos
