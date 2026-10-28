#pragma once
// Conexion directa entre dos maquinas en internet (rama classic-red-nat), sin servidor propio para el duelo:
//  1. reunirCandidatos(): las direcciones por las que el otro nos puede alcanzar: LAN, IPv6 global, el puerto que
//     abre el router por UPnP (o NAT-PMP) y la direccion reflexiva STUN (dos servidores distintos desde el mismo
//     puerto: si ven puertos distintos el NAT es simetrico). Clasifica el NAT y detecta CGNAT.
//  2. Los candidatos viajan al otro por la sala (rama classic-sala) o, de respaldo, a mano: codigoConexion() arma
//     un texto corto (base32 sin caracteres ambiguos + suma de verificacion) y leerCodigo() lo lee.
//  3. perforar(): los dos lados mandan sondeos con idSesion a todos los candidatos del otro a la vez (cada 150 ms) y
//     se quedan con el primer camino que responde en los dos sentidos, prefiriendo LAN > IPv6 > UPnP > STUN.
//  4. liberarUpnp() al salir: borra el mapeo del router.
// Los sondeos empiezan con MARCA_SONDEO ("YCPF"): el canal confiable (classic-red-base) tiene que ignorarlos o
// pasarlos a responderSondeo() (por si el otro sigue sondeando cuando aqui ya se termino).
// Las versiones ...Con(socket, opciones) son plantillas: las pruebas les pasan un socket que atraviesa un NAT
// simulado en el proceso (prueba_red_nat.cpp).
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <array>
#include <cctype>
#include "socket_udp.h"
#include "stun.h"
#include "resolver.h"
#include "upnp.h"

