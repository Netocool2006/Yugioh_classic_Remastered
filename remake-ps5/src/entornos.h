// Yugioh Classic Remaster - entornos por personaje rival (rama classic-entornos).
// Cuando se duela contra un personaje del anime, TODO lo que rodea al tablero es el lugar donde ese personaje duela
// en la serie (claves y personajes: docs/entornos.md): el fondo (cielo, sol o luna, estrellas, nubes y el efecto
// propio: mar de nubes, reticula de Noah, aurora del Oricalco, remolino de las Sombras), el suelo alrededor de la
// mesa, la luz y la niebla del tablero, siluetas 3D por codigo (castillo, columnas, obeliscos, rascacielos con
// ventanas, gradas con publico, laberinto, cubierta del dirigible...) con texturas CC0 de Poly Haven
// (entornos_insumos.py), llamas y halos aditivos, agua animada y particulas por GPU (motas, brasas, hojas, datos,
// nubes, viento, niebla, humo, arena).
//  - Nada tapa el tablero, el retrato del rival ni el HUD: las siluetas van fuera de la mesa y lejos de la linea
//    camara-mesa; las particulas en un anillo alrededor de la mesa.
//  - Una sola malla (todas las siluetas de todos los entornos, ya en el mundo) y un rango por entorno: 2 dibujos de
//    mallas + 1 fondo + hasta 3 de particulas por cuadro. Coste: YUGI_ENTORNO_MEDIR=1 imprime los triangulos.
//  - Al cambiar de entorno, lo viejo se hunde y se funde y lo nuevo sube (peso 0..1). La carta de campo manda:
//    pesoVisible = peso * (1 - ambiente del Bosque); al destruirse vuelve el entorno del rival.
// El juego (yugireload.cpp) elige la clave (campo "entorno" del personaje o el tema del rival) y dibuja.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "bosque.h"   // Generador (tubos, bultos), Vertice, Azar, ruido y stb_image

namespace entornos {

enum Clave {
	NEUTRO = 0, TIENDA, CASTILLO, ISLA, PLAYA, ACANTILADO, LABERINTO, RASCACIELOS, CIUDAD, DIRIGIBLE, ALCATRAZ, MUSEO, NOAH,
	ATLANTIDA, ESTADIO, TEMPLO, SOMBRAS, NUM_ENTORNOS
};
constexpr const char* CLAVES[NUM_ENTORNOS] = { "neutro", "tienda_juegos", "reino_duelistas_castillo", "isla_bosque", "playa_mar",
	                                           "acantilado_noche", "laberinto", "rascacielos_noche", "ciudad_batallas", "dirigible_kaiba",
	                                           "torre_alcatraz", "museo_egipcio", "mundo_virtual_noah", "atlantida_oricalco", "estadio_kc",
	                                           "templo_egipcio", "reino_sombras" };

inline int claveDe(const std::string& s)
{
	for (int i = 0; i < NUM_ENTORNOS; i++) {
		if (s == CLAVES[i]) return i;
	}
	return NEUTRO;
}

// rivales genericos (rivales.txt): el entorno sale de su tema (tabla en docs/entornos.md)
inline int porTema(const std::string& tema)
{
	auto tiene = [&](const char* k) { return tema.find(k) != std::string::npos; };
	if (tiene("Pez") || tiene("Aqua") || tiene("Serpiente Marina") || tiene("attr:AGUA")) return PLAYA;
	if (tiene("Zombi") || tiene("espiritu")) return ACANTILADO;
	if (tiene("Demonio") || tiene("OSCURIDAD") || tiene("caos")) return SOMBRAS;
	if (tiene("Insecto") || tiene("Planta") || tiene("Dinosaurio") || tiene("Reptil") || tiene("Roca") || tiene("Bestia")) return ISLA;
	if (tiene("quina") || tiene("Trueno") || tiene("union")) return CIUDAD;
	if (tiene("Drag") || tiene("Alada") || tiene("VIENTO")) return DIRIGIBLE;
	if (tiene("Lanzador") || tiene("Hada") || tiene("LUZ") || tiene("ritual")) return TEMPLO;
	if (tiene("Guerrero") || tiene("volteo") || tiene("quema") || tiene("TIERRA") || tiene("FUEGO")) return CASTILLO;
	if (tiene("toon")) return TIENDA;
	if (tiene("fusion") || tiene("goat")) return ESTADIO;
	return NEUTRO;
}

// materiales (pos.w; los mismos numeros en entornos.frag)
enum Material { SOLIDO = 0, LUZ = 1, VENTANAS = 2, HOLO = 3, SUELO = 4, AGUA = 5, PUBLICO = 6, FUEGO = 7, HALO = 8, PANTALLA = 9 };
// capas del arreglo (color; la normal es la siguiente): <assets>/entornos/<nombre>_color.png / _normal.png
enum Capa {
	C_CASTILLO = 0, C_ADOQUIN = 2, C_ARENISCA = 4, C_ARENA = 6, C_METAL = 8, C_MARMOL = 10, C_LADRILLO = 12, C_HORMIGON = 14,
	C_ROCA = 16, C_MADERA = 18, C_CESPED = 20, C_CORTEZA = 22, C_HOJAS = 24, C_BALDOSA = 26, C_LOSA = 28, NUM_CAPAS = 30
};
constexpr const char* NOMBRE_CAPA[NUM_CAPAS / 2] = { "castillo", "adoquin", "arenisca", "arena", "metal", "marmol", "ladrillo", "hormigon",
	                                                 "roca", "madera", "cesped", "corteza", "hojas", "baldosa", "losa" };
constexpr float SIN_TEXTURA = -1.0f;
constexpr uint32_t TAM_TEXTURA = 1024;

enum TipoParticula { MOTAS = 0, CHISPAS, CAEN, DATOS, NUBES, VIENTO, NIEBLA, HUMO, ARENA };
struct Particulas {
	int tipo{ -1 };
	uint32_t n{ 0 };
	glm::vec4 color{ 1.0f };   // a < 0: papelitos de colores
	glm::vec4 zona{ 2.0f, 30.0f, 0.0f, 6.0f };   // metros desde el borde de la mesa (min, max), altura (min, max)
};

// la luz y el fondo de cada entorno. Colores del fondo y la niebla en la pantalla (con gamma); luces en lineal
struct Estilo {
	glm::vec3 arriba, horizonte, niebla;
	float estrellas, nubes;
	glm::vec3 solDir;
	float solTam;
	glm::vec3 solColor;
	float halo;
	glm::vec3 luzSol, relleno, rebote;
	float densidad, inicio, noche;
	glm::vec3 efecto;
	float efectoFuerza;
	std::array<Particulas, 3> part;
};

using V3 = glm::vec3;
using V4 = glm::vec4;
inline Particulas P(int tipo, uint32_t n, V4 c, V4 z) { return { tipo, n, c, z }; }

inline const Estilo& estilo(int e)
{
	static const Estilo E[NUM_ENTORNOS] = {
		// NEUTRO (no se dibuja)
		{ V3(0), V3(0), V3(0), 0, 0, V3(0, 1, 0), 0, V3(0), 0, V3(1), V3(0.2f), V3(0.1f), 0, 99, 0, V3(0), 0, {} },
		// TIENDA: calle de Ciudad Domino de dia, la tienda Kame
		{ V3(0.30f, 0.52f, 0.88f), V3(0.78f, 0.86f, 0.95f), V3(0.70f, 0.78f, 0.88f), 0, 0.5f, V3(0.4f, 0.55f, -1.0f), 0.004f, V3(1.0f, 0.95f, 0.8f), 0.8f,
		  V3(2.5f, 2.3f, 2.0f), V3(0.30f, 0.36f, 0.48f), V3(0.16f, 0.14f, 0.12f), 0.006f, 30.0f, 0.0f, V3(0), 0,
		  { P(CAEN, 140, V4(1.0f, 0.62f, 0.75f, 1.0f), V4(1, 22, 0, 12)), P(MOTAS, 30, V4(1.0f, 0.95f, 0.8f, 0.5f), V4(1, 18, 0.5f, 5)), {} } },
		// CASTILLO: el castillo de Pegasus al atardecer
		{ V3(0.22f, 0.20f, 0.42f), V3(1.0f, 0.56f, 0.36f), V3(0.72f, 0.48f, 0.44f), 0.15f, 0.45f, V3(-0.55f, 0.07f, -1.0f), 0.01f, V3(1.0f, 0.68f, 0.38f), 1.0f,
		  V3(2.6f, 1.65f, 1.0f), V3(0.26f, 0.20f, 0.28f), V3(0.14f, 0.09f, 0.07f), 0.008f, 28.0f, 0.4f, V3(1.0f, 0.5f, 0.15f), 1,
		  { P(CHISPAS, 90, V4(1.0f, 0.5f, 0.15f, 1.0f), V4(2, 30, 0, 9)), P(CAEN, 70, V4(0.9f, 0.3f, 0.35f, 1.0f), V4(1, 20, 0, 12)), {} } },
		// ISLA: bosque y pradera del Reino de los Duelistas
		{ V3(0.28f, 0.55f, 0.90f), V3(0.80f, 0.90f, 0.92f), V3(0.62f, 0.74f, 0.70f), 0, 0.55f, V3(0.6f, 0.6f, -1.0f), 0.004f, V3(1.0f, 0.96f, 0.85f), 0.8f,
		  V3(2.5f, 2.35f, 1.9f), V3(0.24f, 0.34f, 0.32f), V3(0.08f, 0.12f, 0.05f), 0.01f, 26.0f, 0.0f, V3(0), 0,
		  { P(CAEN, 140, V4(0.45f, 0.62f, 0.18f, 1.0f), V4(1, 24, 0, 11)), P(MOTAS, 60, V4(1.0f, 0.95f, 0.5f, 0.6f), V4(1, 16, 0.3f, 3.5f)), {} } },
		// PLAYA: plataforma en el mar (Mako)
		{ V3(0.22f, 0.50f, 0.90f), V3(0.78f, 0.90f, 0.98f), V3(0.62f, 0.78f, 0.88f), 0, 0.4f, V3(0.2f, 0.45f, -1.0f), 0.005f, V3(1.0f, 0.97f, 0.88f), 1.0f,
		  V3(2.7f, 2.55f, 2.25f), V3(0.30f, 0.42f, 0.55f), V3(0.10f, 0.16f, 0.20f), 0.005f, 30.0f, 0.0f, V3(0), 0,
		  { P(NIEBLA, 14, V4(0.92f, 0.96f, 1.0f, 0.18f), V4(14, 40, -1.6f, 0)), P(VIENTO, 40, V4(0.9f, 0.95f, 1.0f, 0.25f), V4(2, 30, 0.5f, 8)), {} } },
		// ACANTILADO: acantilados y cementerio de noche (Bandit Keith, Bonz)
		{ V3(0.03f, 0.04f, 0.10f), V3(0.16f, 0.17f, 0.30f), V3(0.11f, 0.12f, 0.20f), 1.0f, 0.25f, V3(0.35f, 0.32f, -1.0f), 0.012f, V3(0.92f, 0.95f, 1.0f), 0.7f,
		  V3(0.55f, 0.65f, 1.0f), V3(0.06f, 0.08f, 0.16f), V3(0.02f, 0.02f, 0.035f), 0.018f, 18.0f, 0.0f, V3(0.4f, 0.9f, 1.0f), 1,
		  { P(NIEBLA, 28, V4(0.55f, 0.62f, 0.8f, 0.25f), V4(1, 26, -0.4f, 0)), P(MOTAS, 40, V4(0.4f, 0.9f, 1.0f, 0.9f), V4(2, 24, 0.5f, 4)), {} } },
		// LABERINTO: el laberinto subterraneo de los Hermanos Paradoja
		{ V3(0.04f, 0.035f, 0.03f), V3(0.10f, 0.08f, 0.06f), V3(0.07f, 0.055f, 0.045f), 0, 0, V3(0, 1, 0), 0, V3(1.0f, 0.6f, 0.3f), 0,
		  V3(1.45f, 0.95f, 0.55f), V3(0.10f, 0.08f, 0.07f), V3(0.05f, 0.035f, 0.025f), 0.03f, 16.0f, 0.0f, V3(1.0f, 0.55f, 0.2f), 1,
		  { P(CHISPAS, 70, V4(1.0f, 0.55f, 0.18f, 1.0f), V4(2, 22, 0, 7)), P(NIEBLA, 16, V4(0.18f, 0.15f, 0.12f, 0.3f), V4(1, 20, -0.4f, 0)), {} } },
		// RASCACIELOS: azotea de noche en Battle City (Lumis y Umbra)
		{ V3(0.02f, 0.03f, 0.10f), V3(0.24f, 0.17f, 0.33f), V3(0.13f, 0.10f, 0.20f), 0.5f, 0.15f, V3(-0.4f, 0.35f, -1.0f), 0.01f, V3(0.95f, 0.92f, 0.85f), 0.6f,
		  V3(0.6f, 0.62f, 0.95f), V3(0.10f, 0.09f, 0.18f), V3(0.08f, 0.05f, 0.06f), 0.006f, 30.0f, 1.0f, V3(1.0f, 0.2f, 0.1f), 1,
		  { P(VIENTO, 70, V4(0.8f, 0.85f, 1.0f, 0.25f), V4(1, 30, 0.5f, 10)), P(MOTAS, 20, V4(1.0f, 0.3f, 0.2f, 0.8f), V4(18, 50, 4, 30)), {} } },
		// CIUDAD: plaza de Battle City de dia
		{ V3(0.32f, 0.55f, 0.92f), V3(0.80f, 0.86f, 0.94f), V3(0.70f, 0.75f, 0.84f), 0, 0.4f, V3(-0.5f, 0.6f, -1.0f), 0.004f, V3(1.0f, 0.96f, 0.85f), 0.8f,
		  V3(2.5f, 2.35f, 2.05f), V3(0.30f, 0.36f, 0.50f), V3(0.14f, 0.13f, 0.12f), 0.006f, 30.0f, 0.15f, V3(0.2f, 0.8f, 1.0f), 1,
		  { P(CAEN, 60, V4(0.7f, 0.5f, 0.25f, 1.0f), V4(1, 22, 0, 12)), P(MOTAS, 40, V4(1.0f, 1.0f, 0.9f, 0.35f), V4(1, 20, 0.5f, 6)), {} } },
		// DIRIGIBLE: cubierta del dirigible de Kaiba sobre las nubes, al anochecer
		{ V3(0.08f, 0.12f, 0.33f), V3(0.92f, 0.58f, 0.55f), V3(0.62f, 0.50f, 0.60f), 0.35f, 0.3f, V3(0.7f, 0.06f, -1.0f), 0.008f, V3(1.0f, 0.72f, 0.5f), 1.0f,
		  V3(2.1f, 1.55f, 1.3f), V3(0.22f, 0.22f, 0.38f), V3(0.16f, 0.13f, 0.17f), 0.004f, 34.0f, 0.6f, V3(0.25f, 0.85f, 1.0f), 1,
		  { P(NUBES, 26, V4(0.95f, 0.88f, 0.92f, 0.85f), V4(24, 70, -26, -4)), P(VIENTO, 90, V4(0.85f, 0.9f, 1.0f, 0.3f), V4(1, 32, 0.3f, 9)), P(NUBES, 8, V4(0.92f, 0.84f, 0.9f, 0.5f), V4(40, 70, 6, 22)) } },
		// ALCATRAZ: la torre de duelo de Alcatraz al atardecer (Marik)
		{ V3(0.25f, 0.10f, 0.20f), V3(1.0f, 0.42f, 0.18f), V3(0.55f, 0.25f, 0.20f), 0.1f, 0.35f, V3(-0.2f, 0.10f, -1.0f), 0.016f, V3(1.0f, 0.55f, 0.2f), 1.3f,
		  V3(2.8f, 1.4f, 0.7f), V3(0.24f, 0.14f, 0.18f), V3(0.16f, 0.07f, 0.04f), 0.006f, 30.0f, 0.5f, V3(1.0f, 0.75f, 0.2f), 1,
		  { P(CHISPAS, 90, V4(1.0f, 0.75f, 0.25f, 1.0f), V4(2, 28, 0, 12)), P(VIENTO, 60, V4(1.0f, 0.8f, 0.6f, 0.22f), V4(1, 30, 0.3f, 9)), {} } },
		// MUSEO: la sala egipcia del Museo de Domino (Ishizu)
		{ V3(0.06f, 0.05f, 0.04f), V3(0.20f, 0.15f, 0.10f), V3(0.14f, 0.11f, 0.08f), 0, 0, V3(0, 1, 0), 0, V3(1.0f, 0.8f, 0.5f), 0,
		  V3(1.6f, 1.35f, 1.0f), V3(0.16f, 0.13f, 0.10f), V3(0.10f, 0.08f, 0.06f), 0.014f, 22.0f, 0.0f, V3(1.0f, 0.8f, 0.4f), 1,
		  { P(MOTAS, 110, V4(1.0f, 0.85f, 0.5f, 0.45f), V4(0, 22, 0.3f, 8)), {}, {} } },
		// NOAH: el mundo virtual de Noah
		{ V3(0.0f, 0.02f, 0.02f), V3(0.0f, 0.16f, 0.09f), V3(0.0f, 0.07f, 0.045f), 0, 0, V3(0, 1, 0), 0, V3(0.3f, 1.0f, 0.6f), 0,
		  V3(0.9f, 1.4f, 1.1f), V3(0.05f, 0.14f, 0.10f), V3(0.02f, 0.06f, 0.04f), 0.014f, 24.0f, 0.0f, V3(0.2f, 1.0f, 0.5f), 1,
		  { P(DATOS, 320, V4(0.25f, 1.0f, 0.5f, 1.0f), V4(1, 40, 0, 18)), P(MOTAS, 60, V4(0.3f, 1.0f, 0.6f, 0.6f), V4(1, 26, 0.5f, 6)), {} } },
		// ATLANTIDA: ruinas de la Atlantida con el Oricalco (Dartz)
		{ V3(0.01f, 0.04f, 0.06f), V3(0.07f, 0.24f, 0.20f), V3(0.04f, 0.13f, 0.12f), 0.6f, 0, V3(0, 1, 0), 0, V3(0.4f, 1.0f, 0.6f), 0,
		  V3(0.85f, 1.35f, 1.0f), V3(0.05f, 0.14f, 0.11f), V3(0.03f, 0.08f, 0.05f), 0.012f, 24.0f, 0.3f, V3(0.25f, 1.0f, 0.35f), 1,
		  { P(CHISPAS, 200, V4(0.3f, 1.0f, 0.4f, 1.0f), V4(1, 30, 0, 10)), P(MOTAS, 50, V4(0.4f, 1.0f, 0.5f, 0.7f), V4(1, 20, 0.5f, 5)), {} } },
		// ESTADIO: el estadio del Gran Campeonato KC, de noche
		{ V3(0.02f, 0.03f, 0.09f), V3(0.16f, 0.20f, 0.38f), V3(0.10f, 0.12f, 0.22f), 0.5f, 0, V3(0, 1, 0), 0, V3(0.8f, 0.9f, 1.0f), 0,
		  V3(2.0f, 2.05f, 2.3f), V3(0.18f, 0.20f, 0.30f), V3(0.08f, 0.08f, 0.10f), 0.006f, 34.0f, 1.0f, V3(0.2f, 0.6f, 1.0f), 1,
		  { P(CAEN, 220, V4(1.0f, 1.0f, 1.0f, -1.0f), V4(2, 26, 0, 14)), P(MOTAS, 30, V4(1.0f, 1.0f, 1.0f, 0.6f), V4(20, 40, 3, 16)), {} } },
		// TEMPLO: el palacio y templo del Egipto antiguo a pleno sol (Memorias del Faraon)
		{ V3(0.22f, 0.47f, 0.85f), V3(0.96f, 0.86f, 0.66f), V3(0.86f, 0.74f, 0.56f), 0, 0.15f, V3(0.35f, 0.75f, -1.0f), 0.006f, V3(1.0f, 0.95f, 0.8f), 1.0f,
		  V3(2.9f, 2.5f, 1.85f), V3(0.30f, 0.32f, 0.40f), V3(0.32f, 0.23f, 0.13f), 0.008f, 26.0f, 0.0f, V3(1.0f, 0.55f, 0.15f), 1,
		  { P(ARENA, 260, V4(0.86f, 0.72f, 0.48f, 0.7f), V4(1, 26, 0, 1.6f)), P(CHISPAS, 40, V4(1.0f, 0.55f, 0.15f, 1.0f), V4(3, 24, 0, 6)), {} } },
		// SOMBRAS: el Reino de las Sombras (Zorc, Bakura)
		{ V3(0.06f, 0.01f, 0.10f), V3(0.30f, 0.08f, 0.30f), V3(0.13f, 0.04f, 0.14f), 0.2f, 0, V3(0, 1, 0), 0, V3(0.9f, 0.2f, 0.3f), 0,
		  V3(1.0f, 0.55f, 0.95f), V3(0.10f, 0.04f, 0.12f), V3(0.05f, 0.015f, 0.04f), 0.02f, 18.0f, 0.0f, V3(0.75f, 0.2f, 1.0f), 1,
		  { P(HUMO, 44, V4(0.06f, 0.0f, 0.08f, 0.55f), V4(2, 30, -0.3f, 12)), P(CHISPAS, 90, V4(0.7f, 0.2f, 1.0f, 1.0f), V4(1, 26, 0, 9)), P(NIEBLA, 18, V4(0.18f, 0.05f, 0.2f, 0.35f), V4(1, 24, -0.4f, 0)) } },
	};
	return E[e < 0 || e >= NUM_ENTORNOS ? 0 : e];
}

// ------------------------------------------------------------------------------------------- obras por codigo
// Todo ya en el mundo: x derecha, y arriba, z hacia el jugador; la mesa ocupa |x| < hx, |z| < hz; la camara de juego
// esta en z ~ +21, y ~ +10. Lo alto va detras (z < -14) y a los lados (|x| > 17); nada en z > 8 con |x| < 26
class Obra {
public:
	bosque::Generador g;
	float hx{ 13.44f }, hz{ 10.08f };

