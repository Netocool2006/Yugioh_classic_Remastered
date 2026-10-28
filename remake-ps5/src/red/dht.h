#pragma once
// Busqueda de rival SIN servidor por la DHT de BitTorrent (Mainline, BEP 5), rama classic-sala. Es el respaldo de la
// sala de Cloudflare (sala.h la arranca si la sala no responde).
//
//  - sha1(), bencode (Bval) y KRPC (ping, find_node, get_peers, announce_peer): un nodo DHT minimo que tambien
//    contesta a los demas (las pruebas arman una mini-red DHT en loopback con varios nodos en el proceso).
//  - La clave de la sala es SHA-1("yugioh-classic-remaster/<version del protocolo>"): todos los que buscan rival se
//    anuncian ahi (announce_peer con implied_port: queda la IP:puerto que ve internet) y piden los pares (get_peers).
//    OJO: el anuncio publica la IP:puerto en la DHT (es inherente a P2P; el juego lo avisa antes de buscar).
//  - A cada par encontrado se le escribe directo por el mismo socket UDP con un mensaje propio: "YGCR" + version +
//    tipo + texto clave=valor (sala_tipos.h). HOLA lleva el perfil y los candidatos de conexion; el emparejamiento
//    es determinista: propone el de id menor (que sera el anfitrion), el otro acepta si esta libre y el primero
//    confirma (PROPONGO -> ACEPTO -> CONFIRMO; RECHAZO si ya estaba ocupado). Version y catalogo tienen que coincidir.
//  - Nada bloquea mas de lo pedido: paso(esperaMs) atiende el socket y los relojes (lo llama el hilo de sala.h).
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>
#include "socket_udp.h"
#include "sala_tipos.h"

namespace red {
namespace dht {

// ---------------------------------------------------------------------------------------------- SHA-1
inline std::string sha1(const std::string& datos)
{
	uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
	std::string m = datos;
	const uint64_t bits = (uint64_t)datos.size() * 8;
	m += (char)0x80;
	while (m.size() % 64 != 56) m += (char)0;
	for (int i = 7; i >= 0; i--) m += (char)((bits >> (i * 8)) & 0xff);
	auto rot = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
	for (size_t b = 0; b < m.size(); b += 64) {
		uint32_t w[80];
		for (int i = 0; i < 16; i++)
			w[i] = ((uint32_t)(uint8_t)m[b + i * 4] << 24) | ((uint32_t)(uint8_t)m[b + i * 4 + 1] << 16) |
			       ((uint32_t)(uint8_t)m[b + i * 4 + 2] << 8) | (uint32_t)(uint8_t)m[b + i * 4 + 3];
		for (int i = 16; i < 80; i++) w[i] = rot(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
		uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4];
		for (int i = 0; i < 80; i++) {
			uint32_t f, k;
			if (i < 20) { f = (bb & c) | (~bb & d); k = 0x5A827999; }
			else if (i < 40) { f = bb ^ c ^ d; k = 0x6ED9EBA1; }
			else if (i < 60) { f = (bb & c) | (bb & d) | (c & d); k = 0x8F1BBCDC; }
			else { f = bb ^ c ^ d; k = 0xCA62C1D6; }
			const uint32_t t = rot(a, 5) + f + e + k + w[i];
			e = d; d = c; c = rot(bb, 30); bb = a; a = t;
		}
		h[0] += a; h[1] += bb; h[2] += c; h[3] += d; h[4] += e;
	}
	std::string r(20, '\0');
	for (int i = 0; i < 5; i++)
		for (int j = 0; j < 4; j++) r[i * 4 + j] = (char)((h[i] >> (24 - j * 8)) & 0xff);
	return r;
}

inline std::string aHex(const std::string& s)
{
	static const char* x = "0123456789abcdef";
	std::string r;
	for (unsigned char c : s) { r += x[c >> 4]; r += x[c & 15]; }
	return r;
}

// la clave de la sala en la DHT (info_hash): SHA-1("yugioh-classic-remaster/<version del protocolo>")
static const char* const VERSION_PROTOCOLO = "1";
inline std::string claveSala(const std::string& version = VERSION_PROTOCOLO)
{
	return sha1("yugioh-classic-remaster/" + version);
}

// ---------------------------------------------------------------------------------------------- bencode
struct Bval {
	enum Tipo { Ent, Cad, Lista, Dic } t = Cad;
	int64_t i = 0;
	std::string s;
	std::vector<Bval> l;
	std::vector<std::pair<std::string, Bval>> d;   // ordenado por clave al codificar