namespace red {

enum class TipoCandidato { Local, Ipv6, Upnp, Stun, Manual };
struct Candidato {
	Direccion dir;
	TipoCandidato tipo = TipoCandidato::Manual;
	int prioridad = 0;
};
enum class TipoNat { Abierto, Cono, Simetrico, Cgnat, SinRed, Desconocido };
struct InfoRed {
	std::vector<Candidato> candidatos;
	TipoNat nat = TipoNat::Desconocido;
	bool upnpOk = false;
	bool ipv6Ok = false;
	std::string ipPublica;
};

// puerto fijo recomendado para el juego (el que se abre a mano en el router si no hay UPnP)
static const uint16_t PUERTO_JUEGO = 47924;
static const uint8_t MARCA_SONDEO[4] = { 'Y', 'C', 'P', 'F' };

// prioridad por tipo: LAN > IPv6 > UPnP > STUN (Manual: lo escribio el usuario, va como UPnP: un puerto abierto)
inline int prioridadTipo(TipoCandidato t)
{
	switch (t) {
	case TipoCandidato::Local: return 400;
	case TipoCandidato::Ipv6: return 300;
	case TipoCandidato::Upnp: return 200;
	case TipoCandidato::Manual: return 190;
	case TipoCandidato::Stun: return 100;
	}
	return 0;
}
inline const char* nombreTipo(TipoCandidato t)
{
	switch (t) {
	case TipoCandidato::Local: return "LAN";
	case TipoCandidato::Ipv6: return "IPv6";
	case TipoCandidato::Upnp: return "UPnP";
	case TipoCandidato::Stun: return "STUN";
	case TipoCandidato::Manual: return "manual";
	}
	return "?";
}
inline const char* nombreNat(TipoNat t)
{
	switch (t) {
	case TipoNat::Abierto: return "abierto";
	case TipoNat::Cono: return "cono";
	case TipoNat::Simetrico: return "simetrico";
	case TipoNat::Cgnat: return "CGNAT";
	case TipoNat::SinRed: return "sin red";
	case TipoNat::Desconocido: return "desconocido";
	}
	return "?";
}

// ---------------------------------------------------------------------------------------------- rangos IPv4/IPv6
inline bool esPrivada(const Direccion& d)   // 10/8, 172.16/12, 192.168/16, 169.254/16 (IPv4); fc00::/7 (ULA)
{
	if (d.v6) return (d.ip[0] & 0xFE) == 0xFC;
	return d.ip[0] == 10 || (d.ip[0] == 172 && (d.ip[1] & 0xF0) == 16) || (d.ip[0] == 192 && d.ip[1] == 168) ||
		(d.ip[0] == 169 && d.ip[1] == 254);
}
inline bool esCgnat(const Direccion& d) { return !d.v6 && d.ip[0] == 100 && (d.ip[1] & 0xC0) == 64; }   // 100.64.0.0/10
inline bool esIpv6Global(const Direccion& d) { return d.v6 && (d.ip[0] & 0xE0) == 0x20; }               // 2000::/3
inline bool mismaIp(const Direccion& a, const Direccion& b) { return a.v6 == b.v6 && memcmp(a.ip, b.ip, a.v6 ? 16 : 4) == 0; }
inline std::string soloIp(Direccion d) { d.puerto = 0; return d.texto(); }

// ---------------------------------------------------------------------------------------------- clasificacion
// Datos crudos de la exploracion -> tipo de NAT. stun: lo que vio cada servidor STUN (IPv4) desde el mismo puerto;
// routerIp: la IP externa que dice el router (UPnP/NAT-PMP; v6 = false y todo en cero si no se sabe)
inline TipoNat clasificar(const std::vector<Direccion>& locales, uint16_t puertoLocal, const std::vector<Direccion>& stun,
	const Direccion* routerIp)
{
	bool hayLocal = false;
	for (const Direccion& d : locales) hayLocal |= !d.esLoopback();
	if (!hayLocal && stun.empty()) return TipoNat::SinRed;
	// CGNAT: el router tiene una IP de CGNAT o privada del lado de internet, o no es la que ve STUN
	if (routerIp && (esCgnat(*routerIp) || esPrivada(*routerIp))) return TipoNat::Cgnat;
	if (routerIp && !stun.empty() && !mismaIp(*routerIp, stun[0])) return TipoNat::Cgnat;
	if (stun.empty()) return TipoNat::Desconocido;
	for (const Direccion& d : locales)
		if (mismaIp(d, stun[0]) && stun[0].puerto == puertoLocal) return TipoNat::Abierto;   // IP publica en la maquina
	if (!routerIp && esCgnat(stun[0])) return TipoNat::Cgnat;   // (raro: STUN dentro del CGNAT)
	if (stun.size() < 2) return TipoNat::Desconocido;
	for (size_t i = 1; i < stun.size(); i++)
		if (stun[i] != stun[0]) return TipoNat::Simetrico;   // otro destino, otro puerto: no sirve el reflexivo
	return TipoNat::Cono;
}

// el aviso para la pantalla (clave en espanol de idioma.h; nullptr si no hace falta avisar)
inline const char* avisoRed(const InfoRed& info)
{
	switch (info.nat) {
	case TipoNat::SinRed: return "Sin conexión a internet.";
	case TipoNat::Cgnat: return info.ipv6Ok ? nullptr : "Tu proveedor usa CGNAT: la conexión directa puede fallar. Pide una IP pública a tu proveedor o usa IPv6.";
	case TipoNat::Simetrico: return info.upnpOk || info.ipv6Ok ? nullptr : "Tu router tiene NAT simétrico: la conexión directa puede fallar. Activa UPnP en el router o abre el puerto a mano.";
	default: return nullptr;
	}
}
// como abrir el puerto a mano (se muestra con el numero: idioma::tr(TEXTO_PUERTO_MANUAL) + " " + PUERTO_JUEGO)
static const char* const TEXTO_PUERTO_MANUAL = "Para abrirlo a mano: en el router, reenvía el puerto UDP a esta consola. Puerto:";

// ---------------------------------------------------------------------------------------------- reunir candidatos
// la ruta de datos/red_stun.txt en la consola (el juego la pone: getAssetPath() + "ui/red_stun.txt"); vacia = la lista fija
inline std::string& archivoStun() { static std::string r; return r; }

struct OpcionesReunir {
	bool usarUpnp = true;                  // UPnP / NAT-PMP en el router
	bool resolverStun = true;              // resolver los nombres de servidoresStun() (si no, solo stunFijos)
	std::string archivoStun;               // datos/red_stun.txt (vacio: la lista de resolver.h)
	std::vector<Direccion> stunFijos;      // servidores STUN ya resueltos (pruebas: el de mentira)
	std::vector<Direccion> stunFijosV6;
	bool localesFijos = false;             // pruebas: usar 'locales' en vez de direccionesLocales()
	std::vector<Direccion> locales;
	const std::vector<std::string>* ubicacionesUpnp = nullptr;   // pruebas: LOCATION ya conocidas (sin SSDP)
};

namespace detalle_nat {
inline void agregar(std::vector<Candidato>& v, const Direccion& d, TipoCandidato t, int ajuste = 0)
{
	for (const Candidato& c : v) if (c.dir == d) return;
	Candidato c;
	c.dir = d;
	c.tipo = t;
	c.prioridad = prioridadTipo(t) + ajuste;
	v.push_back(c);
}
inline void ordenar(std::vector<Candidato>& v)
{
	std::stable_sort(v.begin(), v.end(), [](const Candidato& a, const Candidato& b) { return a.prioridad > b.prioridad; });
}
inline uint64_t aleatorio64()
{
	static std::mt19937_64 g([] { std::random_device rd; return ((uint64_t)rd() << 32) ^ rd() ^ ahoraMs(); }());
	return g();
}
}  // namespace detalle_nat

// STUN a varios servidores desde el socket s; resultado[i] = lo que vio el servidor i (puerto 0 si no respondio)
template <class S>
void consultarStun(S& s, const std::vector<Direccion>& servidores, int esperaMs, std::vector<Direccion>& vistas)
{
	vistas.assign(servidores.size(), Direccion());
	std::vector<std::array<uint8_t, 12>> tids(servidores.size());
	for (auto& t : tids) for (int i = 0; i < 12; i++) t[(size_t)i] = (uint8_t)detalle_nat::aleatorio64();
	const uint64_t fin = ahoraMs() + (uint64_t)esperaMs;
	uint64_t reenvio = 0;
	int intervalo = 250;
	uint8_t b[1500];
	while (ahoraMs() < fin) {
		size_t faltan = 0;
		for (const Direccion& v : vistas) faltan += v.puerto == 0;
		if (!faltan) break;
		if (ahoraMs() >= reenvio) {
			for (size_t i = 0; i < servidores.size(); i++)
				if (vistas[i].puerto == 0) { s.enviar(servidores[i], b, stun::armarPeticion(b, tids[i].data())); }
			reenvio = ahoraMs() + (uint64_t)intervalo;
			intervalo = intervalo < 1000 ? intervalo * 2 : 1000;
		}
		Direccion de;
		const int n = s.recibir(de, b, sizeof b, 20);
		if (n <= 0 || !stun::esStun(b, (size_t)n)) continue;
		for (size_t i = 0; i < servidores.size(); i++) {
			Direccion vista;
			if (vistas[i].puerto == 0 && stun::leerRespuesta(b, (size_t)n, tids[i].data(), vista)) vistas[i] = vista;
		}
	}
}

// la exploracion completa con un socket cualquiera (SocketUdp o el simulado de las pruebas)
template <class S>
InfoRed reunirCandidatosCon(S& s, int esperaMs, const OpcionesReunir& op)
{
	InfoRed info;
	const uint64_t inicio = ahoraMs();
	const uint16_t puerto = s.puertoLocal();
	const bool v6 = s.esV6();
	std::vector<Direccion> locales = op.localesFijos ? op.locales : direccionesLocales();
	// 1) servidores STUN: dos IPv4 distintos (de nombres distintos) y uno IPv6 si el socket es dual-stack
	std::vector<Direccion> stun4 = op.stunFijos, stun6 = op.stunFijosV6;
	if (op.resolverStun) {
		bool dnsMuerto = false, probado6 = false;   // si el DNS no contesta una vez, el resto va con las IP de respaldo
		for (const ServidorStun& sv : servidoresStun(op.archivoStun)) {
			if (stun4.size() >= 2 && (!v6 || probado6)) break;
			std::vector<Direccion> r;
			if (stun4.size() < 2) {
				if (dnsMuerto || !dns::resolver(sv.nombre, false, sv.puerto, r, 800)) { dnsMuerto = true; r = sv.respaldo; }
				for (const Direccion& d : r) {
					if (d.v6) continue;
					bool repetida = false;
					for (const Direccion& x : stun4) repetida |= mismaIp(x, d);
					if (!repetida) { stun4.push_back(d); break; }
				}
			}
			if (v6 && !probado6 && !dnsMuerto) {   // un solo intento de AAAA
				probado6 = true;
				r.clear();
				if (dns::resolver(sv.nombre, true, sv.puerto, r, 500) && !r.empty()) stun6.push_back(r[0]);
			}
		}
	}
	// 2) STUN (IPv4 e IPv6 a la vez)
	std::vector<Direccion> todos = stun4;
	todos.insert(todos.end(), stun6.begin(), stun6.end());
	std::vector<Direccion> vistas;
	const int restante = (int)(esperaMs - (int)(ahoraMs() - inicio));
	consultarStun(s, todos, std::max(800, std::min(restante, 3000)), vistas);
	std::vector<Direccion> vistas4;
	Direccion vista6;
	for (size_t i = 0; i < vistas.size(); i++) {
		if (vistas[i].puerto == 0) continue;
		if (i < stun4.size()) vistas4.push_back(vistas[i]);
		else vista6 = vistas[i];
	}
	// 3) UPnP / NAT-PMP
	Direccion routerIp;
	bool hayRouterIp = false;
	if (op.usarUpnp) {
		const int queda = std::max(1500, (int)(esperaMs - (int)(ahoraMs() - inicio)));
		upnp::Mapeo m;
		if (upnp::abrirPuerto(puerto, std::min(queda, 6000), m, op.ubicacionesUpnp)) {
			info.upnpOk = true;
			routerIp = m.ipExterna;
			hayRouterIp = true;
		}
	}
	// 4) clasificar
	info.nat = clasificar(locales, puerto, vistas4, hayRouterIp ? &routerIp : nullptr);
	if (!vistas4.empty()) info.ipPublica = soloIp(vistas4[0]);
	else if (hayRouterIp && info.nat != TipoNat::Cgnat) info.ipPublica = soloIp(routerIp);
	// 5) candidatos: LAN (IPv4 locales), IPv6 global, UPnP (si no hay CGNAT: el puerto del router no es publico) y STUN
	int k = 0;
	for (Direccion d : locales)
		if (!d.v6 && !d.esLoopback()) { d.puerto = puerto; detalle_nat::agregar(info.candidatos, d, TipoCandidato::Local, -(k++)); }
	k = 0;
	for (Direccion d : locales)
		if (v6 && esIpv6Global(d)) {
			d.puerto = puerto;
			detalle_nat::agregar(info.candidatos, d, TipoCandidato::Ipv6, -(k++));
			if (vista6.puerto != 0 && mismaIp(vista6, d)) info.ipv6Ok = true;   // internet la ve tal cual (sin NAT66)
		}
	if (info.upnpOk && info.nat != TipoNat::Cgnat) {
		Direccion d = routerIp;
		d.puerto = upnp::mapeoActual().externo;
		detalle_nat::agregar(info.candidatos, d, TipoCandidato::Upnp);
	}
	for (size_t i = 0; i < vistas4.size(); i++) detalle_nat::agregar(info.candidatos, vistas4[i], TipoCandidato::Stun, -(int)i);
	detalle_nat::ordenar(info.candidatos);
	return info;
}

// LAN, IPv6 global, mapeo UPnP y reflexivo STUN del socket s (el del juego: los candidatos son de SU puerto)
inline InfoRed reunirCandidatos(SocketUdp& s, int esperaMs)
{
	OpcionesReunir op;
	op.archivoStun = archivoStun();
	return reunirCandidatosCon(s, esperaMs, op);
}

inline void liberarUpnp() { upnp::liberar(); }

// ---------------------------------------------------------------------------------------------- perforacion
// sondeo: MARCA(4) tipo(1: 1 sondeo, 2 respuesta) banderas(1: bit 0 = ya tengo camino) idSesion(8) yo(8) eco(8)
static const size_t LARGO_SONDEO = 30;
namespace detalle_nat {
inline void poner64(uint8_t* p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (56 - 8 * i)); }
inline uint64_t leer64(const uint8_t* p) { uint64_t v = 0; for (int i = 0; i < 8; i++) v = (v << 8) | p[i]; return v; }
inline size_t armarSondeo(uint8_t* b, uint8_t tipo, bool listo, uint64_t idSesion, uint64_t yo, uint64_t eco)
{
	memcpy(b, MARCA_SONDEO, 4);
	b[4] = tipo;
	b[5] = listo ? 1 : 0;
	poner64(b + 6, idSesion);
	poner64(b + 14, yo);
	poner64(b + 22, eco);
	return LARGO_SONDEO;
}
}  // namespace detalle_nat

