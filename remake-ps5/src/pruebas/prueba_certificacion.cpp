// Probador automatico (rama classic-probador): certifica carta por carta que las cartas con efecto hacen lo que dice su
// texto, en el motor (ia::Duelo) y en el front (la mesa de control.h, duelo_juego.h y la logica de las animaciones de
// efectos_logica.h), sin GPU ni capturas: todo con comprobaciones sobre las estructuras de datos.
//  1. Casos a mano: los ~500 casos de prueba_efectos.cpp (incluido tal cual, con ganchos) dan el resultado por carta y
//     cada escena se vuelve a presentar en el front desde la mesa vacia (todos sus sucesos).
//  2. Plantillas: para cada carta con efecto, la expectativa sale de su texto (cartas_ui_en.txt: destruir, quemar,
//     ganar LP, robar, buscar, recuperar, invocar, desterrar, control, negar, equipar, posicion, ATK/DEF, descartar,
//     devolver a la mano / al mazo) y se prueba en varios contextos armados (de la mano en tu fase principal,
//     colocada, en respuesta a un ataque / una invocacion / una activacion / la fase final del rival, volteo,
//     invocacion, destruido en batalla, al atacar). Lo observado sale de los sucesos y de la diferencia del duelo
//     antes / despues. Cada activacion se presenta en el front: animacion encolada y terminada, mesa = duelo, LP,
//     manos y robos animados, nada bloqueado.
//  3. Reporte: docs/certificacion_efectos.md y docs/certificacion_efectos.csv (una fila por carta).
// Compilar y correr (contenedor arch-ps5; ~2 min de compilacion, el probador en ~1 min):
//   bash remake-ps5/src/pruebas/certificar.sh
#include "control.h"
#include "md_ritmo.h"   // rama classic-md-ritmo
#include "entrada_md.h"   // rama classic-md-entradas: md_ritmo_juego.h espera a que termine la entrada de fusion/ritual
#include "efectos_logica.h"
#include "mazo.h"
#include "personajes.h"
#include "historia_dialogos.h"
#include "tutorial.h"

#include <chrono>
#include <map>
#include <regex>
#include <set>

// ---- maquetas de lo que el juego tiene en Vulkan / audio / entornos (como prueba_juego.cpp)
namespace entornos {
inline int porTema(const std::string&) { return 0; }
inline int claveDe(const std::string&) { return 0; }
static const char* const CLAVES[] = { "neutro" };
}
namespace campos {
inline bool esCampo(const std::string& id) { return control::esMagiaDeCampo(id); }
}
enum : uint32_t {
	PAD_OPTIONS = 0x000008, PAD_UP = 0x000010, PAD_RIGHT = 0x000020, PAD_DOWN = 0x000040, PAD_LEFT = 0x000080,
	PAD_L1 = 0x000400, PAD_R1 = 0x000800, PAD_TRIANGLE = 0x001000, PAD_CIRCLE = 0x002000, PAD_CROSS = 0x004000,
	PAD_SQUARE = 0x008000, PAD_R3 = 0x000004, PAD_TOUCH_PAD = 0x100000,
};
struct EstadoMando { uint32_t pressed{ 0 }, held{ 0 }; float left_x{ 0 }, left_y{ 0 }, right_x{ 0 }, right_y{ 0 }, l2{ 0 }, r2{ 0 }; };

// el juego sin Vulkan: duelo_juego.h tal cual, con la logica real de las animaciones (efc::Logica)
struct Juego {
	std::string carpetaUi;
	std::string getAssetPath() const { return carpetaUi + "../"; }
	struct Sonido {
		void tocarMusica(const char*, float) {}
		void sfx(const char*, float = 1.0f, float = 0.0f, float = 0.0f) {}
		bool hay(const char*) const { return false; }
		void pararMusica(float) {}
	} sonido;
	struct Interfaz {
		std::function<bool(const char*, int&, int&)> statsZona;
		int lps[2]{ 8000, 8000 };
		std::string nombreCarta(const std::string& id) const { return id; }
		void cambiarLP(bool rival, int v) { lps[rival ? 1 : 0] = std::max(0, std::min(v, 99999)); }   // (como interfaz.h: clamp)
		void fijarLP(bool rival, int v) { lps[rival ? 1 : 0] = v; }   // (rama classic-visual-final)
		int lp(bool rival) const { return lps[rival ? 1 : 0]; }
		void contarMazos(int, int, int) {}
	} interfazDuelo;
	efc::Logica efectosCartas;
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
	enum { CAMPO_VACIO = 0, CAMPO_COLOCANDO, CAMPO_BOCA_ABAJO, CAMPO_ACTIVANDO, CAMPO_ACTIVO, CAMPO_DESTRUYENDO };
	int estadoCampo{ CAMPO_VACIO };
	bool ataquesSolos{ false }, campoAuto{ false };
	float tInvocada[control::NUM_ZONAS_MESA]{};
	int zonaAtacante{ -1 }, zonaObjetivo{ -1 }, danio{ 0 }, ataquesVistos{ 0 };
	bool danioAlRival{ false }, muereA{ false }, muereO{ false }, ataqueDemo{ false };
	float tAtaque{ -1.0f };
	static constexpr float T_IMPACTO = ritmo::T_ATAQUE_BAJO, DURACION_ATAQUE = ritmo::T_FIN_ATAQUE;   // (rama classic-ps5-ataque3s: el ataque de Joey, ~3 s; antes 0.55 / 1.6)
	bool hayAtaque() const { return tAtaque >= 0.0f && zonaAtacante >= 0 && zonaObjetivo >= 0; }
	// lo que el front recibio (para comprobar)
	std::vector<std::pair<std::string, int>> animaciones;   // activarEfectoCarta: carta, zona
	int camposActivados{ 0 }, camposDestruidos{ 0 };
	float tiempo{ 0.0f };
	bool zonaRival(int z) const { return z >= 0 && z < control::NUM_ZONAS_MESA && control::ZONAS_MESA[z].rival; }
	void activarEfectoCarta(const std::string& carta, int zona)
	{
		animaciones.push_back({ carta, zona });
		efectosCartas.activar(carta, zona, zonaRival(zona), (uint32_t)(tiempo * 1000.0f) + (uint32_t)zona * 7919u);
	}
	void activarCampo(const std::string&, int = 0) { camposActivados++; }
	void destruirCampo(const std::string&, int = 0) { camposDestruidos++; }
	mazo::Coleccion coleccion;
	mazo::Mazo mazoJugador, mazoDuelo;
	std::string rutaMazoJ;
	bool pedidoConstructor{ false };
	struct { bool abierto{ false }; } constructorMazo;
	void repartir() { mazoDuelo = mazoJugador; }
	struct { bool visible{ false }; } ui;
	struct { bool overlay{ true }; } settings;
	bool quit{ false };
	bool ajustesAbiertos() const { return false; }
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

	void preparar(const std::string& ui_)
	{
		carpetaUi = ui_;
		rutaMazoJ = carpetaUi + "../mazo.txt";   // (progreso.txt y variedad.txt al lado: /tmp/cert)
		coleccion.leer(carpetaUi);
		ctl.clase = [this](const std::string& c) {
			const ia::Carta* k = baseIA.carta(c);
			return k ? k->clase : std::string();
		};
		ctl.ataqueEnCurso = [this] { return hayAtaque(); };
		ctl.campoOcupado = [] { return false; };
		ctl.preparar({}, {}, {});
		baseIA.leer(carpetaUi);
		prepararDuelo();
		efectosCartas.cargarTabla(carpetaUi);
		mandoListo = true;
	}
	// copia de yugireload.cpp (avanzarAtaque sin Vulkan ni la demo): el golpe voltea al objetivo y quita LP; al final
	// las destruidas van al cementerio de la mesa
	void avanzarAtaque()
	{
		for (float& t : tInvocada) t += frameTimer;
		if (!hayAtaque()) return;
		const float antes = tAtaque;
		tAtaque += frameTimer;
		if (antes < T_IMPACTO && tAtaque >= T_IMPACTO) {
			control::Celda& co = ctl.zonas[zonaObjetivo];
			if (co.bocaAbajo) {
				co.antesBocaAbajo = true;
				co.bocaAbajo = false;
				co.tVolteo = 0.0f;
			}
			if (!ataqueDemo && danio > 0) interfazDuelo.cambiarLP(danioAlRival, interfazDuelo.lp(danioAlRival) - danio);
		}
		if (tAtaque > DURACION_ATAQUE) {
			if (muereO) ctl.alCementerio(zonaObjetivo);
			if (muereA) ctl.alCementerio(zonaAtacante);
			tAtaque = -1.0f;
			zonaAtacante = zonaObjetivo = -1;
		}
	}
	// un cuadro como render(), sin botones: ataque, control.h, el duelo y las animaciones de las cartas
	void cuadro()
	{
		tiempo += frameTimer;
		avanzarAtaque();
		ctl.avanzar(frameTimer, 0);
		ctl.eventos.clear();
		avanzarDueloIA(frameTimer);
		efectosCartas.avanzar(frameTimer);
	}
	bool alDia() const
	{
		return sucesosVistos >= dueloIA.sucesos.size() && !hayAtaque() && !ctl.vuelo.activo && !ctl.robo.activo && reparto <= 0 && !mesaPendiente &&
		       esperaEfecto <= 0.0f;
	}
	// el duelo "d" pasa a ser el del juego con la mesa ya presentada hasta "vistos" (vistos = 0: desde la mesa vacia)
	void cargar(const ia::Duelo& d, size_t vistos, const ia::Duelo* antes)
	{
		dueloIA = d;
		turnoIA = ia::Turno{};   // (el rival no juega: solo se presenta lo que ya paso)
		ctl.preparar({}, {}, {});
		ctl.robo = {};
		ctl.vuelo = {};
		ctl.vista.pantalla = VD::DUELO;
		ctl.vista.sorteo = false;
		ctl.estado = control::NAVEGAR;
		ctl.enPregunta = ctl.armando = ctl.eleccionDuelo = false;
		reparto = 0;
		ocultas[0] = ocultas[1] = 0;
		esperaEfecto = 0.0f;
		tAtaque = -1.0f;
		zonaAtacante = zonaObjetivo = -1;
		efectosCartas.activos.clear();
		animaciones.clear();
		camposActivados = camposDestruidos = 0;
		if (antes) {   // la mesa como estaba el duelo "antes" (todo presentado)
			dueloIA = *antes;
			sincronizarMesaDuelo();
			interfazDuelo.cambiarLP(false, antes->l[0].lp);
			interfazDuelo.cambiarLP(true, antes->l[1].lp);
			dueloIA = d;
			// (la mesa todavia no sabe de las cartas que entraron a la mano: llegan con su suceso de robo)
		} else {
			interfazDuelo.cambiarLP(false, ia::LP_INICIAL);
			interfazDuelo.cambiarLP(true, ia::LP_INICIAL);
			for (control::Celda& c : ctl.zonas) c = control::Celda{};
		}
		sucesosVistos = vistos;
		mesaPendiente = !antes || vistos < d.sucesos.size();   // (con "antes": la mesa solo se entera por sus sucesos, como en la consola)
	}
};

static Juego juego;

// los casos a mano (prueba_efectos.cpp) con sus ganchos
#define main main_efectos
#include "prueba_efectos.cpp"
#undef main

// ================================================================================================= el front
struct ResultadoFront {
	bool ok{ true };
	std::string falla;
	void mal(const std::string& m)
	{
		if (ok) falla = m;
		ok = false;
	}
};
static std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
static std::string fmt(const char* f, ...)
{
	char b[512];
	va_list a;
	va_start(a, f);
	vsnprintf(b, sizeof(b), f, a);
	va_end(a);
	return b;
}

static int VD_DUELO() { return control::VistaDuelo::DUELO; }

