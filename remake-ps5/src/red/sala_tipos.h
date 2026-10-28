#pragma once
// Tipos comunes de la busqueda de rival en linea (rama classic-sala): el perfil que se publica, la pareja encontrada y
// el texto "clave=valor" que usan la sala de Cloudflare (servidor/sala) y los mensajes propios por la DHT (dht.h).
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <random>
#include "socket_udp.h"
#include "nat.h"        // Candidato/TipoCandidato (rama classic-red-nat)

namespace red {

// Lo que se publica al buscar: id anonimo aleatorio (hex, generado en la consola: elo.h), nombre de duelista, ELO,
// version del juego (obligatoria: con otra version no hay pareja) y plataforma ("ps5" o "pc": se juega entre las dos).
// catalogo: hash del catalogo de cartas (tambien tiene que coincidir; vacio = no se compara)
struct Perfil {
	std::string id, nombre;
	int elo = 1000;
	std::string version;
	std::string plataforma;
	std::string catalogo;
};

// La pareja encontrada: el rival, sus candidatos de conexion, la sesion (la misma en los dos lados) y quien es
// anfitrion (el de id menor, en la sala y en la DHT)
struct Pareja {
	Perfil rival;
	std::vector<Candidato> candidatos;
	uint64_t idSesion = 0;
	bool soyAnfitrion = false;
	bool porDht = false;   // (agregado) la encontro la DHT (sin sala: el resultado no se reporta a la sala)
};

// ---------------------------------------------------------------------------------------------- texto clave=valor
struct Campos {
	std::vector<std::pair<std::string, std::string>> v;

	void poner(const std::string& k, const std::string& x)
	{
		std::string limpio;
		for (char c : x) limpio += (c == '\n' || c == '\r' || c == '\0') ? ' ' : c;
		v.push_back({ k, limpio });
	}
	void poner(const std::string& k, long long x) { poner(k, std::to_string(x)); }
	std::string obtener(const std::string& k, const std::string& def = "") const
	{
		for (const auto& p : v) if (p.first == k) return p.second;
		return def;
	}
	long long numero(const std::string& k, long long def = 0) const
	{
		const std::string s = obtener(k);
		if (s.empty()) return def;
		char* fin = nullptr;
		const long long n = strtoll(s.c_str(), &fin, 10);
		return (fin && *fin == 0) ? n : def;
	}
	uint64_t numeroSinSigno(const std::string& k) const { return strtoull(obtener(k).c_str(), nullptr, 10); }
	std::vector<std::string> todos(const std::string& k) const
	{
		std::vector<std::string> r;
		for (const auto& p : v) if (p.first == k) r.push_back(p.second);
		return r;
	}
	bool tiene(const std::string& k) const
	{
		for (const auto& p : v) if (p.first == k) return true;
		return false;
	}
	std::string texto() const
	{
		std::string s;
		for (const auto& p : v) s += p.first + "=" + p.second + "\n";
		return s;
	}
	static Campos leer(const std::string& s)
	{
		Campos c;
		size_t i = 0;
		while (i < s.size()) {
			size_t fin = s.find('\n', i);
			if (fin == std::string::npos) fin = s.size();
			std::string l = s.substr(i, fin - i);
			if (!l.empty() && l.back() == '\r') l.pop_back();
			const size_t eq = l.find('=');
			if (eq != std::string::npos && eq > 0) c.v.push_back({ l.substr(0, eq), l.substr(eq + 1) });
			i = fin + 1;
		}
		return c;
	}
};

// ---------------------------------------------------------------------------------------------- candidatos en texto
inline const char* claveTipo(TipoCandidato t)   // (en el protocolo de la sala; nat.h nombreTipo es para mostrar)
{
	switch (t) {
	case TipoCandidato::Local: return "local";
	case TipoCandidato::Ipv6: return "ipv6";
	case TipoCandidato::Upnp: return "upnp";
	case TipoCandidato::Stun: return "stun";
	default: return "manual";
	}
}
// "<tipo>,<prioridad>,<direccion>" (lo que entiende la sala)
inline std::string candidatoATexto(const Candidato& c)
{
	return std::string(claveTipo(c.tipo)) + "," + std::to_string(c.prioridad) + "," + c.dir.texto();
}
inline bool candidatoDeTexto(const std::string& s, Candidato& c)
{
	const size_t a = s.find(','), b = a == std::string::npos ? a : s.find(',', a + 1);
	if (b == std::string::npos) return false;
	const std::string tipo = s.substr(0, a);
	if (tipo == "local") c.tipo = TipoCandidato::Local;
	else if (tipo == "ipv6") c.tipo = TipoCandidato::Ipv6;
	else if (tipo == "upnp") c.tipo = TipoCandidato::Upnp;
	else if (tipo == "stun") c.tipo = TipoCandidato::Stun;
	else if (tipo == "manual") c.tipo = TipoCandidato::Manual;
	else return false;
	c.prioridad = atoi(s.substr(a + 1, b - a - 1).c_str());
	return Direccion::desdeTexto(s.substr(b + 1), c.dir) && c.dir.puerto != 0;
}

// el perfil y los candidatos en Campos (para buscar en la sala y para el HOLA de la DHT)
inline void perfilACampos(const Perfil& p, const std::vector<Candidato>& cand, Campos& c)
{
	c.poner("id", p.id);
	c.poner("nombre", p.nombre);
	c.poner("elo", (long long)p.elo);
	c.poner("version", p.version);
	c.poner("catalogo", p.catalogo);
	c.poner("plataforma", p.plataforma);
	for (size_t i = 0; i < cand.size() && i < 8; i++) c.poner("cand", candidatoATexto(cand[i]));
}
inline std::vector<Candidato> candidatosDeCampos(const Campos& c)
{
	std::vector<Candidato> r;
	for (const std::string& s : c.todos("cand")) {
		Candidato x;
		if (candidatoDeTexto(s, x)) r.push_back(x);
	}
	return r;
}

// el anfitrion es el de id menor (la misma regla en la sala y en la DHT)
inline bool esAnfitrion(const std::string& miId, const std::string& idRival) { return miId < idRival; }

// id anonimo nuevo: 32 caracteres hex aleatorios (128 bits)
inline std::string idAleatorio()
{
	std::random_device rd;
	std::mt19937_64 g(((uint64_t)rd() << 32) ^ rd() ^ (uint64_t)ahoraMs());
	static const char* h = "0123456789abcdef";
	std::string s;
	for (int i = 0; i < 32; i++) s += h[g() & 15];
	return s;
}

}  // namespace red
