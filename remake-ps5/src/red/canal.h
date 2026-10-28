#pragma once
// Canal confiable sobre UDP del modo en linea (rama classic-red-base): mensajes ordenados y confiables entre dos
// maquinas (PS5 o PC), con reenvio, acuses, latido cada ~1 s, deteccion de caida y reconexion.
//
// Paquete (little endian):  magia u32 "YGCR" | protocolo u16 | tipo u8 | marcas u8 | sesion u64 | seq u32 | ack u32 |
//                           bitsAck u32 | carga
//  - HOLA (invitado -> anfitrion): nonce, version del juego, hash del catalogo de cartas y nombre. El anfitrion
//    contesta HOLA_OK con la sesion (64 bits al azar, secreta: no viaja en los anuncios) o RECHAZO con el motivo
//    (protocolo, version del juego o catalogo distintos: si no coinciden, no se juega; o ya hay otro jugador).
//  - DATOS: un pedazo de mensaje confiable (seq 1, 2, 3...). Los mensajes largos van en varios pedazos (marca
//    CONTINUA). El receptor entrega en orden, descarta duplicados y guarda los adelantados.
//  - LATIDO: sin datos; lleva los acuses. Se manda si hace ~1 s que no se manda nada (o para acusar enseguida).
//  - ADIOS: el otro se va (abandono explicito).
// Los paquetes con otra magia, otro protocolo u otra sesion se ignoran (paquetes ajenos).
// Caida: 10 s sin recibir nada -> CAIDO (se sigue reintentando); si vuelve antes de los 30 s, CONECTADO otra vez (aunque
// llegue de otra direccion o puerto: la sesion hace de clave); a los 30 s, PERDIDO (el que se quedo gana el duelo).
// Sin hilos: el juego llama actualizar(ahora) cada cuadro. El tiempo entra de afuera (las pruebas simulan minutos).
#include "socket_udp.h"
#include "sha256.h"

#include <algorithm>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace red {

constexpr uint32_t MAGIA = 0x52434759u;          // "YGCR" en little endian
constexpr uint16_t VERSION_PROTOCOLO = 1;
// version del juego para jugar en linea: subirla cuando cambie el motor del duelo (reglas, ia.h) aunque no cambien las
// cartas. El catalogo de cartas va aparte (hashCatalogo en duelo_red.h)
constexpr const char* VERSION_JUEGO = "classic-1.1";   // (rama classic-en-linea: reloj del turno y revancha)
constexpr size_t MAX_PAQUETE = 1200;              // (por debajo del MTU tipico, sin fragmentar en IP)
constexpr size_t CABECERA = 4 + 2 + 1 + 1 + 8 + 4 + 4 + 4;
constexpr size_t MAX_PEDAZO = MAX_PAQUETE - CABECERA;
constexpr uint64_t MS_LATIDO = 1000, MS_CAIDA = 10000, MS_PERDIDA = 30000, MS_HOLA = 250, MS_HOLA_MAX = 15000;

// ------------------------------------------------------------------------------------------------- bytes
// escritura y lectura de mensajes (little endian, textos con su largo)
struct Escritor {
	std::vector<uint8_t> b;
	Escritor& u8(uint8_t v) { b.push_back(v); return *this; }
	Escritor& u16(uint16_t v) { return u8((uint8_t)v).u8((uint8_t)(v >> 8)); }
	Escritor& u32(uint32_t v) { return u16((uint16_t)v).u16((uint16_t)(v >> 16)); }
	Escritor& u64(uint64_t v) { return u32((uint32_t)v).u32((uint32_t)(v >> 32)); }
	Escritor& i32(int32_t v) { return u32((uint32_t)v); }
	Escritor& bytes(const uint8_t* p, size_t n) { b.insert(b.end(), p, p + n); return *this; }
	Escritor& hash(const Hash& h) { return bytes(h.data(), h.size()); }
	Escritor& texto(const std::string& s) { u16((uint16_t)std::min<size_t>(s.size(), 65535)); return bytes((const uint8_t*)s.data(), std::min<size_t>(s.size(), 65535)); }
};
struct Lector {
	const uint8_t* p;
	size_t n, i{ 0 };
	bool ok{ true };
	Lector(const uint8_t* d, size_t len) : p(d), n(len) {}
	explicit Lector(const std::vector<uint8_t>& v) : p(v.data()), n(v.size()) {}
	bool hay(size_t k) { if (i + k > n) ok = false; return ok; }
	uint8_t u8() { if (!hay(1)) return 0; return p[i++]; }
	uint16_t u16() { uint16_t a = u8(); return (uint16_t)(a | (uint16_t)u8() << 8); }
	uint32_t u32() { uint32_t a = u16(); return a | (uint32_t)u16() << 16; }
	uint64_t u64() { uint64_t a = u32(); return a | (uint64_t)u32() << 32; }
	int32_t i32() { return (int32_t)u32(); }
	Hash hash() { Hash h{}; if (hay(32)) { memcpy(h.data(), p + i, 32); i += 32; } return h; }
	std::string texto() { size_t k = u16(); if (!hay(k)) return {}; std::string s((const char*)p + i, k); i += k; return s; }
	bool fin() const { return ok && i == n; }
};

