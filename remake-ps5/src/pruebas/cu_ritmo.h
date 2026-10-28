// Rama classic-md-ritmo: casos CU-FT-ANIM-* del ritmo de Master Duel (md_ritmo.h), en el juego sin GPU de cu_juego.h.
// Se incluye en prueba_front_total.cpp (despues de inyectar / nuevoDuelo); modo "ritmo" (y dentro de "extra").
//  - CU-FT-ANIM-RIVAL-COLOCA / -INVOCA / -MAGIA: la carta del rival sale de su mano (la elige: se levanta) y recorre su
//    trayecto (posiciones intermedias, la zona vacia mientras vuela) durante varios cuadros antes de quedar en su zona.
//  - CU-FT-ANIM-RIVAL-ROBO: el robo del rival vuela del mazo a su mano.
//  - CU-FT-ANIM-ACTIVA-SWORDS / -LUZ / -CAMPO: toda activacion (Swords of Revealing Light desde la mano, Light of
//    Intervention colocada, una magia de campo) tiene su presentacion visible un tiempo minimo (CardShow de MD).
//  - CU-FT-ANIM-CADENA: una cadena de 2 muestra los dos eslabones (numeros 1 y 2) y resuelve en orden.
//  - CU-FT-ANIM-ORDEN: el suceso siguiente no se presenta mientras la carta todavia vuela.
//  - CU-FT-ANIM-RAPIDA: con la velocidad Rapida el mismo viaje dura menos cuadros.
//  - CU-FT-ANIM-SWORDS-CONTADOR / -TURNOS: el contador 3 -> 2 -> 1 sobre Swords y que el motor la mande al cementerio
//    en la fase final del 3er turno del rival.
#pragma once

struct MedidaRitmo {
	int cuadros{ 0 }, enViaje{ 0 }, intermedios{ 0 }, eligiendo{ 0 }, conActivacion{ 0 }, maxEslabon{ 0 };
	bool zonaVaciaEnVuelo{ true }, ordenOk{ true }, llego{ false };
	std::vector<int> eslabones;
};

// pasa las preguntas del duelo (el rival siempre; el jugador, salvo "responder": su jugada una vez)
static void contestarRitmo(Juego& g, ia::Jugada* responder, bool& respondio)
{
	ia::Duelo& d = g.dueloIA;
	for (int k = 0; k < 4 && d.ef.preg.tipo != ia::P_NADA && g.sucesosVistos >= d.sucesos.size() && g.esperaEfecto <= 0.0f; k++) {
		ia::Jugada j;
		j.tipo = ia::J_PASAR;
		if (d.ef.preg.lado == 0 && responder && !respondio && d.ef.preg.tipo == ia::P_RESPUESTA) {
			j = *responder;
			respondio = true;
		}
		if (!d.aplicar(j)) break;
		g.ctl.estado = control::NAVEGAR;
		g.ctl.enPregunta = g.ctl.armando = g.ctl.eleccionDuelo = false;
	}
}

