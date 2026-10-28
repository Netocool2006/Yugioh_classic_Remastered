/*
 * Yugioh Classic Remaster - el duelo en PS5 con Vulkan (RADV de PS5_Vulkan), SIN monstruos 3D.
 *
 * Juego hermano de Yugi Reload (mismo tablero, interfaz, mando, menus, Ajustes, fases, LP y carta de campo Bosque),
 * pero las cartas se ven planas sobre el tablero, como en el juego clasico:
 *  - el tablero (tapete y casillas) es un solo glTF armado por Blender (models/yugi/tablero.glb), con sombras del sol
 *    (mapa de 4096 con PCF) y la camara de juego de Unreal;
 *  - las cartas de la mesa las dibuja la interfaz (interfaz.h) con la mesa de control.h: llegan volando de la mano,
 *    se voltean, giran a defensa, destellan al llegar (invocar = destello de aparicion del Niagara Examples Pack);
 *  - atacar: la carta atacante se levanta, se echa atras, se lanza inclinada hacia el objetivo y vuelve; el objetivo
 *    tiembla con el destello del golpe (chispas del pack) y salta el numero de danno; la destruida se desvanece hacia
 *    arriba con una explosion y va al cementerio de su duenno (ATK contra ATK o DEF, como el juego clasico);
 *  - la demo automatica (antes de tocar el mando) ataca sola cada pocos segundos, sin destruir ni quitar LP.
 * Rama classic-figuras (figuras.h): los monstruos de las CARTAS EMBLEMATICAS con modelo se ven como figura 3D de pie
 * sobre su carta (aparecen, respiran, se defienden, atacan con magia/aliento/rayos/embestida y mueren); el resto, plano.
 * Coordenadas: x derecha, y arriba, z hacia el jugador; 1 unidad = 1 m. OPTIONS corto: menu de pausa; OPTIONS 2 s: los Ajustes.
 * Lo que el juego escribe va a /data/yugioh_classic/ (la carpeta del juego puede ser una imagen exFAT de solo lectura
 * montada por ShadowMount+, docs/imagen_titulo.md): mazo, progreso, idioma.txt, fotos y la cache de shaders de RADV.
 * Fotos de la consola: si existe /data/yugioh_classic/capturas.txt, guarda la pantalla en /data/yugioh_classic/capturas/
 * NN.ppm en reposo y en los segundos del primer ataque (uno por linea; "video N": cuadros JPG a N por segundo en
 * capturas/video/); con "cerrar_al_terminar" sale solo al terminar; si aparece /data/yugioh_classic/cerrar.txt, lo borra
 * y sale. Por compatibilidad tambien se aceptan capturas.txt, cerrar.txt e idioma.txt en la carpeta del juego
 * (/data/homebrew/PPSA77002/, instalacion por carpeta). Se bajan por FTP con fotos_ps5.py.
 * En el PC: YUGI_CAPTURAS=<ruta de un capturas.txt>.
 */

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "platform.h"
#include <algorithm>
#include "interfaz.h"   // HUD del duelo y las cartas de la mesa
#include "control.h"    // el duelo con el mando: mesa, foco, menus y eventos
#include "md_ritmo.h"   // rama classic-md-ritmo
#include "bosque.h"     // carta de campo Bosque: el entorno de bosque
#include "entornos.h"   // rama classic-entornos: el entorno del personaje rival (fondo, suelo, luz, siluetas)
#include "campos.h"     // rama classic-campos: las 25 cartas de campo cambian el escenario (fondo, tablero, particulas)
#include "fx.h"         // efectos del Niagara Examples Pack horneados a flipbook
#include "efectos_cartas.h"   // activacion de magias/trampas, dado y moneda (Master Duel, rama classic-fx-md)
#include "audio.h"      // musica y efectos de sonido (rama classic-audio)
#include "velocidad.h"  // rama classic-resultado: velocidad de animaciones (opciones.txt velocidad_anim) y R2
#include <fstream>
#include <iterator>
#include <ctime>
#include <functional>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include "guardado.h"   // rama classic-pulido: guardado atomico y avisos de guardado
#include "raton_juego.h"   // rama classic-pc: el raton (zonas de la interfaz -> pulsos del mando; solo lo lee Windows)
#include "red/sala.h"   // rama classic-sala: buscar rival en linea (sala de Cloudflare y, de respaldo, la DHT; el menu va al unir las ramas de red)
#include "red/nat.h"    // rama classic-red-nat: conexion directa (STUN, UPnP/NAT-PMP, perforacion, codigo manual)
#include "red/lan.h"    // rama classic-red-base: duelos anunciados en la red local
#include "red/duelo_red.h"   // rama classic-red-base: el duelo en lockstep
#include "red/espejo.h"      // rama classic-en-linea: la vista en espejo del invitado
#include "orientacion_figura.h"   // rama classic-bp-terminar: las figuras miran a la pantalla (OJO); solo al atacar al objetivo

