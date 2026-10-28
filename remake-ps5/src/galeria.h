/*
 * Yugioh Classic Remaster - GALERIA DE FIGURAS 3D (rama classic-galeria): SOLO logica (sin Vulkan). La dibuja
 * interfaz.h (galeria_ui.h, incluido dentro de interfaz::Interfaz) y la escena 3D (pedestal, luz, la figura girando y
 * su ataque) es de galeria_juego.h (dentro de la clase del juego).
 *
 * Se entra desde LISTA DE CARTAS (constructor.h, soloVer): Cuadrado abre la galeria (en la figura de la carta elegida
 * si la tiene) y R3 "Ver figura" (solo si la carta tiene figura). Circulo vuelve a la lista. NO hay boton propio en el
 * menu principal (el arte del menu tiene 5).
 *   cruz / stick izq   elegir figura (rejilla de 5)        L1 / R1    subir / bajar una pagina
 *   stick der          girar (izq/der) y acercar/alejar     L2 / R2    alejar / acercar      R3: vista inicial
 *   X                  su ataque (clip con esqueleto, o el de figuras.h: aliento, magia, rayo, espada, embestida)
 *   triangulo          detalle de la carta (cruz arriba/abajo desplaza el texto)          circulo: volver
 * Las figuras de cartas firma bloqueadas (mazo::Carta::bloqueada, la misma regla del constructor: se desbloquean al
 * vencer a su duenno en el modo historia) salen como silueta oscura con "Vence a X para desbloquear".
 * Memoria: las figuras ya estan cargadas una sola vez para el duelo (figuras.h); la galeria no carga nada mas, muestra
 * una a la vez la que ya esta en memoria.
 */
#pragma once

#include "constructor.h"
#include "mazo.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace galeria {

constexpr int COLS = 5, FILAS = 4;

// tipos de ataque (los de figuras.h: AtaqueFig)
enum TipoAtaque { AT_EMBESTIDA = 0, AT_ESPADA, AT_MAGIA, AT_ALIENTO, AT_RAYO };
inline const char* nombreAtaque(int a)
{
	static const char* const N[] = { "Embestida", "Espada", "Hechizo", "Aliento", "Rayo" };
	return a >= 0 && a <= 4 ? N[a] : "";
}

struct Entrada {
	std::vector<std::string> cartas;   // las cartas que usan esta figura (la primera por nombre la representa)
	std::string carta;                 // la que se muestra
	int fig{ -1 };                     // indice en figuras[] (figuras.h)
	int ataque{ AT_EMBESTIDA };
	bool esqueleto{ false }, clipAtaque{ false };
};

// lo que paso en el cuadro (para los sonidos)
enum : uint32_t { EV_MOVIO = 1u << 0, EV_ATACA = 1u << 1, EV_BLOQUEADA = 1u << 2, EV_DETALLE = 1u << 3, EV_CERRO = 1u << 4, EV_NADA_MAS = 1u << 5 };

class Galeria {
public:
	const mazo::Coleccion* col{ nullptr };
	std::vector<Entrada> entradas;
	std::unordered_map<std::string, int> entradaDeCarta;   // id de carta -> entradas[]
	bool abierta{ false }, detalle{ false };
	bool desdeLista{ false };   // se abrio desde la Lista de cartas (circulo vuelve ahi)
	int cursor{ 0 }, primeraFila{ 0 }, scrollTexto{ 0 };
	float t{ 0.0f }, tCursor{ 0.0f }, tAparece{ 0.0f }, tAtaque{ 99.0f }, tDetalle{ 0.0f }, tAviso{ 99.0f };
	std::string aviso;
	// visor: la figura gira sola; el stick derecho la gira y acerca
	float giro{ 0.0f }, zoom{ 1.0f }, tQuieto{ 99.0f };