// corre los cuadros de la escena y mide la llegada a zonaMesa (-1: no se mide) y las activaciones
static MedidaRitmo medirRitmo(Juego& g, int zonaMesa, const std::string& carta, ia::Jugada* responder = nullptr, float tope = 20.0f)
{
	ia::Duelo& d = g.dueloIA;
	MedidaRitmo r;
	bool respondio = false;
	size_t idxLlegada = (size_t)-1;
	for (size_t k = g.sucesosVistos; k < d.sucesos.size() && idxLlegada == (size_t)-1; k++)
		if ((d.sucesos[k].tipo == ia::S_INVOCA || d.sucesos[k].tipo == ia::S_COLOCA || d.sucesos[k].tipo == ia::S_COLOCA_MT || d.sucesos[k].tipo == ia::S_ACTIVA_MT) &&
		    d.sucesos[k].carta == carta)
			idxLlegada = k;
	for (float t = 0.0f; t < tope; t += g.frameTimer) {
		contestarRitmo(g, responder, respondio);
		g.cuadro(0);
		r.cuadros++;
		const ritmo::Estado& e = ritmo::estado();
		bool viaja = false;
		for (const ritmo::Viaje& v : e.viajes) {
			if (!v.llegada || v.zona != zonaMesa) continue;
			viaja = true;
			r.enViaje++;
			if (v.eligiendo()) r.eligiendo++;
			else if (v.u() > 0.02f && v.u() < 0.98f) r.intermedios++;
			const control::Celda& c = g.ctl.zonas[(size_t)zonaMesa];
			if (!c.vacia() && c.carta == v.carta) r.zonaVaciaEnVuelo = false;
		}
		if (viaja && idxLlegada != (size_t)-1 && g.sucesosVistos > idxLlegada + 1) r.ordenOk = false;
		if (!e.acts.empty()) r.conActivacion++;
		for (const ritmo::Activacion& a : e.acts)
			if (a.eslabon > 0 && std::find(r.eslabones.begin(), r.eslabones.end(), a.eslabon) == r.eslabones.end()) r.eslabones.push_back(a.eslabon);
		r.maxEslabon = std::max(r.maxEslabon, e.maxEslabon());
		const bool alDia = g.sucesosVistos >= d.sucesos.size() && !e.ocupado() && g.esperaEfecto <= 0.0f && !g.hayAtaque() && !g.ctl.robo.activo;
		if (alDia && d.ef.preg.tipo == ia::P_NADA && t > 0.3f) break;
	}
	if (zonaMesa >= 0) r.llego = !g.ctl.zonas[(size_t)zonaMesa].vacia() && g.ctl.zonas[(size_t)zonaMesa].carta == carta;
	return r;
}

// la mesa de la escena: turno 4 del rival, su fase principal 1, sin su IA (la escena juega por el)
static cub::Mesa mesaRitmo(Juego& g)
{
	cub::Mesa m(&g.baseIA, ia::F_PRINCIPAL1);
	m.d.turno = 4;
	m.d.activo = 1;
	return m;
}
static void ponerRitmo(Bot& b, const ia::Duelo& d)
{
	inyectar(b, d);
	b.g.turnoIA = ia::Turno{};
	ritmo::estado().limpiar();
	b.g.ritmoTomarFoto();
}

