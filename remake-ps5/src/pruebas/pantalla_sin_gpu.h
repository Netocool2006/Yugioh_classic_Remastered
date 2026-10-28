// Rama classic-jugador-humano: LO QUE SE DIBUJA, sin GPU. Arma la interfaz de VERDAD del juego (interfaz.h y todos sus
// *_ui.h: mesa, manos, menus, carteles, avisos, ayudas, pausa, selector de fases, moneda, resultado...) sobre el juego
// sin GPU de cu_juego.h y, en cada cuadro, corre lo mismo que yugireload.cpp antes de mandar a la GPU: actualizar() y
// construir(). No se sube nada a la GPU: se leen los vertices armados (cartas con su cara o dorso y su rectangulo en
// pantalla, paneles, el recuadro dorado) y los textos e iconos de botones que se dibujaron (el gancho alDibujarTexto de
// interfaz.h, que solo existe con YUGI_REGISTRO_DIBUJO). De eso sale el Cuadro: lo que un jugador ve en ese cuadro.
//
// Lo que NO es igual a la consola (documentado en docs/jugador_humano.md): sin texturas no hay sprites de Master Duel
// (se ve el respaldo dibujado por codigo que el juego usa si faltan), ni retratos, ni la escena 3D (figuras, entornos,
// efectos del pack): solo la interfaz. La camara es la de juego (CINE) quieta, sin respirar.
#pragma once

// ---- antes de la interfaz: Vulkan (solo tipos; nunca se llama), una maqueta de la base vks y el pad de platform.h
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace vks {
struct VulkanDevice;
struct Buffer {
	VkBuffer buffer{ VK_NULL_HANDLE };
	VkDeviceMemory memory{ VK_NULL_HANDLE };
	VkDescriptorBufferInfo descriptor{};
	VkDeviceSize size{ 0 };
	void* mapped{ nullptr };
	VkResult map(VkDeviceSize = VK_WHOLE_SIZE, VkDeviceSize = 0) { return VK_SUCCESS; }
	void unmap() {}
	void destroy() {}
};
struct Texture {
	VulkanDevice* device{ nullptr };
	VkImage image{ VK_NULL_HANDLE };
	VkImageLayout imageLayout{};
	VkDeviceMemory deviceMemory{ VK_NULL_HANDLE };
	VkImageView view{ VK_NULL_HANDLE };
	uint32_t width{ 0 }, height{ 0 }, mipLevels{ 1 }, layerCount{ 1 };
	VkFormat format{ VK_FORMAT_UNDEFINED };
	VkDescriptorImageInfo descriptor{};
	VkSampler sampler{ VK_NULL_HANDLE };
	void updateDescriptor() {}
	void destroy() {}
};
struct Texture2D : Texture {
	void fromBuffer(void*, VkDeviceSize, VkFormat, uint32_t w, uint32_t h, VulkanDevice*, VkQueue, VkFilter = VK_FILTER_LINEAR,
	                VkImageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT, VkImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		width = w, height = h;
	}
};
struct VulkanDevice {
	VkPhysicalDevice physicalDevice{ VK_NULL_HANDLE };
	VkDevice logicalDevice{ VK_NULL_HANDLE };
	VkPhysicalDeviceFeatures enabledFeatures{};
	VkPhysicalDeviceProperties properties{};
	VkResult createBuffer(VkBufferUsageFlags, VkMemoryPropertyFlags, Buffer*, VkDeviceSize, void* = nullptr) { return VK_SUCCESS; }
	VkResult createBuffer(VkBufferUsageFlags, VkMemoryPropertyFlags, VkDeviceSize, VkBuffer*, VkDeviceMemory*, void* = nullptr) { return VK_SUCCESS; }
	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel, bool = false) { return VK_NULL_HANDLE; }
	void flushCommandBuffer(VkCommandBuffer, VkQueue, bool = true) {}
	uint32_t getMemoryType(uint32_t, VkMemoryPropertyFlags, VkBool32* = nullptr) const { return 0; }
};
namespace tools {
inline void setImageLayout(VkCommandBuffer, VkImage, VkImageLayout, VkImageLayout, VkImageSubresourceRange,
                           VkPipelineStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) {}
inline void setImageLayout(VkCommandBuffer, VkImage, VkImageAspectFlags, VkImageLayout, VkImageLayout,
                           VkPipelineStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) {}
inline VkShaderModule loadShader(const char*, VkDevice) { return VK_NULL_HANDLE; }
}
}
#include "VulkanInitializers.hpp"   // (la de la base: solo arma estructuras)
#define VK_CHECK_RESULT(f) ((void)(f))
constexpr uint32_t maxConcurrentFrames{ 2 };
enum : uint32_t {
	PAD_L3 = 0x000002, PAD_R3 = 0x000004, PAD_OPTIONS = 0x000008, PAD_UP = 0x000010, PAD_RIGHT = 0x000020, PAD_DOWN = 0x000040, PAD_LEFT = 0x000080,
	PAD_L1 = 0x000400, PAD_R1 = 0x000800, PAD_TRIANGLE = 0x001000, PAD_CIRCLE = 0x002000, PAD_CROSS = 0x004000, PAD_SQUARE = 0x008000,
	PAD_TOUCH_PAD = 0x100000,
};
struct pad { uint32_t held{ 0 }, pressed{ 0 }; float left_x{ 0 }, left_y{ 0 }, right_x{ 0 }, right_y{ 0 }, l2{ 0 }, r2{ 0 }; };