	static Bval ent(int64_t x) { Bval b; b.t = Ent; b.i = x; return b; }
	static Bval cad(const std::string& x) { Bval b; b.t = Cad; b.s = x; return b; }
	static Bval lista() { Bval b; b.t = Lista; return b; }
	static Bval dic() { Bval b; b.t = Dic; return b; }
	Bval& poner(const std::string& k, const Bval& v)
	{
		for (auto& p : d) if (p.first == k) { p.second = v; return *this; }
		d.push_back({ k, v });
		return *this;
	}
	const Bval* buscar(const std::string& k) const
	{
		if (t != Dic) return nullptr;
		for (const auto& p : d) if (p.first == k) return &p.second;
		return nullptr;
	}
	// la cadena de la clave k (vacia si no esta o no es cadena)
	std::string cadena(const std::string& k) const
	{
		const Bval* v = buscar(k);
		return v && v->t == Cad ? v->s : std::string();
	}

	void codificar(std::string& o) const
	{
		switch (t) {
		case Ent: o += "i" + std::to_string(i) + "e"; break;
		case Cad: o += std::to_string(s.size()) + ":" + s; break;
		case Lista:
			o += "l";
			for (const Bval& x : l) x.codificar(o);
			o += "e";
			break;
		case Dic: {
			std::vector<const std::pair<std::string, Bval>*> orden;
			for (const auto& p : d) orden.push_back(&p);
			std::sort(orden.begin(), orden.end(), [](auto* a, auto* b) { return a->first < b->first; });
			o += "d";
			for (auto* p : orden) {
				o += std::to_string(p->first.size()) + ":" + p->first;
				p->second.codificar(o);
			}
			o += "e";
			break;
		}
		}
	}
	std::string codificado() const { std::string o; codificar(o); return o; }

	// decodifica desde p[i]; false si esta mal formado (o demasiado hondo/grande)
	static bool decodificar(const std::string& p, size_t& i, Bval& out, int hondura = 0)
	{
		if (i >= p.size() || hondura > 24) return false;
		const char c = p[i];
		if (c == 'i') {
			const size_t fin = p.find('e', i);
			if (fin == std::string::npos || fin == i + 1 || fin - i > 21) return false;
			const std::string n = p.substr(i + 1, fin - i - 1);
			for (size_t k = 0; k < n.size(); k++) if (!(isdigit((unsigned char)n[k]) || (k == 0 && n[k] == '-'))) return false;
			out = ent(strtoll(n.c_str(), nullptr, 10));
			i = fin + 1;
			return true;
		}
		if (c >= '0' && c <= '9') {
			const size_t dp = p.find(':', i);
			if (dp == std::string::npos || dp - i > 7) return false;
			for (size_t k = i; k < dp; k++) if (!isdigit((unsigned char)p[k])) return false;
			const size_t n = (size_t)strtoul(p.substr(i, dp - i).c_str(), nullptr, 10);
			if (dp + 1 + n > p.size()) return false;
			out = cad(p.substr(dp + 1, n));
			i = dp + 1 + n;
			return true;
		}
		if (c == 'l') {
			out = lista();
			i++;
			while (i < p.size() && p[i] != 'e') {
				Bval x;
				if (out.l.size() > 1000 || !decodificar(p, i, x, hondura + 1)) return false;
				out.l.push_back(std::move(x));
			}
			if (i >= p.size()) return false;
			i++;
			return true;
		}
		if (c == 'd') {
			out = dic();
			i++;
			while (i < p.size() && p[i] != 'e') {
				Bval k, v;
				if (out.d.size() > 1000 || !decodificar(p, i, k, hondura + 1) || k.t != Cad) return false;
				if (!decodificar(p, i, v, hondura + 1)) return false;
				out.d.push_back({ k.s, std::move(v) });
			}
			if (i >= p.size()) return false;
			i++;
			return true;
		}
		return false;
	}
	static bool decodificar(const std::string& p, Bval& out)
	{
		size_t i = 0;
		return decodificar(p, i, out) && i == p.size();
	}
};

// ---------------------------------------------------------------------------------------------- nodos y direcciones
// "compact node info": 20 bytes de id + 4 de IPv4 + 2 de puerto; "compact peer": 4 + 2
inline std::string direccionCompacta(const Direccion& d)
{
	std::string r(6, '\0');
	memcpy(&r[0], d.ip, 4);
	r[4] = (char)(d.puerto >> 8);
	r[5] = (char)(d.puerto & 0xff);
	return r;
}
inline bool direccionDeCompacta(const std::string& s, size_t ini, Direccion& d)
{
	if (ini + 6 > s.size()) return false;
	d = Direccion();
	memcpy(d.ip, s.data() + ini, 4);
	d.puerto = (uint16_t)(((uint8_t)s[ini + 4] << 8) | (uint8_t)s[ini + 5]);
	return d.puerto != 0;
}

// distancia XOR: a < b (mas cerca del objetivo)
inline bool masCerca(const std::string& objetivo, const std::string& a, const std::string& b)
{
	for (size_t i = 0; i < 20 && i < a.size() && i < b.size() && i < objetivo.size(); i++) {
		const uint8_t x = (uint8_t)(a[i] ^ objetivo[i]), y = (uint8_t)(b[i] ^ objetivo[i]);
		if (x != y) return x < y;
	}
	return false;
}

struct NodoConocido {
	std::string id;   // 20 bytes
	Direccion dir;
	uint64_t visto = 0;
};

// ---------------------------------------------------------------------------------------------- el nodo DHT
// Un nodo DHT minimo: tabla de nodos (lista simple, no k-buckets completos), contesta ping/find_node/get_peers/
// announce_peer, y hace busquedas iterativas get_peers (con announce_peer al final). Los paquetes que no son KRPC
// (no empiezan con 'd') se le pasan a 'otroMensaje' (los mensajes propios del juego).
class Nodo {
 public:
	struct Opciones {
		size_t maxTabla = 300;
		int esperaRespuestaMs = 2000;
		int duracionBusquedaMs = 15000;
		int alfa = 4;      // consultas a la vez en una busqueda
		int k = 8;         // los mas cercanos que tienen que responder
	};
	Opciones op;
	std::function<void(const Direccion&, const std::string&)> otroMensaje;