	uint32_t base() const { return (uint32_t)g.v.size(); }
	void vert(V3 p, float mat, V3 n, V4 color, glm::vec2 uv, float capa, float ao = 1.0f) { g.vert(p, mat, n, color, uv, capa, ao); }
	void quad(V3 a, V3 b, V3 c, V3 d, float mat, float capa, V4 tinte, glm::vec2 ua, glm::vec2 ub, glm::vec2 uc, glm::vec2 ud, float aoAbajo = 1.0f)
	{
		const V3 n = glm::normalize(glm::cross(b - a, d - a));
		const uint32_t b0 = base();
		vert(a, mat, n, tinte, ua, capa, aoAbajo);
		vert(b, mat, n, tinte, ub, capa, aoAbajo);
		vert(c, mat, n, tinte, uc, capa);
		vert(d, mat, n, tinte, ud, capa);
		g.tri(b0, b0 + 1, b0 + 2);
		g.tri(b0, b0 + 2, b0 + 3);
	}
	// el tinte de lo ultimo agregado (los bultos y tubos de bosque.h salen blancos)
	void tenir(uint32_t desde, V4 tinte)
	{
		for (size_t i = desde; i < g.v.size(); i++) g.v[i].color = tinte;
	}
	void material(uint32_t desde, float mat)
	{
		for (size_t i = desde; i < g.v.size(); i++) g.v[i].pos.w = mat;
	}

	// tronco de piramide de base rectangular girado "giro" en y: base (centro abajo), mitades abajo (ax, az) y arriba
	// (bx, bz), alto. Caja: a == b; piramide: b = 0. uv en metros / esc (u horizontal, v vertical)
	void tronco4(V3 p, float giro, float ax, float az, float bx, float bz, float alto, float mat, float capa, V4 tinte, glm::vec2 esc = glm::vec2(4.0f),
	             bool tapa = true, bool fondo = false)
	{
		const float c = std::cos(giro), s = std::sin(giro);
		auto R = [&](float x, float y, float z) { return p + V3(c * x + s * z, y, -s * x + c * z); };
		const V3 A[4] = { R(-ax, 0, az), R(ax, 0, az), R(ax, 0, -az), R(-ax, 0, -az) };
		const V3 B[4] = { R(-bx, alto, bz), R(bx, alto, bz), R(bx, alto, -bz), R(-bx, alto, -bz) };
		for (int k = 0; k < 4; k++) {
			const int j = (k + 1) % 4;
			const float w = glm::length(A[j] - A[k]) / esc.x, h = alto / esc.y;
			if (bx < 1e-3f && bz < 1e-3f) {   // piramide: triangulos
				const V3 n = glm::normalize(glm::cross(A[j] - A[k], B[k] - A[k]));
				const uint32_t b0 = base();
				vert(A[k], mat, n, tinte, glm::vec2(0, 0), capa, 0.8f);
				vert(A[j], mat, n, tinte, glm::vec2(w, 0), capa, 0.8f);
				vert(B[k], mat, n, tinte, glm::vec2(w * 0.5f, h), capa);
				g.tri(b0, b0 + 1, b0 + 2);
			} else {
				const float wb = glm::length(B[j] - B[k]) / esc.x;
				quad(A[k], A[j], B[j], B[k], mat, capa, tinte, glm::vec2(0, 0), glm::vec2(w, 0), glm::vec2((w + wb) * 0.5f, h), glm::vec2((w - wb) * 0.5f, h), 0.7f);
			}
		}
		if (tapa && (bx > 1e-3f || bz > 1e-3f)) {
			quad(B[0], B[1], B[2], B[3], mat == (float)VENTANAS ? (float)SOLIDO : mat, capa, tinte, glm::vec2(0, 0), glm::vec2(2 * bx / esc.x, 0), glm::vec2(2 * bx / esc.x, 2 * bz / esc.x),
			     glm::vec2(0, 2 * bz / esc.x));
		}
		if (fondo) {
			quad(A[3], A[2], A[1], A[0], mat, capa, tinte, glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));
		}
	}
	void caja(V3 centroAbajo, V3 medio, float mat, float capa, V4 tinte, glm::vec2 esc = glm::vec2(4.0f), float giro = 0.0f)
	{
		tronco4(centroAbajo, giro, medio.x, medio.z, medio.x, medio.z, medio.y * 2.0f, mat, capa, tinte, esc);
	}
	// cilindro (o cono si r1 = 0) con normales suaves
	void cilindro(V3 p, float r0, float r1, float alto, int lados, float mat, float capa, V4 tinte, glm::vec2 esc = glm::vec2(4.0f), bool tapa = true)
	{
		const uint32_t b0 = base();
		const float pend = (r0 - r1) / std::max(alto, 1e-3f);
		const float vuelta = 6.2831853f * std::max(r0, 0.3f) / esc.x;
		for (int k = 0; k <= lados; k++) {
			const float a = 6.2831853f * (float)k / (float)lados;
			const V3 d(std::cos(a), 0.0f, std::sin(a));
			const V3 n = glm::normalize(d + V3(0.0f, pend, 0.0f));
			const float u = std::round(vuelta) * (float)k / (float)lados;
			vert(p + d * r0, mat, n, tinte, glm::vec2(u, 0.0f), capa, 0.7f);
			vert(p + d * r1 + V3(0, alto, 0), mat, n, tinte, glm::vec2(u, alto / esc.y), capa);
		}
		for (int k = 0; k < lados; k++) {
			const uint32_t a = b0 + 2 * k;
			g.tri(a, a + 2, a + 1);
			g.tri(a + 1, a + 2, a + 3);
		}
		if (tapa && r1 > 1e-3f) {
			const uint32_t c = base();
			vert(p + V3(0, alto, 0), mat == (float)VENTANAS ? (float)SOLIDO : mat, V3(0, 1, 0), tinte, glm::vec2(0.0f), capa);
			for (int k = 0; k <= lados; k++) {
				const float a = 6.2831853f * (float)k / (float)lados;
				vert(p + V3(std::cos(a) * r1, alto, std::sin(a) * r1), mat == (float)VENTANAS ? (float)SOLIDO : mat, V3(0, 1, 0), tinte,
				     glm::vec2(std::cos(a), std::sin(a)) * r1 / esc.x, capa);
			}
			for (int k = 0; k < lados; k++) g.tri(c, c + 2 + k, c + 1 + k);
		}
	}
	// disco de suelo (anillos para que la niebla y la precision acompanen), uv en metros / esc
	void disco(float y, float r0, float r1, float mat, float capa, V4 tinte, float esc, int lados = 64)
	{
		const float radios[] = { 0.0f, 6.0f, 12.0f, 18.0f, 25.0f, 33.0f, 42.0f, 52.0f, 64.0f, 80.0f, 100.0f };
		std::vector<float> rs;
		rs.push_back(r0);
		for (float r : radios) if (r > r0 && r < r1) rs.push_back(r);
		rs.push_back(r1);
		const uint32_t b0 = base();
		for (float r : rs) {
			for (int k = 0; k <= lados; k++) {
				const float a = 6.2831853f * (float)k / (float)lados;
				const V3 p(std::cos(a) * r, y, std::sin(a) * r);
				vert(p, mat, V3(0, 1, 0), tinte, glm::vec2(p.x, p.z) / esc, capa);
			}
		}
		const uint32_t L = (uint32_t)lados + 1;
		for (uint32_t i = 0; i + 1 < rs.size(); i++) {
			for (uint32_t k = 0; k < (uint32_t)lados; k++) {
				const uint32_t a = b0 + i * L + k;
				g.tri(a, a + L, a + 1);
				g.tri(a + 1, a + L, a + L + 1);
			}
		}
	}
	// suelo rectangular (losa) subdividido
	void losa(float x0, float z0, float x1, float z1, float y, float mat, float capa, V4 tinte, float esc, float paso = 8.0f)
	{
		const int nx = std::max(1, (int)std::ceil((x1 - x0) / paso)), nz = std::max(1, (int)std::ceil((z1 - z0) / paso));
		const uint32_t b0 = base();
		for (int j = 0; j <= nz; j++) {
			for (int i = 0; i <= nx; i++) {
				const V3 p(x0 + (x1 - x0) * i / nx, y, z0 + (z1 - z0) * j / nz);
				vert(p, mat, V3(0, 1, 0), tinte, glm::vec2(p.x, p.z) / esc, capa);
			}
		}
		for (int j = 0; j < nz; j++) {
			for (int i = 0; i < nx; i++) {
				const uint32_t a = b0 + j * (nx + 1) + i;
				g.tri(a, a + nx + 1, a + 1);
				g.tri(a + 1, a + nx + 1, a + nx + 2);
			}
		}
	}
	// llama (dos tarjetas cruzadas, uv 0..1; uv.z: semilla) y halo (tres tarjetas): van al rango aditivo
	void fuego(V3 p, float alto, float ancho, V4 color, float semilla)
	{
		for (int k = 0; k < 2; k++) {
			const V3 lado = k == 0 ? V3(ancho * 0.5f, 0, 0) : V3(0, 0, ancho * 0.5f);
			quad(p - lado, p + lado, p + lado + V3(0, alto, 0), p - lado + V3(0, alto, 0), FUEGO, semilla, color, glm::vec2(0, 0), glm::vec2(1, 0),
			     glm::vec2(1, 1), glm::vec2(0, 1));
		}
	}
	void halo(V3 p, float tam, V4 color, float semilla = 0.0f)
	{
		const V3 ejes[3][2] = { { V3(1, 0, 0), V3(0, 1, 0) }, { V3(0, 0, 1), V3(0, 1, 0) }, { V3(1, 0, 0), V3(0, 0, 1) } };
		for (auto& e : ejes) {
			const V3 a = e[0] * tam, b = e[1] * tam;
			quad(p - a - b, p + a - b, p + a + b, p - a + b, HALO, semilla, color, glm::vec2(0, 0), glm::vec2(1, 0), glm::vec2(1, 1), glm::vec2(0, 1));
		}
	}
	// arbol estilizado: tronco que se tuerce y copa de bultos con la textura de hojas
	void arbol(V3 p, float alto, uint32_t semilla, V4 tinteHojas = V4(0.5f, 1.3f, 0.4f, 0.0f))
	{
		bosque::Azar a(semilla);
		std::vector<bosque::PuntoTubo> t;
		V3 q = p;
		for (int k = 0; k <= 5; k++) {
			const float f = (float)k / 5.0f;
			t.push_back({ q, alto * 0.045f * (1.0f - 0.6f * f) * (k == 0 ? 1.4f : 1.0f), V4(0.8f, 0.7f, 0.6f, 0.0f), 0.6f + 0.4f * f });
			q += V3(a.entre(-0.08f, 0.08f) * alto, alto * 0.13f, a.entre(-0.08f, 0.08f) * alto);
		}
		uint32_t b0 = base();
		g.tubo(t, 7, SOLIDO, C_CORTEZA, 1.0f, 3.0f, 0.4f, semilla, false);
		tenir(b0, V4(0.85f, 0.8f, 0.75f, 0.0f));
		b0 = base();
		const int n = 3 + (int)(a() * 3.0f);
		for (int k = 0; k < n; k++) {
			const V3 c = q + V3(a.entre(-0.22f, 0.22f) * alto, a.entre(-0.1f, 0.18f) * alto, a.entre(-0.22f, 0.22f) * alto);
			const float r = alto * a.entre(0.2f, 0.3f);
			g.bulto(c, V3(r, r * 0.8f, r), 1, SOLIDO, C_HOJAS, semilla * 7u + (uint32_t)k, 0.3f);
		}
		tenir(b0, tinteHojas);
	}
	void roca(V3 p, V3 radio, uint32_t semilla, V4 tinte = V4(0.75f, 0.72f, 0.7f, 0.0f), int subdiv = 2)
	{
		const uint32_t b0 = base();
		g.bulto(p, radio, subdiv, SOLIDO, C_ROCA, semilla, 0.3f);
		tenir(b0, tinte);
	}
	// columna: basa, fuste (con capitel) y abaco
	void columna(V3 p, float r, float alto, float capa, V4 tinte, bool rota = false)
	{
		caja(p, V3(r * 1.4f, r * 0.3f, r * 1.4f), SOLIDO, capa, tinte, glm::vec2(3.0f));
		const float fuste = rota ? alto * 0.55f : alto;
		cilindro(p + V3(0, r * 0.6f, 0), r, r * 0.9f, fuste - r * 1.2f, 14, SOLIDO, capa, tinte, glm::vec2(3.0f), rota);
		if (!rota) {
			cilindro(p + V3(0, alto - r * 0.6f, 0), r * 0.9f, r * 1.35f, r * 0.4f, 14, SOLIDO, capa, tinte, glm::vec2(3.0f), false);
			caja(p + V3(0, alto - r * 0.2f, 0), V3(r * 1.5f, r * 0.25f, r * 1.5f), SOLIDO, capa, tinte, glm::vec2(3.0f));
		}
	}
	void antorcha(V3 p, float semilla, V4 fuegoColor = V4(1.0f, 0.8f, 0.6f, 2.2f))
	{
		cilindro(p, 0.12f, 0.22f, 0.8f, 6, SOLIDO, SIN_TEXTURA, V4(0.12f, 0.08f, 0.05f, 0.0f));
		aditivo.push_back({ p + V3(0, 0.75f, 0), semilla, fuegoColor });
	}
	void brasero(V3 p, float semilla)
	{
		cilindro(p, 0.35f, 0.25f, 1.6f, 10, SOLIDO, C_ARENISCA, V4(0.6f, 0.5f, 0.4f, 0.0f), glm::vec2(2.0f));
		cilindro(p + V3(0, 1.6f, 0), 0.3f, 0.75f, 0.45f, 12, SOLIDO, SIN_TEXTURA, V4(0.25f, 0.18f, 0.08f, 0.0f));
		aditivo.push_back({ p + V3(0, 1.95f, 0), semilla + 0.5f, V4(1.0f, 0.8f, 0.6f, 3.0f) });
	}
	// lo que se agrega al final como aditivo (llamas de antorchas y braseros, con su halo)
	struct Llama { V3 p; float semilla; V4 color; };
	std::vector<Llama> aditivo;
	struct Brillo { V3 p; float tam; V4 color; };
	std::vector<Brillo> halos;

