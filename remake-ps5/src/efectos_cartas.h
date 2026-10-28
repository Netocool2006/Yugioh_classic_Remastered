// Yugioh Classic Remaster - animaciones de activacion de magias y trampas (rama classic-fx-md).
// Origen: Yu-Gi-Oh! Master Duel (copia del usuario en Steam, uso personal, NADA de eso va al repo). Se investigo
// con UnityPy (herramientas/fx_md/): los efectos de cartas de Master Duel son prefabs con ParticleSystem (Unity) y
// Shader Graph propios, ordenados por un Timeline (duel/timeline/card/tcg/ef<cid>, cid = id de carta de Konami) y los
// genericos del duelo (fxp_bff_apply = aplicar efecto, fxp_cardbrk = carta destruida, ...). herramientas/fx_md/sim_fx.py
// simula esos sistemas de particulas con SUS texturas y parametros y los hornea a flipbook (fx.h, assets/fx/md/).
// El DADO y la MONEDA son mallas 3D de Master Duel (dado_md.glb, moneda_md.glb) con SUS curvas de animacion
// (dice_animation2, coin02_anim3.9: dado_md.txt / moneda_md.txt), aqui reproducidas y con el resultado a la vista.
//  - Tabla efectos_cartas.txt (assets/ui): id_de_carta|efecto (o passcode|efecto). Sin linea: la activacion generica
//    de magia (verde) o de trampa (rosa) segun cartas_classic.txt.
//  - Sin estado de reglas: solo la animacion (el motor de efectos de reglas es otra etapa).
#pragma once

#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "efectos_logica.h"   // (rama classic-probador: la logica, sin Vulkan)
#include "fx.h"
#include "idioma.h"   // rama classic-pulido: "Dado: N" / "Moneda: cara/cruz" en ES/EN

namespace efc {

// Las curvas del dado / moneda de Master Duel (t px py pz qx qy qz qw a 30 por segundo)
struct Curva {
	std::vector<float> d;   // 8 por muestra
	float fin{ 0.0f };
	bool cargar(const std::string& ruta)
	{
		std::ifstream f(ruta);
		if (!f) return false;
		std::string l;
		while (std::getline(f, l)) {
			if (l.empty() || l[0] == '#') continue;
			std::istringstream s(l);
			float v[8];
			for (float& x : v) s >> x;
			if (!s) continue;
			d.insert(d.end(), v, v + 8);
		}
		if (d.empty()) return false;
		fin = d[d.size() - 8];
		return true;
	}
	bool vacia() const { return d.empty(); }
	void muestra(float t, glm::vec3& p, glm::quat& q) const
	{
		const size_t n = d.size() / 8;
		t = glm::clamp(t, 0.0f, fin);
		size_t i = 0;
		while (i + 1 < n && d[(i + 1) * 8] <= t) i++;
		const size_t j = glm::min(i + 1, n - 1);
		const float t0 = d[i * 8], t1 = d[j * 8];
		const float f = t1 > t0 ? (t - t0) / (t1 - t0) : 0.0f;
		const float* a = &d[i * 8];
		const float* b = &d[j * 8];
		p = glm::mix(glm::vec3(a[1], a[2], a[3]), glm::vec3(b[1], b[2], b[3]), f);
		const glm::quat qa(a[7], a[4], a[5], a[6]), qb(b[7], b[4], b[5], b[6]);
		q = glm::normalize(glm::slerp(qa, qb, f));
	}
};

// el motor de la vista: la logica (efectos_logica.h) + las curvas del dado / moneda y lo que se dibuja (fx.h)
class Motor : public Logica {
public:
	Curva dado, moneda;

	// carpetaUi: efectos_cartas.txt y cartas_classic.txt; carpetaFx: dado_md.txt y moneda_md.txt
	void cargar(const std::string& carpetaUi, const std::string& carpetaFx)
	{
		cargarTabla(carpetaUi);
		const bool d = dado.cargar(carpetaFx + "dado_md.txt");
		const bool m = moneda.cargar(carpetaFx + "moneda_md.txt");
		fprintf(stderr, "[efectos] %d cartas con animacion propia (efectos_cartas.txt), %zu subtipos; dado %s, moneda %s\n", nTabla, subtipo.size(),
		        d ? "si" : "NO", m ? "si" : "NO");
	}

