/*
 * Yugi Reload - el control del duelo con el mando (rama mando), SIN motor de reglas.
 *
 * Pedido del usuario (2026-10-16): "poder INTERACTUAR con el mando con todo lo que el juego permite": elegir una
 * carta de la mano y dejarla en el campo, moverse entre cada carta/personaje del campo viendo sus datos en el visor
 * de la izquierda, atacar, cambiar de posicion, voltear, ver el detalle, pasar de fase, y mover la camara.
 *
 * Este archivo es SOLO logica (sin Vulkan ni la interfaz): el estado de la mesa (que carta hay en cada zona, la
 * mano, los cementerios), el foco (lo que el jugador tiene elegido), los menus contextuales y la maquina de estados
 * del flujo. Lo que el juego tiene que hacer (la animacion de invocar, atacar, cambiar la postura, el
 * campo...) sale como Eventos en "eventos" (yugireload.cpp los ejecuta en cada cuadro). La interfaz (interfaz.h)
 * lee este estado para dibujar: el visor, las cartas de la mesa, el menu, la barra de ayuda, el detalle, la fase.
 * Asi se puede probar todo el flujo sin pantalla (src/pruebas/prueba_mando.cpp, y YUGI_GUION en el PC).
 *
 * Navegacion por foco tipo consola: la mesa se recorre como una rejilla de filas, de abajo (la mano del jugador)
 * hacia arriba (la mano del rival): mano J | fusion, magia 0..4, mazo | campo, monstruo 0..4, cementerio |
 * cementerio R, monstruos R, campo R | mazo R, magias R, fusion R | mano R. Arriba/abajo cambia de fila (a la carta
 * mas cercana en x), izquierda/derecha se mueve dentro de la fila. La rejilla es la de la mesa (no la de la
 * pantalla): no cambia con la camara.
 *
 * Mapa de botones (DualSense; en el PC con ventana, el teclado entre parentesis):
 *   cruceta / stick izq   mover el foco (flechas)        X        confirmar / abrir el menu (Enter, Espacio)
 *   circulo               cancelar / atras (Retroceso)     triangulo  ver el detalle grande (T)
 *   cuadrado              (= OPTIONS corto: el selector de fases; rama classic-ps5-control2)  L1 / R1    modo de camara (Q / E; C = siguiente)
 *   stick der             mover/orbitar la camara          L2 / R2    alejar / acercar      R3  recentrar (R)
 *   OPTIONS corto         en el duelo, TU turno: el selector de fases de Master Duel (O): Iniciar Fase de Batalla,
 *                         Ir a M2, Finalizar turno (fases_md.h); fuera del duelo: el menu de pausa
 *   panel tactil          en el duelo: el engranaje = menu del duelo (G): Continuar, Opciones, Construir mazo,
 *                         Abandonar, Salir al menu principal (tambien con el foco en el engranaje y X); fuera del
 *                         duelo: Construir mazo (M)
 *   OPTIONS 2 s           Ajustes (de la plantilla)
 * Lo que queda para el motor de reglas: tributos, limite de una invocacion normal por turno, fases que habilitan
 * cada accion, danno a los LP y destruccion por batalla, efectos de las cartas, el turno del rival.
 */
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include "en_linea.h"   // rama classic-en-linea: lo que se ve del DUELO EN LINEA (VistaDuelo::enLinea)
#include <string>
#include <vector>

#include "idioma.h"   // textos armados (Fusion: <carta>)
#include "ia.h"   // rama classic-ia: el rival (duelo sin efectos, IA y rivales.txt); gancho: sincronizarLado
#include "clips_md.h"   // rama classic-md-fases: los Timeline y Tween de Master Duel (fases_md.h)
#include "opciones.h"   // rama classic-ciclos: Preguntar antes de activar (duelo_juego.h)
#include "moneda.h"     // rama classic-moneda: el lanzamiento de moneda antes del duelo

namespace control {

// ------------------------------------------------------------------------------------------------- la mesa
// Las zonas, en el MISMO orden que interfaz::ZONAS (yugireload.cpp lo comprueba al empezar): x, z en las medidas del
// diseno (remake/diseno/tablero.json; en el juego x1.4)
struct ZonaMesa { const char* id; float x, z; const char* tipo; bool rival; };
static const ZonaMesa ZONAS_MESA[] = {
	{ "jugador_monstruo_0", -5.4f, 2.2f, "monstruo", false }, { "jugador_magia_0", -5.4f, 5.5f, "magia", false },
	{ "jugador_monstruo_1", -2.7f, 2.2f, "monstruo", false }, { "jugador_magia_1", -2.7f, 5.5f, "magia", false },
	{ "jugador_monstruo_2", 0.0f, 2.2f, "monstruo", false }, { "jugador_magia_2", 0.0f, 5.5f, "magia", false },
	{ "jugador_monstruo_3", 2.7f, 2.2f, "monstruo", false }, { "jugador_magia_3", 2.7f, 5.5f, "magia", false },
	{ "jugador_monstruo_4", 5.4f, 2.2f, "monstruo", false }, { "jugador_magia_4", 5.4f, 5.5f, "magia", false },
	{ "jugador_campo", -8.2f, 2.2f, "campo", false }, { "jugador_fusion", -8.2f, 5.5f, "fusion", false },
	{ "jugador_cementerio", 8.2f, 2.2f, "cementerio", false }, { "jugador_mazo", 8.2f, 5.5f, "mazo", false },
	{ "rival_monstruo_0", 5.4f, -2.2f, "monstruo", true }, { "rival_magia_0", 5.4f, -5.5f, "magia", true },
	{ "rival_monstruo_1", 2.7f, -2.2f, "monstruo", true }, { "rival_magia_1", 2.7f, -5.5f, "magia", true },
	{ "rival_monstruo_2", 0.0f, -2.2f, "monstruo", true }, { "rival_magia_2", 0.0f, -5.5f, "magia", true },
	{ "rival_monstruo_3", -2.7f, -2.2f, "monstruo", true }, { "rival_magia_3", -2.7f, -5.5f, "magia", true },
	{ "rival_monstruo_4", -5.4f, -2.2f, "monstruo", true }, { "rival_magia_4", -5.4f, -5.5f, "magia", true },
	{ "rival_campo", 8.2f, -2.2f, "campo", true }, { "rival_fusion", 8.2f, -5.5f, "fusion", true },
	{ "rival_cementerio", -8.2f, -2.2f, "cementerio", true }, { "rival_mazo", -8.2f, -5.5f, "mazo", true },
	// rama classic-duelo-ui: las desterradas de cada lado, junto a su cementerio (al final: no mueve los indices de antes)
	{ "jugador_desterrado", 10.9f, 2.2f, "desterrado", false }, { "rival_desterrado", -10.9f, -2.2f, "desterrado", true },
};
constexpr int NUM_ZONAS_MESA = (int)(sizeof(ZONAS_MESA) / sizeof(ZONAS_MESA[0]));
constexpr float ESCALA_MESA = 1.4f;   // ESCALA_TABLERO del juego

inline int zonaPorId(const std::string& id)
{
	for (int i = 0; i < NUM_ZONAS_MESA; i++) {
		if (id == ZONAS_MESA[i].id) return i;
	}
	return -1;
}
inline bool esTipo(int z, const char* tipo) { return z >= 0 && z < NUM_ZONAS_MESA && strcmp(ZONAS_MESA[z].tipo, tipo) == 0; }

// las magias de campo que conoce la demo (cartas_ui.txt no distingue magia normal de campo)
inline bool esMagiaDeCampo(const std::string& id)
{
	static const char* const CAMPOS[] = { "sogen", "forest", "yami", "umi", "wasteland", "mountain" };
	for (const char* c : CAMPOS) {
		if (id == c) return true;
	}
	return false;
}

// lo que hay en una zona. Las zonas de fusion guardan la carta de arriba de su pila
constexpr float DURACION_ANIM = 0.45f;   // s de los volteos y giros de las cartas de la mesa (la interfaz los dibuja); classic-md-ritmo: MD CardFlipPlate* 0.45 (antes 0.55)
struct Celda {
	std::string carta;            // id de cartas_ui.txt ("" = vacia)
	bool bocaAbajo{ false }, defensa{ false };
	bool posicionTocada{ false }; // el jugador la puso o le cambio la posicion (enDefensa del juego obedece a esto)
	// animacion: s desde el ultimo volteo / giro / llegada, y el estado de antes (para interpolar)
	float tVolteo{ 99.0f }, tGiro{ 99.0f }, tLlega{ 99.0f }, tActiva{ 99.0f };
	bool antesBocaAbajo{ false }, antesDefensa{ false };
	bool vacia() const { return carta.empty(); }
};

// ------------------------------------------------------------------------------------------------- entrada
// pulsos logicos de un cuadro (ya con la repeticion de la cruceta y el stick)
enum Boton : uint32_t {
	B_X = 1u << 0, B_CIRCULO = 1u << 1, B_TRIANGULO = 1u << 2, B_CUADRADO = 1u << 3,
	B_ARRIBA = 1u << 4, B_ABAJO = 1u << 5, B_IZQ = 1u << 6, B_DER = 1u << 7,
	B_L1 = 1u << 8, B_R1 = 1u << 9, B_R3 = 1u << 10, B_OPTIONS = 1u << 11,
	B_L3 = 1u << 12,   // rama classic-duelo-ui: registro del duelo
	B_TACTIL = 1u << 13,   // rama classic-md-fases: el panel tactil (el engranaje del duelo)
};
constexpr uint32_t B_DIRECCIONES = B_ARRIBA | B_ABAJO | B_IZQ | B_DER;

// Convierte lo crudo del mando (botones abajo / recien apretados en las direcciones, stick izquierdo) en pulsos de
// direccion: uno al apretar y repeticion si se mantiene (0.38 s y luego cada 0.13 s); el stick, igual
constexpr float REPETIR_PRIMERO = 0.40f, REPETIR_CADA = 0.09f;   // s (rama classic-menus)
struct Repetidor {
	uint32_t antes{ 0 };
	float espera{ 0.0f }, palancaT{ 0.0f };
	bool palancaSuelta{ true };
	uint32_t paso(uint32_t abajo, uint32_t recien, float lx, float ly, float dt)
	{
		uint32_t r = recien & B_DIRECCIONES;
		const uint32_t dir = abajo & B_DIRECCIONES;
		// rama classic-menus: un toque = un paso; mantenido, repite tras REPETIR_PRIMERO y luego cada REPETIR_CADA
		if (dir && dir == antes) {
			espera -= dt;
			if (espera <= 0.0f) {
				espera = REPETIR_CADA;
				r |= dir;
			}
		} else {
			espera = REPETIR_PRIMERO;
		}
		antes = dir;
		const float largo = sqrtf(lx * lx + ly * ly);
		if (largo > 0.6f) {
			palancaT -= dt;
			if (palancaT <= 0.0f) {
				r |= fabsf(lx) > fabsf(ly) ? (lx > 0.0f ? B_DER : B_IZQ) : (ly > 0.0f ? B_ABAJO : B_ARRIBA);   // y hacia abajo +
				palancaT = palancaSuelta ? REPETIR_PRIMERO : REPETIR_CADA;
				palancaSuelta = false;
			}
		} else if (largo < 0.3f) {
			palancaT = 0.0f;
			palancaSuelta = true;
		}
		return r;
	}
};

// Guion de botones para probar sin mando (YUGI_GUION en el PC): "X,abajo,X,1.5,der*2,X,O,..." separados por comas.
// Cada boton se aprieta y se espera "paso" s (0.35 por defecto); un numero solo es una espera en segundos; "b*n"
// repite n veces. Botones: x, o (circulo), t (triangulo), c (cuadrado), arriba, abajo, izq, der, l1, r1, r3, options, tactil.
// Arranca en "inicio" s (por defecto 1)
struct Guion {
	struct Paso { uint32_t boton; float espera; };
	std::vector<Paso> pasos;
	size_t siguiente{ 0 };
	float reloj{ 0.0f }, proximo{ 1.0f };
	bool activo() const { return !pasos.empty(); }
	bool terminado() const { return siguiente >= pasos.size(); }