	// las almenas sobre un muro de "a" a "b" (a la altura y)
	void almenas(V3 a, V3 b, float grosor, float capa, V4 tinte)
	{
		const float largo = glm::length(b - a);
		const int n = (int)(largo / 2.0f);
		const V3 d = (b - a) / largo;
		const float giro = std::atan2(-d.z, d.x);
		for (int k = 0; k < n; k += 2) {
			caja(a + d * (2.0f * k + 1.0f), V3(0.5f, 0.6f, grosor * 0.5f), SOLIDO, capa, tinte, glm::vec2(3.0f), giro);
		}
	}
	// muro de "a" a "b" con alto y grosor (y almenas)
	void muro(V3 a, V3 b, float alto, float grosor, float capa, V4 tinte, bool conAlmenas = true, float mat = SOLIDO)
	{
		const V3 d = b - a;
		const float largo = glm::length(d);
		const float giro = std::atan2(-d.z, d.x);
		caja((a + b) * 0.5f, V3(largo * 0.5f, alto * 0.5f, grosor * 0.5f), mat, capa, tinte, glm::vec2(4.0f), giro);
		if (conAlmenas) almenas(a + V3(0, alto, 0), b + V3(0, alto, 0), grosor, capa, tinte);
	}
	// ventanas: celdas de 3 x 3.6 m
	void edificio(V3 p, float ax, float az, float alto, float capa, V4 tinte, float giro = 0.0f)
	{
		tronco4(p, giro, ax, az, ax, az, alto, VENTANAS, capa, tinte, glm::vec2(3.0f, 3.6f));
	}
	// lejos de la linea camara-mesa (z > 8 con |x| < 26) y fuera de la mesa con margen
	bool libre(float x, float z, float margen) const
	{
		if (std::fabs(x) < hx + margen && std::fabs(z) < hz + margen) return false;
		if (z > 6.0f && std::fabs(x) < 28.0f) return false;
		return true;
	}
};

// ---------------------------------------------------------------------------------------------- cada entorno
inline void obraTienda(Obra& o)
{
	o.disco(-0.3f, 0.0f, 90.0f, SUELO, C_HORMIGON, V4(0.95f, 0.93f, 0.9f, 0), 5.0f);
	o.losa(-90.0f, -24.0f, 90.0f, -14.0f, -0.27f, SUELO, C_HORMIGON, V4(0.32f, 0.32f, 0.34f, 0), 6.0f);   // la calle
	for (int k = -6; k <= 6; k++) o.caja(V3(k * 7.0f, -0.26f, -19.0f), V3(1.6f, 0.01f, 0.15f), SOLIDO, SIN_TEXTURA, V4(0.9f, 0.9f, 0.85f, 0));
	// la tienda de juegos (2 pisos, techo a cuatro aguas, cartel amarillo, vidriera encendida)
	const V3 t(0.0f, -0.3f, -33.0f);
	o.caja(t, V3(9.0f, 4.5f, 6.0f), SOLIDO, C_LADRILLO, V4(1.0f, 0.95f, 0.88f, 0), glm::vec2(4.0f));
	o.tronco4(t + V3(0, 9.0f, 0), 0.0f, 9.8f, 6.8f, 3.0f, 0.6f, 3.4f, SOLIDO, SIN_TEXTURA, V4(0.18f, 0.32f, 0.22f, 0));
	o.caja(t + V3(0, 3.4f, 6.1f), V3(6.0f, 0.8f, 0.2f), LUZ, SIN_TEXTURA, V4(1.0f, 0.82f, 0.25f, 1.6f));
	o.caja(t + V3(-3.5f, 0.2f, 6.05f), V3(3.6f, 1.2f, 0.1f), LUZ, SIN_TEXTURA, V4(1.0f, 0.85f, 0.6f, 0.9f));
	o.caja(t + V3(4.5f, 0.0f, 6.05f), V3(1.1f, 2.0f, 0.1f), SOLIDO, C_MADERA, V4(0.6f, 0.4f, 0.25f, 0), glm::vec2(2.0f));
	o.caja(t + V3(0, 9.0f, 6.0f), V3(9.2f, 0.25f, 0.6f), SOLIDO, SIN_TEXTURA, V4(0.5f, 0.12f, 0.1f, 0));   // alero
	// los vecinos: casas y edificios a los dos lados de la calle y detras
	bosque::Azar a(101u);
	for (float x = -80.0f; x < 80.0f; x += a.entre(11.0f, 15.0f)) {
		if (std::fabs(x) < 13.0f) continue;
		const float alto = a.entre(8.0f, std::fabs(x) > 30.0f ? 30.0f : 14.0f);
		const int capa = a() < 0.5f ? C_LADRILLO : C_HORMIGON;
		o.edificio(V3(x, -0.3f, a.entre(-38.0f, -34.0f)), a.entre(4.5f, 6.0f), 6.0f, alto, (float)capa, V4(a.entre(0.8f, 1.1f), a.entre(0.8f, 1.0f), a.entre(0.75f, 0.95f), 0.4f));
	}
	for (int lado = -1; lado <= 1; lado += 2) {
		for (float z = -18.0f; z < 14.0f; z += a.entre(10.0f, 13.0f)) {
			o.edificio(V3(lado * a.entre(36.0f, 40.0f), -0.3f, z), 6.0f, 5.0f, a.entre(9.0f, 22.0f), (float)(a() < 0.5f ? C_LADRILLO : C_HORMIGON),
			           V4(a.entre(0.8f, 1.05f), 0.92f, 0.88f, 0.4f));
		}
		for (float z = -18.0f; z < 10.0f; z += 9.0f) o.arbol(V3(lado * 27.0f, -0.3f, z), a.entre(6.0f, 8.0f), (uint32_t)(z * 3 + lado * 50 + 200), V4(1.7f, 0.85f, 1.25f, 0));
		// faroles
		for (float z = -20.0f; z < 8.0f; z += 12.0f) {
			o.cilindro(V3(lado * 22.0f, -0.3f, z), 0.12f, 0.1f, 5.0f, 6, SOLIDO, C_METAL, V4(0.3f, 0.3f, 0.32f, 0));
			o.caja(V3(lado * 22.0f, 4.7f, z), V3(0.35f, 0.3f, 0.35f), LUZ, SIN_TEXTURA, V4(1.0f, 0.9f, 0.7f, 0.8f));
		}
	}
	for (float x = -39.0f; x <= 39.0f; x += 26.0f) o.arbol(V3(x, -0.3f, -27.0f), 6.5f, (uint32_t)(x + 300), V4(1.7f, 0.85f, 1.25f, 0));   // cerezos
}

inline void obraCastillo(Obra& o)
{
	const V4 piedra(0.95f, 0.9f, 0.85f, 0), techo(0.16f, 0.22f, 0.45f, 0);
	o.disco(-0.3f, 0.0f, 90.0f, SUELO, C_ADOQUIN, V4(0.9f, 0.85f, 0.8f, 0), 4.0f);
	// el patio: muralla con almenas detras y a los lados, con antorchas
	// (la camara de juego ve detras de la mesa solo hasta ~7 m de alto a 60 m: muralla baja y el castillo cerca)
	o.muro(V3(-46, -0.3f, -24), V3(-14, -0.3f, -24), 3.0f, 1.6f, C_CASTILLO, piedra);
	o.muro(V3(14, -0.3f, -24), V3(46, -0.3f, -24), 3.0f, 1.6f, C_CASTILLO, piedra);
	for (int lado = -1; lado <= 1; lado += 2) {
		o.muro(V3(lado * 34.0f, -0.3f, -24), V3(lado * 34.0f, -0.3f, 6), 6.0f, 2.0f, C_CASTILLO, piedra);
		o.cilindro(V3(lado * 14.0f, -0.3f, -24), 2.2f, 2.2f, 6.0f, 16, SOLIDO, C_CASTILLO, piedra);
		o.cilindro(V3(lado * 14.0f, 5.7f, -24), 2.6f, 0.0f, 3.5f, 16, SOLIDO, SIN_TEXTURA, techo);
		o.cilindro(V3(lado * 34.0f, -0.3f, -24), 4.0f, 4.0f, 13.0f, 18, SOLIDO, C_CASTILLO, piedra);
		o.cilindro(V3(lado * 34.0f, 12.7f, -24), 4.6f, 0.0f, 7.0f, 18, SOLIDO, SIN_TEXTURA, techo);
		for (float z = -18.0f; z <= 2.0f; z += 10.0f) o.antorcha(V3(lado * 32.6f, 3.0f, z), z * 0.1f + lado);
	}
	for (float x : { -40.0f, -28.0f, -18.0f, 18.0f, 28.0f, 40.0f }) o.antorcha(V3(x, 2.7f, -23.0f), x * 0.05f);
	// el castillo: torre del homenaje, torres redondas con conos, la torre alta del centro y ventanas encendidas
	const float zc = -40.0f;
	// el porton y la escalinata
	o.caja(V3(0, -0.3f, zc + 8.1f), V3(3.2f, 2.8f, 0.3f), SOLIDO, C_MADERA, V4(0.35f, 0.22f, 0.12f, 0), glm::vec2(2.0f));
	o.caja(V3(0, -0.3f, zc + 9.5f), V3(6.0f, 0.3f, 1.4f), SOLIDO, C_CASTILLO, piedra);
	o.caja(V3(0, 5.3f, zc + 8.2f), V3(3.6f, 0.4f, 0.4f), SOLIDO, C_CASTILLO, piedra);
	for (int lado = -1; lado <= 1; lado += 2) o.antorcha(V3(lado * 4.2f, 2.6f, zc + 8.6f), 9.0f + lado);
	o.caja(V3(0, -0.3f, zc), V3(15.0f, 11.0f, 8.0f), SOLIDO, C_CASTILLO, piedra, glm::vec2(4.0f));
	o.almenas(V3(-15, 21.7f, zc + 8), V3(15, 21.7f, zc + 8), 1.0f, C_CASTILLO, piedra);
	const V3 torres[] = { V3(-16, 0, zc + 8), V3(16, 0, zc + 8), V3(-16, 0, zc - 8), V3(16, 0, zc - 8), V3(-27, 0, zc + 2), V3(27, 0, zc + 2) };
	int k = 0;
	for (V3 t : torres) {
		const float alto = (k < 2 ? 30.0f : 26.0f) + (k > 3 ? -6.0f : 0.0f);
		const float r = k > 3 ? 3.2f : 4.0f;
		o.cilindro(t + V3(0, -0.3f, 0), r, r, alto, 20, SOLIDO, C_CASTILLO, piedra);
		o.cilindro(t + V3(0, alto - 0.3f, 0), r * 1.2f, 0.0f, r * 2.6f, 20, SOLIDO, SIN_TEXTURA, techo);
		for (int w = 0; w < 3; w++) o.caja(t + V3(0, 3.0f + w * 7.0f, r - 0.05f), V3(0.5f, 0.9f, 0.1f), LUZ, SIN_TEXTURA, V4(1.0f, 0.75f, 0.4f, 1.4f));
		k++;
	}
	o.cilindro(V3(0, 21.7f, zc), 5.0f, 5.0f, 16.0f, 22, SOLIDO, C_CASTILLO, piedra);
	o.cilindro(V3(0, 37.7f, zc), 6.0f, 0.0f, 12.0f, 22, SOLIDO, SIN_TEXTURA, techo);
	for (int w = -2; w <= 2; w++) {
		if (w != 0) o.caja(V3(w * 5.5f, 3.2f, zc + 8.05f), V3(0.8f, 1.4f, 0.1f), LUZ, SIN_TEXTURA, V4(1.0f, 0.72f, 0.38f, 1.3f));
		o.caja(V3(w * 5.5f, 9.0f, zc + 8.05f), V3(0.8f, 1.6f, 0.1f), LUZ, SIN_TEXTURA, V4(1.0f, 0.72f, 0.38f, 1.3f));
		o.caja(V3(w * 5.5f, 15.0f, zc + 8.05f), V3(0.8f, 1.6f, 0.1f), LUZ, SIN_TEXTURA, V4(1.0f, 0.72f, 0.38f, 1.3f));
	}
	// estandartes rojos colgando del castillo
	for (int w = -1; w <= 1; w += 2) o.caja(V3(w * 9.0f, 1.5f, zc + 8.2f), V3(1.4f, 4.5f, 0.08f), SOLIDO, SIN_TEXTURA, V4(0.55f, 0.04f, 0.06f, 0));
	// la isla detras: arboles oscuros a los lados del castillo
	for (int i = 0; i < 14; i++) {
		const float x = (i < 7 ? -1.0f : 1.0f) * (36.0f + (i % 7) * 6.0f);
		o.arbol(V3(x, -0.3f, -36.0f - (i % 3) * 7.0f), 9.0f + (i % 4), 500u + i, V4(0.4f, 0.75f, 0.35f, 0));
	}
}

inline void obraIsla(Obra& o)
{
	o.disco(-0.3f, 0.0f, 90.0f, SUELO, C_CESPED, V4(0.9f, 1.0f, 0.85f, 0), 7.0f);
	bosque::Azar a(77u);
	int n = 0;
	for (int i = 0; i < 400 && n < 46; i++) {
		const float x = a.entre(-70.0f, 70.0f), z = a.entre(-72.0f, 10.0f);
		if (!o.libre(x, z, 6.0f)) continue;
		const float d = std::sqrt(x * x + z * z);
		o.arbol(V3(x, -0.3f, z), a.entre(7.0f, 12.0f) * (d > 40.0f ? 1.3f : 1.0f), 900u + i);
		n++;
	}
	for (int i = 0; i < 60; i++) {
		const float x = a.entre(-60.0f, 60.0f), z = a.entre(-60.0f, 12.0f);
		if (!o.libre(x, z, 2.0f)) continue;
		const float r = a.entre(0.5f, 2.2f);
		o.roca(V3(x, -0.35f, z), V3(r * 1.3f, r * 0.8f, r), 40u + i, V4(0.7f, 0.72f, 0.65f, 0), 1);
	}
	// un risco detras (donde Rex y Weevil esperaban)
	o.roca(V3(-30, -2.0f, -50), V3(16, 12, 10), 13u, V4(0.6f, 0.58f, 0.52f, 0));
	o.roca(V3(34, -2.0f, -56), V3(14, 9, 9), 17u, V4(0.6f, 0.58f, 0.52f, 0));
}

inline void obraPlaya(Obra& o)
{
	// la plataforma de madera en el mar, los pilotes y el agua alrededor
	o.losa(-30.0f, -26.0f, 30.0f, 16.0f, -0.3f, SUELO, C_MADERA, V4(0.62f, 0.6f, 0.6f, 0), 3.5f);
	o.caja(V3(0, -1.9f, -5.0f), V3(30.0f, 0.8f, 21.0f), SOLIDO, C_MADERA, V4(0.55f, 0.45f, 0.38f, 0), glm::vec2(3.0f));
	for (float x = -28.0f; x <= 28.0f; x += 8.0f) {
		for (float z : { -26.0f, 16.0f }) o.cilindro(V3(x, -4.0f, z), 0.45f, 0.45f, 5.2f, 10, SOLIDO, C_MADERA, V4(0.5f, 0.4f, 0.32f, 0), glm::vec2(2.0f));
	}
	for (float z = -18.0f; z <= 8.0f; z += 8.0f) {
		for (float x : { -30.0f, 30.0f }) o.cilindro(V3(x, -4.0f, z), 0.45f, 0.45f, 5.2f, 10, SOLIDO, C_MADERA, V4(0.5f, 0.4f, 0.32f, 0), glm::vec2(2.0f));
	}
	o.disco(-1.6f, 0.0f, 100.0f, AGUA, SIN_TEXTURA, V4(0.02f, 0.10f, 0.16f, 0), 10.0f, 96);
	// rocas en el mar y el faro de la isla
	bosque::Azar a(5u);
	for (int i = 0; i < 12; i++) {
		const float ang = a.entre(2.4f, 6.9f), r = a.entre(38.0f, 64.0f);
		const float x = std::cos(ang) * r, z = std::sin(ang) * r - 10.0f;
		if (!o.libre(x, z, 10.0f)) continue;
		o.roca(V3(x, -2.5f, z), V3(a.entre(3.0f, 7.0f), a.entre(2.0f, 6.0f), a.entre(3.0f, 6.0f)), 60u + i, V4(0.55f, 0.52f, 0.5f, 0));
	}
	o.roca(V3(-36, -4.0f, -60), V3(18, 7, 12), 3u, V4(0.62f, 0.6f, 0.52f, 0));
	o.cilindro(V3(-36, 2.0f, -60), 2.2f, 1.6f, 16.0f, 16, SOLIDO, SIN_TEXTURA, V4(0.95f, 0.95f, 0.92f, 0));
	o.cilindro(V3(-36, 9.0f, -60), 2.0f, 1.9f, 2.0f, 16, SOLIDO, SIN_TEXTURA, V4(0.7f, 0.08f, 0.06f, 0), glm::vec2(4.0f), false);
	o.cilindro(V3(-36, 18.0f, -60), 1.7f, 1.7f, 2.0f, 12, LUZ, SIN_TEXTURA, V4(1.0f, 0.9f, 0.6f, 1.5f));
	o.cilindro(V3(-36, 20.0f, -60), 2.0f, 0.0f, 2.0f, 12, SOLIDO, SIN_TEXTURA, V4(0.6f, 0.06f, 0.05f, 0));
	o.halos.push_back({ V3(-36, 19.0f, -60), 5.0f, V4(1.0f, 0.85f, 0.5f, 1.0f) });
}