// presenta en el front lo que paso entre "antes" (nullptr: desde la mesa vacia) y "d" y lo comprueba
static ResultadoFront verificarFront(const Duelo& d, const Duelo* antes)
{
	ResultadoFront r;
	Juego& g = juego;
	const size_t desde = antes ? antes->sucesos.size() : 0;
	g.cargar(d, desde, antes);
	// lo que el front tiene que recibir
	int activacionesMT = 0, robosJ = 0;
	std::array<bool, 2> lpTocado{};
	std::set<std::string> cartasActivadas;
	for (size_t k = desde; k < d.sucesos.size(); k++) {
		const Suceso& s = d.sucesos[k];
		if (s.tipo == S_ACTIVA_MT) {
			const int z = s.zona == ZONA_CAMPO ? control::zonaPorId(s.lado ? "rival_campo" : "jugador_campo") : Juego::zonaMesaIA(s.lado, s.zona, true);
			if (z >= 0) {
				activacionesMT++;
				cartasActivadas.insert(s.carta);
			}
		}
		if (s.tipo == S_ROBO && s.lado == 0) robosJ++;
		if (s.tipo == S_LP || s.tipo == S_DANNO) lpTocado[(size_t)(s.lado ? 1 : 0)] = true;
		if (s.tipo == S_RESUELVE || s.tipo == S_COMBATE) lpTocado = { true, true };
	}
	const int serie0 = g.ctl.robo.serie;
	float t = 0.0f;
	const float tope = 25.0f + 3.0f * (float)(d.sucesos.size() - desde);
	while (!(g.alDia() && !g.efectosCartas.hay()) && t < tope) {
		g.cuadro();
		t += g.frameTimer;
		if (g.ctl.vista.pantalla != VD_DUELO()) break;
	}
	if (g.ctl.vista.pantalla != VD_DUELO()) {
		if (d.ganador < 0) r.mal("el front salio de la pantalla del duelo sin ganador");
		return r;   // (fin del duelo: la pantalla de resultado)
	}
	if (!g.alDia()) {
		r.mal(fmt("cuelgue: el front no se pone al dia en %.0f s (sucesos %zu/%zu, ataque %d, vuelo %d, robo %d, mesa pendiente %d, espera %.2f)", tope,
		          g.sucesosVistos, d.sucesos.size(), (int)g.hayAtaque(), (int)g.ctl.vuelo.activo, (int)g.ctl.robo.activo, (int)g.mesaPendiente, g.esperaEfecto));
		return r;
	}
	if (g.efectosCartas.hay()) r.mal(fmt("la animacion de %s no termina", g.efectosCartas.activos[0].carta.c_str()));
	// cada activacion de magia/trampa: su animacion
	std::set<std::string> animadas;
	for (const auto& a : g.animaciones) animadas.insert(a.first);
	for (const std::string& c : cartasActivadas)
		if (!animadas.count(c)) r.mal(fmt("la activacion de %s no encolo su animacion (efectos_cartas)", c.c_str()));
	if ((int)g.animaciones.size() < activacionesMT && (int)cartasActivadas.size() == activacionesMT)
		r.mal(fmt("%d activaciones y %d animaciones", activacionesMT, (int)g.animaciones.size()));
	// la mesa = el duelo
	for (int lado = 0; lado < 2; lado++) {
		const Lado& x = d.l[(size_t)lado];
		for (int i = 0; i < ZONAS; i++) {
			const int zm = Juego::zonaMesaIA(lado, i, false), zt = Juego::zonaMesaIA(lado, i, true);
			const control::Celda& cm = g.ctl.zonas[(size_t)zm];
			const Monstruo& m = x.m[(size_t)i];
			if (cm.carta != m.id || (!m.vacia() && (cm.bocaAbajo != m.bocaAbajo || cm.defensa != m.defensa)))
				r.mal(fmt("mesa != duelo en %s: mesa '%s'%s%s, duelo '%s'%s%s", control::ZONAS_MESA[zm].id, cm.carta.c_str(), cm.bocaAbajo ? " boca abajo" : "",
				          cm.defensa ? " defensa" : "", m.id.c_str(), m.bocaAbajo ? " boca abajo" : "", m.defensa ? " defensa" : ""));
			const control::Celda& ct = g.ctl.zonas[(size_t)zt];
			const MagiaTrampa& c = x.mt[(size_t)i];
			if (ct.carta != c.id || (!c.vacia() && ct.bocaAbajo != c.bocaAbajo))
				r.mal(fmt("mesa != duelo en %s: mesa '%s'%s, duelo '%s'%s", control::ZONAS_MESA[zt].id, ct.carta.c_str(), ct.bocaAbajo ? " boca abajo" : "", c.id.c_str(),
				          c.bocaAbajo ? " boca abajo" : ""));
		}
		const int zc = control::zonaPorId(lado ? "rival_campo" : "jugador_campo");
		if (zc >= 0 && (g.ctl.zonas[(size_t)zc].carta != x.campo.id || (!x.campo.vacia() && g.ctl.zonas[(size_t)zc].bocaAbajo != x.campo.bocaAbajo)))
			r.mal(fmt("mesa != duelo en la zona de campo %d: mesa '%s', duelo '%s'", lado, g.ctl.zonas[(size_t)zc].carta.c_str(), x.campo.id.c_str()));
		// LP mostrados = reales (si el caso no los cambio a mano, sin sucesos)
		if (g.interfazDuelo.lp(lado == 1) != std::max(0, x.lp) && (antes || lpTocado[(size_t)lado]))
			r.mal(fmt("LP mostrados %d, reales %d (lado %d)", g.interfazDuelo.lp(lado == 1), x.lp, lado));
	}
	if (g.ctl.manoJ != d.l[0].mano) r.mal(fmt("mano del jugador en la mesa %d cartas, en el duelo %d", (int)g.ctl.manoJ.size(), (int)d.l[0].mano.size()));
	if (g.ctl.manoR != (int)d.l[1].mano.size()) r.mal(fmt("mano del rival en la mesa %d, en el duelo %d", g.ctl.manoR, (int)d.l[1].mano.size()));
	// las cartas robadas por el jugador pasan por la animacion de robo (las que siguen en la mano)
	const int robosVistos = g.ctl.robo.serie - serie0;
	if (antes && robosVistos < robosJ) {
		int siguen = 0;
		for (size_t k = desde; k < d.sucesos.size(); k++) {
			const Suceso& s = d.sucesos[k];
			if (s.tipo == S_ROBO && s.lado == 0 && std::find(d.l[0].mano.begin(), d.l[0].mano.end(), s.carta) != d.l[0].mano.end()) siguen++;
		}
		if (robosVistos < siguen) r.mal(fmt("%d robos del jugador y %d animaciones de robo", siguen, robosVistos));
	}
	// nada bloqueado: si le toca al jugador, el mando queda libre
	g.cuadro();
	if (d.ganador < 0 && d.decide() == 0 && g.ctl.bloqueado) r.mal("el jugador queda bloqueado con el duelo esperando su jugada");
	return r;
}

// ================================================================================================= las cartas
struct Texto {
	std::string id, nombre, clase, raza, atributo, texto, subtipo;
	int nivel{ 0 }, atk{ 0 }, def{ 0 };
};
static std::vector<Texto> textos;
static std::map<std::string, size_t> indiceTexto;

static bool leerTextos(const std::string& ui)
{
	std::ifstream f(ui + "cartas_ui_en.txt");
	if (!f) return false;
	std::string l;
	while (std::getline(f, l)) {
		while (!l.empty() && (l.back() == '\r' || l.back() == '\n')) l.pop_back();
		std::vector<std::string> k = partir(l, '|');
		if (k.size() < 9) continue;
		Texto t;
		t.id = k[0];
		t.nombre = k[1];
		t.clase = k[2];
		t.atributo = k[3];
		t.raza = k[4];
		t.nivel = atoi(k[5].c_str());
		t.atk = atoi(k[6].c_str());
		t.def = atoi(k[7].c_str());
		for (size_t i = 8; i < k.size(); i++) t.texto += (i > 8 ? "|" : "") + k[i];
		const Carta* c = base.carta(t.id);
		t.subtipo = c ? c->subtipo : "";
		indiceTexto[t.id] = textos.size();
		textos.push_back(t);
	}
	return !textos.empty();
}

// ---- familias de efecto (las plantillas de comprobacion)
enum Fam { F_DESTRUYE = 0, F_QUEMA, F_GANA_LP, F_ROBA, F_BUSCA, F_RECUPERA, F_INVOCA, F_DESTIERRA, F_CONTROL, F_NIEGA, F_EQUIPA, F_POSICION, F_ATKDEF,
	       F_DESCARTA, F_A_LA_MANO, F_AL_MAZO, F_FICHA, F_PIERDE_LP, NUM_FAM };
static const char* const NOMBRE_FAM[] = { "destruir", "quemar", "ganar_lp", "robar", "buscar", "recuperar", "invocar", "desterrar", "control", "negar",
	                                      "equipar", "posicion", "atk_def", "descartar", "a_la_mano", "al_mazo", "ficha", "pierde_lp" };
using Fams = std::array<bool, NUM_FAM>;
static std::string nombres(const Fams& f)
{
	std::string r;
	for (int k = 0; k < NUM_FAM; k++)
		if (f[(size_t)k]) r += (r.empty() ? "" : "+") + std::string(NOMBRE_FAM[k]);
	return r.empty() ? "-" : r;
}

struct Expectativa {
	Fams fam{};
	int danno{ 0 }, ganaLP{ 0 }, roba{ 0 };   // numeros del texto (0 = sin numero o varios)
	bool atkHastaFin{ false }, controlHastaFin{ false };   // ATK/DEF o control "hasta el final del turno": se deshacen al pasarlo
};
static std::string minus(std::string s)
{
	for (char& c : s) c = (char)tolower((unsigned char)c);
	return s;
}
static int numeroPalabra(const std::string& w)
{
	if (w == "a" || w == "an" || w == "one" || w == "1") return 1;
	if (w == "two" || w == "2") return 2;
	if (w == "three" || w == "3") return 3;
	if (w == "four" || w == "4") return 4;
	if (w == "five" || w == "5") return 5;
	return atoi(w.c_str());
}
static Expectativa expectativaDe(const Texto& t)
{
	Expectativa e;
	const std::string s = minus(t.texto);
	auto hay = [&](const char* re) { return std::regex_search(s, std::regex(re)); };
	// destruir: el verbo "destroy" sobre algo que no sea solo esta carta
	{
		std::regex r("\\bdestroy\\b( this card\\b)?");
		for (auto it = std::sregex_iterator(s.begin(), s.end(), r); it != std::sregex_iterator(); ++it)
			if (!(*it)[1].matched) e.fam[F_DESTRUYE] = true;
	}
	std::smatch m;
	auto porCada = [&](const std::smatch& x) {   // "100 damage for each...": el numero no es el total
		const std::string sigue = s.substr((size_t)(x.position(0) + x.length(0)), 70);
		return std::regex_search(sigue, std::regex("^[^.;]{0,60}(for each|per |x the|times|multiplied|equal to)"));
	};
	if (std::regex_search(s, m, std::regex("inflict (\\d+)( points of)? damage"))) {
		e.fam[F_QUEMA] = true;
		e.danno = porCada(m) ? 0 : atoi(m[1].str().c_str());
	} else if (hay("inflict[s]? .{0,60}damage|damage to your opponent's life points|damage to your opponent equal")) {
		e.fam[F_QUEMA] = true;
	}
	if (std::regex_search(s, m, std::regex("(gain|increase your life points by|recover) (\\d+) (life points|lp)"))) {
		e.fam[F_GANA_LP] = true;
		e.ganaLP = porCada(m) ? 0 : atoi(m[2].str().c_str());
	} else if (hay("gain[s]? .{0,30}life points|increases? (your|the) life points|life points of both players|gain lp")) {
		e.fam[F_GANA_LP] = true;
	}
	if (std::regex_search(s, m, std::regex("\\bdraws? (\\d+|a|one|two|three) cards?"))) {
		e.fam[F_ROBA] = true;
		e.roba = numeroPalabra(m[1].str());
		if (hay("both players draw|each player draws|your opponent draws")) e.roba = 0;
	} else if (hay("\\bdraw (cards|1 card|as many)")) {
		e.fam[F_ROBA] = true;
	}
	if (hay("from your deck to your hand|add .{0,80}from your deck|add .{0,80}from (the|your) deck")) e.fam[F_BUSCA] = true;
	if (hay("from your graveyard to your hand|add .{0,80}from your graveyard|return .{0,80}from (your|the) graveyard to (your|the|its owner's) hand|(return|add) .{0,80}(sent to|in) (the|your) graveyard.{0,60}to your hand|return from (your|the) graveyard to"))
		e.fam[F_RECUPERA] = true;
	{
		std::regex r("(cannot |can't |must be |must first be |be )?special summon(ed)?s?\\b");
		for (auto it = std::sregex_iterator(s.begin(), s.end(), r); it != std::sregex_iterator(); ++it)
			if (!(*it)[1].matched && !(*it)[2].matched) e.fam[F_INVOCA] = true;
	}
	if (hay("\\bremove .{0,80}from play|\\bbanish(es)?\\b")) e.fam[F_DESTIERRA] = true;
	if (hay("take control|gain control|control of 1|switch(es)? control")) e.fam[F_CONTROL] = true;
	if (hay("\\bnegate")) e.fam[F_NIEGA] = true;
	if (hay("negate[^.]{0,40}equip")) e.fam[F_ATKDEF] = true;   // (negar los equipos se ve en el ATK/DEF de los equipados)
	if (t.subtipo == "magia_equipo" || t.subtipo == "union" || hay("\\bequip (this card|1|it|to|only|that|the)")) e.fam[F_EQUIPA] = true;
	if (hay("change[ds]? (the )?battle position|change[ds]? .{0,60}(to|into) (face-down )?(defense|attack) position|change[ds]? .{0,40}position|face-down defense position|flip .{0,30}face-down|turn .{0,30}face-(up|down)|\\bflip (all|1|it|them|that|the).{0,40}face-(up|down)"))
		e.fam[F_POSICION] = true;
	if (hay("(increase|decrease|gain|gains|lose|loses|raise|raises|reduce|reduces|lower) .{0,50}\\b(atk|def)\\b|\\b(atk|def)\\b.{0,25}(increase|decrease|by \\d+|becomes|is halved|halve|is doubled|double)|\\d+ (atk|def)\\b|original (atk|def)|\\b(atk|def)\\b[^.]{0,30}(equal to|become)"))
		e.fam[F_ATKDEF] = true;
	if (hay("\\bdiscard|(send|sends) .{0,60}deck to the (graveyard|gy)|top \\d+ cards? of .{0,30}deck to the (graveyard|gy)")) e.fam[F_DESCARTA] = true;   // (descartar o enviar del mazo al cementerio)
	if (hay("return .{0,80}to (its owner's|the owner's|their owners'|its owners'|his|your|the) hand|return .{0,40}to the hand")) e.fam[F_A_LA_MANO] = true;
	if (hay("(return|shuffle|place|put) .{0,80}(into|to|on top of|at the bottom of|on the bottom of|to the top of|in) (the|its owner's|your|their owners'|the owner's|their|your opponent's) deck"))
		e.fam[F_AL_MAZO] = true;
	if (hay("\\btokens?\\b")) e.fam[F_FICHA] = true;
	// (las que van por regla: la magia ritual y Polymerization invocan; el Toon se invoca y ataca directo pagando 500)
	if (t.subtipo == "magia_ritual" || hay("\\britual summon|\\bfusion summon")) e.fam[F_INVOCA] = true;
	if (t.subtipo == "toon") e.fam[F_INVOCA] = e.fam[F_QUEMA] = true;
	// (la duracion va con su familia en la misma frase: Creature Swap cambia el control para siempre)
	if (hay("(atk|def)[^.]{0,80}(until the end phase|until the end of (this|the) turn|for the rest of this turn|during this turn)|(until the end phase|until the end of (this|the) turn)[^.]{0,80}(atk|def)"))
		e.atkHastaFin = true;
	if (hay("control[^.]{0,80}until the end phase|until the end phase[^.]{0,80}control")) e.controlHastaFin = true;
	return e;
}

