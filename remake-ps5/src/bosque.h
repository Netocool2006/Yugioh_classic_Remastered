// Yugi Reload - carta de campo Bosque (rama campo-bosque; realismo: rama bosque-real): el entorno de bosque que
// aparece al activarse la carta de campo y se retira al destruirla.
//  - Arboles por codigo con ramas de verdad (estilo ez-tree de dgreenheck, MIT: tronco con raices, 2 niveles de
//    ramas que se tuercen y buscan la luz) y follaje de TARJETAS con alfa (ramilletes armados con hojas escaneadas
//    CC0 de ambientCG): haya y su variante alta, abeto de ramas en verticilos, arbusto, y versiones livianas para el
//    anillo lejano. Corteza PBR (Poly Haven, CC0) con normal y rugosidad; normales "de copa" (esfericas) en las
//    tarjetas para que la copa se ilumine como un volumen; oclusion por vertice hacia adentro de la copa.
//  - Modelos escaneados de Poly Haven (CC0) reducidos con Blender (bosque_insumos.py): helecho, rocas con musgo,
//    tocon y tronco caido (.malla). Si faltan, se omiten (o la roca sale por codigo).
//  - Suelo de 58 m con tres texturas de Poly Haven/ambientCG (hojarasca, tierra con ramitas, musgo) mezcladas por
//    ruido, raices con corteza que trepan por el canto de la mesa, hongos por codigo y el cesped de siempre.
//  - Todas las texturas en UN arreglo 2D (24 capas de 1024 con mipmaps; el alfa conserva su cobertura en cada mip
//    para que las hojas no se adelgacen de lejos) en un set propio (set 1).
//  - instancias (lugar, escala, giro, semilla) repartidas alrededor de la mesa: nada alto delante de la camara de
//    juego; ademas el fragmento abre (tramado) lo que quede entre la camara y el tablero en cualquier camara;
//  - cesped (bosque_cesped.vert), fondo de bosque profundo (bosque_cielo.*) y particulas (bosque_particula.*).
// Coste: YUGI_CAMPO_MEDIR=1 lo imprime al cargar (triangulos por malla, dibujos). Fuentes y licencias:
// third_party/LICENCIAS_bosque.md. El juego (yugireload.cpp) maneja la carta y la onda; aqui se preparan y dibujan.
#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "stb_image.h"   // la implementacion ya esta en la base (VulkanglTFModel.cpp)

namespace bosque {

struct Vertice {
	glm::vec4 pos;      // xyz; w: material (0 corteza/madera, 1 follaje con alfa, 2 hongo, 3 roca, 5 suelo, 6 raiz)
	glm::vec3 normal;
	glm::vec4 color;    // rgb: tinte (lineal, ~1) o albedo sRGB (hongo); a: flexibilidad al viento (raices: recorrido)
	glm::vec4 uv;       // uv, capa de color del arreglo (la normal es capa + 1; < 0 sin textura), oclusion 0..1
};
struct Instancia {
	glm::vec4 a;        // lugar (x, y, z), escala
	glm::vec4 b;        // giro (rad), semilla 0..1, estirar en altura, cuanto se hunde al retirarse (m)
};

enum TipoMalla {
	ARBOL_A = 0, ARBOL_B, PINO, ARBOL_LEJOS, PINO_LEJOS, ARBUSTO, HELECHO, HELECHO_B, HONGOS, ROCA, ROCA_B, TOCON, TRONCO,
	RAICES, SUELO, NUM_MALLAS
};
enum Material { CORTEZA = 0, HOJA = 1, HONGO = 2, PIEDRA = 3, TIERRA = 5, RAIZ = 6 };
// capas del arreglo de texturas (color y su normal a continuacion), archivos <assets>/bosque/<nombre>_color.png y
// _normal.png (los arma bosque_insumos.py)
enum Capa {
	C_CORTEZA = 0, C_CORTEZA_PINO = 2, C_HOJAS = 4, C_ABETO = 6, C_ARBUSTO = 8, C_HELECHO = 10, C_SUELO = 12, C_SUELO2 = 14,
	C_MUSGO = 16, C_ROCA = 18, C_TOCON = 20, C_TRONCO = 22, NUM_CAPAS = 24
};
constexpr const char* NOMBRE_CAPA[NUM_CAPAS / 2] = { "corteza", "corteza_pino", "hojas", "abeto", "arbusto", "helecho",
                                                     "suelo", "suelo2", "musgo", "roca", "tocon", "tronco" };
constexpr uint32_t TAM_TEXTURA = 1024;
enum TipoParticula { HOJAS = 0, LUCIERNAGAS, POLEN, NIEBLA, RAYOS, NUM_PARTICULAS };
constexpr uint32_t BRIZNAS = 36000;
constexpr uint32_t CUANTAS_PARTICULAS[NUM_PARTICULAS] = { 260, 120, 280, 30, 7 };
constexpr float SUELO_Y = -0.30f;   // el suelo del bosque: la base de la mesa queda hundida en el musgo

// ------------------------------------------------------------------------------------------------- azar y ruido
struct Azar {
	uint32_t s;
	explicit Azar(uint32_t semilla) : s(semilla * 747796405u + 2891336453u) {}
	float operator()()
	{
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return (float)(s & 0xffffffu) / 16777216.0f;
	}
	float entre(float a, float b) { return a + (b - a) * (*this)(); }
	glm::vec3 esfera()   // direccion al azar
	{
		const float z = entre(-1.0f, 1.0f), a = entre(0.0f, 6.2831853f), r = std::sqrt(std::max(0.0f, 1.0f - z * z));
		return glm::vec3(r * std::cos(a), z, r * std::sin(a));
	}
};

inline float hash3(int x, int y, int z, uint32_t semilla)
{
	uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + (uint32_t)z * 2147483647u + semilla * 2654435761u;
	h = (h ^ (h >> 13)) * 1274126177u;
	return (float)((h ^ (h >> 16)) & 0xffffffu) / 16777216.0f;
}

// ruido de valor 3D suave (0..1)
inline float ruido(glm::vec3 p, uint32_t semilla)
{
	const glm::vec3 i = glm::floor(p);
	glm::vec3 f = p - i;
	f = f * f * (3.0f - 2.0f * f);
	const int x = (int)i.x, y = (int)i.y, z = (int)i.z;
	auto h = [&](int a, int b, int c) { return hash3(x + a, y + b, z + c, semilla); };
	return glm::mix(glm::mix(glm::mix(h(0, 0, 0), h(1, 0, 0), f.x), glm::mix(h(0, 1, 0), h(1, 1, 0), f.x), f.y),
	                glm::mix(glm::mix(h(0, 0, 1), h(1, 0, 1), f.x), glm::mix(h(0, 1, 1), h(1, 1, 1), f.x), f.y), f.z);
}

// gira v alrededor del eje (unitario) un angulo a
inline glm::vec3 girar(glm::vec3 v, glm::vec3 eje, float a)
{
	return v * std::cos(a) + glm::cross(eje, v) * std::sin(a) + eje * glm::dot(eje, v) * (1.0f - std::cos(a));
}
inline glm::vec3 perpendicular(glm::vec3 d)
{
	return glm::normalize(glm::cross(d, std::fabs(d.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0)));
}

// --------------------------------------------------------------------------------------------- mallas por codigo
struct PuntoTubo {
	glm::vec3 p;
	float radio;
	glm::vec4 color;   // tinte (o albedo sRGB del hongo), flexibilidad / recorrido
	float ao;
};

class Generador {
public:
	std::vector<Vertice> v;
	std::vector<uint32_t> idx;

	uint32_t base() const { return (uint32_t)v.size(); }
	void tri(uint32_t a, uint32_t b, uint32_t c) { idx.insert(idx.end(), { a, b, c }); }
	void vert(glm::vec3 p, float material, glm::vec3 n, glm::vec4 color, glm::vec2 uv, float capa, float ao)
	{
		v.push_back({ glm::vec4(p, material), n, color, glm::vec4(uv, capa, ao) });
	}

