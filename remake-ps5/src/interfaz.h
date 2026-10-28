/*
 * Yugi Reload - la interfaz del duelo (todo lo que rodea al tablero), dibujada 100% por codigo en la GPU.
 *
 * ESTILO "VIDRIO DE NEON 2026" (propio, sin arte de Gemini ni del juego original):
 *  - Paneles de vidrio oscuro translucido (azul noche) con sombra suave, bisel de luz arriba y un borde fino del
 *    color del duenno; dos destellos recorren el contorno sin parar (cian = jugador, carmesi = rival).
 *  - Tipografia moderna libre (OFL): Chakra Petch para numeros (LP, ATK/DEF, fases, contadores) y Rajdhani para
 *    textos; texto SDF (stb_truetype) nitido a cualquier tamano, con halo y contorno.
 *  - Acentos: dorado para lo que se elige y la fase activa; cian/carmesi para cada lado; color por atributo.
 *  - Micro-animaciones con resortes: el recuadro dorado vuela de un objetivo a otro, la carta elegida se levanta,
 *    el visor gira la carta al cambiar y se inclina en 3D; los LP ruedan, dejan una estela de danno que se
 *    vacia, sacuden el panel y sueltan chispas.
 *  - FX: chispas doradas que salen del recuadro, polvo de luz en el visor, foil holografico y destello en la cara
 *    de la carta. Las cartas boca abajo llevan el dorso ORIGINAL del juego (assets/ui/dorso.png: card_ura.bmp
 *    de Data.dat mejorado x4 con Real-ESRGAN; no va al repo) con un borde fino de luz del color de cada lado; si
 *    falta la imagen se usa el dorso de antes dibujado por shader.
 *  - El campo 3D se desplaza a la derecha (lente desplazada) para dejar la columna izquierda libre, como el juego
 *    de PC: LP rival arriba, visor + texto de la carta, LP jugador abajo, columna de fases, manos arriba y abajo.
 * Referencias de tecnica: SDF de rectangulos redondeados (Inigo Quilez), texto SDF (stb_truetype), foil tipo
 * Balatro/Master Duel reinterpretado; todo el codigo es propio.
 */
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#include "stb_image.h"   // la implementacion ya esta en la base (VulkanglTFModel.cpp)
#include "ataque_md.h"   // rama classic-ps5-ataque: el ataque directo de Master Duel (puro)
#include "md_ritmo.h"    // rama classic-md-ritmo: viajes de cartas y activaciones de Master Duel (md_ritmo_ui.h)
#include "control.h"     // rama mando: el estado de la mesa, el foco y los menus (sin Vulkan)
#include "raton.h"       // rama classic-pc: las zonas donde se puede apuntar con el raton (solo las lee el juego en Windows)
#include "idioma.h"      // rama classic-idioma: textos ES/EN (tr) y cartas en ingles
#include "opciones.h"    // rama classic-portada: opciones::permitirProhibidas() (Opciones del menu)
#include "constructor.h" // rama classic-constructor: la pantalla "Construir mazo" (logica; se dibuja en constructor_ui.h)
#include "galeria.h"     // rama classic-galeria: la galeria de figuras 3D (logica; se dibuja en galeria_ui.h)
#include "resultado.h"   // rama classic-resultado: los datos de la pantalla de resultado (resultado_ui.h)
#include "entrada_clave.h"   // rama classic-cartas-clave: entrada especial de las cartas clave (entrada_clave_ui.h)
#include "personajes.h"  // rama classic-personajes: los personajes del anime (modo historia); retratos en personajes_ui.h
#include "duelo_visores.h" // rama classic-duelo-ui: registro, visores de zonas, cadena y jugables (duelo_consulta_ui.h)
#include "historia_dialogos.h"   // rama classic-historia: dialogos del modo historia, final y Modo Desafio
#include "tutorial.h"          // rama classic-historia: las reglas del duelo (paginas)
#include "cosmeticos.h"        // rama classic-extras: fundas, tapetes, controles (Personalizacion)
#include "estadisticas.h"      // rama classic-extras: estadisticas del jugador
#include <atomic>
#include <thread>

namespace interfaz {

enum Tipo { PANEL = 0, TEXTO, IMAGEN, DORSO, MARCO, PARTICULA, BARRA, ESTRELLA, ZONA, LINEA, FLECHA, LADO_MAZO, ORBE, FONDO, MONEDA, LUZ };   // (MONEDA: rama classic-moneda; LUZ: classic-ps5-efectos)

struct Vertice {
	glm::vec4 pos;      // espacio de recorte
	glm::vec2 uv;
	glm::vec2 local;
	glm::vec4 color, color2, forma, extra;
};

constexpr float TAM_BASE = 52.0f;     // alto en px de los glifos del atlas
constexpr int RELLENO_SDF = 8;
constexpr int ATLAS = 2048;
constexpr uint32_t MAX_VERTICES = 120000;
constexpr float ESCALA = 1.4f;        // ESCALA_TABLERO del juego
// medidas de referencia (pantalla de 1080 de alto)
constexpr float MARGEN = 22.0f, COL_X = 22.0f, COL_ANCHO = 400.0f;
constexpr float FASES_X = 436.0f, FASES_ANCHO = 64.0f;
constexpr float CAMPO_X = 512.0f;     // desde aqui a la derecha se ve el tablero

// paleta
const glm::vec4 VIDRIO{ 0.026f, 0.038f, 0.072f, 0.86f };
const glm::vec4 CIAN{ 0.30f, 0.86f, 1.0f, 1.0f };
const glm::vec4 CARMESI{ 1.0f, 0.27f, 0.42f, 1.0f };
const glm::vec4 ORO{ 1.0f, 0.78f, 0.32f, 1.0f };
const glm::vec4 BLANCO{ 0.94f, 0.97f, 1.0f, 1.0f };
const glm::vec4 GRIS{ 0.62f, 0.69f, 0.80f, 1.0f };
const glm::vec4 NADA{ 0.0f };

// zonas del tablero (remake/diseno/tablero.json; en el juego x1.4)
struct Zona { const char* id; float x, z; const char* tipo; };
static const Zona ZONAS[] = {
	{ "jugador_monstruo_0", -5.4f, 2.2f, "monstruo" }, { "jugador_magia_0", -5.4f, 5.5f, "magia" },
	{ "jugador_monstruo_1", -2.7f, 2.2f, "monstruo" }, { "jugador_magia_1", -2.7f, 5.5f, "magia" },
	{ "jugador_monstruo_2", 0.0f, 2.2f, "monstruo" }, { "jugador_magia_2", 0.0f, 5.5f, "magia" },
	{ "jugador_monstruo_3", 2.7f, 2.2f, "monstruo" }, { "jugador_magia_3", 2.7f, 5.5f, "magia" },
	{ "jugador_monstruo_4", 5.4f, 2.2f, "monstruo" }, { "jugador_magia_4", 5.4f, 5.5f, "magia" },
	{ "jugador_campo", -8.2f, 2.2f, "campo" }, { "jugador_fusion", -8.2f, 5.5f, "fusion" },
	{ "jugador_cementerio", 8.2f, 2.2f, "cementerio" }, { "jugador_mazo", 8.2f, 5.5f, "mazo" },
	{ "rival_monstruo_0", 5.4f, -2.2f, "monstruo" }, { "rival_magia_0", 5.4f, -5.5f, "magia" },
	{ "rival_monstruo_1", 2.7f, -2.2f, "monstruo" }, { "rival_magia_1", 2.7f, -5.5f, "magia" },
	{ "rival_monstruo_2", 0.0f, -2.2f, "monstruo" }, { "rival_magia_2", 0.0f, -5.5f, "magia" },
	{ "rival_monstruo_3", -2.7f, -2.2f, "monstruo" }, { "rival_magia_3", -2.7f, -5.5f, "magia" },
	{ "rival_monstruo_4", -5.4f, -2.2f, "monstruo" }, { "rival_magia_4", -5.4f, -5.5f, "magia" },
	{ "rival_campo", 8.2f, -2.2f, "campo" }, { "rival_fusion", 8.2f, -5.5f, "fusion" },
	{ "rival_cementerio", -8.2f, -2.2f, "cementerio" }, { "rival_mazo", -8.2f, -5.5f, "mazo" },
	{ "jugador_desterrado", 10.9f, 2.2f, "desterrado" }, { "rival_desterrado", -10.9f, -2.2f, "desterrado" },   // rama classic-duelo-ui
};
constexpr size_t NUM_ZONAS = sizeof(ZONAS) / sizeof(ZONAS[0]);
constexpr float ZONA_ANCHO = 1.16f * ESCALA, ZONA_ALTO = 1.62f * ESCALA;
constexpr float CARTA_ANCHO = 1.0f * ESCALA, CARTA_ALTO = 1.45f * ESCALA;

// el duelo de prueba (el mismo de herramientas/blender/escena_tablero.py CAMPO)
struct Ocupa { const char* zona; const char* carta; bool bocaAbajo; bool defensa = false; };
static const Ocupa CAMPO_PRUEBA[] = {
	{ "jugador_monstruo_0", "blue_eyes", false }, { "jugador_monstruo_1", "red_eyes", false },
	{ "jugador_monstruo_2", "dark_magician", false }, { "jugador_monstruo_3", "kuriboh", false, true },
	{ "jugador_monstruo_4", "dark_magician_girl", false },
	{ "rival_monstruo_0", "celtic_guardian", false, true }, { "rival_monstruo_1", "oni_tank_t_34", false },
	{ "rival_monstruo_2", "summoned_skull", false }, { "rival_monstruo_3", "deepsea_shark", false, true },
	{ "rival_monstruo_4", "stone_ogre_grotto", false, true },
	{ "jugador_magia_0", "swords_of_revealing_light", false }, { "jugador_magia_1", "", true },
	{ "jugador_magia_3", "", true }, { "rival_magia_1", "mirror_force", false },
	{ "rival_magia_2", "", true }, { "rival_magia_4", "", true },
	// (campo: Bosque, la carta que activa el entorno de bosque de yugireload.cpp; el tablero.glb trae Sogen, se oculta)
	{ "jugador_campo", "forest", false }, { "jugador_fusion", "flame_swordsman", false },
};
static const char* const MANO_JUGADOR[] = { "blue_eyes", "gaia", "pot_of_greed", "monster_reborn", "trap_hole", "time_wizard" };
constexpr int NUM_MANO_J = 6, NUM_MANO_R = 5;
constexpr int MAX_MANO_R = 10;   // la mano del rival en el duelo real (control.h manoR)
constexpr int MAX_MANO_J = 12;   // rama mando: la mano del jugador cambia de tamanno (control.h)
static const char* const FASES[] = { "DP", "SP", "M1", "BP", "M2", "EP" };
static const char* const NOMBRE_FASE[] = { "ROBO", "ESPERA", "PRINCIPAL 1", "BATALLA", "PRINCIPAL 2", "FINAL" };

// Selector de camara (rama selector-camara): los modos que elige el jugador con L1/R1 (C en un PC con ventana).
// La camara de cada modo la arma yugireload.cpp (elegirCamara); aqui solo se dibuja el selector
enum ModoCamara { CAM_CINE = 0, CAM_TABLERO, CAM_CENITAL, CAM_LATERAL, CAM_LIBRE, NUM_CAMARAS };
static const char* const NOMBRE_CAMARA[NUM_CAMARAS] = { "CINE", "TABLERO", "CENITAL", "LATERAL", "LIBRE" };
static const char* const CLAVE_CAMARA[NUM_CAMARAS] = { "cine", "tablero", "cenital", "lateral", "libre" };   // YUGI_CAMARA

struct Glifo { float u0, v0, u1, v1, xoff, yoff, w, h, avance; };
struct Fuente {
	stbtt_fontinfo info{};
	std::vector<unsigned char> datos;
	float escala{ 1.0f }, ascenso{ 0.0f };
	std::unordered_map<int, Glifo> glifos;
};
enum { F_NUMEROS = 0, F_TITULO, F_TEXTO, NUM_FUENTES };

struct CartaUI {
	std::string id, nombre, clase, atributo, tipo, desc;
	int nivel{ 0 }, atk{ 0 }, def{ 0 };
	// cara_[en_]<id>.png con mipmaps (tablero y vistas) y mini_[en_]<id>.png (mano, rejilla): cache perezosa (constructor_ui.h)
	vks::Texture2D cara, mini;
	VkDescriptorSet setCara{ VK_NULL_HANDLE }, setMini{ VK_NULL_HANDLE };
	bool lista{ false };
	// rama classic-idioma: los textos en ingles (cartas_ui_en.txt); vacios = se muestra el espanol. atributo/tipo
	// en espanol siguen siendo las claves de la logica (color del atributo, Bosque)
	std::string nombreEn, atributoEn, tipoEn, descEn;
	const std::string& vNombre() const { return idioma::ingles() && !nombreEn.empty() ? nombreEn : nombre; }
	const std::string& vAtributo() const { return idioma::ingles() && !atributoEn.empty() ? atributoEn : atributo; }
	const std::string& vTipo() const { return idioma::ingles() && !tipoEn.empty() ? tipoEn : tipo; }
	const std::string& vDesc() const { return idioma::ingles() && !descEn.empty() ? descEn : desc; }
	// rama classic-constructor: con 1690 cartas las caras se cargan a pedido (cache acotada, ver procesarCargas):
	// lista = cara y mini cargadas. uso*: ultimo cuadro en que se dibujo; quiere*: pedida para el proximo cuadro
	bool tieneCara{ false }, tieneMini{ false }, falloCara{ false }, falloMini{ false }, quiereCara{ false }, quiereMini{ false };
	uint64_t usoCara{ 0 }, usoMini{ 0 };
};

struct Chispa { glm::vec2 pos, vel; float vida, dur, tam; glm::vec3 color; float cruz; };

struct Vida {
	int valor{ 8000 };
	float mostrado{ 8000.0f }, fantasma{ 8000.0f };
	float tGolpe{ 99.0f };
	int ultimoDanio{ 0 };
	// rama classic-md-fases: el cambio de LP como DuelLP de Master Duel (fases_md_ui.h): s desde el cambio, de donde
	// cuenta, cuanto cambio (+ cura, - danno)
	float tCambio{ 99.0f }, desde{ 8000.0f };
	int cambio{ 0 };
};

// lo que se elige con el puntero
enum Clase { MANO_J = 0, MANO_R, EN_ZONA };
struct Objetivo { Clase clase; int indice; glm::vec2 pantalla; };

// Cartas de demostracion en las zonas de magia/trampa y de fusion de los dos jugadores (rama cartas-demo). Se
// dibujan planas sobre su casilla con la cara de la interfaz (mini_<id>.png) o el dorso propio, las del rival
// giradas 180 grados; el visor las muestra (ocupante() mira esta tabla antes que CAMPO_PRUEBA). Si falta la
// imagen de una carta en los assets, se dibuja boca abajo. Las de fusion son la carta de arriba de su pila.
static const Ocupa CARTAS_DEMO[] = {
	{ "jugador_magia_0", "swords_of_revealing_light", false }, { "jugador_magia_1", "", true },
	{ "jugador_magia_2", "monster_reborn", false }, { "jugador_magia_3", "", true },
	{ "jugador_magia_4", "polymerization", false },
	{ "rival_magia_0", "magic_cylinder", false }, { "rival_magia_1", "mirror_force", false },
	{ "rival_magia_2", "", true }, { "rival_magia_3", "raigeki", false }, { "rival_magia_4", "", true },
	{ "jugador_fusion", "flame_swordsman", false }, { "rival_fusion", "thousand_dragon", false },
};
constexpr int PILA_FUSION_J = 3, PILA_FUSION_R = 2;   // cartas en cada zona de fusion (la de arriba, a la vista)

inline std::vector<int> utf8(const std::string& s)
{
	std::vector<int> r;
	for (size_t i = 0; i < s.size();) {
		const unsigned char c = (unsigned char)s[i];
		int cp = c, n = 1;
		if (c >= 0xF0) { cp = c & 0x07; n = 4; }
		else if (c >= 0xE0) { cp = c & 0x0F; n = 3; }
		else if (c >= 0xC0) { cp = c & 0x1F; n = 2; }
		for (int k = 1; k < n && i + k < s.size(); k++) {
			cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
		}
		r.push_back(cp);
		i += n;
	}
	return r;
}

inline std::string mayusculas(const std::string& s)
{
	std::string r = s;
	for (char& c : r) {
		if (c >= 'a' && c <= 'z') c = (char)(c - 32);
	}
	return r;
}

class Interfaz {
public:
	// rama classic-pc: zonas para el raton (raton.h) y el ultimo dispositivo de entrada (0 PlayStation, 1 teclado y raton, 2 Xbox:
	// cambia los iconos de los botones de la ayuda; en la PS5 siempre 0)
	raton::Registro rz;
	int dispositivo{ 0 };
	float anchoPantalla() const { return W; }
	float altoPantalla() const { return H; }
	// una zona rectangular (px de pantalla) y atajos: lista (un eje), boton suelto (clic sin mover el foco)
	void zRect(float x, float y, float w, float h, raton::Zona z)
	{
		z.a = glm::vec2(x, y);
		z.b = glm::vec2(x + w, y + h);
		rz.zonas.push_back(z);
	}
	void zLista(float x, float y, float w, float h, int pos, int cur, bool horizontal = false, uint32_t clic = control::B_X)
	{
		raton::Zona z;
		z.e[0] = raton::ejeLista(pos, cur, horizontal ? control::B_DER : control::B_ABAJO, horizontal ? control::B_IZQ : control::B_ARRIBA);
		z.clic = clic;
		zRect(x, y, w, h, z);
	}
	// una zona de la mesa del duelo (la carta de la mano o la zona del tablero que lleva el foco): solo si nada la tapa
	void zFoco(float x, float y, float w, float h, int tipo, int i, uint32_t clic = control::B_X)
	{
		if (rz.tapas > 0) return;
		raton::Zona z;
		z.fTipo = tipo;
		z.fI = i;
		z.clic = clic;
		zRect(x, y, w, h, z);
	}
	void zBoton(float x, float y, float w, float h, uint32_t clic, int clase = 0)
	{
		raton::Zona z;
		z.clic = clic;
		z.clase = clase;
		zRect(x, y, w, h, z);
	}
	// ---------------------------------------------------------------------------------------------- preparar
	void preparar(vks::VulkanDevice* d, VkQueue q, VkFormat formatoColor, VkFormat formatoProf, VkPipelineCache cache,
	              const std::string& assets, const std::string& shaders)
	{
		disp = d;
		dev = d->logicalDevice;
		cola = q;
		rutaAssets = assets;
		leerEntorno();
		cargarCartas();
		cargarDorso();
		empezarFundas();   // rama classic-extras: las fundas por codigo, en otro hilo (cosmeticos_ui.h)
		crearAtlas();
		crearDescriptores();
		crearPipelines(formatoColor, formatoProf, cache, shaders);
		for (auto& b : vertices) {
			VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			                                   &b, MAX_VERTICES * sizeof(Vertice)));
			VK_CHECK_RESULT(b.map());
		}
		v.reserve(MAX_VERTICES);
		listo = true;
	}

	~Interfaz()
	{
		if (!dev) {
			return;
		}
		vkDeviceWaitIdle(dev);
		liberarFundas();   // rama classic-extras
		for (auto& b : vertices) {
			b.destroy();
		}
		for (CartaUI& c : cartas) {
			if (c.tieneCara) c.cara.destroy();
			if (c.tieneMini) c.mini.destroy();
		}
		for (CaraHD& h : hd) {
			if (h.carta >= 0) h.tex.destroy();
		}
		if (hayDorso) {
			dorso.destroy();
			dorsoMini.destroy();
		}
		atlas.destroy();
		vkDestroySampler(dev, muestreador, nullptr);
		vkDestroyPipeline(dev, p2d, nullptr);
		vkDestroyPipeline(dev, p3d, nullptr);
		vkDestroyPipelineLayout(dev, layout, nullptr);
		vkDestroyDescriptorSetLayout(dev, layoutSet, nullptr);
		vkDestroyDescriptorPool(dev, pool, nullptr);
	}

	// El tablero se corre a la derecha de la columna izquierda (lente desplazada en el espacio de recorte):
	// x' = k x + dx w, y' = k y + dy w. Se aplica a la proyeccion de la escena (y la interfaz usa la misma)
	glm::mat4 ajustarProyeccion(const glm::mat4& proy, float ancho, float alto) const
	{
		const float s = alto / 1080.0f;
		const float izq = CAMPO_X * s;
		const float k = (ancho - izq) / ancho * zoom;
		const float cx = ((izq + ancho) * 0.5f) / ancho * 2.0f - 1.0f;
		glm::mat4 m(1.0f);
		m[0][0] = k;
		m[1][1] = k;
		m[3][0] = cx;
		m[3][1] = desplazaY;
		return m * proy;
	}

	// Carta suelta en el mundo 3D (la invocacion de yugireload.cpp la voltea): centro (m), medio ancho y medio alto
	// como vectores (el alto apunta hacia el pie de la imagen), cara o dorso, y de que lado es (color del dorso).
	// Se pide en cada cuadro antes de dibujar(); se dibuja con la profundidad de la escena
	void cartaEnMundo(const std::string& id, glm::vec3 centro, glm::vec3 medioAncho, glm::vec3 medioAlto, bool cara, bool rival, float brillo = 1.0f)
	{
		cartasMundo.push_back({ id, centro, medioAncho, medioAlto, cara, rival, brillo });
	}

	// Pedazo de una carta en el mundo 3D (la carta de campo que estalla, yugireload.cpp): sus 4 esquinas (en orden,
	// alrededor) y su lugar en la cara (uv 0..1 de la carta entera). Se pide en cada cuadro, como cartaEnMundo
	void fragmentoEnMundo(const std::string& id, const glm::vec3 esquinas[4], const glm::vec2 uv[4], float brillo)
	{
		FragmentoMundo f{ id, {}, {}, brillo };
		for (int i = 0; i < 4; i++) {
			f.esquinas[i] = esquinas[i];
			f.uv[i] = uv[i];
		}
		fragmentosMundo.push_back(f);
	}

	// Carta de campo Bosque (rama campo-bosque): "ambiente" 0..1 apaga el fondo de siempre (el juego dibuja debajo el
	// fondo de bosque) y "bono" 0..1 suma +200 a ATK/DEF de los tipos que potencia (rotulos en verde)
	void campoBosque(float ambiente, float bono)
	{
		fondoBosque = glm::clamp(ambiente, 0.0f, 1.0f);
		bonoBosque = glm::clamp(bono, 0.0f, 1.0f);
	}
	// rama classic-efectos: el ATK/DEF de verdad de un monstruo de la mesa (equipos, campos...) por id de zona; si
	// devuelve true, los rotulos usan esos numeros (verde si suben, rojo si bajan)
	std::function<bool(const char* zona, int& atk, int& def)> statsZona;
	// los tipos que potencia Bosque en el juego: Insecto, Bestia, Planta y Guerrero-Bestia
	bool potenciaBosque(const std::string& id) const
	{
		const int n = buscarCarta(id);
		if (n < 0) return false;
		const std::string& t = cartas[n].tipo;
		return t == "Insecto" || t == "Bestia" || t == "Planta" || t == "Guerrero-Bestia";
	}

	// Rotulos ATK/DEF flotantes: solo se ven con la camara general/frontal del tablero. El juego dice en cada cuadro
	// si la camara activa es esa (yugireload.cpp: camaraFrontal(); el selector de camara de la rama selector-camara
	// debe llamar aqui con true solo para su vista frontal). Al pasar a false (ataque, cine, invocacion, otra
	// camara) se desvanecen en ROTULOS_FUNDIDO s, y vuelven igual al volver a true
	void rotulosVisibles(bool visibles) { mostrarRotulos = visibles; }

	// Quien esta en cada casilla de monstruo (rama matriz-casillas: el juego puede poner cualquier monstruo en
	// cualquiera de las 5 casillas de su lado). k = 0..4 como las zonas jugador_monstruo_k / rival_monstruo_k; carta
	// vacia: casilla libre. Mientras el juego no lo diga, vale el duelo de prueba (CAMPO_PRUEBA). Lo usan los rotulos
	// ATK/DEF y el visor
	void ocuparMonstruo(bool rival, int k, const char* carta, bool defensa)
	{
		if (k < 0 || k >= 5) return;
		char id[32];
		snprintf(id, sizeof(id), "%s_monstruo_%d", rival ? "rival" : "jugador", k);
		const Zona* z = zonaPorId(id);
		if (!z) return;
		campoPropio = true;
		ocupantesMonstruo[(rival ? 5 : 0) + k] = { z->id, carta ? carta : "", false, defensa };
	}
	void soltarCampo() { campoPropio = false; }

	// El ataque con cartas planas (como el juego clasico): el juego dice en cada cuadro que zona ataca a cual, el
	// segundo del ataque, el danno a mostrar y que cartas se destruyen. La atacante se levanta, se echa atras, se
	// lanza inclinada hacia el objetivo y vuelve; el objetivo tiembla con el golpe (y se voltea si estaba boca abajo);
	// las destruidas se desvanecen hacia arriba; el numero de danno sube y se apaga sobre el objetivo
	struct Golpe {
		int atacante{ -1 }, objetivo{ -1 };
		float t{ -1.0f };
		int danio{ 0 };
		bool muereA{ false }, muereO{ false };
		bool alto{ false };   // rama classic-md-combate: viaje "High" de MD (ATK >= 2500; el reloj t ya va en el del "Low")
		// rama classic-ps5-ataque (ataque_md.h): ataque directo de MD, el viaje va al punto del que recibe (no a una zona)
		bool directo{ false };
		glm::vec3 punto{ 0.0f };
		bool alJugador{ false };   // el danno es del jugador (DamageFrame de MD)
		// rama classic-ps5-ataque4: el calculo que se muestra sobre las cartas (-1: sin calculo)
		int valorA{ -1 }, valorO{ -1 };
		bool defensaO{ false };
	};
	// rama classic-ps5-ataque: el numero del cambio de LP de MD (ChangeValue, fases_md_ui.h) de ese lado se esta viendo: el
	// del golpe no se dibuja (antes salian dos numeros iguales por golpe; MD tiene uno solo, ShowDamageImpl -> DuelLP)
	// (solo si muestra ESE valor: si enseguida llega otro cambio, Marshmallon, el del golpe sigue viendose)
	bool numeroLPActivoMD(bool rival, int valor) const
	{
		const Vida& vi = rival ? vidaR : vidaJ;
		return vi.cambio == valor && vi.tCambio <= inicioCuentaLP() + 0.25f;
	}
	// rama classic-ps5-ataque5: la carta que ataca se eleva sobre su zona al declarar el ataque (ataque_elevacion_juego.h)
	int zonaElevada{ -1 };
	float alzaElevada{ 0.0f };   // 0..1
	static constexpr float ELEV_ALTO = 0.45f;   // m de nuestra mesa (la carta mide 2.03): ~22% de su alto
	void elevacionAtacante(int zona, float k) { zonaElevada = zona, alzaElevada = k; }
	// rama classic-ps5-ataque: a donde va el ataque en curso (la zona del objetivo o, si es directo, el punto de MD)
	glm::vec3 destinoGolpe() const { return golpe.directo ? golpe.punto : centroZona(golpe.objetivo); }
	// rama classic-md-ritmo: los tiempos del ataque de Master Duel (AttackMotionSetting + BezierMotion Strike*00: viaje 0.45 s
	// con carga lenta y embestida, impacto al final; EffectTaskBattleRun.attackExecPostTime 0.25; vuelta ~0.45; la carta
	// destruida no espera: sale tras la pausa del golpe; el numero de danno se ve ~0.8 s). Antes: 0.35 / 0.55 / 1.0 / 0.9 / 1.6
	// (rama classic-ps5-ataque3s: el ataque de Joey, ~3 s: md_ritmo.h T_POST_GOLPE / T_FIN_ATAQUE. Antes 1.15 / 0.70 / 1.30)
	static constexpr float G_CARGA = 0.25f, G_IMPACTO = 0.45f, G_VUELVE = G_IMPACTO + ritmo::T_POST_GOLPE + ritmo::T_VUELTA,
	                       G_DESVANECE = ritmo::T_ROMPE, G_FIN = ritmo::T_FIN_ATAQUE, G_LP = ritmo::T_LP;   // (G_LP: ataque4)
	void golpePlano(const Golpe& g) { golpe = g; }   // vuelve a mandar la mesa (o el duelo de prueba)
	// rama classic-figuras: true si en esa zona hay una figura 3D de pie (su carta no embiste: ataca la figura)
	std::function<bool(int)> figuraEnZona;
	bool rotulosVisibles() const { return mostrarRotulos; }
	float opacidadRotulos() const { return alfaRotulos; }   // 0..1, con el fundido en curso

	// rama mando: el estado del duelo interactivo (control.h). Con el mando conectado, la mano, las zonas, el foco del
	// recuadro dorado, la fase y los cementerios salen de ahi (sin el, las tablas de prueba de siempre)
	void conectarMando(const control::Mando* m) { ctl = m; }
	// rama classic-constructor: la pantalla "Construir mazo" (si esta abierta se dibuja en lugar del duelo) y las
	// cartas que quedan en cada mazo
	void conectarConstructor(const constructor::Constructor* c) { cons = c; }
	void conectarGaleria(const galeria::Galeria* g) { gal = g; }   // rama classic-galeria
	void contarMazos(int jugador, int rival, int fusionJ)
	{
		mazoJ = std::max(0, jugador);
		mazoR = std::max(0, rival);
		pilaFusionJ = std::max(0, fusionJ);
	}
	// la clase de una carta (normal, efecto, fusion, magia, trampa) de cartas_ui.txt; "" si no esta
	std::string claseCarta(const std::string& id) const
	{
		const int n = buscarCarta(id);
		return n >= 0 ? cartas[n].clase : std::string();
	}

	// LP nuevos de un lado (animacion de danno o curacion)
	void cambiarLP(bool rival, int lp)
	{
		Vida& vi = rival ? vidaR : vidaJ;
		lp = glm::clamp(lp, 0, 99999);
		if (lp != vi.valor) {   // (rama classic-md-fases: la cuenta de MD arranca desde lo que se ve)
			vi.tCambio = 0.0f;
			vi.desde = vi.mostrado;
			vi.cambio = lp - vi.valor;
		}
		if (lp < vi.valor) {
			vi.tGolpe = 0.0f;
			vi.ultimoDanio = vi.valor - lp;
			vi.fantasma = glm::max(vi.fantasma, vi.mostrado);
			estallar(rival);
		}
		vi.valor = lp;
	}
	// rama classic-visual-final (PC, YUGI_RESULTADO): los LP puestos de una vez, sin la animacion de los numeros
	void fijarLP(bool rival, int lp)
	{
		Vida& vi = rival ? vidaR : vidaJ;
		vi.valor = glm::clamp(lp, 0, 99999);
		vi.mostrado = vi.fantasma = vi.desde = (float)vi.valor;
		vi.tCambio = 99.0f;   // (rama classic-md-fases: sin la cuenta de MD)
	}

	// selector de camara (esquina inferior izquierda del campo): el modo actual, segundos desde el ultimo cambio y
	// hacia donde se cambio (+1 R1, -1 L1, 0 por variable/ajustes). El juego lo llama en cada cuadro
	void selectorCamara(int modo, float desdeCambio, int direccion)
	{
		camModo = glm::clamp(modo, 0, (int)NUM_CAMARAS - 1);
		camDesde = desdeCambio;
		camDir = direccion;
	}

	// ---------------------------------------------------------------------------------------------- cada cuadro
	// lee el control (ya leido por el juego en "mando"); libre = false cuando la ventana de ajustes tiene el control
	void actualizar(const struct pad& mando, float dt, bool libre)
	{
		if (!listo) {
			return;
		}
		dt = glm::clamp(dt, 0.0f, 0.1f);
		t += dt;
		procesarCargas();   // rama classic-constructor: caras a pedido
		cargarRetratos();   // rama classic-personajes: retratos a pedido (personajes_ui.h)
		cargarMD();         // rama classic-md-fases: datos y sprites de Master Duel (fases_md_ui.h)
		cargarPortada(dt);  // rama classic-portada (portada_ui.h)
		demo();
		actualizarMando();   // rama mando: el foco, la fase y los cementerios vienen de control.h
		if (libre && !fijo && !ctl) {
			glm::vec2 dir{ 0.0f };
			if (mando.pressed & PAD_LEFT) dir = { -1, 0 };
			if (mando.pressed & PAD_RIGHT) dir = { 1, 0 };
			if (mando.pressed & PAD_UP) dir = { 0, -1 };
			if (mando.pressed & PAD_DOWN) dir = { 0, 1 };
			// cruceta mantenida: repite
			const uint32_t cruz = mando.held & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN);
			if (cruz && cruz == cruzAntes) {
				repetir -= dt;
				if (repetir <= 0.0f) {
					repetir = 0.13f;
					if (cruz & PAD_LEFT) dir = { -1, 0 };
					if (cruz & PAD_RIGHT) dir = { 1, 0 };
					if (cruz & PAD_UP) dir = { 0, -1 };
					if (cruz & PAD_DOWN) dir = { 0, 1 };
				}
			} else {
				repetir = 0.38f;
			}
			cruzAntes = cruz;
			// palanca izquierda: un paso al inclinarla, y repite si se mantiene
			const glm::vec2 palanca{ mando.left_x, mando.left_y };
			if (glm::length(palanca) > 0.6f) {
				palancaT -= dt;
				if (palancaT <= 0.0f) {
					dir = fabsf(palanca.x) > fabsf(palanca.y) ? glm::vec2(glm::sign(palanca.x), 0.0f) : glm::vec2(0.0f, glm::sign(palanca.y));
					palancaT = palancaSuelta ? 0.38f : 0.14f;
					palancaSuelta = false;
				}
			} else if (glm::length(palanca) < 0.3f) {
				palancaT = 0.0f;
				palancaSuelta = true;
			}
			if (dir != glm::vec2(0.0f)) {
				mover(dir);
			}
		}
		animarVidas(dt);
		animarChispas(dt);
		// la carta del visor gira al cambiar lo elegido
		const int sel = seleccion;
		if (sel != selVisor) {
			if (giroVisor < 0.0f) {
				giroVisor = 0.0f;
				selNueva = sel;
			}
		}
		if (giroVisor >= 0.0f) {
			giroVisor += dt / 0.38f;
			if (giroVisor >= 0.5f && selVisor != selNueva) {
				selVisor = selNueva;
				tTexto = 0.0f;
			}
			if (giroVisor >= 1.0f) {
				giroVisor = -1.0f;
			}
		}
		tTexto += dt;
		// manos: la elegida se levanta; la mano con el puntero sube un poco entera
		for (int k = 0; k < nManoJ(); k++) {
			resorte(elevaJ[k], velJ[k], (objetivos.size() > (size_t)seleccion && objetivos[seleccion].clase == MANO_J && objetivos[seleccion].indice == k) || manoElegida(k) ? 1.0f : 0.0f, dt);
		}
		for (int k = 0; k < nManoR(); k++) {
			resorte(elevaR[k], velR[k], objetivos.size() > (size_t)seleccion && objetivos[seleccion].clase == MANO_R && objetivos[seleccion].indice == k ? 1.0f : 0.0f, dt);
		}
		const Clase c = objetivos.empty() ? EN_ZONA : objetivos[seleccion].clase;
		resorte(focoJ, velFocoJ, c == MANO_J ? 1.0f : 0.0f, dt);
		resorte(focoR, velFocoR, c == MANO_R ? 1.0f : 0.0f, dt);
		dtCuadro = dt;
	}

	// arma todo y lo graba en el comando (despues de la escena 3D, en la misma pasada: usa su profundidad)
	void dibujar(VkCommandBuffer cmd, uint32_t cuadro, const glm::mat4& vistaProy, float ancho, float alto)
	{
		if (!listo) {
			return;
		}
		W = ancho;
		H = alto;
		s = alto / 1080.0f;
		vp = vistaProy;
		rz.empezarCuadro();   // rama classic-pc
		subirFundas();   // rama classic-extras: en cuanto el hilo las termino
		v.clear();
		lotes.clear();
		construirObjetivos();
		construir();
		const size_t n = glm::min(v.size(), (size_t)MAX_VERTICES);
		memcpy(vertices[cuadro].mapped, v.data(), n * sizeof(Vertice));
		const glm::vec4 pantalla(W, H, t, s);
		VkDeviceSize desde = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &vertices[cuadro].buffer, &desde);
		VkPipeline actual = VK_NULL_HANDLE;
		for (size_t i = 0; i < lotes.size(); i++) {
			const uint32_t fin = i + 1 < lotes.size() ? lotes[i + 1].desde : (uint32_t)n;
			if (lotes[i].desde >= fin || lotes[i].desde >= n) {
				continue;
			}
			const VkPipeline p = lotes[i].en3D ? p3d : p2d;
			if (p != actual) {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
				vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &pantalla);
				actual = p;
			}
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &lotes[i].set, 0, nullptr);
			vkCmdDraw(cmd, fin - lotes[i].desde, 1, lotes[i].desde, 0);
		}
	}

