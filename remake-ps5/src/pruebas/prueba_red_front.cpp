// Casos de uso del DUELO EN LINEA desde el front (rama classic-en-linea; CU-RED-FE-*): DOS instancias del juego completo
// sin GPU (cu_juego.h), cada una con su jugador automatico de mando (cuft::Bot de cu_front_total.h) en su propio hilo,
// conectadas por UDP de verdad en loopback. Los dos juegos avanzan de a un cuadro por turno (Turnos: nunca corren a la
// vez, asi el estado global de las pantallas no se pisa) con el reloj de la red virtual (enl.relojVirtual: los 10 s de
// una caida o los 3 min del reloj pasan en cuadros, no en segundos de verdad).
// Recorre: aviso de IP, red local, moneda comun, duelo completo con la vista en espejo del invitado, resultado igual en
// los dos, revancha, rendicion, reloj agotado 1 y 3 veces, caida y reconexion a los 10 s, desconexion definitiva (no
// cuenta), codigo manual con el teclado en pantalla y buscar rival por la DHT (mini red en el proceso) jugando contra
// la IA mientras espera.
// Compilar y correr (contenedor arch-ps5):
//   g++ -std=c++17 -O2 -DPS5_HOST_REFERENCE -pthread -I remake-ps5/src -I /work/PS5_ClassicRemaster/external/tinygltf \
//       -I /work/PS5_ClassicRemaster/external/glm remake-ps5/src/pruebas/prueba_red_front.cpp -o /tmp/prueba_red_front
//   /tmp/prueba_red_front <carpeta con ui/> <salida.tsv>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#include "cu_front_total.h"

#include <condition_variable>
#include <mutex>
#include <thread>

using namespace cuft;
using red::Direccion;

// ------------------------------------------------------------------------------------------------- mini red DHT
// (como prueba_sala.cpp: unos nodos de la DHT en el proceso, para buscar rival sin internet)
struct MiniRed {
	std::vector<std::unique_ptr<red::dht::Nodo>> nodos;
	std::atomic<bool> parar{ false };
	std::thread hilo;
	static Direccion loopback(uint16_t p)
	{
		Direccion d;
		Direccion::desdeTexto("127.0.0.1", d);
		d.puerto = p;
		return d;
	}
	explicit MiniRed(int n)
	{
		for (int i = 0; i < n; i++) {
			nodos.emplace_back(new red::dht::Nodo());
			nodos.back()->abrir(0);
		}
		for (int i = 1; i < n; i++) nodos[(size_t)i]->arrancar({ loopback(nodos[0]->puerto()) });
		nodos[0]->arrancar({ loopback(nodos[(size_t)n - 1]->puerto()) });
		bombear(400);
		for (auto& x : nodos) x->refrescar();
		bombear(400);
		hilo = std::thread([this] { while (!parar) for (auto& x : nodos) x->paso(1); });
	}
	~MiniRed()
	{
		parar = true;
		if (hilo.joinable()) hilo.join();
	}
	void bombear(int ms)
	{
		const uint64_t fin = red::ahoraMs() + (uint64_t)ms;
		while (red::ahoraMs() < fin) for (auto& x : nodos) x->paso(1);
	}
	std::vector<Direccion> arranque() const { return { loopback(nodos[0]->puerto()), loopback(nodos[1]->puerto()) }; }
};

// ------------------------------------------------------------------------------------------------- turnos de cuadro
// A y B se turnan: un cuadro de A, uno de B... "congelar" deja a un lado sin cuadros (su juego no corre ni lee la red:
// para el otro, se corto) mientras el otro sigue
struct Turnos {
	std::mutex m;
	std::condition_variable cv;
	int turno{ 0 };
	int congelado[2]{ 0, 0 };   // cuadros que ese lado se salta
	bool termino[2]{ false, false };
	int fase[2]{ 0, 0 };        // citas (los dos llegan antes de seguir)
	void esperar(int yo)
	{
		std::unique_lock<std::mutex> l(m);
		cv.wait(l, [&] { return turno == yo || termino[1 - yo]; });
	}
	void pasar(int yo)
	{
		std::lock_guard<std::mutex> l(m);
		const int otro = 1 - yo;
		if (termino[otro]) return;
		if (congelado[otro] > 0) {
			congelado[otro]--;
			return;   // (el otro sigue sin cuadros: me toca otra vez)
		}
		turno = otro;
		cv.notify_all();
	}
	void salir(int yo)
	{
		std::lock_guard<std::mutex> l(m);
		termino[yo] = true;
		turno = 1 - yo;
		cv.notify_all();
	}
};
static Turnos T;