	// Tubo por una linea de puntos (corteza con uv: u da "vueltas" repeticiones alrededor, v sigue el largo cada
	// "paso" metros). "lobulos": raices en la base (los dos primeros anillos se ensanchan en 5 lobulos)
	void tubo(const std::vector<PuntoTubo>& t, int lados, float material, float capa, float vueltas, float paso, float lobulos = 0.0f,
	          uint32_t semilla = 1u, bool tapa = true)
	{
		const size_t n = t.size();
		if (n < 2) {
			return;
		}
		const uint32_t b0 = base();
		glm::vec3 x = perpendicular(glm::normalize(t[1].p - t[0].p));
		float largo = 0.0f;
		for (size_t i = 0; i < n; i++) {
			const glm::vec3 d = glm::normalize(i + 1 < n ? t[i + 1].p - t[i].p : t[i].p - t[i - 1].p);
			x = glm::normalize(x - d * glm::dot(x, d));   // transporte paralelo
			const glm::vec3 y = glm::cross(d, x);
			if (i > 0) {
				largo += glm::length(t[i].p - t[i - 1].p);
			}
			for (int k = 0; k <= lados; k++) {   // un vertice repetido en la costura de la uv
				const float a = 6.2831853f * (float)(k % lados) / (float)lados;
				const glm::vec3 radial = x * std::cos(a) + y * std::sin(a);
				float rr = t[i].radio * (0.9f + 0.2f * ruido(t[i].p * 2.5f + radial * 1.5f, semilla));
				if (lobulos > 0.0f && i < 2) {
					rr *= 1.0f + lobulos * (i == 0 ? 1.0f : 0.3f) * std::pow(std::max(0.0f, std::cos(a * 5.0f + (float)semilla)), 2.0f);
				}
				vert(t[i].p + radial * rr, material, radial, t[i].color, glm::vec2(vueltas * (float)k / (float)lados, largo / paso), capa, t[i].ao);
			}
		}
		const int L = lados + 1;
		for (size_t i = 0; i + 1 < n; i++) {
			for (int k = 0; k < lados; k++) {
				const uint32_t a = b0 + (uint32_t)(i * L + k), b = a + 1;
				tri(a, b, a + L);
				tri(b, b + L, a + L);
			}
		}
		if (tapa) {
			const glm::vec3 d = glm::normalize(t[n - 1].p - t[n - 2].p);
			const uint32_t c = base();
			vert(t[n - 1].p + d * t[n - 1].radio * 0.5f, material, d, t[n - 1].color, glm::vec2(0.5f * vueltas, largo / paso), capa, t[n - 1].ao);
			for (int k = 0; k < lados; k++) {
				tri(b0 + (uint32_t)((n - 1) * L + k), b0 + (uint32_t)((n - 1) * L + k + 1), c);
			}
		}
	}

	// Tarjeta de follaje: el pie (el tallo del ramillete) en "pie", sube por "arriba" y se abre a lo ancho por
	// "lado". Variante 0..3 (cuadrante de la textura). La normal se mezcla con la de la copa (de "centro" hacia
	// afuera) para que la copa entera se ilumine como un volumen y no como planos sueltos
	void tarjeta(glm::vec3 pie, glm::vec3 arriba, glm::vec3 lado, float ancho, float alto, int variante, bool espejo, float capa,
	             glm::vec3 centro, glm::vec3 achatar, float radioCopa, glm::vec4 tinte, float flexPie, float flexPunta, float aoMin = 0.35f)
	{
		const float u0 = (float)(variante % 2) * 0.5f, v0 = (float)(variante / 2) * 0.5f;
		const glm::vec3 plano = glm::normalize(glm::cross(lado, arriba));
		const glm::vec3 p[4] = { pie - lado * (ancho * 0.5f), pie + lado * (ancho * 0.5f), pie + lado * (ancho * 0.5f) + arriba * alto,
			                     pie - lado * (ancho * 0.5f) + arriba * alto };
		const glm::vec2 uv[4] = { { 0.0f, 0.5f }, { 0.5f, 0.5f }, { 0.5f, 0.0f }, { 0.0f, 0.0f } };
		const uint32_t b0 = base();
		for (int k = 0; k < 4; k++) {
			const glm::vec3 r = (p[k] - centro) * achatar;
			const float d = glm::length(r);
			glm::vec3 nc = d > 1e-4f ? r / d : glm::vec3(0, 1, 0);
			const glm::vec3 np = glm::dot(plano, nc) < 0.0f ? -plano : plano;
			const glm::vec3 n = glm::normalize(glm::mix(np, nc, 0.72f));
			const float ao = glm::mix(aoMin, 1.0f, glm::smoothstep(0.1f, 1.0f, d / std::max(radioCopa, 0.1f)));
			glm::vec2 t = uv[k];
			if (espejo) {
				t.x = 0.5f - t.x;
			}
			vert(p[k], HOJA, n, glm::vec4(glm::vec3(tinte), k < 2 ? flexPie : flexPunta), glm::vec2(u0, v0) + t, capa, ao * tinte.a);
		}
		tri(b0, b0 + 1, b0 + 2);
		tri(b0, b0 + 2, b0 + 3);
	}

	// Bulto (roca por codigo si falta el modelo): icosfera deformada con ruido, uv por proyeccion
	void bulto(glm::vec3 centro, glm::vec3 radio, int subdiv, float material, float capa, uint32_t semilla, float bultos = 0.28f)
	{
		std::vector<glm::vec3> pts;
		std::vector<uint32_t> caras;
		icosfera(subdiv, pts, caras);
		const uint32_t b0 = base();
		std::vector<glm::vec3> pos(pts.size());
		for (size_t i = 0; i < pts.size(); i++) {
			const glm::vec3 d = pts[i];
			const float k = 1.0f + bultos * (ruido(d * 2.2f + glm::vec3((float)semilla), semilla) - 0.5f) * 2.0f;
			glm::vec3 q = d * radio * k;
			if (q.y < -0.35f * radio.y) {
				q.y = -0.35f * radio.y + (q.y + 0.35f * radio.y) * 0.35f;
			}
			pos[i] = centro + q;
		}
		std::vector<glm::vec3> nor(pts.size(), glm::vec3(0.0f));
		for (size_t f = 0; f < caras.size(); f += 3) {
			const glm::vec3 n = glm::cross(pos[caras[f + 1]] - pos[caras[f]], pos[caras[f + 2]] - pos[caras[f]]);
			for (int k = 0; k < 3; k++) {
				nor[caras[f + k]] += n;
			}
		}
		for (size_t i = 0; i < pts.size(); i++) {
			const glm::vec3 n = glm::normalize(nor[i] + pts[i] * 1e-4f);
			vert(pos[i], material, n, glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), glm::vec2(pos[i].x + pos[i].y, pos[i].z + pos[i].y) * 0.3f, capa,
			     glm::mix(0.6f, 1.0f, n.y * 0.5f + 0.5f));
		}
		for (uint32_t c : caras) {
			idx.push_back(b0 + c);
		}
	}

	// Hongo: tallo y sombrero (media esfera aplanada con ala), colores naturales (sRGB en color.rgb)
	void hongo(glm::vec3 p, float alto, float radio, glm::vec3 color, uint32_t semilla)
	{
		Azar az(semilla);
		const glm::vec3 inclina(az.entre(-0.15f, 0.15f), 1.0f, az.entre(-0.15f, 0.15f));
		const glm::vec3 cima = p + glm::normalize(inclina) * alto;
		const glm::vec4 pie(0.80f, 0.76f, 0.66f, 0.0f);
		tubo({ { p, radio * 0.30f, pie * 0.85f, 0.5f }, { glm::mix(p, cima, 0.5f), radio * 0.25f, pie, 0.8f }, { cima, radio * 0.23f, pie, 0.9f } },
		     7, HONGO, -1.0f, 1.0f, 1.0f, 0.0f, semilla, false);
		const int A = 12, B = 5;
		const uint32_t b0 = base();
		for (int j = 0; j <= B; j++) {
			const float fi = 1.5707963f * (float)j / (float)B;   // 0 = borde, pi/2 = arriba
			for (int k = 0; k < A; k++) {
				const float a = 6.2831853f * (float)k / (float)A;
				const glm::vec3 d(std::cos(a) * std::cos(fi), std::sin(fi), std::sin(a) * std::cos(fi));
				const glm::vec3 q = cima + glm::vec3(d.x * radio, d.y * radio * 0.55f - (j == 0 ? radio * 0.12f : 0.0f), d.z * radio);
				vert(q, HONGO, glm::normalize(glm::vec3(d.x, d.y * 1.4f + 0.2f, d.z)), glm::vec4(color * (0.7f + 0.3f * (float)j / B), 0.0f),
				     glm::vec2(0.0f), -1.0f, 0.6f + 0.4f * (float)j / B);
			}
		}
		for (int j = 0; j < B; j++) {
			for (int k = 0; k < A; k++) {
				const uint32_t a = b0 + (uint32_t)(j * A + k), b = b0 + (uint32_t)(j * A + (k + 1) % A);
				tri(a, b, a + A);
				tri(b, b + A, a + A);
			}
		}
		// laminillas claras debajo
		const uint32_t c = base();
		vert(cima - glm::vec3(0.0f, radio * 0.05f, 0.0f), HONGO, glm::vec3(0, -1, 0), glm::vec4(0.78f, 0.72f, 0.60f, 0.0f), glm::vec2(0.0f), -1.0f, 0.5f);
		for (int k = 0; k < A; k++) {
			tri(c, b0 + (uint32_t)((k + 1) % A), b0 + (uint32_t)k);
		}
	}

	static void icosfera(int subdiv, std::vector<glm::vec3>& p, std::vector<uint32_t>& f)
	{
		const float t = (1.0f + std::sqrt(5.0f)) * 0.5f;
		p = { { -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 }, { 0, -1, t }, { 0, 1, t },
		      { 0, -1, -t }, { 0, 1, -t }, { t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 } };
		for (auto& q : p) {
			q = glm::normalize(q);
		}
		f = { 0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
		      3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1 };
		for (int s = 0; s < subdiv; s++) {
			std::map<uint64_t, uint32_t> medio;
			auto mid = [&](uint32_t a, uint32_t b) {
				const uint64_t k = a < b ? ((uint64_t)a << 32) | b : ((uint64_t)b << 32) | a;
				auto it = medio.find(k);
				if (it != medio.end()) {
					return it->second;
				}
				p.push_back(glm::normalize(p[a] + p[b]));
				medio[k] = (uint32_t)p.size() - 1;
				return (uint32_t)p.size() - 1;
			};
			std::vector<uint32_t> g;
			for (size_t i = 0; i < f.size(); i += 3) {
				const uint32_t a = f[i], b = f[i + 1], c = f[i + 2];
				const uint32_t ab = mid(a, b), bc = mid(b, c), ca = mid(c, a);
				g.insert(g.end(), { a, ab, ca, b, bc, ab, c, ca, bc, ab, bc, ca });
			}
			f.swap(g);
		}
	}
};