	static uint32_t botonDe(std::string n)
	{
		for (char& c : n) c = (char)tolower((unsigned char)c);
		if (n == "x" || n == "cruz" || n == "equis") return B_X;
		if (n == "o" || n == "circulo") return B_CIRCULO;
		if (n == "t" || n == "triangulo") return B_TRIANGULO;
		if (n == "c" || n == "cuadrado") return B_CUADRADO;
		if (n == "arriba" || n == "up") return B_ARRIBA;
		if (n == "abajo" || n == "down") return B_ABAJO;
		if (n == "izq" || n == "izquierda" || n == "left") return B_IZQ;
		if (n == "der" || n == "derecha" || n == "right") return B_DER;
		if (n == "l1") return B_L1;
		if (n == "r1") return B_R1;
		if (n == "r3") return B_R3;
		if (n == "l3") return B_L3;
		if (n == "options") return B_OPTIONS;
		if (n == "tactil" || n == "touchpad") return B_TACTIL;   // rama classic-md-fases
		return 0;
	}
	void leer(const char* texto, float paso = 0.35f, float inicio = 1.0f)
	{
		pasos.clear();
		siguiente = 0;
		reloj = 0.0f;
		proximo = inicio;
		std::string t = texto ? texto : "";
		size_t a = 0;
		while (a <= t.size()) {
			size_t b = t.find(',', a);
			if (b == std::string::npos) b = t.size();
			std::string tok = t.substr(a, b - a);
			tok.erase(0, tok.find_first_not_of(" \t"));
			tok.erase(tok.find_last_not_of(" \t") + 1);
			a = b + 1;
			if (tok.empty()) continue;
			int veces = 1;
			const size_t star = tok.find('*');
			if (star != std::string::npos) {
				veces = std::max(1, atoi(tok.c_str() + star + 1));
				tok = tok.substr(0, star);
			}
			if ((tok[0] >= '0' && tok[0] <= '9') || tok[0] == '.') {
				pasos.push_back({ 0u, (float)atof(tok.c_str()) });
				continue;
			}
			const uint32_t bt = botonDe(tok);
			if (!bt) {
				fprintf(stderr, "[mando] guion: boton desconocido '%s'\n", tok.c_str());
				continue;
			}
			for (int k = 0; k < veces; k++) pasos.push_back({ bt, paso });
		}
	}
	// los botones de este cuadro
	uint32_t avanzar(float dt)
	{
		reloj += dt;
		uint32_t r = 0;
		while (!terminado() && reloj >= proximo) {
			const Paso& p = pasos[siguiente++];
			r |= p.boton;
			proximo = reloj + p.espera;
			if (p.boton) break;   // un boton por cuadro
		}
		return r;
	}
};

// ------------------------------------------------------------------------------------------------- flujo
enum Estado { NAVEGAR = 0, MENU, ELEGIR_ZONA, ELEGIR_OBJETIVO, DETALLE, VOLANDO, ELEGIR_TRIBUTOS, ELEGIR_CARTA };
static const char* const NOMBRE_ESTADO[] = { "navegar", "menu", "elegir_zona", "elegir_objetivo", "detalle", "volando", "elegir_tributos",
                                             "elegir_carta" };

enum Accion {
	AC_INVOCAR = 0, AC_COLOCAR_MONSTRUO, AC_ACTIVAR_MAGIA, AC_COLOCAR_MT, AC_ACTIVAR_CAMPO_MANO,   // desde la mano
	AC_ATACAR, AC_POSICION, AC_VOLTEAR_MONSTRUO, AC_ACTIVAR_SET, AC_ACTIVAR_CAMPO, AC_DESTRUIR_CAMPO, AC_CEMENTERIO,
	AC_CANCELAR,
	AC_JUGADA,   // duelo real: la jugada ya armada de la opcion (fusion, ritual, descartar)
	AC_TOON,     // duelo real: invocacion especial de un Toon con Toon World boca arriba
	// rama classic-efectos (motor de efectos de ia.h)
	AC_ACTIVAR_EFECTO,   // activar una carta con efecto (mano, colocada o monstruo): luego costes y objetivos
	AC_PASAR,            // en la pregunta del duelo: no activar nada
	AC_ELEGIR_LISTA,     // elegir de una lista (cementerio...): Opcion::jugada.objetivo = indice del candidato
};
struct Opcion { Accion accion; std::string texto; bool activa{ true }; std::string motivo; ia::Jugada jugada{}; };

// lo que el juego tiene que hacer (yugireload.cpp, cada cuadro)
enum TipoEvento {
	EV_INVOCAR = 0,         // monstruo que acaba de llegar boca arriba a su zona (destello de aparicion)
	EV_VOLTEAR_MONSTRUO,    // monstruo colocado que se voltea (invocacion por volteo)
	EV_ATACAR,              // la carta de zona ataca a la de zona2
	EV_POSICION,            // monstruo cambio de ataque <-> defensa (zona: la suya)
	EV_AL_CEMENTERIO,       // monstruo boca arriba que se fue al cementerio
	EV_ACTIVAR_CAMPO,       // la magia de campo del jugador se activa (gancho activarCampo(), rama campo-bosque)
	EV_DESTRUIR_CAMPO,      // la magia de campo se destruye (gancho destruirCampo())
	EV_ACTIVAR_MAGIA,       // una magia/trampa se activa (boca arriba): solo el destello (sin efecto: motor de reglas)
	EV_FASE,                // paso a la fase "valor" (indice de interfaz::FASES)
	EV_DEMO_APAGADA,        // el usuario toco el mando: se apaga la demo automatica
	EV_A_LA_MANO,           // monstruo que vuelve a la mano al tomar el control (reparto)
};
static const char* const NOMBRE_EVENTO[] = { "invocar", "voltear_monstruo", "atacar", "posicion", "al_cementerio", "activar_campo",
                                             "destruir_campo", "activar_magia", "fase", "demo_apagada", "a_la_mano" };
struct Evento { TipoEvento tipo; int zona{ -1 }, zona2{ -1 }, valor{ 0 }; std::string carta; };

// fases de la mesa libre sin duelo (indices de interfaz::FASES: DP, SP, M1, BP, M2, EP; la de espera no se usa)
static const int CICLO_FASES[] = { 0, 2, 3, 4, 5 };
static const char* const NOMBRE_FASE_LARGO[] = { "FASE DE ROBO", "FASE DE ESPERA", "FASE PRINCIPAL 1", "FASE DE BATALLA",
                                                 "FASE PRINCIPAL 2", "FASE FINAL" };

// lo enfocado: una carta de la mano del jugador (0), de la del rival (1) o una zona (2)
enum TipoFoco { F_MANO_J = 0, F_MANO_R = 1, F_ZONA = 2, F_ENGRANAJE = 3 };   // (F_ENGRANAJE: rama classic-md-fases, el boton del menu del duelo)
struct Foco {
	int tipo{ F_MANO_J }, i{ 0 };
	bool operator==(const Foco& o) const { return tipo == o.tipo && i == o.i; }
};

// rama classic-integracion: lo que la interfaz muestra del duelo real fuera de la mesa (elegir rival, turno del
// rival, resultado). Lo llena el juego (duelo_juego.h); lo dibuja interfaz.h (duelo_ui.h)
struct VistaDuelo {
	enum Pantalla { NINGUNA = 0, ELEGIR_RIVAL, DUELO, RESULTADO, MENU, PORTADA, EN_LINEA };   // MENU: menu principal (rama classic-menus); PORTADA: titulo (classic-portada); EN_LINEA: menus del duelo en linea (classic-en-linea)
	int pantalla{ NINGUNA };
	int nivel{ 1 }, nivelMax{ 1 }, niveles{ 8 };
	std::string rivalEs, rivalEn, lemaEs, lemaEn, personalidad;   // personalidad: clave (ia.personalidad.<clave>)
	int victorias{ 0 }, derrotas{ 0 };
	bool turnoRival{ false }, pensando{ false };
	int ganador{ -1 }, turno{ 0 };
	bool subeNivel{ false };
	float t{ 0.0f };   // s desde que se entro a la pantalla
	// rama classic-personajes: modo historia (personajes del anime por sagas) o duelo libre (los genericos por nivel)
	enum Modo { HISTORIA = 0, LIBRE };
	int modo{ LIBRE };
	int saga{ 0 }, sagas{ 0 }, idx{ 0 };          // saga mostrada, cuantas hay y el personaje elegido dentro de ella
	bool sagaAbierta{ true };
	std::string sagaEs, sagaEn;
	std::vector<std::string> fila;               // los personajes de la saga (ids) y su estado: 0 cerrado, 1 libre, 2 vencido
	std::vector<int> filaEstado;
	std::string personaje;                       // id del personaje rival ("" = generico): su retrato en el duelo
	std::string entorno;                         // clave de entorno del rival (docs/entornos.md de classic-entornos)
	std::string varianteEs, varianteEn;          // nombre del mazo que toca
	int nivelRival{ 0 }, vencidos{ 0 }, totalPersonajes{ 0 };
	bool personajeNuevo{ false };                // resultado: primera victoria contra el personaje
	int firmaNuevas{ 0 };                        //   y cuantas cartas firma desbloquea
	bool sagaNueva{ false };                     //   y si abre la saga siguiente
	// rama classic-arreglos: menu de pausa (OPTIONS corto; lo lleva duelo_juego.h, lo dibuja duelo_ui.h)
	bool pausa{ false };
	int pausaOpcion{ 0 };
	std::vector<std::string> pausaOpciones;
	// rama classic-moneda: el lanzamiento de moneda antes del duelo (moneda.h; lo lleva moneda_juego.h, lo dibuja
	// moneda_ui.h). sorteo: la pantalla del lanzamiento esta abierta (todavia no hay duelo)
	bool sorteo{ false };
	moneda::Estado moneda;
	// rama classic-menus: menu principal y sus Opciones (lo lleva menu_principal.h, lo dibuja menu_ui.h)
	bool menuEnOpciones{ false }, menuSalir{ false };   // dentro de Opciones / pidio salir del juego (X otra vez confirma)
	int menuOpcion{ 0 };
	std::vector<std::string> menuOpciones, menuValores;  // claves en espanol (idioma::tr al dibujar) y el valor de cada una
	// rama classic-menus: duelo libre, el rival dentro del nivel (izq/der) y cuantos hay en el nivel
	int rivalIdx{ 0 }, rivalesNivel{ 0 };
	// rama classic-en-linea: el DUELO EN LINEA (menus, reloj, conexion, rival encontrado; en_linea.h)
	en_linea::Vista enLinea;
};

// lo que llega del juego para armar la mesa
struct MonstruoInicial { std::string carta; float x, z; bool rival, defensa; };
struct CartaInicial { std::string zona, carta; bool bocaAbajo; };

constexpr float DURACION_VUELO = 0.5f;   // s de la carta que vuela de la mano a su zona; classic-md-ritmo: MD CardMoveHand2MonstorZonePower0 0.38 + 0.12 (antes 0.6)
constexpr float DURACION_ROBO = 0.80f;   // s de la carta que sale del mazo y va a la mano (rama classic-duelo-arreglos); classic-md-ritmo: MD CardMoveDeck2HandMyself1+2 0.58 + 0.22 (antes 0.42)
constexpr float DURACION_ROBO_RIVAL = 0.45f;   // classic-md-ritmo: el rival roba con MD CardMoveField2Hand 0.45 s

class Mando {
public:
	// ---- lo pone el juego
	std::function<std::string(const std::string&)> clase; // normal/efecto/fusion/ritual/magia/trampa ("" si no se sabe)
	std::function<bool()> ataqueEnCurso;
	std::function<bool()> campoOcupado;    // la carta de campo esta en plena transicion (bosque creciendo o retirandose)
	bool log{ false };
	// Reparto al tomar el control (tocar()): estas cartas del jugador vuelven de la mesa a la mano (sus zonas quedan
	// libres) y la mano pasa a ser manoInteractiva. Sin manoInteractiva (YUGI_MESA=clasica) no cambia nada
	std::vector<std::string> aLaMano, manoInteractiva;   // cada paso del flujo a stderr ([mando] ...)

	// ---- el estado (lo lee la interfaz)
	std::array<Celda, NUM_ZONAS_MESA> zonas{};
	std::vector<std::string> manoJ;
	int manoR{ 5 };
	std::vector<std::string> cementerioJ, cementerioR;
	std::vector<std::string> desterradosJ, desterradosR, extraJ, extraR;   // rama classic-duelo-ui (sincronizarLado)
	bool demo{ true };          // demo automatica (ataques solos...): se apaga al tocar el mando
	int fase{ 2 };              // indice de interfaz::FASES
	float tFase{ 99.0f };       // s desde el ultimo cambio de fase (el cartel)
	Estado estado{ NAVEGAR };
	Foco foco{};
	std::vector<Opcion> opciones;
	int opcion{ 0 };
	float tMenu{ 0.0f };
	std::string tituloMenu;
	std::vector<int> validas;   // zonas elegibles (ELEGIR_ZONA, ELEGIR_OBJETIVO)
	int origenZona{ -1 };       // la zona del menu / del atacante
	int origenMano{ -1 };       // la carta de la mano del menu
	Accion pendiente{ AC_CANCELAR };
	struct Vuelo {
		bool activo{ false };
		std::string carta;
		int desdeMano{ -1 }, nMano{ 0 }, zona{ -1 };
		float t{ 0.0f };
		bool bocaAbajo{ false }, girada{ false };
		int serie{ 0 };          // cambia con cada vuelo (la interfaz toma de donde sale)
		Accion accion{ AC_CANCELAR };
	} vuelo;
	// rama classic-duelo-arreglos: la carta que se roba, del mazo a la mano (del jugador boca arriba al llegar; del rival
	// boca abajo). indice: su lugar en la mano (la interfaz no la dibuja en la mano mientras vuela)
	struct Robo {
		bool activo{ false }, rival{ false };
		std::string carta;
		int indice{ -1 };
		float t{ 0.0f };
		int serie{ 0 };
		float dur{ 0.0f };   // (rama classic-ps5-ritmo: 0 = DURACION_ROBO / _RIVAL; el del inicio del turno, ritmo_turno.h)
		bool turno{ false }; // (rama classic-ps5-ritmo: el robo de la Fase de Robo; el del jugador se muestra como en MD)
	} robo;
	void robarAnimado(bool rival, const std::string& carta, int indice)
	{
		robo.activo = true;
		robo.rival = rival;
		robo.carta = carta;
		robo.indice = indice;
		robo.t = 0.0f;
		robo.dur = 0.0f;
		robo.turno = false;
		robo.serie++;
		anotar("roba %s '%s' (mano %d)", rival ? "el rival" : "el jugador", rival ? "?" : carta.c_str(), indice);
	}
	std::string cartaDetalle;   // "" con el detalle de una carta boca abajo del rival: el dorso
	bool detalleDorso{ false };
	float tDetalle{ 0.0f };
	Estado antesDetalle{ NAVEGAR };
	// rama classic-pulido: la zona de la carta del detalle (-1: mano o lista; para el ATK/DEF actual) y el
	// desplazamiento del texto, en lineas (cruz arriba/abajo o stick derecho: ejeDetalle lo pone el juego). El maximo lo
	// fija la interfaz al dibujar (cuantas lineas no entran)
	int zonaDetalle{ -1 };
	float ejeDetalle{ 0.0f };
	mutable float desplDetalle{ 0.0f }, desplDetalleMax{ 0.0f };
	std::string aviso;
	float tAviso{ 99.0f };
	// rama classic-ps5-arreglos1: cartel grande (cut-in) cuando el rival activa una carta en el turno del jugador (una
	// trampa en respuesta, p. ej. Trap Hole): la carta, su nombre y quien la activa; el duelo espera a que se lea
	std::string corteCarta;
	bool corteRival{ true };
	float tCorte{ 99.0f };
	static constexpr float DURACION_CORTE = 2.2f;
	bool corteVisible() const { return !corteCarta.empty() && tCorte < DURACION_CORTE; }
	std::vector<Evento> eventos;
	float reloj{ 0.0f };

