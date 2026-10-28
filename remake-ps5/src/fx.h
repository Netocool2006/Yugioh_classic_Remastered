// Yugi Reload - efectos del Niagara Examples Pack (Epic Games, Fab) usados TAL CUAL: cada sistema se horneo en
// Unreal a un flipbook (herramientas/unreal/hornear_niagara.py: atlas de cuadros RGBA con alfa premultiplicado +
// json con cuadros, fps, loop, origen y tamano). Los atlas NO van al repo (son del pack): assets/fx/.
//  - Todos los atlas en UN arreglo 2D (capas de 2048 con mipmaps, set 1): un solo dibujo instanciado por cuadro
//    con todas las instancias ordenadas de lejos a cerca (mezcla ONE, ONE_MINUS_SRC_ALPHA: lo aditivo del pack
//    sale con alfa 0 y el humo tapa lo de atras, en la misma pasada).
//  - Cada instancia: punto de origen del sistema (la base del fuego, el centro de la explosion), tamano en m, tinte
//    y opacidad, cuadro (con mezcla entre dos cuadros seguidos), y como se orienta: de cara a la camara con el
//    "arriba" del mundo (lo que sube en Unreal sube aqui), a lo largo de un eje girando sobre el para mirar a la
//    camara (fuego de un aliento) o plano sobre la mesa (marca del objetivo, horneada desde arriba).
//  - Los efectos se piden sin estado (pedir() con la edad del efecto): yugireload.cpp los pide en cada cuadro.
// Coste: YUGI_FX_MEDIR=1 imprime capas, memoria, tiempo de carga e instancias por cuadro.
#pragma once

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "stb_image.h"   // la implementacion ya esta en la base (VulkanglTFModel.cpp)