	Nodo()
	{
		std::random_device rd;
		azar_.seed(((uint64_t)rd() << 32) ^ rd() ^ ahoraMs());
		id_ = bytesAzar(20);
		secreto_ = bytesAzar(16);
	}
	bool abrir(uint16_t puerto = 0) { return sock_.abrir(puerto, false); }
	void cerrar() { sock_.cerrar(); }
	bool abierto() const { return sock_.abierto(); }
	uint16_t puerto() const { return sock_.puertoLocal(); }
	const std::string& id() const { return id_; }
	SocketUdp& socket() { return sock_; }
	size_t nodosConocidos() const { return tabla_.size(); }

	// manda find_node(propio id) a los nodos de arranque (direcciones ya resueltas)
	void arrancar(const std::vector<Direccion>& arranque)
	{
		for (const Direccion& d : arranque) {
			Bval a = Bval::dic();
			a.poner("target", Bval::cad(id_));
			consultar(d, "find_node", a, Pendiente::Arranque, -1);
		}
	}
	// find_node(propio id) a los nodos de la tabla (para llenarla)
	void refrescar()
	{
		for (size_t i = 0; i < tabla_.size() && i < 8; i++) {
			Bval a = Bval::dic();
			a.poner("target", Bval::cad(id_));
			consultar(tabla_[i].dir, "find_node", a, Pendiente::Arranque, -1);
		}
	}

	// empieza una busqueda get_peers de 'clave' (y announce_peer al final si anunciar)
	void buscarPares(const std::string& clave, bool anunciar)
	{
		busq_ = Busqueda();
		busq_.activa = true;
		busq_.objetivo = clave;
		busq_.anunciar = anunciar;
		busq_.inicio = ahoraMs();
		for (const NodoConocido& n : tabla_) agregarCandidato(n.id, n.dir);
	}
	bool buscando() const { return busq_.activa; }
	int busquedasTerminadas() const { return busquedasHechas_; }
	int anunciosHechos() const { return anuncios_; }

	// los pares encontrados (de todas las busquedas)
	const std::vector<Direccion>& pares() const { return pares_; }

	void mandarCrudo(const Direccion& d, const std::string& datos) { sock_.enviar(d, (const uint8_t*)datos.data(), datos.size()); }

	// atiende el socket hasta esperaMs (el primer paquete espera; los demas se leen sin esperar) y los relojes
	void paso(int esperaMs)
	{
		uint8_t buf[2048];
		Direccion de;
		int espera = esperaMs;
		for (int vueltas = 0; vueltas < 200; vueltas++) {
			const int n = sock_.recibir(de, buf, sizeof buf, espera);
			if (n <= 0) break;
			espera = 0;
			const std::string p((const char*)buf, (size_t)n);
			if (p[0] == 'd') recibirKrpc(de, p);
			else if (otroMensaje) otroMensaje(de, p);
		}
		relojes();
	}

	// (pruebas) agrega un nodo a la tabla a mano
	void conocer(const std::string& id, const Direccion& d) { agregarNodo(id, d); }

 private:
	struct Pendiente {
		enum Tipo { Arranque, Busqueda, Anuncio, Otra } tipo = Otra;
		int indice = -1;   // candidato de la busqueda
		uint64_t enviado = 0;
		Direccion dir;
	};
	struct Candidato {
		std::string id;
		Direccion dir;
		int estado = 0;   // 0 nuevo, 1 consultado, 2 respondio, 3 fallo
		std::string token;
	};
	struct Busqueda {
		bool activa = false, anunciar = false;
		std::string objetivo;
		std::vector<Candidato> cands;
		uint64_t inicio = 0;
	};

	std::string bytesAzar(size_t n)
	{
		std::string s(n, '\0');
		for (auto& c : s) c = (char)(azar_() & 0xff);
		return s;
	}
	std::string token(const Direccion& d) const { return sha1(secreto_ + std::string((const char*)d.ip, 4)).substr(0, 8); }