// ------------------------------------------------------------------------------------- modelos escaneados (.malla)
// Formato de bosque_mallas_blender.py: "MALL", version, vertices, indices, partes; rangos; pos/normal/uv; indices
struct MallaArchivo {
	struct V { float p[3], n[3], uv[2]; };
	std::vector<V> v;
	std::vector<uint32_t> idx;
	std::vector<std::pair<uint32_t, uint32_t>> partes;   // primer indice, indices
	bool leer(const std::string& ruta)
	{
		std::ifstream f(ruta, std::ios::binary);
		if (!f) {
			fprintf(stderr, "[bosque] falta %s (se omite)\n", ruta.c_str());
			return false;
		}
		char magia[4];
		uint32_t cab[4];
		f.read(magia, 4);
		f.read((char*)cab, sizeof(cab));
		if (!f || std::memcmp(magia, "MALL", 4) != 0 || cab[0] != 1) {
			fprintf(stderr, "[bosque] %s no es una .malla\n", ruta.c_str());
			return false;
		}
		partes.resize(cab[3]);
		f.read((char*)partes.data(), partes.size() * sizeof(partes[0]));
		v.resize(cab[1]);
		f.read((char*)v.data(), v.size() * sizeof(V));
		idx.resize(cab[2]);
		f.read((char*)idx.data(), idx.size() * sizeof(uint32_t));
		return (bool)f;
	}
	// copia la parte k al generador (escalada); el follaje se mece mas arriba
	void agregar(Generador& g, size_t k, float escala, float material, float capa, glm::vec3 tinte, bool follaje) const
	{
		if (k >= partes.size()) {
			return;
		}
		const uint32_t i0 = partes[k].first, n = partes[k].second;
		uint32_t vmin = UINT32_MAX, vmax = 0;
		float alto = 0.01f;
		for (uint32_t i = i0; i < i0 + n; i++) {
			vmin = std::min(vmin, idx[i]);
			vmax = std::max(vmax, idx[i]);
		}
		for (uint32_t i = vmin; i <= vmax; i++) {
			alto = std::max(alto, v[i].p[1]);
		}
		const uint32_t b0 = g.base();
		for (uint32_t i = vmin; i <= vmax; i++) {
			const V& q = v[i];
			const float h = q.p[1] / alto;
			const float flex = follaje ? 0.15f + 0.85f * h : 0.0f;
			const float ao = follaje ? glm::mix(0.45f, 1.0f, h) : 1.0f;
			g.vert(glm::vec3(q.p[0], q.p[1], q.p[2]) * escala, material, glm::vec3(q.n[0], q.n[1], q.n[2]), glm::vec4(tinte, flex),
			       glm::vec2(q.uv[0], q.uv[1]), capa, ao);
		}
		for (uint32_t i = i0; i < i0 + n; i++) {
			g.idx.push_back(b0 + idx[i] - vmin);
		}
	}
};

// ------------------------------------------------------------------------------------------------------ arboles
// Especie: parametros por nivel (0 tronco, 1 ramas, 2 ramitas) al estilo de ez-tree
struct Especie {
	int niveles;                 // niveles de ramas bajo el tronco (1 o 2)
	float largo, radio;          // del tronco
	int secciones[3], lados[3];
	float torcer[3];             // azar en cada seccion (nudoso)
	float subir[3];              // tendencia a crecer hacia la luz (+) o a colgar (-)
	int hijos[3];                // ramas que salen de cada rama del nivel
	float desde[3];              // de que parte de la rama padre salen (0..1)
	float angulo[3];             // angulo con la rama padre (rad)
	float largoHijo[3], radioHijo[3];
	float afinar[3];             // cuanto se afina hasta la punta
	int tarjetas;                // tarjetas de follaje por rama del ultimo nivel
	float tamTarjeta;            // m
	float capaHojas, capaCorteza;
	bool lobulos;
	glm::vec3 tinte;             // del follaje
	float tinteAzar;             // variacion de tono por tarjeta
	float dorado;                // fraccion de ramilletes amarillentos (otono)
};

struct Copa {
	glm::vec3 centro, achatar;
	float radio;
};

class Arbolador {
public:
	Generador& g;
	const Especie& e;
	Azar az;
	Copa copa;
	float alto;
	Arbolador(Generador& g_, const Especie& e_, uint32_t semilla, Copa c, float alto_) : g(g_), e(e_), az(semilla), copa(c), alto(alto_) {}

	void rama(glm::vec3 origen, glm::vec3 dir, float largo, float radio, int nivel)
	{
		const int S = e.secciones[nivel];
		std::vector<PuntoTubo> t;
		std::vector<glm::vec3> dirs;
		glm::vec3 p = origen, d = glm::normalize(dir);
		for (int i = 0; i <= S; i++) {
			const float s = (float)i / (float)S;
			const float r = radio * (1.0f - e.afinar[nivel] * s) * (nivel == 0 && i == 0 ? 1.15f : 1.0f);
			const float hRel = glm::clamp(p.y / alto, 0.0f, 1.0f);
			const float flex = nivel == 0 ? 0.02f + 0.18f * s : glm::clamp(0.25f + 0.35f * (float)nivel + 0.4f * s, 0.0f, 1.0f) * (0.4f + 0.6f * hRel);
			const float ao = nivel == 0 ? glm::mix(0.55f, 0.9f, s) : 0.75f;
			const float var = az.entre(0.9f, 1.1f);
			t.push_back({ p, std::max(r, 0.012f), glm::vec4(var, var, var, flex), ao });
			dirs.push_back(d);
			if (i < S) {
				d = glm::normalize(d + az.esfera() * e.torcer[nivel] + glm::vec3(0.0f, e.subir[nivel], 0.0f));
				p += d * (largo / (float)S);
			}
		}
		const float vueltas = std::max(1.0f, std::round(6.2831853f * radio / 1.6f));
		g.tubo(t, e.lados[nivel], CORTEZA, e.capaCorteza, vueltas, 2.2f, nivel == 0 && e.lobulos ? 0.9f : 0.0f, (uint32_t)(az() * 1000.0f), true);
		auto en = [&](float s, glm::vec3& q, glm::vec3& dq, float& rq) {
			const float f = s * (float)S;
			const int i = std::min(S - 1, (int)f);
			const float u = f - (float)i;
			q = glm::mix(t[i].p, t[i + 1].p, u);
			dq = glm::normalize(glm::mix(dirs[i], dirs[i + 1], u));
			rq = glm::mix(t[i].radio, t[i + 1].radio, u);
		};
		if (nivel < e.niveles) {
			const int n = e.hijos[nivel];
			const float giro0 = az.entre(0.0f, 6.2831853f);
			for (int c = 0; c < n; c++) {
				const float s = e.desde[nivel] + (1.0f - e.desde[nivel]) * ((float)c + az.entre(0.1f, 0.9f)) / (float)n;
				glm::vec3 q, dq;
				float rq;
				en(s, q, dq, rq);
				const glm::vec3 perp = perpendicular(dq);
				const float acimut = giro0 + (float)c * 2.39996f + az.entre(-0.3f, 0.3f);   // angulo de oro
				const glm::vec3 eje = girar(perp, dq, acimut);
				const glm::vec3 dh = glm::normalize(girar(dq, eje, e.angulo[nivel] * az.entre(0.8f, 1.2f)));
				const float lh = largo * e.largoHijo[nivel] * (nivel == 0 ? (1.0f - 0.55f * s) : (1.0f - 0.35f * s)) * az.entre(0.8f, 1.15f);
				rama(q, dh, lh, rq * e.radioHijo[nivel], nivel + 1);
			}
		}
		// follaje: en las ramas del ultimo nivel (y en la punta de las del anterior)
		if (nivel == e.niveles || (nivel == e.niveles - 1 && nivel > 0)) {
			const int n = nivel == e.niveles ? e.tarjetas : std::max(2, e.tarjetas / 2);
			for (int k = 0; k < n; k++) {
				const float s = nivel == e.niveles ? az.entre(0.2f, 1.0f) : az.entre(0.6f, 1.0f);
				glm::vec3 q, dq;
				float rq;
				en(s, q, dq, rq);
				hojaEn(q, dq, nivel == e.niveles ? 1.0f : 1.15f);
			}
			// remate en la punta
			hojaEn(t.back().p, dirs.back(), 1.0f);
		}
	}