namespace fx {

// Los efectos horneados, en el orden de archivo(); el comentario dice para que servian en Yugi Reload
enum class Fx : int {
	Fuego,             // NS_Fire (loop): llamas; el motor lo usa para toda llama() (aliento de los dragones, fogonazos)
	ExplosionMediana,  // NS_Explosion_Medium: impacto del aliento de fuego
	ExplosionChica,    // NS_Explosion_Small: impacto de un disparo / proyectil
	Explosion,         // NS_Explosion: explosion grande (maquinas al morir, tanque Oni)
	BolaFuego,         // NS_ExplosionRoil: bola de fuego que rueda (estallido de fuego al morir)
	ColumnaHumo,       // NS_Smoke_Plume (loop): humo que queda (maquina rota, fuego que se apaga)
	Chispas,           // NS_Spark_Burst: chispas y metralla
	ImpactoMetal,      // NS_Impact_Metal: golpe de arma contra el rival
	Fogonazo,          // NS_MuzzleFlash: fogonazo en la boquilla al disparar
	Tesla,             // NS_TeslaCoil (loop): arcos electricos (rayo del Mago Oscuro en el rival)
	Electricidad,      // NS_Player_Electricity_Looping (loop): carga electrica del Mago en la punta del baculo
	TeleportIn,        // NS_Player_Teleport_In: invocacion (el monstruo se materializa)
	TeleportOut,       // NS_Player_Teleport_Out: desaparicion del destruido
	Aparicion,         // NS_Pickup_Spawn: la carta aparece / se voltea
	Disolver,          // NS_HitDissolve: golpe que disuelve (al empezar a desintegrarse)
	Triangulos,        // NS_SkeletalMeshTris_Burst: estallido en triangulos (desintegrarse)
	Polvo,             // NS_Dirt_Explosion: polvo y tierra al caer / azotar la mesa
	Marca,             // NS_Marker_Target (loop, plano en la mesa): marca del monstruo objetivo
	Estela,            // NS_RocketTrail (loop): estela del orbe de magia
	Aura,              // NS_Player_Buff_Looping (loop): aura del atacante
	AuraOscura,        // NS_Player_DeBuff_Looping (loop): aura de magia oscura
	Bocanada,          // NS_SmokePuffLight_BakedAtlas (ya horneado en el pack): bocanada de humo; el motor la usa para humo()
	// Yu-Gi-Oh! Master Duel (copia del usuario, uso personal): sus ParticleSystem horneados con herramientas/fx_md/sim_fx.py
	// (assets/fx/md, no van al repo); los usa efectos_cartas.h
	MdActivar,         // fxp_bff_apply_001 en gris: activacion/aplicacion de efecto (se tine verde magia / rosa trampa)
	MdDestruir,        // fxp_cardbrk_bff_001: carta destruida por un efecto
	MdColocar,         // fxp_som_mgctrpfld_001: humo al colocar una magia/trampa
	MdAgujeroNegro,    // ef04342 (Dark Hole): remolino galactico (vista de frente)
	MdCuchillos,       // ef05166 (Thousand Knives)
	MdPlumero,         // ef04678 (Harpie's Feather Duster): destellos dorados
	MdTornado,         // ef04909: tornado azul (Mystical Space Typhoon / Heavy Storm)
	MdFusion,          // fxp_somldg_fusion_s1_001: fusion
	MdFusionMarco,     // fxp_somldg_fusion_s1imp_001: marco violeta de la fusion
	MdRitual,          // fxp_somldg_ritual_s1_001: circulo azul del ritual
	MdDestruirBatalla, // fxp_cardbrk_btl_001: carta destruida en combate (rama classic-md-combate; CardRoot.ReqBrokenEffect)
	NUM
};
constexpr int NUM = (int)Fx::NUM;
constexpr uint32_t TAM = 2048;          // lado de cada capa (un atlas de 8x8 cuadros de 256)
constexpr uint32_t MIPS = 4;            // 2048 .. 256 (cuadros de 256 .. 32: mas abajo se mezclan los cuadros vecinos
                                        // y aparecen cuadrados alrededor de cada particula)
constexpr uint32_t MAX_INSTANCIAS = 4096;
constexpr int CUADROS_EN_VUELO = 2;     // maxConcurrentFrames del motor

// el archivo de cada efecto en assets/fx (sin extension); el mismo orden que Fx
inline const char* archivo(Fx f)
{
	static const char* const A[NUM] = {
		"NS_Fire", "NS_Explosion_Medium", "NS_Explosion_Small", "NS_Explosion", "pack/NS_ExplosionRoil", "NS_Smoke_Plume",
		"NS_Spark_Burst", "NS_Impact_Metal", "NS_MuzzleFlash", "NS_TeslaCoil", "NS_Player_Electricity_Looping",
		"NS_Player_Teleport_In", "NS_Player_Teleport_Out", "NS_Pickup_Spawn", "NS_HitDissolve", "NS_SkeletalMeshTris_Burst",
		"NS_Dirt_Explosion", "NS_Marker_Target", "NS_RocketTrail", "NS_Player_Buff_Looping", "NS_Player_DeBuff_Looping",
		"pack/NS_SmokePuffLight",
		"md/md_activar", "md/md_destruir", "md/md_colocar", "md/md_agujero_negro", "md/md_cuchillos", "md/md_plumero", "md/md_tornado",
		"md/md_fusion", "md/md_fusion_marco", "md/md_ritual", "md/md_destruir_batalla",
	};
	return A[(int)f];
}

struct Info {
	bool cargado{ false };
	int capa{ 0 };
	int columnas{ 8 }, cuadros{ 64 };
	float fps{ 30.0f }, duracion{ 2.0f };
	bool loop{ false };
	bool suelo{ false };              // horneado desde arriba: va plano sobre la mesa
	float llenado{ 1.0f };            // fraccion de la capa que ocupa la grilla (un atlas de 6x6 de 256 en una capa de 2048)
	float origen[2]{ 0.5f, 0.5f };    // punto de origen del sistema dentro del cuadro (0..1, y hacia abajo)
	float caja[4]{ 0.0f, 0.0f, 1.0f, 1.0f };   // lo que ocupa el efecto en el cuadro (todo el flipbook)
	float tamanoM{ 1.0f };            // lado del cuadro en Unreal (m)
};

// lo que lleva cada instancia al vertice (5 vec4)
struct Instancia {
	float pos[4];      // origen del sistema (xyz) y lado del cuadro en m
	float color[4];    // tinte (rgb, puede pasar de 1) y opacidad
	float eje[4];      // xyz: "arriba" del efecto (modo 1); w: modo (0 camara, 1 eje, 2 suelo)
	float cuadro[4];   // capa, cuadro (con fraccion), cuadros, columnas
	float extra[4];    // origen x, y, llenado, loop (1) / no (0)
	float recorte[4];  // parte del cuadro que se dibuja: v desde, v hasta (0..1, y hacia abajo); z: borde que se
	                   // funde (fraccion del cuadro): el pack deja humo y chispas tocando el borde del cuadro y sin
	                   // esto se veia el cuadrado de cada particula
};

// lee un numero de "clave": ... de un json plano (el que escribe componer_flipbook.py)
inline bool numero(const std::string& j, const char* clave, float& v, int indice = -1)
{
	const std::string k = std::string("\"") + clave + "\":";   // con los dos puntos: "tipo": "loop" no es la clave loop
	size_t p = j.find(k);
	if (p == std::string::npos) {
		return false;
	}
	p += k.size();
	if (indice >= 0) {
		p = j.find('[', p);
		if (p == std::string::npos) {
			return false;
		}
		p++;
		for (int i = 0; i < indice; i++) {
			p = j.find(',', p);
			if (p == std::string::npos) {
				return false;
			}
			p++;
		}
	}
	while (p < j.size() && (j[p] == ' ' || j[p] == '\n' || j[p] == '\r' || j[p] == '\t')) {
		p++;
	}
	if (j.compare(p, 4, "true") == 0) {
		v = 1.0f;
		return true;
	}
	if (j.compare(p, 5, "false") == 0) {
		v = 0.0f;
		return true;
	}
	char* fin = nullptr;
	v = strtof(j.c_str() + p, &fin);
	return fin != j.c_str() + p;
}

class Flipbooks {
public:
	bool listo{ false };
	int capas{ 0 };
	float msCarga{ 0.0f };
	uint32_t ultimasInstancias{ 0 }, maxInstancias{ 0 };
	std::array<Info, NUM> info{};

