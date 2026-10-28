// Yugioh Classic Remaster (rama classic-pc): el raton, sin Vulkan ni sistema operativo (portable).
//
// La interfaz (interfaz.h y sus *_ui.h) registra, al dibujar cada cuadro, las ZONAS donde se puede apuntar con el
// puntero (una fila de menu, un boton de la puerta, una carta de la mano, una zona de la mesa, el icono de un boton de
// la ayuda...). El juego (yugireload.cpp, solo en Windows) mira cual queda bajo el puntero y la traduce a los mismos
// pulsos del mando que ya entiende cada pantalla:
//  - mover el puntero sobre una zona lleva el foco hasta ella con pulsos de la cruceta (un pulso cada RITMO s);
//  - clic izquierdo = la zona lleva el foco hasta ella y luego aprieta su boton (X por defecto);
//  - clic derecho = circulo (volver), clic central = triangulo; rueda = arriba / abajo, o L1 / R1 en la mesa.
// Una zona describe como se llega a ella con hasta dos EJES: "estoy en cur, la zona esta en pos; mas = el boton que
// sube, menos = el que baja" (una lista vertical es un eje con ARRIBA / ABAJO; una rejilla, dos). Las zonas de la mesa
// del duelo llevan en cambio el foco (fTipo / fI) que se pone directo. Cuando se dibujan varias capas (la mesa y un menu
// encima), la ultima en registrarse manda: un menu o pantalla que tapa llama a tapar() y borra lo de abajo.
// En la PS5 el registro se llena igual (es barato) pero nadie lo lee.
#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace raton {

constexpr float RITMO = 0.055f;   // s entre pulsos de navegacion del puntero
constexpr float RITMO_CLIC = 0.032f;   // y con un clic esperando llegar
constexpr float PACIENCIA = 0.8f;   // s que espera un clic a que el foco llegue a la zona

struct Eje {
	bool usa{ false };
	int pos{ 0 }, cur{ 0 };       // donde esta la zona y donde esta el foco ahora
	uint32_t mas{ 0 }, menos{ 0 };  // boton que sube / baja el indice (control::B_*)
};

struct Zona {
	glm::vec2 a{ 0.0f }, b{ 0.0f };   // esquina de arriba a la izquierda y de abajo a la derecha, en px de pantalla
	Eje e[2];
	uint32_t clic{ 0 };               // boton que aprieta el clic izquierdo cuando el foco ya esta (0 = ninguno)
	int fTipo{ -1 }, fI{ 0 };         // mesa del duelo: control::Foco al que lleva
	int clase{ 0 };                   // 0 normal; 1 = icono de la ayuda (no mueve el foco al pasar, solo clic)
	bool contiene(glm::vec2 p) const { return p.x >= a.x && p.x <= b.x && p.y >= a.y && p.y <= b.y; }
	bool llego() const
	{
		for (const Eje& x : e) {
			if (x.usa && x.pos != x.cur) return false;
		}
		return true;
	}
	// el boton que acerca el foco (0 = ya esta). El eje 0 primero, luego el 1
	uint32_t paso() const
	{
		for (const Eje& x : e) {
			if (x.usa && x.pos != x.cur) return x.pos > x.cur ? x.mas : x.menos;
		}
		return 0;
	}
};

struct Registro {
	// (el juego primero actualiza y despues dibuja: al actualizar, "zonas" y "tapas" son los del ultimo dibujo completo)
	std::vector<Zona> zonas;   // lo que se dibujo en el ultimo cuadro
	int tapas{ 0 };            // cuantas pantallas que tapan se dibujaron (la mesa y su ayuda solo se registran con 0)
	void empezarCuadro()
	{
		zonas.clear();
		tapas = 0;
	}
	// una pantalla que tapa todo lo de abajo (menu, portada, pausa...)
	void tapar()
	{
		zonas.clear();
		tapas++;
	}
	// la zona de arriba (la ultima registrada) que contiene el punto; nullptr si ninguna
	const Zona* bajo(glm::vec2 p) const
	{
		for (size_t k = zonas.size(); k-- > 0;) {
			if (zonas[k].contiene(p)) return &zonas[k];
		}
		return nullptr;
	}
};

// el eje de una lista (arriba = indice menor)
inline Eje ejeLista(int pos, int cur, uint32_t mas, uint32_t menos)
{
	Eje e;
	e.usa = true;
	e.pos = pos;
	e.cur = cur;
	e.mas = mas;
	e.menos = menos;
	return e;
}

// el estado del puntero que entrega la plataforma (Windows) una vez por cuadro
struct Puntero {
	bool hay{ false };            // la plataforma tiene raton
	bool movido{ false };         // se movio desde el cuadro anterior
	bool visible{ true };
	glm::vec2 pos{ 0.0f };        // px del area de la ventana (la misma escala de la pantalla del juego)
	bool izq{ false }, der{ false }, centro{ false };   // recien apretados en este cuadro
	int rueda{ 0 };               // muescas desde el cuadro anterior (+ = hacia arriba)
};

}   // namespace raton