#if !defined(PS5_HOST_REFERENCE)
// Carpeta de datos escribible del juego en la consola ("" si no se pudo crear: se usa la del juego, como antes)
static const std::string& carpetaDatosConsola()
{
	static const std::string r = [] {
		const std::string datos = guardado::carpetaDatos();   // (/data/yugioh_classic; Windows: pc/main_windows.cpp)
		mkdir(datos.c_str(), 0777);
		chmod(datos.c_str(), 0777);
		struct stat st{};
		if (stat(datos.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return datos + "/";
		guardado::avisarSinCarpeta();   // (rama classic-pulido: guardado.h, se avisa en pantalla)
		return std::string();
	}();
	return r;
}
// La cache de shaders de RADV iba a /app0/radv-shader-cache (radv_ps5_platform.c la fija con setenv(..., 0), sin pisar
// una ya puesta): se adelanta aqui, antes de crear la instancia de Vulkan, para que sobreviva en una imagen de solo lectura
// (Windows no: el driver de la GPU tiene su cache y la carpeta de datos la elige main_windows.cpp, despues de esto)
#if !defined(_WIN32)
static const bool cacheShadersEnDatos = [] {
	if (!carpetaDatosConsola().empty()) {
		setenv("MESA_SHADER_CACHE_DIR", (carpetaDatosConsola() + "radv-shader-cache").c_str(), 1);
		setenv("PS5VK_SHADER_CACHE_DIR", (carpetaDatosConsola() + "ps5vk-shader-cache").c_str(), 0);
	}
	return true;
}();
#endif
#endif

namespace {
constexpr uint32_t TAM_SOMBRA = 4096;
constexpr VkFormat FORMATO_SOMBRA = VK_FORMAT_D32_SFLOAT;
// CamaraJuego de Unreal: ue(0,12.8,6.4) -> ue(0,-0.6,0), campo de vision horizontal de 62 grados; el tablero se
// agrando x1.4 (escena_tablero.py ESCALA_TABLERO) y la camara se aleja en la misma proporcion (y un poco mas)
constexpr float ESCALA_TABLERO = 1.4f;
const glm::vec3 OJO = orientacion::ojoJuego();   // (= vec3(0, 6.4, 12.8) * ESCALA_TABLERO * 1.08: orientacion_figura.h)
const glm::vec3 OBJETIVO = glm::vec3(0.0f, 0.0f, -0.6f) * ESCALA_TABLERO;
constexpr float FOV_HORIZONTAL = 62.0f;
constexpr float MIRA_EXTRA_GRADOS = 7.0f;   // la camara de juego mira 7 grados mas arriba (rama classic-escenarios)
// hacia el sol: arriba, a la izquierda y hacia la camara (las sombras caen atras a la derecha)
const glm::vec3 HACIA_SOL = glm::normalize(glm::vec3(-0.45f, 1.0f, 0.55f));
// el ataque plano (los momentos estan en interfaz::Interfaz::G_*)
constexpr float DURACION_ATAQUE = interfaz::Interfaz::G_FIN;
constexpr float T_IMPACTO = interfaz::Interfaz::G_IMPACTO;
constexpr float CADA_ATAQUE = 5.0f;   // s entre ataques de la demo automatica

float suave(float a, float b, float x)
{
	const float t = glm::clamp((x - a) / (b - a), 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

// --- camara: matematica (inicio) -------------------------------------------------------------------------------
// Selector de camara (rama selector-camara): las vistas fijas de cada modo y la transicion entre dos camaras. Sin
// estado (la prueba numerica de la rama copia este bloque tal cual, entre "inicio" y "fin")
struct Vista { glm::vec3 ojo, mira; };
constexpr float DURACION_CAMBIO_CAMARA = 0.5f;   // s de la transicion al cambiar de modo
constexpr float LIBRE_ELEV_MIN = 0.09f, LIBRE_ELEV_MAX = 1.48f, LIBRE_DIST_MIN = 7.0f, LIBRE_DIST_MAX = 42.0f;
// CENITAL: desde arriba, casi a plomo (6 grados hacia el jugador: lookAt con "arriba" = +Y necesita que la vista no
// sea vertical) y el lado del jugador abajo en la pantalla
inline Vista vistaCenital()
{
	return { OBJETIVO + glm::vec3(0.0f, 23.5f, 2.5f), OBJETIVO + glm::vec3(0.0f, 0.0f, -0.1f) };
}
// LATERAL: de costado (desde la derecha del jugador), baja: el jugador a la izquierda y el rival a la derecha de
// la pantalla, los monstruos de perfil enfrentados
inline Vista vistaLateral()
{
	const glm::vec3 mira = OBJETIVO + glm::vec3(0.0f, 1.2f, 0.84f);
	return { mira + glm::vec3(23.5f, 4.6f, 3.2f), mira };
}
// LIBRE: orbita alrededor del centro de la mesa; giro (rad, 0 = desde el lado del jugador), elevacion (rad) y
// distancia (m)
inline Vista vistaLibre(float giro, float elev, float dist)
{
	const glm::vec3 mira = OBJETIVO + glm::vec3(0.0f, 0.8f, 0.0f);
	return { mira + dist * glm::vec3(sinf(giro) * cosf(elev), sinf(elev), cosf(giro) * cosf(elev)), mira };
}
// la orbita LIBRE que arranca justo donde esta la camara v (sin salto al entrar a LIBRE)
inline void libreDesde(const Vista& v, float& giro, float& elev, float& dist)
{
	const glm::vec3 d = v.ojo - (OBJETIVO + glm::vec3(0.0f, 0.8f, 0.0f));
	giro = atan2f(d.x, d.z);
	elev = glm::clamp(atan2f(d.y, glm::length(glm::vec2(d.x, d.z))), LIBRE_ELEV_MIN, LIBRE_ELEV_MAX);
	dist = glm::clamp(glm::length(d), LIBRE_DIST_MIN, LIBRE_DIST_MAX);
}
// de la camara a hacia la b (e = 0..1, ya suavizado): el ojo va en arco alrededor del centro de la mesa (angulo,
// radio horizontal y altura), como el corte de cine: en linea recta cruzaria por encima de los monstruos
inline Vista mezclarVistas(const Vista& a, const Vista& b, float e)
{
	const glm::vec2 r0(a.ojo.x - OBJETIVO.x, a.ojo.z - OBJETIVO.z), r1(b.ojo.x - OBJETIVO.x, b.ojo.z - OBJETIVO.z);
	const float a0 = atan2f(r0.x, r0.y);
	const float ang = a0 + remainderf(atan2f(r1.x, r1.y) - a0, 2.0f * glm::pi<float>()) * e;
	const float radio = glm::mix(glm::length(r0), glm::length(r1), e);
	return { glm::vec3(OBJETIVO.x + sinf(ang) * radio, glm::mix(a.ojo.y, b.ojo.y, e), OBJETIVO.z + cosf(ang) * radio), glm::mix(a.mira, b.mira, e) };
}
// --- camara: matematica (fin) ----------------------------------------------------------------------------------
}

class VulkanExample : public VulkanExampleBase
{
public:
	vkglTF::Model tablero;
	vkglTF::Model esfera, rayo;   // primitivas para la luz de la carta de campo (esferas y cilindros aditivos)

	// Ajustes (ventana de la plantilla, OPTIONS 2 s)
	bool animar{ true };
	bool respirar{ true };
	bool ataquesSolos{ true };
	float tiempo{ 0.0f };

	// El ataque plano en curso: zonas de la mesa (control.h), segundo del ataque (< 0: no hay) y su resultado
	int zonaAtacante{ -1 }, zonaObjetivo{ -1 };
	float tAtaque{ -1.0f };
	int danio{ 0 };
	bool danioAlRival{ true };
	bool muereA{ false }, muereO{ false };
	bool ataqueDemo{ false };   // de la demo automatica: no destruye ni quita LP
	float proximoAtaque{ 4.0f };
	int ataquesVistos{ 0 };
	std::array<float, interfaz::NUM_ZONAS> tInvocada{};   // s desde que la carta de esa zona aparecio (destello)
	std::array<float, interfaz::NUM_ZONAS> tRotaCombate{};   // rama classic-md-combate: s desde que la carta de esa zona se rompio en combate
	struct pad mando{};      // la lectura anterior del control (para ver que se acaba de apretar)
	uint32_t mandoAbajo{ 0 };   // rama classic-menus: los botones de verdad abajo en la lectura anterior (ver leerMando)

	// rama classic-menus: lee el mando. "pressed" (lo recien apretado) se calcula contra los botones de la lectura
	// anterior REAL: una pantalla que "consume" el mando borrando mando.held (el constructor lo hacia) convertia cada
	// cuadro con la cruz mantenida en un toque nuevo (la cruz iba "como un rayo"). L2/R2 tambien por su bit digital.
	raton::Juego ratonJuego;   // rama classic-pc
	uint32_t pulsosRaton{ 0 };   // los botones que salieron del raton en este cuadro (avanzarMando / avanzarConstructor / avanzarGaleria los toman)
	void leerMando()
	{
		mando.held = mandoAbajo;
		pad_player(0, &mando);
		pulsosRaton = ratonJuego.paso(interfazDuelo, ctl, glm::clamp(frameTimer, 0.0f, 0.1f));   // rama classic-pc (solo Windows; antes del fin de la pausa)
		mandoAbajo = mando.held;
		constexpr uint32_t PAD_L2_DIGITAL = 0x000100, PAD_R2_DIGITAL = 0x000200;
		if (mando.held & PAD_L2_DIGITAL) mando.l2 = std::max(mando.l2, 1.0f);
		if (mando.held & PAD_R2_DIGITAL) mando.r2 = std::max(mando.r2, 1.0f);
	}
	interfaz::Interfaz interfazDuelo;   // todo lo que rodea al tablero y las cartas de la mesa (interfaz.h)
	audio::Motor sonido;                // rama classic-audio: musica y efectos (audio.h; assets/audio/audio.txt)
	audio::Duelo sonidoDuelo;

	// fotos de la consola a pedido (capturas.txt)
	std::string carpetaCapturas;          // vacia: sin fotos
	std::vector<float> momentosCaptura;   // segundos dentro del primer ataque
	size_t siguienteCaptura{ 0 };
	bool fotoReposo{ false };
	float videoFps{ 0.0f };               // 0: sin video
	bool cerrarAlTerminar{ false };       // salir solo al terminar de grabar el primer ataque
	float tSalida{ -1.0f };
	float revisarCerrar{ 0.0f };          // proxima revision de cerrar.txt
	int cuadroVideo{ 0 };

	struct UniformData {
		glm::mat4 proyeccion;
		glm::mat4 vista;
		glm::mat4 luz;          // mundo -> espacio del mapa de sombras
		glm::vec4 haciaSol;
		glm::vec4 camara;
		glm::vec4 pantalla;     // ancho, alto, tiempo, destello
		glm::vec4 bosque{ 0.0f, -1.0f, -1.0f, 0.0f };   // carta de campo Bosque: ambiente, frente de crecimiento, de retiro, onda
		glm::vec4 bosque2{ 0.0f };                       // origen de la onda (x, z), viento
		glm::vec4 ent[10]{};                             // entorno del rival (entornos::Entornos::uniformes)
	} uniformData;
	std::array<vks::Buffer, maxConcurrentFrames> uniformBuffers;

	// empuje de escena.vert/.frag (el tablero y la luz de la carta de campo; los ojos y alas de los monstruos 3D de
	// Yugi Reload quedan en cero)
	struct Empuje {
		glm::mat4 modelo;
		glm::vec4 efecto;       // x: holograma (< 0: el tapete se cubre de bosque), y: especular, w: emisivo
		glm::vec4 vida;         // xyz: color de la luz, w = 1: modo resplandor
		glm::vec4 ojoA{ 0.0f };
		glm::vec4 ojoB{ 0.0f };
	};
	struct Haz { glm::vec4 a, b, c, d; };   // empuje de efecto_haz.vert/.frag

	struct {
		VkImage imagen{ VK_NULL_HANDLE };
		VkDeviceMemory memoria{ VK_NULL_HANDLE };
		VkImageView vista{ VK_NULL_HANDLE };
		VkSampler muestreador{ VK_NULL_HANDLE };
	} sombra;

	struct {
		VkPipeline escena{ VK_NULL_HANDLE };
		VkPipeline sombra{ VK_NULL_HANDLE };
		VkPipeline efectos{ VK_NULL_HANDLE };      // chispas y resplandores: suman luz
		VkPipeline haz{ VK_NULL_HANDLE };          // aros, columnas de luz y sellos (efecto_haz.*): luz pura, aditiva
	} pipelines;
	fx::Flipbooks flipbooks;
	// rama classic-fx-md: animaciones de activacion de magias/trampas y el dado / la moneda 3D de Master Duel
	efc::Motor efectosCartas;
	vkglTF::Model dadoMd, monedaMd;
	bool hayDadoMd{ false }, hayMonedaMd{ false };
	std::vector<std::string> pruebaCartas;   // YUGI_FX_CARTA=id,id,...: se activan solas (prueba en PC)
	size_t pruebaSiguiente{ 0 };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	std::array<VkDescriptorSet, maxConcurrentFrames> descriptorSets{};

	VulkanExample() : VulkanExampleBase()
	{
		title = "Yugioh Classic Remaster";
		settings.overlay = true;   // la ventana de Ajustes (OPTIONS 2 s) se dibuja; arranca oculta (prepare)
		apiVersion = VK_API_VERSION_1_3;
		useDynamicRendering = true;
		requiresStencil = true;
		deviceCreatepNextChain = &baseDynamicRenderingFeatures;
		tInvocada.fill(99.0f);
		tRotaCombate.fill(99.0f);
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.escena, nullptr);
			vkDestroyPipeline(device, pipelines.sombra, nullptr);
			vkDestroyPipeline(device, pipelines.efectos, nullptr);
			vkDestroyPipeline(device, pipelines.haz, nullptr);
			destruirPipelinesVfxFig();   // rama classic-ps5-ataque
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			vkDestroySampler(device, sombra.muestreador, nullptr);
			vkDestroyImageView(device, sombra.vista, nullptr);
			vkDestroyImage(device, sombra.imagen, nullptr);
			vkFreeMemory(device, sombra.memoria, nullptr);
			for (auto& buffer : uniformBuffers) {
				buffer.destroy();
			}
		}
	}

	void getEnabledFeatures() override
	{
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
	}

	void loadAssets()
	{
		const uint32_t flags = vkglTF::FileLoadingFlags::PreTransformVertices;
		tablero.loadFromFile(getAssetPath() + "models/yugi/tablero.glb", vulkanDevice, queue, flags);
		esfera.loadFromFile(getAssetPath() + "models/yugi/esfera.glb", vulkanDevice, queue, flags);
		rayo.loadFromFile(getAssetPath() + "models/yugi/rayo.glb", vulkanDevice, queue, flags);
		// rama classic-fx-md: dado y moneda de Master Duel (no van al repo; sin ellos no hay objeto 3D, solo el efecto)
		if (std::ifstream(getAssetPath() + "models/yugi/dado_md.glb").good()) {
			dadoMd.loadFromFile(getAssetPath() + "models/yugi/dado_md.glb", vulkanDevice, queue, flags);
			hayDadoMd = true;
		}
		if (std::ifstream(getAssetPath() + "models/yugi/moneda_md.glb").good()) {
			monedaMd.loadFromFile(getAssetPath() + "models/yugi/moneda_md.glb", vulkanDevice, queue, flags);
			hayMonedaMd = true;
		}
	}

	void prepararSombra()
	{
		VkImageCreateInfo imagen = vks::initializers::imageCreateInfo();
		imagen.imageType = VK_IMAGE_TYPE_2D;
		imagen.format = FORMATO_SOMBRA;
		imagen.extent = { TAM_SOMBRA, TAM_SOMBRA, 1 };
		imagen.mipLevels = 1;
		imagen.arrayLayers = 1;
		imagen.samples = VK_SAMPLE_COUNT_1_BIT;
		imagen.tiling = VK_IMAGE_TILING_OPTIMAL;
		imagen.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imagen, nullptr, &sombra.imagen));
		VkMemoryRequirements requisitos;
		vkGetImageMemoryRequirements(device, sombra.imagen, &requisitos);
		VkMemoryAllocateInfo memoria = vks::initializers::memoryAllocateInfo();
		memoria.allocationSize = requisitos.size;
		memoria.memoryTypeIndex = vulkanDevice->getMemoryType(requisitos.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memoria, nullptr, &sombra.memoria));
		VK_CHECK_RESULT(vkBindImageMemory(device, sombra.imagen, sombra.memoria, 0));

		VkImageViewCreateInfo vista = vks::initializers::imageViewCreateInfo();
		vista.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vista.format = FORMATO_SOMBRA;
		vista.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		vista.image = sombra.imagen;
		VK_CHECK_RESULT(vkCreateImageView(device, &vista, nullptr, &sombra.vista));

		// Comparacion por hardware y filtro lineal: cada lectura ya promedia 4 vecinos
		VkSamplerCreateInfo muestreador = vks::initializers::samplerCreateInfo();
		muestreador.magFilter = VK_FILTER_LINEAR;
		muestreador.minFilter = VK_FILTER_LINEAR;
		muestreador.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		muestreador.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		muestreador.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		muestreador.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		muestreador.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		muestreador.compareEnable = VK_TRUE;
		muestreador.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		muestreador.maxLod = 1.0f;
		VK_CHECK_RESULT(vkCreateSampler(device, &muestreador, nullptr, &sombra.muestreador));
	}

	void setupDescriptors()
	{
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxConcurrentFrames),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxConcurrentFrames),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxConcurrentFrames);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
		// Set 0: uniformes del cuadro y mapa de sombras. Set 1: textura de color de cada material (vkglTF)
		const std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VkDescriptorImageInfo mapaSombra = vks::initializers::descriptorImageInfo(sombra.muestreador, sombra.vista, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
		for (size_t i = 0; i < uniformBuffers.size(); i++) {
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets[i]));
			std::array<VkWriteDescriptorSet, 2> writes = {
				vks::initializers::writeDescriptorSet(descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers[i].descriptor),
				vks::initializers::writeDescriptorSet(descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &mapaSombra),
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
		}
	}

	void preparePipelines()
	{
		const std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayout, vkglTF::descriptorSetLayoutImage };
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
		VkPushConstantRange empuje = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Empuje), 0);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &empuje;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
			loadShader(getShadersPath() + "yugireload/escena.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getShadersPath() + "yugireload/escena.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
		};
		VkPipelineRenderingCreateInfo renderingCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapChain.colorFormat,
			.depthAttachmentFormat = depthFormat,
			.stencilAttachmentFormat = depthFormat,
		};
		VkGraphicsPipelineCreateInfo pipelineCI{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderingCreateInfo,
			.stageCount = static_cast<uint32_t>(shaderStages.size()),
			.pStages = shaderStages.data(),
			.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV }),
			.pInputAssemblyState = &inputAssemblyState,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizationState,
			.pMultisampleState = &multisampleState,
			.pDepthStencilState = &depthStencilState,
			.pColorBlendState = &colorBlendState,
			.pDynamicState = &dynamicState,
			.layout = pipelineLayout,
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.escena));
		VkPipelineDepthStencilStateCreateInfo sinEscribir = depthStencilState;
		sinEscribir.depthWriteEnable = VK_FALSE;
		{
			// luz: mezcla aditiva (ONE + ONE) y sin escribir profundidad (se ven a traves unos de otros)
			VkPipelineColorBlendAttachmentState suma = blendAttachmentState;
			suma.blendEnable = VK_TRUE;
			suma.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			suma.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			suma.colorBlendOp = VK_BLEND_OP_ADD;
			suma.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			suma.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			suma.alphaBlendOp = VK_BLEND_OP_ADD;
			VkPipelineColorBlendStateCreateInfo mezclaSuma = vks::initializers::pipelineColorBlendStateCreateInfo(1, &suma);
			VkGraphicsPipelineCreateInfo efectosCI = pipelineCI;
			efectosCI.pColorBlendState = &mezclaSuma;
			efectosCI.pDepthStencilState = &sinEscribir;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &efectosCI, nullptr, &pipelines.efectos));
		}
		VkPipelineVertexInputStateCreateInfo sinVertices = vks::initializers::pipelineVertexInputStateCreateInfo();
		crearPipelineHaz(pipelineCI, sinVertices, sinEscribir);
		crearPipelinesVfxFig(pipelineCI, sinVertices, sinEscribir);   // rama classic-ps5-ataque: fuego y humo de Yugi Reload (ataque_fig_fx.h)

		// Mapa de sombras: solo profundidad, con desplazamiento para evitar el "acne"
		VkPipelineRenderingCreateInfo renderingSombra{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 0,
			.depthAttachmentFormat = FORMATO_SOMBRA,
		};
		VkPipelineColorBlendStateCreateInfo sinColor = vks::initializers::pipelineColorBlendStateCreateInfo(0, nullptr);
		rasterizationState.depthBiasEnable = VK_TRUE;
		rasterizationState.depthBiasConstantFactor = 1.25f;
		rasterizationState.depthBiasSlopeFactor = 1.75f;
		VkPipelineShaderStageCreateInfo etapaSombra = loadShader(getShadersPath() + "yugireload/sombra.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		pipelineCI.pNext = &renderingSombra;
		pipelineCI.stageCount = 1;
		pipelineCI.pStages = &etapaSombra;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal });
		pipelineCI.pColorBlendState = &sinColor;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.sombra));
	}

	void crearPipelineHaz(const VkGraphicsPipelineCreateInfo& base, const VkPipelineVertexInputStateCreateInfo& sinVertices,
	                      const VkPipelineDepthStencilStateCreateInfo& sinEscribir)
	{
		VkPipelineColorBlendAttachmentState suma{};
		suma.colorWriteMask = 0xf;
		suma.blendEnable = VK_TRUE;
		suma.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		suma.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		suma.colorBlendOp = VK_BLEND_OP_ADD;
		suma.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		suma.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		suma.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo mezcla = vks::initializers::pipelineColorBlendStateCreateInfo(1, &suma);
		std::array<VkPipelineShaderStageCreateInfo, 2> etapas = {
			loadShader(getShadersPath() + "yugireload/efecto_haz.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getShadersPath() + "yugireload/efecto_haz.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
		};
		VkGraphicsPipelineCreateInfo ci = base;
		ci.stageCount = 2;
		ci.pStages = etapas.data();
		ci.pVertexInputState = &sinVertices;
		ci.pColorBlendState = &mezcla;
		ci.pDepthStencilState = &sinEscribir;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, nullptr, &pipelines.haz));
	}

	void prepareUniformBuffers()
	{
		for (auto& buffer : uniformBuffers) {
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer, sizeof(UniformData), &uniformData));
			VK_CHECK_RESULT(buffer.map());
		}
	}

	// ------------------------------------------------------------------ el ataque plano, momento a momento
	bool hayAtaque() const { return tAtaque >= 0.0f && zonaAtacante >= 0 && zonaObjetivo >= 0; }
	// destello de la pantalla en el golpe (escena.frag)
	// (rama classic-ps5-ataque: el directo no aclara toda la mesa: MD marca el golpe al jugador con el DamageFrame rojo)
	float destello() const { return hayAtaque() && !ataqueDirecto && tAtaque > T_IMPACTO ? 0.13f * expf(-(tAtaque - T_IMPACTO) * 12.0f) : 0.0f; }
	glm::vec3 centroZona(int z) const { return glm::vec3(interfaz::ZONAS[z].x * interfaz::ESCALA, 0.0f, interfaz::ZONAS[z].z * interfaz::ESCALA); }
	static bool esMonstruo(int z) { return z >= 0 && z < (int)interfaz::NUM_ZONAS && strcmp(interfaz::ZONAS[z].tipo, "monstruo") == 0; }
	static bool zonaRival(int z) { return interfaz::ZONAS[z].id[0] == 'r'; }

	// Empieza el ataque de la carta de la zona a contra la de la zona o y calcula el resultado como el juego clasico:
	// contra un monstruo en ataque gana el de mas ATK (empate: los dos se destruyen) y el duenno del perdedor recibe
	// la diferencia; contra uno en defensa, si el ATK pasa la DEF se destruye sin danno y si no llega, el atacante
	// recibe la diferencia. La demo automatica solo muestra (sin destruir ni quitar LP)
	bool atacar(int a, int o, bool demo)
	{
		if (hayAtaque() || !esMonstruo(a) || !esMonstruo(o) || zonaRival(a) == zonaRival(o) || ctl.zonas[a].vacia() || ctl.zonas[o].vacia()) {
			return false;
		}
		const control::Celda& ca = ctl.zonas[a];
		const control::Celda& co = ctl.zonas[o];
		const int atk = interfazDuelo.atkDe(ca.carta);
		const bool defensa = co.defensa;
		const int otro = defensa ? interfazDuelo.defDe(co.carta) : interfazDuelo.atkDe(co.carta);
		const bool rivalAtaca = zonaRival(a);
		muereA = muereO = false;
		danio = 0;
		if (atk > otro) {
			muereO = true;
			if (!defensa) {
				danio = atk - otro;
				danioAlRival = !rivalAtaca;
			}
		} else if (atk < otro) {
			muereA = !defensa;
			danio = otro - atk;
			danioAlRival = rivalAtaca;
		} else if (!defensa && atk > 0) {
			muereA = muereO = true;
		}
		zonaAtacante = a;
		zonaObjetivo = o;
		tAtaque = 0.0f;
		ataqueDirecto = false;   // rama classic-ps5-ataque (ataque_md_juego.h)
		elevarAtacante(a);       // rama classic-ps5-ataque5 (el ataque de la demo y de las pruebas de PC)
		ataqueDemo = demo;
		ataquesVistos++;
		ataqueAltoMD = ritmo::ataqueAlto(atk);   // rama classic-md-combate: viaje "High" de MD (ATK >= 2500)
		if (ctl.log) {
			fprintf(stderr, "[ataque] %s (%d) -> %s (%d%s): danno %d, destruye %s%s\n", ca.carta.c_str(), atk, co.carta.c_str(), otro, defensa ? " DEF" : "", danio,
			        muereA ? "atacante " : "", muereO ? "objetivo" : "");
		}
		return true;
	}

	// cada cuadro: avanza el ataque en curso (el golpe voltea al objetivo boca abajo, quita LP; al final las
	// destruidas van al cementerio) y, en la demo, lanza el siguiente
	void avanzarAtaque()
	{
		leerMando();
		// (rama classic-arreglo-ciclo-md, jugador humano: la pausa / el constructor abiertos detienen el ataque como al resto
		// del duelo; antes el ataque seguia y al cerrar el combate ya habia pasado)
		if (pausa.abierta || constructorMazo.abierto) return;
		for (float& t : tInvocada) {
			t += frameTimer;
		}
		for (float& t : tRotaCombate) t += frameTimer * escalaAnim();   // rama classic-md-combate
		avanzarRoturasEfectoMD(frameTimer * escalaAnim());   // rama classic-ps5-efectos3
		actualizarEquipos(frameTimer);   // rama classic-ps5-equipo-ovni
		avanzarElevacion(frameTimer * escalaAnim());   // rama classic-ps5-ataque5: el atacante declarado flota sobre su zona
		vigilarElevacion();
		interfazDuelo.elevacionAtacante(elevZona, elevacionDe(elevZona));
		if (hayAtaque()) {
			const float antes = tAtaque;
			// rama classic-figuras: con figura 3D el ataque va mas lento (classic-resultado: y la velocidad elegida). Rama
			// classic-md-combate: el reloj es el del viaje "Low" de MD; con ATK alto el viaje dura 0.65 (md_ritmo.h)
			if (tAtaque >= ritmo::T_FIN_ATAQUE - ELEV_SALE) bajarAtacante();   // rama classic-ps5-ataque5: baja en el ultimo 0.2 s
			if (tAtaque == 0.0f) prepararCalculoAtaque();   // rama classic-ps5-ataque4: el calculo que se muestra sobre las cartas
			tAtaque += ritmo::pasoRelojAtaque(tAtaque, frameTimer * velocidadAtaque() * escalaAnim(), ataqueAltoMD);
			// rama classic-md-combate: tras la pausa del golpe (MD: attackExecPostTime 0.25) la destruida se rompe
			// (fxp_cardbrk_btl_001, SE_CARDBREAK_02) y viaja al cementerio (CardMoveField2GraveNormal), como las de efecto.
			// (la demo la deja en su sitio; con figura 3D, la figura hace su muerte)
			if (antes < ritmo::T_ROMPE && tAtaque >= ritmo::T_ROMPE && !ataqueDemo) {
				const int zs[2] = { muereO ? zonaObjetivo : -1, muereA ? zonaAtacante : -1 };
				for (int z : zs) {
					if (z < 0 || tieneFigura(z) || !ritmoDestruidaCombate(z)) continue;   // (con figura: al final, ver abajo)
					tRotaCombate[z] = 0.0f;
					sonidoDuelo.cemJ = (int)ctl.cementerioJ.size();   // (ya sono la rotura: sin el sonido de la pila que crece)
					sonidoDuelo.cemR = (int)ctl.cementerioR.size();
				}
			}
			if (antes < T_IMPACTO && tAtaque >= T_IMPACTO) {
				control::Celda& co = ctl.zonas[zonaObjetivo];
				if (co.bocaAbajo) {
					co.antesBocaAbajo = true;
					co.bocaAbajo = false;
					co.tVolteo = 0.0f;
				}
			}
			// rama classic-ps5-ataque4: los LP bajan despues del calculo sobre las cartas, como en Joey (ritmo::T_LP)
			if (antes < ritmo::T_LP && tAtaque >= ritmo::T_LP && !ataqueDemo && danio > 0) {
				interfazDuelo.cambiarLP(danioAlRival, interfazDuelo.lp(danioAlRival) - danio);
			}
			if (tAtaque > DURACION_ATAQUE) {
				if (!ataqueDemo) {
					// rama classic-md-combate: con figura 3D, cuando la figura termina de morir la carta se rompe y viaja
					// (las demas ya viajaron a T_ROMPE: su zona esta vacia)
					const int zs[2] = { muereO ? zonaObjetivo : -1, muereA ? zonaAtacante : -1 };
					for (int z : zs) {
						if (z < 0 || ctl.zonas[z].vacia()) continue;
						if (ritmoDestruidaCombate(z)) {
							tRotaCombate[z] = 0.0f;
							sonidoDuelo.cemJ = (int)ctl.cementerioJ.size();
							sonidoDuelo.cemR = (int)ctl.cementerioR.size();
						} else {
							ctl.alCementerio(z);
						}
					}
				} else {
					// demo: la destruida vuelve a aparecer (con el destello de llegada)
					if (muereO) ctl.zonas[zonaObjetivo].tLlega = 0.0f;
					if (muereA) ctl.zonas[zonaAtacante].tLlega = 0.0f;
				}
				tAtaque = -1.0f;
				zonaAtacante = zonaObjetivo = -1;
				bajarAtacante();   // rama classic-ps5-ataque5
				proximoAtaque = tiempo + CADA_ATAQUE;
			}
		} else if (ataquesSolos && ctl.demo && tiempo >= proximoAtaque && !campoEnTransicion()) {
			// demo: un monstruo boca arriba en ataque de un lado contra cualquiera del otro (se turnan los lados)
			std::vector<int> atacantes, objetivos;
			const bool lado = (ataquesVistos % 2) != 0;
			for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
				if (!esMonstruo(z) || ctl.zonas[z].vacia()) continue;
				if (zonaRival(z) == lado && !ctl.zonas[z].bocaAbajo && !ctl.zonas[z].defensa) atacantes.push_back(z);
				if (zonaRival(z) != lado) objetivos.push_back(z);
			}
			proximoAtaque = tiempo + 1.0f;
			if (!atacantes.empty() && !objetivos.empty()) {
				const uint32_t n = (uint32_t)ataquesVistos * 7u + 3u;
				atacar(atacantes[(size_t)(azar(n) * 997.0f) % atacantes.size()], objetivos[(size_t)(azar(n + 1u) * 997.0f) % objetivos.size()], true);
			} else {
				ataquesVistos++;   // (de ese lado no hay quien ataque: le toca al otro)
			}
		}
		interfaz::Interfaz::Golpe g;
		if (hayAtaque()) {
			g = { zonaAtacante, zonaObjetivo, tAtaque, danio, muereA, muereO, ataqueAltoMD };
			// rama classic-ps5-ataque (ataque_md.h): el directo va al punto de MD; el danno del jugador enciende el DamageFrame
			g.directo = ataqueDirecto;
			g.punto = puntoDirecto;
			g.alJugador = !danioAlRival;
			g.valorA = calcA;   // rama classic-ps5-ataque4
			g.valorO = calcO;
			g.defensaO = calcDef;
		}
		interfazDuelo.golpePlano(g);
	}

	// los efectos del pack de este cuadro: la aparicion de cada carta invocada, las chispas del golpe y la explosion
	// de las destruidas
	void efectosPack()
	{
		using fx::Fx;
		if (!flipbooks.listo) {
			return;
		}
		const glm::vec3 arriba(0.0f, 1.0f, 0.0f);
		for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
			const float t = tInvocada[z];
			if (t > 3.0f) continue;
			if (figuras3D && tieneFigura(z)) continue;   // rama classic-ps5-equipo-ovni: la figura entra con la abduccion del Bosque
			const glm::vec4 luz(zonaRival(z) ? glm::vec3(1.3f, 0.75f, 0.85f) : glm::vec3(0.8f, 1.1f, 1.4f), 1.0f);
			flipbooks.pedir(Fx::Aparicion, centroZona(z) + arriba * 0.06f, 2.2f, t, luz);
			flipbooks.pedir(Fx::TeleportIn, centroZona(z), 2.4f, t - 0.05f, luz);
		}
		efectosDeCartas();   // rama classic-fx-md
		pedirRoturasEfectoMD();   // rama classic-ps5-efectos3: las destruidas por un efecto (MD CardBreak)
		// rama classic-md-combate: la destruida en combate estalla en su zona (MD: fxp_cardbrk_btl_001) mientras viaja
		if (flipbooks.hay(Fx::MdDestruirBatalla)) {
			for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
				if (tRotaCombate[z] < ritmo::T_ROMPE_FX) flipbooks.pedir(Fx::MdDestruirBatalla, centroZona(z) + arriba * 0.3f, 3.2f, tRotaCombate[z], glm::vec4(1.0f));
			}
		}
		if (!hayAtaque()) {
			return;
		}
		const float t = tAtaque;
		const glm::vec3 o = centroZona(zonaObjetivo), a = centroZona(zonaAtacante);
		// (rama classic-ps5-ataque: el directo pega en el punto de MD del que recibe, con su golpe grande: ataque_md_juego.h)
		// (rama classic-ps5-ataque4: con figura que ataca, el golpe es en la carta del rival, no en medio del camino)
		const glm::vec3 golpe = ataqueDirecto ? puntoDirecto : tieneFigura(zonaAtacante) ? o + arriba * 0.35f : glm::mix(a, o, 0.75f) + arriba * 0.35f;
		// (el punto del directo al jugador esta cerca de la camara: el golpe se achica para verse del tamanno de uno en la
		// mesa, como el fxp_hit de MD; antes una explosion de 3-4.5 m tapaba media pantalla)
		const float cerca = ataqueDirecto ? glm::clamp(glm::distance(OJO, golpe) / glm::distance(OJO, OBJETIVO), 0.3f, 1.0f) : 1.0f;
		flipbooks.pedir(Fx::ImpactoMetal, golpe, 1.6f * cerca, t - T_IMPACTO, glm::vec4(1.3f, 1.15f, 0.9f, 1.0f));
		flipbooks.pedir(Fx::Chispas, golpe, 1.8f * cerca, t - T_IMPACTO, glm::vec4(1.4f, 1.1f, 0.7f, 1.0f));
		if (ataqueDirecto) flipbooks.pedir(Fx::ExplosionChica, golpe, 1.4f * cerca, t - T_IMPACTO, glm::vec4(1.2f, 1.05f, 0.9f, 1.0f));
		// rama classic-ps5-ataque4 (el usuario: "SIEMPRE la carta debe pegarle a la otra y SIEMPRE sacar humo la del
		// contrincante"): en todo golpe a un monstruo, polvo y humo que salen de la carta del rival, la destruya o no
		// (solo con los flipbooks que estan horneados en classic-assets/fx: NS_SmokePuffLight, NS_Smoke_Plume, NS_Spark_Burst)
		if (!ataqueDirecto) {
			const float u = t - T_IMPACTO;
			if (u > 0.0f && u < 2.2f) {
				const float humo = suave(0.0f, 0.12f, u) * (1.0f - suave(1.0f, 2.2f, u));
				for (int k = 0; k < 4; k++) {   // bocanadas que salen de la carta y suben
					const float a0 = 1.57f * (float)k + 0.6f, sube = 0.25f + 0.9f * suave(0.0f, 1.6f, u);
					const glm::vec3 p = o + glm::vec3(0.45f * cosf(a0) * (0.6f + 0.4f * u), sube + 0.15f * (float)(k % 2), 0.35f * sinf(a0) * (0.6f + 0.4f * u));
					flipbooks.pedir(Fx::Bocanada, p, 1.3f + 0.5f * u, 0.2f * (float)k + 0.5f * u, glm::vec4(0.55f, 0.53f, 0.5f, 0.85f * humo));
				}
				flipbooks.pedir(Fx::ColumnaHumo, o, 2.2f, u, glm::vec4(0.7f, 0.7f, 0.7f, 0.7f * humo));
			}
		}
		const float muere = interfaz::Interfaz::G_DESVANECE;
		for (int k = 0; k < 2; k++) {
			if (!(k == 0 ? muereO : muereA)) continue;
			if (tRotaCombate[k == 0 ? zonaObjetivo : zonaAtacante] < ritmo::T_ROMPE_FX) continue;   // (rama classic-md-combate: ya estalla la de MD)
			const glm::vec3 c = k == 0 ? o : a;
			flipbooks.pedir(Fx::ExplosionMediana, c + arriba * 0.3f, 2.6f, t - muere, glm::vec4(1.2f, 1.0f, 0.9f, 1.0f));
			flipbooks.pedir(Fx::Disolver, c + arriba * 0.4f, 2.0f, t - muere, glm::vec4(1.2f, 1.1f, 1.3f, 1.0f));
		}
	}

	// ------------------------------------------------------------------ rama classic-fx-md: efectos de cartas
	// (efectos_cartas.h). La activacion llega del control (EV_ACTIVAR_MAGIA) y del duelo contra la IA (S_ACTIVA_MT)
	void activarEfectoCarta(const std::string& carta, int zona)
	{
		const size_t antes = efectosCartas.activos.size();
		efectosCartas.activar(carta, zona, zonaRival(zona), (uint32_t)(tiempo * 1000.0f) + (uint32_t)zona * 7919u);
		// rama classic-ps5-efectos: el CardRunEffect de Master Duel (md_efectos_ui.h) y su sonido (seLabel)
		if (efectosCartas.activos.size() > antes) {
			const efc::Receta r = efectosCartas.activos.back().receta;
			if (r == efc::Receta::Espadas) interfazDuelo.lanzarEspadasMD(zonaRival(zona));
			interfazDuelo.lanzarCardRunMD(efc::nombre(r), zonaRival(zona));   // (pista, pantallas y sacudida de MD: md_cardrun_ui.h)
			const char* se = efc::sonidoMD(r);
			if (*se && sonido.hay(se)) sonido.sfx(se, 1.0f, zona >= 0 ? control::ZONAS_MESA[zona].x / 12.0f : 0.0f);
		}
	}

	// cada cuadro: avanza las animaciones, pone el resultado del dado / la moneda en el aviso y la prueba en PC
	void avanzarEfectosCartas(float dt)
	{
		if (!pruebaCartas.empty() && pruebaSiguiente < pruebaCartas.size()) {
			const char* c = getenv("YUGI_FX_CADA");
			const float cada = c ? (float)atof(c) : 4.0f;
			const char* i = getenv("YUGI_FX_INICIO");
			const float inicio = i ? (float)atof(i) : 1.0f;
			if (tiempo >= inicio + cada * (float)pruebaSiguiente) {
				const std::string& id = pruebaCartas[pruebaSiguiente++];
				const bool rival = id.rfind("r:", 0) == 0;
				activarEfectoCarta(rival ? id.substr(2) : id, control::zonaPorId(rival ? "rival_magia_2" : "jugador_magia_2"));
			}
		}
		efectosCartas.avanzar(dt);
		interfazDuelo.avanzarEfectosMD(dt);   // rama classic-ps5-efectos (md_efectos_ui.h)
		std::string texto;
		for (const efc::Activo& a : efectosCartas.activos) {
			if (efectosCartas.resultadoListo(a, dt, texto)) {
				ctl.aviso = texto;
				ctl.tAviso = 0.0f;
			}
		}
	}

	void efectosDeCartas()
	{
		if (!efectosCartas.hay()) return;
		std::vector<int> monstruos[2], magias[2];
		for (int lado = 0; lado < 2; lado++) {
			const std::string l = lado ? "rival_" : "jugador_";
			for (int i = 0; i < 5; i++) {
				monstruos[lado].push_back(control::zonaPorId(l + "monstruo_" + std::to_string(i)));
				magias[lado].push_back(control::zonaPorId(l + "magia_" + std::to_string(i)));
			}
		}
		const int mazos[2] = { control::zonaPorId("jugador_mazo"), control::zonaPorId("rival_mazo") };
		efectosCartas.pedir(
			flipbooks, [&](int z) { return centroZona(z); }, [&](int z) { return z >= 0 && !ctl.zonas[z].vacia(); }, monstruos, magias, mazos);
	}

	// el dado / la moneda 3D (mallas y curvas de Master Duel): rueda en el centro del lado de quien activa
	void dibujarDadoMoneda(VkCommandBuffer cmd)
	{
		if (!efectosCartas.hay()) return;
		bool atado = false;
		for (const efc::Activo& a : efectosCartas.activos) {
			const bool dado = a.receta == efc::Receta::Dado;
			if (dado ? !hayDadoMd : (a.receta != efc::Receta::Moneda || !hayMonedaMd)) continue;
			const glm::vec3 base = 0.5f * (centroZona(control::zonaPorId(a.rival ? "rival_monstruo_2" : "jugador_monstruo_2")) +
			                               centroZona(control::zonaPorId(a.rival ? "rival_magia_2" : "jugador_magia_2")));
			glm::mat4 m;
			if (!efectosCartas.matrizObjeto(a, base, glm::vec3(uniformData.camara), m)) continue;
			if (!atado) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.escena);
				atado = true;
			}
			// especular alto (dado lacado / moneda metalica) y un poco de brillo propio para que se lea en la mesa oscura
			dibujar(cmd, dado ? dadoMd : monedaMd, m, glm::vec4(0.0f, 0.9f, 0.0f, 0.0f), glm::vec4(0.0f), true);
		}
		// Raigeki: rayos quebrados del cielo a cada zona de monstruo del rival (cilindros que suman luz). Rama
		// classic-ps5-efectos: ya no (eran propios); Raigeki es su CardRunEffect de MD, Ef04343 (md_cardrun_ui.h)
		bool aditivo = false;
		for (const efc::Activo& a : efectosCartas.activos) {
			if (a.receta == efc::Receta::Raigeki || a.receta != efc::Receta::Raigeki) continue;
			if (!aditivo) {
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.efectos);
				aditivo = true;
			}
			const std::string otro = a.rival ? "jugador_monstruo_" : "rival_monstruo_";
			for (int k = 0; k < 5; k++) {
				const float tk = a.t - (0.35f + 0.12f * (float)k);
				if (tk < 0.0f || tk > 0.55f) continue;
				const float vida = 1.0f - tk / 0.55f;
				const glm::vec3 suelo = centroZona(control::zonaPorId(otro + std::to_string(k)));
				const uint32_t parpadeo = (uint32_t)(a.t * 20.0f) * 31u + (uint32_t)k * 977u;
				// (desde arriba y hacia la camara: lo que queda por encima del borde lejano de la mesa lo tapa el fondo)
				const glm::vec3 cielo(0.0f, 7.5f, 4.5f);
				glm::vec3 desde = suelo + cielo + glm::vec3((azar(parpadeo) - 0.5f) * 2.0f, 0.0f, 0.0f);
				const int tramos = 8;
				for (int i = 1; i <= tramos; i++) {
					const float f = (float)i / (float)tramos;
					glm::vec3 hasta = glm::mix(suelo + cielo, suelo, f);
					if (i < tramos) hasta += glm::vec3(azar(parpadeo + (uint32_t)i * 7u) - 0.5f, 0.0f, azar(parpadeo + (uint32_t)i * 13u) - 0.5f) * 1.6f;
					tramo(cmd, desde, hasta, 0.16f, glm::vec3(0.9f, 0.95f, 1.0f), 4.0f * vida);
					tramo(cmd, desde, hasta, 0.6f, glm::vec3(0.45f, 0.55f, 1.0f), 0.7f * vida);
					desde = hasta;
				}
				bola(cmd, suelo + glm::vec3(0.0f, 0.3f, 0.0f), 0.5f + 0.4f * vida, glm::vec3(0.6f, 0.7f, 1.0f), 0.6f * vida);
			}
		}
	}

	// ------------------------------------------------------------------ camara y uniformes
	void updateUniformBuffers()
	{
		if (galeriaVisible()) {   // rama classic-galeria: la camara y la luz del visor (galeria_juego.h)
			uniformesGaleria();
			return;
		}
		const float aspecto = (float)width / (float)height;
		const float fovVertical = 2.0f * atanf(tanf(glm::radians(FOV_HORIZONTAL) * 0.5f) / aspecto);
		uniformData.proyeccion = glm::perspectiveRH(fovVertical, aspecto, 0.1f, 80.0f);
		uniformData.proyeccion[1][1] *= -1.0f; // Vulkan: Y de la pantalla hacia abajo
		// el tablero se corre a la derecha de la columna de la interfaz (LP, visor, fases)
		uniformData.proyeccion = interfazDuelo.ajustarProyeccion(uniformData.proyeccion, (float)width, (float)height);
		glm::vec3 ojo = OJO;
		glm::vec3 mira = OBJETIVO;
		// la mira de la camara de juego (CINE/TABLERO) sube 7 grados (rama classic-escenarios: el usuario lo pidio para
		// ver el fondo, p. ej. las torres del castillo, sobre la mano del rival). PC: YUGI_CAMARA_MIRA=<grados> la cambia
		// (0 = la de antes). Las otras camaras (cenital, lateral, libre) no cambian
#if defined(PS5_HOST_REFERENCE)
		static const float miraExtra = glm::radians(getenv("YUGI_CAMARA_MIRA") ? (float)atof(getenv("YUGI_CAMARA_MIRA")) : MIRA_EXTRA_GRADOS);
#else
		const float miraExtra = glm::radians(MIRA_EXTRA_GRADOS);
#endif
		if (miraExtra != 0.0f) {
			const glm::vec3 d = mira - ojo;
			const float h = glm::length(glm::vec2(d.x, d.z));
			const float p = atan2f(d.y, h) + miraExtra;
			mira = ojo + glm::vec3(d.x / h, 0.0f, d.z / h) * (glm::length(d) * cosf(p)) + glm::vec3(0.0f, glm::length(d) * sinf(p), 0.0f);
		}
		if (respirar) {
			// la camara respira: un vaiven lento de lado a lado y un poco de altura
			ojo.x += sinf(tiempo * 0.21f) * 0.7f;
			ojo.y += sinf(tiempo * 0.13f) * 0.25f;
		}
		const Vista vistaTablero{ ojo, mira };   // la camara general/frontal del tablero (modo TABLERO del selector)
		pesoCine = 0.0f;
		if (hayAtaque() && ataqueDirecto) {
			// rama classic-ps5-ataque: el directo como MD: la camara no se acerca a una zona; en el golpe la sacude
			// (MainCameraOrganizer.Shake "AttackDirectNear/Far", ataque_md_juego.h)
			sacudidaAtaqueDirecto(ojo, mira, realAtaqueFig(tAtaque) - realAtaqueFig(ritmo::T_LP));   // (ataque4: con la bajada de LP)
		} else if (hayAtaque()) {
			// CINE: la camara se acerca un poco al choque y vuelve; sacudida en el golpe
			const float cine = suave(0.0f, 0.4f, tAtaque) * (1.0f - suave(DURACION_ATAQUE - 0.45f, DURACION_ATAQUE, tAtaque));
			pesoCine = cine;
			const glm::vec3 medio = 0.5f * (centroZona(zonaAtacante) + centroZona(zonaObjetivo));
			mira = glm::mix(mira, medio, 0.45f * cine);
			ojo = glm::mix(ojo, medio + (ojo - mira) * 0.8f, 0.25f * cine);
			ojo += glm::vec3(sinf(tAtaque * 71.0f), cosf(tAtaque * 53.0f), 0.0f) * 0.15f * destello();
		}
		elegirCamara(ojo, mira, vistaTablero);   // selector de camara: CINE deja lo de arriba; los otros modos la reemplazan
		camaraEntradaClave(ojo, mira);   // rama classic-cartas-clave: la camara de la entrada de una carta clave (entrada_clave_juego.h)
		// rotulos ATK/DEF de la interfaz: solo con la camara frontal
		camaraIntacta = ojo == vistaMostrada.ojo && mira == vistaMostrada.mira;
		interfazDuelo.rotulosVisibles(camaraFrontalActiva());
		{   // rama classic-ps5-efectos: la sacudida de camara de MD (CameraShaker.shakeOffset, unidades de MD -> la mesa por
			// la distancia de la camara: la de MD esta a ~102 de la mesa; z al reves)
			const glm::vec3 sac = interfazDuelo.sacudidaMD();
			if (sac != glm::vec3(0.0f)) {
				const glm::vec3 d = glm::vec3(sac.x, sac.y, -sac.z) * (glm::length(ojo - mira) / 102.0f);
				ojo += d;
				mira += d;
			}
		}
		uniformData.vista = glm::lookAtRH(ojo, mira, glm::vec3(0.0f, 1.0f, 0.0f));

		// Sol: proyeccion ortografica que cubre la mesa (19.5 x 14.7 m)
		const glm::mat4 vistaSol = glm::lookAtRH(HACIA_SOL * (20.0f * ESCALA_TABLERO), glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
		const glm::mat4 proySol = glm::orthoRH(-13.5f * ESCALA_TABLERO, 13.5f * ESCALA_TABLERO, -12.0f * ESCALA_TABLERO, 12.0f * ESCALA_TABLERO, 1.0f, 45.0f * ESCALA_TABLERO);
		uniformData.luz = proySol * vistaSol;
		uniformData.haciaSol = glm::vec4(HACIA_SOL, 0.0f);
		uniformData.camara = glm::vec4(ojo, 1.0f);
		uniformData.pantalla = glm::vec4((float)width, (float)height, tiempo, destello());
		uniformesCampo();   // carta de campo Bosque
		entornosRival.uniformes(uniformData.ent);   // rama classic-entornos
		memcpy(uniformBuffers[currentBuffer].mapped, &uniformData, sizeof(uniformData));
	}

	static float azar(uint32_t n)
	{
		n = (n << 13u) ^ n;
		n = n * (n * n * 15731u + 789221u) + 1376312589u;
		return (float)(n & 0x7fffffffu) / 2147483647.0f;   // 0..1
	}

	// un tramo de rayo: cilindro de "desde" a "hasta"; brillo = intensidad, radio en metros
	void tramo(VkCommandBuffer cmd, glm::vec3 desde, glm::vec3 hasta, float radio, glm::vec3 color, float brillo)
	{
		const glm::vec3 d = hasta - desde;
		const float largo = glm::length(d);
		if (largo < 1e-4f) {
			return;
		}
		const glm::mat4 m = glm::translate(glm::mat4(1.0f), desde) * glm::mat4_cast(glm::quat(glm::vec3(0.0f, 1.0f, 0.0f), d / largo)) *
		                    glm::scale(glm::mat4(1.0f), glm::vec3(radio, largo, radio));
		// efecto.w = brillo; vida.xyz = color; vida.w = 1: modo resplandor (centro brillante, bordes suaves)
		dibujar(cmd, rayo, m, glm::vec4(0.0f, 0.0f, 0.0f, brillo), glm::vec4(color, 1.0f), true);
	}

	void bola(VkCommandBuffer cmd, glm::vec3 centro, float radio, glm::vec3 color, float brillo)
	{
		dibujar(cmd, esfera, glm::scale(glm::translate(glm::mat4(1.0f), centro), glm::vec3(radio)),
		        glm::vec4(0.0f, 0.0f, 0.0f, brillo), glm::vec4(color, 1.0f), true);
	}

	void haz(VkCommandBuffer cmd, glm::vec4 a, glm::vec4 b, glm::vec4 c, glm::vec4 d)
	{
		const Haz h{ a, b, c, d };
		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Haz), &h);
		vkCmdDraw(cmd, 6, 1, 0, 0);
	}

	void dibujar(VkCommandBuffer cmd, vkglTF::Model& modelo, const glm::mat4& matriz, glm::vec4 efecto, glm::vec4 vida, bool imagenes,
	             glm::vec4 ojoA = glm::vec4(0.0f), glm::vec4 ojoB = glm::vec4(0.0f))
	{
		const Empuje empuje{ matriz, efecto, vida, ojoA, ojoB };
		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Empuje), &empuje);
		modelo.draw(cmd, imagenes ? vkglTF::RenderFlags::BindImages : 0, pipelineLayout);
	}

	// Donde escribe el juego: /data/yugioh_classic/ en la consola (o la carpeta del juego si no se pudo crear); en el PC
	// la carpeta del juego, como siempre
	std::string rutaDatos()
	{
#if !defined(PS5_HOST_REFERENCE)
		if (!carpetaDatosConsola().empty()) {
			return carpetaDatosConsola();
		}
#endif
		return getAssetPath() + "../";
	}

	// ------------------------------------------------------------------ fotos de la consola (capturas.txt)
	void prepararCapturas()
	{
		std::string contenido, raiz;
#if !defined(PS5_HOST_REFERENCE)
		raiz = rutaDatos();
		// el pedido: en la carpeta de datos o (compatibilidad) en la del juego; las fotos van siempre a la de datos
		std::string archivo = raiz + "capturas.txt";
		if (access(archivo.c_str(), F_OK) != 0) {
			archivo = getAssetPath() + "../capturas.txt";
		}
		std::ifstream pedido(archivo);
		if (!pedido) {
			return;
		}
		// pase de un solo uso: se borra al leerlo, asi una apertura manual posterior arranca normal
		contenido.assign((std::istreambuf_iterator<char>(pedido)), std::istreambuf_iterator<char>());
		pedido.close();
		remove(archivo.c_str());
#else
		// PC: YUGI_CAPTURAS=<ruta de un capturas.txt> (no se borra; las fotos van a <su carpeta>/capturas/)
		const char* v = getenv("YUGI_CAPTURAS");
		if (!v) {
			return;
		}
		std::ifstream pedido(v);
		if (!pedido) {
			return;
		}
		contenido.assign((std::istreambuf_iterator<char>(pedido)), std::istreambuf_iterator<char>());
		raiz = v;
		raiz = raiz.substr(0, raiz.find_last_of('/') + 1);
#endif
		std::istringstream palabras(contenido);
		std::string palabra;
		while (palabras >> palabra) {
			if (palabra == "video") {
				palabras >> videoFps;
			} else if (palabra == "cerrar_al_terminar") {
				cerrarAlTerminar = true;
			} else {
				momentosCaptura.push_back((float)atof(palabra.c_str()));
			}
		}
		if (momentosCaptura.empty() && videoFps <= 0.0f) {
			momentosCaptura = { 0.2f, 0.45f, 0.6f, 0.8f, 1.1f, 1.4f };
		}
		carpetaCapturas = raiz + "capturas/";
		mkdir(carpetaCapturas.c_str(), 0777);
		chmod(carpetaCapturas.c_str(), 0777);
		if (videoFps > 0.0f) {
			mkdir((carpetaCapturas + "video").c_str(), 0777);
			chmod((carpetaCapturas + "video").c_str(), 0777);
		}
	}

	// pide a la base que copie la pantalla de este cuadro (la escribe en submitFrame)
	void sacarFoto(const std::string& nombre)
	{
		ps5.screenshotPath = carpetaCapturas + nombre + ".ppm";
		ps5.capturaPedida = true;
	}

	// Salida sin manos: cerrar.txt en la carpeta de datos o en la del juego (se borra), o el fin de la grabacion pedida
	void revisarCierre()
	{
#if !defined(PS5_HOST_REFERENCE)
		if (cerrarAlTerminar && ataquesVistos >= 1 && !hayAtaque()) {
			// se espera un poco: las ultimas fotos se escriben en otro hilo
			if (tSalida < 0.0f) {
				tSalida = tiempo;
			} else if (tiempo - tSalida > 2.5f) {
				quit = true;
			}
			return;
		}
		if (tiempo >= revisarCerrar) {
			revisarCerrar = tiempo + 1.0f;
			for (const std::string& pedido : { rutaDatos() + "cerrar.txt", getAssetPath() + "../cerrar.txt" }) {
				if (access(pedido.c_str(), F_OK) == 0) {
					remove(pedido.c_str());
					quit = true;
				}
			}
		}
#endif
	}

	void revisarCapturas()
	{
		if (carpetaCapturas.empty()) {
			return;
		}
		if (!fotoReposo && tiempo > 2.5f) {
			fotoReposo = true;
			sacarFoto("00_reposo");
		} else if (ataquesVistos == 1 && hayAtaque() && siguienteCaptura < momentosCaptura.size() && tAtaque >= momentosCaptura[siguienteCaptura]) {
			char nombre[32];
			snprintf(nombre, sizeof(nombre), "%02d_ataque_%.2fs", (int)siguienteCaptura + 1, momentosCaptura[siguienteCaptura]);
			siguienteCaptura++;
			sacarFoto(nombre);
		} else if (videoFps > 0.0f && ataquesVistos == 1 && hayAtaque() && tAtaque >= cuadroVideo / videoFps) {
			// un cuadro de video (JPG): el cuadro N corresponde al segundo N / fps del ataque
			char nombre[48];
			snprintf(nombre, sizeof(nombre), "video/%04d", cuadroVideo);
			cuadroVideo = (int)(tAtaque * videoFps) + 1;
			ps5.screenshotPath = carpetaCapturas + nombre + ".jpg";
			ps5.capturaPedida = true;
		}
	}

	void prepare() override
	{
		VulkanExampleBase::prepare();
		ui.visible = false;   // Ajustes oculta al arrancar; se abre manteniendo OPTIONS 2 s (parche_base.py)
#if defined(PS5_HOST_REFERENCE)
		if (getenv("YUGI_ATAQUE_YA")) {
			proximoAtaque = 0.3f;   // videos en el PC: el ataque empieza enseguida
		}
#endif
		prepararCamara();
		prepararCapturas();
		loadAssets();
		prepararSombra();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		prepararFiguras();   // rama classic-figuras: figuras 3D de las cartas emblematicas (figuras.h)
		bosqueCampo.preparar(vulkanDevice, queue, swapChain.colorFormat, depthFormat, FORMATO_SOMBRA, pipelineCache, descriptorSetLayout, getShadersPath(),
		                     getAssetPath(), MESA_X, MESA_Z);
		entornosRival.preparar(vulkanDevice, queue, swapChain.colorFormat, depthFormat, pipelineCache, descriptorSetLayout, getShadersPath(), getAssetPath(),
		                       MESA_X, MESA_Z);
		camposMesa.preparar(vulkanDevice, queue, swapChain.colorFormat, depthFormat, pipelineCache, descriptorSetLayout, getShadersPath(), getAssetPath(),
		                    MESA_X, MESA_Z);
		prepararEntorno();
		prepararCampo();
		flipbooks.preparar(vulkanDevice, queue, swapChain.colorFormat, depthFormat, pipelineCache, descriptorSetLayout, getShadersPath(), getAssetPath());
		efectosCartas.cargar(getAssetPath() + "ui/", getAssetPath() + "fx/md/");
		if (const char* v = getenv("YUGI_FX_CARTA")) {   // id,id,... (o id+id+...)
			std::string ids(v);
			std::replace(ids.begin(), ids.end(), '+', ',');
			std::stringstream lista(ids);
			std::string id;
			while (std::getline(lista, id, ',')) {
				if (!id.empty()) pruebaCartas.push_back(id);
			}
		}
		{
			// rama classic-idioma: /data/yugioh_classic/idioma.txt; si aun no existe se lee el viejo de la carpeta del
			// juego (instalacion por carpeta) y la proxima vez se guarda en la de datos
			const std::string nuevo = rutaDatos() + "idioma.txt", viejo = getAssetPath() + "../idioma.txt";
			idioma::cargar(access(nuevo.c_str(), F_OK) != 0 && access(viejo.c_str(), F_OK) == 0 ? viejo : nuevo);
			idioma::rutaPreferencia() = nuevo;
		}
		interfazDuelo.preparar(vulkanDevice, queue, swapChain.colorFormat, depthFormat, pipelineCache, getAssetPath(), getShadersPath());
		sonido.iniciar(getAssetPath() + "audio/");   // rama classic-audio (la musica la elige la pantalla: duelo_juego.h)
		prepararMando();   // (despues de la interfaz: lee la clase de cada carta de cartas_ui.txt)
		prepared = true;
	}

	void pasadaSombra(VkCommandBuffer cmd)
	{
		const VkImageSubresourceRange rango{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		vks::tools::insertImageMemoryBarrier(cmd, sombra.imagen,
			VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
			rango);
		VkRenderingAttachmentInfo profundidad{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = sombra.vista,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = { .depthStencil = { 1.0f, 0 } },
		};
		VkRenderingInfo info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = { { 0, 0 }, { TAM_SOMBRA, TAM_SOMBRA } },
			.layerCount = 1,
			.pDepthAttachment = &profundidad,
		};
		vkCmdBeginRendering(cmd, &info);
		VkViewport viewport = vks::initializers::viewport((float)TAM_SOMBRA, (float)TAM_SOMBRA, 0.0f, 1.0f);
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor = vks::initializers::rect2D(TAM_SOMBRA, TAM_SOMBRA, 0, 0);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sombra);
		dibujarTablero(cmd, false);
		dibujarFiguras(cmd, false);   // rama classic-figuras
		if (bosqueVisible()) {
			bosqueCampo.sombra(cmd, descriptorSets[currentBuffer]);   // los arboles del bosque tambien dan sombra
		}
		vkCmdEndRendering(cmd);
		vks::tools::insertImageMemoryBarrier(cmd, sombra.imagen,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			rango);
	}

	void buildCommandBuffer()
	{
		if (galeriaVisible()) {   // rama classic-galeria: el cuadro es solo la galeria (galeria_juego.h)
			cuadroGaleria();
			return;
		}
		VkCommandBuffer cmd = drawCmdBuffers[currentBuffer];
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBufInfo));
		pasadaSombra(cmd);
		beginDynamicRendering(cmd);
		VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.escena);
		dibujarTablero(cmd, true);
		if (const campos::Campo* tp = tapeteMesa()) {   // rama classic-extras: el tapete elegido (debajo de la carta de campo)
			camposMesa.superficie(cmd, descriptorSets[currentBuffer], *tp, 1.0f, true);
		}
		if (const campos::Campo* cc = campoMesa()) {   // rama classic-campos: el material de la carta de campo sobre el tapete
			camposMesa.superficie(cmd, descriptorSets[currentBuffer], *cc, ambienteCampo());
		}
		if (bosqueVisible()) {   // carta de campo Bosque: suelo, arboles, cesped, raices y el fondo de bosque
			bosqueCampo.opaco(cmd, descriptorSets[currentBuffer], glm::inverse(uniformData.proyeccion * uniformData.vista), ambienteBosque());
		}
		// entorno del rival: suelo, siluetas y fondo (despues del Bosque: su fondo se funde encima del del bosque)
		entornosRival.opaco(cmd, descriptorSets[currentBuffer], glm::inverse(uniformData.proyeccion * uniformData.vista));
		dibujarFiguras(cmd, true);   // rama classic-figuras: las emblematicas de pie sobre su carta
		dibujarCampo(cmd);        // la carta de campo (se voltea, brilla, estalla), sus ondas y el aura de los potenciados
		dibujarEquipos(cmd);      // rama classic-ps5-equipo-ovni: el circulo de equipamiento y su rayo de union
		dibujarDadoMoneda(cmd);   // rama classic-fx-md: dado / moneda 3D de Master Duel
		efectosFiguras(cmd);      // rama classic-figuras: magia, aliento y rayos de las figuras
		efectosPack();            // aparicion, golpe y explosion (Niagara Examples Pack)
		flipbooks.dibujar(cmd, descriptorSets[currentBuffer], currentBuffer, glm::vec3(uniformData.camara));
		if (bosqueVisible()) {
			bosqueCampo.particulas(cmd, descriptorSets[currentBuffer], ambienteBosque());   // hojas, luciernagas, polen, niebla, rayos
		}
		entornosRival.transparente(cmd, descriptorSets[currentBuffer]);   // llamas, halos y particulas del entorno
		if (const campos::Campo* cc = campoMesa()) {
			camposMesa.particulas(cmd, descriptorSets[currentBuffer], *cc, ambienteCampo());   // gotas, brasas, chispas... entre las casillas
		} else if (const campos::Campo* tp = tapeteMesa()) {
			camposMesa.particulas(cmd, descriptorSets[currentBuffer], *tp, 0.6f);   // rama classic-extras: las del tapete, mas suaves
		}
		interfazDuelo.dibujar(cmd, currentBuffer, uniformData.proyeccion * uniformData.vista, (float)width, (float)height);
		drawUI(cmd);
		endDynamicRendering(cmd);
		VK_CHECK_RESULT(vkEndCommandBuffer(cmd));
	}

	// ================================================================ carta de campo: Bosque (rama campo-bosque)
	// Pedido del usuario (2026-10-16): al ACTIVARSE la carta de campo (Bosque) TODO el entorno se vuelve un bosque, y
	// al DESTRUIRSE vuelve exactamente el tablero de siempre.
	//  - La carta (en la zona "jugador_campo") espera boca abajo; al activarse se levanta, se voltea y brilla, baja una
	//    columna de luz verde y dorada y sale de ella una onda que barre el tablero: detras del frente el tapete se
	//    cubre de musgo, cesped, hojarasca y raices (escena.frag; los marcos de las casillas siguen a la vista y
	//    brillan), brotan los arboles, pinos, arbustos, helechos, hongos y rocas alrededor de la mesa (bosque.h), el
	//    cesped, las raices que trepan por el canto, el suelo del bosque y el fondo de bosque profundo; cambian la luz
	//    (sol dorado moteado por las copas, relleno verde, sombras de los arboles), el tono y la niebla verdosa; caen
	//    hojas, vagan luciernagas y polen, bancos de niebla y rayos de sol entre las copas (~2.5 s el tablero, ~5 s
	//    hasta el ultimo arbol del fondo).
	//  - Los monstruos de los tipos que potencia Bosque (Insecto, Bestia, Planta, Guerrero-Bestia) llevan un aura verde
	//    (aro, sello y chispas que suben) y sus rotulos muestran ATK/DEF +200 en verde. (En el duelo de prueba de hoy
	//    ninguno es de esos tipos; en el PC YUGI_CAMPO_AURA=carta,carta... la fuerza para verla.)
	//  - Al destruirse: la carta se levanta temblando, se agrieta (los pedazos se separan y sale luz por las grietas) y
	//    estalla en pedazos que vuelan y caen; con el estallido sale la onda de retiro: los arboles se hunden y se
	//    disuelven con brasas verdes, el cesped se recoge, las raices vuelven atras, la niebla y la luz se apagan, y
	//    queda el tablero exacto de antes (con ambiente 0 y sin frentes no se dibuja nada del bosque).
	// Produccion (PS5 y PC): unos segundos despues de terminar la invocacion del arranque (y entre ataques) la carta
	// se activa; despues de 3 ataques se destruye; a los 2.5 s se coloca otra boca abajo y a los 6 s se activa: en
	// bucle. Mientras se activa o se destruye no empieza ningun ataque automatico. Ajustes (OPTIONS): "Carta de campo:
	// activar/destruir" y "Bosque automatico" (el bucle). PC: YUGI_CAMPO_BOSQUE=1 la activa al arrancar (=0 sin bucle),
	// YUGI_CAMPO_CICLO=<s> activa y destruye en bucle cada <s> s de bosque, YUGI_CAMPO_MEDIR=1 imprime el coste.
	enum EstadoCampo { CAMPO_BOCA_ABAJO = 0, CAMPO_ACTIVANDO, CAMPO_ACTIVO, CAMPO_DESTRUYENDO, CAMPO_VACIO, CAMPO_COLOCANDO };
	static constexpr float MESA_X = 13.44f, MESA_Z = 10.08f;   // mitad del ancho y del fondo del tapete (19.2 x 14.4 x 1.4)
	static constexpr float CAMPO_VOLTEA = 0.75f;      // activacion: la carta se levanta y queda boca arriba
	static constexpr float CAMPO_ONDA = 0.95f;        // sale la onda de crecimiento
	static constexpr float CAMPO_VELOCIDAD = 22.0f;   // m/s de los frentes
	static constexpr float CAMPO_ACTIVADA = 5.3f;     // todo crecido (el frente paso los ~95 m del bosque)
	static constexpr float CAMPO_GRIETA = 0.15f, CAMPO_ESTALLA = 0.85f;   // destruccion: se agrieta y estalla
	static constexpr float CAMPO_RETIRO = 0.9f;       // sale la onda de retiro
	static constexpr float CAMPO_DESTRUIDA = 5.4f;
	static constexpr float CAMPO_COLOCA = 0.9f;       // una carta nueva cae boca abajo en la zona
	static constexpr int CAMPO_NX = 4, CAMPO_NY = 5;  // pedazos de la carta al estallar
	int estadoCampo{ CAMPO_BOCA_ABAJO };
	float tCampo{ 0.0f };            // s en el estado actual
	bool campoAuto{ true };          // el bucle de produccion
	float campoCiclo{ 0.0f };        // PC: YUGI_CAMPO_CICLO
	float campoForzar{ -1.0f };      // PC: YUGI_CAMPO_BOSQUE=1 (se activa en ese segundo)
	float proximoCampo{ -1.0f };     // cuando se activa sola (produccion)
	int ataquesAlActivar{ 0 };
	bool campoMedir{ false };
	float medirSuma{ 0.0f };
	int medirCuadros{ 0 };
	std::vector<std::string> auraForzada;   // PC: YUGI_CAMPO_AURA
	bosque::Bosque bosqueCampo;
	// rama classic-campos: CUAL carta de campo lleva la maquina de estados (la misma del Bosque: carta, onda, retiro) y de
	// que lado esta. La ultima activada manda y cambia TODO el escenario (docs/campos.md). PC: YUGI_CAMPO=<carta>[,rival]
	campos::Campos camposMesa;
	std::string idCampo{ "forest" };
	int ladoCampo{ 0 };
	bool esBosque() const { return idCampo == "forest"; }
	// la capa del tablero de la carta (nullptr: Bosque, que tiene su mundo propio, o nada visible)
	const campos::Campo* campoMesa() const
	{
		if (esBosque() || !(bosqueVisibleOnda())) return nullptr;
		const campos::Campo* c = campos::buscar(idCampo);
		return c && c->superficie != campos::S_NADA ? c : nullptr;
	}
	glm::vec3 colorCampo() const
	{
		const campos::Campo* c = campos::buscar(idCampo);
		return c ? c->color : glm::vec3(0.35f, 1.25f, 0.30f);
	}

	glm::vec3 centroCampo() const
	{
		for (const interfaz::Zona& z : interfaz::ZONAS) {
			if (strcmp(z.id, ladoCampo ? "rival_campo" : "jugador_campo") == 0) {
				return glm::vec3(z.x * interfaz::ESCALA, 0.0f, z.z * interfaz::ESCALA);
			}
		}
		return glm::vec3(-11.48f, 0.0f, 3.08f);
	}

	// la onda de la carta de campo ya salio (cualquier carta)
	bool bosqueVisibleOnda() const
	{
		return (estadoCampo == CAMPO_ACTIVANDO && tCampo >= CAMPO_ONDA) || estadoCampo == CAMPO_ACTIVO || estadoCampo == CAMPO_DESTRUYENDO;
	}
	bool bosqueVisible() const { return esBosque() && bosqueVisibleOnda(); }
	bool campoEnTransicion() const { return estadoCampo == CAMPO_ACTIVANDO || estadoCampo == CAMPO_DESTRUYENDO; }

	// cuanto bosque hay en la luz, el fondo y la niebla (0..1); con otra carta de campo, 0
	float ambienteBosque() const { return esBosque() ? ambienteCampo() : 0.0f; }
	// cuanto de la carta de campo activa (cualquiera) hay en el escenario (0..1)
	float ambienteCampo() const
	{
		switch (estadoCampo) {
		case CAMPO_ACTIVANDO: return suave(CAMPO_ONDA + 0.1f, CAMPO_ONDA + 2.4f, tCampo);
		case CAMPO_ACTIVO: return 1.0f;
		case CAMPO_DESTRUYENDO: return 1.0f - suave(CAMPO_RETIRO + 0.3f, CAMPO_RETIRO + 3.2f, tCampo);
		default: return 0.0f;
		}
	}
	// radio (m) que ya alcanzo el frente de crecimiento desde la carta (< 0: nada)
	float frenteCrece() const
	{
		if (estadoCampo == CAMPO_ACTIVANDO) {
			return tCampo < CAMPO_ONDA ? -1.0f : (tCampo - CAMPO_ONDA) * CAMPO_VELOCIDAD;
		}
		return estadoCampo == CAMPO_ACTIVO || estadoCampo == CAMPO_DESTRUYENDO ? 1e4f : -1.0f;
	}
	float frenteRetiro() const
	{
		return estadoCampo == CAMPO_DESTRUYENDO && tCampo >= CAMPO_RETIRO ? (tCampo - CAMPO_RETIRO) * CAMPO_VELOCIDAD : -1.0f;
	}
	// brillo del frente de la onda mientras barre (0..1)
	float ondaBosque() const
	{
		if (estadoCampo == CAMPO_ACTIVANDO) {
			return suave(CAMPO_ONDA, CAMPO_ONDA + 0.2f, tCampo) * (1.0f - suave(CAMPO_ONDA + 2.2f, CAMPO_ONDA + 3.4f, tCampo));
		}
		if (estadoCampo == CAMPO_DESTRUYENDO) {
			return suave(CAMPO_RETIRO, CAMPO_RETIRO + 0.2f, tCampo) * (1.0f - suave(CAMPO_RETIRO + 2.2f, CAMPO_RETIRO + 3.4f, tCampo));
		}
		return 0.0f;
	}

	void uniformesCampo()
	{
		const glm::vec3 c = centroCampo();
		uniformData.bosque = glm::vec4(ambienteBosque(), frenteCrece(), frenteRetiro(), ondaBosque());
		uniformData.bosque2 = glm::vec4(c.x, c.z, animar ? 1.0f : 0.0f, 0.0f);
		// (el fondo de siempre de la interfaz se apaga con el bosque o con el entorno del rival)
		interfazDuelo.campoBosque(std::max(ambienteBosque(), entornosRival.pesoVisible), ambienteBosque());
	}

	void cambiarCampo(int estado)
	{
		estadoCampo = estado;
		tCampo = 0.0f;
		// rama classic-campos: el fondo de la carta reemplaza al del rival mientras esta activa (el Bosque trae su mundo)
		if (estado == CAMPO_ACTIVANDO && !esBosque() && campos::esCampo(idCampo)) {
			entornosRival.pedirCampo("campo_" + idCampo);
		} else if (estado == CAMPO_DESTRUYENDO || estado == CAMPO_VACIO || (estado == CAMPO_ACTIVANDO && esBosque())) {
			entornosRival.soltarCampo();
		}
		if (estado == CAMPO_ACTIVANDO) {
			ataquesAlActivar = ataquesVistos;
		} else if (estado == CAMPO_BOCA_ABAJO) {
			proximoCampo = tiempo + 6.0f;
		}
#if defined(PS5_HOST_REFERENCE)
		if (campoMedir) {
			static const char* const NOMBRE[] = { "boca abajo", "activando", "activo", "destruyendo", "vacio", "colocando" };
			fprintf(stderr, "[campo] %s (t %.2f)\n", NOMBRE[estado], tiempo);
		}
#endif
	}

	// el boton de los ajustes: activa la carta boca abajo o destruye la activa
	void botonCampo()
	{
		if (estadoCampo == CAMPO_BOCA_ABAJO) {
			cambiarCampo(CAMPO_ACTIVANDO);
		} else if (estadoCampo == CAMPO_ACTIVO) {
			cambiarCampo(CAMPO_DESTRUYENDO);
		} else if (estadoCampo == CAMPO_VACIO) {
			cambiarCampo(CAMPO_COLOCANDO);   // sin carta: primero se coloca otra (y el siguiente toque la activa)
		}
	}

	void prepararCampo()
	{
#if defined(PS5_HOST_REFERENCE)
		if (const char* v = getenv("YUGI_CAMPO")) {   // rama classic-campos: la carta de campo de la demo (",rival": del rival)
			std::string c(v);
			const size_t coma = c.find(',');
			if (coma != std::string::npos) {
				ladoCampo = c.substr(coma + 1) == "rival" ? 1 : 0;
				c = c.substr(0, coma);
			}
			if (campos::esCampo(c)) idCampo = c;
		}
		if (getenv("YUGI_CAMPO_MEDIR")) camposMesa.informe();
		if (const char* v = getenv("YUGI_CAMPO_BOSQUE")) {
			if (atof(v) != 0.0) {
				campoForzar = atof(v) > 1.0 ? (float)atof(v) : 0.4f;   // (rama classic-campos: =<s> la activa a los <s> s, p. ej. ya en el duelo)
			} else {
				campoAuto = false;
			}
		}
		if (const char* v = getenv("YUGI_CAMPO_CICLO")) {
			campoCiclo = (float)atof(v);
			if (campoForzar < 0.0f) {
				campoForzar = 0.4f;
			}
		}
		if (const char* v = getenv("YUGI_CAMPO_AURA")) {
			std::stringstream lista(v);
			std::string carta;
			while (std::getline(lista, carta, ',')) {
				auraForzada.push_back(carta);
			}
		}
		campoMedir = getenv("YUGI_CAMPO_MEDIR") != nullptr;
		if (campoMedir) {
			bosqueCampo.informe();
		}
#endif
	}

	void avanzarCampo()
	{
		tCampo += frameTimer;
		if (estadoCampo == CAMPO_ACTIVANDO && tCampo >= CAMPO_ACTIVADA) {
			cambiarCampo(CAMPO_ACTIVO);
		} else if (estadoCampo == CAMPO_DESTRUYENDO && tCampo >= CAMPO_DESTRUIDA) {
			cambiarCampo(CAMPO_VACIO);
		} else if (estadoCampo == CAMPO_COLOCANDO && tCampo >= CAMPO_COLOCA) {
			cambiarCampo(CAMPO_BOCA_ABAJO);
		}
		if (campoForzar >= 0.0f && tiempo >= campoForzar) {
			campoForzar = -1.0f;
			if (estadoCampo == CAMPO_BOCA_ABAJO || estadoCampo == CAMPO_VACIO) {
				activarCampo(idCampo, ladoCampo);   // (rama classic-campos: tambien con la zona vacia, ya en el duelo)
			}
		}
		if (campoCiclo > 0.0f) {
			// PC: activa y destruye en bucle, sin esperar ataques
			if (estadoCampo == CAMPO_ACTIVO && tCampo >= campoCiclo) {
				cambiarCampo(CAMPO_DESTRUYENDO);
			} else if (estadoCampo == CAMPO_VACIO && tCampo >= 1.0f) {
				cambiarCampo(CAMPO_COLOCANDO);
			} else if (estadoCampo == CAMPO_BOCA_ABAJO && tCampo >= 1.5f && campoForzar < 0.0f) {
				cambiarCampo(CAMPO_ACTIVANDO);
			}
		} else if (campoAuto && !hayAtaque()) {
			// produccion: se activa unos segundos despues del arranque y se destruye tras unos ataques, en bucle
			if (estadoCampo == CAMPO_BOCA_ABAJO) {
				if (proximoCampo < 0.0f) {
					proximoCampo = tiempo + 4.0f;
				}
				if (tiempo >= proximoCampo) {
					cambiarCampo(CAMPO_ACTIVANDO);
				}
			} else if (estadoCampo == CAMPO_ACTIVO && ataquesVistos - ataquesAlActivar >= 3 && tCampo > 20.0f) {
				cambiarCampo(CAMPO_DESTRUYENDO);
			} else if (estadoCampo == CAMPO_VACIO && tCampo >= 2.5f) {
				cambiarCampo(CAMPO_COLOCANDO);
			}
		}
#if defined(PS5_HOST_REFERENCE)
		if (campoMedir) {
			// coste aproximado en la vista previa: tiempo medio de cuadro (CPU + GPU de este PC) con y sin bosque
			medirSuma += frameTimer;
			if (++medirCuadros == 60) {
				fprintf(stderr, "[campo] %s: %.2f ms por cuadro (60 cuadros), dibujos del bosque %u + %u de sombra\n",
				        bosqueVisible() ? "con bosque" : "sin bosque", 1000.0f * medirSuma / 60.0f,
				        bosqueVisible() ? bosqueCampo.dibujosCuadro : 0u, bosqueVisible() ? bosqueCampo.dibujosSombra : 0u);
				medirSuma = 0.0f;
				medirCuadros = 0;
			}
		}
#endif
	}

	// el tapete y las cartas de Blender, sin la carta de campo que trae (Sogen): esa zona la maneja dibujarCampo
	void dibujarNodoTablero(VkCommandBuffer cmd, vkglTF::Node* nodo, bool imagenes)
	{
		if (nodo->name == "Carta_jugador_campo" || (nodo->mesh && nodo->mesh->name == "Carta_jugador_campo")) {
			return;
		}
		// rama classic-ps5-arreglos1 (PS5: "la carta no se ve, solo un recuadro negro con dos pestannas naranjas"): con el
		// duelo real las cartas horneadas del duelo de prueba (CAMPO_PRUEBA: Kuriboh, Celtic Guardian... en defensa) no se
		// dibujan. Antes las tapaba una casilla de vidrio vertical que no alcanzaba a cubrir las que estan de lado (sus
		// bordes asomaban como pestannas) y que, opaca, se veia como un recuadro negro en lugar de la casilla del tapete
		if (ctl.duelo && nodo->name.rfind("Carta_", 0) == 0) {
			return;
		}
		if (nodo->mesh) {
			for (vkglTF::Primitive* p : nodo->mesh->primitives) {
				if (imagenes) {
					vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &p->material.descriptorSet, 0, nullptr);
				}
				vkCmdDrawIndexed(cmd, p->indexCount, 1, p->firstIndex, 0, 0);
			}
		}
		for (vkglTF::Node* hijo : nodo->children) {
			dibujarNodoTablero(cmd, hijo, imagenes);
		}
	}

	void dibujarTablero(VkCommandBuffer cmd, bool imagenes)
	{
		// efecto.x < 0: el tapete se cubre de bosque detras del frente (escena.frag); sin bosque, como siempre
		const Empuje empuje{ glm::mat4(1.0f), glm::vec4(bosqueVisible() ? -1.0f : 0.0f, 0.35f, 0.0f, 0.0f), glm::vec4(0.0f) };
		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Empuje), &empuje);
		const VkDeviceSize desde = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &tablero.vertices.buffer, &desde);
		vkCmdBindIndexBuffer(cmd, tablero.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		for (vkglTF::Node* n : tablero.nodes) {
			dibujarNodoTablero(cmd, n, imagenes);
		}
	}

	// un disco de luz (esfera aplastada) para la luz que sale por las grietas
	void discoLuz(VkCommandBuffer cmd, glm::vec3 c, glm::vec3 radios, glm::vec3 color, float brillo)
	{
		dibujar(cmd, esfera, glm::scale(glm::translate(glm::mat4(1.0f), c), radios), glm::vec4(0.0f, 0.0f, 0.0f, brillo), glm::vec4(color, 1.0f), true);
	}

	// esquinas de la rejilla de los pedazos (uv de la carta), con las interiores corridas al azar
	glm::vec2 esquinaPedazo(int i, int j) const
	{
		glm::vec2 uv((float)i / CAMPO_NX, (float)j / CAMPO_NY);
		if (i > 0 && i < CAMPO_NX && j > 0 && j < CAMPO_NY) {
			uv += (glm::vec2(azar((uint32_t)(i * 31 + j * 7)), azar((uint32_t)(i * 13 + j * 17 + 5))) - 0.5f) * glm::vec2(0.55f / CAMPO_NX, 0.55f / CAMPO_NY);
		}
		return uv;
	}

	// La carta de campo segun el estado: boca abajo, cayendo (colocada), volteandose y brillando, activa (late), o
	// agrietandose y estallando en pedazos (fragmentoEnMundo de la interfaz)
	void cartaCampo()
	{
		const glm::vec3 c = centroCampo();
		const float giroLado = ladoCampo ? -1.0f : 1.0f;   // (la del rival mira hacia el)
		const glm::vec3 w0(giroLado, 0.0f, 0.0f), h0(0.0f, 0.0f, giroLado), arriba(0.0f, 1.0f, 0.0f);
		const float W = interfaz::CARTA_ANCHO, H = interfaz::CARTA_ALTO;
		const float t = tCampo;
		float alto = 0.05f, giro = glm::pi<float>(), brillo = 1.0f;
		switch (estadoCampo) {
		case CAMPO_VACIO:
			return;
		case CAMPO_COLOCANDO: {
			const float u = glm::clamp(t / CAMPO_COLOCA, 0.0f, 1.0f);
			alto += 1.6f * (1.0f - u) * (1.0f - u);
			brillo = 0.4f + 0.6f * u + 0.8f * expf(-fabsf(t - CAMPO_COLOCA * 0.95f) * 12.0f);
			break;
		}
		case CAMPO_ACTIVANDO: {
			const float v = suave(0.0f, CAMPO_VOLTEA, t);
			alto += 0.9f * sinf(glm::pi<float>() * v);
			giro = glm::pi<float>() * (1.0f - v);
			brillo = 1.0f + 1.8f * expf(-t * 5.0f) + 0.9f * sinf(glm::pi<float>() * v) + 1.4f * expf(-fabsf(t - CAMPO_ONDA) * 4.0f);
			break;
		}
		case CAMPO_ACTIVO:
			giro = 0.0f;
			brillo = 1.08f + 0.12f * sinf(tiempo * 2.1f);
			break;
		case CAMPO_DESTRUYENDO:
			giro = 0.0f;
			break;
		default:
			break;
		}
		if (estadoCampo != CAMPO_DESTRUYENDO) {
			const glm::vec3 wf = w0 * cosf(giro) + arriba * sinf(giro);
			const glm::vec3 nf = arriba * cosf(giro) - w0 * sinf(giro);
			const glm::vec3 centro = c + glm::vec3(0.0f, alto, 0.0f);
			const bool cara = glm::dot(nf, glm::vec3(uniformData.camara) - centro) > 0.0f;
			interfazDuelo.cartaEnMundo(idCampo, centro, (cara ? wf : -wf) * (0.5f * W), h0 * (0.5f * H), cara, false, brillo);
			return;
		}
		// destruccion: se levanta temblando, los pedazos se separan (luz por las grietas) y despues vuelan
		const float sube = 0.05f + 0.22f * suave(0.0f, 0.5f, t);
		const float temblor = 0.02f * suave(CAMPO_GRIETA, CAMPO_ESTALLA, t);
		const glm::vec3 centro = c + glm::vec3(sinf(t * 83.0f) * temblor, sube, cosf(t * 71.0f) * temblor);
		const float separa = 0.035f * suave(CAMPO_GRIETA, CAMPO_ESTALLA, t);
		const float vuela = t - CAMPO_ESTALLA;
		auto mundo = [&](glm::vec2 uv) { return w0 * ((uv.x - 0.5f) * W) + h0 * ((uv.y - 0.5f) * H); };
		for (int j = 0; j < CAMPO_NY; j++) {
			for (int i = 0; i < CAMPO_NX; i++) {
				const glm::vec2 uv[4] = { esquinaPedazo(i, j), esquinaPedazo(i + 1, j), esquinaPedazo(i + 1, j + 1), esquinaPedazo(i, j + 1) };
				const glm::vec2 cuv = 0.25f * (uv[0] + uv[1] + uv[2] + uv[3]);
				const glm::vec3 local = mundo(cuv);
				const uint32_t k = (uint32_t)(j * CAMPO_NX + i);
				glm::vec3 pos = centro + local + glm::normalize(local + glm::vec3(1e-4f)) * separa;
				glm::mat4 giroPedazo(1.0f);
				float esc = 1.0f, br = 1.0f + 1.5f * suave(CAMPO_GRIETA, CAMPO_ESTALLA, t) * (0.6f + 0.4f * sinf(t * 40.0f + (float)k));
				if (vuela > 0.0f) {
					if (vuela > 1.6f) {
						continue;
					}
					const glm::vec3 fuera = glm::normalize(glm::vec3(local.x, 0.0f, local.z) + glm::vec3(1e-4f));
					const glm::vec3 vel = fuera * (1.6f + 2.2f * azar(k * 3u + 1u)) + glm::vec3(0.0f, 2.6f + 2.2f * azar(k * 5u + 2u), 0.0f);
					pos += vel * vuela + glm::vec3(0.0f, -4.9f * vuela * vuela, 0.0f);
					pos.y = glm::max(pos.y, 0.03f);
					const glm::vec3 eje = glm::normalize(glm::vec3(azar(k * 7u) - 0.5f, azar(k * 11u) - 0.5f, azar(k * 13u) - 0.5f) + glm::vec3(1e-3f));
					giroPedazo = glm::rotate(glm::mat4(1.0f), vuela * (6.0f + 8.0f * azar(k * 17u)), eje);
					esc = 1.0f - suave(0.7f, 1.6f, vuela);
					br = 1.0f + 2.0f * expf(-vuela * 5.0f);
				}
				glm::vec3 esquinas[4];
				for (int e = 0; e < 4; e++) {
					esquinas[e] = pos + glm::vec3(giroPedazo * glm::vec4((mundo(uv[e]) - local) * esc, 0.0f));
				}
				interfazDuelo.fragmentoEnMundo(idCampo, esquinas, uv, br);
			}
		}
	}

	// los monstruos que potencia el Bosque (o los de YUGI_CAMPO_AURA)
	bool potenciado(int z) const
	{
		if (!esMonstruo(z) || ctl.zonas[z].vacia() || ctl.zonas[z].bocaAbajo) {
			return false;
		}
		const std::string& carta = ctl.zonas[z].carta;
		for (const std::string& c : auraForzada) {
			if (c == carta) {
				return true;
			}
		}
		return interfazDuelo.potenciaBosque(carta);
	}

	void dibujarCampo(VkCommandBuffer cmd)
	{
		cartaCampo();
		const glm::vec3 c = centroCampo();
		const glm::vec3 mesa = c + glm::vec3(0.0f, 0.06f, 0.0f);
		// (el color de la carta: verde para el Bosque, azul para Umi, naranja para la lava...)
		const glm::vec3 verde = colorCampo(), dorado = esBosque() ? glm::vec3(1.3f, 1.0f, 0.35f) : glm::mix(colorCampo(), glm::vec3(1.3f, 1.2f, 1.0f), 0.45f),
		                blanco(0.95f, 1.0f, 0.85f);
		const float t = tCampo;
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		// chispas y luz (esferas y tramos aditivos)
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.efectos);
		if (estadoCampo == CAMPO_ACTIVANDO && t > CAMPO_VOLTEA - 0.05f && t < CAMPO_VOLTEA + 1.6f) {
			// estallido de hojas de luz que salen de la carta al quedar boca arriba
			const float u = t - (CAMPO_VOLTEA - 0.05f);
			for (uint32_t k = 0; k < 28; k++) {
				const float a = 6.2832f * azar(k * 3u + 7u);
				const glm::vec3 vel(cosf(a) * (2.0f + 3.0f * azar(k * 5u)), 2.5f + 3.5f * azar(k * 7u + 1u), sinf(a) * (2.0f + 3.0f * azar(k * 5u)));
				const glm::vec3 p = mesa + vel * u + glm::vec3(0.0f, -3.0f * u * u, 0.0f);
				const float vida = 1.0f - u / 1.6f;
				bola(cmd, p, 0.08f, k % 3 ? verde : dorado, 0.35f * vida);
				bola(cmd, p, 0.03f, blanco, 1.2f * vida);
				tramo(cmd, p, p - vel * 0.05f, 0.02f, verde, 0.5f * vida);
			}
		}
		if (estadoCampo == CAMPO_DESTRUYENDO) {
			const float grieta = suave(CAMPO_GRIETA, CAMPO_ESTALLA, t) * (t < CAMPO_ESTALLA ? 1.0f : 0.0f);
			if (grieta > 0.0f) {
				// luz que sale por las grietas, debajo de los pedazos
				const glm::vec3 bajo = c + glm::vec3(0.0f, 0.05f + 0.22f * suave(0.0f, 0.5f, t) - 0.03f, 0.0f);
				discoLuz(cmd, bajo, glm::vec3(0.75f, 0.03f, 1.05f), dorado, (0.5f + 0.3f * sinf(t * 50.0f)) * grieta);
				discoLuz(cmd, bajo, glm::vec3(0.55f, 0.02f, 0.8f), blanco, 1.4f * grieta);
				bola(cmd, bajo, 1.3f * grieta, verde, 0.12f * grieta);
			}
			const float u = t - CAMPO_ESTALLA;
			if (u > 0.0f && u < 1.4f) {
				// el estallido: destello y chispas que caen
				const float f = expf(-u * 6.0f);
				bola(cmd, mesa + glm::vec3(0.0f, 0.3f, 0.0f), 0.4f + 1.6f * u, dorado, 0.5f * f);
				bola(cmd, mesa + glm::vec3(0.0f, 0.3f, 0.0f), 0.3f + 0.6f * u, blanco, 1.6f * f);
				for (uint32_t k = 0; k < 36; k++) {
					const float a = 6.2832f * azar(k * 7u + 3u);
					const glm::vec3 vel(cosf(a) * (2.5f + 4.0f * azar(k * 11u)), 1.5f + 4.0f * azar(k * 13u + 1u), sinf(a) * (2.5f + 4.0f * azar(k * 11u)));
					glm::vec3 p = mesa + glm::vec3(0.0f, 0.3f, 0.0f) + vel * u + glm::vec3(0.0f, -4.9f * u * u, 0.0f);
					p.y = glm::max(p.y, 0.03f);
					const float vida = 1.0f - u / 1.4f;
					bola(cmd, p, 0.06f, k % 2 ? dorado : verde, 0.4f * vida);
					tramo(cmd, p, p - vel * 0.04f, 0.015f, blanco, 0.9f * vida);
				}
			}
		}
		// aura verde de los potenciados: chispas que suben
		const float amb = ambienteBosque();
		if (amb > 0.0f) {
			for (int i = 0; i < (int)interfaz::NUM_ZONAS; i++) {
				if (!potenciado(i)) {
					continue;
				}
				const float alto = 1.6f;   // (lo que sube el aura sobre la carta)
				for (uint32_t k = 0; k < 10; k++) {
					const float u = fmodf(tiempo * (0.35f + 0.2f * azar(k * 3u + (uint32_t)i)) + azar(k * 7u + (uint32_t)i * 5u), 1.0f);
					const float a = 6.2832f * azar(k * 11u + (uint32_t)i) + tiempo * 0.6f;
					const float r = 0.6f + 0.4f * azar(k * 13u + (uint32_t)i);
					const glm::vec3 p = centroZona(i) + glm::vec3(cosf(a) * r, u * alto * 1.1f, sinf(a) * r);
					const float vida = amb * sinf(glm::pi<float>() * u);
					bola(cmd, p, 0.07f, verde, 0.35f * vida);
					bola(cmd, p, 0.025f, blanco, 1.0f * vida);
				}
			}
		}
		// aros, columna de luz y sellos (efecto_haz)
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.haz);
		if (estadoCampo == CAMPO_ACTIVANDO) {
			if (t < 0.6f) {
				const float u = t / 0.6f;
				haz(cmd, glm::vec4(mesa, 0.9f + 1.1f * u), glm::vec4(0.0f), glm::vec4(1.0f, 0.3f, 1.6f * (1.0f - u) * (1.0f - u), 0.0f), glm::vec4(verde, 0.04f));
			}
			// columna de luz verde y dorada que baja sobre la carta
			const float luz = suave(0.5f, 0.75f, t) * (1.0f - suave(1.8f, 2.6f, t));
			if (luz > 0.0f) {
				const glm::vec3 cielo = c + glm::vec3(0.0f, 10.0f, 0.0f);
				const float frente = suave(0.5f, 0.85f, t);
				haz(cmd, glm::vec4(cielo, 0.15f), glm::vec4(mesa, 1.15f), glm::vec4(0.0f, 0.61f, 0.65f * luz, frente), glm::vec4(verde, 0.0f));
				haz(cmd, glm::vec4(cielo, 0.08f), glm::vec4(mesa, 0.5f), glm::vec4(0.0f, 0.23f, 0.5f * luz, frente), glm::vec4(dorado, 0.0f));
			}
			const float sello = suave(0.7f, 1.0f, t) * (1.0f - suave(3.0f, 4.2f, t));
			if (sello > 0.0f) {
				haz(cmd, glm::vec4(mesa, 1.7f), glm::vec4(0.0f), glm::vec4(2.0f, 0.4f, 1.2f * sello, 0.0f), glm::vec4(glm::mix(verde, dorado, 0.3f), 0.0f));
			}
			// la onda que barre el tablero (y sigue por el bosque): tres aros detras del frente
			for (int k = 0; k < 3; k++) {
				const float R = frenteCrece() - 2.2f * (float)k;
				if (R > 0.2f && R < 48.0f) {
					const float f = powf(1.0f - R / 48.0f, 1.4f);
					haz(cmd, glm::vec4(c.x, 0.04f, c.z, R), glm::vec4(0.0f), glm::vec4(1.0f, 0.37f * (float)k, (k == 0 ? 2.2f : 1.0f) * f, 0.0f),
					    glm::vec4(k == 1 ? dorado : verde, 0.012f + 0.01f * (float)k));
				}
			}
		} else if (estadoCampo == CAMPO_ACTIVO) {
			// activa: un sello tenue que late bajo la carta y, cada tanto, un aro suave
			haz(cmd, glm::vec4(mesa, 1.5f), glm::vec4(0.0f), glm::vec4(2.0f, 0.4f, 0.22f + 0.08f * sinf(tiempo * 2.1f), 0.0f), glm::vec4(verde, 0.0f));
			const float u = fmodf(tiempo, 6.0f) / 1.8f;
			if (u < 1.0f) {
				haz(cmd, glm::vec4(mesa, 0.9f + 3.0f * u), glm::vec4(0.0f), glm::vec4(1.0f, 0.5f, 0.7f * (1.0f - u) * (1.0f - u), 0.0f), glm::vec4(verde, 0.03f));
			}
		} else if (estadoCampo == CAMPO_DESTRUYENDO) {
			const float u = (t - CAMPO_ESTALLA) / 0.8f;
			if (u > 0.0f && u < 1.0f) {
				haz(cmd, glm::vec4(mesa, 0.6f + 6.0f * u), glm::vec4(0.0f), glm::vec4(1.0f, 0.9f, 2.0f * (1.0f - u) * (1.0f - u), 0.0f), glm::vec4(dorado, 0.05f));
			}
			// la onda de retiro
			const float R = frenteRetiro();
			if (R > 0.2f && R < 48.0f) {
				haz(cmd, glm::vec4(c.x, 0.04f, c.z, R), glm::vec4(0.0f), glm::vec4(1.0f, 0.2f, 1.6f * powf(1.0f - R / 48.0f, 1.4f), 0.0f), glm::vec4(dorado, 0.015f));
			}
		} else if (estadoCampo == CAMPO_COLOCANDO && t > CAMPO_COLOCA * 0.85f) {
			const float u = (t - CAMPO_COLOCA * 0.85f) / (CAMPO_COLOCA * 0.15f + 0.5f);
			haz(cmd, glm::vec4(mesa, 0.9f + 1.2f * u), glm::vec4(0.0f), glm::vec4(1.0f, 0.6f, 0.9f * glm::max(0.0f, 1.0f - u), 0.0f), glm::vec4(verde, 0.04f));
		}
		// aura: aro y sello verdes bajo los potenciados
		if (amb > 0.0f) {
			for (int i = 0; i < (int)interfaz::NUM_ZONAS; i++) {
				if (!potenciado(i)) {
					continue;
				}
				const float R = 1.2f;
				const glm::vec3 b = centroZona(i) + glm::vec3(0.0f, 0.06f, 0.0f);
				haz(cmd, glm::vec4(b, R), glm::vec4(0.0f), glm::vec4(1.0f, 0.13f * (float)i, (0.8f + 0.3f * sinf(tiempo * 3.0f + (float)i)) * amb, 0.0f), glm::vec4(verde, 0.05f));
				haz(cmd, glm::vec4(b, R * 1.25f), glm::vec4(0.0f), glm::vec4(2.0f, 0.3f, 0.45f * amb, 0.0f), glm::vec4(verde, 0.0f));
			}
		}
	}

	// ---------------------------------------------------------------------------------------- selector de camara
	// Pedido del usuario: "en la esquina inferior izquierda un selector de camara para que el usuario decida que
	// camara quiere dejar". Modos (interfaz::ModoCamara; el selector lo dibuja interfaz.h):
	//  - CINE: la de siempre: vista general y, en cada ataque, el plano automatico que no tapan los gigantes
	//  - TABLERO: la vista general frontal fija (los ataques se ven desde ahi, sin cortes)
	//  - CENITAL: desde arriba del tablero
	//  - LATERAL: de costado, baja
	//  - LIBRE: orbita con el stick derecho; L2 aleja y R2 acerca
	// Mando: R1 pasa al modo siguiente y L1 al anterior (no chocan: la X lanza ataques, la cruceta y el stick
	// izquierdo mueven el puntero, el panel tactil y OPTIONS abren los ajustes; con los ajustes abiertos L1/R1 son
	// de esa ventana y no cambian la camara). En un PC con ventana, la tecla C pasa al siguiente. Cada cambio hace
	// una transicion suave de 0.5 s desde donde estaba la camara (aunque se cambie a mitad de otra transicion) y la
	// eleccion queda hasta que el usuario la cambie.
	// PC (vista previa sin ventana ni mando): YUGI_CAMARA=cine|tablero|cenital|lateral|libre elige el modo de entrada;
	// YUGI_CAMARA_CICLO=<s> pasa al siguiente modo cada <s> segundos (para ver las transiciones)
	int modoCamara{ interfaz::CAM_CINE };
	float tCambioCamara{ 99.0f };        // s desde el ultimo cambio
	int dirCambioCamara{ 0 };            // +1 R1/C, -1 L1, 0 variable o ajustes
	Vista vistaDesde{ OJO, OBJETIVO };   // donde estaba la camara al cambiar (la transicion parte de ahi)
	Vista vistaMostrada{ OJO, OBJETIVO };
	float frontalDesde{ 1.0f }, frontalMostrada{ 1.0f };   // peso de la camara frontal (ver pesoCamaraFrontal)
	float pesoCine{ 0.0f };              // 0..1: cuanto del plano de cine del ataque hay en la camara CINE
	float libreGiro{ 0.0f }, libreElev{ 0.42f }, libreDist{ 22.0f };
	float cicloCamara{ 0.0f };           // PC: YUGI_CAMARA_CICLO
	bool camaraIntacta{ true };          // la camara mostrada es la del selector (sin inspeccion ni camaras de prueba)

	// Visibilidad de los rotulos ATK/DEF flotantes (para interfaz.h, rama interfaz-dorso): true solo cuando la
	// camara es la vista general frontal: siempre en TABLERO, y en CINE mientras no hay ataque (ni la camara de
	// invocacion de prueba del PC); false en CENITAL, LATERAL y LIBRE, durante la transicion de un cambio de modo y
	// si otra camara (inspeccion, YUGI_CAM_*) reemplaza a la del selector. updateUniformBuffers la pasa en cada
	// cuadro a interfazDuelo.rotulosVisibles() (que hace el fundido); pesoCamaraFrontal() es la version continua
	bool camaraFrontalActiva() const
	{
		if (!camaraIntacta || tCambioCamara < DURACION_CAMBIO_CAMARA) {
			return false;
		}
		if (modoCamara == interfaz::CAM_TABLERO) {
			return true;
		}
		return modoCamara == interfaz::CAM_CINE && !hayAtaque();
	}
	// lo mismo, continuo (0..1): sigue la entrada y salida del plano de cine y las transiciones entre modos
	float pesoCamaraFrontal() const { return frontalMostrada; }

	void cambiarCamara(int modo, int direccion)
	{
		modo = (modo % interfaz::NUM_CAMARAS + interfaz::NUM_CAMARAS) % interfaz::NUM_CAMARAS;
		if (modo == modoCamara) {
			return;
		}
		vistaDesde = vistaMostrada;
		frontalDesde = frontalMostrada;
		if (modo == interfaz::CAM_LIBRE) {
			libreDesde(vistaMostrada, libreGiro, libreElev, libreDist);
		}
		modoCamara = modo;
		tCambioCamara = 0.0f;
		dirCambioCamara = direccion;
		if (direccion != 0 && cicloCamara <= 0.0f) recordarCamara(modo);   // rama classic-extras: L1/R1 (o C) se recuerda (opciones.txt)
#if defined(PS5_HOST_REFERENCE)
		if (getenv("YUGI_DEPURA_CAMARA")) {
			fprintf(stderr, "[camara] modo %s (t %.2f)\n", interfaz::CLAVE_CAMARA[modo], tiempo);
		}
#endif
	}

	void keyPressed(uint32_t tecla) override
	{
		if (tecla == 'C' || tecla == 'c') {
			cambiarCamara(modoCamara + 1, 1);
		}
		teclaMando(tecla);   // rama mando: el mando con el teclado (PC con ventana)
	}

	void prepararCamara()
	{
#if defined(PS5_HOST_REFERENCE)
		if (const char* v = getenv("YUGI_CAMARA")) {
			for (int i = 0; i < interfaz::NUM_CAMARAS; i++) {
				if (strcmp(v, interfaz::CLAVE_CAMARA[i]) == 0) {
					modoCamara = i;   // de entrada: sin transicion
				}
			}
			if (modoCamara == interfaz::CAM_LIBRE) {
				libreDesde(Vista{ OJO, OBJETIVO }, libreGiro, libreElev, libreDist);
				// PC: YUGI_CAM_LIBRE="giro,elev,dist" (rad, rad, m) para capturas fijas (p. ej. del bosque)
				if (const char* l = getenv("YUGI_CAM_LIBRE")) {
					sscanf(l, "%f,%f,%f", &libreGiro, &libreElev, &libreDist);
				}
			}
		}
		if (const char* c = getenv("YUGI_CAMARA_CICLO")) {
			cicloCamara = (float)atof(c);
		}
#endif
	}

	// cada cuadro, despues de leer el mando (avanzarAtaque)
	void avanzarCamara()
	{
		const float dt = glm::clamp(frameTimer, 0.0f, 0.1f);
		tCambioCamara += dt;
		if (!ajustesAbiertos()) {
			if (mando.pressed & PAD_R1) {
				cambiarCamara(modoCamara + 1, 1);
			} else if (mando.pressed & PAD_L1) {
				cambiarCamara(modoCamara - 1, -1);
			}
			moverCamaraMando(dt);   // rama mando: stick derecho / L2 / R2 / R3 en las camaras fijas
			if (modoCamara == interfaz::CAM_LIBRE) {
				// stick derecho: a los lados gira alrededor de la mesa, arriba sube; L2 aleja, R2 acerca
				libreGiro = remainderf(libreGiro + mando.right_x * 1.7f * dt, 2.0f * glm::pi<float>());
				libreElev = glm::clamp(libreElev - mando.right_y * 0.9f * dt, LIBRE_ELEV_MIN, LIBRE_ELEV_MAX);
				libreDist = glm::clamp(libreDist * expf((mando.l2 - mando.r2) * 1.1f * dt), LIBRE_DIST_MIN, LIBRE_DIST_MAX);
			}
		}
#if defined(PS5_HOST_REFERENCE)
		if (cicloCamara > 0.0f && tCambioCamara >= cicloCamara) {
			cambiarCamara(modoCamara + 1, 1);
		}
#endif
		interfazDuelo.selectorCamara(modoCamara, tCambioCamara, dirCambioCamara);
	}

	// la camara del modo elegido (ojo/mira llegan con la de CINE ya armada; tablero: la vista general)
	void elegirCamara(glm::vec3& ojo, glm::vec3& mira, const Vista& tablero)
	{
		Vista meta{ ojo, mira };
		float frontal = 1.0f - pesoCine;
		switch (modoCamara) {
		case interfaz::CAM_TABLERO: meta = tablero; frontal = 1.0f; break;
		case interfaz::CAM_CENITAL: meta = vistaCenital(); frontal = 0.0f; break;
		case interfaz::CAM_LATERAL: meta = vistaLateral(); frontal = 0.0f; break;
		case interfaz::CAM_LIBRE: meta = vistaLibre(libreGiro, libreElev, libreDist); frontal = 0.0f; break;
		default: break;
		}
		if (modoCamara != interfaz::CAM_LIBRE) {
			meta = ajusteCamaraMando(meta);   // rama mando: el stick derecho y L2/R2 en las camaras fijas
		}
		if (modoCamara != interfaz::CAM_CINE && hayAtaque()) {
			// la sacudida del impacto tambien en las camaras fijas (CINE ya la tiene)
			meta.ojo += glm::vec3(sinf(tAtaque * 71.0f), cosf(tAtaque * 53.0f), 0.0f) * 0.15f * destello();
		}
		const float e = suave(0.0f, DURACION_CAMBIO_CAMARA, tCambioCamara);
		vistaMostrada = e < 1.0f ? mezclarVistas(vistaDesde, meta, e) : meta;
		frontalMostrada = glm::mix(frontalDesde, frontal, e);
		ojo = vistaMostrada.ojo;
		mira = vistaMostrada.mira;
	}

	// ======================================================================================= el mando
	// La logica (mesa, foco, menus, flujo) esta en control.h y las cartas de la mesa las dibuja la interfaz; aqui se
	// lee el mando y se ejecutan sus eventos (invocar, atacar, campo). La demo automatica (ataques solos, Bosque en
	// bucle) sigue hasta que el usuario toca el mando (Ajustes: "Demo automatica" la vuelve a prender). Al tomar el
	// control hay un reparto: Ojos Rojos, Ojos Azules y Monstruo renacido vuelven a la mano.
	// PC: YUGI_GUION="X,abajo,X,1.5,der*2,..." aprieta botones solo (control::Guion) y escribe el flujo en stderr
	// ([mando] ...; YUGI_MANDO_LOG=1 lo escribe sin guion). YUGI_MESA=clasica: sin reparto (la mano de prueba).
	control::Mando ctl;
	control::Repetidor repetidor;
	control::Guion guion;