// ¿es un paquete de la perforacion? (el canal confiable lo tiene que saltar)
inline bool esSondeo(const uint8_t* p, size_t n) { return n >= LARGO_SONDEO && memcmp(p, MARCA_SONDEO, 4) == 0 && (p[4] == 1 || p[4] == 2); }

// despues de perforar: si llega un sondeo tardio del otro, contestarlo (asi el otro tambien termina)
template <class S>
bool responderSondeoCon(S& s, const uint8_t* p, size_t n, const Direccion& de, uint64_t idSesion)
{
	if (!esSondeo(p, n) || p[4] != 1 || detalle_nat::leer64(p + 6) != idSesion) return false;
	uint8_t b[LARGO_SONDEO];
	s.enviar(de, b, detalle_nat::armarSondeo(b, 2, true, idSesion, 0, detalle_nat::leer64(p + 14)));
	return true;
}
inline bool responderSondeo(SocketUdp& s, const uint8_t* p, size_t n, const Direccion& de, uint64_t idSesion)
{
	return responderSondeoCon(s, p, n, de, idSesion);
}

// Los dos lados a la vez: sondeos a todos los candidatos del otro cada 150 ms; contestar cada sondeo a la direccion
// de la que llego (asi pasa un NAT simetrico: esa direccion "reflexiva del par" se suma a los destinos). Un camino
// queda confirmado cuando llega la respuesta a un sondeo propio. Tras el primero se esperan 500 ms por uno de mas
// prioridad; se termina cuando el otro tambien avisa que tiene camino (o al vencer el tiempo, con lo confirmado).
template <class S>
bool perforarCon(S& s, const std::vector<Candidato>& remotos, uint64_t idSesion, int esperaMs, Direccion& elegido)
{
	struct Destino { Direccion dir; int prioridad; };
	std::vector<Destino> destinos;
	for (const Candidato& c : remotos)
		if (!c.dir.v6 || s.esV6()) {
			bool repetido = false;
			for (const Destino& d : destinos) repetido |= d.dir == c.dir;
			if (!repetido && c.dir.puerto != 0) destinos.push_back({ c.dir, c.prioridad });
		}
	const uint64_t yo = detalle_nat::aleatorio64() | 1;
	const uint64_t inicio = ahoraMs(), fin = inicio + (uint64_t)esperaMs;
	uint64_t proximo = inicio, eleccion = 0;
	bool confirmado = false, otroListo = false;
	int mejor = -1;
	uint8_t b[1500], sal[LARGO_SONDEO];
	auto prioridadDe = [&](const Direccion& d) {
		for (const Destino& x : destinos) if (x.dir == d) return x.prioridad;
		return 90;   // reflexiva del par (no estaba en sus candidatos)
	};
	int maxima = 0;
	for (const Destino& d : destinos) maxima = std::max(maxima, d.prioridad);
	while (true) {
		const uint64_t t = ahoraMs();
		if (confirmado && t >= eleccion && otroListo) break;
		if (t >= fin) break;
		if (t >= proximo) {
			for (const Destino& d : destinos) s.enviar(d.dir, sal, detalle_nat::armarSondeo(sal, 1, confirmado, idSesion, yo, 0));
			proximo = t + 150;
		}
		Direccion de;
		const int espera = (int)std::min<uint64_t>(proximo > t ? proximo - t : 0, 50);
		const int n = s.recibir(de, b, sizeof b, espera);
		if (n <= 0 || !esSondeo(b, (size_t)n) || detalle_nat::leer64(b + 6) != idSesion) continue;
		const uint64_t suyo = detalle_nat::leer64(b + 14);
		if (suyo == yo) continue;   // el propio (vuelta por el mismo router)
		if (b[5] & 1) otroListo = true;
		if (b[4] == 1) {
			s.enviar(de, sal, detalle_nat::armarSondeo(sal, 2, confirmado, idSesion, yo, suyo));
			bool conocido = false;
			for (const Destino& d : destinos) conocido |= d.dir == de;
			if (!conocido && (!de.v6 || s.esV6())) {
				destinos.push_back({ de, 90 });
				s.enviar(de, sal, detalle_nat::armarSondeo(sal, 1, confirmado, idSesion, yo, 0));
			}
		} else if (detalle_nat::leer64(b + 22) == yo) {
			const int p = prioridadDe(de);
			if (p > mejor) { mejor = p; elegido = de; }
			if (!confirmado) {
				confirmado = true;
				eleccion = ahoraMs() + (p >= maxima ? 0 : 500);
				proximo = ahoraMs();   // avisar ya que hay camino
			} else if (p >= maxima) eleccion = std::min<uint64_t>(eleccion, ahoraMs());
		}
	}
	if (!confirmado) return false;
	for (int i = 0; i < 3; i++) s.enviar(elegido, sal, detalle_nat::armarSondeo(sal, 1, true, idSesion, yo, 0));
	return true;
}