// ------------------------------------------------------------------------------------------------- transporte
// por donde salen los paquetes: el socket UDP de verdad o la red simulada de las pruebas (perdida, duplicado, desorden)
struct Transporte {
	virtual ~Transporte() {}
	virtual bool enviar(const Direccion& a, const uint8_t* datos, size_t n) = 0;
	virtual int recibir(Direccion& de, uint8_t* buf, size_t cap) = 0;   // sin esperar: -1 error, 0 nada
};
struct TransporteUdp : Transporte {
	SocketUdp* s;
	explicit TransporteUdp(SocketUdp* so) : s(so) {}
	bool enviar(const Direccion& a, const uint8_t* datos, size_t n) override { return s->enviar(a, datos, n); }
	int recibir(Direccion& de, uint8_t* buf, size_t cap) override { return s->recibir(de, buf, cap, 0); }
};

// ------------------------------------------------------------------------------------------------- el canal
enum EstadoCanal { C_NADA = 0, C_ESPERANDO, C_CONECTANDO, C_CONECTADO, C_CAIDO, C_PERDIDO, C_RECHAZADO, C_CERRADO };
static const char* const NOMBRE_ESTADO_CANAL[] = { "nada", "esperando", "conectando", "conectado", "caido", "perdido", "rechazado", "cerrado" };
enum Rechazo { R_NINGUNO = 0, R_PROTOCOLO, R_VERSION, R_CATALOGO, R_OCUPADO, R_SIN_RESPUESTA };
static const char* const NOMBRE_RECHAZO[] = { "ninguno", "protocolo distinto", "version del juego distinta", "catalogo de cartas distinto",
                                              "ya hay otro jugador", "sin respuesta" };
enum TipoPaquete : uint8_t { P_HOLA = 1, P_HOLA_OK, P_RECHAZO, P_DATOS, P_LATIDO, P_ADIOS };
enum : uint8_t { MARCA_CONTINUA = 1 };

// quien soy para el otro: lo que se compara al conectar
struct Identidad {
	std::string versionJuego{ VERSION_JUEGO };
	Hash catalogo{};
	std::string nombre;
};

class Canal {
 public:
	// estadisticas (pruebas y aviso en pantalla)
	struct Cuentas {
		int enviados{ 0 }, recibidos{ 0 }, reenviados{ 0 }, duplicados{ 0 }, desordenados{ 0 }, ajenos{ 0 }, caidas{ 0 }, reconexiones{ 0 };
	} cuentas;

	Canal() = default;
	void usar(Transporte* t, const Identidad& yo, uint64_t azar)
	{
		t_ = t;
		yo_ = yo;
		azar_ = azar ? azar : 0x9E3779B97F4A7C15ull;
	}
	// anfitrion: espera el HOLA de un invitado
	void escuchar(uint64_t ahora)
	{
		reiniciarSesion();
		estado_ = C_ESPERANDO;
		ultimoRecibido_ = ahora;
	}
	// invitado: llama al anfitrion
	void conectar(const Direccion& a, uint64_t ahora)
	{
		reiniciarSesion();
		par_ = a;
		nonce_ = siguienteAzar();
		estado_ = C_CONECTANDO;
		inicioHola_ = ahora;
		ultimoHola_ = 0;
		ultimoRecibido_ = ahora;
		enviarHola(ahora);
	}
	// cerrar avisando al otro (abandono): ADIOS tres veces por si se pierde alguno
	void cerrar(uint64_t ahora)
	{
		if (estado_ == C_CONECTADO || estado_ == C_CAIDO)
			for (int k = 0; k < 3; k++) enviarControl(P_ADIOS, {}, ahora);
		estado_ = C_CERRADO;
	}

