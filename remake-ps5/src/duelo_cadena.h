/*
 * Yugioh Classic Remaster (rama classic-duelo-ui): CADENA VISIBLE Y CARTAS JUGABLES (auditoria vs Master Duel, punto 9).
 *
 *  - Pila: los eslabones de la cadena (C1, C2...) con su carta, tal como la mesa los presenta (S_ACTIVA_MT /
 *    S_ACTIVA_EFECTO traen el numero de eslabon). Al resolverse (S_RESUELVE, de arriba hacia abajo: orden inverso)
 *    cada eslabon se marca y se va con su animacion; los negados quedan tachados. La dibuja duelo_consulta_ui.h.
 *  - Jugables: lo que el jugador puede jugar o activar ahora (de la mano y del campo), para el brillo azul de Master
 *    Duel. Activaciones: ia::activables (el mismo motor que la pregunta de respuesta). Ademas, en su turno, lo que su
 *    menu deja hacer (invocar, colocar, voltear, atacar: control::Mando::opcionesPara con alguna opcion activa).
 * Solo logica (sin Vulkan).
 */
#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "control.h"
#include "ia.h"

namespace cadena {

constexpr float T_ENTRA = 0.30f;   // s de la llegada de un eslabon
constexpr float T_SALE = 0.55f;    // s de la salida al resolverse

struct Eslabon {
	int n{ 1 }, lado{ 0 };
	std::string carta;
	bool deMonstruo{ false }, negado{ false };
	float t{ 0.0f };      // s desde que llego
	float tFin{ -1.0f };  // s desde que se resolvio (-1: en la pila)
};

struct Pila {
	std::vector<Eslabon> v;   // de abajo (C1) hacia arriba
	float tVacia{ 99.0f };    // s desde que la pila quedo vacia (la interfaz desvanece el marco)

	void limpiar()
	{
		v.clear();
		tVacia = 99.0f;
	}
	bool visible() const { return !v.empty(); }
	int vivos() const
	{
		int n = 0;
		for (const Eslabon& e : v) n += e.tFin < 0.0f;
		return n;
	}

	void suceso(const ia::Suceso& s)
	{
		switch (s.tipo) {
		case ia::S_ACTIVA_MT:
		case ia::S_ACTIVA_EFECTO: {
			const int n = std::max(1, s.valor);
			// una cadena nueva (C1) despues de que la anterior termino: se saca lo que quedaba
			if (n == 1) v.erase(std::remove_if(v.begin(), v.end(), [](const Eslabon& e) { return e.tFin >= 0.0f; }), v.end());
			v.erase(std::remove_if(v.begin(), v.end(), [&](const Eslabon& e) { return e.n >= n; }), v.end());
			Eslabon e;
			e.n = n;
			e.lado = s.lado;
			e.carta = s.carta;
			e.deMonstruo = s.tipo == ia::S_ACTIVA_EFECTO;
			v.push_back(e);
			break;
		}
		case ia::S_NEGADO:
			for (auto it = v.rbegin(); it != v.rend(); ++it)
				if (it->tFin < 0.0f && it->carta == s.carta) {
					it->negado = true;
					break;
				}
			break;
		case ia::S_RESUELVE:
			for (Eslabon& e : v)
				if (e.n == s.valor && e.tFin < 0.0f) e.tFin = 0.0f;
			break;
		case ia::S_TURNO:
		case ia::S_FIN: limpiar(); break;
		default: break;
		}
	}

	// cadenaViva: el duelo todavia tiene eslabones (si no, lo que quede en la pila se va solo)
	void avanzar(float dt, bool cadenaViva, bool alDia)
	{
		for (Eslabon& e : v) {
			e.t += dt;
			if (e.tFin >= 0.0f) e.tFin += dt;
		}
		// (sin cadena en el duelo y todo presentado: una activacion sola, sin S_RESUELVE visible, tambien se va)
		if (!cadenaViva && alDia)
			for (Eslabon& e : v)
				if (e.tFin < 0.0f && e.t > 0.9f) e.tFin = 0.0f;
		v.erase(std::remove_if(v.begin(), v.end(), [](const Eslabon& e) { return e.tFin > T_SALE; }), v.end());
		tVacia = v.empty() ? tVacia + dt : 0.0f;
	}
};

// ------------------------------------------------------------------------------------------------- jugables
struct Jugables {
	std::vector<int> mano;    // indices de la mano del jugador (control::Mando::manoJ)
	std::vector<int> zonas;   // zonas de la mesa (control::ZONAS_MESA)
	std::string clave;        // cuando cambia, se recalcula (no en cada cuadro)

