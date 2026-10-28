#pragma once
// Duelo en linea en lockstep (rama classic-red-base): el rival humano remoto reemplaza a la IA.
//
// Las dos maquinas corren el mismo motor (ia::Duelo) con los mismos lados: el anfitrion es el lado 0 y el invitado el
// lado 1 en las dos. Por la red viajan solo las decisiones (cada respuesta a lo que el motor pregunta: la ia::Jugada
// del lado que decide, ia::Duelo::decide()), la semilla comun y los mazos con su compromiso. Cada jugada lleva el hash
// del estado despues de aplicarla: el otro la aplica, compara y, si difiere, el duelo termina sin ganador
// (desincronizacion) y se guarda un registro para depurar.
//
// Mensajes (sobre el canal confiable, canal.h):
//   INICIO  compromiso de la mitad de la semilla comun y de la semilla privada, opciones que tocan el motor
//           ("preguntar antes de activar") y nombre
//   MITAD   la mitad de la semilla comun (despues de recibir el INICIO del otro: nadie elige el resultado)
//   MAZO    compromiso del mazo (compromiso.h), mazo de fusion y (version 1, todo conocido) el orden
//   ORDEN   el que gano la moneda (bit de la semilla comun) dice quien empieza
//   JUGADA  numero, si el motor la acepto, la jugada y el hash del estado
//   REVELA  una carta que salio de arriba del mazo, con su sal (se verifica contra el compromiso)
//   FIN     la semilla privada (verificacion final del mazo) y el hash del estado final
//   RENDIRSE, DESINCRONIZADO
//   AGOTADO  (rama classic-en-linea) al que decide se le acabo el reloj del turno: lo manda el mismo (el otro no decide
//            por su reloj, solo lo recibe); los dos lo cuentan y al llegar a ConfigDuelo::agotamientosDerrota pierde
//   REVANCHA (rama classic-en-linea) pide la revancha al terminar; con las dos pedidas se vuelve a empezar en el mismo
//            canal (semillas, moneda y mazos nuevos)
// Abandono (el rival se corto 30 s o se fue): el duelo termina SIN ganador (no cuenta para ninguno; rama classic-en-linea).
// La entrada para el juego: iniciarDueloRed() (socket UDP + canal + duelo). El menu "DUELO EN LINEA" se arma despues
// de unir las ramas de red (classic-red-nat, classic-sala); el lado 1 se ve con la vista en espejo (red/espejo.h,
// rama classic-en-linea).
#include "../ia.h"
#include "canal.h"
#include "compromiso.h"

#include <cstdio>
#include <ctime>
#include <memory>