	// ---- duelo real (rama classic-integracion): con "duelo" las reglas de ia::Duelo mandan (lado 0 = el jugador). El
	// mando arma la jugada (ia::Jugada) y la entrega a "jugar" (el juego la aplica, anima y sincroniza la mesa con
	// sincronizarLado). bloqueado: turno del rival o una animacion en curso (solo se mira). Sin duelo (prueba_mando,
	// YUGI_MESA=clasica) todo sigue como la mesa libre de antes
	const ia::Duelo* duelo{ nullptr };
	std::function<bool(const ia::Jugada&)> jugar;
	std::function<std::string(const std::string&)> nombre;   // nombre visible de una carta (idioma)
	bool bloqueado{ false };
	// rama classic-md-fases: el cuadrado ya no pasa de fase ni el circulo termina el turno desde la batalla (antes:
	// classic-arreglos "fase pedida" y classic-bp-terminar "terminar pedido"); lo hace el selector de fases (OPTIONS)
	void circuloAjeno() {}   // (lo siguen llamando los arneses de prueba)
	VistaDuelo vista;
	std::vector<int> tributos;   // zonas de la mesa elegidas como sacrificio
	std::vector<std::pair<int, Celda>> sacrificados;
	bool focoDescarte{ false };   // (rama classic-arreglo-ciclo-md) ya se llevo el foco a la mano para el descarte de la Fase Final   // (rama classic-arreglo-ciclo-md) los que dejaron la mesa: md_ritmo_juego.h
	int tributosFaltan{ 0 };
	// rama classic-efectos: la activacion que se esta armando (costes y objetivos), la pregunta del duelo (responder
	// o elegir) y las cartas elegibles con el foco (mano o mesa) en ELEGIR_CARTA
	ia::Jugada armado;
	bool armando{ false }, enPregunta{ false }, eleccionDuelo{ false };
	int etapaArmado{ 0 };                  // 0 descartes, 1 objetivos
	std::vector<Foco> validasFoco;         // ELEGIR_CARTA
	std::vector<ia::Lugar> lugaresFoco;    //   y su lugar en el duelo
	std::vector<ia::Lugar> lugaresLista;   // AC_ELEGIR_LISTA: los candidatos de la lista
	// rama classic-ps5-elegir-cartas (PS5: "ahorita saca una lista de nombres, eso no sirve porque uno no se sabe el nombre de
	// todas las cartas"): la eleccion entre cartas de una pila (mazo, cementerio, desterradas, extra...) es una fila de CARTAS con
	// su arte, como el selector de cartas de Master Duel (lista_cartas_ui.h). idsLista[k] = la carta del candidato k ("" = boca
	// abajo / oculta del rival: se ve el dorso, no se revela)
	std::vector<std::string> idsLista;
	bool listaCartas() const { return estado == MENU && !idsLista.empty() && !opciones.empty() && opciones[0].accion == AC_ELEGIR_LISTA; }
	// la carta del candidato g no se puede ver (mano, mazo o extra del rival; sus monstruos / magias y trampas boca abajo)
	bool candidatoOculto(const ia::Lugar& g) const
	{
		if (!duelo || g.lado == 0) return false;
		switch (g.donde) {
		case ia::EN_MANO:
		case ia::EN_MAZO:
		case ia::EN_EXTRA: return true;
		case ia::EN_MONSTRUO: return g.i >= 0 && g.i < ia::ZONAS && duelo->l[1].m[(size_t)g.i].bocaAbajo;
		case ia::EN_MT: return g.i >= 0 && g.i < ia::ZONAS && duelo->l[1].mt[(size_t)g.i].bocaAbajo;
		default: return false;
		}
	}
	std::string tituloTexto;               // titulo del menu cuando no es una carta ("El rival ataca con ...")

	// ------------------------------------------------------------------------------------------ preparar
	// monstruos: cada uno a la zona de monstruo mas cercana de su lado; cartas: magias, trampas, campo y fusion;
	// mano: la del jugador. fuera: cartas de "monstruos" que empiezan en la mano (sus zonas quedan libres)
	void preparar(const std::vector<MonstruoInicial>& monstruos, const std::vector<CartaInicial>& cartas, const std::vector<std::string>& mano,
	              const std::vector<std::string>& fuera = {})
	{
		for (Celda& c : zonas) c = Celda{};
		manoJ.clear();
		cementerioJ.clear();
		cementerioR.clear();
		desterradosJ.clear(), desterradosR.clear(), extraJ.clear(), extraR.clear();   // (rama classic-duelo-ui)
		for (const MonstruoInicial& m : monstruos) {
			if (std::find(fuera.begin(), fuera.end(), m.carta) != fuera.end()) continue;
			int mejor = -1;
			float d = 1e9f;
			for (int z = 0; z < NUM_ZONAS_MESA; z++) {
				if (!esTipo(z, "monstruo") || ZONAS_MESA[z].rival != m.rival || !zonas[z].vacia()) continue;
				const float dx = ZONAS_MESA[z].x * ESCALA_MESA - m.x, dz = ZONAS_MESA[z].z * ESCALA_MESA - m.z;
				if (dx * dx + dz * dz < d) {
					d = dx * dx + dz * dz;
					mejor = z;
				}
			}
			if (mejor >= 0) {
				Celda& c = zonas[mejor];
				c.carta = m.carta;
				c.defensa = c.antesDefensa = m.defensa;
			}
		}
		for (const CartaInicial& k : cartas) {
			const int z = zonaPorId(k.zona);
			if (z < 0) continue;
			zonas[z].carta = k.carta.empty() ? std::string("?") : k.carta;   // "?": boca abajo sin carta conocida
			zonas[z].bocaAbajo = zonas[z].antesBocaAbajo = k.bocaAbajo;
		}
		manoJ = mano;
		foco = Foco{ F_MANO_J, (int)manoJ.size() / 2 };
		if (manoJ.empty()) foco = Foco{ F_ZONA, zonaPorId("jugador_monstruo_2") };
		estado = NAVEGAR;
	}

	// ------------------------------------------------------------------------------------------ consultas
	std::string idFoco() const { return idDe(foco); }
	static std::string idDe(const Foco& f)
	{
		if (f.tipo == F_MANO_J) return "jugador_mano_" + std::to_string(f.i);
		if (f.tipo == F_MANO_R) return "rival_mano_" + std::to_string(f.i);
		if (f.tipo == F_ENGRANAJE) return "engranaje";   // (rama classic-md-fases)
		return f.i >= 0 && f.i < NUM_ZONAS_MESA ? ZONAS_MESA[f.i].id : "";
	}
	std::string claseDe(const std::string& id) const { return clase ? clase(id) : std::string(); }
	bool esMonstruo(const std::string& id) const
	{
		const std::string c = claseDe(id);
		return c == "normal" || c == "efecto" || c == "fusion" || c == "ritual";
	}
	// la carta que se ve con este foco ("" si nada); dorso = true si se ve boca abajo (las del rival)
	std::string cartaVisible(const Foco& f, bool* dorso = nullptr) const
	{
		if (dorso) *dorso = false;
		if (f.tipo == F_MANO_J) return f.i >= 0 && f.i < (int)manoJ.size() ? manoJ[f.i] : "";
		if (f.tipo == F_MANO_R) {
			if (dorso) *dorso = true;
			return "";
		}
		if (f.tipo == F_ENGRANAJE || f.i < 0 || f.i >= NUM_ZONAS_MESA) return "";
		const ZonaMesa& z = ZONAS_MESA[f.i];
		if (strcmp(z.tipo, "desterrado") == 0) {   // rama classic-duelo-ui: la de arriba de las desterradas
			const auto& c = z.rival ? desterradosR : desterradosJ;
			return c.empty() ? "" : c.back();
		}
		if (strcmp(z.tipo, "cementerio") == 0) {
			const auto& c = z.rival ? cementerioR : cementerioJ;
			return c.empty() ? "" : c.back();
		}
		const Celda& c = zonas[f.i];
		if (c.vacia()) return "";
		if (c.bocaAbajo && (z.rival || c.carta == "?")) {
			if (dorso) *dorso = true;
			return "";
		}
		return c.carta;
	}
	bool hayCarta(const Foco& f) const
	{
		bool dorso = false;
		const std::string c = cartaVisible(f, &dorso);
		return !c.empty() || dorso;
	}
	int libresDe(const char* tipo) const
	{
		int n = 0;
		for (int z = 0; z < NUM_ZONAS_MESA; z++) {
			if (esTipo(z, tipo) && !ZONAS_MESA[z].rival && zonas[z].vacia()) n++;
		}
		return n;
	}

	// ------------------------------------------------------------------------------------------ cada cuadro
	// el usuario toco el mando (cualquier boton o stick): se apaga la demo automatica
	void tocar()
	{
		if (demo) {
			demo = false;
			if (!manoInteractiva.empty()) {
				for (int z = 0; z < NUM_ZONAS_MESA; z++) {
					Celda& c = zonas[z];
					if (ZONAS_MESA[z].rival || c.vacia() || std::find(aLaMano.begin(), aLaMano.end(), c.carta) == aLaMano.end()) continue;
					if (!c.bocaAbajo) emitir({ EV_A_LA_MANO, z, -1, 0, c.carta });
					anotar("'%s' vuelve de %s a la mano", c.carta.c_str(), ZONAS_MESA[z].id);
					c = Celda{};
				}
				manoJ = manoInteractiva;
				manoInteractiva.clear();
				estado = NAVEGAR;
				foco = Foco{ F_MANO_J, 0 };
			}
			emitir({ EV_DEMO_APAGADA, -1, -1, 0, "" });
			avisar("Demo automática apagada: tienes el control");
			anotar("demo apagada: el jugador tiene el control");
		}
	}

	// ---- Ajustes: una mano de prueba nueva o robar una carta. Solo con el foco libre (sin menu,
	// vuelo ni eleccion de zona); toman el control como cualquier boton (apagan la demo). Devuelven si se hizo
	bool libreParaMano() const { return estado == NAVEGAR && !vuelo.activo; }
	bool ponerMano(const std::vector<std::string>& mano)
	{
		if (!libreParaMano()) return false;
		tocar();
		manoJ = mano;
		foco = manoJ.empty() ? Foco{ F_ZONA, zonaPorId("jugador_monstruo_2") } : Foco{ F_MANO_J, 0 };
		anotar("mano nueva de %d cartas", (int)manoJ.size());
		return true;
	}
	bool robar(const std::string& carta, int maximo)
	{
		if (!libreParaMano() || (int)manoJ.size() >= maximo) return false;
		tocar();
		manoJ.push_back(carta);
		foco = Foco{ F_MANO_J, (int)manoJ.size() - 1 };
		anotar("roba '%s' (mano %d)", carta.c_str(), (int)manoJ.size());
		return true;
	}
	// las 5 zonas de monstruo del jugador quedan libres: lo que haya va al cementerio (para invocar en cualquier casilla)
	bool despejarMonstruosJ()
	{
		if (!libreParaMano()) return false;
		tocar();
		for (int z = 0; z < NUM_ZONAS_MESA; z++) {
			Celda& c = zonas[z];
			if (ZONAS_MESA[z].rival || !esTipo(z, "monstruo") || c.vacia()) continue;
			if (c.carta != "?") cementerioJ.push_back(c.carta);
			if (!c.bocaAbajo) emitir({ EV_AL_CEMENTERIO, z, -1, 0, c.carta });
			c = Celda{};
		}
		anotar("zonas de monstruo del jugador despejadas (cementerio %d)", (int)cementerioJ.size());
		return true;
	}

	// una carta destruida en combate (yugireload.cpp, al terminar la animacion del ataque) va al cementerio de su duenno
	void alCementerio(int z)
	{
		if (z < 0 || z >= NUM_ZONAS_MESA || zonas[z].vacia()) return;
		if (zonas[z].carta != "?") (ZONAS_MESA[z].rival ? cementerioR : cementerioJ).push_back(zonas[z].carta);
		anotar("'%s' destruida en combate (%s)", zonas[z].carta.c_str(), ZONAS_MESA[z].id);
		zonas[z] = Celda{};
	}

	// ---- rama classic-ia: un lado de la mesa queda como lo dejo el duelo (ia::Duelo, p. ej. tras cada jugada del
	// rival). Solo toca las zonas que cambiaron y arranca su animacion (llegada, volteo, giro); la mano del rival es
	// solo el numero de cartas
	void sincronizarLado(bool rival, const ia::Lado& x)
	{
		const std::string pre = rival ? "rival_" : "jugador_";
		for (int i = 0; i < ia::ZONAS; i++) {
			for (int mg = 0; mg < 2; mg++) {
				const int z = zonaPorId(pre + (mg ? "magia_" : "monstruo_") + std::to_string(i));
				if (z < 0) continue;
				Celda& c = zonas[z];
				const std::string id = mg ? x.mt[(size_t)i].id : x.m[(size_t)i].id;
				const bool abajo = mg ? x.mt[(size_t)i].bocaAbajo : x.m[(size_t)i].bocaAbajo;
				const bool def = mg ? false : x.m[(size_t)i].defensa;
				if (c.carta == id && c.bocaAbajo == abajo && c.defensa == def) continue;
				if (c.carta != id) {
					c = Celda{};
					c.carta = id;
					c.bocaAbajo = c.antesBocaAbajo = abajo;
					c.defensa = c.antesDefensa = def;
					c.tLlega = 0.0f;
				} else {
					if (c.bocaAbajo != abajo) {
						c.antesBocaAbajo = c.bocaAbajo;
						c.bocaAbajo = abajo;
						c.tVolteo = 0.0f;
					}
					if (c.defensa != def) {
						c.antesDefensa = c.defensa;
						c.defensa = def;
						c.tGiro = 0.0f;
					}
				}
			}
		}
		(rival ? cementerioR : cementerioJ) = x.cementerio;
		(rival ? desterradosR : desterradosJ) = x.desterrados;   // rama classic-duelo-ui: visores y contadores
		(rival ? extraR : extraJ) = x.extra;
		if (rival) manoR = (int)x.mano.size();
		anotar("lado %s sincronizado con el duelo (mano %d, cementerio %d)", rival ? "rival" : "jugador", (int)x.mano.size(), (int)x.cementerio.size());
	}

