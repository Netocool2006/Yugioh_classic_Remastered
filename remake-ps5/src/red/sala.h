#pragma once
// Busqueda de rival en linea (rama classic-sala): la interfaz unica para el juego.
//
//   red::Sala sala;                                   // (lee la URL de la sala: urlSalaPorDefecto())
//   sala.buscar(sala.perfilLocal("Yugi", "1.0", "ps5", catalogo), misCandidatos);   // candidatos: nat.h
//   cada cuadro: sala.actualizar(); if (sala.estado() == red::Sala::Emparejado && sala.pareja(p)) -> perforar (nat.h)
//   al terminar el duelo: sala.reportarResultado(p.idSesion, gane);
//
// 1. Prueba primero la sala de Cloudflare (servidor/sala, HTTP sin TLS, long-polling) en un hilo.
// 2. Si la sala no responde en esperaNubeMs (o falla un pedido, o no hay URL configurada), arranca la DHT de
//    BitTorrent (dht.h) EN PARALELO en otro hilo; la primera que empareja gana.
// Nada bloquea el cuadro: los pedidos y la DHT van en hilos (std::thread, como cosmeticos_ui.h) que comparten un
// estado con candado; actualizar() solo mira ese estado. Cancelar o destruir la Sala no espera a los hilos: se les
// avisa y terminan solos (el long-polling se corta en 100 ms y se manda "cancelar" a la sala).
//
// Estados: Buscando (hay busqueda en curso), Emparejado (pareja() la devuelve), SinConexion (sin busqueda, o la sala
// no responde y la DHT no consigue ningun nodo: no hay internet), Error (no se pudo ni abrir un socket).
#include <atomic>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "socket_udp.h"
#include "sala_tipos.h"
#include "http.h"
#include "dht.h"
#include "elo.h"

namespace red {

// la URL de la sala publicada (servidor/sala/LEEME.md). Mientras diga CUENTA no esta configurada: se usa solo la DHT
static const char* const URL_SALA = "http://yugioh-classic-sala.CUENTA.workers.dev";

// la URL: YUGI_SALA (PC/pruebas), o la linea "sala=http://..." de red.txt (consola: /data/yugioh_classic/red.txt; PC:
// ./red.txt), o URL_SALA
inline std::string urlSalaPorDefecto()
{
	if (const char* v = getenv("YUGI_SALA")) return v;
#if defined(PS5_HOST_REFERENCE)
	std::ifstream f("red.txt");
#else
	std::ifstream f("/data/yugioh_classic/red.txt");
#endif
	std::string l;
	while (std::getline(f, l)) {
		if (!l.empty() && l.back() == 13) l.pop_back();   // (fin de linea de Windows)
		if (l.compare(0, 5, "sala=") == 0) return l.substr(5);
	}
	return URL_SALA;
}
inline bool urlConfigurada(const std::string& u) { return !u.empty() && u.find("CUENTA") == std::string::npos; }

struct SalaConfig {
	std::string url = urlSalaPorDefecto();
	bool usarNube = true, usarDht = true;
	int esperaNubeMs = 5000;       // sin respuesta de la sala en este tiempo: DHT en paralelo
	int esperaPedidoMs = 8000;     // plazo de un pedido normal
	int esperaLargaMs = 20000;     // long-polling de /estado
	int reintentoMs = 3000;        // la sala fallo: se reintenta despues de esto
	int sinNodosMs = 12000;        // la DHT sin ningun nodo en este tiempo (y la sala caida): SinConexion
	std::vector<dht::Arranque> arranque = dht::arranquePublico();
	std::vector<Direccion> arranqueDirecto;   // (pruebas) nodos de arranque ya resueltos
	uint16_t puertoDht = 0;
	int cadaBusquedaDhtMs = 20000;   // cada cuanto se vuelve a anunciar y buscar en la DHT
	std::string claveDht = dht::claveSala();
	std::string rutaElo;           // vacio = elo::ruta()
	bool guardarElo = true;
};

class Sala {
 public:
	enum Estado { Buscando, Emparejado, SinConexion, Error };

	explicit Sala(SalaConfig c = SalaConfig()) : cfg_(std::move(c))
	{
		datos_ = elo::cargar(rutaElo());
	}
	~Sala() { soltar(); }
	Sala(const Sala&) = delete;
	Sala& operator=(const Sala&) = delete;

	// el perfil de este jugador con su id anonimo y su ELO (en_linea.txt)
	Perfil perfilLocal(const std::string& nombre, const std::string& version, const std::string& plataforma,
		const std::string& catalogo = "") const
	{
		Perfil p;
		p.id = datos_.id;
		p.nombre = nombre;
		p.elo = datos_.elo;
		p.version = version;
		p.plataforma = plataforma;
		p.catalogo = catalogo;
		return p;
	}
	const elo::Datos& datosElo() const { return datos_; }