	void agregarNodo(const std::string& id, const Direccion& d)
	{
		if (id.size() != 20 || d.v6 || d.puerto == 0 || id == id_) return;
		for (NodoConocido& n : tabla_) {
			if (n.id == id || n.dir == d) { n.id = id; n.dir = d; n.visto = ahoraMs(); return; }
		}
		if (tabla_.size() >= op.maxTabla) {
			auto viejo = std::min_element(tabla_.begin(), tabla_.end(), [](const NodoConocido& a, const NodoConocido& b) { return a.visto < b.visto; });
			*viejo = { id, d, ahoraMs() };
			return;
		}
		tabla_.push_back({ id, d, ahoraMs() });
	}
	void agregarCandidato(const std::string& id, const Direccion& d)
	{
		if (!busq_.activa || id.size() != 20 || id == id_ || busq_.cands.size() >= 400) return;
		for (const Candidato& c : busq_.cands) if (c.id == id || c.dir == d) return;
		Candidato c;
		c.id = id;
		c.dir = d;
		busq_.cands.push_back(c);
	}
	void agregarPar(const Direccion& d)
	{
		if (d.puerto == 0) return;
		for (const Direccion& x : pares_) if (x == d) return;
		if (pares_.size() < 500) pares_.push_back(d);
	}

	uint16_t consultar(const Direccion& d, const char* q, Bval a, Pendiente::Tipo tipo, int indice)
	{
		uint16_t tid = (uint16_t)(azar_() & 0xffff);
		while (pend_.count(tid)) tid++;
		a.poner("id", Bval::cad(id_));
		Bval m = Bval::dic();
		m.poner("t", Bval::cad(std::string{ (char)(tid >> 8), (char)(tid & 0xff) }));
		m.poner("y", Bval::cad("q"));
		m.poner("q", Bval::cad(q));
		m.poner("a", a);
		m.poner("v", Bval::cad("YC01"));
		Pendiente p;
		p.tipo = tipo;
		p.indice = indice;
		p.enviado = ahoraMs();
		p.dir = d;
		pend_[tid] = p;
		mandarCrudo(d, m.codificado());
		return tid;
	}
	void responder(const Direccion& d, const std::string& t, const Bval& r)
	{
		Bval m = Bval::dic();
		m.poner("t", Bval::cad(t));
		m.poner("y", Bval::cad("r"));
		m.poner("r", r);
		mandarCrudo(d, m.codificado());
	}
	void error(const Direccion& d, const std::string& t, int codigo, const char* texto)
	{
		Bval e = Bval::lista();
		e.l.push_back(Bval::ent(codigo));
		e.l.push_back(Bval::cad(texto));
		Bval m = Bval::dic();
		m.poner("t", Bval::cad(t));
		m.poner("y", Bval::cad("e"));
		m.poner("e", e);
		mandarCrudo(d, m.codificado());
	}
	std::string nodosCercanos(const std::string& objetivo) const
	{
		std::vector<const NodoConocido*> v;
		for (const NodoConocido& n : tabla_) v.push_back(&n);
		std::sort(v.begin(), v.end(), [&](auto* a, auto* b) { return masCerca(objetivo, a->id, b->id); });
		std::string r;
		for (size_t i = 0; i < v.size() && i < 8; i++) r += v[i]->id + direccionCompacta(v[i]->dir);
		return r;
	}
	void leerNodos(const std::string& nodos)
	{
		for (size_t i = 0; i + 26 <= nodos.size(); i += 26) {
			Direccion d;
			if (!direccionDeCompacta(nodos, i + 20, d)) continue;
			const std::string id = nodos.substr(i, 20);
			agregarCandidato(id, d);
			if (tabla_.size() < 16) agregarNodo(id, d);   // la tabla se llena sobre todo con los que responden
		}
	}