// ================================================================================================= el escenario
static std::vector<std::string> mazoVariado;
static void armarMazoVariado()
{
	// un monstruo de nivel 1-4 de cada tipo y de cada atributo (las busquedas encuentran algo), mas magias y trampas
	std::set<std::string> razas, atributos;
	for (const Carta& c : base.cartas) {
		if (!c.monstruo || c.nivel > 4 || c.nivel < 1 || c.subtipo != "normal") continue;
		if (!razas.count(c.raza) || !atributos.count(c.atributo)) {
			razas.insert(c.raza);
			atributos.insert(c.atributo);
			mazoVariado.push_back(c.id);
		}
	}
	for (const char* id : { "mystical_space_typhoon", "monster_reborn", "dark_hole", "trap_hole", "mirror_force", "pot_of_greed", "kuriboh", "sangan", "dark_magician",
	                        "summoned_skull", "baby_dragon", "celtic_guardian", "mystical_elf", "feral_imp", "polymerization", "toon_world", "buster_blader",
	                        "axe_of_despair", "gather_your_mind" })
		if (base.carta(id)) mazoVariado.push_back(id);
	while (mazoVariado.size() < 40) mazoVariado.push_back(mazoVariado[mazoVariado.size() % 7]);
}

// los contextos en que se intenta cada carta
enum Contexto { K_MANO = 0, K_COLOCADA, K_IGNICION, K_VOLTEO, K_INVOCACION, K_ATAQUE_RIVAL, K_ATAQUE_DIRECTO_RIVAL, K_INVOCA_RIVAL, K_ACTIVA_RIVAL,
	            K_FIN_RIVAL, K_DESTRUIDO_BATALLA, K_ATACA, K_ATACA_DIRECTO, K_PASIVO, K_RITUAL, K_TOON, K_FUSION, NUM_K };
static const char* const NOMBRE_K[] = { "de la mano (tu fase principal)", "colocada (tu fase principal)", "efecto de ignicion", "volteo", "invocacion normal",
	                                    "responde a un ataque", "responde a un ataque directo", "responde a una invocacion", "responde a una activacion",
	                                    "fase final del rival", "destruido en batalla", "ataca", "ataque directo", "en el campo (efecto continuo)", "invocacion ritual", "Toon (invocacion y ataque directo)", "invocacion por fusion" };

// el escenario rico: los dos lados con monstruos (arriba, abajo, ataque, defensa), magias/trampas colocadas,
// cementerios con monstruos, magias y trampas, manos y mazos variados
// Tres escenarios (A, B, C) con otros tipos, atributos y niveles en el campo, la mano y el cementerio: si en el A la
// carta no se puede activar o su efecto no se ve (equipos de un tipo, descartar un monstruo de AGUA...), se prueba en
// el B y en el C. La zona 0 de cada lado es siempre la misma (el ataque del rival sin danno ni destruccion).
static int escenarioActual = 0;
static const char* const NOMBRE_ESCENARIO[] = { "A", "B", "C" };
static void armar(Escena& e, bool sinMonstruosPropios = false, bool sinMonstruosRival = false)
{
	Duelo& d = e.d;
	const int esc = escenarioActual;
	for (Lado& x : d.l) x.mazo = mazoVariado;
	static const char* const PROPIOS[3][3] = { { "battle_ox", nullptr, nullptr }, { "soul_tiger", "turtle_tiger", "basic_insect" },
		                                       { "shining_friendship", "fireyarou", "dancing_elf" } };
	static const char* const RIVALES[3][3] = { { "dark_magician", "giant_soldier_of_stone", "battle_footballer" },
		                                       { "terra_the_terrible", "dark_assailant", "killer_needle" },
		                                       { "tripwire_beast", "the_statue_of_easter_island", "bean_soldier" } };
	if (!sinMonstruosPropios) {
		e.mon(0, "celtic_guardian", true);   // 1400/1200 en defensa boca arriba (el blanco del ataque del rival)
		for (const char* id : PROPIOS[esc])
			if (id) e.mon(0, id);
	}
	if (!sinMonstruosRival) {
		e.mon(1, "baby_dragon");                     // 1200/700: ataca a Celtic (1200 DEF): sin danno ni destruccion
		e.mon(1, RIVALES[esc][0]);                   // A: Dark Magician 2500/2100
		e.mon(1, RIVALES[esc][1], true, true);       // boca abajo en defensa
		e.mon(1, RIVALES[esc][2]);                   // A: Maquina de nivel 4, 1000/2100 (la zona 4 queda libre)
	}
	e.colocar(1, "trap_hole");
	e.colocar(1, "mystical_space_typhoon");
	if (esc > 0 && !sinMonstruosRival) {   // un equipo boca arriba en un monstruo del rival y una colocada propia
		const int ze = e.colocar(1, "axe_of_despair");
		d.l[1].mt[(size_t)ze].bocaAbajo = false;
		d.l[1].mt[(size_t)ze].eqUid = d.l[1].m[1].uid;
		e.colocar(0, esc == 1 ? "trap_hole" : "mystical_space_typhoon");
	}
	d.l[0].cementerio = { "summoned_skull", "mystical_elf", "dark_magician", "pot_of_greed", "trap_hole", "baby_dragon", "feral_imp", "dark_assailant", "aqua_madoor", "lesser_dragon" };
	d.l[1].cementerio = { "summoned_skull", "feral_imp", "pot_of_greed", "mirror_force", "celtic_guardian", "hitotsu_me_giant", "fireyarou" };
	d.l[0].desterrados = { "summoned_skull", "lesser_dragon" };
	d.l[1].desterrados = { "terra_the_terrible" };
	d.l[0].mano = { "celtic_guardian", "mystical_elf", "battle_ox" };
	d.l[1].mano = { "vorse_raider", "pot_of_greed", "celtic_guardian" };
	if (esc == 1) {
		d.l[0].cementerio = { "terra_the_terrible", "killer_needle", "turtle_tiger", "takriminos", "shining_friendship", "dark_hole", "mirror_force", "soul_tiger" };
		d.l[1].cementerio = { "fireyarou", "tripwire_beast", "uraby", "monster_reborn" };
		d.l[0].mano = { "turtle_tiger", "fireyarou", "shining_friendship" };
	} else if (esc == 2) {
		d.l[0].cementerio = { "bean_soldier", "uraby", "fiend_reflection_2", "ancient_lizard_warrior", "souleater", "fireyarou", "tripwire_beast", "raigeki" };
		d.l[1].cementerio = { "turtle_tiger", "killer_needle", "dark_assailant", "trap_hole" };
		d.l[0].mano = { "uraby", "takriminos", "killer_needle" };
	}
	d.recalcular();
}

// los objetivos de una activacion (variante 0: los del rival primero; 1: los propios primero)
static void armarJugada(const Duelo& d, int lado, const Lugar& f, int variante, Jugada& j)
{
	j = jugadaActivar(f);
	const DefCarta* df = defCarta(cartaEn(d, f));
	if (!df) return;
	if (df->nObj > 0) {
		std::vector<Lugar> c = variante == 0 && df->objetivosIA ? df->objetivosIA(d, lado, f) : objetivosDe(d, lado, *df, f);
		const bool propiosPrimero = (variante == 1) != df->favorece;
		if (variante == 2) std::reverse(c.begin(), c.end());
		std::stable_sort(c.begin(), c.end(), [&](const Lugar& a, const Lugar& b) {
			const bool pa = a.lado == lado, pb = b.lado == lado;
			return pa != pb && (propiosPrimero ? pa : pb);
		});
		for (int k = 0; k < df->nObj && k < (int)c.size(); k++) j.objs.push_back(c[(size_t)k]);
	}
	for (int k = 0; k < (int)d.l[(size_t)lado].mano.size() && (int)j.descartes.size() < df->descartes; k++)
		if (!(f.donde == EN_MANO && f.i == k)) j.descartes.push_back(k);
	j.costes = costesIA(d, lado, *df, f);
}

// resuelve todo lo pendiente: pasa, elige como la IA y, si al lado de la prueba se le ofrece activar la carta (un
// disparo opcional, la respuesta), la activa (una vez)
static void resolverTodo(Escena& e, int lado, const std::string& id, int variante, bool& activada)
{
	Duelo& d = e.d;
	int usos = 0;
	for (int g = 0; g < 80 && d.ganador < 0; g++) {
		if (d.ef.preg.tipo == P_NADA) {
			if (!d.ef.disparos.empty() && d.ef.cadena.empty() && !d.ef.resolviendo) {
				const size_t n = d.ef.disparos.size();
				motorVentana(d, Ventana{ V_DISPARO, d.activo, -1, 0, {} });
				if (d.ef.preg.tipo == P_NADA && d.ef.disparos.size() == n) break;
				continue;
			}
			break;
		}
		if (d.ef.preg.tipo == P_ELEGIR) {
			const int r = elegirIA(d, d.ef.preg.lado);
			if (r < 0 && d.ef.preg.opcional) {
				Jugada j;
				j.tipo = J_PASAR;
				if (!d.aplicar(j)) break;
			} else if (!e.elige(d.ef.preg.lado, std::max(0, r))) {
				break;
			}
			continue;
		}
		// P_RESPUESTA
		if (d.ef.preg.lado == lado && usos < 1) {
			bool hecho = false;
			for (const Lugar& f : activables(d, lado, true)) {
				if (cartaEn(d, f) != id) continue;
				Jugada j;
				armarJugada(d, lado, f, variante, j);
				if (d.aplicar(j)) {
					hecho = true;
					activada = true;
					usos++;
					break;
				}
			}
			if (hecho) continue;
		}
		Jugada j;
		j.tipo = J_PASAR;
		if (!d.aplicar(j)) break;
	}
}