	bool tieneFigura(const std::string& carta) const { return entradaDeCarta.count(carta) != 0; }
	const Entrada* actual() const { return cursor >= 0 && cursor < (int)entradas.size() ? &entradas[(size_t)cursor] : nullptr; }
	const mazo::Carta* carta() const
	{
		const Entrada* e = actual();
		return e && col ? col->carta(e->carta) : nullptr;
	}
	const mazo::Carta* cartaDe(const Entrada& e) const { return col ? col->carta(e.carta) : nullptr; }
	bool bloqueada(const Entrada& e) const
	{
		const mazo::Carta* k = cartaDe(e);
		return k && k->bloqueada;
	}
	bool bloqueadaActual() const { return actual() && bloqueada(*actual()); }
	int desbloqueadas() const
	{
		int n = 0;
		for (const Entrada& e : entradas) n += bloqueada(e) ? 0 : 1;
		return n;
	}
	bool atacando() const { return tAtaque < 3.9f; }

	// las entradas (una por figura) y el indice por carta; las cartas sin dato en la coleccion no entran
	void preparar(const mazo::Coleccion* c, std::vector<Entrada> e)
	{
		col = c;
		entradas.clear();
		for (Entrada& x : e) {
			if (x.cartas.empty()) continue;
			entradas.push_back(std::move(x));
		}
		ordenar();
	}

	// por nombre (en el idioma actual) de la carta que representa a cada figura
	void ordenar()
	{
		if (!col) return;
		const std::string antes = actual() ? actual()->carta : std::string();
		auto nombre = [&](const std::string& id) {
			const mazo::Carta* k = col->carta(id);
			return k ? k->nombre : id;
		};
		for (Entrada& e : entradas) {
			std::sort(e.cartas.begin(), e.cartas.end(), [&](const std::string& a, const std::string& b) { return nombre(a) < nombre(b); });
			if (std::find(e.cartas.begin(), e.cartas.end(), e.carta) == e.cartas.end() || !col->carta(e.carta)) e.carta = e.cartas[0];
		}
		entradas.erase(std::remove_if(entradas.begin(), entradas.end(), [&](const Entrada& e) { return !col->carta(e.carta); }), entradas.end());
		std::sort(entradas.begin(), entradas.end(), [&](const Entrada& a, const Entrada& b) { return nombre(a.carta) < nombre(b.carta); });
		entradaDeCarta.clear();
		for (size_t i = 0; i < entradas.size(); i++)
			for (const std::string& id : entradas[i].cartas) entradaDeCarta[id] = (int)i;
		if (!antes.empty()) elegirCarta(antes);
	}

	void elegirCarta(const std::string& id)
	{
		const auto it = entradaDeCarta.find(id);
		if (it == entradaDeCarta.end()) return;
		cursor = it->second;
		entradas[(size_t)cursor].carta = id;   // (la carta elegida en la lista se muestra tal cual)
	}

	// abre en la figura de esa carta (si la tiene; si no, donde estaba)
	void abrir(const std::string& cartaElegida, bool lista)
	{
		ordenar();
		abierta = true;
		desdeLista = lista;
		detalle = false;
		if (!cartaElegida.empty()) elegirCarta(cartaElegida);
		cursor = std::clamp(cursor, 0, std::max(0, (int)entradas.size() - 1));
		t = 0.0f;
		tAviso = 99.0f;
		tAtaque = 99.0f;
		alCambiar();
		giro = 0.0f;
		zoom = 1.0f;
		tQuieto = 99.0f;
		seguirCursor();
	}
	void cerrar()
	{
		abierta = false;
		detalle = false;
	}
	void avisar(const std::string& s)
	{
		aviso = s;
		tAviso = 0.0f;
	}

	void alCambiar()
	{
		tCursor = 0.0f;
		tAparece = 0.0f;
		tAtaque = 99.0f;
		scrollTexto = 0;
	}
	void seguirCursor()
	{
		const int fila = cursor / COLS;
		if (fila < primeraFila) primeraFila = fila;
		if (fila >= primeraFila + FILAS) primeraFila = fila - FILAS + 1;
		const int filas = ((int)entradas.size() + COLS - 1) / COLS;
		primeraFila = std::clamp(primeraFila, 0, std::max(0, filas - FILAS));
	}