inline void obraAcantilado(Obra& o)
{
	o.disco(-0.3f, 0.0f, 90.0f, SUELO, C_ROCA, V4(0.42f, 0.42f, 0.48f, 0), 8.0f);
	bosque::Azar a(31u);
	// acantilados alrededor (detras y a los lados)
	for (int i = 0; i < 16; i++) {
		const float ang = -3.1416f + 3.1416f * (float)i / 15.0f;   // de la izquierda a la derecha por detras
		const float r = a.entre(42.0f, 60.0f);
		const float x = std::cos(ang) * r * 1.2f, z = std::sin(ang) * r - 6.0f;
		if (!o.libre(x, z, 12.0f)) continue;
		o.roca(V3(x, -3.0f, z), V3(a.entre(9.0f, 16.0f), a.entre(8.0f, 22.0f), a.entre(7.0f, 12.0f)), 200u + i, V4(0.5f, 0.5f, 0.58f, 0));
	}
	// cementerio: lapidas y cruces en grupos a los lados y detras
	for (int i = 0; i < 70; i++) {
		const float x = a.entre(-40.0f, 40.0f), z = a.entre(-34.0f, 6.0f);
		if (!o.libre(x, z, 3.0f)) continue;
		const float giro = a.entre(-0.25f, 0.25f);
		if (a() < 0.6f) {
			o.caja(V3(x, -0.3f, z), V3(0.6f, a.entre(0.6f, 1.0f), 0.18f), SOLIDO, C_ARENISCA, V4(0.5f, 0.5f, 0.55f, 0), glm::vec2(2.0f), giro);
		} else {
			o.caja(V3(x, -0.3f, z), V3(0.12f, 1.0f, 0.12f), SOLIDO, C_MADERA, V4(0.35f, 0.3f, 0.28f, 0), glm::vec2(2.0f), giro);
			o.caja(V3(x, 1.0f, z), V3(0.55f, 0.1f, 0.1f), SOLIDO, C_MADERA, V4(0.35f, 0.3f, 0.28f, 0), glm::vec2(2.0f), giro);
		}
	}
	// arboles secos
	for (int i = 0; i < 10; i++) {
		const float x = a.entre(-50.0f, 50.0f), z = a.entre(-44.0f, 4.0f);
		if (!o.libre(x, z, 6.0f)) continue;
		bosque::Azar b(700u + i);
		std::vector<bosque::PuntoTubo> t;
		V3 q(x, -0.3f, z);
		for (int k = 0; k <= 5; k++) {
			t.push_back({ q, 0.5f * (1.0f - 0.15f * k), V4(0.5f, 0.45f, 0.42f, 0), 0.8f });
			q += V3(b.entre(-0.8f, 0.8f), 1.6f, b.entre(-0.8f, 0.8f));
		}
		const uint32_t b0 = o.base();
		o.g.tubo(t, 6, SOLIDO, C_CORTEZA, 1.0f, 3.0f, 0.5f, 700u + i, true);
		for (int r = 0; r < 4; r++) {
			std::vector<bosque::PuntoTubo> rr;
			V3 s = t[2 + r % 3].p;
			const V3 d = glm::normalize(V3(b.entre(-1, 1), b.entre(0.3f, 0.9f), b.entre(-1, 1)));
			for (int k = 0; k <= 3; k++) {
				rr.push_back({ s, 0.22f * (1.0f - 0.25f * k), V4(0.5f, 0.45f, 0.42f, 0), 0.8f });
				s += d * 1.4f + V3(0, 0.3f * k, 0);
			}
			o.g.tubo(rr, 5, SOLIDO, C_CORTEZA, 1.0f, 3.0f, 0.0f, 900u + i * 4 + r, true);
		}
		o.tenir(b0, V4(0.35f, 0.32f, 0.32f, 0));
	}
	// fuegos fatuos fijos sobre algunas tumbas
	for (int i = 0; i < 5; i++) o.halos.push_back({ V3(a.entre(-30.0f, 30.0f), 2.0f, a.entre(-28.0f, -16.0f)), 1.2f, V4(0.4f, 0.9f, 1.0f, 1.2f) });
}

inline void obraLaberinto(Obra& o)
{
	const V4 piedra(0.62f, 0.58f, 0.55f, 0);
	o.disco(-0.3f, 0.0f, 90.0f, SUELO, C_ADOQUIN, V4(0.5f, 0.46f, 0.42f, 0), 4.0f);
	bosque::Azar a(4242u);
	const float celda = 7.0f;
	for (int i = -10; i <= 10; i++) {
		for (int j = -10; j <= 2; j++) {
			const float x = i * celda, z = j * celda;
			if (x * x + z * z > 72.0f * 72.0f) continue;
			// un muro horizontal o vertical por celda (laberinto de "arbol binario")
			const bool horizontal = a() < 0.5f;
			const V3 p0(x, -0.3f, z);
			const V3 p1 = horizontal ? V3(x + celda, -0.3f, z) : V3(x, -0.3f, z + celda);
			const V3 m = (p0 + p1) * 0.5f;
			if (!o.libre(m.x, m.z, 4.0f) || !o.libre(p0.x, p0.z, 3.0f) || !o.libre(p1.x, p1.z, 3.0f)) continue;
			o.muro(p0, p1, 7.0f, 1.2f, C_CASTILLO, piedra, false);
			if (a() < 0.18f) {
				const V3 c = m + V3(horizontal ? 0.0f : 0.7f, 4.0f, horizontal ? 0.7f : 0.0f);
				o.antorcha(c, (float)(i * 31 + j));
			}
		}
	}
	// las dos puertas de los Hermanos Paradoja, detras
	for (int lado = -1; lado <= 1; lado += 2) {
		o.caja(V3(lado * 6.0f, -0.3f, -30.0f), V3(3.5f, 5.0f, 0.5f), SOLIDO, C_MADERA, V4(0.45f, 0.3f, 0.2f, 0), glm::vec2(2.0f));
		o.caja(V3(lado * 6.0f, 9.7f, -30.0f), V3(3.9f, 0.6f, 0.8f), SOLIDO, C_CASTILLO, piedra);
		o.cilindro(V3(lado * 6.0f, 5.0f, -29.4f), 0.9f, 0.9f, 0.15f, 16, LUZ, SIN_TEXTURA, V4(1.0f, 0.75f, 0.2f, 0.8f));
	}
}

inline void obraRascacielos(Obra& o)
{
	// la azotea: losa con parapeto, el tanque de agua, la antena y el helipuerto (suelo)
	o.losa(-32.0f, -30.0f, 32.0f, 18.0f, -0.3f, SUELO, C_HORMIGON, V4(0.6f, 0.6f, 0.62f, 0), 5.0f);
	o.caja(V3(0, -40.0f, -6.0f), V3(32.0f, 19.85f, 24.0f), VENTANAS, C_HORMIGON, V4(0.5f, 0.5f, 0.55f, 0.9f), glm::vec2(3.0f, 3.6f));
	o.muro(V3(-32, -0.3f, -30), V3(32, -0.3f, -30), 1.2f, 0.4f, C_HORMIGON, V4(0.55f, 0.55f, 0.58f, 0), false);
	o.muro(V3(-32, -0.3f, -30), V3(-32, -0.3f, 18), 1.2f, 0.4f, C_HORMIGON, V4(0.55f, 0.55f, 0.58f, 0), false);
	o.muro(V3(32, -0.3f, -30), V3(32, -0.3f, 18), 1.2f, 0.4f, C_HORMIGON, V4(0.55f, 0.55f, 0.58f, 0), false);
	o.cilindro(V3(-24, -0.3f, -22), 0.25f, 0.25f, 4.0f, 6, SOLIDO, C_METAL, V4(0.4f, 0.4f, 0.42f, 0));
	o.cilindro(V3(-24, 3.7f, -22), 3.0f, 3.0f, 4.5f, 18, SOLIDO, C_MADERA, V4(0.5f, 0.42f, 0.36f, 0), glm::vec2(2.0f));
	o.cilindro(V3(-24, 8.2f, -22), 3.2f, 0.0f, 1.6f, 18, SOLIDO, C_METAL, V4(0.35f, 0.35f, 0.38f, 0));
	o.cilindro(V3(22, -0.3f, -24), 0.2f, 0.08f, 18.0f, 6, SOLIDO, C_METAL, V4(0.5f, 0.5f, 0.52f, 0));
	o.halos.push_back({ V3(22, 17.8f, -24), 1.6f, V4(1.0f, 0.15f, 0.1f, 1.6f) });
	for (int i = 0; i < 4; i++) o.caja(V3(18.0f + (i % 2) * 4.0f, -0.3f, -14.0f - (i / 2) * 4.0f), V3(1.5f, 1.0f, 1.5f), SOLIDO, C_METAL, V4(0.55f, 0.55f, 0.58f, 0));
	// la ciudad de noche: rascacielos alrededor que suben desde abajo
	bosque::Azar a(64u);
	for (int i = 0; i < 70; i++) {
		const float ang = a.entre(0.0f, 6.2832f), r = a.entre(42.0f, 72.0f);
		const float x = std::cos(ang) * r, z = std::sin(ang) * r - 8.0f;
		if (!o.libre(x, z, 26.0f)) continue;
		const float top = a.entre(-14.0f, 34.0f);
		const float w = a.entre(4.0f, 8.0f);
		o.edificio(V3(x, -70.0f, z), w, a.entre(4.0f, 8.0f), top + 70.0f, (float)(a() < 0.5f ? C_HORMIGON : C_METAL), V4(0.35f, 0.36f, 0.42f, 1.2f), a.entre(0.0f, 1.5f));
		if (a() < 0.35f) o.halos.push_back({ V3(x, top + 0.8f, z), 0.9f, V4(1.0f, 0.15f, 0.1f, 1.2f) });
	}
}

inline void obraCiudad(Obra& o)
{
	o.disco(-0.3f, 0.0f, 90.0f, SUELO, C_ADOQUIN, V4(0.85f, 0.82f, 0.8f, 0), 5.0f);
	bosque::Azar a(808u);
	for (int i = 0; i < 46; i++) {
		const float ang = a.entre(0.0f, 6.2832f), r = a.entre(36.0f, 70.0f);
		const float x = std::cos(ang) * r, z = std::sin(ang) * r - 10.0f;
		if (!o.libre(x, z, 18.0f)) continue;
		o.edificio(V3(x, -0.3f, z), a.entre(5.0f, 9.0f), a.entre(5.0f, 9.0f), a.entre(14.0f, 48.0f), (float)(a() < 0.4f ? C_LADRILLO : C_HORMIGON),
		           V4(a.entre(0.75f, 1.0f), a.entre(0.78f, 1.0f), a.entre(0.8f, 1.0f), 0.5f), a.entre(-0.3f, 0.3f));
	}
	// la torre de Kaiba Corp (de vidrio) y dos pantallas gigantes
	o.edificio(V3(10, -0.3f, -62), 7.0f, 7.0f, 52.0f, C_METAL, V4(0.55f, 0.65f, 0.8f, 0.6f));
	o.cilindro(V3(10, 51.7f, -62), 5.0f, 2.0f, 6.0f, 16, SOLIDO, C_METAL, V4(0.7f, 0.75f, 0.85f, 0));
	o.caja(V3(-24, 1.5f, -34), V3(7.0f, 3.5f, 0.3f), PANTALLA, SIN_TEXTURA, V4(0.2f, 0.7f, 1.0f, 1.4f));
	o.caja(V3(30, 1.0f, -28), V3(5.5f, 3.2f, 0.3f), PANTALLA, SIN_TEXTURA, V4(1.0f, 0.3f, 0.6f, 1.3f), glm::vec2(4.0f), -0.6f);
	// el dirigible de Kaiba en el cielo (lejos, quieto)
	const uint32_t b0 = o.base();
	o.g.bulto(V3(-30, 46.0f, -66), V3(16.0f, 4.5f, 5.0f), 2, SOLIDO, SIN_TEXTURA, 3u, 0.0f);
	o.tenir(b0, V4(0.75f, 0.76f, 0.8f, 0));
	for (int lado = -1; lado <= 1; lado += 2) {
		for (float z = -20.0f; z < 8.0f; z += 12.0f) {
			o.cilindro(V3(lado * 24.0f, -0.3f, z), 0.12f, 0.1f, 5.0f, 6, SOLIDO, C_METAL, V4(0.3f, 0.3f, 0.32f, 0));
			o.caja(V3(lado * 24.0f, 4.7f, z), V3(0.35f, 0.3f, 0.35f), LUZ, SIN_TEXTURA, V4(1.0f, 0.95f, 0.8f, 0.5f));
		}
		o.arbol(V3(lado * 28.0f, -0.3f, -16.0f), 7.0f, 77u + lado);
		o.arbol(V3(lado * 30.0f, -0.3f, 0.0f), 6.5f, 78u + lado);
	}
}

inline void obraDirigible(Obra& o)
{
	const V4 metal(0.62f, 0.64f, 0.7f, 0);
	// la cubierta (circulo de la arena y la pista hacia atras) y nada abajo: el mar de nubes del fondo
	o.disco(-0.3f, 0.0f, 28.0f, SUELO, C_METAL, V4(0.7f, 0.72f, 0.78f, 0), 4.0f, 72);
	o.losa(-16.0f, -60.0f, 16.0f, -24.0f, -0.31f, SUELO, C_METAL, V4(0.62f, 0.64f, 0.7f, 0), 4.0f);
	o.cilindro(V3(0, -3.3f, 0), 28.0f, 28.0f, 3.0f, 72, SOLIDO, C_METAL, V4(0.45f, 0.47f, 0.52f, 0), glm::vec2(4.0f), false);
	// baranda: postes y pasamanos alrededor
	const int N = 56;
	for (int k = 0; k < N; k++) {
		const float a0 = 6.2832f * k / N, a1 = 6.2832f * (k + 1) / N;
		const V3 p0(std::cos(a0) * 27.6f, -0.3f, std::sin(a0) * 27.6f), p1(std::cos(a1) * 27.6f, -0.3f, std::sin(a1) * 27.6f);
		if (std::fabs(p0.x) < 16.5f && p0.z < -20.0f) continue;   // la pista hacia atras
		o.cilindro(p0, 0.08f, 0.08f, 1.3f, 6, SOLIDO, C_METAL, metal);
		const V3 d = p1 - p0;
		o.caja((p0 + p1) * 0.5f + V3(0, 1.2f, 0), V3(glm::length(d) * 0.5f, 0.05f, 0.05f), SOLIDO, C_METAL, metal, glm::vec2(4.0f), std::atan2(-d.z, d.x));
	}
	// la torre de mando atras, con ventanas encendidas, el radar y luces de posicion
	o.caja(V3(0, -0.3f, -50.0f), V3(10.0f, 6.0f, 6.0f), SOLIDO, C_METAL, metal);
	o.caja(V3(0, 5.0f, -43.9f), V3(8.5f, 1.2f, 0.1f), LUZ, SIN_TEXTURA, V4(0.5f, 0.85f, 1.0f, 1.2f));
	o.tronco4(V3(0, 11.7f, -50.0f), 0.0f, 7.0f, 4.5f, 4.0f, 3.0f, 5.0f, SOLIDO, C_METAL, metal);
	o.caja(V3(0, 14.0f, -45.4f), V3(5.0f, 0.7f, 0.1f), LUZ, SIN_TEXTURA, V4(0.5f, 0.85f, 1.0f, 1.0f));
	o.cilindro(V3(0, 16.7f, -50.0f), 0.2f, 0.15f, 6.0f, 6, SOLIDO, C_METAL, metal);
	o.cilindro(V3(0, 21.0f, -50.0f), 2.4f, 0.4f, 0.8f, 16, SOLIDO, C_METAL, V4(0.8f, 0.82f, 0.88f, 0));
	o.halos.push_back({ V3(0, 23.0f, -50.0f), 1.0f, V4(1.0f, 0.2f, 0.15f, 1.6f) });
	o.halos.push_back({ V3(-27.6f, 1.6f, 0), 0.8f, V4(1.0f, 0.15f, 0.1f, 1.6f) });
	o.halos.push_back({ V3(27.6f, 1.6f, 0), 0.8f, V4(0.2f, 1.0f, 0.3f, 1.6f) });
	// motores a los lados, mas abajo (cilindros con helice girada)
	for (int lado = -1; lado <= 1; lado += 2) {
		const V3 m(lado * 38.0f, -6.0f, -20.0f);
		o.caja(V3(lado * 31.0f, -5.0f, -20.0f), V3(5.0f, 0.4f, 1.0f), SOLIDO, C_METAL, metal);
		const uint32_t b0 = o.base();
		o.g.bulto(m, V3(2.4f, 2.4f, 6.0f), 2, SOLIDO, C_METAL, 9u, 0.0f);
		o.tenir(b0, V4(0.6f, 0.62f, 0.68f, 0));
		o.halos.push_back({ m + V3(0, 0, 6.5f), 2.2f, V4(0.4f, 0.8f, 1.0f, 0.8f) });
	}
}

