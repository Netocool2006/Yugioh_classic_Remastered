// Prueba de la ruta REAL del juego (la de la PS5), sin pantalla: incluye duelo_juego.h tal cual (el mismo codigo
// que va dentro de la clase del juego en yugireload.cpp) con lo de Vulkan/sonido/interfaz como maquetas, y cada
// cuadro hace lo mismo que render(): avanzarAtaque -> avanzarMando (botones -> pausa / pantallas / control.h) ->
// avanzarDueloIA. Diferencia con prueba_duelo.cpp: aqui NADIE toca ctl.bloqueado a mano; lo pone avanzarDueloIA
// como en la consola. Recorrido: arranque -> modo historia -> elegir rival con X -> duelo -> el jugador pasa por
// todas las fases con CUADRADO (y descarta si hace falta) -> turno del rival (la IA) -> vuelta al jugador, varias
// veces. Tambien prueba el menu de pausa (OPTIONS corto): Continuar, Abandonar duelo y Salir al menu principal.
// Compilar y correr (contenedor arch-ps5):
//   g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_juego.cpp -o /tmp/prueba_juego &&
//   /tmp/prueba_juego <carpeta ui/ con cartas_ui.txt, rivales.txt y personajes.txt> [duelos]
#include "control.h"
#include "md_ritmo.h"   // rama classic-md-ritmo
#include "entrada_md.h"   // rama classic-md-entradas: md_ritmo_juego.h espera a que termine la entrada de fusion/ritual
#include "duelo_visores.h"   // rama classic-duelo-ui
#include "mazo.h"
#include "personajes.h"
#include "historia_dialogos.h"
#include "tutorial.h"
#include <chrono>
#include <atomic>
#include <set>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

// ---- maquetas de lo que el juego tiene en Vulkan / audio / entornos
namespace efc {
inline float duracion(int) { return 1.0f; }
}
namespace entornos {
inline int porTema(const std::string&) { return 0; }
inline int claveDe(const std::string&) { return 0; }
static const char* const CLAVES[] = { "neutro" };
}
namespace campos {
inline bool esCampo(const std::string&) { return false; }
}

// botones del mando de la consola (platform.h): los mismos bits
enum : uint32_t {
	PAD_OPTIONS = 0x000008, PAD_UP = 0x000010, PAD_RIGHT = 0x000020, PAD_DOWN = 0x000040, PAD_LEFT = 0x000080,
	PAD_L1 = 0x000400, PAD_R1 = 0x000800, PAD_TRIANGLE = 0x001000, PAD_CIRCLE = 0x002000, PAD_CROSS = 0x004000,
	PAD_SQUARE = 0x008000, PAD_R3 = 0x000004, PAD_TOUCH_PAD = 0x100000, PAD_L3 = 0x000002,
};
struct EstadoMando { uint32_t pressed{ 0 }, held{ 0 }; float left_x{ 0 }, left_y{ 0 }, right_x{ 0 }, right_y{ 0 }, l2{ 0 }, r2{ 0 }; };

static int fallos = 0;
#define FALLO(...) do { fprintf(stderr, "FALLO: " __VA_ARGS__); fprintf(stderr, "\n"); fallos++; } while (0)

