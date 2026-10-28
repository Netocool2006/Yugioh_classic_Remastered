/*
 * Yugioh Classic Remaster (rama classic-moneda): el lanzamiento de moneda antes del duelo, al estilo de Master Duel
 * (DuelStartViewController). SOLO logica y tiempos (sin Vulkan): lo lleva moneda_juego.h (dentro de la clase del
 * juego), lo dibuja moneda_ui.h (dentro de la interfaz) y la moneda la pinta el shader de la interfaz (tipo MONEDA).
 *
 * Etapas:
 *   VS            los dos duelistas entran desde los costados, frente a frente, con un "VS" en el centro.
 *   GIRO          la moneda sube girando, cae y rebota (brillo y particulas al caer). Cara = gana el jugador,
 *                 cruz = gana el rival: el jugador NO elige, es al azar (moneda justa).
 *   RESULTADO     "Ganaste el lanzamiento de moneda" / "El rival ganó el lanzamiento".
 *   ORDEN         (gana el jugador) "Ir primero" / "Ir segundo": izquierda/derecha y X. No hay forma de saltarla.
 *   RIVAL_DECIDE  (gana el rival) la IA decide con su personalidad y se muestra "El rival decide ir ...".
 * Los relojes corren con el dt del duelo, que ya trae la velocidad de animaciones (velocidad.h): con Rapida o Muy
 * rapida (o R2) toda la animacion se acorta.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace moneda {

enum Etapa { VS = 0, GIRO, RESULTADO, ORDEN, RIVAL_DECIDE };
enum { CRUZ = 0, CARA = 1 };

// tiempos (s a velocidad normal)
constexpr float T_VS = 1.6f;          // entrada de los duelistas
constexpr float T_SUBE = 0.55f;       // la moneda sube hasta aqui
constexpr float T_CAE = 1.45f;        // y toca la mesa aqui (cae con la cara final)
constexpr float T_GIRO = 2.3f;        // rebotes y reposo
constexpr float T_RESULTADO = 1.6f;   // el cartel del resultado
constexpr float T_RIVAL = 2.4f;       // el aviso de lo que decidio el rival
constexpr float T_MIN_X = 0.3f;       // X recien cuenta pasado este tiempo en cada etapa
constexpr float VUELTAS = 4.0f;       // vueltas completas en el aire

// sonidos que ya sonaron en el lanzamiento (bits)
enum Sonido : unsigned { S_VS = 1u, S_LANZA = 2u, S_GIRA = 4u, S_CAE = 8u, S_REBOTA = 16u, S_BRILLO = 32u };

struct Estado {
	int etapa{ VS };
	int opcion{ 0 };     // ORDEN: 0 ir primero, 1 ir segundo
	int cara{ -1 };      // CARA / CRUZ (-1 aun no se lanzo)
	int ganador{ -1 };   // 0 jugador, 1 rival
	int empieza{ -1 };   // 0 empieza el jugador, 1 el rival
	float t{ 0.0f };     // s en la etapa
	unsigned sonidos{ 0 };
	void reiniciar() { *this = Estado{}; }
};

// la moneda: un bit del azar (justa). 1 cara, 0 cruz
inline int lanzar(uint32_t azar) { return (int)((azar >> 13) & 1u); }
inline int ganadorDe(int cara) { return cara == CARA ? 0 : 1; }

// donde esta la moneda en el GIRO (t en s desde que se lanzo). y: desplazamiento vertical en fracciones del alto de
// la pantalla respecto del punto de reposo (negativo = arriba); escala: tamanno relativo (mas cerca de la camara
// arriba); angulo: giro alrededor del eje horizontal (cos > 0 se ve la cara, cos < 0 la cruz); brillo: destello al caer
// (1 al tocar, se apaga); cayo: ya toco la mesa
struct Pose {
	float y{ 0.0f }, escala{ 1.0f }, angulo{ 0.0f }, brillo{ 0.0f };
	bool cayo{ false };
};
inline float anguloFinal(int cara) { return 6.2831853f * VUELTAS + (cara == CARA ? 0.0f : 3.1415927f); }
inline Pose pose(float t, int cara)
{
	Pose p;
	const float fin = anguloFinal(cara);
	const float arriba = -0.2f, abajo = 0.34f;
	if (t < T_CAE) {
		const float u = std::max(0.0f, t) / T_CAE;
		p.angulo = fin * (1.0f - (1.0f - u) * (1.0f - u));   // gira rapido y frena hasta caer con la cara final
		if (t < T_SUBE) {
			const float a = std::max(0.0f, t) / T_SUBE;
			p.y = abajo + (arriba - abajo) * (1.0f - (1.0f - a) * (1.0f - a));
		} else {
			const float a = (t - T_SUBE) / (T_CAE - T_SUBE);
			p.y = arriba * (1.0f - a * a);
		}
		p.escala = 1.0f + 0.32f * std::max(0.0f, std::min(1.0f, -p.y / -arriba)) - 0.3f * std::max(0.0f, std::min(1.0f, p.y / abajo));
		return p;
	}
	const float tb = t - T_CAE;
	p.cayo = true;
	p.brillo = std::exp(-tb * 2.6f);
	// dos rebotes cada vez mas bajos y un cabeceo que se apaga (se ve el canto un instante)
	const float rebote = 0.05f * std::fabs(std::sin(tb * 3.1415927f / 0.26f)) * std::exp(-tb * 5.0f);
	p.y = -rebote;
	p.escala = 1.0f + rebote;
	p.angulo = fin + 0.5f * std::exp(-tb * 4.5f) * std::sin(tb * 19.0f);
	return p;
}
// cuanto dura la animacion hasta que se puede elegir / se avisa (s a velocidad normal)
inline float duracionHastaDecidir() { return T_VS + T_GIRO + T_RESULTADO; }

}   // namespace moneda