	bool enMano(int k) const { return std::find(mano.begin(), mano.end(), k) != mano.end(); }
	bool enZona(int z) const { return std::find(zonas.begin(), zonas.end(), z) != zonas.end(); }

	static int zonaDe(const ia::Lugar& g)
	{
		if (g.lado != 0) return -1;
		if (g.donde == ia::EN_CAMPO) return control::zonaPorId("jugador_campo");
		if (g.donde == ia::EN_MT) return control::zonaPorId("jugador_magia_" + std::to_string(g.i));
		if (g.donde == ia::EN_MONSTRUO) return control::zonaPorId("jugador_monstruo_" + std::to_string(g.i));
		return -1;
	}

	// sucesos: cuantos presento la mesa (con el estado del mando, decide cuando recalcular)
	void calcular(const control::Mando& ctl, const ia::Duelo& d, size_t sucesos)
	{
		const bool preg = d.ef.preg.tipo == ia::P_RESPUESTA && d.ef.preg.lado == 0;
		const bool libre = !ctl.bloqueado || preg;
		char k[160];
		snprintf(k, sizeof(k), "%zu/%d/%d/%d/%d/%d/%d/%zu/%d", sucesos, d.turno, (int)d.fase, (int)ctl.bloqueado, (int)d.ef.preg.tipo, d.ef.preg.lado, (int)ctl.estado,
		         ctl.manoJ.size(), d.ganador);
		if (clave == k) return;
		clave = k;
		mano.clear();
		zonas.clear();
		if (d.ganador >= 0 || !libre || ctl.vista.pantalla != control::VistaDuelo::DUELO || ctl.vista.sorteo) return;
		// lo que se puede activar (como respuesta si el duelo pregunta al jugador)
		for (const ia::Lugar& g : ia::activables(d, 0, preg)) {
			if (g.donde == ia::EN_MANO) {
				// (activables da una opcion por carta repetida: brillan todas las copias)
				const std::string id = g.i >= 0 && g.i < (int)d.l[0].mano.size() ? d.l[0].mano[(size_t)g.i] : std::string();
				for (int i = 0; i < (int)ctl.manoJ.size() && i < (int)d.l[0].mano.size(); i++)
					if (d.l[0].mano[(size_t)i] == id && !enMano(i)) mano.push_back(i);
			} else {
				const int z = zonaDe(g);
				if (z >= 0 && !enZona(z)) zonas.push_back(z);
			}
		}
		if (preg || d.activo != 0 || d.decide() != 0) return;
		// en su turno: invocar, colocar, voltear, atacar (lo que el menu de la carta deja hacer)
		auto util = [](const std::vector<control::Opcion>& ops) {
			for (const control::Opcion& o : ops)
				if (o.activa && o.accion != control::AC_CANCELAR && o.accion != control::AC_POSICION && o.accion != control::AC_PASAR &&
				    !(o.accion == control::AC_JUGADA && o.jugada.tipo == ia::J_DESCARTAR))
					return true;
			return false;
		};
		for (int i = 0; i < (int)ctl.manoJ.size(); i++)
			if (!enMano(i) && util(ctl.opcionesPara({ control::F_MANO_J, i }))) mano.push_back(i);
		for (int z = 0; z < control::NUM_ZONAS_MESA; z++) {
			if (control::ZONAS_MESA[z].rival || enZona(z)) continue;
			if (!control::esTipo(z, "monstruo") && !control::esTipo(z, "magia") && !control::esTipo(z, "campo")) continue;
			if (util(ctl.opcionesPara({ control::F_ZONA, z }))) zonas.push_back(z);
		}
	}
};

}  // namespace cadena