#include "mazo_duelo.h"   // rama classic-constructor: coleccion, mazo armado, reparto y la pantalla "Construir mazo"
// ================================================================ entorno del rival (rama classic-entornos)
	// El entorno (entornos.h) es el del personaje rival: su campo "entorno" (rama classic-personajes) o, en los rivales
	// genericos, el de su tema. Se elige al elegir el rival (la pantalla de eleccion ya lo muestra detras). La carta
	// de campo manda (entornos.h: pesoVisible). PC: YUGI_ENTORNO=<clave o id de personaje> lo fija, YUGI_ENTORNO_CICLO=<s> los recorre,
	// YUGI_ENTORNO_MEDIR=1 imprime el coste. Ajustes: "Entorno: siguiente".
	entornos::Entornos entornosRival;
	bool entornoFijo{ false };
	float entornoCiclo{ 0.0f }, tEntornoCiclo{ 0.0f };
	std::string entornoVista;   // el ultimo "entorno" visto en ctl.vista (classic-personajes)

	void prepararEntorno()
	{
#if defined(PS5_HOST_REFERENCE)
		if (const char* e = getenv("YUGI_ENTORNO")) {
			entornosRival.pedir(std::string(e));   // una clave de tema o el id de un personaje (su fondo propio)
			entornosRival.fijarYa();               // sin fundido al arrancar (capturas)
			entornoFijo = true;
		}
		if (const char* c = getenv("YUGI_ENTORNO_CICLO")) {
			entornoCiclo = (float)atof(c);
			entornoFijo = true;
		}
		if (getenv("YUGI_ENTORNO_MEDIR")) {
			entornosRival.informe();
		}
#endif
	}

	void avanzarEntorno()
	{
		if (entornoCiclo > 0.0f) {
			tEntornoCiclo += frameTimer;
			if (tEntornoCiclo >= entornoCiclo) {
				tEntornoCiclo = 0.0f;
				siguienteEntorno();
			}
		}
		// classic-personajes: el personaje elegido trae su "entorno" en la vista del duelo (manda sobre el tema)
		const std::string ve = campoEntorno(ctl.vista, 0);
		if (ve != entornoVista) {
			entornoVista = ve;
			if (!entornoFijo && !ve.empty()) {
				entornosRival.pedir(ve);
				fprintf(stderr, "[entornos] personaje: %s\n", entornosRival.nombreObjetivo().c_str());
			}
		}
		entornosRival.avanzar(frameTimer, ambienteBosque());
	}

	void siguienteEntorno()
	{
		entornosRival.pedir(entornosRival.objetivo % (entornos::NUM_ENTORNOS - 1) + 1);
		fprintf(stderr, "[entornos] %s\n", entornosRival.nombreObjetivo().c_str());
	}

	// el campo "entorno" del rival si lo tiene (classic-personajes); si no, vacio
	template <class R> static auto campoEntorno(const R& r, int) -> decltype(std::string(r.entorno)) { return std::string(r.entorno); }
	template <class R> static std::string campoEntorno(const R&, long) { return std::string(); }

	void entornoDelRival(const ia::Rival* r)
	{
		if (entornoFijo) return;
		if (!r) {
			entornosRival.pedir(entornos::NEUTRO);
			return;
		}
		// el "entorno" de un personaje es su id (su fondo propio, rama classic-fondos) o una clave de tema
		const std::string clave = campoEntorno(*r, 0);
		if (clave.empty()) {
			entornosRival.pedir(entornos::porTema(r->tema));
		} else {
			entornosRival.pedir(clave);
		}
		entornoVista.clear();   // si el rival es un personaje, su "entorno" se vuelve a pedir en avanzarEntorno
		fprintf(stderr, "[entornos] rival %s: %s\n", r->clave.c_str(), entornosRival.nombreObjetivo().c_str());
	}