	// un cuadro: p = botones (bits de constructor::C_*, con la repeticion de la cruz); rx, ry: stick derecho;
	// l2, r2: gatillos (0..1). Devuelve EV_* para los sonidos. venceA: "Vence a X" de la carta bloqueada
	template <class VenceA>
	uint32_t avanzar(float dt, uint32_t p, float rx, float ry, float l2, float r2, VenceA venceA)
	{
		using namespace constructor;
		uint32_t ev = 0;
		t += dt;
		tCursor += dt;
		tAparece += dt;
		tAviso += dt;
		tDetalle += dt;
		if (tAtaque < 50.0f) tAtaque += dt;
		const int n = (int)entradas.size();
		// el visor: gira solo si no se toca el stick; stick der y gatillos giran y acercan
		const float ax = fabsf(rx) > 0.15f ? rx : 0.0f, ay = fabsf(ry) > 0.15f ? ry : 0.0f;
		if (ax != 0.0f || ay != 0.0f || l2 > 0.1f || r2 > 0.1f) tQuieto = 0.0f;
		else tQuieto += dt;
		giro += ax * 2.4f * dt;
		if (tQuieto > 2.5f && !atacando()) giro += 0.38f * dt * std::min(1.0f, (tQuieto - 2.5f) * 1.5f);
		zoom = std::clamp(zoom * expf((ay + l2 - r2) * 1.1f * dt), 0.42f, 1.75f);
		if (p & C_R3) {
			giro = 0.0f;
			zoom = 1.0f;
		}
		if (detalle) {
			if (p & C_ABAJO) scrollTexto++;
			if ((p & C_ARRIBA) && scrollTexto > 0) scrollTexto--;
			if (p & (C_TRIANGULO | C_CIRCULO)) {
				detalle = false;
				ev |= EV_DETALLE;
			}
			if ((p & C_X) && n > 0) ev |= atacar(venceA);
			return ev;
		}
		if (n > 0) {
			const int antes = cursor;
			const int colu = cursor % COLS;
			if ((p & C_IZQ) && colu > 0) cursor--;
			if ((p & C_DER) && colu < COLS - 1 && cursor + 1 < n) cursor++;
			if (p & C_ARRIBA) cursor = cursor >= COLS ? cursor - COLS : cursor;
			if (p & C_ABAJO) {
				if (cursor + COLS < n) cursor += COLS;
				else if (cursor / COLS < (n - 1) / COLS) cursor = n - 1;
			}
			if (p & C_L1) cursor = std::max(cursor % COLS, cursor - COLS * FILAS);
			if (p & C_R1) cursor = std::min(n - 1, cursor + COLS * FILAS);
			if (cursor != antes) {
				alCambiar();
				seguirCursor();
				ev |= EV_MOVIO;
			}
			if (p & C_X) ev |= atacar(venceA);
			if (p & C_TRIANGULO) {
				detalle = true;
				tDetalle = 0.0f;
				scrollTexto = 0;
				ev |= EV_DETALLE;
			}
		}
		if (p & C_CIRCULO) {
			cerrar();
			ev |= EV_CERRO;
		}
		return ev;
	}

	template <class VenceA>
	uint32_t atacar(VenceA venceA)
	{
		const Entrada* e = actual();
		if (!e) return 0;
		if (bloqueada(*e)) {
			const mazo::Carta* k = cartaDe(*e);
			avisar(k ? venceA(*k) + constructor::T(" para desbloquear") : constructor::T("Carta firma bloqueada"));
			return EV_BLOQUEADA;
		}
		if (atacando() && tAtaque < 2.0f) return 0;   // (deja terminar el golpe)
		tAtaque = 0.0f;
		tQuieto = 0.0f;
		return EV_ATACA;
	}
};

}   // namespace galeria