	void preparar(vks::VulkanDevice* d, VkQueue cola, VkFormat formatoColor, VkFormat formatoProf, VkPipelineCache cache, VkDescriptorSetLayout set0,
	              const std::string& shaders, const std::string& assets)
	{
		disp = d;
		dev = d->logicalDevice;
		const auto t0 = std::chrono::steady_clock::now();
		cargar(cola, assets + "fx/");
		if (capas == 0) {
			fprintf(stderr, "[fx] sin atlas en %sfx (correr herramientas/unreal/hornear_niagara.bat): quedan los efectos por codigo\n", assets.c_str());
			return;
		}
		for (int k = 0; k < CUADROS_EN_VUELO; k++) {
			VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			                                   &instancias[k], MAX_INSTANCIAS * sizeof(Instancia)));
			VK_CHECK_RESULT(instancias[k].map());
		}
		crearPipeline(formatoColor, formatoProf, cache, set0, shaders);
		msCarga = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count();
		listo = true;
		if (getenv("YUGI_FX_MEDIR")) {
			informe();
		}
	}

	~Flipbooks()
	{
		if (!dev) {
			return;
		}
		for (vks::Buffer& b : instancias) {
			b.destroy();
		}
		vkDestroyPipeline(dev, pipeline, nullptr);
		vkDestroyPipelineLayout(dev, layout, nullptr);
		vkDestroyDescriptorPool(dev, pool, nullptr);
		vkDestroyDescriptorSetLayout(dev, layoutAtlas, nullptr);
		vkDestroySampler(dev, muestreador, nullptr);
		vkDestroyImageView(dev, vista, nullptr);
		vkDestroyImage(dev, imagen, nullptr);
		vkFreeMemory(dev, memoria, nullptr);
	}

	bool hay(Fx f) const { return listo && info[(int)f].cargado; }
	const Info& de(Fx f) const { return info[(int)f]; }

	// Pide un efecto este cuadro. donde: origen del sistema; tamano: m que mide lo que se ve del efecto (su caja);
	// edad: s desde que empezo (loop: se repite; si no, despues de terminar no se dibuja); color: tinte y opacidad;
	// eje: si no es cero, el "arriba" del efecto. Devuelve false si no hay atlas (el que llama dibuja su respaldo)
	bool pedir(Fx f, glm::vec3 donde, float tamano, float edad, glm::vec4 color, glm::vec3 eje = glm::vec3(0.0f), float desdeV = 0.0f)
	{
		if (!hay(f)) {
			return false;
		}
		const Info& in = info[(int)f];
		if (tamano <= 0.0f || color.a <= 0.0f || edad < 0.0f || cola.size() >= MAX_INSTANCIAS) {
			return true;
		}
		float c = edad * in.fps;
		if (in.loop) {
			c = fmodf(c, (float)in.cuadros);
		} else if (c > (float)in.cuadros - 1.0f) {
			return true;   // ya termino
		}
		const float visible = glm::max(in.caja[2] - in.caja[0], in.caja[3] - in.caja[1]);
		const float lado = tamano / glm::max(visible, 0.05f);
		const float largoEje = glm::length(eje);
		const float modo = in.suelo ? 2.0f : (largoEje > 1e-4f ? 1.0f : 0.0f);
		const glm::vec3 e = largoEje > 1e-4f ? eje / largoEje : glm::vec3(0.0f, 1.0f, 0.0f);
		Instancia i{
			{ donde.x, donde.y, donde.z, lado },
			{ color.r, color.g, color.b, glm::min(color.a, 1.0f) },
			{ e.x, e.y, e.z, modo },
			{ (float)in.capa, c, (float)in.cuadros, (float)in.columnas },
			{ in.origen[0], in.origen[1], in.llenado, in.loop ? 1.0f : 0.0f },
			{ glm::clamp(desdeV, 0.0f, 0.9f), 1.0f, 0.14f, 0.0f },
		};
		cola.push_back(i);
		return true;
	}

	// todo lo pedido en el cuadro, de lejos a cerca, en un dibujo
	void dibujar(VkCommandBuffer cmd, VkDescriptorSet set0, uint32_t cuadroEnVuelo, glm::vec3 ojo)
	{
		ultimasInstancias = (uint32_t)cola.size();
		maxInstancias = glm::max(maxInstancias, ultimasInstancias);
		if (!listo || cola.empty()) {
			cola.clear();
			return;
		}
		auto lejos = [&](const Instancia& i) {
			const glm::vec3 d = glm::vec3(i.pos[0], i.pos[1], i.pos[2]) - ojo;
			return glm::dot(d, d);
		};
		std::stable_sort(cola.begin(), cola.end(), [&](const Instancia& a, const Instancia& b) { return lejos(a) > lejos(b); });
		vks::Buffer& b = instancias[cuadroEnVuelo % CUADROS_EN_VUELO];
		memcpy(b.mapped, cola.data(), cola.size() * sizeof(Instancia));
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		const VkDescriptorSet sets[2] = { set0, setAtlas };
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 2, sets, 0, nullptr);
		const VkDeviceSize o = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &b.buffer, &o);
		vkCmdDraw(cmd, 6, (uint32_t)cola.size(), 0, 0);
		cola.clear();
	}

	void informe() const
	{
		const double mb = (double)capas * TAM * TAM * 4.0 * 4.0 / 3.0 / 1048576.0;
		fprintf(stderr, "[fx] %d atlas del Niagara Examples Pack en un arreglo de %u x %u (%.0f MB con mips), cargados en %.0f ms; "
		        "1 dibujo por cuadro, hasta %u instancias (%u KB por cuadro en vuelo)\n",
		        capas, TAM, TAM, mb, msCarga, MAX_INSTANCIAS, (unsigned)(MAX_INSTANCIAS * sizeof(Instancia) / 1024));
		for (int f = 0; f < NUM; f++) {
			const Info& i = info[f];
			fprintf(stderr, "[fx]   %-32s %s capa %2d  %2d cuadros %5.1f fps %s %.2f m\n", archivo((Fx)f), i.cargado ? "OK  " : "FALTA", i.capa, i.cuadros,
			        i.fps, i.loop ? "loop" : "uno ", i.tamanoM);
		}
	}

