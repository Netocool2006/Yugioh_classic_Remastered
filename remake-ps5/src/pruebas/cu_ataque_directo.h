// Casos de uso del ATAQUE DIRECTO (rama classic-ps5-ataque, ataque_md.h / ataque_md_juego.h). El usuario probo en la PS5:
// "cuando el oponente no tiene monstruos en su campo la carta debe atacar directo al oponente, no al tablero vacio".
// Como en Master Duel (EffectTaskBattleRun.AttackInitStep), el viaje del ataque directo va al punto del que recibe:
// abajo al centro de la pantalla (hacia la camara) si recibe el jugador, detras del campo del rival si recibe el rival.
// Estos casos FALLAN si el objetivo visual del ataque directo es una zona del tablero (lo de antes: la zona del centro).
//  - CU-FE-ATAQUE-DIRECTO-PUNTO: el punto de MD de los dos lados, fuera de la mesa, en la pantalla y a la altura de MD.
//  - CU-FE-ATAQUE-DIRECTO-JUGADOR: el jugador ataca directo al CPU desde el duelo real (duelo_juego.h): el ataque animado
//    es directo, va al lado del rival fuera de la mesa (no a una zona) y baja los LP del rival.
//  - CU-FE-ATAQUE-DIRECTO-CPU: el CPU ataca directo al jugador: va hacia la camara, fuera de la mesa, y baja los LP.
#pragma once

#include <glm/gtc/matrix_transform.hpp>

#include "casos_uso.h"
#include "cu_front.h"