	void hojaEn(glm::vec3 q, glm::vec3 dq, float tam)
	{
		const glm::vec3 fuera = glm::normalize((q - copa.centro) * glm::vec3(1.0f, 0.6f, 1.0f) + glm::vec3(0.0f, 0.001f, 0.0f));
		glm::vec3 arriba = glm::normalize(dq * 0.55f + fuera * 0.45f + az.esfera() * 0.35f + glm::vec3(0.0f, 0.12f, 0.0f));
		glm::vec3 lado = glm::normalize(glm::cross(arriba, az.esfera()));
		const float w = e.tamTarjeta * tam * az.entre(0.8f, 1.2f);
		const float tv = az.entre(-e.tinteAzar, e.tinteAzar);
		glm::vec3 tinte = e.tinte * glm::vec3(1.0f + tv, 1.0f + tv * 0.6f, 1.0f + tv * 0.2f);
		if (az() < e.dorado) {
			tinte *= glm::vec3(1.25f, 1.0f, 0.45f);
		}
		const float hRel = glm::clamp(q.y / alto, 0.0f, 1.0f);
		g.tarjeta(q - arriba * (w * 0.08f), arriba, lado, w, w, (int)(az() * 4.0f) & 3, az() < 0.5f, e.capaHojas, copa.centro, copa.achatar,
		          copa.radio, glm::vec4(tinte, 1.0f), 0.45f + 0.4f * hRel, 0.7f + 0.3f * hRel);
	}
};

inline Especie especieHaya(bool alta, bool lejos)
{
	Especie e{};
	e.niveles = lejos ? 1 : 2;
	e.largo = alta ? 6.2f : 5.0f;
	e.radio = alta ? 0.40f : 0.36f;
	const int sec[3] = { 9, 6, 4 }, lad[3] = { lejos ? 7 : 12, lejos ? 4 : 7, 4 };
	const float tor[3] = { 0.06f, 0.22f, 0.3f }, sub[3] = { 0.02f, 0.10f, 0.06f };
	const int hij[3] = { alta ? 8 : 7, 4, 0 };
	const float des[3] = { alta ? 0.42f : 0.35f, 0.25f, 0.0f }, ang[3] = { 0.95f, 0.75f, 0.0f };
	const float lh[3] = { alta ? 0.62f : 0.70f, 0.55f, 0.0f }, rh[3] = { 0.55f, 0.55f, 0.0f }, af[3] = { 0.55f, 0.7f, 0.75f };
	for (int i = 0; i < 3; i++) {
		e.secciones[i] = lejos ? std::max(2, sec[i] / 2) : sec[i];
		e.lados[i] = lad[i];
		e.torcer[i] = tor[i];
		e.subir[i] = sub[i];
		e.hijos[i] = hij[i];
		e.desde[i] = des[i];
		e.angulo[i] = ang[i];
		e.largoHijo[i] = lh[i];
		e.radioHijo[i] = rh[i];
		e.afinar[i] = af[i];
	}
	if (lejos) {
		e.hijos[0] = 8;
	}
	e.tarjetas = lejos ? 9 : 7;
	e.tamTarjeta = lejos ? 2.6f : 1.65f;
	e.capaHojas = C_HOJAS;
	e.capaCorteza = C_CORTEZA;
	e.lobulos = true;
	e.tinte = alta ? glm::vec3(0.95f, 1.0f, 0.82f) : glm::vec3(0.82f, 0.95f, 0.78f);
	e.tinteAzar = 0.12f;
	e.dorado = alta ? 0.12f : 0.0f;
	return e;
}

inline void arbolHaya(Generador& g, uint32_t semilla, bool alta, bool lejos)
{
	const Especie e = especieHaya(alta, lejos);
	const float H = e.largo;
	Arbolador a(g, e, semilla, { glm::vec3(0.0f, H * 0.95f, 0.0f), glm::vec3(1.0f, 1.25f, 1.0f), H * 0.62f }, H * 1.5f);
	a.rama(glm::vec3(0.0f, -0.2f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), H, e.radio, 0);
}