struct Juego {
	std::string carpetaUi;
	std::string getAssetPath() const { return carpetaUi + "../"; }
	// sonido
	struct Sonido {
		void tocarMusica(const char*, float) {}
		void sfx(const char*, float = 1.0f, float = 0.0f, float = 0.0f) {}
		bool hay(const char*) const { return false; }
		void pararMusica(float) {}
		std::atomic<float> volMusica{ 1.0f }, volEfectos{ 1.0f };   // (menu_principal.h: Opciones)
	} sonido;
	// interfaz
	struct Interfaz {
		std::function<bool(const char*, int&, int&)> statsZona;
		int lps[2]{ 8000, 8000 };
		std::string nombreCarta(const std::string& id) const { return id; }
		void cambiarLP(bool rival, int v) { lps[rival ? 1 : 0] = v; }
		void fijarLP(bool rival, int v) { lps[rival ? 1 : 0] = v; }   // (rama classic-visual-final)
		int lp(bool rival) const { return lps[rival ? 1 : 0]; }
		int mazos[3]{ -1, -1, -1 };   // (rama classic-duelo-ui: los contadores de la mesa)
		void contarMazos(int j, int r, int f) { mazos[0] = j, mazos[1] = r, mazos[2] = f; }
		void conectarConsulta(const visores::Consulta*) {}
	} interfazDuelo;
	struct EfectosCartas { int receta(const std::string&) const { return 0; } } efectosCartas;
	// (como entornos.h: un tema por numero o, desde classic-fondos, la clave de tema / el id del personaje)
	struct EntornosRival {
		int objetivo{ 0 };
		std::string clave;
		void pedir(int e) { objetivo = e, clave.clear(); }
		void pedir(const std::string& c) { clave = c; }
		void soltarCampo() {}
	} entornosRival;
	bool entornoFijo{ false };
	std::string entornoVista;
	void entornoDelRival(const ia::Rival*) {}
	void siguienteEntorno() {}
	// mesa / ataque (yugireload.cpp)
	enum { CAMPO_VACIO = 0, CAMPO_COLOCANDO, CAMPO_BOCA_ABAJO, CAMPO_ACTIVANDO, CAMPO_ACTIVO, CAMPO_DESTRUYENDO };
	int estadoCampo{ CAMPO_VACIO };
	bool ataquesSolos{ true }, campoAuto{ true };
	float tInvocada[control::NUM_ZONAS_MESA]{};
	int zonaAtacante{ -1 }, zonaObjetivo{ -1 }, danio{ 0 }, ataquesVistos{ 0 };
	bool danioAlRival{ false }, muereA{ false }, muereO{ false }, ataqueDemo{ false };
	float tAtaque{ -1.0f };
	static constexpr float T_IMPACTO = ritmo::T_ATAQUE_BAJO, DURACION_ATAQUE = ritmo::T_FIN_ATAQUE;   // (rama classic-ps5-ataque3s: el ataque de Joey, ~3 s; antes 0.55 / 1.6)
	bool hayAtaque() const { return tAtaque >= 0.0f && zonaAtacante >= 0 && zonaObjetivo >= 0; }
	void activarEfectoCarta(const std::string&, int) {}
	void activarCampo(const std::string&, int = 0) {}
	void destruirCampo(const std::string&, int = 0) {}
	void romperPorEfectoMD(const ia::Suceso&, int) {}   // (rama classic-ps5-efectos3, efectos3_juego.h: maquetas, sin Vulkan)
	bool campoActivoMD() const { return false; }
	// mazo (mazo_duelo.h: lo que usa el duelo)
	mazo::Coleccion coleccion;
	mazo::Mazo mazoJugador, mazoDuelo;
	std::string rutaMazoJ;
	bool pedidoConstructor{ false };
	struct { bool abierto{ false }; } constructorMazo;
	void repartir()
	{
		std::string motivo;
		mazoDuelo = mazoJugador.valido(coleccion, &motivo) ? mazoJugador : mazo::Mazo::porDefecto(coleccion);
	}
	// ajustes (la ventana ImGui de la plantilla)
	struct { bool visible{ false }; } ui;
	struct { bool overlay{ true }; } settings;
	bool quit{ false };
	bool ajustesAbiertos() const { return settings.overlay && ui.visible; }
	// el mando
	control::Mando ctl;
	control::Repetidor repetidor;
	EstadoMando mando;
	float frameTimer{ 1.0f / 60.0f };
	bool mandoListo{ false };

#include "ataque_elevacion_juego.h"   // rama classic-ps5-ataque5 (sin glm)
	void prepararAtaqueDirecto(int, bool) {}   // rama classic-ps5-ataque (ataque_md_juego.h: solo presenta; aqui sin glm)
#include "duelo_juego.h"
#include "md_ritmo_juego.h"   // rama classic-md-ritmo: el ritmo de Master Duel (viajes de cartas, activaciones, contadores)
#include "maquetas_juego.h"   // (resultado, velocidad, entrada de las cartas clave, menu y portada: maquetas)
#include "duelo_consulta.h"   // rama classic-duelo-ui

	void preparar(const std::string& ui_)
	{
		carpetaUi = ui_;
		rutaMazoJ = "/tmp/prueba_juego_mazo.txt";
		remove("/tmp/prueba_juego_progreso.txt");
		remove("/tmp/historia.txt");
		coleccion.leer(carpetaUi);
		mazoJugador = mazo::Mazo::porDefecto(coleccion);
		ctl.clase = [this](const std::string& c) {
			const ia::Carta* k = baseIA.carta(c);
			return k ? k->clase : std::string();
		};
		ctl.ataqueEnCurso = [this] { return hayAtaque(); };
		ctl.campoOcupado = [] { return false; };
		ctl.preparar({}, {}, {});
		ctl.log = getenv("PRUEBA_LOG") != nullptr;
		baseIA.leer(carpetaUi);
		prepararDuelo();
		mandoListo = true;
	}

