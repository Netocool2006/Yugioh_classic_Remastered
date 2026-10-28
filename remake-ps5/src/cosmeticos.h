/*
 * Yugioh Classic Remaster (rama classic-extras): cosmeticos y Personalizacion. SOLO logica y datos (sin Vulkan):
 *  - FUNDAS (dorso de las cartas del jugador): la clasica (assets/ui/dorso.png) y 8 tematicas hechas POR CODIGO al
 *    arrancar (generarFunda): fondo con su dibujo, marco y un medallon con el arte ya remasterizado de una carta
 *    (assets/ui/cara_<id>.png, solo la zona del arte) o un emblema dibujado (Ojo del Milenio, Oricalco). Ninguna usa
 *    imagenes de Konami. Cada una se desbloquea al vencer en la historia a su personaje (Kaiba: Kaiba Corp...).
 *  - TAPETES: el automatico (el del rival, por defecto) o 6 fijos que reusan los materiales de campos.h (capa sobre el
 *    tapete con cobertura completa). La carta de campo activa sigue mandando.
 *  - CAMARA: la elegida en el duelo (L1/R1) se recuerda.
 * Se guardan en opciones.txt (funda=<clave>, tapete=<clave>, camara=N) sin tocar las demas lineas (guardarAtomico).
 * Lo lleva el juego (cosmeticos_juego.h) y lo dibuja la interfaz (cosmeticos_ui.h). Textos ES/EN con idioma::t.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "guardado.h"
#include "idioma.h"
#include "stb_image.h"   // la implementacion ya esta en la base (VulkanglTFModel.cpp)

namespace cosmeticos {

// ------------------------------------------------------------------------------------------------ fundas
enum { F_CLASICA = 0, F_KAIBA, F_MAGO, F_OJO, F_EXODIA, F_OJOS_ROJOS, F_ARPIAS, F_DIOSES, F_ORICALCO, NUM_FUNDAS };
struct Funda { const char *clave, *es, *en, *personaje; };   // personaje: el que la desbloquea ("" = siempre)
static const Funda FUNDAS[NUM_FUNDAS] = {
	{ "clasica", "Clásica", "Classic", "" },
	{ "kaiba_corp", "Kaiba Corp", "Kaiba Corp", "kaiba" },
	{ "mago_oscuro", "Mago Oscuro", "Dark Magician", "arkana" },
	{ "ojo_milenio", "Ojo del Milenio", "Millennium Eye", "pegasus" },
	{ "exodia", "Exodia", "Exodia", "solomon" },
	{ "ojos_rojos", "Ojos Rojos", "Red-Eyes", "joey" },
	{ "arpias", "Arpías", "Harpies", "mai" },
	{ "dioses", "Dioses Egipcios", "Egyptian Gods", "marik" },
	{ "oricalco", "Oricalco", "Orichalcos", "dartz" },
};

// ------------------------------------------------------------------------------------------------ tapetes
enum { T_AUTO = 0, NUM_TAPETES = 7 };
struct Tapete { const char *clave, *es, *en, *campo, *personaje; float r, g, b; };   // campo: carta de campos.h; rgb: muestra
static const Tapete TAPETES[NUM_TAPETES] = {
	{ "auto", "Automático (el del rival)", "Automatic (opponent's)", "", "", 0.55f, 0.62f, 0.75f },
	{ "pradera", "Pradera", "Grassland", "sogen", "rex_raptor", 0.45f, 0.75f, 0.25f },
	{ "mar", "Mar", "Sea", "umi", "mako", 0.20f, 0.50f, 0.95f },
	{ "sombras", "Reino de las Sombras", "Shadow Realm", "yami", "yami_bakura", 0.55f, 0.20f, 0.85f },
	{ "necrovalle", "Necrovalle", "Necrovalley", "necrovalley", "ishizu", 0.90f, 0.70f, 0.35f },
	{ "santuario", "Santuario en el Cielo", "Sanctuary in the Sky", "the_sanctuary_in_the_sky", "rafael", 0.95f, 0.90f, 0.70f },
	{ "castillo", "Castillo Dorado", "Golden Castle", "golden_castle_of_stromberg", "zigfried", 1.0f, 0.75f, 0.25f },
};

// ------------------------------------------------------------------------------------------------ estado
struct Estado {
	int funda{ F_CLASICA }, tapete{ T_AUTO }, camara{ -1 };   // camara -1: nunca se eligio
	std::string ruta;                                        // opciones.txt
	bool fundaLibre[NUM_FUNDAS]{ true }, tapeteLibre[NUM_TAPETES]{ true };
	std::string fundaPorEs[NUM_FUNDAS], fundaPorEn[NUM_FUNDAS], tapetePorEs[NUM_TAPETES], tapetePorEn[NUM_TAPETES];   // "Vence a X"
};
inline Estado& estado()
{
	static Estado e;
	return e;
}
// lo que se ve: la elegida si esta desbloqueada (si no, la clasica / el automatico)
inline int fundaVisible()
{
	const Estado& e = estado();
	return e.funda > 0 && e.funda < NUM_FUNDAS && e.fundaLibre[e.funda] ? e.funda : F_CLASICA;
}
inline int tapeteVisible()
{
	const Estado& e = estado();
	return e.tapete > 0 && e.tapete < NUM_TAPETES && e.tapeteLibre[e.tapete] ? e.tapete : T_AUTO;
}
inline const char* nombreFunda(int k) { return idioma::t(FUNDAS[k].es, FUNDAS[k].en); }
inline const char* nombreTapete(int k) { return idioma::t(TAPETES[k].es, TAPETES[k].en); }

// opciones.txt: lee funda=, tapete= y camara= (las demas claves son de menu_principal.h)
inline void leer(const std::string& ruta)
{
	Estado& e = estado();
	e.ruta = ruta;
	std::ifstream f(ruta);
	std::string l;
	while (std::getline(f, l)) {
		for (char& c : l)
			if (c == '=') c = ' ';
		std::istringstream ss(l);
		std::string clave, valor;
		if (!(ss >> clave >> valor)) continue;
		if (clave == "funda")
			for (int k = 0; k < NUM_FUNDAS; k++)
				if (valor == FUNDAS[k].clave) e.funda = k;
		if (clave == "tapete")
			for (int k = 0; k < NUM_TAPETES; k++)
				if (valor == TAPETES[k].clave) e.tapete = k;
		if (clave == "camara") e.camara = std::max(-1, std::min(9, atoi(valor.c_str())));
	}
	fprintf(stderr, "[cosmeticos] funda %s, tapete %s, camara %d\n", FUNDAS[e.funda].clave, TAPETES[e.tapete].clave, e.camara);
}

// guarda las tres claves conservando las demas lineas del archivo (guardado atomico)
inline void guardar()
{
	const Estado& e = estado();
	if (e.ruta.empty()) return;
	std::string s;
	{
		std::ifstream f(e.ruta);
		std::string l;
		while (std::getline(f, l)) {
			const std::string clave = l.substr(0, l.find_first_of("= \t"));
			if (clave.empty() || clave == "funda" || clave == "tapete" || clave == "camara") continue;
			guardado::agregar(s, "%s\n", l.c_str());
		}
	}
	guardado::agregar(s, "funda=%s\ntapete=%s\n", FUNDAS[e.funda].clave, TAPETES[e.tapete].clave);
	if (e.camara >= 0) guardado::agregar(s, "camara=%d\n", e.camara);
	guardado::guardarAtomico(e.ruta, s);
}

// ------------------------------------------------------------------------------------------------ controles
// el mapa del DualSense por pantalla (cosmeticos_ui.h lo dibuja; el boton se resalta en el dibujo del mando)
enum BotonMando { BM_CRUZ = 0, BM_STICK_I, BM_STICK_D, BM_X, BM_O, BM_T, BM_C, BM_L1, BM_R1, BM_L2, BM_R2, BM_L3, BM_R3, BM_OPTIONS, BM_TACTIL, NUM_BM };
struct Control { int boton; const char* icono; const char *es, *en; };
enum { PEST_DUELO = 0, PEST_CONSTRUCTOR, PEST_MENUS, NUM_PESTANAS };
inline const char* nombrePestana(int p)
{
	switch (p) {
	case PEST_DUELO: return idioma::t("DUELO", "DUEL");
	case PEST_CONSTRUCTOR: return idioma::t("CONSTRUCTOR", "DECK BUILDER");
	default: return idioma::t("MENÚS", "MENUS");
	}
}
inline const std::vector<Control>& controles(int pestana)
{
	static const std::vector<Control> duelo = {
		{ BM_CRUZ, "CRUZ", "Mover el foco por la mano y la mesa", "Move the focus around your hand and the table" },
		{ BM_X, "X", "Acciones de la carta (invocar, colocar, activar, atacar) y confirmar", "Card actions (summon, set, activate, attack) and confirm" },
		{ BM_O, "O", "Atrás / cancelar; en una pregunta, no activar", "Back / cancel; when asked, don't activate" },
		{ BM_T, "T", "Ver la carta en grande con su texto", "View the card enlarged with its text" },
		{ BM_C, "C", "Pasar a la fase siguiente / terminar el turno", "Next phase / end turn" },
		{ BM_L1, "L1", "Cámara anterior (se recuerda para el próximo duelo)", "Previous camera (remembered for the next duel)" },
		{ BM_R1, "R1", "Cámara siguiente: cine, tablero, cenital, lateral, libre", "Next camera: cinema, board, top-down, side, free" },
		{ BM_L2, "L2", "Alejar la cámara", "Zoom the camera out" },
		{ BM_R2, "R2", "Acercar; mantener acelera las animaciones, un toque las salta", "Zoom in; hold to speed up animations, tap to skip" },
		{ BM_STICK_D, "STICK_R", "Girar la cámara (modo libre)", "Orbit the camera (free mode)" },
		{ BM_R3, "R3", "Recentrar la cámara", "Recenter the camera" },
		{ BM_L3, "L3", "Registro del duelo", "Duel log" },
		{ BM_OPTIONS, "OPTIONS", "Pausa: continuar, opciones, construir mazo, abandonar", "Pause: continue, options, build deck, forfeit" },
	};
	static const std::vector<Control> cons = {
		{ BM_CRUZ, "CRUZ", "Moverse por el mazo y la colección", "Move through the deck and the collection" },
		{ BM_X, "X", "Agregar una copia (las fusiones van al extra)", "Add a copy (Fusions go to the Extra Deck)" },
		{ BM_C, "C", "Quitar una copia / usar el mazo en los duelos", "Remove a copy / use the deck in duels" },
		{ BM_T, "T", "Búsqueda y filtros", "Search and filters" },
		{ BM_O, "O", "Volver (pregunta si hay cambios sin guardar)", "Back (asks if there are unsaved changes)" },
		{ BM_L1, "L1", "Página anterior de la colección", "Previous collection page" },
		{ BM_R1, "R1", "Página siguiente de la colección", "Next collection page" },
		{ BM_L2, "L2", "Clase rápida anterior (Todas, Normal, Efecto...)", "Previous quick class (All, Normal, Effect...)" },
		{ BM_R2, "R2", "Clase rápida siguiente", "Next quick class" },
		{ BM_L3, "L3", "Quitar los filtros", "Clear the filters" },
		{ BM_STICK_D, "STICK_R", "Izq./der.: saltar de lado; arr./ab.: desplazar el texto", "Left/right: jump sides; up/down: scroll the text" },
		{ BM_OPTIONS, "OPTIONS", "Guardar el mazo (si es válido)", "Save the deck (if valid)" },
	};
	static const std::vector<Control> menus = {
		{ BM_CRUZ, "CRUZ", "Elegir opción; izq./der. cambian el valor", "Choose an option; left/right change the value" },
		{ BM_STICK_I, "STICK_L", "Igual que la cruz", "Same as the D-pad" },
		{ BM_X, "X", "Confirmar / abrir", "Confirm / open" },
		{ BM_O, "O", "Volver", "Back" },
		{ BM_T, "T", "Elegir rival: cambiar entre Historia y Duelo libre", "Choose opponent: switch between Story and Free Duel" },
		{ BM_R1, "R1", "Elegir rival: reglas del duelo", "Choose opponent: duel rules" },
		{ BM_L1, "L1", "Elegir rival: Modo Desafío (al terminar la historia)", "Choose opponent: Challenge Mode (after the story)" },
		{ BM_OPTIONS, "OPTIONS", "En el duelo: pausa (con Opciones)", "In a duel: pause (with Options)" },
		{ BM_TACTIL, "TÁCTIL", "Construir mazo (fuera del duelo)", "Build deck (outside the duel)" },
	};
	return pestana == PEST_DUELO ? duelo : pestana == PEST_CONSTRUCTOR ? cons : menus;
}

// ------------------------------------------------------------------------------------------------ la pantalla
// Personalizacion (dentro de Opciones): la lleva cosmeticos_juego.h y la dibuja cosmeticos_ui.h
enum Pantalla { P_CERRADA = 0, P_MENU, P_FUNDA, P_TAPETE, P_CONTROLES, P_ESTADISTICAS };
enum OpcionPerso { PERSO_FUNDA = 0, PERSO_TAPETE, PERSO_CONTROLES, PERSO_ESTADISTICAS, PERSO_VOLVER, NUM_PERSO };
struct VistaPerso {
	int pantalla{ P_CERRADA };
	int opcion{ 0 };      // P_MENU
	int sel{ 0 };         // P_FUNDA / P_TAPETE: la que mira el cursor; P_CONTROLES: la fila
	int pestana{ 0 };     // P_CONTROLES
	int desplazar{ 0 };   // P_ESTADISTICAS: primera fila de la tabla por rival
	float t{ 0.0f };      // s desde que cambio la pantalla
	std::string aviso;    // "Vence a X para desbloquear" (un momento, al intentar equipar una bloqueada)
	float tAviso{ 99.0f };
};
inline VistaPerso& vista()
{
	static VistaPerso v;
	return v;
}
inline const char* textoOpcionPerso(int o)
{
	switch (o) {
	case PERSO_FUNDA: return idioma::t("Funda", "Card sleeve");
	case PERSO_TAPETE: return idioma::t("Tapete", "Duel field");
	case PERSO_CONTROLES: return idioma::t("Controles", "Controls");
	case PERSO_ESTADISTICAS: return idioma::t("Estadísticas", "Statistics");
	default: return idioma::t("Volver", "Back");
	}
}
inline std::string venceA(const std::string& es, const std::string& en)
{
	return idioma::ingles() ? "Defeat " + en + " in Story mode to unlock" : "Vence a " + es + " en la historia para desbloquear";
}

// ================================================================================================ fundas por codigo
namespace gen {

struct C3 {
	float r, g, b;
	C3 operator+(C3 o) const { return { r + o.r, g + o.g, b + o.b }; }
	C3 operator*(float k) const { return { r * k, g * k, b * k }; }
	C3 operator*(C3 o) const { return { r * o.r, g * o.g, b * o.b }; }
};
inline C3 mezclar(C3 a, C3 b, float t) { return a * (1.0f - t) + b * t; }
inline float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
inline float suave(float a, float b, float x)
{
	const float t = clamp01((x - a) / (b - a));
	return t * t * (3.0f - 2.0f * t);
}
inline float hash(int x, int y)
{
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return (float)((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
}
inline float ruido(float x, float y)
{
	const int ix = (int)floorf(x), iy = (int)floorf(y);
	const float fx = x - (float)ix, fy = y - (float)iy;
	const float ux = fx * fx * (3.0f - 2.0f * fx), uy = fy * fy * (3.0f - 2.0f * fy);
	const float a = hash(ix, iy), b = hash(ix + 1, iy), c = hash(ix, iy + 1), d = hash(ix + 1, iy + 1);
	return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}
inline float fbm(float x, float y)
{
	float v = 0.0f, a = 0.5f;
	for (int i = 0; i < 4; i++) {
		v += a * ruido(x, y);
		x = x * 2.03f + 7.1f;
		y = y * 2.03f + 3.7f;
		a *= 0.5f;
	}
	return v;
}
// distancia de p al segmento ab
inline float dSeg(float px, float py, float ax, float ay, float bx, float by)
{
	const float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
	const float h = clamp01((pax * bax + pay * bay) / std::max(1e-6f, bax * bax + bay * bay));
	const float dx = pax - bax * h, dy = pay - bay * h;
	return sqrtf(dx * dx + dy * dy);
}
// distancia al contorno de un poligono regular de n lados (radio r, giro a0) como lineas
inline float dPoligono(float px, float py, int n, float r, float a0, int salto = 1)
{
	float d = 1e9f;
	for (int i = 0; i < n; i++) {
		const float a = a0 + 6.2831853f * (float)i / (float)n, b = a0 + 6.2831853f * (float)((i + salto) % n) / (float)n;
		d = std::min(d, dSeg(px, py, r * cosf(a), r * sinf(a), r * cosf(b), r * sinf(b)));
	}
	return d;
}
inline float linea(float d, float ancho) { return 1.0f - suave(ancho * 0.5f, ancho * 0.5f + 1.5f, d); }
inline float halo(float d, float radio) { return expf(-d / radio); }

struct Img {
	int w{ 0 }, h{ 0 };
	std::vector<uint8_t> px;
	bool ok() const { return w > 0 && h > 0; }
	C3 at(float u, float v) const   // bilineal, u/v en px
	{
		u = std::max(0.0f, std::min((float)w - 1.001f, u));
		v = std::max(0.0f, std::min((float)h - 1.001f, v));
		const int x = (int)u, y = (int)v;
		const float fx = u - (float)x, fy = v - (float)y;
		auto p = [&](int xx, int yy) {
			const uint8_t* q = &px[((size_t)yy * w + xx) * 4];
			return C3{ q[0] / 255.0f, q[1] / 255.0f, q[2] / 255.0f };
		};
		return mezclar(mezclar(p(x, y), p(x + 1, y), fx), mezclar(p(x, y + 1), p(x + 1, y + 1), fx), fy);
	}
};
inline Img cargar(const std::string& ruta)
{
	Img r;
	int n = 0;
	stbi_uc* d = stbi_load(ruta.c_str(), &r.w, &r.h, &n, 4);
	if (!d) {
		r.w = r.h = 0;
		fprintf(stderr, "[cosmeticos] falta %s (el medallon va sin arte)\n", ruta.c_str());
		return r;
	}
	r.px.assign(d, d + (size_t)r.w * r.h * 4);
	stbi_image_free(d);
	return r;
}
// el arte de una carta remasterizada (cara_<id>.png: la zona del arte, sin marco ni textos) en un disco: q en -1..1
inline C3 arte(const Img& im, float qx, float qy, float zoom = 0.86f)
{
	const float cx = 0.5f * (float)im.w, cy = 0.455f * (float)im.h, med = 0.40f * (float)im.w * zoom;
	return im.at(cx + qx * med, cy + qy * med);
}

// el Ojo del Milenio: piramide con el ojo de Horus (q en px desde el centro; e: escala)
inline float ojoMilenio(float qx, float qy, float e)
{
	float v = 0.0f;
	// triangulo (piramide)
	const float ax = 0.0f, ay = -95.0f * e, bx = -100.0f * e, by = 75.0f * e, cx = 100.0f * e, cy = 75.0f * e;
	const float dt = std::min({ dSeg(qx, qy, ax, ay, bx, by), dSeg(qx, qy, bx, by, cx, cy), dSeg(qx, qy, cx, cy, ax, ay) });
	v = std::max(v, linea(dt, 7.0f * e));
	// ojo: almendra (dos arcos), iris y pupila
	const float oy = 18.0f * e;
	const float r = 62.0f * e, sep = 40.0f * e;
	const float d1 = sqrtf(qx * qx + (qy - oy + sep) * (qy - oy + sep)), d2 = sqrtf(qx * qx + (qy - oy - sep) * (qy - oy - sep));
	const bool dentroAlm = d1 < r && d2 < r;
	const float borde = std::min(fabsf(d1 - r) + (d2 < r ? 0.0f : 99.0f), fabsf(d2 - r) + (d1 < r ? 0.0f : 99.0f));
	v = std::max(v, linea(borde, 5.0f * e));
	const float di = sqrtf(qx * qx + (qy - oy) * (qy - oy));
	if (dentroAlm) v = std::max(v, linea(fabsf(di - 18.0f * e), 5.0f * e));
	if (di < 9.0f * e) v = 1.0f;
	// lagrima y espiral de Horus
	v = std::max(v, linea(dSeg(qx, qy, -6.0f * e, oy + 26.0f * e, -14.0f * e, oy + 50.0f * e), 5.0f * e));
	v = std::max(v, linea(dSeg(qx, qy, 30.0f * e, oy + 18.0f * e, 52.0f * e, oy + 46.0f * e), 5.0f * e));
	return v;
}

// el sello de Oricalco: circulo doble, hexagrama y runas (q en px desde el centro)
inline float sello(float qx, float qy, float R)
{
	const float d = sqrtf(qx * qx + qy * qy);
	float v = linea(fabsf(d - R), 4.0f) + linea(fabsf(d - R * 0.88f), 2.5f) * 0.8f;
	v = std::max(v, linea(dPoligono(qx, qy, 6, R * 0.88f, -1.5707963f, 2), 4.0f));
	v = std::max(v, linea(fabsf(d - R * 0.36f), 3.0f));
	// runas: marcas cortas entre los dos circulos
	if (d > R * 0.88f && d < R) {
		const float a = atan2f(qy, qx) * 36.0f / 6.2831853f;
		const float f = a - floorf(a);
		if (hash((int)floorf(a), 7) > 0.35f && f > 0.3f && f < 0.7f) v = std::max(v, 0.75f);
	}
	return std::min(v, 1.0f);
}

}   // namespace gen

// genera la funda k (1..NUM_FUNDAS-1) de w x h RGBA en out. ui: carpeta assets/ui/ (con "/")
inline void generarFunda(int k, const std::string& ui, int w, int h, std::vector<uint8_t>& out)
{
	using namespace gen;
	out.assign((size_t)w * h * 4, 0);
	const char* arteId = k == F_KAIBA ? "blue_eyes" : k == F_MAGO ? "dark_magician" : k == F_EXODIA ? "exodia_the_forbidden_one" : k == F_OJOS_ROJOS ? "red_eyes"
	                     : k == F_ARPIAS                                                                                         ? "harpie_lady_sisters"
	                                                                                                                             : nullptr;
	Img im, im2;
	if (arteId) im = cargar(ui + "cara_" + arteId + ".png");
	if (k == F_DIOSES) {
		im = cargar(ui + "cara_slifer_the_sky_dragon.png");
		im2 = cargar(ui + "cara_obelisk_the_tormentor.png");
	}
	// paleta: fondo (dos tonos), marco, linea del marco y brillo
	C3 f0{ 0, 0, 0 }, f1{ 0, 0, 0 }, marco{ 0, 0, 0 }, filo{ 1.0f, 0.8f, 0.35f }, brillo{ 1, 1, 1 };
	switch (k) {
	case F_KAIBA: f0 = { 0.01f, 0.03f, 0.10f }, f1 = { 0.07f, 0.24f, 0.55f }, marco = { 0.72f, 0.78f, 0.88f }, filo = { 0.85f, 0.95f, 1.0f }, brillo = { 0.35f, 0.8f, 1.0f }; break;
	case F_MAGO: f0 = { 0.04f, 0.01f, 0.08f }, f1 = { 0.30f, 0.09f, 0.45f }, marco = { 0.38f, 0.16f, 0.60f }, brillo = { 0.75f, 0.45f, 1.0f }; break;
	case F_OJO: f0 = { 0.16f, 0.09f, 0.02f }, f1 = { 0.62f, 0.45f, 0.16f }, marco = { 0.85f, 0.65f, 0.25f }, brillo = { 1.0f, 0.85f, 0.4f }; break;
	case F_EXODIA: f0 = { 0.05f, 0.02f, 0.01f }, f1 = { 0.45f, 0.17f, 0.03f }, marco = { 0.80f, 0.45f, 0.10f }, brillo = { 1.0f, 0.55f, 0.15f }; break;
	case F_OJOS_ROJOS: f0 = { 0.02f, 0.0f, 0.0f }, f1 = { 0.35f, 0.02f, 0.03f }, marco = { 0.35f, 0.06f, 0.07f }, filo = { 0.9f, 0.25f, 0.2f }, brillo = { 1.0f, 0.2f, 0.12f }; break;
	case F_ARPIAS: f0 = { 0.14f, 0.02f, 0.10f }, f1 = { 0.70f, 0.25f, 0.55f }, marco = { 0.85f, 0.55f, 0.65f }, brillo = { 1.0f, 0.6f, 0.85f }; break;
	case F_DIOSES: f0 = { 0.05f, 0.03f, 0.02f }, f1 = { 0.4f, 0.3f, 0.1f }, marco = { 0.85f, 0.68f, 0.28f }, brillo = { 1.0f, 0.85f, 0.45f }; break;
	case F_ORICALCO: f0 = { 0.0f, 0.03f, 0.01f }, f1 = { 0.03f, 0.18f, 0.08f }, marco = { 0.12f, 0.30f, 0.16f }, filo = { 0.45f, 1.0f, 0.55f }, brillo = { 0.3f, 1.0f, 0.45f }; break;
	default: break;
	}
	const float cx = (float)w * 0.5f, cy = (float)h * 0.5f, esc = (float)w / 400.0f;
	const float R = 128.0f * esc;   // radio del medallon
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			const float fx = (float)x + 0.5f, fy = (float)y + 0.5f;
			const float qx = fx - cx, qy = fy - cy;
			const float d = sqrtf(qx * qx + qy * qy), ang = atan2f(qy, qx);
			// ---- fondo
			const float rad = clamp01(1.0f - d / (360.0f * esc));
			C3 c = mezclar(f0, f1, rad * rad);
			switch (k) {
			case F_KAIBA: {   // circuito: pistas por celdas y nodos
				const float celda = 22.0f * esc;
				const int gx = (int)floorf(fx / celda), gy = (int)floorf(fy / celda);
				const float lx = fx - (float)gx * celda, ly = fy - (float)gy * celda;
				float v = 0.0f;
				if (hash(gx, gy) > 0.45f) v = std::max(v, linea(fabsf(ly - celda * 0.5f), 2.0f * esc));
				if (hash(gx + 31, gy + 17) > 0.55f) v = std::max(v, linea(fabsf(lx - celda * 0.5f), 2.0f * esc));
				const float dn = sqrtf((lx - celda * 0.5f) * (lx - celda * 0.5f) + (ly - celda * 0.5f) * (ly - celda * 0.5f));
				if (hash(gx + 5, gy + 9) > 0.78f) v = std::max(v, linea(dn, 6.0f * esc));
				c = c + brillo * (v * 0.35f * (0.4f + 0.6f * rad));
				break;
			}
			case F_MAGO: {   // circulo magico: anillos, marcas y estrella de cinco puntas
				float v = linea(fabsf(d - 170.0f * esc), 2.5f * esc) + linea(fabsf(d - 186.0f * esc), 1.5f * esc);
				const float a = ang * 48.0f / 6.2831853f;
				if (d > 170.0f * esc && d < 186.0f * esc && (a - floorf(a)) < 0.4f) v += 0.6f;
				v = std::max(v, linea(dPoligono(qx, qy, 5, 230.0f * esc, -1.5707963f, 2), 2.5f * esc) * 0.7f);
				c = c + brillo * (std::min(v, 1.0f) * 0.45f);
				c = c + C3{ 0.6f, 0.4f, 1.0f } * (powf(fbm(fx * 0.01f, fy * 0.01f), 3.0f) * 0.35f);
				break;
			}
			case F_OJO: {   // arena con bandas de jeroglificos arriba y abajo
				c = c * (0.85f + 0.3f * fbm(fx * 0.03f, fy * 0.012f));
				const float banda = fy < h * 0.17f ? fy / (h * 0.17f) : fy > h * 0.83f ? (h - fy) / (h * 0.17f) : -1.0f;
				if (banda >= 0.0f) {
					const float cel = 26.0f * esc;
					const int gx = (int)floorf(fx / cel), gy = (int)floorf(fy / cel);
					const float lx = fx - gx * cel, ly = fy - gy * cel;
					const float hh = hash(gx, gy);
					float v = 0.0f;
					if (hh < 0.33f) v = linea(fabsf(sqrtf((lx - cel * 0.5f) * (lx - cel * 0.5f) + (ly - cel * 0.5f) * (ly - cel * 0.5f)) - cel * 0.28f), 2.5f * esc);
					else if (hh < 0.66f) v = linea(dSeg(lx, ly, cel * 0.5f, cel * 0.15f, cel * 0.5f, cel * 0.85f), 3.0f * esc) + linea(dSeg(lx, ly, cel * 0.2f, cel * 0.35f, cel * 0.8f, cel * 0.35f), 2.5f * esc);
					else v = linea(dPoligono(lx - cel * 0.5f, ly - cel * 0.5f, 3, cel * 0.32f, -1.5707963f), 2.5f * esc);
					c = mezclar(c, C3{ 0.15f, 0.08f, 0.02f }, std::min(v, 1.0f) * 0.8f);
				}
				break;
			}
			case F_EXODIA: {   // rayos y la estrella de las 5 piezas
				const float rayo = powf(0.5f + 0.5f * cosf(ang * 16.0f), 6.0f) * (1.0f - rad * 0.5f);
				c = c + brillo * (rayo * 0.18f);
				const float st = linea(dPoligono(qx, qy, 5, 200.0f * esc, -1.5707963f, 2), 3.0f * esc);
				c = c + brillo * (st * 0.65f + halo(dPoligono(qx, qy, 5, 200.0f * esc, -1.5707963f, 2), 10.0f * esc) * 0.25f);
				for (int i = 0; i < 5; i++) {   // las 5 piezas: orbes en las puntas
					const float a = -1.5707963f + 6.2831853f * (float)i / 5.0f;
					const float dd = sqrtf((qx - 200.0f * esc * cosf(a)) * (qx - 200.0f * esc * cosf(a)) + (qy - 200.0f * esc * sinf(a)) * (qy - 200.0f * esc * sinf(a)));
					c = c + brillo * (halo(dd, 9.0f * esc) * 0.9f);
				}
				break;
			}
			case F_OJOS_ROJOS: {   // llamas que suben
				const float n = fbm(fx * 0.012f, fy * 0.02f + fbm(fx * 0.02f, fy * 0.01f) * 2.0f);
				const float fuego = powf(clamp01(n * 1.35f - 0.25f), 2.2f) * (0.35f + 0.65f * fy / (float)h);
				c = c + C3{ 1.0f, 0.3f, 0.06f } * (fuego * 0.9f);
				break;
			}
			case F_ARPIAS: {   // plumas: franjas en diagonal con ruido
				const float pl = 0.5f + 0.5f * sinf((fx * 0.8f + fy) * 0.06f + fbm(fx * 0.02f, fy * 0.02f) * 6.0f);
				c = c * (0.75f + 0.35f * pl);
				const float vena = linea(fabsf(fmodf(fx * 0.8f + fy + 4000.0f, 46.0f * esc) - 23.0f * esc), 1.5f * esc);
				c = c + C3{ 1.0f, 0.85f, 0.95f } * (vena * 0.12f);
				break;
			}
			case F_DIOSES: {   // tres tercios: rojo (Slifer), azul (Obelisk) y oro (Ra)
				const float a = ang + 1.5707963f;   // 0 arriba
				const float t3 = fmodf(a + 6.2831853f * 2.0f + 1.0471976f, 6.2831853f) / 2.0943951f;
				const int tercio = std::min(2, (int)t3);
				const C3 col[3] = { { 0.55f, 0.06f, 0.05f }, { 0.75f, 0.55f, 0.12f }, { 0.08f, 0.15f, 0.55f } };
				c = mezclar(c, col[tercio] * (0.5f + 0.7f * rad), 0.75f);
				const float borde = fabsf(t3 - floorf(t3 + 0.5f));
				c = c + brillo * (linea(borde * d * 2.0943951f, 3.0f * esc) * 0.6f);
				break;
			}
			case F_ORICALCO: {   // el sello grande en verde
				const float v = sello(qx, qy, 185.0f * esc);
				c = c + brillo * (v * 0.9f + halo(1.0f - v, 0.25f) * 0.0f);
				c = c + brillo * (powf(fbm(fx * 0.015f, fy * 0.015f), 4.0f) * 0.25f);
				break;
			}
			default: break;
			}
			// ---- medallon(es)
			auto medallon = [&](float mx, float my, float r, const Img* img, int emblema) {
				const float ex = fx - mx, ey = fy - my, dd = sqrtf(ex * ex + ey * ey);
				if (dd > r + 40.0f * esc) return;
				c = c + brillo * (halo(std::max(0.0f, dd - r), 14.0f * esc) * 0.35f);   // resplandor
				const float aro = 7.0f * esc;
				if (dd < r - aro) {
					C3 a{ 0, 0, 0 };
					if (img && img->ok()) {
						a = arte(*img, ex / (r - aro), ey / (r - aro));
					} else if (emblema == 1) {   // Ojo del Milenio
						a = mezclar(C3{ 0.06f, 0.04f, 0.02f }, C3{ 0.18f, 0.12f, 0.05f }, 1.0f - dd / r);
						a = mezclar(a, C3{ 1.0f, 0.82f, 0.35f }, ojoMilenio(ex, ey, r / 128.0f));
					} else if (emblema == 2) {   // Ra: disco solar con rayos
						const float rr = dd / (r - aro);
						a = mezclar(C3{ 1.0f, 0.95f, 0.6f }, C3{ 0.85f, 0.45f, 0.05f }, clamp01(rr * 1.6f));
						a = a + C3{ 1.0f, 0.8f, 0.3f } * (powf(0.5f + 0.5f * cosf(atan2f(ey, ex) * 12.0f), 4.0f) * clamp01(rr * 1.5f - 0.5f) * 0.6f);
					} else if (emblema == 3) {   // Oricalco: centro oscuro con el sello chico
						a = mezclar(C3{ 0.0f, 0.06f, 0.02f }, C3{ 0.02f, 0.2f, 0.07f }, 1.0f - dd / r);
						a = a + C3{ 0.4f, 1.0f, 0.5f } * sello(ex, ey, (r - aro) * 0.85f);
					} else {
						a = mezclar(f1, f0, dd / r);
					}
					const float vin = 1.0f - 0.35f * powf(dd / r, 3.0f);   // un poco de vineta hacia el aro
					c = a * vin;
				} else if (dd < r + aro * 0.5f) {   // aro con bisel
					const float b = 1.0f - fabsf(dd - (r - aro * 0.25f)) / (aro * 0.75f);
					c = filo * (0.45f + 0.75f * clamp01(b)) + C3{ 1, 1, 1 } * (powf(clamp01(b), 8.0f) * 0.3f);
				}
			};
			if (k == F_DIOSES) {
				medallon(cx, cy - 120.0f * esc, 80.0f * esc, &im, 0);
				medallon(cx - 82.0f * esc, cy + 95.0f * esc, 80.0f * esc, &im2, 0);
				medallon(cx + 82.0f * esc, cy + 95.0f * esc, 80.0f * esc, nullptr, 2);
			} else {
				medallon(cx, cy, R, arteId ? &im : nullptr, k == F_OJO ? 1 : k == F_ORICALCO ? 3 : 0);
			}
			// ---- marco: banda con bisel, filo dorado y esquinas redondeadas
			const float e = std::min(std::min(fx, (float)w - fx), std::min(fy, (float)h - fy));
			const float bandaW = 13.0f * esc;
			if (e < bandaW) {
				const float b = 1.0f - fabsf(e - bandaW * 0.5f) / (bandaW * 0.5f);
				c = marco * (0.55f + 0.6f * b) + C3{ 1, 1, 1 } * (powf(clamp01(b), 10.0f) * 0.25f);
			} else if (e < bandaW + 3.0f * esc) {
				c = c * 0.25f;
			} else if (e < bandaW + 6.0f * esc) {
				c = filo * 0.9f;
			}
			// esquinas: rombos dorados
			for (int i = 0; i < 4; i++) {
				const float ox = (i & 1) ? (float)w - 34.0f * esc : 34.0f * esc, oy = (i & 2) ? (float)h - 34.0f * esc : 34.0f * esc;
				const float dr = fabsf(fx - ox) + fabsf(fy - oy);
				if (dr < 11.0f * esc) c = filo * (0.7f + 0.3f * (1.0f - dr / (11.0f * esc)));
			}
			uint8_t* o = &out[((size_t)y * w + x) * 4];
			o[0] = (uint8_t)(clamp01(c.r) * 255.0f + 0.5f);
			o[1] = (uint8_t)(clamp01(c.g) * 255.0f + 0.5f);
			o[2] = (uint8_t)(clamp01(c.b) * 255.0f + 0.5f);
			o[3] = 255;   // (opaca: las esquinas las redondea el shader del dorso)
		}
	}
}

}   // namespace cosmeticos
