// Prueba de la busqueda de rival en linea (rama classic-sala): src/red/sala.h, dht.h, http.h, elo.h y sala_tipos.h.
//  - bencode/KRPC/SHA-1, texto clave=valor, candidatos y ELO (la misma formula que la sala);
//  - una mini-red DHT en loopback (varios nodos en el proceso): ping, find_node, get_peers, announce_peer y el
//    emparejamiento por la DHT (version distinta no empareja, anfitrion unico, candidatos del otro);
//  - con una URL (el Worker local de servidor/sala: `bash servidor/sala/probar.sh <este binario>` la pasa sola), el
//    cliente C++ contra la sala: emparejar, version distinta, cancelar, resultado y ELO, y que nada bloquee el cuadro;
//  - la sala caida: la Sala cae sola a la DHT (mini-red) y empareja igual.
//  - "--dht-real": una prueba breve contra la DHT publica (no va al ciclo; no imprime IPs).
// Compilar y correr (contenedor arch-ps5):
//   g++ -std=c++17 -O2 -pthread -I remake-ps5/src/red -I remake-ps5/src remake-ps5/src/pruebas/prueba_sala.cpp -o /tmp/prueba_sala
//   /tmp/prueba_sala [http://127.0.0.1:8787] [--dht-real]
// Imprime "OK <caso>" o "FALLA <caso>: ..."; sale con 1 si hubo fallas.
#include <cstdio>
#include <memory>
#include <thread>
#include "red/sala.h"

using namespace red;