	// ---- copia de yugireload.cpp (botonesDe, avanzarAtaque sin Vulkan, avanzarMando, procesarEventos)
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
		if (pad & PAD_L3) r |= control::B_L3;
		return r;
	}
	void avanzarAtaque()
	{
		for (float& t : tInvocada) t += frameTimer;
		if (hayAtaque()) {
			tAtaque += frameTimer;
			if (tAtaque > DURACION_ATAQUE) {
				if (!ataqueDemo) {
					if (muereO) ctl.alCementerio(zonaObjetivo);
					if (muereA) ctl.alCementerio(zonaAtacante);
				}
				tAtaque = -1.0f;
				zonaAtacante = zonaObjetivo = -1;
			}
		}
	}
	void atacar(int a, int o, bool demo)
	{
		zonaAtacante = a;
		zonaObjetivo = o;
		tAtaque = 0.0f;
		ataqueDemo = demo;
	}
	void ejecutarEvento(const control::Evento& e)
	{
		if (e.tipo == control::EV_ATACAR) atacar(e.zona, e.zona2, false);
		if (e.tipo == control::EV_DEMO_APAGADA) ataquesSolos = campoAuto = false;
	}
	void procesarEventos()
	{
		ctl.eventos.clear();
	}
	// (el mismo orden que yugireload.cpp: pulsos -> pausa / Ajustes -> pantallas -> control.h)
	void avanzarMando()
	{
		const float dt = frameTimer;
		const uint32_t recien = botonesDe(mando.pressed);
		const uint32_t pulsos = repetidor.paso(botonesDe(mando.held), recien, 0.0f, 0.0f, dt) | (recien & ~control::B_DIRECCIONES);
		if (ctl.log) registrarBotones(recien);
		const bool circulo = (pulsos & control::B_CIRCULO) != 0;   // (rama classic-bp-terminar: como yugireload.cpp)
		if (avanzarPausa(pulsos)) {
			if (circulo) ctl.circuloAjeno();
			procesarEventos();
			return;
		}
		if (ajustesAbiertos()) {
			if (circulo) ctl.circuloAjeno();
			return;
		}
		const bool toco = (mando.pressed & ~(uint32_t)(PAD_OPTIONS | PAD_TOUCH_PAD)) != 0;
		if (toco) ctl.tocar();
		if (avanzarPantallaDuelo(pulsos)) {
			if (circulo) ctl.circuloAjeno();
			procesarEventos();
			return;
		}
		if (mandoConsulta(pulsos, dt)) {   // (rama classic-duelo-ui)
			if (circulo) ctl.circuloAjeno();
			procesarEventos();
			return;
		}
		ctl.avanzar(dt, pulsos);
		for (const control::Evento& e : ctl.eventos) ejecutarEvento(e);
		procesarEventos();
	}

	// un cuadro como render(): pad = lo apretado en este cuadro (se mantiene "held" mientras dure)
	void cuadro(uint32_t pad = 0)
	{
		mando.held = pad;
		mando.pressed = pad & ~padAntes;
		padAntes = pad;
		avanzarAtaque();
		avanzarMando();
		if (mandoListo && !ajustesAbiertos() && !constructorMazo.abierto && !pausa.abierta) {
			avanzarDueloIA(frameTimer);
			avanzarConsulta(frameTimer);   // (rama classic-duelo-ui)
		}
		revisarMesa();
	}
	// la mesa (lo que se dibuja) igual al duelo cuando no hay nada pendiente (fallo 1: la carta que desaparece)
	int fallosMesa{ 0 };
	void revisarMesa()
	{
		if (ctl.vista.pantalla != control::VistaDuelo::DUELO || ctl.vista.sorteo || reparto > 0 || ctl.robo.activo || hayAtaque() || ctl.vuelo.activo || mesaPendiente || sucesosVistos < dueloIA.sucesos.size() || esperaEfecto > 0.0f) return;
		for (int lado = 0; lado < 2; lado++) {
			for (int i = 0; i < ia::ZONAS; i++) {
				for (int mg = 0; mg < 2; mg++) {
					const int z = zonaMesaIA(lado, i, mg != 0);
					const std::string id = mg ? dueloIA.l[(size_t)lado].mt[(size_t)i].id : dueloIA.l[(size_t)lado].m[(size_t)i].id;
					if (ctl.zonas[(size_t)z].carta != id && fallosMesa++ < 5)
						fprintf(stderr, "FALLO: mesa distinta del duelo en %s: mesa '%s', duelo '%s' (turno %d)\n", control::ZONAS_MESA[z].id, ctl.zonas[(size_t)z].carta.c_str(), id.c_str(), dueloIA.turno);
				}
			}
		}
		// rama classic-duelo-ui: los contadores de la mesa (mazo, extra, cementerio, desterradas) y las listas de los
		// visores son los de ia::Duelo
		revisionesContadores++;
		for (int lado = 0; lado < 2; lado++) {
			const ia::Lado& x = dueloIA.l[(size_t)lado];
			const char* quien = lado ? "rival" : "jugador";
			auto mal = [&](const char* que, int mesa, int duelo) {
				if (mesa != duelo && fallosContadores++ < 6)
					fprintf(stderr, "FALLO: contador de %s del %s: mesa %d, duelo %d (turno %d)\n", que, quien, mesa, duelo, dueloIA.turno);
			};
			mal("cementerio", (int)(lado ? ctl.cementerioR : ctl.cementerioJ).size(), (int)x.cementerio.size());
			mal("desterradas", (int)(lado ? ctl.desterradosR : ctl.desterradosJ).size(), (int)x.desterrados.size());
			mal("extra", (int)(lado ? ctl.extraR : ctl.extraJ).size(), (int)x.extra.size());
			mal("mazo", interfazDuelo.mazos[lado], (int)x.mazo.size());
			mal("cementerio (visor)", (int)visores::lista(ctl, visores::P_CEMENTERIO, lado).size(), (int)x.cementerio.size());
			mal("desterradas (visor)", (int)visores::lista(ctl, visores::P_DESTERRADAS, lado).size(), (int)x.desterrados.size());
			mal("extra (visor)", (int)visores::lista(ctl, visores::P_EXTRA, lado).size(), (int)x.extra.size());
			if ((lado ? ctl.cementerioR : ctl.cementerioJ) != x.cementerio && fallosContadores++ < 6) fprintf(stderr, "FALLO: el cementerio del %s no es el del duelo\n", quien);
			if ((lado ? ctl.desterradosR : ctl.desterradosJ) != x.desterrados && fallosContadores++ < 6) fprintf(stderr, "FALLO: las desterradas del %s no son las del duelo\n", quien);
		}
		if (interfazDuelo.mazos[2] != (int)dueloIA.l[0].extra.size() && fallosContadores++ < 6)
			fprintf(stderr, "FALLO: contador de extra del jugador: %d, duelo %d\n", interfazDuelo.mazos[2], (int)dueloIA.l[0].extra.size());
		// el registro va al dia: su ultimo turno es el del duelo
		if (!consulta.reg.lineas.empty() && consulta.reg.turno != dueloIA.turno && fallosContadores++ < 6)
			fprintf(stderr, "FALLO: el registro va por el turno %d y el duelo por el %d\n", consulta.reg.turno, dueloIA.turno);
	}
	int fallosContadores{ 0 }, revisionesContadores{ 0 };
	uint32_t padAntes{ 0 };
	// apretar y soltar un boton (como el usuario: ~0.1 s abajo) y esperar "espera" s
	void apretar(uint32_t pad, float espera = 0.3f)
	{
		for (int k = 0; k < (int)(0.1f / frameTimer); k++) cuadro(pad);
		esperar(espera);
	}
	void esperar(float s)
	{
		for (int k = 0; k < (int)(s / frameTimer); k++) cuadro(0);
	}
};

static Juego juego;