// Abeto: tronco recto y verticilos de ramas que cuelgan un poco, cada una cubierta de tarjetas de ramitas de abeto
// (acostadas a lo largo de la rama y otra cruzada) y una punta vertical
inline void abeto(Generador& g, uint32_t semilla, bool lejos)
{
	Azar az(semilla);
	const float H = 9.5f;
	std::vector<PuntoTubo> t;
	for (int i = 0; i <= 6; i++) {
		const float s = (float)i / 6.0f;
		t.push_back({ glm::vec3(az.entre(-0.03f, 0.03f), -0.2f + H * s, az.entre(-0.03f, 0.03f)), 0.34f * (1.0f - 0.85f * s) * (i == 0 ? 1.15f : 1.0f),
		              glm::vec4(1.0f, 1.0f, 1.0f, 0.03f + 0.25f * s), glm::mix(0.5f, 0.85f, s) });
	}
	g.tubo(t, lejos ? 6 : 10, CORTEZA, C_CORTEZA_PINO, 2.0f, 2.2f, 0.6f, semilla, true);
	const Copa copa{ glm::vec3(0.0f, H * 0.5f, 0.0f), glm::vec3(1.0f, 0.35f, 1.0f), 2.8f };
	const int pisos = lejos ? 7 : 12;
	const float y0 = 1.5f;
	for (int k = 0; k < pisos; k++) {
		const float s = (float)k / (float)(pisos - 1);
		const float y = y0 + s * (H - 1.0f - y0);
		const float L = (0.35f + 2.6f * std::pow(1.0f - s, 0.95f)) * az.entre(0.88f, 1.12f);
		const int nr = lejos ? 5 : 5 + (int)(az() * 3.0f);
		const float giro0 = az.entre(0.0f, 6.28f);
		for (int r = 0; r < nr; r++) {
			const float a = giro0 + 6.2831853f * (float)r / (float)nr + az.entre(-0.25f, 0.25f);
			const glm::vec3 radial(std::cos(a), 0.0f, std::sin(a));
			const float caida = az.entre(-0.18f, 0.05f) - 0.1f * (1.0f - s);
			const glm::vec3 dir = glm::normalize(radial + glm::vec3(0.0f, caida, 0.0f));
			const glm::vec3 p0(0.0f, y, 0.0f);
			const glm::vec3 p2 = p0 + dir * L + glm::vec3(0.0f, -0.12f * L, 0.0f);
			const float flex0 = 0.2f + 0.3f * s, flex1 = 0.55f + 0.45f * s;
			if (!lejos) {
				g.tubo({ { p0, 0.07f * (0.5f + L / 3.0f), glm::vec4(1.0f, 1.0f, 1.0f, flex0), 0.5f },
				         { glm::mix(p0, p2, 0.55f) + glm::vec3(0.0f, 0.04f * L, 0.0f), 0.04f * (0.5f + L / 3.0f), glm::vec4(1.0f, 1.0f, 1.0f, (flex0 + flex1) * 0.5f), 0.6f },
				         { p2, 0.015f, glm::vec4(1.0f, 1.0f, 1.0f, flex1), 0.7f } },
				       4, CORTEZA, C_CORTEZA_PINO, 1.0f, 2.2f, 0.0f, semilla + (uint32_t)r, false);
			}
			// tarjetas a lo largo de la rama: el ramillete apunta hacia la punta, acostado (y otro cruzado)
			const int nt = std::max(1, (int)std::ceil(L / (lejos ? 1.6f : 1.05f)));
			for (int j = 0; j < nt; j++) {
				const float u = ((float)j + 0.15f) / (float)nt;
				const glm::vec3 pie = glm::mix(p0 + radial * 0.15f, p2, u * 0.85f);
				const float w = std::min(L * 1.15f, (lejos ? 2.2f : 1.5f)) * az.entre(0.85f, 1.1f);
				const glm::vec3 arriba = glm::normalize(dir + az.esfera() * 0.12f);
				const glm::vec3 lado = glm::normalize(glm::cross(arriba, glm::vec3(0.0f, 1.0f, 0.0f)) + az.esfera() * 0.2f);
				const float tv = az.entre(0.88f, 1.08f);
				const glm::vec4 tinte(0.78f * tv, 0.88f * tv, 0.80f * tv, 1.0f);
				const float aoMin = glm::mix(0.35f, 0.65f, s);
				g.tarjeta(pie, arriba, lado, w, w, (int)(az() * 4.0f) & 3, az() < 0.5f, C_ABETO, copa.centro, copa.achatar, copa.radio + L, tinte,
				          glm::mix(flex0, flex1, u), flex1, aoMin);
				if (!lejos && L > 0.9f) {
					const glm::vec3 lado2 = glm::normalize(girar(lado, arriba, 1.25f));
					g.tarjeta(pie + glm::vec3(0.0f, 0.05f, 0.0f), arriba, lado2, w * 0.85f, w * 0.85f, (int)(az() * 4.0f) & 3, az() < 0.5f, C_ABETO,
					          copa.centro, copa.achatar, copa.radio + L, tinte * glm::vec4(0.9f, 0.9f, 0.9f, 1.0f), glm::mix(flex0, flex1, u), flex1, aoMin);
				}
			}
		}
	}
	// la punta: dos tarjetas verticales cruzadas
	for (int k = 0; k < 2; k++) {
		const float a = 1.5707963f * (float)k;
		g.tarjeta(glm::vec3(0.0f, H - 1.6f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(std::cos(a), 0.0f, std::sin(a)), 1.3f, 1.9f, k, false, C_ABETO,
		          copa.centro, copa.achatar, copa.radio, glm::vec4(0.85f, 0.95f, 0.85f, 1.0f), 0.6f, 1.0f, 0.9f);
	}
}

// Arbusto: 5-7 tallos cortos desde el suelo con ramilletes (mezcla de hiedra y haya)
inline void arbusto(Generador& g, uint32_t semilla)
{
	Azar az(semilla);
	const Copa copa{ glm::vec3(0.0f, 0.7f, 0.0f), glm::vec3(1.0f, 1.4f, 1.0f), 1.3f };
	const int n = 6;
	for (int k = 0; k < n; k++) {
		const float a = 6.2831853f * (float)k / (float)n + az.entre(-0.3f, 0.3f);
		const glm::vec3 d = glm::normalize(glm::vec3(std::cos(a) * 0.6f, 1.0f, std::sin(a) * 0.6f));
		const float L = az.entre(0.9f, 1.4f);
		const glm::vec3 p0(std::cos(a) * 0.08f, -0.05f, std::sin(a) * 0.08f);
		const glm::vec3 p1 = p0 + d * L;
		g.tubo({ { p0, 0.035f, glm::vec4(1.0f, 1.0f, 1.0f, 0.1f), 0.5f }, { p1, 0.012f, glm::vec4(1.0f, 1.0f, 1.0f, 0.6f), 0.8f } }, 4, CORTEZA, C_CORTEZA, 1.0f, 2.2f,
		       0.0f, semilla + (uint32_t)k, false);
		for (int j = 0; j < 4; j++) {
			const float u = 0.35f + 0.65f * (float)j / 3.0f;
			const glm::vec3 q = glm::mix(p0, p1, u);
			const glm::vec3 fuera = glm::normalize(glm::vec3(q.x, 0.0f, q.z) + glm::vec3(0.001f, 0.0f, 0.0f));
			const glm::vec3 arriba = glm::normalize(d * 0.5f + fuera * 0.5f + az.esfera() * 0.4f + glm::vec3(0.0f, 0.2f, 0.0f));
			const glm::vec3 lado = glm::normalize(glm::cross(arriba, az.esfera()));
			const float w = az.entre(0.75f, 1.05f);
			const bool hiedra = az() < 0.6f;
			const float tv = az.entre(0.85f, 1.1f);
			g.tarjeta(q, arriba, lado, w, w, (int)(az() * 4.0f) & 3, az() < 0.5f, hiedra ? C_ARBUSTO : C_HOJAS, copa.centro, copa.achatar, copa.radio,
			          glm::vec4(glm::vec3(0.85f, 0.95f, 0.8f) * tv, 1.0f), 0.3f, 0.7f, 0.45f);
		}
	}
}

inline void hongos(Generador& g, uint32_t semilla)
{
	Azar az(semilla);
	const glm::vec3 tonos[3] = { { 0.62f, 0.42f, 0.24f }, { 0.74f, 0.58f, 0.36f }, { 0.48f, 0.30f, 0.18f } };
	for (int k = 0; k < 5; k++) {
		const float a = az.entre(0.0f, 6.2831853f);
		const float d = k == 0 ? 0.0f : az.entre(0.1f, 0.3f);
		const float alto = az.entre(0.10f, 0.24f) * (k == 0 ? 1.3f : 1.0f);
		g.hongo(glm::vec3(std::cos(a) * d, 0.0f, std::sin(a) * d), alto, alto * az.entre(0.55f, 0.8f), tonos[(semilla + k) % 3] * az.entre(0.85f, 1.1f),
		        semilla * 3u + (uint32_t)k);
	}
}

// Raices que salen del bosque, trepan por el canto de la mesa y avanzan sobre el borde del tablero (sin llegar a
// las casillas: como mucho 0.9 m hacia adentro). En el mundo; recorrido 0 (base) .. 1 (punta) en color.a
inline void raices(Generador& g, float hx, float hz, uint32_t semilla)
{
	Azar az(semilla);
	const int N = 26;
	for (int k = 0; k < N; k++) {
		const int lado = k % 4;   // 0 fondo, 1 derecha, 2 frente, 3 izquierda
		const float u = az.entre(-0.9f, 0.9f);
		glm::vec3 borde, haciaAdentro;
		if (lado == 0 || lado == 2) {
			borde = glm::vec3(u * hx, 0.0f, lado == 0 ? -hz : hz);
			haciaAdentro = glm::vec3(0.0f, 0.0f, lado == 0 ? 1.0f : -1.0f);
		} else {
			borde = glm::vec3(lado == 1 ? hx : -hx, 0.0f, u * hz);
			haciaAdentro = glm::vec3(lado == 1 ? -1.0f : 1.0f, 0.0f, 0.0f);
		}
		const glm::vec3 deLado(-haciaAdentro.z, 0.0f, haciaAdentro.x);
		const float fuera = az.entre(1.2f, 2.6f), dentro = az.entre(0.35f, 0.85f);
		const float grueso = az.entre(0.05f, 0.1f);
		const float serpentea = az.entre(-0.4f, 0.4f);
		std::vector<glm::vec3> pts;
		for (int i = 0; i <= 4; i++) {
			const float s = (float)i / 4.0f;
			pts.push_back(borde - haciaAdentro * (fuera * (1.0f - s) + 0.2f) + deLado * (serpentea * std::sin(s * 3.0f)) + glm::vec3(0.0f, SUELO_Y + grueso * 0.4f, 0.0f));
		}
		pts.push_back(borde - haciaAdentro * 0.17f + glm::vec3(0.0f, -0.12f, 0.0f) + deLado * (serpentea * std::sin(3.0f)));
		pts.push_back(borde - haciaAdentro * 0.05f + glm::vec3(0.0f, 0.03f, 0.0f) + deLado * (serpentea * std::sin(3.0f)));
		for (int i = 1; i <= 5; i++) {
			const float s = (float)i / 5.0f;
			pts.push_back(borde + haciaAdentro * (dentro * s) + deLado * (serpentea * std::sin(3.0f) + 0.15f * std::sin(s * 5.0f + (float)k)) +
			              glm::vec3(0.0f, grueso * 0.35f * (1.0f - s) + 0.008f, 0.0f));
		}
		std::vector<PuntoTubo> t;
		for (size_t i = 0; i < pts.size(); i++) {
			const float s = (float)i / (float)(pts.size() - 1);
			t.push_back({ pts[i], grueso * (1.0f - 0.85f * s), glm::vec4(0.9f, 0.85f, 0.8f, s), 0.75f });
		}
		g.tubo(t, 6, RAIZ, C_CORTEZA, 1.0f, 1.2f, 0.0f, semilla + (uint32_t)k, true);
	}
}

// el suelo: disco de anillos (mas finos cerca de la mesa); la uv sale del lugar en el fragmento
inline void suelo(Generador& g)
{
	const int A = 72, R = 26;
	const uint32_t b0 = g.base();
	g.vert(glm::vec3(0.0f, SUELO_Y, 0.0f), TIERRA, glm::vec3(0, 1, 0), glm::vec4(1.0f), glm::vec2(0.0f), C_SUELO, 1.0f);
	for (int j = 1; j <= R; j++) {
		const float r = 58.0f * std::pow((float)j / (float)R, 1.6f);
		for (int k = 0; k < A; k++) {
			const float a = 6.2831853f * (float)k / (float)A;
			g.vert(glm::vec3(std::cos(a) * r, SUELO_Y, std::sin(a) * r), TIERRA, glm::vec3(0, 1, 0), glm::vec4(1.0f), glm::vec2(0.0f), C_SUELO, 1.0f);
		}
	}
	for (int k = 0; k < A; k++) {
		g.tri(b0, b0 + 1 + (uint32_t)((k + 1) % A), b0 + 1 + (uint32_t)k);
	}
	for (int j = 1; j < R; j++) {
		for (int k = 0; k < A; k++) {
			const uint32_t a = b0 + 1 + (uint32_t)((j - 1) * A + k), b = b0 + 1 + (uint32_t)((j - 1) * A + (k + 1) % A);
			g.tri(a, b, b + A);
			g.tri(a, b + A, a + A);
		}
	}
}

// ------------------------------------------------------------------------------------------- lugares (instancias)
// hx, hz: mitad del ancho y del fondo de la mesa. La camara de juego mira desde +z: delante (z > hz - 1) solo hay
// plantas bajas, y lo alto queda a los costados y detras
inline void repartir(std::array<std::vector<Instancia>, NUM_MALLAS>& inst, float hx, float hz)
{
	Azar az(20261016u);
	std::vector<glm::vec3> ocupado;   // x, z, radio
	auto libre = [&](float x, float z, float r) {
		for (const glm::vec3& o : ocupado) {
			if (glm::length(glm::vec2(x - o.x, z - o.y)) < r + o.z) {
				return false;
			}
		}
		return true;
	};
	auto fueraMesa = [&](float x, float z, float m) { return std::fabs(x) > hx + m || std::fabs(z) > hz + m; };
	auto alto = [&](float x, float z) {   // donde puede ir algo alto
		const bool delante = z > hz - 1.0f && std::fabs(x) < hx + 7.0f;
		return !delante && fueraMesa(x, z, 2.2f);
	};
	auto poner = [&](TipoMalla t, float x, float z, float esc, float radio, float hunde) {
		ocupado.push_back(glm::vec3(x, z, radio));
		inst[t].push_back({ glm::vec4(x, SUELO_Y, z, esc), glm::vec4(az.entre(0.0f, 6.2831853f), az(), az.entre(0.88f, 1.15f), hunde) });
	};
	// arboles que enmarcan la mesa (fijos: siempre los mismos, a los costados y detras)
	const glm::vec3 marco[] = { { -hx - 3.2f, -hz - 2.8f, 1.9f }, { hx + 3.4f, -hz - 3.4f, 2.0f }, { -hx - 4.5f, 1.0f, 1.7f },
	                            { hx + 4.2f, -2.0f, 1.8f }, { -6.0f, -hz - 4.5f, 1.6f }, { 7.0f, -hz - 5.2f, 1.7f },
	                            { -hx - 5.5f, hz - 2.5f, 1.5f }, { hx + 5.0f, hz - 3.0f, 1.6f }, { 0.5f, -hz - 7.5f, 1.9f } };
	int n = 0;
	for (const glm::vec3& m : marco) {
		poner(n % 3 == 2 ? PINO : n % 2 ? ARBOL_B : ARBOL_A, m.x, m.y, m.z * (n % 3 == 2 ? 0.85f : 1.0f), 2.4f * m.z, 14.0f * m.z);
		n++;
	}
	// el resto del bosque: arboles y abetos hacia afuera (mas lejos, mas grandes y apretados)
	struct Cuota { TipoMalla t; int n; float r0, r1, e0, e1, radio, hunde; };
	const Cuota cuotas[] = {
		{ ARBOL_A, 30, 3.0f, 34.0f, 1.2f, 2.0f, 2.4f, 14.0f }, { ARBOL_B, 22, 3.0f, 34.0f, 1.2f, 1.9f, 2.6f, 15.0f },
		{ PINO, 36, 6.0f, 38.0f, 1.0f, 1.7f, 2.0f, 12.0f },
	};
	for (const Cuota& c : cuotas) {
		int puestos = 0;
		for (int intento = 0; intento < c.n * 60 && puestos < c.n; intento++) {
			const float ang = az.entre(0.0f, 6.2831853f);
			const float d = c.r0 + (c.r1 - c.r0) * std::pow(az(), 1.3f);
			const float x = std::cos(ang) * (hx + d), z = std::sin(ang) * (hz + d) - 2.0f;
			const float esc = az.entre(c.e0, c.e1) * (1.0f + 0.012f * d);
			if (!alto(x, z) || !libre(x, z, c.radio * esc * 0.75f)) {
				continue;
			}
			poner(c.t, x, z, esc, c.radio * esc * 0.75f, c.hunde * esc);
			puestos++;
		}
	}
	// anillo lejano (versiones livianas): da la pared del bosque que se pierde en la niebla
	for (int k = 0; k < 70; k++) {
		const float ang = 6.2831853f * ((float)k + az.entre(0.0f, 0.8f)) / 70.0f;
		const float d = az.entre(38.0f, 52.0f);
		const float x = std::cos(ang) * d * 1.05f, z = std::sin(ang) * d - 2.0f;
		if (!alto(x, z) || !libre(x, z, 2.0f)) {
			continue;
		}
		const bool pino = az() < 0.45f;
		poner(pino ? PINO_LEJOS : ARBOL_LEJOS, x, z, az.entre(1.6f, 2.4f), 2.0f, 30.0f);
	}
	// plantas bajas: tambien delante, cerca de la mesa
	const Cuota bajas[] = {
		{ ARBUSTO, 80, 0.4f, 24.0f, 0.8f, 1.6f, 1.0f, 2.2f }, { HELECHO, 70, 0.3f, 20.0f, 1.6f, 2.6f, 0.9f, 1.6f },
		{ HELECHO_B, 60, 0.3f, 22.0f, 1.6f, 2.6f, 0.9f, 1.6f }, { HONGOS, 36, 0.2f, 12.0f, 0.8f, 1.6f, 0.4f, 0.6f },
		{ ROCA, 14, 1.0f, 26.0f, 0.25f, 0.6f, 1.0f, 1.6f }, { ROCA_B, 12, 1.0f, 26.0f, 0.25f, 0.6f, 1.0f, 1.6f },
		{ TOCON, 10, 1.0f, 22.0f, 1.4f, 2.0f, 0.7f, 1.6f }, { TRONCO, 8, 1.5f, 24.0f, 1.8f, 2.6f, 1.0f, 1.2f },
	};
	for (const Cuota& c : bajas) {
		int puestos = 0;
		for (int intento = 0; intento < c.n * 60 && puestos < c.n; intento++) {
			const float ang = az.entre(0.0f, 6.2831853f);
			const float d = c.r0 + (c.r1 - c.r0) * std::pow(az(), 1.6f);
			const float x = std::cos(ang) * (hx + 0.2f + d), z = std::sin(ang) * (hz + 0.2f + d);
			const float esc = az.entre(c.e0, c.e1);
			if (!fueraMesa(x, z, 0.35f + 0.6f * esc) || !libre(x, z, c.radio * esc * 0.6f)) {
				continue;
			}
			// delante de la camara de juego, solo cosas bajas y chicas
			const bool delante = z > hz + 1.0f && std::fabs(x) < hx + 4.0f;
			if (delante && (c.t == ROCA || c.t == ROCA_B || c.t == TRONCO || c.t == TOCON)) {
				continue;
			}
			poner(c.t, x, z, esc, c.radio * esc * 0.6f, c.hunde * esc);
			puestos++;
		}
	}
	// sobre el tablero: helechos y hongos en las cuatro esquinas (fuera de las casillas)
	for (int k = 0; k < 4; k++) {
		const float sx = k % 2 ? 1.0f : -1.0f, sz = k / 2 ? 1.0f : -1.0f;
		inst[HELECHO].push_back({ glm::vec4(sx * (hx - 0.45f), 0.0f, sz * (hz - 0.45f), 0.8f), glm::vec4(az.entre(0.0f, 6.28f), az(), 1.0f, 0.8f) });
		inst[HONGOS].push_back({ glm::vec4(sx * (hx - 0.35f) + sx * -0.6f, 0.0f, sz * (hz - 0.3f), 0.7f), glm::vec4(az.entre(0.0f, 6.28f), az(), 1.0f, 0.4f) });
	}
	// raices y suelo: una sola "instancia" neutra cada uno (la malla ya esta en el mundo)
	inst[RAICES].push_back({ glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), glm::vec4(0.0f) });
	inst[SUELO].push_back({ glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), glm::vec4(0.0f) });
}