// ---- rama classic-md-combate: el combate de Master Duel (md_ritmo.h T_ATAQUE_*, T_ROMPE; md_ritmo_juego.h ritmoDestruidaCombate)
//  - CU-FT-ANIM-COMBATE-VIAJE: la carta destruida en combate viaja de su zona a su cementerio (CardMoveField2GraveNormal)
//    durante varios cuadros, sale tras la pausa del golpe (impacto 0.45 + attackExecPostTime 0.25) y queda en la pila.
//  - CU-FT-ANIM-COMBATE-BAJO / -ALTO: el viaje del ataque dura 0.45 s ("Low") o 0.65 s ("High": ATK >= 2500).
//  - CU-FT-ANIM-COMBATE-AMBOS: si se destruyen los dos (mismo ATK) viajan los dos.
//  - CU-FT-ANIM-COMBATE-DATOS: los BezierMotion del ataque de MD estan en movimientos.txt (StrikeLight00 0.45, 02 0.65).
struct MedidaCombate {
	bool ok{ false }, alto{ false }, alCementerio{ false }, zonaVacia{ true };
	int viajes{ 0 }, intermedios{ 0 }, cuadrosHastaImpacto{ 0 };
	float tRompe{ -1.0f };
};
static MedidaCombate medirCombate(Bot& b, const std::string& atacante, const std::string& defensor)
{
	Juego& g = b.g;
	ia::Duelo& d = g.dueloIA;
	cub::Mesa m(&g.baseIA, ia::F_BATALLA);
	m.d.turno = 4;
	m.d.activo = 1;
	const int za = m.mon(1, atacante), zo = m.mon(0, defensor);
	ponerRitmo(b, m.d);
	ia::Jugada j;
	j.tipo = ia::J_ATACAR;
	j.zona = za;
	j.objetivo = zo;
	MedidaCombate r;
	r.ok = za >= 0 && zo >= 0 && d.aplicar(j);
	const int zMesaO = Juego::zonaMesaIA(0, zo), zMesaA = Juego::zonaMesaIA(1, za);
	std::vector<int> series;
	bool respondio = false, vio = false;
	for (float t = 0.0f; t < 8.0f; t += g.frameTimer) {
		contestarRitmo(g, nullptr, respondio);
		g.cuadro(0);
		if (g.hayAtaque()) {
			if (!vio) r.alto = g.ataqueAltoMD;
			vio = true;
			if (g.tAtaque < ritmo::T_ATAQUE_BAJO) r.cuadrosHastaImpacto++;
		}
		for (const ritmo::Viaje& v : ritmo::estado().viajes) {
			if (v.llegada || v.origen != ritmo::O_CEMENTERIO || (v.zona != zMesaO && v.zona != zMesaA)) continue;
			if (std::find(series.begin(), series.end(), v.serie) == series.end()) {
				series.push_back(v.serie);
				if (r.tRompe < 0.0f) r.tRompe = g.tAtaque;
			}
			if (v.u() > 0.02f && v.u() < 0.98f) r.intermedios++;
			const control::Celda& c = g.ctl.zonas[(size_t)v.zona];
			if (!c.vacia() && c.carta == v.carta) r.zonaVacia = false;
		}
		if (vio && !g.hayAtaque() && !ritmo::estado().ocupado() && g.sucesosVistos >= d.sucesos.size() && g.esperaEfecto <= 0.0f && d.ef.preg.tipo == ia::P_NADA) break;
	}
	r.viajes = (int)series.size();
	r.alCementerio = std::find(g.ctl.cementerioJ.begin(), g.ctl.cementerioJ.end(), defensor) != g.ctl.cementerioJ.end();
	return r;
}
static void combateMD(Bot& b, int minimo)
{
	Juego& g = b.g;
	const float dt = g.frameTimer;
	const MedidaCombate bajo = medirCombate(b, "vorse_raider", "celtic_guardian");   // (1900 contra 1400: Low)
	b.caso("CU-FT-ANIM-COMBATE-VIAJE", bajo.ok, "el ataque de Vorse Raider a Celtic Guardian no se pudo aplicar");
	b.caso("CU-FT-ANIM-COMBATE-VIAJE", bajo.viajes == 1 && bajo.intermedios >= minimo, "la destruida no viajo al cementerio (%d viajes, %d cuadros intermedios, minimo %d)", bajo.viajes,
	       bajo.intermedios, minimo);
	b.caso("CU-FT-ANIM-COMBATE-VIAJE", bajo.zonaVacia, "la destruida seguia en su zona mientras viajaba");
	b.caso("CU-FT-ANIM-COMBATE-VIAJE", bajo.tRompe >= ritmo::T_ROMPE - 0.01f && bajo.tRompe <= ritmo::T_ROMPE + 3.0f * dt,
	       "la destruida salio a los %.2f s del ataque (MD: impacto 0.45 + pausa 0.25 = %.2f)", bajo.tRompe, ritmo::T_ROMPE);
	b.caso("CU-FT-ANIM-COMBATE-VIAJE", bajo.alCementerio, "Celtic Guardian no quedo en el cementerio del jugador");
	const float tBajo = (float)bajo.cuadrosHastaImpacto * dt;
	b.caso("CU-FT-ANIM-COMBATE-BAJO", !bajo.alto && fabsf(tBajo - ritmo::T_ATAQUE_BAJO) <= 2.5f * dt, "ATK 1900: viaje hasta el impacto %.2f s, alto %d (MD Low: 0.45)", tBajo, (int)bajo.alto);

	const MedidaCombate alto = medirCombate(b, "blue_eyes", "celtic_guardian");   // (3000: High)
	const float tAlto = (float)alto.cuadrosHastaImpacto * dt;
	b.caso("CU-FT-ANIM-COMBATE-ALTO", alto.ok && alto.alto && fabsf(tAlto - ritmo::T_ATAQUE_ALTO) <= 2.5f * dt,
	       "ATK 3000: viaje hasta el impacto %.2f s, alto %d, aplicado %d (MD High: 0.65)", tAlto, (int)alto.alto, (int)alto.ok);
	b.caso("CU-FT-ANIM-COMBATE-ALTO", alto.viajes == 1 && alto.alCementerio, "con el ataque High la destruida no viajo al cementerio (%d viajes)", alto.viajes);
	const MedidaCombate borde = medirCombate(b, "summoned_skull", "celtic_guardian");   // (2500 justo: High)
	b.caso("CU-FT-ANIM-COMBATE-ALTO", borde.ok && borde.alto, "ATK 2500 (Summoned Skull) no hizo el viaje High (MD: GetAttackLevel >= 2500 = Largest)");

	const MedidaCombate ambos = medirCombate(b, "celtic_guardian", "celtic_guardian");   // (1400 contra 1400: los dos)
	b.caso("CU-FT-ANIM-COMBATE-AMBOS", ambos.ok && ambos.viajes == 2 && ambos.zonaVacia, "con los dos destruidos viajaron %d (aplicado %d, zonas vacias %d)", ambos.viajes, (int)ambos.ok,
	       (int)ambos.zonaVacia);

	const ritmo::Movimiento* lo = ritmo::movimiento("StrikeLight00");
	const ritmo::Movimiento* hi = ritmo::movimiento("StrikeLight02");
	if (ritmo::movimiento("CardMoveField2GraveNormal")) {   // (con los datos de MD instalados)
		b.caso("CU-FT-ANIM-COMBATE-DATOS", lo && hi && fabsf(lo->tiempo - ritmo::T_ATAQUE_BAJO) < 0.01f && fabsf(hi->tiempo - ritmo::T_ATAQUE_ALTO) < 0.01f && lo->acel.hay(),
		       "faltan los BezierMotion del ataque en movimientos.txt (exportar_movimientos.py con ataque_bezier.json)");
	}
}

