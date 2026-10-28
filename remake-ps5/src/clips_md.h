/*
 * Yugioh Classic Remaster - reproductor generico de los datos de animacion de Yu-Gi-Oh! Master Duel (rama classic-md-fases).
 *
 * Pedido del usuario: traer de Master Duel (la copia de Steam del usuario) lo visual del duelo TAL CUAL, no rehacerlo
 * a ojo. El codigo C# de MD depende de Unity, asi que se traen sus DATOS (herramientas/md_fases/exportar_clips.py, que
 * los saca con UnityPy de masterduel_Data/data.unity3d y los deja en <assets>/md_ui/, fuera del repo) y aca se
 * reproducen igual que Unity:
 *  - .mdtl: un Timeline (PlayableDirector). Pistas de animacion con las curvas EXACTAS de cada AnimationClip de
 *    Mecanim: el StreamedClip guarda por cada clave un tramo cubico (t, a, b, c, d) y Unity evalua
 *    v = ((a*dt + b)*dt + c)*dt + d con dt = tiempo - t del ultimo tramo que empezo; el DenseClip son muestras a
 *    sr por segundo (interpoladas linealmente) y el ConstantClip, constantes. Cada pista tiene su objeto (ruta dentro
 *    del prefab, hermanos repetidos con #2...), inicio, duracion (-1 = clip infinito de la pista), clipIn, escala de
 *    tiempo y extrapolacion antes/despues (0 nada, 1 mantener). Mas las pistas de activacion (el objeto se ve entre
 *    inicio e inicio+dur), los sonidos (SoundPlayableAsset.startLabel) y los eventos (EventPlayableAsset) con su tiempo.
 *  - .mdui: un prefab de uGUI: RectTransform de cada objeto (anclas, posicion, tamanno, pivote, escala), sus Image
 *    (sprite y color) y sus Tween de MD (TweenAlpha/Position/Scale/Size/Color: label, easing, style, duracion,
 *    demora, from y to), que se evaluan con easing() y estilo() como YgomSystem.UI.Tween.
 *  - md_ui.png + md_ui_atlas.txt: los sprites (nombre x y w h).
 * Sin Vulkan: lo usa la interfaz (fases_md_ui.h) y lo puede usar cualquier otra rama (classic-md-ritmo).
 * Atributos: los de Unity (pos.x/y/z, rot.x..w, scale.x/y/z, euler.x/y/z, m_Color.a, m_fontColor.a, m_SizeDelta.x...).
 */
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace clipsmd {

inline std::vector<std::string> campos(const std::string& linea)
{
	std::string l = linea;
	while (!l.empty() && (l.back() == '\r' || l.back() == '\n')) l.pop_back();   // (archivos escritos en Windows: CRLF)
	std::vector<std::string> r;
	size_t a = 0;
	while (a <= l.size()) {
		size_t b = l.find('\t', a);
		if (b == std::string::npos) b = l.size();
		r.push_back(l.substr(a, b - a));
		a = b + 1;
	}
	return r;
}
inline float num(const std::vector<std::string>& c, size_t i, float def = 0.0f) { return i < c.size() ? (float)atof(c[i].c_str()) : def; }

// ---------------------------------------------------------------------------------------------- curvas de Mecanim
struct Curva {
	std::string obj, attr;
	char tipo{ 'c' };                         // s: tramos cubicos, d: muestras, c: constante
	std::vector<std::array<float, 5>> tramos; // t, a, b, c, d
	float t0{ 0.0f }, sr{ 30.0f };
	std::vector<float> muestras;
	float constante{ 0.0f };
	float valor(float t) const
	{
		if (tipo == 'c') return constante;
		if (tipo == 'd') {
			if (muestras.empty()) return 0.0f;
			const float f = std::max(0.0f, (t - t0) * sr);
			const size_t i = (size_t)f;
			if (i + 1 >= muestras.size()) return muestras.back();
			const float u = f - (float)i;
			return muestras[i] + (muestras[i + 1] - muestras[i]) * u;
		}
		if (tramos.empty()) return 0.0f;
		// el ultimo tramo que empezo antes de t (el primero es el de -infinito de Unity)
		size_t k = 0;
		for (size_t i = 0; i < tramos.size(); i++) {
			if (tramos[i][0] <= t) k = i;
			else break;
		}
		const std::array<float, 5>& s = tramos[k];
		const float dt = s[0] < -1e30f ? 0.0f : t - s[0];
		return ((s[1] * dt + s[2]) * dt + s[3]) * dt + s[4];
	}
};

// una pista de animacion del Timeline (un clip puesto en el tiempo)
struct Pista {
	std::string obj;
	float inicio{ 0.0f }, dur{ -1.0f }, clipIn{ 0.0f }, escala{ 1.0f };
	int pre{ 1 }, post{ 1 };
	std::vector<Curva> curvas;
	// el tiempo local del clip a "t" del Timeline, o false si la pista no anima en ese momento
	bool local(float t, float& l) const
	{
		if (dur < 0.0f) {
			l = std::max(t, 0.0f);
			return true;
		}
		if (t < inicio) {
			if (pre == 0) return false;
			l = clipIn;
			return true;
		}
		if (t > inicio + dur) {
			if (post == 0) return false;
			l = clipIn + dur * escala;
			return true;
		}
		l = clipIn + (t - inicio) * escala;
		return true;
	}
};
struct Activa { std::string obj; float inicio, dur; };
struct Marca { std::string etiqueta; float t; };

inline bool mismoObjeto(const std::string& ruta, const std::string& q)
{
	if (ruta == q) return true;
	return ruta.size() > q.size() && ruta[ruta.size() - q.size() - 1] == '/' && ruta.compare(ruta.size() - q.size(), q.size(), q) == 0;
}

struct Timeline {
	std::string nombre;
	float duracion{ 0.0f };
	std::vector<Pista> pistas;
	std::vector<Activa> activas;
	std::vector<Marca> sonidos, eventos;
	bool hay() const { return !pistas.empty() || !activas.empty(); }

	bool cargar(const std::string& ruta)
	{
		std::ifstream f(ruta);
		if (!f) return false;
		*this = Timeline{};
		std::string l;
		Pista* p = nullptr;
		while (std::getline(f, l)) {
			if (l.empty() || l[0] == '#') continue;
			std::vector<std::string> c = campos(l);
			if (c[0] == "timeline") {
				nombre = c.size() > 1 ? c[1] : "";
				duracion = num(c, 2);
			} else if (c[0] == "pista" && c.size() >= 8) {
				pistas.push_back(Pista{ c[1], num(c, 2), num(c, 3), num(c, 4), num(c, 5), (int)num(c, 6), (int)num(c, 7), {} });
				p = &pistas.back();
			} else if (c[0] == "curva" && p && c.size() >= 5) {
				Curva k;
				k.obj = c[1];
				k.attr = c[2];
				k.tipo = c[3].empty() ? 'c' : c[3][0];
				if (k.tipo == 'c') {
					k.constante = num(c, 4);
				} else if (k.tipo == 's') {
					const int n = (int)num(c, 4);
					for (int i = 0; i < n && std::getline(f, l); i++) {
						const std::vector<std::string> v = campos(l);
						k.tramos.push_back({ num(v, 0), num(v, 1), num(v, 2), num(v, 3), num(v, 4) });
					}
				} else if (k.tipo == 'd') {
					k.t0 = num(c, 5);
					k.sr = num(c, 6, 30.0f);
					if (std::getline(f, l)) {
						for (const std::string& v : campos(l)) k.muestras.push_back((float)atof(v.c_str()));
					}
				}
				p->curvas.push_back(std::move(k));
			} else if (c[0] == "activa" && c.size() >= 4) {
				activas.push_back({ c[1], num(c, 2), num(c, 3) });
			} else if (c[0] == "sonido" && c.size() >= 3) {
				sonidos.push_back({ c[1], num(c, 2) });
			} else if (c[0] == "evento" && c.size() >= 3) {
				eventos.push_back({ c[1], num(c, 2) });
			}
		}
		return hay();
	}

	// el valor del atributo del objeto a "t" s (la ultima pista que lo anima manda, como en Unity); def si nadie lo anima
	// Con varios clips del mismo objeto y atributo (en Unity, varios clips en una pista) manda el que esta sonando (el
	// ultimo que empezo); si ninguno, el ultimo que termino (se mantiene: post-extrapolacion) y si no, el primero que
	// va a empezar (pre-extrapolacion)
	float valor(const std::string& obj, const char* attr, float t, float def) const
	{
		const Curva* mejor = nullptr;
		float lMejor = 0.0f;
		int rango = -1;      // 2 sonando, 1 ya termino, 0 todavia no empieza
		float clave = 0.0f;  // para desempatar dentro del rango
		for (const Pista& p : pistas) {
			float l;
			if (!p.local(t, l)) continue;
			const bool infinito = p.dur < 0.0f;
			const int r = infinito || (t >= p.inicio && t <= p.inicio + p.dur) ? 2 : t > p.inicio + p.dur ? 1 : 0;
			const float c = r == 2 ? p.inicio : r == 1 ? p.inicio + p.dur : -p.inicio;
			for (const Curva& k : p.curvas) {
				if (k.attr != attr || !mismoObjeto(k.obj, obj)) continue;
				if (r > rango || (r == rango && c >= clave)) {
					mejor = &k;
					lMejor = l;
					rango = r;
					clave = c;
				}
			}
		}
		return mejor ? mejor->valor(lMejor) : def;
	}
	// si el objeto se ve a "t" (pistas de activacion; def si no tiene)
	bool activo(const std::string& obj, float t, bool def = true) const
	{
		bool hayPista = false;
		for (const Activa& a : activas) {
			if (!mismoObjeto(a.obj, obj)) continue;
			hayPista = true;
			if (t >= a.inicio && t < a.inicio + a.dur) return true;
		}
		return hayPista ? false : def;
	}
	// el tiempo de un evento o sonido (-1 si no esta)
	float marca(const char* etiqueta) const
	{
		for (const Marca& m : eventos)
			if (m.etiqueta == etiqueta) return m.t;
		for (const Marca& m : sonidos)
			if (m.etiqueta == etiqueta) return m.t;
		return -1.0f;
	}
	// el inicio de la activacion de un objeto (-1 si no tiene)
	float inicioActiva(const std::string& obj) const
	{
		for (const Activa& a : activas)
			if (mismoObjeto(a.obj, obj)) return a.inicio;
		return -1.0f;
	}
};

// ---------------------------------------------------------------------------------------------- AnimationCurve suelta
// Una AnimationCurve de Unity (las de los ScriptableObject de MD: BezierMotionSetting, CardMoveMotionSetting...): claves
// (time, value, inSlope, outSlope, inWeight, outWeight, weightedMode) evaluadas como Unity: Hermite entre dos claves con
// las pendientes por el ancho del tramo; si una de las dos usa peso (weightedMode: 1 entrada, 2 salida, 3 ambos), el
// tramo es una Bezier cubica en (tiempo, valor) con los manejadores a peso*ancho, resuelta en el tiempo con Newton.
// Fuera de las claves: pre/post = WrapMode de Unity (0 Default y 1 Once y 8 ClampForever: se mantiene; 2 Loop; 4 PingPong).
// JSON (classic-md-ritmo): {"keys":[[t,v,inSlope,outSlope,inW,outW,modo],...],"pre":wrap,"post":wrap}
struct Clave { float t{ 0 }, v{ 0 }, entra{ 0 }, sale{ 0 }, pesoEntra{ 1.0f / 3.0f }, pesoSale{ 1.0f / 3.0f }; int modo{ 0 }; };
struct CurvaUnity {
	std::vector<Clave> claves;
	int pre{ 1 }, post{ 1 };
	bool hay() const { return !claves.empty(); }
	// de un objeto JSON con "keys", "pre" y "post" (los numeros se leen en orden; sin dependencias)
	bool desdeJSON(const std::string& j)
	{
		claves.clear();
		auto entero = [&](const char* k, int def) {
			const size_t p = j.find(std::string("\"") + k + "\"");
			if (p == std::string::npos) return def;
			const size_t c = j.find(':', p);
			return c == std::string::npos ? def : atoi(j.c_str() + c + 1);
		};
		pre = entero("pre", 1);
		post = entero("post", 1);
		size_t p = j.find("\"keys\"");
		if (p == std::string::npos) return false;
		p = j.find('[', p);
		if (p == std::string::npos) return false;
		size_t q = p + 1;
		while (true) {
			const size_t a = j.find('[', q), fin = j.find(']', q);
			if (a == std::string::npos || (fin != std::string::npos && fin < a)) break;   // (el ] de "keys")
			const size_t b = j.find(']', a);
			if (b == std::string::npos) break;
			std::vector<float> v;
			const char* c = j.c_str() + a + 1;
			const char* e = j.c_str() + b;
			while (c < e) {
				char* sig = nullptr;
				const float x = strtof(c, &sig);
				if (sig == c) { c++; continue; }
				v.push_back(x);
				c = sig;
			}
			Clave k;
			if (v.size() >= 2) k.t = v[0], k.v = v[1];
			if (v.size() >= 4) k.entra = v[2], k.sale = v[3];
			if (v.size() >= 6) k.pesoEntra = v[4], k.pesoSale = v[5];
			if (v.size() >= 7) k.modo = (int)v[6];
			claves.push_back(k);
			q = b + 1;
		}
		return hay();
	}
	static float hermite(const Clave& a, const Clave& b, float t)
	{
		const float dt = b.t - a.t;
		if (dt <= 0.0f || !std::isfinite(a.sale) || !std::isfinite(b.entra)) return a.v;   // (pendiente infinita: escalon)
		const float u = (t - a.t) / dt, u2 = u * u, u3 = u2 * u;
		return (2 * u3 - 3 * u2 + 1) * a.v + (u3 - 2 * u2 + u) * a.sale * dt + (-2 * u3 + 3 * u2) * b.v + (u3 - u2) * b.entra * dt;
	}
	static float bezier(const Clave& a, const Clave& b, float t)
	{
		const float dt = b.t - a.t;
		const float wa = (a.modo & 2) ? a.pesoSale : 1.0f / 3.0f, wb = (b.modo & 1) ? b.pesoEntra : 1.0f / 3.0f;
		const float x1 = wa, x2 = 1.0f - wb;   // (tiempos normalizados de los manejadores)
		const float y0 = a.v, y1 = a.v + a.sale * wa * dt, y2 = b.v - b.entra * wb * dt, y3 = b.v;
		const float x = (t - a.t) / dt;
		float u = x;
		for (int i = 0; i < 12; i++) {   // Newton: el parametro u cuyo tiempo es x
			const float m = 1.0f - u;
			const float fx = 3 * m * m * u * x1 + 3 * m * u * u * x2 + u * u * u - x;
			const float d = 3 * m * m * x1 + 6 * m * u * (x2 - x1) + 3 * u * u * (1.0f - x2);
			if (fabsf(d) < 1e-6f) break;
			u = std::max(0.0f, std::min(1.0f, u - fx / d));
		}
		const float m = 1.0f - u;
		return m * m * m * y0 + 3 * m * m * u * y1 + 3 * m * u * u * y2 + u * u * u * y3;
	}
	float valor(float t) const
	{
		if (claves.empty()) return 0.0f;
		const float t0 = claves.front().t, t1 = claves.back().t, L = t1 - t0;
		auto envolver = [&](float x, int w) {
			if (L <= 0.0f) return x;
			if (w == 2) return t0 + (x - t0) - floorf((x - t0) / L) * L;
			if (w == 4) {
				const float m = fmodf(fabsf(x - t0), 2.0f * L);
				return t0 + (m <= L ? m : 2.0f * L - m);
			}
			return std::max(t0, std::min(x, t1));
		};
		if (t < t0) t = envolver(t, pre);
		if (t > t1) t = envolver(t, post);
		if (t <= t0) return claves.front().v;
		if (t >= t1) return claves.back().v;
		size_t i = 0;
		while (i + 1 < claves.size() && claves[i + 1].t < t) i++;
		const Clave &a = claves[i], &b = claves[i + 1];
		return ((a.modo & 2) || (b.modo & 1)) ? bezier(a, b, t) : hermite(a, b, t);
	}
	float duracion() const { return claves.empty() ? 0.0f : claves.back().t - claves.front().t; }
};

// ---------------------------------------------------------------------------------------------- Tween de MD (uGUI)
// YgomSystem.UI.Tween.Easing y Tween.Style (dump de Il2CppDumper); formulas de Tween.EasingValue (docs/md_fases.md)
enum Easing { LINEAR = 0, CUBIC_IN, CUBIC_OUT, CUBIC_IN_OUT, BACK_IN, BACK_OUT, BACK_IN_OUT, BOUNCE_IN, BOUNCE_OUT, BOUNCE_IN_OUT, CUSTOMIZE,
              QUART_IN, QUART_OUT, QUART_IN_OUT };
enum Estilo { ONCE = 0, LOOP, PING_PONG, PING_PONG_LOOP, SYNC_LOOP, SYNC_PING_PONG_LOOP };

inline float rebote(float k)
{
	if (k < 1.0f / 2.75f) return 7.5625f * k * k;
	if (k < 2.0f / 2.75f) { const float u = k - 1.5f / 2.75f; return 7.5625f * u * u + 0.75f; }
	if (k < 2.5f / 2.75f) { const float u = k - 2.25f / 2.75f; return 7.5625f * u * u + 0.9375f; }
	const float u = k - 2.625f / 2.75f;
	return 7.5625f * u * u + 0.984375f;
}
inline float easing(float k, int e)
{
	k = std::max(0.0f, std::min(k, 1.0f));
	const float s = 1.70158f, s2 = s * 1.525f;
	switch (e) {
	case CUBIC_IN: return k * k * k;
	case CUBIC_OUT: { const float u = k - 1.0f; return u * u * u + 1.0f; }
	case CUBIC_IN_OUT: { if (k < 0.5f) return 4.0f * k * k * k; const float u = 2.0f * k - 2.0f; return 0.5f * u * u * u + 1.0f; }
	case BACK_IN: return k * k * ((s + 1.0f) * k - s);
	case BACK_OUT: { const float u = k - 1.0f; return u * u * ((s + 1.0f) * u + s) + 1.0f; }
	case BACK_IN_OUT: {
		float u = k * 2.0f;
		if (u < 1.0f) return 0.5f * (u * u * ((s2 + 1.0f) * u - s2));
		u -= 2.0f;
		return 0.5f * (u * u * ((s2 + 1.0f) * u + s2) + 2.0f);
	}
	case BOUNCE_IN: return 1.0f - rebote(1.0f - k);
	case BOUNCE_OUT: return rebote(k);
	case BOUNCE_IN_OUT: return k < 0.5f ? (1.0f - rebote(1.0f - 2.0f * k)) * 0.5f : rebote(2.0f * k - 1.0f) * 0.5f + 0.5f;
	case QUART_IN: return k * k * k * k;
	case QUART_OUT: { const float u = k - 1.0f; return 1.0f - u * u * u * u; }
	case QUART_IN_OUT: { if (k < 0.5f) return 8.0f * k * k * k * k; const float u = k - 1.0f; return 1.0f - 8.0f * u * u * u * u; }
	default: return k;   // LINEAR (y CUSTOMIZE sin su AnimationCurve)
	}
}
// el avance 0..1 de un tween a "t" s desde que arranco (con su demora y su estilo)
inline float estilo(float t, float dur, float demora, int est)
{
	t -= demora;
	if (t <= 0.0f) return 0.0f;
	if (dur <= 0.0f) return 1.0f;
	const float k = t / dur;
	switch (est) {
	case LOOP:
	case SYNC_LOOP: return k - floorf(k);
	case PING_PONG: return k >= 2.0f ? 0.0f : k <= 1.0f ? k : 2.0f - k;
	case PING_PONG_LOOP:
	case SYNC_PING_PONG_LOOP: { const float m = fmodf(k, 2.0f); return m <= 1.0f ? m : 2.0f - m; }
	default: return std::min(k, 1.0f);
	}
}

struct Tween {
	std::string obj, tipo, label;
	int easing{ 0 }, estilo{ 0 };
	float dur{ 0.0f }, demora{ 0.0f };
	std::vector<float> desde, hasta;
	CurvaUnity curva;                                         // (easing CUSTOMIZE: la AnimationCurve del tween)
	std::vector<std::pair<std::string, std::string>> alTerminar;   // la cadena: (metodo:etiqueta, objeto) que arranca al terminar
	// el avance 0..1 ya con su easing (o su curva propia) a "t" s desde que arranco
	float avance(float t) const
	{
		const float k = clipsmd::estilo(t, dur, demora, estilo);
		return easing == CUSTOMIZE && curva.hay() ? curva.valor(k) : clipsmd::easing(k, easing);
	}
	// componente c (0 x / alfa, 1 y, 2 z...) a "t" s desde que arranco; los Tween*To no tienen "desde": se pasa el actual
	float valor(float t, int c = 0, float desdeActual = 0.0f) const
	{
		const float a = c < (int)desde.size() ? desde[(size_t)c] : desdeActual, b = c < (int)hasta.size() ? hasta[(size_t)c] : a;
		return a + (b - a) * avance(t);
	}
	float total() const { return demora + dur; }
	bool termino(float t) const { return (estilo == ONCE || estilo == PING_PONG) && t >= demora + dur * (estilo == PING_PONG ? 2.0f : 1.0f); }
};
struct Rect { float anclaMin[2]{}, anclaMax[2]{}, pos[2]{}, tam[2]{}, pivote[2]{ 0.5f, 0.5f }, escala[2]{ 1.0f, 1.0f }; };
struct Imagen { std::string sprite; float color[4]{ 1.0f, 1.0f, 1.0f, 1.0f }; };

struct Prefab {
	std::map<std::string, Rect> rects;
	std::map<std::string, Imagen> imagenes;
	std::vector<Tween> tweens;
	std::map<std::string, std::string> valores;   // campos serializados de los scripts (DuelLP.countTime...)
	bool hay() const { return !rects.empty(); }
	float campo(const char* k, float def) const
	{
		auto it = valores.find(k);
		return it == valores.end() ? def : (float)atof(it->second.c_str());
	}
	// cuando arranca "label" de "obj" siguiendo la cadena de al_terminar desde (obj0, label0) (-1 si no esta en la cadena)
	float inicioEnCadena(const std::string& obj0, const char* label0, const std::string& obj, const char* label) const
	{
		float t = 0.0f;
		const Tween* w = tween(obj0, label0);
		for (int paso = 0; w && paso < 16; paso++) {
			if (w->label == label && mismoObjeto(w->obj, obj)) return t;
			const float fin = t + w->total();
			const Tween* sig = nullptr;
			for (const auto& c : w->alTerminar) {
				if (c.first.rfind("PlayLabel:", 0) != 0) continue;
				const std::string lab = c.first.substr(10);
				const std::string& o = c.second == "?" || c.second.empty() ? w->obj : c.second;
				if ((sig = tween(o, lab.c_str()))) break;
			}
			t = fin;
			w = sig;
		}
		return -1.0f;
	}
	bool cargar(const std::string& ruta)
	{
		std::ifstream f(ruta);
		if (!f) return false;
		*this = Prefab{};
		std::string l;
		while (std::getline(f, l)) {
			if (l.empty() || l[0] == '#') continue;
			const std::vector<std::string> c = campos(l);
			if (c[0] == "rect" && c.size() >= 14) {
				Rect r;
				for (int i = 0; i < 2; i++) {
					r.anclaMin[i] = num(c, 2 + (size_t)i);
					r.anclaMax[i] = num(c, 4 + (size_t)i);
					r.pos[i] = num(c, 6 + (size_t)i);
					r.tam[i] = num(c, 8 + (size_t)i);
					r.pivote[i] = num(c, 10 + (size_t)i);
					r.escala[i] = num(c, 12 + (size_t)i);
				}
				rects[c[1]] = r;
			} else if (c[0] == "imagen" && c.size() >= 7) {
				Imagen im;
				im.sprite = c[2];
				for (int i = 0; i < 4; i++) im.color[i] = num(c, 3 + (size_t)i);
				imagenes[c[1]] = im;
			} else if (c[0] == "tween" && c.size() >= 10) {
				Tween w;
				w.obj = c[1];
				w.tipo = c[2];
				w.label = c[3];
				w.easing = (int)num(c, 4);
				w.estilo = (int)num(c, 5);
				w.dur = num(c, 6);
				w.demora = num(c, 7);
				std::istringstream d(c[8]), h(c[9]);
				float v;
				while (d >> v) w.desde.push_back(v);
				while (h >> v) w.hasta.push_back(v);
				tweens.push_back(std::move(w));
			} else if (c[0] == "curva_tween" && c.size() >= 2 && !tweens.empty()) {
				// "t v entra sale pesoEntra pesoSale modo;..." -> el JSON de CurvaUnity
				std::string j = "{\"keys\":[";
				std::istringstream claves(c[1]);
				std::string k;
				bool primera = true;
				while (std::getline(claves, k, ';')) {
					std::istringstream n(k);
					std::string x, js;
					while (n >> x) js += (js.empty() ? "" : ",") + x;
					j += (primera ? "[" : ",[") + js + "]";
					primera = false;
				}
				j += "],\"pre\":1,\"post\":1}";
				tweens.back().curva.desdeJSON(j);
			} else if (c[0] == "al_terminar" && c.size() >= 4 && !tweens.empty()) {
				tweens.back().alTerminar.push_back({ c[1] + ":" + c[2], c[3] });
			} else if (c[0] == "campo" && c.size() >= 4) {
				valores[c[2]] = c[3];
			}
		}
		return hay();
	}
	const Tween* tween(const std::string& obj, const char* label, const char* tipo = nullptr) const
	{
		for (const Tween& w : tweens)
			if (w.label == label && mismoObjeto(w.obj, obj) && (!tipo || w.tipo == tipo) && (tipo || w.tipo != "TweenEvent")) return &w;
		for (const Tween& w : tweens)
			if (w.label == label && mismoObjeto(w.obj, obj) && (!tipo || w.tipo == tipo)) return &w;
		return nullptr;
	}
	const Rect* rect(const std::string& obj) const
	{
		for (const auto& kv : rects)
			if (mismoObjeto(kv.first, obj)) return &kv.second;
		return nullptr;
	}
	const Imagen* imagen(const std::string& obj) const
	{
		for (const auto& kv : imagenes)
			if (mismoObjeto(kv.first, obj)) return &kv.second;
		return nullptr;
	}
};

// los sprites del atlas (en pixeles de md_ui.png)
struct Sprite { float x, y, w, h; };
struct Atlas {
	std::map<std::string, Sprite> sprites;
	float ancho{ 1.0f }, alto{ 1.0f };
	bool cargar(const std::string& ruta, float w, float h)
	{
		std::ifstream f(ruta);
		if (!f) return false;
		ancho = w;
		alto = h;
		std::string l;
		while (std::getline(f, l)) {
			if (l.empty() || l[0] == '#') continue;
			const std::vector<std::string> c = campos(l);
			if (c.size() >= 5) sprites[c[0]] = { num(c, 1), num(c, 2), num(c, 3), num(c, 4) };
		}
		return !sprites.empty();
	}
	const Sprite* sprite(const std::string& n) const
	{
		auto it = sprites.find(n);
		return it == sprites.end() ? nullptr : &it->second;
	}
};

// todo lo de md_ui/ (se carga una vez: cargar(<assets>/md_ui/))
struct Biblioteca {
	std::string dir;
	std::map<std::string, Timeline> timelines;
	std::map<std::string, Prefab> prefabs;
	Atlas atlas;
	bool cargada{ false };
	void cargar(const std::string& carpeta)
	{
		dir = carpeta;
		cargada = true;
		static const char* const TL[] = { "DuelTurnChange00", "DuelTurnChange01", "DuelTextStart", "DuelTextWin2", "DueltextLose", "DueltextDraw", "DuelDrawPhase_near", "DuelDrawPhase_far", "DuelStanbyPhase_near",
			                              "DuelStanbyPhase_far", "DuelMain01Phase_near", "DuelMain01Phase_far", "DuelBattlePhase_near", "DuelBattlePhase_far",
			                              "DuelMain02Phase_near", "DuelMain02Phase_far", "DuelEndPhase_near", "DuelEndPhase_far" };
		static const char* const UI[] = { "PhaseSelectWindow", "DuelMenuButton", "SurrenderButton", "MenuButtons", "DuelLPNear", "DuelLPFar" };
		int n = 0;
		for (const char* k : TL) n += timelines[k].cargar(carpeta + k + ".mdtl") ? 1 : 0;
		for (const char* k : UI) n += prefabs[k].cargar(carpeta + k + ".mdui") ? 1 : 0;
		fprintf(stderr, "[clips_md] %d de %d timelines/prefabs de Master Duel en %s\n", n, (int)(sizeof(TL) / sizeof(TL[0]) + sizeof(UI) / sizeof(UI[0])), carpeta.c_str());
	}
	const Timeline* timeline(const std::string& n) const
	{
		auto it = timelines.find(n);
		return it == timelines.end() || !it->second.hay() ? nullptr : &it->second;
	}
	const Prefab* prefab(const std::string& n) const
	{
		auto it = prefabs.find(n);
		return it == prefabs.end() || !it->second.hay() ? nullptr : &it->second;
	}
};
inline Biblioteca& biblioteca()
{
	static Biblioteca b;
	return b;
}

}   // namespace clipsmd