// la interfaz de verdad, con todo a la vista (la prueba lee sus vertices y su estado)
#define YUGI_REGISTRO_DIBUJO 1
#define private public
#define protected public
#include "../interfaz.h"
#undef private
#undef protected
#include "../orientacion_figura.h"

#define CU_INTERFAZ_REAL 1
#include "cu_front_total.h"

namespace pantalla {

constexpr float ANCHO = 1920.0f, ALTO = 1080.0f;

struct Texto {
	std::string s;
	float x{ 0 }, y{ 0 }, tam{ 0 };   // x: donde empieza (ya alineado); y: la base de la linea
	float w{ 0 };                     // ancho aproximado
	glm::vec4 col{ 1.0f };
	size_t v{ 0 };                    // orden de dibujo
	bool icono{ false };              // "[X]", "[O]", "[OPTIONS]"...: un icono de boton
	bool tapado{ false };
	float cx() const { return x + w * 0.5f; }
	float cy() const { return y - tam * 0.35f; }
};
struct Rect {
	float x0{ 0 }, y0{ 0 }, x1{ 0 }, y1{ 0 };
	float alfa{ 0 };
	size_t v{ 0 };
	int tipo{ 0 };
	float cx() const { return (x0 + x1) * 0.5f; }
	float cy() const { return (y0 + y1) * 0.5f; }
	bool contiene(float x, float y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
	float area() const { return std::max(0.0f, x1 - x0) * std::max(0.0f, y1 - y0); }
};
struct Carta : Rect {
	std::string id;   // "" = dorso
	bool en3D{ false }, tapada{ false };
};

// lo que se ve en un cuadro
struct Cuadro {
	int n{ 0 };
	float t{ 0 };
	std::vector<Texto> textos;
	std::vector<Carta> cartas;
	std::vector<Rect> paneles;
	bool hayMarco{ false };
	Rect marco;   // el recuadro dorado (donde esta el foco)