	void avanzar(float dt, uint32_t pulsos)
	{
		reloj += dt;
		tFase += dt;
		tAviso += dt;
		tCorte += dt;
		tMenu += dt;
		tDetalle += dt;
		if (cierreDetallePendiente && (estado != DETALLE || tDetalle >= DETALLE_ENTRA)) {   // (rama classic-arreglo-ciclo-md)
			if (estado == DETALLE) cerrarDetalle("pendiente");
			cierreDetallePendiente = false;
		}
		for (Celda& c : zonas) {
			c.tVolteo += dt;
			c.tGiro += dt;
			c.tLlega += dt;
			c.tActiva += dt;
		}
		// la mano pudo achicarse: el foco no se sale
		if (foco.tipo == F_MANO_J && foco.i >= (int)manoJ.size()) {
			if (manoJ.empty()) {
				foco = Foco{ F_ZONA, zonaPorId("jugador_monstruo_2") };
			} else {
				foco.i = (int)manoJ.size() - 1;
			}
		}
		if (robo.activo) {   // (rama classic-duelo-arreglos: no frena el mando; el duelo espera a que llegue)
			robo.t += dt / (robo.dur > 0.0f ? robo.dur : robo.rival ? DURACION_ROBO_RIVAL : DURACION_ROBO);   // (rama classic-md-ritmo: los tiempos de Master Duel)
			if (robo.t >= 1.0f) robo.activo = false;
		}
		if (vuelo.activo) {
			vuelo.t += dt / DURACION_VUELO;
			if (vuelo.t >= 1.0f) {
				aterrizar();
			}
			return;   // mientras vuela la carta no se acepta nada
		}
		// rama classic-efectos: el duelo pregunta al jugador (responder con una carta o elegir): se abre solo. Si el jugador
		// miraba el detalle de una carta desde la mesa (p. ej. X durante la animacion del ataque), la pregunta lo cierra y
		// sale (como MD): antes quedaba detras del detalle y OPTIONS decia "Primero responde a la pregunta" sin pregunta
		if (duelo && !bloqueado && duelo->ef.preg.tipo != ia::P_NADA && duelo->ef.preg.lado == 0 && estado == DETALLE && antesDetalle == NAVEGAR) {
			anotar("la pregunta del duelo cierra el detalle de '%s'", cartaDetalle.c_str());
			cambiarEstado(NAVEGAR);
		}
		if (duelo && !bloqueado && duelo->ef.preg.tipo != ia::P_NADA && duelo->ef.preg.lado == 0 && estado == NAVEGAR) {
			abrirPregunta();
			pulsos = 0;
		}
		// (rama classic-arreglo-ciclo-md, jugador humano: en la Fase Final con cartas de mas el duelo espera el descarte y
		// OPTIONS ya no ofrece fases; con el foco fuera de la mano no habia nada a la vista que hacer. Como MD, que abre la
		// seleccion: el foco va a la mano con el aviso, cada vez que hace falta descartar y X no tiene que hacer)
		{
			const bool descarte = duelo && !bloqueado && estado == NAVEGAR && duelo->activo == 0 && duelo->ganador < 0 && duelo->fase == ia::F_FINAL &&
			                      duelo->ef.preg.tipo == ia::P_NADA && !manoJ.empty() && (int)duelo->l[0].mano.size() > ia::limiteMano(*duelo, 0);
			if (descarte && foco.tipo != F_MANO_J && (!focoDescarte || ((pulsos & B_X) && !tieneMenu(foco)))) {
				foco = Foco{ F_MANO_J, (int)manoJ.size() - 1 };
				avisar(avisoDescarte());
				anotar("descarte en la Fase Final: el foco a la mano");
				pulsos &= ~B_X;
			}
			focoDescarte = descarte;
		}
		// (marcas de una pregunta que ya no esta, con el menu cerrado: no deben frenar OPTIONS ni el resto del turno)
		if (duelo && estado == NAVEGAR && (enPregunta || armando || eleccionDuelo) && !(duelo->ef.preg.tipo != ia::P_NADA && duelo->ef.preg.lado == 0)) {
			fprintf(stderr, "[mando] marcas de pregunta sin pregunta: se limpian\n");
			enPregunta = armando = eleccionDuelo = false;
		}
		// rama classic-md-fases: OPTIONS llega soltado (corto) desde duelo_juego.h (pulsarOptions); el selector de fases
		// abierto se queda con los botones, y la fase elegida sigue sola en cuanto el jugador queda libre
		pulsos &= ~B_OPTIONS;
		if (optionsPendiente) {
			optionsPendiente = false;
			pulsos |= B_OPTIONS;
		}
		selFases.t += dt;
		selFases.tCierre += dt;
		selFases.tElegida += dt;
		tEngranaje += dt;
		tCartelTurno += dt;
		tCartelInicio += dt;
		if (avanzarSelectorFases(pulsos)) return;
		seguirFaseObjetivo();
		switch (estado) {
		case NAVEGAR: navegar(pulsos); break;
		case MENU: menu(pulsos); break;
		case ELEGIR_ZONA:
		case ELEGIR_OBJETIVO:
		case ELEGIR_TRIBUTOS: elegir(pulsos); break;
		case ELEGIR_CARTA: elegirCarta(pulsos); break;
		case DETALLE:
			if (pulsos & (B_CIRCULO | B_TRIANGULO | B_X)) cerrarDetalle(nullptr);
			// rama classic-pulido: desplazar el texto largo (cruz: de a una linea; stick derecho: continuo)
			if (pulsos & B_ABAJO) desplDetalle = std::floor(desplDetalle + 1.0f);
			if (pulsos & B_ARRIBA) desplDetalle = std::ceil(desplDetalle - 1.0f);
			desplDetalle += ejeDetalle * 9.0f * dt;
			desplDetalle = std::max(0.0f, std::min(desplDetalle, desplDetalleMax));
			break;
		default: break;
		}
	}

	// rama classic-duelo-arreglos / classic-md-fases: el boton de fase del HUD (OPTIONS): "" si ahora no se puede elegir
	// fase; si se puede, a donde (la primera valida: "Iniciar Fase de Batalla", "Ir a M2" o "Finalizar turno")
	const char* botonFase() const
	{
		if (!duelo || vista.pantalla != VistaDuelo::DUELO || !selectorDisponible()) return "";
		const std::array<bool, SEL_FASES> v = fasesValidas();
		for (int k = 0; k < SEL_FASES; k++)
			if (v[(size_t)k]) return textoSelector(k);
		return "";
	}

	// la barra de ayuda: (boton, texto); botones: X, O, T, C (cuadrado), CRUZ, L1R1, STICK_R, R3
	std::vector<std::pair<std::string, std::string>> ayuda() const
	{
		std::vector<std::pair<std::string, std::string>> r;
		if (vista.sorteo) return r;   // (rama classic-duelo-arreglos: el sorteo trae su propia ayuda)
		switch (estado) {
		case NAVEGAR:
			r.push_back({ "CRUZ", "Mover" });
			if (duelo && foco.tipo == F_ZONA && (esTipo(foco.i, "cementerio") || esTipo(foco.i, "desterrado") || esTipo(foco.i, "fusion"))) {
				r.push_back({ "X", "Ver lista" });   // rama classic-duelo-ui: visor de la zona
			} else
			if (tieneMenu(foco)) {
				r.push_back({ "X", "Acciones" });
			} else if (hayCarta(foco)) {
				r.push_back({ "X", "Ver" });
			}
			if (hayCarta(foco)) r.push_back({ "T", "Detalle" });
			if (selFases.abierto) {   // rama classic-md-fases: el selector de fases (OPTIONS)
				r.clear();
				r.push_back({ "CRUZ", "Fase" });
				r.push_back({ "X", "Confirmar" });
				r.push_back({ "O", "Cerrar" });
				break;
			}
			if (foco.tipo == F_ENGRANAJE) r.push_back({ "X", "Menú del duelo" });
			if (duelo ? selectorDisponible() : true) r.push_back({ "C|OPTIONS", "Fases" });   // (rama classic-md-fases; classic-ps5-control2: y el cuadrado)
			r.push_back({ "STICK_R", "Cámara" });
			if (duelo) r.push_back({ "L3", "Registro" });   // rama classic-duelo-ui: registro del duelo y visores
			if (duelo) r.push_back({ "TÁCTIL", "Menú" });   // (rama classic-md-fases: el engranaje)
			break;
		case MENU:
			r.push_back({ "CRUZ", "Opción" });
			r.push_back({ "X", "Confirmar" });
			if (!(enPregunta && eleccionDuelo)) r.push_back({ "O", enPregunta ? "No activar" : "Cancelar" });
			break;
		case ELEGIR_ZONA:
			r.push_back({ "CRUZ", "Zona" });
			r.push_back({ "X", pendiente == AC_INVOCAR ? "Invocar" : pendiente == AC_ACTIVAR_MAGIA ? "Activar" : "Colocar" });
			r.push_back({ "O", "Atrás" });
			break;
		case ELEGIR_OBJETIVO:
			r.push_back({ "CRUZ", "Objetivo" });
			r.push_back({ "X", "Atacar" });
			r.push_back({ "O", "Atrás" });
			break;
		case ELEGIR_TRIBUTOS:
			r.push_back({ "CRUZ", "Monstruo" });
			r.push_back({ "X", "Sacrificar" });
			r.push_back({ "O", "Atrás" });
			break;
		case DETALLE:
			if (desplDetalleMax > 0.0f) r.push_back({ "CRUZ", "Desplazar" });   // (rama classic-pulido: o el stick derecho)
			r.push_back({ "O", "Cerrar" });
			break;
		case VOLANDO: break;
		case ELEGIR_CARTA:
			r.push_back({ "CRUZ", "Carta" });
			r.push_back({ "X", "Elegir" });
			if (!eleccionDuelo) r.push_back({ "O", "Atrás" });
			break;
		}
		return r;
	}

#include "fases_md.h"   // rama classic-md-fases: selector de fases (OPTIONS), engranaje y carteles de Master Duel

private:
	// ------------------------------------------------------------------------------------------ utilidades
	void anotar(const char* fmt, ...) const __attribute__((format(printf, 2, 3)))
	{
		if (!log) return;
		char b[512];
		va_list a;
		va_start(a, fmt);
		vsnprintf(b, sizeof(b), fmt, a);
		va_end(a);
		fprintf(stderr, "[mando] %6.2f %s\n", reloj, b);
	}
	void emitir(Evento e)
	{
		anotar("evento %s zona %d zona2 %d valor %d carta '%s'", NOMBRE_EVENTO[e.tipo], e.zona, e.zona2,
		       e.valor, e.carta.c_str());
		eventos.push_back(std::move(e));
	}
	// "Descarta hasta tener N cartas en la mano" con el limite de mano de ahora (Infinite Cards, Hieroglyph Lithograph...)
	std::string avisoDescarte() const
	{
		const int n = duelo ? ia::limiteMano(*duelo, 0) : ia::MAX_MANO;
		if (n == ia::MAX_MANO) return "Descarta hasta tener 6 cartas en la mano";
		char b[128];
		snprintf(b, sizeof(b), idioma::tr("Descarta hasta tener %d cartas en la mano"), n);
		return b;
	}
	void avisar(const std::string& txt)
	{
		aviso = txt;
		tAviso = 0.0f;
		anotar("aviso: %s", txt.c_str());
	}
	void cambiarEstado(Estado e)
	{
		if (e != estado) anotar("estado %s -> %s (foco %s)", NOMBRE_ESTADO[estado], NOMBRE_ESTADO[e], idFoco().c_str());
		estado = e;
	}

	// ------------------------------------------------------------------------------------------ navegacion
	// fila de la rejilla (0 mano J .. 5 mano R) y posicion x en la mesa
	int fila(const Foco& f) const
	{
		if (f.tipo == F_MANO_J) return 0;
		if (f.tipo == F_MANO_R) return 5;
		if (f.tipo == F_ENGRANAJE) return 0;   // (rama classic-md-fases: abajo a la izquierda, junto a la mano)
		const float z = ZONAS_MESA[f.i].z;
		return z > 4.0f ? 1 : z > 0.0f ? 2 : z > -4.0f ? 3 : 4;
	}
	float posX(const Foco& f) const
	{
		if (f.tipo == F_MANO_J) return ((float)f.i - 0.5f * (float)((int)manoJ.size() - 1)) * 2.0f;
		if (f.tipo == F_MANO_R) return -((float)f.i - 0.5f * (float)(manoR - 1)) * 2.0f;   // la del rival se ve girada
		if (f.tipo == F_ENGRANAJE) return -14.0f;   // (a la izquierda de toda la mesa)
		return ZONAS_MESA[f.i].x;
	}
	std::vector<Foco> todos() const
	{
		std::vector<Foco> r;
		for (int k = 0; k < (int)manoJ.size(); k++) r.push_back({ F_MANO_J, k });
		for (int k = 0; k < manoR; k++) r.push_back({ F_MANO_R, k });
		for (int z = 0; z < NUM_ZONAS_MESA; z++) r.push_back({ F_ZONA, z });
		if (duelo) r.push_back({ F_ENGRANAJE, -1 });   // (rama classic-md-fases: el engranaje, solo con el duelo real)
		return r;
	}
	// mueve el foco: dx -1/+1 en la fila; dy +1 hacia el rival (arriba), -1 hacia el jugador. Solo entre los que
	// cumplen "vale" (todos si no hay filtro)
	bool mover(int dx, int dy, const std::function<bool(const Foco&)>& vale = nullptr)
	{
		const int f0 = fila(foco);
		const float x0 = posX(foco);
		Foco mejor{ -1, -1 };
		float puntos = 1e9f;
		for (const Foco& f : todos()) {
			if (f == foco || (vale && !vale(f))) continue;
			const int fi = fila(f);
			const float x = posX(f);
			float p;
			if (dx != 0) {
				if (fi != f0 || (x - x0) * (float)dx <= 0.01f) continue;
				p = fabsf(x - x0);
			} else {
				if ((fi - f0) * dy <= 0) continue;
				p = (float)abs(fi - f0) * 100.0f + fabsf(x - x0);
			}
			if (p < puntos) {
				puntos = p;
				mejor = f;
			}
		}
		// eligiendo zona u objetivo, a los costados tambien se pasa a otra fila si en la propia no hay mas
		if (mejor.tipo < 0 && dx != 0 && vale) {
			for (const Foco& f : todos()) {
				if (f == foco || !vale(f)) continue;
				const float x = posX(f);
				if ((x - x0) * (float)dx <= 0.01f) continue;
				const float p = fabsf(x - x0) + (float)abs(fila(f) - f0) * 3.0f;
				if (p < puntos) {
					puntos = p;
					mejor = f;
				}
			}
		}
		if (mejor.tipo < 0) return false;
		foco = mejor;
		anotar("foco -> %s", idFoco().c_str());
		return true;
	}
	bool moverConPulsos(uint32_t p, const std::function<bool(const Foco&)>& vale = nullptr)
	{
		if (p & B_IZQ) return mover(-1, 0, vale);
		if (p & B_DER) return mover(1, 0, vale);
		if (p & B_ARRIBA) return mover(0, 1, vale);
		if (p & B_ABAJO) return mover(0, -1, vale);
		return false;
	}

	void navegar(uint32_t p)
	{
		moverConPulsos(p);
		if (foco.tipo == F_ENGRANAJE) {   // rama classic-md-fases: el engranaje (menu del duelo) con X
			if (p & B_X) {
				pedidoEngranaje = true;
				tEngranaje = 0.0f;
				anotar("X en el engranaje: menu del duelo");
			}
			return;
		}
		if (duelo && bloqueado) {   // turno del rival o animacion: solo mirar
			if (p & (B_TRIANGULO | B_X)) abrirDetalle(foco);
			return;
		}
		if (p & B_TRIANGULO) {
			abrirDetalle(foco);
		} else if (p & B_X) {
			if (tieneMenu(foco)) {
				abrirMenu();
			} else if (hayCarta(foco)) {
				abrirDetalle(foco);
			}
		}
	}