inline void obraAlcatraz(Obra& o)
{
	const V4 metal(0.55f, 0.52f, 0.5f, 0);
	o.disco(-0.3f, 0.0f, 26.0f, SUELO, C_METAL, V4(0.62f, 0.58f, 0.55f, 0), 4.0f, 72);
	o.cilindro(V3(0, -46.0f, 0), 30.0f, 26.0f, 45.7f, 64, VENTANAS, C_HORMIGON, V4(0.55f, 0.5f, 0.5f, 0.6f), glm::vec2(3.0f, 3.6f), false);
	for (int k = 0; k < 4; k++) o.cilindro(V3(0, -4.0f - k * 9.0f, 0), 26.4f + k * 0.9f, 26.4f + k * 0.9f, 0.6f, 64, LUZ, SIN_TEXTURA, V4(1.0f, 0.6f, 0.2f, 0.9f));
	// los cuatro pilares de la torre de duelo y el arco por encima
	for (int k = 0; k < 4; k++) {
		const float a = 0.785f + 1.5708f * k;
		const V3 p(std::cos(a) * 23.5f, -0.3f, std::sin(a) * 23.5f);
		if (p.z > 0.0f) continue;
		o.tronco4(p, -a, 0.7f, 0.7f, 0.35f, 0.35f, 22.0f, SOLIDO, C_METAL, metal);
		o.halos.push_back({ p + V3(0, 22.4f, 0), 1.2f, V4(1.0f, 0.4f, 0.1f, 1.5f) });
	}
	o.muro(V3(-25, -0.3f, -10), V3(-25, -0.3f, 10), 1.2f, 0.4f, C_METAL, metal, false);
	o.muro(V3(25, -0.3f, -10), V3(25, -0.3f, 10), 1.2f, 0.4f, C_METAL, metal, false);
	// el mar muy abajo y la isla con la prision
	o.disco(-46.0f, 30.0f, 100.0f, AGUA, SIN_TEXTURA, V4(0.04f, 0.04f, 0.07f, 0), 10.0f, 96);
	o.roca(V3(-10, -48.0f, -62), V3(30, 10, 14), 21u, V4(0.5f, 0.42f, 0.38f, 0));
	o.edificio(V3(-10, -40.0f, -60), 14.0f, 6.0f, 12.0f, C_HORMIGON, V4(0.6f, 0.55f, 0.5f, 0.5f));
}

inline void obraMuseo(Obra& o)
{
	const V4 piedra(0.95f, 0.85f, 0.7f, 0), oro(1.0f, 0.75f, 0.3f, 0);
	o.disco(-0.3f, 0.0f, 60.0f, SUELO, C_BALDOSA, V4(0.9f, 0.85f, 0.78f, 0), 4.0f);
	// la sala: paredes (sin techo a la vista de la camara de juego) con columnas
	o.muro(V3(-40, -0.3f, -44), V3(40, -0.3f, -44), 18.0f, 2.0f, C_ARENISCA, piedra, false);
	for (int lado = -1; lado <= 1; lado += 2) {
		o.muro(V3(lado * 40.0f, -0.3f, -44), V3(lado * 40.0f, -0.3f, 24), 18.0f, 2.0f, C_ARENISCA, piedra, false);
		for (float z = -38.0f; z <= 10.0f; z += 12.0f) {
			o.columna(V3(lado * 36.0f, -0.3f, z), 1.2f, 17.0f, C_ARENISCA, piedra);
			o.halos.push_back({ V3(lado * 38.5f, 12.0f, z + 6.0f), 2.5f, V4(1.0f, 0.75f, 0.4f, 0.35f) });
		}
		// vitrinas encendidas
		for (float z = -26.0f; z <= 2.0f; z += 14.0f) {
			o.caja(V3(lado * 27.0f, -0.3f, z), V3(2.0f, 0.6f, 1.2f), SOLIDO, C_MADERA, V4(0.4f, 0.28f, 0.18f, 0), glm::vec2(2.0f));
			o.caja(V3(lado * 27.0f, 0.9f, z), V3(1.9f, 0.9f, 1.1f), LUZ, SIN_TEXTURA, V4(0.9f, 0.8f, 0.55f, 0.35f));
		}
	}
	// la tabla de piedra del Faraon (relieve: bandas), los sarcofagos y dos estatuas
	o.caja(V3(0, -0.3f, -40.5f), V3(11.0f, 6.5f, 1.2f), SOLIDO, C_ROCA, V4(0.55f, 0.5f, 0.45f, 0));
	for (int k = 0; k < 4; k++) o.caja(V3(0, 1.5f + k * 2.8f, -39.25f), V3(9.5f, 0.12f, 0.06f), LUZ, SIN_TEXTURA, V4(1.0f, 0.8f, 0.4f, 0.25f));
	o.halos.push_back({ V3(0, 7.0f, -37.5f), 9.0f, V4(1.0f, 0.75f, 0.45f, 0.25f) });
	for (int lado = -1; lado <= 1; lado += 2) {
		o.caja(V3(lado * 18.0f, -0.3f, -34.0f), V3(1.3f, 0.7f, 3.0f), SOLIDO, SIN_TEXTURA, oro);
		o.tronco4(V3(lado * 18.0f, 1.1f, -34.0f), 0.0f, 1.2f, 2.9f, 0.9f, 2.6f, 0.6f, SOLIDO, SIN_TEXTURA, oro);
		o.caja(V3(lado * 26.0f, -0.3f, -38.0f), V3(1.6f, 1.0f, 1.6f), SOLIDO, C_ARENISCA, piedra);
		o.tronco4(V3(lado * 26.0f, 1.7f, -38.0f), 0.0f, 1.1f, 1.0f, 0.8f, 0.7f, 7.0f, SOLIDO, C_ROCA, V4(0.35f, 0.32f, 0.3f, 0));
		o.cilindro(V3(lado * 26.0f, 8.7f, -38.0f), 0.9f, 0.6f, 1.6f, 10, SOLIDO, SIN_TEXTURA, oro);
	}
}

inline void obraNoah(Obra& o)
{
	const V4 verde(0.25f, 1.0f, 0.55f, 1.3f);
	o.disco(-0.3f, 0.0f, 90.0f, SUELO, SIN_TEXTURA, V4(1.0f), 4.0f);
	bosque::Azar a(1337u);
	for (int i = 0; i < 40; i++) {
		const float x = a.entre(-66.0f, 66.0f), z = a.entre(-70.0f, 8.0f);
		if (!o.libre(x, z, 8.0f)) continue;
		const float t = a.entre(1.5f, 5.0f);
		const float y = a() < 0.5f ? -0.3f : a.entre(4.0f, 20.0f);
		if (a() < 0.7f) {
			o.tronco4(V3(x, y, z), a.entre(0.0f, 1.5f), t, t, t, t, t * 2.0f, HOLO, SIN_TEXTURA, verde, glm::vec2(t * 2.0f), true, true);
		} else {
			o.tronco4(V3(x, y, z), a.entre(0.0f, 1.5f), t, t, 0.0f, 0.0f, t * 2.2f, HOLO, SIN_TEXTURA, verde, glm::vec2(t * 2.0f));
		}
	}
	// torres de datos altas a los lados y detras
	for (int i = 0; i < 12; i++) {
		const float ang = -3.1416f + 3.1416f * i / 11.0f;
		const float x = std::cos(ang) * 55.0f, z = std::sin(ang) * 50.0f - 8.0f;
		if (!o.libre(x, z, 12.0f)) continue;
		const float alto = a.entre(20.0f, 46.0f);
		o.tronco4(V3(x, -0.3f, z), 0.0f, 2.5f, 2.5f, 2.5f, 2.5f, alto, HOLO, SIN_TEXTURA, verde, glm::vec2(5.0f, 4.0f));
	}
}

inline void obraAtlantida(Obra& o)
{
	const V4 marmol(0.75f, 0.85f, 0.82f, 0), oricalco(0.3f, 1.0f, 0.4f, 1.6f);
	o.disco(-0.3f, 0.0f, 44.0f, SUELO, C_MARMOL, V4(0.6f, 0.7f, 0.68f, 0), 6.0f);
	o.disco(-1.4f, 40.0f, 100.0f, AGUA, SIN_TEXTURA, V4(0.01f, 0.06f, 0.06f, 0), 10.0f, 96);
	o.cilindro(V3(0, -3.0f, 0), 44.0f, 44.0f, 2.7f, 72, SOLIDO, C_MARMOL, marmol, glm::vec2(6.0f), false);
	bosque::Azar a(99u);
	// anillo de columnas (algunas rotas)
	for (int k = 0; k < 24; k++) {
		const float ang = 6.2832f * k / 24.0f;
		const V3 p(std::cos(ang) * 33.0f, -0.3f, std::sin(ang) * 33.0f);
		if (!o.libre(p.x, p.z, 4.0f)) continue;
		const bool rota = a() < 0.35f;
		o.columna(p, 1.3f, 14.0f, C_MARMOL, marmol, rota);
		if (!rota) o.halos.push_back({ p + V3(0, 14.5f, 0), 1.4f, V4(0.3f, 1.0f, 0.4f, 0.7f) });
		if (rota) o.roca(p + V3(a.entre(-3.0f, 3.0f), 0.0f, a.entre(-3.0f, 3.0f)), V3(1.4f, 0.8f, 1.2f), 300u + k, marmol, 1);
	}
	// las agujas de la Atlantida detras, con vetas de Oricalco
	for (int i = 0; i < 7; i++) {
		const float x = -54.0f + i * 18.0f, z = -64.0f + std::fabs((float)i - 3.0f) * 6.0f;
		const float alto = 30.0f + (3 - std::abs(i - 3)) * 12.0f;
		o.tronco4(V3(x, -2.0f, z), 0.785f, 4.0f, 4.0f, 0.4f, 0.4f, alto, SOLIDO, C_MARMOL, V4(0.5f, 0.62f, 0.62f, 0));
		for (int k = 1; k <= 3; k++) o.caja(V3(x, -2.0f + 2.4f * k, z), V3(4.1f - k * 0.9f, 0.18f, 4.1f - k * 0.9f), LUZ, SIN_TEXTURA, V4(0.3f, 1.0f, 0.4f, 1.2f), glm::vec2(4.0f), 0.785f);
		o.halos.push_back({ V3(x, alto - 2.0f, z), 2.0f + (3 - std::abs(i - 3)) * 0.5f, oricalco });
		o.halos.push_back({ V3(x, 3.0f, z + 4.5f), 3.0f, V4(0.3f, 1.0f, 0.4f, 0.5f) });
	}
	// la gran piedra de Oricalco (el ojo) detras de la mesa
	o.halos.push_back({ V3(0, 4.0f, -36.0f), 5.0f, V4(0.3f, 1.0f, 0.4f, 0.9f) });
}

inline void obraEstadio(Obra& o)
{
	o.disco(-0.3f, 0.0f, 30.0f, SUELO, C_HORMIGON, V4(0.38f, 0.42f, 0.5f, 0), 5.0f, 72);
	// gradas: 8 escalones con publico (frente de cada escalon) y la pasarela; el muro del campo
	o.cilindro(V3(0, -0.3f, 0), 30.0f, 30.0f, 2.0f, 72, SOLIDO, C_METAL, V4(0.25f, 0.3f, 0.45f, 0), glm::vec2(4.0f), false);
	o.cilindro(V3(0, 1.4f, 0), 30.05f, 30.05f, 0.25f, 72, LUZ, SIN_TEXTURA, V4(0.2f, 0.6f, 1.0f, 1.6f), glm::vec2(4.0f), false);
	for (int k = 0; k < 9; k++) {
		const float r = 31.0f + k * 3.2f, y = 1.7f + k * 2.0f;
		const int lados = 96;
		const uint32_t b0 = o.base();
		for (int i = 0; i <= lados; i++) {
			const float a = 6.2832f * i / lados;
			const V3 d(std::cos(a), 0, std::sin(a));
			const float u = 3.1416f * 2.0f * r / 1.3f * i / lados;   // una persona cada 1.3 m
			o.vert(d * r + V3(0, y - 2.0f, 0), PUBLICO, -d, V4(1.0f), glm::vec2(u / 3.0f, 0.0f), SIN_TEXTURA);
			o.vert(d * r + V3(0, y, 0), PUBLICO, -d, V4(1.0f), glm::vec2(u / 3.0f, 1.0f), SIN_TEXTURA);
			o.vert(d * r + V3(0, y, 0), SOLIDO, V3(0, 1, 0), V4(0.3f, 0.32f, 0.38f, 0), glm::vec2(u / 3.0f, 0.0f), C_HORMIGON);
			o.vert(d * (r + 3.2f) + V3(0, y, 0), SOLIDO, V3(0, 1, 0), V4(0.3f, 0.32f, 0.38f, 0), glm::vec2(u / 3.0f, 0.8f), C_HORMIGON);
		}
		for (int i = 0; i < lados; i++) {
			const uint32_t a = b0 + 4 * i;
			o.g.tri(a, a + 4, a + 1);
			o.g.tri(a + 1, a + 4, a + 5);
			o.g.tri(a + 2, a + 6, a + 3);
			o.g.tri(a + 3, a + 6, a + 7);
		}
	}
	// techo y torres de luz, pantallas gigantes
	o.cilindro(V3(0, 19.7f, 0), 60.0f, 56.0f, 1.5f, 96, SOLIDO, C_METAL, V4(0.35f, 0.38f, 0.45f, 0), glm::vec2(4.0f), false);
	for (int k = 0; k < 4; k++) {
		const float a = 0.785f + 1.5708f * k + 3.1416f;
		const V3 p(std::cos(a) * 62.0f, -0.3f, std::sin(a) * 62.0f);
		o.tronco4(p, -a, 1.2f, 1.2f, 0.8f, 0.8f, 38.0f, SOLIDO, C_METAL, V4(0.45f, 0.48f, 0.55f, 0));
		const V3 f = p + V3(0, 39.0f, 0) - glm::normalize(V3(p.x, 0, p.z)) * 1.2f;
		o.caja(f, V3(4.0f, 2.2f, 0.3f), LUZ, SIN_TEXTURA, V4(1.0f, 1.0f, 0.95f, 2.0f), glm::vec2(4.0f), -a + 1.5708f);
		o.halos.push_back({ f + V3(0, 2.2f, 0), 9.0f, V4(0.9f, 0.95f, 1.0f, 0.45f) });
	}
	o.caja(V3(0, 26.0f, -58.0f), V3(14.0f, 6.0f, 0.5f), PANTALLA, SIN_TEXTURA, V4(0.2f, 0.55f, 1.0f, 1.3f));
	o.caja(V3(-22, -0.3f, -22.0f), V3(5.0f, 3.0f, 0.4f), PANTALLA, SIN_TEXTURA, V4(0.2f, 0.6f, 1.0f, 1.2f), glm::vec2(4.0f), -0.6f);
	o.caja(V3(22, -0.3f, -22.0f), V3(5.0f, 3.0f, 0.4f), PANTALLA, SIN_TEXTURA, V4(1.0f, 0.4f, 0.3f, 1.2f), glm::vec2(4.0f), 0.6f);
	o.caja(V3(-48, 24.0f, -32.0f), V3(9.0f, 4.0f, 0.5f), PANTALLA, SIN_TEXTURA, V4(1.0f, 0.4f, 0.2f, 1.2f), glm::vec2(4.0f), 0.98f);
	o.caja(V3(48, 24.0f, -32.0f), V3(9.0f, 4.0f, 0.5f), PANTALLA, SIN_TEXTURA, V4(0.3f, 1.0f, 0.6f, 1.2f), glm::vec2(4.0f), -0.98f);
}