struct Lado {
	int id;
	const char* nombre;
	Juego* g{ nullptr };
	Bot* b{ nullptr };
	std::string carpeta;
};
static Lado L[2] = { { 0, "A", nullptr, nullptr, "" }, { 1, "B", nullptr, nullptr, "" } };

// los casos los anota el lado A (los dos escriben en cu::, pero nunca a la vez: Turnos)
static void caso(const std::string& id, bool ok, const std::string& det = "")
{
	cu::caso(id, ok, "%s", det.c_str());
	fprintf(stderr, "%s %s %s\n", ok ? "OK   " : "FALLO", id.c_str(), det.c_str());
}

// ------------------------------------------------------------------------------------------------- ayudas de un lado
static Juego& G(int k) { return *L[k].g; }
static Bot& B(int k) { return *L[k].b; }
static en_linea::Vista& EV(int k) { return G(k).ctl.vista.enLinea; }

// los dos lados llegan a la cita n antes de seguir (mientras espera, su juego sigue andando)
static void cita(int k, int n)
{
	{
		std::lock_guard<std::mutex> l(T.m);
		T.fase[k] = n;
	}
	for (int i = 0; i < 2000000; i++) {
		{
			std::lock_guard<std::mutex> l(T.m);
			if (T.fase[1 - k] >= n || T.termino[1 - k]) return;
		}
		B(k).cuadro(0);
	}
}
// esperar (con cuadros) a que pase algo, como mucho s segundos del juego
static bool hasta(int k, float s, const std::function<bool()>& f) { return B(k).esperarQue(s, f); }
// esperar algo que depende del tiempo de verdad (hilos de la DHT, perforacion), como mucho ms
static bool hastaReal(int k, uint64_t ms, const std::function<bool()>& f)
{
	const uint64_t fin = red::ahoraMs() + ms;
	while (red::ahoraMs() < fin) {
		if (f()) return true;
		B(k).cuadro(0);
	}
	return f();
}
// del lugar que sea al menu principal (resultado: circulo; duelo: abandonar)
static void alMenu(int k)
{
	Juego& g = G(k);
	VD& v = g.ctl.vista;
	for (int i = 0; i < 12 && v.pantalla != VD::MENU; i++) {
		if (v.pantalla == VD::PORTADA) {   // (la portada: X y el menu aparece desde negro)
			B(k).esperar(1.0f);
			B(k).apretar(PAD_CROSS, 0.2f);
			B(k).esperar(1.5f);
		} else if (v.pantalla == VD::DUELO && !v.sorteo) B(k).abandonar();
		else B(k).apretar(PAD_CIRCLE, 0.3f);
		B(k).esperar(0.9f);
	}
}
// en el menu principal: a la opcion a y X
static void elegirMenu(int k, int a)
{
	Juego& g = G(k);
	const std::vector<int> ops = g.accionesMenu();
	for (int i = 0; i < 10 && ops[(size_t)g.menuP.opcion] != a; i++) B(k).apretar(PAD_DOWN, 0.15f);
	B(k).apretar(PAD_CROSS, 0.3f);
}
// en una pantalla del modo en linea: a la opcion cuyo texto empieza con "txt" y X
static bool opcionEnLinea(int k, const std::string& txt)
{
	en_linea::Vista& e = EV(k);
	for (int i = 0; i < 20; i++) {
		if (e.opcion < (int)e.opciones.size() && e.opciones[(size_t)e.opcion].rfind(txt, 0) == 0) {
			B(k).apretar(PAD_CROSS, 0.3f);
			return true;
		}
		B(k).apretar(PAD_DOWN, 0.12f);
	}
	return false;
}
// del menu principal a DUELO EN LINEA (aceptando el aviso si sale)
static bool aEnLinea(int k)
{
	alMenu(k);
	elegirMenu(k, Juego::MP_EN_LINEA);
	if (EV(k).pantalla == en_linea::AVISO_IP) opcionEnLinea(k, idioma::tr("Aceptar"));
	return G(k).ctl.vista.pantalla == VD::EN_LINEA && EV(k).pantalla == en_linea::MENU;
}
static red::DueloRed* duelo(int k) { return G(k).enl.sesion ? &G(k).enl.sesion->duelo : nullptr; }

