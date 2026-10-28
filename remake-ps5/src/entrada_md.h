/*
 * Yugioh Classic Remaster (rama classic-md-entradas): las entradas de la INVOCACION POR FUSION y RITUAL de Yu-Gi-Oh!
 * Master Duel (FusionEffect / RitualEffect), con sus tiempos, sus curvas y sus sprites. docs/md_entradas.md tiene la
 * tabla MD -> aca; la extraccion, D:\Docker\compartido\md_extraido\NOTAS.md ("Entradas de fusion y ritual").
 *
 * Como lo hace Master Duel (dump.cs + desensamblado de FusionEffect.PlayFusionEffect / RitualEffect.PlayRitualEffect y
 * los Timeline de LocalData duel/timeline/duel/universal/summon/):
 *  FUSION: primero SummonFusionShowUnitCard0N (1.333 s: los N materiales se muestran a la camara; SE_SMN_CMN_CARD_01 a
 *    0.067 s) y despues SummonFusion0N_01 (N = materiales, 3.04 s):
 *      0      SE_SMN_FUSION_01_02, fondo negro 0-2.0 s, el vortice (SummonFusionVortex 1.2 s: dos remolinos rojos
 *             (1, 0.25, 0) de 110 u que giran -pi rad/s y se achican a 0.55, y dos azules (0, 0.29, 0.6) de 100 u que giran
 *             -pi/2 rad/s y se achican a 0.64; alfa 0 -> 1 (6 % / 15 %) -> 1 (67 %) -> 0)
 *      0-1.33 SummonFusionInCard0N: cada material cae desde arriba (y 50 -> 0 en 0.5 s, ease-out; inclinado 60 -> 0 grados)
 *             y de 0.5 a 1.33 s se retuerce y se disuelve en el vortice
 *      0.47-2.47 SummonFusionOut: rayos (0.47-1.67), destellos (0.62-1.67, 0.9-1.67, 1.13-1.83) y anillos (1.83-2.46)
 *      1.17-3.04 SummonFusionPostFusion: el estallido a 1.79 s (0.5 s)
 *      1.8    evento StrongSummon + SE_SMN_FUSION_02: sale el monstruo de fusion
 *      2.82   evento StartCard: la carta va a su zona
 *  RITUAL: con materiales SummonRitual01 (5.89 s) y SummonRitualShowUnitCard0N a la vez (0-1.333 s); sin materiales
 *    SummonRitual02 (3.55 s). Ritual01:
 *      1.33   RitualCircle01/02 (3.4 s), 1.72 RitualCircle03/04/05 (3.02 s): cinco circulos azules y violeta que giran
 *             (+0.39, -0.39, +0.196, -0.087 rad/s) y se asientan a 0.5 de su tamanno en 2-8 % de su vida
 *      1.367  SE_SMN_RITUAL_01
 *      1.83-2.30 RitualFire01..08: ocho llamas, una cada 0.067 s, hasta 4.73 s, con su reflejo rojo (0.69, 0.05, 0.09)
 *      2.32   chispas (20, rojizas (1, 0.27, 0.31), 1.78 s)
 *      2.38-3.76 RitualTrailIn: los materiales entran al circulo (SE_SMN_RITUAL_02_01 a 2.447; CenterFlare a 3.26, 0.5 s)
 *      3.5-5.5 la columna de luz (Cylinder: azul (0.23, 0.48, 1) y violeta (0.63, 0, 1))
 *      3.583  SE_SMN_RITUAL_03
 *      3.87-5.89 SummonRitualPostRitual: el estallido (explosion 190 u rosa (0.93, 0.4, 1), 0.09 -> 1 en 0.3 s) a 4.74 s
 *      4.7    StrongSummon; 4.767 SE_SMN_RITUAL_04; 5.2 StartCard
 *    Ritual02 (sin materiales): circulos 0.17, SE 0.217, llamas 0.67-1.13, columna 1.17, SE 1.25, estallido 2.40,
 *    StrongSummon 2.37, SE 2.433, StartCard 2.83.
 *
 * Nuestro juego: el duelo espera la secuencia hasta el StartCard (como EffectTaskRunSummon espera el Timeline) y
 * despues la carta viaja a su zona con el ritmo de MD (md_ritmo_juego.h). Las CARTAS CLAVE (emblematicas y firmas)
 * conservan su entrada (entrada_clave*.h), que es el "StrongSummon" de MD: la secuencia de fusion/ritual se corta en
 * su StrongSummon y sigue la entrada de carta clave (sin repetir el vortice ni el circulo ni sus sonidos: entrada::Estado::trasMD).
 *
 * Este archivo: estado y tiempos, sin Vulkan. Lo usan entrada_md_juego.h (dentro de la clase del juego) y
 * entrada_md_ui.h (dentro de interfaz::Interfaz). El reloj corre con el dt del duelo (velocidad.h y R2).
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace entradamd {

enum Tipo { NINGUNA = 0, FUSION, RITUAL };

// ------------------------------------------------------------------------------------------- tiempos de Master Duel
constexpr float MUESTRA = 1.333f;            // Summon*ShowUnitCard0N (las dos clases)
// fusion: tiempos del Timeline SummonFusion0N_01 (se suma MUESTRA: va despues de mostrar los materiales)
constexpr float F_NEGRO = 2.0f;              // pista "black"
constexpr float F_VORTICE = 1.2f;            // SummonFusionVortex
constexpr float F_CAE = 0.5f;                // SummonFusionInCard: la caida
constexpr float F_ENTRA = 1.333f;            // SummonFusionInCard: fin (se disuelve)
constexpr float F_POST = 1.167f, F_POST_DUR = 1.867f, F_ESTALLIDO = 0.617f;   // SummonFusionPostFusion (+ su Activation 0.62-1.12)
constexpr float F_OUT = 2.467f;              // SummonFusionOut
constexpr float F_STRONG = 1.8f, F_START = 2.817f;
// ritual 01 (con materiales) y 02 (sin)
struct TiemposRitual {
	float circulos, circulos2, durCirc, durCirc2, sonido1, fuego, durFuego, chispas, trail, sonido2, flare, columna, sonido3, post, estallido, strong,
	    sonido4, start, fin;
};
constexpr TiemposRitual RITUAL01{ 1.333f, 1.717f, 3.4f, 3.017f, 1.367f, 1.833f, 2.9f, 2.317f, 2.383f, 2.447f, 3.263f, 3.5f, 3.583f, 3.867f, 4.733f, 4.7f,
	                              4.767f, 5.2f, 5.883f };
constexpr TiemposRitual RITUAL02{ 0.167f, 0.55f, 2.35f, 1.94f, 0.217f, 0.667f, 1.82f, 1.15f, -1.0f, -1.0f, -1.0f, 1.167f, 1.25f, 1.533f, 2.4f, 2.367f,
	                              2.433f, 2.833f, 3.55f };

// la caida de un material de fusion (SummonFusionInCard02 "Recorded (1)": y 50 -> 0, muestreado cada 0.1 s)
inline float caidaFusion(float t)
{
	static const float Y[] = { 1.0f, 0.882f, 0.6064f, 0.2916f, 0.0554f, 0.0f };
	if (t <= 0.0f) return 1.0f;
	const float k = t / 0.1f;
	const int i = (int)k;
	if (i >= 5) return 0.0f;
	return Y[i] + (Y[i + 1] - Y[i]) * (k - (float)i);
}
// la torsion del material ya caido (las propiedades del material de MD suben de 0 a ~29 entre 0.5 y 1.33 s): 0..1
inline float torsionFusion(float t)
{
	if (t <= F_CAE) return 0.0f;
	const float u = std::min(1.0f, (t - F_CAE) / (F_ENTRA - F_CAE));
	return u * u * (1.6f - 0.6f * u);
}

// el alfa de un sistema de particulas de MD con su degradado de 4 llaves (0 -> 1 en a, 1 hasta b, -> 0 en 1)
inline float alfaVida(float u, float a, float b)
{
	if (u <= 0.0f || u >= 1.0f) return 0.0f;
	if (u < a) return u / a;
	if (u <= b) return 1.0f;
	return 1.0f - (u - b) / (1.0f - b);
}
// el tamanno de un circulo ritual: de "desde" a 0.5 en la fraccion "f" de su vida y despues quieto
inline float tamCirculo(float u, float desde, float f)
{
	if (u <= 0.0f) return desde;
	return u >= f ? 0.5f : desde + (0.5f - desde) * (u / f);
}

inline float suave(float a, float b, float x)
{
	const float u = std::max(0.0f, std::min(1.0f, (x - a) / (b - a)));
	return u * u * (3.0f - 2.0f * u);
}

// ------------------------------------------------------------------------------------------------------- estado
struct Sonido { float t; const char* clave; };

struct Estado {
	bool activa{ false };
	Tipo tipo{ NINGUNA };
	std::string carta;                    // la que se invoca
	int lado{ 0 };
	std::vector<std::string> materiales;  // (los monstruos que se fueron en la invocacion, en orden)
	bool conMateriales{ true };           // ritual: 01 (con) o 02 (sin)
	bool clave{ false };                  // sigue la entrada de carta clave (se corta en el StrongSummon)
	bool saltando{ false };               // R2 (sin sonidos)
	float t{ 0.0f };
	float espera{ 0.0f };                 // lo que espera el duelo (StartCard; con carta clave, StrongSummon)
	float fin{ 0.0f };                    // lo que se sigue viendo (la cola corre mientras la carta viaja)
	float strong{ 0.0f };                 // el evento StrongSummon (tiempo de la secuencia)
	size_t suceso{ (size_t)-1 };          // el S_INVOCA que la pidio (no se repite)
	std::string ultima;                   // "lado:carta" de la ultima secuencia (la entrada de carta clave no repite el vortice)
	float tUltima{ 99.0f };               // s desde que termino
};
inline Estado& estado()
{
	static Estado e;
	return e;
}

// los sonidos de la secuencia (claves de audio.txt; herramientas/audio/seleccion.json)
inline std::vector<Sonido> sonidos(const Estado& e)
{
	std::vector<Sonido> v;
	if (e.tipo == FUSION) {
		v.push_back({ 0.067f, "sfx_md_carta" });                        // SE_SMN_CMN_CARD_01 (ShowUnitCard)
		v.push_back({ MUESTRA, "sfx_md_fusion_1" });                    // SE_SMN_FUSION_01_02
		v.push_back({ MUESTRA + F_STRONG, "sfx_md_fusion_2" });         // SE_SMN_FUSION_02
	} else if (e.tipo == RITUAL) {
		const TiemposRitual& r = e.conMateriales ? RITUAL01 : RITUAL02;
		if (e.conMateriales) v.push_back({ 0.0f, "sfx_md_carta" });     // SE_SMN_CMN_CARD_01 (ShowUnitCard)
		v.push_back({ r.sonido1, "sfx_md_ritual_1" });                  // SE_SMN_RITUAL_01
		if (r.sonido2 >= 0.0f) v.push_back({ r.sonido2, "sfx_md_ritual_2" });   // SE_SMN_RITUAL_02_01 (RitualTrailIn)
		v.push_back({ r.sonido3, "sfx_md_ritual_3" });                  // SE_SMN_RITUAL_03
		v.push_back({ r.sonido4, "sfx_md_ritual_4" });                  // SE_SMN_RITUAL_04
	}
	return v;
}

// arma la secuencia: tiempos de espera y de fin segun la clase, los materiales y si sigue una carta clave
inline void preparar(Estado& e)
{
	if (e.tipo == FUSION) {
		e.strong = MUESTRA + F_STRONG;
		e.espera = MUESTRA + F_START;
		e.fin = MUESTRA + F_POST + F_POST_DUR;
	} else {
		const TiemposRitual& r = e.conMateriales ? RITUAL01 : RITUAL02;
		e.strong = r.strong;
		e.espera = r.start;
		e.fin = r.fin;
	}
	if (e.clave) {   // (la entrada de carta clave hace de StrongSummon: la secuencia se va 0.25 s despues)
		e.espera = e.strong;
		e.fin = e.strong + 0.25f;
	}
}

}  // namespace entradamd
