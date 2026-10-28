// rama classic-ps5-ataque: el ATAQUE DIRECTO de Master Duel traducido a C++ (funciones puras, sin Vulkan; las usan
// ataque_md_juego.h en el juego, la interfaz y las pruebas). Fuente: GameAssembly.dll de MD (Il2CppDumper + desensamblado,
// md_extraido/ritmo/desasm/desasm_batalla2.txt) y sus ScriptableObject (md_extraido/ritmo/otros_settings.txt):
//  - EffectTaskBattleRun.AttackInitStep: si el ataque es directo, el objetivo del viaje (attackTargetPos) NO es una zona:
//      pos = Util.ScreenPointToFixedHeightWorldPoint(camara, Camera.ViewportToScreenPoint(vp), SharedDefinition.directAttackPositionHeight)
//    con vp = SharedDefinition.directAttackViewportNear (0.5, 0.0) si el que recibe (dstTeam) es "Myself" (el jugador:
//    abajo al centro de la pantalla, hacia la camara) y directAttackViewportFar (0.5, 1.0) si es el rival (arriba al
//    centro, detras de su campo, donde esta su retrato), a 15 unidades de MD de alto (la carta mide 8: 1.875 cartas).
//  - El viaje es el mismo Bezier Strike*Low/High (AttackMotionSetting) hasta ese punto; el golpe (hitTiming OnFinish)
//    es el mismo; luego ShowDamageImpl -> DuelLP.ChangeLP baja los LP del que recibe, DuelHUD.ShowDamageFrame (el
//    marco rojo, solo si el danno es del jugador) y MainCameraOrganizer.Shake("AttackDirectNear" / "AttackDirectFar").
//  - CameraShakerSetting AttackDirectNear/Far: 0.5 s, sacudida al azar (useRandomShake): x -0.88..0.88, y 8.8..11,
//    z -8.8..-6.6 (unidades de MD), un valor nuevo cada 3 cuadros (randomDelayFrame) que se atenua x0.8 (randomDelayGain).
//  - DuelHUD.DamageFrame (prefab): alfa 0 -> 1 en 0.1 s -> 0 a 1.0 s, color (0.61, 0.09, 0.09).
// Adaptacion (solo la geometria): nuestra camara de juego mira mas baja que la "Top" de MD (y 95, 70 grados): el rayo
// por el borde de ARRIBA de la pantalla casi no baja y no corta la altura de 15 dentro de la mesa; entonces el punto
// lejano queda detras de la fila de magias del rival (una carta mas alla), a la misma altura. El cercano sale tal cual.
#pragma once

#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

