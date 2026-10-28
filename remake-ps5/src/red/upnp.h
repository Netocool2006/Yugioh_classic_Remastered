#pragma once
// UPnP IGD y NAT-PMP del modo en linea (rama classic-red-nat): pedirle al router que abra el puerto UDP del juego.
// - SSDP: M-SEARCH a 239.255.255.250:1900; el router responde con LOCATION (la URL de su descripcion).
// - HTTP (TCP propio, sin bloqueo y con tiempo limite): la descripcion XML y las acciones SOAP sobre
//   WANIPConnection:2/:1 o WANPPPConnection:1: GetExternalIPAddress, AddPortMapping (UDP, 2 h, "Yugioh Classic
//   Remaster") y DeletePortMapping al salir.
// - NAT-PMP (RFC 6886, puerto 5351 del router) si no hay UPnP: direccion externa y mapeo UDP.
// Las funciones de texto (armar y leer) son puras: las prueban prueba_red_nat.cpp con respuestas de routers grabadas.
#include <cstdint>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include "socket_udp.h"

namespace red {
namespace upnp {

static const char* const DESCRIPCION_MAPEO = "Yugioh Classic Remaster";
static const int DURACION_MAPEO_S = 7200;   // 2 h: si el juego se cuelga, el router lo borra solo

// ---------------------------------------------------------------------------------------------- texto
inline std::string minusculas(std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; }
inline std::string recortar(const std::string& s)
{
	size_t a = 0, b = s.size();
	while (a < b && isspace((unsigned char)s[a])) a++;
	while (b > a && isspace((unsigned char)s[b - 1])) b--;
	return s.substr(a, b - a);
}

// M-SEARCH para un tipo de busqueda (ST)
inline std::string armarBusqueda(const std::string& st, int mx = 2)
{
	return "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: " + std::to_string(mx) +
		"\r\nST: " + st + "\r\n\r\n";
}
// lo que se busca: la pasarela (v1 y v2) y los servicios directos (hay routers que solo responden a esos)
inline std::vector<std::string> tiposBusqueda()
{
	return { "urn:schemas-upnp-org:device:InternetGatewayDevice:1", "urn:schemas-upnp-org:device:InternetGatewayDevice:2",
		"urn:schemas-upnp-org:service:WANIPConnection:1", "urn:schemas-upnp-org:service:WANPPPConnection:1" };
}

// una cabecera HTTP (sin importar mayusculas); "" si no esta
inline std::string cabecera(const std::string& msj, const std::string& nombre)
{
	const std::string m = minusculas(msj), buscar = minusculas(nombre) + ":";
	size_t i = 0;
	while (i < m.size()) {
		size_t fin = m.find('\n', i);
		if (fin == std::string::npos) fin = m.size();
		if (m.compare(i, buscar.size(), buscar) == 0) return recortar(msj.substr(i + buscar.size(), fin - i - buscar.size()));
		if (fin == i || (fin == i + 1 && m[i] == '\r')) break;   // fin de las cabeceras
		i = fin + 1;
	}
	return "";
}

// respuesta SSDP -> LOCATION (solo si es una pasarela o un servicio WAN)
inline bool leerRespuestaSsdp(const std::string& msj, std::string& ubicacion)
{
	if (minusculas(msj.substr(0, 12)).find("http/1.") != 0) return false;
	if (msj.find(" 200") == std::string::npos) return false;
	const std::string st = minusculas(cabecera(msj, "ST") + cabecera(msj, "NT") + cabecera(msj, "USN"));
	if (st.find("internetgatewaydevice") == std::string::npos && st.find("wanipconnection") == std::string::npos &&
		st.find("wanpppconnection") == std::string::npos) return false;
	ubicacion = cabecera(msj, "LOCATION");
	return ubicacion.compare(0, 7, "http://") == 0 || ubicacion.compare(0, 7, "HTTP://") == 0;
}

// http://ip[:puerto][/ruta] -> partes (solo IP literal: los routers no mandan nombres)
struct Url {
	Direccion dir;
	std::string ruta = "/";
	std::string anfitrion() const { return dir.texto(); }
};
inline bool leerUrl(const std::string& url, Url& u)
{
	if (minusculas(url.substr(0, 7)) != "http://") return false;
	const size_t barra = url.find('/', 7);
	const std::string host = url.substr(7, barra == std::string::npos ? std::string::npos : barra - 7);
	u.ruta = barra == std::string::npos ? "/" : url.substr(barra);
	if (!Direccion::desdeTexto(host, u.dir)) return false;
	if (u.dir.puerto == 0) u.dir.puerto = 80;
	return true;
}
// la URL de control (absoluta, "/ruta" o "ruta") contra la base
inline bool resolverUrl(const Url& base, const std::string& ref, Url& u)
{
	if (minusculas(ref.substr(0, 7)) == "http://") return leerUrl(ref, u);
	u = base;
	if (!ref.empty() && ref[0] == '/') u.ruta = ref;
	else {
		const size_t b = base.ruta.rfind('/');
		u.ruta = (b == std::string::npos ? "/" : base.ruta.substr(0, b + 1)) + ref;
	}
	return true;
}

// el texto de la primera etiqueta <nombre> o <pref:nombre> desde i ("" si no esta); fin = despues del cierre
inline std::string etiqueta(const std::string& xml, const std::string& nombre, size_t desde = 0, size_t* fin = nullptr)
{
	const std::string abre = nombre + ">";
	for (size_t p = xml.find(abre, desde); p != std::string::npos; p = xml.find(abre, p + 1)) {
		size_t a = p;   // retrocede hasta el '<' (puede haber prefijo "u:")
		while (a > 0 && xml[a - 1] != '<' && xml[a - 1] != '>' && xml[a - 1] != '/' && !isspace((unsigned char)xml[a - 1])) a--;
		if (a == 0 || xml[a - 1] != '<') continue;   // era un cierre (</..) o texto
		if (a != p && xml[p - 1] != ':') continue;
		const size_t ini = p + abre.size();
		const size_t cierre = xml.find("</", ini);
		if (cierre == std::string::npos) return "";
		if (fin) *fin = cierre;
		return recortar(xml.substr(ini, cierre - ini));
	}
	return "";
}

// un servicio WAN de la descripcion
struct Servicio {
	std::string tipo;   // urn:schemas-upnp-org:service:WANIPConnection:1 ...
	Url control;
};
// los servicios WAN de la descripcion, del preferido al ultimo: WANIPConnection:2, :1, WANPPPConnection:1
inline std::vector<Servicio> leerDescripcion(const std::string& xml, const Url& ubicacion)
{
	Url base = ubicacion;
	const std::string urlBase = etiqueta(xml, "URLBase");
	if (!urlBase.empty()) { Url b; if (leerUrl(urlBase, b)) base = b; }
	std::vector<Servicio> r[3];
	size_t i = 0;
	while (true) {
		const size_t ini = xml.find("<service>", i);
		if (ini == std::string::npos) break;
		size_t fin = xml.find("</service>", ini);
		if (fin == std::string::npos) break;
		const std::string bloque = xml.substr(ini, fin - ini);
		i = fin + 10;
		Servicio s;
		s.tipo = etiqueta(bloque, "serviceType");
		const std::string ctl = etiqueta(bloque, "controlURL");
		if (ctl.empty() || !resolverUrl(base, ctl, s.control)) continue;
		if (s.tipo.find(":WANIPConnection:2") != std::string::npos) r[0].push_back(s);
		else if (s.tipo.find(":WANIPConnection:1") != std::string::npos) r[1].push_back(s);
		else if (s.tipo.find(":WANPPPConnection:1") != std::string::npos) r[2].push_back(s);
	}
	std::vector<Servicio> todo;
	for (auto& v : r) todo.insert(todo.end(), v.begin(), v.end());
	return todo;
}

// cuerpo SOAP de una accion (args: nombre, valor; en el orden del estandar: algunos routers lo exigen)
inline std::string armarSoap(const std::string& tipo, const std::string& accion, const std::vector<std::pair<std::string, std::string>>& args)
{
	std::string c = "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:" + accion + " xmlns:u=\"" + tipo + "\">";
	for (const auto& a : args) c += "<" + a.first + ">" + a.second + "</" + a.first + ">";
	return c + "</u:" + accion + "></s:Body></s:Envelope>\r\n";
}
inline std::string armarPeticionSoap(const Url& ctl, const std::string& tipo, const std::string& accion, const std::string& cuerpo)
{
	return "POST " + ctl.ruta + " HTTP/1.1\r\nHOST: " + ctl.anfitrion() + "\r\nCONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
		"SOAPACTION: \"" + tipo + "#" + accion + "\"\r\nCONTENT-LENGTH: " + std::to_string(cuerpo.size()) +
		"\r\nCONNECTION: close\r\nUSER-AGENT: YugiohClassicRemaster UPnP/1.1\r\n\r\n" + cuerpo;
}
inline std::string armarAgregar(const std::string& tipo, uint16_t externo, uint16_t interno, const std::string& ipInterna, int duracionS)
{
	return armarSoap(tipo, "AddPortMapping", {
		{ "NewRemoteHost", "" }, { "NewExternalPort", std::to_string(externo) }, { "NewProtocol", "UDP" },
		{ "NewInternalPort", std::to_string(interno) }, { "NewInternalClient", ipInterna }, { "NewEnabled", "1" },
		{ "NewPortMappingDescription", DESCRIPCION_MAPEO }, { "NewLeaseDuration", std::to_string(duracionS) } });
}
inline std::string armarBorrar(const std::string& tipo, uint16_t externo)
{
	return armarSoap(tipo, "DeletePortMapping", { { "NewRemoteHost", "" }, { "NewExternalPort", std::to_string(externo) }, { "NewProtocol", "UDP" } });
}
inline std::string armarIpExterna(const std::string& tipo) { return armarSoap(tipo, "GetExternalIPAddress", {}); }

// respuesta HTTP partida: estado y cuerpo (Content-Length o "chunked")
struct RespuestaHttp {
	int estado = 0;
	std::string cabeceras, cuerpo;
};
inline bool leerHttp(const std::string& crudo, RespuestaHttp& r)
{
	const size_t fc = crudo.find("\r\n\r\n");
	if (crudo.compare(0, 5, "HTTP/") != 0 || fc == std::string::npos) return false;
	r.cabeceras = crudo.substr(0, fc + 2);
	const size_t esp = crudo.find(' ');
	r.estado = esp == std::string::npos ? 0 : atoi(crudo.c_str() + esp + 1);
	std::string cuerpo = crudo.substr(fc + 4);
	if (minusculas(cabecera(r.cabeceras, "Transfer-Encoding")).find("chunked") != std::string::npos) {
		std::string limpio;
		size_t i = 0;
		while (i < cuerpo.size()) {
			const size_t eol = cuerpo.find("\r\n", i);
			if (eol == std::string::npos) return false;
			const long t = strtol(cuerpo.substr(i, eol - i).c_str(), nullptr, 16);
			if (t <= 0) break;
			if (eol + 2 + (size_t)t > cuerpo.size()) return false;
			limpio += cuerpo.substr(eol + 2, (size_t)t);
			i = eol + 2 + (size_t)t + 2;
		}
		cuerpo = limpio;
	} else {
		const std::string cl = cabecera(r.cabeceras, "Content-Length");
		if (!cl.empty()) {
			const size_t n = (size_t)atol(cl.c_str());
			if (cuerpo.size() < n) return false;
			cuerpo.resize(n);
		}
	}
	r.cuerpo = cuerpo;
	return true;
}
// ¿ya llego entera? (para dejar de leer antes de que el router cierre)
inline bool httpCompleta(const std::string& crudo)
{
	const size_t fc = crudo.find("\r\n\r\n");
	if (fc == std::string::npos) return false;
	const std::string cab = crudo.substr(0, fc + 2);
	if (minusculas(cabecera(cab, "Transfer-Encoding")).find("chunked") != std::string::npos)
		return crudo.find("\r\n0\r\n", fc) != std::string::npos || crudo.compare(fc + 4, 3, "0\r\n") == 0;
	const std::string cl = cabecera(cab, "Content-Length");
	return !cl.empty() && crudo.size() >= fc + 4 + (size_t)atol(cl.c_str());
}

// el codigo de error UPnP de una falla SOAP (718 = puerto ocupado, 725 = solo mapeos permanentes...), 0 si no hay
inline int codigoError(const std::string& cuerpo) { const std::string e = etiqueta(cuerpo, "errorCode"); return e.empty() ? 0 : atoi(e.c_str()); }

// ---------------------------------------------------------------------------------------------- TCP (HTTP)
namespace detalle_tcp {
#if defined(_WIN32)
inline bool enProgreso() { return WSAGetLastError() == WSAEWOULDBLOCK; }
inline void sinBloqueo(red::detalle::Fd f) { u_long nb = 1; ioctlsocket(f, FIONBIO, &nb); }
inline int esperar(red::detalle::Fd f, short ev, int ms) { WSAPOLLFD p{}; p.fd = f; p.events = ev; return WSAPoll(&p, 1, ms); }
#else
inline bool enProgreso() { return errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN; }
inline void sinBloqueo(red::detalle::Fd f) { fcntl(f, F_SETFL, fcntl(f, F_GETFL, 0) | O_NONBLOCK); }
inline int esperar(red::detalle::Fd f, short ev, int ms) { pollfd p{}; p.fd = f; p.events = ev; return poll(&p, 1, ms); }
#endif
}  // namespace detalle_tcp

// una peticion HTTP entera por TCP (conectar, mandar, leer hasta completar o cerrar) con tiempo limite total.
// local = la IP propia con la que se llego al router (la "NewInternalClient" del mapeo)
inline bool pedirHttp(const Direccion& a, const std::string& peticion, int esperaMs, std::string& respuesta, Direccion* local = nullptr)
{
	using namespace red::detalle;
	respuesta.clear();
	if (!iniciarRed() || a.v6) return false;
	Fd f = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (f == FD_NULO) return false;
	detalle_tcp::sinBloqueo(f);
	const uint64_t fin = ahoraMs() + (uint64_t)esperaMs;
	auto queda = [&]() { const uint64_t t = ahoraMs(); return t >= fin ? 0 : (int)(fin - t); };
	sockaddr_storage ss;
	const socklen_t l = aSockaddr(a, false, ss);
	bool ok = connect(f, (const sockaddr*)&ss, l) == 0;
	if (!ok && detalle_tcp::enProgreso() && detalle_tcp::esperar(f, POLLOUT, queda()) > 0) {
		int err = 0;
		socklen_t le = sizeof err;
		ok = getsockopt(f, SOL_SOCKET, SO_ERROR, (char*)&err, &le) == 0 && err == 0;
	}
	if (ok && local) {
		sockaddr_storage yo;
		socklen_t ly = sizeof yo;
		if (getsockname(f, (sockaddr*)&yo, &ly) == 0) deSockaddr(yo, *local);
	}
	size_t enviado = 0;
	while (ok && enviado < peticion.size()) {
		const int n = (int)send(f, peticion.data() + enviado, (int)(peticion.size() - enviado), 0);
		if (n > 0) { enviado += (size_t)n; continue; }
		if (n < 0 && detalle_tcp::enProgreso() && queda() > 0 && detalle_tcp::esperar(f, POLLOUT, queda()) > 0) continue;
		ok = false;
	}
	char b[4096];
	while (ok && queda() > 0 && respuesta.size() < (1u << 20)) {
		if (detalle_tcp::esperar(f, POLLIN, queda()) <= 0) break;
		const int n = (int)recv(f, b, sizeof b, 0);
		if (n > 0) { respuesta.append(b, (size_t)n); if (httpCompleta(respuesta)) break; continue; }
		if (n < 0 && detalle_tcp::enProgreso()) continue;
		break;   // cerrado
	}
	cerrarFd(f);
	return ok && !respuesta.empty();
}

// una accion SOAP: devuelve el estado HTTP (0 sin respuesta) y el cuerpo
inline int accion(const Servicio& s, const std::string& nombre, const std::string& cuerpoSoap, std::string& cuerpo, int esperaMs,
	Direccion* local = nullptr)
{
	std::string crudo;
	RespuestaHttp r;
	if (!pedirHttp(s.control.dir, armarPeticionSoap(s.control, s.tipo, nombre, cuerpoSoap), esperaMs, crudo, local) || !leerHttp(crudo, r))
		return 0;
	cuerpo = r.cuerpo;
	return r.estado;
}

// ---------------------------------------------------------------------------------------------- NAT-PMP
// peticion de direccion externa (2 bytes) y de mapeo UDP (12 bytes)
inline size_t armarPmpDireccion(uint8_t* b) { b[0] = 0; b[1] = 0; return 2; }
inline size_t armarPmpMapeo(uint8_t* b, uint16_t interno, uint16_t externo, uint32_t duracionS)
{
	b[0] = 0; b[1] = 1; b[2] = 0; b[3] = 0;
	b[4] = (uint8_t)(interno >> 8); b[5] = (uint8_t)interno; b[6] = (uint8_t)(externo >> 8); b[7] = (uint8_t)externo;
	b[8] = (uint8_t)(duracionS >> 24); b[9] = (uint8_t)(duracionS >> 16); b[10] = (uint8_t)(duracionS >> 8); b[11] = (uint8_t)duracionS;
	return 12;
}
inline bool leerPmpDireccion(const uint8_t* p, size_t n, Direccion& ip)
{
	if (n < 12 || p[0] != 0 || p[1] != 128 || p[2] != 0 || p[3] != 0) return false;
	ip = Direccion();
	memcpy(ip.ip, p + 8, 4);
	return true;
}
inline bool leerPmpMapeo(const uint8_t* p, size_t n, uint16_t interno, uint16_t& externo, uint32_t& duracionS)
{
	if (n < 16 || p[0] != 0 || p[1] != 129 || p[2] != 0 || p[3] != 0) return false;
	if (((p[8] << 8) | p[9]) != interno) return false;
	externo = (uint16_t)((p[10] << 8) | p[11]);
	duracionS = ((uint32_t)p[12] << 24) | ((uint32_t)p[13] << 16) | ((uint32_t)p[14] << 8) | p[15];
	return true;
}

// ---------------------------------------------------------------------------------------------- el mapeo
inline std::string soloIpUpnp(Direccion d) { d.puerto = 0; return d.texto(); }
// lo que quedo abierto en el router (para borrarlo al salir)
struct Mapeo {
	bool activo = false, pmp = false;
	Servicio servicio;        // UPnP
	Direccion pasarela;       // NAT-PMP
	uint16_t interno = 0, externo = 0;
	Direccion ipExterna;      // la del router (sin puerto)
};
inline Mapeo& mapeoActual() { static Mapeo m; return m; }

// SSDP: busca la pasarela; devuelve las LOCATION que respondieron (sin repetir), esperando a lo sumo esperaMs
inline std::vector<std::string> descubrir(int esperaMs)
{
	std::vector<std::string> r;
	SocketUdp s;
	if (!s.abrir(0, false)) return r;
	Direccion grupo;
	Direccion::desdeTexto("239.255.255.250:1900", grupo);
	const uint64_t fin = ahoraMs() + (uint64_t)esperaMs;
	uint64_t reenvio = 0;
	int envios = 0;
	uint8_t b[2048];
	while (ahoraMs() < fin) {
		if (envios < 2 && ahoraMs() >= reenvio) {   // dos veces: el multicast se pierde a veces
			for (const std::string& st : tiposBusqueda()) {
				const std::string m = armarBusqueda(st);
				s.enviar(grupo, (const uint8_t*)m.data(), m.size());
			}
			envios++;
			reenvio = ahoraMs() + 600;
		}
		Direccion de;
		const int n = s.recibir(de, b, sizeof b - 1, 50);
		if (n <= 0) { if (!r.empty() && ahoraMs() + 0 >= reenvio) break; continue; }
		std::string ubic;
		if (leerRespuestaSsdp(std::string((const char*)b, (size_t)n), ubic)) {
			bool repetida = false;
			for (const std::string& x : r) repetida |= x == ubic;
			if (!repetida) r.push_back(ubic);
		}
	}
	return r;
}

// la descripcion de la pasarela -> sus servicios WAN
inline std::vector<Servicio> servicios(const std::string& ubicacion, int esperaMs)
{
	Url u;
	if (!leerUrl(ubicacion, u)) return {};
	std::string crudo;
	RespuestaHttp r;
	const std::string pet = "GET " + u.ruta + " HTTP/1.1\r\nHOST: " + u.anfitrion() + "\r\nCONNECTION: close\r\nUSER-AGENT: YugiohClassicRemaster UPnP/1.1\r\n\r\n";
	if (!pedirHttp(u.dir, pet, esperaMs, crudo) || !leerHttp(crudo, r) || r.estado != 200) return {};
	return leerDescripcion(r.cuerpo, u);
}

// IP externa del router por el servicio (sin puerto); false si no responde o no esta conectado
inline bool ipExterna(const Servicio& s, Direccion& ip, int esperaMs, Direccion* local = nullptr)
{
	std::string cuerpo;
	if (accion(s, "GetExternalIPAddress", armarIpExterna(s.tipo), cuerpo, esperaMs, local) != 200) return false;
	const std::string t = etiqueta(cuerpo, "NewExternalIPAddress");
	return Direccion::desdeTexto(t, ip) && !ip.v6 && (ip.ip[0] | ip.ip[1] | ip.ip[2] | ip.ip[3]) != 0;
}

// abre el puerto UDP interno en el router (UPnP; si no hay, NAT-PMP). El externo: el mismo si esta libre, si no
// otros. Guarda el mapeo en mapeoActual() para liberar(). esperaMs: tiempo total aproximado
inline bool abrirPuerto(uint16_t interno, int esperaMs, Mapeo& m, const std::vector<std::string>* ubicaciones = nullptr)
{
	m = Mapeo();
	const uint64_t fin = ahoraMs() + (uint64_t)esperaMs;
	auto queda = [&]() { const uint64_t t = ahoraMs(); return t >= fin ? 0 : (int)(fin - t); };
	std::vector<std::string> ubic = ubicaciones ? *ubicaciones : descubrir(queda() < 2000 ? queda() : 2000);
	for (const std::string& u : ubic) {
		for (const Servicio& s : servicios(u, queda() < 2000 ? queda() : 2000)) {
			Direccion ip, local;
			if (queda() <= 0 || !ipExterna(s, ip, queda() < 2000 ? queda() : 2000, &local)) continue;
			int duracion = DURACION_MAPEO_S;
			for (int intento = 0; intento < 6 && queda() > 0; intento++) {
				const uint16_t externo = (uint16_t)(intento == 0 ? interno : 20000 + (interno * 7 + intento * 7919) % 40000);
				std::string cuerpo;
				const int est = accion(s, "AddPortMapping", armarAgregar(s.tipo, externo, interno, soloIpUpnp(local), duracion), cuerpo,
					queda() < 2000 ? queda() : 2000);
				if (est == 200) {
					m.activo = true; m.servicio = s; m.interno = interno; m.externo = externo; m.ipExterna = ip;
					mapeoActual() = m;
					return true;
				}
				const int e = codigoError(cuerpo);
				if (e == 725 && duracion != 0) { duracion = 0; intento--; continue; }   // solo permanentes: se borra al salir
				if (e == 724 && intento == 0) break;   // exige el mismo puerto y esta ocupado
				if (e != 718 && e != 724 && e != 0) break;   // 718 = ocupado: probar otro
				if (est == 0) break;
			}
		}
	}
	// NAT-PMP: el router por su LOCATION o, sin eso, el x.y.z.1 de cada IPv4 local
	std::vector<Direccion> pasarelas;
	for (const std::string& u : ubic) { Url x; if (leerUrl(u, x)) { x.dir.puerto = 5351; pasarelas.push_back(x.dir); } }
	if (pasarelas.empty())
		for (Direccion d : direccionesLocales())
			if (!d.v6) { d.ip[3] = 1; d.puerto = 5351; pasarelas.push_back(d); }
	SocketUdp s;
	if (pasarelas.empty() || queda() <= 0 || !s.abrir(0, false)) return false;
	uint8_t pet[12], b[64];
	for (const Direccion& g : pasarelas) {
		Direccion ip;
		bool hayIp = false;
		for (int intento = 0; intento < 2 && !hayIp && queda() > 0; intento++) {
			s.enviar(g, pet, armarPmpDireccion(pet));
			const uint64_t lim = ahoraMs() + 250u * (intento + 1);
			while (!hayIp && ahoraMs() < lim) {
				Direccion de;
				const int n = s.recibir(de, b, sizeof b, 50);
				hayIp = n > 0 && de == g && leerPmpDireccion(b, (size_t)n, ip);
			}
		}
		if (!hayIp) continue;
		for (int intento = 0; intento < 2 && queda() > 0; intento++) {
			s.enviar(g, pet, armarPmpMapeo(pet, interno, interno, DURACION_MAPEO_S));
			const uint64_t lim = ahoraMs() + 250u * (intento + 1);
			while (ahoraMs() < lim) {
				Direccion de;
				uint16_t ext = 0;
				uint32_t dur = 0;
				const int n = s.recibir(de, b, sizeof b, 50);
				if (n > 0 && de == g && leerPmpMapeo(b, (size_t)n, interno, ext, dur) && ext != 0) {
					m.activo = true; m.pmp = true; m.pasarela = g; m.interno = interno; m.externo = ext; m.ipExterna = ip;
					mapeoActual() = m;
					return true;
				}
			}
		}
	}
	return false;
}

// borra el mapeo abierto (al salir del modo en linea o del juego)
inline void liberar(int esperaMs = 1500)
{
	Mapeo& m = mapeoActual();
	if (!m.activo) return;
	if (m.pmp) {
		SocketUdp s;
		uint8_t pet[12];
		if (s.abrir(0, false)) for (int i = 0; i < 2; i++) s.enviar(m.pasarela, pet, armarPmpMapeo(pet, m.interno, 0, 0));
	} else {
		std::string cuerpo;
		accion(m.servicio, "DeletePortMapping", armarBorrar(m.servicio.tipo, m.externo), cuerpo, esperaMs);
	}
	m = Mapeo();
}

}  // namespace upnp
}  // namespace red