// ---- lo observado: sucesos y diferencia del duelo
struct Observado {
	Fams fam{};
	std::vector<int> dannosRival, ganancias;   // S_DANNO al rival / S_LP propios
	int robos{ 0 };
	std::string detalle;
	bool algo{ false };
};
static std::multiset<std::string> conjunto(const std::vector<std::string>& v) { return std::multiset<std::string>(v.begin(), v.end()); }
static std::vector<std::string> resta(const std::multiset<std::string>& a, std::multiset<std::string> b)   // a - b
{
	std::vector<std::string> r;
	for (const std::string& s : a) {
		auto it = b.find(s);
		if (it != b.end()) b.erase(it);
		else r.push_back(s);
	}
	return r;
}
struct EnCampo { std::string id; int lado, zona; bool monstruo, bocaAbajo, defensa; int atk, def, eq; bool token; };
static std::map<int, EnCampo> campoPorUid(const Duelo& d)
{
	std::map<int, EnCampo> r;
	for (int lado = 0; lado < 2; lado++) {
		const Lado& x = d.l[(size_t)lado];
		for (int i = 0; i < ZONAS; i++) {
			const Monstruo& m = x.m[(size_t)i];
			if (!m.vacia()) r[m.uid] = { m.id, lado, i, true, m.bocaAbajo, m.defensa, m.atkEf, m.defEf, 0, m.token };
			const MagiaTrampa& c = x.mt[(size_t)i];
			if (!c.vacia()) r[c.uid] = { c.id, lado, i, false, c.bocaAbajo, false, 0, 0, c.eqUid, false };
		}
		if (!x.campo.vacia()) r[x.campo.uid] = { x.campo.id, lado, ZONA_CAMPO, false, x.campo.bocaAbajo, false, 0, 0, 0, false };
	}
	return r;
}
// ignorar: cartas que dejan el campo por el contexto (el blanco del ataque, los sacrificios); sinCombate: el danno
// de batalla del contexto no cuenta
static Observado observar(const Duelo& a, const Duelo& b, int lado, const std::string& id, size_t desdeSuceso, size_t hastaSuceso,
                          const std::multiset<std::string>& ignorar = {}, bool sinCombate = false, bool invocaPropia = false)
{
	Observado o;
	const int otro = 1 - lado;
	bool enCombate = false, conDanno = false;
	for (size_t k = desdeSuceso; k < hastaSuceso && k < b.sucesos.size(); k++) {
		const Suceso& s = b.sucesos[k];
		if (s.tipo == S_COMBATE) {
			enCombate = true;
			conDanno = true;
			continue;
		}
		if (enCombate && s.tipo != S_DANNO && s.tipo != S_AL_CEMENTERIO && s.tipo != S_VOLTEA && s.tipo != S_MODIFICA && s.tipo != S_FIN) enCombate = false;
		if (s.tipo == S_DANNO) conDanno = true;
		if (enCombate && sinCombate) continue;
		switch (s.tipo) {
		case S_DANNO:
			if (s.lado == otro) {
				o.fam[F_QUEMA] = true;
				o.dannosRival.push_back(s.valor);
			} else {
				o.fam[F_PIERDE_LP] = true;
			}
			break;
		case S_LP:
			if (s.lado == lado && s.valor > 0) {
				o.fam[F_GANA_LP] = true;
				o.ganancias.push_back(s.valor);
			}
			break;
		case S_ROBO:
			if (s.lado == lado) o.fam[F_ROBA] = true;
			break;
		case S_NEGADO:
		case S_ATAQUE_NEGADO: o.fam[F_NIEGA] = true; break;
		case S_CONTROL: o.fam[F_CONTROL] = true; break;
		case S_DESTERRADO:
			if (s.valor != 2) o.fam[F_DESTIERRA] = true;
			break;
		case S_DESCARTA: o.fam[F_DESCARTA] = true; break;
		case S_POSICION:
		case S_VOLTEA:
			if (s.carta != id) o.fam[F_POSICION] = true;
			break;
		case S_MODIFICA:
			if (s.zona >= 0 && !s.magia) o.fam[F_ATKDEF] = true;
			break;
		default: break;
		}
	}
	// LP sin suceso
	if (b.l[(size_t)otro].lp < a.l[(size_t)otro].lp && !conDanno) o.fam[F_QUEMA] = true;
	if (b.l[(size_t)lado].lp > a.l[(size_t)lado].lp) o.fam[F_GANA_LP] = true;
	if (b.l[(size_t)lado].lp < a.l[(size_t)lado].lp) o.fam[F_PIERDE_LP] = true;
	// el campo
	const auto ca = campoPorUid(a), cb = campoPorUid(b);
	std::multiset<std::string> salieron, ign = ignorar;
	for (const auto& kv : ca) {
		auto it = cb.find(kv.first);
		if (it == cb.end()) {
			if (kv.second.id != id) {
				auto q = ign.find(kv.second.id);
				if (q != ign.end()) ign.erase(q);
				else salieron.insert(kv.second.id);
			}
			continue;
		}
		const EnCampo &x = kv.second, &y = it->second;
		if (x.id == id) {   // la propia carta: su ATK/DEF (se sube a si misma), boca arriba antes y despues
			if (x.monstruo && !x.bocaAbajo && !y.bocaAbajo && (x.atk != y.atk || x.def != y.def)) o.fam[F_ATKDEF] = true;
			continue;
		}
		if (x.lado != y.lado) o.fam[F_CONTROL] = true;
		if (x.monstruo && (x.bocaAbajo != y.bocaAbajo || x.defensa != y.defensa)) o.fam[F_POSICION] = true;
		if (x.monstruo && (x.atk != y.atk || x.def != y.def)) o.fam[F_ATKDEF] = true;
		if (!x.monstruo && x.bocaAbajo != y.bocaAbajo) o.fam[F_POSICION] = true;
		if (!x.monstruo && y.eq && !x.eq) o.fam[F_EQUIPA] = true;
	}
	for (const auto& kv : cb) {
		if (ca.count(kv.first)) continue;
		const EnCampo& y = kv.second;
		if (y.monstruo && y.id != id) {
			o.fam[F_INVOCA] = true;   // (tambien el monstruo ritual que invoca la magia)
			if (y.token) o.fam[F_FICHA] = true;
		}
		if (y.monstruo && y.id == id && y.token) o.fam[F_FICHA] = true;
		if (y.monstruo && y.id == id && invocaPropia) o.fam[F_INVOCA] = true;
		if (!y.monstruo && y.eq) o.fam[F_EQUIPA] = true;   // (la propia carta de equipo)
		if (y.monstruo && y.atk != 0 && false) o.fam[F_ATKDEF] = true;
	}
	// a donde fueron las cartas (las que dejaron el campo, la mano, el cementerio, el mazo)
	for (int ld = 0; ld < 2; ld++) {
		const Lado &x = a.l[(size_t)ld], &y = b.l[(size_t)ld];
		auto manoA = conjunto(x.mano), manoB = conjunto(y.mano), cemA = conjunto(x.cementerio), cemB = conjunto(y.cementerio);
		auto desA = conjunto(x.desterrados), desB = conjunto(y.desterrados);
		const std::vector<std::string> aMano = resta(manoB, manoA), deMano = resta(manoA, manoB);
		const std::vector<std::string> aCem = resta(cemB, cemA), deCem = resta(cemA, cemB);
		const std::vector<std::string> aDes = resta(desB, desA);
		int robosLado = 0;   // (todos: tambien el robo del turno siguiente que queda fuera del rango)
		for (size_t k = desdeSuceso; k < b.sucesos.size(); k++) robosLado += b.sucesos[k].tipo == S_ROBO && b.sucesos[k].lado == ld;
		std::multiset<std::string> sal = salieron;
		auto quita = [](std::multiset<std::string>& m, const std::string& s) {
			auto it = m.find(s);
			if (it == m.end()) return false;
			m.erase(it);
			return true;
		};
		std::multiset<std::string> deCemS(deCem.begin(), deCem.end()), deManoS(deMano.begin(), deMano.end());
		int entraMano = 0;
		for (const std::string& s : aMano) {
			if (quita(deCemS, s)) o.fam[F_RECUPERA] = true;
			else if (quita(sal, s)) o.fam[F_A_LA_MANO] = true;
			else entraMano++;
		}
		if (entraMano > robosLado && y.mazo.size() < x.mazo.size()) o.fam[F_BUSCA] = true;
		std::multiset<std::string> ignCem = ignorar;
		for (const std::string& s : aCem) {
			if (s == id) continue;
			{
				auto q = ignCem.find(s);
				if (q != ignCem.end()) {
					ignCem.erase(q);
					continue;
				}
			}
			if (quita(sal, s)) o.fam[F_DESTRUYE] = true;
			else if (quita(deManoS, s)) o.fam[F_DESCARTA] = true;
			else if (y.mazo.size() < x.mazo.size()) o.fam[F_DESCARTA] = true;   // (del mazo al cementerio: lo cuenta como enviar)
		}
		if (!aDes.empty()) o.fam[F_DESTIERRA] = true;
		if (y.mazo.size() > x.mazo.size()) o.fam[F_AL_MAZO] = true;
		if (ld == lado && y.mazo.size() == x.mazo.size() && y.mazo != x.mazo) o.fam[F_AL_MAZO] = true;   // (puso una carta encima / ordeno)
		(void)0;   // (robar: solo por S_ROBO del lado, en el rango)
	}
	if (!salieron.empty()) {
		// las que salieron del campo y no aparecen en ningun lado conocido (fichas): destruidas
		o.algo = true;
	}
	for (int k = 0; k < NUM_FAM; k++) o.algo |= o.fam[(size_t)k] && k != F_PIERDE_LP;
	o.robos = 0;
	for (size_t k = desdeSuceso; k < hastaSuceso && k < b.sucesos.size(); k++) o.robos += b.sucesos[k].tipo == S_ROBO && b.sucesos[k].lado == lado;
	return o;
}

// ---- un intento: arma el contexto, activa / dispara, resuelve y observa
struct Intento {
	bool activada{ false }, resuelta{ false };
	Observado obs;
	ResultadoFront front;
	std::string motivo;   // por que no se activo
	std::string revertir; // fallo de duracion (no se deshizo al final del turno)
	bool frontProbado{ false };
};

static int duracionesProbadas = 0, duracionesOk = 0;   // (comprobaciones de "hasta el final del turno")
static int zonaLibreM(const Duelo& d, int lado)
{
	return d.l[(size_t)lado].libre();
}

