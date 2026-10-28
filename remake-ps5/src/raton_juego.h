// Yugioh Classic Remaster (rama classic-pc): del puntero a los pulsos del mando. Solo se usa en Windows (yugireload.cpp,
// #if defined(_WIN32)); el resto de la logica (zonas, ejes) esta en raton.h y la interfaz las registra al dibujar.
//
//  - Mover el puntero sobre una zona lleva el foco hasta ella (un pulso de la cruceta cada raton::RITMO s, o el foco directo
//    en la mesa del duelo). Si el teclado o el mando mueven el foco despues, el puntero no lo vuelve a tirar hasta moverse.
//  - Clic izquierdo: lleva el foco a la zona y aprieta su boton (X por defecto); clic derecho = circulo, central = triangulo.
//  - Rueda: arriba / abajo en los menus y listas (y el detalle); L1 / R1 en la mesa (la camara).
//  - Si la zona no se alcanza (un eje que no avanza: nivel bloqueado, opcion apagada...) se abandona tras 3 intentos.
#pragma once

#include "interfaz.h"
#include "raton.h"

#if defined(_WIN32)
#include "raton_plataforma.h"
#endif

namespace raton {

struct Juego {
	bool hoverActivo{ false }, clicPend{ false };
	float tPaso{ 0.0f }, tClic{ 0.0f };
	int sinProgreso{ 0 };
	float ultA{ -1.0f }, ultB{ -1.0f };
	int ultCur0{ -9999 }, ultCur1{ -9999 };
	bool hayRaton{ false };
	int dispositivo{ 0 };
	// para las pruebas (YUGI_RATON_LOG=1): lo ultimo que se hizo
	std::string ultimaAccion;

	static bool enRango(float v, float lo, float hi) { return v >= lo && v <= hi; }

	// los botones (control::B_*) que salen del puntero en este cuadro. Llamar una vez por cuadro, despues de leer el mando
	uint32_t paso(interfaz::Interfaz& ui, control::Mando& ctl, float dt)
	{
#if defined(_WIN32)
		raton_estado r;
		raton_tomar(&r);
		dispositivo = r.dispositivo;
		static const char* forzado = getenv("YUGI_DISPOSITIVO");   // pruebas: 0 PlayStation, 1 teclado, 2 Xbox (los iconos de la ayuda)
		if (forzado && forzado[0] >= '0' && forzado[0] <= '2') dispositivo = forzado[0] - '0';
		ui.dispositivo = dispositivo;
		hayRaton = r.ancho > 0.0f;
		if (!hayRaton) return 0;
		const float ex = ui.anchoPantalla() / r.ancho, ey = ui.altoPantalla() / r.alto;
		const glm::vec2 pos(r.x * ex, r.y * ey);
		const bool dentro = r.x >= 0.0f && r.y >= 0.0f && r.x <= r.ancho && r.y <= r.alto;
		return procesar(ui, ctl, dt, pos, dentro, r.movido != 0, r.izq != 0, r.der != 0, r.centro != 0, r.rueda);
#else
		(void)ui; (void)ctl; (void)dt;
		return 0;
#endif
	}

	// la logica pura (se puede probar con eventos sinteticos)
	uint32_t procesar(interfaz::Interfaz& ui, control::Mando& ctl, float dt, glm::vec2 pos, bool dentro, bool movido, bool izq, bool der, bool centro, int rueda)
	{
		const uint32_t r = procesarSin(ui, ctl, dt, pos, dentro, movido, izq, der, centro, rueda);
		if (r || izq || der || centro) {   // (el registro: que boton salio del puntero y sobre que zona)
			const raton::Zona* z = dentro ? ui.rz.bajo(pos) : nullptr;
			fprintf(stderr, "[raton] (%.0f,%.0f)%s%s%s botones 0x%x zona %s [%.0f,%.0f %.0f,%.0f] ejes (%d/%d, %d/%d) foco %s\n", pos.x, pos.y, izq ? " izq" : "", der ? " der" : "",
			        centro ? " centro" : "", (unsigned)r, z ? "si" : "no", z ? z->a.x : 0.0f, z ? z->a.y : 0.0f, z ? z->b.x : 0.0f, z ? z->b.y : 0.0f, z ? z->e[0].pos : 0,
			        z ? z->e[0].cur : 0, z ? z->e[1].pos : 0, z ? z->e[1].cur : 0, ctl.idFoco().c_str());
		}
		return r;
	}

