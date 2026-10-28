/*
 * Yugioh Classic Remaster (rama classic-cartas-clave): ENTRADA ESPECIAL de las cartas clave, al estilo de Master Duel.
 *
 * Cuando el jugador o el rival invocan una carta clave (firma de un personaje o emblematica con figura 3D), antes de
 * que la figura quede en su casilla se juega una secuencia corta:
 *  1. CORTE (2D): la pantalla se oscurece, entran bandas de cine, lineas de velocidad y un destello del color del
 *     atributo; la carta entra en primer plano boca abajo, se da vuelta y su nombre sale en grande (ES/EN).
 *  2. FIGURA (3D, si la carta tiene figura y "Figuras 3D" esta encendida): la camara se acerca a la casilla, la
 *     figura se materializa con su entrada (con esqueleto: su clip de ataque; rigida: crece girando y cae con polvo o
 *     energia) y los efectos de su clase: dragon (rugido y llamarada), lanzador de conjuros (circulo magico), Dioses
 *     Egipcios (mas larga: el cielo se abre, rayos, temblor de camara), fusion (vortice) y ritual (circulo ritual).
 *  Sin figura (o con las figuras apagadas) solo el corte 2D. La misma carta (del mismo lado) hace la entrada completa
 *  la primera vez del duelo; las siguientes, la version corta (1 s: destello, nombre y la figura girando).
 *
 * Este archivo: el estado y los tiempos (sin Vulkan; lo leen entrada_clave_juego.h, dentro de la clase del juego, y
 * entrada_clave_ui.h, dentro de interfaz::Interfaz). El reloj (t) corre con el dt del duelo: respeta velocidad.h
 * (Normal / Rapida / Muy rapida, R2 acelera o salta) y se detiene con la pausa.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "entrada_md.h"   // rama classic-md-entradas: las entradas de fusion y ritual de Master Duel

namespace entrada {

enum Clase { NORMAL = 0, DRAGON, MAGO, DIOS, FUSION, RITUAL };

struct Estado {
	bool activa{ false };
	std::string carta;      // id de la carta
	int zona{ -1 };         // zona de la mesa (control.h / interfaz::ZONAS)
	bool rival{ false };
	Clase clase{ NORMAL };
	std::string atributo;   // clave en espanol (LUZ, OSCURIDAD, ..., DIVINIDAD)
	bool corta{ false };    // version corta (la carta ya hizo su entrada en este duelo)
	bool con3D{ false };    // hay figura y las figuras estan encendidas
	bool saltando{ false }; // R2 la esta saltando (sin sonidos)
	bool trasMD{ false };   // (rama classic-md-entradas) llega de su fusion/ritual de MD: sin vortice ni circulo propios
	float t{ 0.0f }, dur{ 0.0f };
	float tFigura{ 0.0f };  // cuando termina el corte y aparece la figura
	std::vector<std::string> vistas;   // "lado:carta" que ya hicieron la entrada completa en este duelo
};
inline Estado& estado()
{
	static Estado e;
	return e;
}

// duracion del corte 2D (la carta en primer plano) y de toda la entrada
inline float duracionCorte(Clase c, bool con3D, bool corta)
{
	if (corta) return 0.0f;
	if (!con3D) return c == DIOS ? 2.6f : 2.0f;
	return c == DIOS ? 1.6f : 1.25f;
}
inline float duracion(Clase c, bool con3D, bool corta)
{
	if (corta) return 1.0f;
	if (!con3D) return duracionCorte(c, con3D, corta);
	switch (c) {
	case DIOS: return 4.0f;
	case DRAGON: return 3.3f;
	case FUSION:
	case RITUAL: return 3.4f;
	case MAGO: return 3.2f;
	default: return 3.0f;
	}
}

// la clase de la entrada segun la carta (clase: normal/efecto/fusion/ritual; raza y atributo en espanol)
inline Clase claseDe(const std::string& clase, const std::string& raza, const std::string& atributo)
{
	if (atributo == "DIVINIDAD" || atributo == "DIVINE" || raza == "Bestia Divina") return DIOS;
	if (clase == "fusion") return FUSION;
	if (clase == "ritual") return RITUAL;
	if (raza.rfind("Drag", 0) == 0) return DRAGON;   // (Dragón)
	if (raza == "Lanzador de Conjuros") return MAGO;
	return NORMAL;
}

// el sonido del corte por atributo (audio.txt: sfx_clave_<atributo>, se_monster_cutin_* de Master Duel)
inline const char* sonidoCorte(const std::string& a, Clase c)
{
	if (c == DIOS || a == "DIVINIDAD") return "sfx_clave_divinidad";
	if (a == "OSCURIDAD") return "sfx_clave_oscuridad";
	if (a == "LUZ") return "sfx_clave_luz";
	if (a == "TIERRA") return "sfx_clave_tierra";
	if (a == "AGUA") return "sfx_clave_agua";
	if (a == "FUEGO") return "sfx_clave_fuego";
	if (a == "VIENTO") return "sfx_clave_viento";
	return "sfx_clave_luz";
}

// el color del atributo (lineas de velocidad, destello, halo del nombre, energia)
struct Color { float r, g, b; };
inline Color colorAtributo(const std::string& a)
{
	if (a == "OSCURIDAD") return { 0.70f, 0.42f, 1.0f };
	if (a == "LUZ") return { 1.0f, 0.90f, 0.45f };
	if (a == "TIERRA") return { 0.90f, 0.62f, 0.32f };
	if (a == "AGUA") return { 0.35f, 0.65f, 1.0f };
	if (a == "FUEGO") return { 1.0f, 0.42f, 0.22f };
	if (a == "VIENTO") return { 0.45f, 1.0f, 0.62f };
	if (a == "DIVINIDAD") return { 1.0f, 0.80f, 0.30f };
	return { 0.30f, 0.86f, 1.0f };
}

inline float suave(float a, float b, float x)
{
	const float u = std::max(0.0f, std::min(1.0f, (x - a) / (b - a)));
	return u * u * (3.0f - 2.0f * u);
}
// sale de 0 a 1 pasandose un poco (rebote al crecer)
inline float rebote(float u)
{
	u = std::max(0.0f, std::min(1.0f, u));
	const float c = 1.70158f, d = u - 1.0f;
	return 1.0f + (c + 1.0f) * d * d * d + c * d * d;
}

// el rotulo de la clase de invocacion (los textos estan en idioma.h, bloque "// cartas-clave")
inline const char* rotulo(Clase c)
{
	switch (c) {
	case DIOS: return "DIOS EGIPCIO";
	case FUSION: return "INVOCACIÓN POR FUSIÓN";
	case RITUAL: return "INVOCACIÓN RITUAL";
	default: return "CARTA CLAVE";
	}
}

}  // namespace entrada