private:
	vks::VulkanDevice* disp{ nullptr };
	VkDevice dev{ VK_NULL_HANDLE };
	VkQueue cola{ VK_NULL_HANDLE };
	std::string rutaAssets;
	bool listo{ false };
	VkDescriptorPool pool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout layoutSet{ VK_NULL_HANDLE };
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	VkPipeline p2d{ VK_NULL_HANDLE }, p3d{ VK_NULL_HANDLE };
	VkSampler muestreador{ VK_NULL_HANDLE };
	vks::Texture2D atlas;
	VkDescriptorSet setAtlas{ VK_NULL_HANDLE };
	std::array<vks::Buffer, maxConcurrentFrames> vertices;
	std::array<Fuente, NUM_FUENTES> fuentes;
	mutable std::vector<CartaUI> cartas;   // (mutable: listaCarta marca el uso al dibujar)
	mutable std::unordered_map<std::string, int> indiceCartas;   // id -> indice en cartas (buscarCarta)
	uint64_t cuadroN{ 10 };                                      // cuadros (la cache de caras)
	const constructor::Constructor* cons{ nullptr };             // rama classic-constructor
	const galeria::Galeria* gal{ nullptr };                      // rama classic-galeria
	std::vector<std::string> monstruos_;   // monstruos de cartas_ui.txt (mano de prueba)
	Golpe golpe;   // el ataque plano en curso (golpePlano)
	// dorso original (dorso.png) y su mitad para el tablero (sin mipmaps: la de tamano completo parpadearia lejos)
	vks::Texture2D dorso, dorsoMini;
	VkDescriptorSet setDorso{ VK_NULL_HANDLE }, setDorsoMini{ VK_NULL_HANDLE };
	bool hayDorso{ false };

	std::vector<Vertice> v;
	struct Lote { VkDescriptorSet set; bool en3D; uint32_t desde; };
	std::vector<Lote> lotes;

	float W{ 1920 }, H{ 1080 }, s{ 1 }, t{ 0 }, dtCuadro{ 0.016f };
	glm::mat4 vp{ 1.0f };

	// puntero
	std::vector<Objetivo> objetivos;
	int seleccion{ 0 };
	std::string pedido{ "jugador_mano_0" };   // lo elegido por nombre (se resuelve al armar los objetivos)
	bool fijo{ false }, recorrido{ false };
	uint32_t cruzAntes{ 0 };
	float repetir{ 0.38f }, palancaT{ 0.0f };
	bool palancaSuelta{ true };
	// recuadro dorado: esquinas (recorte) y forma, con resorte
	std::array<glm::vec4, 4> marco{}, velMarco{};
	glm::vec4 formaMarco{ 0.0f }, velForma{ 0.0f };   // medio ancho, medio alto, grosor, halo
	bool marcoListo{ false };
	bool marcoEn3D{ false };
	float llegada{ 0.0f };
	float emitir{ 0.0f };
	// manos
	float elevaJ[MAX_MANO_J]{}, velJ[MAX_MANO_J]{}, elevaR[MAX_MANO_R]{}, velR[MAX_MANO_R]{};
	float focoJ{ 0 }, velFocoJ{ 0 }, focoR{ 0 }, velFocoR{ 0 };
	// visor
	int selVisor{ -1 }, selNueva{ 0 };
	float giroVisor{ -1.0f }, tTexto{ 0.0f };
	// partida
	Vida vidaJ, vidaR;
	int fase{ 3 };
	int mazoJ{ 34 }, mazoR{ 35 }, cementerioJ{ 0 }, cementerioR{ 0 };
	int pilaFusionJ{ PILA_FUSION_J };   // rama classic-constructor: el mazo de fusion del jugador
	std::vector<Chispa> chispas;
	struct CartaMundo { std::string id; glm::vec3 centro, ancho, alto; bool cara, rival; float brillo; };
	std::vector<CartaMundo> cartasMundo;   // cartaEnMundo (se vacia en cada cuadro)
	struct FragmentoMundo { std::string id; glm::vec3 esquinas[4]; glm::vec2 uv[4]; float brillo; };
	std::vector<FragmentoMundo> fragmentosMundo;   // fragmentoEnMundo (se vacia en cada cuadro)
	float fondoBosque{ 0.0f }, bonoBosque{ 0.0f };   // campoBosque()
	std::array<float, 16> alfaAtkDef{};   // visibilidad de cada etiqueta ATK/DEF (se desvanecen si se pisan)
	bool campoPropio{ false };            // ocuparMonstruo: el juego dice quien esta en cada casilla de monstruo
	std::array<Ocupa, 10> ocupantesMonstruo{};   // jugador 0..4, rival 0..4
	static constexpr float ROTULOS_FUNDIDO = 0.25f;   // s del fundido de los rotulos al cambiar de camara
	bool mostrarRotulos{ true };          // rotulosVisibles(): la camara activa es la frontal
	float alfaRotulos{ 1.0f };            // opacidad global de los rotulos (fundido)
	uint32_t semilla{ 1234567u };
	// pruebas en el PC
	float zoom{ 0.71f }, desplazaY{ -0.17f };
	int lpPedidoR{ -1 }, lpPedidoJ{ -1 };
	bool lpHecho{ false };
	// selector de camara (lo llena selectorCamara)
	int camModo{ CAM_CINE }, camDir{ 0 };
	float camDesde{ 99.0f };

	float azar()
	{
		semilla = semilla * 1664525u + 1013904223u;
		return (float)(semilla >> 8) / 16777216.0f;
	}

	static void resorte(float& x, float& vel, float meta, float dt, float k = 260.0f, float amort = 0.72f)
	{
		const float a = (meta - x) * k - vel * 2.0f * sqrtf(k) * amort;
		vel += a * dt;
		x += vel * dt;
	}

	// ------------------------------------------------------------------------------------------ recursos
	void leerEntorno()
	{
#if defined(PS5_HOST_REFERENCE)
		if (const char* p = getenv("YUGI_PUNTERO")) {
			if (strcmp(p, "recorrido") == 0) {
				recorrido = true;
			} else {
				pedido = p;
			}
			fijo = true;
		}
		if (const char* l = getenv("YUGI_LP")) {
			sscanf(l, "%d,%d", &lpPedidoR, &lpPedidoJ);
		}
		if (const char* f = getenv("YUGI_FASE")) {
			for (int i = 0; i < 6; i++) {
				if (strcmp(f, FASES[i]) == 0) fase = i;
			}
		}
		if (const char* z = getenv("YUGI_UI_ZOOM")) zoom = (float)atof(z);
		if (const char* y = getenv("YUGI_UI_DY")) desplazaY = (float)atof(y);
#endif
	}

	void demo()
	{
		if (recorrido) {
			static const char* const PASOS[] = { "jugador_mano_0", "jugador_mano_2", "jugador_monstruo_2", "jugador_monstruo_1",
			                                     "rival_monstruo_0", "rival_magia_1", "rival_mano_2", "rival_mazo",
			                                     "jugador_fusion", "jugador_mazo", "jugador_mano_4" };
			const int n = sizeof(PASOS) / sizeof(PASOS[0]);
			pedido = PASOS[(int)(t / 1.3f) % n];
		}
		if (!lpHecho && lpPedidoR >= 0 && t > 1.2f) {
			cambiarLP(true, lpPedidoR);
			lpHecho = true;
		}
		if (lpPedidoJ >= 0 && t > 2.8f && vidaJ.valor != lpPedidoJ && vidaJ.tGolpe > 50.0f) {
			cambiarLP(false, lpPedidoJ);
		}
	}

	int buscarCarta(const std::string& id) const
	{
		if (indiceCartas.size() != cartas.size()) {   // (cartas solo crece: se rehace el indice si cambio)
			indiceCartas.clear();
			for (size_t i = 0; i < cartas.size(); i++) indiceCartas.emplace(cartas[i].id, (int)i);
		}
		auto it = indiceCartas.find(id);
		return it == indiceCartas.end() ? -1 : it->second;
	}

	void cargarCartas()
	{
		leerCartas(rutaAssets + "ui/cartas_ui.txt");   // (las imagenes, a pedido: procesarCargas)
		leerCartasIngles(rutaAssets + "ui/cartas_ui_en.txt");   // rama classic-idioma: nombre/tipo/texto en ingles
		// las caras se cargan cuando se ven por primera vez (caraLista): son 1690 cartas en HD
		for (const CartaUI& k : cartas) {
			if ((k.clase == "normal" || k.clase == "efecto" || k.clase == "fusion" || k.clase == "ritual")) monstruos_.push_back(k.id);
		}
		fprintf(stderr, "[interfaz] cartas de monstruo para la mano de prueba: %d\n", (int)monstruos_.size());
	}

	void leerCartas(const std::string& ruta)
	{
		std::ifstream f(ruta);
		std::string linea;
		while (std::getline(f, linea)) {
			if (!linea.empty() && linea.back() == '\r') linea.pop_back();
			std::vector<std::string> c;
			size_t a = 0, b;
			while ((b = linea.find('|', a)) != std::string::npos) {
				c.push_back(linea.substr(a, b - a));
				a = b + 1;
			}
			c.push_back(linea.substr(a));
			if (c.size() < 9) continue;
			CartaUI k;
			k.id = c[0]; k.nombre = c[1]; k.clase = c[2]; k.atributo = c[3]; k.tipo = c[4];
			k.nivel = atoi(c[5].c_str()); k.atk = atoi(c[6].c_str()); k.def = atoi(c[7].c_str()); k.desc = idioma::reglasEs(c[8]);
			if (buscarCarta(k.id) >= 0) continue;
			cartas.push_back(std::move(k));
		}
	}

public:
	// las cartas de monstruo con cara (cartas_ui.txt): la mano de prueba de Ajustes
	const std::vector<std::string>& monstruos() const { return monstruos_; }
	// ATK y DEF de una carta (0 si no esta)
	int atkDe(const std::string& id) const { const int n = buscarCarta(id); return n >= 0 ? cartas[n].atk : 0; }
	int defDe(const std::string& id) const { const int n = buscarCarta(id); return n >= 0 ? cartas[n].def : 0; }
	int lp(bool rival) const { return (rival ? vidaR : vidaJ).valor; }