static Intento intentar(const std::string& id, Contexto k, int variante, const Expectativa& ex)
{
	Intento r;
	const Carta* c = base.carta(id);
	const DefCarta* df = defCarta(id);
	if (!c) {
		r.motivo = "sin datos";
		return r;
	}
	const bool sinPropios = k == K_ATAQUE_DIRECTO_RIVAL;
	const bool sinRival = k == K_ATACA_DIRECTO;
	Escena e(F_PRINCIPAL1);
	armar(e, sinPropios, sinRival);
	Duelo& d = e.d;
	Lugar fuente{ 0, EN_MANO, -1 };
	auto aMano = [&]() {
		d.l[0].mano.push_back(id);
		return (int)d.l[0].mano.size() - 1;
	};
	// preparar el momento
	size_t marca = 0;   // desde que suceso se mide
	std::multiset<std::string> ignorar;
	bool sinCombate = false;
	int pasivoZona = -1;
	Duelo antes{ &base };
	bool listo = false;   // el momento en que se activa a mano (Jugada)
	auto foto = [&]() {
		antes = d;
		marca = d.sucesos.size();
	};
	switch (k) {
	case K_MANO: {
		const int h = aMano();
		fuente = { 0, EN_MANO, h };
		listo = true;
		break;
	}
	case K_COLOCADA: {
		const int z = e.colocar(0, id.c_str(), 1);
		fuente = { 0, EN_MT, z };
		listo = true;
		break;
	}
	case K_IGNICION: {
		const int z = e.mon(0, id.c_str());
		if (z < 0) {
			r.motivo = "no entra al campo";
			return r;
		}
		if (esGeminis(e.d, id)) e.d.l[0].m[(size_t)z].geminado = true;   // (rama classic-torneo-extras: un Geminis ya invocado de nuevo, con su efecto)
		fuente = { 0, EN_MONSTRUO, z };
		listo = true;
		break;
	}
	case K_PASIVO: {   // llega boca arriba al campo: lo que cambia con solo estar
		foto();
		const int z = e.mon(0, id.c_str());
		if (z < 0) {
			r.motivo = "no entra al campo";
			return r;
		}
		r.activada = true;
		pasivoZona = z;
		break;
	}
	default: break;
	}
	if (k == K_PASIVO) {
		// (nada que activar: ya esta en el campo)
	} else if (listo) {
		foto();
		Jugada j;
		armarJugada(d, 0, fuente, variante, j);
		const std::string mot = motivoNoActivar(d, 0, fuente, false);
		if (!mot.empty() || !d.aplicar(j)) {
			r.motivo = mot.empty() ? "aplicar rechazo la activacion" : mot;
			return r;
		}
		r.activada = true;
		resolverTodo(e, 0, id, variante, r.activada);
	} else {
		switch (k) {
		case K_VOLTEO: {
			const int z = e.mon(0, id.c_str(), true, true);
			if (z < 0) {
				r.motivo = "no entra al campo";
				return r;
			}
			foto();
			Jugada j;
			j.tipo = J_VOLTEAR;
			j.zona = z;
			if (!d.aplicar(j)) {
				r.motivo = "no se pudo voltear";
				return r;
			}
			resolverTodo(e, 0, id, variante, r.activada);
			break;
		}
		case K_INVOCACION: {
			const int h = aMano();
			Jugada j;
			j.tipo = J_INVOCAR;
			j.mano = h;
			const int nt = motorTributos(d, 0, c);
			// sacrificios: los monstruos propios (y si hace falta, mas relleno)
			while (d.l[0].monstruos() < nt && d.l[0].libre() >= 0) e.mon(0, "mystical_elf");
			for (int z = 0; z < ZONAS && (int)j.tributos.size() < nt; z++)
				if (!d.l[0].m[(size_t)z].vacia()) j.tributos.push_back(z);
			j.zona = -1;
			for (int z : j.tributos) ignorar.insert(d.l[0].m[(size_t)z].id);
			foto();
			if (!d.aplicar(j)) {
				r.motivo = "no se pudo invocar normal";
				return r;
			}
			resolverTodo(e, 0, id, variante, r.activada);
			break;
		}
		case K_ATAQUE_RIVAL:
		case K_ATAQUE_DIRECTO_RIVAL:
		case K_INVOCA_RIVAL:
		case K_ACTIVA_RIVAL:
		case K_FIN_RIVAL: {
			// la carta: colocada (trampa / rapida), en la mano (Kuriboh) o un monstruo con efecto rapido en el campo
			if (c->monstruo) {
				if (df && df->desdeMano) aMano();
				else if (e.mon(0, id.c_str()) < 0) {
					r.motivo = "no entra al campo";
					return r;
				}
			} else {
				e.colocar(0, id.c_str(), 1);
			}
			d.turno = 4;
			d.activo = 1;
			d.fase = k == K_ATAQUE_RIVAL || k == K_ATAQUE_DIRECTO_RIVAL ? F_BATALLA : k == K_FIN_RIVAL ? F_PRINCIPAL2 : F_PRINCIPAL1;
			Jugada j;
			if (k == K_ATAQUE_RIVAL || k == K_ATAQUE_DIRECTO_RIVAL) {
				j.tipo = J_ATACAR;
				j.zona = 0;   // Baby Dragon
				j.objetivo = k == K_ATAQUE_RIVAL ? 0 : -1;   // Celtic en defensa / directo
			} else if (k == K_INVOCA_RIVAL) {
				j.tipo = J_INVOCAR;
				j.mano = 0;   // Vorse Raider
				j.zona = -1;
			} else if (k == K_ACTIVA_RIVAL) {
				j = jugadaActivar({ 1, EN_MANO, 1 });   // Pot of Greed
			} else {
				j.tipo = J_FASE;   // a la fase final
			}
			if (!d.aplicar(j)) {
				r.motivo = "el rival no pudo hacer su jugada";
				return r;
			}
			foto();   // (lo que hizo el rival no cuenta: desde que el lado de la prueba puede responder)
			sinCombate = true;
			if (k == K_ACTIVA_RIVAL) ignorar.insert("pot_of_greed");
			resolverTodo(e, 0, id, variante, r.activada);
			break;
		}
		case K_RITUAL: {   // la magia ritual + su monstruo en la mano, los sacrificios del campo
			std::string magia = id, monstruo;
			if (c->magia) {   // (la magia: su monstruo, o uno de su atributo si es generica)
				for (const Carta& x : base.cartas)
					if (monstruo.empty() && base.ritualCon(x.id, id)) monstruo = x.id;
			} else {          // (el monstruo ritual: su magia)
				monstruo = id;
				magia.clear();
				for (const Carta& x : base.cartas)
					if (magia.empty() && x.subtipo == "magia_ritual" && base.ritualCon(id, x.id)) magia = x.id;
			}
			if (monstruo.empty() || !base.carta(magia)) {
				r.motivo = "sin pareja ritual (magia / monstruo)";
				return r;
			}
			Jugada j;
			j.tipo = J_RITUAL;
			d.l[0].mano.push_back(magia);
			j.mano = (int)d.l[0].mano.size() - 1;
			d.l[0].mano.push_back(monstruo);
			j.mano2 = (int)d.l[0].mano.size() - 1;
			int suma = 0;
			for (int z = 0; z < ZONAS; z++)
				if (!d.l[0].m[(size_t)z].vacia()) suma += d.nivel(d.l[0].m[(size_t)z].id);
			while (suma < d.nivel(monstruo) && d.l[0].libre() >= 0) {
				e.mon(0, "summoned_skull");
				suma += 6;
			}
			for (int z = 0; z < ZONAS; z++)
				if (!d.l[0].m[(size_t)z].vacia()) {
					j.tributos.push_back(z);
					ignorar.insert(d.l[0].m[(size_t)z].id);
				}
			ignorar.insert(magia);
			foto();
			if (!d.aplicar(j)) {
				r.motivo = "no se pudo invocar por ritual";
				return r;
			}
			r.activada = true;
			resolverTodo(e, 0, id, variante, r.activada);
			break;
		}
		case K_TOON: {   // Toon World boca arriba; el Toon de la mano (con sus sacrificios); el turno siguiente ataca directo
			const int zt = e.colocar(0, "toon_world");
			d.l[0].mt[(size_t)zt].bocaAbajo = false;
			Jugada j;
			j.tipo = J_TOON;
			j.mano = aMano();
			const int nt = motorTributos(d, 0, c);
			while (d.l[0].monstruos() < nt && d.l[0].libre() >= 0) e.mon(0, "mystical_elf");
			for (int z = 0; z < ZONAS && (int)j.tributos.size() < nt; z++)
				if (!d.l[0].m[(size_t)z].vacia()) {
					j.tributos.push_back(z);
					ignorar.insert(d.l[0].m[(size_t)z].id);
				}
			foto();
			if (!d.aplicar(j)) {
				r.motivo = "no se pudo invocar con Toon World";
				return r;
			}
			r.activada = true;
			resolverTodo(e, 0, id, variante, r.activada);
			int zToon = -1;
			for (int z = 0; z < ZONAS; z++)
				if (d.l[0].m[(size_t)z].id == id) zToon = z;
			if (zToon >= 0) {
				d.l[0].m[(size_t)zToon].llegoEsteTurno = false;   // (como si fuera el turno siguiente)
				d.fase = F_BATALLA;
				Jugada a;
				a.tipo = J_ATACAR;
				a.zona = zToon;
				a.objetivo = -1;
				if (d.aplicar(a)) resolverTodo(e, 0, id, variante, r.activada);
				else r.motivo = "el Toon no pudo atacar directo";
			}
			break;
		}
		case K_FUSION: {   // Polymerization: Flame Swordsman con sus dos materiales de la mano
			d.l[0].extra = { "flame_swordsman" };
			Jugada j;
			j.tipo = J_FUSION;
			j.mano = aMano();
			j.carta = "flame_swordsman";
			d.l[0].mano.push_back("flame_manipulator");
			j.manoExtra.push_back((int)d.l[0].mano.size() - 1);
			d.l[0].mano.push_back("masaki_the_legendary_swordsman");
			j.manoExtra.push_back((int)d.l[0].mano.size() - 1);
			ignorar.insert("flame_manipulator");
			ignorar.insert("masaki_the_legendary_swordsman");
			foto();
			if (!d.aplicar(j)) {
				r.motivo = "no se pudo invocar por fusion";
				return r;
			}
			r.activada = true;
			resolverTodo(e, 0, id, variante, r.activada);
			break;
		}
		case K_DESTRUIDO_BATALLA: {
			const int z = e.mon(0, id.c_str());
			if (z < 0) {
				r.motivo = "no entra al campo";
				return r;
			}
			const int zb = e.mon(1, "blue_eyes");
			d.turno = 4;
			d.activo = 1;
			d.fase = F_BATALLA;
			foto();
			sinCombate = true;
			Jugada j;
			j.tipo = J_ATACAR;
			j.zona = zb;
			j.objetivo = z;
			if (zb < 0 || !d.aplicar(j)) {
				r.motivo = "no se pudo atacar";
				return r;
			}
			resolverTodo(e, 0, id, variante, r.activada);
			break;
		}
		case K_ATACA:
		case K_ATACA_DIRECTO: {
			const int z = e.mon(0, id.c_str());
			if (z < 0) {
				r.motivo = "no entra al campo";
				return r;
			}
			d.fase = F_BATALLA;
			if (k == K_ATACA) {   // un blanco debil boca arriba en ataque
				for (int i = 0; i < ZONAS; i++) d.l[1].m[(size_t)i] = Monstruo{};
				e.mon(1, "mokey_mokey");   // 300/100
				ignorar.insert("mokey_mokey");
			}
			foto();
			sinCombate = true;
			Jugada j;
			j.tipo = J_ATACAR;
			j.zona = z;
			j.objetivo = k == K_ATACA ? 0 : -1;
			if (!d.aplicar(j)) {
				r.motivo = "no pudo atacar";
				return r;
			}
			resolverTodo(e, 0, id, variante, r.activada);
			break;
		}
		default: break;
		}
		// el disparo o la activacion se vio? (ritual / Toon / fusion: la jugada misma)
		for (size_t q = marca; q < d.sucesos.size() && !(k == K_RITUAL || k == K_TOON || k == K_FUSION); q++) {
			const Suceso& s = d.sucesos[q];
			if ((s.tipo == S_ACTIVA_EFECTO || s.tipo == S_ACTIVA_MT) && s.carta == id && s.lado == 0) r.activada = true;
		}
		if (!r.activada) {
			r.motivo = "no se activo ni se disparo";
			return r;
		}
	}
	r.resuelta = d.ef.cadena.empty() && d.ef.preg.tipo == P_NADA;
	size_t hasta = d.sucesos.size();
	if (k == K_FIN_RIVAL || k == K_DESTRUIDO_BATALLA)   // (hasta que empieza el turno siguiente: su robo no es el efecto)
		for (size_t q = marca; q < d.sucesos.size(); q++)
			if (d.sucesos[q].tipo == S_TURNO) {
				hasta = q;
				break;
			}
	r.obs = observar(antes, d, 0, id, marca, hasta, ignorar, sinCombate, k == K_RITUAL || k == K_TOON || k == K_FUSION);
	if (pasivoZona >= 0) {   // su propio ATK/DEF distinto del impreso (se sube por algo del campo / cementerio)
		const Monstruo& m = d.l[0].m[(size_t)pasivoZona];
		if (m.id == id && (m.atkEf != c->atk || m.defEf != c->def)) {
			r.obs.fam[F_ATKDEF] = true;
			r.obs.algo = true;
		}
	}
	// el front: lo que paso desde la foto
	r.front = verificarFront(d, &antes);
	r.frontProbado = true;
	// duracion: lo que dura "hasta el final del turno" se deshace al pasar el turno (ATK/DEF y control)
	if (((ex.atkHastaFin && r.obs.fam[F_ATKDEF]) || (ex.controlHastaFin && r.obs.fam[F_CONTROL])) && d.ganador < 0 && r.resuelta) {
		const auto ca = campoPorUid(antes);
		const auto cb0 = campoPorUid(d);
		std::vector<int> tocados, prestados;
		for (const auto& kv : ca) {
			auto it = cb0.find(kv.first);
			if (it == cb0.end() || !kv.second.monstruo) continue;
			if (ex.atkHastaFin && r.obs.fam[F_ATKDEF] && (kv.second.atk != it->second.atk || kv.second.def != it->second.def)) tocados.push_back(kv.first);
			if (ex.controlHastaFin && r.obs.fam[F_CONTROL] && kv.second.lado != it->second.lado) prestados.push_back(kv.first);
		}
		if (!tocados.empty() || !prestados.empty()) {
			const int turnoAhora = d.turno;
			Jugada f;
			f.tipo = J_FASE;
			bool act = false;
			for (int g = 0; g < 14 && d.turno == turnoAhora && d.ganador < 0; g++) {
				if (!d.aplicar(f)) resolverTodo(e, 0, "", 0, act);
				resolverTodo(e, 0, "", 0, act);
			}
			duracionesProbadas++;
			const auto cb = campoPorUid(d);
			for (int u : tocados) {
				auto ia_ = ca.find(u);
				auto ib = cb.find(u);
				if (ib == cb.end() || ia_ == ca.end()) continue;
				if (ib->second.atk != ia_->second.atk || ib->second.def != ia_->second.def) {
					r.revertir = fmt("%s quedo con %d/%d tras el fin del turno (antes %d/%d)", ib->second.id.c_str(), ib->second.atk, ib->second.def, ia_->second.atk,
					                 ia_->second.def);
					break;
				}
			}
			for (int u : prestados) {
				auto ia_ = ca.find(u);
				auto ib = cb.find(u);
				if (ib == cb.end() || ia_ == ca.end()) continue;
				if (ib->second.lado != ia_->second.lado) {
					r.revertir = fmt("%s sigue en el campo del lado %d tras el fin del turno", ib->second.id.c_str(), ib->second.lado);
					break;
				}
			}
			if (r.revertir.empty()) duracionesOk++;
		}
	}
	return r;
}

// ================================================================================================= resultados por carta
struct ResultadoCarta {
	std::string id;
	std::string tipo;          // monstruo / magia / trampa (subtipo)
	Expectativa ex;
	bool conDef{ false };      // tiene DefCarta (efecto en el motor)
	// casos a mano
	int casosOk{ 0 }, casosMal{ 0 };
	std::string casoMal;
	// plantilla
	std::string plantilla{ "SIN PRUEBA" };   // OK / FALLA / SIN PRUEBA
	std::string contexto, esperado, obtenido, motivoSinPrueba;
	// front
	int frontOk{ 0 }, frontMal{ 0 };
	std::string frontFalla;
	// duracion
	std::string duracion;
	std::string estado, detalle;
	std::string revision;   // anotacion a mano (expectativas ajustadas / dudosas)
	bool noImplementado{ false }, noConcluyente{ false };
};
static std::map<std::string, ResultadoCarta> resultados;


