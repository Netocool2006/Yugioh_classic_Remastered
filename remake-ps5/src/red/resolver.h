#pragma once
// Resolver nombres (DNS) del modo en linea (rama classic-red-nat), para los servidores STUN.
// - PC (Linux/Windows): getaddrinfo del sistema y, si falla, la consulta propia.
// - PS5: el SDK de payloads trae getaddrinfo (libc de FreeBSD), pero lee /etc/resolv.conf, que el titulo no tiene:
//   ahi va directo la consulta propia (un paquete UDP a 1.1.1.1 / 8.8.8.8 / 9.9.9.9 y al router), con tiempo acotado.
// - Si nada resuelve, las IP de respaldo de datos/red_stun.txt (actualizable sin recompilar) o las de este archivo.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <random>
#include "socket_udp.h"
#if !defined(_WIN32)
#include <netdb.h>
#endif

namespace red {
namespace dns {

// consulta DNS (RD = recursiva) de tipo 1 (A) o 28 (AAAA). Devuelve el largo (0 si el nombre no sirve)
inline size_t armarConsulta(uint8_t* buf, size_t cap, uint16_t id, const std::string& nombre, uint16_t tipo)
{
	if (cap < 18 + nombre.size()) return 0;
	const uint8_t cab[12] = { (uint8_t)(id >> 8), (uint8_t)id, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0 };
	memcpy(buf, cab, 12);
	size_t i = 12;
	size_t ini = 0;
	while (ini <= nombre.size()) {
		size_t fin = nombre.find('.', ini);
		if (fin == std::string::npos) fin = nombre.size();
		const size_t largo = fin - ini;
		if (largo == 0) { if (fin == nombre.size()) break; return 0; }   // (permite el punto final)
		if (largo > 63) return 0;
		buf[i++] = (uint8_t)largo;
		memcpy(buf + i, nombre.data() + ini, largo);
		i += largo;
		ini = fin + 1;
	}
	buf[i++] = 0;
	buf[i++] = (uint8_t)(tipo >> 8); buf[i++] = (uint8_t)tipo;
	buf[i++] = 0; buf[i++] = 1;   // clase IN
	return i;
}

// salta un nombre (con punteros de compresion); false si se sale del paquete
inline bool saltarNombre(const uint8_t* p, size_t n, size_t& i)
{
	for (int vueltas = 0; vueltas < 128; vueltas++) {
		if (i >= n) return false;
		const uint8_t l = p[i];
		if ((l & 0xC0) == 0xC0) { if (i + 2 > n) return false; i += 2; return true; }
		if (l & 0xC0) return false;
		if (l == 0) { i++; return true; }
		i += 1 + l;
	}
	return false;
}

// lee la respuesta: las A (o AAAA) de las respuestas, con el puerto dado
inline bool leerRespuesta(const uint8_t* p, size_t n, uint16_t id, uint16_t tipo, uint16_t puerto, std::vector<Direccion>& out)
{
	if (n < 12 || ((p[0] << 8) | p[1]) != id || !(p[2] & 0x80) || (p[3] & 0x0F) != 0) return false;
	const int qd = (p[4] << 8) | p[5], an = (p[6] << 8) | p[7];
	size_t i = 12;
	for (int q = 0; q < qd; q++) { if (!saltarNombre(p, n, i) || i + 4 > n) return false; i += 4; }
	const size_t antes = out.size();
	for (int a = 0; a < an; a++) {
		if (!saltarNombre(p, n, i) || i + 10 > n) return false;
		const uint16_t t = (uint16_t)((p[i] << 8) | p[i + 1]), clase = (uint16_t)((p[i + 2] << 8) | p[i + 3]);
		const uint16_t largo = (uint16_t)((p[i + 8] << 8) | p[i + 9]);
		i += 10;
		if (i + largo > n) return false;
		if (t == tipo && clase == 1 && largo == (tipo == 28 ? 16 : 4)) {
			Direccion d;
			d.v6 = tipo == 28;
			memcpy(d.ip, p + i, largo);
			d.puerto = puerto;
			out.push_back(d);
		}
		i += largo;
	}
	return out.size() > antes;
}

// consulta propia por UDP a los servidores dados (en paralelo, reintento cada 400 ms)
inline bool consultar(const std::string& nombre, bool v6, uint16_t puerto, const std::vector<Direccion>& servidores,
	int esperaMs, std::vector<Direccion>& out)
{
	SocketUdp s;
	if (!s.abrir(0, v6) && !s.abrir(0, false)) return false;
	std::random_device rd;
	const uint16_t id = (uint16_t)rd();
	const uint16_t tipo = v6 ? 28 : 1;
	uint8_t q[300], r[1500];
	const size_t nq = armarConsulta(q, sizeof q, id, nombre, tipo);
	if (!nq) return false;
	const uint64_t fin = ahoraMs() + (uint64_t)esperaMs;
	uint64_t reenvio = 0;
	while (ahoraMs() < fin) {
		if (ahoraMs() >= reenvio) {
			for (const Direccion& d : servidores) s.enviar(d, q, nq);
			reenvio = ahoraMs() + 400;
		}
		Direccion de;
		const int n = s.recibir(de, r, sizeof r, 50);
		if (n > 0 && de.puerto == 53 && leerRespuesta(r, (size_t)n, id, tipo, puerto, out)) return true;
	}
	return false;
}

// el resolver del sistema (no en la PS5)
inline bool sistema(const std::string& nombre, bool v6, uint16_t puerto, std::vector<Direccion>& out)
{
#if defined(__PROSPERO__)
	(void)nombre; (void)v6; (void)puerto; (void)out;
	return false;
#else
	if (!detalle::iniciarRed()) return false;
	addrinfo pista{}, *res = nullptr;
	pista.ai_family = v6 ? AF_INET6 : AF_INET;
	pista.ai_socktype = SOCK_DGRAM;
	if (getaddrinfo(nombre.c_str(), nullptr, &pista, &res) != 0) return false;
	const size_t antes = out.size();
	for (addrinfo* a = res; a; a = a->ai_next) {
		sockaddr_storage ss;
		memset(&ss, 0, sizeof ss);
		memcpy(&ss, a->ai_addr, a->ai_addrlen < sizeof ss ? a->ai_addrlen : sizeof ss);
		Direccion d;
		if (!detalle::deSockaddr(ss, d) || d.v6 != v6) continue;
		d.puerto = puerto;
		bool repetida = false;
		for (const Direccion& x : out) repetida |= x == d;
		if (!repetida) out.push_back(d);
	}
	freeaddrinfo(res);
	return out.size() > antes;
#endif
}

// servidores DNS publicos para la consulta propia (+ el router, si se sabe)
inline std::vector<Direccion> servidoresPublicos(bool v6)
{
	std::vector<Direccion> r;
	const char* const v4[] = { "1.1.1.1:53", "8.8.8.8:53", "9.9.9.9:53" };
	const char* const v6s[] = { "[2606:4700:4700::1111]:53", "[2001:4860:4860::8888]:53" };
	if (v6) for (const char* t : v6s) { Direccion d; if (Direccion::desdeTexto(t, d)) r.push_back(d); }
	else for (const char* t : v4) { Direccion d; if (Direccion::desdeTexto(t, d)) r.push_back(d); }
	return r;
}

// nombre -> direcciones (con puerto). Orden: sistema (no PS5) y consulta propia
inline bool resolver(const std::string& nombre, bool v6, uint16_t puerto, std::vector<Direccion>& out, int esperaMs = 1500,
	const Direccion* router = nullptr)
{
	Direccion literal;
	if (Direccion::desdeTexto(nombre, literal)) {
		if (literal.v6 != v6) return false;
		literal.puerto = puerto;
		out.push_back(literal);
		return true;
	}
	if (sistema(nombre, v6, puerto, out)) return true;
	std::vector<Direccion> serv = servidoresPublicos(v6);
	if (router && router->v6 == v6) { Direccion r = *router; r.puerto = 53; serv.push_back(r); }
	return consultar(nombre, v6, puerto, serv, esperaMs, out);
}

}  // namespace dns

// ---------------------------------------------------------------------------------------------- servidores STUN
// Un servidor STUN: nombre, puerto e IP de respaldo (por si no hay DNS)
struct ServidorStun {
	std::string nombre;
	uint16_t puerto = 3478;
	std::vector<Direccion> respaldo;
};

// La lista: datos/red_stun.txt ("nombre puerto [ip_respaldo ...]", # comenta) o, sin archivo, la de aqui
inline std::vector<ServidorStun> servidoresStun(const std::string& archivo = "")
{
	std::vector<ServidorStun> r;
	auto linea = [&](const std::string& l) {
		std::istringstream is(l);
		ServidorStun s;
		int puerto = 0;
		if (!(is >> s.nombre >> puerto) || s.nombre[0] == '#' || puerto <= 0 || puerto > 65535) return;
		s.puerto = (uint16_t)puerto;
		std::string ip;
		while (is >> ip) {
			if (ip[0] == '#') break;
			Direccion d;
			if (Direccion::desdeTexto(ip, d)) { d.puerto = s.puerto; s.respaldo.push_back(d); }
		}
		r.push_back(s);
	};
	if (!archivo.empty()) {
		std::ifstream f(archivo);
		std::string l;
		while (std::getline(f, l)) if (!l.empty()) linea(l);
	}
	if (r.empty()) {
		const char* const fijos[] = {
			"stun.l.google.com 19302 74.125.250.129", "stun.cloudflare.com 3478 162.159.207.0",
			"stun1.l.google.com 19302", "stun2.l.google.com 19302", "stun3.l.google.com 19302", "stun4.l.google.com 19302",
		};
		for (const char* l : fijos) linea(l);
	}
	return r;
}

}  // namespace red