	void recibirKrpc(const Direccion& de, const std::string& p)
	{
		Bval m;
		if (de.v6 || !Bval::decodificar(p, m) || m.t != Bval::Dic) return;
		const std::string t = m.cadena("t"), y = m.cadena("y");
		if (y == "q") {
			const Bval* a = m.buscar("a");
			if (!a || a->t != Bval::Dic) return error(de, t, 203, "Protocol Error");
			const std::string suId = a->cadena("id");
			if (suId.size() != 20) return error(de, t, 203, "Protocol Error");
			agregarNodo(suId, de);
			const std::string q = m.cadena("q");
			Bval r = Bval::dic();
			r.poner("id", Bval::cad(id_));
			if (q == "ping") return responder(de, t, r);
			if (q == "find_node") {
				r.poner("nodes", Bval::cad(nodosCercanos(a->cadena("target"))));
				return responder(de, t, r);
			}
			if (q == "get_peers") {
				const std::string ih = a->cadena("info_hash");
				if (ih.size() != 20) return error(de, t, 203, "Protocol Error");
				r.poner("token", Bval::cad(token(de)));
				auto it = guardados_.find(ih);
				if (it != guardados_.end() && !it->second.empty()) {
					Bval v = Bval::lista();
					for (const auto& g : it->second) v.l.push_back(Bval::cad(direccionCompacta(g.first)));
					r.poner("values", v);
				} else r.poner("nodes", Bval::cad(nodosCercanos(ih)));
				return responder(de, t, r);
			}
			if (q == "announce_peer") {
				const std::string ih = a->cadena("info_hash");
				if (ih.size() != 20 || a->cadena("token") != token(de)) return error(de, t, 203, "Bad token");
				Direccion par = de;
				const Bval* implicito = a->buscar("implied_port");
				const Bval* puerto = a->buscar("port");
				if (!(implicito && implicito->t == Bval::Ent && implicito->i == 1) && puerto && puerto->t == Bval::Ent)
					par.puerto = (uint16_t)puerto->i;
				auto& lista = guardados_[ih];
				bool esta = false;
				for (auto& g : lista) if (g.first == par) { g.second = ahoraMs(); esta = true; }
				if (!esta && lista.size() < 200) lista.push_back({ par, ahoraMs() });
				return responder(de, t, r);
			}
			return error(de, t, 204, "Method Unknown");
		}
		if (y != "r" && y != "e") return;
		if (t.size() != 2) return;
		const uint16_t tid = (uint16_t)(((uint8_t)t[0] << 8) | (uint8_t)t[1]);
		auto it = pend_.find(tid);
		if (it == pend_.end() || !(it->second.dir == de)) return;
		const Pendiente pe = it->second;
		pend_.erase(it);
		if (y == "e") {
			if (pe.tipo == Pendiente::Busqueda) marcar(pe.indice, 3);
			return;
		}
		const Bval* r = m.buscar("r");
		if (!r || r->t != Bval::Dic) return;
		const std::string suId = r->cadena("id");
		if (suId.size() != 20) return;
		agregarNodo(suId, de);
		const std::string nodos = r->cadena("nodes");
		if (pe.tipo == Pendiente::Arranque) {
			for (size_t i = 0; i + 26 <= nodos.size(); i += 26) {
				Direccion d;
				if (direccionDeCompacta(nodos, i + 20, d)) agregarNodo(nodos.substr(i, 20), d);
			}
			return;
		}
		if (pe.tipo == Pendiente::Anuncio) { anuncios_++; return; }
		if (pe.tipo != Pendiente::Busqueda || !busq_.activa) return;
		if (pe.indice >= 0 && pe.indice < (int)busq_.cands.size()) {
			Candidato& c = busq_.cands[pe.indice];
			c.estado = 2;
			c.token = r->cadena("token");
		}
		leerNodos(nodos);
		const Bval* v = r->buscar("values");
		if (v && v->t == Bval::Lista)
			for (const Bval& x : v->l) {
				Direccion d;
				if (x.t == Bval::Cad && x.s.size() == 6 && direccionDeCompacta(x.s, 0, d)) agregarPar(d);
			}
	}
	void marcar(int indice, int estado)
	{
		if (busq_.activa && indice >= 0 && indice < (int)busq_.cands.size()) busq_.cands[indice].estado = estado;
	}

	void relojes()
	{
		const uint64_t t = ahoraMs();
		for (auto it = pend_.begin(); it != pend_.end();) {
			if (t - it->second.enviado > (uint64_t)op.esperaRespuestaMs) {
				if (it->second.tipo == Pendiente::Busqueda) marcar(it->second.indice, 3);
				it = pend_.erase(it);
			} else ++it;
		}
		if (!busq_.activa) return;
		// los k mas cercanos que no fallaron: si todos respondieron, termina; si no, consulta (alfa a la vez)
		std::vector<int> orden;
		for (int i = 0; i < (int)busq_.cands.size(); i++) if (busq_.cands[i].estado != 3) orden.push_back(i);
		std::sort(orden.begin(), orden.end(), [&](int a, int b) { return masCerca(busq_.objetivo, busq_.cands[a].id, busq_.cands[b].id); });
		int enVuelo = 0, respondieron = 0, mirados = 0;
		for (int i : orden) if (busq_.cands[i].estado == 1) enVuelo++;
		for (int i : orden) {
			if (mirados >= op.k) break;
			mirados++;
			Candidato& c = busq_.cands[i];
			if (c.estado == 2) { respondieron++; continue; }
			if (c.estado == 0 && enVuelo < op.alfa) {
				Bval a = Bval::dic();
				a.poner("info_hash", Bval::cad(busq_.objetivo));
				c.estado = 1;
				consultar(c.dir, "get_peers", a, Pendiente::Busqueda, i);
				enVuelo++;
			}
		}
		const bool completa = mirados > 0 && respondieron == mirados;
		const bool vencida = t - busq_.inicio > (uint64_t)op.duracionBusquedaMs || (mirados == 0 && t - busq_.inicio > 3000);
		if (completa || vencida) {
			if (busq_.anunciar) {
				int n = 0;
				for (int i : orden) {
					const Candidato& c = busq_.cands[i];
					if (c.estado != 2 || c.token.empty()) continue;
					Bval a = Bval::dic();
					a.poner("info_hash", Bval::cad(busq_.objetivo));
					a.poner("implied_port", Bval::ent(1));
					a.poner("port", Bval::ent(sock_.puertoLocal()));
					a.poner("token", Bval::cad(c.token));
					consultar(c.dir, "announce_peer", a, Pendiente::Anuncio, -1);
					if (++n >= op.k) break;
				}
			}
			busq_.activa = false;
			busquedasHechas_++;
		}
	}