	bool siguienteFase()
	{
		if (duelo) {
			ia::Jugada j;
			j.tipo = ia::J_FASE;
			if (!jugar || !jugar(j)) {
				if (duelo->fase == ia::F_BATALLA && duelo->efectos && ia::hayForzadosM2(*duelo))   // rama classic-elecciones
					avisar("Amazoness Archers: tus monstruos deben atacar este turno");
				else
					avisar(duelo->fase == ia::F_FINAL ? avisoDescarte() : std::string("No se puede"));
				return false;
			} else if (duelo->activo == 0 && duelo->fase == ia::F_FINAL && (int)duelo->l[0].mano.size() > ia::limiteMano(*duelo, 0)) {
				avisar(avisoDescarte());
			}
			return true;
		}
		const int n = (int)(sizeof(CICLO_FASES) / sizeof(CICLO_FASES[0]));
		int k = 0;
		for (int i = 0; i < n; i++) {
			if (CICLO_FASES[i] == fase) k = i;
		}
		fase = CICLO_FASES[(k + 1) % n];
		tFase = 0.0f;
		emitir({ EV_FASE, -1, -1, fase, "" });
		anotar("fase -> %s", NOMBRE_FASE_LARGO[fase]);
		return true;
	}

	void abrirDetalle(const Foco& f)
	{
		bool dorso = false;
		const std::string c = cartaVisible(f, &dorso);
		if (c.empty() && !dorso) return;
		cartaDetalle = c;
		detalleDorso = dorso;
		tDetalle = 0.0f;
		zonaDetalle = f.tipo == F_ZONA ? f.i : -1;   // (rama classic-pulido)
		desplDetalle = desplDetalleMax = 0.0f;
		antesDetalle = estado == DETALLE ? NAVEGAR : estado;
		anotar("detalle de '%s'%s", c.c_str(), dorso ? " (dorso)" : "");
		cambiarEstado(DETALLE);
	}
public:   // (rama classic-arreglo-visor: lo abren los visores, duelo_visores.h)
	// rama classic-arreglo-visor: el detalle de una carta que no esta en la mesa (los visores del cementerio, las
	// desterradas, el extra y el registro). Como abrirDetalle: sin la zona ni el desplazamiento del detalle anterior y
	// volviendo al estado de verdad (p. ej. la pregunta del duelo que se abrio detras del visor; antes se forzaba
	// NAVEGAR y el circulo la cerraba: se perdia la opcion elegida y se volvia a abrir de cero)
	void abrirDetalleCarta(const std::string& c)
	{
		if (c.empty()) return;
		cartaDetalle = c;
		detalleDorso = false;
		tDetalle = 0.0f;
		zonaDetalle = -1;
		desplDetalle = desplDetalleMax = 0.0f;
		antesDetalle = estado == DETALLE || estado == VOLANDO ? NAVEGAR : estado;
		anotar("detalle de '%s' (visor; vuelve a %s)", c.c_str(), NOMBRE_ESTADO[antesDetalle]);
		cambiarEstado(DETALLE);
	}

	// cierra el detalle y vuelve a donde estaba. por != nullptr: lo cierra el duelo (rama classic-arreglo-ciclo-md: como
	// Master Duel, DuelHUD.CloseCardInfo al empezar cada movimiento de carta -EffectTaskCardMove.CommonMove-, al voltear
	// -EffectTaskCardFlipTurn- y en cada paso de la cadena -EffectTaskChainStep-: lo que pasa en la mesa no queda tapado)
	// (el que se acaba de abrir se cierra un par de cuadros despues: si no, abierto y cerrado en el mismo cuadro no se llega
	// a dibujar)
	static constexpr float DETALLE_ENTRA = 0.07f;
	bool cierreDetallePendiente{ false };
	void cerrarDetalle(const char* por)
	{
		if (estado != DETALLE) return;
		if (por && tDetalle < DETALLE_ENTRA) {
			cierreDetallePendiente = true;
			return;
		}
		cierreDetallePendiente = false;
		// (rama classic-arreglo-visor: si el detalle tapaba la pregunta del duelo y esa pregunta ya no esta, a la mesa)
		if (enPregunta && antesDetalle != NAVEGAR && (!duelo || duelo->ef.preg.tipo == ia::P_NADA || duelo->ef.preg.lado != 0)) {
			enPregunta = armando = eleccionDuelo = false;
			antesDetalle = NAVEGAR;
		}
		if (por) anotar("detalle de '%s' cerrado por el duelo: %s", cartaDetalle.c_str(), por);
		cambiarEstado(antesDetalle);
	}

	// ------------------------------------------------------------------------------------------ menus
public:   // (rama classic-duelo-ui: las jugables de duelo_cadena.h preguntan por los menus)
	bool tieneMenu(const Foco& f) const { return !opcionesPara(f).empty(); }

	std::vector<Opcion> opcionesPara(const Foco& f) const
	{
		if (f.tipo == F_ENGRANAJE) return {};   // (rama classic-md-fases: el engranaje no es una carta)
		if (duelo) return opcionesDuelo(f);
		std::vector<Opcion> r;
		auto agregar = [&](Accion a, const char* txt, bool activa = true, const char* motivo = "") { r.push_back({ a, txt, activa, motivo }); };
		if (f.tipo == F_MANO_J) {
			if (f.i < 0 || f.i >= (int)manoJ.size()) return r;
			const std::string& c = manoJ[f.i];
			const std::string cl = claseDe(c);
			if (esMonstruo(c)) {
				const bool hay = libresDe("monstruo") > 0;
				agregar(AC_INVOCAR, "Invocar", hay, "No hay zonas de monstruo libres");
				agregar(AC_COLOCAR_MONSTRUO, "Colocar boca abajo", hay, "No hay zonas de monstruo libres");
			} else if (cl == "magia" && esMagiaDeCampo(c)) {
				agregar(AC_ACTIVAR_CAMPO_MANO, "Activar", zonas[zonaPorId("jugador_campo")].vacia(), "Ya hay una carta de campo: destrúyela primero");
			} else if (cl == "magia") {
				const bool hay = libresDe("magia") > 0;
				agregar(AC_ACTIVAR_MAGIA, "Activar", hay, "No hay zonas de magia y trampa libres");
				agregar(AC_COLOCAR_MT, "Colocar", hay, "No hay zonas de magia y trampa libres");
			} else if (cl == "trampa") {
				agregar(AC_COLOCAR_MT, "Colocar", libresDe("magia") > 0, "No hay zonas de magia y trampa libres");
			}
			agregar(AC_CANCELAR, "Cancelar");
			return r;
		}
		if (f.tipo != F_ZONA || f.i < 0 || f.i >= NUM_ZONAS_MESA) return r;
		const ZonaMesa& z = ZONAS_MESA[f.i];
		const Celda& c = zonas[f.i];
		if (z.rival || c.vacia()) return r;   // lo del rival y las zonas vacias: solo se miran (el visor de la izquierda; triangulo = detalle grande)
		if (strcmp(z.tipo, "monstruo") == 0) {
			if (c.bocaAbajo) {
				agregar(AC_VOLTEAR_MONSTRUO, "Voltear (invocación)");
			} else {
				const bool ocupado = ataqueEnCurso && ataqueEnCurso();
				const char* motivo = c.defensa ? "En defensa no puede atacar" : ocupado ? "Espera a que termine el ataque" : "";
				agregar(AC_ATACAR, "Atacar", !c.defensa && !ocupado, motivo);
				agregar(AC_POSICION, c.defensa ? "Cambiar a ataque" : "Cambiar a defensa");
			}
			agregar(AC_CEMENTERIO, "Enviar al cementerio");
		} else if (strcmp(z.tipo, "magia") == 0) {
			if (c.bocaAbajo) {
				agregar(AC_ACTIVAR_SET, claseDe(c.carta) == "trampa" ? "Voltear" : "Activar");
			}
			agregar(AC_CEMENTERIO, "Enviar al cementerio");
		} else if (strcmp(z.tipo, "campo") == 0) {
			const bool ocupado = campoOcupado && campoOcupado();
			const char* motivo = "La carta de campo está en plena transición";
			if (c.bocaAbajo) {
				agregar(AC_ACTIVAR_CAMPO, "Activar", !ocupado, motivo);
			} else {
				agregar(AC_DESTRUIR_CAMPO, "Destruir", !ocupado, motivo);
			}
		} else {
			return r;   // fusion, mazo, cementerio: solo se miran
		}
		agregar(AC_CANCELAR, "Cancelar");
		return r;
	}
private:

	// ------------------------------------------------------------------------------------------ duelo real
	// la zona de la mesa -> su indice en ia::Lado (0..4) o -1
	static int indiceIA(int z)
	{
		if (z < 0 || z >= NUM_ZONAS_MESA) return -1;
		const char* id = ZONAS_MESA[z].id;
		const size_t n = strlen(id);
		return (esTipo(z, "monstruo") || esTipo(z, "magia")) && n > 0 ? id[n - 1] - '0' : -1;
	}
	std::string nombreDe(const std::string& id) const { return nombre ? nombre(id) : id; }

	// materiales de una fusion: primero de la mano (sin la Polymerization), despues del campo
	bool materialesFusion(const std::vector<std::string>& receta, int poli, ia::Jugada& j) const
	{
		const ia::Lado& x = duelo->l[0];
		std::vector<bool> usadaMano(x.mano.size(), false), usadaZona(ia::ZONAS, false);
		bool sustitutoUsado = false;
		for (const std::string& r : receta) {
			bool ok = false;
			for (int k = 0; k < (int)x.mano.size() && !ok; k++) {
				if (k == poli || usadaMano[(size_t)k] || x.mano[(size_t)k] != r) continue;
				usadaMano[(size_t)k] = true;
				j.manoExtra.push_back(k);
				ok = true;
			}
			for (int i = 0; i < ia::ZONAS && !ok; i++) {
				if (usadaZona[(size_t)i] || x.m[(size_t)i].id != r) continue;
				usadaZona[(size_t)i] = true;
				j.tributos.push_back(i);
				ok = true;
			}
			if (!ok && duelo->efectos && !sustitutoUsado) {   // (rama classic-efectos: un sustituto de fusion hace de este)
				for (int k = 0; k < (int)x.mano.size() && !ok; k++) {
					if (k == poli || usadaMano[(size_t)k] || !ia::esSustitutoFusion(x.mano[(size_t)k])) continue;
					usadaMano[(size_t)k] = true;
					j.manoExtra.push_back(k);
					ok = sustitutoUsado = true;
				}
				for (int i = 0; i < ia::ZONAS && !ok; i++) {
					if (usadaZona[(size_t)i] || !ia::esSustitutoFusion(x.m[(size_t)i].id)) continue;
					usadaZona[(size_t)i] = true;
					j.tributos.push_back(i);
					ok = sustitutoUsado = true;
				}
			}
			if (!ok) return false;
		}
		return !j.tributos.empty() || x.libre() >= 0;
	}

	// sacrificios de un ritual: los de menos ATK (mano y campo) hasta sumar el nivel
	bool sacrificiosRitual(int magia, int ritual, ia::Jugada& j) const
	{
		const ia::Lado& x = duelo->l[0];
		struct Cand { bool mano; int i, nivel, atk; };
		std::vector<Cand> c;
		for (int k = 0; k < (int)x.mano.size(); k++) {
			if (k == magia || k == ritual) continue;
			const ia::Carta* cc = duelo->carta(x.mano[(size_t)k]);
			if (cc && cc->monstruo && cc->nivel > 0) c.push_back({ true, k, cc->nivel, cc->atk });
		}
		for (int i = 0; i < ia::ZONAS; i++) {
			if (x.m[(size_t)i].vacia()) continue;
			c.push_back({ false, i, duelo->nivel(x.m[(size_t)i].id), duelo->atk(x.m[(size_t)i].id) });
		}
		std::sort(c.begin(), c.end(), [](const Cand& a, const Cand& b) { return a.atk < b.atk; });
		const int nec = duelo->nivel(x.mano[(size_t)ritual]);
		int suma = 0;
		for (const Cand& k : c) {
			if (suma >= nec) break;
			(k.mano ? j.manoExtra : j.tributos).push_back(k.i);
			suma += k.nivel;
		}
		return suma >= nec && (!j.tributos.empty() || x.libre() >= 0);
	}

