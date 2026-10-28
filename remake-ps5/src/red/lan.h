#pragma once
// Red local del modo en linea (rama classic-red-base): "hay un duelo esperando" por broadcast UDP (IPv4) y multicast
// IPv6 (ff02::, todos los equipos del enlace), con el nombre del jugador. El que busca ve la lista y se une llamando
// al anfitrion (DueloRed::invitado con la direccion del anuncio).
//
// Anuncio (little endian): magia u32 "YGCL" | protocolo u16 | tipo u8 (1 anuncio, 2 retiro) | id u64 | puerto u16 |
//                          version del juego (texto) | catalogo (32) | nombre (texto)
// El anfitrion lo manda cada ~1 s; un anuncio que no se repite en 4 s sale de la lista. Los de otra version o catalogo
// se muestran como incompatibles (no se puede unir).
#include "canal.h"

#include <vector>

namespace red {

constexpr uint16_t PUERTO_LAN = 47321;
constexpr uint32_t MAGIA_LAN = 0x4C434759u;   // "YGCL"
constexpr const char* GRUPO_LAN_V6 = "ff02::1";   // todos los nodos del enlace (no hace falta unirse)
constexpr uint64_t MS_ANUNCIO = 1000, MS_ANUNCIO_VENCE = 4000;

struct Anuncio {
	uint64_t id{ 0 };
	std::string nombre, versionJuego;
	Hash catalogo{};
	Direccion anfitrion;   // a donde llamar (ip de quien anuncia y el puerto del duelo)
	uint64_t visto{ 0 };
	bool compatible{ false };
};

inline std::vector<uint8_t> paqueteAnuncio(uint8_t tipo, uint64_t id, uint16_t puerto, const Identidad& yo)
{
	Escritor w;
	w.u32(MAGIA_LAN).u16(VERSION_PROTOCOLO).u8(tipo).u64(id).u16(puerto).texto(yo.versionJuego).hash(yo.catalogo).texto(yo.nombre);
	return w.b;
}

// el anfitrion: anuncia su duelo
class AnuncioLan {
 public:
	std::vector<Direccion> extra;   // destinos de mas (pruebas: 127.0.0.1; o una ip escrita a mano)
	uint16_t puertoLan{ PUERTO_LAN };

	bool empezar(const Identidad& yo, uint16_t puertoDuelo, uint64_t id)
	{
		yo_ = yo;
		puerto_ = puertoDuelo;
		id_ = id ? id : 1;
		const bool a = s4_.abrir(0, false) && s4_.permitirBroadcast();
		const bool b = s6_.abrirCompartido(0, true, true);
		ultimo_ = 0;
		activo_ = a || b;
		return activo_;
	}
	void actualizar(uint64_t ahora)
	{
		if (!activo_ || (ultimo_ && ahora - ultimo_ < MS_ANUNCIO)) return;
		ultimo_ = ahora;
		mandar(1);
	}
	void detener()
	{
		if (!activo_) return;
		mandar(2);
		activo_ = false;
		s4_.cerrar();
		s6_.cerrar();
	}
	bool activo() const { return activo_; }

 private:
	void mandar(uint8_t tipo)
	{
		const std::vector<uint8_t> p = paqueteAnuncio(tipo, id_, puerto_, yo_);
		Direccion d;
		if (s4_.abierto()) {
			Direccion::desdeTexto("255.255.255.255", d);
			d.puerto = puertoLan;
			s4_.enviar(d, p.data(), p.size());
			// (el broadcast dirigido de cada red local: algunos routers no pasan 255.255.255.255)
			for (const Direccion& l : direccionesLocales())
				if (!l.v6) {
					Direccion b = l;
					b.ip[3] = 255;   // (suponiendo /24, lo comun en casa)
					b.puerto = puertoLan;
					s4_.enviar(b, p.data(), p.size());
				}
		}
		if (s6_.abierto() && Direccion::desdeTexto(GRUPO_LAN_V6, d)) {
			d.puerto = puertoLan;
			s6_.enviar(d, p.data(), p.size());
		}
		for (Direccion e : extra) {
			if (e.puerto == 0) e.puerto = puertoLan;
			if (e.v6 ? s6_.abierto() : s4_.abierto()) (e.v6 ? s6_ : s4_).enviar(e, p.data(), p.size());
		}
	}
	SocketUdp s4_, s6_;
	Identidad yo_;
	uint16_t puerto_{ 0 };
	uint64_t id_{ 1 }, ultimo_{ 0 };
	bool activo_{ false };
};

// el que busca: escucha los anuncios y arma la lista
class BuscadorLan {
 public:
	bool empezar(const Identidad& yo, uint16_t puertoLan = PUERTO_LAN)
	{
		yo_ = yo;
		const bool a = s4_.abrirCompartido(puertoLan, false);
		const bool b = s6_.abrirCompartido(puertoLan, true, true);
		if (b) {
			Direccion g;
			if (Direccion::desdeTexto(GRUPO_LAN_V6, g)) s6_.unirMulticastV6(g);
		}
		return a || b;
	}
	void detener()
	{
		s4_.cerrar();
		s6_.cerrar();
		lista_.clear();
	}
	void actualizar(uint64_t ahora)
	{
		uint8_t buf[1500];
		Direccion de;
		for (SocketUdp* s : { &s4_, &s6_ }) {
			if (!s->abierto()) continue;
			for (int k = 0; k < 64; k++) {
				const int n = s->recibir(de, buf, sizeof buf, 0);
				if (n <= 0) break;
				procesar(de, buf, (size_t)n, ahora);
			}
		}
		lista_.erase(std::remove_if(lista_.begin(), lista_.end(), [&](const Anuncio& a) { return ahora - a.visto > MS_ANUNCIO_VENCE; }), lista_.end());
	}
	const std::vector<Anuncio>& lista() const { return lista_; }

 private:
	void procesar(const Direccion& de, const uint8_t* p, size_t n, uint64_t ahora)
	{
		Lector r(p, n);
		if (r.u32() != MAGIA_LAN || r.u16() != VERSION_PROTOCOLO) return;
		const uint8_t tipo = r.u8();
		Anuncio a;
		a.id = r.u64();
		a.anfitrion = de;
		a.anfitrion.puerto = r.u16();
		a.versionJuego = r.texto();
		a.catalogo = r.hash();
		a.nombre = r.texto();
		if (!r.ok) return;
		a.visto = ahora;
		a.compatible = a.versionJuego == yo_.versionJuego && a.catalogo == yo_.catalogo;
		for (size_t i = 0; i < lista_.size(); i++)
			if (lista_[i].id == a.id) {
				if (tipo == 2) lista_.erase(lista_.begin() + (long)i);
				else {
					// (el mismo anuncio llega por varias vias: se prefiere IPv4 que no sea loopback)
					if ((lista_[i].anfitrion.esLoopback() || lista_[i].anfitrion.v6) && !a.anfitrion.v6 && !a.anfitrion.esLoopback()) lista_[i].anfitrion = a.anfitrion;
					lista_[i].visto = ahora;
				}
				return;
			}
		if (tipo == 1) lista_.push_back(a);
	}
	SocketUdp s4_, s6_;
	Identidad yo_;
	std::vector<Anuncio> lista_;
};

}  // namespace red