	SocketUdp sock_;
	std::mt19937_64 azar_;
	std::string id_, secreto_;
	std::vector<NodoConocido> tabla_;
	std::map<uint16_t, Pendiente> pend_;
	std::map<std::string, std::vector<std::pair<Direccion, uint64_t>>> guardados_;
	std::vector<Direccion> pares_;
	Busqueda busq_;
	int busquedasHechas_ = 0, anuncios_ = 0;
};

// ---------------------------------------------------------------------------------------------- nodos de arranque
struct Arranque {
	std::string nombre;
	uint16_t puerto;
};
inline std::vector<Arranque> arranquePublico()
{
	return { { "router.bittorrent.com", 6881 }, { "dht.transmissionbt.com", 6881 }, { "router.utorrent.com", 6881 } };
}

// ---------------------------------------------------------------------------------------------- emparejador
// Busca rival por la DHT: anuncia y busca la clave de la sala cada 'cadaBusquedaMs', le escribe HOLA a cada par y
// empareja con PROPONGO/ACEPTO/CONFIRMO (ver arriba). Todo en paso(): lo llama un hilo (sala.h) o la prueba.
static const char MARCA[4] = { 'Y', 'G', 'C', 'R' };
static const uint8_t VERSION_MENSAJE = 1;
enum TipoMensaje : uint8_t { HOLA = 1, PROPONGO = 2, ACEPTO = 3, CONFIRMO = 4, RECHAZO = 5 };

inline std::string armarMensaje(TipoMensaje tipo, const Campos& c)
{
	std::string s(MARCA, 4);
	s += (char)VERSION_MENSAJE;
	s += (char)tipo;
	s += c.texto();
	return s;
}
inline bool leerMensaje(const std::string& p, TipoMensaje& tipo, Campos& c)
{
	if (p.size() < 6 || memcmp(p.data(), MARCA, 4) != 0 || (uint8_t)p[4] != VERSION_MENSAJE) return false;
	tipo = (TipoMensaje)(uint8_t)p[5];
	if (tipo < HOLA || tipo > RECHAZO) return false;
	c = Campos::leer(p.substr(6));
	return true;
}

// ventana de ELO (la misma regla que la sala): 100 al empezar, +25 por segundo, a los 30 s cualquiera
inline bool eloEnVentana(int a, int b, uint64_t esperaMs)
{
	if (esperaMs >= 30000) return true;
	return std::abs(a - b) <= 100 + 25 * (int)(esperaMs / 1000);
}

class Emparejador {
 public:
	enum Fase { Libre, Propuse, Reservado, Emparejado };
	struct Opciones {
		int cadaBusquedaMs = 20000;
		int cadaHolaMs = 1000;
		int esperaPropuestaMs = 3000;
		int esperaReservaMs = 4000;
		int vidaRemotoMs = 15000;   // sin noticias de un par en este tiempo, deja de contar
	};
	Opciones op;
	Nodo nodo;

	Emparejador()
	{
		nodo.otroMensaje = [this](const Direccion& de, const std::string& p) { recibir(de, p); };
	}

	// abre el socket (puerto 0 = cualquiera) y arranca; 'arranque' ya resuelto (sala.h resuelve los nombres)
	bool iniciar(const Perfil& yo, const std::vector<Candidato>& mios, const std::vector<Direccion>& arranque, uint16_t puerto = 0,
		const std::string& clave = claveSala())
	{
		yo_ = yo;
		mios_ = mios;
		clave_ = clave;
		if (!nodo.abierto() && !nodo.abrir(puerto)) return false;
		arranque_ = arranque;
		nodo.arrancar(arranque);
		inicio_ = ahoraMs();
		proximaBusqueda_ = inicio_ + 1500;   // primero se llena la tabla con el arranque
		fase_ = Libre;
		return true;
	}
	void cerrar() { nodo.cerrar(); }