	// rama classic-front-total (CU-FT-ACCION-ATACAR): hay a quien atacar (un monstruo del rival atacable, o el ataque
	// directo si no tiene monstruos o por un efecto); si no, "Atacar" se ve desactivado (antes daba "No se puede")
public:
	static bool hayBlanco(const ia::Duelo& d, int i)
	{
		if (d.l[1].monstruos() == 0) return true;
		for (int z = 0; z < ia::ZONAS; z++)
			if (!d.l[1].m[(size_t)z].vacia() && ia::motorAtacable(d, 1, z)) return true;
		(void)i;
		// (rama classic-arreglo-visor: el ataque directo por un efecto ya no cuenta: con monstruos del rival que no se
		// pueden atacar, p. ej. dos Command Knight, "Atacar" quedaba activo y daba "No se puede"; ese ataque es la
		// opcion "Atacar directamente")
		return false;
	}
	std::vector<Opcion> opcionesDuelo(const Foco& f) const
	{
		std::vector<Opcion> r;
		auto agregar = [&](Accion a, const std::string& txt, bool activa = true, const char* motivo = "", const ia::Jugada& j = ia::Jugada{}) {
			r.push_back({ a, txt, activa, motivo, j });
		};
		const ia::Duelo& d = *duelo;
		const ia::Lado& x = d.l[0];
		const bool mio = d.activo == 0 && d.ganador < 0 && !bloqueado;
		const bool principal = mio && d.principal();
		const char* fueraDeFase = !mio ? "Espera tu turno" : "Solo en tu fase principal";
		if (f.tipo == F_MANO_J) {
			if (f.i < 0 || f.i >= (int)manoJ.size() || f.i >= (int)x.mano.size()) return r;
			const std::string& id = x.mano[(size_t)f.i];
			const ia::Carta* c = d.carta(id);
			if (mio && d.fase == ia::F_FINAL && (int)x.mano.size() > ia::limiteMano(d, 0)) {
				ia::Jugada j;
				j.tipo = ia::J_DESCARTAR;
				j.mano = f.i;
				agregar(AC_JUGADA, "Descartar", true, "", j);
				agregar(AC_CANCELAR, "Cancelar");
				return r;
			}
			if (!c) return r;
			if (c->monstruo) {   // rama classic-efectos: invocacion especial por su procedimiento (Envoy, Chaos Emperor)
				const ia::DefCarta* df = ia::defCarta(id);
				if (df && df->puedeProcedimiento) opcionActivar(r, { 0, ia::EN_MANO, f.i }, mio, "Invocación especial");
				if (df && df->equipoDesdeMano) opcionActivar(r, { 0, ia::EN_MANO, f.i }, mio, "Equipar");
				if (df && df->ignicionMano) opcionActivar(r, { 0, ia::EN_MANO, f.i }, mio, "Activar efecto");
			}
			if (c->clase == "normal" || c->clase == "efecto") {
				int nec = d.efectos ? ia::motorTributos(d, 0, c) : ia::Duelo::tributosPara(c);   // (los Dioses Egipcios: 3; Cost Down)
				if (d.efectos && id == "fusilier_dragon_the_dual_mode_beast" && x.monstruos() < nec) nec = 0;
				if (d.efectos && id == "swift_gaia_the_fierce_knight" && x.mano.size() == 1) nec = 0;   // (Swift Gaia: unica carta de la mano)
				if (d.efectos && id == "behemoth_the_king_of_all_animals" && x.monstruos() == 1) nec = 1;   // (rama classic-efectos: Fusilier Dragon sin sacrificios)
				if (c->subtipo == "toon" && x.tieneBocaArriba("toon_world")) {
					const char* m = !principal ? fueraDeFase : x.monstruos() < nec ? "Faltan monstruos para el sacrificio"
					                : (nec == 0 && x.libre() < 0) ? "No hay zonas de monstruo libres" : "";
					agregar(AC_TOON, "Invocar Toon", !*m, m);
				}
				const char* m = !principal ? fueraDeFase : c->noInvocable ? "No se puede invocar de forma normal" : x.invocoNormal ? "Ya invocaste este turno"
				                : (d.efectos && !ia::motorPuedeInvocar(d, 0, id, nec)) ? "No se puede invocar ahora"
				                : (d.efectos && nec > 0 && ia::motorSinSacrificios(d)) ? "No se puede sacrificar"
				                : (d.efectos && !ia::motorPuedePagarMano(d, 0, 1)) ? "Faltan LP"
				                : x.monstruos() < nec ? "Faltan monstruos para el sacrificio" : (nec == 0 && x.libre() < 0) ? "No hay zonas de monstruo libres" : "";
				// rama classic-elecciones: The Last Warrior from Another Planet (o Scapegoat) prohibe invocar, no colocar:
				// "Invocar" se ve desactivado con su motivo
				const char* mInv = *m || !d.efectos || ia::motorPuedeInvocarM2(d, 0, 0) ? m
				                   : ia::hayArribaM2(d, -1, "the_last_warrior_from_another_planet") ? "The Last Warrior from Another Planet: nadie puede invocar"
				                                                                                     : "No puedes invocar este turno";
				agregar(AC_INVOCAR, nec ? std::string(idioma::tr("Invocar")) + " (" + std::to_string(nec) + " " + idioma::tr(nec == 1 ? "sacrificio" : "sacrificios") + ")"
				                        : std::string("Invocar"),
				        !*mInv, mInv);
				if (ia::Duelo::divina(c)) agregar(AC_COLOCAR_MONSTRUO, "Colocar boca abajo", false, "Un Dios no se puede colocar");
				else if (d.efectos && ia::motorLuzIntervencion(d))   // rama classic-front-total: con Light of Intervention colocar es invocar boca arriba
					agregar(AC_COLOCAR_MONSTRUO, "Colocar boca abajo", false, "Light of Intervention: no se puede colocar");
				else agregar(AC_COLOCAR_MONSTRUO, "Colocar boca abajo", !*m, m);
			} else if (c->clase == "ritual") {
				agregar(AC_INVOCAR, "Invocar", false, "Se invoca con su magia de ritual");
			} else if (c->magia || c->trampa) {
				const bool hayMT = x.libreMT() >= 0;
				// rama classic-front-total (CU-FT-ACCION-COLOCAR-MT): las mismas condiciones que el motor (Cold Wave, Chain Energy)
				const char* m = !principal ? fueraDeFase : !hayMT ? "No hay zonas de magia y trampa libres"
				                : (d.efectos && ia::ondaFria(d)) ? "Cold Wave: no se pueden colocar ni activar magias y trampas"
				                : (d.efectos && !ia::motorPuedePagarMano(d, 0, 1)) ? "Faltan LP" : "";
				if (c->magia && id == "polymerization" && d.base) {
					for (const std::string& fus : x.extra) {
						auto it = d.base->fusiones.find(fus);
						if (it == d.base->fusiones.end()) continue;
						ia::Jugada j;
						j.tipo = ia::J_FUSION;
						j.mano = f.i;
						j.carta = fus;
						if (!materialesFusion(it->second, f.i, j)) continue;
						bool repetida = false;
						for (const Opcion& o : r) repetida |= o.accion == AC_JUGADA && o.jugada.carta == fus;
						if (!repetida) agregar(AC_JUGADA, std::string(idioma::tr("Fusión")) + ": " + nombreDe(fus), principal, fueraDeFase, j);
					}
				}
				if (c->magia && d.base) {
					for (int k = 0; k < (int)x.mano.size(); k++) {
						if (!d.base->ritualCon(x.mano[(size_t)k], id)) continue;   // (rama classic-probador: tambien las genericas)
						ia::Jugada j;
						j.tipo = ia::J_RITUAL;
						j.mano = f.i;
						j.mano2 = k;
						j.carta = x.mano[(size_t)k];
						const bool ok = sacrificiosRitual(f.i, k, j);
						agregar(AC_JUGADA, std::string(idioma::tr("Ritual")) + ": " + nombreDe(x.mano[(size_t)k]), principal && ok,
						        !principal ? fueraDeFase : "Faltan niveles para el sacrificio", j);
					}
				}
				if (c->magia && ia::defCarta(id)) opcionActivar(r, { 0, ia::EN_MANO, f.i }, mio);   // rama classic-efectos
				else if (id == "toon_world") agregar(AC_ACTIVAR_MAGIA, "Activar", !*m, m);
				else if (c->magia) opcionActivar(r, { 0, ia::EN_MANO, f.i }, mio);
				agregar(AC_COLOCAR_MT, "Colocar", !*m, m);
			}
			agregar(AC_CANCELAR, "Cancelar");
			return r;
		}
		if (f.tipo != F_ZONA || f.i < 0 || f.i >= NUM_ZONAS_MESA) return r;
		const ZonaMesa& z = ZONAS_MESA[f.i];
		// rama classic-efectos: las magias/trampas propias colocadas (y la de campo) se activan desde su zona
		if (!z.rival && (strcmp(z.tipo, "magia") == 0 || strcmp(z.tipo, "campo") == 0)) {
			const bool campo = strcmp(z.tipo, "campo") == 0;
			const int k = campo ? ia::ZONA_CAMPO : indiceIA(f.i);
			const ia::MagiaTrampa* mt = d.mtEn({ 0, campo ? ia::EN_CAMPO : ia::EN_MT, k });
			if (!mt || mt->vacia() || !mt->bocaAbajo) return r;
			opcionActivar(r, { 0, campo ? ia::EN_CAMPO : ia::EN_MT, k }, mio);
			agregar(AC_CANCELAR, "Cancelar");
			return r;
		}
		if (z.rival || strcmp(z.tipo, "monstruo") != 0) return r;   // campo, mazos: solo se miran
		const int i = indiceIA(f.i);
		if (i < 0 || x.m[(size_t)i].vacia()) return r;
		const ia::Monstruo& mo = x.m[(size_t)i];
		if (!mo.bocaAbajo) {   // rama classic-efectos: efecto de ignicion
			const ia::DefCarta* df = ia::defCarta(mo.id);
			if (df && ia::geminisPendiente(d, { 0, ia::EN_MONSTRUO, i }))   // (rama classic-torneo-extras: Geminis, Il Blud)
				opcionActivar(r, { 0, ia::EN_MONSTRUO, i }, mio, "Invocación normal (Géminis)");
			else if (df && df->ignicion) opcionActivar(r, { 0, ia::EN_MONSTRUO, i }, mio, "Activar efecto");
		}
		if (d.efectos)   // rama classic-motor2: los X-Y-Z se invocan desterrando sus piezas del campo (sin Polymerization)
			for (const std::string& fus : x.extra) {
				std::vector<int> mats;
				if (!ia::contactoPosible(d, 0, fus, &mats) || std::find(mats.begin(), mats.end(), i) == mats.end()) continue;
				bool repetida = false;
				for (const Opcion& o : r) repetida |= o.accion == AC_JUGADA && o.jugada.tipo == ia::J_FUSION && o.jugada.carta == fus;
				if (repetida) continue;
				ia::Jugada j;
				j.tipo = ia::J_FUSION;
				j.carta = fus;
				j.tributos = mats;
				const bool puede = principal && ia::motorPuedeInvocarM2(d, 0, 2);
				agregar(AC_JUGADA, std::string(idioma::tr("Invocación especial")) + ": " + nombreDe(fus), puede, !principal ? fueraDeFase : "No se puede invocar ahora", j);
			}
		// rama classic-front-total (CU-FT-ACCION-POSICION): lo mismo que el motor (J_POSICION): Second Goblin permite otro
		// cambio; Gaap, Level Limit y compania lo impiden (antes el menu lo ofrecia y daba "No se puede")
		const bool goblin = d.efectos && ia::goblinCambia(d, 0, i);
		const char* mPos = !principal ? fueraDeFase : (mo.llegoEsteTurno && !goblin) ? "Llegó este turno" : (mo.cambioPosicion && !goblin) ? "Ya cambió de posición este turno"
		                   : (d.efectos && !goblin && ia::motorPosicionFija(d, 0, i)) ? "Un efecto le impide cambiar de posición" : "";
		if (mo.bocaAbajo) {
			agregar(AC_VOLTEAR_MONSTRUO, "Voltear (invocación)", !*mPos, mPos);
		} else {
			const char* mPuede = !mio ? "Espera tu turno" : d.fase != ia::F_BATALLA ? "Solo en la fase de batalla" : d.turno <= 1 ? "No se ataca en el primer turno"
			                     : mo.defensa ? "En defensa no puede atacar" : mo.ataco ? "Ya atacó este turno"
			                     : !d.puedeAtacar(0, i) ? "Un efecto le impide atacar"   // (rama classic-efectos: Swords, Gravity Bind...)
			                     : "";
			const char* mAt = *mPuede ? mPuede : !hayBlanco(d, i) ? "No hay monstruos del rival que se puedan atacar" : "";
			agregar(AC_ATACAR, "Atacar", !*mAt, mAt);
			// rama classic-efectos: ataque directo por un efecto (Toons, Drillago, Gear Golem...) con monstruos del rival
			// (rama classic-arreglo-visor: tambien sin ninguno que se pueda atacar: dos Command Knight, Guardian Kay'est)
			if (!*mPuede && d.l[1].monstruos() > 0 && ia::motorAtaqueDirecto(d, 0, i) && ia::motorPuedeDirecto(d, 0, i)) {
				ia::Jugada j;
				j.tipo = ia::J_ATACAR;
				j.zona = i;
				j.objetivo = -1;
				agregar(AC_JUGADA, "Atacar directamente", true, "", j);
			}
			agregar(AC_POSICION, mo.defensa ? "Cambiar a ataque" : "Cambiar a defensa", !*mPos, mPos);
		}
		agregar(AC_CANCELAR, "Cancelar");
		return r;
	}