	static bool visible(float alfa, float x, float y) { return alfa >= 0.25f && x > -5.0f && x < ANCHO + 5.0f && y > -5.0f && y < ALTO + 5.0f; }
	// los textos legibles (no iconos)
	std::vector<const Texto*> legibles() const
	{
		std::vector<const Texto*> r;
		for (const Texto& t : textos)
			if (!t.icono && !t.tapado && visible(t.col.a, t.cx(), t.cy()) && !t.s.empty()) r.push_back(&t);
		return r;
	}
	const Texto* buscar(const std::string& sub) const
	{
		for (const Texto& t : textos)
			if (!t.tapado && visible(t.col.a, t.cx(), t.cy()) && t.s.find(sub) != std::string::npos) return &t;
		return nullptr;
	}
	bool hay(const std::string& sub) const { return buscar(sub) != nullptr; }
	// la carta (cara) con ese id a la vista
	const Carta* carta(const std::string& id) const
	{
		const Carta* mejor = nullptr;
		for (const Carta& c : cartas)
			if (c.id == id && !c.tapada && visible(c.alfa, c.cx(), c.cy()) && (!mejor || c.area() > mejor->area())) mejor = &c;
		return mejor;
	}
	// los iconos de boton a la vista
	std::vector<const Texto*> iconos() const
	{
		std::vector<const Texto*> r;
		for (const Texto& t : textos)
			if (t.icono && !t.tapado && visible(t.col.a, t.x, t.y)) r.push_back(&t);
		return r;
	}
	// el texto que sigue a un icono en su misma fila (la ayuda: "[X] Elegir")
	const Texto* textoDe(const Texto& icono) const
	{
		const Texto* mejor = nullptr;
		for (const Texto& t : textos) {
			if (t.icono || t.tapado || t.col.a < 0.25f) continue;
			if (fabsf(t.cy() - icono.y) > icono.tam * 0.9f) continue;
			if (t.x < icono.x || t.x > icono.x + icono.tam * 3.0f) continue;
			if (!mejor || t.x < mejor->x) mejor = &t;
		}
		return mejor;
	}
	// todo el texto legible en una linea (para los informes)
	std::string resumen(size_t max = 400) const
	{
		std::string r;
		for (const Texto* t : legibles()) {
			if (r.size() > max) break;
			r += (r.empty() ? "" : " | ") + t->s;
		}
		return r;
	}
};

// ---------------------------------------------------------------------------------------------------- la pantalla
class Pantalla {
public:
	interfaz::Interfaz ui;
	Juego& g;
	Cuadro cuadro;
	int n{ 0 };
	glm::mat4 vp{ 1.0f };
	VkDescriptorSet setAtlasFalso{ (VkDescriptorSet)(uintptr_t)0x10 };
	std::unordered_map<uintptr_t, int> cartaDeSet;   // set falso -> carta
	explicit Pantalla(Juego& j) : g(j) {}

	// todas las caras "cargadas" (otra vez si el idioma cambia: procesarCargas las suelta)
	void marcarCartas()
	{
		for (size_t i = 0; i < ui.cartas.size(); i++) {
			interfaz::CartaUI& k = ui.cartas[i];
			k.tieneCara = k.tieneMini = k.lista = false;
			k.falloCara = k.falloMini = k.quiereCara = k.quiereMini = false;
			k.cara.width = 512, k.cara.height = 746;
			k.mini.width = 256, k.mini.height = 373;
			k.setCara = falso(2 * i);
			k.setMini = falso(2 * i + 1);
			cartaDeSet[(uintptr_t)k.setCara] = (int)i;
			cartaDeSet[(uintptr_t)k.setMini] = (int)i;
		}
		ui.versionCaras = constructor::versionIdioma();
	}
	static VkDescriptorSet falso(uintptr_t k) { return (VkDescriptorSet)(uintptr_t)(0x100000u + k); }