	// Lo que dibuja cada activacion con los flipbooks. centro(z): centro de una zona en el mundo; ocupada(z): hay carta;
	// zonas: los indices de monstruo y de magia de cada lado (0 jugador, 1 rival)
	void pedir(fx::Flipbooks& fb, const std::function<glm::vec3(int)>& centro, const std::function<bool(int)>& ocupada,
	           const std::vector<int> monstruos[2], const std::vector<int> magias[2], const int mazos[2]) const
	{
		using fx::Fx;
		const glm::vec3 arriba(0.0f, 1.0f, 0.0f);
		for (const Activo& a : activos) {
			const float t = a.t;
			const glm::vec3 c = a.zona >= 0 ? centro(a.zona) : glm::vec3(0.0f);
			const int yo = a.rival ? 1 : 0, otro = 1 - yo;
			const float fin = duracion(a.receta);
			const float sale = 1.0f - glm::smoothstep(fin - 0.35f, fin, t);   // todo se apaga al final
			// la carta que se activa: el "aplicar efecto" de MD (fxp_bff_apply, la activacion generica), verde la magia, rosa la
			// trampa. Rama classic-ps5-efectos: sin la "Aparicion" (no era de MD)
			const bool trampa = a.receta == Receta::Trampa || a.receta == Receta::FuerzaEspejo || a.receta == Receta::CilindroMagico ||
			                    subtipoDe(a.carta).rfind("trampa", 0) == 0;
			const glm::vec3 tinte = trampa ? glm::vec3(1.9f, 0.55f, 1.35f) : glm::vec3(0.55f, 1.9f, 1.0f);
			fb.pedir(Fx::MdActivar, c + arriba * 0.15f, 3.0f, t, glm::vec4(tinte, sale));
			// (rama classic-ps5-efectos3: la rotura de cada destruida, MD CardBreak, la pone efectos3_juego.h al presentar su
			// destruccion; aca ya no). Rama classic-ps5-efectos: solo lo que MD tiene para la carta (CardRunEffectSetting); las cartas sin CardRunEffect
			// en MD van solo con la activacion generica. Raigeki, Mirror Force, Monster Reborn, Magic Cylinder, Dark Hole (su
			// Black y su sacudida) y Swords: md_cardrun_ui.h / md_efectos_ui.h. La destruccion de MD (CardBreak,
			// fxp_cardbrk_bff) sobre lo que se lleva cada una sigue aca
			switch (a.receta) {
			case Receta::AgujeroNegro: {
				// Ef04342 (el remolino de MD, horneado a flipbook: md_agujero_negro) plano sobre la mesa; traga todo y se cierra
				const glm::vec3 medio(0.0f, 0.0f, 0.0f);
				const float abre = glm::smoothstep(0.2f, 0.7f, t) * (1.0f - glm::smoothstep(2.4f, 3.0f, t));
				fb.pedir(Fx::MdAgujeroNegro, medio + glm::vec3(0.0f, 0.05f, 0.0f), 15.0f * glm::max(abre, 0.01f), fmodf(glm::max(t - 0.2f, 0.0f), 2.1f),
				         glm::vec4(1.0f, 1.0f, 1.0f, abre));
				break;
			}
			case Receta::Tifon: {   // Ef04909 de MD (runTiming 3, sobre la carta elegida): el tornado horneado de MD
				int z = a.objetivo >= 0 ? a.objetivo : primeraOcupada(magias[otro], ocupada);
				if (z < 0) z = magias[otro][2];
				fb.pedir(Fx::MdTornado, centro(z), 6.5f, t - 0.25f, glm::vec4(1.0f, 1.0f, 1.0f, sale), arriba);
				break;
			}
			case Receta::Plumero:   // Ef04678 de MD (plumas doradas, horneado a flipbook: md_plumero)
				fb.pedir(Fx::MdPlumero, glm::mix(centro(monstruos[otro][2]), centro(magias[otro][2]), 0.5f) + arriba * 2.0f, 13.0f, t, glm::vec4(1.3f, 1.2f, 0.8f, sale));
				break;
			case Receta::Dado:
			case Receta::Moneda:
				// el objeto 3D de MD (dado / moneda) lo dibuja yugireload.cpp (matrizObjeto). Rama classic-ps5-efectos: sin el
				// polvo al caer (no era de MD)
				(void)mazos;
				break;
			default: break;
			}
		}
	}