inline void obraTemplo(Obra& o, bool sombras)
{
	const V4 piedra = sombras ? V4(0.45f, 0.38f, 0.42f, 0) : V4(1.0f, 0.92f, 0.8f, 0);
	o.disco(-0.3f, 0.0f, 36.0f, SUELO, C_LOSA, sombras ? V4(0.42f, 0.36f, 0.4f, 0) : V4(0.95f, 0.9f, 0.82f, 0), 6.0f);
	o.disco(-0.32f, 36.0f, 100.0f, SUELO, C_ARENA, sombras ? V4(0.35f, 0.28f, 0.32f, 0) : V4(1.0f, 0.92f, 0.78f, 0), 9.0f);
	bosque::Azar a(sombras ? 666u : 1919u);
	// filas de columnas a los lados (rotas en las Sombras)
	for (int lado = -1; lado <= 1; lado += 2) {
		for (float z = -40.0f; z <= 6.0f; z += 8.0f) {
			const bool rota = sombras && a() < 0.45f;
			o.columna(V3(lado * 24.0f, -0.3f, z), 1.6f, 15.0f, C_ARENISCA, piedra, rota);
			if (!rota && !sombras) o.caja(V3(lado * 24.0f, 14.9f, z + 4.0f), V3(2.0f, 0.6f, 4.0f), SOLIDO, C_ARENISCA, piedra, glm::vec2(3.0f));
		}
		o.brasero(V3(lado * 19.0f, -0.3f, -16.0f), lado * 3.0f);
		o.brasero(V3(lado * 19.0f, -0.3f, 2.0f), lado * 5.0f);
		// obeliscos y esfinges (bloques) junto al portico
		o.tronco4(V3(lado * 9.0f, -0.3f, -30.0f), 0.0f, 1.3f, 1.3f, 0.8f, 0.8f, 16.0f, SOLIDO, C_ARENISCA, piedra, glm::vec2(3.0f));
		o.tronco4(V3(lado * 9.0f, 15.7f, -30.0f), 0.0f, 0.8f, 0.8f, 0.0f, 0.0f, 1.5f, SOLIDO, SIN_TEXTURA, V4(1.0f, 0.78f, 0.3f, 0));
		o.caja(V3(lado * 15.0f, -0.3f, -26.0f), V3(1.4f, 1.2f, 3.5f), SOLIDO, C_ARENISCA, piedra, glm::vec2(3.0f));
		o.caja(V3(lado * 15.0f, 2.1f, -23.2f), V3(1.1f, 1.4f, 1.0f), SOLIDO, C_ARENISCA, piedra, glm::vec2(3.0f));
	}
	// el portico (pilono): dos torres inclinadas y el dintel, con la puerta oscura y el ojo dorado
	const float zp = -40.0f;
	for (int lado = -1; lado <= 1; lado += 2) o.tronco4(V3(lado * 12.0f, -0.3f, zp), 0.0f, 8.0f, 4.0f, 6.5f, 3.2f, sombras ? 7.0f : 22.0f, SOLIDO, C_ARENISCA, piedra);
	if (!sombras) o.caja(V3(0, -0.3f, zp - 1.0f), V3(4.2f, 8.0f, 2.5f), SOLIDO, SIN_TEXTURA, V4(0.05f, 0.04f, 0.04f, 0));
	if (!sombras) o.caja(V3(0, 15.7f, zp), V3(5.0f, 2.0f, 3.0f), SOLIDO, C_ARENISCA, piedra);
	o.cilindro(V3(0, sombras ? 4.0f : 17.7f, zp + 3.05f), 1.6f, 1.6f, 0.1f, 20, LUZ, SIN_TEXTURA, sombras ? V4(0.8f, 0.1f, 0.9f, 1.2f) : V4(1.0f, 0.75f, 0.25f, 0.8f));
	// piramides lejanas
	if (!sombras) {
		o.tronco4(V3(-38, -0.5f, -70), 0.3f, 22.0f, 22.0f, 0.0f, 0.0f, 28.0f, SOLIDO, C_ARENISCA, V4(1.0f, 0.9f, 0.72f, 0), glm::vec2(5.0f));
		o.tronco4(V3(30, -0.5f, -72), -0.2f, 16.0f, 16.0f, 0.0f, 0.0f, 20.0f, SOLIDO, C_ARENISCA, V4(1.0f, 0.9f, 0.72f, 0), glm::vec2(5.0f));
		for (int i = 0; i < 6; i++) {   // palmeras
			const float x = (i < 3 ? -1.0f : 1.0f) * (32.0f + (i % 3) * 7.0f), z = -10.0f - (i % 3) * 12.0f;
			std::vector<bosque::PuntoTubo> t;
			for (int k = 0; k <= 5; k++) t.push_back({ V3(x + std::sin(k * 0.4f) * 1.2f, -0.3f + k * 2.0f, z), 0.4f - k * 0.03f, V4(0.7f, 0.6f, 0.5f, 0), 1.0f });
			const uint32_t b0 = o.base();
			o.g.tubo(t, 7, SOLIDO, C_CORTEZA, 1.0f, 2.0f, 0.0f, 50u + i, true);
			o.tenir(b0, V4(0.8f, 0.7f, 0.55f, 0));
			const uint32_t b1 = o.base();
			o.g.bulto(t.back().p + V3(0, 0.5f, 0), V3(3.8f, 1.0f, 3.8f), 1, SOLIDO, C_HOJAS, 80u + i, 0.6f);
			o.tenir(b1, V4(0.5f, 1.25f, 0.4f, 0));
		}
	} else {
		// la silueta de Zorc detras del portico: cuerpo, cabeza, cuernos y los ojos rojos
		const V4 negro(0.03f, 0.02f, 0.035f, 0);
		uint32_t b0 = o.base();
		// (sale del suelo detras del portico: la cabeza y los hombros a la altura que ve la camara de juego)
		const float zy = -36.0f;
		o.g.bulto(V3(0, 18.0f + zy, -50.0f), V3(16.0f, 22.0f, 7.0f), 2, SOLIDO, SIN_TEXTURA, 6u, 0.15f);
		o.g.bulto(V3(0, 42.0f + zy, -48.0f), V3(6.0f, 6.5f, 5.0f), 2, SOLIDO, SIN_TEXTURA, 7u, 0.1f);
		o.g.bulto(V3(-18, 30.0f + zy, -48.0f), V3(9.0f, 5.0f, 5.0f), 1, SOLIDO, SIN_TEXTURA, 8u, 0.2f);
		o.g.bulto(V3(18, 30.0f + zy, -48.0f), V3(9.0f, 5.0f, 5.0f), 1, SOLIDO, SIN_TEXTURA, 9u, 0.2f);
		o.tenir(b0, negro);
		for (int lado = -1; lado <= 1; lado += 2) {
			std::vector<bosque::PuntoTubo> t;
			for (int k = 0; k <= 5; k++) t.push_back({ V3(lado * (4.0f + k * 1.8f), 46.0f + zy + k * 1.6f - k * k * 0.15f, -48.0f), 1.3f * (1.0f - k / 5.5f), negro, 1.0f });
			b0 = o.base();
			o.g.tubo(t, 6, SOLIDO, SIN_TEXTURA, 1.0f, 2.0f, 0.0f, 60u, true);
			o.tenir(b0, negro);
			o.halos.push_back({ V3(lado * 2.2f, 42.5f + zy, -43.0f), 1.6f, V4(1.0f, 0.1f, 0.05f, 2.5f) });
		}
		for (int i = 0; i < 16; i++) {
			const float x = a.entre(-60.0f, 60.0f), z = a.entre(-60.0f, 4.0f);
			if (!o.libre(x, z, 14.0f)) continue;
			o.roca(V3(x, -0.5f, z), V3(a.entre(1.0f, 3.5f), a.entre(0.6f, 2.0f), a.entre(1.0f, 3.0f)), 500u + i, V4(0.35f, 0.3f, 0.34f, 0), 1);
		}
	}
}

inline void construir(Obra& o, int e)
{
	switch (e) {
	case TIENDA: obraTienda(o); break;
	case CASTILLO: obraCastillo(o); break;
	case ISLA: obraIsla(o); break;
	case PLAYA: obraPlaya(o); break;
	case ACANTILADO: obraAcantilado(o); break;
	case LABERINTO: obraLaberinto(o); break;
	case RASCACIELOS: obraRascacielos(o); break;
	case CIUDAD: obraCiudad(o); break;
	case DIRIGIBLE: obraDirigible(o); break;
	case ALCATRAZ: obraAlcatraz(o); break;
	case MUSEO: obraMuseo(o); break;
	case NOAH: obraNoah(o); break;
	case ATLANTIDA: obraAtlantida(o); break;
	case ESTADIO: obraEstadio(o); break;
	case TEMPLO: obraTemplo(o, false); break;
	case SOMBRAS: obraTemplo(o, true); break;
	default: break;
	}
}

// ------------------------------------------------------------------------------------- fondos pre-renderizados
// Rama classic-escenarios (docs/escenarios.md): el lugar se pre-renderiza en Blender (Cycles, insumos CC0) como un
// panorama equirectangular visto desde la camara de juego, con su mapa de distancias. El fondo (entornos_cielo.frag)
// lo reproyecta desde la camara real con esas distancias (paralaje correcto al mover la camara) y el motor le suma lo
// vivo: llamas animadas donde estan las antorchas del render, particulas y la luz del tablero acorde.
// <assets>/entornos/pano/<clave>.jpg (color, sRGB), <clave>_prof.png (16 bits: log2(m) / 12; 65535 = cielo) y
// <clave>.txt: "origen x y z" (de donde se renderizo, coordenadas del juego) y "llama x y z alto" por antorcha.
struct InfoPano {
	bool hay{ false };
	V3 origen{ 0.0f };
	std::vector<V4> llamas;   // xyz (la base de la llama), alto (m)
};
inline InfoPano leerInfoPano(const std::string& dir, int e)
{
	InfoPano r;
	FILE* f = fopen((dir + CLAVES[e] + ".txt").c_str(), "r");
	if (!f) return r;
	char linea[256];
	while (fgets(linea, sizeof(linea), f)) {
		float x, y, z, w;
		if (sscanf(linea, "origen %f %f %f", &x, &y, &z) == 3) {
			r.origen = V3(x, y, z);
			r.hay = true;
		} else if (sscanf(linea, "llama %f %f %f %f", &x, &y, &z, &w) == 4) {
			r.llamas.push_back(V4(x, y, z, w));
		}
	}
	fclose(f);
	return r;
}

// ------------------------------------------------------------------------------- fondos por personaje (fotos)
// Rama classic-fondos (docs/fondos.md): cada personaje tiene SU fondo, una imagen panoramica fotorrealista (matte
// painting de Gemini, escalada) con su mapa de profundidad (Depth Anything V2), sin geometria por codigo delante.
// datos/fondos.txt (en ui/): "id|tema|respaldo|lugar_es|lugar_en": el tema (una clave de arriba) da las particulas y
// la luz base; respaldo: el fondo que se usa mientras falte el propio (los "tema_<clave>" de los rivales genericos).
// <assets>/fondos/<id>.jpg (color sRGB), <id>_prof.png (16 bits: log2(m) / 12; 65535 = cielo) y <id>.txt
// ("plano <ancho grados> <abajo grados>", "origen x y z", "cielo r g b", "niebla r g b", "tinte r g b"; fondos_post.py).
// La imagen se envuelve en un cilindro alrededor de la camara de juego: u = acimut / ancho (espejada fuera), v lineal
// en tan(elevacion) desde "abajo" (el borde de abajo queda escondido tras la mesa) con el aspecto de la imagen.
struct FilaFondo {
	std::string id, respaldo;
	int tema{ NEUTRO };
};
struct InfoFondo {
	bool hay{ false };
	V3 origen{ 0.0f, 9.6768f, 19.3536f };   // la camara de juego (OJO de yugireload.cpp)
	float ancho{ 110.0f }, abajo{ -28.0f };   // grados
	V3 cielo{ 0.6f, 0.7f, 0.85f }, niebla{ 0.5f }, tinte{ 1.0f };
};
inline InfoFondo leerInfoFondo(const std::string& ruta)
{
	InfoFondo r;
	FILE* f = fopen(ruta.c_str(), "r");
	if (!f) return r;
	char linea[256];
	while (fgets(linea, sizeof(linea), f)) {
		float x, y, z;
		if (sscanf(linea, "plano %f %f", &x, &y) == 2) {
			r.ancho = x;
			r.abajo = y;
			r.hay = true;
		} else if (sscanf(linea, "origen %f %f %f", &x, &y, &z) == 3) {
			r.origen = V3(x, y, z);
		} else if (sscanf(linea, "cielo %f %f %f", &x, &y, &z) == 3) {
			r.cielo = V3(x, y, z);
		} else if (sscanf(linea, "niebla %f %f %f", &x, &y, &z) == 3) {
			r.niebla = V3(x, y, z);
		} else if (sscanf(linea, "tinte %f %f %f", &x, &y, &z) == 3) {
			r.tinte = V3(x, y, z);
		}
	}
	fclose(f);
	return r;
}
inline std::vector<FilaFondo> leerTablaFondos(const std::string& ruta)
{
	std::vector<FilaFondo> t;
	FILE* f = fopen(ruta.c_str(), "r");
	if (!f) return t;
	char linea[512];
	while (fgets(linea, sizeof(linea), f)) {
		std::string l(linea);
		while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
		if (l.empty() || l[0] == '#') continue;
		std::vector<std::string> c;
		size_t a = 0;
		for (size_t b; (b = l.find('|', a)) != std::string::npos; a = b + 1) c.push_back(l.substr(a, b - a));
		c.push_back(l.substr(a));
		if (c.size() < 3 || c[0].empty()) continue;
		t.push_back({ c[0], c[2], claveDe(c[1]) });
	}
	fclose(f);
	return t;
}

// ------------------------------------------------------------------------------------------------- el sistema
class Entornos {
public:
	struct Rango { uint32_t primerIndice{ 0 }, indices{ 0 }, primerAditivo{ 0 }, aditivos{ 0 }; };
	std::array<Rango, NUM_ENTORNOS> rangos{};
	bool listo{ false };
	int actual{ NEUTRO }, objetivo{ NEUTRO };
	float peso{ 0.0f };          // 0..1 del entorno actual (el cambio: baja, se cambia y sube)
	float pesoVisible{ 0.0f };   // con la carta de campo: peso * (1 - ambiente)
	float msTexturas{ 0.0f };
	uint32_t dibujosCuadro{ 0 };

	// Rama classic-campos: la carta de campo manda sobre el fondo del rival. pedirCampo("campo_<carta>") cambia el fondo
	// (con el mismo fundido) y lo que se pida mientras tanto (pedir) queda guardado; soltarCampo() vuelve a eso.
	void pedir(int e) { conBase([&] { elegir(e); }); }
	void pedir(const std::string& clave) { conBase([&] { elegir(clave); }); }
	void pedirCampo(const std::string& id)
	{
		if (campo.empty()) {
			baseObjetivo = objetivo;
			baseFondo = fondoObjetivo;
		}
		campo = id;
		elegir(id);
	}
	void soltarCampo()
	{
		if (campo.empty()) return;
		campo.clear();
		objetivo = baseObjetivo;
		fondoObjetivo = baseFondo;
	}
	bool hayFondo(const std::string& id) const { return existeFondo(id); }
	std::string campo;   // el fondo de la carta de campo activa ("" = ninguna)

	// un tema (los rivales genericos): su fondo "tema_<clave>" si existe, si no el respaldo de la tabla
	void elegir(int e)
	{
		objetivo = (e < 0 || e >= NUM_ENTORNOS) ? NEUTRO : e;
		fondoObjetivo.clear();
		if (objetivo != NEUTRO) fondoObjetivo = resolverFondo(std::string("tema_") + CLAVES[objetivo]);
	}
	// una clave de tema o el id de un personaje (su fondo propio: datos/fondos.txt)
	void elegir(const std::string& clave)
	{
		const int e = claveDe(clave);
		if (e != NEUTRO || clave == CLAVES[NEUTRO] || clave.empty()) {
			elegir(e);
			return;
		}
		const FilaFondo* f = fila(clave);
		objetivo = f ? f->tema : NEUTRO;
		fondoObjetivo = resolverFondo(clave);
		if (objetivo == NEUTRO && !fondoObjetivo.empty()) objetivo = CASTILLO;   // un fondo sin tema: luz de atardecer
	}
	const char* clave() const { return CLAVES[actual]; }
	// lo pedido, ya, sin fundido (capturas)
	void fijarYa()
	{
		actual = objetivo;
		fondoActual = fondoObjetivo;
		peso = 1.0f;
		cargaSincrona = true;   // (la primera imagen se espera: las capturas la necesitan ya)
	}
	// lo que se pidio (para los registros): el fondo propio o la clave del tema
	std::string nombreObjetivo() const { return fondoObjetivo.empty() ? std::string(CLAVES[objetivo]) : fondoObjetivo + " (" + CLAVES[objetivo] + ")"; }
	bool conFondo() const { return !fondoActual.empty() && cargada == "f:" + fondoActual && infoFondo.hay; }

	// cada cuadro: el fundido del cambio de entorno y lo que tapa la carta de campo (ambiente del Bosque 0..1)
	void avanzar(float dt, float ambienteCampo)
	{
		if (objetivo != actual || fondoObjetivo != fondoActual) {
			peso = std::max(0.0f, peso - dt / 1.1f);
			if (peso <= 0.0f) {
				actual = objetivo;
				fondoActual = fondoObjetivo;
			}
		} else if (actual != NEUTRO && !cargandoFondo()) {   // (con la imagen nueva ya en la GPU)
			peso = std::min(1.0f, peso + dt / 1.6f);
		}
		// rama classic-campos: la imagen pedida se decodifica en otro hilo mientras lo viejo se funde (sin tirones)
		if (listo && !fondoObjetivo.empty() && fondoObjetivo != fondoActual) empezarDecodificar(dirFondos + fondoObjetivo);
		pesoVisible = peso * (1.0f - glm::clamp(ambienteCampo, 0.0f, 1.0f));
		if (actual == NEUTRO) pesoVisible = 0.0f;
		// la imagen del entorno actual (una a la vez en memoria; se carga con el fondo ya fundido): el fondo propio
		// del personaje (rama classic-fondos) o, si no tiene, el pre-renderizado del tema (classic-escenarios)
		if (!listo || actual == NEUTRO) return;
		if (!fondoActual.empty()) {
			if (cargada != "f:" + fondoActual && cargada != "x:" + fondoActual) cargarFondo(fondoActual);
		} else if (infoPano[actual].hay && cargada != std::string("p:") + CLAVES[actual]) {
			cargarPano(actual);
		}
	}
	bool visible() const { return listo && pesoVisible > 0.002f && actual != NEUTRO; }

	// los vec4 del UBO (ver UniformData en yugireload.cpp: ent .. entEfecto, en ese orden). Con fondo propio, la
	// niebla es la del horizonte de la imagen y la luz del tablero toma su tinte (mitad tema, mitad imagen)
	void uniformes(glm::vec4* u) const
	{
		const Estilo& s = estilo(actual);
		const float e = pesoVisible;
		const float suave = e * e * (3.0f - 2.0f * e);
		V3 niebla = s.niebla, luzSol = s.luzSol, relleno = s.relleno;
		if (conFondo()) {
			auto lum = [](V3 c) { return std::max(1e-3f, glm::dot(c, V3(0.2126f, 0.7152f, 0.0722f))); };
			auto tenir = [&](V3 base, V3 t) { return glm::mix(base, t / lum(t) * lum(base), 0.5f); };
			niebla = infoFondo.niebla;
			luzSol = tenir(luzSol, infoFondo.tinte);
			relleno = tenir(relleno, glm::pow(infoFondo.cielo, V3(2.2f)));
		}
		u[0] = V4(e, (float)actual, suave, 0.0f);
		u[1] = V4(s.arriba, s.estrellas);
		u[2] = V4(s.horizonte, s.nubes);
		u[3] = V4(niebla, s.densidad);
		u[4] = V4(glm::normalize(s.solDir), s.solTam);
		u[5] = V4(s.solColor, s.halo);
		u[6] = V4(luzSol, actual != NEUTRO ? 1.0f : 0.0f);
		u[7] = V4(relleno, s.inicio);
		u[8] = V4(s.rebote, s.noche);
		u[9] = V4(s.efecto, s.efectoFuerza);
	}