namespace ataquemd {

// SharedDefinition (DuelEffectDefine): DirectAttackPositionHeight, DirectAttackViewportNear / Far
constexpr float MD_ALTO_CARTA = 8.0f;   // alto de una carta en unidades de MD (NOTAS.md)
constexpr float ALTO_DIRECTO_MD = 15.0f;
constexpr float VIEWPORT_NEAR_Y = 0.0f, VIEWPORT_FAR_Y = 1.0f, VIEWPORT_X = 0.5f;
// CameraShakerSetting "AttackDirectNear" / "AttackDirectFar" (los dos iguales)
constexpr float SACUDIDA_DURACION = 0.5f;
constexpr float SACUDIDA_MIN[3] = { -0.88f, 8.8f, -8.8f }, SACUDIDA_MAX[3] = { 0.88f, 11.0f, -6.6f };
constexpr int SACUDIDA_CADA_CUADROS = 3;   // randomDelayFrame (cuadros de MD, 60 por segundo)
constexpr float SACUDIDA_GANANCIA = 0.8f;  // randomDelayGain
// DuelHUD DamageFrame
constexpr float MARCO_SUBE = 0.1f, MARCO_FIN = 1.0f;
constexpr float MARCO_COLOR[3] = { 0.61f, 0.09f, 0.09f };

// la mesa (interfaz.h ZONAS, x1.4): fila de magias a 5.5, la carta mide 1.45 x 1.0 (x1.4)
constexpr float ESCALA = 1.4f;
constexpr float CARTA_ALTO = 1.45f * ESCALA, CARTA_ANCHO = 1.0f * ESCALA;
constexpr float Z_MAGIAS = 5.5f * ESCALA, X_BORDE = 8.2f * ESCALA;

inline float altoDirecto() { return ALTO_DIRECTO_MD / MD_ALTO_CARTA * CARTA_ALTO; }

// Util.ScreenPointToFixedHeightWorldPoint: el rayo de la camara por el punto de la pantalla (viewport x, y: 0 abajo, 1
// arriba, como Unity) cortado con el plano y = alto. ojo/mira: la camara; fovVertical en radianes; aspecto ancho/alto.
// Devuelve false si el rayo no baja hasta esa altura
inline bool puntoPantallaAAltura(glm::vec3 ojo, glm::vec3 mira, float fovVertical, float aspecto, float vx, float vy, float alto, glm::vec3& p)
{
	const glm::vec3 f = glm::normalize(mira - ojo);
	glm::vec3 der = glm::cross(f, glm::vec3(0.0f, 1.0f, 0.0f));
	der = glm::length(der) > 1e-4f ? glm::normalize(der) : glm::vec3(1.0f, 0.0f, 0.0f);
	const glm::vec3 arr = glm::cross(der, f);
	const float th = tanf(0.5f * fovVertical);
	const glm::vec3 d = glm::normalize(f + der * ((2.0f * vx - 1.0f) * th * aspecto) + arr * ((2.0f * vy - 1.0f) * th));
	if (fabsf(d.y) < 1e-4f) return false;
	const float t = (alto - ojo.y) / d.y;
	if (t <= 0.0f) return false;
	p = ojo + d * t;
	return true;
}

// EffectTaskBattleRun.AttackInitStep (ataque directo): a donde vuela el atacante. alJugador: el que recibe es el
// jugador (MD "Myself": viewport Near, abajo); si no, el rival (Far, arriba). El punto queda siempre fuera de la mesa:
// el cercano entre el campo del jugador y la camara, el lejano detras del campo del rival
inline glm::vec3 puntoDirecto(bool alJugador, glm::vec3 ojo, glm::vec3 mira, float fovVertical, float aspecto)
{
	const float h = altoDirecto();
	glm::vec3 p(0.0f, h, 0.0f);
	const bool hay = puntoPantallaAAltura(ojo, mira, fovVertical, aspecto, VIEWPORT_X, alJugador ? VIEWPORT_NEAR_Y : VIEWPORT_FAR_Y, h, p);
	const float zFueraJugador = Z_MAGIAS + CARTA_ALTO;         // pasando la fila de magias del jugador
	const float zFueraRival = -(Z_MAGIAS + CARTA_ALTO);        // detras de la fila de magias del rival
	if (alJugador) {
		if (!hay || p.z < zFueraJugador) p = glm::vec3(0.0f, h, zFueraJugador);
		if (p.z > ojo.z - 1.0f) p.z = ojo.z - 1.0f;   // (nunca detras de la camara)
	} else {
		if (!hay || p.z > zFueraRival || p.z < zFueraRival - 2.0f * CARTA_ALTO) p = glm::vec3(0.0f, h, zFueraRival);
	}
	p.x = 0.0f;   // (viewport x 0.5: el centro de la mesa)
	p.y = h;
	return p;
}

// el punto cae sobre una zona de la mesa (lo que el usuario rechazo: "no al tablero vacio")? Las pruebas lo usan
inline bool sobreZonaDeMesa(glm::vec3 p, float zx, float zz)
{
	return fabsf(p.x - zx) <= 0.5f * CARTA_ALTO && fabsf(p.z - zz) <= 0.5f * CARTA_ALTO && p.y < 0.6f * altoDirecto();
}
// el punto esta fuera de la mesa (mas alla de las filas de magias o por encima de la altura de MD)
inline bool fueraDeLaMesa(glm::vec3 p) { return fabsf(p.z) >= Z_MAGIAS + 0.5f * CARTA_ALTO || p.y >= 0.9f * altoDirecto(); }

static inline float azarMD(uint32_t n)
{
	n = (n << 13u) ^ n;
	n = n * (n * n * 15731u + 789221u) + 1376312589u;
	return (float)(n & 0x7fffffffu) / 2147483647.0f;
}

// CameraShaker.UpdateShake con el "AttackDirectNear/Far" (confirmado en GameAssembly.dll, docs/ataque_md.md):
// CameraShakerSetting.Info.GetShake (RVA 0x4a6d20) con useRandomShake: por eje Random.Range(min, max) * (1 - t/duration),
// un valor nuevo cada randomDelayFrame (3) cuadros de 60 Hz que se acerca al anterior con randomDelayGain (0.8); lo deja
// en CameraShaker.shakeOffset y MainCameraOrganizer.UpdateGameCam (RVA 0x506160) lo SUMA A LA POSICION de la camara en
// ejes del mundo (la rotacion no cambia). Es un empujon (y 8.8..11 arriba, z -8.8..-6.6 hacia el lado de la camara) que
// se apaga lineal en 0.5 s, no un vaiven. Devuelve el desplazamiento en NUESTROS ejes (la camara de MD mira +z desde
// z -37; la nuestra mira -z: z y x cambian de signo), en m: escala = m por unidad de MD (carta: CARTA_ALTO / 8)
inline glm::vec3 sacudidaDirecto(float t, uint32_t semilla, float escala)
{
	if (t < 0.0f || t >= SACUDIDA_DURACION) return glm::vec3(0.0f);
	const int k = (int)(t * 60.0f) / SACUDIDA_CADA_CUADROS;
	glm::vec3 v(0.0f);
	for (int j = 0; j <= k; j++) {
		glm::vec3 r;
		for (int i = 0; i < 3; i++) {
			const float a = azarMD(semilla * 7919u + (uint32_t)j * 31u + (uint32_t)i * 101u);
			r[i] = SACUDIDA_MIN[i] + (SACUDIDA_MAX[i] - SACUDIDA_MIN[i]) * a;
		}
		v = j == 0 ? r : glm::mix(v, r, SACUDIDA_GANANCIA);
	}
	v *= 1.0f - t / SACUDIDA_DURACION;
	return glm::vec3(-v.x, v.y, -v.z) * escala;
}
inline float escalaSacudidaMD() { return CARTA_ALTO / MD_ALTO_CARTA; }

// DuelHUD.ShowDamageFrame: alfa del marco rojo t segundos despues del golpe
inline float alfaMarcoDanno(float t)
{
	if (t < 0.0f || t > MARCO_FIN) return 0.0f;
	if (t < MARCO_SUBE) return t / MARCO_SUBE;
	return 1.0f - (t - MARCO_SUBE) / (MARCO_FIN - MARCO_SUBE);
}

}   // namespace ataquemd