private:
	vks::VulkanDevice* disp{ nullptr };
	VkDevice dev{ VK_NULL_HANDLE };
	std::vector<Instancia> cola;
	std::array<vks::Buffer, CUADROS_EN_VUELO> instancias;
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkImage imagen{ VK_NULL_HANDLE };
	VkDeviceMemory memoria{ VK_NULL_HANDLE };
	VkImageView vista{ VK_NULL_HANDLE };
	VkSampler muestreador{ VK_NULL_HANDLE };
	VkDescriptorPool pool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout layoutAtlas{ VK_NULL_HANDLE };
	VkDescriptorSet setAtlas{ VK_NULL_HANDLE };

	static bool leerTexto(const std::string& ruta, std::string& s)
	{
		std::ifstream f(ruta, std::ios::binary);
		if (!f) {
			return false;
		}
		std::stringstream b;
		b << f.rdbuf();
		s = b.str();
		return true;
	}

	// los atlas que hay (json + png) a capas de TAM x TAM con mipmaps (promedio 2x2: el color ya viene
	// premultiplicado, asi el promedio no oscurece los bordes)
	void cargar(VkQueue colaVk, const std::string& dir)
	{
		std::vector<std::vector<uint8_t>> niveles0;   // la capa entera (TAM x TAM) de cada atlas cargado
		for (int f = 0; f < NUM; f++) {
			Info& in = info[f];
			std::string j;
			const std::string base = dir + archivo((Fx)f);
			if (!leerTexto(base + ".json", j)) {
				continue;
			}
			int w = 0, h = 0, n = 0;
			stbi_uc* px = stbi_load((base + ".png").c_str(), &w, &h, &n, 4);
			if (!px) {
				continue;
			}
			float v = 0.0f;
			float lado = 256.0f;
			if (numero(j, "columnas", v)) in.columnas = glm::max(1, (int)v);
			if (numero(j, "cuadros", v)) in.cuadros = glm::max(1, (int)v);
			if (numero(j, "fps", v)) in.fps = v;
			if (numero(j, "duracion", v)) in.duracion = v;
			if (numero(j, "loop", v)) in.loop = v > 0.5f;
			if (numero(j, "lado_cuadro", v)) lado = v;
			if (numero(j, "tamano_cm", v)) in.tamanoM = v * 0.01f;
			for (int k = 0; k < 2; k++) {
				if (numero(j, "origen", v, k)) in.origen[k] = v;
			}
			for (int k = 0; k < 4; k++) {
				if (numero(j, "caja", v, k)) in.caja[k] = v;
			}
			in.suelo = j.find("\"vista\": \"arriba\"") != std::string::npos;
			// el atlas en la capa: si es mas grande, se reduce por un factor entero; si es mas chico, va arriba a la izquierda
			uint32_t paso = 1;
			while ((uint32_t)w / paso > TAM || (uint32_t)h / paso > TAM) {
				paso *= 2;
			}
			const uint32_t aw = (uint32_t)w / paso, ah = (uint32_t)h / paso;
			std::vector<uint8_t> capa((size_t)TAM * TAM * 4, 0);
			for (uint32_t y = 0; y < ah; y++) {
				for (uint32_t x = 0; x < aw; x++) {
					uint32_t acc[4] = { 0, 0, 0, 0 };
					for (uint32_t dy = 0; dy < paso; dy++) {
						for (uint32_t dx = 0; dx < paso; dx++) {
							const stbi_uc* q = &px[(((size_t)(y * paso + dy)) * (uint32_t)w + x * paso + dx) * 4];
							for (int k = 0; k < 4; k++) {
								acc[k] += q[k];
							}
						}
					}
					for (int k = 0; k < 4; k++) {
						capa[((size_t)y * TAM + x) * 4 + k] = (uint8_t)(acc[k] / (paso * paso));
					}
				}
			}
			stbi_image_free(px);
			in.llenado = (float)in.columnas * lado / (float)(paso * TAM);
			in.capa = (int)niveles0.size();
			in.cargado = true;
			niveles0.push_back(std::move(capa));
		}
		capas = (int)niveles0.size();
		if (capas == 0) {
			return;
		}
		std::vector<VkDeviceSize> tamMip(MIPS);
		VkDeviceSize porCapa = 0;
		for (uint32_t m = 0; m < MIPS; m++) {
			const uint32_t s = TAM >> m;
			tamMip[m] = (VkDeviceSize)s * s * 4;
			porCapa += tamMip[m];
		}
		vks::Buffer subida;
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &subida,
		                                   porCapa * (VkDeviceSize)capas));
		VK_CHECK_RESULT(subida.map());
		uint8_t* datos = (uint8_t*)subida.mapped;
		std::vector<VkBufferImageCopy> copias;
		for (int c = 0; c < capas; c++) {
			VkDeviceSize ofs = porCapa * (VkDeviceSize)c;
			std::vector<uint8_t> act = std::move(niveles0[c]);
			for (uint32_t m = 0; m < MIPS; m++) {
				const uint32_t s = TAM >> m;
				if (m > 0) {
					const uint32_t sp = s * 2;
					std::vector<uint8_t> sig((size_t)s * s * 4);
					for (uint32_t y = 0; y < s; y++) {
						for (uint32_t x = 0; x < s; x++) {
							const uint8_t* a = &act[(((size_t)(2 * y)) * sp + 2 * x) * 4];
							const uint8_t* b = a + (size_t)sp * 4;
							uint8_t* o = &sig[((size_t)y * s + x) * 4];
							for (int k = 0; k < 4; k++) {
								o[k] = (uint8_t)((a[k] + a[k + 4] + b[k] + b[k + 4] + 2) / 4);
							}
						}
					}
					act.swap(sig);
				}
				memcpy(datos + ofs, act.data(), act.size());
				VkBufferImageCopy r{};
				r.bufferOffset = ofs;
				r.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, m, (uint32_t)c, 1 };
				r.imageExtent = { s, s, 1 };
				copias.push_back(r);
				ofs += tamMip[m];
			}
		}
		VkImageCreateInfo ii = vks::initializers::imageCreateInfo();
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = VK_FORMAT_R8G8B8A8_UNORM;   // el sRGB lo deshace el fragmento (como el bosque: UNORM en todos lados)
		ii.extent = { TAM, TAM, 1 };
		ii.mipLevels = MIPS;
		ii.arrayLayers = (uint32_t)capas;
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
		const VkImageSubresourceRange todo{ VK_IMAGE_ASPECT_COLOR_BIT, 0, MIPS, 0, (uint32_t)capas };
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
		disp->flushCommandBuffer(cmd, colaVk, true);
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
		si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		si.maxLod = (float)(MIPS - 1);
		VK_CHECK_RESULT(vkCreateSampler(dev, &si, nullptr, &muestreador));
		std::vector<VkDescriptorPoolSize> tam = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo pi = vks::initializers::descriptorPoolCreateInfo(tam, 1);
		VK_CHECK_RESULT(vkCreateDescriptorPool(dev, &pi, nullptr, &pool));
		const std::vector<VkDescriptorSetLayoutBinding> enlaces = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo li = vks::initializers::descriptorSetLayoutCreateInfo(enlaces);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev, &li, nullptr, &layoutAtlas));
		VkDescriptorSetAllocateInfo ai = vks::initializers::descriptorSetAllocateInfo(pool, &layoutAtlas, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(dev, &ai, &setAtlas));
		VkDescriptorImageInfo img{ muestreador, vista, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet w = vks::initializers::writeDescriptorSet(setAtlas, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &img);
		vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
	}

	void crearPipeline(VkFormat formatoColor, VkFormat formatoProf, VkPipelineCache cache, VkDescriptorSetLayout set0, const std::string& shaders)
	{
		const VkDescriptorSetLayout sets[2] = { set0, layoutAtlas };
		VkPipelineLayoutCreateInfo pl = vks::initializers::pipelineLayoutCreateInfo(sets, 2);
		VK_CHECK_RESULT(vkCreatePipelineLayout(dev, &pl, nullptr, &layout));
		const VkVertexInputBindingDescription enlace{ 0, sizeof(Instancia), VK_VERTEX_INPUT_RATE_INSTANCE };
		const std::array<VkVertexInputAttributeDescription, 6> atributos = { {
			{ 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instancia, pos) },
			{ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instancia, color) },
			{ 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instancia, eje) },
			{ 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instancia, cuadro) },
			{ 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instancia, extra) },
			{ 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Instancia, recorte) },
		} };
		VkPipelineVertexInputStateCreateInfo entrada = vks::initializers::pipelineVertexInputStateCreateInfo();
		entrada.vertexBindingDescriptionCount = 1;
		entrada.pVertexBindingDescriptions = &enlace;
		entrada.vertexAttributeDescriptionCount = (uint32_t)atributos.size();
		entrada.pVertexAttributeDescriptions = atributos.data();
		VkPipelineInputAssemblyStateCreateInfo ensamble = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo raster = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState premult = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_TRUE);
		premult.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		premult.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		premult.colorBlendOp = VK_BLEND_OP_ADD;
		premult.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		premult.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		premult.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo mezcla = vks::initializers::pipelineColorBlendStateCreateInfo(1, &premult);
		VkPipelineDepthStencilStateCreateInfo prof = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo vp = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo muestras = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dinamicos = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dinamico = vks::initializers::pipelineDynamicStateCreateInfo(dinamicos);
		const std::string dir = shaders + "yugireload/";
		VkShaderModule mv = vks::tools::loadShader((dir + "fx_flipbook.vert.spv").c_str(), dev);
		VkShaderModule mf = vks::tools::loadShader((dir + "fx_flipbook.frag.spv").c_str(), dev);
		const std::array<VkPipelineShaderStageCreateInfo, 2> etapas = { {
			{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, mv, "main", nullptr },
			{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, mf, "main", nullptr },
		} };
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
			.pVertexInputState = &entrada,
			.pInputAssemblyState = &ensamble,
			.pViewportState = &vp,
			.pRasterizationState = &raster,
			.pMultisampleState = &muestras,
			.pDepthStencilState = &prof,
			.pColorBlendState = &mezcla,
			.pDynamicState = &dinamico,
			.layout = layout,
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &pipeline));
		vkDestroyShaderModule(dev, mv, nullptr);
		vkDestroyShaderModule(dev, mf, nullptr);
	}
};

}   // namespace fx