// ---- cartas que los casos a mano USAN de verdad (en su escenario: mano, campo, activar...). Muchos casos marcan
// cartas en una lista (for (id : {...}) probadas[id]++) sin armarles nada: esas NO cuentan como certificadas por el
// caso. Se lee el fuente de prueba_efectos.cpp (CERT_CASOS) y se quitan esas listas.
static std::set<std::string> usadasEnCasos, soloMarcadas;
static bool leerUsadas(const std::string& ruta)
{
	std::ifstream f(ruta);
	if (!f) return false;
	std::stringstream b;
	b << f.rdbuf();
	std::string s = b.str();
	s = std::regex_replace(s, std::regex("for \\(const char\\* id : \\{[^}]*\\}\\)\\s*probadas\\[id\\]\\+\\+;"), "");
	s = std::regex_replace(s, std::regex("probadas\\[\"[a-z0-9_]+\"\\]\\+\\+;"), "");
	std::regex r("\"([a-z0-9_]+)\"");
	for (auto it = std::sregex_iterator(s.begin(), s.end(), r); it != std::sregex_iterator(); ++it) usadasEnCasos.insert((*it)[1].str());
	return true;
}

// ---- ganchos de los casos a mano: el caso marca sus cartas (caso() y probadas[...]++), su resultado (fin()) y
// cada escena que se deshace pasa por el front (antes o despues de fin(): se guarda hasta saber las cartas)
static std::map<std::string, int> probadasAntes;
static std::vector<std::string> cartasCasoActual;
static std::string nombreCasoActual;
static bool finVisto = false;
static std::vector<ResultadoFront> pendientesFront;
static int escenasFront = 0, escenasFrontMal = 0;
static std::vector<std::string> fallasFrontEscenas;
static void anotarFront(const ResultadoFront& f)
{
	for (const std::string& id : cartasCasoActual) {
		ResultadoCarta& rc = resultados[id];
		rc.id = id;
		if (f.ok) rc.frontOk++;
		else {
			rc.frontMal++;
			if (rc.frontFalla.empty()) rc.frontFalla = "caso '" + nombreCasoActual + "': " + f.falla;
		}
	}
}
static void alCaso(const char* nombre, const char* carta)
{
	probadasAntes.clear();
	for (const auto& kv : probadas) probadasAntes[kv.first] = kv.second - (kv.first == carta ? 1 : 0);
	cartasCasoActual.clear();
	nombreCasoActual = nombre;
	finVisto = false;
	pendientesFront.clear();
}
static void alFin(bool ok)
{
	for (const auto& kv : probadas) {
		auto it = probadasAntes.find(kv.first);
		if (it == probadasAntes.end() || it->second < kv.second) cartasCasoActual.push_back(kv.first);
	}
	std::vector<std::string> usadas;
	for (const std::string& id : cartasCasoActual)
		if (usadasEnCasos.count(id)) usadas.push_back(id);
		else soloMarcadas.insert(id);
	cartasCasoActual = usadas;   // (las solo marcadas en una lista no reciben ni el resultado ni el front del caso)
	for (const std::string& id : cartasCasoActual) {
		ResultadoCarta& rc = resultados[id];
		rc.id = id;
		if (ok) rc.casosOk++;
		else {
			rc.casosMal++;
			if (rc.casoMal.empty()) rc.casoMal = nombreCasoActual;
		}
	}
	finVisto = true;
	for (const ResultadoFront& f : pendientesFront) anotarFront(f);
	pendientesFront.clear();
}
static void alEscena(const Duelo& d)
{
	escenasFront++;
	const ResultadoFront f = verificarFront(d, nullptr);
	if (!f.ok) {
		escenasFrontMal++;
		if (fallasFrontEscenas.size() < 400) fallasFrontEscenas.push_back(nombreCasoActual + ": " + f.falla);
	}
	if (finVisto) anotarFront(f);
	else pendientesFront.push_back(f);
}

// ================================================================================================= las plantillas
static std::vector<Contexto> contextosDe(const Carta& c, const DefCarta* df)
{
	std::vector<Contexto> r;
	const std::string& st = c.subtipo;
	if (st == "magia_ritual" || st == "ritual") r.push_back(K_RITUAL);
	if (st == "toon") r.push_back(K_TOON);
	if (c.id == "polymerization") r.push_back(K_FUSION);
	if (c.magia) {
		r.push_back(K_MANO);
		if (st == "magia_rapida") {
			r.push_back(K_COLOCADA);
			r.push_back(K_ATAQUE_RIVAL);
			r.push_back(K_INVOCA_RIVAL);
			r.push_back(K_FIN_RIVAL);
		}
		return r;
	}
	if (c.trampa) {
		for (Contexto k : { K_COLOCADA, K_ATAQUE_RIVAL, K_ATAQUE_DIRECTO_RIVAL, K_INVOCA_RIVAL, K_ACTIVA_RIVAL, K_FIN_RIVAL }) r.push_back(k);
		return r;
	}
	if (!df) return r;
	if (df->ignicion || df->sinLimite || df->repetible) r.push_back(K_IGNICION);

	if (df->ignicionMano || df->equipoDesdeMano) r.push_back(K_MANO);
	if (df->desdeMano) {
		r.push_back(K_ATAQUE_RIVAL);
		r.push_back(K_ATAQUE_DIRECTO_RIVAL);
	}
	if (df->rapidoMonstruo) {
		r.push_back(K_ATAQUE_RIVAL);
		r.push_back(K_INVOCA_RIVAL);
	}
	switch (df->disparo) {
	case D_VOLTEO:
	case D_VOLTEO_INVOCACION: r.push_back(K_VOLTEO); break;
	case D_INVOCADO:
	case D_NORMAL_O_VOLTEO:
	case D_NORMAL_O_VOLTEO_INV:
		r.push_back(K_INVOCACION);
		r.push_back(K_VOLTEO);
		break;
	case D_INVOCADO_NORMAL:
	case D_INVOCADO_SACRIFICIO: r.push_back(K_INVOCACION); break;
	case D_AL_CEMENTERIO:
	case D_A_CEMENTERIO:
	case D_DESTRUIDO_BATALLA: r.push_back(K_DESTRUIDO_BATALLA); break;
	case D_DESTRUYE_BATALLA:
	case D_DANNO_BATALLA: r.push_back(K_ATACA); break;
	case D_DANNO_DIRECTO: r.push_back(K_ATACA_DIRECTO); break;
	default: break;
	}
	return r;
}

static bool algunaFam(const Fams& f)
{
	for (bool b : f)
		if (b) return true;
	return false;
}
static bool cruza(const Fams& a, const Fams& b)
{
	for (int k = 0; k < NUM_FAM; k++)
		if (a[(size_t)k] && b[(size_t)k]) return true;
	return false;
}
static std::string numeros(const Observado& o)
{
	std::string r;
	for (int v : o.dannosRival) r += fmt(" danno %d", v);
	for (int v : o.ganancias) r += fmt(" +%d LP", v);
	if (o.robos) r += fmt(" roba %d", o.robos);
	return r;
}

// la plantilla de una carta: los contextos (y variantes de objetivos) hasta que lo observado cumple lo esperado
static void plantilla(ResultadoCarta& rc)
{
	const Carta* c = base.carta(rc.id);
	const DefCarta* df = defCarta(rc.id);
	if (!c) return;
	const Expectativa& ex = rc.ex;
	std::vector<Contexto> ks = contextosDe(*c, df);
	if (c->monstruo && df) ks.push_back(K_PASIVO);   // (todo monstruo: lo que hace con solo estar en el campo)
	if (ks.empty()) {
		rc.motivoSinPrueba = df ? "efecto continuo / de procedimiento: sin contexto de activacion (solo caso a mano)" : "sin efecto en el motor (por regla)";
		return;
	}
	bool algunaActivada = false, ok = false, algoVisto = false;
	int mejorNf = -1;
	std::string esperado = nombres(ex.fam), obtenido, contexto, motivo;
	for (int esc = 0; esc < 3 && !ok; esc++) {
	escenarioActual = esc;
	const std::string sufijo = esc ? std::string(" [escenario ") + NOMBRE_ESCENARIO[esc] + "]" : std::string();
	for (Contexto k : ks) {
		const int variantes = df && df->nObj > 0 ? 3 : 1;
		for (int v = 0; v < variantes && !ok; v++) {
			Intento in = intentar(rc.id, k, v, ex);
			if (in.frontProbado) {
				if (in.front.ok) rc.frontOk++;
				else {
					rc.frontMal++;
					if (rc.frontFalla.empty()) rc.frontFalla = std::string(NOMBRE_K[k]) + ": " + in.front.falla;
				}
			}
			if (!in.activada) {
				if (motivo.empty()) motivo = std::string(NOMBRE_K[k]) + ": " + in.motivo;
				continue;
			}
			algunaActivada = true;
			const Observado& o = in.obs;
			algoVisto |= o.algo;
			std::string obt = nombres(o.fam) + numeros(o);
			if (!in.resuelta) obt += " (la cadena quedo abierta)";
			bool cumple = algunaFam(ex.fam) ? cruza(ex.fam, o.fam) : o.algo;
			if (c->subtipo == "toon")   // (el Toon: llega, paga 500 al atacar y ataca directo)
				cumple = o.fam[F_INVOCA] && o.fam[F_QUEMA] && o.fam[F_PIERDE_LP] == (minus(textos[indiceTexto[rc.id]].texto).find("pay 500") != std::string::npos);
			// numeros del texto
			std::string numMal;
			if (cumple && ex.danno > 0 && c->subtipo != "toon" && o.fam[F_QUEMA] && !o.dannosRival.empty() &&
			    std::find(o.dannosRival.begin(), o.dannosRival.end(), ex.danno) == o.dannosRival.end())
				numMal = fmt("danno %d (esperado %d)", o.dannosRival[0], ex.danno);
			if (cumple && ex.ganaLP > 0 && o.fam[F_GANA_LP] && !o.ganancias.empty() &&
			    std::find(o.ganancias.begin(), o.ganancias.end(), ex.ganaLP) == o.ganancias.end())
				numMal = fmt("gana %d LP (esperado %d)", o.ganancias[0], ex.ganaLP);
			if (cumple && ex.roba > 0 && ex.fam[F_ROBA] && o.robos > 0 && o.robos != ex.roba && !ex.fam[F_DESCARTA])
				numMal = fmt("roba %d (esperado %d)", o.robos, ex.roba);
			if (!numMal.empty()) {
				cumple = false;
				obt += " [" + numMal + "]";
			}
			if (!in.revertir.empty()) rc.duracion = in.revertir;
			if (cumple) {
				ok = true;
				contexto = NOMBRE_K[k] + sufijo;
				obtenido = obt;
				break;
			}
			int nf = 0;
			for (int q = 0; q < NUM_FAM; q++) nf += o.fam[(size_t)q] && q != F_PIERDE_LP;
			if (obtenido.empty() || nf > mejorNf) {
				obtenido = obt;
				contexto = NOMBRE_K[k] + sufijo;
				mejorNf = nf;
			}
		}
		if (ok) break;
	}
	}
	escenarioActual = 0;
	rc.esperado = esperado;
	rc.obtenido = obtenido;
	rc.contexto = contexto;
	if (ok) rc.plantilla = "OK";
	else if (!algunaActivada) {
		rc.plantilla = "SIN PRUEBA";
		rc.motivoSinPrueba = "no se activo en ningun contexto armado (" + motivo + ")";
	} else if (!algunaFam(ex.fam)) {
		rc.plantilla = "SIN PRUEBA";
		rc.motivoSinPrueba = "se activa, pero el texto no tiene una expectativa medible y no se vio cambio";
	} else if (!algoVisto) {
		// se activo y no hizo nada visible en ningun contexto: la condicion de su efecto no se da en el escenario
		// generico (tipos, niveles, contadores...). No es prueba de un fallo: la certifica su caso a mano
		rc.plantilla = "SIN PRUEBA";
		rc.motivoSinPrueba = "no concluyente: se activa (" + contexto + ") pero su efecto no se ve en el escenario generico";
		rc.noConcluyente = true;
	} else {
		rc.plantilla = "FALLA";
	}
}

