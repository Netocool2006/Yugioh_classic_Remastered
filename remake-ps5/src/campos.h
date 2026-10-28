// Yugioh Classic Remaster - cartas de campo (rama classic-campos): cada una de las 25 cartas de campo cambia el
// escenario completo al activarse, como ya hacia el Bosque (bosque.h, que queda igual):
//  - el FONDO: el del rival se cambia por el de la carta (entornos.h: pedirCampo("campo_<carta>"), un matte painting de
//    Gemini como los de los personajes; mientras falte, un respaldo ya hecho de datos/fondos.txt), con el fundido de
//    siempre y la imagen decodificada en otro hilo; al destruirse vuelve el del personaje;
//  - el TABLERO: una capa sobre el tapete (campos.frag) con el material de la carta: texturas PBR CC0 de Poly Haven
//    (campos_insumos.py: cesped, roca, tierra agrietada, pizarra, arena, losas, marmol, tierra quemada, metal, piedra
//    de mar, suelo de selva, acantilado; 512x512, 24 capas con mipmaps, ~34 MB) y lo propio de cada carta por codigo
//    (agua con olas, reflejos y causticas, grietas de lava, filamentos de plasma, el circulo del Oricalco, jeroglificos,
//    vortice, anillos, nubes, niebla...). Crece y se retira con la MISMA onda del Bosque (ubo.bosque y, z, w desde la
//    carta). Las casillas quedan a la vista: dentro se oscurece el material y el marco brilla con el color del campo;
//  - PARTICULAS cercanas sutiles (campos_particula.*: gotas, brasas, chispas, hojas, plumas, polvo, niebla, burbujas)
//    a ras del tapete, entre las casillas (sobre una casilla se apagan): no tapan las cartas ni el HUD.
// Lado: la ultima carta de campo activada manda y cambia TODO el escenario (fondo y tablero entero), sea del jugador o
// del rival (en las reglas Goat solo hay una carta de campo a la vez: activar otra destruye la anterior); la onda sale
// de la zona de campo de quien la activo. El juego (yugireload.cpp) lleva el estado; aqui se preparan y dibujan.
// Coste estimado en PS5 a 4K: la capa es UN cuadro sobre el tapete (lo que se ve de el: ~40 % de la pantalla, ~6
// lecturas de textura + 4 de sombra por pixel, ruido barato) + 2 dibujos de 60-160 particulas: < 1 ms de GPU.
// Sogen (pastoSogen en campos.frag): briznas por paralaje, ~8 pasos medios x 2 consultas de celda (azar sin seno,
// sin texturas) hasta dar con una brizna (medido con una simulacion de la misma funcion: 60-86 % de los rayos dan
// con una, segun la altura de la camara) + 1 consulta de sombra: ~500 operaciones mas por pixel, sin lecturas de
// textura nuevas; estimado ~0.2-0.3 ms mas a 4K solo con Sogen activo. De lejos (brizna < 1 pixel) no recorre.
#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "stb_image.h"   // la implementacion ya esta en la base (VulkanglTFModel.cpp)

