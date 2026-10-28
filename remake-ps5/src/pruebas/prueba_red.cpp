// Prueba del modo en linea (rama classic-red-base, src/red/): socket UDP, SHA-256, canal confiable con perdida,
// duplicado y desorden simulados, caida y reconexion, abandono, version y catalogo distintos, duelos completos en
// lockstep (dos motores en el mismo proceso, la IA juega por cada lado) con el mismo resultado en las dos maquinas,
// desincronizacion forzada, barajado con compromiso (verificado y trampas detectadas), la moneda y la red local.
// Cada caso imprime "CU-RED-<caso> OK" o "FALLA CU-RED-<caso>: ..." (casos_suites.txt).
// Compilar y correr (contenedor arch-ps5; ~1 min):
//   g++ -std=c++17 -O2 -Wall -Wextra -I remake-ps5/src remake-ps5/src/pruebas/prueba_red.cpp -o /tmp/prueba_red &&
//   /tmp/prueba_red <carpeta con cartas_ui.txt, cartas_classic.txt y los datos de la rama>/ [duelos]
#include "ia.h"
#include "red/duelo_red.h"
#include "red/lan.h"
#include "red/espejo.h"

#include <map>
#include <set>
#include <sys/stat.h>
#include <thread>

using namespace red;

static int fallos = 0;
static void caso(const char* id, bool ok, const std::string& detalle = {})
{
	if (ok) fprintf(stderr, "%s OK %s\n", id, detalle.c_str());
	else {
		fallos++;
		fprintf(stderr, "FALLA %s: %s\n", id, detalle.c_str());
	}
}

// ------------------------------------------------------------------------------------------------- red simulada
// dos puntas; cada paquete puede perderse, duplicarse o llegar tarde (desorden). El tiempo es virtual
struct RedSimulada {
	struct Paquete {
		int a;
		Direccion de;
		std::vector<uint8_t> datos;
		uint64_t llega;
	};
	struct Punta : Transporte {
		RedSimulada* r{ nullptr };
		int yo{ 0 };
		bool enviar(const Direccion& a, const uint8_t* d, size_t n) override { return r->mandar(yo, a, d, n); }
		int recibir(Direccion& de, uint8_t* buf, size_t cap) override { return r->sacar(yo, de, buf, cap); }
	};
	uint64_t ahora{ 0 };
	ia::Azar azar;
	float perdida{ 0 }, duplicado{ 0 }, desorden{ 0 };
	uint64_t latencia{ 15 };
	bool cortado[2]{};
	Direccion dir[2];
	Punta punta[2];
	std::vector<Paquete> cola;
	int mandados{ 0 }, perdidos{ 0 };
	explicit RedSimulada(uint32_t semilla) : azar(semilla)
	{
		Direccion::desdeTexto("10.0.0.1:4000", dir[0]);
		Direccion::desdeTexto("10.0.0.2:5000", dir[1]);
		for (int k = 0; k < 2; k++) {
			punta[k].r = this;
			punta[k].yo = k;
		}
	}
	bool mandar(int de, const Direccion& a, const uint8_t* d, size_t n)
	{
		mandados++;
		int destino = -1;
		for (int k = 0; k < 2; k++)
			if (dir[k] == a) destino = k;
		if (destino < 0 || cortado[de] || cortado[destino]) return true;
		if (azar.uno() < perdida) {
			perdidos++;
			return true;
		}
		const int copias = azar.uno() < duplicado ? 2 : 1;
		for (int c = 0; c < copias; c++) {
			Paquete p{ destino, dir[de], std::vector<uint8_t>(d, d + n), ahora + latencia };
			if (azar.uno() < desorden) p.llega += 1 + (uint64_t)azar.hasta(80);
			cola.push_back(p);
		}
		return true;
	}
	int sacar(int yo, Direccion& de, uint8_t* buf, size_t cap)
	{
		for (size_t i = 0; i < cola.size(); i++)
			if (cola[i].a == yo && cola[i].llega <= ahora) {
				const size_t n = std::min(cap, cola[i].datos.size());
				memcpy(buf, cola[i].datos.data(), n);
				de = cola[i].de;
				cola.erase(cola.begin() + (long)i);
				return (int)n;
			}
		return 0;
	}
	// un paquete cualquiera (ajeno) para el lado "a"
	void inyectar(int a, const std::vector<uint8_t>& d) { cola.push_back({ a, dir[1 - a], d, ahora }); }
};

static Hash entropia(uint32_t k, int lado) { return Sha256().agregarTexto("prueba").agregarU32(k).agregarI32(lado).fin(); }