namespace cuad {

using VD = control::VistaDuelo;

// el punto no cae sobre ninguna zona de la mesa (las de monstruo, magia, campo, cementerio...)
inline int zonaBajo(glm::vec3 p)
{
	for (int z = 0; z < control::NUM_ZONAS_MESA; z++) {
		if (ataquemd::sobreZonaDeMesa(p, control::ZONAS_MESA[z].x * ataquemd::ESCALA, control::ZONAS_MESA[z].z * ataquemd::ESCALA)) return z;
	}
	return -1;
}

inline void punto()
{
	glm::vec3 ojo, mira;
	float fov, aspecto;
	Juego::camaraJuegoMD(ojo, mira, fov, aspecto);
	const float h = ataquemd::altoDirecto();
	for (int lado = 0; lado < 2; lado++) {
		const bool alJugador = lado == 0;
		const glm::vec3 p = ataquemd::puntoDirecto(alJugador, ojo, mira, fov, aspecto);
		cu::caso("CU-FE-ATAQUE-DIRECTO-PUNTO", ataquemd::fueraDeLaMesa(p) && zonaBajo(p) < 0, "el punto del ataque directo %s (%.2f %.2f %.2f) cae sobre la mesa (zona %d)",
		         alJugador ? "al jugador" : "al rival", p.x, p.y, p.z, zonaBajo(p));
		cu::caso("CU-FE-ATAQUE-DIRECTO-PUNTO", fabsf(p.y - h) < 0.01f, "el punto no esta a la altura de MD (%.2f, esperada %.2f)", p.y, h);
		cu::caso("CU-FE-ATAQUE-DIRECTO-PUNTO", alJugador ? p.z > ataquemd::Z_MAGIAS : p.z < -ataquemd::Z_MAGIAS,
		         "el ataque directo %s no va hacia el lado del que recibe (z %.2f)", alJugador ? "al jugador" : "al rival", p.z);
		// en la pantalla de la camara de juego (el golpe se ve)
		const glm::mat4 vp = glm::perspectiveRH(fov, aspecto, 0.1f, 80.0f) * glm::lookAtRH(ojo, mira, glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::vec4 c = vp * glm::vec4(p, 1.0f);
		const bool enPantalla = c.w > 0.0f && fabsf(c.x / c.w) <= 1.0f && fabsf(c.y / c.w) <= 1.05f;
		cu::caso("CU-FE-ATAQUE-DIRECTO-PUNTO", enPantalla, "el punto del ataque directo %s queda fuera de la pantalla (%.2f %.2f)", alJugador ? "al jugador" : "al rival",
		         c.w > 0.0f ? c.x / c.w : 9.0f, c.w > 0.0f ? c.y / c.w : 9.0f);
	}
	// la sacudida de la camara (AttackDirectNear/Far): se mueve en el golpe y se apaga a los 0.5 s
	// (MD: un empujon de la POSICION hacia arriba y hacia atras que se apaga lineal; nunca baja ni pasa de su maximo, asi no
	// se ve el borde de la escena)
	const float e = ataquemd::escalaSacudidaMD();
	bool sube = true, acotada = true, apaga = true;
	float antes = 1e9f;
	for (float t = 0.0f; t < 0.5f; t += 1.0f / 60.0f) {
		const glm::vec3 v = ataquemd::sacudidaDirecto(t, 7u, e);
		sube &= v.y >= 0.0f;
		acotada &= fabsf(v.x) <= 0.89f * e && v.y <= 11.01f * e && v.z >= 0.0f && v.z <= 8.81f * e;
		if (((int)(t * 60.0f + 0.5f)) % 3 == 0) {
			apaga &= v.y <= antes + 2.3f * e;   // (cada valor nuevo puede saltar dentro del rango, pero va bajando)
			antes = v.y;
		}
	}
	cu::caso("CU-FE-ATAQUE-DIRECTO-PUNTO", glm::length(ataquemd::sacudidaDirecto(0.02f, 7u, e)) > 0.5f * 8.8f * e && ataquemd::sacudidaDirecto(0.5f, 7u, e) == glm::vec3(0.0f),
	         "la sacudida del ataque directo no sigue CameraShakerSetting AttackDirectNear/Far (empujon de 0.5 s)");
	cu::caso("CU-FE-ATAQUE-DIRECTO-PUNTO", sube && acotada && apaga, "la sacudida del ataque directo baja la camara o se pasa del rango de MD (sube %d, acotada %d, apaga %d)",
	         (int)sube, (int)acotada, (int)apaga);
	cu::caso("CU-FE-ATAQUE-DIRECTO-PUNTO", ataquemd::alfaMarcoDanno(0.1f) > 0.99f && ataquemd::alfaMarcoDanno(1.01f) == 0.0f, "el marco rojo del danno no sigue DamageFrame de MD");
}

// mira el ataque animado hasta que empiece (s segundos); guarda si es directo, a donde va y quien ataca
struct Visto {
	bool hubo{ false }, directo{ false };
	int atacante{ -1 }, objetivo{ -1 };
	glm::vec3 destino{ 0.0f };
};
inline Visto mirarAtaque(Juego& g, float s, const std::function<bool(const Juego&)>& cual)
{
	Visto v;
	for (float t = 0.0f; t < s && !v.hubo; t += 0.05f) {
		if (g.hayAtaque() && cual(g)) {
			v.hubo = true;
			v.directo = g.ataqueDirecto;
			v.atacante = g.zonaAtacante;
			v.objetivo = g.zonaObjetivo;
			v.destino = g.destinoAtaqueMundo();
			break;
		}
		if (g.dueloIA.ganador >= 0) break;
		// (preguntas del duelo: no activar nada)
		if (g.dueloIA.decide() == 0 && g.ctl.estado == control::MENU && g.ctl.enPregunta) {
			g.ctl.opcion = (int)g.ctl.opciones.size() - 1;
			g.apretar(PAD_CROSS, 0.05f);
			continue;
		}
		g.esperar(0.05f);
	}
	return v;
}

inline void comprobar(const char* id, const Visto& v, bool alJugador)
{
	cu::caso(id, v.hubo, "no hubo ataque animado");
	if (!v.hubo) return;
	cu::caso(id, v.directo, "el ataque sin monstruos del que recibe no se animo como directo (va a la zona %s)", control::ZONAS_MESA[v.objetivo].id);
	const int z = zonaBajo(v.destino);
	cu::caso(id, z < 0 && ataquemd::fueraDeLaMesa(v.destino), "el objetivo visual del ataque directo es una zona del tablero (%s, punto %.2f %.2f %.2f)",
	         z >= 0 ? control::ZONAS_MESA[z].id : "-", v.destino.x, v.destino.y, v.destino.z);
	cu::caso(id, alJugador ? v.destino.z > ataquemd::Z_MAGIAS : v.destino.z < -ataquemd::Z_MAGIAS, "el ataque directo no va hacia %s (z %.2f)",
	         alJugador ? "el jugador (la camara)" : "el rival", v.destino.z);
}

inline void duelo(Juego& g)
{
	cuf::irARivales(g, VD::LIBRE);
	g.apretar(PAD_CROSS, 0.3f);
	cuf::pasarMoneda(g, false);
	if (!cuf::esperarJugador(g, 90.0f)) {
		cu::caso("CU-FE-ATAQUE-DIRECTO-JUGADOR", false, "el jugador nunca quedo libre para atacar directo");
		cu::caso("CU-FE-ATAQUE-DIRECTO-CPU", false, "el jugador nunca quedo libre (sin el ataque directo del CPU)");
		cuf::abandonar(g);
		return;
	}
	ia::Duelo& d = g.dueloIA;
	auto mesaLimpia = [&]() {
		for (ia::Lado& x : d.l) {
			x.m = {};
			x.mt = {};
		}
		d.l[0].mano.clear();
		d.l[1].mano.clear();
		d.recalcular();
	};
	// 1) el jugador ataca directo al CPU
	mesaLimpia();
	const int a = cuf::ponerMonstruoCU(d, 0, "summoned_skull");
	d.turno = std::max(d.turno, 3);
	d.fase = ia::F_BATALLA;
	d.ef.batallaTerminada = false;
	const int lpR = d.l[1].lp;
	ia::Jugada j;
	j.tipo = ia::J_ATACAR;
	j.zona = a;
	j.objetivo = -1;
	const bool ok = d.aplicar(j);
	cu::caso("CU-FE-ATAQUE-DIRECTO-JUGADOR", ok, "el motor no dejo atacar directo con el campo del rival vacio");
	const Visto vj = mirarAtaque(g, 10.0f, [](const Juego& x) { return !Juego::zonaDelRivalMD(x.zonaAtacante); });
	comprobar("CU-FE-ATAQUE-DIRECTO-JUGADOR", vj, false);
	// rama classic-ps5-ataque3s: el ataque dura como en Joey (~3 s) en velocidad Normal (mirarAtaque lo vio empezar)
	if (vj.hubo) {
		float d = 0.0f;
		while (g.hayAtaque() && d < 10.0f) {   // (de a un cuadro: esperar() redondea hacia abajo los cuadros)
			g.cuadro(0);
			d += g.frameTimer;
		}
		cu::caso("CU-FE-ATAQUE-DURACION-JOEY", velocidad::nivel() != velocidad::NORMAL || (d >= 2.8f - 0.06f && d <= 3.3f),
		         "el ataque animado del duelo (velocidad Normal) duro %.2f s (Joey: ~3 s; se pide entre 2.8 y 3.3)", d);
	}
	cuf::esperarQue(g, 12.0f, [&]() { return !g.hayAtaque() && g.interfazDuelo.lp(true) < lpR; });
	cu::caso("CU-FE-ATAQUE-DIRECTO-JUGADOR", g.interfazDuelo.lp(true) == std::max(0, lpR - 2500), "los LP del rival en pantalla no bajaron por el ataque directo (%d -> %d)", lpR,
	         g.interfazDuelo.lp(true));
	if (d.ganador >= 0) return;
	// 2) el CPU ataca directo al jugador: el jugador sin monstruos, el rival con uno; el jugador pasa el turno
	cuf::esperarQue(g, 20.0f, [&]() { return !d.ocupado() && !g.ctl.bloqueado; });
	mesaLimpia();
	cuf::ponerMonstruoCU(d, 1, "summoned_skull");
	const int lpJ = d.l[0].lp;
	for (int k = 0; k < 8 && d.activo == 0 && d.ganador < 0; k++) {
		ia::Jugada f;
		f.tipo = ia::J_FASE;
		if (!d.aplicar(f)) break;
		cuf::esperarQue(g, 6.0f, [&]() { return !d.ocupado(); });
	}
	const Visto vc = mirarAtaque(g, 60.0f, [](const Juego& x) { return Juego::zonaDelRivalMD(x.zonaAtacante); });
	comprobar("CU-FE-ATAQUE-DIRECTO-CPU", vc, true);
	cuf::esperarQue(g, 12.0f, [&]() { return !g.hayAtaque(); });
	cu::caso("CU-FE-ATAQUE-DIRECTO-CPU", !vc.hubo || g.interfazDuelo.lp(false) < lpJ, "los LP del jugador en pantalla no bajaron por el ataque directo del CPU (%d -> %d)", lpJ,
	         g.interfazDuelo.lp(false));
	if (d.ganador < 0) cuf::abandonar(g);
}

// rama classic-ps5-ataque3s: "lo quiero como Joey, el ataque a 3 segundos". En velocidad Normal el ataque dura entre 2.8 y
// 3.3 s: carta plana con viaje "Low" (0.45) y "High" (0.65) y figura 3D que ataca (carga del rayo a VEL_FIG_PREVIA); el
// golpe (bajada de LP) sigue en el impacto de MD; Rapida y Muy rapida lo acortan en proporcion
inline void duracionJoey()
{
	const float bajo = ritmo::realAtaque(ritmo::T_FIN_ATAQUE, false), alto = ritmo::realAtaque(ritmo::T_FIN_ATAQUE, true);
	const float fig = ritmo::T_ATAQUE_BAJO / ritmo::VEL_FIG_PREVIA + (ritmo::T_FIN_ATAQUE - ritmo::T_ATAQUE_BAJO);
	auto en = [](float d) { return d >= 2.8f && d <= 3.3f; };
	cu::caso("CU-FE-ATAQUE-DURACION-JOEY", en(bajo) && en(alto) && en(fig), "el ataque en Normal no dura ~3 s como Joey: plana %.2f / %.2f (High), figura %.2f", bajo, alto, fig);
	cu::caso("CU-FE-ATAQUE-DURACION-JOEY", ritmo::realAtaque(ritmo::T_ATAQUE_BAJO, false) <= 0.46f && ritmo::T_ROMPE < ritmo::T_FIN_ATAQUE,
	         "el golpe (bajada de LP) ya no cae en el impacto de MD (0.45 s)");
	for (int n = velocidad::RAPIDA; n < velocidad::NIVELES; n++) {
		const float d = bajo / velocidad::factor(n);
		cu::caso("CU-FE-ATAQUE-DURACION-JOEY", d < bajo && fabsf(d * velocidad::factor(n) - bajo) < 1e-3f, "la velocidad %d no acorta el ataque en proporcion (%.2f s)", n, d);
	}
}

inline void todo(Juego& g)
{
	punto();
	duracionJoey();
	duelo(g);
}

}   // namespace cuad