	EstadoCanal estado() const { return estado_; }
	bool conectado() const { return estado_ == C_CONECTADO || estado_ == C_CAIDO; }
	Rechazo rechazo() const { return rechazo_; }
	const Identidad& otro() const { return otro_; }
	const Direccion& par() const { return par_; }
	uint64_t sesion() const { return sesion_; }
	bool esAnfitrion() const { return anfitrion_; }
	bool adiosRecibido() const { return adios_; }
	// ms sin recibir nada del otro
	uint64_t silencio(uint64_t ahora) const { return ahora > ultimoRecibido_ ? ahora - ultimoRecibido_ : 0; }
	size_t enVuelo() const { return pendientes_.size(); }

	// manda un mensaje confiable (llega una vez, en orden). false: no hay conexion
	bool enviar(const std::vector<uint8_t>& msg, uint64_t ahora)
	{
		if (!conectado() && estado_ != C_CONECTANDO && estado_ != C_ESPERANDO) return false;
		size_t i = 0;
		do {
			const size_t k = std::min(MAX_PEDAZO, msg.size() - i);
			Pendiente p;
			p.seq = siguienteSeq_++;
			p.marcas = i + k < msg.size() ? MARCA_CONTINUA : 0;
			p.datos.assign(msg.begin() + (long)i, msg.begin() + (long)(i + k));
			p.rto = 150;
			pendientes_[p.seq] = p;
			i += k;
		} while (i < msg.size());
		if (conectado()) vaciarPendientes(ahora);   // (antes de conectar salen al conectar)
		return true;
	}
	// el siguiente mensaje recibido, en orden
	bool recibir(std::vector<uint8_t>& msg)
	{
		if (entrantes_.empty()) return false;
		msg = std::move(entrantes_.front());
		entrantes_.pop_front();
		return true;
	}

	// cada cuadro: lee lo que llego, reenvia lo no acusado, latidos, caida y perdida
	void actualizar(uint64_t ahora)
	{
		if (!t_) return;
		uint8_t buf[2048];
		Direccion de;
		for (int vueltas = 0; vueltas < 512; vueltas++) {
			const int n = t_->recibir(de, buf, sizeof buf);
			if (n <= 0) break;
			procesar(de, buf, (size_t)n, ahora);
		}
		if (estado_ == C_CONECTANDO) {
			if (ahora - inicioHola_ > MS_HOLA_MAX) {
				estado_ = C_RECHAZADO;
				rechazo_ = R_SIN_RESPUESTA;
			} else if (ahora - ultimoHola_ >= MS_HOLA)
				enviarHola(ahora);
			return;
		}
		if (!conectado()) return;
		// reenvios
		for (auto& kv : pendientes_) {
			Pendiente& p = kv.second;
			if (p.enviado && ahora - p.ultimo >= p.rto) {
				enviarDatos(p, ahora);
				p.rto = std::min<uint64_t>(1000, p.rto * 2);
				cuentas.reenviados++;
			}
		}
		vaciarPendientes(ahora);
		if (ackPendiente_ || ahora - ultimoEnvio_ >= MS_LATIDO) enviarControl(P_LATIDO, {}, ahora);
		const uint64_t s = silencio(ahora);
		if (s >= MS_PERDIDA) {
			estado_ = C_PERDIDO;
		} else if (s >= MS_CAIDA && estado_ == C_CONECTADO) {
			estado_ = C_CAIDO;
			cuentas.caidas++;
		}
	}