	void preparar(vks::VulkanDevice* d, VkQueue cola, VkFormat formatoColor, VkFormat formatoProf, VkPipelineCache cache, VkDescriptorSetLayout set0,
	              const std::string& shaders, const std::string& assets, float hx_, float hz_)
	{
		disp = d;
		dev = d->logicalDevice;
		hx = hx_;
		hz = hz_;
		colaCarga = cola;
		dirPano = assets + "entornos/pano/";
		dirFondos = assets + "fondos/";
		tablaFondos = leerTablaFondos(assets + "ui/fondos.txt");
		if (const char* m = getenv("YUGI_PANO_MAX")) panoMax = (uint32_t)std::max(256, atoi(m));
		cargarTexturas(cola, assets + "entornos/");
		// todas las obras en una malla: primero lo opaco de cada entorno, despues sus llamas y halos. Con fondo
		// pre-renderizado no hay obra por codigo: solo las llamas vivas sobre las antorchas del render
		Obra todo;
		std::vector<Obra> adit(NUM_ENTORNOS);
		for (int e = 1; e < NUM_ENTORNOS; e++) {
			Obra o;
			o.hx = hx;
			o.hz = hz;
			infoPano[e] = leerInfoPano(dirPano, e);
			if (infoPano[e].hay) {
				for (const V4& l : infoPano[e].llamas) o.aditivo.push_back({ V3(l), l.x * 0.37f + l.z * 0.11f, V4(1.0f, 0.8f, 0.6f, 2.2f) });
			} else {
				construir(o, e);
			}
			anexar(todo, o, rangos[e].primerIndice, rangos[e].indices);
			Obra& a = adit[e];
			for (size_t i = 0; i < o.aditivo.size(); i++) {
				const auto& l = o.aditivo[i];
				const float esc = infoPano[e].hay ? infoPano[e].llamas[i].w / 1.1f : 1.0f;   // el alto de la llama del render
				a.fuego(l.p, 1.1f * esc, 0.7f * esc, V4(1.0f, 1.0f, 1.0f, l.color.a), l.semilla);
				a.halo(l.p + V3(0, 0.45f * esc, 0), 1.8f * esc, V4(1.0f, 0.55f, 0.2f, 0.45f), l.semilla);
			}
			for (const auto& h : o.halos) a.halo(h.p, h.tam, h.color, h.p.x);
			triangulos[e] = rangos[e].indices / 3;
		}
		for (int e = 1; e < NUM_ENTORNOS; e++) {
			anexar(todo, adit[e], rangos[e].primerAditivo, rangos[e].aditivos);
		}
		const VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, visible, &vertices, todo.g.v.size() * sizeof(bosque::Vertice), todo.g.v.data()));
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, visible, &indices, todo.g.idx.size() * sizeof(uint32_t), todo.g.idx.data()));
		totalVertices = (uint32_t)todo.g.v.size();
		crearPipelines(formatoColor, formatoProf, cache, set0, shaders);
		listo = true;
	}

	~Entornos()
	{
		if (!dev) return;
		vertices.destroy();
		indices.destroy();
		for (VkPipeline p : { pMalla, pAditivo, pCielo, pParticulas }) vkDestroyPipeline(dev, p, nullptr);
		vkDestroyPipelineLayout(dev, layout, nullptr);
		vkDestroyDescriptorPool(dev, pool, nullptr);
		vkDestroyDescriptorSetLayout(dev, layoutTexturas, nullptr);
		vkDestroySampler(dev, muestreador, nullptr);
		vkDestroySampler(dev, muestreoPano, nullptr);
		vkDestroySampler(dev, muestreoProf, nullptr);
		pano.destruir(dev);
		prof.destruir(dev);
		vkDestroyImageView(dev, vistaTexturas, nullptr);
		vkDestroyImage(dev, imagenTexturas, nullptr);
		vkFreeMemory(dev, memoriaTexturas, nullptr);
	}

	// lo solido del entorno y el fondo (despues del tablero y del Bosque: el fondo solo pinta lo que queda)
	void opaco(VkCommandBuffer cmd, VkDescriptorSet set0, const glm::mat4& inversaVP)
	{
		dibujosCuadro = 0;
		if (!visible()) return;
		enlazar(cmd, set0);
		const Rango& r = rangos[actual];
		const bool fondo = conFondo();
		if (r.indices > 0 && !fondo) {   // con fondo propio no hay nada por codigo delante (ni suelo ni siluetas)
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pMalla);
			empujar(cmd, glm::mat4(1.0f), V4(0.0f, 40.0f, 0.0f, 0.0f), V4(0.0f));
			vkCmdDrawIndexed(cmd, r.indices, 1, r.primerIndice, 0, 0);
			dibujosCuadro++;
		}
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pCielo);
		if (fondo) {
			// cilindro de radio 1: u = acimut / ancho, v = (tanArriba - tan(elevacion)) / (tanArriba - tanAbajo)
			const float ancho = glm::radians(infoFondo.ancho);
			const float tAbajo = tanf(glm::radians(infoFondo.abajo));
			const float tArriba = tAbajo + ancho * (float)pano.alto / (float)std::max(1u, pano.ancho);
			empujar(cmd, inversaVP, V4(infoFondo.origen, 2.0f), V4((float)pano.ancho, ancho, tAbajo, tArriba), V4(infoFondo.cielo, 0.0f));
		} else {
			const bool hayPano = cargada == std::string("p:") + CLAVES[actual] && infoPano[actual].hay;
			empujar(cmd, inversaVP, V4(infoPano[actual].origen, hayPano ? 1.0f : 0.0f), V4((float)pano.ancho, 0.0f, 0.0f, 0.0f));
		}
		vkCmdDraw(cmd, 3, 1, 0, 0);
		dibujosCuadro++;
	}

	// llamas, halos y particulas (despues de lo opaco y de los efectos)
	void transparente(VkCommandBuffer cmd, VkDescriptorSet set0)
	{
		if (!visible()) return;
		enlazar(cmd, set0);
		const Rango& r = rangos[actual];
		if (r.aditivos > 0 && !conFondo()) {   // las llamas son de la obra o del pre-renderizado del tema
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pAditivo);
			empujar(cmd, glm::mat4(1.0f), V4(1.0f, 40.0f, 0.0f, 0.0f), V4(0.0f));
			vkCmdDrawIndexed(cmd, r.aditivos, 1, r.primerAditivo, 0, 0);
			dibujosCuadro++;
		}
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pParticulas);
		for (const Particulas& p : estilo(actual).part) {
			if (p.tipo < 0 || p.n == 0) continue;
			struct { V4 modo, color, zona; } e{ V4((float)p.tipo, pesoVisible, hx, hz), p.color, p.zona };
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(e), &e);
			vkCmdDraw(cmd, 6, p.n, 0, 0);
			dibujosCuadro++;
		}
	}

	void informe() const
	{
		fprintf(stderr, "[entornos] %u vertices; texturas: %d capas de %u en %.0f ms\n", totalVertices, (int)NUM_CAPAS, TAM_TEXTURA, msTexturas);
		int propios = 0;
		for (const FilaFondo& f : tablaFondos) propios += existeFondo(f.id) ? 1 : 0;
		fprintf(stderr, "[entornos] fondos propios: %d de %zu en %s\n", propios, tablaFondos.size(), dirFondos.c_str());
		for (int e = 1; e < NUM_ENTORNOS; e++) {
			if (infoPano[e].hay) fprintf(stderr, "[entornos]   %-26s fondo pre-renderizado (%zu llamas)\n", CLAVES[e], infoPano[e].llamas.size());
		}
		for (int e = 1; e < NUM_ENTORNOS; e++) {
			uint32_t part = 0;
			for (const auto& p : estilo(e).part) part += p.tipo >= 0 ? p.n : 0;
			fprintf(stderr, "[entornos]   %-26s %7u triangulos, %5u aditivos, %4u particulas\n", CLAVES[e], rangos[e].indices / 3, rangos[e].aditivos / 3, part);
		}
	}