	void buscar(const Perfil& yo, const std::vector<Candidato>& mios)
	{
		soltar();
		auto b = std::make_shared<Busqueda>();
		b->cfg = cfg_;
		b->yo = yo;
		b->mios = mios;
		b->inicio = ahoraMs();
		b->contador = contador_;
		actual_ = b;
		pareja_ = Pareja();
		hayPareja_ = false;
		if (cfg_.usarNube && urlConfigurada(cfg_.url)) std::thread([b] { hiloNube(b); }).detach();
		else iniciarDht();   // sin sala: directo a la DHT
	}
	void cancelar()
	{
		soltar();
		hayPareja_ = false;
	}

	Estado estado()
	{
		if (!actual_) return hayPareja_ ? Emparejado : SinConexion;
		std::lock_guard<std::mutex> l(actual_->m);
		const Busqueda& b = *actual_;
		if (b.hayPareja) return Emparejado;
		const bool nubeUsable = cfg_.usarNube && urlConfigurada(cfg_.url);
		const bool nubeMal = !nubeUsable || b.nubeFallos > 0 || (!b.nubeRespondio && ahoraMs() - b.inicio > (uint64_t)cfg_.esperaNubeMs);
		if (b.dhtError && nubeMal) return Error;
		if (nubeMal && (!cfg_.usarDht || (b.dhtIniciada && b.dhtNodos == 0 && ahoraMs() - b.dhtInicio > (uint64_t)cfg_.sinNodosMs)))
			return SinConexion;
		return Buscando;
	}
	bool pareja(Pareja& p)
	{
		if (actual_) {
			std::lock_guard<std::mutex> l(actual_->m);
			if (actual_->hayPareja) { pareja_ = actual_->pareja; hayPareja_ = true; }
		}
		if (!hayPareja_) return false;
		p = pareja_;
		return true;
	}
	// jugadores en linea y buscando ahora (de la sala; sin sala, los pares del juego vistos por la DHT). -1: no se sabe
	int enLinea()
	{
		std::lock_guard<std::mutex> l(contador_->m);
		return contador_->conectados;
	}
	int buscandoAhora()
	{
		std::lock_guard<std::mutex> l(contador_->m);
		return contador_->buscando;
	}
	int duelosEnCurso()
	{
		std::lock_guard<std::mutex> l(contador_->m);
		return contador_->duelos;
	}
	// pide el contador a la sala sin buscar (para el menu); el resultado llega a enLinea()/buscandoAhora()
	void consultarEnLinea()
	{
		if (!cfg_.usarNube || !urlConfigurada(cfg_.url)) return;
		auto c = contador_;
		const std::string url = cfg_.url;
		const int espera = cfg_.esperaPedidoMs;
		std::thread([c, url, espera] {
			const http::Respuesta r = http::pedir(url + "/v1/enLinea", "GET", "", espera);
			if (r.estado == 200) leerContador(*c, Campos::leer(r.cuerpo));
		}).detach();
	}

	// el duelo termino: ELO local enseguida (en_linea.txt) y, si la pareja vino de la sala, reporte a la sala (cuenta
	// solo si los dos reportan lo mismo; su ELO manda y llega por actualizar())
	void reportarResultado(uint64_t idSesion, bool gane)
	{
		Pareja p;
		if (!pareja(p) || p.idSesion != idSesion || reportada_ == idSesion) return;
		reportada_ = idSesion;
		elo::anotar(datos_, p.rival.elo, gane);
		if (cfg_.guardarElo) elo::guardar(datos_, rutaElo());
		if (p.porDht || !cfg_.usarNube || !urlConfigurada(cfg_.url)) return;
		auto e = eloSala_;
		const std::string url = cfg_.url;
		Campos c;
		c.poner("id", datos_.id);
		c.poner("sesion", std::to_string(idSesion));
		c.poner("gane", gane ? 1LL : 0LL);
		const std::string cuerpo = c.texto();
		const int espera = cfg_.esperaPedidoMs;
		std::thread([e, url, cuerpo, espera] {
			// el otro puede reportar despues: se pregunta unas veces hasta que cuente (o se descarte)
			for (int i = 0; i < 6; i++) {
				const http::Respuesta r = http::pedir(url + "/v1/resultado", "POST", cuerpo, espera);
				if (r.estado != 200) break;
				const Campos f = Campos::leer(r.cuerpo);
				const std::string est = f.obtener("estado");
				if (est == "contado") { e->store((int)f.numero("elo", -1)); break; }
				if (est == "descartado") break;
				std::this_thread::sleep_for(std::chrono::seconds(10));
			}
		}).detach();
	}
	// 'pendiente' del reporte: el ELO que confirmo la sala (-1 si todavia no)
	int eloConfirmado() const { return eloConfirmado_; }
	// (rama classic-en-linea) cuantas veces confirmo la sala un ELO: la pantalla de resultado sabe si llego el de ESTE duelo
	int confirmaciones() const { return confirmaciones_; }