 private:
	struct Pendiente {
		uint32_t seq{ 0 };
		uint8_t marcas{ 0 };
		std::vector<uint8_t> datos;
		bool enviado{ false };
		uint64_t ultimo{ 0 }, rto{ 150 };
	};
	uint64_t siguienteAzar()
	{
		// splitmix64 (identificadores; la semilla viene de afuera: reloj y direcciones)
		uint64_t z = (azar_ += 0x9E3779B97F4A7C15ull);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}
	void reiniciarSesion()
	{
		sesion_ = 0;
		nonce_ = 0;
		siguienteSeq_ = 1;
		esperado_ = 1;
		pendientes_.clear();
		adelantados_.clear();
		entrantes_.clear();
		armando_.clear();
		rechazo_ = R_NINGUNO;
		adios_ = false;
		ackPendiente_ = false;
		anfitrion_ = false;
	}
	void cabecera(Escritor& w, uint8_t tipo, uint8_t marcas, uint64_t sesion, uint32_t seq)
	{
		w.u32(MAGIA).u16(VERSION_PROTOCOLO).u8(tipo).u8(marcas).u64(sesion).u32(seq).u32(esperado_);
		uint32_t bits = 0;   // adelantados recibidos: bit i = seq esperado + 1 + i
		for (auto& kv : adelantados_)
			if (kv.first > esperado_ && kv.first - esperado_ - 1 < 32) bits |= 1u << (kv.first - esperado_ - 1);
		w.u32(bits);
	}
	void mandar(const Escritor& w, uint64_t ahora)
	{
		t_->enviar(par_, w.b.data(), w.b.size());
		ultimoEnvio_ = ahora;
		cuentas.enviados++;
	}
	void enviarControl(uint8_t tipo, const std::vector<uint8_t>& carga, uint64_t ahora)
	{
		Escritor w;
		cabecera(w, tipo, 0, sesion_, 0);
		w.b.insert(w.b.end(), carga.begin(), carga.end());
		mandar(w, ahora);
		ackPendiente_ = false;
	}
	void enviarDatos(Pendiente& p, uint64_t ahora)
	{
		Escritor w;
		cabecera(w, P_DATOS, p.marcas, sesion_, p.seq);
		w.b.insert(w.b.end(), p.datos.begin(), p.datos.end());
		mandar(w, ahora);
		p.enviado = true;
		p.ultimo = ahora;
		ackPendiente_ = false;
	}
	// manda los que nunca salieron (ventana de 256 en vuelo)
	void vaciarPendientes(uint64_t ahora)
	{
		int enVuelo = 0;
		for (auto& kv : pendientes_) {
			if (enVuelo >= 256) break;
			if (!kv.second.enviado) enviarDatos(kv.second, ahora);
			enVuelo++;
		}
	}
	void enviarHola(uint64_t ahora)
	{
		Escritor c;
		c.u64(nonce_).texto(yo_.versionJuego).hash(yo_.catalogo).texto(yo_.nombre);
		Escritor w;
		w.u32(MAGIA).u16(VERSION_PROTOCOLO).u8(P_HOLA).u8(0).u64(0).u32(0).u32(0).u32(0);
		w.b.insert(w.b.end(), c.b.begin(), c.b.end());
		mandar(w, ahora);
		ultimoHola_ = ahora;
	}
	void responderHola(const Direccion& a, uint8_t tipo, uint64_t nonce, Rechazo motivo, uint64_t ahora)
	{
		Escritor w;
		w.u32(MAGIA).u16(VERSION_PROTOCOLO).u8(tipo).u8(0).u64(0).u32(0).u32(0).u32(0);
		w.u64(nonce);
		if (tipo == P_HOLA_OK) w.u64(sesion_);
		else w.u8((uint8_t)motivo);
		w.texto(yo_.versionJuego).hash(yo_.catalogo).texto(yo_.nombre);
		t_->enviar(a, w.b.data(), w.b.size());
		cuentas.enviados++;
		(void)ahora;
	}