// un duelo entre dos DueloRed sobre la red simulada, la IA juega por los dos lados
struct Partida {
	RedSimulada red;
	DueloRed a, b;
	std::unique_ptr<ManoIA> ma, mb;
	ConfigDuelo ca, cb;
	Partida(const ia::Base* base, uint32_t semilla) : red(semilla), a(base), b(base) {}
	void empezar(uint32_t k)
	{
		ca.entropia = entropia(k, 0);
		cb.entropia = entropia(k, 1);
		ca.elegirOrdenAuto = cb.elegirOrdenAuto = true;
		ca.irPrimeroSiGano = (k & 2) != 0;
		cb.irPrimeroSiGano = (k & 4) != 0;
		a.anfitrion(&red.punta[0], ca, red.ahora);
		b.invitado(&red.punta[1], red.dir[0], cb, red.ahora);
	}
	void manos(const ia::Base& base, uint32_t k)
	{
		const ia::Rival& ra = base.rivales[(size_t)(k * 7u % base.rivales.size())];
		const ia::Rival& rb = base.rivales[(size_t)(k * 13u % base.rivales.size())];
		ma.reset(new ManoIA(a, ra.personalidad(), ra.nivel, k * 3u + 1u));
		mb.reset(new ManoIA(b, rb.personalidad(), rb.nivel, k * 5u + 2u));
	}
	// avanza "ms" de tiempo virtual (pasos de 5 ms); las manos juegan si les toca
	void correr(uint64_t ms, bool jugarA = true, bool jugarB = true, uint64_t paso = 5)
	{
		const uint64_t fin = red.ahora + ms;
		while (red.ahora < fin) {
			red.ahora += paso;
			a.actualizar(red.ahora);
			b.actualizar(red.ahora);
			for (int k = 0; k < 6; k++) {
				bool alguna = false;
				if (jugarA && ma) alguna |= ma->paso(a, red.ahora);
				if (jugarB && mb) alguna |= mb->paso(b, red.ahora);
				if (!alguna) break;
			}
			if (a.estado() == D_TERMINADO && b.estado() == D_TERMINADO && a.verificado() && b.verificado()) break;
		}
	}
	bool terminado() const { return a.estado() == D_TERMINADO && b.estado() == D_TERMINADO; }
};

// rama classic-en-linea: el invitado decide mirando la VISTA EN ESPEJO (como en el juego: sus cartas son el lado 0 de
// la copia) y su jugada vuelve al lado real con espejoJugada
struct ManoEspejo {
	ia::Duelo copia;
	ia::Turno t;
	int rechazadas{ 0 };
	ManoEspejo(const ia::Personalidad& p, int nivel, uint32_t semilla) : t(&copia, 0, p, nivel, semilla) {}
	bool paso(DueloRed& r, uint64_t ahora)
	{
		if (!r.meToca()) return false;
		copia = vistaDe(r.duelo(), r.yo());
		if (copia.decide() != 0) return false;   // (la vista en espejo tiene que preguntarle al "jugador")
		if (t.turnoVisto != copia.turno) {
			t.turnoVisto = copia.turno;
			t.jugadasEsteTurno = 0;
			if (copia.activo == 0) t.cerebro.alEmpezarTurno(copia, 0);
		}
		ia::Jugada j = t.cerebro.decidir(copia);
		if (r.yo() == 1) j = espejoJugada(j);
		return r.jugarConRespaldo(j, ahora, &t.jugadasEsteTurno, &rechazadas);
	}
};