inline bool perforar(SocketUdp& s, const std::vector<Candidato>& remotos, uint64_t idSesion, int esperaMs, Direccion& elegido)
{
	return perforarCon(s, remotos, idSesion, esperaMs, elegido);
}

// ---------------------------------------------------------------------------------------------- codigo manual
// bytes: version/cantidad(1) idSesion(8) y por candidato: cabecera(1: tipo 3 bits, v6, "misma IP que el anterior")
// + IP (4/16, si no se repite) + puerto(2); al final 2 bytes de suma (CRC-32). Texto: base32 de Crockford (sin I, L,
// O, U) en grupos de 4 con guiones. Con STUN + UPnP + una LAN: ~45 caracteres
static const char* const ALFABETO_CODIGO = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
static const int VERSION_CODIGO = 1, MAX_CANDIDATOS_CODIGO = 6;

inline std::string codigoDeCandidatos(const std::vector<Candidato>& candidatos, uint64_t idSesion)
{
	std::vector<Candidato> c = candidatos;
	detalle_nat::ordenar(c);
	// lo que sirve desde afuera primero; de las LAN y las IPv6 alcanza una o dos
	std::vector<Candidato> elegidos;
	int lan = 0, v6 = 0;
	for (const Candidato& x : c) {
		if ((int)elegidos.size() >= MAX_CANDIDATOS_CODIGO) break;
		if (x.tipo == TipoCandidato::Local && lan++ >= 1) continue;
		if (x.tipo == TipoCandidato::Ipv6 && v6++ >= 1) continue;
		elegidos.push_back(x);
	}
	std::vector<uint8_t> b;
	b.push_back((uint8_t)((VERSION_CODIGO << 5) | elegidos.size()));
	for (int i = 0; i < 8; i++) b.push_back((uint8_t)(idSesion >> (56 - 8 * i)));
	const Direccion* anterior = nullptr;
	for (const Candidato& x : elegidos) {
		const bool repite = anterior && mismaIp(*anterior, x.dir);
		b.push_back((uint8_t)(((int)x.tipo << 5) | (x.dir.v6 ? 0x10 : 0) | (repite ? 0x08 : 0)));
		if (!repite) b.insert(b.end(), x.dir.ip, x.dir.ip + (x.dir.v6 ? 16 : 4));
		b.push_back((uint8_t)(x.dir.puerto >> 8));
		b.push_back((uint8_t)x.dir.puerto);
		anterior = &x.dir;
	}
	const uint32_t suma = stun::crc32(b.data(), b.size());
	b.push_back((uint8_t)(suma >> 8));
	b.push_back((uint8_t)suma);
	std::string t;
	uint32_t acum = 0;
	int bits = 0;
	for (uint8_t x : b) {
		acum = (acum << 8) | x;
		bits += 8;
		while (bits >= 5) { t += ALFABETO_CODIGO[(acum >> (bits - 5)) & 31]; bits -= 5; }
	}
	if (bits > 0) t += ALFABETO_CODIGO[(acum << (5 - bits)) & 31];
	std::string g;
	for (size_t i = 0; i < t.size(); i++) { if (i && i % 4 == 0) g += '-'; g += t[i]; }
	return g;
}
inline std::string codigoConexion(const InfoRed& info, uint64_t idSesion) { return codigoDeCandidatos(info.candidatos, idSesion); }

