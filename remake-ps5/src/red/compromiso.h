#pragma once
// Barajado con compromiso del modo en linea (rama classic-red-base): anti trampas con desconocidos, sin servidor.
//
//  1. Cada jugador saca una semilla privada (32 bytes) y manda solo su hash (compromiso de la semilla) junto con el
//     hash de su mitad de la semilla comun. Despues manda la mitad; la semilla comun = SHA-256(mitad anfitrion,
//     mitad invitado): ninguno elige el resultado (la moneda y el azar del duelo salen de ahi).
//  2. Cada uno baraja SU mazo con SHA-256(semilla privada, semilla comun): como la privada quedo comprometida antes
//     de conocer la comun, nadie puede probar semillas hasta dar con un buen orden (ni ordenar el mazo a mano).
//  3. Manda el compromiso del orden: por cada carta, SHA-256(sal_i, carta_i) con sal_i = SHA-256("sal", privada, i), y
//     el hash de todo (cartas + compromiso de la semilla).
//  4. Cuando una carta sale de arriba del mazo (robo), el duenno la revela con su sal: el otro la verifica contra la
//     posicion comprometida. Al terminar, cada uno revela su semilla privada: el otro recalcula el orden y todas las
//     sales y verifica todo. Si algo no coincide, se marca la trampa.
// Version 1 (ver docs/decisiones_pendientes.md): el motor necesita las cartas del rival, asi que el orden tambien se
// manda al empezar (todo conocido en las dos maquinas); el compromiso garantiza que el orden es justo (no elegido) y
// la verificacion final lo comprueba. Esconder el mazo y la mano del rival a la otra maquina queda pendiente.
#include "sha256.h"

#include <algorithm>
#include <string>
#include <vector>

namespace red {

// azar a partir de un hash (SHA-256 en modo contador): igual en todas las plataformas
class AzarHash {
 public:
	explicit AzarHash(const Hash& semilla) : s_(semilla) {}
	uint32_t u32()
	{
		if (pos_ >= 32) {
			buf_ = Sha256().agregar(s_).agregarU64(ctr_++).fin();
			pos_ = 0;
		}
		uint32_t v = (uint32_t)buf_[(size_t)pos_] | (uint32_t)buf_[(size_t)pos_ + 1] << 8 | (uint32_t)buf_[(size_t)pos_ + 2] << 16 |
		             (uint32_t)buf_[(size_t)pos_ + 3] << 24;
		pos_ += 4;
		return v;
	}
	// [0, n) sin sesgo (rechazo)
	uint32_t hasta(uint32_t n)
	{
		if (n <= 1) return 0;
		const uint32_t lim = 0xFFFFFFFFu - 0xFFFFFFFFu % n;
		uint32_t v;
		do v = u32();
		while (v >= lim);
		return v % n;
	}

 private:
	Hash s_, buf_{};
	uint64_t ctr_{ 0 };
	int pos_{ 32 };
};

// semilla comun a partir de las dos mitades (primero la del anfitrion)
inline Hash semillaComun(const Hash& mitadAnfitrion, const Hash& mitadInvitado)
{
	return Sha256().agregarTexto("yugioh-classic/comun").agregar(mitadAnfitrion).agregar(mitadInvitado).fin();
}
inline Hash compromisoDe(const Hash& secreto) { return Sha256().agregarTexto("yugioh-classic/compromiso").agregar(secreto).fin(); }

// el orden del mazo: se ordena primero (no depende de como venia) y se baraja con (privada, comun). La ultima del
// vector es la de arriba (ia::Duelo roba de mazo.back())
inline std::vector<std::string> ordenMazo(std::vector<std::string> mazo, const Hash& privada, const Hash& comun)
{
	std::sort(mazo.begin(), mazo.end());
	AzarHash a(Sha256().agregarTexto("yugioh-classic/orden").agregar(privada).agregar(comun).fin());
	for (size_t i = mazo.size(); i > 1; i--) std::swap(mazo[i - 1], mazo[a.hasta((uint32_t)i)]);
	return mazo;
}
inline Hash salDe(const Hash& privada, uint32_t i) { return Sha256().agregarTexto("yugioh-classic/sal").agregar(privada).agregarU32(i).fin(); }
inline Hash compromisoCarta(const Hash& sal, const std::string& id) { return Sha256().agregar(sal).agregarTexto(id).fin(); }

struct CompromisoMazo {
	std::vector<Hash> cartas;   // por posicion (0 = la de abajo, como el vector del mazo)
	Hash semilla{};             // compromiso de la semilla privada (el mismo que se mando al principio)
	Hash mazo{};                // hash de todo lo anterior
	Hash calcularMazo() const
	{
		Sha256 s;
		s.agregarTexto("yugioh-classic/mazo").agregar(semilla).agregarU32((uint32_t)cartas.size());
		for (const Hash& h : cartas) s.agregar(h);
		return s.fin();
	}
};
inline CompromisoMazo comprometer(const std::vector<std::string>& orden, const Hash& privada)
{
	CompromisoMazo c;
	c.semilla = compromisoDe(privada);
	for (size_t i = 0; i < orden.size(); i++) c.cartas.push_back(compromisoCarta(salDe(privada, (uint32_t)i), orden[i]));
	c.mazo = c.calcularMazo();
	return c;
}
// una carta revelada (robo): posicion, carta y su sal
inline bool verificarCarta(const CompromisoMazo& c, uint32_t pos, const std::string& id, const Hash& sal)
{
	return pos < c.cartas.size() && compromisoCarta(sal, id) == c.cartas[pos];
}

enum Verificacion { VER_OK = 0, VER_SEMILLA, VER_COMPROMISO, VER_ORDEN, VER_JUGADO, VER_REVELACION };
static const char* const NOMBRE_VERIFICACION[] = { "ok", "la semilla privada no es la comprometida", "el compromiso del mazo no cuadra",
                                                   "el orden no sale de las semillas (mazo arreglado)", "el mazo jugado no es el comprometido",
                                                   "una carta revelada no coincide" };
// la verificacion final del mazo del otro: su semilla privada revelada contra todo lo que mando
inline Verificacion verificarFinal(const CompromisoMazo& c, const Hash& compromisoSemilla, const Hash& privada, const Hash& comun,
                                   const std::vector<std::string>& ordenJugado)
{
	if (compromisoDe(privada) != compromisoSemilla || c.semilla != compromisoSemilla) return VER_SEMILLA;
	if (c.calcularMazo() != c.mazo || c.cartas.size() != ordenJugado.size()) return VER_COMPROMISO;
	for (size_t i = 0; i < ordenJugado.size(); i++)
		if (compromisoCarta(salDe(privada, (uint32_t)i), ordenJugado[i]) != c.cartas[i]) return VER_JUGADO;
	if (ordenMazo(ordenJugado, privada, comun) != ordenJugado) return VER_ORDEN;
	return VER_OK;
}

}  // namespace red