// la vista en espejo: lo que el invitado ve abajo son SUS cartas (el lado 1 del motor); las del otro lado, las del rival
static int espejoComprobado = 0, espejoMal = 0;
static std::string espejoDetalle;
static void mirarEspejo(int k)
{
	Juego& g = G(k);
	red::DueloRed* d = duelo(k);
	if (!d || d->estado() != red::D_JUGANDO || g.ctl.vista.pantalla != VD::DUELO || g.ctl.vista.sorteo || !B(k).alDia()) return;
	const ia::Duelo& real = d->duelo();
	const int yo = d->yo();
	const bool ok = g.dueloIA.l[0].mano == real.l[(size_t)yo].mano && g.dueloIA.l[1].mano == real.l[(size_t)(1 - yo)].mano &&
	                g.dueloIA.l[0].lp == real.l[(size_t)yo].lp && (g.dueloIA.decide() == 0) == (real.decide() == yo) && g.ctl.manoJ == g.dueloIA.l[0].mano;
	espejoComprobado++;
	if (!ok) {
		espejoMal++;
		if (espejoDetalle.empty())
			espejoDetalle = std::string("lado ") + L[k].nombre + " (motor " + std::to_string(yo) + "): mano vista " + std::to_string(g.ctl.manoJ.size()) + ", real " +
			                std::to_string(real.l[(size_t)yo].mano.size());
	}
}

// un duelo en linea ya conectado: la moneda y jugarlo con el bot (hasta maxTurnos; si no termina, abandonar = rendirse)
static Bot::ResDuelo jugar(int k, int maxTurnos, const std::string& nombre)
{
	Bot& b = B(k);
	hasta(k, 60.0f, [&] { return G(k).ctl.vista.pantalla == VD::DUELO; });
	b.reiniciarVigia();
	return b.jugarDuelo(nombre, maxTurnos, 3000.0f);
}