#include "figuras.h"      // rama classic-figuras: figuras 3D de las cartas emblematicas
#include "ataque_elevacion_juego.h"   // rama classic-ps5-ataque5: el atacante se eleva al declarar el ataque
#include "ataque_md_juego.h"   // rama classic-ps5-ataque: el ataque directo de Master Duel (al que recibe, no a su zona)
#include "ataque_fig_fx.h"     // rama classic-ps5-ataque: rayo, aliento y magia de las figuras como en Yugi Reload 3D
#include "duelo_juego.h"  // rama classic-integracion: el duelo real contra la IA (ia.h), elegir rival y progreso
#include "md_ritmo_juego.h"   // rama classic-md-ritmo: el ritmo de Master Duel (viajes de cartas, activaciones, contadores)
#include "efectos3_juego.h"   // rama classic-ps5-efectos3: destruccion por efecto y carta de campo como MD
#include "equipo_ovni_juego.h"   // rama classic-ps5-equipo-ovni: circulo de equipamiento y la abduccion del Bosque en las figuras
#include "velocidad_juego.h"   // rama classic-resultado: la velocidad de animaciones aplicada al duelo
#include "resultado_juego.h"   // rama classic-resultado: pantalla de resultado, revancha, musica y Fase de Espera
#include "duelo_consulta.h"   // rama classic-duelo-ui: registro, visores de zonas, cadena visible y jugables
#include "entrada_clave_juego.h"   // rama classic-cartas-clave: entrada especial de las cartas clave (camara, figura, efectos, sonidos)
#include "menu_principal.h"   // rama classic-menus: menu principal (Duelo, Historia, Construir mazo, Opciones, Salir)
#include "en_linea_juego.h"   // rama classic-en-linea: DUELO EN LINEA (buscar, red local, codigo, duelo en red, reloj)
#include "cosmeticos_juego.h"  // rama classic-extras: Personalizacion (funda, tapete, controles, estadisticas) y camara recordada
#include "portada.h"          // rama classic-portada: la portada (titulo) antes del menu principal
#include "galeria_juego.h"    // rama classic-galeria: galeria de figuras 3D (desde la Lista de cartas)
#if defined(PS5_HOST_REFERENCE)
#include "front_total_pc.h"   // rama classic-front-total: prueba de pixeles de la mesa (YUGI_PRUEBA_FT, solo PC)
#include "md_ritmo_pc.h"   // rama classic-md-ritmo: capturas en secuencia del ritmo de Master Duel (YUGI_PRUEBA_RITMO, solo PC)
#endif
	bool mandoListo{ false };
	uint32_t teclas{ 0 };   // PC con ventana: lo apretado en el teclado desde el ultimo cuadro (como botones)
	float camGiro{ 0.0f }, camElev{ 0.0f }, camZoom{ 1.0f };    // ajuste del stick derecho y L2/R2 en las camaras fijas
	bool guionTerminado{ false };

	// la ventana de ajustes tiene el mando
	bool ajustesAbiertos() const { return settings.overlay && ui.visible; }

	void prepararMando()
	{
		static_assert(control::NUM_ZONAS_MESA == (int)interfaz::NUM_ZONAS, "control.h e interfaz.h deben tener las mismas zonas");
		ctl.clase = [this](const std::string& c) { return interfazDuelo.claseCarta(c); };
		ctl.ataqueEnCurso = [this] { return hayAtaque(); };
		interfazDuelo.contadorZona = [this](int z, int& n, bool& turnos) { return contadorZonaMD(z, n, turnos); };   // rama classic-md-ritmo
		ctl.campoOcupado = [this] { return campoEnTransicion() || estadoCampo == CAMPO_COLOCANDO; };
		prepararMazos();   // rama classic-constructor: el mazo armado (mazo_duelo.h)
		// rama classic-integracion: sin la mesa de prueba; el duelo real empieza en "elegir rival" (duelo_juego.h)
		ctl.preparar({}, {}, {});
#if defined(PS5_HOST_REFERENCE)
		if (const char* g = getenv("YUGI_GUION")) {
			guion.leer(g);
			ctl.log = true;
		}
		if (getenv("YUGI_MANDO_LOG")) ctl.log = true;
#endif
		interfazDuelo.conectarMando(&ctl);
		interfazDuelo.conectarConsulta(&consulta);   // rama classic-duelo-ui
		prepararDuelo();
		prepararMenu();   // rama classic-menus: el juego arranca en el menu principal
		mandoListo = true;
	}

	static uint32_t botonesDe(uint32_t pad)
	{
		uint32_t r = 0;
		if (pad & PAD_CROSS) r |= control::B_X;
		if (pad & PAD_CIRCLE) r |= control::B_CIRCULO;
		if (pad & PAD_TRIANGLE) r |= control::B_TRIANGULO;
		if (pad & PAD_SQUARE) r |= control::B_CUADRADO;
		if (pad & PAD_UP) r |= control::B_ARRIBA;
		if (pad & PAD_DOWN) r |= control::B_ABAJO;
		if (pad & PAD_LEFT) r |= control::B_IZQ;
		if (pad & PAD_RIGHT) r |= control::B_DER;
		if (pad & PAD_L1) r |= control::B_L1;
		if (pad & PAD_R1) r |= control::B_R1;
		if (pad & PAD_R3) r |= control::B_R3;
		if (pad & PAD_L3) r |= control::B_L3;   // rama classic-duelo-ui: registro del duelo
		return r;
	}

	// PC con ventana: Enter/Espacio = X, Retroceso = circulo, T = triangulo, F = cuadrado, flechas = cruceta,
	// Q/E = L1/R1, R = R3 (C sigue pasando de camara), O = OPTIONS corto (pausa)
	void teclaMando(uint32_t tecla)
	{
		switch (tecla) {
		case 0x0D: case ' ': teclas |= control::B_X; break;
		case 0x08: teclas |= control::B_CIRCULO; break;
		case 'T': case 't': teclas |= control::B_TRIANGULO; break;
		case 'F': case 'f': teclas |= control::B_CUADRADO; break;
		case 0x25: teclas |= control::B_IZQ; break;
		case 0x26: teclas |= control::B_ARRIBA; break;
		case 0x27: teclas |= control::B_DER; break;
		case 0x28: teclas |= control::B_ABAJO; break;
		case 'Q': case 'q': teclas |= control::B_L1; break;
		case 'E': case 'e': teclas |= control::B_R1; break;
		case 'R': case 'r': teclas |= control::B_R3; break;
		case 'L': case 'l': teclas |= control::B_L3; break;   // rama classic-duelo-ui: L3 (registro)
		case 'M': case 'm': pedidoConstructor = true; break;   // rama classic-constructor: abrir "Construir mazo"
		case 'K': case 'k': pedidoLista = true; break;   // rama classic-constructor2: la lista de cartas (solo ver; la L es L3)
		case 'O': case 'o': teclas |= control::B_OPTIONS; break;   // rama classic-arreglos: OPTIONS corto (en el duelo: el selector de fases)
		case 'G': case 'g': teclas |= control::B_TACTIL; break;   // rama classic-md-fases: el panel tactil (engranaje del duelo)
		default: break;
		}
	}

	// cada cuadro, despues de leer el mando (avanzarAtaque) y antes de la camara
	void avanzarMando()
	{
		if (!mandoListo) {
			return;
		}
		if (constructorMazo.abierto) {
			return;   // rama classic-constructor2: el constructor se queda con el mando (y con el guion del PC)
		}
		const float dt = glm::clamp(frameTimer, 0.0f, 0.1f);
		// el guion (PC) y el teclado entran como botones del mando; L1/R1/R3 tambien para la camara
		uint32_t extra = teclas | pulsosRaton;   // (rama classic-pc: y el raton)
		teclas = 0;
		pulsosRaton = 0;
		if (guion.activo()) {
			extra |= guion.avanzar(dt);
			if (guion.terminado() && !guionTerminado) {
				guionTerminado = true;
				fprintf(stderr, "[mando] guion terminado (%.2f s): estado %s, foco %s, mano %d, cementerio %d\n", guion.reloj, control::NOMBRE_ESTADO[ctl.estado],
				        ctl.idFoco().c_str(), (int)ctl.manoJ.size(), (int)ctl.cementerioJ.size());
			}
		}
		if (extra & control::B_L1) mando.pressed |= PAD_L1;
		if (extra & control::B_R1) mando.pressed |= PAD_R1;
		if (extra & control::B_R3) mando.pressed |= PAD_R3;
		atenderPedidoMano();   // los botones de Ajustes (se dibujan dentro del cuadro: se hacen aca)
		const uint32_t recien = botonesDe(mando.pressed) | extra;
		const uint32_t pulsos = repetidor.paso(botonesDe(mando.held), recien, mando.left_x, mando.left_y, dt) | (recien & ~control::B_DIRECCIONES);
		registrarBotones(recien);   // rama classic-arreglos: [boton] ... en el klog
		if (extra & control::B_OPTIONS) pausa.abajo = std::max(pausa.abajo, 0.01f);   // (PC: tecla O o el guion = un OPTIONS corto)
		vigilarAjustes();   // rama classic-menus: los Ajustes de desarrollo solo con la combinacion secreta
		// rama classic-arreglos: OPTIONS corto abre/cierra la pausa (y cierra los Ajustes); la pausa abierta se queda
		// con el mando (duelo_juego.h)
		// rama classic-bp-terminar: un circulo que se queda otra pantalla (pausa, ajustes, visores) tambien cuenta para la
		// pausa del circulo de "Terminar turno" (control.h): el que cierra un panel no termina el turno con el siguiente
		const bool circulo = (pulsos & control::B_CIRCULO) != 0;
		if (avanzarPausa(pulsos)) {
			if (circulo) ctl.circuloAjeno();
			procesarEventos();
			return;
		}
		if (ajustesAbiertos()) {
			if (circulo) ctl.circuloAjeno();
			return;   // los ajustes tienen el mando
		}
		// tocar el mando apaga la demo (OPTIONS y el panel tactil no: abren la pausa y el constructor)
		const bool toco = (mando.pressed & ~(uint32_t)(PAD_OPTIONS | PAD_TOUCH_PAD)) != 0 || extra != 0 || fabsf(mando.left_x) > 0.5f || fabsf(mando.left_y) > 0.5f ||
		                  fabsf(mando.right_x) > 0.5f || fabsf(mando.right_y) > 0.5f || mando.l2 > 0.3f || mando.r2 > 0.3f;
		if (toco) {
			ctl.tocar();
		}
		if (avanzarPantallaDuelo(pulsos)) {   // elegir rival / resultado: la pantalla se queda con el mando
			if (circulo) ctl.circuloAjeno();
			procesarEventos();
			return;
		}
		if (mandoConsulta(pulsos, dt)) {   // rama classic-duelo-ui: el panel de registro y visores (L3, X en la zona)
			if (circulo) ctl.circuloAjeno();
			procesarEventos();
			return;
		}
		ctl.ejeDetalle = fabsf(mando.right_y) > 0.2f ? mando.right_y : 0.0f;   // rama classic-pulido: desplaza el detalle
		ctl.avanzar(dt * escalaMesa(), pulsos);   // (rama classic-resultado: velocidad de animaciones)
		if (ctl.estado == control::DETALLE) mando.right_x = mando.right_y = 0.0f;   // (con el detalle el stick no mueve la camara)
		procesarEventos();
		sonidoDuelo.cuadro(sonido, ctl, pulsos, tAtaque, T_IMPACTO, interfazDuelo.lp(false), interfazDuelo.lp(true));   // rama classic-audio
	}

	void procesarEventos()
	{
		for (size_t k = 0; k < ctl.eventos.size(); k++) {
			ejecutarEvento(ctl.eventos[k]);
			sonidoDuelo.evento(sonido, ctl.eventos[k]);   // rama classic-audio
		}
		ctl.eventos.clear();
		sincronizarCampo();
	}

	// Ajustes: una mano de prueba nueva con las cartas de monstruo de cartas_ui.txt (cada mano sigue donde termino la
	// anterior), robar una, o despejar las zonas de monstruo del jugador
	static constexpr int MANO_PRUEBA = 6;
	size_t siguienteCarta{ 0 };
	enum PedidoMano { PM_NADA = 0, PM_MANO, PM_ROBAR, PM_DESPEJAR };
	PedidoMano pedidoMano{ PM_NADA };

	std::vector<std::string> sacarCartas(int n)
	{
		const std::vector<std::string>& todas = interfazDuelo.monstruos();
		std::vector<std::string> r;
		for (int k = 0; k < n && k < (int)todas.size(); k++) {
			r.push_back(todas[siguienteCarta % todas.size()]);
			siguienteCarta = (siguienteCarta + 1) % todas.size();
		}
		return r;
	}

	void atenderPedidoMano()
	{
		if (pedidoMano == PM_NADA || !ctl.libreParaMano()) {
			return;   // (con un menu abierto o una carta volando espera)
		}
		const PedidoMano p = pedidoMano;
		pedidoMano = PM_NADA;
		if (p == PM_MANO) {
			ctl.ponerMano(sacarCartas(MANO_PRUEBA));
		} else if (p == PM_ROBAR) {
			const std::vector<std::string> una = sacarCartas(1);
			if (!una.empty()) ctl.robar(una[0], interfaz::MAX_MANO_J);
		} else if (p == PM_DESPEJAR) {
			ctl.despejarMonstruosJ();
		}
		procesarEventos();
	}

	void ejecutarEvento(const control::Evento& e)
	{
		switch (e.tipo) {
		case control::EV_INVOCAR:
		case control::EV_VOLTEAR_MONSTRUO:
			if (e.zona >= 0 && e.zona < (int)interfaz::NUM_ZONAS) tInvocada[e.zona] = 0.0f;   // destello de aparicion
			break;
		case control::EV_ATACAR: atacar(e.zona, e.zona2, false); break;
		case control::EV_ACTIVAR_MAGIA:   // rama classic-fx-md: la animacion de la carta (o la generica de magia/trampa)
			if (e.zona >= 0 && e.zona < (int)interfaz::NUM_ZONAS) activarEfectoCarta(e.carta, e.zona);
			break;
		case control::EV_ACTIVAR_CAMPO: activarCampo(e.carta); break;
		case control::EV_DESTRUIR_CAMPO: destruirCampo(e.carta); break;
		case control::EV_FASE:
			break;
		case control::EV_DEMO_APAGADA:
			ataquesSolos = false;
			campoAuto = false;   // el bosque en bucle tambien es de la demo
			break;
		default: break;   // posicion, cementerio, fases y magias: los dibuja la interfaz
		}
	}

	// La magia de campo del jugador (rama campo-bosque, ya en main): el menu de su zona y "Activar" desde la mano
	// llaman a la maquina de estados del Bosque. activarCampo(): la carta boca abajo se activa y crece el bosque (si la
	// zona estaba vacia, la carta que llego volando de la mano queda boca abajo y se activa enseguida);
	// destruirCampo(): estalla y el bosque se retira. sincronizarCampo() deja la celda de control.h igual al Bosque
	void activarCampo(const std::string& carta, int lado = 0)
	{
		// rama classic-campos: cualquier carta de campo, de cualquier lado; si ya habia otra activa, la nueva la reemplaza
		if (campos::esCampo(carta) && (carta != idCampo || lado != ladoCampo)) {
			if (estadoCampo == CAMPO_ACTIVO || estadoCampo == CAMPO_ACTIVANDO || estadoCampo == CAMPO_DESTRUYENDO) estadoCampo = CAMPO_BOCA_ABAJO;
			idCampo = carta;
			ladoCampo = lado;
		}
		if (estadoCampo == CAMPO_VACIO || estadoCampo == CAMPO_COLOCANDO) {
			cambiarCampo(CAMPO_BOCA_ABAJO);
		}
		if (estadoCampo == CAMPO_BOCA_ABAJO) {
			cambiarCampo(CAMPO_ACTIVANDO);
		}
		if (ctl.log) fprintf(stderr, "[mando] activarCampo(%s): estado del campo %d\n", carta.c_str(), estadoCampo);
		sonidoCampoMD(lado);   // rama classic-ps5-efectos3: su sonido y su musica
		musicaDePantalla();
	}
	void destruirCampo(const std::string& carta, int lado = 0)
	{
		if (campos::esCampo(carta) && (carta != idCampo || lado != ladoCampo)) return;   // (no es la que manda)
		if (estadoCampo == CAMPO_ACTIVO || estadoCampo == CAMPO_BOCA_ABAJO || estadoCampo == CAMPO_ACTIVANDO) {
			cambiarCampo(CAMPO_DESTRUYENDO);
		}
		if (ctl.log) fprintf(stderr, "[mando] destruirCampo(%s): estado del campo %d\n", carta.c_str(), estadoCampo);
		musicaDePantalla();   // rama classic-ps5-efectos3: vuelve la musica del duelo
	}
	void sincronizarCampo()
	{
		if (ctl.vista.pantalla == control::VistaDuelo::DUELO && ctl.duelo) return;   // (rama classic-efectos: la zona de campo la lleva el duelo)
		control::Celda& c = ctl.zonas[control::zonaPorId("jugador_campo")];
		const bool hay = estadoCampo != CAMPO_VACIO && estadoCampo != CAMPO_DESTRUYENDO;
		const bool abajo = estadoCampo == CAMPO_BOCA_ABAJO || estadoCampo == CAMPO_COLOCANDO;
		if (!hay) {
			c = control::Celda{};
		} else {
			if (c.vacia()) c.carta = idCampo;
			c.bocaAbajo = abajo;
		}
	}

	// camaras fijas (CINE, TABLERO, CENITAL, LATERAL): el stick derecho corre/orbita la camara alrededor de lo que
	// mira y vuelve suave al soltarlo; L2 aleja y R2 acerca (queda); R3 recentra (en LIBRE, vuelve a la orbita de
	// entrada)
	void moverCamaraMando(float dt)
	{
		if (modoCamara != interfaz::CAM_LIBRE) {
			const float k = 1.0f - expf(-dt * 6.0f);
			camGiro += (mando.right_x * 0.75f - camGiro) * k;
			camElev += (-mando.right_y * 0.40f - camElev) * k;
			camZoom = glm::clamp(camZoom * expf((mando.l2 - mando.r2) * 0.9f * dt), 0.55f, 1.6f);
			if (fabsf(camGiro) < 1e-4f && fabsf(mando.right_x) < 1e-4f) camGiro = 0.0f;
			if (fabsf(camElev) < 1e-4f && fabsf(mando.right_y) < 1e-4f) camElev = 0.0f;
		}
		if (mando.pressed & PAD_R3) {
			camGiro = camElev = 0.0f;
			camZoom = 1.0f;
			if (modoCamara == interfaz::CAM_LIBRE) {
				libreDesde(Vista{ OJO, OBJETIVO }, libreGiro, libreElev, libreDist);
			}
		}
	}

	Vista ajusteCamaraMando(Vista v) const
	{
		if (camGiro == 0.0f && camElev == 0.0f && camZoom == 1.0f) {
			return v;
		}
		const glm::vec3 d = v.ojo - v.mira;
		const float r = glm::length(d);
		if (r < 1e-3f) {
			return v;
		}
		const float giro = atan2f(d.x, d.z) + camGiro;
		const float elev = glm::clamp(asinf(glm::clamp(d.y / r, -1.0f, 1.0f)) + camElev, 0.06f, 1.50f);
		v.ojo = v.mira + r * camZoom * glm::vec3(sinf(giro) * cosf(elev), sinf(elev), cosf(giro) * cosf(elev));
		return v;
	}

	void render() override
	{
		if (!prepared) {
			return;
		}
		VulkanExampleBase::prepareFrame();
		tiempo += frameTimer;
		avanzarAtaque();
		avanzarGaleria();       // rama classic-galeria: abierta, se queda con el mando (antes que el constructor)
		avanzarConstructor();   // rama classic-constructor: si "Construir mazo" esta abierto, se queda con el mando
		avanzarMando();       // foco, menus, invocar/colocar/atacar desde la mano y el campo (control.h)
		if (mandoListo) avanzarRed(glm::clamp(frameTimer, 0.0f, 0.1f));   // rama classic-en-linea: la red no se detiene (ni con la pausa)
		if (mandoListo && !ajustesAbiertos() && !constructorMazo.abierto && !pausa.abierta && !congelarIAFig) {   // (la pausa detiene el duelo)
			avanzarDueloIA(glm::clamp(frameTimer, 0.0f, 0.1f));   // rama classic-integracion: el duelo real y el turno del rival
			avanzarConsulta(glm::clamp(frameTimer, 0.0f, 0.1f));   // rama classic-duelo-ui: registro, pila de la cadena y jugables
		}
		avanzarFiguras(dtAnim(glm::clamp(frameTimer, 0.0f, 0.1f)));   // rama classic-figuras (classic-resultado: con la velocidad)
		avanzarCamara();      // selector de camara (L1/R1)
		avanzarCampo();       // carta de campo Bosque
		avanzarEfectosCartas(dtAnim(glm::clamp(frameTimer, 0.0f, 0.1f)));   // rama classic-fx-md (classic-resultado: con la velocidad)
		avanzarEntorno();     // entorno del rival (rama classic-entornos)
		interfazDuelo.actualizar(mando, frameTimer, !ajustesAbiertos());
		revisarCapturas();
		revisarCierre();
#if defined(PS5_HOST_REFERENCE)
		pruebaFrontTotalPC();   // rama classic-front-total (YUGI_PRUEBA_FT)
		pruebaRitmoPC();        // rama classic-md-ritmo (YUGI_PRUEBA_RITMO)
#endif
		updateUniformBuffers();
		buildCommandBuffer();
		VulkanExampleBase::submitFrame();
	}

	void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if (overlay->header((std::string(idioma::tr("Ajustes")) + "###ajustes").c_str())) {
			// rama classic-idioma: Idioma / Language (se guarda en idioma.txt y se recuerda al reabrir)
			int32_t lengua = idioma::actual();
			if (overlay->comboBox(idioma::tr("Idioma"), &lengua, { "Español", "English" })) {
				idioma::poner(lengua);
			}
			overlay->checkBox(idioma::tr("Camara que respira"), &respirar);
			// (Figuras 3D y Dificultad adaptable: en Opciones del menu, rama classic-portada)
			int32_t camara = modoCamara;
			if (overlay->comboBox(idioma::tr("Camara"), &camara, { idioma::tr("Cine"), idioma::tr("Tablero"), idioma::tr("Cenital"), idioma::tr("Lateral"), idioma::tr("Libre") })) {
				cambiarCamara(camara, 0);
			}
			// rama classic-integracion: dejar el duelo en curso (cuenta como derrota) y volver a elegir rival
			if (ctl.vista.pantalla == control::VistaDuelo::DUELO && overlay->button(idioma::tr("Abandonar duelo"))) {
				ctl.vista.derrotas++;
				guardarProgreso();
				registrarVariedad(1);
				irAElegirRival();
				ui.visible = false;
			}
			// rama classic-entornos: probar los entornos sin cambiar de rival
			if (overlay->button((std::string(idioma::tr("Entorno: siguiente")) + " (" + entornos::CLAVES[entornosRival.objetivo] + ")###entorno").c_str())) {
				entornoFijo = true;
				siguienteEntorno();
			}
			// rama classic-constructor: la pantalla del mazo (tambien con el panel tactil)
			if (overlay->button(constructor::T("Construir mazo").c_str())) {
				pedidoConstructor = true;
			}
			// rama classic-audio
			float vm = sonido.volMusica.load(), ve = sonido.volEfectos.load();
			if (overlay->sliderFloat(idioma::tr("Volumen musica"), &vm, 0.0f, 1.0f)) sonido.volMusica = vm;
			if (overlay->sliderFloat(idioma::tr("Volumen efectos"), &ve, 0.0f, 1.0f)) sonido.volEfectos = ve;
			if (overlay->button(idioma::tr("Salir"))) {
				quit = true;
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()