	void paso(int esperaMs)
	{
		nodo.paso(esperaMs);
		const uint64_t t = ahoraMs();
		if (!nodo.buscando() && t >= proximaBusqueda_) {
			if (nodo.nodosConocidos() < 8) { nodo.arrancar(arranque_); nodo.refrescar(); }
			nodo.buscarPares(clave_, fase_ != Emparejado);
			proximaBusqueda_ = t + (uint64_t)op.cadaBusquedaMs;
		}
		// HOLA a los pares encontrados que todavia no contestaron (y a los conocidos, para seguir vivos)
		if (t >= proximoHola_) {
			proximoHola_ = t + (uint64_t)op.cadaHolaMs;
			for (const Direccion& d : nodo.pares()) {
				Remoto* r = remotoPorDir(d);
				if (r && r->conocido && t - r->ultimoHola < 3000) continue;
				mandar(d, HOLA, campoPerfil());
				if (!r) remotos_.push_back(nuevoRemoto(d));
				if ((r = remotoPorDir(d))) r->ultimoHola = t;
			}
		}
		maquina(t);
	}

	Fase fase() const { return fase_; }
	bool pareja(Pareja& p) const
	{
		if (fase_ != Emparejado) return false;
		p = pareja_;
		return true;
	}
	// pares del juego vistos hace poco (para el contador "en linea" sin sala)
	int paresVivos() const
	{
		int n = 0;
		for (const Remoto& r : remotos_) if (r.conocido && ahoraMs() - r.visto < (uint64_t)op.vidaRemotoMs) n++;
		return n;
	}
	int paresBuscando() const
	{
		int n = 0;
		for (const Remoto& r : remotos_) if (r.conocido && r.libre && ahoraMs() - r.visto < (uint64_t)op.vidaRemotoMs) n++;
		return n;
	}

 private:
	struct Remoto {
		Direccion dir;
		bool conocido = false, libre = false;
		Perfil perfil;
		std::vector<Candidato> cand;
		uint64_t visto = 0, ultimoHola = 0, ocupadoHasta = 0;
	};

	static Remoto nuevoRemoto(const Direccion& d)
	{
		Remoto r;
		r.dir = d;
		return r;
	}
	Remoto* remotoPorDir(const Direccion& d)
	{
		for (Remoto& r : remotos_) if (r.dir == d) return &r;
		return nullptr;
	}
	Remoto* remotoPorId(const std::string& id)
	{
		for (Remoto& r : remotos_) if (r.conocido && r.perfil.id == id) return &r;
		return nullptr;
	}
	Campos campoPerfil() const
	{
		Campos c;
		perfilACampos(yo_, mios_, c);
		c.poner("libre", fase_ == Emparejado ? 0LL : 1LL);
		return c;
	}
	void mandar(const Direccion& d, TipoMensaje tipo, const Campos& c) { nodo.mandarCrudo(d, armarMensaje(tipo, c)); }
	void mandarCorto(const Direccion& d, TipoMensaje tipo, uint64_t sesion)
	{
		Campos c;
		c.poner("id", yo_.id);
		c.poner("sesion", std::to_string(sesion));
		mandar(d, tipo, c);
	}
	bool compatible(const Remoto& r) const
	{
		return r.perfil.version == yo_.version && r.perfil.catalogo == yo_.catalogo && r.perfil.id != yo_.id;
	}
	uint64_t sesionDe(const std::string& a, const std::string& b) const
	{
		const std::string h = sha1(a + "|" + b + "|" + std::to_string(inicio_));
		uint64_t v = 0;
		for (int i = 0; i < 8; i++) v = (v << 8) | (uint8_t)h[i];
		return v ? v : 1;
	}
	void emparejar(Remoto& r, uint64_t sesion)
	{
		fase_ = Emparejado;
		pareja_ = Pareja();
		pareja_.rival = r.perfil;
		pareja_.candidatos = r.cand;
		pareja_.idSesion = sesion;
		pareja_.soyAnfitrion = esAnfitrion(yo_.id, r.perfil.id);
		pareja_.porDht = true;
		// si el rival no mando candidatos, al menos la direccion por la que nos hablamos
		if (pareja_.candidatos.empty()) {
			Candidato c;
			c.dir = r.dir;
			c.tipo = TipoCandidato::Stun;
			pareja_.candidatos.push_back(c);
		}
	}