private:
	// ---- rama classic-idioma: los textos en ingles de cada carta (mismo formato que cartas_ui.txt; solo se usan
	// nombre, atributo, tipo y texto: la clase y los numeros siguen los del espanol)
	void leerCartasIngles(const std::string& ruta)
	{
		std::ifstream f(ruta);
		std::string linea;
		int n = 0;
		while (std::getline(f, linea)) {
			if (!linea.empty() && linea.back() == '\r') linea.pop_back();
			std::vector<std::string> c;
			size_t a = 0, b;
			while ((b = linea.find('|', a)) != std::string::npos) {
				c.push_back(linea.substr(a, b - a));
				a = b + 1;
			}
			c.push_back(linea.substr(a));
			if (c.size() < 9) continue;
			const int i = buscarCarta(c[0]);
			if (i < 0) continue;
			CartaUI& k = cartas[i];
			k.nombreEn = c[1]; k.atributoEn = c[3]; k.tipoEn = c[4]; k.descEn = c[8];
			n++;
		}
		fprintf(stderr, "[interfaz] cartas en ingles: %d\n", n);
	}

	static bool existe(const std::string& ruta)
	{
		if (FILE* f = fopen(ruta.c_str(), "rb")) {
			fclose(f);
			return true;
		}
		return false;
	}

	void apuntarSet(VkDescriptorSet set, vks::Texture2D& tex)
	{
		VkDescriptorImageInfo img{ muestreador, tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet w = vks::initializers::writeDescriptorSet(set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &img);
		vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
	}

	// ---- cara HD (cara_hd_[en_]<id>.jpg) para las vistas grandes (visor y detalle): la cache de HD del constructor
	// (constructor_ui.h: caraHD/cargarHD, MAX_HD con descarte); sin HD cargada todavia, la cara normal
	VkDescriptorSet setCaraGrande(const CartaUI* k) const
	{
		const CaraHD* h = caraHD((int)(k - cartas.data()));
		return h ? h->set : k->setCara;
	}

	bool cargarImagen(const std::string& ruta, vks::Texture2D& tex)
	{
		int w, h, n;
		stbi_uc* px = stbi_load(ruta.c_str(), &w, &h, &n, 4);
		if (!px) {
			fprintf(stderr, "[interfaz] falta %s\n", ruta.c_str());
			return false;
		}
		tex.fromBuffer(px, (VkDeviceSize)w * h * 4, VK_FORMAT_R8G8B8A8_UNORM, w, h, disp, cola, VK_FILTER_LINEAR);
		stbi_image_free(px);
		return true;
	}

	bool cargarConMips(const std::string& ruta, vks::Texture2D& tex, bool avisar)
	{
		int w, h, n;
		stbi_uc* px = stbi_load(ruta.c_str(), &w, &h, &n, 4);
		if (!px) {
			if (avisar) fprintf(stderr, "[interfaz] falta %s\n", ruta.c_str());
			return false;
		}
		const uint32_t mips = (uint32_t)floorf(log2f((float)std::max(w, h))) + 1;
		vks::Buffer subida;
		VK_CHECK_RESULT(disp->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &subida,
		                                   (VkDeviceSize)w * h * 4, px));
		stbi_image_free(px);
		VkImageCreateInfo ii = vks::initializers::imageCreateInfo();
		ii.imageType = VK_IMAGE_TYPE_2D;
		ii.format = VK_FORMAT_R8G8B8A8_UNORM;
		ii.mipLevels = mips;
		ii.arrayLayers = 1;
		ii.samples = VK_SAMPLE_COUNT_1_BIT;
		ii.tiling = VK_IMAGE_TILING_OPTIMAL;
		ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ii.extent = { (uint32_t)w, (uint32_t)h, 1 };
		ii.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VK_CHECK_RESULT(vkCreateImage(dev, &ii, nullptr, &tex.image));
		VkMemoryRequirements req;
		vkGetImageMemoryRequirements(dev, tex.image, &req);
		VkMemoryAllocateInfo ma = vks::initializers::memoryAllocateInfo();
		ma.allocationSize = req.size;
		ma.memoryTypeIndex = disp->getMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(dev, &ma, nullptr, &tex.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(dev, tex.image, tex.deviceMemory, 0));
		VkCommandBuffer cmd = disp->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vks::tools::setImageLayout(cmd, tex.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, 0, 1 });
		VkBufferImageCopy copia{};
		copia.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copia.imageExtent = ii.extent;
		vkCmdCopyBufferToImage(cmd, subida.buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copia);
		for (uint32_t i = 1; i < mips; i++) {
			vks::tools::setImageLayout(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 1, 0, 1 });
			VkImageBlit b{};
			b.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1 };
			b.srcOffsets[1] = { std::max(w >> (i - 1), 1), std::max(h >> (i - 1), 1), 1 };
			b.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };
			b.dstOffsets[1] = { std::max(w >> i, 1), std::max(h >> i, 1), 1 };
			vkCmdBlitImage(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &b, VK_FILTER_LINEAR);
		}
		if (mips > 1) {
			vks::tools::setImageLayout(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, 0, mips - 1, 0, 1 });
		}
		vks::tools::setImageLayout(cmd, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { VK_IMAGE_ASPECT_COLOR_BIT, mips - 1, 1, 0, 1 });
		disp->flushCommandBuffer(cmd, cola, true);
		subida.destroy();
		VkImageViewCreateInfo vi = vks::initializers::imageViewCreateInfo();
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = ii.format;
		vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, 0, 1 };
		vi.image = tex.image;
		VK_CHECK_RESULT(vkCreateImageView(dev, &vi, nullptr, &tex.view));
		tex.device = disp;
		tex.sampler = VK_NULL_HANDLE;
		tex.width = (uint32_t)w;
		tex.height = (uint32_t)h;
		tex.mipLevels = mips;
		tex.layerCount = 1;
		tex.format = ii.format;
		tex.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		return true;
	}

	// el dorso original: dorso.png (400x580) y una copia a la mitad (promedio 2x2) para las cartas del tablero
	void cargarDorso()
	{
		const std::string ruta = rutaAssets + "ui/dorso.png";
		int w, h, n;
		stbi_uc* px = stbi_load(ruta.c_str(), &w, &h, &n, 4);
		if (!px) {
			fprintf(stderr, "[interfaz] falta %s (se usa el dorso dibujado por shader)\n", ruta.c_str());
			return;
		}
		dorso.fromBuffer(px, (VkDeviceSize)w * h * 4, VK_FORMAT_R8G8B8A8_UNORM, w, h, disp, cola, VK_FILTER_LINEAR);
		const int mw = w / 2, mh = h / 2;
		std::vector<stbi_uc> mini((size_t)mw * mh * 4);
		for (int y = 0; y < mh; y++) {
			for (int x = 0; x < mw; x++) {
				for (int c = 0; c < 4; c++) {
					const auto at = [&](int xx, int yy) { return (int)px[((size_t)yy * w + xx) * 4 + c]; };
					mini[((size_t)y * mw + x) * 4 + c] = (stbi_uc)((at(2 * x, 2 * y) + at(2 * x + 1, 2 * y) + at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1) + 2) / 4);
				}
			}
		}
		dorsoMini.fromBuffer(mini.data(), mini.size(), VK_FORMAT_R8G8B8A8_UNORM, mw, mh, disp, cola, VK_FILTER_LINEAR);
		stbi_image_free(px);
		hayDorso = true;
	}

	bool cargarFuente(Fuente& f, const std::string& ruta, std::vector<uint8_t>& px, int& cx, int& cy, int& fila)
	{
		std::ifstream a(ruta, std::ios::binary);
		if (!a) {
			fprintf(stderr, "[interfaz] falta la fuente %s\n", ruta.c_str());
			return false;
		}
		f.datos.assign(std::istreambuf_iterator<char>(a), std::istreambuf_iterator<char>());
		if (!stbtt_InitFont(&f.info, f.datos.data(), stbtt_GetFontOffsetForIndex(f.datos.data(), 0))) {
			return false;
		}
		f.escala = stbtt_ScaleForPixelHeight(&f.info, TAM_BASE);
		int asc, desc, gap;
		stbtt_GetFontVMetrics(&f.info, &asc, &desc, &gap);
		f.ascenso = asc * f.escala;
		std::vector<int> lista;
		for (int c = 32; c < 127; c++) lista.push_back(c);
		for (int c = 160; c < 256; c++) lista.push_back(c);
		for (int c : { 0x2014, 0x2022, 0x2026, 0x2605, 0x25B2, 0x25BC }) lista.push_back(c);
		for (int c : lista) {
			if (c != 32 && stbtt_FindGlyphIndex(&f.info, c) == 0) continue;
			int adv, lsb;
			stbtt_GetCodepointHMetrics(&f.info, c, &adv, &lsb);
			Glifo g{};
			g.avance = adv * f.escala;
			int w = 0, h = 0, xo = 0, yo = 0;
			unsigned char* sdf = stbtt_GetCodepointSDF(&f.info, f.escala, c, RELLENO_SDF, 128, 128.0f / RELLENO_SDF, &w, &h, &xo, &yo);
			if (sdf) {
				if (cx + w + 2 > ATLAS) {
					cx = 1;
					cy += fila + 2;
					fila = 0;
				}
				if (cy + h + 2 <= ATLAS) {
					for (int y = 0; y < h; y++) {
						memcpy(&px[(size_t)(cy + y) * ATLAS + cx], &sdf[y * w], w);
					}
					g.u0 = (float)cx / ATLAS; g.v0 = (float)cy / ATLAS;
					g.u1 = (float)(cx + w) / ATLAS; g.v1 = (float)(cy + h) / ATLAS;
					g.xoff = (float)xo; g.yoff = (float)yo; g.w = (float)w; g.h = (float)h;
					cx += w + 2;
					fila = std::max(fila, h);
				}
				stbtt_FreeSDF(sdf, nullptr);
			}
			f.glifos[c] = g;
		}
		return true;
	}

	void crearAtlas()
	{
		std::vector<uint8_t> px((size_t)ATLAS * ATLAS, 0);
		int cx = 1, cy = 1, fila = 0;
		const std::string d = rutaAssets + "ui/fuentes/";
		cargarFuente(fuentes[F_NUMEROS], d + "ChakraPetch-Bold.ttf", px, cx, cy, fila);
		cargarFuente(fuentes[F_TITULO], d + "Rajdhani-Bold.ttf", px, cx, cy, fila);
		cargarFuente(fuentes[F_TEXTO], d + "Rajdhani-SemiBold.ttf", px, cx, cy, fila);
		atlas.fromBuffer(px.data(), px.size(), VK_FORMAT_R8_UNORM, ATLAS, ATLAS, disp, cola, VK_FILTER_LINEAR);
	}

	VkDescriptorSet nuevoSet(vks::Texture2D& tex)
	{
		VkDescriptorSetAllocateInfo info = vks::initializers::descriptorSetAllocateInfo(pool, &layoutSet, 1);
		VkDescriptorSet set;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(dev, &info, &set));
		VkDescriptorImageInfo img{ muestreador, tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
		VkWriteDescriptorSet w = vks::initializers::writeDescriptorSet(set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &img);
		vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
		return set;
	}

	void crearDescriptores()
	{
		VkSamplerCreateInfo m = vks::initializers::samplerCreateInfo();
		m.magFilter = m.minFilter = VK_FILTER_LINEAR;
		m.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		m.addressModeU = m.addressModeV = m.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		// caras con mipmaps y filtro anisotropo: las cartas chicas e inclinadas del tablero se ven nitidas
		m.maxLod = VK_LOD_CLAMP_NONE;
		m.anisotropyEnable = disp->enabledFeatures.samplerAnisotropy;
		m.maxAnisotropy = m.anisotropyEnable ? std::min(16.0f, disp->properties.limits.maxSamplerAnisotropy) : 1.0f;
		VK_CHECK_RESULT(vkCreateSampler(dev, &m, nullptr, &muestreador));
		const uint32_t total = 1 + 2 * (uint32_t)cartas.size() + 2 + 2 + MAX_HD + MAX_RETRATOS + 7 + 2 * cosmeticos::NUM_FUNDAS + 1 + 1 + 1 + 12;   // (+1 md_efectos.png y +12 pistas de MD: ramas classic-ps5-efectos y efectos2)   // (+1: los sprites de Master Duel, rama classic-md-fases; +1: los de fusion/ritual, rama classic-md-entradas)   // (+ portada y menu: portada_ui.h)   // (+ retratos de personajes)   // (+ caras HD del constructor)   // atlas, caras y minis, reserva, dorso y su mini
		std::vector<VkDescriptorPoolSize> tam = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, total) };
		VkDescriptorPoolCreateInfo pi = vks::initializers::descriptorPoolCreateInfo(tam, total);
		VK_CHECK_RESULT(vkCreateDescriptorPool(dev, &pi, nullptr, &pool));
		const std::vector<VkDescriptorSetLayoutBinding> b = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo li = vks::initializers::descriptorSetLayoutCreateInfo(b);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(dev, &li, nullptr, &layoutSet));
		setAtlas = nuevoSet(atlas);
		if (hayDorso) {
			setDorso = nuevoSet(dorso);
			setDorsoMini = nuevoSet(dorsoMini);
		}
	}

	void crearPipelines(VkFormat formatoColor, VkFormat formatoProf, VkPipelineCache cache, const std::string& shaders)
	{
		VkPushConstantRange empuje = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec4), 0);
		VkPipelineLayoutCreateInfo pl = vks::initializers::pipelineLayoutCreateInfo(&layoutSet, 1);
		pl.pushConstantRangeCount = 1;
		pl.pPushConstantRanges = &empuje;
		VK_CHECK_RESULT(vkCreatePipelineLayout(dev, &pl, nullptr, &layout));

		VkVertexInputBindingDescription enlace{ 0, sizeof(Vertice), VK_VERTEX_INPUT_RATE_VERTEX };
		const std::array<VkVertexInputAttributeDescription, 7> atributos = { {
			{ 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertice, pos) },
			{ 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertice, uv) },
			{ 2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertice, local) },
			{ 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertice, color) },
			{ 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertice, color2) },
			{ 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertice, forma) },
			{ 6, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertice, extra) },
		} };
		VkPipelineVertexInputStateCreateInfo entrada = vks::initializers::pipelineVertexInputStateCreateInfo();
		entrada.vertexBindingDescriptionCount = 1;
		entrada.pVertexBindingDescriptions = &enlace;
		entrada.vertexAttributeDescriptionCount = (uint32_t)atributos.size();
		entrada.pVertexAttributeDescriptions = atributos.data();
		VkPipelineInputAssemblyStateCreateInfo ensamble = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo raster = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		// alfa premultiplicado: lo opaco tapa, los brillos (alfa 0) solo suman luz
		VkPipelineColorBlendAttachmentState mezcla = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_TRUE);
		mezcla.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		mezcla.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		mezcla.colorBlendOp = VK_BLEND_OP_ADD;
		mezcla.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		mezcla.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		mezcla.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo mezclaCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &mezcla);
		VkPipelineDepthStencilStateCreateInfo sinProf = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
		VkPipelineDepthStencilStateCreateInfo conProf = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo vista = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo muestras = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dinamicos = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dinamico = vks::initializers::pipelineDynamicStateCreateInfo(dinamicos);
		VkShaderModule mv = vks::tools::loadShader((shaders + "yugireload/interfaz.vert.spv").c_str(), dev);
		VkShaderModule mf = vks::tools::loadShader((shaders + "yugireload/interfaz.frag.spv").c_str(), dev);
		std::array<VkPipelineShaderStageCreateInfo, 2> etapas{};
		etapas[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, mv, "main", nullptr };
		etapas[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, mf, "main", nullptr };
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
			.pViewportState = &vista,
			.pRasterizationState = &raster,
			.pMultisampleState = &muestras,
			.pDepthStencilState = &sinProf,
			.pColorBlendState = &mezclaCI,
			.pDynamicState = &dinamico,
			.layout = layout,
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &p2d));
		ci.pDepthStencilState = &conProf;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(dev, cache, 1, &ci, nullptr, &p3d));
		vkDestroyShaderModule(dev, mv, nullptr);
		vkDestroyShaderModule(dev, mf, nullptr);
	}

	// ------------------------------------------------------------------------------------------ primitivas
	void usar(VkDescriptorSet set, bool en3D)
	{
		if (lotes.empty() || lotes.back().set != set || lotes.back().en3D != en3D) {
			lotes.push_back({ set, en3D, (uint32_t)v.size() });
		}
	}

	// el dorso de una carta boca abajo: elige su textura (mini = la mitad, para el tablero) y devuelve los colores y el
	// "extra" del quad. Con la imagen original: col = tinte (brillo) de la imagen, col2 = borde de luz del lado
	// (cian jugador, carmesi rival), extra.w = 1. Sin ella: el dorso por shader de antes (acento, dorado, fase)
	struct Dorso { glm::vec4 col, col2, extra; };
	Dorso usarDorso(bool en3D, bool mini, bool rival, float brillo, float fase)
	{
		const glm::vec4 lado = rival ? CARMESI : CIAN;
		if (hayDorso) {
			usar(rival ? (mini ? setDorsoMini : setDorso) : setDorsoJugador(mini), en3D);   // (rama classic-extras: la funda del jugador)
			return { glm::vec4(glm::vec3(brillo), 1.0f), glm::vec4(glm::vec3(lado) * 0.55f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) };
		}
		usar(setAtlas, en3D);
		return { glm::vec4(glm::vec3(lado) * brillo, 1.0f), ORO, glm::vec4(fase, 0.0f, 0.0f, 0.0f) };
	}
	// medio texel hacia dentro (como las caras): sin franjas del borde de la imagen
	glm::vec2 uvDorso0(bool mini) const
	{
		const vks::Texture2D& d = mini ? dorsoMini : dorso;
		return hayDorso ? glm::vec2(0.5f / d.width, 0.5f / d.height) : glm::vec2(0.0f);
	}
	glm::vec2 uvDorso1(bool mini) const { return glm::vec2(1.0f) - uvDorso0(mini); }

	glm::vec4 recorte(glm::vec2 px) const { return glm::vec4(px.x / W * 2.0f - 1.0f, px.y / H * 2.0f - 1.0f, 0.0f, 1.0f); }
	glm::vec4 proyectar(glm::vec3 p) const { return vp * glm::vec4(p, 1.0f); }
	glm::vec2 aPantalla(glm::vec4 c) const
	{
		const float w = glm::max(c.w, 1e-4f);
		return glm::vec2((c.x / w * 0.5f + 0.5f) * W, (c.y / w * 0.5f + 0.5f) * H);
	}

	// un quad: esquinas 0 arriba-izq, 1 arriba-der, 2 abajo-der, 3 abajo-izq
	void quad(const glm::vec4 c[4], const glm::vec2 l[4], const glm::vec2 uv[4], glm::vec4 col, glm::vec4 col2, glm::vec4 forma, glm::vec4 extra)
	{
		if (v.size() + 6 > MAX_VERTICES) {
			return;
		}
		static const int orden[6] = { 0, 1, 2, 0, 2, 3 };
		for (int i : orden) {
			v.push_back({ c[i], uv[i], l[i], col, col2, forma, extra });
		}
	}

	// rectangulo en pantalla (px), girado "ang" alrededor de su centro; margen = cuanto sobra para brillos/sombra
	void rect(glm::vec2 centro, glm::vec2 tam, float ang, float margen, Tipo tipo, glm::vec4 col, glm::vec4 col2, float radio,
	          glm::vec4 extra, glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f))
	{
		const glm::vec2 m = tam * 0.5f + glm::vec2(margen);
		const glm::vec2 l[4] = { { -m.x, -m.y }, { m.x, -m.y }, { m.x, m.y }, { -m.x, m.y } };
		const float ca = cosf(ang), sa = sinf(ang);
		glm::vec4 c[4];
		glm::vec2 uv[4];
		for (int i = 0; i < 4; i++) {
			const glm::vec2 q{ l[i].x * ca - l[i].y * sa, l[i].x * sa + l[i].y * ca };
			c[i] = recorte(centro + q);
			const glm::vec2 f = (l[i] + tam * 0.5f) / glm::max(tam, glm::vec2(1e-3f));
			uv[i] = glm::mix(uv0, uv1, f);
		}
		quad(c, l, uv, col, col2, glm::vec4(tam * 0.5f, radio, (float)tipo), extra);
	}

	void caja(float x, float y, float w, float h, float margen, Tipo tipo, glm::vec4 col, glm::vec4 col2, float radio, glm::vec4 extra)
	{
		rect(glm::vec2(x + w * 0.5f, y + h * 0.5f), glm::vec2(w, h), 0.0f, margen, tipo, col, col2, radio, extra);
	}

	// rectangulo con inclinacion 3D (perspectiva real: w varia por esquina)
	void rectInclinado(glm::vec2 centro, glm::vec2 tam, float angY, float angX, float margen, Tipo tipo, glm::vec4 col, glm::vec4 col2,
	                   float radio, glm::vec4 extra, glm::vec2 uv0 = glm::vec2(0.0f), glm::vec2 uv1 = glm::vec2(1.0f))
	{
		const glm::vec2 m = tam * 0.5f + glm::vec2(margen);
		const glm::vec2 l[4] = { { -m.x, -m.y }, { m.x, -m.y }, { m.x, m.y }, { -m.x, m.y } };
		const float f = 1600.0f * s;
		const glm::mat3 giro = glm::mat3(glm::rotate(glm::mat4(1.0f), angY, glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1.0f), angX, glm::vec3(1, 0, 0)));
		glm::vec4 c[4];
		glm::vec2 uv[4];
		for (int i = 0; i < 4; i++) {
			const glm::vec3 q = giro * glm::vec3(l[i], 0.0f);
			const float w = (f + q.z) / f;
			const glm::vec4 r = recorte(centro + glm::vec2(q) / w);
			c[i] = glm::vec4(r.x * w, r.y * w, 0.0f, w);
			uv[i] = glm::mix(uv0, uv1, (l[i] + tam * 0.5f) / glm::max(tam, glm::vec2(1e-3f)));
		}
		quad(c, l, uv, col, col2, glm::vec4(tam * 0.5f, radio, (float)tipo), extra);
	}

	// rectangulo tendido sobre el tablero (y = altura), local en cm
	// (girar: la imagen 180 grados, para las cartas del rival)
	void rectSuelo(glm::vec3 centro, glm::vec2 tam, float margen, Tipo tipo, glm::vec4 col, glm::vec4 col2, float radio, glm::vec4 extra,
	               bool girar = false)
	{
		const glm::vec2 m = tam * 0.5f + glm::vec2(margen);
		const glm::vec2 l[4] = { { -m.x, -m.y }, { m.x, -m.y }, { m.x, m.y }, { -m.x, m.y } };
		glm::vec4 c[4];
		glm::vec2 uv[4];
		glm::vec2 lc[4];
		for (int i = 0; i < 4; i++) {
			c[i] = proyectar(centro + glm::vec3(l[i].x, 0.0f, l[i].y));
			uv[i] = (l[i] + tam * 0.5f) / glm::max(tam, glm::vec2(1e-3f));
			if (girar) uv[i] = glm::vec2(1.0f) - uv[i];
			lc[i] = l[i] * 100.0f;
		}
		quad(c, lc, uv, col, col2, glm::vec4(tam * 50.0f, radio, (float)tipo), extra);
	}

	// rama classic-idioma: texto()/medir()/parrafo() pasan todo por idioma::tr (salvo dentro de un parrafo ya traducido)
	bool crudo{ false };
	const std::string& trd(const std::string& t) const { return crudo ? t : idioma::tr(t); }

	float medir(int fu, const std::string& txt, float tam, float espaciado = 0.0f) const
	{
		const Fuente& f = fuentes[fu];
		const float k = tam / TAM_BASE;
		float x = 0.0f;
		const std::vector<int> cps = utf8(trd(txt));
		for (size_t i = 0; i < cps.size(); i++) {
			auto g = f.glifos.find(cps[i]);
			if (g == f.glifos.end()) continue;
			x += g->second.avance * k + espaciado * s;
			if (i + 1 < cps.size()) {
				x += stbtt_GetCodepointKernAdvance(&f.info, cps[i], cps[i + 1]) * f.escala * k;
			}
		}
		return x;
	}

	// texto con la base en y; alin 0 izq, 1 centro, 2 der; extra: halo, contorno, degradado
#if defined(YUGI_REGISTRO_DIBUJO)
	// rama classic-jugador-humano: SOLO en la prueba del jugador humano (prueba_humano.cpp define YUGI_REGISTRO_DIBUJO):
	// anota cada texto y cada icono de boton que se dibuja (con su lugar, tamanno y alfa). El juego no lo compila
	std::function<void(const std::string& txt, float x, float y, float tam, glm::vec4 col, int alin, size_t vertice)> alDibujarTexto;