namespace red {

// ------------------------------------------------------------------------------------------------- hashes
// el catalogo de cartas y las recetas que usa el motor: si no coinciden, no se juega
inline Hash hashCatalogo(const ia::Base& b)
{
	Sha256 s;
	s.agregarTexto("yugioh-classic/catalogo").agregarU32((uint32_t)b.cartas.size());
	for (const ia::Carta& c : b.cartas) {
		s.agregarTexto(c.id).agregarTexto(c.clase).agregarTexto(c.subtipo).agregarTexto(c.atributo).agregarTexto(c.raza);
		s.agregarI32(c.nivel).agregarI32(c.atk).agregarI32(c.def).agregarI32(c.limite);
		s.agregarU32((uint32_t)c.monstruo | (uint32_t)c.magia << 1 | (uint32_t)c.trampa << 2 | (uint32_t)c.noInvocable << 3 | (uint32_t)c.invocaEspecial << 4);
	}
	// (unordered_map: se ordenan las claves para que el hash no dependa de la plataforma)
	std::vector<std::string> claves;
	for (const auto& kv : b.fusiones) claves.push_back(kv.first);
	std::sort(claves.begin(), claves.end());
	for (const std::string& k : claves) {
		s.agregarTexto(k).agregarU32((uint32_t)b.fusiones.at(k).size());
		for (const std::string& m : b.fusiones.at(k)) s.agregarTexto(m);
	}
	claves.clear();
	for (const auto& kv : b.rituales) claves.push_back(kv.first);
	std::sort(claves.begin(), claves.end());
	for (const std::string& k : claves) s.agregarTexto(k).agregarTexto(b.rituales.at(k));
	return s.fin();
}

// el estado del duelo que importa a las reglas (no la interfaz ni los sucesos)
inline Hash hashEstado(const ia::Duelo& d)
{
	Sha256 s;
	auto lista = [&](const std::vector<std::string>& v) {
		s.agregarU32((uint32_t)v.size());
		for (const std::string& x : v) s.agregarTexto(x);
	};
	auto lugares = [&](const std::vector<ia::Lugar>& v) {
		s.agregarU32((uint32_t)v.size());
		for (const ia::Lugar& g : v) s.agregarI32(g.lado).agregarI32(g.donde).agregarI32(g.i);
	};
	auto mt = [&](const ia::MagiaTrampa& c) {
		s.agregarTexto(c.id).agregarU32(c.bocaAbajo).agregarI32(c.uid).agregarI32(c.turnoColocada).agregarI32(c.eqUid).agregarI32(c.contadores);
	};
	s.agregarI32(d.turno).agregarI32(d.activo).agregarI32(d.ganador).agregarI32((int)d.fase);
	for (const ia::Lado& x : d.l) {
		s.agregarI32(x.lp);
		lista(x.mazo);
		lista(x.mano);
		lista(x.extra);
		lista(x.cementerio);
		lista(x.desterrados);
		s.agregarU32(x.invocoNormal);
		for (bool b : x.bloqueada) s.agregarU32(b);
		for (const ia::Monstruo& m : x.m) {
			s.agregarTexto(m.id).agregarU32((uint32_t)m.bocaAbajo | (uint32_t)m.defensa << 1 | (uint32_t)m.llegoEsteTurno << 2 |
			                                 (uint32_t)m.cambioPosicion << 3 | (uint32_t)m.ataco << 4 | (uint32_t)m.visto << 5 |
			                                 (uint32_t)m.especial << 6 | (uint32_t)m.token << 7);
			s.agregarI32(m.uid).agregarI32(m.duenno).agregarI32(m.atkEf).agregarI32(m.defEf).agregarI32(m.atkTurno).agregarI32(m.defTurno);
			s.agregarI32(m.atkPerm).agregarI32(m.defPerm).agregarI32(m.contadores).agregarI32(m.ataquesExtra).agregarI32(m.nivel);
			s.agregarTexto(m.atributo).agregarTexto(m.raza);
		}
		for (const ia::MagiaTrampa& c : x.mt) mt(c);
		mt(x.campo);
	}
	const ia::EstadoEfectos& e = d.ef;
	s.agregarU32((uint32_t)e.cadena.size());
	for (const ia::Eslabon& x : e.cadena) {
		s.agregarTexto(x.carta).agregarI32(x.lado).agregarI32(x.fuente.lado).agregarI32(x.fuente.donde).agregarI32(x.fuente.i);
		s.agregarI32(x.paso).agregarI32(x.valor).agregarI32(x.efecto).agregarU32(x.negado);
		lugares(x.obj);
		lugares(x.elegidos);
	}
	s.agregarU32((uint32_t)e.disparos.size());
	s.agregarI32(e.preg.tipo).agregarI32(e.preg.lado).agregarTexto(e.preg.carta).agregarI32(e.preg.faltan);
	lugares(e.preg.cands);
	s.agregarI32(e.ventana.tipo).agregarI32(e.ventana.lado).agregarI32(e.pases).agregarI32(e.pendiente).agregarU32(e.resolviendo);
	s.agregarI32(e.uid).agregarU32(e.azar.s).agregarI32(e.resueltos);
	s.agregarU32(e.ataque.hay).agregarI32(e.ataque.lado).agregarI32(e.ataque.zona).agregarI32(e.ataque.objetivo).agregarI32(e.ataque.uidA);
	return s.fin();
}

// ------------------------------------------------------------------------------------------------- jugadas
inline void escribirJugada(Escritor& w, const ia::Jugada& j)
{
	auto enteros = [&](const std::vector<int>& v) {
		w.u16((uint16_t)v.size());
		for (int x : v) w.i32(x);
	};
	auto lugares = [&](const std::vector<ia::Lugar>& v) {
		w.u16((uint16_t)v.size());
		for (const ia::Lugar& g : v) w.u8((uint8_t)g.lado).u8((uint8_t)g.donde).i32(g.i);
	};
	w.u8((uint8_t)j.tipo).i32(j.mano).i32(j.mano2).i32(j.zona).i32(j.objetivo);
	enteros(j.tributos);
	enteros(j.manoExtra);
	w.texto(j.carta).u8(j.deMonstruo);
	lugares(j.objs);
	enteros(j.descartes);
	lugares(j.costes);
}
inline bool leerJugada(Lector& r, ia::Jugada& j)
{
	auto enteros = [&](std::vector<int>& v) {
		v.resize(r.u16());
		for (int& x : v) x = r.i32();
	};
	auto lugares = [&](std::vector<ia::Lugar>& v) {
		const size_t n = r.u16();
		v.clear();
		for (size_t k = 0; k < n && r.ok; k++) {
			ia::Lugar g;
			g.lado = r.u8();
			g.donde = r.u8();
			g.i = r.i32();
			v.push_back(g);
		}
	};
	j = ia::Jugada{};
	const uint8_t t = r.u8();
	if (t > ia::J_ELEGIR) return false;
	j.tipo = (ia::TipoJugada)t;
	j.mano = r.i32();
	j.mano2 = r.i32();
	j.zona = r.i32();
	j.objetivo = r.i32();
	enteros(j.tributos);
	enteros(j.manoExtra);
	j.carta = r.texto();
	j.deMonstruo = r.u8() != 0;
	lugares(j.objs);
	enteros(j.descartes);
	lugares(j.costes);
	return r.ok;
}
inline std::string textoJugada(const ia::Jugada& j)
{
	std::string s = ia::NOMBRE_JUGADA[j.tipo];
	s += " mano=" + std::to_string(j.mano) + " mano2=" + std::to_string(j.mano2) + " zona=" + std::to_string(j.zona) + " obj=" + std::to_string(j.objetivo);
	if (!j.carta.empty()) s += " carta=" + j.carta;
	if (j.deMonstruo) s += " deMonstruo";
	for (int t : j.tributos) s += " trib" + std::to_string(t);
	for (int t : j.manoExtra) s += " extra" + std::to_string(t);
	for (int t : j.descartes) s += " desc" + std::to_string(t);
	for (const ia::Lugar& g : j.objs) s += " o(" + std::to_string(g.lado) + "," + std::to_string(g.donde) + "," + std::to_string(g.i) + ")";
	for (const ia::Lugar& g : j.costes) s += " c(" + std::to_string(g.lado) + "," + std::to_string(g.donde) + "," + std::to_string(g.i) + ")";
	return s;
}
// la jugada vista desde el otro lado (para la vista en espejo del lado 1 en pantalla: los Lugar cambian de lado; las
// zonas e indices de Jugada ya son relativos al que decide)
inline ia::Jugada espejoJugada(ia::Jugada j)
{
	for (ia::Lugar& g : j.objs) g.lado = 1 - g.lado;
	for (ia::Lugar& g : j.costes) g.lado = 1 - g.lado;
	return j;
}

// un mazo que se puede jugar: 40-60 cartas que existen (sin fusiones), hasta 15 de fusion, copias segun el limite
inline bool mazoLegal(const ia::Base& b, const std::vector<std::string>& principal, const std::vector<std::string>& fusion)
{
	if (principal.size() < 40 || principal.size() > 60 || fusion.size() > 15) return false;
	std::map<std::string, int> n;
	for (const std::string& id : principal) {
		const ia::Carta* c = b.carta(id);
		if (!c || c->clase == "fusion") return false;
		n[id]++;
	}
	for (const std::string& id : fusion) {
		const ia::Carta* c = b.carta(id);
		if (!c || c->clase != "fusion") return false;
		n[id]++;
	}
	for (const auto& kv : n)
		if (kv.second > std::min(3, b.carta(kv.first)->limite)) return false;
	return true;
}

// azar local para las semillas: relojes, direcciones de memoria y de red (no hace falta que sea perfecto: el compromiso
// lo protege; solo que el otro no lo adivine)
inline Hash entropiaLocal()
{
	Sha256 s;
	int pila = 0;
	s.agregarTexto("yugioh-classic/entropia").agregarU64(ahoraMs()).agregarU64((uint64_t)time(nullptr));
	s.agregarU64((uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count()).agregarU64((uint64_t)(uintptr_t)&pila);
	for (const Direccion& d : direccionesLocales()) s.agregar(d.ip, 16);
	static uint64_t llamadas = 0;
	s.agregarU64(++llamadas);
	return s.fin();
}

// ------------------------------------------------------------------------------------------------- el duelo en red
enum MensajeRed : uint8_t { M_INICIO = 1, M_MITAD, M_MAZO, M_ORDEN, M_JUGADA, M_REVELA, M_FIN, M_RENDIRSE, M_DESINCRONIZADO, M_AGOTADO, M_REVANCHA };
enum EstadoDuelo { D_CONECTANDO = 0, D_PREPARANDO, D_ELIGE_ORDEN, D_JUGANDO, D_TERMINADO };
enum FinDuelo { FIN_NINGUNO = 0, FIN_NORMAL, FIN_ABANDONO, FIN_RENDICION, FIN_DESINCRONIZADO, FIN_RECHAZADO, FIN_TRAMPA, FIN_TIEMPO };
static const char* const NOMBRE_FIN[] = { "ninguno", "normal", "abandono", "rendicion", "desincronizado", "rechazado", "trampa", "tiempo" };

struct ConfigDuelo {
	std::vector<std::string> principal, fusion;   // el mazo propio (sin barajar)
	std::string nombre;
	bool preguntarSiempre{ false };               // Opciones > "Preguntar antes de activar" = Siempre (toca el motor)
	std::string carpetaRegistro;                  // donde guardar el registro de una desincronizacion ("" = no se guarda)
	Hash entropia{};                              // azar local (cero: entropiaLocal())
	bool irPrimeroSiGano{ true };                 // con elegirOrdenAuto: lo que elige el que gano la moneda
	bool elegirOrdenAuto{ false };                // (pruebas / IA) sin preguntar al jugador
	int agotamientosDerrota{ 3 };                 // relojes agotados en el duelo que son derrota (rama classic-en-linea)
};

class DueloRed {
 public:
	explicit DueloRed(const ia::Base* base) : base_(base), d_(base) {}

	// anfitrion (lado 0): espera al invitado. invitado (lado 1): llama al anfitrion
	void anfitrion(Transporte* t, const ConfigDuelo& c, uint64_t ahora)
	{
		preparar(t, c, 0);
		canal_.escuchar(ahora);
		mandarInicio(ahora);
	}
	void invitado(Transporte* t, const Direccion& a, const ConfigDuelo& c, uint64_t ahora)
	{
		preparar(t, c, 1);
		canal_.conectar(a, ahora);
		mandarInicio(ahora);
	}

	EstadoDuelo estado() const { return estado_; }
	FinDuelo fin() const { return fin_; }
	int ganador() const { return ganador_; }   // 0/1 (lado), 2 empate, -1 ninguno (desincronizacion) o aun en juego
	int yo() const { return yo_; }
	ia::Duelo& duelo() { return d_; }
	const ia::Duelo& duelo() const { return d_; }
	Canal& canal() { return canal_; }
	const Canal& canal() const { return canal_; }
	bool esperandoReconexion() const { return canal_.estado() == C_CAIDO && estado_ != D_TERMINADO; }
	bool meToca() const { return estado_ == D_JUGANDO && d_.ganador < 0 && d_.decide() == yo_; }
	int ganoMoneda() const { return ganoMoneda_; }   // lado que gano la moneda (-1 aun no)
	const std::string& nombreOtro() const { return nombreOtro_; }
	// rama classic-en-linea: relojes agotados de cada lado y la revancha
	int agotados(int lado) const { return agotados_[(size_t)(lado & 1)]; }
	bool revanchaMia() const { return revMia_; }
	bool revanchaDelOtro() const { return revOtro_; }
	int revanchas() const { return revanchas_; }   // cuantas veces se volvio a empezar (el juego lo mira para la moneda)
	// se puede pedir la revancha: termino con un ganador (o empate) y el canal sigue
	bool revanchaPosible() const
	{
		return estado_ == D_TERMINADO && (fin_ == FIN_NORMAL || fin_ == FIN_RENDICION || fin_ == FIN_TIEMPO) && canal_.conectado();
	}
	bool debeElegirOrden() const { return estado_ == D_ELIGE_ORDEN; }
	int empieza() const { return empieza_; }
	uint32_t pasos() const { return paso_; }
	const Hash& semillaComunDuelo() const { return comun_; }
	// la verificacion del mazo del otro (al final) y las revelaciones verificadas durante el duelo
	bool verificado() const { return verificado_; }
	Verificacion verificacion() const { return verificacion_; }
	bool trampaDelOtro() const { return verificacion_ != VER_OK; }
	int revelacionesVerificadas() const { return revelados_; }
	const std::string& motivo() const { return motivo_; }
	const std::string& rutaRegistro() const { return rutaRegistro_; }

	// el que gano la moneda: ir primero o segundo
	void elegirOrden(bool irPrimero, uint64_t ahora)
	{
		if (estado_ != D_ELIGE_ORDEN) return;
		empieza_ = irPrimero ? yo_ : 1 - yo_;
		Escritor w;
		w.u8(M_ORDEN).u8((uint8_t)empieza_);
		canal_.enviar(w.b, ahora);
		estado_ = D_PREPARANDO;
		intentarEmpezar(ahora);
	}

	// una decision local (del jugador o de la IA que juega por el): se aplica y se manda, aceptada o no (el motor la
	// rechaza igual en las dos maquinas). Devuelve si el motor la acepto
	bool jugar(const ia::Jugada& j, uint64_t ahora)
	{
		if (!meToca()) return false;
		aplicarOpciones();
		const bool ok = d_.aplicar(j);
		paso_++;
		const Hash h = hashEstado(d_);
		historial_.push_back("yo   #" + std::to_string(paso_) + (ok ? " ok  " : " no  ") + textoJugada(j) + " | " + hex(h, 8));
		Escritor w;
		w.u8(M_JUGADA).u32(paso_).u8(ok);
		escribirJugada(w, j);
		w.hash(h);
		canal_.enviar(w.b, ahora);
		despuesDeAplicar(ahora);
		return ok;
	}
	// la decision con el mismo respaldo que ia::Turno::jugar (rechazada: pasar o elegir el primero; mano llena: descartar)
	bool jugarConRespaldo(ia::Jugada j, uint64_t ahora, int* jugadasTurno = nullptr, int* rechazadas = nullptr)
	{
		if (!meToca()) return false;
		const bool pregunta = d_.ef.preg.tipo != ia::P_NADA;
		if (jugadasTurno && ++*jugadasTurno > 40 && !pregunta) j = ia::Jugada{};
		bool ok = jugar(j, ahora);
		if (!ok && j.tipo != ia::J_FASE && meToca()) {
			if (rechazadas) ++*rechazadas;
			j = ia::Jugada{};
			if (pregunta) {
				j.tipo = d_.ef.preg.tipo == ia::P_ELEGIR ? ia::J_ELEGIR : ia::J_PASAR;
				j.objetivo = 0;
			}
			ok = jugar(j, ahora);
		}
		if (!ok && d_.fase == ia::F_FINAL && meToca()) {
			ia::Jugada dsc;
			dsc.tipo = ia::J_DESCARTAR;
			dsc.mano = 0;
			ok = jugar(dsc, ahora);
		}
		return ok;
	}

	void rendirse(uint64_t ahora)
	{
		if (estado_ == D_TERMINADO) return;
		Escritor w;
		w.u8(M_RENDIRSE);
		canal_.enviar(w.b, ahora);
		mandarFin(ahora);
		terminar(FIN_RENDICION, 1 - yo_, "te rendiste");
	}
	// cerrar el canal (salir del duelo): si no termino, el otro lo vera como abandono
	void salir(uint64_t ahora) { canal_.cerrar(ahora); }

	// rama classic-en-linea: al que decide se le acabo el reloj del turno. Solo lo llama la maquina de ese jugador (el
	// otro no decide por su reloj: lo recibe); antes de la jugada automatica, asi los dos cuentan lo mismo
	void agotarTiempo(uint64_t ahora)
	{
		if (estado_ != D_JUGANDO || !meToca()) return;
		const int n = ++agotados_[(size_t)yo_];
		Escritor w;
		w.u8(M_AGOTADO).u8((uint8_t)n);
		canal_.enviar(w.b, ahora);
		historial_.push_back("yo   reloj agotado (" + std::to_string(n) + ")");
		if (n >= cfg_.agotamientosDerrota) {
			mandarFin(ahora);
			terminar(FIN_TIEMPO, 1 - yo_, "se agoto tu tiempo " + std::to_string(n) + " veces");
		}
	}
	// rama classic-en-linea: pedir la revancha (con la del otro, se vuelve a empezar en el mismo canal)
	void pedirRevancha(uint64_t ahora)
	{
		if (!revanchaPosible() || revMia_) return;
		revMia_ = true;
		Escritor w;
		w.u8(M_REVANCHA);
		canal_.enviar(w.b, ahora);
		if (revOtro_) reiniciarRevancha(ahora);
	}

	// cada cuadro
	void actualizar(uint64_t ahora)
	{
		canal_.actualizar(ahora);
		std::vector<uint8_t> m;
		while (canal_.recibir(m)) procesar(m, ahora);
		if (estado_ == D_CONECTANDO && canal_.conectado()) estado_ = D_PREPARANDO;
		if (canal_.estado() == C_RECHAZADO && estado_ != D_TERMINADO)
			terminar(FIN_RECHAZADO, -1, std::string("conexion rechazada: ") + NOMBRE_RECHAZO[canal_.rechazo()]);
		if (canal_.estado() == C_PERDIDO && estado_ != D_TERMINADO)   // (rama classic-en-linea: no cuenta para ninguno)
			terminar(FIN_ABANDONO, -1, canal_.adiosRecibido() ? "el rival se fue" : "el rival no volvio en 30 s");
	}

	// (pruebas) rompe el estado local a proposito: la proxima jugada debe detectar la desincronizacion
	void forzarDesincronizacion() { d_.l[(size_t)yo_].lp += 1; }
	// (pruebas) un tramposo: el mazo que juega no sale de sus semillas (lo arregla a mano antes de comprometerlo)
	void hacerTrampaMazo() { trampaMazo_ = true; }
	// (pruebas) un tramposo: revela una carta falsa
	void hacerTrampaRevelacion() { trampaRevela_ = true; }

 private:
	void preparar(Transporte* t, const ConfigDuelo& c, int lado)
	{
		cfg_ = c;
		yo_ = lado;
		const Hash ent = c.entropia == Hash{} ? entropiaLocal() : c.entropia;
		privada_ = Sha256().agregarTexto("privada").agregar(ent).fin();
		mitad_ = Sha256().agregarTexto("mitad").agregar(ent).fin();
		Identidad id;
		id.catalogo = hashCatalogo(*base_);
		id.nombre = c.nombre;
		canal_.usar(t, id, aU64(Sha256().agregarTexto("canal").agregar(ent).fin()));
		estado_ = D_CONECTANDO;
	}
	// el INICIO sale en cuanto haya conexion (el canal lo guarda)
	void mandarInicio(uint64_t ahora)
	{
		Escritor w;
		w.u8(M_INICIO).hash(compromisoDe(mitad_)).hash(compromisoDe(privada_)).u8(cfg_.preguntarSiempre).texto(cfg_.nombre);
		canal_.enviar(w.b, ahora);
	}
	void aplicarOpciones()
	{
		// (las opciones que tocan el motor son las que cada uno mando al empezar; la interfaz no las cambia a mitad)
		d_.preguntarSiempre[(size_t)yo_] = cfg_.preguntarSiempre;
		d_.preguntarSiempre[(size_t)(1 - yo_)] = preguntarOtro_;
	}
	void procesar(const std::vector<uint8_t>& m, uint64_t ahora)
	{
		if (estado_ == D_TERMINADO && fin_ != FIN_NORMAL && fin_ != FIN_RENDICION && fin_ != FIN_TIEMPO) return;
		Lector r(m);
		const uint8_t tipo = r.u8();
		switch (tipo) {
		case M_INICIO: {
			compMitadOtro_ = r.hash();
			compPrivadaOtro_ = r.hash();
			preguntarOtro_ = r.u8() != 0;
			nombreOtro_ = r.texto();
			if (!r.ok || tengoInicio_) return protocolo("INICIO");
			tengoInicio_ = true;
			Escritor w;
			w.u8(M_MITAD).hash(mitad_);
			canal_.enviar(w.b, ahora);
			break;
		}
		case M_MITAD: {
			const Hash mitadOtro = r.hash();
			if (!r.ok || !tengoInicio_ || tengoMitad_) return protocolo("MITAD");
			if (compromisoDe(mitadOtro) != compMitadOtro_) {
				verificacion_ = VER_SEMILLA;
				return terminar(FIN_TRAMPA, yo_, "la mitad de la semilla del rival no es la que comprometio");
			}
			tengoMitad_ = true;
			comun_ = yo_ == 0 ? semillaComun(mitad_, mitadOtro) : semillaComun(mitadOtro, mitad_);
			ganoMoneda_ = comun_[0] & 1;   // la moneda: un bit de la semilla comun (justa: nadie la elige)
			mandarMazo(ahora);
			if (ganoMoneda_ == yo_) {
				estado_ = D_ELIGE_ORDEN;
				if (cfg_.elegirOrdenAuto) elegirOrden(cfg_.irPrimeroSiGano, ahora);
			}
			break;
		}
		case M_MAZO: {
			CompromisoMazo c;
			c.cartas.resize(r.u16());
			for (Hash& h : c.cartas) h = r.hash();
			c.semilla = r.hash();
			c.mazo = r.hash();
			std::vector<std::string> fus(r.u16()), orden(r.u16());
			for (std::string& s : fus) s = r.texto();
			for (std::string& s : orden) s = r.texto();
			if (!r.ok || !tengoMitad_ || tengoMazo_) return protocolo("MAZO");
			if (c.semilla != compPrivadaOtro_ || c.calcularMazo() != c.mazo || c.cartas.size() != orden.size()) {
				verificacion_ = VER_COMPROMISO;
				return terminar(FIN_TRAMPA, yo_, "el compromiso del mazo del rival no cuadra");
			}
			if (!mazoLegal(*base_, orden, fus)) {
				verificacion_ = VER_COMPROMISO;
				return terminar(FIN_TRAMPA, yo_, "el mazo del rival no es legal");
			}
			compOtro_ = c;
			ordenOtro_ = orden;
			fusionOtro_ = fus;
			tengoMazo_ = true;
			intentarEmpezar(ahora);
			break;
		}
		case M_ORDEN: {
			const int e = r.u8();
			if (!r.ok || !tengoMitad_ || ganoMoneda_ == yo_ || empieza_ >= 0 || e > 1) return protocolo("ORDEN");
			empieza_ = e;
			intentarEmpezar(ahora);
			break;
		}
		case M_JUGADA: {
			const uint32_t paso = r.u32();
			const bool okOtro = r.u8() != 0;
			ia::Jugada j;
			const bool leida = leerJugada(r, j);
			const Hash h = r.hash();
			if (!leida || !r.ok || estado_ != D_JUGANDO || paso != paso_ + 1) return protocolo("JUGADA");
			if (d_.decide() != 1 - yo_ || d_.ganador >= 0) return desincronizar("jugada del rival cuando no le tocaba decidir", ahora);
			aplicarOpciones();
			const bool ok = d_.aplicar(j);
			paso_++;
			const Hash mio = hashEstado(d_);
			historial_.push_back("otro #" + std::to_string(paso_) + (ok ? " ok  " : " no  ") + textoJugada(j) + " | " + hex(mio, 8) + " (suyo " + hex(h, 8) + ")");
			if (ok != okOtro) return desincronizar("el motor acepto/rechazo distinto la jugada", ahora);
			if (mio != h) return desincronizar("el hash del estado difiere", ahora, &h);
			despuesDeAplicar(ahora);
			break;
		}
		case M_REVELA: {
			const uint32_t pos = r.u32();
			const std::string id = r.texto();
			const Hash sal = r.hash();
			if (!r.ok || !tengoMazo_) return protocolo("REVELA");
			if (pos >= ordenOtro_.size() || ordenOtro_[pos] != id || !verificarCarta(compOtro_, pos, id, sal)) {
				verificacion_ = VER_REVELACION;
				motivo_ = "una carta revelada por el rival no coincide con su compromiso";
				historial_.push_back("TRAMPA: revelacion " + std::to_string(pos) + " " + id);
			} else
				revelados_++;
			break;
		}
		case M_FIN: {
			const Hash privadaOtro = r.hash();
			const Hash h = r.hash();
			if (!r.ok || !tengoMazo_) return protocolo("FIN");
			const Verificacion v = verificarFinal(compOtro_, compPrivadaOtro_, privadaOtro, comun_, ordenOtro_);
			if (v != VER_OK) {
				verificacion_ = v;
				motivo_ = std::string("trampa del rival: ") + NOMBRE_VERIFICACION[v];
				historial_.push_back("TRAMPA: " + motivo_);
			}
			verificado_ = true;
			if (estado_ == D_TERMINADO && fin_ == FIN_NORMAL && h != hashEstado(d_)) desincronizar("el estado final difiere", ahora, &h);
			break;
		}
		case M_RENDIRSE:
			if (estado_ == D_TERMINADO) break;
			mandarFin(ahora);
			terminar(FIN_RENDICION, yo_, "el rival se rindio");
			break;
		case M_DESINCRONIZADO:
			if (estado_ != D_TERMINADO) desincronizar("el rival detecto una desincronizacion", ahora, nullptr, false);
			break;
		case M_AGOTADO: {   // (rama classic-en-linea) el reloj del rival se agoto: lo cuenta y, si es el ultimo, gana
			const int n = r.u8();
			if (!r.ok) return protocolo("AGOTADO");
			if (estado_ != D_JUGANDO) break;
			const int k = ++agotados_[(size_t)(1 - yo_)];
			historial_.push_back("otro reloj agotado (" + std::to_string(k) + ")");
			if (k != n) return desincronizar("la cuenta de relojes agotados difiere", ahora);
			if (k >= cfg_.agotamientosDerrota) {
				mandarFin(ahora);
				terminar(FIN_TIEMPO, yo_, "se agoto el tiempo del rival " + std::to_string(k) + " veces");
			}
			break;
		}
		case M_REVANCHA:   // (rama classic-en-linea)
			if (!revanchaPosible() || revOtro_) return;
			revOtro_ = true;
			if (revMia_) reiniciarRevancha(ahora);
			break;
		default:
			protocolo("tipo desconocido");
		}
	}
	void mandarMazo(uint64_t ahora)
	{
		orden_ = ordenMazo(cfg_.principal, privada_, comun_);
		if (trampaMazo_) std::sort(orden_.begin(), orden_.end());   // (prueba: un mazo "arreglado")
		const CompromisoMazo c = comprometer(orden_, privada_);
		Escritor w;
		w.u8(M_MAZO).u16((uint16_t)c.cartas.size());
		for (const Hash& h : c.cartas) w.hash(h);
		w.hash(c.semilla).hash(c.mazo);
		w.u16((uint16_t)cfg_.fusion.size()).u16((uint16_t)orden_.size());
		for (const std::string& s : cfg_.fusion) w.texto(s);
		for (const std::string& s : orden_) w.texto(s);   // (version 1: todo conocido)
		canal_.enviar(w.b, ahora);
		cimaMia_ = orden_.size();
	}
	void intentarEmpezar(uint64_t ahora)
	{
		if (estado_ == D_JUGANDO || estado_ == D_TERMINADO || !tengoMazo_ || empieza_ < 0 || orden_.empty()) return;
		const std::vector<std::string>& p0 = yo_ == 0 ? orden_ : ordenOtro_;
		const std::vector<std::string>& p1 = yo_ == 0 ? ordenOtro_ : orden_;
		const std::vector<std::string>& f0 = yo_ == 0 ? cfg_.fusion : fusionOtro_;
		const std::vector<std::string>& f1 = yo_ == 0 ? fusionOtro_ : cfg_.fusion;
		d_ = ia::Duelo(base_);
		d_.ladoVista = yo_;   // (rama classic-en-linea: las cartas que mira este jugador se le muestran a el)
		aplicarOpciones();
		uint32_t semilla = (uint32_t)aU64(Sha256().agregarTexto("duelo").agregar(comun_).fin());
		d_.empezar(p0, f0, p1, f1, semilla, empieza_, false);
		estado_ = D_JUGANDO;
		paso_ = 0;
		historial_.push_back("empieza el lado " + std::to_string(empieza_) + ", estado " + hex(hashEstado(d_), 8));
		revelarRobos(ahora);
	}
	// despues de cada jugada (propia o del rival): robos para revelar, fin del duelo
	void despuesDeAplicar(uint64_t ahora)
	{
		revelarRobos(ahora);
		if (d_.ganador >= 0 && estado_ != D_TERMINADO) {
			mandarFin(ahora);
			terminar(FIN_NORMAL, d_.ganador, "fin del duelo");
		}
	}
	// las cartas que salieron de arriba de mi mazo se revelan con su sal (mientras el mazo siga siendo el comprometido sin
	// las de arriba: una busqueda, un barajado o una carta devuelta al mazo cortan esto y queda la verificacion final)
	void revelarRobos(uint64_t ahora)
	{
		if (!prefijoMio_) return;
		const std::vector<std::string>& mz = d_.l[(size_t)yo_].mazo;
		if (mz.size() > cimaMia_ || !std::equal(mz.begin(), mz.end(), orden_.begin())) {
			prefijoMio_ = false;
			return;
		}
		for (size_t p = mz.size(); p < cimaMia_; p++) {
			Escritor w;
			w.u8(M_REVELA).u32((uint32_t)p).texto(trampaRevela_ ? std::string("dark_magician") : orden_[p]).hash(salDe(privada_, (uint32_t)p));
			canal_.enviar(w.b, ahora);
		}
		cimaMia_ = mz.size();
	}
	// rama classic-en-linea: las dos revanchas pedidas: todo de nuevo en el mismo canal (semillas nuevas: moneda y
	// barajado nuevos). El anfitrion sigue siendo el lado 0
	void reiniciarRevancha(uint64_t ahora)
	{
		const Hash ent = entropiaLocal();
		privada_ = Sha256().agregarTexto("privada").agregar(ent).fin();
		mitad_ = Sha256().agregarTexto("mitad").agregar(ent).fin();
		comun_ = compMitadOtro_ = compPrivadaOtro_ = Hash{};
		tengoInicio_ = tengoMitad_ = tengoMazo_ = finMandado_ = verificado_ = false;
		prefijoMio_ = true;
		verificacion_ = VER_OK;
		compOtro_ = CompromisoMazo{};
		orden_.clear();
		ordenOtro_.clear();
		fusionOtro_.clear();
		cimaMia_ = 0;
		paso_ = 0;
		ganador_ = ganoMoneda_ = empieza_ = -1;
		revelados_ = 0;
		agotados_ = { { 0, 0 } };
		revMia_ = revOtro_ = false;
		revanchas_++;
		fin_ = FIN_NINGUNO;
		motivo_.clear();
		historial_.clear();
		d_ = ia::Duelo(base_);
		estado_ = D_PREPARANDO;
		mandarInicio(ahora);
	}
	void mandarFin(uint64_t ahora)
	{
		if (finMandado_ || orden_.empty()) return;
		finMandado_ = true;
		Escritor w;
		w.u8(M_FIN).hash(privada_).hash(hashEstado(d_));
		canal_.enviar(w.b, ahora);
	}
	void terminar(FinDuelo f, int ganador, const std::string& motivo)
	{
		estado_ = D_TERMINADO;
		fin_ = f;
		ganador_ = ganador;
		if (motivo_.empty() || f != FIN_NORMAL) motivo_ = motivo;
	}
	void protocolo(const char* que)
	{
		historial_.push_back(std::string("mensaje fuera de lugar: ") + que);
		if (estado_ != D_TERMINADO) terminar(FIN_DESINCRONIZADO, -1, std::string("protocolo: ") + que);
	}
	void desincronizar(const std::string& porque, uint64_t ahora, const Hash* suyo = nullptr, bool avisar = true)
	{
		if (avisar) {
			Escritor w;
			w.u8(M_DESINCRONIZADO).u32(paso_).hash(hashEstado(d_));
			canal_.enviar(w.b, ahora);
		}
		terminar(FIN_DESINCRONIZADO, -1, "desincronizacion: " + porque);
		guardarRegistro(porque, suyo);
	}
	void guardarRegistro(const std::string& porque, const Hash* suyo)
	{
		if (cfg_.carpetaRegistro.empty()) return;
		char nombre[96];
		snprintf(nombre, sizeof nombre, "desincronizacion_%llu_lado%d.txt", (unsigned long long)(ahoraMs() % 100000000ull), yo_);
		rutaRegistro_ = cfg_.carpetaRegistro + nombre;
		FILE* f = fopen(rutaRegistro_.c_str(), "w");
		if (!f) {
			rutaRegistro_.clear();
			return;
		}
		fprintf(f, "Yugioh Classic Remaster - registro de desincronizacion del duelo en linea\n");
		fprintf(f, "motivo: %s\nversion: %s, protocolo %d, catalogo %s\n", porque.c_str(), VERSION_JUEGO, VERSION_PROTOCOLO, hex(hashCatalogo(*base_)).c_str());
		fprintf(f, "yo: lado %d (%s), rival: %s\nsemilla comun: %s\nempieza: %d\npaso: %u\n", yo_, cfg_.nombre.c_str(), nombreOtro_.c_str(), hex(comun_).c_str(), empieza_, paso_);
		fprintf(f, "hash mio: %s\nhash suyo: %s\n", hex(hashEstado(d_)).c_str(), suyo ? hex(*suyo).c_str() : "-");
		fprintf(f, "turno %d, activo %d, fase %s, LP %d / %d\n", d_.turno, d_.activo, ia::NOMBRE_FASE[d_.fase], d_.l[0].lp, d_.l[1].lp);
		for (int a = 0; a < 2; a++) {
			const ia::Lado& x = d_.l[(size_t)a];
			fprintf(f, "lado %d: mazo %d, mano:", a, (int)x.mazo.size());
			for (const std::string& s : x.mano) fprintf(f, " %s", s.c_str());
			fprintf(f, "\n  campo:");
			for (const ia::Monstruo& m : x.m) fprintf(f, " [%s %d/%d]", m.id.c_str(), m.atkEf, m.defEf);
			for (const ia::MagiaTrampa& c : x.mt) fprintf(f, " {%s}", c.id.c_str());
			fprintf(f, "\n  cementerio:");
			for (const std::string& s : x.cementerio) fprintf(f, " %s", s.c_str());
			fprintf(f, "\n");
		}
		fprintf(f, "orden propio:");
		for (const std::string& s : orden_) fprintf(f, " %s", s.c_str());
		fprintf(f, "\norden del rival:");
		for (const std::string& s : ordenOtro_) fprintf(f, " %s", s.c_str());
		fprintf(f, "\njugadas:\n");
		for (const std::string& s : historial_) fprintf(f, "  %s\n", s.c_str());
		fclose(f);
	}

	const ia::Base* base_;
	ia::Duelo d_;
	Canal canal_;
	ConfigDuelo cfg_;
	int yo_{ 0 }, ganador_{ -1 }, ganoMoneda_{ -1 }, empieza_{ -1 }, revelados_{ 0 };
	EstadoDuelo estado_{ D_CONECTANDO };
	FinDuelo fin_{ FIN_NINGUNO };
	Hash privada_{}, mitad_{}, comun_{}, compMitadOtro_{}, compPrivadaOtro_{};
	bool preguntarOtro_{ false }, tengoInicio_{ false }, tengoMitad_{ false }, tengoMazo_{ false }, finMandado_{ false };
	bool verificado_{ false }, prefijoMio_{ true }, trampaMazo_{ false }, trampaRevela_{ false };
	std::array<int, 2> agotados_{ { 0, 0 } };   // (rama classic-en-linea) relojes agotados por lado
	bool revMia_{ false }, revOtro_{ false };
	int revanchas_{ 0 };
	Verificacion verificacion_{ VER_OK };
	CompromisoMazo compOtro_;
	std::vector<std::string> orden_, ordenOtro_, fusionOtro_;
	size_t cimaMia_{ 0 };
	uint32_t paso_{ 0 };
	std::string nombreOtro_, motivo_, rutaRegistro_;
	std::vector<std::string> historial_;
};

// ------------------------------------------------------------------------------------------------- la IA como mano
// La IA decide por un lado del duelo en red (pruebas, y "dejar que la IA juegue por mi"): no aplica nada, solo decide
struct ManoIA {
	ia::Turno t;
	int rechazadas{ 0 };
	ManoIA(DueloRed& r, const ia::Personalidad& p, int nivel, uint32_t semilla) : t(&r.duelo(), r.yo(), p, nivel, semilla) {}
	bool paso(DueloRed& r, uint64_t ahora)
	{
		if (!r.meToca()) return false;
		ia::Duelo& d = r.duelo();
		if (t.turnoVisto != d.turno) {
			t.turnoVisto = d.turno;
			t.jugadasEsteTurno = 0;
			if (d.activo == t.lado) t.cerebro.alEmpezarTurno(d, t.lado);
		}
		return r.jugarConRespaldo(t.cerebro.decidir(d), ahora, &t.jugadasEsteTurno, &rechazadas);
	}
};

// ------------------------------------------------------------------------------------------------- la entrada
// Duelo en red con su socket: anfitrion (escucha en "puerto"; 0 = uno libre) o invitado (llama a "anfitrionDir").
// El juego llama actualizar(ahoraMs()) cada cuadro y juega con jugar() cuando meToca()
struct SesionRed {
	SocketUdp socket;
	TransporteUdp transporte{ &socket };
	DueloRed duelo;
	explicit SesionRed(const ia::Base* b) : duelo(b) {}
	void actualizar() { duelo.actualizar(ahoraMs()); }
};
inline std::unique_ptr<SesionRed> iniciarDueloRed(const ia::Base* base, const ConfigDuelo& cfg, bool comoAnfitrion, const Direccion& anfitrionDir = {},
                                                 uint16_t puerto = 0, bool v6 = true)
{
	std::unique_ptr<SesionRed> s(new SesionRed(base));
	if (!s->socket.abrir(puerto, v6)) return nullptr;
	if (comoAnfitrion) s->duelo.anfitrion(&s->transporte, cfg, ahoraMs());
	else s->duelo.invitado(&s->transporte, anfitrionDir, cfg, ahoraMs());
	return s;
}

}  // namespace red