	// ---- rama classic-efectos: activar cartas con efecto, responder y elegir
	static std::string textoEfecto(const std::string& clave) { return ia::textoEfecto(clave.c_str(), idioma::ingles()); }
	// la opcion "Activar" de una carta (mano, zona o monstruo), con el motivo si ahora no se puede
	void opcionActivar(std::vector<Opcion>& r, const ia::Lugar& f, bool mio, const char* texto = "Activar") const
	{
		ia::Jugada j = ia::jugadaActivar(f);
		j.carta = ia::cartaEn(*duelo, f);
		if (!ia::defCarta(j.carta)) {
			r.push_back({ AC_ACTIVAR_EFECTO, texto, false, textoEfecto("ef.no_disponible"), j });
			return;
		}
		const std::string m = !mio ? std::string("Espera tu turno") : ia::motivoNoActivar(*duelo, 0, f, false);
		r.push_back({ AC_ACTIVAR_EFECTO, texto, m.empty(), !mio ? m : textoEfecto(m), j });
	}
	// donde esta un lugar del duelo en la mesa (foco) o false (cementerio, mazo...)
	static bool focoDe(const ia::Lugar& g, Foco& f)
	{
		const std::string pre = g.lado ? "rival_" : "jugador_";
		if (g.donde == ia::EN_MANO && g.lado == 0) {
			f = Foco{ F_MANO_J, g.i };
			return true;
		}
		int z = -1;
		if (g.donde == ia::EN_MONSTRUO) z = zonaPorId(pre + "monstruo_" + std::to_string(g.i));
		else if (g.donde == ia::EN_MT) z = zonaPorId(pre + "magia_" + std::to_string(g.i));
		else if (g.donde == ia::EN_CAMPO) z = zonaPorId(pre + "campo");
		if (z < 0) return false;
		f = Foco{ F_ZONA, z };
		return true;
	}
	// el nombre de un candidato para la lista ("Dark Magician (cementerio rival)")
	std::string nombreLugar(const ia::Lugar& g) const
	{
		static const char* const DONDE[] = { "campo", "campo", "mano", "cementerio", "campo", "desterrada", "mazo", "fusión" };
		if (g.donde == ia::EN_MONSTRUO && g.i < 0) return idioma::tr("Ataque directo");   // (rama classic-reglas-oficiales: repeticion del ataque)
		if (candidatoOculto(g)) return std::string(idioma::tr("Carta boca abajo")) + " (" + idioma::tr(DONDE[g.donde]) + " " + idioma::tr("rival") + ")";   // (rama classic-ps5-elegir-cartas: no se revela)
		std::string r = nombreDe(ia::cartaEn(*duelo, g));
		return r + " (" + idioma::tr(DONDE[g.donde]) + (g.lado ? std::string(" ") + idioma::tr("rival") : std::string()) + ")";
	}
	// elegir entre lugares: con el foco si estan todos en la mesa o la mano; si no, una lista
	void pedirEleccion(const std::vector<ia::Lugar>& cands, const std::string& aviso)
	{
		validasFoco.clear();
		lugaresFoco.clear();
		bool todosMesa = !cands.empty();
		for (const ia::Lugar& g : cands) {
			Foco f;
			if (!focoDe(g, f)) {
				todosMesa = false;
				break;
			}
			validasFoco.push_back(f);
			lugaresFoco.push_back(g);
		}
		avisar(aviso);
		tituloTexto = aviso;   // (rama classic-ps5-control2: la pregunta queda escrita en pantalla mientras se elige: pregunta_ui.h)
		if (todosMesa) {
			foco = validasFoco[0];
			cambiarEstado(ELEGIR_CARTA);
			return;
		}
		opciones.clear();
		lugaresLista = cands;
		idsLista.clear();
		// (rama classic-ps5-elegir-cartas: una sola carta con "puedes" es un si/no -p. ej. la posicion de Monster Reborn-: texto)
		bool siNo = eleccionDuelo && duelo->ef.preg.opcional && cands.size() == 1;
		for (const ia::Lugar& g : cands) siNo = siNo || (g.donde == ia::EN_MONSTRUO && g.i < 0);   // ("Ataque directo": una opcion, no una carta)
		for (int k = 0; k < (int)cands.size(); k++) {
			ia::Jugada j;
			j.objetivo = k;
			opciones.push_back({ AC_ELEGIR_LISTA, nombreLugar(cands[(size_t)k]), true, "", j });
			if (!siNo) idsLista.push_back(candidatoOculto(cands[(size_t)k]) ? std::string() : ia::cartaEn(*duelo, cands[(size_t)k]));
		}
		if (!eleccionDuelo) opciones.push_back({ AC_CANCELAR, "Cancelar", true, "" });
		else if (duelo->ef.repeticion.hay) opciones.push_back({ AC_PASAR, "No atacar", true, "" });   // (rama classic-reglas-oficiales: repeticion del ataque)
		else if (duelo->ef.preg.opcional) opciones.push_back({ AC_PASAR, "No", true, "" });   // (rama classic-efectos: "puedes")
		opcion = 0;
		tMenu = 0.0f;
		tituloMenu.clear();
		tituloTexto = aviso;
		cambiarEstado(MENU);
	}
	// sigue armando la activacion: descartes, objetivos y, completa, la jugada
	void seguirArmado()
	{
		const ia::Lugar f = ia::fuenteDe(armado, 0);
		const ia::DefCarta* df = ia::defCarta(ia::cartaEn(*duelo, f));
		if (!df) {
			cancelarArmado();
			return;
		}
		if ((int)armado.descartes.size() < df->descartes) {
			std::vector<ia::Lugar> c;
			for (int k = 0; k < (int)duelo->l[0].mano.size(); k++) {
				if ((f.donde == ia::EN_MANO && k == f.i) || std::find(armado.descartes.begin(), armado.descartes.end(), k) != armado.descartes.end()) continue;
				if (!ia::descarteVale(*duelo, ia::cartaEn(*duelo, f), duelo->l[0].mano[(size_t)k])) continue;   // (Light of Judgment: un LUZ)
				c.push_back({ 0, ia::EN_MANO, k });
			}
			etapaArmado = 0;
			pedirEleccion(c, idioma::tr("Elige una carta para descartar"));
			return;
		}
		// costes elegidos (sacrificios de Obelisk / Crush Card Virus, destierros de Envoy)
		const int nCostes = df->candCostes && !(ia::costesDeProcedimiento(*df) && f.donde != ia::EN_MANO) ? df->nCostes : 0;
		if ((int)armado.costes.size() < nCostes) {
			const std::vector<ia::Lugar> c = df->candCostes(*duelo, 0, f, armado.costes);
			if (c.empty()) {
				avisar("No se puede");
				cancelarArmado();
				return;
			}
			etapaArmado = 2;
			pedirEleccion(c, idioma::tr(c[0].donde == ia::EN_CEMENTERIO ? "Elige la carta a desterrar (coste)" : "Elige el monstruo a sacrificar (coste)"));
			return;
		}
		if ((int)armado.objs.size() < df->nObj) {
			std::vector<ia::Lugar> c;
			for (const ia::Lugar& g : ia::objetivosDe(*duelo, 0, *df, f))
				if (std::find(armado.objs.begin(), armado.objs.end(), g) == armado.objs.end()) c.push_back(g);
			if ((int)c.size() == df->nObj - (int)armado.objs.size()) {   // (un solo objetivo posible: el atacante, el invocado)
				for (const ia::Lugar& g : c) armado.objs.push_back(g);
				seguirArmado();
				return;
			}
			etapaArmado = 1;
			pedirEleccion(c, idioma::tr("Elige el objetivo"));
			return;
		}
		armando = false;
		const bool ok = jugar && jugar(armado);
		if (!ok) avisar("No se puede");
		enPregunta = false;
		Foco ff;
		if (f.donde != ia::EN_MANO && focoDe(f, ff)) foco = ff;
		cambiarEstado(NAVEGAR);
	}
	void cancelarArmado()
	{
		armando = false;
		if (enPregunta && duelo->ef.preg.tipo == ia::P_RESPUESTA) {
			abrirPregunta();   // (vuelve a la pregunta: activar otra o no activar)
			return;
		}
		enPregunta = false;
		cambiarEstado(NAVEGAR);
	}
	// lo que acaba de pasar (titulo de la pregunta)
	std::string textoVentana() const
	{
		const ia::EstadoEfectos& e = duelo->ef;
		if (!e.cadena.empty()) {
			const ia::Eslabon& t = e.cadena.back();
			return std::string(idioma::tr(t.lado == 1 ? "El rival activó" : "Activaste")) + " " + nombreDe(t.carta);
		}
		switch (e.ventana.tipo) {
		case ia::V_ATAQUE: return std::string(idioma::tr("El rival ataca con")) + " " + nombreDe(e.ventana.carta);
		case ia::V_INVOCACION_NORMAL:
		case ia::V_VOLTEO:
		case ia::V_INVOCACION_ESPECIAL: return std::string(idioma::tr(e.ventana.lado == 1 ? "El rival invocó" : "Invocaste")) + " " + nombreDe(e.ventana.carta);
		case ia::V_FASE_FINAL: return idioma::tr("Fase final del rival");
		case ia::V_INICIO_BATALLA: return idioma::tr("El rival entra en la fase de batalla");
		case ia::V_PASO_DANNO: return std::string(idioma::tr("Paso de daño")) + ": " + nombreDe(e.ventana.carta);
		case ia::V_ESPERA: return idioma::tr(e.ventana.lado == 1 ? "Fase de espera del rival" : "Tu fase de espera");   // rama classic-motor2
		case ia::V_FIN_BATALLA: return idioma::tr("El rival termina la fase de batalla");
		case ia::V_COLOCADO: return idioma::tr("El rival colocó un monstruo");
		case ia::V_DANNO: return idioma::tr("Recibiste daño");   // (rama classic-reglas-oficiales)
		default: return idioma::tr("¿Activar una carta?");
		}
	}
	// el duelo espera algo del jugador: la lista de cartas que puede activar (y "No activar") o una eleccion
	void abrirPregunta()
	{
		const ia::Pregunta& p = duelo->ef.preg;
		enPregunta = true;
		armando = false;
		if (p.tipo == ia::P_ELEGIR) {
			eleccionDuelo = true;
			const std::string txt = nombreDe(p.carta) + ": " + textoEfecto(p.texto);
			pedirEleccion(p.cands, txt);
			return;
		}
		eleccionDuelo = false;
		opciones.clear();
		bool primera = true;
		for (const ia::Lugar& g : ia::activables(*duelo, 0, true)) {
			ia::Jugada j = ia::jugadaActivar(g);
			j.carta = ia::cartaEn(*duelo, g);
			opciones.push_back({ AC_ACTIVAR_EFECTO, std::string(idioma::tr("Activar")) + ": " + nombreDe(j.carta), true, "", j });
			Foco ff;
			if (primera && focoDe(g, ff)) foco = ff;
			primera = false;
		}
		opciones.push_back({ AC_PASAR, "No activar", true, "" });
		opcion = 0;
		tMenu = 0.0f;
		tituloMenu.clear();
		tituloTexto = textoVentana();
		anotar("pregunta: %s (%d opciones)", tituloTexto.c_str(), (int)opciones.size());
		cambiarEstado(MENU);
	}
	// elegir una carta con el foco (ELEGIR_CARTA)
	void elegirCarta(uint32_t p)
	{
		auto vale = [this](const Foco& f) { return std::find(validasFoco.begin(), validasFoco.end(), f) != validasFoco.end(); };
		moverConPulsos(p, vale);
		if (p & B_TRIANGULO) {
			abrirDetalle(foco);
			return;
		}
		if ((p & B_CIRCULO) && !eleccionDuelo) {
			cancelarArmado();
			return;
		}
		if ((p & B_CIRCULO) && eleccionDuelo && duelo->ef.preg.opcional) {   // ("puedes": O = no)
			ejecutar(AC_PASAR);
			return;
		}
		if (!(p & B_X) || !vale(foco)) return;
		const size_t k = (size_t)(std::find(validasFoco.begin(), validasFoco.end(), foco) - validasFoco.begin());
		elegido(k);
	}
	// el candidato k (de validasFoco / lugaresFoco o de la lista)
	void elegido(size_t k)
	{
		if (eleccionDuelo) {
			ia::Jugada j;
			j.tipo = ia::J_ELEGIR;
			j.objetivo = (int)k;
			if (!jugar || !jugar(j)) avisar("No se puede");
			eleccionDuelo = enPregunta = false;
			cambiarEstado(NAVEGAR);   // (si quedan por elegir, la pregunta se abre de nuevo)
			return;
		}
		const ia::Lugar g = estado == ELEGIR_CARTA ? lugaresFoco[k] : lugaresLista[k];
		if (etapaArmado == 0) armado.descartes.push_back(g.i);
		else if (etapaArmado == 2) armado.costes.push_back(g);
		else armado.objs.push_back(g);
		seguirArmado();
	}

	void ejecutarDuelo(Accion a)
	{
		tributos.clear();
		tributosFaltan = 0;
		switch (a) {
		case AC_CANCELAR:
			if (armando) cancelarArmado();
			else cambiarEstado(NAVEGAR);
			break;
		case AC_ACTIVAR_EFECTO:   // rama classic-efectos
			armado = opciones[(size_t)opcion].jugada;
			armado.objs.clear();
			armado.descartes.clear();
			armado.costes.clear();
			armando = true;
			seguirArmado();
			break;
		case AC_PASAR: {
			ia::Jugada j;
			j.tipo = ia::J_PASAR;
			if (!jugar || !jugar(j)) avisar("No se puede");
			enPregunta = false;
			cambiarEstado(NAVEGAR);
			break;
		}
		case AC_ELEGIR_LISTA: elegido((size_t)opciones[(size_t)opcion].jugada.objetivo); break;
		case AC_JUGADA: {
			const ia::Jugada j = opciones[(size_t)opcion].jugada;
			if (!jugar || !jugar(j)) avisar("No se puede");
			cambiarEstado(NAVEGAR);
			break;
		}
		case AC_INVOCAR:
		case AC_COLOCAR_MONSTRUO:
		case AC_TOON: {
			const std::string id = origenMano >= 0 && origenMano < (int)manoJ.size() ? manoJ[(size_t)origenMano] : "";
			tributosFaltan = duelo->efectos ? ia::motorTributos(*duelo, 0, duelo->carta(id)) : ia::Duelo::tributosPara(duelo->carta(id));
			if (duelo->efectos && id == "fusilier_dragon_the_dual_mode_beast" && duelo->l[0].monstruos() < tributosFaltan) tributosFaltan = 0;   // (Fusilier Dragon)
			if (duelo->efectos && id == "swift_gaia_the_fierce_knight" && duelo->l[0].mano.size() == 1) tributosFaltan = 0;   // (Swift Gaia: unica de la mano)
			if (duelo->efectos && id == "moisture_creature" && duelo->l[0].monstruos() >= 3 && ia::contarMT(*duelo, 1) > 0) tributosFaltan = 3;   // (Moisture Creature: con 3)
			if (duelo->efectos && id == "gilford_the_lightning" && duelo->l[0].monstruos() >= 3 && duelo->l[1].monstruos() > 0) tributosFaltan = 3;   // (Gilford: con 3)
			if (duelo->efectos && id == "behemoth_the_king_of_all_animals" && duelo->l[0].monstruos() == 1) tributosFaltan = 1;   // (Behemoth: con 1)
			if (tributosFaltan > 0) {
				validas.clear();
				for (int z = 0; z < NUM_ZONAS_MESA; z++) {
					if (esTipo(z, "monstruo") && !ZONAS_MESA[z].rival && !zonas[z].vacia()) validas.push_back(z);
				}
				enfocarValida(0.0f);
				avisar(tributosFaltan == 1 ? "Elige 1 monstruo para sacrificar" : "Elige 2 monstruos para sacrificar");
				cambiarEstado(ELEGIR_TRIBUTOS);
				break;
			}
			validas = libres("monstruo");
			if (validas.empty()) {
				avisar("No hay zonas libres");
				cambiarEstado(NAVEGAR);
				break;
			}
			enfocarValida(0.0f);
			cambiarEstado(ELEGIR_ZONA);
			break;
		}
		case AC_ACTIVAR_MAGIA:
		case AC_COLOCAR_MT:
			validas = libres("magia");
			if (validas.empty()) {
				avisar("No hay zonas libres");
				cambiarEstado(NAVEGAR);
				break;
			}
			enfocarValida(0.0f);
			cambiarEstado(ELEGIR_ZONA);
			break;
		case AC_ATACAR: {
			validas.clear();
			for (int z = 0; z < NUM_ZONAS_MESA; z++) {
				if (esTipo(z, "monstruo") && ZONAS_MESA[z].rival && !zonas[z].vacia() && ia::motorAtacable(*duelo, 1, indiceIA(z))) validas.push_back(z);
			}
			if (validas.empty() && duelo->l[1].monstruos() > 0) {   // (rama classic-efectos: ninguno se puede atacar)
				avisar("No se puede");
				cambiarEstado(NAVEGAR);
				break;
			}
			if (validas.empty()) {   // sin monstruos del rival: ataque directo
				ia::Jugada j;
				j.tipo = ia::J_ATACAR;
				j.zona = indiceIA(origenZona);
				j.objetivo = -1;
				if (!jugar || !jugar(j)) avisar("No se puede");
				cambiarEstado(NAVEGAR);
				break;
			}
			enfocarValida(ZONAS_MESA[origenZona].x);
			cambiarEstado(ELEGIR_OBJETIVO);
			break;
		}
		case AC_POSICION:
		case AC_VOLTEAR_MONSTRUO: {
			ia::Jugada j;
			j.tipo = a == AC_POSICION ? ia::J_POSICION : ia::J_VOLTEAR;
			j.zona = indiceIA(origenZona);
			if (!jugar || !jugar(j)) avisar("No se puede");
			cambiarEstado(NAVEGAR);
			break;
		}
		default: cambiarEstado(NAVEGAR); break;
		}
	}