private:
	vks::VulkanDevice* disp{ nullptr };
	VkDevice dev{ VK_NULL_HANDLE };
	float hx{ 13.44f }, hz{ 10.08f };
	uint32_t totalVertices{ 0 };
	std::array<uint32_t, NUM_ENTORNOS> triangulos{};
	vks::Buffer vertices, indices;
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	VkPipeline pMalla{ VK_NULL_HANDLE }, pAditivo{ VK_NULL_HANDLE }, pCielo{ VK_NULL_HANDLE }, pParticulas{ VK_NULL_HANDLE };
	VkImage imagenTexturas{ VK_NULL_HANDLE };
	VkDeviceMemory memoriaTexturas{ VK_NULL_HANDLE };
	VkImageView vistaTexturas{ VK_NULL_HANDLE };
	VkSampler muestreador{ VK_NULL_HANDLE };
	VkDescriptorPool pool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout layoutTexturas{ VK_NULL_HANDLE };
	VkDescriptorSet setTexturas{ VK_NULL_HANDLE };
	// el fondo pre-renderizado del entorno actual: color (con mipmaps) y distancias
	struct Imagen {
		VkImage img{ VK_NULL_HANDLE };
		VkDeviceMemory mem{ VK_NULL_HANDLE };
		VkImageView vista{ VK_NULL_HANDLE };
		uint32_t ancho{ 0 }, alto{ 0 };
		void destruir(VkDevice d)
		{
			if (vista) vkDestroyImageView(d, vista, nullptr);
			if (img) vkDestroyImage(d, img, nullptr);
			if (mem) vkFreeMemory(d, mem, nullptr);
			*this = Imagen{};
		}
	};
	Imagen pano, prof;
	VkSampler muestreoPano{ VK_NULL_HANDLE }, muestreoProf{ VK_NULL_HANDLE };
	std::array<InfoPano, NUM_ENTORNOS> infoPano{};
	std::string cargada;   // la imagen en la GPU: "p:<clave>" (pre-renderizado), "f:<id>" (fondo propio), "x:<id>" (fallo)
	// fondos por personaje (rama classic-fondos)
	std::string dirFondos, fondoActual, fondoObjetivo;
	std::vector<FilaFondo> tablaFondos;
	InfoFondo infoFondo;

	const FilaFondo* fila(const std::string& id) const
	{
		for (const FilaFondo& f : tablaFondos) {
			if (f.id == id) return &f;
		}
		return nullptr;
	}
	bool existeFondo(const std::string& id) const
	{
		FILE* f = fopen((dirFondos + id + ".txt").c_str(), "r");
		if (f) fclose(f);
		return f != nullptr;
	}
	// el fondo que se vera para <id>: el suyo si ya existe; si no, su respaldo (en cadena); "" = ninguno
	std::string resolverFondo(const std::string& id) const
	{
		std::string x = id;
		for (int i = 0; i < 4 && !x.empty(); i++) {
			if (existeFondo(x)) return x;
			const FilaFondo* f = fila(x);
			x = f ? f->respaldo : std::string();
		}
		return std::string();
	}
	VkQueue colaCarga{ VK_NULL_HANDLE };
	std::string dirPano;
	int baseObjetivo{ NEUTRO };   // lo pedido para el rival mientras manda la carta de campo
	std::string baseFondo;
	template <class F> void conBase(F f)
	{
		if (campo.empty()) {
			f();
			return;
		}
		const int o = objetivo;
		const std::string fo = fondoObjetivo;
		f();
		baseObjetivo = objetivo;
		baseFondo = fondoObjetivo;
		objetivo = o;
		fondoObjetivo = fo;
	}
	uint32_t panoMax{ 8192 };

	// imagen 2D en la GPU desde niveles ya armados en la CPU (cada nivel: bytes contiguos)
	void crearImagen(Imagen& im, VkFormat fmt, uint32_t w, uint32_t h, const std::vector<std::vector<uint8_t>>& niveles)
	{
		const uint32_t mips = (uint32_t)niveles.size();
		std::vector<VkBufferImageCopy> copias;
		size_t ofs = 0;
		for (uint32_t m = 0; m < mips; m++) {
			VkBufferImageCopy r{};
			r.bufferOffset = ofs;
			r.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, m, 0, 1 };
			r.imageExtent = { std::max(1u, w >> m), std::max(1u, h >> m), 1 };
			copias.push_back(r);
			ofs = (ofs + niveles[m].size() + 15) & ~(size_t)15;
		}
		std::vector<uint8_t> datos(ofs);
		for (uint32_t m = 0; m < mips; m++) std::memcpy(&datos[(size_t)copias[m].bufferOffset], niveles[m].data(), niveles[m].size());
		vks::Buffer subida;
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &subida,
		                                   datos.size(), datos.data()));
		VkImageCreateInfo ii = vks::initializers::imageCreateInfo();
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = fmt;
		ii.extent = { w, h, 1 };
		ii.mipLevels = mips;
		ii.arrayLayers = 1;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK_RESULT(vkCreateImage(dev, &ii, nullptr, &im.img));
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, im.img, &req);
		VkMemoryAllocateInfo ma = vks::initializers::memoryAllocateInfo();
		ma.allocationSize = req.size;
		ma.memoryTypeIndex = disp->getMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(dev, &ma, nullptr, &im.mem));
		VK_CHECK_RESULT(vkBindImageMemory(dev, im.img, im.mem, 0));
		VkCommandBuffer cmd = disp->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		const VkImageSubresourceRange todo{ VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, 0, 1 };
		VkImageMemoryBarrier b = vks::initializers::imageMemoryBarrier();
		b.image = im.img;
		b.subresourceRange = todo;
		b.srcAccessMask = 0;
		b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
		vkCmdCopyBufferToImage(cmd, subida.buffer, im.img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)copias.size(), copias.data());
		b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
		disp->flushCommandBuffer(cmd, colaCarga, true);
		subida.destroy();
		VkImageViewCreateInfo vi = vks::initializers::imageViewCreateInfo();
		vi.image = im.img;
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = fmt;
		vi.subresourceRange = todo;
		VK_CHECK_RESULT(vkCreateImageView(dev, &vi, nullptr, &im.vista));
		im.ancho = w;
		im.alto = h;
	}

	void escribirPano()
	{
		VkDescriptorImageInfo a{ muestreoPano, pano.vista, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkDescriptorImageInfo b{ muestreoProf, prof.vista, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet w[2] = { vks::initializers::writeDescriptorSet(setTexturas, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &a),
			                          vks::initializers::writeDescriptorSet(setTexturas, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &b) };
		vkUpdateDescriptorSets(dev, 2, w, 0, nullptr);
	}

	// sin fondo cargado: imagenes de 1x1 (el descriptor siempre apunta a algo valido)
	void panoVacio()
	{
		crearImagen(pano, VK_FORMAT_R8G8B8A8_UNORM, 1, 1, { std::vector<uint8_t>{ 0, 0, 0, 255 } });
		crearImagen(prof, VK_FORMAT_R16_UNORM, 1, 1, { std::vector<uint8_t>{ 255, 255 } });
		escribirPano();
	}

	// el color (jpg) con sus mipmaps (promedio 2x2; los niveles de mas de panoMax de ancho se descartan) y las
	// distancias (png de 16 bits, sin mipmaps)
	void cargarPano(int e)
	{
		cargada = std::string("p:") + CLAVES[e];
		if (!cargarImagenes(dirPano + CLAVES[e])) infoPano[e].hay = false;
	}
	// el fondo propio de un personaje (o de un tema): si falla queda el del tema por codigo. Rama classic-campos: la
	// imagen (jpg de 8192 px + distancias + mipmaps, ~0.5-1 s de CPU) se decodifica en otro hilo; aqui solo se sube
	// cuando esta lista (mientras tanto el entorno sigue fundido y el juego no se traba)
	void cargarFondo(const std::string& id)
	{
		const std::string base = dirFondos + id;
		empezarDecodificar(base);
		if (cargaSincrona) {
			while (!decod->hecho.load()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
			cargaSincrona = false;
		}
		if (!decod->hecho.load()) return;
		infoFondo = leerInfoFondo(base + ".txt");
		cargada = (infoFondo.hay && subir(*decod)) ? "f:" + id : "x:" + id;
		decod.reset();
	}
	bool cargandoFondo() const { return !fondoActual.empty() && cargada != "f:" + fondoActual && cargada != "x:" + fondoActual; }
	struct Decodificado {
		std::string base;
		std::vector<std::vector<uint8_t>> niveles;
		uint32_t ancho{ 0 }, alto{ 0 };
		std::vector<uint8_t> prof;
		uint32_t wp{ 0 }, hp{ 0 };
		bool ok{ false };
		float ms{ 0.0f };
		std::atomic<bool> hecho{ false };
	};
	std::shared_ptr<Decodificado> decod;
	bool cargaSincrona{ false };
	void empezarDecodificar(const std::string& base)
	{
		if (decod && decod->base == base) return;
		auto d = std::make_shared<Decodificado>();
		d->base = base;
		decod = d;
		const uint32_t pm = panoMax;
		std::thread([d, pm] {
			decodificar(*d, pm);
			d->hecho.store(true);
		}).detach();
	}
	bool cargarImagenes(const std::string& base)
	{
		Decodificado d;
		d.base = base;
		decodificar(d, panoMax);
		return subir(d);
	}
	// (en cualquier hilo: solo CPU) el color con sus mipmaps y las distancias
	static void decodificar(Decodificado& r, uint32_t panoMax)
	{
		const auto t0 = std::chrono::steady_clock::now();
		const std::string& base = r.base;
		int w = 0, h = 0, n = 0;
		stbi_uc* px = stbi_load((base + ".jpg").c_str(), &w, &h, &n, 4);
		int wp = 0, hp = 0, np = 0;
		stbi_us* pp = stbi_load_16((base + "_prof.png").c_str(), &wp, &hp, &np, 1);
		if (!px || !pp) {
			if (px) stbi_image_free(px);
			if (pp) stbi_image_free(pp);
			return;
		}
		std::vector<std::vector<uint8_t>>& niveles = r.niveles;
		std::vector<uint8_t> act(px, px + (size_t)w * h * 4);
		stbi_image_free(px);
		uint32_t aw = (uint32_t)w, ah = (uint32_t)h, ancho = 0, alto = 0;
		while (true) {
			if (aw <= panoMax) {
				if (niveles.empty()) {
					ancho = aw;
					alto = ah;
				}
				niveles.push_back(act);
			}
			if (aw == 1 && ah == 1) break;
			const uint32_t nw = std::max(1u, aw / 2), nh = std::max(1u, ah / 2);
			std::vector<uint8_t> sig((size_t)nw * nh * 4);
			for (uint32_t y = 0; y < nh; y++) {
				const uint8_t* f0 = &act[(size_t)std::min(2 * y, ah - 1) * aw * 4];
				const uint8_t* f1 = &act[(size_t)std::min(2 * y + 1, ah - 1) * aw * 4];
				uint8_t* o = &sig[(size_t)y * nw * 4];
				for (uint32_t x = 0; x < nw; x++) {
					const uint32_t x0 = std::min(2 * x, aw - 1) * 4, x1 = std::min(2 * x + 1, aw - 1) * 4;
					for (int k = 0; k < 4; k++) o[x * 4 + k] = (uint8_t)((f0[x0 + k] + f0[x1 + k] + f1[x0 + k] + f1[x1 + k] + 2) >> 2);
				}
			}
			act.swap(sig);
			aw = nw;
			ah = nh;
		}
		r.ancho = ancho;
		r.alto = alto;
		r.prof.resize((size_t)wp * hp * 2);
		std::memcpy(r.prof.data(), pp, r.prof.size());
		stbi_image_free(pp);
		r.wp = (uint32_t)wp;
		r.hp = (uint32_t)hp;
		r.ok = true;
		r.ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}
	// (hilo principal) a la GPU
	bool subir(Decodificado& r)
	{
		if (!r.ok) {
			fprintf(stderr, "[entornos] falta el fondo %s(.jpg|_prof.png): queda el cielo por codigo\n", r.base.c_str());
			return false;
		}
		const auto t0 = std::chrono::steady_clock::now();
		vkQueueWaitIdle(colaCarga);   // el descriptor del fondo anterior puede estar en uso
		pano.destruir(dev);
		prof.destruir(dev);
		crearImagen(pano, VK_FORMAT_R8G8B8A8_UNORM, r.ancho, r.alto, r.niveles);
		crearImagen(prof, VK_FORMAT_R16_UNORM, r.wp, r.hp, { r.prof });
		escribirPano();
		size_t bytes = r.prof.size();
		for (const auto& nv : r.niveles) bytes += nv.size();
		fprintf(stderr, "[entornos] fondo %s: %ux%u, distancias %ux%u, %.1f MB en la GPU, %.0f ms decodificando (otro hilo) + %.0f ms subiendo\n",
		        r.base.c_str(), r.ancho, r.alto, r.wp, r.hp, (float)bytes / (1024.0f * 1024.0f), r.ms,
		        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count());
		r.niveles.clear();
		r.prof.clear();
		return true;
	}

	static void anexar(Obra& todo, const Obra& o, uint32_t& primero, uint32_t& cuantos)
	{
		const uint32_t base = (uint32_t)todo.g.v.size();
		primero = (uint32_t)todo.g.idx.size();
		todo.g.v.insert(todo.g.v.end(), o.g.v.begin(), o.g.v.end());
		for (uint32_t i : o.g.idx) todo.g.idx.push_back(base + i);
		cuantos = (uint32_t)o.g.idx.size();
	}

	void empujar(VkCommandBuffer cmd, const glm::mat4& m, V4 a, V4 b, V4 c = V4(0.0f))
	{
		struct { glm::mat4 m; V4 a, b, c; } e{ m, a, b, c };
		// las mallas leen solo el primer vec4 (modo): va primero
		if (m == glm::mat4(1.0f)) {
			struct { V4 a, b, c; } f{ a, b, V4(0.0f) };
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(f), &f);
			return;
		}
		vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(e), &e);
	}

	void enlazar(VkCommandBuffer cmd, VkDescriptorSet set0)
	{
		const VkDescriptorSet sets[2] = { set0, setTexturas };
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets, 0, nullptr);
		const VkDeviceSize o = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &vertices.buffer, &o);
		vkCmdBindIndexBuffer(cmd, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	// las capas (png RGBA de entornos_insumos.py) en un arreglo 2D con mipmaps (promedio 2x2; normales
	// renormalizadas). Si falta una capa queda neutra
	void cargarTexturas(VkQueue cola, const std::string& dir)
	{
		const auto t0 = std::chrono::steady_clock::now();
		const uint32_t T = TAM_TEXTURA;
		uint32_t mips = 1;
		while ((T >> mips) > 0) mips++;
		std::vector<VkDeviceSize> tamMip(mips);
		VkDeviceSize porCapa = 0;
		for (uint32_t m = 0; m < mips; m++) {
			const uint32_t s = std::max(1u, T >> m);
			tamMip[m] = (VkDeviceSize)s * s * 4;
			porCapa += tamMip[m];
		}
		std::vector<uint8_t> datos((size_t)(porCapa * NUM_CAPAS));
		std::vector<VkBufferImageCopy> copias;
		int faltan = 0;
		for (uint32_t c = 0; c < NUM_CAPAS; c++) {
			const bool normal = c % 2 == 1;
			const std::string ruta = dir + NOMBRE_CAPA[c / 2] + (normal ? "_normal.png" : "_color.png");
			std::vector<uint8_t> act((size_t)T * T * 4);
			int w = 0, h = 0, n = 0;
			stbi_uc* px = stbi_load(ruta.c_str(), &w, &h, &n, 4);
			if (px) {
				for (uint32_t y = 0; y < T; y++) {
					for (uint32_t x = 0; x < T; x++) {
						std::memcpy(&act[((size_t)y * T + x) * 4], &px[(((size_t)(y * (uint32_t)h / T)) * (uint32_t)w + x * (uint32_t)w / T) * 4], 4);
					}
				}
				stbi_image_free(px);
			} else {
				faltan++;
				const uint8_t neutro[4] = { (uint8_t)(normal ? 128 : 150), (uint8_t)(normal ? 128 : 145), (uint8_t)(normal ? 255 : 140), (uint8_t)(normal ? 200 : 255) };
				for (size_t i = 0; i < (size_t)T * T; i++) std::memcpy(&act[i * 4], neutro, 4);
			}
			VkDeviceSize ofs = porCapa * c;
			for (uint32_t m = 0; m < mips; m++) {
				const uint32_t s = std::max(1u, T >> m);
				if (m > 0) {
					const uint32_t sp = s * 2;
					std::vector<uint8_t> sig((size_t)s * s * 4);
					for (uint32_t y = 0; y < s; y++) {
						for (uint32_t x = 0; x < s; x++) {
							float acc[4] = { 0, 0, 0, 0 };
							for (uint32_t dy = 0; dy < 2; dy++) {
								for (uint32_t dx = 0; dx < 2; dx++) {
									const uint8_t* q = &act[(((size_t)(2 * y + dy)) * sp + 2 * x + dx) * 4];
									for (int k = 0; k < 4; k++) acc[k] += q[k];
								}
							}
							uint8_t* o = &sig[((size_t)y * s + x) * 4];
							if (normal) {
								glm::vec3 nv(acc[0] / 510.0f - 1.0f, acc[1] / 510.0f - 1.0f, acc[2] / 510.0f - 1.0f);
								nv = glm::length(nv) > 1e-4f ? glm::normalize(nv) : glm::vec3(0, 0, 1);
								o[0] = (uint8_t)(nv.x * 127.5f + 127.5f);
								o[1] = (uint8_t)(nv.y * 127.5f + 127.5f);
								o[2] = (uint8_t)(nv.z * 127.5f + 127.5f);
								o[3] = (uint8_t)(acc[3] * 0.25f + 0.5f);
							} else {
								for (int k = 0; k < 4; k++) o[k] = (uint8_t)(acc[k] * 0.25f + 0.5f);
							}
						}
					}
					act.swap(sig);
				}
				std::memcpy(&datos[(size_t)ofs], act.data(), act.size());
				VkBufferImageCopy r{};
				r.bufferOffset = ofs;
				r.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, m, c, 1 };
				r.imageExtent = { s, s, 1 };
				copias.push_back(r);
				ofs += tamMip[m];
			}
		}
		if (faltan > 0) fprintf(stderr, "[entornos] faltan %d capas de textura en %s (correr entornos_insumos.py): quedan neutras\n", faltan, dir.c_str());
		vks::Buffer subida;
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &subida,
		                                   datos.size(), datos.data()));
		VkImageCreateInfo ii = vks::initializers::imageCreateInfo();
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = VK_FORMAT_R8G8B8A8_UNORM;
		ii.extent = { T, T, 1 };
		ii.mipLevels = mips;
		ii.arrayLayers = NUM_CAPAS;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		VK_CHECK_RESULT(vkCreateImage(dev, &ii, nullptr, &imagenTexturas));
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, imagenTexturas, &req);
		VkMemoryAllocateInfo ma = vks::initializers::memoryAllocateInfo();
		ma.allocationSize = req.size;
		ma.memoryTypeIndex = disp->getMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(dev, &ma, nullptr, &memoriaTexturas));
		VK_CHECK_RESULT(vkBindImageMemory(dev, imagenTexturas, memoriaTexturas, 0));
		VkCommandBuffer cmd = disp->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		const VkImageSubresourceRange todo{ VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, 0, NUM_CAPAS };
		VkImageMemoryBarrier b = vks::initializers::imageMemoryBarrier();
		b.image = imagenTexturas;
		b.subresourceRange = todo;
		b.srcAccessMask = 0;
		b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
		vkCmdCopyBufferToImage(cmd, subida.buffer, imagenTexturas, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)copias.size(), copias.data());
		b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
		disp->flushCommandBuffer(cmd, cola, true);
		subida.destroy();

		VkImageViewCreateInfo vi = vks::initializers::imageViewCreateInfo();
		vi.image = imagenTexturas;
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		vi.format = VK_FORMAT_R8G8B8A8_UNORM;
		vi.subresourceRange = todo;
		VK_CHECK_RESULT(vkCreateImageView(dev, &vi, nullptr, &vistaTexturas));
		VkSamplerCreateInfo si = vks::initializers::samplerCreateInfo();
		si.magFilter = si.minFilter = VK_FILTER_LINEAR;
		si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		si.maxLod = (float)mips;
		si.anisotropyEnable = disp->enabledFeatures.samplerAnisotropy;
		si.maxAnisotropy = disp->enabledFeatures.samplerAnisotropy ? 8.0f : 1.0f;
		VK_CHECK_RESULT(vkCreateSampler(dev, &si, nullptr, &muestreador));
		// el fondo pre-renderizado: u da la vuelta, v se detiene en los polos; distancias sin filtrar
		si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.maxLod = 16.0f;
		VK_CHECK_RESULT(vkCreateSampler(dev, &si, nullptr, &muestreoPano));
		si.magFilter = si.minFilter = VK_FILTER_NEAREST;
		si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		si.anisotropyEnable = VK_FALSE;
		si.maxAnisotropy = 1.0f;
		si.maxLod = 0.0f;
		VK_CHECK_RESULT(vkCreateSampler(dev, &si, nullptr, &muestreoProf));
		std::vector<VkDescriptorPoolSize> tam = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3) };
		VkDescriptorPoolCreateInfo pi = vks::initializers::descriptorPoolCreateInfo(tam, 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(dev, &pi, nullptr, &pool));
		const std::vector<VkDescriptorSetLayoutBinding> enlaces = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		};
		VkDescriptorSetLayoutCreateInfo li = vks::initializers::descriptorSetLayoutCreateInfo(enlaces);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev, &li, nullptr, &layoutTexturas));
		VkDescriptorSetAllocateInfo ai = vks::initializers::descriptorSetAllocateInfo(pool, &layoutTexturas, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(dev, &ai, &setTexturas));
		VkDescriptorImageInfo img{ muestreador, vistaTexturas, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet w = vks::initializers::writeDescriptorSet(setTexturas, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &img);
		vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
		panoVacio();
		msTexturas = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}

	VkPipelineShaderStageCreateInfo etapa(const std::string& ruta, VkShaderStageFlagBits s, std::vector<VkShaderModule>& modulos)
	{
		VkShaderModule m = vks::tools::loadShader(ruta.c_str(), dev);
		modulos.push_back(m);
		return { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, s, m, "main", nullptr };
	}

	void crearPipelines(VkFormat formatoColor, VkFormat formatoProf, VkPipelineCache cache, VkDescriptorSetLayout set0, const std::string& shaders)
	{
		VkPushConstantRange empuje = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4) + 3 * sizeof(glm::vec4), 0);
		const VkDescriptorSetLayout sets[2] = { set0, layoutTexturas };
		VkPipelineLayoutCreateInfo pl = vks::initializers::pipelineLayoutCreateInfo(sets, 2);
		pl.pushConstantRangeCount = 1;
		pl.pPushConstantRanges = &empuje;
		VK_CHECK_RESULT(vkCreatePipelineLayout(dev, &pl, nullptr, &layout));

		const VkVertexInputBindingDescription enlace{ 0, sizeof(bosque::Vertice), VK_VERTEX_INPUT_RATE_VERTEX };
		const std::array<VkVertexInputAttributeDescription, 4> atributos = { {
			{ 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(bosque::Vertice, pos) },
			{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(bosque::Vertice, normal) },
			{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(bosque::Vertice, color) },
			{ 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(bosque::Vertice, uv) },
		} };
		VkPipelineVertexInputStateCreateInfo entradaMalla = vks::initializers::pipelineVertexInputStateCreateInfo();
		entradaMalla.vertexBindingDescriptionCount = 1;
		entradaMalla.pVertexBindingDescriptions = &enlace;
		entradaMalla.vertexAttributeDescriptionCount = (uint32_t)atributos.size();
		entradaMalla.pVertexAttributeDescriptions = atributos.data();
		VkPipelineVertexInputStateCreateInfo sinVertices = vks::initializers::pipelineVertexInputStateCreateInfo();

		VkPipelineInputAssemblyStateCreateInfo ensamble = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo raster = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState opaco = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo mezclaOpaca = vks::initializers::pipelineColorBlendStateCreateInfo(1, &opaco);
		VkPipelineColorBlendAttachmentState premult = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_TRUE);
		premult.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		premult.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		premult.colorBlendOp = VK_BLEND_OP_ADD;
		premult.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		premult.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		premult.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo mezclaPremult = vks::initializers::pipelineColorBlendStateCreateInfo(1, &premult);
		VkPipelineColorBlendAttachmentState alfa = premult;
		alfa.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		VkPipelineColorBlendStateCreateInfo mezclaAlfa = vks::initializers::pipelineColorBlendStateCreateInfo(1, &alfa);
		VkPipelineDepthStencilStateCreateInfo prof = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineDepthStencilStateCreateInfo profSinEscribir = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo vista = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo muestras = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dinamicos = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dinamico = vks::initializers::pipelineDynamicStateCreateInfo(dinamicos);
		std::vector<VkShaderModule> modulos;
		const std::string dir = shaders + "yugireload/";
		const VkPipelineShaderStageCreateInfo vMalla = etapa(dir + "entornos.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos);
		const VkPipelineShaderStageCreateInfo fMalla = etapa(dir + "entornos.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos);
		const VkPipelineShaderStageCreateInfo vCielo = etapa(dir + "entornos_cielo.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos);
		const VkPipelineShaderStageCreateInfo fCielo = etapa(dir + "entornos_cielo.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos);
		const VkPipelineShaderStageCreateInfo vPart = etapa(dir + "entornos_particula.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos);
		const VkPipelineShaderStageCreateInfo fPart = etapa(dir + "entornos_particula.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos);

		VkPipelineRenderingCreateInfo render{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &formatoColor,
			.depthAttachmentFormat = formatoProf,
			.stencilAttachmentFormat = formatoProf,
		};
		std::array<VkPipelineShaderStageCreateInfo, 2> etapas = { vMalla, fMalla };
		VkGraphicsPipelineCreateInfo ci{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &render,
			.stageCount = 2,
			.pStages = etapas.data(),
			.pVertexInputState = &entradaMalla,
			.pInputAssemblyState = &ensamble,
			.pViewportState = &vista,
			.pRasterizationState = &raster,
			.pMultisampleState = &muestras,
			.pDepthStencilState = &prof,
			.pColorBlendState = &mezclaOpaca,
			.pDynamicState = &dinamico,
			.layout = layout,
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pMalla));
		// llamas y halos: suman luz (alfa premultiplicado con alfa 0), sin escribir profundidad
		ci.pDepthStencilState = &profSinEscribir;
		ci.pColorBlendState = &mezclaPremult;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pAditivo));
		// fondo: en la profundidad maxima, mezclado con alfa = peso
		etapas = { vCielo, fCielo };
		ci.pVertexInputState = &sinVertices;
		ci.pColorBlendState = &mezclaAlfa;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pCielo));
		// particulas
		etapas = { vPart, fPart };
		ci.pColorBlendState = &mezclaPremult;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pParticulas));
		for (VkShaderModule m : modulos) vkDestroyShaderModule(dev, m, nullptr);
	}
};

}   // namespace entornos