static int fallas = 0, casos = 0;
static void caso(const char* id, bool ok, const std::string& detalle = "")
{
	casos++;
	if (ok) printf("OK %s\n", id);
	else {
		fallas++;
		printf("FALLA %s: %s\n", id, detalle.c_str());
	}
	fflush(stdout);
}
static void dormir(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

static Direccion loopback(uint16_t puerto)
{
	Direccion d;
	Direccion::desdeTexto("127.0.0.1", d);
	d.puerto = puerto;
	return d;
}
static Candidato candidato(const char* texto, TipoCandidato t, int prio)
{
	Candidato c;
	Direccion::desdeTexto(texto, c.dir);
	c.tipo = t;
	c.prioridad = prio;
	return c;
}

// ---------------------------------------------------------------------------------------------- 1. piezas
static void pruebaPiezas()
{
	caso("CU-RED-SALA-SHA1", dht::aHex(dht::sha1("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d" &&
		dht::aHex(dht::sha1("")) == "da39a3ee5e6b4b0d3255bfef95601890afd80709" &&
		dht::aHex(dht::sha1(std::string(1000, 'a'))) == "291e9a6c66994949b57ba5e650361e98fc36b1ba", dht::aHex(dht::sha1("abc")));
	caso("CU-RED-SALA-CLAVE-DHT", dht::claveSala() == dht::sha1("yugioh-classic-remaster/1") && dht::claveSala().size() == 20);

	// el ejemplo de BEP 5 (ping) ida y vuelta
	const std::string ping = "d1:ad2:id20:abcdefghij0123456789e1:q4:ping1:t2:aa1:y1:qe";
	dht::Bval b;
	const bool leyo = dht::Bval::decodificar(ping, b);
	caso("CU-RED-SALA-BENCODE-LEER", leyo && b.cadena("q") == "ping" && b.buscar("a")->cadena("id") == "abcdefghij0123456789" && b.cadena("t") == "aa");
	caso("CU-RED-SALA-BENCODE-ESCRIBIR", leyo && b.codificado() == ping, b.codificado());
	dht::Bval x = dht::Bval::dic();
	dht::Bval l = dht::Bval::lista();
	l.l.push_back(dht::Bval::ent(-42));
	l.l.push_back(dht::Bval::cad(std::string("\0\1:e", 4)));
	x.poner("zeta", l).poner("alfa", dht::Bval::ent(7));
	dht::Bval y;
	caso("CU-RED-SALA-BENCODE-ORDEN", x.codificado() == std::string("d4:alfai7e4:zetali-42e4:\0\1:eee", 30) &&
		dht::Bval::decodificar(x.codificado(), y) && y.buscar("zeta")->l[0].i == -42);
	dht::Bval z;
	caso("CU-RED-SALA-BENCODE-MALO", !dht::Bval::decodificar("d1:a", z) && !dht::Bval::decodificar("i12", z) &&
		!dht::Bval::decodificar("5:abc", z) && !dht::Bval::decodificar("di1ei2ee", z) && !dht::Bval::decodificar(std::string(100, 'l'), z) &&
		!dht::Bval::decodificar("i1ex", z));

	// texto clave=valor y candidatos
	Perfil p{ "00112233445566778899aabbccddeeff", "Yugi\nMuto", 1234, "1.0", "ps5", "goat" };
	std::vector<Candidato> c = { candidato("192.168.1.5:5000", TipoCandidato::Local, 100),
		candidato("[2001:db8::7]:6000", TipoCandidato::Ipv6, 90), candidato("203.0.113.9:7000", TipoCandidato::Stun, 50) };
	Campos f;
	perfilACampos(p, c, f);
	const Campos g = Campos::leer(f.texto());
	const auto c2 = candidatosDeCampos(g);
	caso("CU-RED-SALA-CAMPOS", g.obtener("nombre") == "Yugi Muto" && g.numero("elo") == 1234 && g.obtener("plataforma") == "ps5" &&
		c2.size() == 3 && c2[1].dir == c[1].dir && c2[1].tipo == TipoCandidato::Ipv6 && c2[2].prioridad == 50, f.texto());
	Candidato malo;
	caso("CU-RED-SALA-CANDIDATO-MALO", !candidatoDeTexto("raro,1,1.2.3.4:5", malo) && !candidatoDeTexto("local,1,1.2.3.4", malo) &&
		!candidatoDeTexto("local,1", malo));
	caso("CU-RED-SALA-ANFITRION-REGLA", esAnfitrion("0a", "0b") && !esAnfitrion("0b", "0a"));

	// ELO: la misma formula que el Worker (1000 vs 1000: +16 / -16)
	caso("CU-RED-SALA-ELO-FORMULA", elo::nuevo(1000, 1000, true) == 1016 && elo::nuevo(1000, 1000, false) == 984 &&
		elo::nuevo(1400, 1000, true) == 1403 && elo::nuevo(1000, 1400, true) == 1029,
		std::to_string(elo::nuevo(1400, 1000, true)) + " " + std::to_string(elo::nuevo(1000, 1400, true)));
	const std::string ruta = "/tmp/prueba_sala_en_linea_" + std::to_string(getpid()) + ".txt";
	unlink(ruta.c_str());
	elo::Datos d = elo::cargar(ruta);
	const std::string id = d.id;
	elo::anotar(d, 1000, true);
	elo::guardar(d, ruta);
	const elo::Datos e = elo::cargar(ruta);
	caso("CU-RED-SALA-ELO-GUARDADO-LOCAL", id.size() == 32 && e.id == id && e.elo == 1016 && e.partidas == 1 && e.ganadas == 1 &&
		access((ruta + ".tmp").c_str(), F_OK) != 0, id);
	caso("CU-RED-SALA-ID-ALEATORIO", idAleatorio() != idAleatorio() && elo::deTexto("id=NO-HEX\nelo=99999\n").elo == elo::INICIAL);
	unlink(ruta.c_str());
}

// ---------------------------------------------------------------------------------------------- 2. mini-red DHT
struct MiniRed {
	std::vector<std::unique_ptr<dht::Nodo>> nodos;
	std::atomic<bool> parar{ false };
	std::thread hilo;
	std::mutex m;

	explicit MiniRed(int n)
	{
		for (int i = 0; i < n; i++) {
			nodos.emplace_back(new dht::Nodo());
			nodos.back()->abrir(0);
		}
		// cada uno arranca con el primero; el primero con el ultimo
		for (int i = 1; i < n; i++) nodos[i]->arrancar({ loopback(nodos[0]->puerto()) });
		nodos[0]->arrancar({ loopback(nodos[n - 1]->puerto()) });
		bombear(400);
		for (auto& x : nodos) x->refrescar();
		bombear(400);
	}
	~MiniRed() { detener(); }
	void bombear(int ms)
	{
		const uint64_t fin = ahoraMs() + (uint64_t)ms;
		while (ahoraMs() < fin) for (auto& x : nodos) x->paso(1);
	}
	void enHilo()
	{
		hilo = std::thread([this] { while (!parar) for (auto& x : nodos) x->paso(1); });
	}
	void detener()
	{
		parar = true;
		if (hilo.joinable()) hilo.join();
	}
	std::vector<Direccion> arranque() const { return { loopback(nodos[0]->puerto()), loopback(nodos[1]->puerto()) }; }
};

static void pruebaKrpc()
{
	MiniRed red(12);
	size_t minimo = 1000;
	for (auto& n : red.nodos) minimo = std::min(minimo, n->nodosConocidos());
	caso("CU-RED-SALA-KRPC-FIND-NODE", minimo >= 3, "el que menos conoce: " + std::to_string(minimo));

	// ping a mano: un nodo nuevo manda un ping crudo y recibe la respuesta con el id
	SocketUdp s;
	s.abrir(0);
	dht::Bval a = dht::Bval::dic();
	a.poner("id", dht::Bval::cad(std::string(20, 'x')));
	dht::Bval q = dht::Bval::dic();
	q.poner("t", dht::Bval::cad("pp")).poner("y", dht::Bval::cad("q")).poner("q", dht::Bval::cad("ping")).poner("a", a);
	const std::string ping = q.codificado();
	s.enviar(loopback(red.nodos[3]->puerto()), (const uint8_t*)ping.data(), ping.size());
	red.bombear(100);
	uint8_t buf[1500];
	Direccion de;
	const int n = s.recibir(de, buf, sizeof buf, 300);
	dht::Bval r;
	const bool ok = n > 0 && dht::Bval::decodificar(std::string((char*)buf, (size_t)n), r) && r.cadena("y") == "r" && r.cadena("t") == "pp" &&
		r.buscar("r") && r.buscar("r")->cadena("id") == red.nodos[3]->id();
	caso("CU-RED-SALA-KRPC-PING", ok, std::to_string(n));
	// un metodo desconocido devuelve error 204
	q.poner("q", dht::Bval::cad("vote"));
	const std::string voto = q.codificado();
	s.enviar(loopback(red.nodos[3]->puerto()), (const uint8_t*)voto.data(), voto.size());
	red.bombear(100);
	const int n2 = s.recibir(de, buf, sizeof buf, 300);
	dht::Bval e;
	caso("CU-RED-SALA-KRPC-ERROR", n2 > 0 && dht::Bval::decodificar(std::string((char*)buf, (size_t)n2), e) && e.cadena("y") == "e" &&
		e.buscar("e")->l[0].i == 204);

	// get_peers + announce_peer: el nodo 5 se anuncia en una clave; el nodo 9 la busca y lo encuentra
	const std::string clave = dht::sha1("prueba-anuncio");
	red.nodos[5]->buscarPares(clave, true);
	const uint64_t t0 = ahoraMs();
	while (red.nodos[5]->buscando() && ahoraMs() - t0 < 5000) red.bombear(20);
	red.bombear(300);
	caso("CU-RED-SALA-KRPC-ANNOUNCE", red.nodos[5]->anunciosHechos() >= 1, std::to_string(red.nodos[5]->anunciosHechos()));
	red.nodos[9]->buscarPares(clave, false);
	const uint64_t t1 = ahoraMs();
	while (red.nodos[9]->buscando() && ahoraMs() - t1 < 5000) red.bombear(20);
	bool hallado = false;
	for (const Direccion& d : red.nodos[9]->pares()) if (d == loopback(red.nodos[5]->puerto())) hallado = true;
	caso("CU-RED-SALA-KRPC-GET-PEERS", hallado, std::to_string(red.nodos[9]->pares().size()) + " pares");
	// announce con token falso: rechazado
	dht::Bval an = dht::Bval::dic();
	an.poner("id", dht::Bval::cad(std::string(20, 'y'))).poner("info_hash", dht::Bval::cad(clave)).poner("port", dht::Bval::ent(9))
		.poner("token", dht::Bval::cad("falso"));
	dht::Bval qa = dht::Bval::dic();
	qa.poner("t", dht::Bval::cad("an")).poner("y", dht::Bval::cad("q")).poner("q", dht::Bval::cad("announce_peer")).poner("a", an);
	const std::string anuncio = qa.codificado();
	s.enviar(loopback(red.nodos[2]->puerto()), (const uint8_t*)anuncio.data(), anuncio.size());
	red.bombear(100);
	// (el socket de la prueba ya esta en las tablas: puede llegar alguna consulta de los nodos antes que la respuesta)
	bool rechazado = false;
	for (int i = 0; i < 20 && !rechazado; i++) {
		const int n3 = s.recibir(de, buf, sizeof buf, 100);
		dht::Bval ea;
		if (n3 > 0 && dht::Bval::decodificar(std::string((char*)buf, (size_t)n3), ea) && ea.cadena("t") == "an") rechazado = ea.cadena("y") == "e";
		red.bombear(5);
	}
	caso("CU-RED-SALA-KRPC-TOKEN", rechazado);
}

static Perfil perfil(const char* id, int elo, const char* version, const char* plataforma = "ps5")
{
	Perfil p;
	p.id = id;
	p.nombre = std::string("Duelista ") + id;
	p.elo = elo;
	p.version = version;
	p.plataforma = plataforma;
	p.catalogo = "goat";
	return p;
}

static void pruebaEmparejadorDht()
{
	MiniRed red(10);
	red.enHilo();
	// dos de la misma version y uno de otra
	dht::Emparejador a, b, c;
	for (dht::Emparejador* x : { &a, &b, &c }) { x->op.cadaBusquedaMs = 1500; }
	const std::vector<Candidato> ca = { candidato("10.0.0.1:5001", TipoCandidato::Local, 100) };
	const std::vector<Candidato> cb = { candidato("10.0.0.2:5002", TipoCandidato::Local, 100), candidato("[2001:db8::2]:5002", TipoCandidato::Ipv6, 90) };
	a.iniciar(perfil("1111111111111111aaaa", 1000, "1.0"), ca, red.arranque());
	b.iniciar(perfil("2222222222222222bbbb", 1300, "1.0", "pc"), cb, red.arranque());
	c.iniciar(perfil("0000000000000000cccc", 1000, "2.0"), ca, red.arranque());
	const uint64_t t0 = ahoraMs();
	while (ahoraMs() - t0 < 15000 && !(a.fase() == dht::Emparejador::Emparejado && b.fase() == dht::Emparejador::Emparejado)) {
		a.paso(2);
		b.paso(2);
		c.paso(2);
	}
	Pareja pa, pb;
	const bool ok = a.pareja(pa) && b.pareja(pb);
	caso("CU-RED-SALA-DHT-EMPAREJA", ok && pa.rival.id == "2222222222222222bbbb" && pb.rival.id == "1111111111111111aaaa",
		"fases " + std::to_string(a.fase()) + " " + std::to_string(b.fase()) + " en " + std::to_string(ahoraMs() - t0) + " ms");
	caso("CU-RED-SALA-DHT-SESION", ok && pa.idSesion == pb.idSesion && pa.idSesion != 0 && pa.porDht);
	caso("CU-RED-SALA-DHT-ANFITRION", ok && pa.soyAnfitrion && !pb.soyAnfitrion);
	caso("CU-RED-SALA-DHT-CANDIDATOS", ok && pa.candidatos.size() == 2 && pa.candidatos[1].dir == cb[1].dir && pb.candidatos.size() == 1 &&
		pa.rival.plataforma == "pc" && pa.rival.elo == 1300);
	for (int k = 0; k < 1500 && c.paresVivos() == 0; k++) c.paso(2);   // (c tiene que llegar a ver a los otros)
	caso("CU-RED-SALA-DHT-VERSION-DISTINTA", c.fase() == dht::Emparejador::Libre && c.paresVivos() >= 1, std::to_string(c.paresVivos()));
	printf("  (mini-red: emparejados en %llu ms)\n", (unsigned long long)(ahoraMs() - t0));
	// cuatro a la vez: dos parejas, nadie con dos
	dht::Emparejador e[4];
	const char* ids[4] = { "a1aaaaaaaaaaaaaaaaaa", "b2bbbbbbbbbbbbbbbbbb", "c3cccccccccccccccccc", "d4dddddddddddddddddd" };
	const int elos[4] = { 1000, 1800, 1010, 1790 };
	for (int i = 0; i < 4; i++) {
		e[i].op.cadaBusquedaMs = 1500;
		e[i].iniciar(perfil(ids[i], elos[i], "3.0"), ca, red.arranque());
	}
	const uint64_t t1 = ahoraMs();
	auto todos = [&] { for (auto& x : e) if (x.fase() != dht::Emparejador::Emparejado) return false; return true; };
	while (ahoraMs() - t1 < 20000 && !todos())
		for (auto& x : e) x.paso(2);
	for (int k = 0; k < 300; k++) for (auto& x : e) x.paso(2);   // (los CONFIRMO que quedan)
	bool coherente = todos();
	std::string det;
	for (int i = 0; i < 4 && coherente; i++) {
		Pareja p;
		e[i].pareja(p);
		int j = -1;
		for (int k = 0; k < 4; k++) if (p.rival.id == ids[k]) j = k;
		Pareja q;
		if (j < 0 || !e[j].pareja(q) || q.rival.id != ids[i] || q.idSesion != p.idSesion) coherente = false;
		det += std::string(ids[i]).substr(0, 2) + "-" + p.rival.id.substr(0, 2) + " ";
	}
	caso("CU-RED-SALA-DHT-CUATRO", coherente, det);
	red.detener();
}

// ---------------------------------------------------------------------------------------------- 3. contra el Worker
static SalaConfig configNube(const std::string& url, const std::string& ruta)
{
	SalaConfig c;
	c.url = url;
	c.usarDht = false;
	c.esperaLargaMs = 4000;
	c.rutaElo = ruta;
	return c;
}
template <class F> static bool esperarHasta(F cond, int ms, Sala* s1 = nullptr, Sala* s2 = nullptr)
{
	const uint64_t fin = ahoraMs() + (uint64_t)ms;
	while (ahoraMs() < fin) {
		if (s1) s1->actualizar();
		if (s2) s2->actualizar();
		if (cond()) return true;
		dormir(16);   // un cuadro
	}
	return cond();
}

static void pruebaWorker(const std::string& url)
{
	const std::string r1 = "/tmp/prueba_sala_a_" + std::to_string(getpid()) + ".txt", r2 = "/tmp/prueba_sala_b_" + std::to_string(getpid()) + ".txt";
	unlink(r1.c_str());
	unlink(r2.c_str());
	Sala a(configNube(url, r1)), b(configNube(url, r2));
	const std::vector<Candidato> ca = { candidato("192.168.0.10:7000", TipoCandidato::Local, 100), candidato("198.51.100.4:7000", TipoCandidato::Stun, 50) };
	const std::vector<Candidato> cb = { candidato("[2001:db8::b]:7001", TipoCandidato::Ipv6, 90) };
	const std::string version = "prueba-" + std::to_string(getpid());
	a.buscar(a.perfilLocal("Yugi", version, "ps5", "goat"), ca);
	// nada bloquea: estado() y actualizar() en cuadros mientras el hilo hace long-polling
	uint64_t peor = 0;
	for (int i = 0; i < 60; i++) {
		const uint64_t t = ahoraMs();
		a.actualizar();
		a.estado();
		a.enLinea();
		peor = std::max(peor, ahoraMs() - t);
		dormir(16);
	}
	caso("CU-RED-SALA-NO-BLOQUEA", peor <= 5 && a.estado() == Sala::Buscando && a.salaResponde(), "peor cuadro " + std::to_string(peor) + " ms");
	b.buscar(b.perfilLocal("Kaiba", version, "pc", "goat"), cb);
	const bool emp = esperarHasta([&] { return a.estado() == Sala::Emparejado && b.estado() == Sala::Emparejado; }, 10000, &a, &b);
	Pareja pa, pb;
	a.pareja(pa);
	b.pareja(pb);
	caso("CU-RED-SALA-CLIENTE-EMPAREJA", emp && pa.rival.nombre == "Kaiba" && pb.rival.nombre == "Yugi" && pa.idSesion == pb.idSesion && pa.idSesion != 0);
	caso("CU-RED-SALA-CLIENTE-ANFITRION", emp && pa.soyAnfitrion != pb.soyAnfitrion && pa.soyAnfitrion == (a.datosElo().id < b.datosElo().id));
	caso("CU-RED-SALA-CLIENTE-CANDIDATOS", emp && pa.candidatos.size() == 1 && pa.candidatos[0].dir == cb[0].dir && pb.candidatos.size() == 2 &&
		pb.candidatos[1].tipo == TipoCandidato::Stun && pa.rival.plataforma == "pc" && !pa.porDht);
	a.consultarEnLinea();
	esperarHasta([&] { return a.enLinea() >= 2; }, 5000);
	caso("CU-RED-SALA-CLIENTE-EN-LINEA", a.enLinea() >= 2 && a.duelosEnCurso() >= 1, std::to_string(a.enLinea()));
	// resultado de los dos lados: cuenta y la sala confirma el ELO (manda sobre el local)
	a.reportarResultado(pa.idSesion, true);
	b.reportarResultado(pb.idSesion, false);
	const bool confirmado = esperarHasta([&] { return a.eloConfirmado() > 0 && b.eloConfirmado() > 0; }, 30000, &a, &b);
	caso("CU-RED-SALA-CLIENTE-RESULTADO", confirmado && a.eloConfirmado() == 1016 && b.eloConfirmado() == 984 && a.datosElo().elo == 1016,
		std::to_string(a.eloConfirmado()) + " " + std::to_string(b.eloConfirmado()));
	caso("CU-RED-SALA-CLIENTE-ELO-ARCHIVO", elo::cargar(r1).elo == 1016 && elo::cargar(r2).elo == 984 && elo::cargar(r1).partidas == 1);
	// version distinta: no empareja
	Sala c(configNube(url, r1 + "c")), d(configNube(url, r2 + "d"));
	c.buscar(c.perfilLocal("Joey", version + "-x", "ps5"), ca);
	d.buscar(d.perfilLocal("Mai", version + "-y", "ps5"), ca);
	const bool mal = esperarHasta([&] { return c.estado() == Sala::Emparejado || d.estado() == Sala::Emparejado; }, 3000, &c, &d);
	caso("CU-RED-SALA-CLIENTE-VERSION", !mal && c.estado() == Sala::Buscando && d.estado() == Sala::Buscando);
	// cancelar: no espera al hilo y la sala deja de contarlo
	const uint64_t t = ahoraMs();
	c.cancelar();
	d.cancelar();
	const uint64_t tarda = ahoraMs() - t;
	Sala consulta(configNube(url, r1 + "e"));
	const bool bajo = esperarHasta([&] {
		consulta.consultarEnLinea();
		dormir(300);
		return consulta.buscandoAhora() == 0;
	}, 8000);
	caso("CU-RED-SALA-CLIENTE-CANCELAR", tarda <= 5 && c.estado() == Sala::SinConexion && bajo, std::to_string(tarda) + " ms, buscando " + std::to_string(consulta.buscandoAhora()));
	for (const std::string& r : { r1, r2, r1 + "c", r2 + "d", r1 + "e" }) unlink(r.c_str());
}

// ---------------------------------------------------------------------------------------------- 4. sala caida -> DHT
static void pruebaRespaldo()
{
	MiniRed red(8);
	red.enHilo();
	auto config = [&](const std::string& ruta) {
		SalaConfig c;
		c.url = "http://127.0.0.1:1";   // nadie escucha: la sala "no responde"
		c.reintentoMs = 500;
		c.arranque.clear();
		c.arranqueDirecto = red.arranque();
		c.cadaBusquedaDhtMs = 1500;
		c.rutaElo = ruta;
		return c;
	};
	const std::string r1 = "/tmp/prueba_sala_r1_" + std::to_string(getpid()), r2 = "/tmp/prueba_sala_r2_" + std::to_string(getpid());
	Sala a(config(r1)), b(config(r2));
	a.buscar(a.perfilLocal("Yugi", "respaldo", "ps5"), { candidato("10.1.1.1:6000", TipoCandidato::Local, 100) });
	b.buscar(b.perfilLocal("Kaiba", "respaldo", "pc"), { candidato("10.1.1.2:6000", TipoCandidato::Local, 100) });
	const bool dht = esperarHasta([&] { return a.usandoDht() && b.usandoDht(); }, 6000, &a, &b);
	caso("CU-RED-SALA-RESPALDO-DHT-ARRANCA", dht && !a.salaResponde());
	const bool emp = esperarHasta([&] { return a.estado() == Sala::Emparejado && b.estado() == Sala::Emparejado; }, 20000, &a, &b);
	Pareja pa, pb;
	a.pareja(pa);
	b.pareja(pb);
	caso("CU-RED-SALA-RESPALDO-EMPAREJA", emp && pa.porDht && pa.idSesion == pb.idSesion && pa.soyAnfitrion != pb.soyAnfitrion &&
		pa.rival.nombre == "Kaiba" && pa.candidatos.size() == 1);
	// sin sala ni nodos DHT: SinConexion
	SalaConfig sin;
	sin.url = "http://127.0.0.1:1";
	sin.reintentoMs = 300;
	sin.arranque.clear();
	sin.sinNodosMs = 1500;
	sin.rutaElo = r1 + "s";
	Sala s(sin);
	s.buscar(s.perfilLocal("Solo", "respaldo", "ps5"), {});
	const bool sinCon = esperarHasta([&] { return s.estado() == Sala::SinConexion; }, 6000, &s);
	caso("CU-RED-SALA-SIN-CONEXION", sinCon);
	s.cancelar();
	a.reportarResultado(pa.idSesion, true);   // por DHT: solo local
	caso("CU-RED-SALA-RESPALDO-ELO-LOCAL", a.datosElo().elo == 1016 && elo::cargar(r1).elo == 1016);
	red.detener();
	for (const std::string& r : { r1, r2, r1 + "s" }) unlink(r.c_str());
}

// ---------------------------------------------------------------------------------------------- 5. DHT publica
static void pruebaDhtReal()
{
	dht::Emparejador e;
	std::vector<Direccion> arranque;
	for (const dht::Arranque& a : dht::arranquePublico()) dns::resolver(a.nombre, false, a.puerto, arranque, 2000);
	caso("CU-RED-SALA-DHT-REAL-ARRANQUE-DNS", !arranque.empty(), "");
	e.op.cadaBusquedaMs = 12000;
	const std::string clave = dht::claveSala("prueba-" + std::to_string(getpid()));   // una clave propia de la prueba
	e.iniciar(perfil("ffffffffffffffffffff", 1000, "real"), {}, arranque, 0, clave);
	const uint64_t t0 = ahoraMs();
	while (ahoraMs() - t0 < 30000 && e.nodo.busquedasTerminadas() < 2) e.paso(20);
	printf("  (DHT publica: %zu nodos conocidos, %d busquedas, %d anuncios aceptados, %zu pares, %llu ms)\n", e.nodo.nodosConocidos(),
		e.nodo.busquedasTerminadas(), e.nodo.anunciosHechos(), e.nodo.pares().size(), (unsigned long long)(ahoraMs() - t0));
	caso("CU-RED-SALA-DHT-REAL-NODOS", e.nodo.nodosConocidos() >= 8);
	caso("CU-RED-SALA-DHT-REAL-ANUNCIO", e.nodo.anunciosHechos() >= 1);
	// la segunda busqueda deberia encontrarnos a nosotros mismos (nuestro anuncio) en la clave propia
	caso("CU-RED-SALA-DHT-REAL-SE-ENCUENTRA", !e.nodo.pares().empty());
}

int main(int argc, char** argv)
{
	std::string url;
	bool real = false;
	for (int i = 1; i < argc; i++) {
		const std::string a = argv[i];
		if (a == "--dht-real") real = true;
		else url = a;
	}
	if (real) {
		pruebaDhtReal();
	} else {
		pruebaPiezas();
		pruebaKrpc();
		pruebaEmparejadorDht();
		if (!url.empty()) pruebaWorker(url);
		else printf("(sin URL del Worker: se saltan los casos del cliente contra la sala)\n");
		pruebaRespaldo();
	}
	printf("%s: %d casos, %d fallas\n", fallas ? "PRUEBA CON FALLOS" : "TODO OK", casos, fallas);
	return fallas ? 1 : 0;
}