	// como Interfaz::preparar sin la GPU: datos de cartas, fuentes (atlas SDF en la CPU), sprites de MD que haya (no hay
	// textura: se usan los respaldos por codigo) y todas las caras "cargadas" con un set falso por carta (asi se dibujan
	// con su cara y se sabe cual es cual)
	void preparar(const std::string& assets)
	{
		ui.rutaAssets = assets;
		ui.leerEntorno();
		ui.cargarCartas();
		ui.crearAtlas();
		ui.setAtlas = setAtlasFalso;
		ui.efMDCargado = true;
		ui.pistasMDSinGPU = true;   // (las pistas de MD se leen, sin subir su atlas)   // (rama classic-ps5-efectos: sin GPU no se sube md_efectos.png: halo y espadas por codigo)
		marcarCartas();
		ui.portadaFalta = ui.menuFalta = true;
		ui.v.reserve(interfaz::MAX_VERTICES);
		ui.listo = true;
		ui.conectarMando(&g.ctl);
		ui.consulta_ = &g.consulta;
		ui.conectarConstructor(&g.constructorMazo);
		ui.statsZona = g.interfazDuelo.statsZona;
		ui.contadorZona = [this](int z, int& k, bool& turnos) { return g.contadorZonaMD(z, k, turnos); };
		ui.figuraEnZona = [](int) { return false; };
		g.interfazDuelo.nombreReal = [this](const std::string& id) {
			const int ci = ui.buscarCarta(id);
			return ci >= 0 ? ui.cartas[(size_t)ci].vNombre() : id;
		};
		ui.alDibujarTexto = [this](const std::string& s, float x, float y, float tam, glm::vec4 col, int alin, size_t v) {
			Texto t;
			t.s = s;
			t.icono = s.size() > 2 && s.front() == '[' && s.back() == ']';
			t.tam = tam;
			t.col = col;
			t.v = v;
			if (t.icono) {
				t.x = x, t.y = y, t.w = tam;
			} else {
				t.w = ui.medir(interfaz::F_TITULO, s, tam);
				t.x = alin == 1 ? x - t.w * 0.5f : alin == 2 ? x - t.w : x;
				t.y = y;
			}
			cuadro.textos.push_back(std::move(t));
		};
		// la camara de juego (yugireload.cpp updateUniformBuffers, modo CINE, sin respirar ni ataque)
		const float aspecto = ANCHO / ALTO;
		const float fovV = 2.0f * atanf(tanf(glm::radians(62.0f) * 0.5f) / aspecto);
		glm::mat4 proy = glm::perspectiveRH(fovV, aspecto, 0.1f, 80.0f);
		proy[1][1] *= -1.0f;
		proy = ui.ajustarProyeccion(proy, ANCHO, ALTO);
		const glm::vec3 ojo = orientacion::ojoJuego();
		glm::vec3 mira = glm::vec3(0.0f, 0.0f, -0.6f) * orientacion::ESCALA_TABLERO;
		{
			const glm::vec3 d = mira - ojo;
			const float h = glm::length(glm::vec2(d.x, d.z));
			const float p = atan2f(d.y, h) + glm::radians(7.0f);
			mira = ojo + glm::vec3(d.x / h, 0.0f, d.z / h) * (glm::length(d) * cosf(p)) + glm::vec3(0.0f, glm::length(d) * sinf(p), 0.0f);
		}
		vp = proy * glm::lookAtRH(ojo, mira, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	// lo de yugireload.cpp en cada cuadro: los LP y mazos que el juego le pasa (aca los toma de la maqueta que llama el
	// juego), el golpe del ataque, la camara; actualizar(); y lo que dibujar() arma antes de la GPU
	int lpVistos[2]{ -1, -1 };
	int mazosVistos[3]{ -9, -9, -9 };
	float negroVisto{ -1.0f };
	void dibujar(float dt)
	{
		for (int lado = 0; lado < 2; lado++) {
			const int v = g.interfazDuelo.lps[lado];
			if (v != lpVistos[lado]) {
				ui.cambiarLP(lado == 1, v);
				lpVistos[lado] = v;
			}
		}
		if (memcmp(mazosVistos, g.interfazDuelo.mazos, sizeof(mazosVistos)) != 0) {
			memcpy(mazosVistos, g.interfazDuelo.mazos, sizeof(mazosVistos));
			ui.contarMazos(mazosVistos[0], mazosVistos[1], mazosVistos[2]);
		}
		// (portada.h lo pone al cambiar; despues la interfaz lo desvanece sola: solo se copia cuando el juego lo cambia)
		if (g.interfazDuelo.portadaNegro != negroVisto) ui.portadaNegro = negroVisto = g.interfazDuelo.portadaNegro;
		interfaz::Interfaz::Golpe gp;
		if (g.hayAtaque()) {
			gp = { g.zonaAtacante, g.zonaObjetivo, g.tAtaque, g.danio, g.muereA, g.muereO, g.ataqueAltoMD };
			gp.directo = g.ataqueDirecto;   // (rama classic-ps5-ataque: como yugireload.cpp avanzarAtaque)
			gp.punto = g.puntoDirecto;
			gp.alJugador = !g.danioAlRival;
		}
		ui.golpePlano(gp);
		ui.rotulosVisibles(true);
		ui.selectorCamara(g.modoCamara, g.tCambioCamara, 0);
		ui.versionCaras = constructor::versionIdioma();   // (sin GPU no hay caras que soltar al cambiar de idioma)
		pad p;
		// (las caras que la interfaz pidio en el cuadro anterior quedan "cargadas", como en la consola un cuadro despues; su
		// cache expulsa las que no se usan, igual que alla)
		for (size_t i = 0; i < ui.cartas.size(); i++) {
			interfaz::CartaUI& k = ui.cartas[i];
			if (k.quiereCara) k.tieneCara = true, k.quiereCara = false, k.setCara = falso(2 * i), k.cara.width = 512, k.cara.height = 746;
			if (k.quiereMini) k.tieneMini = true, k.quiereMini = false, k.setMini = falso(2 * i + 1), k.mini.width = 256, k.mini.height = 373;
			k.lista = k.tieneCara && k.tieneMini;
		}
		ui.actualizar(p, dt, true);
		// dibujar() sin la GPU
		ui.W = ANCHO, ui.H = ALTO, ui.s = ALTO / 1080.0f;
		ui.vp = vp;
		ui.v.clear();
		ui.lotes.clear();
		cuadro = Cuadro{};
		cuadro.n = ++n;
		cuadro.t = ui.t;
		ui.construirObjetivos();
		ui.construir();
		leer();
	}

private:
	static glm::vec2 aPantalla(const glm::vec4& c)
	{
		const float w = std::max(c.w, 1e-4f);
		return glm::vec2((c.x / w * 0.5f + 0.5f) * ANCHO, (c.y / w * 0.5f + 0.5f) * ALTO);
	}
	// los vertices armados: cartas (cara o dorso), paneles y el recuadro dorado
	void leer()
	{
		const std::vector<interfaz::Vertice>& v = ui.v;
		for (size_t l = 0; l < ui.lotes.size(); l++) {
			const size_t desde = ui.lotes[l].desde, hasta = l + 1 < ui.lotes.size() ? ui.lotes[l + 1].desde : v.size();
			const uintptr_t set = (uintptr_t)ui.lotes[l].set;
			auto it = cartaDeSet.find(set);
			for (size_t q = desde; q + 6 <= hasta; q += 6) {
				const int tipo = (int)lroundf(v[q].forma.w);
				if (tipo == interfaz::TEXTO || tipo == interfaz::PARTICULA || tipo == interfaz::FONDO || tipo == interfaz::LINEA) continue;
				// esquinas: 0, 1, 2 y 5 (orden 0 1 2 0 2 3)
				const glm::vec4 c[4] = { v[q].pos, v[q + 1].pos, v[q + 2].pos, v[q + 5].pos };
				Rect r;
				r.x0 = r.y0 = 1e9f, r.x1 = r.y1 = -1e9f;
				bool detras = false;
				for (const glm::vec4& k : c) {
					if (k.w <= 1e-4f) detras = true;
					const glm::vec2 p = aPantalla(k);
					r.x0 = std::min(r.x0, p.x), r.y0 = std::min(r.y0, p.y), r.x1 = std::max(r.x1, p.x), r.y1 = std::max(r.y1, p.y);
				}
				if (detras) continue;
				r.alfa = v[q].color.a;
				r.v = q;
				r.tipo = tipo;
				if (tipo == interfaz::IMAGEN && it != cartaDeSet.end()) {
					Carta k;
					static_cast<Rect&>(k) = r;
					k.id = ui.cartas[(size_t)it->second].id;
					k.en3D = ui.lotes[l].en3D;
					cuadro.cartas.push_back(k);
				} else if (tipo == interfaz::DORSO) {
					Carta k;
					static_cast<Rect&>(k) = r;
					k.en3D = ui.lotes[l].en3D;
					cuadro.cartas.push_back(k);
				} else if (tipo == interfaz::PANEL) {
					if (r.alfa >= 0.8f && r.area() > 400.0f) cuadro.paneles.push_back(r);
				} else if (tipo == interfaz::MARCO && fabsf(v[q].forma.x - v[q].forma.y) > 1.0f && !cuadro.hayMarco) {
					// (el recuadro dorado del foco: el primero rectangular; los redondos son la fase activa, el engranaje y el selector)
					cuadro.hayMarco = true;
					cuadro.marco = r;
				}
			}
		}
		// lo tapado: un panel casi opaco dibujado despues que cubre el centro
		// (minimo: un panel mas chico que lo tapado es parte de su dibujo, p. ej. la cruceta de un icono)
		auto tapa = [&](size_t v0, float x, float y, float minimo) {
			for (const Rect& p : cuadro.paneles)
				if (p.v > v0 && p.contiene(x, y) && p.area() > minimo && p.area() < ANCHO * ALTO * 1.01f) return true;
			return false;
		};
		for (Texto& t : cuadro.textos)
			t.tapado = t.icono ? tapa(t.v, t.x, t.y, t.tam * t.tam * 4.0f) : tapa(t.v, t.cx(), t.cy(), std::max(400.0f, t.w * t.tam * 1.5f));
		for (Carta& c : cuadro.cartas) c.tapada = tapa(c.v, c.cx(), c.cy(), c.area() * 0.8f);
	}
};

}  // namespace pantalla