static const char* nombreFase(ia::Fase f)
{
	static const char* const N[] = { "ROBO", "PRINCIPAL 1", "BATALLA", "PRINCIPAL 2", "FINAL" };
	return N[(int)f];
}

// una jugada con el mando: la primera carta de la mano con algo que hacer (o, en la batalla, el primer monstruo que
// pueda atacar): X, la opcion, X, y X en cada eleccion (zona, sacrificio, objetivo, carta); luego espera la animacion
// rama classic-md-fases: pasar a la fase siguiente con el selector de fases (OPTIONS, cursor, X), como un jugador
static void siguienteFaseMD(Juego& g)
{
	const ia::Fase f0 = g.dueloIA.fase;
	const int t0 = g.dueloIA.turno;
	for (int r = 0; r < 20 && !g.ctl.selFases.abierto && g.dueloIA.fase == f0 && g.dueloIA.turno == t0 && g.dueloIA.activo == 0; r++) {
		g.apretar(PAD_OPTIONS, 0.1f);
		if (!g.ctl.selFases.abierto) g.esperar(0.4f);
	}
	if (!g.ctl.selFases.abierto) return;
	int k = 0;
	while (k < control::Mando::SEL_FASES - 1 && !g.ctl.selFases.valida[(size_t)k]) k++;
	for (int i = 0; i < 4 && g.ctl.selFases.opcion != k; i++) g.apretar(g.ctl.selFases.opcion < k ? PAD_RIGHT : PAD_LEFT, 0.1f);
	g.apretar(PAD_CROSS, 1.0f);
}
static int jugadasHechas = 0;
static void jugarAlgo(Juego& g, bool atacar)
{
	using namespace control;
	std::vector<Foco> focos;
	if (atacar) {
		for (int i = 0; i < ia::ZONAS; i++) focos.push_back({ F_ZONA, zonaPorId("jugador_monstruo_" + std::to_string(i)) });
	} else {
		for (int k = 0; k < (int)g.ctl.manoJ.size(); k++) focos.push_back({ F_MANO_J, k });
	}
	for (const Foco& f : focos) {
		if (g.ctl.bloqueado || g.ctl.estado != NAVEGAR) return;
		g.ctl.foco = f;
		g.apretar(PAD_CROSS, 0.2f);
		if (g.ctl.estado != MENU) {
			if (g.ctl.estado == DETALLE) g.apretar(PAD_CIRCLE, 0.2f);
			continue;
		}
		int elegida = -1;
		// (fallo 1: "coloque un monstruo y desaparecio") primero colocar un monstruo boca abajo, si se puede
		for (int k = 0; k < (int)g.ctl.opciones.size() && elegida < 0 && !atacar; k++)
			if (g.ctl.opciones[(size_t)k].activa && g.ctl.opciones[(size_t)k].accion == AC_COLOCAR_MONSTRUO) elegida = k;
		for (int k = 0; k < (int)g.ctl.opciones.size() && elegida < 0; k++) {
			const Opcion& o = g.ctl.opciones[(size_t)k];
			if (!o.activa) continue;
			if (atacar ? o.accion == AC_ATACAR : (o.accion == AC_INVOCAR || o.accion == AC_COLOCAR_MONSTRUO || o.accion == AC_ACTIVAR_EFECTO || o.accion == AC_COLOCAR_MT))
				elegida = k;
		}
		if (elegida < 0) {
			g.apretar(PAD_CIRCLE, 0.2f);
			continue;
		}
		g.ctl.opcion = elegida;
		g.apretar(PAD_CROSS, 0.3f);
		for (int k = 0; k < 8 && g.ctl.estado != NAVEGAR; k++) {
			if (g.ctl.estado == MENU) g.ctl.opcion = 0;
			g.apretar(PAD_CROSS, 0.4f);
		}
		jugadasHechas++;
		g.esperar(2.5f);
		return;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "uso: prueba_juego <carpeta ui/> [duelos]\n");
		return 2;
	}
	const int N = argc > 2 ? atoi(argv[2]) : 3;
	Juego& g = juego;
	// rama classic-ps5-duelo-log: la bitacora del duelo (duelo_log.h) va a una carpeta de prueba
	const std::string carpetaLog = "/tmp/prueba_juego_log";
	mkdir(carpetaLog.c_str(), 0777);
	setenv("YUGI_DUELO_LOG_DIR", carpetaLog.c_str(), 1);
	remove((carpetaLog + "/duelo_log.txt").c_str());
	if (const char* f = getenv("PRUEBA_HZ")) g.frameTimer = 1.0f / (float)atof(f);   // (la consola: 120 Hz)
	g.preparar(std::string(argv[1]) + "/");
	using VD = control::VistaDuelo;
	if (!g.baseIAlista) {
		fprintf(stderr, "sin datos en %s\n", argv[1]);
		return 2;
	}
	int turnosVistos = 0, fasesJugador = 0, jugadasTurno = 0, ataquesTurno = 0;
	for (int n = 0; n < N; n++) {
		// arranque: pantalla de elegir rival (modo historia si hay personajes)
		g.esperar(1.0f);
		if (g.ctl.vista.pantalla == VD::RESULTADO) g.apretar(PAD_CROSS, 0.5f);   // (rama classic-resultado: la pantalla del final)
		if (g.ctl.vista.pantalla != VD::ELEGIR_RIVAL) FALLO("no arranca en elegir rival (pantalla %d)", g.ctl.vista.pantalla);
		if (n == 0 && g.ctl.vista.modo != VD::HISTORIA) FALLO("no arranca en modo historia");
		if (const char* r = getenv("PRUEBA_RIVAL")) {   // (otro personaje de la saga: con la cruceta, como el usuario)
			for (int k = 0; k < 12 && g.ctl.vista.personaje != r; k++) g.apretar(PAD_RIGHT, 0.2f);
		}
		g.apretar(PAD_CROSS, 1.0f);   // elegir el rival (el primero sin vencer)
		// rama classic-historia: la escena del rival (dialogo) sale antes del duelo; circulo la salta (el tiempo de cada
		// linea lo lleva el dibujo, que aca no hay) y empieza el duelo
		if (historia_dialogos::escena().activa()) g.apretar(PAD_CIRCLE, 1.0f);
		if (historia_dialogos::escena().activa()) FALLO("el dialogo del rival no se salta con circulo");
		if (g.ctl.vista.pantalla != VD::DUELO) {
			FALLO("X no empieza el duelo (pantalla %d, modo %d, saga %d idx %d abierta %d, aviso %s)", g.ctl.vista.pantalla, g.ctl.vista.modo, g.ctl.vista.saga, g.ctl.vista.idx, (int)g.ctl.vista.sagaAbierta, g.ctl.aviso.c_str());
			break;
		}
		// rama classic-moneda: el lanzamiento de moneda (X adelanta; si gana el jugador, derecha/X = ir primero o segundo)
		if (!g.ctl.vista.sorteo) FALLO("el duelo no empieza con el lanzamiento de moneda");
		const int serie0 = g.ctl.robo.serie;
		for (int k = 0; k < 30 && g.ctl.vista.sorteo; k++) g.apretar(n % 2 ? PAD_RIGHT : 0, 0.1f), g.apretar(PAD_CROSS, 0.5f);
		if (g.ctl.vista.sorteo) FALLO("el lanzamiento de moneda no termina");
		if (g.ctl.vista.moneda.empieza != g.dueloIA.activo) FALLO("empieza %d y la moneda dijo %d", g.dueloIA.activo, g.ctl.vista.moneda.empieza);
		// el reparto: las manos se llenan carta por carta (robo animado)
		{
			int robos = g.ctl.robo.serie - serie0, serie = g.ctl.robo.serie;
			for (int k = 0; k < (int)(14.0f / g.frameTimer) && (g.reparto > 0 || g.ctl.robo.activo || g.sucesosVistos < g.dueloIA.sucesos.size()); k++) {   // (14 s: rama classic-md-fases, los carteles de inicio y de turno de MD: 2.4 s + 2.2 s)
				g.cuadro(0);
				if (g.ctl.robo.serie != serie) robos++, serie = g.ctl.robo.serie;
			}
			if (robos < 10) FALLO("el reparto inicial anima %d cartas (se esperaban 10 o 11)", robos);
			if (g.ctl.manoJ != g.dueloIA.l[0].mano || g.ctl.manoR != (int)g.dueloIA.l[1].mano.size())
				FALLO("tras el reparto la mano en pantalla (%d / %d) no es la del duelo (%d / %d)", (int)g.ctl.manoJ.size(), g.ctl.manoR, (int)g.dueloIA.l[0].mano.size(),
				      (int)g.dueloIA.l[1].mano.size());
			printf("reparto: %d cartas animadas, empieza %s\n", robos, g.dueloIA.activo == 0 ? "el jugador" : "el rival");
		}
		g.esperar(2.0f);   // el cartel de inicio y los sucesos del reparto
		const int turnoInicial = g.dueloIA.turno;
		int vueltas = 0;
		// el jugador pasa por todas sus fases con CUADRADO; el rival juega solo; 4 turnos completos
		while (g.dueloIA.ganador < 0 && g.dueloIA.turno < turnoInicial + 4 && vueltas++ < 400) {
			if (g.dueloIA.activo == 0 && g.dueloIA.decide() == 0) {
				const ia::Fase antes = g.dueloIA.fase;
				const int turnoAntes = g.dueloIA.turno;
				if (g.ctl.estado == control::ELEGIR_CARTA) {   // (el duelo pide elegir una carta: X sobre la primera)
					g.apretar(PAD_CROSS, 0.6f);
					continue;
				}
				if (g.ctl.estado == control::MENU) {
					// una pregunta del duelo (responder): "No activar"
					g.ctl.opcion = (int)g.ctl.opciones.size() - 1;
					g.apretar(PAD_CROSS, 0.6f);
					continue;
				}
				if (g.dueloIA.fase == ia::F_FINAL && (int)g.dueloIA.l[0].mano.size() > ia::MAX_MANO) {
					// descartar: la primera de la mano (menu de la carta -> "Descartar")
					g.ctl.foco = control::Foco{ control::F_MANO_J, 0 };
					g.apretar(PAD_CROSS, 0.3f);
					g.apretar(PAD_CROSS, 1.0f);
					continue;
				}
				// como un jugador: en la principal 1 juega algo de la mano (invocar, colocar, activar) y en la batalla ataca,
				// todo con el mando (menus, zonas, objetivos); luego CUADRADO
				if (g.dueloIA.fase == ia::F_PRINCIPAL1 && jugadasTurno < 2) {
					jugadasTurno++;
					jugarAlgo(g, false);
					continue;
				}
				if (g.dueloIA.fase == ia::F_BATALLA && ataquesTurno < 2) {
					ataquesTurno++;
					jugarAlgo(g, true);
					continue;
				}
				siguienteFaseMD(g);   // rama classic-md-fases: OPTIONS -> la fase siguiente (antes: CUADRADO)
				// la fase elegida sigue sola cuando el jugador queda libre (el rival responde, animaciones)
				for (int k = 0; k < 30 && (g.ctl.bloqueado || g.ctl.faseObjetivo >= 0) && g.dueloIA.fase == antes && g.dueloIA.activo == 0; k++) g.esperar(0.5f);
				if (g.dueloIA.fase == antes && g.dueloIA.turno == turnoAntes && g.dueloIA.activo == 0 && g.dueloIA.ganador < 0 &&
				    g.dueloIA.decide() == 0 && g.ctl.estado != control::MENU && g.ctl.estado != control::ELEGIR_CARTA) {
					// (una eleccion obligatoria abierta, p. ej. Ante: el cuadrado no la salta; la vuelta siguiente la resuelve con X)
					FALLO("OPTIONS (selector de fases) no cambia de fase: turno %d, fase %s, bloqueado %d, estado %s, aviso '%s'", g.dueloIA.turno, nombreFase(antes),
					      (int)g.ctl.bloqueado, control::NOMBRE_ESTADO[g.ctl.estado], g.ctl.aviso.c_str());
					fprintf(stderr, "  duelo: preg %d (lado %d), cadena %d, resolviendo %d, pendiente %d, decide %d, sucesos %d/%d, esperaEfecto %.2f, ataque %d\n",
					        (int)g.dueloIA.ef.preg.tipo, g.dueloIA.ef.preg.lado, (int)g.dueloIA.ef.cadena.size(), (int)g.dueloIA.ef.resolviendo, (int)g.dueloIA.ef.pendiente,
					        g.dueloIA.decide(), (int)g.sucesosVistos, (int)g.dueloIA.sucesos.size(), g.esperaEfecto, (int)g.hayAtaque());
					for (size_t k = g.dueloIA.sucesos.size() > 12 ? g.dueloIA.sucesos.size() - 12 : 0; k < g.dueloIA.sucesos.size(); k++)
						fprintf(stderr, "  suceso %d lado %d zona %d valor %d carta %s\n", (int)g.dueloIA.sucesos[k].tipo, g.dueloIA.sucesos[k].lado, g.dueloIA.sucesos[k].zona,
						        g.dueloIA.sucesos[k].valor, g.dueloIA.sucesos[k].carta.c_str());
					break;
				}
				fasesJugador++;
				if (g.dueloIA.turno != turnoAntes || g.dueloIA.activo != 0) jugadasTurno = ataquesTurno = 0;
				// (rama classic-bp-terminar: con lo que el rival respondio todavia en pantalla, p. ej. en la ventana del final de
				// la batalla, la fase se muestra al terminar de presentarlo: se mira con el duelo al dia)
				for (int k = 0; k < 40 && g.ctl.bloqueado && g.dueloIA.activo == 0 && g.dueloIA.ganador < 0; k++) g.esperar(0.25f);
				if (g.ctl.fase != Juego::faseInterfaz(g.dueloIA.fase) && g.dueloIA.activo == 0)
					FALLO("la fase en pantalla (%d) no es la del duelo (%s)", g.ctl.fase, nombreFase(g.dueloIA.fase));
			} else {
				// turno del rival (o una pregunta del rival): solo esperar; si el jugador tiene que responder, "No"
				const int t0 = g.dueloIA.turno;
				float s = 0.0f;
				while (g.dueloIA.ganador < 0 && (g.dueloIA.activo == 1 || g.dueloIA.decide() == 1) && s < 120.0f) {
					if (g.dueloIA.decide() == 0 && g.ctl.estado == control::MENU) {
						g.ctl.opcion = (int)g.ctl.opciones.size() - 1;
						g.apretar(PAD_CROSS, 0.3f);
					}
					g.esperar(0.5f);
					s += 0.5f;
				}
				if (g.dueloIA.activo == 1 && g.dueloIA.ganador < 0) {
					FALLO("el turno %d del rival no termina (fase %s)", t0, nombreFase(g.dueloIA.fase));
					break;
				}
				g.esperar(1.0f);
			}
			turnosVistos = std::max(turnosVistos, g.dueloIA.turno);
		}
		if (g.dueloIA.ganador < 0 && g.dueloIA.turno < turnoInicial + 4) FALLO("el duelo %d no llego a 4 turnos (turno %d)", n, g.dueloIA.turno);
		printf("duelo %d: contra %s, turnos %d -> %d, fases del jugador con CUADRADO %d, LP %d / %d\n", n, g.ctl.vista.rivalEs.c_str(), turnoInicial,
		       g.dueloIA.turno, fasesJugador, g.dueloIA.l[0].lp, g.dueloIA.l[1].lp);
		// rama classic-duelo-ui: el registro (L3) y los visores de zonas (X en el cementerio), con el mando
		if (g.ctl.vista.pantalla == VD::DUELO) {
			// (rama classic-arreglo-visor: el duelo de 4 turnos corta al empezar la Principal 1 del turno 5, con lo de la
			// Standby todavia en curso, p. ej. Shadowknight Archfiend: si el duelo le pregunta al jugador, se contesta
			// "No activar" como en las fases; los visores se miran con la mesa en calma. Con la pregunta abierta detras del
			// visor el detalle vuelve a la pregunta: lo mira prueba_front_total, CU-FT-VISOR-PREGUNTA)
			// (rama classic-ia-red: hasta 60 s de juego; con la maquina cargada el turno del rival puede pasar de 20 s y,
			// si el rival sigue jugando mientras se miran los visores, su cementerio cambia entre dos botones)
			for (int k = 0; k < 120 && g.dueloIA.ganador < 0 &&
			                (g.ctl.estado != control::NAVEGAR || g.ctl.bloqueado || g.dueloIA.ef.preg.tipo != ia::P_NADA || g.dueloIA.decide() != 0);
			     k++) {
				if (g.ctl.estado == control::MENU && g.ctl.enPregunta) g.ctl.opcion = (int)g.ctl.opciones.size() - 1, g.apretar(PAD_CROSS, 0.4f);
				else if (g.ctl.estado == control::ELEGIR_CARTA) g.apretar(PAD_CROSS, 0.4f);
				else if (g.ctl.estado != control::NAVEGAR) g.apretar(PAD_CIRCLE, 0.2f);
				else g.esperar(0.5f);
			}
			g.apretar(PAD_L3, 0.3f);
			if (!g.consulta.vis.abierto || g.consulta.vis.pestana != visores::P_REGISTRO) FALLO("L3 no abre el registro del duelo");
			if (g.consulta.reg.lineas.empty()) FALLO("el registro esta vacio despues de %d turnos", g.dueloIA.turno);
			int turnos = 0, fases = 0;
			for (const registro::Linea& l : g.consulta.reg.lineas) turnos += l.tipo == ia::S_TURNO, fases += l.tipo == ia::S_FASE;
			if (turnos < 2 || fases < 2) FALLO("el registro no anota los turnos (%d) o las fases (%d)", turnos, fases);
			printf("registro: %d lineas (%d turnos, %d fases); ultima: %s\n", (int)g.consulta.reg.lineas.size(), turnos, fases,
			       g.consulta.reg.lineas.empty() ? "-" : registro::Registro::texto(g.consulta.reg.lineas.back(), nullptr).c_str());
			g.apretar(PAD_DOWN, 0.2f);
			if (g.consulta.reg.lineas.size() > 1 && g.consulta.vis.sel != 1) FALLO("abajo no recorre el registro (fila %d)", g.consulta.vis.sel);
			g.apretar(PAD_RIGHT, 0.2f);   // -> cementerio
			if (g.consulta.vis.pestana != visores::P_CEMENTERIO) FALLO("derecha no pasa a la pestanna del cementerio");
			if (g.consulta.filas(g.ctl) != (int)g.dueloIA.l[0].cementerio.size())
				FALLO("el visor del cementerio tiene %d cartas y el duelo %d", g.consulta.filas(g.ctl), (int)g.dueloIA.l[0].cementerio.size());
			g.apretar(PAD_SQUARE, 0.2f);   // -> el del rival
			if (g.consulta.vis.lado != 1 || g.consulta.filas(g.ctl) != (int)g.dueloIA.l[1].cementerio.size())
				FALLO("cuadrado no muestra el cementerio del rival (%d cartas, duelo %d)", g.consulta.filas(g.ctl), (int)g.dueloIA.l[1].cementerio.size());
			if (g.consulta.filas(g.ctl) > 0) {   // X: el detalle grande de la carta; circulo vuelve al visor
				g.apretar(PAD_CROSS, 0.2f);
				if (g.ctl.estado != control::DETALLE || g.ctl.cartaDetalle != g.dueloIA.l[1].cementerio.back()) FALLO("X en el visor no abre el detalle de la carta");
				g.apretar(PAD_CIRCLE, 0.2f);
				if (!g.consulta.vis.abierto || g.ctl.estado != control::NAVEGAR) FALLO("circulo en el detalle no vuelve al visor");
			}
			g.apretar(PAD_RIGHT, 0.2f), g.apretar(PAD_RIGHT, 0.2f);   // -> extra
			if (g.consulta.vis.pestana != visores::P_EXTRA || g.consulta.filas(g.ctl) != (int)g.dueloIA.l[1].extra.size()) FALLO("el visor del extra del rival no coincide");
			g.apretar(PAD_CIRCLE, 0.2f);
			if (g.consulta.vis.abierto) FALLO("circulo no cierra el panel");
			// X sobre el cementerio de la mesa: su visor
			g.ctl.foco = control::Foco{ control::F_ZONA, control::zonaPorId("rival_cementerio") };
			g.apretar(PAD_CROSS, 0.2f);
			if (!g.consulta.vis.abierto || g.consulta.vis.pestana != visores::P_CEMENTERIO || g.consulta.vis.lado != 1) FALLO("X en el cementerio del rival no abre su visor");
			g.apretar(PAD_L3, 0.2f);
			if (g.consulta.vis.abierto) FALLO("L3 no cierra el panel");
			g.ctl.foco = control::Foco{ control::F_ZONA, control::zonaPorId("jugador_desterrado") };
			g.apretar(PAD_CROSS, 0.2f);
			if (!g.consulta.vis.abierto || g.consulta.vis.pestana != visores::P_DESTERRADAS || g.consulta.vis.lado != 0) FALLO("X en las desterradas no abre su visor");
			g.apretar(PAD_CIRCLE, 0.2f);
			printf("visores: cementerio %d / %d, desterradas %d / %d, extra %d / %d; jugables ahora: mano %d, zonas %d; contadores revisados %d veces\n",
			       (int)g.dueloIA.l[0].cementerio.size(), (int)g.dueloIA.l[1].cementerio.size(), (int)g.dueloIA.l[0].desterrados.size(),
			       (int)g.dueloIA.l[1].desterrados.size(), (int)g.dueloIA.l[0].extra.size(), (int)g.dueloIA.l[1].extra.size(), (int)g.consulta.jug.mano.size(),
			       (int)g.consulta.jug.zonas.size(), g.revisionesContadores);
		}
		// salir: con el menu del duelo (rama classic-md-fases: el engranaje, panel tactil). Alternando: Abandonar duelo / Salir al menu principal
		if (g.ctl.vista.pantalla == VD::DUELO) {
			g.esperar(1.0f);
			if (g.ctl.vista.pantalla == VD::DUELO) {
				const int derrotas = g.ctl.vista.derrotas;
				g.apretar(PAD_OPTIONS, 0.3f);
				if (g.pausa.abierta) FALLO("OPTIONS en el duelo abrio la pausa (es el selector de fases)");
				if (g.ctl.selFases.abierto) g.apretar(PAD_CIRCLE, 0.3f);
				g.apretar(PAD_TOUCH_PAD, 0.3f);
				if (!g.pausa.abierta) FALLO("el panel tactil no abre el menu del duelo (engranaje)");
				if (g.ajustesAbiertos()) FALLO("el panel tactil abrio los Ajustes");
				// Continuar: cierra y el duelo sigue
				g.pausa.opcion = 0;
				g.apretar(PAD_CROSS, 0.3f);
				if (g.pausa.abierta || g.ctl.vista.pantalla != VD::DUELO) FALLO("Continuar no vuelve al duelo");
				g.apretar(PAD_TOUCH_PAD, 0.3f);
				const int quiero = n % 2 == 0 ? Juego::PA_ABANDONAR : Juego::PA_MENU_PRINCIPAL;
				for (int k = 0; k < 8 && g.opcionesPausa()[(size_t)g.pausa.opcion] != quiero; k++) g.apretar(PAD_DOWN, 0.2f);
				g.apretar(PAD_CROSS, 0.5f);
				if (quiero == Juego::PA_ABANDONAR && g.ctl.vista.derrotas != derrotas + 1) FALLO("Abandonar duelo no cuenta la derrota");
				if (quiero == Juego::PA_ABANDONAR && g.ctl.vista.pantalla == VD::RESULTADO) g.apretar(PAD_CROSS, 0.5f);   // (rama classic-resultado)
				// (rama classic-menus: "Salir al menu principal" va al menu principal; de ahi se vuelve a elegir rival)
				const int pantallaEsperada = quiero == Juego::PA_MENU_PRINCIPAL ? VD::MENU : VD::ELEGIR_RIVAL;
				if (g.ctl.vista.pantalla != pantallaEsperada || g.pausa.abierta) FALLO("la pausa no va a la pantalla esperada (pantalla %d, esperada %d)", g.ctl.vista.pantalla, pantallaEsperada);
				if (g.ctl.vista.pantalla == VD::MENU) g.irAElegirRival();
			}
		} else if (g.ctl.vista.pantalla == VD::RESULTADO) {
			g.esperar(1.0f);
			g.apretar(PAD_CROSS, 1.0f);
		}
		// en elegir rival: la pausa tambien sirve (cambiar de modo historia / libre)
		if (n == 1 && g.ctl.vista.pantalla == VD::ELEGIR_RIVAL) {
			const int modo = g.ctl.vista.modo;
			g.apretar(PAD_OPTIONS, 0.3f);
			if (!g.pausa.abierta) FALLO("OPTIONS no abre la pausa en elegir rival");
			const int quiero = modo == VD::HISTORIA ? Juego::PA_DUELO_LIBRE : Juego::PA_HISTORIA;
			for (int k = 0; k < 8 && g.opcionesPausa()[(size_t)g.pausa.opcion] != quiero; k++) g.apretar(PAD_DOWN, 0.2f);
			g.apretar(PAD_CROSS, 0.5f);
			if (g.ctl.vista.modo == modo) FALLO("la pausa no cambia de modo");
		}
	}
	// rama classic-ps5-duelo-log: la ruta REAL del juego dejo una bitacora legible (cabecera, robos, jugadas, decisiones del CPU)
	{
		std::ifstream f(carpetaLog + "/duelo_log.txt", std::ios::binary);
		std::stringstream ss;
		ss << f.rdbuf();
		const std::string t = ss.str();
		auto cuenta = [&](const char* x) {
			int n = 0;
			for (size_t p = t.find(x); p != std::string::npos; p = t.find(x, p + strlen(x))) n++;
			return n;
		};
		if (t.empty()) FALLO("el juego no escribio la bitacora del duelo");
		else {
			if (cuenta("DUELO NUEVO") < N) FALLO("la bitacora tiene %d cabeceras y se jugaron %d duelos", cuenta("DUELO NUEVO"), N);
			if (cuenta(" roba ") < 4 || cuenta("===== Turno ") < 4 || cuenta("Fase: ") < 4) FALLO("la bitacora no tiene robos, turnos y fases");
			if (cuenta(" CPU] ") < 5 || cuenta(" J] ") < 5) FALLO("la bitacora no tiene lineas del jugador y del CPU");
			if (cuenta("[CPU decide]") < 1) FALLO("la bitacora no tiene decisiones del CPU");
			printf("bitacora del duelo: %d bytes, %d cabeceras, %d robos, %d decisiones del CPU\n", (int)t.size(), cuenta("DUELO NUEVO"), cuenta(" roba "), cuenta("[CPU decide]"));
		}
	}
	if (g.fallosMesa) FALLO("la mesa no refleja el duelo %d veces", g.fallosMesa);
	if (g.fallosContadores) FALLO("los contadores de las zonas no son los del duelo %d veces", g.fallosContadores);
	if (g.revisionesContadores == 0) FALLO("no se revisaron los contadores de las zonas");
	// OPTIONS mantenido 2 s (lo hace la plantilla): no abre la pausa al soltar
	{
		g.apretar(PAD_CIRCLE, 0.2f);
		const bool antes = g.pausa.abierta;
		for (int k = 0; k < 130; k++) g.cuadro(PAD_OPTIONS);
		g.esperar(0.3f);
		if (g.pausa.abierta != antes) FALLO("mantener OPTIONS 2 s abre/cierra la pausa (debe ser solo Ajustes)");
	}
	printf("turnos jugados hasta %d, fases cambiadas por el jugador %d\n", turnosVistos, fasesJugador);
	printf(fallos ? "PRUEBA CON FALLOS (%d)\n" : "PRUEBA OK (%d fallos)\n", fallos);
	return fallos ? 1 : 0;
}