// ------------------------------------------------------------------------------------- texturas (arreglo 2D)
// El alfa de los mipmaps se escala para conservar la cobertura del nivel 0 (con prueba de alfa a 0.5): sin esto
// el follaje se "adelgaza" y desaparece de lejos (Castano, "Computing Alpha Mipmaps")
inline void mipAlfaConCobertura(std::vector<uint8_t>& m, float cobertura)
{
	const size_t n = m.size() / 4;
	if (n == 0 || cobertura <= 0.0f) {
		return;
	}
	auto cubre = [&](float k) {
		size_t c = 0;
		for (size_t i = 0; i < n; i++) {
			c += (float)m[i * 4 + 3] * k > 127.5f;
		}
		return (float)c / (float)n;
	};
	float lo = 0.5f, hi = 4.0f;
	for (int it = 0; it < 12; it++) {
		const float k = 0.5f * (lo + hi);
		(cubre(k) < cobertura ? lo : hi) = k;
	}
	const float k = 0.5f * (lo + hi);
	for (size_t i = 0; i < n; i++) {
		m[i * 4 + 3] = (uint8_t)std::min(255.0f, (float)m[i * 4 + 3] * k + 0.5f);
	}
}

// ------------------------------------------------------------------------------------------------- en la GPU
class Bosque {
public:
	struct Rango { uint32_t primerIndice{ 0 }, indices{ 0 }, primeraInstancia{ 0 }, instancias{ 0 }; };
	std::array<Rango, NUM_MALLAS> rangos{};
	uint32_t totalInstancias{ 0 }, totalTriangulos{ 0 }, dibujosCuadro{ 0 }, dibujosSombra{ 0 };
	bool listo{ false };
	float msTexturas{ 0.0f };