namespace campos {

// superficies (los mismos numeros en campos.frag)
enum Superficie { S_NADA = 0, S_TEXTURA, S_AGUA, S_OSCURO, S_NECRO, S_MARMOL, S_LAVA, S_PLASMA, S_ORICALCO, S_METAL, S_NUBES };
// capas del arreglo (color; la normal es la siguiente): <assets>/campos/<nombre>_color.png / _normal.png
enum Capa { CESPED = 0, ROCA, GRIETA, PIZARRA, ARENA, LOSA, MARMOL, QUEMADO, METAL, PIEDRAMAR, SELVA, ACANTILADO, NUM_MATERIALES };
constexpr const char* NOMBRE_CAPA[NUM_MATERIALES] = { "cesped", "roca", "grieta", "pizarra", "arena", "losa", "marmol",
	                                                  "quemado", "metal", "piedramar", "selva", "acantilado" };
constexpr uint32_t NUM_CAPAS = NUM_MATERIALES * 2;
constexpr uint32_t TAM_TEXTURA = 512;
// particulas (los mismos numeros en campos_particula.vert)
enum Particula { P_NADA = -1, P_MOTAS = 0, P_BRASAS, P_HOJAS, P_GOTAS, P_POLVO, P_NIEBLA, P_PLUMAS, P_CHISPAS, P_BURBUJAS };

struct Parts { int tipo{ P_NADA }; uint32_t n{ 0 }; glm::vec4 color{ 1.0f }; };   // color.a: opacidad
struct Campo {
	const char* carta;
	int superficie, variante, capaA, capaB;
	float escalaA, escalaB;     // metros por repeticion
	glm::vec3 tinteA, tinteB;
	float mezclaB;              // 0..1: cuanto de la capa B (por manchas de ruido)
	glm::vec3 color;            // color del campo: marcos, onda, emision (lineal)
	Parts p0, p1;
};

using V3 = glm::vec3;
using V4 = glm::vec4;
inline Parts P(int t, uint32_t n, V4 c) { return { t, n, c }; }
static const Parts SIN{};

inline const std::vector<Campo>& tabla()
{
	static const std::vector<Campo> T = {
		// el Bosque tiene su propio mundo (bosque.h): aqui solo su color (la onda y la carta)
		{ "forest", S_NADA, 0, CESPED, CESPED, 2.5f, 2.5f, V3(1), V3(1), 0, V3(0.35f, 1.25f, 0.30f), SIN, SIN },
		{ "sogen", S_TEXTURA, 3, CESPED, SELVA, 2.6f, 3.0f, V3(0.78f, 1.08f, 0.5f), V3(0.9f), 0.15f, V3(0.75f, 1.2f, 0.35f),
		  P(P_HOJAS, 70, V4(0.55f, 0.75f, 0.22f, 0.9f)), P(P_MOTAS, 30, V4(1.0f, 0.95f, 0.6f, 0.5f)) },
		{ "mountain", S_TEXTURA, 4, ROCA, ACANTILADO, 3.2f, 4.0f, V3(0.95f), V3(0.9f), 0.35f, V3(0.75f, 0.9f, 1.2f),
		  P(P_POLVO, 70, V4(0.92f, 0.95f, 1.0f, 0.35f)), P(P_MOTAS, 20, V4(0.9f, 0.95f, 1.0f, 0.4f)) },
		{ "umi", S_AGUA, 0, PIEDRAMAR, PIEDRAMAR, 3.0f, 3.0f, V3(0.005f, 0.028f, 0.06f), V3(1), 0, V3(0.3f, 0.7f, 1.3f),
		  P(P_GOTAS, 110, V4(0.75f, 0.9f, 1.0f, 0.9f)), P(P_BURBUJAS, 40, V4(0.7f, 0.9f, 1.0f, 0.5f)) },
		{ "wasteland", S_TEXTURA, 5, GRIETA, ROCA, 3.0f, 3.5f, V3(1.05f, 0.92f, 0.78f), V3(0.95f, 0.82f, 0.7f), 0.2f, V3(1.2f, 0.8f, 0.4f),
		  P(P_POLVO, 110, V4(0.85f, 0.7f, 0.5f, 0.45f)), SIN },
		{ "yami", S_OSCURO, 0, PIZARRA, PIZARRA, 2.8f, 2.8f, V3(0.55f, 0.5f, 0.65f), V3(0.5f), 0, V3(0.75f, 0.25f, 1.25f),
		  P(P_NIEBLA, 26, V4(0.35f, 0.12f, 0.5f, 0.35f)), P(P_MOTAS, 40, V4(0.75f, 0.3f, 1.0f, 0.7f)) },
		{ "a_legendary_ocean", S_AGUA, 1, PIEDRAMAR, MARMOL, 2.6f, 3.0f, V3(0.0f, 0.07f, 0.09f), V3(1), 0, V3(0.3f, 1.2f, 1.1f),
		  P(P_GOTAS, 90, V4(0.8f, 1.0f, 0.95f, 0.9f)), P(P_BURBUJAS, 60, V4(0.7f, 1.0f, 0.95f, 0.55f)) },
		{ "umiiruka", S_AGUA, 2, PIEDRAMAR, PIEDRAMAR, 3.0f, 3.0f, V3(0.01f, 0.06f, 0.075f), V3(1), 0, V3(0.4f, 1.0f, 1.3f),
		  P(P_GOTAS, 150, V4(0.85f, 0.95f, 1.0f, 1.0f)), P(P_BURBUJAS, 40, V4(0.8f, 1.0f, 1.0f, 0.5f)) },
		{ "rising_air_current", S_NUBES, 0, MARMOL, MARMOL, 4.0f, 4.0f, V3(1), V3(1), 0, V3(0.7f, 0.95f, 1.3f),
		  P(P_POLVO, 80, V4(1.0f, 1.0f, 1.0f, 0.3f)), P(P_PLUMAS, 16, V4(1.0f, 1.0f, 1.0f, 0.8f)) },
		{ "gaia_power", S_TEXTURA, 1, CESPED, ROCA, 2.4f, 3.4f, V3(0.85f, 1.05f, 0.75f), V3(0.9f, 0.95f, 0.85f), 0.4f, V3(0.5f, 1.3f, 0.35f),
		  P(P_MOTAS, 60, V4(0.6f, 1.0f, 0.4f, 0.7f)), P(P_HOJAS, 40, V4(0.4f, 0.65f, 0.18f, 0.9f)) },
		{ "luminous_spark", S_MARMOL, 2, MARMOL, MARMOL, 3.0f, 3.0f, V3(1.05f, 1.03f, 1.0f), V3(1), 0, V3(1.3f, 1.2f, 0.9f),
		  P(P_MOTAS, 120, V4(1.0f, 0.95f, 0.75f, 0.9f)), P(P_CHISPAS, 40, V4(1.0f, 1.0f, 0.9f, 0.8f)) },
		{ "molten_destruction", S_LAVA, 0, QUEMADO, PIZARRA, 2.6f, 3.0f, V3(0.55f, 0.5f, 0.48f), V3(0.45f), 0.4f, V3(1.6f, 0.55f, 0.12f),
		  P(P_BRASAS, 160, V4(1.0f, 0.5f, 0.12f, 1.0f)), P(P_NIEBLA, 14, V4(0.15f, 0.08f, 0.05f, 0.35f)) },
		{ "mystic_plasma_zone", S_PLASMA, 0, PIZARRA, METAL, 2.8f, 3.0f, V3(0.4f, 0.36f, 0.5f), V3(0.4f, 0.35f, 0.5f), 0.35f, V3(1.0f, 0.3f, 1.4f),
		  P(P_CHISPAS, 90, V4(0.9f, 0.35f, 1.0f, 1.0f)), P(P_MOTAS, 40, V4(0.8f, 0.4f, 1.0f, 0.7f)) },
		{ "chorus_of_sanctuary", S_MARMOL, 1, MARMOL, MARMOL, 3.0f, 3.0f, V3(1.0f, 0.98f, 0.94f), V3(1), 0, V3(1.3f, 1.05f, 0.55f),
		  P(P_MOTAS, 80, V4(1.0f, 0.9f, 0.6f, 0.8f)), P(P_PLUMAS, 14, V4(1.0f, 1.0f, 1.0f, 0.85f)) },
		{ "the_sanctuary_in_the_sky", S_MARMOL, 0, MARMOL, MARMOL, 3.0f, 3.0f, V3(1.25f, 1.22f, 1.18f), V3(1), 0, V3(1.3f, 1.1f, 0.65f),
		  P(P_MOTAS, 70, V4(1.0f, 0.92f, 0.7f, 0.8f)), P(P_NIEBLA, 18, V4(1.0f, 1.0f, 1.0f, 0.22f)) },
		{ "dark_sanctuary", S_OSCURO, 1, PIZARRA, LOSA, 2.8f, 3.0f, V3(0.5f, 0.48f, 0.56f), V3(0.3f, 0.28f, 0.32f), 0.35f, V3(0.65f, 0.3f, 1.2f),
		  P(P_NIEBLA, 22, V4(0.25f, 0.2f, 0.35f, 0.35f)), P(P_MOTAS, 50, V4(0.7f, 0.5f, 1.0f, 0.7f)) },
		{ "necrovalley", S_NECRO, 0, LOSA, ARENA, 3.2f, 4.0f, V3(1.05f, 0.95f, 0.8f), V3(1.05f, 0.95f, 0.8f), 0.45f, V3(1.3f, 0.95f, 0.45f),
		  P(P_POLVO, 120, V4(0.9f, 0.75f, 0.5f, 0.45f)), SIN },
		{ "pandemonium", S_OSCURO, 2, PIZARRA, QUEMADO, 2.8f, 3.0f, V3(0.45f, 0.4f, 0.42f), V3(0.4f, 0.32f, 0.32f), 0.4f, V3(1.5f, 0.2f, 0.12f),
		  P(P_BRASAS, 80, V4(1.0f, 0.3f, 0.1f, 1.0f)), P(P_NIEBLA, 18, V4(0.2f, 0.04f, 0.04f, 0.35f)) },
		{ "jurassic_world", S_TEXTURA, 6, SELVA, CESPED, 2.6f, 2.6f, V3(0.9f, 1.0f, 0.85f), V3(0.8f, 0.95f, 0.7f), 0.35f, V3(0.7f, 1.1f, 0.35f),
		  P(P_HOJAS, 70, V4(0.35f, 0.55f, 0.15f, 0.9f)), P(P_NIEBLA, 14, V4(0.75f, 0.8f, 0.7f, 0.2f)) },
		{ "harpies_hunting_ground", S_TEXTURA, 2, ACANTILADO, ROCA, 3.4f, 3.0f, V3(0.95f, 0.92f, 0.9f), V3(0.9f), 0.35f, V3(1.0f, 0.45f, 0.8f),
		  P(P_PLUMAS, 40, V4(0.95f, 0.85f, 0.9f, 0.9f)), P(P_POLVO, 50, V4(0.9f, 0.9f, 0.95f, 0.3f)) },
		{ "golden_castle_of_stromberg", S_METAL, 2, MARMOL, METAL, 3.0f, 2.4f, V3(1.0f, 0.78f, 0.38f), V3(1.0f, 0.75f, 0.32f), 0.5f, V3(1.4f, 1.05f, 0.35f),
		  P(P_MOTAS, 90, V4(1.0f, 0.85f, 0.4f, 0.9f)), P(P_CHISPAS, 30, V4(1.0f, 0.9f, 0.5f, 0.8f)) },
		{ "the_seal_of_orichalcos", S_ORICALCO, 0, PIZARRA, LOSA, 2.8f, 3.2f, V3(0.45f, 0.5f, 0.48f), V3(0.35f, 0.4f, 0.38f), 0.3f, V3(0.3f, 1.5f, 0.45f),
		  P(P_CHISPAS, 120, V4(0.35f, 1.0f, 0.45f, 1.0f)), P(P_MOTAS, 40, V4(0.4f, 1.0f, 0.5f, 0.7f)) },
		{ "fusion_gate", S_METAL, 0, METAL, PIZARRA, 2.6f, 3.0f, V3(0.5f, 0.5f, 0.6f), V3(0.4f), 0.3f, V3(0.6f, 0.45f, 1.5f),
		  P(P_CHISPAS, 80, V4(0.6f, 0.5f, 1.0f, 1.0f)), P(P_MOTAS, 40, V4(1.0f, 0.8f, 0.4f, 0.7f)) },
		{ "centrifugal_field", S_METAL, 1, METAL, METAL, 2.6f, 2.6f, V3(0.6f, 0.62f, 0.66f), V3(0.6f), 0, V3(0.3f, 1.1f, 1.4f),
		  P(P_CHISPAS, 60, V4(0.4f, 0.95f, 1.0f, 1.0f)), P(P_POLVO, 40, V4(0.8f, 0.9f, 1.0f, 0.25f)) },
		{ "array_of_revealing_light", S_MARMOL, 3, MARMOL, LOSA, 3.0f, 3.2f, V3(0.9f, 0.88f, 0.82f), V3(0.8f, 0.75f, 0.65f), 0.3f, V3(1.4f, 1.25f, 0.8f),
		  P(P_MOTAS, 100, V4(1.0f, 0.95f, 0.8f, 0.9f)), SIN },
	};
	return T;
}

inline const Campo* buscar(const std::string& carta)
{
	for (const Campo& c : tabla()) {
		if (carta == c.carta) return &c;
	}
	return nullptr;
}
inline bool esCampo(const std::string& carta) { return buscar(carta) != nullptr; }

class Campos {
public:
	bool listo{ false };
	float msTexturas{ 0.0f };
	uint32_t dibujosCuadro{ 0 };