// ================================================================================================= el guion de cada lado
static void guion(int k)
{
	Juego& g = G(k);
	VD& v = g.ctl.vista;
	const bool A = k == 0;
	int n = 0;
	B(k).esperar(0.5f);

	// ---- 1. el aviso de privacidad: la primera vez sale; cancelar vuelve al menu sin guardar; aceptar lo recuerda
	alMenu(k);
	elegirMenu(k, Juego::MP_EN_LINEA);
	const bool salio = v.pantalla == VD::EN_LINEA && EV(k).pantalla == en_linea::AVISO_IP;
	if (A) {
		opcionEnLinea(k, idioma::tr("Cancelar"));
		const bool cancelo = v.pantalla == VD::MENU && !g.enl.avisoIp;
		elegirMenu(k, Juego::MP_EN_LINEA);
		const bool otraVez = EV(k).pantalla == en_linea::AVISO_IP;
		opcionEnLinea(k, idioma::tr("Aceptar"));
		std::ifstream f(g.rutaOpciones);
		const std::string txt((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		caso("CU-RED-FE-AVISO-IP", salio && cancelo && otraVez && g.enl.avisoIp && EV(k).pantalla == en_linea::MENU && txt.find("aviso_ip=1") != std::string::npos,
		     "sale la primera vez, cancelar vuelve al menu, aceptar se guarda en opciones.txt");
		// y se recuerda: al volver a entrar ya no sale
		alMenu(k);
		elegirMenu(k, Juego::MP_EN_LINEA);
		caso("CU-RED-FE-AVISO-IP-RECORDADO", EV(k).pantalla == en_linea::MENU);
	} else {
		opcionEnLinea(k, idioma::tr("Aceptar"));
	}
	cita(k, ++n);

	// ---- 2. red local: A crea el duelo, B lo ve en la lista y se une
	if (A) {
		opcionEnLinea(k, idioma::tr("Red local"));
		opcionEnLinea(k, idioma::tr("Crear duelo en la red local"));
		caso("CU-RED-FE-LAN-CREAR", EV(k).pantalla == en_linea::LAN_ESPERA && g.enl.anunciando, "esperando rival en la red local (anuncio)");
	} else {
		B(k).esperar(1.0f);
		opcionEnLinea(k, idioma::tr("Red local"));
		const bool visto = hastaReal(k, 8000, [&] { return g.enl.lanAbierta && !g.enl.buscador.lista().empty(); });
		B(k).esperar(0.3f);
		caso("CU-RED-FE-LAN-LISTA", visto && EV(k).opciones.size() >= 3 && EV(k).opciones[1].rfind(L[0].g->nombreRed(), 0) == 0,
		     visto ? EV(k).opciones[1] : std::string("no aparecio el duelo de A"));
		B(k).apretar(PAD_DOWN, 0.12f);
		B(k).apretar(PAD_CROSS, 0.3f);
	}
	const bool moneda = hastaReal(k, 20000, [&] { return v.pantalla == VD::DUELO && v.sorteo; });
	caso(std::string("CU-RED-FE-LAN-CONECTA-") + L[k].nombre, moneda && v.enLinea.rival == L[1 - k].g->nombreRed(), "moneda contra " + v.enLinea.rival);
	// la moneda: los dos ven el mismo resultado (uno gana, el otro pierde) y el mismo que empieza
	hasta(k, 15.0f, [&] { return v.moneda.ganador >= 0; });
	const int ganoMoneda = v.moneda.ganador;
	const int lineaAntes = estadisticas::datos().lineaDuelos, iaAntes = estadisticas::datos().duelos;
	const Bot::ResDuelo r = jugar(k, 60, std::string("lan-") + L[k].nombre);
	hasta(k, 30.0f, [&] { return v.pantalla == VD::RESULTADO; });
	cita(k, ++n);
	{
		red::DueloRed* d = duelo(k);
		red::DueloRed* o = duelo(1 - k);
		if (A && d && o) {
			caso("CU-RED-FE-MONEDA", ganoMoneda >= 0 && d->ganoMoneda() == o->ganoMoneda() && d->empieza() == o->empieza() && (ganoMoneda == 0) == (d->ganoMoneda() == 0),
			     "la moneda sale de la semilla comun: igual en los dos (gana el lado " + std::to_string(d->ganoMoneda()) + ")");
			caso("CU-RED-FE-INVITADO-LADO-1", d->yo() == 0 && o->yo() == 1, "A anfitrion (lado 0), B invitado (lado 1)");
			const int ga = G(0).ctl.vista.ganador, gb = G(1).ctl.vista.ganador;
			const bool iguales = d->fin() == o->fin() && red::hashEstado(d->duelo()) == red::hashEstado(o->duelo()) &&
			                     ((ga == 0 && gb == 1) || (ga == 1 && gb == 0) || (ga == 2 && gb == 2));
			caso("CU-RED-FE-DUELO-COMPLETO", r.empezo && G(0).ctl.vista.pantalla == VD::RESULTADO && G(1).ctl.vista.pantalla == VD::RESULTADO && iguales,
			     std::string("fin ") + red::NOMBRE_FIN[d->fin()] + ", ganador visto A " + std::to_string(ga) + " / B " + std::to_string(gb) + ", turnos " + std::to_string(d->duelo().turno));
			caso("CU-RED-FE-ESTADISTICAS", estadisticas::datos().lineaDuelos == lineaAntes + 2 && estadisticas::datos().duelos == iaAntes,
			     "los duelos en linea se cuentan aparte (" + std::to_string(estadisticas::datos().lineaDuelos) + ") y no tocan los de la IA");
			caso("CU-RED-FE-ESPEJO", espejoComprobado > 50 && espejoMal == 0,
			     std::to_string(espejoComprobado) + " cuadros comprobados, " + std::to_string(espejoMal) + " mal " + espejoDetalle);
			caso("CU-RED-FE-SIN-ELO-LAN", EV(0).eloTexto.empty() && EV(1).eloTexto.empty(), "la red local no toca el ELO");
		}
	}
	cita(k, ++n);

	// ---- 3. revancha: los dos la piden; otra moneda y otro duelo; A se rinde (cuenta como derrota)
	B(k).esperar(1.0f);
	{
		resultado::Resumen& rs = resultado::resumen();
		for (int i = 0; i < 4 && rs.opcion != resultado::O_REVANCHA; i++) B(k).apretar(PAD_LEFT, 0.12f);
		B(k).apretar(PAD_CROSS, 0.3f);
	}
	const bool revancha = hastaReal(k, 20000, [&] { return v.pantalla == VD::DUELO; });
	caso(std::string("CU-RED-FE-REVANCHA-") + L[k].nombre, revancha && duelo(k) && duelo(k)->revanchas() == 1, "los dos pidieron la revancha: otra moneda");
	cita(k, ++n);
	{
		const int lineaAntes2 = estadisticas::datos().lineaDuelos;
		hasta(k, 60.0f, [&] { return v.pantalla == VD::DUELO && !v.sorteo; });
		B(k).resolverInicio(true);
		B(k).esperar(2.0f);
		if (A) B(k).abandonar();
		hasta(k, 30.0f, [&] { return v.pantalla == VD::RESULTADO; });
		cita(k, ++n);
		if (A)
			caso("CU-RED-FE-RENDICION", G(0).ctl.vista.ganador == 1 && G(1).ctl.vista.ganador == 0 && G(0).motivoFin() == resultado::M_RENDICION &&
			                            G(1).motivoFin() == resultado::M_RENDICION_RIVAL && estadisticas::datos().lineaDuelos == lineaAntes2 + 2,
			     "A se rinde: derrota de A, victoria de B (cuenta)");
	}
	cita(k, ++n);

	// ---- 4. reloj del turno: A no juega; a los 3 min el juego pasa solo (1 agotamiento); al tercero pierde
	B(k).esperar(1.0f);
	{
		resultado::Resumen& rs = resultado::resumen();
		for (int i = 0; i < 4 && rs.opcion != resultado::O_REVANCHA; i++) B(k).apretar(PAD_LEFT, 0.12f);
		B(k).apretar(PAD_CROSS, 0.3f);
	}
	hastaReal(k, 20000, [&] { return v.pantalla == VD::DUELO; });
	B(k).resolverInicio(false);   // (si A gana la moneda, va segundo: B empieza y juega su turno)
	cita(k, ++n);
	{
		red::DueloRed* d = duelo(k);
		bool alerta = false, uno = false, pasoSolo = false;
		if (A) {
			// A no toca nada: espera su turno y deja correr el reloj
			int turnoAgotado = -1;
			for (int i = 0; i < 400000 && d && d->estado() == red::D_JUGANDO; i++) {
				B(k).cuadro(0);
				B(k).tRival = 0.0f;
				if (d->meToca() && g.enl.reloj[0] <= en_linea::RELOJ_ALERTA_S && g.enl.reloj[0] > 0.0f && v.enLinea.relojMioCorre) alerta = true;
				if (!uno && d->agotados(0) == 1) {
					uno = true;
					turnoAgotado = d->duelo().turno;
				}
				if (uno && !pasoSolo && d->duelo().turno != turnoAgotado) pasoSolo = true;
			}
			hasta(k, 10.0f, [&] { return v.pantalla == VD::RESULTADO; });
		} else {
			// B pasa sus turnos sin invocar ni atacar (y contesta en los de A): asi A no pierde por danno antes de su
			// tercer agotamiento
			for (int i = 0; i < 400000 && d && d->estado() == red::D_JUGANDO; i++) {
				B(k).tRival = 0.0f;
				if (g.dueloIA.activo == 0 && g.dueloIA.decide() == 0 && B(k).enDuelo()) B(k).turnoJugador(0);
				else B(k).cuadro(0);
			}
			hasta(k, 10.0f, [&] { return v.pantalla == VD::RESULTADO; });
		}
		cita(k, ++n);
		if (A) {
			red::DueloRed* o = duelo(1);
			caso("CU-RED-FE-RELOJ-ALERTA", alerta, "con 30 s o menos el reloj propio avisa (sonido y parpadeo)");
			caso("CU-RED-FE-RELOJ-1", uno && pasoSolo && o && o->agotados(0) >= 1, "el reloj de A se agoto: paso de turno solo y B lo conto (lo manda A)");
			caso("CU-RED-FE-RELOJ-3", d && o && d->fin() == red::FIN_TIEMPO && o->fin() == red::FIN_TIEMPO && G(0).ctl.vista.ganador == 1 && G(1).ctl.vista.ganador == 0 &&
			                          G(0).motivoFin() == resultado::M_TIEMPO && d->agotados(0) == 3,
			     std::string("fin ") + (d ? red::NOMBRE_FIN[d->fin()] : "-") + ", agotados " + (d ? std::to_string(d->agotados(0)) : "-"));
		}
	}
	cita(k, ++n);

	// ---- 5. caida y reconexion a los 10 s (el duelo sigue), y desconexion definitiva (no cuenta)
	B(k).esperar(1.0f);
	{
		resultado::Resumen& rs = resultado::resumen();
		for (int i = 0; i < 4 && rs.opcion != resultado::O_REVANCHA; i++) B(k).apretar(PAD_LEFT, 0.12f);
		B(k).apretar(PAD_CROSS, 0.3f);
	}
	hastaReal(k, 20000, [&] { return v.pantalla == VD::DUELO; });
	B(k).resolverInicio(true);
	hasta(k, 20.0f, [&] { return v.pantalla == VD::DUELO && !v.sorteo && B(k).alDia(); });
	cita(k, ++n);
	if (A) {
		red::DueloRed* d = duelo(k);
		const uint32_t pasos0 = d ? d->pasos() : 0;
		{
			std::lock_guard<std::mutex> l(T.m);
			T.congelado[1] = 60 * 12;   // B sin cuadros 12 s
		}
		bool cartel = false;
		int seg = 0;
		for (int i = 0; i < 60 * 12; i++) {
			B(k).cuadro(0);
			if (v.enLinea.conexion == 1) {
				cartel = true;
				seg = v.enLinea.reconectando;
			}
		}
		const bool volvio = hasta(k, 5.0f, [&] { return v.enLinea.conexion == 0; });
		caso("CU-RED-FE-RECONEXION", cartel && seg >= 1 && volvio && d && d->estado() == red::D_JUGANDO && d->pasos() >= pasos0,
		     "cartel 'Esperando al rival' (" + std::to_string(seg) + " s) y a los 12 s vuelve: el duelo sigue");
		B(k).esperar(2.0f);
		// la definitiva: B sin cuadros 31 s
		const int lineaAntes = estadisticas::datos().lineaDuelos, eloAntes = g.salaRed().datosElo().partidas;
		{
			std::lock_guard<std::mutex> l(T.m);
			T.congelado[1] = 60 * 32;
		}
		for (int i = 0; i < 60 * 32; i++) B(k).cuadro(0);
		hasta(k, 3.0f, [&] { return v.pantalla == VD::EN_LINEA; });
		caso("CU-RED-FE-DESCONEXION-NO-CUENTA", v.pantalla == VD::EN_LINEA && EV(k).pantalla == en_linea::INTERRUMPIDO && estadisticas::datos().lineaDuelos == lineaAntes &&
		                                        g.salaRed().datosElo().partidas == eloAntes && !g.enl.sesion,
		     "a los 30 s: 'Duelo interrumpido: no cuenta' (" + EV(k).titulo + "); ni estadisticas ni ELO");
	} else {
		// B: cuando vuelve a tener cuadros, sigue; tras la definitiva tambien lo ve interrumpido (a los 30 s suyos)
		hasta(k, 200.0f, [&] { return v.pantalla == VD::EN_LINEA && EV(k).pantalla == en_linea::INTERRUMPIDO; });
		caso("CU-RED-FE-DESCONEXION-OTRO-LADO", v.pantalla == VD::EN_LINEA && EV(k).pantalla == en_linea::INTERRUMPIDO, "el otro lado tambien: no cuenta");
	}
	cita(k, ++n);
	B(k).apretar(PAD_CROSS, 0.3f);   // (Continuar: al menu en linea)

	// ---- 6. codigo de conexion: A muestra el suyo; B lo escribe con el teclado en pantalla
	if (A) {
		opcionEnLinea(k, idioma::tr("Código de conexión"));
		hastaReal(k, 10000, [&] { return g.enl.fase == Juego::FR_CODIGO; });
	}
	cita(k, ++n);
	if (!A) {
		const std::string codigo = L[0].g->enl.codigo;
		opcionEnLinea(k, idioma::tr("Código de conexión"));
		hastaReal(k, 10000, [&] { return g.enl.fase == Juego::FR_CODIGO; });
		opcionEnLinea(k, idioma::tr("Escribir el código del rival"));
		const bool teclado = EV(k).pantalla == en_linea::TECLADO;
		// cada letra con la cruceta (como un jugador) y X
		for (char c : codigo) {
			const char* p = strchr(en_linea::TECLAS, c);
			if (!p) continue;
			const int meta = (int)(p - en_linea::TECLAS), f = en_linea::TECLAS_FILA;
			while (EV(k).tecla / f > meta / f) B(k).apretar(PAD_UP, 0.05f);
			while (EV(k).tecla / f < meta / f) B(k).apretar(PAD_DOWN, 0.05f);
			while (EV(k).tecla % f > meta % f) B(k).apretar(PAD_LEFT, 0.05f);
			while (EV(k).tecla % f < meta % f) B(k).apretar(PAD_RIGHT, 0.05f);
			B(k).apretar(PAD_CROSS, 0.05f);
		}
		const bool escrito = EV(k).texto == codigo;
		B(k).apretar(PAD_TRIANGLE, 0.3f);
		const bool conecta = hastaReal(k, 20000, [&] { return v.pantalla == VD::DUELO && v.sorteo; });
		caso("CU-RED-FE-CODIGO", teclado && escrito && conecta && !codigo.empty(), "codigo de A (" + codigo + ") escrito con el teclado en pantalla: conectan");
	}
	hastaReal(k, 20000, [&] { return v.pantalla == VD::DUELO; });
	B(k).resolverInicio(true);
	B(k).esperar(1.0f);
	if (!A) B(k).abandonar();   // (B se rinde: termina rapido)
	hasta(k, 30.0f, [&] { return v.pantalla == VD::RESULTADO; });
	cita(k, ++n);
	if (A) caso("CU-RED-FE-CODIGO-DUELO", G(0).ctl.vista.ganador == 0 && G(1).ctl.vista.ganador == 1, "el duelo por codigo termina igual en los dos");
	cita(k, ++n);

	// ---- 7. buscar rival (DHT de la mini red): A juega contra la IA mientras espera; aparece B: "¡Rival encontrado!"
	alMenu(k);
	aEnLinea(k);
	const int iaAntes7 = estadisticas::datos().duelos;
	if (A) {
		opcionEnLinea(k, idioma::tr("Buscar rival"));
		hastaReal(k, 8000, [&] { return g.enl.fase == Juego::FR_BUSCANDO; });
		opcionEnLinea(k, idioma::tr("Jugar contra la IA mientras espero"));
		const bool elegir = v.pantalla == VD::ELEGIR_RIVAL && g.enl.conIA;
		B(k).apretar(PAD_CROSS, 0.5f);   // (el rival que toca)
		B(k).resolverInicio(true);
		const bool contraIA = v.pantalla == VD::DUELO && !g.enLineaActivo();
		caso("CU-RED-FE-IA-ESPERA", elegir && contraIA, "busca y juega contra la IA a la vez");
	}
	cita(k, ++n);
	if (!A) {
		B(k).esperar(1.0f);
		opcionEnLinea(k, idioma::tr("Buscar rival"));
	}
	if (A) {
		const bool cartel = hastaReal(k, 40000, [&] { return v.enLinea.rivalEncontrado; });
		caso("CU-RED-FE-RIVAL-ENCONTRADO", cartel && v.enLinea.rivalEncontradoTexto.find(L[1].g->nombreRed()) != std::string::npos,
		     "cartel en pleno duelo contra la IA: " + v.enLinea.rivalEncontradoTexto);
		B(k).apretar(PAD_CROSS, 0.3f);
	}
	const bool conecta = hastaReal(k, 40000, [&] { return v.pantalla == VD::DUELO && v.sorteo && g.enLineaActivo(); });
	caso(std::string("CU-RED-FE-BUSCAR-") + L[k].nombre, conecta, "buscar rival por la DHT: emparejados y conectados");
	B(k).resolverInicio(true);
	B(k).esperar(1.0f);
	if (A) B(k).abandonar();
	hasta(k, 30.0f, [&] { return v.pantalla == VD::RESULTADO; });
	cita(k, ++n);
	if (A)
		caso("CU-RED-FE-IA-NO-CUENTA", estadisticas::datos().duelos == iaAntes7 && G(0).ctl.vista.ganador == 1 && G(1).ctl.vista.ganador == 0,
		     "el duelo contra la IA se abandono sin contar; el duelo en linea si");
	cita(k, ++n);
	alMenu(k);
}

// (rama classic-arreglo-ciclo-md, ciclo md_ciclo1: CU-FT-MESA-*/POSICION-MESA/ACCION-COLOCAR-MT en lan-A) el ritmo de Master
// Duel guarda su estado en globales del proceso (ritmo::estado(): cartas en vuelo, eslabones; entradamd::estado() y
// entrada::estado(): la fusion/ritual y la carta clave en curso). En la consola hay un juego por proceso; aca hay DOS,
// y la carta que volaba en B aterrizaba en la mesa de A (Horus en la zona del rival sin estar en su duelo) y su espera
// bloqueaba a A (X abria el detalle en vez del menu). Cada juego tiene su copia: se pone antes de su cuadro y se guarda
// despues (los cuadros nunca corren a la vez: Turnos)
struct GlobalesMD {
	ritmo::Estado ritmo;
	entradamd::Estado entradaMD;
	entrada::Estado entradaClave;
	void poner() const
	{
		ritmo::estado() = ritmo;
		entradamd::estado() = entradaMD;
		entrada::estado() = entradaClave;
	}
	void guardar()
	{
		ritmo = ritmo::estado();
		entradaMD = entradamd::estado();
		entradaClave = entrada::estado();
	}
};
static GlobalesMD globalesMD[2];

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "uso: prueba_red_front <carpeta con ui/> <salida.tsv>\n");
		return 2;
	}
	const std::string assets = std::string(argv[1]) + "/", salida = argv[2];
	for (const char* v : { "YUGI_GUION", "YUGI_IDIOMA", "YUGI_FIGURAS", "YUGI_CAMARA", "YUGI_VELOCIDAD", "YUGI_RESULTADO", "YUGI_SALA", "YUGI_EN_LINEA", "YUGI_NOMBRE" }) unsetenv(v);
	MiniRed dht(8);
	opciones::preguntar() = opciones::PREGUNTAR_AUTO;
	for (int k = 0; k < 2; k++) {
		L[k].carpeta = assets + "datos_red_" + L[k].nombre + "/";
		system(("rm -rf '" + L[k].carpeta + "' && mkdir -p '" + L[k].carpeta + "'").c_str());
		setenv("YUGI_MAZO", (L[k].carpeta + "mazo.txt").c_str(), 1);
		if (k == 0) idioma::cargar(L[k].carpeta + "idioma.txt");
		Juego* g = new Juego;
		// la red de la prueba: loopback, sin STUN ni UPnP, reloj virtual, la sala apagada (DHT de la mini red)
		g->enl.stun = g->enl.upnp = false;
		g->enl.puerto = 0;
		Direccion lo;
		Direccion::desdeTexto("127.0.0.1", lo);
		g->enl.localesExtra = { lo };
		g->enl.lanExtra = { lo };
		g->enl.puertoLan = 47400;
		g->enl.relojVirtual = true;
		g->enl.nombre = k == 0 ? "Yugi" : "Kaiba";
		g->enl.sala.usarNube = false;
		g->enl.sala.arranque.clear();
		g->enl.sala.arranqueDirecto = dht.arranque();
		g->enl.sala.cadaBusquedaDhtMs = 1500;
		g->enl.sala.rutaElo = L[k].carpeta + "en_linea.txt";
		g->preparar(assets);
		if (!g->baseIAlista) {
			fprintf(stderr, "sin datos en %s\n", assets.c_str());
			return 2;
		}
		g->ctl.vista.nivelMax = ia::NIVEL_MAX;
		L[k].g = g;
		L[k].b = new Bot(*g);
		L[k].b->azar = 777u + (uint32_t)k * 31u;
		const int yo = k;
		globalesMD[k].guardar();
		L[k].b->antesDeCuadro = [yo] {
			T.esperar(yo);
			globalesMD[yo].poner();
		};
		L[k].b->despuesDeCuadro = [yo] {
			globalesMD[yo].guardar();
			mirarEspejo(yo);
			T.pasar(yo);
		};
	}
	std::thread ha([] {
		guion(0);
		T.salir(0);
	});
	std::thread hb([] {
		guion(1);
		T.salir(1);
	});
	ha.join();
	hb.join();
	fprintf(stderr, "botones: A %d, B %d\n", B(0).nBotones, B(1).nBotones);
	return cu::resumen(salida.c_str());
}
