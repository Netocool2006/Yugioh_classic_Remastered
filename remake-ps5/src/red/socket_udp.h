#pragma once
// Socket UDP multiplataforma del modo en linea (rama classic-red-base): PS5 (SDK de payloads, sockets BSD de FreeBSD),
// Linux (las pruebas del contenedor) y Windows (Winsock, la version de PC). Sin bloqueo: recibir() espera a lo sumo
// esperaMs con poll()/WSAPoll(). Lo usan el canal confiable (canal.h), la red local (lan.h) y las ramas de NAT y sala.
// API comun: no cambiar las firmas (las usan otras ramas); agregar es valido.
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace red {

// Direccion IPv4 o IPv6 con puerto. IPv4: ip[0..3] (el resto en cero) y v6 = false
struct Direccion {
	uint8_t ip[16] = {};
	bool v6 = false;
	uint16_t puerto = 0;

	// "1.2.3.4:5000" o "[2001:db8::1]:5000" (sin puerto si es 0)
	std::string texto() const
	{
		char b[64] = {};
		if (!v6) {
			snprintf(b, sizeof b, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
			if (puerto == 0) return b;
			return std::string(b) + ":" + std::to_string(puerto);
		}
		inet_ntop(AF_INET6, (void*)ip, b, sizeof b);
		if (puerto == 0) return b;
		return "[" + std::string(b) + "]:" + std::to_string(puerto);
	}
	// Acepta "1.2.3.4", "1.2.3.4:5000", "2001:db8::1", "[2001:db8::1]" y "[2001:db8::1]:5000"
	static bool desdeTexto(const std::string& s, Direccion& d)
	{
		d = Direccion();
		std::string host = s, puerto;
		if (!s.empty() && s[0] == '[') {
			size_t c = s.find(']');
			if (c == std::string::npos) return false;
			host = s.substr(1, c - 1);
			if (c + 1 < s.size()) {
				if (s[c + 1] != ':') return false;
				puerto = s.substr(c + 2);
			}
		} else if (s.find(':') != std::string::npos && s.find(':') == s.rfind(':')) {   // un solo ':' = v4 con puerto
			host = s.substr(0, s.find(':'));
			puerto = s.substr(s.find(':') + 1);
		}
		if (!puerto.empty()) {
			char* fin = nullptr;
			long p = strtol(puerto.c_str(), &fin, 10);
			if (*fin || p < 0 || p > 65535) return false;
			d.puerto = (uint16_t)p;
		}
		if (inet_pton(AF_INET, host.c_str(), d.ip) == 1) { d.v6 = false; return true; }
		if (inet_pton(AF_INET6, host.c_str(), d.ip) == 1) { d.v6 = true; return true; }
		d = Direccion();
		return false;
	}
	bool operator==(const Direccion& o) const
	{
		return v6 == o.v6 && puerto == o.puerto && memcmp(ip, o.ip, v6 ? 16 : 4) == 0;
	}
	bool operator!=(const Direccion& o) const { return !(*this == o); }
	// (agregado) loopback 127.x o ::1
	bool esLoopback() const
	{
		if (!v6) return ip[0] == 127;
		static const uint8_t uno[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
		return memcmp(ip, uno, 16) == 0;
	}
};

namespace detalle {
#if defined(_WIN32)
typedef SOCKET Fd;
static const Fd FD_NULO = INVALID_SOCKET;
inline bool iniciarRed()
{
	static const bool ok = [] { WSADATA w; return WSAStartup(MAKEWORD(2, 2), &w) == 0; }();
	return ok;
}
inline void cerrarFd(Fd f) { closesocket(f); }
#else
typedef int Fd;
static const Fd FD_NULO = -1;
inline bool iniciarRed() { return true; }
inline void cerrarFd(Fd f) { close(f); }
#endif

// Direccion -> sockaddr del socket (si el socket es v6 dual-stack, la v4 va como ::ffff:a.b.c.d)
inline socklen_t aSockaddr(const Direccion& d, bool socketV6, sockaddr_storage& ss)
{
	memset(&ss, 0, sizeof ss);
	if (!socketV6) {
		sockaddr_in* s = (sockaddr_in*)&ss;
		s->sin_family = AF_INET;
		s->sin_port = htons(d.puerto);
		memcpy(&s->sin_addr, d.ip, 4);
		return sizeof(sockaddr_in);
	}
	sockaddr_in6* s = (sockaddr_in6*)&ss;
	s->sin6_family = AF_INET6;
	s->sin6_port = htons(d.puerto);
	if (d.v6) memcpy(&s->sin6_addr, d.ip, 16);
	else {
		uint8_t* b = (uint8_t*)&s->sin6_addr;
		b[10] = 0xff; b[11] = 0xff;
		memcpy(b + 12, d.ip, 4);
	}
	return sizeof(sockaddr_in6);
}
// sockaddr -> Direccion (::ffff:a.b.c.d vuelve a ser v4)
inline bool deSockaddr(const sockaddr_storage& ss, Direccion& d)
{
	d = Direccion();
	if (ss.ss_family == AF_INET) {
		const sockaddr_in* s = (const sockaddr_in*)&ss;
		memcpy(d.ip, &s->sin_addr, 4);
		d.puerto = ntohs(s->sin_port);
		return true;
	}
	if (ss.ss_family == AF_INET6) {
		const sockaddr_in6* s = (const sockaddr_in6*)&ss;
		const uint8_t* b = (const uint8_t*)&s->sin6_addr;
		static const uint8_t mapeo[12] = {0,0,0,0,0,0,0,0,0,0,0xff,0xff};
		if (memcmp(b, mapeo, 12) == 0) memcpy(d.ip, b + 12, 4);
		else { memcpy(d.ip, b, 16); d.v6 = true; }
		d.puerto = ntohs(s->sin6_port);
		return true;
	}
	return false;
}
}  // namespace detalle

class SocketUdp {
 public:
	SocketUdp() = default;
	~SocketUdp() { cerrar(); }
	SocketUdp(const SocketUdp&) = delete;
	SocketUdp& operator=(const SocketUdp&) = delete;

	// 0 = puerto libre; v6 = dual-stack (IPv6 y IPv4 mapeada) si se puede, si no queda solo IPv4
	bool abrir(uint16_t puerto = 0, bool v6 = false)
	{
		cerrar();
		if (!detalle::iniciarRed()) return false;
		if (v6 && abrirFamilia(AF_INET6, puerto)) { v6_ = true; return true; }
		if (abrirFamilia(AF_INET, puerto)) { v6_ = false; return true; }
		return false;
	}
	void cerrar()
	{
		if (fd_ != detalle::FD_NULO) detalle::cerrarFd(fd_);
		fd_ = detalle::FD_NULO;
		puerto_ = 0;
	}
	bool enviar(const Direccion& a, const uint8_t* datos, size_t n)
	{
		if (fd_ == detalle::FD_NULO) return false;
		if (a.v6 && !v6_) return false;   // socket solo IPv4
		sockaddr_storage ss;
		socklen_t l = detalle::aSockaddr(a, v6_, ss);
		return sendto(fd_, (const char*)datos, (int)n, 0, (const sockaddr*)&ss, l) == (int)n;
	}
	// -1 error, 0 nada en esperaMs, >0 bytes recibidos (de = quien lo mando)
	int recibir(Direccion& de, uint8_t* buf, size_t cap, int esperaMs)
	{
		if (fd_ == detalle::FD_NULO) return -1;
#if defined(_WIN32)
		WSAPOLLFD p{};
		p.fd = fd_;
		p.events = POLLRDNORM;
		int r = WSAPoll(&p, 1, esperaMs < 0 ? 0 : esperaMs);
#else
		pollfd p{};
		p.fd = fd_;
		p.events = POLLIN;
		int r = poll(&p, 1, esperaMs < 0 ? 0 : esperaMs);
		if (r < 0 && errno == EINTR) return 0;
#endif
		if (r < 0) return -1;
		if (r == 0) return 0;
		sockaddr_storage ss;
		socklen_t l = sizeof ss;
		int n = (int)recvfrom(fd_, (char*)buf, (int)cap, 0, (sockaddr*)&ss, &l);
		if (n < 0) {
#if defined(_WIN32)
			int e = WSAGetLastError();
			if (e == WSAEWOULDBLOCK || e == WSAECONNRESET || e == WSAEMSGSIZE) return 0;   // ICMP de puerto cerrado
#else
			if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ECONNREFUSED || errno == EINTR) return 0;
#endif
			return -1;
		}
		if (!detalle::deSockaddr(ss, de)) return 0;
		return n;
	}
	uint16_t puertoLocal() const { return puerto_; }
	bool abierto() const { return fd_ != detalle::FD_NULO; }

	// (agregados) socket IPv6 dual-stack; permitir broadcast IPv4 (lan.h); unirse a un grupo multicast IPv6 (lan.h)
	bool esV6() const { return v6_; }
	bool permitirBroadcast()
	{
		int si = 1;
		return fd_ != detalle::FD_NULO && setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, (const char*)&si, sizeof si) == 0;
	}
	bool unirMulticastV6(const Direccion& grupo, unsigned interfaz = 0)
	{
		if (fd_ == detalle::FD_NULO || !v6_ || !grupo.v6) return false;
		ipv6_mreq m{};
		memcpy(&m.ipv6mr_multiaddr, grupo.ip, 16);
		m.ipv6mr_interface = interfaz;
		return setsockopt(fd_, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char*)&m, sizeof m) == 0;
	}
	// (agregado) reutilizar el puerto (varios oyentes de la red local en la misma maquina). soloV6: sin IPv4 mapeada (para
	// tener otro socket IPv4 en el mismo puerto)
	bool abrirCompartido(uint16_t puerto, bool v6, bool soloV6 = false)
	{
		cerrar();
		if (!detalle::iniciarRed()) return false;
		compartir_ = true;
		soloV6_ = soloV6;
		bool ok = false;
		if (v6 && abrirFamilia(AF_INET6, puerto)) { v6_ = true; ok = true; }
		else if (abrirFamilia(AF_INET, puerto)) { v6_ = false; ok = true; }
		compartir_ = false;
		soloV6_ = false;
		return ok;
	}

 private:
	bool abrirFamilia(int fam, uint16_t puerto)
	{
		detalle::Fd f = socket(fam, SOCK_DGRAM, IPPROTO_UDP);
		if (f == detalle::FD_NULO) return false;
		int no = 0, si = 1;
		if (fam == AF_INET6) setsockopt(f, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)(soloV6_ ? &si : &no), sizeof no);   // dual-stack
		if (compartir_) {
			setsockopt(f, SOL_SOCKET, SO_REUSEADDR, (const char*)&si, sizeof si);
#if defined(SO_REUSEPORT)
			setsockopt(f, SOL_SOCKET, SO_REUSEPORT, (const char*)&si, sizeof si);
#endif
		}
		sockaddr_storage ss;
		memset(&ss, 0, sizeof ss);
		socklen_t l;
		if (fam == AF_INET6) {
			sockaddr_in6* s = (sockaddr_in6*)&ss;
			s->sin6_family = AF_INET6;
			s->sin6_port = htons(puerto);
			s->sin6_addr = in6addr_any;
			l = sizeof(sockaddr_in6);
		} else {
			sockaddr_in* s = (sockaddr_in*)&ss;
			s->sin_family = AF_INET;
			s->sin_port = htons(puerto);
			s->sin_addr.s_addr = htonl(INADDR_ANY);
			l = sizeof(sockaddr_in);
		}
		if (bind(f, (const sockaddr*)&ss, l) != 0) { detalle::cerrarFd(f); return false; }
#if defined(_WIN32)
		u_long nb = 1;
		ioctlsocket(f, FIONBIO, &nb);
		// sin esto, un ICMP «puerto inalcanzable» hace fallar el siguiente recvfrom con WSAECONNRESET
		DWORD falso = FALSE, dev = 0;
		WSAIoctl(f, _WSAIOW(IOC_VENDOR, 12), &falso, sizeof falso, nullptr, 0, &dev, nullptr, nullptr);
#else
		fcntl(f, F_SETFL, fcntl(f, F_GETFL, 0) | O_NONBLOCK);
#endif
		sockaddr_storage yo;
		socklen_t ly = sizeof yo;
		if (getsockname(f, (sockaddr*)&yo, &ly) == 0) {
			Direccion d;
			detalle::deSockaddr(yo, d);
			puerto_ = d.puerto;
		}
		fd_ = f;
		return true;
	}
	detalle::Fd fd_ = detalle::FD_NULO;
	bool v6_ = false, compartir_ = false, soloV6_ = false;
	uint16_t puerto_ = 0;
};