	void preparar(vks::VulkanDevice* d, VkQueue cola, VkFormat formatoColor, VkFormat formatoProf, VkPipelineCache cache, VkDescriptorSetLayout set0,
	              const std::string& shaders, const std::string& assets, float hx_, float hz_)
	{
		disp = d;
		dev = d->logicalDevice;
		hx = hx_;
		hz = hz_;
		cargarTexturas(cola, assets + "campos/");
		crearPipelines(formatoColor, formatoProf, cache, set0, shaders);
		listo = true;
	}

	~Campos()
	{
		if (!dev) return;
		for (VkPipeline p : { pSuperficie, pParticulas }) vkDestroyPipeline(dev, p, nullptr);
		vkDestroyPipelineLayout(dev, layout, nullptr);
		vkDestroyDescriptorPool(dev, pool, nullptr);
		vkDestroyDescriptorSetLayout(dev, layoutTexturas, nullptr);
		vkDestroySampler(dev, muestreador, nullptr);
		vkDestroyImageView(dev, vista, nullptr);
		vkDestroyImage(dev, imagen, nullptr);
		vkFreeMemory(dev, memoria, nullptr);
	}

	// la capa del tablero (despues del tapete). amb: 0..1 (la onda la lleva ubo.bosque); origen: la zona de la carta
	// (fija: rama classic-extras, el tapete elegido: cubre toda la mesa sin la onda de la carta)
	void superficie(VkCommandBuffer cmd, VkDescriptorSet set0, const Campo& c, float amb, bool fija = false)
	{
		dibujosCuadro = 0;
		if (!listo || c.superficie == S_NADA || amb <= 0.0f) return;
		enlazar(cmd, set0);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pSuperficie);
		struct { V4 a, b, tA, tB, color; } e{
			V4((float)c.superficie, (float)c.variante, (float)(c.capaA * 2), (float)(c.capaB * 2)),
			V4(c.escalaA, c.escalaB, amb, hx), V4(c.tinteA, hz), V4(c.tinteB, c.mezclaB), V4(c.color, fija ? 2.0f : 1.0f)
		};
		vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(e), &e);
		vkCmdDraw(cmd, 6, 1, 0, 0);
		dibujosCuadro++;
	}

	void particulas(VkCommandBuffer cmd, VkDescriptorSet set0, const Campo& c, float amb)
	{
		if (!listo || amb <= 0.0f) return;
		enlazar(cmd, set0);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pParticulas);
		for (const Parts* p : { &c.p0, &c.p1 }) {
			if (p->tipo < 0 || p->n == 0) continue;
			struct { V4 modo, color, mesa; } e{ V4((float)p->tipo, (float)p->n, amb, 0.0f), p->color, V4(hx, hz, 0.0f, 0.0f) };
			vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(e), &e);
			vkCmdDraw(cmd, 6, p->n, 0, 0);
			dibujosCuadro++;
		}
	}

	void informe() const
	{
		fprintf(stderr, "[campos] %zu cartas de campo, %u capas de %u px (%.1f MB con mipmaps) en %.0f ms\n", tabla().size(), NUM_CAPAS, TAM_TEXTURA,
		        (float)NUM_CAPAS * TAM_TEXTURA * TAM_TEXTURA * 4.0f * 1.333f / (1024.0f * 1024.0f), msTexturas);
	}