	void preparar(vks::VulkanDevice* d, VkQueue cola, VkFormat formatoColor, VkFormat formatoProf, VkFormat formatoSombra, VkPipelineCache cache,
	              VkDescriptorSetLayout set0, const std::string& shaders, const std::string& assets, float hx_, float hz_)
	{
		disp = d;
		dev = d->logicalDevice;
		hx = hx_;
		hz = hz_;
		const std::string dir = assets + "bosque/";
		cargarTexturas(cola, dir);
		// modelos escaneados (Poly Haven)
		MallaArchivo helecho, rocas, tocon, tronco;
		helecho.leer(dir + "fern_02.malla");
		const bool hayRocas = rocas.leer(dir + "rock_moss_set_01.malla");
		tocon.leer(dir + "tree_stump_01.malla");
		tronco.leer(dir + "dead_tree_trunk.malla");
		// mallas
		Generador g;
		std::array<std::vector<Instancia>, NUM_MALLAS> inst;
		repartir(inst, hx, hz);
		for (int t = 0; t < NUM_MALLAS; t++) {
			const uint32_t desde = (uint32_t)g.idx.size();
			switch (t) {
			case ARBOL_A: arbolHaya(g, 11u, false, false); break;
			case ARBOL_B: arbolHaya(g, 23u, true, false); break;
			case PINO: abeto(g, 37u, false); break;
			case ARBOL_LEJOS: arbolHaya(g, 29u, true, true); break;
			case PINO_LEJOS: abeto(g, 43u, true); break;
			case ARBUSTO: arbusto(g, 41u); break;
			case HELECHO: helecho.agregar(g, 0, 2.3f, HOJA, C_HELECHO, glm::vec3(0.9f, 1.0f, 0.85f), true); break;
			case HELECHO_B: helecho.agregar(g, 1, 2.3f, HOJA, C_HELECHO, glm::vec3(1.0f, 1.0f, 0.9f), true); break;
			case HONGOS: hongos(g, 61u); break;
			case ROCA:
				if (hayRocas) {
					rocas.agregar(g, 1, 1.0f, PIEDRA, C_ROCA, glm::vec3(1.0f), false);
				} else {
					g.bulto(glm::vec3(0.0f, 0.4f, 0.0f), glm::vec3(1.6f, 1.0f, 1.3f), 2, PIEDRA, C_ROCA, 71u, 0.22f);
				}
				break;
			case ROCA_B: rocas.agregar(g, 3, 1.0f, PIEDRA, C_ROCA, glm::vec3(1.0f), false); break;
			case TOCON: tocon.agregar(g, 0, 1.0f, CORTEZA, C_TOCON, glm::vec3(1.0f), false); break;
			case TRONCO: tronco.agregar(g, 0, 1.0f, CORTEZA, C_TRONCO, glm::vec3(1.0f), false); break;
			case RAICES: raices(g, hx, hz, 83u); break;
			case SUELO: suelo(g); break;
			}
			rangos[t].primerIndice = desde;
			rangos[t].indices = (uint32_t)g.idx.size() - desde;
		}
		std::vector<Instancia> todas;
		for (int t = 0; t < NUM_MALLAS; t++) {
			rangos[t].primeraInstancia = (uint32_t)todas.size();
			rangos[t].instancias = (uint32_t)inst[t].size();
			todas.insert(todas.end(), inst[t].begin(), inst[t].end());
			totalTriangulos += rangos[t].indices / 3 * rangos[t].instancias;
		}
		totalInstancias = (uint32_t)todas.size();
		const VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, visible, &vertices, g.v.size() * sizeof(Vertice), g.v.data()));
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, visible, &indices, g.idx.size() * sizeof(uint32_t), g.idx.data()));
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, visible, &instancias, todas.size() * sizeof(Instancia), todas.data()));
		crearPipelines(formatoColor, formatoProf, formatoSombra, cache, set0, shaders);
		listo = true;
	}

	~Bosque()
	{
		if (!dev) {
			return;
		}
		vertices.destroy();
		indices.destroy();
		instancias.destroy();
		for (VkPipeline p : { pMalla, pSombra, pCesped, pCielo, pParticulas }) {
			vkDestroyPipeline(dev, p, nullptr);
		}
		vkDestroyPipelineLayout(dev, layout, nullptr);
		vkDestroyDescriptorPool(dev, pool, nullptr);
		vkDestroyDescriptorSetLayout(dev, layoutTexturas, nullptr);
		vkDestroySampler(dev, muestreador, nullptr);
		vkDestroyImageView(dev, vistaTexturas, nullptr);
		vkDestroyImage(dev, imagenTexturas, nullptr);
		vkFreeMemory(dev, memoriaTexturas, nullptr);
	}

	// el bosque en el mapa de sombras (arboles, abetos, arbustos, rocas, tocones y troncos; el follaje con su alfa)
	void sombra(VkCommandBuffer cmd, VkDescriptorSet set0)
	{
		if (!listo) {
			return;
		}
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pSombra);
		enlazarSets(cmd, set0);
		enlazarMallas(cmd);
		dibujosSombra = 0;
		empujar(cmd, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
		for (TipoMalla t : { ARBOL_A, ARBOL_B, PINO, ARBUSTO, ROCA, ROCA_B, TOCON, TRONCO }) {
			dibujarMalla(cmd, t);
			dibujosSombra++;
		}
	}

	// lo solido: suelo, mallas, raices, cesped y el fondo de bosque profundo (va ultimo: solo pinta lo que queda)
	void opaco(VkCommandBuffer cmd, VkDescriptorSet set0, const glm::mat4& inversaVP, float ambiente)
	{
		if (!listo) {
			return;
		}
		dibujosCuadro = 0;
		enlazarSets(cmd, set0);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pMalla);
		enlazarMallas(cmd);
		empujar(cmd, glm::vec4(2.0f, 0.0f, 0.0f, 0.0f));
		dibujarMalla(cmd, SUELO);
		empujar(cmd, glm::vec4(0.0f));
		// primero lo opaco (rocas, troncos), despues lo que tiene alfa (arboles con follaje, plantas)
		for (TipoMalla t : { ROCA, ROCA_B, TOCON, TRONCO, HONGOS, ARBOL_A, ARBOL_B, PINO, ARBOL_LEJOS, PINO_LEJOS, ARBUSTO, HELECHO, HELECHO_B }) {
			dibujarMalla(cmd, t);
		}
		empujar(cmd, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		dibujarMalla(cmd, RAICES);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pCesped);
		empujar(cmd, glm::vec4(3.0f, 0.0f, hx, hz));
		vkCmdDraw(cmd, 15, BRIZNAS, 0, 0);
		dibujosCuadro++;
		if (ambiente > 0.0f) {
			struct { glm::mat4 inversa; glm::vec4 p; } e{ inversaVP, glm::vec4(ambiente, 0.0f, 0.0f, 0.0f) };
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pCielo);
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(e), &e);
			vkCmdDraw(cmd, 3, 1, 0, 0);
			dibujosCuadro++;
		}
	}

	// hojas que caen, luciernagas, polen, niebla y rayos de sol (transparentes: despues de lo opaco)
	void particulas(VkCommandBuffer cmd, VkDescriptorSet set0, float ambiente)
	{
		if (!listo || ambiente <= 0.0f) {
			return;
		}
		enlazarSets(cmd, set0);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pParticulas);
		for (int t : { (int)NIEBLA, (int)RAYOS, (int)HOJAS, (int)POLEN, (int)LUCIERNAGAS }) {
			empujar(cmd, glm::vec4((float)t, ambiente, hx, hz));
			vkCmdDraw(cmd, 6, CUANTAS_PARTICULAS[t], 0, 0);
			dibujosCuadro++;
		}
	}

	void informe() const
	{
		uint32_t particulas = 0;
		for (uint32_t c : CUANTAS_PARTICULAS) {
			particulas += c;
		}
		fprintf(stderr, "[bosque] mallas: %u instancias, %u triangulos (sin cesped) en %d dibujos; cesped: %u briznas (%u triangulos), "
		        "particulas: %u en %d dibujos; fondo: 1 dibujo; sombras: 8 dibujos; texturas: %d capas de %u (%.0f MB con mips) en %.0f ms\n",
		        totalInstancias, totalTriangulos, (int)NUM_MALLAS, BRIZNAS, BRIZNAS * 5u, particulas, (int)NUM_PARTICULAS, (int)NUM_CAPAS, TAM_TEXTURA,
		        (double)NUM_CAPAS * TAM_TEXTURA * TAM_TEXTURA * 4.0 * 4.0 / 3.0 / 1048576.0, msTexturas);
		const char* nombres[NUM_MALLAS] = { "haya", "haya alta", "abeto", "haya lejana", "abeto lejano", "arbusto", "helecho", "helecho b",
			                                "hongos", "roca", "roca b", "tocon", "tronco caido", "raices", "suelo" };
		for (int t = 0; t < NUM_MALLAS; t++) {
			fprintf(stderr, "[bosque]   %-13s %6u triangulos x %3u = %7u\n", nombres[t], rangos[t].indices / 3, rangos[t].instancias,
			        rangos[t].indices / 3 * rangos[t].instancias);
		}
	}

