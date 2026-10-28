// rama classic-bp-terminar: la orientacion de las figuras 3D en la mesa (regla del usuario): las figuras MIRAN A LA
// PANTALLA (la camara de juego, OJO); solo cuando atacan se voltean hacia el objetivo, atacan y vuelven a mirar a la
// pantalla. No miran al rival en reposo, ni en defensa, ni al aparecer, ni al terminar la entrada de una carta clave;
// igual para el jugador y el rival, con esqueleto o rigidas. Son funciones puras (sin Vulkan): figuras.h
// (matrizFigura) y entrada_clave_juego.h (poseEntradaClave) hacen la cuenta con ellas y la bateria desde el front
// (CU-FT-FIGURA-ORIENTACION*) mide los mismos angulos.
// Convencion: el frente del modelo glTF es +Z; girar "giro" radianes alrededor de Y lo lleva a (sin giro, 0, cos giro).
#pragma once

#include <cmath>

#include <glm/glm.hpp>

namespace orientacion {

// la camara de juego (yugireload.cpp: OJO): CamaraJuego de Unreal ue(0,12.8,6.4), con el tablero x1.4 y un poco mas lejos
constexpr float ESCALA_TABLERO = 1.4f;
inline glm::vec3 ojoJuego() { return glm::vec3(0.0f, 6.4f, 12.8f) * (ESCALA_TABLERO * 1.08f); }

inline float suaveO(float a, float b, float x)
{
	const float t = glm::clamp((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

// angulo (alrededor de Y) que pone el frente del modelo (+Z) mirando hacia d
inline float giroHacia(glm::vec3 d) { return atan2f(d.x, d.z); }
// el frente del modelo (en el plano de la mesa) con ese giro
inline glm::vec3 frente(float giro) { return glm::vec3(sinf(giro), 0.0f, cosf(giro)); }

// en reposo (y en defensa, al aparecer, despues de la entrada clave): de frente a la pantalla
inline float giroReposo(glm::vec3 base, glm::vec3 ojo) { return giroHacia(ojo - base); }

// en defensa, los bipedos rigidos se ladean un poco (grados; figuras.h): sigue de frente a la pantalla
constexpr float GIRA_DEFENSA_BIPEDO = 16.0f;

// durante el ataque (tg: segundos de la animacion del combate): cuanto mira al otro (0 = a la pantalla, 1 = al otro).
// Gira en 0.22 s, se queda mirando mientras ataca y vuelve a la pantalla entre 1.25 y 1.55 s
inline float pesoMiraAtaque(float tg) { return suaveO(0.0f, 0.22f, tg) * (1.0f - suaveO(1.25f, 1.55f, tg)); }
constexpr float FIN_MIRA_ATAQUE = 1.55f;

// el atacante (y el objetivo) gira de "giro" hacia el otro, por el lado mas corto, y vuelve
inline float giroAtaque(float giro, glm::vec3 desde, glm::vec3 hacia, float tg)
{
	glm::vec3 d = hacia - desde;
	const float largo = glm::length(d);
	d = largo > 1e-3f ? d / largo : glm::vec3(0.0f, 0.0f, -1.0f);
	return giro + remainderf(giroHacia(d) - giro, 2.0f * 3.14159265358979f) * pesoMiraAtaque(tg);
}

// la entrada de una carta clave (u: s desde que aparece la figura): un giro extra que se apaga (al terminar, 0: de
// vuelta a la pantalla). Rigida: vueltas completas; con esqueleto: media vuelta que se deshace
inline float giroEntrada(bool corta, bool esqueleto, bool dios, float u)
{
	if (u < 0.0f) return 0.0f;
	const float dosPi = 2.0f * 3.14159265358979f;
	if (corta) {
		const float k = suaveO(0.0f, 0.7f, u);
		return (1.0f - k) * (1.0f - k) * dosPi;
	}
	const float k = suaveO(0.0f, dios ? 1.5f : 0.9f, u);
	if (!esqueleto) return (1.0f - k) * (1.0f - k) * dosPi * (dios ? 1.0f : 1.5f);
	return (1.0f - k) * (1.0f - k) * 3.14159265358979f;
}

// grados entre el frente con ese giro y la direccion d (en el plano de la mesa)
inline float grados(float giro, glm::vec3 d)
{
	d.y = 0.0f;
	if (glm::length(d) < 1e-5f) return 0.0f;
	const float c = glm::clamp(glm::dot(frente(giro), glm::normalize(d)), -1.0f, 1.0f);
	return acosf(c) * 180.0f / 3.14159265358979f;
}

}  // namespace orientacion