#endif
	float texto(int fu, const std::string& txt, float x, float y, float tam, glm::vec4 col, glm::vec4 col2 = NADA,
	            glm::vec3 ex = glm::vec3(0.0f), int alin = 0, float espaciado = 0.0f)
	{
#if defined(YUGI_REGISTRO_DIBUJO)
		if (alDibujarTexto) alDibujarTexto(trd(txt), x, y, tam, col, alin, v.size());
#endif
		usar(setAtlas, false);
		const Fuente& f = fuentes[fu];
		const float k = tam / TAM_BASE;
		const float ancho = medir(fu, txt, tam, espaciado);
		if (alin == 1) x -= ancho * 0.5f;
		if (alin == 2) x -= ancho;
		const std::vector<int> cps = utf8(trd(txt));
		const float arriba = y - tam * 0.72f;
		for (size_t i = 0; i < cps.size(); i++) {
			auto it = f.glifos.find(cps[i]);
			if (it == f.glifos.end()) continue;
			const Glifo& g = it->second;
			if (g.w > 0.0f) {
				const float x0 = x + g.xoff * k, y0 = y + g.yoff * k, x1 = x0 + g.w * k, y1 = y0 + g.h * k;
				auto ly = [&](float py) { return (py - arriba) / (tam * 0.72f) * 2.0f - 1.0f; };
				const glm::vec4 c[4] = { recorte({ x0, y0 }), recorte({ x1, y0 }), recorte({ x1, y1 }), recorte({ x0, y1 }) };
				const glm::vec2 l[4] = { { 0, ly(y0) }, { 0, ly(y0) }, { 0, ly(y1) }, { 0, ly(y1) } };
				const glm::vec2 uv[4] = { { g.u0, g.v0 }, { g.u1, g.v0 }, { g.u1, g.v1 }, { g.u0, g.v1 } };
				quad(c, l, uv, col, col2, glm::vec4(0, 0, 0, (float)TEXTO), glm::vec4(ex, 0.0f));
			}
			x += g.avance * k + espaciado * s;
			if (i + 1 < cps.size()) {
				x += stbtt_GetCodepointKernAdvance(&f.info, cps[i], cps[i + 1]) * f.escala * k;
			}
		}
		return ancho;
	}

	// parrafo con cortes de linea por palabras; devuelve cuantas lineas uso
	int parrafo(int fu, const std::string& original, float x, float y, float tam, float ancho, float interlinea, int maxLineas, glm::vec4 col)
	{
		// rama classic-idioma: se traduce el parrafo entero; sus pedazos (lineas) ya no
		const std::string txt = trd(original);
		struct Crudo { bool& b; bool antes; Crudo(bool& x) : b(x), antes(x) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
		std::vector<std::string> palabras;
		size_t a = 0, b;
		while ((b = txt.find(' ', a)) != std::string::npos) {
			palabras.push_back(txt.substr(a, b - a));
			a = b + 1;
		}
		palabras.push_back(txt.substr(a));
		std::string linea;
		int n = 0;
		for (size_t i = 0; i <= palabras.size(); i++) {
			const std::string prueba = i < palabras.size() ? (linea.empty() ? palabras[i] : linea + " " + palabras[i]) : linea;
			if (i == palabras.size() || (medir(fu, prueba, tam) > ancho && !linea.empty())) {
				if (n == maxLineas - 1 && i < palabras.size()) {
					linea += "…";
					texto(fu, linea, x, y + n * interlinea, tam, col);
					return n + 1;
				}
				texto(fu, linea, x, y + n * interlinea, tam, col);
				n++;
				linea = i < palabras.size() ? palabras[i] : "";
			} else {
				linea = prueba;
			}
		}
		return n;
	}

	// ------------------------------------------------------------------------------------------ puntero
	const Zona* zonaPorId(const std::string& id) const
	{
		for (const Zona& z : ZONAS) {
			if (id == z.id) return &z;
		}
		return nullptr;
	}

	glm::vec3 centroZona(int i) const { return glm::vec3(ZONAS[i].x * ESCALA, 0.0f, ZONAS[i].z * ESCALA); }

	// mano del jugador: centro, giro y escala de la carta k
	void cartaManoJ(int k, glm::vec2& c, float& ang, float& esc) const
	{
		const float cx = (CAMPO_X * s + W) * 0.5f;
		const float m = (nManoJ() - 1) * 0.5f;
		const float d = (float)k - m;
		const float e = elevaJ[k];
		c = glm::vec2(cx + d * 128.0f * s, H - (52.0f + 34.0f * focoJ) * s + d * d * 4.0f * s - e * 70.0f * s);
		ang = glm::radians(d * 3.2f) * (1.0f - e);
		esc = 1.0f + 0.14f * e;
	}

	void cartaManoR(int k, glm::vec2& c, float& ang, float& esc) const
	{
		const float cx = (CAMPO_X * s + W) * 0.5f;
		const float m = (float)(nManoR() - 1) * 0.5f;
		const float d = (float)k - m;
		const float e = elevaR[k];
		c = glm::vec2(cx - d * 100.0f * s, (14.0f + 26.0f * focoR) * s - d * d * 3.0f * s + e * 40.0f * s);
		ang = glm::radians(180.0f - d * 3.0f) ;
		esc = 1.0f + 0.1f * e;
	}

	std::string idObjetivo(const Objetivo& o) const
	{
		if (o.clase == MANO_J) return "jugador_mano_" + std::to_string(o.indice);
		if (o.clase == MANO_R) return "rival_mano_" + std::to_string(o.indice);
		return ZONAS[o.indice].id;
	}

	void construirObjetivos()
	{
		objetivos.clear();
		for (int k = 0; k < nManoJ(); k++) {
			glm::vec2 c; float a, e;
			cartaManoJ(k, c, a, e);
			objetivos.push_back({ MANO_J, k, glm::vec2(c.x, H - 60.0f * s) });
		}
		for (int k = 0; k < nManoR(); k++) {
			glm::vec2 c; float a, e;
			cartaManoR(k, c, a, e);
			objetivos.push_back({ MANO_R, k, glm::vec2(c.x, 40.0f * s) });
		}
		for (int i = 0; i < (int)NUM_ZONAS; i++) {
			objetivos.push_back({ EN_ZONA, i, aPantalla(proyectar(centroZona(i))) });
		}
		seleccion = 0;
		for (size_t i = 0; i < objetivos.size(); i++) {
			if (idObjetivo(objetivos[i]) == pedido) {
				seleccion = (int)i;
			}
		}
	}

	// navegacion espacial: el objetivo mas cercano en la direccion pedida (penaliza el desvio)
	void mover(glm::vec2 dir)
	{
		if (objetivos.empty()) return;
		const glm::vec2 p = objetivos[seleccion].pantalla;
		float mejor = 1e9f;
		int elegido = -1;
		for (size_t i = 0; i < objetivos.size(); i++) {
			if ((int)i == seleccion) continue;
			const glm::vec2 d = objetivos[i].pantalla - p;
			const float avance = glm::dot(d, dir);
			if (avance < 12.0f * s) continue;
			const float desvio = fabsf(d.x * dir.y - d.y * dir.x);
			const float puntos = avance + desvio * 2.2f;
			if (puntos < mejor) {
				mejor = puntos;
				elegido = (int)i;
			}
		}
		if (elegido >= 0) {
			pedido = idObjetivo(objetivos[elegido]);
			seleccion = elegido;
		}
	}

	// ------------------------------------------------------------------------------------------ animacion
	void animarVidas(float dt)
	{
		for (Vida* vi : { &vidaJ, &vidaR }) {
			vi->tGolpe += dt;
			vi->tCambio += dt;
			// rama classic-md-fases: como DuelLP.TransitionLPStep de Master Duel: el valor que vuela termina (LPCounterSub,
			// 1.2 s) y recien ahi el numero cuenta en linea recta durante countTime (1 s) hasta el valor nuevo
			const float t0 = inicioCuentaLP(), dur = duracionCuentaLP();
			const float k = glm::clamp((vi->tCambio - t0) / dur, 0.0f, 1.0f);
			vi->mostrado = vi->tCambio < t0 ? vi->desde : vi->desde + ((float)vi->valor - vi->desde) * k;
			// la estela del danno espera 0.7 s y se vacia
			if (vi->tGolpe > 0.7f) {
				vi->fantasma += (vi->mostrado - vi->fantasma) * (1.0f - expf(-dt * 2.6f));
			}
			if (vi->fantasma < vi->mostrado) vi->fantasma = vi->mostrado;
		}
	}

	void chispa(glm::vec2 pos, glm::vec2 vel, float dur, float tam, glm::vec3 col, float cruz = 0.0f)
	{
		if (chispas.size() < 900) {
			chispas.push_back({ pos, vel, 0.0f, dur, tam, col, cruz });
		}
	}

	void animarChispas(float dt)
	{
		for (Chispa& c : chispas) {
			c.vida += dt;
			c.pos += c.vel * dt;
			c.vel *= expf(-dt * 1.6f);
			c.vel.y -= 18.0f * dt * s;
		}
		chispas.erase(std::remove_if(chispas.begin(), chispas.end(), [](const Chispa& c) { return c.vida >= c.dur; }), chispas.end());
	}

	void estallar(bool rival)
	{
		const glm::vec2 c(COL_X * s + 150.0f * s, (rival ? 80.0f : 1004.0f) * s);
		const glm::vec3 col = rival ? glm::vec3(1.0f, 0.4f, 0.35f) : glm::vec3(1.0f, 0.5f, 0.3f);
		for (int i = 0; i < 70; i++) {
			const float a = azar() * 6.2832f;
			const float r = (60.0f + 260.0f * azar()) * s;
			chispa(c + glm::vec2(azar() - 0.5f, azar() - 0.5f) * glm::vec2(200.0f, 40.0f) * s, glm::vec2(cosf(a), sinf(a) * 0.6f) * r,
			       0.5f + 0.8f * azar(), (2.0f + 4.0f * azar()) * s, glm::mix(col, glm::vec3(1.0f, 0.9f, 0.6f), azar()), azar() > 0.7f ? 1.0f : 0.0f);
		}
	}

	// ------------------------------------------------------------------------------------------ armar el cuadro
	void construir()
	{
		if (gal && gal->abierta) {
			dibujarGaleria();   // rama classic-galeria: la figura 3D va detras (galeria_juego.h)
			dibujarAvisoGuardado();
			return;
		}
		if (cons && cons->abierto) {
			dibujarConstructor();   // rama classic-constructor: tapa todo el duelo
			dibujarAvisoGuardado();
			return;
		}
		dibujarTablero3D();
		dibujarCartasMundo();
		registrarZonasMesa();    // rama classic-pc: las zonas del tablero como zonas del raton
		dibujarJugablesMesa();   // rama classic-duelo-ui: el brillo azul de lo que se puede jugar (duelo_consulta_ui.h)
		dibujarEfectosMD();      // rama classic-ps5-efectos: los CardRunEffect de Master Duel (md_efectos_ui.h)
		dibujarEtiquetasTablero();
		dibujarAtkDef();
		dibujarCardRunMD(true);  // rama classic-ps5-efectos: los CardRunEffect de pantalla de MD, encima de los rotulos de la mesa
		dibujarManoRival();
		dibujarManoJugador();
		dibujarColumna();
		dibujarFases();
		dibujarSelectorCamara();
		dibujarMarco();
		dibujarChispas();
		dibujarCadenaPila();     // rama classic-duelo-ui: la pila de la cadena (C1, C2...)
		dibujarMandoEncima();   // rama mando: carta que vuela, menu, fase, avisos, detalle y barra de ayuda
		dibujarEntradaClave();   // rama classic-cartas-clave: el corte 2D de la entrada de una carta clave
		dibujarPantallaDuelo();  // rama classic-integracion: elegir rival, turno del rival, resultado
		dibujarConsulta();       // rama classic-duelo-ui: registro y visores de zonas (L3 / X en la zona)
		dibujarMenuPrincipal();  // rama classic-menus: menu principal (tapa la mesa)
		dibujarEnLinea();        // rama classic-en-linea: DUELO EN LINEA (menus, relojes, conexion, rival encontrado)
		dibujarPersonalizacion();   // rama classic-extras: Opciones > Personalizacion (cosmeticos_ui.h)
		dibujarPortada();        // rama classic-portada: la portada al arrancar (tapa todo) y el menu desde negro
		dibujarEscenaHistoria();   // rama classic-historia: dialogo antes del duelo, final, linea de cierre y atajos
		dibujarTutorial();         // rama classic-historia: las reglas del duelo
		dibujarPausa();          // rama classic-arreglos: el menu de pausa (OPTIONS corto), encima de todo
		dibujarAvisoGuardado();  // rama classic-pulido: no se pudo guardar / sin carpeta de datos (en cualquier pantalla)
	}

	// rama classic-pulido: aviso de guardado (guardado.h) arriba al centro, en cualquier pantalla, unos segundos: si
	// /data/yugioh_classic no se pudo crear o si fallo la escritura de un archivo
	int serieAvisoGuardado{ 0 };
	float tAvisoGuardado{ 99.0f };
	void dibujarAvisoGuardado()
	{
		const guardado::Estado& e = guardado::estado();
		if (e.serie != serieAvisoGuardado) {
			serieAvisoGuardado = e.serie;
			tAvisoGuardado = 0.0f;
		}
		tAvisoGuardado += dtCuadro;
		const float dura = 7.0f;
		if (e.serie == 0 || tAvisoGuardado > dura) return;
		const float a = suave(0.0f, 0.25f, tAvisoGuardado) * (1.0f - suave(dura - 0.6f, dura, tAvisoGuardado));
		if (a < 0.01f) return;
		const std::string msg = e.ultimoFallo ? std::string(idioma::tr("No se pudo guardar")) + " " + e.archivo
		                                      : std::string(idioma::tr("No se pudo crear /data/yugioh_classic: el progreso podría no guardarse"));
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float tt = 24.0f * s;
		const float tw = medir(F_TITULO, msg, tt, 1.0f);
		const float w = tw + 80.0f * s, h = 52.0f * s;
		const float x = (W - w) * 0.5f, y = 28.0f * s;
		usar(setAtlas, false);
		caja(x, y, w, h, 18.0f * s, PANEL, A(glm::vec4(0.16f, 0.03f, 0.04f, 0.92f)), A(CARMESI), h * 0.5f, glm::vec4(1.4f * s, 1.0f, 0.2f, 14.0f * s));
		rect(glm::vec2(x + 28.0f * s, y + h * 0.5f), glm::vec2(15.0f * s), 0.0f, 8.0f * s, ORBE, A(CARMESI), A(glm::vec4(0.5f, 0.05f, 0.05f, 1.0f)), 0.0f,
		     glm::vec4(0.9f, 1.0f, 0.0f, 0.0f));
		texto(F_TITULO, msg, x + 50.0f * s, y + h * 0.5f + 8.0f * s, tt, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
	}

	// lo que hay en una zona del duelo de prueba
	const Ocupa* ocupante(const char* zona) const
	{
		if (campoPropio && strstr(zona, "_monstruo_")) {   // (ataque por casillas: manda el juego)
			for (const Ocupa& o : ocupantesMonstruo) {
				if (o.zona && strcmp(o.zona, zona) == 0) return o.carta[0] ? &o : nullptr;
			}
		}
		if (ctl) {
			return ocupanteMando(zona);
		}
		for (const Ocupa& o : CARTAS_DEMO) {
			if (strcmp(o.zona, zona) == 0) return &o;
		}
		for (const Ocupa& o : CAMPO_PRUEBA) {
			if (strcmp(o.zona, zona) == 0) return &o;
		}
		return nullptr;
	}

	glm::vec4 colorZona(const std::string& tipo) const
	{
		if (tipo == "fusion") return glm::vec4(0.72f, 0.42f, 1.0f, 1.0f);
		if (tipo == "cementerio") return glm::vec4(0.62f, 0.75f, 0.95f, 1.0f);
		if (tipo == "mazo") return CIAN;
		if (tipo == "desterrado") return glm::vec4(1.0f, 0.62f, 0.30f, 1.0f);   // rama classic-duelo-ui
		return glm::vec4(0.45f, 1.0f, 0.75f, 1.0f);
	}

	void dibujarTablero3D()
	{
		usar(setAtlas, true);
		// fondo: un quad a toda pantalla en la profundidad maxima (z = w): solo pinta donde la escena no dibujo
		{
			const glm::vec2 m(W * 0.5f, H * 0.5f);
			const glm::vec4 c[4] = { { -1, -1, 1, 1 }, { 1, -1, 1, 1 }, { 1, 1, 1, 1 }, { -1, 1, 1, 1 } };
			const glm::vec2 l[4] = { { -m.x, -m.y }, { m.x, -m.y }, { m.x, m.y }, { -m.x, m.y } };
			const glm::vec2 uv[4] = {};
			quad(c, l, uv, NADA, NADA, glm::vec4(m, 0.0f, (float)FONDO), glm::vec4(fondoBosque, 0.0f, 0.0f, 0.0f));
		}
		// marcas animadas de las zonas especiales (fusion, cementerio, mazo, campo)
		for (int i = 0; i < (int)NUM_ZONAS; i++) {
			const std::string tipo = ZONAS[i].tipo;
			if (tipo == "monstruo" || tipo == "magia") continue;
			const glm::vec4 c = colorZona(tipo);
			rectSuelo(centroZona(i) + glm::vec3(0.0f, 0.02f, 0.0f), glm::vec2(ZONA_ANCHO, ZONA_ALTO) * 1.04f, 0.25f, ZONA, c,
			          glm::vec4(glm::vec3(c) * 0.25f, 1.0f), 10.0f, glm::vec4(6.0f, tipo == "mazo" ? 0.6f : 0.35f, 0.85f, 0.0f));
		}
		// pilas de los mazos: cantos apilados y el dorso original arriba (el del rival girado 180 grados)
		for (int lado = 0; lado < 2; lado++) {
			const Zona* z = zonaPorId(lado ? "rival_mazo" : "jugador_mazo");
			const int n = lado ? mazoR : mazoJ;
			const glm::vec3 c(z->x * ESCALA, 0.0f, z->z * ESCALA);
			const float alto = 0.016f * n;
			const glm::vec2 m(CARTA_ANCHO * 0.5f, CARTA_ALTO * 0.5f);
			const glm::vec3 esq[4] = { c + glm::vec3(-m.x, 0, -m.y), c + glm::vec3(m.x, 0, -m.y), c + glm::vec3(m.x, 0, m.y), c + glm::vec3(-m.x, 0, m.y) };
			const glm::vec4 canto(0.82f, 0.86f, 0.95f, 1.0f), sombra(0.12f, 0.14f, 0.22f, 1.0f);
			for (int k = 0; k < 4; k++) {
				const glm::vec3 a = esq[k], b = esq[(k + 1) % 4];
				const glm::vec4 cc[4] = { proyectar(a + glm::vec3(0, alto, 0)), proyectar(b + glm::vec3(0, alto, 0)), proyectar(b), proyectar(a) };
				const glm::vec2 l[4] = { {}, {}, {}, {} };
				const glm::vec2 uv[4] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } };
				quad(cc, l, uv, canto, sombra, glm::vec4(0, 0, 0, (float)LADO_MAZO), glm::vec4((float)n * 0.5f, 0, 0, 0));
			}
			const Dorso dz = usarDorso(true, true, lado != 0, 0.9f, lado * 2.0f);
			rectSuelo(c + glm::vec3(0.0f, alto + 0.002f, 0.0f), glm::vec2(CARTA_ANCHO, CARTA_ALTO), 0.0f, DORSO, dz.col, dz.col2, 6.0f, dz.extra, lado != 0);
			usar(setAtlas, true);
		}
		dibujarCartasDemo();
		// el recuadro dorado sobre el tablero (con profundidad: los monstruos lo tapan)
		if (marcoEn3D && llegada > 0.9f) {
			marcoQuad(true);
		}
	}

	// las cartas sueltas de cartaEnMundo: cara (con su foil) o el dorso original, en el plano que digan sus ejes
	void dibujarCartasMundo()
	{
		for (const CartaMundo& k : cartasMundo) {
			const float mx = glm::length(k.ancho) * 100.0f, my = glm::length(k.alto) * 100.0f;   // en cm, como rectSuelo
			const glm::vec2 l[4] = { { -mx, -my }, { mx, -my }, { mx, my }, { -mx, my } };
			const glm::vec2 uv[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
			glm::vec4 c[4];
			for (int i = 0; i < 4; i++) {
				c[i] = proyectar(k.centro + k.ancho * (l[i].x / mx) + k.alto * (l[i].y / my));
			}
			const int n = k.cara ? buscarCarta(k.id) : -1;
			if (n >= 0 && listaCarta(n)) {
				usar(cartas[n].setCara, true);
				quad(c, l, uv, glm::vec4(glm::vec3(k.brillo), 1.0f), CIAN, glm::vec4(mx, my, 6.0f, (float)IMAGEN), glm::vec4(0.45f, 0.8f, (float)n * 0.13f, 0.0f));
			} else {
				const Dorso dz = usarDorso(true, false, k.rival, k.brillo, k.rival ? 2.0f : 0.0f);
				glm::vec2 uvd[4];
				for (int i = 0; i < 4; i++) uvd[i] = glm::mix(uvDorso0(false), uvDorso1(false), uv[i]);
				quad(c, l, uvd, dz.col, dz.col2, glm::vec4(mx, my, 6.0f, (float)DORSO), dz.extra);
			}
		}
		cartasMundo.clear();
		// pedazos de carta: la cara con su foil; local = el lugar en la carta entera (esquinas redondeadas donde toca)
		const float fx = CARTA_ANCHO * 50.0f, fy = CARTA_ALTO * 50.0f;
		for (const FragmentoMundo& f : fragmentosMundo) {
			const int n = buscarCarta(f.id);
			if (n < 0 || !listaCarta(n)) continue;
			glm::vec4 c[4];
			glm::vec2 l[4];
			for (int i = 0; i < 4; i++) {
				c[i] = proyectar(f.esquinas[i]);
				l[i] = (f.uv[i] * 2.0f - 1.0f) * glm::vec2(fx, fy);
			}
			usar(cartas[n].setCara, true);
			quad(c, l, f.uv, glm::vec4(glm::vec3(f.brillo), 1.0f), glm::vec4(1.0f, 0.85f, 0.4f, 1.0f), glm::vec4(fx, fy, 6.0f, (float)IMAGEN),
			     glm::vec4(0.45f, 0.8f, (float)n * 0.13f, 0.0f));
		}
		fragmentosMundo.clear();
		usar(setAtlas, true);
	}

	// una carta tendida sobre el tablero en c (centro de su cara de arriba): la cara (mini) o el dorso original. La
	// imagen va con su parte de arriba hacia el fondo del duenno: hacia el rival en el jugador, girada 180 grados
	// en el rival (uv invertidas). La cara se tinta un poco para no brillar mas que la mesa iluminada
	void cartaSuelo(glm::vec3 c, bool rival, int ci, float fase)
	{
		const glm::vec2 m(CARTA_ANCHO * 0.5f, CARTA_ALTO * 0.5f);
		const glm::vec2 l[4] = { { -m.x, -m.y }, { m.x, -m.y }, { m.x, m.y }, { -m.x, m.y } };
		glm::vec4 cc[4];
		glm::vec2 uv[4], lc[4];
		for (int i = 0; i < 4; i++) {
			cc[i] = proyectar(c + glm::vec3(l[i].x, 0.0f, l[i].y));
			const glm::vec2 u = (l[i] + m) / (m * 2.0f);
			uv[i] = rival ? glm::vec2(1.0f) - u : u;
			lc[i] = l[i] * 100.0f;
		}
		const glm::vec4 forma(m * 100.0f, 5.0f, (float)(ci >= 0 ? IMAGEN : DORSO));
		if (ci >= 0) {
			const glm::vec4 borde = rival ? CARMESI : CIAN;
			usar(cartas[ci].setCara, true);
			quad(cc, lc, uv, glm::vec4(0.88f, 0.88f, 0.9f, 1.0f), borde * 0.35f, forma, glm::vec4(0.10f, 0.22f, fase, 0.0f));
		} else {
			const Dorso dz = usarDorso(true, true, rival, 0.9f, fase * 2.0f);
			for (int i = 0; i < 4; i++) uv[i] = glm::mix(uvDorso0(true), uvDorso1(true), uv[i]);
			quad(cc, lc, uv, dz.col, dz.col2, forma, dz.extra);
		}
	}

	// las cartas de CARTAS_DEMO: magias/trampas planas en su casilla y las pilas de fusion (cantos + la de arriba)
	void dibujarCartasDemo()
	{
		const float GROSOR = 0.016f;   // el mismo de los mazos
		int k = 0;
		if (ctl) {
			dibujarMesaMando();   // rama mando: monstruos, magias/trampas, campo y cementerios de control.h
		}
		for (const Ocupa& o : CARTAS_DEMO) {
			const Zona* z = zonaPorId(o.zona);
			if (!z) continue;
			const bool rival = strncmp(o.zona, "rival", 5) == 0;
			const bool fusion = strcmp(z->tipo, "fusion") == 0;
			if (ctl && !fusion) continue;   // (con el mando solo las pilas de fusion salen de esta tabla)
			const glm::vec3 c(z->x * ESCALA, 0.0f, z->z * ESCALA);
			// 4.5 cm: encima de las cartas que trae el tablero de Blender (3.7 cm), sin pelear con su profundidad
			float alto = 0.045f;
			if (fusion) {
				const int n = rival ? PILA_FUSION_R : pilaFusionJ;
				alto = GROSOR * n;
				const glm::vec2 m(CARTA_ANCHO * 0.5f, CARTA_ALTO * 0.5f);
				const glm::vec3 esq[4] = { c + glm::vec3(-m.x, 0, -m.y), c + glm::vec3(m.x, 0, -m.y), c + glm::vec3(m.x, 0, m.y), c + glm::vec3(-m.x, 0, m.y) };
				usar(setAtlas, true);
				for (int e = 0; e < 4; e++) {
					const glm::vec3 a = esq[e], b = esq[(e + 1) % 4];
					const glm::vec4 cc[4] = { proyectar(a + glm::vec3(0, alto, 0)), proyectar(b + glm::vec3(0, alto, 0)), proyectar(b), proyectar(a) };
					const glm::vec2 l[4] = { {}, {}, {}, {} };
					const glm::vec2 uv[4] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 0, 0 } };
					quad(cc, l, uv, glm::vec4(0.78f, 0.66f, 0.95f, 1.0f), glm::vec4(0.14f, 0.10f, 0.24f, 1.0f),
					     glm::vec4(0, 0, 0, (float)LADO_MAZO), glm::vec4((float)n * 0.5f, 0, 0, 0));
				}
				alto += 0.002f;
			}
			int ci = (o.bocaAbajo || !o.carta[0]) ? -1 : buscarCarta(o.carta);
			if (ci >= 0 && !listaCarta(ci)) ci = -1;
			cartaSuelo(c + glm::vec3(0.0f, alto, 0.0f), rival, ci, (float)(k++) * 0.37f);
		}
	}

	// etiqueta flotante de una zona: una pastilla de vidrio con el nombre y el numero en una sola linea
	// (rama classic-visual-final) puestas: las ya dibujadas en este cuadro (x0, y0, x1, y1): la etiqueta queda entera
	// dentro de la pantalla (a la derecha de los paneles) y, si pisa a otra, sube una fila
	void etiqueta(glm::vec2 p, const std::string& titulo, const std::string& numero, glm::vec4 col, std::vector<glm::vec4>* puestas = nullptr)
	{
		const float tt = 14.0f * s, tn = 20.0f * s;
		const float wt = medir(F_TITULO, titulo, tt, 2.0f), wn = medir(F_NUMEROS, numero, tn);
		const float w = wt + wn + 34.0f * s, h = 30.0f * s;
		float x = p.x - w * 0.5f, y = p.y - h;
		if (puestas) {
			if (x < W && x + w > CAMPO_X * s) x = glm::clamp(x, CAMPO_X * s + 8.0f * s, W - 10.0f * s - w);   // (solo las que asoman)
			for (int vuelta = 0; vuelta < 4; vuelta++) {
				bool pisa = false;
				for (const glm::vec4& o : *puestas) pisa = pisa || (x < o.z + 4.0f * s && x + w > o.x - 4.0f * s && y < o.w + 3.0f * s && y + h > o.y - 3.0f * s);
				if (!pisa) break;
				y -= h + 6.0f * s;
			}
			puestas->push_back(glm::vec4(x, y, x + w, y + h));
		}
		caja(x, y, w, h, 10.0f * s, PANEL, glm::vec4(0.02f, 0.03f, 0.06f, 0.7f), col, 15.0f * s, glm::vec4(1.2f * s, 0.8f, 0.0f, 8.0f * s));
		texto(F_TITULO, titulo, x + 13.0f * s, y + 20.0f * s, tt, glm::vec4(glm::vec3(col), 1.0f), NADA, glm::vec3(0.0f), 0, 2.0f);
		texto(F_NUMEROS, numero, x + w - 12.0f * s, y + 22.0f * s, tn, BLANCO, col, glm::vec3(0.5f, 0.0f, 0.0f), 2);
	}

	void dibujarEtiquetasTablero()
	{
		if (entrada::estado().activa) return;   // (rama classic-visual-final: no flotan sobre la figura de la entrada clave)
		std::vector<glm::vec4> puestas;
		for (int i = 0; i < (int)NUM_ZONAS; i++) {
			const std::string tipo = ZONAS[i].tipo;
			if (tipo != "mazo" && tipo != "fusion" && tipo != "cementerio" && tipo != "desterrado") continue;
			const bool rival = strncmp(ZONAS[i].id, "rival", 5) == 0;
			const glm::vec4 c = colorZona(tipo);
			// la etiqueta flota sobre el borde lejano de la zona (o sobre la pila del mazo)
			const float alto = tipo == "mazo" ? 0.016f * (rival ? mazoR : mazoJ) + 0.25f : 0.25f;
			const glm::vec2 p = aPantalla(proyectar(centroZona(i) + glm::vec3(0.0f, alto, -ZONA_ALTO * 0.5f)));
			std::string titulo, numero;
			if (tipo == "mazo") {
				titulo = "MAZO";
				numero = std::to_string(rival ? mazoR : mazoJ);
			} else if (tipo == "fusion") {
				titulo = "FUSIÓN";
				numero = std::to_string(ctl && ctl->duelo ? (int)(rival ? ctl->extraR : ctl->extraJ).size() : rival ? PILA_FUSION_R : pilaFusionJ);   // (rama classic-duelo-ui: el del rival, real)
			} else if (tipo == "desterrado") {   // rama classic-duelo-ui
				if (!ctl || !ctl->duelo) continue;
				titulo = idioma::t("DESTERRADAS", "BANISHED");
				numero = std::to_string((rival ? ctl->desterradosR : ctl->desterradosJ).size());
			} else {
				titulo = "CEMENTERIO";
				numero = std::to_string(rival ? cementerioR : cementerioJ);
			}
			etiqueta(p, titulo, numero, c, &puestas);
		}
	}

	// ATK / DEF de cada monstruo del campo, en 3D: anclado delante de su carta (hacia el jugador, pasando el
	// recuadro dorado), un poco sobre la mesa, siempre de frente a la camara (billboard) y del tamano que le da la
	// perspectiva. No sigue al monstruo (no gira ni se mueve con sus ataques): flota con un vaiven leve y su brillo
	// late. ATK calido, DEF frio; en defensa se resalta DEF (y ATK se apaga). Compactos (la mitad de antes) y de
	// vidrio translucido con el texto nitido; solo con la camara frontal (rotulosVisibles): en los ataques, el cine o
	// la camara de invocacion se desvanecen en ROTULOS_FUNDIDO s y vuelven al terminar
	void dibujarAtkDef()
	{
		const float meta = mostrarRotulos ? 1.0f : 0.0f;
		const float paso = dtCuadro / ROTULOS_FUNDIDO;
		alfaRotulos = alfaRotulos < meta ? glm::min(meta, alfaRotulos + paso) : glm::max(meta, alfaRotulos - paso);
		if (alfaRotulos <= 0.0f) {
			return;
		}
		const float fundido = suave(0.0f, 1.0f, alfaRotulos);
		struct Etiqueta { int carta; bool defensa; glm::vec2 p; float w, tn, ancho, fase; int id; float cerca; int atk, def; };
		std::vector<Etiqueta> lista;
		int i = 0;
		for (const Ocupa& o : campoActual()) {
			const Zona* z = zonaPorId(o.zona);
			if (!z || strcmp(z->tipo, "monstruo") != 0 || o.bocaAbajo || !o.carta[0]) continue;
			const int ci = buscarCarta(o.carta);
			const int id = i++;
			if (ci < 0 || id >= (int)alfaAtkDef.size()) continue;
			const float fase = (float)id * 1.3f;
			const float vaiven = 0.05f * sinf(t * 1.5f + fase);
			const glm::vec3 ancla(z->x * ESCALA, 0.32f + vaiven, z->z * ESCALA + ZONA_ALTO * 0.56f + 0.75f);
			const glm::vec4 c0 = proyectar(ancla);
			if (c0.w <= 0.1f) continue;
			// tamano: la mitad de la altura en pantalla de 0.5 m en ese punto (antes la altura entera, 17..40 px a
			// 1080), con un minimo legible para la fila lejana del rival
			const glm::vec2 p = aPantalla(c0);
			const float px = glm::length(aPantalla(proyectar(ancla + glm::vec3(0.0f, 0.5f, 0.0f))) - p);
			const float tn = glm::clamp(px * 0.5f, 13.0f * s, 20.0f * s);
			const CartaUI& k = cartas[ci];
			const float tl = tn * 0.68f, esp = tn * 0.22f;
			const int bono = bonoBosque > 0.5f && potenciaBosque(k.id) ? 200 : 0;   // carta de campo Bosque
			int va = k.atk + bono, vd = k.def + bono;
			if (statsZona) statsZona(o.zona, va, vd);   // (rama classic-efectos)
			const float ancho = medir(F_TITULO, "ATK", tl, 1.0f) + medir(F_NUMEROS, std::to_string(va), tn) + medir(F_NUMEROS, "/", tn) +
			                    medir(F_TITULO, "DEF", tl, 1.0f) + medir(F_NUMEROS, std::to_string(vd), tn) + esp * 3.2f;
			lista.push_back({ ci, o.defensa, p, c0.w, tn, ancho, fase, id, suave(28.0f * s, 44.0f * s, px), va, vd });
		}
		// sin amontonarse: de la mas cercana a la mas lejana, una etiqueta que pisa a otra ya puesta se desvanece
		// (en los cortes de cine se cruzan; en la vista de juego no)
		std::sort(lista.begin(), lista.end(), [](const Etiqueta& a, const Etiqueta& b) { return a.w < b.w; });
		std::vector<glm::vec4> puestas;
		for (const Etiqueta& e : lista) {
			// (el rectangulo un poco achicado: pueden rozarse)
			const glm::vec4 r(e.p.x - e.ancho * 0.5f - e.tn * 0.3f, e.p.y - e.tn * 0.6f, e.p.x + e.ancho * 0.5f + e.tn * 0.3f, e.p.y + e.tn * 0.6f);
			bool pisa = false;
			for (const glm::vec4& q : puestas) {
				if (r.x < q.z && r.z > q.x && r.y < q.w && r.w > q.y) pisa = true;
			}
			if (!pisa) puestas.push_back(r);
			float& a = alfaAtkDef[e.id];
			// con la camara muy cerca (cortes de cine) se vuelven discretas para no tapar la accion
			a += ((pisa ? 0.0f : 1.0f - 0.65f * e.cerca) - a) * glm::min(1.0f, dtCuadro * 8.0f);
		}
		// se dibujan de la mas lejana a la mas cercana
		for (auto it = lista.rbegin(); it != lista.rend(); ++it) {
			const Etiqueta& e = *it;
			const float al = alfaAtkDef[e.id] * fundido;
			if (al < 0.02f) continue;
			const CartaUI& k = cartas[e.carta];
			const float tn = e.tn, tl = tn * 0.68f, esp = tn * 0.22f;
			// Bosque / efectos (rama classic-efectos): los numeros en verde si suben; en rojo si bajan
			const bool potenciado = e.atk > k.atk || e.def > k.def;
			const bool debilitado = !potenciado && (e.atk < k.atk || e.def < k.def);
			const std::string atk = std::to_string(e.atk), def = std::to_string(e.def);
			const float wA = medir(F_TITULO, "ATK", tl, 1.0f), wa = medir(F_NUMEROS, atk, tn);
			const float wD = medir(F_TITULO, "DEF", tl, 1.0f), wb = medir(F_NUMEROS, "/", tn);
			const float pad = tn * 0.5f, alto = tn * 1.4f;
			const float late = 0.5f + 0.5f * sinf(t * 2.6f + e.fase);
			const bool def0 = e.defensa;
			const glm::vec4 verde = debilitado ? glm::vec4(1.0f, 0.35f, 0.3f, 1.0f) : glm::vec4(0.45f, 1.0f, 0.35f, 1.0f);
			const glm::vec4 verdeClaro = debilitado ? glm::vec4(1.0f, 0.8f, 0.75f, 1.0f) : glm::vec4(0.85f, 1.0f, 0.75f, 1.0f);
			const glm::vec4 calido = potenciado || debilitado ? verde : glm::vec4(1.0f, 0.62f, 0.22f, 1.0f);
			const glm::vec4 calidoClaro = potenciado || debilitado ? verdeClaro : glm::vec4(1.0f, 0.93f, 0.72f, 1.0f);
			const glm::vec4 frio = potenciado || debilitado ? verde : glm::vec4(0.30f, 0.80f, 1.0f, 1.0f);
			const glm::vec4 frioClaro = potenciado || debilitado ? verdeClaro : glm::vec4(0.85f, 0.97f, 1.0f, 1.0f);
			const glm::vec4 borde = def0 ? frio : glm::mix(calido, frio, 0.35f);
			const float x0 = e.p.x - e.ancho * 0.5f, yb = e.p.y + tn * 0.36f;
			auto alfa = [&](glm::vec4 c, float a) { return glm::vec4(glm::vec3(c), a * al); };
			usar(setAtlas, false);
			// vidrio translucido (se ve el tablero detras): relleno al 34%, sombra corta, borde fino que late sin destello
			caja(x0 - pad, e.p.y - alto * 0.5f, e.ancho + pad * 2.0f, alto, 5.0f * s, PANEL, glm::vec4(0.05f, 0.07f, 0.12f, 0.34f * al),
			     glm::vec4(glm::vec3(borde) * al, al), alto * 0.5f, glm::vec4(0.9f * s, (0.45f + 0.3f * late) * al, 0.0f, 3.0f * s));
			const float aA = def0 ? 0.45f : 1.0f, aD = def0 ? 1.0f : 0.8f;
			float x = x0;
			texto(F_TITULO, "ATK", x, yb - tn * 0.04f, tl, alfa(calido, aA), NADA, glm::vec3(0.0f), 0, 1.0f);
			x += wA + esp * 0.6f;
			texto(F_NUMEROS, atk, x, yb, tn, alfa(calidoClaro, aA), alfa(calido, aA), glm::vec3(def0 ? 0.0f : 0.35f + 0.35f * late, 0.1f, 0.9f));
			x += wa + esp;
			texto(F_NUMEROS, "/", x, yb, tn, alfa(glm::vec4(0.6f, 0.66f, 0.78f, 1.0f), 0.8f));
			x += wb + esp;
			texto(F_TITULO, "DEF", x, yb - tn * 0.04f, tl, alfa(frio, aD), NADA, glm::vec3(0.0f), 0, 1.0f);
			x += wD + esp * 0.6f;
			texto(F_NUMEROS, def, x, yb, tn, alfa(frioClaro, aD), alfa(frio, aD), glm::vec3(def0 ? 0.45f + 0.4f * late : 0.25f, 0.1f, 0.9f));
		}
	}

	void dibujarManoRival()
	{
		for (int k = 0; k < nManoR(); k++) {
			if (ctl && ctl->robo.activo && ctl->robo.rival && ctl->robo.indice == k) continue;   // (rama classic-duelo-arreglos: viene volando)
			glm::vec2 c; float a, e;
			cartaManoR(k, c, a, e);
			glm::vec2 tam = glm::vec2(120.0f, 174.0f) * s * e;
			usar(setAtlas, false);
			ajusteManoRivalMD(k, c, tam);   // rama classic-md-ritmo: la que elige se levanta
			if (dibujarMuestraCPU(k, c, tam)) continue;   // rama classic-ps5-cpu-joey: la ensena como Joey (cpu_muestra_ui.h)
			usar(setAtlas, false);
			rect(c, tam, a, 18.0f * s, PANEL, glm::vec4(0, 0, 0, 0.5f), NADA, 9.0f * s, glm::vec4(0, 0, 0, 12.0f * s));
			// girada 180 grados (a): el dorso queda del reves, como lo ve el jugador
			const Dorso dz = usarDorso(false, false, true, 1.0f, (float)k);
			rect(c, tam, a, 0.0f, DORSO, dz.col, dz.col2, 9.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
	}

	void dibujarManoJugador()
	{
		int sel = -1;
		if (!objetivos.empty() && objetivos[seleccion].clase == MANO_J) sel = objetivos[seleccion].indice;
		auto una = [&](int k) {
			const int ci = buscarCarta(idManoJ(k));
			glm::vec2 c; float a, e;
			cartaManoJ(k, c, a, e);
			posManoJ[k] = c;   // rama mando: de aqui sale volando la carta que se juega
			zFoco(c.x - 75.0f * s * e, c.y - 109.0f * s * e, 150.0f * s * e, 218.0f * s * e, control::F_MANO_J, k);   // rama classic-pc
			if (ctl && ctl->robo.activo && !ctl->robo.rival && ctl->robo.indice == k) return;   // (rama classic-duelo-arreglos: viene volando)
			const glm::vec2 tam = glm::vec2(150.0f, 218.0f) * s * e;
			brilloJugableMano(k, c, tam, a);   // rama classic-duelo-ui
			usar(setAtlas, false);
			rect(c, tam, a, 22.0f * s, PANEL, glm::vec4(0, 0, 0, 0.55f), NADA, 10.0f * s, glm::vec4(0, 0, 0, 14.0f * s));
			if (ci >= 0 && listaCarta(ci)) {
				usar(cartas[ci].setMini, false);
				const float u = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
				rect(c, tam, a, 0.0f, IMAGEN, glm::vec4(glm::vec3(k == sel ? 1.08f : 0.92f), 1.0f), glm::vec4(0.6f, 0.8f, 1.0f, 1.0f), 8.0f * s,
				     glm::vec4(k == sel ? 0.55f : 0.15f, k == sel ? 1.0f : 0.25f, k * 0.17f, 0), glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
			} else {
				const Dorso dz = usarDorso(false, false, false, 1.0f, (float)k);
				rect(c, tam, a, 0.0f, DORSO, dz.col, dz.col2, 8.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
			}
		};
		for (int k = 0; k < nManoJ(); k++) {
			if (k != sel) una(k);
		}
		if (sel >= 0) una(sel);
	}

	// panel de LP: etiqueta, numero que rueda con degradado y halo, barra con estela de danno
	void panelLP(float y, bool rival)   // (y: rama classic-md-fases, la sacudida de MD la mueve)
	{
		const Vida& vi = rival ? vidaR : vidaJ;
		const glm::vec4 acento = rival ? CARMESI : CIAN;
		const float golpe = expf(-vi.tGolpe * 2.2f);
		const glm::vec2 sac = sacudidaLPMD(vi);   // rama classic-md-fases: el "Damage" de DuelLP (su curva propia, 0.5 s)
		const float x = COL_X * s + sac.x;
		y += sac.y;
		const float w = COL_ANCHO * s, h = 112.0f * s;
		const glm::vec4 vid = glm::mix(VIDRIO, glm::vec4(0.35f, 0.04f, 0.06f, 0.8f), golpe * 0.8f);
		caja(x, y, w, h, 24.0f * s, PANEL, vid, glm::mix(acento, glm::vec4(1, 0.3f, 0.3f, 1), golpe), 14.0f * s,
		     glm::vec4(1.4f * s, 0.9f, rival ? -0.11f : 0.11f, 16.0f * s));
		// etiqueta del lado
		texto(F_TITULO, rival ? "RIVAL" : "JUGADOR", x + 22.0f * s, y + 30.0f * s, 19.0f * s, acento, NADA, glm::vec3(0.0f), 0, 3.0f);
		rect(glm::vec2(x + w - 46.0f * s, y + 24.0f * s), glm::vec2(48.0f, 22.0f) * s, 0.0f, 6.0f * s, PANEL, glm::vec4(glm::vec3(acento) * 0.25f, 0.9f),
		     acento, 11.0f * s, glm::vec4(1.0f * s, 1.0f, 0.0f, 0.0f));
		texto(F_NUMEROS, "LP", x + w - 46.0f * s, y + 31.0f * s, 15.0f * s, BLANCO, NADA, glm::vec3(0.0f), 1, 1.0f);
		// numero
		char num[16];
		snprintf(num, sizeof(num), "%d", (int)roundf(vi.mostrado));
		// rama classic-md-fases: mientras cuenta, el numero toma el color de danno o de cura (DuelLP.lpColorDamage /
		// lpColorRecover, con nuestra paleta) y crece a 1.5 (CMainZoomIn 0.1 s; CMainZoomOut 1.5 s despues)
		const float cuenta = colorCuentaLPMD(vi);
		const glm::vec4 colCuenta = vi.cambio < 0 ? glm::vec4(1.0f, 0.35f, 0.32f, 1.0f) : glm::vec4(0.45f, 0.85f, 1.0f, 1.0f);
		glm::vec4 arriba = glm::mix(glm::mix(BLANCO, glm::vec4(1.0f, 0.55f, 0.5f, 1.0f), golpe), colCuenta, cuenta);
		glm::vec4 abajo = glm::mix(glm::mix(acento, glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), golpe), colCuenta, cuenta);
		// rama classic-md-entradas: LP bajos (1..1000), como DuelLP.IdleStep de MD: el numero late entre blanco y rojo (menu_md_ui.h)
		const float emer = cuenta < 0.01f ? emergenciaLPMD(vi) : -1.0f;
		if (emer >= 0.0f) {
			arriba = glm::vec4(1.0f, emer, emer, 1.0f);
			abajo = glm::mix(glm::vec4(1.0f, 0.12f, 0.16f, 1.0f), abajo, emer);
		}
		const float zoom = zoomNumeroLPMD(vi);
		texto(F_NUMEROS, num, x + 20.0f * s, y + 84.0f * s + 18.0f * s * (zoom - 1.0f), 52.0f * s * zoom, arriba, abajo, glm::vec3(0.55f + 0.25f * golpe, 0.0f, 0.85f), 0, 2.0f);
		// barra
		const float bx = x + 20.0f * s, bw = w - 40.0f * s;
		caja(bx, y + 94.0f * s, bw, 9.0f * s, 8.0f * s, BARRA, glm::vec4(glm::vec3(acento) * 0.75f, 1.0f), glm::mix(acento, BLANCO, 0.35f), 4.5f * s,
		     glm::vec4(glm::clamp(vi.mostrado / 8000.0f, 0.0f, 1.0f), glm::clamp(vi.fantasma / 8000.0f, 0.0f, 1.0f), golpe, 0.0f));
	}

	static float suave(float a, float b, float x)
	{
		const float u = glm::clamp((x - a) / (b - a), 0.0f, 1.0f);
		return u * u * (3.0f - 2.0f * u);
	}

	struct Contenido { int carta{ -1 }; bool dorso{ false }; bool rival{ false }; std::string titulo, subtitulo, texto; glm::vec4 color{ CIAN }; };

	Contenido contenidoDe(int sel) const
	{
		Contenido r;
		if (sel < 0 || sel >= (int)objetivos.size()) return r;
		const Objetivo& o = objetivos[sel];
		if (o.clase == MANO_J) {
			r.carta = buscarCarta(idManoJ(o.indice));
			return r;
		}
		if (o.clase == MANO_R) {
			r.dorso = true;
			r.rival = true;
			r.titulo = "MANO DEL RIVAL";
			r.subtitulo = std::to_string(nManoR()) + idioma::tr(" cartas ocultas");
			r.texto = "El rival guarda estas cartas boca abajo. Su contenido se revelara cuando las juegue.";
			r.color = CARMESI;
			return r;
		}
		const Zona& z = ZONAS[o.indice];
		const bool rival = strncmp(z.id, "rival", 5) == 0;
		const std::string tipo = z.tipo;
		r.rival = rival;
		r.color = rival ? CARMESI : CIAN;
		const Ocupa* oc = ocupante(z.id);
		if (tipo == "mazo") {
			const int n = rival ? mazoR : mazoJ;
			r.dorso = true;
			r.titulo = rival ? "MAZO DEL RIVAL" : "TU MAZO";
			r.subtitulo = std::to_string(n) + idioma::tr(" cartas restantes");
			r.texto = "Al comenzar cada turno se roba la carta de arriba. Si no quedan cartas para robar, el duelo se pierde.";
			return r;
		}
		if (tipo == "fusion") {
			if (oc && oc->carta[0]) {
				r.carta = buscarCarta(oc->carta);
				return r;
			}
			r.titulo = "ZONA DE FUSIÓN";
			r.subtitulo = "Sin monstruos de fusión";
			r.texto = "Aqui esperan los monstruos de fusion hasta que se invoquen con Polimerizacion.";
			r.color = colorZona("fusion");
			return r;
		}
		if (tipo == "cementerio" || tipo == "desterrado") {   // (rama classic-duelo-ui: tambien las desterradas)
			if (ctl) {
				const auto& pila = tipo == "desterrado" ? (rival ? ctl->desterradosR : ctl->desterradosJ) : rival ? ctl->cementerioR : ctl->cementerioJ;
				if (!pila.empty()) {
					r.carta = buscarCarta(pila.back());   // la de arriba de la pila
					return r;
				}
			}
			if (tipo == "desterrado") {
				r.titulo = rival ? idioma::t("DESTERRADAS DEL RIVAL", "OPPONENT'S BANISHED") : idioma::t("TUS DESTERRADAS", "YOUR BANISHED");
				r.subtitulo = "Vacío";
				r.texto = idioma::t("Las cartas desterradas por efectos quedan aquí, fuera del duelo. X: ver la lista.",
				                    "Cards banished by effects stay here, out of the duel. X: view the list.");
				r.color = colorZona("desterrado");
				return r;
			}
			r.titulo = rival ? "CEMENTERIO DEL RIVAL" : "TU CEMENTERIO";
			r.subtitulo = "Vacío";
			r.texto = "Las cartas destruidas, usadas o descartadas terminan aqui, boca arriba.";
			r.color = colorZona("cementerio");
			return r;
		}
		if (oc) {
			if (ctl && oc->bocaAbajo && !rival && oc->carta[0]) {
				r.carta = buscarCarta(oc->carta);   // rama mando: las propias boca abajo se ven (las del rival, el dorso)
				return r;
			}
			if (oc->bocaAbajo || !oc->carta[0]) {
				r.dorso = true;
				r.titulo = "CARTA BOCA ABAJO";
				r.subtitulo = rival ? "Colocada por el rival" : "Colocada por ti";
				r.texto = rival ? "Una magia o trampa oculta. Cuidado al atacar." : "Tu carta colocada, lista para activarse.";
				return r;
			}
			r.carta = buscarCarta(oc->carta);
			return r;
		}
		r.titulo = tipo == "monstruo" ? "ZONA DE MONSTRUO" : tipo == "magia" ? "ZONA DE MAGIA Y TRAMPA" : "ZONA DE CAMPO";
		r.subtitulo = "Vacía";
		r.texto = rival ? "Zona del rival sin cartas." : "Puedes colocar una carta aqui.";
		return r;
	}

	glm::vec4 colorAtributo(const std::string& a) const
	{
		if (a == "OSCURIDAD") return glm::vec4(0.70f, 0.42f, 1.0f, 1.0f);
		if (a == "LUZ") return glm::vec4(1.0f, 0.90f, 0.45f, 1.0f);
		if (a == "TIERRA") return glm::vec4(0.90f, 0.62f, 0.32f, 1.0f);
		if (a == "AGUA") return glm::vec4(0.35f, 0.65f, 1.0f, 1.0f);
		if (a == "FUEGO") return glm::vec4(1.0f, 0.42f, 0.22f, 1.0f);
		if (a == "VIENTO") return glm::vec4(0.45f, 1.0f, 0.62f, 1.0f);
		if (a == "MAGIA") return glm::vec4(0.25f, 0.95f, 0.75f, 1.0f);
		if (a == "TRAMPA") return glm::vec4(1.0f, 0.38f, 0.72f, 1.0f);
		return GRIS;
	}

	void dibujarColumna()
	{
		panelLP(MARGEN * s, true);
		panelLP((1080.0f - MARGEN - 112.0f) * s, false);
		// visor
		const float x = COL_X * s, y = 148.0f * s, w = COL_ANCHO * s, h = 784.0f * s;
		Contenido c = contenidoDe(selVisor >= 0 ? selVisor : seleccion);
		contenidoRitmoMD(c);   // rama classic-md-ritmo: como Master Duel, el visor muestra la carta que se activa / llega
		contenidoLista(c);     // rama classic-ps5-elegir-cartas: y la carta enfocada del selector de cartas
		const CartaUI* k = c.carta >= 0 ? &cartas[c.carta] : nullptr;
		const glm::vec4 acento = k ? colorAtributo(k->atributo) : c.color;
		caja(x, y, w, h, 26.0f * s, PANEL, VIDRIO, glm::mix(CIAN, acento, 0.5f), 16.0f * s, glm::vec4(1.4f * s, 0.8f, 0.07f, 18.0f * s));
		// polvo de luz que sube dentro del visor
		if (azar() < dtCuadro * 9.0f) {
			chispa(glm::vec2(x + (30.0f + azar() * (COL_ANCHO - 60.0f)) * s, y + (440.0f + 40.0f * azar()) * s), glm::vec2(0.0f, -(20.0f + 30.0f * azar()) * s),
			       2.5f + 2.0f * azar(), (1.5f + 2.0f * azar()) * s, glm::vec3(acento) * 0.8f);
		}
		// carta: se inclina un poco en 3D y gira al cambiar
		const glm::vec2 centro(x + w * 0.5f, y + 233.0f * s);
		const glm::vec2 tam = glm::vec2(297.0f, 430.0f) * s;
		const float flip = giroVisor >= 0.0f ? (giroVisor < 0.5f ? giroVisor : giroVisor - 1.0f) * glm::pi<float>() : 0.0f;
		const float angY = glm::radians(7.0f) * sinf(t * 0.6f) + flip;
		const float angX = glm::radians(3.5f) * sinf(t * 0.45f + 1.0f);
		usar(setAtlas, false);
		rect(centro, tam * 1.35f, 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(acento) * 0.22f, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		rectInclinado(centro + glm::vec2(0.0f, 10.0f * s), tam, angY, angX, 26.0f * s, PANEL, glm::vec4(0, 0, 0, 0.7f), NADA, 12.0f * s, glm::vec4(0, 0, 0, 18.0f * s));
		if (k && listaCarta((int)(k - cartas.data()))) {
			usar(setCaraGrande(k), false);
			const float u = 0.5f / k->cara.width, vv = 0.5f / k->cara.height;
			rectInclinado(centro, tam, angY, angX, 0.0f, IMAGEN, glm::vec4(1.0f), acento, 10.0f * s,
			              glm::vec4(0.55f, 1.0f, (float)c.carta * 0.13f, 0.0f), glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
		} else if (c.dorso) {
			const Dorso dz = usarDorso(false, false, c.rival, 1.0f, 0.0f);
			rectInclinado(centro, tam, angY, angX, 0.0f, DORSO, dz.col, dz.col2, 10.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		} else {
			// zona vacia: holograma del contorno
			rectInclinado(centro, tam * 0.92f, angY, angX, 20.0f * s, ZONA, acento, acento, 12.0f * s, glm::vec4(2.5f * s, 0.5f, 1.0f, 0.0f));
		}
		usar(setAtlas, false);
		// escaneo holografico: una linea de luz barre la carta recien elegida
		if (tTexto < 0.6f) {
			const float u = tTexto / 0.6f;
			const float yy = centro.y - tam.y * 0.5f + tam.y * u;
			rect(glm::vec2(centro.x, yy), glm::vec2(tam.x * 1.15f, 4.0f * s), 0.0f, 10.0f * s, LINEA, glm::vec4(glm::vec3(acento) * 1.3f, 1.0f - u), NADA, 0.0f,
			     glm::vec4(0.0f));
		}
		// texto (entra deslizando)
		const float a = suave(0.0f, 0.28f, tTexto);
		const float dx = (1.0f - a) * 14.0f * s;
		const float tx = x + 24.0f * s + dx, tw = w - 48.0f * s;
		float ty = y + 498.0f * s;
		auto alfa = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * a); };
		if (k) {
			std::string nombre = k->vNombre();
			float tn = 33.0f * s;
			while (medir(F_TITULO, nombre, tn) > tw && tn > 18.0f * s) tn -= 1.0f * s;
			texto(F_TITULO, nombre, tx, ty, tn, alfa(BLANCO), alfa(acento), glm::vec3(0.35f, 0.0f, 0.0f));
			ty += 30.0f * s;
			const bool monstruo = k->clase != "magia" && k->clase != "trampa";
			if (monstruo) {
				for (int i = 0; i < k->nivel; i++) {
					rect(glm::vec2(tx + (10.0f + i * 23.0f) * s, ty - 6.0f * s), glm::vec2(11.0f * s), 0.0f, 10.0f * s, ESTRELLA,
					     alfa(glm::vec4(1.0f, 0.86f, 0.35f, 1.0f)), alfa(glm::vec4(1.0f, 0.55f, 0.15f, 1.0f)), 0.0f, glm::vec4(0.8f, 0, 0, 0));
				}
			}
			// atributo: ficha y nombre (a la derecha)
			const float aw = medir(F_TITULO, k->vAtributo(), 17.0f * s, 2.0f);
			rect(glm::vec2(tx + tw - aw - 18.0f * s, ty - 6.0f * s), glm::vec2(20.0f * s), 0.0f, 10.0f * s, ORBE, alfa(acento),
			     glm::vec4(glm::vec3(acento) * 0.35f, 1.0f), 0.0f, glm::vec4(0.8f, 0.9f, 0, 0));
			texto(F_TITULO, k->vAtributo(), tx + tw, ty, 17.0f * s, alfa(acento), NADA, glm::vec3(0.0f), 2, 2.0f);
			ty += 32.0f * s;
			std::string tipo = "[ " + k->vTipo();
			if (k->clase == "efecto") tipo += idioma::tr(" / Efecto");
			if (k->clase == "fusion") tipo += idioma::tr(" / Fusión");
			tipo += " ]";
			texto(F_TEXTO, tipo, tx, ty, 23.0f * s, alfa(glm::vec4(0.72f, 0.86f, 1.0f, 1.0f)));
			if (monstruo) {
				ty += 38.0f * s;
				texto(F_TITULO, "ATK", tx, ty - 3.0f * s, 17.0f * s, alfa(GRIS), NADA, glm::vec3(0.0f), 0, 2.0f);
				texto(F_NUMEROS, std::to_string(k->atk), tx + 40.0f * s, ty, 28.0f * s, alfa(glm::vec4(1.0f, 0.93f, 0.7f, 1.0f)), alfa(ORO), glm::vec3(0.45f, 0.0f, 0.8f));
				texto(F_TITULO, "DEF", tx + tw * 0.52f, ty - 3.0f * s, 17.0f * s, alfa(GRIS), NADA, glm::vec3(0.0f), 0, 2.0f);
				texto(F_NUMEROS, std::to_string(k->def), tx + tw * 0.52f + 42.0f * s, ty, 28.0f * s, alfa(glm::vec4(0.8f, 0.95f, 1.0f, 1.0f)), alfa(CIAN), glm::vec3(0.45f, 0.0f, 0.8f));
			}
			ty += 22.0f * s;
			rect(glm::vec2(x + w * 0.5f, ty), glm::vec2(tw, 3.0f * s), 0.0f, 6.0f * s, LINEA, alfa(acento), NADA, 0.0f, glm::vec4(2.0f, 0.3f, 0, 0));
			ty += 30.0f * s;
			parrafo(F_TEXTO, k->vDesc(), tx, ty, 23.0f * s, tw, 27.0f * s, (int)((y + h - 18.0f * s - ty) / (27.0f * s)) + 1, alfa(glm::vec4(0.82f, 0.86f, 0.93f, 1.0f)));
		} else {
			texto(F_TITULO, c.titulo, tx, ty, 28.0f * s, alfa(BLANCO), alfa(c.color), glm::vec3(0.35f, 0.0f, 0.0f));
			ty += 34.0f * s;
			texto(F_TEXTO, c.subtitulo, tx, ty, 22.0f * s, alfa(c.color));
			ty += 22.0f * s;
			rect(glm::vec2(x + w * 0.5f, ty), glm::vec2(tw, 3.0f * s), 0.0f, 6.0f * s, LINEA, alfa(c.color), NADA, 0.0f, glm::vec4(2.0f, 0.3f, 0, 0));
			ty += 32.0f * s;
			parrafo(F_TEXTO, c.texto, tx, ty, 23.0f * s, tw, 27.0f * s, 8, alfa(glm::vec4(0.82f, 0.86f, 0.93f, 1.0f)));
		}
	}

	void dibujarFases()
	{
		const float cx = (FASES_X + FASES_ANCHO * 0.5f) * s;
		const float paso = 82.0f * s;
		const float y0 = 540.0f * s - paso * 2.5f;
		// riel vertical de luz
		usar(setAtlas, false);
		rect(glm::vec2(cx, 540.0f * s), glm::vec2(paso * 5.6f, 2.0f * s), glm::half_pi<float>(), 8.0f * s, LINEA, glm::vec4(0.35f, 0.75f, 1.0f, 0.55f),
		     NADA, 0.0f, glm::vec4(3.0f, 0.0f, 0, 0));
		texto(F_TITULO, "FASE", cx, y0 - 76.0f * s, 14.0f * s, GRIS, NADA, glm::vec3(0.0f), 1, 3.0f);
		texto(F_TITULO, NOMBRE_FASE[fase], cx, y0 - 52.0f * s, 17.0f * s, ORO, NADA, glm::vec3(0.5f, 0.0f, 0.0f), 1, 1.5f);
		for (int i = 0; i < 6; i++) {
			const float y = y0 + i * paso;
			const bool activa = i == fase;
			const float pulso = 0.8f + 0.2f * sinf(t * 4.0f);
			if (activa) {
				rect(glm::vec2(cx, y), glm::vec2(70.0f * s), 0.0f, 30.0f * s, PARTICULA, glm::vec4(glm::vec3(ORO) * 0.45f * pulso, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
				rect(glm::vec2(cx, y), glm::vec2(58.0f * s), 0.0f, 22.0f * s, ORBE, ORO, glm::vec4(0.55f, 0.36f, 0.08f, 1.0f), 0.0f, glm::vec4(1.4f * pulso, 1.0f, 0, 0));
				rect(glm::vec2(cx, y), glm::vec2(62.0f * s), t * 0.8f, 12.0f * s, MARCO, ORO, ORO, 31.0f * s, glm::vec4(2.0f * s, 6.0f * s, 1.0f, 0.0f));
				texto(F_NUMEROS, FASES[i], cx, y + 7.5f * s, 19.0f * s, glm::vec4(1.0f, 0.98f, 0.9f, 1.0f), NADA, glm::vec3(0.0f, 0.12f, 0.0f), 1);
			} else {
				const bool pasada = i < fase;
				rect(glm::vec2(cx, y), glm::vec2(50.0f * s), 0.0f, 16.0f * s, ORBE, pasada ? glm::vec4(0.35f, 0.75f, 1.0f, 0.9f) : glm::vec4(0.4f, 0.5f, 0.65f, 0.8f),
				     glm::vec4(0.03f, 0.05f, 0.1f, 1.0f), 0.0f, glm::vec4(pasada ? 0.4f : 0.15f, 0.85f, 0, 0));
				texto(F_NUMEROS, FASES[i], cx, y + 6.5f * s, 16.0f * s, pasada ? glm::vec4(0.75f, 0.9f, 1.0f, 1.0f) : GRIS, NADA, glm::vec3(0.0f), 1);
			}
		}
	}

	// Selector de camara: pastilla de vidrio en la esquina inferior izquierda del campo (a la derecha de la columna,
	// a la altura del panel de LP del jugador, sin tocar la mano). Teclas L1 y R1 a los costados, icono de camara,
	// "CAMARA" y el modo actual en dorado, y un punto por modo. Al cambiar, el borde destella, la tecla usada se
	// enciende y encima se despliega la lista de modos (se apaga sola a los ~2 s). En LIBRE recuerda los controles
	void dibujarSelectorCamara()
	{
		usar(setAtlas, false);
		const float h = 64.0f * s;
		const float x0 = (CAMPO_X + 14.0f) * s;
		const float y0 = (1080.0f - MARGEN) * s - h;
		const float cambio = 1.0f - suave(0.0f, 0.5f, camDesde);   // destello del cambio (1 al cambiar)
		const float tn = 23.0f * s, te = 12.5f * s;
		const bool libre = camModo == CAM_LIBRE;
		const std::string etiqueta = libre ? "STICK R  ·  L2 / R2" : "CÁMARA";
		// ancho fijo (el del modo mas largo): la pastilla no salta al cambiar
		float wTxt = glm::max(medir(F_TITULO, "CÁMARA", te, 3.0f), medir(F_TITULO, "STICK R  ·  L2 / R2", te, 1.5f));
		for (int i = 0; i < NUM_CAMARAS; i++) {
			wTxt = glm::max(wTxt, medir(F_NUMEROS, NOMBRE_CAMARA[i], tn, 1.5f));
		}
		const float chip = 34.0f * s, icono = 34.0f * s, hueco = 11.0f * s, borde = 12.0f * s;
		const float w = borde * 2.0f + chip * 2.0f + icono + wTxt + hueco * 3.0f;
		const float cy = y0 + h * 0.5f;
		caja(x0, y0, w, h, 22.0f * s, PANEL, VIDRIO, glm::mix(CIAN, ORO, cambio), h * 0.5f,
		     glm::vec4(1.3f * s, 0.75f + 0.9f * cambio, 0.09f, 14.0f * s));
		// teclas L1 / R1 (la usada se enciende un momento)
		auto tecla = [&](float cx, const char* nombre, bool usada) {
			const float luz = usada ? cambio : 0.0f;
			rect(glm::vec2(cx, cy), glm::vec2(chip, 24.0f * s), 0.0f, 6.0f * s, PANEL, glm::mix(glm::vec4(0.05f, 0.07f, 0.12f, 0.95f), glm::vec4(0.35f, 0.26f, 0.06f, 0.95f), luz),
			     glm::mix(GRIS, ORO, luz), 7.0f * s, glm::vec4(1.0f * s, 0.8f + luz, 0.0f, 0.0f));
			texto(F_NUMEROS, nombre, cx, cy + 5.0f * s, 14.0f * s, glm::mix(glm::vec4(0.8f, 0.86f, 0.95f, 1.0f), BLANCO, luz), NADA, glm::vec3(0.0f), 1);
		};
		float x = x0 + borde;
		tecla(x + chip * 0.5f, "L1", camDir < 0);
		x += chip + hueco;
		// icono: cuerpo de camara con visor y lente de luz
		const glm::vec2 ic(x + icono * 0.5f, cy + 2.0f * s);
		rect(ic + glm::vec2(-7.0f, -11.0f) * s, glm::vec2(11.0f, 6.0f) * s, 0.0f, 3.0f * s, PANEL, glm::vec4(0.10f, 0.14f, 0.22f, 0.95f), ORO, 2.0f * s,
		     glm::vec4(1.0f * s, 0.9f, 0.0f, 0.0f));
		rect(ic, glm::vec2(32.0f, 21.0f) * s, 0.0f, 4.0f * s, PANEL, glm::vec4(0.10f, 0.14f, 0.22f, 0.95f), ORO, 5.0f * s, glm::vec4(1.2f * s, 1.0f + cambio, 0.0f, 0.0f));
		rect(ic, glm::vec2(13.0f * s), 0.0f, 8.0f * s, ORBE, glm::mix(CIAN, ORO, cambio), glm::vec4(0.03f, 0.08f, 0.16f, 1.0f), 0.0f,
		     glm::vec4(0.6f + 0.8f * cambio, 0.9f, 0.0f, 0.0f));
		x += icono + hueco;
		// rotulo, modo y puntos de los modos
		texto(F_TITULO, etiqueta, x, y0 + 21.0f * s, te, libre ? glm::vec4(0.72f, 0.86f, 1.0f, 1.0f) : GRIS, NADA, glm::vec3(0.0f), 0, libre ? 1.5f : 3.0f);
		texto(F_NUMEROS, NOMBRE_CAMARA[camModo], x, y0 + 45.0f * s, tn, glm::vec4(1.0f, 0.95f, 0.78f, 1.0f), ORO, glm::vec3(0.4f + 0.5f * cambio, 0.0f, 0.8f), 0, 1.5f);
		for (int i = 0; i < NUM_CAMARAS; i++) {
			const bool es = i == camModo;
			rect(glm::vec2(x + (3.0f + i * 11.0f) * s, y0 + 55.0f * s), glm::vec2((es ? 6.0f : 4.0f) * s), 0.0f, 4.0f * s, ORBE,
			     es ? ORO : glm::vec4(0.45f, 0.55f, 0.7f, 0.8f), glm::vec4(0.05f, 0.08f, 0.14f, 1.0f), 0.0f, glm::vec4(es ? 0.8f : 0.0f, 0.9f, 0.0f, 0.0f));
		}
		x += wTxt + hueco;
		tecla(x + chip * 0.5f, "R1", camDir > 0);
		// lista de modos desplegada encima al cambiar
		const float a = suave(0.0f, 0.18f, camDesde) * (1.0f - suave(1.9f, 2.4f, camDesde));
		if (a < 0.01f) {
			return;
		}
		const float fila = 31.0f * s, hl = fila * (float)NUM_CAMARAS + 16.0f * s;
		const float yl = y0 - 10.0f * s - hl + (1.0f - a) * 14.0f * s;
		caja(x0, yl, w, hl, 18.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), VIDRIO.a * a), glm::vec4(glm::vec3(CIAN) * a, a), 14.0f * s,
		     glm::vec4(1.1f * s, 0.7f * a, 0.0f, 12.0f * s));
		for (int i = 0; i < NUM_CAMARAS; i++) {
			const bool es = i == camModo;
			const float fy = yl + 8.0f * s + fila * i;
			if (es) {
				caja(x0 + 8.0f * s, fy + 2.0f * s, w - 16.0f * s, fila - 4.0f * s, 6.0f * s, PANEL, glm::vec4(0.30f, 0.22f, 0.05f, 0.55f * a), glm::vec4(glm::vec3(ORO) * a, a),
				     (fila - 4.0f * s) * 0.5f, glm::vec4(1.0f * s, 0.9f * a, 0.0f, 0.0f));
			}
			rect(glm::vec2(x0 + 26.0f * s, fy + fila * 0.5f), glm::vec2((es ? 8.0f : 5.0f) * s), 0.0f, 5.0f * s, ORBE,
			     glm::vec4(glm::vec3(es ? ORO : glm::vec4(0.45f, 0.55f, 0.7f, 1.0f)), a), glm::vec4(0.05f, 0.08f, 0.14f, 1.0f), 0.0f,
			     glm::vec4(es ? 0.8f : 0.0f, 0.9f, 0.0f, 0.0f));
			texto(F_NUMEROS, NOMBRE_CAMARA[i], x0 + 44.0f * s, fy + fila * 0.5f + 7.0f * s, 17.0f * s,
			      es ? glm::vec4(1.0f, 0.95f, 0.78f, a) : glm::vec4(glm::vec3(GRIS), a), NADA, glm::vec3(0.0f), 0, 1.5f);
		}
	}

	// el recuadro dorado: metas segun lo elegido; las esquinas vuelan con resorte
	void metaMarco(std::array<glm::vec4, 4>& c, glm::vec4& forma, bool& en3D)
	{
		const Objetivo& o = objetivos[seleccion];
		en3D = false;
		if (o.clase == MANO_J || o.clase == MANO_R) {
			glm::vec2 cc; float a, e;
			glm::vec2 tam;
			if (o.clase == MANO_J) {
				cartaManoJ(o.indice, cc, a, e);
				tam = glm::vec2(150.0f, 218.0f) * s * e;
			} else {
				cartaManoR(o.indice, cc, a, e);
				tam = glm::vec2(120.0f, 174.0f) * s * e;
			}
			const glm::vec2 m = tam * 0.5f + glm::vec2(7.0f * s);
			const glm::vec2 l[4] = { { -m.x, -m.y }, { m.x, -m.y }, { m.x, m.y }, { -m.x, m.y } };
			for (int i = 0; i < 4; i++) {
				const glm::vec2 q{ l[i].x * cosf(a) - l[i].y * sinf(a), l[i].x * sinf(a) + l[i].y * cosf(a) };
				c[i] = recorte(cc + q);
			}
			forma = glm::vec4(m, 3.5f * s, 14.0f * s);
			return;
		}
		en3D = true;
		const glm::vec3 z = centroZona(o.indice) + glm::vec3(0.0f, 0.2f, 0.0f);
		const glm::vec2 m = glm::vec2(ZONA_ANCHO, ZONA_ALTO) * 0.56f;
		const glm::vec3 esq[4] = { z + glm::vec3(-m.x, 0, -m.y), z + glm::vec3(m.x, 0, -m.y), z + glm::vec3(m.x, 0, m.y), z + glm::vec3(-m.x, 0, m.y) };
		for (int i = 0; i < 4; i++) c[i] = proyectar(esq[i]);
		forma = glm::vec4(m * 100.0f, 12.0f, 26.0f);
	}

	void marcoQuad(bool en3D, float intensidad = 1.0f)
	{
		usar(setAtlas, en3D);
		// margen para el halo: se extiende la geometria (en el espacio de las esquinas) un 14%
		glm::vec4 c[4];
		const glm::vec4 centro = (marco[0] + marco[1] + marco[2] + marco[3]) * 0.25f;
		const float ext = 1.0f + 2.2f * formaMarco.w / glm::max(glm::min(formaMarco.x, formaMarco.y), 1.0f);
		for (int i = 0; i < 4; i++) c[i] = centro + (marco[i] - centro) * ext;
		const glm::vec2 m = glm::vec2(formaMarco) * ext;
		const glm::vec2 l[4] = { { -m.x, -m.y }, { m.x, -m.y }, { m.x, m.y }, { -m.x, m.y } };
		const glm::vec2 uv[4] = {};
		quad(c, l, uv, ORO, ORO, glm::vec4(formaMarco.x, formaMarco.y, formaMarco.z * 2.5f, (float)MARCO),
		     glm::vec4(formaMarco.z, formaMarco.w, intensidad, 0.0f));
	}

	void dibujarMarco()
	{
		if (objetivos.empty()) return;
		std::array<glm::vec4, 4> meta;
		glm::vec4 forma;
		bool en3D;
		metaMarco(meta, forma, en3D);
		if (!marcoListo) {
			marco = meta;
			formaMarco = forma;
			marcoListo = true;
		}
		const float dt = dtCuadro;
		float dist = 0.0f;
		for (int i = 0; i < 4; i++) {
			for (int k = 0; k < 4; k++) {
				resorte(marco[i][k], velMarco[i][k], meta[i][k], dt, 220.0f, 0.8f);
			}
			dist = glm::max(dist, glm::length(aPantalla(marco[i]) - aPantalla(meta[i])));
		}
		for (int k = 0; k < 4; k++) resorte(formaMarco[k], velForma[k], forma[k], dt, 220.0f, 0.8f);
		marcoEn3D = en3D;
		llegada = dist < 6.0f * s ? glm::min(1.0f, llegada + dt * 6.0f) : 0.0f;
		if (!(en3D && llegada > 0.9f)) {
			marcoQuad(false);
		} else {
			marcoQuad(false, 0.45f);   // lo que tapan los monstruos se ve como un fantasma (rayos X)
		}
		// chispas doradas que nacen en el borde
		glm::vec2 p[4];
		for (int i = 0; i < 4; i++) p[i] = aPantalla(marco[i]);
		emitir += dt * 34.0f;
		while (emitir >= 1.0f) {
			emitir -= 1.0f;
			const int lado = (int)(azar() * 4.0f) % 4;
			const glm::vec2 q = glm::mix(p[lado], p[(lado + 1) % 4], azar());
			chispa(q, glm::vec2((azar() - 0.5f) * 30.0f, -(30.0f + 50.0f * azar())) * s, 0.6f + 0.7f * azar(), (1.6f + 2.4f * azar()) * s,
			       glm::mix(glm::vec3(1.0f, 0.75f, 0.25f), glm::vec3(1.0f, 0.95f, 0.7f), azar()), azar() > 0.8f ? 1.0f : 0.0f);
		}
		// el puntero: flecha que flota sobre lo elegido
		glm::vec2 arriba = p[0];
		for (int i = 1; i < 4; i++) {
			if (p[i].y < arriba.y) arriba = p[i];
		}
		const glm::vec2 centro = (p[0] + p[1] + p[2] + p[3]) * 0.25f;
		const float bob = sinf(t * 4.0f) * 6.0f * s;
		const bool rivalMano = objetivos[seleccion].clase == MANO_R;
		const glm::vec2 punta = rivalMano ? glm::vec2(centro.x, glm::max(glm::max(p[0].y, p[1].y), glm::max(p[2].y, p[3].y)) + (30.0f * s - bob))
		                                  : glm::vec2(centro.x, arriba.y - 30.0f * s + bob);
		usar(setAtlas, false);
		rect(punta, glm::vec2(34.0f * s), rivalMano ? glm::pi<float>() : 0.0f, 14.0f * s, FLECHA, ORO, glm::vec4(1.0f, 0.97f, 0.8f, 1.0f), 0.0f, glm::vec4(0.0f));
	}

	void dibujarChispas()
	{
		usar(setAtlas, false);
		for (const Chispa& c : chispas) {
			if (c.tam <= 0.0f) continue;
			const float u = c.vida / c.dur;
			const float a = glm::min(u * 6.0f, 1.0f) * (1.0f - u) * (1.0f - u);
			const float tam = c.tam * (1.0f + c.cruz * 2.5f);
			rect(c.pos, glm::vec2(tam * 2.0f), 0.0f, tam * 2.0f, PARTICULA, glm::vec4(c.color * 1.4f, a), NADA, 0.0f, glm::vec4(c.cruz, 0, 0, 0));
		}
	}

	// ================================================================================ rama mando (control.h)
	// Todo lo que dibuja el duelo interactivo: las cartas de la mesa segun control::Mando (con sus volteos, giros a
	// defensa y llegadas), las zonas elegibles, la carta que vuela de la mano a su zona, el menu contextual de vidrio,
	// el cartel de la fase, los avisos, el detalle grande y la barra de ayuda con los botones de PlayStation
	// dibujados por codigo. Solo se usa con conectarMando(); sin el, la interfaz es la de siempre.
	const control::Mando* ctl{ nullptr };
	mutable std::array<Ocupa, NUM_ZONAS> ocupaCache{};
	std::array<glm::vec2, MAX_MANO_J> posManoJ{};
	int serieVuelo{ -1 };
	glm::vec2 vueloDesde{ 0.0f };

	int nManoR() const { return ctl ? std::min(std::max(ctl->manoR, 0), MAX_MANO_R) : NUM_MANO_R; }
	int nManoJ() const { return ctl ? std::min((int)ctl->manoJ.size(), MAX_MANO_J) : NUM_MANO_J; }
	std::string idManoJ(int k) const
	{
		if (ctl) return k >= 0 && k < (int)ctl->manoJ.size() ? ctl->manoJ[k] : std::string();
		return k >= 0 && k < NUM_MANO_J ? MANO_JUGADOR[k] : "";
	}
	// la carta k de la mano esta elegida (su menu abierto o eligiendo donde va): se queda levantada
	bool manoElegida(int k) const
	{
		return ctl && (ctl->estado == control::MENU || ctl->estado == control::ELEGIR_ZONA || ctl->estado == control::ELEGIR_TRIBUTOS) && ctl->origenMano == k;
	}

	void actualizarMando()
	{
		if (!ctl) return;
		if (!fijo) pedido = ctl->idFoco();   // (YUGI_PUNTERO del PC manda sobre el mando)
		if (!ctl->demo) fase = ctl->fase;
		cementerioJ = (int)ctl->cementerioJ.size();
		cementerioR = (int)ctl->cementerioR.size();
	}

	const Ocupa* ocupanteMando(const char* zona) const
	{
		const int z = control::zonaPorId(zona);
		if (z < 0 || z >= (int)NUM_ZONAS || ctl->zonas[z].vacia()) return nullptr;
		const control::Celda& c = ctl->zonas[z];
		ocupaCache[z] = Ocupa{ ZONAS[z].id, c.carta == "?" ? "" : c.carta.c_str(), c.bocaAbajo, c.defensa };
		return &ocupaCache[z];
	}

	// los monstruos del campo para los rotulos ATK/DEF
	std::vector<Ocupa> campoActual() const
	{
		std::vector<Ocupa> r;
		if (campoPropio) {   // (ataque por casillas: el juego dijo quien esta en cada casilla)
			for (const Ocupa& o : ocupantesMonstruo) {
				if (o.zona && o.carta && o.carta[0]) r.push_back(o);
			}
			return r;
		}
		if (!ctl) {
			r.assign(std::begin(CAMPO_PRUEBA), std::end(CAMPO_PRUEBA));
			return r;
		}
		for (int i = 0; i < (int)NUM_ZONAS; i++) {
			if (strcmp(ZONAS[i].tipo, "monstruo") != 0) continue;
			if (const Ocupa* o = ocupanteMando(ZONAS[i].id)) r.push_back(*o);
		}
		return r;
	}

	// la carta que trae dibujada el tablero de Blender en esa zona (tablero.glb: escena_tablero.py CAMPO, que es
	// CAMPO_PRUEBA sin la fusion)
	const Ocupa* horneada(const char* zona) const
	{
		for (const Ocupa& o : CAMPO_PRUEBA) {
			if (strcmp(o.zona, zona) == 0 && !strstr(zona, "fusion")) return &o;
		}
		return nullptr;
	}

	// una carta plana en el mundo: centro, medio ancho y medio alto (vectores; el alto apunta al pie de la imagen),
	// la cara chica (mini) o el dorso original, con brillo y opacidad (alfa premultiplicado)
	void cartaPlano(glm::vec3 centro, glm::vec3 ancho, glm::vec3 alto, int ci, bool rival, float brillo, float alfa, float fase)
	{
		const float mx = glm::length(ancho) * 100.0f, my = glm::length(alto) * 100.0f;
		const glm::vec2 l[4] = { { -mx, -my }, { mx, -my }, { mx, my }, { -mx, my } };
		const glm::vec2 uv[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };
		glm::vec4 c[4];
		for (int i = 0; i < 4; i++) {
			c[i] = proyectar(centro + ancho * (l[i].x / mx) + alto * (l[i].y / my));
		}
		if (ci >= 0 && listaCarta(ci)) {
			const glm::vec4 borde = rival ? CARMESI : CIAN;
			usar(cartas[ci].setCara, true);
			quad(c, l, uv, glm::vec4(glm::vec3(0.88f * brillo), alfa), borde * 0.35f * brillo, glm::vec4(mx, my, 5.0f, (float)IMAGEN),
			     glm::vec4(0.10f + 0.3f * (brillo - 1.0f), 0.22f, fase, 0.0f));
		} else {
			Dorso dz = usarDorso(true, true, rival, 0.9f * brillo, fase * 2.0f);
			dz.col.a = alfa;
			glm::vec2 uvd[4];
			for (int i = 0; i < 4; i++) uvd[i] = glm::mix(uvDorso0(true), uvDorso1(true), uv[i]);
			quad(c, l, uvd, dz.col, dz.col2, glm::vec4(mx, my, 5.0f, (float)DORSO), dz.extra);
		}
	}

	// la carta de una zona con sus animaciones: gira 90 grados al pasar a defensa, se levanta y se voltea alrededor
	// de su eje largo al voltearse (como la invocacion), y destella al llegar o activarse
	void cartaZona(int i, const control::Celda& k, bool rival, float alfa = 1.0f, float alto0 = 0.045f)
	{
		glm::vec3 mueve(0.0f), haciaObj(0.0f);
		float inclina = 0.0f, brilloGolpe = 0.0f;
		if (golpe.t >= 0.0f && golpe.atacante >= 0 && golpe.objetivo >= 0 && alfa >= 1.0f) {
			const float tg = golpe.t;
			glm::vec3 d = destinoGolpe() - centroZona(golpe.atacante);   // (rama classic-ps5-ataque: directo, al punto de MD)
			const float largo = glm::length(d);
			d = largo > 1e-3f ? d / largo : glm::vec3(0.0f, 0.0f, -1.0f);
			if (i == golpe.atacante && !(figuraEnZona && figuraEnZona(i))) {
				// carga (se levanta y se echa atras), embestida inclinada hasta casi tocar al objetivo, y vuelta
				// rama classic-md-combate: el viaje de Master Duel (BezierMotion StrikeLight00 "Low" 0.45 s / StrikeLight02 "High"
				// 0.65 s): un Bezier de 3 puntos, inicio en la zona, via atras y arriba (MD: 25 atras / 20 arriba; High 35 / 25), fin
				// casi sobre el objetivo; avanza con su accelerationCurve (carga lenta, embestida: Low (0.71, 0.31), High (0.81,
				// 0.46)) y gira hacia el objetivo con su rotationAccelerationCurve. Tras el golpe espera attackExecPostTime (0.25)
				// y vuelve. Amplitud de la via: la de antes (0.35 atras, 0.55 arriba) en la proporcion de MD
				const ritmo::Movimiento* mv = ritmo::movimiento(golpe.alto ? "StrikeLight02" : "StrikeLight00");
				const float vuelve = suave(G_IMPACTO + ritmo::T_POST_GOLPE, G_VUELVE, tg);
				const float hasta = glm::max(largo - CARTA_ALTO * 0.9f, 0.0f);
				if (mv && mv->acel.hay()) {
					const float x = glm::clamp(tg / G_IMPACTO, 0.0f, 1.0f);
					const float w = glm::clamp(mv->acel.valor(x), 0.0f, 1.0f), gr = mv->giro.hay() ? glm::clamp(mv->giro.valor(x), 0.0f, 1.0f) : x;
					const glm::vec3 via = d * (-0.70f * (golpe.alto ? 35.0f / 25.0f : 1.0f)) + glm::vec3(0.0f, 1.10f * (golpe.alto ? 25.0f / 20.0f : 1.0f), 0.0f);
					const glm::vec3 fin = d * hasta + glm::vec3(0.0f, golpe.alto ? 0.12f : 0.08f, 0.0f);
					mueve = (2.0f * w * (1.0f - w) * via + w * w * fin) * (1.0f - vuelve);
					inclina = (-0.35f * 4.0f * w * (1.0f - w) * (1.0f - gr) + 0.45f * gr) * (1.0f - vuelve);
					brilloGolpe = 0.6f * glm::min(1.0f, x * 4.0f) * (1.0f - vuelve);
				} else {   // (sin los datos de MD: la carga y la embestida de antes)
					const float carga = suave(0.0f, G_CARGA, tg), va = suave(G_CARGA, G_IMPACTO, tg);
					mueve = d * (hasta * va * (1.0f - vuelve) - 0.35f * carga * (1.0f - va)) + glm::vec3(0.0f, (0.55f * carga + 0.25f * va) * (1.0f - vuelve), 0.0f);
					inclina = (-0.35f * carga * (1.0f - va) + 0.45f * va) * (1.0f - vuelve);
					brilloGolpe = 0.6f * carga * (1.0f - vuelve);
				}
				haciaObj = golpe.directo ? glm::normalize(glm::vec3(d.x, 0.0f, d.z) + glm::vec3(0.0f, 0.0f, 1e-4f)) : d;
			} else if (i == golpe.objetivo && !golpe.directo) {   // (directo: la zona esta vacia, nadie tiembla)
				const float u = tg - G_IMPACTO;
				if (u > 0.0f) {
					const float tiembla = expf(-u * 7.0f);
					mueve = glm::vec3(sinf(u * 70.0f), 0.0f, cosf(u * 53.0f)) * 0.09f * tiembla + d * 0.25f * tiembla;
					brilloGolpe = 2.2f * expf(-u * 9.0f);
				}
			}
			const bool muere = (i == golpe.atacante && golpe.muereA) || (i == golpe.objetivo && golpe.muereO);
			// (rama classic-md-combate: en el duelo la destruida deja la zona a G_DESVANECE y viaja al cementerio; con figura 3D
			// la carta espera entera a que la figura muera y luego viaja: no se desvanece)
			if (muere && tg > G_DESVANECE && !(figuraEnZona && figuraEnZona(i))) {
				const float u = suave(G_DESVANECE, G_FIN - 0.1f, tg);
				alfa = 1.0f - u;
				mueve.y += 0.8f * u;
				brilloGolpe += 1.5f * u;
			}
		}
		const float D = control::DURACION_ANIM;
		const float ug = suave(0.0f, D, k.tGiro), uv = suave(0.0f, D, k.tVolteo);
		const float giro90 = glm::mix(k.antesDefensa ? 1.0f : 0.0f, k.defensa ? 1.0f : 0.0f, ug);
		const float pi = glm::pi<float>();
		const float phi = glm::mix(k.antesBocaAbajo ? pi : 0.0f, k.bocaAbajo ? pi : 0.0f, uv);
		const float g = glm::radians((rival ? 180.0f : 0.0f) + 90.0f * giro90);
		const glm::vec3 w0(cosf(g), 0.0f, -sinf(g)), h0(sinf(g), 0.0f, cosf(g)), arriba(0.0f, 1.0f, 0.0f);
		const bool volteando = k.antesBocaAbajo != k.bocaAbajo && k.tVolteo < D;
		float brilloMD = brilloActivaMD(i), miraMD = 0.0f;   // rama classic-md-ritmo: activa (brilla) o activandose (se levanta y mira a la camara)
		const float elevK = i == zonaElevada ? alzaElevada : 0.0f;   // rama classic-ps5-ataque5
		const float alto = alto0 + (volteando ? 0.7f * sinf(pi * uv) : 0.0f) + (alfa >= 1.0f ? alzaRitmoMD(i, brilloMD, miraMD) : 0.0f) + ELEV_ALTO * elevK;
		const glm::vec3 wf = w0 * cosf(phi) + arriba * sinf(phi);
		const bool cara = cosf(phi) >= 0.0f;
		const float brillo = 1.0f + 1.3f * expf(-k.tActiva * 2.5f) + 0.7f * expf(-k.tLlega * 4.0f) + brilloMD;
		int ci = cara && k.carta != "?" ? buscarCarta(k.carta) : -1;
		if (ci >= 0 && !listaCarta(ci)) ci = -1;
		glm::vec3 ancho = (cara ? wf : -wf) * (0.5f * CARTA_ANCHO), largoV = h0 * (0.5f * CARTA_ALTO);
		if (miraMD > 0.0f) {   // rama classic-md-ritmo (CardShow de MD): se para mirando a la camara (el pie de la imagen baja)
			const float am = glm::radians(55.0f) * miraMD, lv = glm::length(largoV);
			largoV = largoV * cosf(am) + arriba * (rival ? 1.0f : -1.0f) * lv * sinf(am);
		}
		if (inclina != 0.0f) {
			// inclinada hacia el objetivo: el borde que mira al objetivo baja (embestida) o sube (carga)
			ancho += arriba * (-glm::dot(ancho, haciaObj) * inclina);
			largoV += arriba * (-glm::dot(largoV, haciaObj) * inclina);
		}
		if (elevK > 0.0f) {   // la sombra de la carta elevada sobre la mesa (mas chica y tenue cuanto mas arriba) y un brillo ligero
			usar(setAtlas, true);
			const glm::vec3 s0 = centroZona(i) + mueve * glm::vec3(1.0f, 0.0f, 1.0f) + glm::vec3(0.0f, 0.046f, 0.0f);
			rectSuelo(s0 + glm::vec3(0.0f, 0.0f, rival ? -0.10f * elevK : 0.10f * elevK), glm::vec2(CARTA_ANCHO + 0.16f, CARTA_ALTO + 0.16f) * (1.0f + 0.06f * elevK), 0.18f, PANEL,
			          glm::vec4(0.0f, 0.0f, 0.0f, 0.40f * elevK), NADA, 10.0f, glm::vec4(0.0f), rival);
			brilloGolpe += 0.28f * elevK;
		}
		cartaPlano(centroZona(i) + glm::vec3(0.0f, alto, 0.0f) + mueve, ancho, largoV, ci, rival, brillo + brilloGolpe, alfa, (float)i * 0.37f);
	}

	void dibujarMesaMando()
	{
		dibujarActivasMD();   // rama classic-md-ritmo: el marco de luz de las magias/trampas activas (debajo de las cartas)
		const float D = control::DURACION_ANIM;
		for (int i = 0; i < (int)NUM_ZONAS; i++) {
			const std::string tipo = ZONAS[i].tipo;
			const bool rival = strncmp(ZONAS[i].id, "rival", 5) == 0;
			if (tipo == "cementerio" || tipo == "desterrado") {   // (rama classic-duelo-ui: tambien las desterradas)
				// la carta de arriba del cementerio, boca arriba sobre una pila tan alta como las cartas que tiene
				const auto& pila = tipo == "desterrado" ? (rival ? ctl->desterradosR : ctl->desterradosJ) : rival ? ctl->cementerioR : ctl->cementerioJ;
				if (!pila.empty()) {
					int ci = buscarCarta(pila.back());
					if (ci >= 0 && !listaCarta(ci)) ci = -1;
					cartaSuelo(centroZona(i) + glm::vec3(0.0f, 0.045f + 0.004f * (float)pila.size(), 0.0f), rival, ci, (float)i * 0.37f);
				}
				continue;
			}
			if (tipo != "monstruo" && tipo != "magia") continue;   // (la carta de campo la dibuja el Bosque, rama campo-bosque)
			const control::Celda& k = ctl->zonas[i];
			const Ocupa* h = horneada(ZONAS[i].id);
			const bool anim = k.tVolteo < D || k.tGiro < D || k.tLlega < 0.4f || k.tActiva < 1.5f || (golpe.t >= 0.0f && (i == golpe.atacante || i == golpe.objetivo));
			// rama classic-duelo-arreglos (fallo "la carta colocada desaparece"): con el duelo real la carta de la mesa se dibuja
			// siempre, con la casilla de vidrio que tapa la del tablero; antes, si coincidia con la horneada en el tablero.glb,
			// no se dibujaba y quedaba a merced de lo que pinta encima del tapete (capa de la carta de campo, bosque, entorno)
			const bool igual = !ctl->duelo && h && !k.vacia() && h->bocaAbajo == k.bocaAbajo && h->defensa == k.defensa && (h->bocaAbajo || k.carta == h->carta);
			if (tipo != "magia" && igual && !anim) continue;   // la del tablero ya es esa (las magias se dibujaban siempre encima)
			// (rama classic-ps5-arreglos1: con el duelo real el tablero ya no trae cartas horneadas: sin casilla de vidrio)
			if (h && !ctl->duelo && (k.vacia() || !igual || anim)) {
				// casilla de vidrio que tapa la carta del tablero (vacia, girada o animandose)
				const glm::vec4 lado = rival ? CARMESI : CIAN;
				usar(setAtlas, true);
				rectSuelo(centroZona(i) + glm::vec3(0.0f, 0.041f, 0.0f), glm::vec2(CARTA_ANCHO + 0.14f, CARTA_ALTO + 0.14f), 0.0f, PANEL,
				          glm::vec4(0.030f, 0.044f, 0.082f, 1.0f), glm::vec4(glm::vec3(lado) * 0.7f, 1.0f), 9.0f, glm::vec4(2.5f, 0.9f, 0.0f, 0.0f), rival);
			}
			if (!k.vacia()) cartaZona(i, k, rival);
		}
		usar(setAtlas, true);
		// zonas elegibles: doradas para dejar una carta, carmesi para atacar; la del atacante en cian
		if (ctl->estado == control::ELEGIR_ZONA || ctl->estado == control::ELEGIR_OBJETIVO || ctl->estado == control::ELEGIR_TRIBUTOS) {
			const bool ataque = ctl->estado == control::ELEGIR_OBJETIVO;
			const bool sacrificio = ctl->estado == control::ELEGIR_TRIBUTOS;
			for (int z : ctl->validas) {
				// sacrificios: los elegidos en carmesi, los demas en violeta
				const bool elegido = std::find(ctl->tributos.begin(), ctl->tributos.end(), z) != ctl->tributos.end();
				const glm::vec4 col = ataque || (sacrificio && elegido) ? CARMESI : sacrificio ? glm::vec4(0.72f, 0.42f, 1.0f, 1.0f) : ORO;
				const bool enFoco = ctl->foco.tipo == control::F_ZONA && ctl->foco.i == z;
				const float pulso = 0.75f + 0.25f * sinf(t * 5.0f + (float)z);
				rectSuelo(centroZona(z) + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec2(ZONA_ANCHO, ZONA_ALTO) * (enFoco ? 1.06f : 1.0f), 0.25f, ZONA, col,
				          glm::vec4(glm::vec3(col) * (enFoco ? 0.5f : 0.25f), 1.0f), 10.0f, glm::vec4(enFoco ? 9.0f : 6.0f, 0.8f, pulso * (enFoco ? 1.3f : 0.9f), 0.0f));
			}
			if (ataque && ctl->origenZona >= 0) {
				rectSuelo(centroZona(ctl->origenZona) + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec2(ZONA_ANCHO, ZONA_ALTO), 0.25f, ZONA, CIAN,
				          glm::vec4(glm::vec3(CIAN) * 0.3f, 1.0f), 10.0f, glm::vec4(7.0f, 1.2f, 1.1f, 0.0f));
			}
			// la carta "fantasma" en la zona con el foco: como va a quedar (cara o dorso, de lado si es de defensa)
			if (!ataque && !sacrificio && ctl->foco.tipo == control::F_ZONA && ctl->origenMano >= 0 && ctl->origenMano < (int)ctl->manoJ.size()) {
				control::Celda fantasma;
				fantasma.carta = ctl->manoJ[ctl->origenMano];
				fantasma.bocaAbajo = fantasma.antesBocaAbajo = ctl->pendiente == control::AC_COLOCAR_MONSTRUO || ctl->pendiente == control::AC_COLOCAR_MT;
				fantasma.defensa = fantasma.antesDefensa = ctl->pendiente == control::AC_COLOCAR_MONSTRUO;
				cartaZona(ctl->foco.i, fantasma, false, 0.45f + 0.15f * sinf(t * 4.0f), 0.07f);
			}
			usar(setAtlas, true);
		}
	}

	// ------------------------------------------------------------------------------ encima de todo (2D)
	// el numero de danno del ataque plano: salta con el golpe sobre el objetivo, sube y se apaga
	void dibujarDanio()
	{
		if (golpe.t < G_IMPACTO || golpe.danio <= 0 || golpe.objetivo < 0) return;
		// rama classic-ps5-ataque4 (el usuario: "la resta o suma del calculo de danno no se ve en las cartas, solo se ve
		// reflejada en los LP generales", como Joey: el dmg_ref y los LP van en el marcador de cada jugador,
		// FUN_004142c0 paso 6 en FUN_0045c140(jugador)): en el duelo no hay numero sobre la carta
		if (ctl && ctl->vista.pantalla == control::VistaDuelo::DUELO) return;
		if (numeroLPActivoMD(!golpe.alJugador, -golpe.danio)) return;   // rama classic-ps5-ataque: MD muestra UN numero (el de LPCounterSub)
		// rama classic-md-ritmo: el numero de Master Duel (prefab ChangeValue de LPCounterSub): entra a escala 2 en 0.02 s,
		// a los 0.56 s baja a 1.5; sube 80 px en 0.1 s tras 0.2 s; se disuelve en 0.2 s (visible ~0.8 s)
		const float u = golpe.t - G_IMPACTO;
		const float a = 1.0f - suave(0.6f, 0.8f, u);
		if (a <= 0.0f) return;
		const glm::vec2 p = aPantalla(proyectar(destinoGolpe() + glm::vec3(0.0f, 0.6f, 0.0f))) - glm::vec2(0.0f, 80.0f * s * suave(0.2f, 0.3f, u));
		const float esc = u < 0.02f ? 2.0f * u / 0.02f : u < 0.56f ? 2.0f : 1.5f;
		const float tam = 40.0f * esc * s;
		texto(F_NUMEROS, "-" + std::to_string(golpe.danio), p.x, p.y, tam, glm::vec4(1.0f, 0.9f, 0.55f, a), glm::vec4(CARMESI.r, CARMESI.g, CARMESI.b, a),
		      glm::vec3(0.6f, 0.0f, 1.0f), 1);
	}

	void dibujarMandoEncima()
	{
		if (!ctl) return;
		dibujarDanio();
		dibujarMarcoDannoMD();   // rama classic-ps5-ataque: el DamageFrame de MD cuando el danno del combate es del jugador (ataque_md_ui.h)
		dibujarCalculoMD();      // rama classic-ps5-ataque4: el calculo de danno sobre las cartas que chocan (ataque_md_ui.h)
		dibujarRobo();        // rama classic-duelo-arreglos: del mazo a la mano
		dibujarVuelo();
		dibujarRitmoMD();     // rama classic-md-ritmo: viajes, activaciones, eslabones y contadores (md_ritmo_ui.h)
		dibujarObjetivoAtaque();
		dibujarFaseMando();
		dibujarCartelInicioMD();  // rama classic-md-fases: el cartel de inicio del duelo (DuelTextStart de MD)
		dibujarCartelTurnoMD();   // rama classic-md-fases: el cartel de turno de Master Duel (fases_md_ui.h)
		dibujarCambioLPMD();      // rama classic-md-fases: el valor que cambia los LP (LPCounterSub de MD)
		dibujarAvisoMando();
		dibujarCorteMando();   // rama classic-ps5-arreglos1: la carta del rival en el turno del jugador
		// rama classic-ps5-control2 (PS5: "salio 'responde a la pregunta' y no se cual es la pregunta"): el menu y la
		// pregunta del duelo van ENCIMA de los carteles de fase / turno y del corte del rival (antes quedaban debajo)
		dibujarMenuMando();
		dibujarListaCartas();    // rama classic-ps5-elegir-cartas: el selector de cartas de MD (lista_cartas_ui.h)
		dibujarPreguntaFija();   // (pregunta_ui.h)
		dibujarDetalleMando();
		dibujarBotonFase();   // rama classic-duelo-arreglos / classic-md-fases: la placa FASES (OPTIONS)
		dibujarEngranajeMD();     // rama classic-md-fases: el engranaje (menu del duelo), abajo a la izquierda
		dibujarSelectorFasesMD();   // rama classic-md-fases: el selector de fases de Master Duel (OPTIONS)
		dibujarAyudaMando();
	}

	// rama classic-duelo-arreglos: la carta que se roba sale del mazo (arriba de la pila), sube en arco y va a su lugar en
	// la mano; la del jugador se da vuelta en el aire (dorso -> cara), la del rival llega boca abajo
	int serieRobo{ -1 };
	glm::vec2 roboDesde{ 0.0f };
	void dibujarRobo()
	{
		const control::Mando::Robo& ro = ctl->robo;
		if (!ro.activo || ro.indice < 0) return;
		if (ro.turno && !ro.rival && dibujarRoboTurnoMD()) return;   // (rama classic-ps5-ritmo: el robo de la Fase de Robo de MD, ritmo_turno_ui.h)
		const bool rival = ro.rival;
		if (ro.serie != serieRobo) {
			serieRobo = ro.serie;
			const Zona* z = zonaPorId(rival ? "rival_mazo" : "jugador_mazo");
			const int n = rival ? mazoR : mazoJ;
			roboDesde = z ? aPantalla(proyectar(glm::vec3(z->x * ESCALA, 0.016f * (float)n + 0.05f, z->z * ESCALA))) : glm::vec2(W * 0.85f, rival ? 0.0f : H);
		}
		glm::vec2 p1;
		float a1, e1;
		if (rival) cartaManoR(ro.indice, p1, a1, e1);
		else cartaManoJ(ro.indice, p1, a1, e1);
		const float pi = glm::pi<float>();
		const float u = suave(0.0f, 1.0f, ro.t);
		const glm::vec2 p = glm::mix(roboDesde, p1, u) + glm::vec2(0.0f, (rival ? 90.0f : -150.0f) * s * sinf(pi * u));
		const glm::vec2 tam1 = (rival ? glm::vec2(120.0f, 174.0f) : glm::vec2(150.0f, 218.0f)) * s * e1;
		glm::vec2 tam = glm::mix(tam1 * 0.55f, tam1, u) * (1.0f + 0.12f * sinf(pi * u));
		const float ang = glm::mix(rival ? pi : 0.0f, a1, u);
		const bool cara = !rival && u > 0.5f;
		if (!rival) tam.x *= glm::max(fabsf(cosf(pi * u)), 0.04f);   // (se da vuelta a mitad de camino)
		usar(setAtlas, false);
		rect(p + glm::vec2(0.0f, 12.0f * s), tam, ang, 18.0f * s, PANEL, glm::vec4(0, 0, 0, 0.45f), NADA, 9.0f * s, glm::vec4(0, 0, 0, 12.0f * s));
		const int ci = cara ? buscarCarta(ro.carta) : -1;
		if (ci >= 0 && listaCarta(ci)) {
			usar(cartas[ci].setMini, false);
			const float uu = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
			rect(p, tam, ang, 0.0f, IMAGEN, glm::vec4(glm::vec3(1.05f), 1.0f), CIAN, 8.0f * s, glm::vec4(0.5f, 1.0f, 0.2f, 0.0f), glm::vec2(uu, vv),
			     glm::vec2(1.0f - uu, 1.0f - vv));
		} else {
			const Dorso dz = usarDorso(false, false, rival, 1.0f, 0.0f);
			rect(p, tam, ang, 0.0f, DORSO, dz.col, dz.col2, 8.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		usar(setAtlas, false);
		// estela de luz
		const glm::vec4 brillo = rival ? CARMESI : CIAN;
		rect(p, tam * 1.3f, ang, 0.0f, PARTICULA, glm::vec4(glm::vec3(brillo) * 0.25f * sinf(pi * u), 1.0f), NADA, 0.0f, glm::vec4(0.0f));
	}

	// rama classic-duelo-arreglos / classic-md-fases: la placa del boton de fase, siempre a la vista en TU turno cuando
	// se puede cambiar de fase (a la derecha, sobre la barra de ayuda): el icono de OPTIONS, "FASES" y la primera fase
	// a la que se puede ir ("Iniciar Fase de Batalla", "Ir a M2" o "Finalizar turno"); OPTIONS abre el selector
	void dibujarBotonFase()
	{
		const char* txt = ctl->botonFase();
		if (!txt || !*txt || entrada::estado().activa) return;   // (classic-visual-final: la placa de la entrada clave va abajo)
		const bool fin = strcmp(txt, "Finalizar turno") == 0;
		const std::string titulo = idioma::tr("FASES"), sub = idioma::tr(txt);
		const float h = 74.0f * s;
		const float w = anchoPlaca(titulo, sub, "C|OPTIONS");
		// (rama classic-visual-final) siempre encima de la pila de la barra de ayuda, la tenga en 1, 2 o 3 filas
		const float x0 = W - 22.0f * s - w;
		float y0 = glm::min(topeAyuda(planAyuda()), H - 22.0f * s - 42.0f * s) - 16.0f * s - h;
		for (int k = 0; k < nManoJ(); k++) {   // (y sin pisar las cartas de la mano que quedan debajo)
			glm::vec2 c;
			float an, e;
			cartaManoJ(k, c, an, e);
			if (c.x + 80.0f * s > x0) y0 = glm::min(y0, c.y + e * 70.0f * s - 115.0f * s - 12.0f * s - h);   // (sin la elevacion: no salta)
		}
		placaBoton("C|OPTIONS", titulo, sub, fin ? CARMESI : ORO, y0);   // (rama classic-ps5-control2: el cuadrado tambien abre las fases)
	}

	float anchoPlaca(const std::string& titulo, const std::string& sub, const std::string& icono = "")
	{
		const float extra = icono.empty() ? 0.0f : anchoIcono(icono, 20.0f * s) - 2.0f * 20.0f * s;
		return glm::max(medir(F_NUMEROS, titulo, 24.0f * s, 2.0f), medir(F_TITULO, sub, 19.0f * s, 1.0f)) + 2.0f * 20.0f * s + 64.0f * s + extra;
	}
	// (rama classic-ps5-control2) el ancho que ocupa iconoBoton (con "A|B", los dos)
	float anchoIcono(const std::string& b, float r)
	{
		const size_t barra = b.find('|');
		if (barra != std::string::npos) return anchoIcono(b.substr(0, barra), r) + 6.0f * s + anchoIcono(b.substr(barra + 1), r);
		if (dispositivo == 2 && (b == "X" || b == "O" || b == "T" || b == "C")) return 2.0f * r;
		const std::string et = etiquetaDispositivo(b);   // rama classic-pc
		if (!et.empty()) return medir(F_NUMEROS, et, 14.0f * s, 1.0f) + 14.0f * s;
		if (b == "X" || b == "O" || b == "T" || b == "C" || b == "CRUZ" || b == "STICK_R") return 2.0f * r;
		return medir(F_NUMEROS, b == "L1R1" ? "L1 R1" : idioma::tr(b.c_str()), 14.0f * s, 1.0f) + 14.0f * s;
	}

	// rama classic-pc: el nombre del boton en el ultimo dispositivo usado ("" = se dibuja el de PlayStation de siempre)
	std::string etiquetaDispositivo(const std::string& b) const
	{
		if (dispositivo == 1) {   // teclado y raton (platform: pc/plataforma_windows.c, tabla de teclas del LEEME)
			const bool en = idioma::ingles();
			if (b == "X") return "Enter";
			if (b == "O") return "Esc";
			if (b == "T") return "T";
			if (b == "C") return "F";
			if (b == "CRUZ") return en ? "Arrows" : "Flechas";
			if (b == "STICK_R") return "IJKL";
			if (b == "L1") return "Q";
			if (b == "R1") return "E";
			if (b == "L2") return "1";
			if (b == "R2") return "3";
			if (b == "L1R1") return "Q E";
			if (b == "OPTIONS") return "Tab";
			if (b == "TÁCTIL" || b == "TACTIL" || b == "TOUCH") return "P";
			if (b == "L3") return "Z";
			if (b == "R3") return "C";
			return "";
		}
		if (dispositivo == 2) {   // Xbox
			if (b == "X") return "A";
			if (b == "O") return "B";
			if (b == "T") return "Y";
			if (b == "C") return "X";
			if (b == "L1") return "LB";
			if (b == "R1") return "RB";
			if (b == "L2") return "LT";
			if (b == "R2") return "RT";
			if (b == "L1R1") return "LB RB";
			if (b == "OPTIONS") return "MENU";
			if (b == "TÁCTIL" || b == "TACTIL" || b == "TOUCH") return "VIEW";
			if (b == "L3") return "LS";
			if (b == "R3") return "RS";
			return "";
		}
		return "";
	}

	// rama classic-pc: cada zona del tablero (centro proyectado a pantalla, caja de sus cuatro esquinas) lleva el foco a ella
	void registrarZonasMesa()
	{
		if (!ctl || rz.tapas > 0) return;
		for (int i = 0; i < (int)NUM_ZONAS; i++) {
			const glm::vec3 c = centroZona(i);
			glm::vec2 lo(1e9f), hi(-1e9f);
			for (int q = 0; q < 4; q++) {
				const glm::vec4 pc = proyectar(c + glm::vec3((q & 1 ? 0.5f : -0.5f) * ZONA_ANCHO, 0.0f, (q & 2 ? 0.5f : -0.5f) * ZONA_ALTO));
				if (pc.w <= 0.0f) { lo = glm::vec2(1e9f); hi = glm::vec2(-1e9f); break; }
				const glm::vec2 p = aPantalla(pc);
				lo = glm::min(lo, p);
				hi = glm::max(hi, p);
			}
			if (lo.x > hi.x) continue;
			zFoco(lo.x, lo.y, hi.x - lo.x, hi.y - lo.y, control::F_ZONA, i);
		}
	}

	// rama classic-pc: el boton de la ayuda como zona del raton (clic = apretar ese boton)
	static uint32_t botonDeIcono(const std::string& b)
	{
		if (b == "X") return control::B_X;
		if (b == "O") return control::B_CIRCULO;
		if (b == "T") return control::B_TRIANGULO;
		if (b == "C") return control::B_CUADRADO;
		if (b == "L1") return control::B_L1;
		if (b == "R1") return control::B_R1;
		if (b == "L3") return control::B_L3;
		if (b == "R3") return control::B_R3;
		if (b == "OPTIONS") return control::B_OPTIONS;
		if (b == "TÁCTIL" || b == "TACTIL" || b == "TOUCH") return control::B_TACTIL;
		return 0;
	}

	// una placa de boton del HUD (fase / terminar turno): icono, titulo y subtitulo, alineada a la derecha en y0
	void placaBoton(const char* icono, const std::string& titulo, const std::string& sub, const glm::vec4& borde, float y0)
	{
		const float r = 20.0f * s, h = 74.0f * s;
		const float w = anchoPlaca(titulo, sub, icono);
		const float x0 = W - 22.0f * s - w;
		const float lat = 0.75f + 0.25f * sinf(t * 3.0f);
		usar(setAtlas, false);
		caja(x0, y0, w, h, 22.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.86f), glm::vec4(glm::vec3(borde) * lat, 1.0f), h * 0.5f,
		     glm::vec4(1.5f * s, 1.1f, 0.15f, 16.0f * s));
		if (rz.tapas == 0) zBoton(x0, y0, w, h, control::B_CUADRADO, 1);   // rama classic-pc: la placa de la fase abre el selector
		const float wi = iconoBoton(glm::vec2(x0 + 22.0f * s + r, y0 + h * 0.5f), icono, r);
		const float tx = x0 + 22.0f * s + glm::max(wi, 2.0f * r) + 18.0f * s;
		texto(F_NUMEROS, titulo, tx, y0 + 33.0f * s, 24.0f * s, glm::vec4(1.0f, 0.95f, 0.8f, 1.0f), borde, glm::vec3(0.4f, 0.0f, 0.6f), 0, 2.0f);
		texto(F_TITULO, sub, tx, y0 + 60.0f * s, 19.0f * s, glm::vec4(0.82f, 0.88f, 0.96f, 1.0f), NADA, glm::vec3(0.0f), 0, 1.0f);
	}

	// la carta que vuela de la mano a su zona: en arco, achicandose a su tamanno en la mesa; si se coloca, se da
	// vuelta en el aire (y gira de lado si es un monstruo en defensa); deja una estela de chispas doradas
	void dibujarVuelo()
	{
		const control::Mando::Vuelo& vu = ctl->vuelo;
		if (!vu.activo || vu.zona < 0) return;
		if (vu.serie != serieVuelo) {
			serieVuelo = vu.serie;
			vueloDesde = vu.desdeMano >= 0 && vu.desdeMano < MAX_MANO_J ? posManoJ[vu.desdeMano] : glm::vec2(W * 0.6f, H);
		}
		const float u = suave(0.0f, 1.0f, vu.t);
		const glm::vec3 zc = centroZona(vu.zona) + glm::vec3(0.0f, 0.05f, 0.0f);
		const glm::vec2 p1 = aPantalla(proyectar(zc));
		const float h1 = glm::max(glm::length(aPantalla(proyectar(zc + glm::vec3(0.0f, 0.0f, CARTA_ALTO * 0.5f))) -
		                                      aPantalla(proyectar(zc - glm::vec3(0.0f, 0.0f, CARTA_ALTO * 0.5f)))), 30.0f * s);
		const glm::vec2 tam0 = glm::vec2(150.0f, 218.0f) * s * 1.14f, tam1(h1 * 150.0f / 218.0f, h1);
		const float pi = glm::pi<float>();
		glm::vec2 p = glm::mix(vueloDesde, p1, u) - glm::vec2(0.0f, 170.0f * s * sinf(pi * u));
		glm::vec2 tam = glm::mix(tam0, tam1, u) * (1.0f + 0.15f * sinf(pi * u));
		vueloMD(vu, vueloDesde, p1, tam0, tam1, p, tam);   // rama classic-md-ritmo: el movimiento de Master Duel (Hand2MonstorZone / Hand2MagicZone)
		const float ang = vu.girada ? -0.5f * pi * u : 0.0f;
		const bool cara = !vu.bocaAbajo || u < 0.5f;
		if (vu.bocaAbajo) tam.x *= glm::max(fabsf(cosf(pi * u)), 0.03f);
		usar(setAtlas, false);
		rect(p + glm::vec2(0.0f, 14.0f * s), tam, ang, 20.0f * s, PANEL, glm::vec4(0, 0, 0, 0.5f), NADA, 8.0f * s, glm::vec4(0, 0, 0, 14.0f * s));
		const int ci = cara ? buscarCarta(vu.carta) : -1;
		if (ci >= 0 && listaCarta(ci)) {
			usar(cartas[ci].setMini, false);
			const float uu = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
			rect(p, tam, ang, 0.0f, IMAGEN, glm::vec4(glm::vec3(1.1f), 1.0f), ORO, 8.0f * s, glm::vec4(0.6f, 1.0f, 0.3f, 0.0f), glm::vec2(uu, vv),
			     glm::vec2(1.0f - uu, 1.0f - vv));
		} else {
			const Dorso dz = usarDorso(false, false, false, 1.0f, 0.0f);
			rect(p, tam, ang, 0.0f, DORSO, dz.col, dz.col2, 8.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		for (int k = 0; k < 3; k++) {
			chispa(p + glm::vec2(azar() - 0.5f, azar() - 0.5f) * tam, glm::vec2((azar() - 0.5f) * 60.0f, -(20.0f + 60.0f * azar())) * s, 0.4f + 0.5f * azar(),
			       (1.5f + 2.5f * azar()) * s, glm::mix(glm::vec3(1.0f, 0.75f, 0.25f), glm::vec3(1.0f, 0.95f, 0.7f), azar()), azar() > 0.8f ? 1.0f : 0.0f);
		}
	}

	// eligiendo a quien atacar: un arco de luz del atacante al objetivo y una flecha carmesi sobre el objetivo
	void dibujarObjetivoAtaque()
	{
		if (ctl->estado != control::ELEGIR_OBJETIVO || ctl->origenZona < 0 || ctl->foco.tipo != control::F_ZONA) return;
		const glm::vec3 a = centroZona(ctl->origenZona), b = centroZona(ctl->foco.i);
		usar(setAtlas, false);
		const int N = 22;
		for (int k = 0; k < N; k++) {
			const float u = fmodf((float)k / (float)N + t * 0.6f, 1.0f);
			const glm::vec3 q = glm::mix(a, b, u) + glm::vec3(0.0f, 0.4f + 2.2f * sinf(glm::pi<float>() * u), 0.0f);
			const glm::vec4 c = proyectar(q);
			if (c.w <= 0.1f) continue;
			const float al = sinf(glm::pi<float>() * u);
			rect(aPantalla(c), glm::vec2(9.0f * s), 0.0f, 9.0f * s, PARTICULA, glm::vec4(glm::mix(glm::vec3(CIAN), glm::vec3(CARMESI), u) * 1.4f, al), NADA, 0.0f,
			     glm::vec4(0.0f));
		}
		const glm::vec2 pb = aPantalla(proyectar(b + glm::vec3(0.0f, 3.2f, 0.0f)));
		const float bob = sinf(t * 5.0f) * 6.0f * s;
		rect(pb + glm::vec2(0.0f, bob), glm::vec2(40.0f * s), 0.0f, 14.0f * s, FLECHA, CARMESI, glm::vec4(1.0f, 0.8f, 0.85f, 1.0f), 0.0f, glm::vec4(0.0f));
	}

	// un boton de PlayStation dibujado por codigo (centro, radio): X azul, circulo rojo, triangulo verde, cuadrado
	// rosa sobre una ficha oscura; CRUZ = cruceta; STICK_R = palanca derecha; lo demas, una tecla con su nombre
	float iconoBoton(glm::vec2 c, const std::string& b, float r, float alfa = 1.0f)
	{
		const float w = iconoBotonDibujo(c, b, r, alfa);
		if (alfa >= 0.4f && b.find('|') == std::string::npos) {   // rama classic-pc: clic en el icono = apretar ese boton
			const uint32_t bt = botonDeIcono(b);
			if (bt) zBoton(c.x - r, c.y - r, std::max(w, 2.0f * r), 2.0f * r, bt, 1);
		}
		return w;
	}

	// rama classic-pc: un boton que no es el de PlayStation (teclado: la tecla; Xbox: A B X Y en su color, el resto como tecla)
	float iconoOtroDispositivo(glm::vec2 c, const std::string& b, const std::string& et, float r, float alfa)
	{
		usar(setAtlas, false);
		auto A = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * alfa); };
		const glm::vec4 ficha(0.05f, 0.07f, 0.12f, 1.0f), aro(0.70f, 0.76f, 0.88f, 1.0f);
		if (dispositivo == 2 && (b == "X" || b == "O" || b == "T" || b == "C")) {
			const glm::vec4 col = b == "X" ? glm::vec4(0.45f, 0.85f, 0.35f, 1.0f) : b == "O" ? glm::vec4(1.0f, 0.38f, 0.40f, 1.0f)
			                    : b == "T" ? glm::vec4(1.0f, 0.85f, 0.25f, 1.0f) : glm::vec4(0.40f, 0.65f, 1.0f, 1.0f);
			rect(c, glm::vec2(r * 2.0f), 0.0f, 5.0f * s, ORBE, A(aro), A(ficha), 0.0f, glm::vec4(0.25f, 1.0f, 0.0f, 0.0f));
			texto(F_NUMEROS, et, c.x, c.y + r * 0.36f, r * 1.15f, A(col), NADA, glm::vec3(0.0f), 1, 1.0f);
			return r * 2.0f;
		}
		const float w = medir(F_NUMEROS, et, 14.0f * s, 1.0f) + 14.0f * s;
		rect(c + glm::vec2(w * 0.5f - r, 0.0f), glm::vec2(w, r * 1.5f), 0.0f, 4.0f * s, PANEL, A(ficha), A(aro), 6.0f * s, glm::vec4(1.0f * s, 1.0f, 0.0f, 0.0f));
		texto(F_NUMEROS, et, c.x + w * 0.5f - r, c.y + 5.0f * s, 14.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 1, 1.0f);
		return w;
	}

	float iconoBotonDibujo(glm::vec2 c, const std::string& b, float r, float alfa)
	{
		if (dispositivo != 0 && b.find('|') == std::string::npos) {   // rama classic-pc
			const std::string et = etiquetaDispositivo(b);
			if (!et.empty()) return iconoOtroDispositivo(c, b, et, r, alfa);
		}
		// (rama classic-ps5-control2: dos botones que hacen lo mismo, "C|OPTIONS": el cuadrado y OPTIONS abren las fases)
		const size_t barra = b.find('|');
		if (barra != std::string::npos) {
			const float w1 = iconoBoton(c, b.substr(0, barra), r, alfa);
			const float sep = 6.0f * s;
			return w1 + sep + iconoBoton(c + glm::vec2(w1 + sep, 0.0f), b.substr(barra + 1), r, alfa);
		}
#if defined(YUGI_REGISTRO_DIBUJO)
		if (alDibujarTexto) alDibujarTexto("[" + b + "]", c.x, c.y, r * 2.0f, glm::vec4(1.0f, 1.0f, 1.0f, alfa), 1, v.size());   // (rama classic-jugador-humano)
#endif
		usar(setAtlas, false);
		auto A = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * alfa); };
		const glm::vec4 ficha(0.05f, 0.07f, 0.12f, 1.0f), aro(0.70f, 0.76f, 0.88f, 1.0f);
		if (b == "X" || b == "O" || b == "T" || b == "C") {
			rect(c, glm::vec2(r * 2.0f), 0.0f, 5.0f * s, ORBE, A(aro), A(ficha), 0.0f, glm::vec4(0.25f, 1.0f, 0.0f, 0.0f));
			const float k = r * 0.5f;
			if (b == "X") {
				const glm::vec4 azul(0.55f, 0.72f, 1.0f, 1.0f);
				for (float g : { 0.25f, -0.25f }) {
					rect(c, glm::vec2(k * 2.1f, 2.8f * s), g * glm::pi<float>(), 0.0f, PANEL, A(azul), NADA, 1.4f * s, glm::vec4(0.0f));
				}
			} else if (b == "O") {
				rect(c, glm::vec2(k * 1.75f), 0.0f, 0.0f, PANEL, NADA, A(glm::vec4(1.0f, 0.38f, 0.45f, 1.0f)), k * 0.875f, glm::vec4(2.6f * s, 2.4f, 0.0f, 0.0f));
			} else if (b == "C") {
				rect(c, glm::vec2(k * 1.55f), 0.0f, 0.0f, PANEL, NADA, A(glm::vec4(1.0f, 0.55f, 0.85f, 1.0f)), 1.5f * s, glm::vec4(2.6f * s, 2.4f, 0.0f, 0.0f));
			} else {
				const glm::vec4 verde(0.35f, 0.95f, 0.70f, 1.0f);
				glm::vec2 p[3];
				for (int i = 0; i < 3; i++) {
					const float a = glm::radians(120.0f * (float)i);
					p[i] = c + glm::vec2(0.0f, k * 0.15f) + k * 1.0f * glm::vec2(sinf(a), -cosf(a));
				}
				for (int i = 0; i < 3; i++) {
					const glm::vec2 d = p[(i + 1) % 3] - p[i];
					rect((p[i] + p[(i + 1) % 3]) * 0.5f, glm::vec2(glm::length(d) + 2.6f * s, 2.6f * s), atan2f(d.y, d.x), 0.0f, PANEL, A(verde), NADA, 1.3f * s,
					     glm::vec4(0.0f));
				}
			}
			return r * 2.0f;
		}
		if (b == "CRUZ") {
			const glm::vec4 gris(0.80f, 0.85f, 0.95f, 1.0f);
			rect(c, glm::vec2(r * 1.8f, r * 0.62f), 0.0f, 3.0f * s, PANEL, A(gris), NADA, 2.5f * s, glm::vec4(0.0f));
			rect(c, glm::vec2(r * 0.62f, r * 1.8f), 0.0f, 3.0f * s, PANEL, A(gris), NADA, 2.5f * s, glm::vec4(0.0f));
			rect(c, glm::vec2(r * 0.34f), 0.0f, 0.0f, ORBE, A(ficha), A(ficha), 0.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
			return r * 2.0f;
		}
		if (b == "STICK_R") {
			rect(c, glm::vec2(r * 2.0f), 0.0f, 5.0f * s, ORBE, A(aro), A(ficha), 0.0f, glm::vec4(0.25f, 1.0f, 0.0f, 0.0f));
			rect(c, glm::vec2(r * 1.3f), 0.0f, 0.0f, ORBE, A(glm::vec4(0.45f, 0.55f, 0.7f, 1.0f)), A(glm::vec4(0.12f, 0.15f, 0.22f, 1.0f)), 0.0f,
			     glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
			texto(F_NUMEROS, "R", c.x, c.y + 5.0f * s, 14.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 1);
			return r * 2.0f;
		}
		const std::string nombre = b == "L1R1" ? "L1 R1" : idioma::tr(b.c_str());   // (rama classic-md-fases: TÁCTIL / TOUCH)
		const float w = medir(F_NUMEROS, nombre, 14.0f * s, 1.0f) + 14.0f * s;
		rect(c + glm::vec2(w * 0.5f - r, 0.0f), glm::vec2(w, r * 1.5f), 0.0f, 4.0f * s, PANEL, A(ficha), A(aro), 6.0f * s, glm::vec4(1.0f * s, 1.0f, 0.0f, 0.0f));
		texto(F_NUMEROS, nombre, c.x + w * 0.5f - r, c.y + 5.0f * s, 14.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 1, 1.0f);
		return w;
	}

	// menu contextual de vidrio junto a la carta elegida: el nombre de la carta y sus acciones; la elegida en dorado
	// (las que no se pueden, apagadas)
	void dibujarMenuMando()
	{
		if (ctl->estado != control::MENU || ctl->opciones.empty()) return;
		if (ctl->listaCartas()) return;   // rama classic-ps5-elegir-cartas: las cartas, no una lista de nombres (lista_cartas_ui.h)
		const float a = suave(0.0f, 0.16f, ctl->tMenu);
		const int nTotal = (int)ctl->opciones.size();
		const float fila = 44.0f * s, w = 310.0f * s, cab = 50.0f * s;
		// (rama classic-ps5-control2: una lista larga -elegir del cementerio, del mazo- no se sale de la pantalla: se ve una
		// ventana de filas alrededor de la elegida, con flechas arriba / abajo si hay mas)
		const int maxFilas = glm::max(3, (int)((H - 24.0f * s - cab - 12.0f * s) / fila));
		const int n = glm::min(nTotal, maxFilas);
		const int ini = nTotal <= n ? 0 : glm::clamp(ctl->opcion - n / 2, 0, nTotal - n);
		const float h = cab + fila * (float)n + 12.0f * s;
		float x0, y0;
		if (ctl->foco.tipo == control::F_MANO_J && ctl->foco.i < nManoJ()) {
			glm::vec2 c;
			float an, e;
			cartaManoJ(ctl->foco.i, c, an, e);
			x0 = c.x - w * 0.5f;
			y0 = c.y - 218.0f * s * e * 0.5f - 22.0f * s - h;
		} else {
			const int z = ctl->foco.tipo == control::F_ZONA ? ctl->foco.i : 0;
			const glm::vec2 p = aPantalla(proyectar(centroZona(z) + glm::vec3(0.0f, 0.3f, 0.0f)));
			const glm::vec2 q = aPantalla(proyectar(centroZona(z) + glm::vec3(ZONA_ANCHO * 0.5f, 0.3f, 0.0f)));
			x0 = p.x + glm::max(glm::length(q - p), 30.0f * s) + 34.0f * s;
			y0 = p.y - h * 0.5f;
			if (x0 + w > W - 12.0f * s) x0 = p.x - glm::max(glm::length(q - p), 30.0f * s) - 34.0f * s - w;   // a la izquierda si no entra
		}
		x0 = glm::clamp(x0, CAMPO_X * s + 12.0f * s, W - w - 12.0f * s);
		y0 = glm::clamp(y0, 12.0f * s, H - h - 12.0f * s) + (1.0f - a) * 14.0f * s;
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		usar(setAtlas, false);
		caja(x0, y0, w, h, 26.0f * s, PANEL, A(VIDRIO), A(glm::mix(CIAN, ORO, 0.55f)), 16.0f * s, glm::vec4(1.4f * s, 0.9f, 0.09f, 18.0f * s));
		rz.tapar();   // rama classic-pc: el menu de acciones se queda con el raton
		for (int f = 0; f < n; f++) zLista(x0 + 10.0f * s, y0 + cab + fila * (float)f + 3.0f * s, w - 20.0f * s, fila - 6.0f * s, ini + f, ctl->opcion);
		const int ci = buscarCarta(ctl->tituloMenu);
		std::string titulo = ci >= 0 ? cartas[ci].vNombre() : !ctl->tituloTexto.empty() ? ctl->tituloTexto : "ACCIONES";   // (rama classic-efectos: la pregunta)
		float tt = 22.0f * s;
		while (medir(F_TITULO, titulo, tt, 1.0f) > w - 36.0f * s && tt > 14.0f * s) tt -= 1.0f * s;
		texto(F_TITULO, titulo, x0 + 18.0f * s, y0 + 32.0f * s, tt, A(ORO), NADA, glm::vec3(0.3f, 0.0f, 0.0f), 0, 1.0f);
		rect(glm::vec2(x0 + w * 0.5f, y0 + cab - 6.0f * s), glm::vec2(w - 30.0f * s, 2.0f * s), 0.0f, 5.0f * s, LINEA, A(glm::vec4(glm::vec3(ORO), 0.8f)), NADA, 0.0f,
		     glm::vec4(1.5f, 0.2f, 0.0f, 0.0f));
		if (ini > 0) texto(F_TITULO, "▲", x0 + w - 30.0f * s, y0 + 32.0f * s, 18.0f * s, A(ORO), NADA, glm::vec3(0.0f), 1, 1.0f);
		if (ini + n < nTotal) texto(F_TITULO, "▼", x0 + w - 30.0f * s, y0 + h - 2.0f * s, 18.0f * s, A(ORO), NADA, glm::vec3(0.0f), 1, 1.0f);
		for (int f = 0; f < n; f++) {
			const int k = ini + f;
			const control::Opcion& o = ctl->opciones[k];
			const bool sel = k == ctl->opcion;
			const float fy = y0 + cab + fila * (float)f;
			if (sel) {
				caja(x0 + 10.0f * s, fy + 3.0f * s, w - 20.0f * s, fila - 6.0f * s, 6.0f * s, PANEL, A(glm::vec4(0.32f, 0.23f, 0.05f, 0.6f)), A(ORO), (fila - 6.0f * s) * 0.5f,
				     glm::vec4(1.2f * s, 1.1f, 0.0f, 0.0f));
				rect(glm::vec2(x0 + 30.0f * s, fy + fila * 0.5f), glm::vec2(9.0f * s), 0.0f, 6.0f * s, ORBE, A(ORO), A(glm::vec4(0.5f, 0.32f, 0.06f, 1.0f)), 0.0f,
				     glm::vec4(0.9f, 1.0f, 0.0f, 0.0f));
				iconoBoton(glm::vec2(x0 + w - 32.0f * s, fy + fila * 0.5f), "X", 12.0f * s, a);
			}
			const glm::vec4 col = !o.activa ? glm::vec4(0.55f, 0.6f, 0.7f, 0.5f) : sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : glm::vec4(0.86f, 0.9f, 0.97f, 1.0f);
			texto(F_TITULO, o.texto, x0 + 46.0f * s, fy + fila * 0.5f + 8.0f * s, 23.0f * s, A(col), A(ORO), glm::vec3(sel ? 0.3f : 0.0f, 0.0f, 0.0f), 0, 1.0f);
		}
	}

	// cartel de la fase nueva: el de Master Duel (rama classic-md-fases: fases_md_ui.h, con las curvas de su Timeline)
	void dibujarFaseMando() { dibujarCartelFaseMD(); }

	// aviso corto (no hay zonas, demo apagada...): pastilla de vidrio arriba en el centro del campo
	void dibujarAvisoMando()
	{
		if (ctl->aviso.empty() || ctl->tAviso > 2.6f) return;
		if ((preguntaFijaVisible() || ctl->listaCartas()) && ctl->aviso == ctl->tituloTexto) return;   // (rama classic-ps5-control2: ya la dice la pregunta fija)
		const float a = suave(0.0f, 0.15f, ctl->tAviso) * (1.0f - suave(2.1f, 2.6f, ctl->tAviso));
		if (a < 0.01f) return;
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float tt = 22.0f * s;
		const float tw = medir(F_TITULO, ctl->aviso, tt, 1.0f);
		const float w = tw + 70.0f * s, h = 46.0f * s;
		// con el cartel TURNO DEL RIVAL (duelo_ui.h: y 150..214) el aviso va debajo de el, no encima
		const float cx = (CAMPO_X * s + W) * 0.5f, y = (ctl->vista.turnoRival ? 246.0f : 196.0f) * s - (1.0f - a) * 10.0f * s;
		usar(setAtlas, false);
		caja(cx - w * 0.5f, y, w, h, 18.0f * s, PANEL, A(VIDRIO), A(ORO), h * 0.5f, glm::vec4(1.2f * s, 1.0f, 0.2f, 12.0f * s));
		rect(glm::vec2(cx - w * 0.5f + 26.0f * s, y + h * 0.5f), glm::vec2(14.0f * s), 0.0f, 8.0f * s, ORBE, A(ORO), A(glm::vec4(0.5f, 0.32f, 0.06f, 1.0f)), 0.0f,
		     glm::vec4(0.9f, 1.0f, 0.0f, 0.0f));
		texto(F_TITULO, ctl->aviso, cx - w * 0.5f + 44.0f * s, y + h * 0.5f + 8.0f * s, tt, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
	}

	// rama classic-ps5-arreglos1: el cartel grande (cut-in) cuando el rival activa una carta en el turno del jugador: una
	// franja de vidrio carmesi cruza el campo, entra la carta (cara grande) y al lado "EL RIVAL ACTIVA UNA TRAMPA" y su nombre
	void dibujarCorteMando()
	{
		if (!ctl->corteVisible()) return;
		const float tc = ctl->tCorte, D = control::Mando::DURACION_CORTE;
		const float a = suave(0.0f, 0.18f, tc) * (1.0f - suave(D - 0.35f, D, tc));
		if (a < 0.01f) return;
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const int ci = buscarCarta(ctl->corteCarta);
		const CartaUI* k = ci >= 0 ? &cartas[ci] : nullptr;
		const bool trampa = k && k->atributo == "TRAMPA", magia = k && k->atributo == "MAGIA";
		const glm::vec4 acento = trampa ? glm::vec4(1.0f, 0.38f, 0.72f, 1.0f) : magia ? glm::vec4(0.25f, 0.95f, 0.75f, 1.0f) : CARMESI;
		const float x0 = CAMPO_X * s, ancho = W - x0;
		const float h = 250.0f * s, y = H * 0.40f - h * 0.5f;
		const float entra = 1.0f - suave(0.0f, 0.3f, tc);
		usar(setAtlas, false);
		caja(x0, y, ancho, h, 20.0f * s, PANEL, A(glm::vec4(0.02f, 0.012f, 0.03f, 0.9f)), A(acento), 0.0f, glm::vec4(2.0f * s, 1.0f, 0.4f, 16.0f * s));
		caja(x0, y + 8.0f * s, ancho, 3.0f * s, 4.0f * s, BARRA, A(acento), A(acento), 1.5f * s, glm::vec4(0.0f));
		caja(x0, y + h - 11.0f * s, ancho, 3.0f * s, 4.0f * s, BARRA, A(acento), A(acento), 1.5f * s, glm::vec4(0.0f));
		const glm::vec2 tam = glm::vec2(150.0f, 218.0f) * s * (1.0f + 0.08f * expf(-tc * 6.0f));
		const glm::vec2 centro(x0 + ancho * 0.28f - 260.0f * s * entra, y + h * 0.5f);
		rect(centro, tam * 1.6f, 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(acento) * 0.35f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		if (k && listaCarta(ci)) {
			usar(setCaraGrande(k), false);
			const float u = 0.5f / k->cara.width, vv = 0.5f / k->cara.height;
			rect(centro, tam, glm::radians(-4.0f), 0.0f, IMAGEN, A(glm::vec4(1.0f)), acento, 8.0f * s, glm::vec4(0.6f, 1.0f, (float)ci * 0.13f, 0.0f), glm::vec2(u, vv),
			     glm::vec2(1.0f - u, 1.0f - vv));
		} else {
			Dorso dz = usarDorso(false, false, true, 1.0f, 0.0f);
			dz.col.a *= a;
			rect(centro, tam, glm::radians(-4.0f), 0.0f, DORSO, dz.col, dz.col2, 8.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		usar(setAtlas, false);
		const float tx = centro.x + tam.x * 0.5f + 46.0f * s + 120.0f * s * entra;
		const std::string titulo = trampa ? "EL RIVAL ACTIVA UNA TRAMPA" : magia ? "EL RIVAL ACTIVA UNA MAGIA" : "EL RIVAL ACTIVA UN EFECTO";
		texto(F_TITULO, titulo, tx, y + h * 0.5f - 22.0f * s, 26.0f * s, A(acento), NADA, glm::vec3(0.0f), 0, 3.0f);
		const std::string nom = k ? k->vNombre() : ctl->corteCarta;
		texto(F_TITULO, nom, tx, y + h * 0.5f + 34.0f * s, 46.0f * s, A(BLANCO), A(acento), glm::vec3(0.5f, 0.0f, 0.6f), 0, 1.0f);
	}

	// detalle grande (triangulo): el campo se oscurece, la carta grande a la izquierda y su ficha
	// completa (nombre, estrellas, atributo, tipo, ATK/DEF y todo el texto) en un panel de vidrio a la derecha
	void dibujarDetalleMando()
	{
		if (ctl->estado != control::DETALLE) return;
		const float a = suave(0.0f, 0.22f, ctl->tDetalle);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float x0 = CAMPO_X * s, ancho = W - x0;
		usar(setAtlas, false);
		caja(x0, 0.0f, ancho, H, 0.0f, PANEL, A(glm::vec4(0.006f, 0.010f, 0.024f, 0.74f)), NADA, 0.0f, glm::vec4(0.0f));
		rz.tapar();   // rama classic-pc: un clic cierra el detalle; la rueda lo desplaza
		zBoton(0.0f, 0.0f, W, H, control::B_X);
		const int ci = ctl->detalleDorso ? -1 : buscarCarta(ctl->cartaDetalle);
		const CartaUI* k = ci >= 0 ? &cartas[ci] : nullptr;
		const glm::vec4 acento = k ? colorAtributo(k->atributo) : CIAN;
		const glm::vec2 tam = glm::vec2(440.0f, 638.0f) * s * (0.9f + 0.1f * a);
		const glm::vec2 centro(x0 + ancho * 0.30f, H * 0.48f);
		const float angY = glm::radians(6.0f) * sinf(t * 0.5f), angX = glm::radians(3.0f) * sinf(t * 0.4f + 1.0f);
		rect(centro, tam * 1.4f, 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(acento) * 0.25f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		rectInclinado(centro + glm::vec2(0.0f, 14.0f * s), tam, angY, angX, 30.0f * s, PANEL, A(glm::vec4(0, 0, 0, 0.7f)), NADA, 14.0f * s, glm::vec4(0, 0, 0, 22.0f * s));
		if (k && listaCarta((int)(k - cartas.data()))) {
			usar(setCaraGrande(k), false);
			const float u = 0.5f / k->cara.width, vv = 0.5f / k->cara.height;
			rectInclinado(centro, tam, angY, angX, 0.0f, IMAGEN, A(glm::vec4(1.0f)), acento, 12.0f * s, glm::vec4(0.6f, 1.0f, (float)ci * 0.13f, 0.0f), glm::vec2(u, vv),
			              glm::vec2(1.0f - u, 1.0f - vv));
		} else {
			Dorso dz = usarDorso(false, false, true, 1.0f, 0.0f);
			dz.col.a *= a;
			rectInclinado(centro, tam, angY, angX, 0.0f, DORSO, dz.col, dz.col2, 12.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		usar(setAtlas, false);
		const float px = centro.x + tam.x * 0.5f + 60.0f * s, pw = glm::min(640.0f * s, W - px - 40.0f * s);
		const float py = centro.y - tam.y * 0.5f, ph = tam.y;
		caja(px, py, pw, ph, 26.0f * s, PANEL, A(VIDRIO), A(glm::mix(CIAN, acento, 0.5f)), 16.0f * s, glm::vec4(1.4f * s, 0.8f, 0.07f, 18.0f * s));
		const float tx = px + 30.0f * s, tw = pw - 60.0f * s;
		float ty = py + 62.0f * s;
		if (!k) {
			texto(F_TITULO, "CARTA BOCA ABAJO", tx, ty, 36.0f * s, A(BLANCO), A(CARMESI), glm::vec3(0.35f, 0.0f, 0.0f));
			ty += 48.0f * s;
			parrafo(F_TEXTO, "Una carta oculta del rival. Se revelara cuando la juegue o la destruyas.", tx, ty, 26.0f * s, tw, 32.0f * s, 6, A(glm::vec4(0.82f, 0.86f, 0.93f, 1.0f)));
			return;
		}
		float tn = 40.0f * s;
		while (medir(F_TITULO, k->vNombre(), tn) > tw && tn > 22.0f * s) tn -= 1.0f * s;
		texto(F_TITULO, k->vNombre(), tx, ty, tn, A(BLANCO), A(acento), glm::vec3(0.35f, 0.0f, 0.0f));
		ty += 44.0f * s;
		const bool monstruo = k->clase != "magia" && k->clase != "trampa";
		if (monstruo) {
			for (int i = 0; i < k->nivel; i++) {
				rect(glm::vec2(tx + (13.0f + i * 30.0f) * s, ty - 8.0f * s), glm::vec2(15.0f * s), 0.0f, 12.0f * s, ESTRELLA, A(glm::vec4(1.0f, 0.86f, 0.35f, 1.0f)),
				     A(glm::vec4(1.0f, 0.55f, 0.15f, 1.0f)), 0.0f, glm::vec4(0.8f, 0, 0, 0));
			}
		}
		texto(F_TITULO, k->vAtributo(), tx + tw, ty, 21.0f * s, A(acento), NADA, glm::vec3(0.0f), 2, 2.0f);
		ty += 44.0f * s;
		std::string tipo = "[ " + k->vTipo();
		if (k->clase == "efecto") tipo += idioma::tr(" / Efecto");
		if (k->clase == "fusion") tipo += idioma::tr(" / Fusión");
		tipo += " ]";
		texto(F_TEXTO, tipo, tx, ty, 28.0f * s, A(glm::vec4(0.72f, 0.86f, 1.0f, 1.0f)));
		if (monstruo) {
			ty += 56.0f * s;
			// rama classic-pulido: en el duelo, el ATK/DEF ACTUAL (equipos, campos y efectos, como los rotulos de la mesa);
			// si cambio, en verde (subio) o rojo (bajo) y el impreso al lado, en gris y tachado
			int va = k->atk, vd = k->def;
			const int zd = ctl->zonaDetalle;
			if (zd >= 0 && zd < (int)NUM_ZONAS && strcmp(ZONAS[zd].tipo, "monstruo") == 0 && ctl->zonas[zd].carta == ctl->cartaDetalle && !ctl->zonas[zd].bocaAbajo) {
				const int bono = bonoBosque > 0.5f && potenciaBosque(k->id) ? 200 : 0;   // carta de campo Bosque
				va += bono;
				vd += bono;
				if (statsZona) statsZona(ZONAS[zd].id, va, vd);
			}
			auto valor = [&](const char* rotulo, int actual, int impreso, float x, glm::vec4 claro, glm::vec4 borde) {
				texto(F_TITULO, rotulo, x, ty - 4.0f * s, 21.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 0, 2.0f);
				const float xn = x + 53.0f * s;
				if (actual != impreso) {
					const bool sube = actual > impreso;
					claro = sube ? glm::vec4(0.75f, 1.0f, 0.65f, 1.0f) : glm::vec4(1.0f, 0.72f, 0.68f, 1.0f);
					borde = sube ? glm::vec4(0.35f, 0.95f, 0.3f, 1.0f) : glm::vec4(1.0f, 0.3f, 0.25f, 1.0f);
				}
				const std::string sa = std::to_string(actual);
				texto(F_NUMEROS, sa, xn, ty, 40.0f * s, A(claro), A(borde), glm::vec3(0.45f, 0.0f, 0.8f));
				if (actual == impreso) return;
				const std::string si = std::to_string(impreso);
				const float xo = xn + medir(F_NUMEROS, sa, 40.0f * s) + 12.0f * s, wo = medir(F_NUMEROS, si, 24.0f * s);
				const glm::vec4 gris(0.55f, 0.58f, 0.64f, 0.9f);
				texto(F_NUMEROS, si, xo, ty, 24.0f * s, A(gris));
				caja(xo - 2.0f * s, ty - 10.0f * s, wo + 4.0f * s, 2.5f * s, 0.0f, PANEL, A(gris), NADA, 0.0f, glm::vec4(0.0f));
			};
			valor("ATK", va, k->atk, tx, glm::vec4(1.0f, 0.93f, 0.7f, 1.0f), ORO);
			valor("DEF", vd, k->def, tx + tw * 0.5f, glm::vec4(0.8f, 0.95f, 1.0f, 1.0f), CIAN);
		}
		ty += 30.0f * s;
		rect(glm::vec2(px + pw * 0.5f, ty), glm::vec2(tw, 3.0f * s), 0.0f, 6.0f * s, LINEA, A(acento), NADA, 0.0f, glm::vec4(2.0f, 0.3f, 0, 0));
		ty += 42.0f * s;
		// rama classic-pulido: el texto se desplaza (cruz o stick derecho: ctl->desplDetalle, en lineas) con su barra
		const float tamT = 27.0f * s, inter = 33.0f * s, anchoT = tw - 18.0f * s;
		const int visibles = glm::max(1, (int)((py + ph - 24.0f * s - ty) / inter) + 1);
		const std::vector<std::string> lineas = cortarLineas(F_TEXTO, k->vDesc(), tamT, anchoT);
		const int sobran = glm::max(0, (int)lineas.size() - visibles);
		ctl->desplDetalleMax = (float)sobran;
		const float d = glm::clamp(ctl->desplDetalle, 0.0f, (float)sobran);
		const int primera = (int)floorf(d);
		const float frac = d - (float)primera, yFin = ty + (float)(visibles - 1) * inter;
		{
			const bool antes = crudo;
			crudo = true;   // (ya traducido: cortarLineas)
			for (int i = 0; i <= visibles && primera + i < (int)lineas.size(); i++) {
				const float y = ty + ((float)i - frac) * inter;
				const float al = glm::clamp(1.0f - (ty - y) / inter, 0.0f, 1.0f) * glm::clamp(1.0f - (y - yFin) / inter, 0.0f, 1.0f);
				if (al < 0.02f) continue;
				texto(F_TEXTO, lineas[(size_t)(primera + i)], tx, y, tamT, A(glm::vec4(0.86f, 0.9f, 0.96f, al)));
			}
			crudo = antes;
		}
		if (sobran > 0) {   // barra de desplazamiento a la derecha del texto
			const float bx = px + pw - 22.0f * s, by = ty - tamT, bh = yFin + 10.0f * s - by;
			const float th = glm::max(36.0f * s, bh * (float)visibles / (float)lineas.size());
			const float tyb = by + (bh - th) * (d / (float)sobran);
			caja(bx, by, 6.0f * s, bh, 3.0f * s, PANEL, A(glm::vec4(1.0f, 1.0f, 1.0f, 0.12f)), NADA, 0.0f, glm::vec4(0.0f));
			caja(bx - 1.0f * s, tyb, 8.0f * s, th, 4.0f * s, PANEL, A(glm::vec4(glm::vec3(glm::mix(CIAN, acento, 0.5f)), 0.9f)), NADA, 0.0f, glm::vec4(0.0f));
		}
	}

	// rama classic-pulido: corta un texto en lineas por palabras (como parrafo, sin dibujar ni recortar)
	std::vector<std::string> cortarLineas(int fu, const std::string& original, float tam, float ancho)
	{
		const std::string txt = trd(original);
		const bool antes = crudo;
		crudo = true;
		std::vector<std::string> r;
		std::string linea;
		size_t a = 0;
		while (a <= txt.size()) {
			size_t b = txt.find(' ', a);
			if (b == std::string::npos) b = txt.size();
			const std::string palabra = txt.substr(a, b - a);
			const std::string prueba = linea.empty() ? palabra : linea + " " + palabra;
			if (!linea.empty() && medir(fu, prueba, tam) > ancho) {
				r.push_back(linea);
				linea = palabra;
			} else {
				linea = prueba;
			}
			a = b + 1;
		}
		if (!linea.empty()) r.push_back(linea);
		crudo = antes;
		return r;
	}

	// barra de ayuda: los botones que sirven ahora (control::Mando::ayuda), abajo a la derecha, en pastillas de vidrio;
	// si no entran al lado de la mano del jugador se apilan en filas hacia arriba
	// rama classic-visual-final: como se reparte la barra de ayuda (filas de abajo hacia arriba, a la derecha de la mano
	// del jugador). Si con la mano ancha no entra en 3 filas, se achican letra y separaciones (hasta 80%) para que la
	// pila no suba hasta el mazo; el boton SIGUIENTE FASE va siempre encima de la pila (topeAyuda), nunca encima de ella
	struct PlanAyuda {
		std::vector<std::pair<std::string, std::string>> items;
		std::vector<float> ancho;
		std::vector<std::vector<int>> filas;
		float h{ 0 }, r{ 0 }, tt{ 0 }, sep{ 0 }, pad{ 0 };
	};
	PlanAyuda planAyuda()
	{
		PlanAyuda pl;
		if (!ctl || consultaAbierta()) return pl;   // (rama classic-duelo-ui: el panel trae sus botones)
		for (const auto& it : ctl->ayuda()) pl.items.emplace_back(it.first, it.second);
		if (pl.items.empty()) return pl;
		// borde derecho de la mano del jugador (la barra no la tapa)
		float manoDer = CAMPO_X * s;
		for (int k = 0; k < nManoJ(); k++) {
			glm::vec2 c;
			float an, e;
			cartaManoJ(k, c, an, e);
			manoDer = glm::max(manoDer, c.x + 75.0f * s * e + 12.0f * s);
		}
		const float derecha = W - 22.0f * s;
		// (rama classic-arreglo-ciclo-md, jugador humano: con la mano llena la barra bajaba a letra de 15 px en 4 filas y no se
		// leia; el minimo es 0.85, 16 px: si no entra, una fila mas)
		for (float k = 1.0f; k >= 0.84f; k -= 0.05f) {
			pl.h = 42.0f * s * glm::max(k, 0.9f);
			pl.r = 14.0f * s;
			pl.tt = 19.0f * s * k;
			pl.sep = 22.0f * s * k;
			pl.pad = 16.0f * s * k;
			pl.ancho.clear();
			for (const auto& it : pl.items) {
				float wi = 0.0f;
				size_t a = 0;
				for (size_t b; a <= it.first.size(); a = b + 1) {   // (rama classic-ps5-control2: "C|OPTIONS", dos iconos)
					b = it.first.find('|', a);
					if (b == std::string::npos) b = it.first.size();
					const std::string k = it.first.substr(a, b - a);
					if (a > 0) wi += 6.0f * s;
					if (k != "X" && k != "O" && k != "T" && k != "C" && k != "CRUZ" && k != "STICK_R") wi += medir(F_NUMEROS, k == "L1R1" ? "L1 R1" : k, 14.0f * s, 1.0f) + 14.0f * s;
					else wi += 2.0f * pl.r;
				}
				pl.ancho.push_back(wi + 8.0f * s + medir(F_TITULO, it.second, pl.tt, 1.0f));
			}
			const float disponible = glm::max(derecha - manoDer - 2.0f * pl.pad, 260.0f * s);
			pl.filas.assign(1, {});
			float usado = 0.0f;
			for (int i = 0; i < (int)pl.items.size(); i++) {
				const float wi = pl.ancho[i] + (pl.filas.back().empty() ? 0.0f : pl.sep);
				if (!pl.filas.back().empty() && usado + wi > disponible) {
					pl.filas.emplace_back();
					usado = 0.0f;
				}
				usado += pl.ancho[i] + (pl.filas.back().empty() ? 0.0f : pl.sep);
				pl.filas.back().push_back(i);
			}
			if (pl.filas.size() <= 3) break;
		}
		return pl;
	}
	// la y de arriba de la barra de ayuda (H - 22 si no hay barra)
	float topeAyuda(const PlanAyuda& pl) const
	{
		const float abajo = H - 22.0f * s;
		if (pl.items.empty()) return abajo;
		return abajo - (float)pl.filas.size() * pl.h - (float)(pl.filas.size() - 1) * 8.0f * s;
	}

	void dibujarAyudaMando()
	{
		if (entrada::estado().activa) return;   // (classic-visual-final: la entrada clave trae su ayuda: R2 Saltar)
		const PlanAyuda pl = planAyuda();
		if (pl.items.empty()) return;
		const float derecha = W - 22.0f * s;
		const float h = pl.h, r = pl.r, tt = pl.tt, sep = pl.sep, pad = pl.pad;
		const auto& items = pl.items;
		const auto& ancho = pl.ancho;
		const auto& filas = pl.filas;
		usar(setAtlas, false);
		float y = H - 22.0f * s - h;
		for (int f = (int)filas.size() - 1; f >= 0; f--) {
			float total = 0.0f;
			for (size_t j = 0; j < filas[f].size(); j++) total += ancho[filas[f][j]] + (j ? sep : 0.0f);
			const float x0 = derecha - total - 2.0f * pad;
			caja(x0, y, total + 2.0f * pad, h, 16.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.78f), glm::vec4(glm::vec3(CIAN) * 0.8f, 1.0f), h * 0.5f,
			     glm::vec4(1.1f * s, 0.6f, 0.0f, 12.0f * s));
			float x = x0 + pad;
			for (size_t j = 0; j < filas[f].size(); j++) {
				const auto& it = items[filas[f][j]];
				const float wi = iconoBoton(glm::vec2(x + r, y + h * 0.5f), it.first, r);
				texto(F_TITULO, it.second, x + wi + 8.0f * s, y + h * 0.5f + tt * 0.37f, tt, glm::vec4(0.88f, 0.92f, 0.98f, 1.0f), NADA, glm::vec3(0.0f), 0, 1.0f);
				x += ancho[filas[f][j]] + sep;
			}
			y -= h + 8.0f * s;
		}
	}

#include "constructor_ui.h"   // rama classic-constructor: cache de caras y la pantalla "Construir mazo"
#include "galeria_ui.h"        // rama classic-galeria: la galeria de figuras 3D (rejilla, visor y detalle)
#include "duelo_ui.h"         // rama classic-integracion: pantallas del duelo real (elegir rival, resultado)
#include "portada_ui.h"       // rama classic-portada: la portada (titulo) al arrancar
#include "personajes_ui.h"    // rama classic-personajes: retratos, modo historia y el retrato del rival en el duelo
#include "menu_ui.h"          // rama classic-menus: el menu principal y sus Opciones
#include "resultado_ui.h"     // rama classic-resultado: la pantalla de resultado (motivo, resumen, revancha)
#include "duelo_consulta_ui.h" // rama classic-duelo-ui: registro, visores de zonas, pila de la cadena y jugables
#include "historia_dialogos_ui.h"   // rama classic-historia: dialogos del modo historia y el final
#include "tutorial_ui.h"            // rama classic-historia: las reglas del duelo con esquemas de la mesa
#include "entrada_clave_ui.h"  // rama classic-cartas-clave: el corte 2D de la entrada especial de las cartas clave
#include "entrada_md_ui.h"     // rama classic-md-entradas: las entradas de fusion y ritual de Master Duel
#include "menu_md_ui.h"        // rama classic-md-entradas: el menu del duelo, la transicion del fin y los LP bajos de Master Duel
#include "en_linea_ui.h"            // rama classic-en-linea: el DUELO EN LINEA (en_linea.h)
#include "cosmeticos_ui.h"          // rama classic-extras: Personalizacion (fundas, tapete, controles, estadisticas)
#include "lista_cartas_ui.h"        // rama classic-ps5-elegir-cartas: el selector de cartas de MD (fila de cartas con su arte)
#include "pregunta_ui.h"            // rama classic-ps5-control2: la pregunta del duelo siempre a la vista (elegir carta / zona)
#include "md_ritmo_ui.h"            // rama classic-md-ritmo: el ritmo de Master Duel (viajes, activaciones, cartas activas, contadores)
#include "md_efectos_ui.h"          // rama classic-ps5-efectos: efectos de cartas de Master Duel (brillo de la activa, Swords of Revealing Light)
#include "md_cardrun_ui.h"          // rama classic-ps5-efectos: los CardRunEffect de MD (pistas 3D, pantallas, sacudidas)
#include "fases_md_ui.h"            // rama classic-md-fases: selector de fases, engranaje y carteles de Master Duel
#include "ataque_md_ui.h"           // rama classic-ps5-ataque: el marco rojo del danno (DamageFrame de MD)
};

}   // namespace interfaz
