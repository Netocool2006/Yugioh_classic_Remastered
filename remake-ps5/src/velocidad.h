/*
 * Yugioh Classic Remaster (rama classic-resultado): velocidad de las animaciones del duelo (punto 5 de la auditoria).
 *
 * - Opcion del jugador: Normal / Rapida / Muy rapida. Escala TODAS las esperas fijas del duelo: el turno del rival
 *   (lo que piensa), la cadena (esperaEfecto de cada eslabon), el robo y el reparto, los ataques, las activaciones
 *   de magias/trampas y el sorteo. La aplica velocidad_juego.h (dentro de la clase del juego).
 * - Se guarda en opciones.txt con la clave  velocidad_anim=0|1|2  (0 Normal, 1 Rapida, 2 Muy rapida).
 *
 * PARA LA PANTALLA DE OPCIONES (rama classic-portada, menu_principal.h) solo hace falta:
 *   - al leer cada linea "clave valor" / "clave=valor":   velocidad::leerClave(clave, x);
 *   - al guardar, una linea mas:                          fprintf(f, "%s\n", velocidad::linea().c_str());
 *   - la fila del menu:  velocidad::nombre(velocidad::nivel())  y  velocidad::poner((velocidad::nivel() + 1) % velocidad::NIVELES)
 *   Sin eso tambien funciona: velocidad::leer(rutaOpciones) relee el archivo al empezar cada duelo, y
 *   velocidad::guardar(ruta) cambia solo esa clave conservando las demas lineas.
 *
 * - En el duelo, con el mando (velocidad::Control): mantener R2 mientras hay una animacion o juega el rival la acelera
 *   (x3 sobre la velocidad elegida) y un toque corto de R2 la salta (la animacion en curso corre muy rapido hasta
 *   terminar). Cuando el jugador tiene el control, R2 sigue siendo el zoom de la camara de siempre.
 */
#pragma once

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "idioma.h"

namespace velocidad {

enum { NORMAL = 0, RAPIDA = 1, MUY_RAPIDA = 2, NIVELES = 3 };
static constexpr const char* CLAVE = "velocidad_anim";

inline int& nivelRef() { static int v = NORMAL; return v; }
inline int nivel() { return nivelRef(); }
inline void poner(int n) { nivelRef() = std::max(0, std::min(n, NIVELES - 1)); }

// cuanto mas rapido corren los relojes de las animaciones y las esperas
inline float factor(int n)
{
	static const float F[NIVELES] = { 1.0f, 1.75f, 2.75f };
	return F[std::max(0, std::min(n, NIVELES - 1))];
}
inline float factor() { return factor(nivel()); }

// el nombre visible (ES/EN)
inline const char* nombre(int n)
{
	switch (n) {
	case RAPIDA: return idioma::t("Rápida", "Fast");
	case MUY_RAPIDA: return idioma::t("Muy rápida", "Very fast");
	default: return idioma::t("Normal", "Normal");
	}
}
inline const char* titulo() { return idioma::t("Velocidad de animaciones", "Animation speed"); }

// la linea de opciones.txt
inline std::string linea() { return std::string(CLAVE) + "=" + std::to_string(nivel()); }

// una clave ya leida de opciones.txt: true si era la de la velocidad
inline bool leerClave(const std::string& clave, float x)
{
	if (clave != CLAVE) return false;
	poner((int)(x + 0.5f));
	return true;
}

// lee la velocidad de opciones.txt ("clave valor" o "clave=valor"); sin la clave queda la que estaba
inline int leer(const std::string& ruta)
{
	std::ifstream f(ruta);
	std::string l;
	while (std::getline(f, l)) {
		for (char& c : l)
			if (c == '=') c = ' ';
		std::istringstream s(l);
		std::string clave;
		float x;
		if (s >> clave >> x) leerClave(clave, x);
	}
	return nivel();
}

// escribe la clave en opciones.txt conservando las demas lineas
inline bool guardar(const std::string& ruta)
{
	std::vector<std::string> lineas;
	{
		std::ifstream f(ruta);
		std::string l;
		while (std::getline(f, l))
			if (l.rfind(CLAVE, 0) != 0 && !l.empty()) lineas.push_back(l);
	}
	lineas.push_back(linea());
	FILE* f = fopen(ruta.c_str(), "w");
	if (!f) {
		fprintf(stderr, "[velocidad] no se pudo guardar %s\n", ruta.c_str());
		return false;
	}
	for (const std::string& l : lineas) fprintf(f, "%s\n", l.c_str());
	fclose(f);
	return true;
}

// ---- R2 en el duelo: mantener acelera, un toque salta la animacion en curso
static constexpr float TOQUE = 0.25f;     // s: soltar antes es un toque (saltar); mantener mas, acelerar
static constexpr float ACELERA = 3.0f;    // x sobre la velocidad elegida mientras se mantiene
static constexpr float SALTO = 14.0f;     // x mientras se salta una animacion
static constexpr float SALTO_MAX = 2.0f;  // s reales como mucho (por si la animacion no termina nunca)

struct Control {
	float abajo{ 0.0f };       // s con R2 apretado
	float saltando{ 0.0f };    // s reales que le quedan al salto
	bool antes{ false };
	bool consume{ false };     // R2 es de la velocidad (no del zoom) en este cuadro
	float escala{ 1.0f };

	// cada cuadro. r2: el gatillo (0..1); animando: hay una animacion o una espera del duelo en curso; rival: juega el
	// rival (piensa). Devuelve la escala de los relojes del duelo
	float cuadro(float r2, bool animando, bool rival, float dt)
	{
		const bool aprieta = r2 > 0.55f;
		const bool ocupado = animando || rival;
		bool toque = false;
		if (aprieta) {
			abajo += dt;
		} else {
			toque = antes && abajo > 0.0f && abajo < TOQUE;
			abajo = 0.0f;
		}
		antes = aprieta;
		if (toque && ocupado) {
			saltando = SALTO_MAX;
			fprintf(stderr, "[velocidad] R2: saltar la animacion\n");
		}
		saltando = animando ? std::max(0.0f, saltando - dt) : 0.0f;
		escala = factor();
		if (aprieta && abajo >= TOQUE && ocupado) escala *= ACELERA;
		if (saltando > 0.0f) escala = std::max(escala, SALTO);
		consume = ocupado && (aprieta || saltando > 0.0f);
		return escala;
	}
	void reiniciar() { abajo = saltando = 0.0f; antes = consume = false; escala = factor(); }
};

}  // namespace velocidad