	// cada cuadro: no bloquea. Arranca la DHT si la sala no responde y aplica el ELO que confirmo la sala
	void actualizar()
	{
		const int e = eloSala_->exchange(-1);
		if (e > 0) {
			elo::aplicarSala(datos_, e);
			eloConfirmado_ = e;
			confirmaciones_++;
			if (cfg_.guardarElo) elo::guardar(datos_, rutaElo());
		}
		if (!actual_ || !cfg_.usarDht) return;
		bool arrancar = false;
		{
			std::lock_guard<std::mutex> l(actual_->m);
			if (actual_->hayPareja || actual_->dhtIniciada) return;
			if (actual_->nubeFallos > 0 || (!actual_->nubeRespondio && ahoraMs() - actual_->inicio > (uint64_t)cfg_.esperaNubeMs)) arrancar = true;
		}
		if (arrancar) iniciarDht();
	}

	// (agregados, para la interfaz y las pruebas)
	bool usandoDht()
	{
		if (!actual_) return false;
		std::lock_guard<std::mutex> l(actual_->m);
		return actual_->dhtIniciada;
	}
	bool salaResponde()
	{
		if (!actual_) return false;
		std::lock_guard<std::mutex> l(actual_->m);
		return actual_->nubeRespondio && actual_->nubeFallos == 0;
	}
	int nodosDht()
	{
		if (!actual_) return 0;
		std::lock_guard<std::mutex> l(actual_->m);
		return actual_->dhtNodos;
	}

 private:
	struct Contador {
		std::mutex m;
		int conectados = -1, buscando = -1, duelos = -1;
	};
	// una busqueda: la comparten el juego y sus hilos (shared_ptr: los hilos la sueltan al terminar)
	struct Busqueda {
		std::mutex m;
		std::atomic<bool> parar{ false };
		SalaConfig cfg;
		Perfil yo;
		std::vector<Candidato> mios;
		uint64_t inicio = 0, dhtInicio = 0;
		bool hayPareja = false;
		Pareja pareja;
		bool nubeRespondio = false;
		int nubeFallos = 0;   // fallos seguidos (0 si el ultimo pedido anduvo)
		bool dhtIniciada = false, dhtError = false;
		int dhtNodos = 0;
		std::shared_ptr<Contador> contador;
	};

	std::string rutaElo() const { return cfg_.rutaElo.empty() ? elo::ruta() : cfg_.rutaElo; }

	void soltar()
	{
		if (actual_) {
			std::lock_guard<std::mutex> l(actual_->m);
			if (actual_->hayPareja) { pareja_ = actual_->pareja; hayPareja_ = true; }
			actual_->parar = true;
		}
		actual_.reset();
	}

	void iniciarDht()
	{
		auto b = actual_;
		if (!b || !cfg_.usarDht) return;
		{
			std::lock_guard<std::mutex> l(b->m);
			if (b->dhtIniciada) return;
			b->dhtIniciada = true;
			b->dhtInicio = ahoraMs();
		}
		std::thread([b] { hiloDht(b); }).detach();
	}

	static void leerContador(Contador& c, const Campos& f)
	{
		if (!f.tiene("conectados")) return;
		std::lock_guard<std::mutex> l(c.m);
		c.conectados = (int)f.numero("conectados", -1);
		c.buscando = (int)f.numero("buscando", -1);
		c.duelos = (int)f.numero("duelos", -1);
	}

	// espera 'ms' en trozos de 100 ms; false si hay que parar
	static bool dormir(Busqueda& b, int ms)
	{
		for (int t = 0; t < ms && !b.parar; t += 100) std::this_thread::sleep_for(std::chrono::milliseconds(100));
		return !b.parar;
	}