	void recibir(const Direccion& de, const std::string& p)
	{
		TipoMensaje tipo;
		Campos c;
		if (!leerMensaje(p, tipo, c)) return;
		const std::string id = c.obtener("id");
		if (id.empty() || id == yo_.id) return;
		const uint64_t t = ahoraMs();
		Remoto* r = remotoPorDir(de);
		if (!r) { remotos_.push_back(nuevoRemoto(de)); r = &remotos_.back(); }
		r->visto = t;
		if (tipo == HOLA) {
			const bool nuevo = !r->conocido;
			r->conocido = true;
			r->libre = c.numero("libre", 1) != 0;
			r->perfil.id = id;
			r->perfil.nombre = c.obtener("nombre");
			r->perfil.elo = (int)c.numero("elo", 1000);
			r->perfil.version = c.obtener("version");
			r->perfil.catalogo = c.obtener("catalogo");
			r->perfil.plataforma = c.obtener("plataforma");
			r->cand = candidatosDeCampos(c);
			if (nuevo || t - r->ultimoHola > 1000) {   // contesta (los dos se enteran aunque solo uno encontro al otro)
				mandar(de, HOLA, campoPerfil());
				r->ultimoHola = t;
			}
			return;
		}
		if (!r->conocido || r->perfil.id != id) return;   // primero el HOLA
		const uint64_t sesion = c.numeroSinSigno("sesion");
		if (tipo == PROPONGO) {
			if (!(id < yo_.id) || !compatible(*r)) return mandarCorto(de, RECHAZO, sesion);
			if (fase_ == Emparejado && pareja_.rival.id == id && pareja_.idSesion == sesion) return mandarCorto(de, ACEPTO, sesion);
			if (fase_ == Libre || (fase_ == Reservado && conQuien_ == id)) {
				fase_ = Reservado;
				conQuien_ = id;
				sesion_ = sesion;
				desde_ = t;
				ultimoEnvio_ = t;
				return mandarCorto(de, ACEPTO, sesion);
			}
			return mandarCorto(de, RECHAZO, sesion);
		}
		if (tipo == ACEPTO) {
			if (fase_ == Propuse && conQuien_ == id && sesion_ == sesion) {
				emparejar(*r, sesion);
				for (int i = 0; i < 3; i++) mandarCorto(de, CONFIRMO, sesion);
				return;
			}
			if (fase_ == Emparejado && pareja_.rival.id == id && pareja_.idSesion == sesion) return mandarCorto(de, CONFIRMO, sesion);
			return mandarCorto(de, RECHAZO, sesion);
		}
		if (tipo == CONFIRMO) {
			if (fase_ == Reservado && conQuien_ == id && sesion_ == sesion) emparejar(*r, sesion);
			return;
		}
		if (tipo == RECHAZO) {
			r->ocupadoHasta = t + 5000;
			if ((fase_ == Propuse || fase_ == Reservado) && conQuien_ == id && sesion_ == sesion) fase_ = Libre;
		}
	}

	void maquina(uint64_t t)
	{
		if (fase_ == Propuse) {
			Remoto* r = remotoPorId(conQuien_);
			if (!r || t - desde_ > (uint64_t)op.esperaPropuestaMs) {
				if (r) r->ocupadoHasta = t + 3000;
				fase_ = Libre;
			} else if (t - ultimoEnvio_ > 500) {
				mandarCorto(r->dir, PROPONGO, sesion_);
				ultimoEnvio_ = t;
			}
			return;
		}
		if (fase_ == Reservado) {
			Remoto* r = remotoPorId(conQuien_);
			if (!r || t - desde_ > (uint64_t)op.esperaReservaMs) fase_ = Libre;
			else if (t - ultimoEnvio_ > 500) {
				mandarCorto(r->dir, ACEPTO, sesion_);
				ultimoEnvio_ = t;
			}
			return;
		}
		if (fase_ != Libre) return;
		// propone el de id menor: al compatible libre mas cercano en ELO (ventana que se ensancha; con pocos, el primero)
		std::vector<Remoto*> compat;
		for (Remoto& r : remotos_)
			if (r.conocido && r.libre && compatible(r) && t - r.visto < (uint64_t)op.vidaRemotoMs) compat.push_back(&r);
		const bool pocos = compat.size() <= 2;
		Remoto* mejor = nullptr;
		for (Remoto* r : compat) {
			if (!(yo_.id < r->perfil.id) || r->ocupadoHasta > t) continue;
			if (!pocos && !eloEnVentana(yo_.elo, r->perfil.elo, t - inicio_)) continue;
			if (!mejor || std::abs(r->perfil.elo - yo_.elo) < std::abs(mejor->perfil.elo - yo_.elo)) mejor = r;
		}
		if (!mejor) return;
		fase_ = Propuse;
		conQuien_ = mejor->perfil.id;
		sesion_ = sesionDe(yo_.id, mejor->perfil.id);
		desde_ = t;
		ultimoEnvio_ = t;
		mandarCorto(mejor->dir, PROPONGO, sesion_);
	}

	Perfil yo_;
	std::vector<Candidato> mios_;
	std::string clave_;
	std::vector<Direccion> arranque_;
	std::vector<Remoto> remotos_;
	Fase fase_ = Libre;
	std::string conQuien_;
	uint64_t sesion_ = 0, desde_ = 0, ultimoEnvio_ = 0, inicio_ = 0, proximaBusqueda_ = 0, proximoHola_ = 0;
	Pareja pareja_;
};

}  // namespace dht
}  // namespace red