private:
	vks::VulkanDevice* disp{ nullptr };
	VkDevice dev{ VK_NULL_HANDLE };
	float hx{ 13.44f }, hz{ 10.08f };
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	VkPipeline pSuperficie{ VK_NULL_HANDLE }, pParticulas{ VK_NULL_HANDLE };
	VkImage imagen{ VK_NULL_HANDLE };
	VkDeviceMemory memoria{ VK_NULL_HANDLE };
	VkImageView vista{ VK_NULL_HANDLE };
	VkSampler muestreador{ VK_NULL_HANDLE };
	VkDescriptorPool pool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout layoutTexturas{ VK_NULL_HANDLE };
	VkDescriptorSet setTexturas{ VK_NULL_HANDLE };

	void enlazar(VkCommandBuffer cmd, VkDescriptorSet set0)
	{
		const VkDescriptorSet sets[2] = { set0, setTexturas };
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets, 0, nullptr);
	}

	// las capas (png RGBA de campos_insumos.py) en un arreglo 2D con mipmaps (promedio 2x2; normales renormalizadas).
	// Si falta una capa queda neutra
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
		if (faltan > 0) fprintf(stderr, "[campos] faltan %d capas de textura en %s (correr campos_insumos.py): quedan neutras\n", faltan, dir.c_str());
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
		VK_CHECK_RESULT(vkCreateImage(dev, &ii, nullptr, &imagen));
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, imagen, &req);
		VkMemoryAllocateInfo ma = vks::initializers::memoryAllocateInfo();
		ma.allocationSize = req.size;
		ma.memoryTypeIndex = disp->getMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(dev, &ma, nullptr, &memoria));
		VK_CHECK_RESULT(vkBindImageMemory(dev, imagen, memoria, 0));
		VkCommandBuffer cmd = disp->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		const VkImageSubresourceRange todo{ VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, 0, NUM_CAPAS };
		VkImageMemoryBarrier b = vks::initializers::imageMemoryBarrier();
		b.image = imagen;
		b.subresourceRange = todo;
		b.srcAccessMask = 0;
		b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
		vkCmdCopyBufferToImage(cmd, subida.buffer, imagen, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)copias.size(), copias.data());
		b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
		disp->flushCommandBuffer(cmd, cola, true);
		subida.destroy();

		VkImageViewCreateInfo vi = vks::initializers::imageViewCreateInfo();
		vi.image = imagen;
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		vi.format = VK_FORMAT_R8G8B8A8_UNORM;
		vi.subresourceRange = todo;
		VK_CHECK_RESULT(vkCreateImageView(dev, &vi, nullptr, &vista));
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
		VkDescriptorImageInfo img{ muestreador, vista, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
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

	void crearPipelines(VkFormat formatoColor, VkFormat formatoProf, VkPipelineCache cache, VkDescriptorSetLayout set0, const std::string& shaders)
	{
		VkPushConstantRange empuje = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 5 * sizeof(glm::vec4), 0);
		const VkDescriptorSetLayout sets[2] = { set0, layoutTexturas };
		VkPipelineLayoutCreateInfo pl = vks::initializers::pipelineLayoutCreateInfo(sets, 2);
		pl.pushConstantRangeCount = 1;
		pl.pPushConstantRanges = &empuje;
		VK_CHECK_RESULT(vkCreatePipelineLayout(dev, &pl, nullptr, &layout));

		VkPipelineVertexInputStateCreateInfo sinVertices = vks::initializers::pipelineVertexInputStateCreateInfo();
		VkPipelineInputAssemblyStateCreateInfo ensamble = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo raster = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		// la capa: mezcla con alfa (la cobertura de la onda), sin escribir profundidad
		VkPipelineColorBlendAttachmentState alfa = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_TRUE);
		alfa.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		alfa.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		alfa.colorBlendOp = VK_BLEND_OP_ADD;
		alfa.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		alfa.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		alfa.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo mezclaAlfa = vks::initializers::pipelineColorBlendStateCreateInfo(1, &alfa);
		// particulas: alfa premultiplicado (alfa 0 = suma luz)
		VkPipelineColorBlendAttachmentState premult = alfa;
		premult.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		VkPipelineColorBlendStateCreateInfo mezclaPremult = vks::initializers::pipelineColorBlendStateCreateInfo(1, &premult);
		VkPipelineDepthStencilStateCreateInfo profSinEscribir = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo vistaVp = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo muestras = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dinamicos = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dinamico = vks::initializers::pipelineDynamicStateCreateInfo(dinamicos);
		std::vector<VkShaderModule> modulos;
		const std::string dir = shaders + "yugireload/";
		std::array<VkPipelineShaderStageCreateInfo, 2> etapas = { etapa(dir + "campos.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos),
			                                                      etapa(dir + "campos.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos) };
		VkPipelineRenderingCreateInfo render{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &formatoColor,
			.depthAttachmentFormat = formatoProf,
			.stencilAttachmentFormat = formatoProf,
		};
		VkGraphicsPipelineCreateInfo ci{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &render,
			.stageCount = 2,
			.pStages = etapas.data(),
			.pVertexInputState = &sinVertices,
			.pInputAssemblyState = &ensamble,
			.pViewportState = &vistaVp,
			.pRasterizationState = &raster,
			.pMultisampleState = &muestras,
			.pDepthStencilState = &profSinEscribir,
			.pColorBlendState = &mezclaAlfa,
			.pDynamicState = &dinamico,
			.layout = layout,
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pSuperficie));
		etapas = { etapa(dir + "campos_particula.vert.spv", VK_SHADER_STAGE_VERTEX_BIT, modulos),
			       etapa(dir + "campos_particula.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT, modulos) };
		ci.pColorBlendState = &mezclaPremult;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pParticulas));
		for (VkShaderModule m : modulos) vkDestroyShaderModule(dev, m, nullptr);
	}
};

}   // namespace campos