// ---- expectativas ajustadas a mano (revisadas una por una): la plantilla no puede ver el efecto en sus contextos
// (condiciones raras, efectos continuos, al azar...) o el texto engania al clasificador. Plantilla -> SIN PRUEBA con
// la nota; la carta queda certificada por sus casos a mano (prueba_efectos.cpp) si los tiene.
struct Ajuste { const char* id; const char* nota; };
static const Ajuste AJUSTES[] = {
	{ "harpie_lady_sisters", "sin efecto propio: solo se invoca con Elegant Egotist (la plantilla y el caso de esa magia)" },
	{ "hyena", "plantilla: invoca Hyenas del mazo y el mazo generico no tiene; solo se ve que baraja (caso a mano)" },
	{ "impenetrable_formation", "plantilla: queda vinculada al monstruo elegido (se ve como equipo); el +700 DEF pide 2+ monstruos todos en defensa (caso a mano)" },
	{ "insect_queen", "plantilla: para atacar sacrifica 1 monstruo (se ve como destruir); la ficha sale en la fase final (caso a mano)" },
};
// parciales: el efecto esta pero falta una parte clara del texto: FALLA con la nota (lo que falta)
static const Ajuste PARCIALES[] = {
	{ "", "" },   // (rama classic-motor2: las parciales se completaron segun su texto; ver tandaMotor2* en prueba_efectos.cpp)
};
// dudosas: se anotan en el reporte para revisarlas (no son un fallo claro)
static const Ajuste DUDOSAS[] = {
	{ "", "" },   // (rama classic-motor2: las dudosas se corrigieron segun su texto; ver tandaMotor2Ventanas en prueba_efectos.cpp)
};

// el texto de una fusion / un ritual dice algo mas que sus materiales y como se invoca?
static bool efectoEnTexto(std::string t)
{
	static const char* const QUITAR[] = {
		"^\"[^\"]*\"( \\+ \"[^\"]*\")+\\.?",
		"(This (card|monster) can only be Ritual Summoned with the Ritual Spell Card, |You can Ritual Summon this card with )\"[^\"]*\"\\.?",
		"\\(This card is always treated as an? \"[^\"]*\" card\\.\\)",
		"A Fusion Summon of this (card|monster) can only be (done|conducted) with the above Fusion Material( Monster)?s?\\.",
	};
	for (const char* q : QUITAR) t = std::regex_replace(t, std::regex(q), "");
	t = std::regex_replace(t, std::regex("^\\s+|\\s+$"), "");
	return t.size() > 15;
}
// sin DefCarta pero hechas por regla del motor (o sin efecto propio)
static bool porRegla(const std::string& id)
{
	return pagaToon(id) || id == "harpie_lady_sisters";
}
static bool estricto = false;   // CERT_ESTRICTO=1: lo no concluyente es FALLA y no hay expectativas ajustadas