	// matriz del dado o la moneda de una activacion (false si no le toca). base: donde rueda (el centro del lado de
	// quien activa, sobre la mesa); ojo: la camara. Unidades de Master Duel a metros: 0.9 (arista del dado ~1.3 m, la casilla mide ~3.8 m)
	bool matrizObjeto(const Activo& a, glm::vec3 base, glm::vec3 ojo, glm::mat4& m) const
	{
		const bool esDado = a.receta == Receta::Dado;
		if (!esDado && a.receta != Receta::Moneda) return false;
		const Curva& cv = esDado ? dado : moneda;
		if (cv.vacia()) return false;
		const float u = 0.9f;
		const float t = a.t;
		glm::vec3 p;
		glm::quat q;
		cv.muestra(glm::min(t, cv.fin), p, q);
		p.z = glm::min(p.z, 2.5f);   // al final Master Duel lo acerca a su camara: aqui solo un poco
		// marco: Y arriba, Z hacia la camara (en el plano de la mesa)
		glm::vec3 zc = ojo - base;
		zc.y = 0.0f;
		zc = glm::length(zc) > 1e-3f ? glm::normalize(zc) : glm::vec3(0.0f, 0.0f, 1.0f);
		const glm::vec3 yc(0.0f, 1.0f, 0.0f);
		const glm::vec3 xc = glm::normalize(glm::cross(yc, zc));
		glm::mat4 marco(1.0f);
		marco[0] = glm::vec4(xc, 0.0f);
		marco[1] = glm::vec4(yc, 0.0f);
		marco[2] = glm::vec4(zc, 0.0f);
		marco[3] = glm::vec4(base, 1.0f);
		// resultado: la cara pedida queda arriba (dado) o mirando a la camara (moneda)
		glm::quat res(1.0f, 0.0f, 0.0f, 0.0f);
		if (esDado) {
			// caras de la malla (dado_md.glb, medidas con herramientas/fx_md/blender/caras_glb.py): 1 +Y (la estrella:
			// arriba en reposo, como en Master Duel), 6 -Y, 2 +Z, 5 -Z, 3 +X, 4 -X
			static const glm::vec3 N[7] = { {}, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, -1 }, { 0, -1, 0 } };
			const glm::vec3 n = N[glm::clamp(a.resultado, 1, 6)];
			const glm::vec3 eje = glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f));
			if (glm::length(eje) > 1e-4f) {
				res = glm::angleAxis(acosf(glm::clamp(n.y, -1.0f, 1.0f)), glm::normalize(eje));
			} else if (n.y < 0.0f) {
				res = glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
			}
		} else if (a.resultado == 1) {
			res = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));   // cruz
		}
		// al final: queda quieto mostrando el resultado (el dado se inclina hacia la camara) y se encoge
		float incl = 0.0f, esc = 1.0f;
		if (t > cv.fin) {
			incl = esDado ? glm::smoothstep(cv.fin, cv.fin + 0.35f, t) * glm::radians(55.0f) : 0.0f;
			esc = 1.0f - glm::smoothstep(duracion(a.receta) - 0.4f, duracion(a.receta), t);
		}
		const float alto = esDado ? 0.731f : 0.55f;   // la malla tiene el centro en el medio: que apoye en la mesa
		m = marco * glm::scale(glm::mat4(1.0f), glm::vec3(u * esc)) * glm::translate(glm::mat4(1.0f), p + glm::vec3(0.0f, alto, 0.0f)) *
		    glm::mat4_cast(glm::angleAxis(incl, glm::vec3(1.0f, 0.0f, 0.0f)) * q * res);
		return true;
	}

	// el resultado a la vista en el aviso de la interfaz, al terminar de rodar
	bool resultadoListo(const Activo& a, float dt, std::string& texto) const
	{
		const bool esDado = a.receta == Receta::Dado;
		if (!esDado && a.receta != Receta::Moneda) return false;
		const float fin = (esDado ? dado.fin : moneda.fin) - 0.6f;
		if (!(a.t >= fin && a.t - dt < fin)) return false;
		texto = esDado ? std::string(idioma::tr("Dado: ")) + std::to_string(a.resultado) : std::string(idioma::tr(a.resultado == 0 ? "Moneda: cara" : "Moneda: cruz"));
		return true;
	}

private:
	static int primeraOcupada(const std::vector<int>& zs, const std::function<bool(int)>& ocupada)
	{
		for (int z : zs) {
			if (ocupada(z)) return z;
		}
		return -1;
	}
};

}   // namespace efc
