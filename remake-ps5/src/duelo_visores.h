/*
 * Yugioh Classic Remaster (rama classic-duelo-ui): VISORES DE ZONAS Y CONSULTA DEL DUELO (auditoria vs Master Duel,
 * puntos 2, 7 y 9).
 *
 * Un panel de consulta con pestannas, como el de Master Duel, que se abre en cualquier momento del duelo (tambien en el
 * turno del rival) y no toca el duelo:
 *   REGISTRO     el historial del duelo (duelo_registro.h), de los dos jugadores
 *   CEMENTERIO   la lista completa del cementerio      | de los dos jugadores: CUADRADO cambia entre el tuyo y el
 *   DESTERRADAS  la lista de las cartas desterradas    | del rival
 *   EXTRA        el mazo de fusion (el del rival se ve boca abajo: solo cuantas tiene)
 * Botones (con el panel abierto): izq/der pestanna, arriba/abajo recorre la lista (mantener = rapido), CUADRADO lado,
 * X detalle grande de la carta elegida (el de control.h: el mismo Triangulo de la mesa), CIRCULO o L3 cierra.
 * Se abre: L3 (el registro) o X sobre el cementerio, las desterradas o el extra de la mesa (su lista).
 *
 * Solo logica (sin Vulkan). El estado vive en el juego (duelo_consulta.h) y lo dibuja duelo_consulta_ui.h.
 */
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "control.h"
#include "duelo_cadena.h"
#include "duelo_registro.h"

namespace visores {

enum Pestana { P_REGISTRO = 0, P_CEMENTERIO, P_DESTERRADAS, P_EXTRA, NUM_PESTANAS };

inline const char* nombrePestana(int p, bool en)
{
	static const char* const ES[] = { "REGISTRO", "CEMENTERIO", "DESTERRADAS", "EXTRA" };
	static const char* const EN[] = { "DUEL LOG", "GRAVEYARD", "BANISHED", "EXTRA DECK" };
	return (en ? EN : ES)[std::max(0, std::min(p, NUM_PESTANAS - 1))];
}

// la pestanna de la zona de la mesa (-1: no tiene visor)
inline int pestanaDeZona(int z)
{
	if (control::esTipo(z, "cementerio")) return P_CEMENTERIO;
	if (control::esTipo(z, "desterrado")) return P_DESTERRADAS;
	if (control::esTipo(z, "fusion")) return P_EXTRA;
	return -1;
}

// la lista que muestra una pestanna de cartas (la de arriba de la pila primero, como se ve en la mesa)
inline std::vector<std::string> lista(const control::Mando& m, int pestana, int lado)
{
	const std::vector<std::string>* v = nullptr;
	if (pestana == P_CEMENTERIO) v = lado ? &m.cementerioR : &m.cementerioJ;
	else if (pestana == P_DESTERRADAS) v = lado ? &m.desterradosR : &m.desterradosJ;
	else if (pestana == P_EXTRA) v = lado ? &m.extraR : &m.extraJ;
	if (!v) return {};
	std::vector<std::string> r(v->rbegin(), v->rend());
	if (pestana == P_EXTRA && lado == 1) std::fill(r.begin(), r.end(), std::string());   // (el extra del rival: boca abajo)
	return r;
}

struct Estado {
	bool abierto{ false };
	int pestana{ P_REGISTRO }, lado{ 0 };
	int sel{ 0 };          // fila elegida (en el registro: desde la mas nueva, 0 = la ultima)
	float t{ 0.0f };       // s desde que se abrio (la interfaz lo anima)
	float tCambio{ 99.0f };   // s desde el ultimo cambio de pestanna / lado
};

// todo lo de la consulta del duelo que lleva el juego y lee la interfaz
struct Consulta {
	Estado vis;
	registro::Registro reg;
	cadena::Pila pila;
	cadena::Jugables jug;
	size_t vistos{ 0 };    // sucesos del duelo ya pasados al registro y a la pila
	int pantallaAntes{ -1 };

	void abrir(int pestana, int lado)
	{
		vis.abierto = true;
		vis.pestana = pestana;
		vis.lado = lado;
		vis.sel = 0;
		vis.t = 0.0f;
		vis.tCambio = 99.0f;
		if (pestana == P_REGISTRO) reg.nuevas = 0;
	}
	void cerrar() { vis.abierto = false; }

	int filas(const control::Mando& m) const
	{
		if (vis.pestana == P_REGISTRO) return (int)reg.lineas.size();
		return (int)lista(m, vis.pestana, vis.lado).size();
	}
	// la carta de la fila elegida ("" si no hay o esta oculta)
	std::string cartaElegida(const control::Mando& m) const
	{
		if (vis.pestana == P_REGISTRO) {
			const int n = (int)reg.lineas.size();
			if (vis.sel < 0 || vis.sel >= n) return {};
			const registro::Linea& l = reg.lineas[(size_t)(n - 1 - vis.sel)];
			if (l.tipo == ia::S_TURNO || l.tipo == ia::S_FASE || l.tipo == ia::S_DANNO || l.tipo == ia::S_LP || l.tipo == ia::S_ROBO) return {};
			return l.carta;
		}
		const std::vector<std::string> v = lista(m, vis.pestana, vis.lado);
		return vis.sel >= 0 && vis.sel < (int)v.size() ? v[(size_t)vis.sel] : std::string();
	}

	// los botones con el panel abierto. Devuelve true si el panel se quedo con ellos. m: para abrir el detalle
	bool mando(control::Mando& m, uint32_t p, float dt)
	{
		vis.t += dt;
		vis.tCambio += dt;
		if (!vis.abierto) return false;
		if (m.estado == control::DETALLE) return false;   // (el detalle grande es el de control.h: se cierra solo)
		if (p & (control::B_CIRCULO | control::B_L3)) {
			cerrar();
			return true;
		}
		const int n = filas(m);
		if (p & control::B_IZQ) vis.pestana = (vis.pestana + NUM_PESTANAS - 1) % NUM_PESTANAS, vis.sel = 0, vis.tCambio = 0.0f;
		if (p & control::B_DER) vis.pestana = (vis.pestana + 1) % NUM_PESTANAS, vis.sel = 0, vis.tCambio = 0.0f;
		if ((p & control::B_CUADRADO) && vis.pestana != P_REGISTRO) vis.lado = 1 - vis.lado, vis.sel = 0, vis.tCambio = 0.0f;
		if (vis.pestana == P_REGISTRO) reg.nuevas = 0;
		if (n > 0) {
			if (p & control::B_ABAJO) vis.sel = std::min(n - 1, vis.sel + 1);
			if (p & control::B_ARRIBA) vis.sel = std::max(0, vis.sel - 1);
		}
		vis.sel = std::max(0, std::min(vis.sel, std::max(0, filas(m) - 1)));
		if (p & (control::B_X | control::B_TRIANGULO)) {
			const std::string c = cartaElegida(m);
			if (!c.empty() && c != "?") m.abrirDetalleCarta(c);   // (rama classic-arreglo-visor: vuelve al estado de verdad)
		}
		return true;   // (abierto: ningun boton llega a la mesa)
	}
};

}  // namespace visores