	void procesar(const Direccion& de, const uint8_t* p, size_t n, uint64_t ahora)
	{
		Lector r(p, n);
		const uint32_t magia = r.u32();
		const uint16_t proto = r.u16();
		const uint8_t tipo = r.u8(), marcas = r.u8();
		const uint64_t sesion = r.u64();
		const uint32_t seq = r.u32(), ack = r.u32(), bits = r.u32();
		if (!r.ok || magia != MAGIA) {
			cuentas.ajenos++;
			return;
		}
		if (proto != VERSION_PROTOCOLO) {   // otro protocolo: al que llama se le contesta (sabe por que no se juega)
			if (tipo == P_HOLA && estado_ == C_ESPERANDO) {
				Lector h(p + CABECERA, n - CABECERA);
				responderHola(de, P_RECHAZO, h.u64(), R_PROTOCOLO, ahora);
			}
			cuentas.ajenos++;
			return;
		}
		if (tipo == P_HOLA) {
			procesarHola(de, r, ahora);
			return;
		}
		if (tipo == P_HOLA_OK || tipo == P_RECHAZO) {
			if (estado_ != C_CONECTANDO || !(de == par_)) return;
			const uint64_t nonce = r.u64();
			if (nonce != nonce_) {
				cuentas.ajenos++;
				return;
			}
			if (tipo == P_HOLA_OK) {
				sesion_ = r.u64();
				otro_.versionJuego = r.texto();
				otro_.catalogo = r.hash();
				otro_.nombre = r.texto();
				if (!r.ok) return;
				estado_ = C_CONECTADO;
				ultimoRecibido_ = ahora;
				vaciarPendientes(ahora);
			} else {
				rechazo_ = (Rechazo)r.u8();
				otro_.versionJuego = r.texto();
				otro_.catalogo = r.hash();
				otro_.nombre = r.texto();
				estado_ = C_RECHAZADO;
			}
			return;
		}
		// lo demas es de la sesion
		if (!conectado() || sesion == 0 || sesion != sesion_) {
			cuentas.ajenos++;
			return;
		}
		if (!(de == par_)) par_ = de;   // reconexion desde otra direccion o puerto (la sesion es la clave)
		ultimoRecibido_ = ahora;
		if (estado_ == C_CAIDO) {
			estado_ = C_CONECTADO;
			cuentas.reconexiones++;
		}
		// acuses: todo lo anterior a ack y los bits
		for (auto it = pendientes_.begin(); it != pendientes_.end();) {
			const uint32_t s = it->first;
			const bool acusado = s < ack || (s > ack && s - ack - 1 < 32 && (bits >> (s - ack - 1) & 1u));
			if (acusado) it = pendientes_.erase(it);
			else ++it;
		}
		if (tipo == P_ADIOS) {
			adios_ = true;
			estado_ = C_PERDIDO;
			return;
		}
		if (tipo != P_DATOS) return;
		cuentas.recibidos++;
		ackPendiente_ = true;
		if (seq < esperado_ || adelantados_.count(seq)) {
			cuentas.duplicados++;
			return;
		}
		if (seq > esperado_ + 4096) return;   // (fuera de toda ventana)
		Pedazo pz;
		pz.marcas = marcas;
		pz.datos.assign(p + CABECERA, p + n);
		if (seq != esperado_) cuentas.desordenados++;
		adelantados_[seq] = std::move(pz);
		while (!adelantados_.empty() && adelantados_.begin()->first == esperado_) {
			Pedazo& x = adelantados_.begin()->second;
			armando_.insert(armando_.end(), x.datos.begin(), x.datos.end());
			if (!(x.marcas & MARCA_CONTINUA)) {
				entrantes_.push_back(std::move(armando_));
				armando_.clear();
			}
			adelantados_.erase(adelantados_.begin());
			esperado_++;
		}
	}
	void procesarHola(const Direccion& de, Lector& r, uint64_t ahora)
	{
		const uint64_t nonce = r.u64();
		Identidad o;
		o.versionJuego = r.texto();
		o.catalogo = r.hash();
		o.nombre = r.texto();
		if (!r.ok) return;
		if (estado_ != C_ESPERANDO && !(conectado() && anfitrion_)) return;
		if (conectado()) {   // ya hay sesion: el mismo invitado repite (se perdio el HOLA_OK) u otro llega tarde
			responderHola(de, nonce == nonceInvitado_ ? P_HOLA_OK : P_RECHAZO, nonce, R_OCUPADO, ahora);
			if (nonce == nonceInvitado_) par_ = de;
			return;
		}
		Rechazo m = R_NINGUNO;
		if (o.versionJuego != yo_.versionJuego) m = R_VERSION;
		else if (o.catalogo != yo_.catalogo) m = R_CATALOGO;
		if (m != R_NINGUNO) {
			rechazo_ = m;   // (se sigue esperando a otro; el juego puede mostrar el motivo)
			otro_ = o;
			responderHola(de, P_RECHAZO, nonce, m, ahora);
			return;
		}
		do sesion_ = siguienteAzar();
		while (sesion_ == 0);
		nonceInvitado_ = nonce;
		otro_ = o;
		par_ = de;
		anfitrion_ = true;
		estado_ = C_CONECTADO;
		ultimoRecibido_ = ahora;
		responderHola(de, P_HOLA_OK, nonce, R_NINGUNO, ahora);
		vaciarPendientes(ahora);
	}

	struct Pedazo {
		uint8_t marcas{ 0 };
		std::vector<uint8_t> datos;
	};
	Transporte* t_{ nullptr };
	Identidad yo_, otro_;
	EstadoCanal estado_{ C_NADA };
	Rechazo rechazo_{ R_NINGUNO };
	Direccion par_;
	uint64_t azar_{ 1 }, sesion_{ 0 }, nonce_{ 0 }, nonceInvitado_{ 0 };
	uint64_t ultimoRecibido_{ 0 }, ultimoEnvio_{ 0 }, inicioHola_{ 0 }, ultimoHola_{ 0 };
	uint32_t siguienteSeq_{ 1 }, esperado_{ 1 };
	bool ackPendiente_{ false }, adios_{ false }, anfitrion_{ false };
	std::map<uint32_t, Pendiente> pendientes_;
	std::map<uint32_t, Pedazo> adelantados_;
	std::deque<std::vector<uint8_t>> entrantes_;
	std::vector<uint8_t> armando_;
};

}  // namespace red