	// ---------------------------------------------------------------------------------------- hilo de la sala
	static void hiloNube(std::shared_ptr<Busqueda> b)
	{
		const SalaConfig& c = b->cfg;
		Campos perfil;
		perfilACampos(b->yo, b->mios, perfil);
		bool registrado = false, emparejadoAqui = false;
		while (!b->parar) {
			{
				std::lock_guard<std::mutex> l(b->m);
				if (b->hayPareja) break;   // la DHT encontro antes
			}
			http::Respuesta r;
			if (!registrado) r = http::pedir(c.url + "/v1/buscar", "POST", perfil.texto(), c.esperaPedidoMs, &b->parar);
			else {
				Campos e;
				e.poner("id", b->yo.id);
				e.poner("espera", (long long)c.esperaLargaMs);
				r = http::pedir(c.url + "/v1/estado", "POST", e.texto(), c.esperaLargaMs + c.esperaPedidoMs, &b->parar);
			}
			if (b->parar) break;
			if (r.estado != 200) {
				int fallos;
				{
					std::lock_guard<std::mutex> l(b->m);
					fallos = ++b->nubeFallos;
				}
				if (fallos == 1) fprintf(stderr, "[sala] la sala no respondio (%s, HTTP %d): se reintenta y se busca tambien por la DHT\n", r.error.c_str(), r.estado);
				registrado = false;
				if (!dormir(*b, c.reintentoMs)) break;
				continue;
			}
			const Campos f = Campos::leer(r.cuerpo);
			if (b->contador) leerContador(*b->contador, f);
			const std::string est = f.obtener("estado");
			std::lock_guard<std::mutex> l(b->m);
			b->nubeRespondio = true;
			b->nubeFallos = 0;
			if (est == "emparejado" && !b->hayPareja) {
				Pareja p;
				p.rival.id = f.obtener("rival_id");
				p.rival.nombre = f.obtener("rival_nombre");
				p.rival.elo = (int)f.numero("rival_elo", 1000);
				p.rival.version = f.obtener("rival_version");
				p.rival.plataforma = f.obtener("rival_plataforma");
				p.rival.catalogo = b->yo.catalogo;
				p.candidatos = candidatosDeCampos(f);
				p.idSesion = f.numeroSinSigno("sesion");
				p.soyAnfitrion = f.obtener("anfitrion") == "1";
				b->pareja = p;
				b->hayPareja = true;
				emparejadoAqui = true;
				break;
			}
			if (est == "buscando") registrado = true;
			else registrado = false;   // desconocido (la sala se reinicio o caduco): se vuelve a buscar
		}
		if (!emparejadoAqui) {   // se cancelo o encontro la DHT: la sala deja de ofrecernos
			Campos e;
			e.poner("id", b->yo.id);
			http::pedir(c.url + "/v1/cancelar", "POST", e.texto(), 3000);
		}
	}

	// ---------------------------------------------------------------------------------------- hilo de la DHT
	static void hiloDht(std::shared_ptr<Busqueda> b)
	{
		const SalaConfig& c = b->cfg;
		std::vector<Direccion> arranque = c.arranqueDirecto;
		for (const dht::Arranque& a : c.arranque) {
			if (b->parar) return;
			dns::resolver(a.nombre, false, a.puerto, arranque, 1500);
		}
		dht::Emparejador em;
		em.op.cadaBusquedaMs = c.cadaBusquedaDhtMs;
		if (!em.iniciar(b->yo, b->mios, arranque, c.puertoDht, c.claveDht)) {
			std::lock_guard<std::mutex> l(b->m);
			b->dhtError = true;
			return;
		}
		uint64_t emparejadoEn = 0;
		while (!b->parar) {
			em.paso(50);
			std::lock_guard<std::mutex> l(b->m);
			b->dhtNodos = (int)em.nodo.nodosConocidos();
			if (b->contador && !b->nubeRespondio) {
				std::lock_guard<std::mutex> lc(b->contador->m);
				b->contador->conectados = em.paresVivos() + 1;
				b->contador->buscando = em.paresBuscando() + (b->hayPareja ? 0 : 1);
			}
			Pareja p;
			if (!emparejadoEn && em.pareja(p)) {
				emparejadoEn = ahoraMs();
				if (!b->hayPareja) {
					b->pareja = p;
					b->hayPareja = true;
				}
			}
			if (b->hayPareja && !emparejadoEn) break;   // encontro la sala
			// emparejado por aca: unos segundos mas para contestar los ACEPTO/CONFIRMO repetidos del otro
			if (emparejadoEn && ahoraMs() - emparejadoEn > 8000) break;
		}
		em.cerrar();
	}

	SalaConfig cfg_;
	elo::Datos datos_;
	std::shared_ptr<Busqueda> actual_;
	std::shared_ptr<Contador> contador_ = std::make_shared<Contador>();
	std::shared_ptr<std::atomic<int>> eloSala_ = std::make_shared<std::atomic<int>>(-1);
	Pareja pareja_;
	bool hayPareja_ = false;
	uint64_t reportada_ = 0;
	int eloConfirmado_ = -1;
	int confirmaciones_ = 0;
};

}  // namespace red