	uint32_t procesarSin(interfaz::Interfaz& ui, control::Mando& ctl, float dt, glm::vec2 pos, bool dentro, bool movido, bool izq, bool der, bool centro, int rueda)
	{
		uint32_t out = 0;
		tPaso -= dt;
		if (der) out |= control::B_CIRCULO;
		if (centro) out |= control::B_TRIANGULO;
		if (rueda != 0) {
			const int n = std::min(std::abs(rueda), 3);
			if (ctl.estado == control::DETALLE || ui.rz.tapas > 0) out |= rueda > 0 ? control::B_ARRIBA : control::B_ABAJO;
			else out |= rueda > 0 ? control::B_L1 : control::B_R1;
			(void)n;
		}
		const raton::Zona* z = dentro ? ui.rz.bajo(pos) : nullptr;
		if (movido) {
			hoverActivo = z != nullptr && z->clase == 0;
			sinProgreso = 0;
		}
		if (izq) {
			clicPend = z != nullptr;
			tClic = raton::PACIENCIA;
			sinProgreso = 0;
		}
		if (clicPend) {
			tClic -= dt;
			if (tClic <= 0.0f) clicPend = false;
		}
		if (!z || (!hoverActivo && !clicPend)) return out;
		if (z->clase == 1) {   // el icono de un boton de la ayuda: solo clic
			if (clicPend) {
				out |= z->clic;
				clicPend = false;
				ultimaAccion = "icono";
			}
			return out;
		}
		bool llego;
		if (z->fTipo >= 0) {
			llego = irAFoco(ctl, *z);
		} else {
			llego = z->llego();
			if (!llego && tPaso <= 0.0f) {
				// sin avance en los ultimos pulsos hacia esta misma zona: se abandona
				if (z->a.x == ultA && z->a.y == ultB && z->e[0].cur == ultCur0 && z->e[1].cur == ultCur1) {
					if (++sinProgreso >= 3) {
						hoverActivo = false;
						clicPend = false;
						sinProgreso = 0;
						ultimaAccion = "abandona";
						return out;
					}
				} else {
					sinProgreso = 0;
				}
				ultA = z->a.x;
				ultB = z->a.y;
				ultCur0 = z->e[0].cur;
				ultCur1 = z->e[1].cur;
				out |= z->paso();
				tPaso = clicPend ? raton::RITMO_CLIC : raton::RITMO;
			}
		}
		if (llego) {
			hoverActivo = false;
			sinProgreso = 0;
			if (clicPend) {
				clicPend = false;
				out |= z->clic;
				ultimaAccion = "clic";
			}
		}
		return out;
	}

	// la mesa del duelo: el foco directo (si el estado lo permite); true cuando el foco ya esta en la zona
	bool irAFoco(control::Mando& ctl, const raton::Zona& z)
	{
		const control::Foco f{ z.fTipo, z.fI };
		if (ctl.foco == f) return true;
		bool vale = true;
		switch (ctl.estado) {
		case control::NAVEGAR: break;
		case control::ELEGIR_ZONA:
		case control::ELEGIR_OBJETIVO:
		case control::ELEGIR_TRIBUTOS: vale = f.tipo == control::F_ZONA && std::find(ctl.validas.begin(), ctl.validas.end(), f.i) != ctl.validas.end(); break;
		case control::ELEGIR_CARTA: vale = std::find(ctl.validasFoco.begin(), ctl.validasFoco.end(), f) != ctl.validasFoco.end(); break;
		default: vale = false; break;
		}
		if (!vale) {
			hoverActivo = false;
			clicPend = false;
			return false;
		}
		ctl.foco = f;
		ultimaAccion = "foco";
		return true;
	}
};

}   // namespace raton