static void ritmoMD(Bot& b)
{
	Juego& g = b.g;
	ia::Duelo& d = g.dueloIA;
	if (g.ctl.vista.pantalla != VD::DUELO && !nuevoDuelo(b)) {
		b.caso("CU-FT-ANIM-INICIO", false, "no se pudo empezar el duelo para las pruebas del ritmo");
		return;
	}
	b.rivalPorBot = true;
	b.grabar = false;
	b.duelo = "ritmo-md";
	const int pregunta0 = opciones::preguntar();
	opciones::preguntar() = opciones::PREGUNTAR_SIEMPRE;
	velocidad::poner(velocidad::NORMAL);
	g.leerVelocidad();
	const int minimo = (int)(0.3f / g.frameTimer);   // (cuadros: al menos 0.3 s de trayecto)

	// el rival coloca / invoca un monstruo de su mano
	auto llegaMonstruo = [&](const char* caso, ia::TipoJugada tipo, const std::string& id) {
		cub::Mesa m = mesaRitmo(g);
		const int k = m.mano(1, id);
		m.mano(1, "celtic_guardian");
		ponerRitmo(b, m.d);
		ia::Jugada j;
		j.tipo = tipo;
		j.mano = k;
		j.zona = 2;
		const bool ok = d.aplicar(j);
		const int z = Juego::zonaMesaIA(1, 2);
		const MedidaRitmo r = medirRitmo(g, z, id);
		b.caso(caso, ok, "la jugada del rival no se pudo aplicar");
		b.caso(caso, r.eligiendo > 0, "el rival no eligio la carta de su mano (no se levanto)");
		const int min = (int)((float)minimo / velocidad::factor());   // (con la velocidad elegida, proporcionalmente menos)
		b.caso(caso, r.intermedios >= min, "la carta no recorrio su trayecto: %d cuadros intermedios (minimo %d)", r.intermedios, min);
		b.caso(caso, r.zonaVaciaEnVuelo, "la carta ya estaba en su zona mientras volaba (aparece de la nada)");
		b.caso(caso, r.llego, "la carta no quedo en su zona al terminar");
		b.caso("CU-FT-ANIM-ORDEN", r.ordenOk, "%s: se presento el suceso siguiente con la carta todavia en el aire", caso);
		return r;
	};
	const MedidaRitmo coloca = llegaMonstruo("CU-FT-ANIM-RIVAL-COLOCA", ia::J_COLOCAR, "giant_soldier_of_stone");
	b.caso("CU-FT-ANIM-RIVAL-COLOCA", g.ctl.zonas[(size_t)Juego::zonaMesaIA(1, 2)].bocaAbajo, "la carta colocada no quedo boca abajo");
	const MedidaRitmo invoca = llegaMonstruo("CU-FT-ANIM-RIVAL-INVOCA", ia::J_INVOCAR, "vorse_raider");
	b.caso("CU-FT-ANIM-RIVAL-INVOCA", !g.ctl.zonas[(size_t)Juego::zonaMesaIA(1, 2)].bocaAbajo, "la carta invocada no quedo boca arriba");

	// con la velocidad Rapida el mismo viaje es mas corto
	velocidad::poner(velocidad::RAPIDA);
	g.leerVelocidad();
	const MedidaRitmo rapida = llegaMonstruo("CU-FT-ANIM-RAPIDA", ia::J_INVOCAR, "vorse_raider");
	b.caso("CU-FT-ANIM-RAPIDA", rapida.enViaje < invoca.enViaje && rapida.enViaje > 0, "con la velocidad Rapida el viaje no se acorto (%d cuadros contra %d en Normal)", rapida.enViaje,
	       invoca.enViaje);
	velocidad::poner(velocidad::NORMAL);
	g.leerVelocidad();
	(void)coloca;

	// el rival coloca una magia/trampa
	{
		cub::Mesa m = mesaRitmo(g);
		const int k = m.mano(1, "trap_hole");
		ponerRitmo(b, m.d);
		ia::Jugada j;
		j.tipo = ia::J_COLOCAR_MT;
		j.mano = k;
		j.zona = 1;
		const bool ok = d.aplicar(j);
		const MedidaRitmo r = medirRitmo(g, Juego::zonaMesaIA(1, 1, true), "trap_hole");
		b.caso("CU-FT-ANIM-RIVAL-MAGIA", ok && r.intermedios >= minimo && r.zonaVaciaEnVuelo && r.llego, "la trampa colocada no viajo de la mano a su zona (ok %d, intermedios %d, zona vacia %d, llego %d)",
		       (int)ok, r.intermedios, (int)r.zonaVaciaEnVuelo, (int)r.llego);
	}

	// el robo del rival
	{
		cub::Mesa m = mesaRitmo(g);
		m.d.activo = 0;
		m.d.fase = ia::F_FINAL;
		ponerRitmo(b, m.d);
		const int serie0 = g.ctl.robo.serie;
		ia::Jugada j;
		j.tipo = ia::J_FASE;
		d.aplicar(j);   // (fin del turno del jugador: el rival roba)
		int cuadrosRobo = 0;
		for (float t = 0.0f; t < 6.0f; t += g.frameTimer) {
			bool r0 = false;
			contestarRitmo(g, nullptr, r0);
			g.cuadro(0);
			cuadrosRobo += g.ctl.robo.activo && g.ctl.robo.rival ? 1 : 0;
		}
		b.caso("CU-FT-ANIM-RIVAL-ROBO", g.ctl.robo.serie > serie0 && cuadrosRobo >= (int)(0.3f / g.frameTimer), "el robo del rival no voló del mazo a su mano (%d cuadros)", cuadrosRobo);
	}

	// Swords of Revealing Light desde la mano del rival: viaja, se presenta (CardShow) y queda con su contador de 3 turnos
	const int zSwords = Juego::zonaMesaIA(1, 0, true);
	{
		cub::Mesa m = mesaRitmo(g);
		const int k = m.mano(1, "swords_of_revealing_light");
		m.mon(0, "mystical_elf", true, true);
		ponerRitmo(b, m.d);
		const bool ok = d.aplicar(ia::jugadaActivar({ 1, ia::EN_MANO, k }));
		const MedidaRitmo r = medirRitmo(g, zSwords, "swords_of_revealing_light");
		const float tAct = (float)r.conActivacion * g.frameTimer;
		b.caso("CU-FT-ANIM-ACTIVA-SWORDS", ok, "no se pudo activar Swords of Revealing Light");
		b.caso("CU-FT-ANIM-ACTIVA-SWORDS", r.intermedios >= minimo && r.zonaVaciaEnVuelo, "Swords no viajo de la mano a su zona (%d cuadros intermedios)", r.intermedios);
		b.caso("CU-FT-ANIM-ACTIVA-SWORDS", tAct >= ritmo::T_ACTIVA * 0.9f, "la activacion de Swords se vio %.2f s (minimo %.2f)", tAct, ritmo::T_ACTIVA * 0.9f);
		int n = 0;
		bool turnos = false;
		const bool hay = g.contadorZonaMD(zSwords, n, turnos);
		b.caso("CU-FT-ANIM-SWORDS-CONTADOR", hay && turnos && n == 3, "Swords recien activada no muestra 3 turnos (hay %d, turnos %d, n %d)", (int)hay, (int)turnos, n);
	}
	// el motor: Swords se va en la fase final del 3er turno del rival; el contador baja 3 -> 2 -> 1
	{
		cub::Mesa m(&g.baseIA, ia::F_PRINCIPAL1);
		m.d.turno = 4;
		m.d.activo = 1;
		const int k = m.mano(1, "swords_of_revealing_light");
		ia::Duelo x = m.d;
		x.aplicar(ia::jugadaActivar({ 1, ia::EN_MANO, k }));
		auto pasar = [&] {
			for (int q = 0; q < 10 && x.ef.preg.tipo != ia::P_NADA; q++) {
				ia::Jugada p;
				p.tipo = ia::J_PASAR;
				x.aplicar(p);
			}
		};
		pasar();
		std::vector<int> vistos;   // (lo que muestra el contador al empezar cada turno del jugador)
		int turnosJugador = 0, seFueEn = -1;
		for (int paso = 0; paso < 200 && seFueEn < 0 && x.ganador < 0; paso++) {
			const int turnoAntes = x.turno;
			ia::Jugada f;
			f.tipo = ia::J_FASE;
			if (!x.aplicar(f)) pasar();
			pasar();
			if (x.turno != turnoAntes && x.activo == 0) {
				turnosJugador++;
				if (x.l[1].mt[0].id == "swords_of_revealing_light") vistos.push_back(3 - x.l[1].mt[0].contadores);
				else seFueEn = turnosJugador;
			}
		}
		std::string v;
		for (int n : vistos) v += std::to_string(n) + " ";
		// (cuenta los turnos del rival de su duenno: al empezar cada uno quedan 3, 2, 1; se va en la fase final del 3ro)
		b.caso("CU-FT-ANIM-SWORDS-TURNOS", seFueEn == 4 && vistos == std::vector<int>({ 3, 2, 1 }),
		       "Swords no duro 3 turnos del rival de su duenno (se fue al empezar su turno %d; contador al empezar cada turno: %s)", seFueEn, v.c_str());
	}

	// Light of Intervention colocada antes: se da vuelta y se presenta
	{
		cub::Mesa m = mesaRitmo(g);
		const int zl = m.colocar(1, "light_of_intervention", 1);
		ponerRitmo(b, m.d);
		const bool ok = d.aplicar(ia::jugadaActivar({ 1, ia::EN_MT, zl }));
		const MedidaRitmo r = medirRitmo(g, -1, "light_of_intervention");
		const float tAct = (float)r.conActivacion * g.frameTimer;
		b.caso("CU-FT-ANIM-ACTIVA-LUZ", ok && tAct >= ritmo::T_ACTIVA * 0.9f, "Light of Intervention: activada %d, presentacion %.2f s (minimo %.2f)", (int)ok, tAct, ritmo::T_ACTIVA * 0.9f);
		const control::Celda& c = g.ctl.zonas[(size_t)Juego::zonaMesaIA(1, zl, true)];
		b.caso("CU-FT-ANIM-ACTIVA-LUZ", c.carta == "light_of_intervention" && !c.bocaAbajo, "Light of Intervention no quedo boca arriba (activa) en su zona");
	}
	// una magia de campo del rival desde la mano
	{
		cub::Mesa m = mesaRitmo(g);
		const int k = m.mano(1, "yami");
		ponerRitmo(b, m.d);
		ia::Jugada j = ia::jugadaActivar({ 1, ia::EN_MANO, k });
		bool ok = d.aplicar(j);
		if (!ok) {   // (sin efecto propio: J_ACTIVAR_MT)
			j = ia::Jugada{};
			j.tipo = ia::J_ACTIVAR_MT;
			j.mano = k;
			ok = d.aplicar(j);
		}
		const int zc = control::zonaPorId("rival_campo");
		const MedidaRitmo r = medirRitmo(g, zc, "yami");
		const float tAct = (float)r.conActivacion * g.frameTimer;
		b.caso("CU-FT-ANIM-ACTIVA-CAMPO", ok && r.intermedios >= minimo && tAct >= ritmo::T_ACTIVA * 0.9f, "Yami: activada %d, %d cuadros de viaje, presentacion %.2f s", (int)ok, r.intermedios, tAct);
	}
	// cadena de 2: Swords del rival y el Jar of Greed colocado del jugador
	{
		cub::Mesa m = mesaRitmo(g);
		const int k = m.mano(1, "swords_of_revealing_light");
		const int zw = m.colocar(0, "jar_of_greed", 1);
		ponerRitmo(b, m.d);
		ia::Jugada resp = ia::jugadaActivar({ 0, ia::EN_MT, zw });
		const bool ok = d.aplicar(ia::jugadaActivar({ 1, ia::EN_MANO, k }));
		const MedidaRitmo r = medirRitmo(g, -1, "swords_of_revealing_light", &resp);
		const bool uno = std::find(r.eslabones.begin(), r.eslabones.end(), 1) != r.eslabones.end(), dos = std::find(r.eslabones.begin(), r.eslabones.end(), 2) != r.eslabones.end();
		b.caso("CU-FT-ANIM-CADENA", ok && uno && dos && r.maxEslabon >= 2, "la cadena de 2 no mostro sus eslabones (activada %d, eslabones 1 %d 2 %d, max %d)", (int)ok, (int)uno, (int)dos, r.maxEslabon);
		b.caso("CU-FT-ANIM-CADENA", ritmo::estado().cadena.empty(), "quedaron numeros de eslabon tras resolver la cadena (%d)", (int)ritmo::estado().cadena.size());
	}
	combateMD(b, minimo);   // rama classic-md-combate
	opciones::preguntar() = pregunta0;
	b.resolverUI();
	// (de vuelta a un duelo normal: el rival con su IA)
	g.turnoIA = ia::Turno(&g.dueloIA, 1, ia::PERSONALIDADES[0], 4, 77u);
}
