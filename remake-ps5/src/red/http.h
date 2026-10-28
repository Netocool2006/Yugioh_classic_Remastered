#pragma once
// Cliente HTTP/1.1 minimo (rama classic-sala) para la sala de Cloudflare: solo http:// (sin TLS: los *.workers.dev
// atienden http en el puerto 80; ver servidor/sala/LEEME.md), un pedido por conexion (Connection: close), cuerpo con
// Content-Length o "chunked". Es BLOQUEANTE con tiempo acotado: se llama desde el hilo de la sala (sala.h), nunca
// desde el cuadro. PS5 (sockets BSD), Linux y Windows (Winsock, como socket_udp.h).
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>
#include "socket_udp.h"
#include "resolver.h"   // dns::resolver: en la PS5 la consulta DNS propia (rama classic-red-nat)

namespace red {
namespace http {

struct Url {
	std::string host, ruta = "/";
	uint16_t puerto = 80;
};

// "http://host[:puerto][/ruta]" (https no: no hay TLS)
inline bool leerUrl(const std::string& u, Url& r)
{
	const std::string pre = "http://";
	if (u.compare(0, pre.size(), pre) != 0) return false;
	std::string resto = u.substr(pre.size());
	const size_t barra = resto.find('/');
	std::string hp = barra == std::string::npos ? resto : resto.substr(0, barra);
	r.ruta = barra == std::string::npos ? "/" : resto.substr(barra);
	if (!hp.empty() && hp[0] == '[') {   // [v6]:puerto
		const size_t c = hp.find(']');
		if (c == std::string::npos) return false;
		r.host = hp.substr(1, c - 1);
		if (c + 1 < hp.size() && hp[c + 1] == ':') r.puerto = (uint16_t)atoi(hp.c_str() + c + 2);
	} else {
		const size_t dp = hp.rfind(':');
		r.host = dp == std::string::npos ? hp : hp.substr(0, dp);
		if (dp != std::string::npos) r.puerto = (uint16_t)atoi(hp.c_str() + dp + 1);
	}
	return !r.host.empty() && r.puerto != 0;
}

struct Respuesta {
	int estado = 0;          // codigo HTTP (0: no hubo respuesta)
	std::string cuerpo;
	std::string error;       // por que fallo (sin red, tiempo, ...)
};

namespace detalle_http {
inline void noBloquear(red::detalle::Fd f)
{
#if defined(_WIN32)
	u_long nb = 1;
	ioctlsocket(f, FIONBIO, &nb);
#else
	fcntl(f, F_SETFL, fcntl(f, F_GETFL, 0) | O_NONBLOCK);
#endif
}
// espera a que el socket se pueda leer (leer = true) o escribir; false si se cumple el plazo o se cancela (se mira
// cada 100 ms: cancelar un long-polling no tarda)
inline bool esperar(red::detalle::Fd f, bool leer, uint64_t limite, const std::atomic<bool>* cancelar = nullptr)
{
	while (true) {
		const uint64_t t = ahoraMs();
		if (t >= limite || (cancelar && cancelar->load())) return false;
		const int ms = (int)std::min<uint64_t>(limite - t, 100);
#if defined(_WIN32)
		WSAPOLLFD p{};
		p.fd = f;
		p.events = leer ? POLLRDNORM : POLLWRNORM;
		const int r = WSAPoll(&p, 1, ms);
#else
		pollfd p{};
		p.fd = f;
		p.events = leer ? POLLIN : POLLOUT;
		int r = poll(&p, 1, ms);
		if (r < 0 && errno == EINTR) continue;
#endif
		if (r > 0) return true;
		if (r < 0) return false;
	}
}
inline bool enProgreso()
{
#if defined(_WIN32)
	const int e = WSAGetLastError();
	return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
#else
	return errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR;
#endif
}
// conecta (TCP) con plazo; FD_NULO si no se pudo
inline red::detalle::Fd conectar(const Direccion& d, uint64_t limite, const std::atomic<bool>* cancelar)
{
	sockaddr_storage ss;
	const socklen_t l = red::detalle::aSockaddr(d, d.v6, ss);
	red::detalle::Fd f = socket(d.v6 ? AF_INET6 : AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (f == red::detalle::FD_NULO) return f;
	noBloquear(f);
	if (connect(f, (const sockaddr*)&ss, l) != 0) {
		if (!enProgreso() || !esperar(f, false, limite, cancelar)) { red::detalle::cerrarFd(f); return red::detalle::FD_NULO; }
		int err = 0;
		socklen_t le = sizeof err;
		getsockopt(f, SOL_SOCKET, SO_ERROR, (char*)&err, &le);
		if (err != 0) { red::detalle::cerrarFd(f); return red::detalle::FD_NULO; }
	}
	return f;
}
// "chunked" -> cuerpo
inline bool desfragmentar(const std::string& s, std::string& out)
{
	size_t i = 0;
	out.clear();
	while (i < s.size()) {
		const size_t fin = s.find("\r\n", i);
		if (fin == std::string::npos) return false;
		const unsigned long n = strtoul(s.substr(i, fin - i).c_str(), nullptr, 16);
		if (n == 0) return true;
		if (fin + 2 + n > s.size()) return false;
		out += s.substr(fin + 2, n);
		i = fin + 2 + n + 2;
	}
	return false;
}
}  // namespace detalle_http

// un pedido entero (resolver, conectar, mandar, leer) en esperaMs como mucho. metodo: "GET" o "POST". cancelar: si se
// pone en true, el pedido se corta (en 100 ms como mucho; la resolucion del nombre no se corta)
inline Respuesta pedir(const std::string& url, const std::string& metodo, const std::string& cuerpo, int esperaMs,
	const std::atomic<bool>* cancelar = nullptr)
{
	Respuesta r;
	Url u;
	if (!leerUrl(url, u)) { r.error = "url"; return r; }
	if (!red::detalle::iniciarRed()) { r.error = "red"; return r; }
	const uint64_t limite = ahoraMs() + (uint64_t)esperaMs;
	std::vector<Direccion> dirs;
	if (!dns::resolver(u.host, false, u.puerto, dirs, 1500)) dns::resolver(u.host, true, u.puerto, dirs, 1500);
	if (dirs.empty()) { r.error = "dns"; return r; }
	red::detalle::Fd f = red::detalle::FD_NULO;
	for (const Direccion& d : dirs) {
		f = detalle_http::conectar(d, limite, cancelar);
		if (f != red::detalle::FD_NULO) break;
	}
	if (f == red::detalle::FD_NULO) { r.error = "conectar"; return r; }
	std::string pedido = metodo + " " + u.ruta + " HTTP/1.1\r\nHost: " + u.host +
		(u.puerto != 80 ? ":" + std::to_string(u.puerto) : "") +
		"\r\nUser-Agent: YugiohClassicRemaster\r\nConnection: close\r\nAccept: text/plain\r\n";
	if (metodo == "POST") pedido += "Content-Type: text/plain; charset=utf-8\r\nContent-Length: " + std::to_string(cuerpo.size()) + "\r\n";
	pedido += "\r\n";
	if (metodo == "POST") pedido += cuerpo;
	size_t hecho = 0;
	while (hecho < pedido.size()) {
		if (!detalle_http::esperar(f, false, limite, cancelar)) { r.error = "tiempo"; red::detalle::cerrarFd(f); return r; }
		#if defined(MSG_NOSIGNAL)
		const int n = (int)send(f, pedido.data() + hecho, (int)(pedido.size() - hecho), MSG_NOSIGNAL);   // sin SIGPIPE
#else
		const int n = (int)send(f, pedido.data() + hecho, (int)(pedido.size() - hecho), 0);
#endif
		if (n < 0 && detalle_http::enProgreso()) continue;
		if (n <= 0) { r.error = "mandar"; red::detalle::cerrarFd(f); return r; }
		hecho += (size_t)n;
	}
	std::string todo;
	char buf[4096];
	size_t largo = std::string::npos, cab = std::string::npos;
	while (true) {
		if (!detalle_http::esperar(f, true, limite, cancelar)) { r.error = "tiempo"; break; }
		const int n = (int)recv(f, buf, sizeof buf, 0);
		if (n < 0 && detalle_http::enProgreso()) continue;
		if (n <= 0) break;   // el servidor cerro: fin
		todo.append(buf, (size_t)n);
		if (todo.size() > (1u << 20)) { r.error = "grande"; break; }
		if (cab == std::string::npos) {
			cab = todo.find("\r\n\r\n");
			if (cab != std::string::npos) {
				std::string cabeza = todo.substr(0, cab);
				for (char& c : cabeza) c = (char)tolower((unsigned char)c);
				const size_t cl = cabeza.find("\r\ncontent-length:");
				if (cl != std::string::npos) largo = strtoul(cabeza.c_str() + cl + 17, nullptr, 10);
			}
		}
		if (cab != std::string::npos && largo != std::string::npos && todo.size() >= cab + 4 + largo) break;
	}
	red::detalle::cerrarFd(f);
	if (cab == std::string::npos) { if (r.error.empty()) r.error = "respuesta"; return r; }
	// "HTTP/1.1 200 OK"
	const size_t esp = todo.find(' ');
	if (esp == std::string::npos || esp > cab) { r.error = "respuesta"; return r; }
	r.estado = atoi(todo.c_str() + esp + 1);
	std::string cabeza = todo.substr(0, cab);
	for (char& c : cabeza) c = (char)tolower((unsigned char)c);
	r.cuerpo = todo.substr(cab + 4);
	if (largo != std::string::npos) {
		if (r.cuerpo.size() < largo) { r.estado = 0; if (r.error.empty()) r.error = "cortada"; return r; }
		r.cuerpo.resize(largo);
	} else if (cabeza.find("transfer-encoding: chunked") != std::string::npos) {
		std::string b;
		if (!detalle_http::desfragmentar(r.cuerpo, b)) { r.estado = 0; r.error = "cortada"; return r; }
		r.cuerpo = b;
	}
	r.error.clear();
	return r;
}

}  // namespace http
}  // namespace red