	void abrirMenu()
	{
		opciones = opcionesPara(foco);
		opcion = 0;
		while (opcion < (int)opciones.size() - 1 && !opciones[opcion].activa) opcion++;
		tMenu = 0.0f;
		origenMano = foco.tipo == F_MANO_J ? foco.i : -1;
		origenZona = foco.tipo == F_ZONA ? foco.i : -1;
		tituloMenu = cartaVisible(foco);
		tituloTexto.clear();
		std::string lista;
		for (const Opcion& o : opciones) lista += (lista.empty() ? "" : " | ") + o.texto + (o.activa ? "" : " (x)");
		anotar("menu de '%s': %s", tituloMenu.c_str(), lista.c_str());
		cambiarEstado(MENU);
	}

	void menu(uint32_t p)
	{
		const int n = (int)opciones.size();
		if (enPregunta && duelo && (p & B_CIRCULO)) {   // rama classic-efectos: O = no activar (o atras al armar)
			if (armando) cancelarArmado();
			else if (!eleccionDuelo || duelo->ef.preg.opcional) ejecutar(AC_PASAR);
			return;
		}
		if (armando && (p & B_CIRCULO)) {
			cancelarArmado();
			return;
		}
		if (n == 0 || (p & B_CIRCULO)) {
			cambiarEstado(NAVEGAR);
			return;
		}
		if (p & (B_ARRIBA | B_IZQ)) opcion = (opcion + n - 1) % n;
		if (p & (B_ABAJO | B_DER)) opcion = (opcion + 1) % n;
		if (p & B_DIRECCIONES) anotar("opcion -> %s", opciones[opcion].texto.c_str());
		if (p & B_TRIANGULO) {
			abrirDetalle(foco);
			return;
		}
		if (p & B_X) {
			const Opcion o = opciones[opcion];
			if (!o.activa) {
				avisar(o.motivo.empty() ? "No se puede" : o.motivo);
				return;
			}
			anotar("elige '%s'", o.texto.c_str());
			ejecutar(o.accion);
		}
	}

	// zonas libres del jugador de un tipo
	std::vector<int> libres(const char* tipo) const
	{
		std::vector<int> r;
		for (int z = 0; z < NUM_ZONAS_MESA; z++) {
			if (esTipo(z, tipo) && !ZONAS_MESA[z].rival && zonas[z].vacia()) r.push_back(z);
		}
		return r;
	}
	// pone el foco en la zona valida mas cercana (en x) a "x"
	void enfocarValida(float x)
	{
		int mejor = validas.empty() ? -1 : validas[0];
		for (int z : validas) {
			if (fabsf(ZONAS_MESA[z].x - x) < fabsf(ZONAS_MESA[mejor].x - x)) mejor = z;
		}
		if (mejor >= 0) foco = Foco{ F_ZONA, mejor };
	}

	void ejecutar(Accion a)
	{
		pendiente = a;
		if (duelo) {
			ejecutarDuelo(a);
			return;
		}
		switch (a) {
		case AC_CANCELAR:
		case AC_JUGADA:
		case AC_ACTIVAR_EFECTO:
		case AC_PASAR:
		case AC_ELEGIR_LISTA:
		case AC_TOON: cambiarEstado(NAVEGAR); break;   // (solo en el duelo real)
		case AC_INVOCAR:
		case AC_COLOCAR_MONSTRUO:
		case AC_ACTIVAR_MAGIA:
		case AC_COLOCAR_MT: {
			validas = libres(a == AC_INVOCAR || a == AC_COLOCAR_MONSTRUO ? "monstruo" : "magia");
			if (validas.empty()) {
				avisar("No hay zonas libres");
				cambiarEstado(NAVEGAR);
				break;
			}
			enfocarValida(0.0f);
			cambiarEstado(ELEGIR_ZONA);
			break;
		}
		case AC_ACTIVAR_CAMPO_MANO: lanzar(origenMano, zonaPorId("jugador_campo")); break;   // (solo con la zona libre)
		case AC_ATACAR: {
			validas.clear();
			for (int z = 0; z < NUM_ZONAS_MESA; z++) {
				const Celda& c = zonas[z];
				if (esTipo(z, "monstruo") && ZONAS_MESA[z].rival && !c.vacia()) validas.push_back(z);
			}
			if (validas.empty()) {
				avisar("El rival no tiene monstruos a los que atacar");
				cambiarEstado(NAVEGAR);
				break;
			}
			enfocarValida(ZONAS_MESA[origenZona].x);
			cambiarEstado(ELEGIR_OBJETIVO);
			break;
		}
		case AC_POSICION: {
			Celda& c = zonas[origenZona];
			c.antesDefensa = c.defensa;
			c.defensa = !c.defensa;
			c.posicionTocada = true;
			c.tGiro = 0.0f;
			emitir({ EV_POSICION, origenZona, -1, c.defensa ? 1 : 0, c.carta });
			cambiarEstado(NAVEGAR);
			break;
		}
		case AC_VOLTEAR_MONSTRUO: {
			// invocacion por volteo: boca arriba y en ataque
			Celda& c = zonas[origenZona];
			c.antesBocaAbajo = true;
			c.bocaAbajo = false;
			c.antesDefensa = c.defensa;
			c.defensa = false;
			c.posicionTocada = true;
			c.tVolteo = c.tGiro = 0.0f;
			emitir({ EV_VOLTEAR_MONSTRUO, origenZona, -1, 0, c.carta });
			cambiarEstado(NAVEGAR);
			break;
		}
		case AC_ACTIVAR_SET: {
			Celda& c = zonas[origenZona];
			c.antesBocaAbajo = true;
			c.bocaAbajo = false;
			c.tVolteo = 0.0f;
			c.tActiva = 0.0f;
			emitir({ EV_ACTIVAR_MAGIA, origenZona, -1, 0, c.carta });
			cambiarEstado(NAVEGAR);
			break;
		}
		case AC_ACTIVAR_CAMPO: {
			zonas[origenZona].bocaAbajo = false;
			zonas[origenZona].tActiva = 0.0f;
			emitir({ EV_ACTIVAR_CAMPO, origenZona, -1, 0, zonas[origenZona].carta });
			cambiarEstado(NAVEGAR);
			break;
		}
		case AC_DESTRUIR_CAMPO: {
			cementerioJ.push_back(zonas[origenZona].carta);
			emitir({ EV_DESTRUIR_CAMPO, origenZona, -1, 0, zonas[origenZona].carta });
			zonas[origenZona] = Celda{};
			cambiarEstado(NAVEGAR);
			break;
		}
		case AC_CEMENTERIO: {
			Celda& c = zonas[origenZona];
			if (c.carta != "?") cementerioJ.push_back(c.carta);
			if (!c.bocaAbajo) emitir({ EV_AL_CEMENTERIO, origenZona, -1, 0, c.carta });
			anotar("'%s' al cementerio (%d cartas)", c.carta.c_str(), (int)cementerioJ.size());
			c = Celda{};
			cambiarEstado(NAVEGAR);
			break;
		}
		}
	}

	// eligiendo zona u objetivo con la cruceta
	void elegir(uint32_t p)
	{
		auto vale = [this](const Foco& f) { return f.tipo == F_ZONA && std::find(validas.begin(), validas.end(), f.i) != validas.end(); };
		moverConPulsos(p, vale);
		if (p & B_CIRCULO) {
			// atras: al menu de la carta
			foco = origenMano >= 0 ? Foco{ F_MANO_J, origenMano } : Foco{ F_ZONA, origenZona };
			cambiarEstado(MENU);
			return;
		}
		if (p & B_TRIANGULO) {
			abrirDetalle(foco);
			return;
		}
		if (!(p & B_X) || !vale(foco)) return;
		if (estado == ELEGIR_TRIBUTOS) {
			auto it = std::find(tributos.begin(), tributos.end(), foco.i);
			if (it != tributos.end()) {
				tributos.erase(it);
			} else {
				tributos.push_back(foco.i);
			}
			if ((int)tributos.size() >= tributosFaltan) {
				// los sacrificios elegidos: ahora la zona (libres + las de los sacrificios)
				validas = libres("monstruo");
				for (int z : tributos) validas.push_back(z);
				enfocarValida(ZONAS_MESA[tributos[0]].x);
				cambiarEstado(ELEGIR_ZONA);
			}
			return;
		}
		if (estado == ELEGIR_OBJETIVO && duelo) {
			ia::Jugada j;
			j.tipo = ia::J_ATACAR;
			j.zona = indiceIA(origenZona);
			j.objetivo = indiceIA(foco.i);
			if (!jugar || !jugar(j)) avisar("No se puede");
			foco = Foco{ F_ZONA, origenZona };
			cambiarEstado(NAVEGAR);
			return;
		}
		if (estado == ELEGIR_OBJETIVO) {
			if (ataqueEnCurso && ataqueEnCurso()) {
				avisar("Espera a que termine el ataque");
				return;
			}
			const Celda& a = zonas[origenZona];
			const Celda& o = zonas[foco.i];
			anotar("ataque: '%s' (%s) -> '%s' (%s)", a.carta.c_str(), ZONAS_MESA[origenZona].id, o.carta.c_str(), ZONAS_MESA[foco.i].id);
			emitir({ EV_ATACAR, origenZona, foco.i, 0, a.carta });
			foco = Foco{ F_ZONA, origenZona };
			cambiarEstado(NAVEGAR);
			return;
		}
		lanzar(origenMano, foco.i);
	}

	// la carta k de la mano sale volando a la zona z (al llegar se coloca: aterrizar)
	void lanzar(int k, int z)
	{
		if (k < 0 || k >= (int)manoJ.size()) {
			cambiarEstado(NAVEGAR);
			return;
		}
		if (duelo) {
			// la jugada se aplica antes del vuelo (si las reglas no la aceptan, no vuela); los sacrificios se van ya
			ia::Jugada j;
			j.tipo = pendiente == AC_INVOCAR ? ia::J_INVOCAR : pendiente == AC_COLOCAR_MONSTRUO ? ia::J_COLOCAR : pendiente == AC_TOON ? ia::J_TOON
			         : pendiente == AC_ACTIVAR_MAGIA ? ia::J_ACTIVAR_MT : ia::J_COLOCAR_MT;
			j.mano = k;
			j.zona = indiceIA(z);
			for (int t : tributos) j.tributos.push_back(indiceIA(t));
			if (!jugar || !jugar(j)) {
				avisar("No se puede");
				tributos.clear();
				cambiarEstado(NAVEGAR);
				return;
			}
			for (int t : tributos) {
				Celda& c = zonas[t];
				if (!c.bocaAbajo) emitir({ EV_AL_CEMENTERIO, t, -1, 0, c.carta });
				sacrificados.push_back({ t, c });   // (rama classic-arreglo-ciclo-md: el juego los hace viajar al cementerio)
				c = Celda{};
			}
			tributos.clear();
			// donde quedo de verdad (con sacrificios el duelo pudo elegir otra zona)
			if (j.tipo == ia::J_COLOCAR_MT || j.tipo == ia::J_ACTIVAR_MT) {
				for (int i = 0; i < ia::ZONAS; i++) {
					const int zz = zonaPorId("jugador_magia_" + std::to_string(i));
					if (zonas[zz].vacia() && !duelo->l[0].mt[(size_t)i].vacia()) z = zz;
				}
				const int zc = zonaPorId("jugador_campo");   // (rama classic-efectos: la magia de campo va a su zona)
				if (zc >= 0 && !duelo->l[0].campo.vacia() && zonas[zc].carta != duelo->l[0].campo.id) z = zc;
			} else {
				for (int i = 0; i < ia::ZONAS; i++) {
					const int zz = zonaPorId("jugador_monstruo_" + std::to_string(i));
					if (zonas[zz].vacia() && !duelo->l[0].m[(size_t)i].vacia()) z = zz;
				}
			}
		}
		vuelo.activo = true;
		vuelo.carta = manoJ[k];
		vuelo.desdeMano = k;
		vuelo.nMano = (int)manoJ.size();
		vuelo.zona = z;
		vuelo.t = 0.0f;
		vuelo.accion = pendiente;
		vuelo.bocaAbajo = pendiente == AC_COLOCAR_MONSTRUO || pendiente == AC_COLOCAR_MT;
		vuelo.girada = pendiente == AC_COLOCAR_MONSTRUO;
		vuelo.serie++;
		manoJ.erase(manoJ.begin() + k);
		anotar("vuela '%s' de la mano (%d) a %s%s", vuelo.carta.c_str(), k, ZONAS_MESA[z].id, vuelo.bocaAbajo ? " boca abajo" : "");
		foco = Foco{ F_ZONA, z };
		cambiarEstado(VOLANDO);
	}

	void aterrizar()
	{
		vuelo.activo = false;
		Celda& c = zonas[vuelo.zona];
		c = Celda{};
		c.carta = vuelo.carta;
		c.bocaAbajo = c.antesBocaAbajo = vuelo.bocaAbajo;
		c.defensa = c.antesDefensa = vuelo.girada;
		c.posicionTocada = true;
		c.tLlega = 0.0f;
		anotar("llega '%s' a %s (%s%s)", c.carta.c_str(), ZONAS_MESA[vuelo.zona].id, c.bocaAbajo ? "boca abajo" : "boca arriba",
		       c.defensa ? ", defensa" : "");
		switch (vuelo.accion) {
		case AC_TOON:
		case AC_INVOCAR:
			emitir({ EV_INVOCAR, vuelo.zona, -1, 0, c.carta });
			break;
		case AC_ACTIVAR_MAGIA:
			c.tActiva = 0.0f;
			emitir({ EV_ACTIVAR_MAGIA, vuelo.zona, -1, 0, c.carta });
			break;
		case AC_ACTIVAR_CAMPO_MANO:
			c.tActiva = 0.0f;
			emitir({ EV_ACTIVAR_CAMPO, vuelo.zona, -1, 0, c.carta });
			break;
		default: break;
		}
		cambiarEstado(NAVEGAR);
	}
};

}   // namespace control