private:
	vks::VulkanDevice* disp{ nullptr };
	VkDevice dev{ VK_NULL_HANDLE };
	float hx{ 13.44f }, hz{ 10.08f };
	vks::Buffer vertices, indices, instancias;
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	VkPipeline pMalla{ VK_NULL_HANDLE }, pSombra{ VK_NULL_HANDLE }, pCesped{ VK_NULL_HANDLE }, pCielo{ VK_NULL_HANDLE }, pParticulas{ VK_NULL_HANDLE };
	VkImage imagenTexturas{ VK_NULL_HANDLE };
	VkDeviceMemory memoriaTexturas{ VK_NULL_HANDLE };
	VkImageView vistaTexturas{ VK_NULL_HANDLE };
	VkSampler muestreador{ VK_NULL_HANDLE };
	VkDescriptorPool pool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout layoutTexturas{ VK_NULL_HANDLE };
	VkDescriptorSet setTexturas{ VK_NULL_HANDLE };

	void empujar(VkCommandBuffer cmd, glm::vec4 m)
	{
		vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &m);
	}

	void enlazarSets(VkCommandBuffer cmd, VkDescriptorSet set0)
	{
		const VkDescriptorSet sets[2] = { set0, setTexturas };
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets, 0, nullptr);
	}

	void enlazarMallas(VkCommandBuffer cmd)
	{
		const VkBuffer b[2] = { vertices.buffer, instancias.buffer };
		const VkDeviceSize o[2] = { 0, 0 };
		vkCmdBindVertexBuffers(cmd, 0, 2, b, o);
		vkCmdBindIndexBuffer(cmd, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	}

	void dibujarMalla(VkCommandBuffer cmd, TipoMalla t)
	{
		const Rango& r = rangos[t];
		if (r.instancias == 0 || r.indices == 0) {
			return;
		}
		vkCmdDrawIndexed(cmd, r.indices, r.instancias, r.primerIndice, 0, r.primeraInstancia);
		dibujosCuadro++;
	}

	// Las 24 capas (png RGBA de bosque_insumos.py) en un arreglo 2D con mipmaps hechos aqui (promedio 2x2; las
	// normales se renormalizan y el alfa del follaje conserva su cobertura). Si falta una capa queda neutra
	void cargarTexturas(VkQueue cola, const std::string& dir)
	{
		const auto t0 = std::chrono::steady_clock::now();
		const uint32_t T = TAM_TEXTURA;
		uint32_t mips = 1;
		while ((T >> mips) > 0) {
			mips++;
		}
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
			const bool alfa = !normal && (c == C_HOJAS || c == C_ABETO || c == C_ARBUSTO || c == C_HELECHO);
			const std::string ruta = dir + NOMBRE_CAPA[c / 2] + (normal ? "_normal.png" : "_color.png");
			std::vector<uint8_t> m0((size_t)T * T * 4);
			int w = 0, h = 0, n = 0;
			stbi_uc* px = stbi_load(ruta.c_str(), &w, &h, &n, 4);
			if (px) {
				for (uint32_t y = 0; y < T; y++) {   // si no mide T x T, al vecino mas cercano
					for (uint32_t x = 0; x < T; x++) {
						std::memcpy(&m0[((size_t)y * T + x) * 4], &px[(((size_t)(y * (uint32_t)h / T)) * (uint32_t)w + x * (uint32_t)w / T) * 4], 4);
					}
				}
				stbi_image_free(px);
			} else {
				faltan++;
				const uint8_t neutro[4] = { (uint8_t)(normal ? 128 : 110), (uint8_t)(normal ? 128 : 100), (uint8_t)(normal ? 255 : 80), (uint8_t)(normal ? 200 : 255) };
				for (size_t i = 0; i < (size_t)T * T; i++) {
					std::memcpy(&m0[i * 4], neutro, 4);
				}
			}
			float cobertura = 0.0f;
			if (alfa) {
				size_t k = 0;
				for (size_t i = 0; i < (size_t)T * T; i++) {
					k += m0[i * 4 + 3] > 127;
				}
				cobertura = (float)k / (float)((size_t)T * T);
			}
			VkDeviceSize ofs = porCapa * c;
			std::vector<uint8_t> act = m0;
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
									for (int k = 0; k < 4; k++) {
										acc[k] += q[k];
									}
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
								for (int k = 0; k < 4; k++) {
									o[k] = (uint8_t)(acc[k] * 0.25f + 0.5f);
								}
							}
						}
					}
					act.swap(sig);
					if (alfa) {
						mipAlfaConCobertura(act, cobertura);
					}
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
		if (faltan > 0) {
			fprintf(stderr, "[bosque] faltan %d capas de textura en %s (correr bosque_insumos.py): quedan neutras\n", faltan, dir.c_str());
		}
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
		std::vector<VkDescriptorPoolSize> tam = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo pi = vks::initializers::descriptorPoolCreateInfo(tam, 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(dev, &pi, nullptr, &pool));
		const std::vector<VkDescriptorSetLayoutBinding> enlaces = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo li = vks::initializers::descriptorSetLayoutCreateInfo(enlaces);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev, &li, nullptr, &layoutTexturas));
		VkDescriptorSetAllocateInfo ai = vks::initializers::descriptorSetAllocateInfo(pool, &layoutTexturas, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(dev, &ai, &setTexturas));
		VkDescriptorImageInfo img{ muestreador, vistaTexturas, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet w = vks::initializers::writeDescriptorSet(setTexturas, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &img);
		vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
		msTexturas = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}

	VkPipelineShaderStageCreateInfo etapa(const std::string& ruta, VkShaderStageFlagBits s, std::vector<VkShaderModule>& modulos)
	{
		VkShaderModule m = vks::tools::loadShader(ruta.c_str(), dev);
		modulos.push_back(m);
		return { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, s, m, "main", nullptr };
	}

	void crearPipelines(VkFormat formatoColor, VkFormat formatoProf, VkFormat formatoSombra, VkPipelineCache cache, VkDescriptorSetLayout set0,
	                    const std::string& shaders)
	{
		VkPushConstantRange empuje = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), 0);
		const VkDescriptorSetLayout sets[2] = { set0, layoutTexturas };
		VkPipelineLayoutCreateInfo pl = vks::initializers::pipelineLayoutCreateInfo(sets, 2);
		pl.pushConstantRangeCount = 1;
		pl.pPushConstantRanges = &empuje;
		VK_CHECK_RESULT(vkCreatePipelineLayout(dev, &pl, nullptr, &layout));

		const std::array<VkVertexInputBindingDescription, 2> enlaces = { {
			{ 0, sizeof(Vertice), VK_VERTEX_INPUT_RATE_VERTEX },
			{ 1, sizeof(Instancia), VK_VERTEX_INPUT_RATE_INSTANCE },
		} };
		const std::array<VkVertexInputAttributeDescription, 6> atributos = { {
			{ 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertice, pos) },
			{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertice, normal) },
			{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertice, color) },
			{ 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instancia, a) },
			{ 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instancia, b) },
			{ 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertice, uv) },
		} };
		VkPipelineVertexInputStateCreateInfo entradaMalla = vks::initializers::pipelineVertexInputStateCreateInfo();
		entradaMalla.vertexBindingDescriptionCount = (uint32_t)enlaces.size();
		entradaMalla.pVertexBindingDescriptions = enlaces.data();
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
		VkPipelineDepthStencilStateCreateInfo prof = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineDepthStencilStateCreateInfo profSinEscribir = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo vista = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo muestras = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dinamicos = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dinamico = vks::initializers::pipelineDynamicStateCreateInfo(dinamicos);
		std::vector<VkShaderModule> modulos;
		const std::string dir = shaders + "yugireload/";
		const VkPipelineShaderStageCreateInfo vMalla = etapa(dir + "bosque.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos);
		const VkPipelineShaderStageCreateInfo fMalla = etapa(dir + "bosque.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos);
		const VkPipelineShaderStageCreateInfo fSombra = etapa(dir + "bosque_sombra.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos);
		const VkPipelineShaderStageCreateInfo vCesped = etapa(dir + "bosque_cesped.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos);
		const VkPipelineShaderStageCreateInfo vCielo = etapa(dir + "bosque_cielo.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos);
		const VkPipelineShaderStageCreateInfo fCielo = etapa(dir + "bosque_cielo.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos);
		const VkPipelineShaderStageCreateInfo vPart = etapa(dir + "bosque_particula.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos);
		const VkPipelineShaderStageCreateInfo fPart = etapa(dir + "bosque_particula.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos);

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
		// cesped: sin vertices, el mismo fragmento
		etapas = { vCesped, fMalla };
		ci.pVertexInputState = &sinVertices;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pCesped));
		// fondo: en la profundidad maxima, sin escribirla
		etapas = { vCielo, fCielo };
		ci.pDepthStencilState = &profSinEscribir;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pCielo));
		// particulas: alfa premultiplicado (alfa 0 = suma luz)
		etapas = { vPart, fPart };
		ci.pColorBlendState = &mezclaPremult;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pParticulas));
		// sombras: el vertice (modo.y = 1 proyecta con la luz) y un fragmento que solo recorta el alfa del follaje
		VkPipelineRenderingCreateInfo renderSombra{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 0,
			.depthAttachmentFormat = formatoSombra,
		};
		VkPipelineColorBlendStateCreateInfo sinColor = vks::initializers::pipelineColorBlendStateCreateInfo(0, nullptr);
		raster.depthBiasEnable = VK_TRUE;
		raster.depthBiasConstantFactor = 1.25f;
		raster.depthBiasSlopeFactor = 1.75f;
		etapas = { vMalla, fSombra };
		ci.pNext = &renderSombra;
		ci.pVertexInputState = &entradaMalla;
		ci.pDepthStencilState = &prof;
		ci.pColorBlendState = &sinColor;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pSombra));
		for (VkShaderModule m : modulos) {
			vkDestroyShaderModule(dev, m, nullptr);
		}
	}
};

}   // namespace bosque