// Lee un codigo (mayusculas o minusculas, con o sin guiones/espacios; O->0, I/L->1). Tambien acepta una direccion
// a mano ("1.2.3.4:47924" o "[2001:db8::1]:47924": candidato Manual, idSesion 0; los dos lados usan 0)
inline bool leerCodigo(const std::string& texto, std::vector<Candidato>& candidatos, uint64_t& idSesion)
{
	candidatos.clear();
	idSesion = 0;
	{
		std::string limpio;
		for (char ch : texto) if (!isspace((unsigned char)ch)) limpio += ch;
		Direccion d;
		if (limpio.find_first_of(".:[") != std::string::npos && Direccion::desdeTexto(limpio, d)) {
			if (d.puerto == 0) d.puerto = PUERTO_JUEGO;
			Candidato c;
			c.dir = d;
			c.tipo = TipoCandidato::Manual;
			c.prioridad = prioridadTipo(c.tipo);
			candidatos.push_back(c);
			return true;
		}
	}
	std::vector<uint8_t> b;
	uint32_t acum = 0;
	int bits = 0;
	for (char ch : texto) {
		if (ch == '-' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') continue;
		char u = (char)toupper((unsigned char)ch);
		if (u == 'O') u = '0';
		if (u == 'I' || u == 'L') u = '1';
		const char* p = strchr(ALFABETO_CODIGO, u);
		if (!p || !u) return false;
		acum = (acum << 5) | (uint32_t)(p - ALFABETO_CODIGO);
		bits += 5;
		if (bits >= 8) { b.push_back((uint8_t)(acum >> (bits - 8))); bits -= 8; }
	}
	if (bits >= 5 || (acum & ((1u << bits) - 1)) != 0) return false;   // relleno: menos de 5 bits y en cero
	if (b.size() < 11 || (b[0] >> 5) != VERSION_CODIGO) return false;
	const uint32_t suma = stun::crc32(b.data(), b.size() - 2);
	if (b[b.size() - 2] != (uint8_t)(suma >> 8) || b[b.size() - 1] != (uint8_t)suma) return false;
	const size_t fin = b.size() - 2;
	const int cantidad = b[0] & 31;
	for (int i = 0; i < 8; i++) idSesion = (idSesion << 8) | b[1 + (size_t)i];
	size_t i = 9;
	const Direccion* anterior = nullptr;
	for (int k = 0; k < cantidad; k++) {
		if (i >= fin) return false;
		const uint8_t cab = b[i++];
		const int tipo = cab >> 5;
		if (tipo > (int)TipoCandidato::Manual) return false;
		Candidato c;
		c.tipo = (TipoCandidato)tipo;
		c.prioridad = prioridadTipo(c.tipo);
		c.dir.v6 = (cab & 0x10) != 0;
		const size_t largo = c.dir.v6 ? 16 : 4;
		if (cab & 0x08) {
			if (!anterior || anterior->v6 != c.dir.v6) return false;
			memcpy(c.dir.ip, anterior->ip, largo);
		} else {
			if (i + largo > fin) return false;
			memcpy(c.dir.ip, b.data() + i, largo);
			i += largo;
		}
		if (i + 2 > fin) return false;
		c.dir.puerto = (uint16_t)((b[i] << 8) | b[i + 1]);
		i += 2;
		candidatos.push_back(c);
		anterior = &candidatos.back().dir;
	}
	if (i != fin) return false;
	for (size_t k = 0; k < candidatos.size(); k++) candidatos[k].prioridad -= (int)k;   // conserva el orden
	return !candidatos.empty();
}

// un idSesion nuevo (nunca 0: el 0 es el de las direcciones a mano)
inline uint64_t nuevoIdSesion() { uint64_t v = 0; while (!v) v = detalle_nat::aleatorio64(); return v; }

}  // namespace red