// IPv4/IPv6 de las interfaces (sin loopback ni link-local IPv6 fe80::/10); puerto 0
inline std::vector<Direccion> direccionesLocales()
{
	std::vector<Direccion> r;
	auto agregar = [&](const sockaddr* sa) {
		if (!sa) return;
		sockaddr_storage ss;
		memset(&ss, 0, sizeof ss);
		memcpy(&ss, sa, sa->sa_family == AF_INET6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in));
		Direccion d;
		if (!detalle::deSockaddr(ss, d) || d.esLoopback()) return;
		d.puerto = 0;
		if (d.v6 && d.ip[0] == 0xfe && (d.ip[1] & 0xc0) == 0x80) return;
		static const uint8_t cero[16] = {};
		if (memcmp(d.ip, cero, d.v6 ? 16 : 4) == 0) return;
		for (const Direccion& x : r) if (x == d) return;
		r.push_back(d);
	};
#if defined(_WIN32)
	if (!detalle::iniciarRed()) return r;
	char nombre[256] = {};
	if (gethostname(nombre, sizeof nombre) != 0) return r;
	addrinfo pista{}, *res = nullptr;
	pista.ai_family = AF_UNSPEC;
	pista.ai_socktype = SOCK_DGRAM;
	if (getaddrinfo(nombre, nullptr, &pista, &res) != 0) return r;
	for (addrinfo* a = res; a; a = a->ai_next) agregar(a->ai_addr);
	freeaddrinfo(res);
#else
	ifaddrs* lista = nullptr;
	if (getifaddrs(&lista) != 0) return r;
	for (ifaddrs* i = lista; i; i = i->ifa_next) {
		if (!i->ifa_addr || !(i->ifa_flags & IFF_UP)) continue;
		if (i->ifa_addr->sa_family != AF_INET && i->ifa_addr->sa_family != AF_INET6) continue;
		agregar(i->ifa_addr);
	}
	freeifaddrs(lista);
#endif
	return r;
}

// Milisegundos de un reloj monotono (no la hora del dia)
inline uint64_t ahoraMs()
{
	return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace red
