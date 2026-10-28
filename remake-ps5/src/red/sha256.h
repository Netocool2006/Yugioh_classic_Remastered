#pragma once
// SHA-256 (FIPS 180-4) pequenno y propio, sin dependencias, para el modo en linea: compromisos del barajado, de la
// semilla comun (moneda), hash del catalogo de cartas y del estado del duelo. Mismo resultado en PS5, Linux y Windows.
#include <array>
#include <cstdint>
#include <cstring>
#include <string>

namespace red {

using Hash = std::array<uint8_t, 32>;

class Sha256 {
 public:
	Sha256() { reiniciar(); }
	void reiniciar()
	{
		static const uint32_t ini[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
		memcpy(h_, ini, sizeof h_);
		n_ = 0;
		total_ = 0;
	}
	Sha256& agregar(const void* datos, size_t n)
	{
		const uint8_t* p = (const uint8_t*)datos;
		total_ += n;
		while (n > 0) {
			size_t k = 64 - n_;
			if (k > n) k = n;
			memcpy(buf_ + n_, p, k);
			n_ += k;
			p += k;
			n -= k;
			if (n_ == 64) {
				bloque(buf_);
				n_ = 0;
			}
		}
		return *this;
	}
	Sha256& agregar(const std::string& s) { return agregar(s.data(), s.size()); }
	Sha256& agregar(const Hash& h) { return agregar(h.data(), h.size()); }
	// enteros en little endian (igual en todas las plataformas)
	Sha256& agregarU32(uint32_t v)
	{
		uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
		return agregar(b, 4);
	}
	Sha256& agregarI32(int32_t v) { return agregarU32((uint32_t)v); }
	Sha256& agregarU64(uint64_t v) { return agregarU32((uint32_t)v).agregarU32((uint32_t)(v >> 32)); }
	// texto con su largo delante (sin ambiguedad entre "ab"+"c" y "a"+"bc")
	Sha256& agregarTexto(const std::string& s) { return agregarU32((uint32_t)s.size()).agregar(s); }
	Hash fin()
	{
		const uint64_t bits = total_ * 8;
		const uint8_t uno = 0x80, cero = 0;
		agregar(&uno, 1);
		while (n_ != 56) agregar(&cero, 1);
		uint8_t lb[8];
		for (int i = 0; i < 8; i++) lb[i] = (uint8_t)(bits >> (56 - 8 * i));
		agregar(lb, 8);
		Hash r;
		for (int i = 0; i < 8; i++) {
			r[(size_t)(4 * i)] = (uint8_t)(h_[i] >> 24);
			r[(size_t)(4 * i + 1)] = (uint8_t)(h_[i] >> 16);
			r[(size_t)(4 * i + 2)] = (uint8_t)(h_[i] >> 8);
			r[(size_t)(4 * i + 3)] = (uint8_t)h_[i];
		}
		reiniciar();
		return r;
	}

 private:
	static uint32_t rot(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
	void bloque(const uint8_t* b)
	{
		static const uint32_t K[64] = {
			0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
			0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
			0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
			0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
			0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
			0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
		uint32_t w[64];
		for (int i = 0; i < 16; i++) w[i] = (uint32_t)b[4 * i] << 24 | (uint32_t)b[4 * i + 1] << 16 | (uint32_t)b[4 * i + 2] << 8 | b[4 * i + 3];
		for (int i = 16; i < 64; i++) {
			const uint32_t s0 = rot(w[i - 15], 7) ^ rot(w[i - 15], 18) ^ (w[i - 15] >> 3);
			const uint32_t s1 = rot(w[i - 2], 17) ^ rot(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}
		uint32_t a = h_[0], bb = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
		for (int i = 0; i < 64; i++) {
			const uint32_t t1 = h + (rot(e, 6) ^ rot(e, 11) ^ rot(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
			const uint32_t t2 = (rot(a, 2) ^ rot(a, 13) ^ rot(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
			h = g;
			g = f;
			f = e;
			e = d + t1;
			d = c;
			c = bb;
			bb = a;
			a = t1 + t2;
		}
		h_[0] += a; h_[1] += bb; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
	}
	uint32_t h_[8];
	uint8_t buf_[64];
	size_t n_;
	uint64_t total_;
};

inline Hash sha256(const void* datos, size_t n) { return Sha256().agregar(datos, n).fin(); }
inline Hash sha256(const std::string& s) { return sha256(s.data(), s.size()); }
inline std::string hex(const Hash& h, size_t bytes = 32)
{
	static const char* d = "0123456789abcdef";
	std::string r;
	for (size_t i = 0; i < bytes && i < h.size(); i++) {
		r += d[h[i] >> 4];
		r += d[h[i] & 15];
	}
	return r;
}
// los primeros 8 bytes como entero (identificadores, semillas)
inline uint64_t aU64(const Hash& h)
{
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--) v = (v << 8) | h[(size_t)i];
	return v;
}

}  // namespace red
