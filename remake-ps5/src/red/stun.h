#pragma once
// STUN minimo (RFC 5389) del modo en linea (rama classic-red-nat): solo Binding Request/Response y la direccion
// reflexiva (XOR-MAPPED-ADDRESS; MAPPED-ADDRESS de los servidores viejos). Sin autenticacion: los servidores publicos
// (Google, Cloudflare) no la piden. Funciones puras sobre bytes: el envio lo hace nat.h con el SocketUdp.
// Tambien arma respuestas (el servidor STUN de mentira de las pruebas y del NAT simulado).
#include <cstdint>
#include <cstring>
#include <cstddef>
#include "socket_udp.h"

namespace red {
namespace stun {

static const uint32_t COOKIE = 0x2112A442;
static const uint16_t BINDING_PETICION = 0x0001, BINDING_RESPUESTA = 0x0101, BINDING_ERROR = 0x0111;
static const uint16_t ATR_MAPPED = 0x0001, ATR_XOR_MAPPED = 0x0020, ATR_XOR_MAPPED_VIEJO = 0x8020,
	ATR_SOFTWARE = 0x8022, ATR_FINGERPRINT = 0x8028;

inline uint16_t leer16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
inline uint32_t leer32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
inline void poner16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
inline void poner32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

// CRC-32 (ISO 3309, el de zlib): FINGERPRINT = crc32 ^ 0x5354554e. Tambien la usa el codigo manual (nat.h)
inline uint32_t crc32(const uint8_t* p, size_t n, uint32_t c = 0)
{
	static uint32_t t[256];
	static bool lista = false;
	if (!lista) {
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t k = i;
			for (int j = 0; j < 8; j++) k = (k & 1) ? 0xEDB88320u ^ (k >> 1) : k >> 1;
			t[i] = k;
		}
		lista = true;
	}
	c = ~c;
	for (size_t i = 0; i < n; i++) c = t[(c ^ p[i]) & 0xff] ^ (c >> 8);
	return ~c;
}

// ¿parece STUN? (los dos bits altos en 0, largo multiplo de 4 y la cookie): para separarlo del trafico del juego
inline bool esStun(const uint8_t* p, size_t n)
{
	if (n < 20 || (p[0] & 0xC0) != 0) return false;
	const uint16_t largo = leer16(p + 2);
	return (largo & 3) == 0 && (size_t)largo + 20 <= n && leer32(p + 4) == COOKIE;
}

// Binding Request de 20 bytes (sin atributos) con el id de transaccion dado (12 bytes). Devuelve el largo
inline size_t armarPeticion(uint8_t* buf, const uint8_t tid[12])
{
	poner16(buf, BINDING_PETICION);
	poner16(buf + 2, 0);
	poner32(buf + 4, COOKIE);
	memcpy(buf + 8, tid, 12);
	return 20;
}

// el valor de un (XOR-)MAPPED-ADDRESS: familia 1 = IPv4, 2 = IPv6
inline bool leerDireccion(const uint8_t* v, size_t largo, bool conXor, const uint8_t* cabecera, Direccion& d)
{
	if (largo < 8) return false;
	d = Direccion();
	const uint8_t familia = v[1];
	uint16_t puerto = leer16(v + 2);
	if (conXor) puerto ^= (uint16_t)(COOKIE >> 16);
	d.puerto = puerto;
	if (familia == 1) {
		for (int i = 0; i < 4; i++) d.ip[i] = (uint8_t)(v[4 + i] ^ (conXor ? cabecera[4 + i] : 0));   // xor con la cookie
		return true;
	}
	if (familia == 2 && largo >= 20) {
		d.v6 = true;
		for (int i = 0; i < 16; i++) d.ip[i] = (uint8_t)(v[4 + i] ^ (conXor ? cabecera[4 + i] : 0));   // cookie + id
		return true;
	}
	return false;
}

// Lee una Binding Response. tid (opcional) tiene que coincidir. Si trae FINGERPRINT, se verifica. Prefiere
// XOR-MAPPED-ADDRESS sobre MAPPED-ADDRESS (este lo reescriben algunos routers con "ALG")
inline bool leerRespuesta(const uint8_t* p, size_t n, const uint8_t* tid, Direccion& mapeada)
{
	if (!esStun(p, n) || leer16(p) != BINDING_RESPUESTA) return false;
	if (tid && memcmp(p + 8, tid, 12) != 0) return false;
	const size_t fin = 20 + leer16(p + 2);
	bool hayXor = false, hayPlana = false;
	Direccion conXor, plana;
	for (size_t i = 20; i + 4 <= fin;) {
		const uint16_t tipo = leer16(p + i), largo = leer16(p + i + 2);
		const uint8_t* v = p + i + 4;
		if (i + 4 + largo > fin) return false;
		if ((tipo == ATR_XOR_MAPPED || tipo == ATR_XOR_MAPPED_VIEJO) && !hayXor) hayXor = leerDireccion(v, largo, true, p, conXor);
		else if (tipo == ATR_MAPPED && !hayPlana) hayPlana = leerDireccion(v, largo, false, p, plana);
		else if (tipo == ATR_FINGERPRINT) {
			if (largo != 4 || (crc32(p, i) ^ 0x5354554eu) != leer32(v)) return false;
		}
		i += 4 + ((largo + 3u) & ~3u);
	}
	if (hayXor) { mapeada = conXor; return true; }
	if (hayPlana) { mapeada = plana; return true; }
	return false;
}

// ¿Binding Request? (para el servidor de mentira) Copia el id de transaccion. Si trae FINGERPRINT, lo verifica
inline bool leerPeticion(const uint8_t* p, size_t n, uint8_t tid[12])
{
	if (!esStun(p, n) || leer16(p) != BINDING_PETICION) return false;
	const size_t fin = 20 + leer16(p + 2);
	for (size_t i = 20; i + 4 <= fin;) {
		const uint16_t tipo = leer16(p + i), largo = leer16(p + i + 2);
		if (i + 4 + largo > fin) return false;
		if (tipo == ATR_FINGERPRINT && (largo != 4 || (crc32(p, i) ^ 0x5354554eu) != leer32(p + i + 4))) return false;
		i += 4 + ((largo + 3u) & ~3u);
	}
	memcpy(tid, p + 8, 12);
	return true;
}

// Binding Response con XOR-MAPPED-ADDRESS = vista y FINGERPRINT (buf de 64 bytes o mas). Devuelve el largo
inline size_t armarRespuesta(uint8_t* buf, const uint8_t tid[12], const Direccion& vista)
{
	poner16(buf, BINDING_RESPUESTA);
	poner32(buf + 4, COOKIE);
	memcpy(buf + 8, tid, 12);
	size_t i = 20;
	const size_t largoDir = vista.v6 ? 20 : 8;
	poner16(buf + i, ATR_XOR_MAPPED);
	poner16(buf + i + 2, (uint16_t)largoDir);
	buf[i + 4] = 0;
	buf[i + 5] = vista.v6 ? 2 : 1;
	poner16(buf + i + 6, (uint16_t)(vista.puerto ^ (COOKIE >> 16)));
	for (size_t k = 0; k < (vista.v6 ? 16u : 4u); k++) buf[i + 8 + k] = (uint8_t)(vista.ip[k] ^ buf[4 + k]);
	i += 4 + largoDir;
	poner16(buf + 2, (uint16_t)(i + 8 - 20));   // el largo ya cuenta el FINGERPRINT (asi lo pide el RFC)
	poner16(buf + i, ATR_FINGERPRINT);
	poner16(buf + i + 2, 4);
	poner32(buf + i + 4, crc32(buf, i) ^ 0x5354554eu);
	return i + 8;
}

}  // namespace stun
}  // namespace red