// ================================================================================================= el reporte
static std::string grupoTipo(const std::string& st)
{
	if (st.rfind("magia", 0) == 0) return "Magia (" + st.substr(st.find('_') == std::string::npos ? st.size() : st.find('_') + 1) + ")";
	if (st.rfind("trampa", 0) == 0) return "Trampa (" + st.substr(st.find('_') + 1) + ")";
	if (st.empty()) return "Monstruo (?)";
	return "Monstruo (" + st + ")";
}
static std::string csv(std::string s)
{
	for (char& ch : s)
		if (ch == ';' || ch == '\n' || ch == '\r') ch = ',';
	return s;
}
static std::string md(std::string s)
{
	std::string r;
	for (char ch : s) {
		if (ch == '|') r += "\\|";
		else if (ch == '\n') r += ' ';
		else r += ch;
	}
	return r;
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_certificacion <carpeta ui/ (cartas_ui*.txt, cartas_classic.txt, rivales.txt...)> <carpeta docs/> [solo_id]\n");
		return 2;
	}
	const std::string ui = std::string(argv[1]) + "/";
	const std::string docs = std::string(argv[2]) + "/";
	const char* solo = argc > 3 ? argv[3] : nullptr;
	const auto t0 = std::chrono::steady_clock::now();
	estricto = getenv("CERT_ESTRICTO") != nullptr;
	juego.preparar(ui);
	if (!juego.baseIAlista) {
		fprintf(stderr, "sin datos del juego en %s\n", ui.c_str());
		return 2;
	}
	// 1. los casos a mano, cada escena por el front
	const char* rutaCasos = getenv("CERT_CASOS");
	if (!rutaCasos || !leerUsadas(rutaCasos)) {
		fprintf(stderr, "CERT_CASOS=<ruta de prueba_efectos.cpp>: hace falta para saber que cartas usa cada caso\n");
		return 2;
	}
	if (!solo) {
		ganchoCaso = alCaso;
		ganchoFin = alFin;
		ganchoEscena = alEscena;
		std::string uiArg = ui;
		char* av[] = { argv[0], &uiArg[0] };
		main_efectos(2, av);
		ganchoCaso = nullptr;
		ganchoFin = nullptr;
		ganchoEscena = nullptr;
		fprintf(stderr, "casos a mano: %d casos, %d fallos; escenas por el front: %d (%d con fallas)\n", casos, fallos, escenasFront, escenasFrontMal);
	} else if (!base.leerCartas(ui)) {
		return 2;
	}
	// (las recetas de ritual y de fusion y las lineas N van en rivales.txt: con los casos a mano ya las leyo su main)
	if (solo) {
		base.leerRivales(ui + "rivales.txt");
		base.leerRivales(ui + "personajes_rivales.txt");
	}
	if (!leerTextos(ui)) {
		fprintf(stderr, "sin cartas_ui_en.txt en %s\n", ui.c_str());
		return 2;
	}
	armarMazoVariado();
	// 2. las plantillas, carta por carta
	std::set<std::string> ajustadas, dudosas;
	std::map<std::string, std::string> notas;
	for (const Ajuste& a : AJUSTES)
		if (a.id[0]) ajustadas.insert(a.id), notas[a.id] = a.nota;
	for (const Ajuste& a : DUDOSAS)
		if (a.id[0]) dudosas.insert(a.id), notas[a.id] = a.nota;
	for (const Texto& t : textos) {
		if (solo && t.id != solo) continue;
		const Carta* c = base.carta(t.id);
		if (!c) continue;
		const DefCarta* df = defCarta(t.id);
		const bool textoConEfecto = (t.subtipo == "fusion" || t.subtipo == "ritual") && efectoEnTexto(t.texto);
		const bool tieneEfecto = df || c->magia || c->trampa || textoConEfecto || (c->monstruo && t.subtipo != "normal" && t.subtipo != "fusion" && t.subtipo != "ritual");
		if (!tieneEfecto) continue;
		ResultadoCarta& rc = resultados[t.id];
		rc.id = t.id;
		rc.tipo = grupoTipo(t.subtipo);
		rc.conDef = df != nullptr;
		rc.ex = expectativaDe(t);
		plantilla(rc);
		// un monstruo con efecto en su texto que el motor no tiene (ni DefCarta ni regla propia): FALLA, este lo que
		// este la plantilla (el Toon se invoca y ataca por regla, pero lo suyo no esta)
		if (c->monstruo && !df && !porRegla(t.id)) {
			rc.noImplementado = true;
			rc.revision = "el motor no tiene su efecto (sin DefCarta en tablaCartas() ni regla propia)";
		}
		if (ajustadas.count(t.id) && rc.plantilla != "OK" && !estricto) {
			rc.plantilla = "SIN PRUEBA";
			rc.motivoSinPrueba = notas[t.id];
			rc.revision = notas[t.id];
		}
		if (dudosas.count(t.id)) rc.revision = notas[t.id];
		if (solo)
			fprintf(stderr, "%s: plantilla %s (%s) esperado %s obtenido %s; front %d ok %d mal %s; %s\n", t.id.c_str(), rc.plantilla.c_str(), rc.contexto.c_str(),
			        rc.esperado.c_str(), rc.obtenido.c_str(), rc.frontOk, rc.frontMal, rc.frontFalla.c_str(), rc.motivoSinPrueba.c_str());
	}
	// 3. el veredicto por carta
	struct Cuenta { int ok{ 0 }, falla{ 0 }, sin{ 0 }; };
	std::map<std::string, Cuenta> porTipo, porFam;
	Cuenta total;
	std::vector<const ResultadoCarta*> fallas, revisar;
	for (auto& kv : resultados) {
		ResultadoCarta& rc = kv.second;
		if (rc.tipo.empty()) continue;   // (marcada por un caso a mano pero sin efecto propio: no cuenta)
		std::vector<std::string> porque;
		if (rc.casosMal > 0) porque.push_back("caso a mano '" + rc.casoMal + "' falla");
		for (const Ajuste& a : PARCIALES)
			if (rc.id == a.id) porque.push_back(std::string("efecto parcial: ") + a.nota);
		if (rc.noImplementado) porque.push_back("efecto no implementado en el motor: " + md(textos[indiceTexto[rc.id]].texto).substr(0, 160));
		if (estricto && rc.noConcluyente) porque.push_back("(estricto) plantilla no concluyente: " + rc.motivoSinPrueba);
		if (rc.plantilla == "FALLA") porque.push_back("plantilla (" + rc.contexto + "): esperado " + rc.esperado + ", obtenido " + rc.obtenido);
		if (rc.frontMal > 0) porque.push_back("front: " + rc.frontFalla);
		if (!rc.duracion.empty()) porque.push_back("duracion: " + rc.duracion);
		if (!porque.empty() && !dudosas.count(rc.id)) {
			rc.estado = "FALLA";
			for (const std::string& p : porque) rc.detalle += (rc.detalle.empty() ? "" : "; ") + p;
		} else if (rc.casosOk > 0 || rc.plantilla == "OK") {
			rc.estado = "OK";
			rc.detalle = rc.casosOk > 0 && rc.plantilla == "OK" ? "caso a mano + plantilla" : rc.casosOk > 0 ? "caso a mano" : "plantilla";
		} else {
			rc.estado = "SIN PRUEBA";
			rc.detalle = rc.motivoSinPrueba;
			if (soloMarcadas.count(rc.id)) rc.detalle += "; un caso a mano la marca en su lista pero no la usa";
		}
		if (dudosas.count(rc.id)) {
			for (const std::string& p : porque) rc.detalle += "; dudosa: " + p;
			revisar.push_back(&rc);
		}
		std::string fam = "otra / continua";
		for (int k = 0; k < NUM_FAM; k++)
			if (rc.ex.fam[(size_t)k]) {
				fam = NOMBRE_FAM[k];
				break;
			}
		for (Cuenta* x : { &porTipo[rc.tipo], &porFam[fam], &total }) {
			if (rc.estado == "OK") x->ok++;
			else if (rc.estado == "FALLA") x->falla++;
			else x->sin++;
		}
		if (rc.estado == "FALLA") fallas.push_back(&rc);
	}
	int nPlantilla[3] = { 0, 0, 0 };
	for (const auto& kv : resultados)
		if (!kv.second.tipo.empty()) nPlantilla[kv.second.plantilla == "OK" ? 0 : kv.second.plantilla == "FALLA" ? 1 : 2]++;
	const double seg = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
	fprintf(stderr, "CERTIFICACION: %d cartas con efecto: OK %d, FALLA %d, SIN PRUEBA %d (%.0f s)\n", total.ok + total.falla + total.sin, total.ok, total.falla,
	        total.sin, seg);
	if (solo) return total.falla ? 1 : 0;
	// CSV
	{
		std::ofstream f(docs + "certificacion_efectos.csv");
		f << "id;nombre;tipo;subtipo;familias_esperadas;estado;casos_ok;casos_mal;plantilla;contexto;obtenido;front_ok;front_mal;detalle;motivo_plantilla\n";
		for (const auto& kv : resultados) {
			const ResultadoCarta& rc = kv.second;
			if (rc.tipo.empty()) continue;
			const Texto& t = textos[indiceTexto[rc.id]];
			f << rc.id << ';' << csv(t.nombre) << ';' << csv(rc.tipo) << ';' << t.subtipo << ';' << nombres(rc.ex.fam) << ';' << rc.estado << ';' << rc.casosOk << ';'
			  << rc.casosMal << ';' << rc.plantilla << ';' << csv(rc.contexto) << ';' << csv(rc.obtenido) << ';' << rc.frontOk << ';' << rc.frontMal << ';'
			  << csv(rc.detalle) << ';' << csv(rc.motivoSinPrueba) << '\n';
		}
	}
	// el reporte
	{
		std::ofstream f(docs + "certificacion_efectos.md");
		f << "# Certificacion de efectos (probador automatico)\n\n";
		f << "Generado por `src/pruebas/prueba_certificacion.cpp` (rama classic-probador; correr con `bash src/pruebas/certificar.sh`). "
		     "Una fila por carta en `certificacion_efectos.csv`.\n\n";
		f << "## Resumen\n\n";
		f << "**" << total.ok + total.falla + total.sin << " cartas con efecto** (de " << textos.size() << "): **OK " << total.ok << "**, **FALLA " << total.falla
		  << "**, **SIN PRUEBA " << total.sin << "**.\n\n";
		f << "- Casos a mano (prueba_efectos.cpp): " << casos << " casos, " << fallos << " fallos. Solo cuentan para una carta si el caso la usa "
		     "en su escenario: " << soloMarcadas.size() << " cartas aparecen unicamente en listas `probadas[id]++` de un caso sin que el caso las "
		     "use, y para esas el caso no cuenta.\n";
		{
			int ambos = 0, soloCaso = 0, soloPlantilla = 0;
			for (const auto& kv : resultados) {
				const ResultadoCarta& rc = kv.second;
				if (rc.tipo.empty() || rc.estado != "OK") continue;
				if (rc.casosOk > 0 && rc.plantilla == "OK") ambos++;
				else if (rc.casosOk > 0) soloCaso++;
				else soloPlantilla++;
			}
			f << "- De las OK: " << ambos << " por caso a mano y plantilla, " << soloCaso << " solo por su caso a mano (la plantilla no pudo verlas en "
			  << "el escenario generico), " << soloPlantilla << " solo por la plantilla.\n";
		}
		f << "- Escenas de los casos presentadas en el front desde la mesa vacia: " << escenasFront << " (" << escenasFrontMal << " con fallas).\n";
		f << "- Duracion (\"hasta el final del turno\": ATK/DEF y control vuelven al pasar el turno): " << duracionesProbadas << " comprobadas, " << duracionesOk << " bien.\n";
		f << "- Tiempo: " << (int)seg << " s.\n\n";
		f << "Como se certifica cada carta:\n\n"
		     "1. **Caso a mano**: escenario armado para esa carta con lo esperado segun su texto (prueba_efectos.cpp).\n"
		     "2. **Plantilla**: la expectativa sale del texto (familias: destruir, quemar, ganar LP, robar, buscar, recuperar, invocar, desterrar, "
		     "control, negar, equipar, posicion, ATK/DEF, descartar, a la mano, al mazo, ficha) y se prueba en contextos armados (de la mano, "
		     "colocada, en respuesta a ataque / invocacion / activacion / fase final del rival, volteo, invocacion, destruido en batalla, al atacar). "
		     "OK si lo observado (sucesos del duelo + diferencia antes/despues) cumple la familia esperada y los numeros del texto (danno, LP, robos); "
		     "lo de \"hasta el final del turno\" se comprueba pasando el turno.\n"
		     "3. **Front**: cada activacion de la plantilla y cada escena de los casos se presenta con duelo_juego.h: la animacion de "
		     "efectos_cartas se encola y termina, la mesa queda igual a ia::Duelo (cartas, boca arriba/abajo, posicion, zona de campo), los LP "
		     "mostrados son los reales, las manos coinciden, los robos pasan por su animacion y nada queda bloqueado.\n\n"
		     "Estado: **FALLA** si falla cualquiera de las tres; **OK** si lo certifica un caso a mano o la plantilla (y nada falla); **SIN PRUEBA** "
		     "si ninguna de las dos pudo comprobar la carta.\n\n";
		f << "## Por tipo de carta\n\n| tipo | OK | FALLA | SIN PRUEBA |\n|---|---:|---:|---:|\n";
		for (const auto& kv : porTipo) f << "| " << kv.first << " | " << kv.second.ok << " | " << kv.second.falla << " | " << kv.second.sin << " |\n";
		f << "| **total** | **" << total.ok << "** | **" << total.falla << "** | **" << total.sin << "** |\n\n";
		f << "## Por tipo de efecto (familia principal del texto)\n\n| familia | OK | FALLA | SIN PRUEBA |\n|---|---:|---:|---:|\n";
		for (const auto& kv : porFam) f << "| " << kv.first << " | " << kv.second.ok << " | " << kv.second.falla << " | " << kv.second.sin << " |\n";
		f << "## Arreglos hechos (lo que el probador encontro)\n\n"
		     "| donde | falla | arreglo |\n|---|---|---|\n"
		     "| front: `duelo_juego.h` (presentarSucesos) | LP mostrados de menos cuando en la misma jugada se resolvia una cadena y despues "
		     "habia combate (responder a un ataque con una trampa que no lo para: tras un ataque directo de 1200 se veia 5600 en vez de 6800). "
		     "`S_RESUELVE` ponia los LP finales y el ataque animado restaba su danno otra vez. Tambien un combate con dos dannos (Reflect Bounder) "
		     "dejaba los LP mostrados distintos de los reales. 51 cartas (plantilla) y 17 escenas de los casos a mano | `lpTras()`: cada suceso "
		     "muestra los LP de ese momento (los de ahora menos los `S_DANNO` / `S_LP` que siguen) y, con todo presentado, los LP mostrados se "
		     "igualan a los del duelo |\n"
		     "| motor: `ia.h` (J_RITUAL, la IA) y `control.h` (menu) | Contract with the Abyss y Earth Chant (magias rituales genericas: "
		     "cualquier ritual de OSCURIDAD / TIERRA) no tenian receta y no se podian usar nunca | `Base::ritualCon()`: la receta propia o "
		     "la generica por atributo, en el motor, en el menu del jugador y en la IA |\n"
		     "| logica de vista: `efectos_cartas.h` | la logica de las animaciones (receta, cola, avance) estaba atada a Vulkan (fx.h) | "
		     "separada en `efectos_logica.h` (efc::Logica, sin Vulkan ni glm); efc::Motor hereda de ella: el juego no cambia |\n"
		     "| motor: `ia.h` (Appropriate, Solemn Wishes, Silent Magician LV4) | \"each time ... draws a card(s)\" contaba por carta: con Pot "
		     "of Greed del rival Appropriate robaba 4 (no 2), Solemn Wishes daba 1000 (no 500), Silent Magician LV4 ponia 2 contadores | un robo de "
		     "varias cartas dentro de un eslabon cuenta una vez (`roboEnEslabon`, `apropiarPendiente`); casos nuevos en prueba_efectos.cpp |\n"
		     "| motor: `ia.h` (Soul Reversal, Royal Command) | buscaban monstruos de volteo por `clase == \"volteo\"`, pero la clase de "
		     "cartas_ui.txt es \"efecto\" (el volteo esta en el subtipo): Soul Reversal no se podia activar nunca y Royal Command no negaba "
		     "los efectos continuos de los de volteo | `subtipo == \"volteo\"`; caso nuevo |\n"
		     "| motor: `ia.h` (Ojama Delta Hurricane!!) | usaba la resolucion de Huge Revolution, que ademas descarta toda la mano del "
		     "rival | solo destruye las cartas del campo del rival; caso nuevo |\n"
		     "| motor: `ia.h` (Nutrient Z) | no se podia activar ante un ataque directo de 2000 o mas (solo contra un monstruo en ataque) "
		     "| tambien con ataque directo; caso nuevo |\n"
		     "| motor: `ia.h` (Servant of Catabolism) | estaba en la lista \"por reglas del motor\" sin la regla: no atacaba directo | "
		     "agregado a `motorAtaqueDirecto`; caso nuevo |\n"
		     "| motor: `ia.h` (Cross Counter) | usaba la resolucion de Destruction Punch: destruia al atacante pero no doblaba el danno "
		     "de batalla que recibe | `ef.cruzado`: el danno al atacante de ese combate es el doble; caso nuevo |\n"
		     "| motor: `ia.h` (Second Goblin, Inaba White Rabbit) | Second Goblin copiaba la Union de Y-Dragon Head pero `uneA` no lo "
		     "conocia: no se equipaba nunca; Inaba White Rabbit estaba en la lista \"por reglas\" sin su ataque directo | `uneA` (Giant Orc) y "
		     "`motorAtaqueDirecto`; casos nuevos |\n"
		     "| motor: `ia.h` (destruirMonstruos, ignoraMagia) | los monstruos \"no afectados por magias\" (Deepsea Warrior y Cannonball "
		     "Spear Shellfish con Umi, Silent Swordsman, The Agent of Force - Mars...) solo se protegian de ser objetivo: Raigeki o Dark Hole "
		     "los destruian igual; The Legendary Fisherman con Umi no estaba | `destruirMonstruos` salta a los que ignoran la magia que se "
		     "esta resolviendo; el Pescador en `ignoraMagia`; casos nuevos |\n"
		     "| motor: `ia.h` (Senri Eye) | cobraba 1000 LP en vez de 100 | 100; caso nuevo |\n"
		     "| motor: `ia.h` (The Dragon's Bead) | se usaba una vez y se iba (es trampa continua) | se queda y se vuelve a usar en respuesta; "
		     "caso nuevo |\n"
		     "| motor: `ia.h` (Master Monk, Ninjitsu Art of Decoy) | Master Monk no atacaba dos veces; Ninjitsu Art of Decoy (trampa "
		     "continua) protegia solo el turno en que se activaba | Master Monk con los de dos ataques; Decoy se queda vinculada al Ninja; "
		     "casos nuevos |\n"
		     "| motor: `ia.h` (Kaminote Blow) | el que combatia con Chu-Ske / Monk Fighter / Master Monk se destruia en la fase final, no al "
		     "final del paso de danno | `destruirTrasCalculo`; caso nuevo |\n"
		     "| datos: `datos/rivales.txt` (linea N) | Guardian Dreadscythe, Perfectly Ultimate Great Moth y Toon Dark Magician Girl dicen \"Cannot "
		     "be Normal Summoned\" y no estaban en la lista: se podian invocar normal | agregadas (un barrido de los textos no encuentra otras) |\n"
		     "| prueba de PC: `prueba_efectos.cpp` | los casos no leian rivales.txt (lineas N, recetas): Harpie Lady Sisters se podia invocar "
		     "normal en las pruebas aunque en el juego no | lee rivales.txt y personajes_rivales.txt como el juego; todos los casos siguen bien |\n"
		     "| motor: `ia.h` (5 Toons) | Toon Cannon Soldier, Toon Masked Sorcerer, Toon Gemini Elf, Toon Dark Magician Girl y Toon Goblin "
		     "Attack Force solo tenian la regla Toon, no su efecto propio | `cartasProbador()`: sus efectos (copiados de sus versiones no Toon "
		     "cuando existen); casos nuevos |\n"
		     "| motor: `ia.h` (fusiones) | 15 fusiones con efecto no tenian DefCarta: Thunder Giant, Flame Wingman, Dark Blade, Dark Flare Knight, "
		     "Gatling Dragon, Super Robolady/Roboyarou, Chimera, Mokey Mokey King, Alligator's Sword Dragon, Reaper on the Nightmare y los X-Y-Z | "
		     "`cartasProbador()` y `motorSucesoProbador()`; casos nuevos (los X-Y-Z y Reaper siguen FALLA por partes que faltan) |\n"
		     "| motor: `ia.h` (Battle-Scarred) | el rival no pagaba: se miraba que el pago fuera en la fase de robo, pero el mantenimiento se "
		     "paga en su disparo de la fase de espera | tambien durante ese disparo; caso nuevo |\n"
		     "| prueba de PC: `prueba_juego.cpp` | no compilaba desde classic-menus (menu principal) | maquetas del menu principal y la pausa "
		     "\"Salir al menu principal\" espera el menu |\n\n";
		if (const char* notas = getenv("CERT_NOTAS")) {   // (la revision escrita a mano: src/pruebas/certificacion_notas.md)
			std::ifstream n(notas);
			f << "\n" << n.rdbuf() << "\n";
		}
		f << "\n## Fallas (" << fallas.size() << ")\n\n";
		if (fallas.empty()) f << "Ninguna.\n";
		else {
			f << "| carta | tipo | esperado | obtenido / detalle |\n|---|---|---|---|\n";
			for (const ResultadoCarta* rc : fallas) {
				const Texto& t = textos[indiceTexto[rc->id]];
				f << "| " << md(t.nombre) << " (`" << rc->id << "`) | " << md(rc->tipo) << " | " << md(rc->esperado) << " | " << md(rc->detalle) << " |\n";
			}
		}
		f << "\n## Dudosas para revisar (" << revisar.size() + std::size(AJUSTES) << ")\n\n";
		for (const ResultadoCarta* rc : revisar) f << "- `" << rc->id << "`: " << md(rc->revision) << " " << md(rc->detalle) << "\n";
		f << "\nExpectativas de la plantilla ajustadas a mano (la plantilla no puede ver su efecto; quedan certificadas por su caso a mano):\n\n";
		for (const Ajuste& a : AJUSTES)
			if (a.id[0]) f << "- `" << a.id << "`: " << md(a.nota) << "\n";
		f << "\nPlantilla por carta: " << nPlantilla[0] << " OK, " << nPlantilla[1] << " FALLA, " << nPlantilla[2]
		  << " sin prueba (no se pudo activar en los escenarios genericos A/B/C o su efecto no se ve ahi: condiciones de tipo, nivel, "
		     "contadores, azar, efectos continuos). Esas cartas quedan certificadas por su caso a mano; la columna `motivo_plantilla` del CSV "
		     "dice por que.\n";
		f << "\n## Escenas de los casos con fallas en el front (" << fallasFrontEscenas.size() << ")\n\n";
		for (const std::string& s : fallasFrontEscenas) f << "- " << md(s) << "\n";
		f << "\n## Sin prueba (" << total.sin << ")\n\n";
		for (const auto& kv : resultados) {
			const ResultadoCarta& rc = kv.second;
			if (rc.estado == "SIN PRUEBA") f << "- `" << rc.id << "` (" << md(rc.tipo) << "): " << md(rc.motivoSinPrueba) << "\n";
		}
	}
	return total.falla ? 1 : 0;
}