static std::vector<std::string> mazoAzar(const ia::Base& base, uint32_t s, std::vector<std::string>* fusion)
{
	const ia::Rival& r = base.rivales[(size_t)(s % base.rivales.size())];
	ia::MazoRival m = base.mazoDe(r, s);
	*fusion = m.fusion;
	return m.principal;
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		fprintf(stderr, "uso: prueba_red <carpeta ui/> [duelos]\n");
		return 2;
	}
	const int N = argc > 2 ? atoi(argv[2]) : 200;
	const std::string ui = argv[1];
	ia::Base base;
	if (!base.leer(ui)) return 1;
	const Hash catalogo = hashCatalogo(base);

	// ---- SHA-256 (vectores de FIPS 180-2)
	{
		const bool ok = hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" &&
		                hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" &&
		                hex(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")) ==
		                    "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1";
		std::string mil(1000000, 'a');
		caso("CU-RED-SHA256", ok && hex(sha256(mil)) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
	}

	// ---- socket UDP de verdad en loopback (IPv4 e IPv6) y Direccion
	{
		bool ok = true;
		std::string det;
		SocketUdp s1, s2;
		ok &= s1.abrir(0, true) && s2.abrir(0, false) && s1.puertoLocal() && s2.puertoLocal();
		Direccion d;
		ok &= Direccion::desdeTexto("127.0.0.1:" + std::to_string(s2.puertoLocal()), d);
		const uint8_t m[5] = { 1, 2, 3, 4, 5 };
		uint8_t buf[64];
		Direccion de;
		ok &= s1.enviar(d, m, 5) && s2.recibir(de, buf, 64, 500) == 5 && buf[4] == 5 && de.puerto == s1.puertoLocal() && !de.v6;
		d.puerto = s1.puertoLocal();
		ok &= s2.enviar(d, m, 3) && s1.recibir(de, buf, 64, 500) == 3;   // (dual-stack: IPv4 a un socket IPv6)
		SocketUdp s6;
		Direccion l6;
		if (s6.abrir(0, true) && Direccion::desdeTexto("[::1]:" + std::to_string(s1.puertoLocal()), l6) && s6.enviar(l6, m, 4)) {
			ok &= s1.recibir(de, buf, 64, 500) == 4 && de.v6;
			det += "ipv6 si; ";
		} else
			det += "ipv6 no disponible; ";
		ok &= s1.recibir(de, buf, 64, 0) == 0;   // (nada mas)
		Direccion x;
		ok &= Direccion::desdeTexto("[2001:db8::1]:5000", x) && x.v6 && x.texto() == "[2001:db8::1]:5000";
		ok &= Direccion::desdeTexto("192.168.1.20:47321", x) && !x.v6 && x.texto() == "192.168.1.20:47321";
		ok &= !Direccion::desdeTexto("no.es.ip", x) && !Direccion::desdeTexto("1.2.3.4:99999", x);
		det += std::to_string(direccionesLocales().size()) + " direcciones locales";
		caso("CU-RED-SOCKET", ok, det);
	}

	// ---- canal confiable: perdida, duplicado y desorden
	{
		RedSimulada r(77);
		r.perdida = 0.25f;
		r.duplicado = 0.15f;
		r.desorden = 0.3f;
		Canal c0, c1;
		Identidad id;
		id.catalogo = catalogo;
		c0.usar(&r.punta[0], id, 1);
		c1.usar(&r.punta[1], id, 2);
		c0.escuchar(0);
		c1.conectar(r.dir[0], 0);
		ia::Azar az(5);
		std::vector<std::vector<uint8_t>> mand[2], rec[2];
		for (int k = 0; k < 600; k++)
			for (int lado = 0; lado < 2; lado++) {
				std::vector<uint8_t> m((size_t)(1 + az.hasta(k % 50 == 0 ? 4000 : 300)));
				for (uint8_t& b : m) b = (uint8_t)az.siguiente();
				mand[lado].push_back(m);
				(lado ? c1 : c0).enviar(m, 0);
			}
		for (int t = 0; t < 120000 && (rec[0].size() < 600 || rec[1].size() < 600); t += 5) {
			r.ahora = (uint64_t)t;
			c0.actualizar(r.ahora);
			c1.actualizar(r.ahora);
			std::vector<uint8_t> m;
			while (c0.recibir(m)) rec[0].push_back(m);
			while (c1.recibir(m)) rec[1].push_back(m);
		}
		const bool ok = rec[1] == mand[0] && rec[0] == mand[1] && c0.estado() == C_CONECTADO && c1.estado() == C_CONECTADO;
		caso("CU-RED-CANAL-PERDIDA", ok,
		     "perdidos " + std::to_string(r.perdidos) + " de " + std::to_string(r.mandados) + ", reenviados " +
		         std::to_string(c0.cuentas.reenviados + c1.cuentas.reenviados) + ", duplicados " + std::to_string(c0.cuentas.duplicados + c1.cuentas.duplicados) +
		         ", desordenados " + std::to_string(c0.cuentas.desordenados + c1.cuentas.desordenados));

		// caida y reconexion (con cambio de direccion del invitado: otra red / otro puerto)
		std::vector<uint8_t> m{ 9, 9, 9 }, x;
		r.perdida = r.duplicado = r.desorden = 0;
		r.cortado[1] = true;
		c0.enviar(m, r.ahora);
		const uint64_t t0 = r.ahora;
		bool vioCaida = false;
		while (r.ahora < t0 + 20000) {
			r.ahora += 10;
			c0.actualizar(r.ahora);
			c1.actualizar(r.ahora);
			vioCaida |= c0.estado() == C_CAIDO && c1.estado() == C_CAIDO;
		}
		r.cortado[1] = false;
		Direccion::desdeTexto("10.0.0.2:6111", r.dir[1]);
		bool llego = false;
		for (int k = 0; k < 400; k++) {
			r.ahora += 10;
			c0.actualizar(r.ahora);
			c1.actualizar(r.ahora);
			while (c1.recibir(x)) llego |= x == m;
		}
		caso("CU-RED-CAIDA-RECONEXION", vioCaida && llego && c0.estado() == C_CONECTADO && c1.estado() == C_CONECTADO && c0.cuentas.reconexiones == 1 &&
		                                    c0.par() == r.dir[1],
		     "caida a los 10 s, vuelve a los 20 s desde " + r.dir[1].texto());
		// sin volver en 30 s: perdido
		r.cortado[1] = true;
		const uint64_t t1 = r.ahora;
		while (r.ahora < t1 + 31000) {
			r.ahora += 50;
			c0.actualizar(r.ahora);
		}
		caso("CU-RED-PERDIDA-30S", c0.estado() == C_PERDIDO);
	}

	// ---- version, catalogo y paquetes ajenos
	{
		auto conectar = [&](const Identidad& i0, const Identidad& i1, Rechazo* motivo) {
			RedSimulada r(3);
			Canal c0, c1;
			c0.usar(&r.punta[0], i0, 1);
			c1.usar(&r.punta[1], i1, 2);
			c0.escuchar(0);
			c1.conectar(r.dir[0], 0);
			for (int k = 0; k < 200; k++) {
				r.ahora += 5;
				c0.actualizar(r.ahora);
				c1.actualizar(r.ahora);
			}
			*motivo = c1.rechazo();
			return c1.estado();
		};
		Identidad bien, otraVersion, otroCatalogo;
		bien.catalogo = otraVersion.catalogo = catalogo;
		otraVersion.versionJuego = "classic-0.9";
		ia::Base base2 = base;
		base2.cartas[0].atk += 100;
		otroCatalogo.catalogo = hashCatalogo(base2);
		Rechazo m1, m2, m3;
		const EstadoCanal e1 = conectar(bien, bien, &m1), e2 = conectar(bien, otraVersion, &m2), e3 = conectar(bien, otroCatalogo, &m3);
		caso("CU-RED-VERSION", e1 == C_CONECTADO && e2 == C_RECHAZADO && m2 == R_VERSION && e3 == C_RECHAZADO && m3 == R_CATALOGO && otroCatalogo.catalogo != catalogo,
		     std::string("otra version: ") + NOMBRE_RECHAZO[m2] + ", otro catalogo: " + NOMBRE_RECHAZO[m3]);
		// el duelo entero rechazado por catalogo distinto
		Partida p(&base, 11);
		DueloRed otro(&base2);
		p.ca.principal = mazoAzar(base, 1, &p.ca.fusion);
		p.ca.entropia = entropia(1, 0);
		ConfigDuelo cb;
		cb.principal = mazoAzar(base, 2, &cb.fusion);
		p.a.anfitrion(&p.red.punta[0], p.ca, 0);
		otro.invitado(&p.red.punta[1], p.red.dir[0], cb, 0);
		for (int k = 0; k < 300; k++) {
			p.red.ahora += 5;
			p.a.actualizar(p.red.ahora);
			otro.actualizar(p.red.ahora);
		}
		caso("CU-RED-CATALOGO", otro.fin() == FIN_RECHAZADO && p.a.estado() == D_CONECTANDO, otro.motivo());
	}

	// ---- duelos completos en lockstep con mazos variados, perdida, duplicado y desorden
	{
		int iguales = 0, terminados = 0, rechazadas = 0, revelaciones = 0, ganaMonedaAnfitrion = 0, empiezaAnfitrion = 0, conPreguntar = 0;
		int ajenos = 0;
		Sha256 huella;   // (de todos los estados finales: igual con cualquier compilador y biblioteca estandar, PS5 y PC)
		std::set<std::string> resultados;
		std::map<int, int> fines;
		for (int k = 0; k < N; k++) {
			Partida p(&base, (uint32_t)k * 31u + 7u);
			p.red.perdida = k % 3 == 0 ? 0.0f : 0.1f;
			p.red.duplicado = k % 4 == 1 ? 0.1f : 0.0f;
			p.red.desorden = k % 2 ? 0.2f : 0.0f;
			p.ca.principal = mazoAzar(base, (uint32_t)k * 2654435761u + 1u, &p.ca.fusion);
			p.cb.principal = mazoAzar(base, (uint32_t)k * 40503u + 99u, &p.cb.fusion);
			// (opciones locales distintas en cada lado: "preguntar antes de activar" viaja en el INICIO)
			p.ca.preguntarSiempre = k % 5 == 0;
			p.cb.preguntarSiempre = k % 7 == 0;
			conPreguntar += p.ca.preguntarSiempre || p.cb.preguntarSiempre;
			p.empezar((uint32_t)k);
			p.manos(base, (uint32_t)k);
			if (k % 10 == 3) {   // (paquetes ajenos: basura y otra sesion)
				p.red.inyectar(0, { 1, 2, 3 });
				Escritor w;
				w.u32(MAGIA).u16(VERSION_PROTOCOLO).u8(P_DATOS).u8(0).u64(0x1234).u32(1).u32(1).u32(0).u8(M_JUGADA);
				p.red.inyectar(1, w.b);
			}
			p.correr(1800 * 1000);
			ajenos += p.a.canal().cuentas.ajenos + p.b.canal().cuentas.ajenos;
			fines[p.a.fin()]++;
			if (!p.terminado()) {
				fprintf(stderr, "  duelo %d sin terminar: estados %d/%d, pasos %u/%u, turno %d\n", k, p.a.estado(), p.b.estado(), p.a.pasos(), p.b.pasos(), p.a.duelo().turno);
				continue;
			}
			terminados++;
			const bool igual = p.a.fin() == FIN_NORMAL && p.b.fin() == FIN_NORMAL && p.a.ganador() == p.b.ganador() &&
			                   hashEstado(p.a.duelo()) == hashEstado(p.b.duelo()) && p.a.pasos() == p.b.pasos() && p.a.verificado() && p.b.verificado() &&
			                   !p.a.trampaDelOtro() && !p.b.trampaDelOtro() && p.a.ganoMoneda() == p.b.ganoMoneda() && p.a.empieza() == p.b.empieza();
			huella.agregar(hashEstado(p.a.duelo()));
			if (igual) iguales++;
			else
				fprintf(stderr, "  duelo %d distinto: fin %s/%s (%s | %s), ganador %d/%d, pasos %u/%u\n", k, NOMBRE_FIN[p.a.fin()], NOMBRE_FIN[p.b.fin()],
				        p.a.motivo().c_str(), p.b.motivo().c_str(), p.a.ganador(), p.b.ganador(), p.a.pasos(), p.b.pasos());
			rechazadas += p.ma->rechazadas + p.mb->rechazadas;
			revelaciones += p.a.revelacionesVerificadas() + p.b.revelacionesVerificadas();
			ganaMonedaAnfitrion += p.a.ganoMoneda() == 0;
			empiezaAnfitrion += p.a.empieza() == 0;
			resultados.insert(std::to_string(p.a.ganador()) + "/" + std::to_string(p.a.duelo().turno) + "/" + std::to_string(p.a.duelo().l[0].lp) + "/" +
			                  std::to_string(p.a.duelo().l[1].lp));
		}
		char det[300];
		snprintf(det, sizeof det, "%d duelos: %d terminados, %d iguales en las dos maquinas, %d resultados distintos, %d revelaciones verificadas, %d jugadas de la IA rechazadas, %d paquetes ajenos ignorados",
		         N, terminados, iguales, (int)resultados.size(), revelaciones, rechazadas, ajenos);
		caso("CU-RED-LOCKSTEP", iguales == N && (int)resultados.size() > N / 4, det);
		fprintf(stderr, "huella de los %d duelos: %s\n", N, hex(huella.fin()).c_str());
		caso("CU-RED-PREGUNTAR-SIEMPRE", iguales == N && conPreguntar > 0, std::to_string(conPreguntar) + " duelos con opciones locales distintas, sin desincronizar");
		caso("CU-RED-AJENOS", iguales == N && ajenos > 0, std::to_string(ajenos) + " paquetes ajenos ignorados");
		snprintf(det, sizeof det, "el anfitrion gano la moneda %d de %d, empezo %d", ganaMonedaAnfitrion, terminados, empiezaAnfitrion);
		caso("CU-RED-MONEDA", terminados > 20 && ganaMonedaAnfitrion > terminados / 5 && ganaMonedaAnfitrion < terminados * 4 / 5, det);
		caso("CU-RED-COMPROMISO", iguales == N && revelaciones > N * 5, std::to_string(revelaciones) + " robos revelados y verificados; verificacion final OK en todos");
	}

	// ---- desincronizacion forzada
	{
		mkdir("registros_red", 0777);   // (en la carpeta donde corre la prueba)
		Partida p(&base, 5);
		p.ca.principal = mazoAzar(base, 5, &p.ca.fusion);
		p.cb.principal = mazoAzar(base, 6, &p.cb.fusion);
		p.ca.carpetaRegistro = p.cb.carpetaRegistro = "registros_red/";
		p.empezar(5);
		p.manos(base, 5);
		while (p.a.estado() != D_JUGANDO || p.a.duelo().turno < 3) p.correr(5);
		p.a.forzarDesincronizacion();
		p.correr(600000);
		FILE* f = p.b.rutaRegistro().empty() ? nullptr : fopen(p.b.rutaRegistro().c_str(), "r");   // (los dos guardan su registro)
		if (f) fclose(f);
		FILE* g = p.a.rutaRegistro().empty() ? nullptr : fopen(p.a.rutaRegistro().c_str(), "r");
		if (g) fclose(g);
		if (!g) f = nullptr;
		caso("CU-RED-DESINCRONIZACION", p.a.fin() == FIN_DESINCRONIZADO && p.b.fin() == FIN_DESINCRONIZADO && p.a.ganador() == -1 && p.b.ganador() == -1 && f,
		     p.b.motivo() + ", registro " + p.b.rutaRegistro());
	}

	// ---- abandono: el rival se corta y no vuelve; se rinde; se va (ADIOS)
	{
		Partida p(&base, 6);
		p.ca.principal = mazoAzar(base, 7, &p.ca.fusion);
		p.cb.principal = mazoAzar(base, 8, &p.cb.fusion);
		p.empezar(6);
		p.manos(base, 6);
		while (p.a.estado() != D_JUGANDO || p.a.duelo().turno < 2) p.correr(5);
		p.red.cortado[1] = true;
		p.correr(15000);
		const bool esperando = p.a.esperandoReconexion() && p.a.estado() == D_JUGANDO;
		p.correr(20000);
		caso("CU-RED-ABANDONO", esperando && p.a.fin() == FIN_ABANDONO && p.a.ganador() == -1, p.a.motivo());

		Partida q(&base, 7);
		q.ca.principal = mazoAzar(base, 9, &q.ca.fusion);
		q.cb.principal = mazoAzar(base, 10, &q.cb.fusion);
		q.empezar(7);
		q.manos(base, 7);
		while (q.a.estado() != D_JUGANDO || q.a.duelo().turno < 2) q.correr(5);
		q.b.rendirse(q.red.ahora);
		q.correr(2000);
		caso("CU-RED-RENDICION", q.a.fin() == FIN_RENDICION && q.a.ganador() == 0 && q.b.ganador() == 0 && q.a.verificado() && !q.a.trampaDelOtro(), q.a.motivo());

		Partida s(&base, 8);
		s.ca.principal = mazoAzar(base, 11, &s.ca.fusion);
		s.cb.principal = mazoAzar(base, 12, &s.cb.fusion);
		s.empezar(8);
		s.manos(base, 8);
		while (s.a.estado() != D_JUGANDO || s.a.duelo().turno < 2) s.correr(5);
		s.a.salir(s.red.ahora);
		s.correr(2000, false, false);
		caso("CU-RED-SALIR", s.b.fin() == FIN_ABANDONO && s.b.ganador() == -1 && s.b.canal().adiosRecibido(), s.b.motivo());
	}

	// ---- rama classic-en-linea: reloj agotado (3 = derrota), revancha en el mismo canal y la vista en espejo
	{
		Partida p(&base, 21);
		p.ca.principal = mazoAzar(base, 31, &p.ca.fusion);
		p.cb.principal = mazoAzar(base, 32, &p.cb.fusion);
		p.empezar(21);
		p.manos(base, 21);
		while (p.a.estado() != D_JUGANDO || p.a.duelo().turno < 2) p.correr(5);
		int veces = 0, vistoA = 0;
		for (int k = 0; k < 400000 && p.b.estado() == D_JUGANDO; k++) {
			if (p.b.meToca()) {
				p.b.agotarTiempo(p.red.ahora);
				veces++;
				p.correr(300, true, false);
				if (veces == 1) vistoA = p.a.agotados(1);
				ia::Jugada f;   // (lo que haria el juego: pasa de fase / no activa / elige la primera)
				if (p.b.meToca()) p.b.jugarConRespaldo(f, p.red.ahora);
			}
			p.correr(5, true, false);
		}
		p.correr(2000, true, false);
		caso("CU-RED-RELOJ-AGOTADO", vistoA == 1 && veces == 3 && p.a.fin() == FIN_TIEMPO && p.b.fin() == FIN_TIEMPO && p.a.ganador() == 0 && p.b.ganador() == 0 &&
		     p.a.agotados(1) == 3 && p.b.agotados(1) == 3 && p.a.revanchaPosible(), std::to_string(veces) + " veces, " + p.a.motivo());

		// revancha: los dos la piden; el duelo nuevo arranca en el mismo canal y se juega hasta el final
		auto bombear = [&](int ms) {   // (correr() se corta con el duelo terminado)
			for (int k = 0; k < ms / 5; k++) {
				p.red.ahora += 5;
				p.a.actualizar(p.red.ahora);
				p.b.actualizar(p.red.ahora);
			}
		};
		p.a.pedirRevancha(p.red.ahora);
		bombear(1500);
		const bool esperaOtro = p.a.revanchaMia() && p.b.revanchaDelOtro() && p.a.revanchas() == 0;
		p.b.pedirRevancha(p.red.ahora);
		bombear(1500);
		p.manos(base, 22);
		p.correr(1800 * 1000);
		caso("CU-RED-REVANCHA", esperaOtro && p.a.revanchas() == 1 && p.b.revanchas() == 1 && p.terminado() && p.a.fin() == FIN_NORMAL &&
		     p.a.ganador() == p.b.ganador() && hashEstado(p.a.duelo()) == hashEstado(p.b.duelo()) && p.a.agotados(1) == 0,
		     std::string("fin ") + NOMBRE_FIN[p.a.fin()] + ", ganador " + std::to_string(p.a.ganador()));

		// la vista en espejo: dos veces es el duelo de nuevo; el invitado juega duelos enteros mirando la copia
		Partida q(&base, 23);
		q.ca.principal = mazoAzar(base, 33, &q.ca.fusion);
		q.cb.principal = mazoAzar(base, 34, &q.cb.fusion);
		q.empezar(23);
		q.manos(base, 23);
		while (q.a.estado() != D_JUGANDO || q.a.duelo().turno < 3) q.correr(5);
		const ia::Duelo& d = q.b.duelo();
		const ia::Duelo e = espejoDuelo(d), e2 = espejoDuelo(e);
		const bool espejo = hashEstado(e2) == hashEstado(d) && e.l[0].mano == d.l[1].mano && e.l[1].mazo == d.l[0].mazo && e.activo == 1 - d.activo &&
		                    e.sucesos.size() == d.sucesos.size() && (d.sucesos.empty() || e.sucesos.back().lado == 1 - d.sucesos.back().lado) &&
		                    (d.decide() == 1) == (e.decide() == 0);
		int iguales = 0, rechazadas = 0, decideEspejo = 0;
		const int N = 12;
		for (int k = 0; k < N; k++) {
			Partida r(&base, 40 + (uint32_t)k);
			r.ca.principal = mazoAzar(base, 50 + (uint32_t)k, &r.ca.fusion);
			r.cb.principal = mazoAzar(base, 70 + (uint32_t)k, &r.cb.fusion);
			r.empezar(40 + (uint32_t)k);
			const ia::Rival& ra = base.rivales[(size_t)(k * 7 % (int)base.rivales.size())];
			const ia::Rival& rb = base.rivales[(size_t)(k * 11 % (int)base.rivales.size())];
			r.ma.reset(new ManoIA(r.a, ra.personalidad(), ra.nivel, (uint32_t)k * 3u + 1u));
			ManoEspejo mb(rb.personalidad(), rb.nivel, (uint32_t)k * 5u + 2u);
			const uint64_t fin = r.red.ahora + 1800 * 1000;
			while (r.red.ahora < fin && !(r.terminado() && r.a.verificado() && r.b.verificado())) {
				r.red.ahora += 5;
				r.a.actualizar(r.red.ahora);
				r.b.actualizar(r.red.ahora);
				for (int i = 0; i < 6; i++) {
					bool alguna = r.ma->paso(r.a, r.red.ahora);
					if (r.b.meToca()) decideEspejo++;
					alguna |= mb.paso(r.b, r.red.ahora);
					if (!alguna) break;
				}
			}
			rechazadas += mb.rechazadas;
			if (r.terminado() && r.a.fin() == FIN_NORMAL && r.b.fin() == FIN_NORMAL && r.a.ganador() == r.b.ganador() &&
			    hashEstado(r.a.duelo()) == hashEstado(r.b.duelo()))
				iguales++;
			else
				fprintf(stderr, "  espejo %d: fin %s/%s, ganador %d/%d, %s\n", k, NOMBRE_FIN[r.a.fin()], NOMBRE_FIN[r.b.fin()], r.a.ganador(), r.b.ganador(),
				        r.b.motivo().c_str());
		}
		caso("CU-RED-ESPEJO", espejo, "espejo(espejo(d)) = d, manos, mazos, activo, sucesos y quien decide");
		caso("CU-RED-ESPEJO-DUELOS", iguales == N && decideEspejo > 0,
		     std::to_string(iguales) + "/" + std::to_string(N) + " duelos del invitado jugados desde la vista en espejo, " + std::to_string(rechazadas) + " jugadas rechazadas");
	}

	// ---- trampas del barajado: mazo arreglado y carta revelada falsa
	{
		Partida p(&base, 9);
		p.ca.principal = mazoAzar(base, 13, &p.ca.fusion);
		p.cb.principal = mazoAzar(base, 14, &p.cb.fusion);
		p.b.hacerTrampaMazo();
		p.empezar(9);
		p.manos(base, 9);
		p.correr(1800 * 1000);
		caso("CU-RED-TRAMPA-MAZO", p.a.verificado() && p.a.verificacion() == VER_ORDEN && !p.b.trampaDelOtro(), p.a.motivo());

		Partida q(&base, 10);
		q.ca.principal = mazoAzar(base, 15, &q.ca.fusion);
		q.cb.principal = mazoAzar(base, 16, &q.cb.fusion);
		q.a.hacerTrampaRevelacion();
		q.empezar(10);
		q.manos(base, 10);
		q.correr(1800 * 1000);
		caso("CU-RED-TRAMPA-REVELACION", q.b.verificacion() == VER_REVELACION && q.b.revelacionesVerificadas() == 0, q.b.motivo());

		// la sal equivocada o la carta cambiada no pasan
		Hash priv = sha256("x"), comun = sha256("y");
		std::vector<std::string> mz = { "a", "b", "c", "d" };
		const std::vector<std::string> orden = ordenMazo(mz, priv, comun);
		const CompromisoMazo c = comprometer(orden, priv);
		bool ok = verificarCarta(c, 2, orden[2], salDe(priv, 2)) && !verificarCarta(c, 2, orden[2], salDe(priv, 3)) &&
		          !verificarCarta(c, 2, orden[2] == "a" ? "b" : "a", salDe(priv, 2));
		ok &= verificarFinal(c, compromisoDe(priv), priv, comun, orden) == VER_OK;
		ok &= verificarFinal(c, compromisoDe(priv), sha256("z"), comun, orden) == VER_SEMILLA;
		ok &= ordenMazo({ "d", "c", "b", "a" }, priv, comun) == orden;   // (no depende del orden en que venia)
		caso("CU-RED-COMPROMISO-UNIDAD", ok);
	}

	// ---- duelos por UDP de verdad en loopback (iniciarDueloRed)
	{
		int bien = 0;
		const int M = 3;
		for (int k = 0; k < M; k++) {
			ConfigDuelo ca, cb;
			ca.principal = mazoAzar(base, 100u + (uint32_t)k, &ca.fusion);
			cb.principal = mazoAzar(base, 200u + (uint32_t)k, &cb.fusion);
			ca.nombre = "Anfitrion";
			cb.nombre = "Invitado";
			ca.elegirOrdenAuto = cb.elegirOrdenAuto = true;
			auto sa = iniciarDueloRed(&base, ca, true, {}, 0, k != 1);
			if (!sa) break;
			Direccion d;
			Direccion::desdeTexto(k == 2 ? "[::1]" : "127.0.0.1", d);
			d.puerto = sa->socket.puertoLocal();
			auto sb = iniciarDueloRed(&base, cb, false, d, 0, k != 1);
			if (!sb) break;
			ManoIA *ma = nullptr, *mb = nullptr;
			const uint64_t t0 = ahoraMs();
			while (ahoraMs() - t0 < 60000) {
				sa->actualizar();
				sb->actualizar();
				if (!ma) ma = new ManoIA(sa->duelo, base.rivales[(size_t)k].personalidad(), 4, 1u + (uint32_t)k);
				if (!mb) mb = new ManoIA(sb->duelo, base.rivales[(size_t)k].personalidad(), 4, 9u + (uint32_t)k);
				for (int j = 0; j < 8; j++) {
					const bool x = ma->paso(sa->duelo, ahoraMs());
					const bool y = mb->paso(sb->duelo, ahoraMs());
					if (!x && !y) break;
				}
				if (sa->duelo.estado() == D_TERMINADO && sb->duelo.estado() == D_TERMINADO && sa->duelo.verificado() && sb->duelo.verificado()) break;
				std::this_thread::sleep_for(std::chrono::microseconds(200));
			}
			if (sa->duelo.fin() == FIN_NORMAL && sb->duelo.fin() == FIN_NORMAL && hashEstado(sa->duelo.duelo()) == hashEstado(sb->duelo.duelo()) &&
			    sb->duelo.canal().otro().nombre == "Anfitrion")
				bien++;
			else
				fprintf(stderr, "  udp %d: %s / %s (%s)\n", k, NOMBRE_FIN[sa->duelo.fin()], NOMBRE_FIN[sb->duelo.fin()], sb->duelo.motivo().c_str());
			delete ma;
			delete mb;
		}
		caso("CU-RED-UDP-LOOPBACK", bien == M, std::to_string(bien) + " de " + std::to_string(M) + " duelos por UDP (IPv4, solo IPv4 e IPv6)");
	}

	// ---- red local: anuncio y descubrimiento (broadcast + loopback en un puerto de prueba)
	{
		const uint16_t PUERTO = 47399;
		Identidad yo, otraVersion;
		yo.catalogo = otraVersion.catalogo = catalogo;
		yo.nombre = "Yugi";
		otraVersion.versionJuego = "classic-0.1";
		otraVersion.nombre = "Viejo";
		BuscadorLan bus;
		AnuncioLan an, an2;
		const bool abrio = bus.empezar(yo, PUERTO);
		Direccion lo;
		Direccion::desdeTexto("127.0.0.1", lo);
		an.extra.push_back(lo);
		an.puertoLan = an2.puertoLan = PUERTO;
		an2.extra.push_back(lo);
		an.empezar(yo, 5555, 42);
		an2.empezar(otraVersion, 6666, 43);
		const uint64_t t0 = ahoraMs();
		bool visto = false, incompatible = false;
		while (ahoraMs() - t0 < 2500 && !(visto && incompatible)) {
			an.actualizar(ahoraMs());
			an2.actualizar(ahoraMs());
			bus.actualizar(ahoraMs());
			for (const Anuncio& a : bus.lista()) {
				visto |= a.id == 42 && a.nombre == "Yugi" && a.anfitrion.puerto == 5555 && a.compatible;
				incompatible |= a.id == 43 && !a.compatible;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		an.detener();
		bool retirado = false;
		const uint64_t t1 = ahoraMs();
		while (ahoraMs() - t1 < 1000 && !retirado) {
			bus.actualizar(ahoraMs());
			retirado = true;
			for (const Anuncio& a : bus.lista()) retirado &= a.id != 42;
		}
		caso("CU-RED-LAN", abrio && visto && incompatible && retirado, std::string(visto ? "anuncio visto" : "anuncio NO visto") + (incompatible ? ", version distinta marcada" : "") + (retirado ? ", retiro" : ""));
	}

	fprintf(stderr, "%s (%d fallos)\n", fallos ? "PRUEBA CON FALLOS" : "PRUEBA OK", fallos);
	return fallos ? 1 : 0;
}
