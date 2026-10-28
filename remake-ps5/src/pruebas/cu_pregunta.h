// Rama classic-arreglo-ciclo-md: la PREGUNTA FANTASMA y la respuesta que no se ve (PS5, ciclo md_ciclo1). En la consola el
// usuario (1) perdio una carta al cementerio sin enterarse (el Mirror Force del rival al atacar: la pregunta para
// responder no se abrio o no se vio) y (2) quedo trancado con OPTIONS diciendo "Primero responde a la pregunta" sin
// pregunta a la vista. La causa: con el ritmo de Master Duel la mesa queda bloqueada mas tiempo (viajes, cartel de la
// trampa, cadena) y lo que el jugador abre en ese rato (el detalle con X o triangulo, el panel de visores con L3 o X en
// una pila) se quedaba delante: la pregunta del duelo no se abria (solo se abre desde la mesa libre) o se abria detras
// del panel. Ahora el detalle se cierra solo, como el CardInfo de Master Duel, al empezar cada movimiento de carta, volteo o
// paso de cadena; el panel se queda (se puede seguir mirando el
// cementerio, rama classic-arreglo-visor) pero OPTIONS lo cierra y deja la pregunta delante; y OPTIONS sin la pregunta a
// la vista dice que se espere a la animacion. Se incluye en prueba_front_total.cpp (modos "extra" y "pregunta").
//  - CU-FT-PREGUNTA-FANTASMA-<variante>: atacar con el mando a un rival con Mirror Force (el jugador tiene Trap Jammer
//    para responder) y, mientras la mesa esta bloqueada, apretar lo que apretaria el usuario: X (el detalle, leerlo 1.5 s
//    y cerrarlo, si no lo cerro antes el duelo al mover una carta, como MD),
//    OPTIONS, el engranaje (panel tactil), L3 (visores) o todo junto. Cada cuadro: si el duelo espera la respuesta del
//    jugador y la mesa no esta bloqueada, la pregunta tiene que estar a la vista (nunca mas de 0.5 s sin verse; con el
//    panel de visores abierto por el jugador la pregunta queda detras y OPTIONS la trae), y el aviso "Primero responde a
//    la pregunta" solo puede salir con la pregunta a la vista.
//  - CU-FT-PREGUNTA-ANTES-DEL-CEMENTERIO: la pregunta se vio antes de que Summoned Skull se fuera al cementerio.
//  - CU-FT-PREGUNTA-OPTIONS: con la pregunta a la vista OPTIONS no abre el selector ni la tapa; contestada ("No activar"),
//    el duelo sigue y OPTIONS abre el selector de fases (no queda trancado).
#pragma once

static void preguntaFantasma(Bot& b)
{
	Juego& g = b.g;
	ia::Duelo& d = g.dueloIA;
	const control::Mando::SelectorFases& sf = g.ctl.selFases;
	const std::vector<std::string> mazo = mazoVariado(g.baseIA);
	b.rivalPorBot = true;
	b.grabar = false;
	static const char* const VARIANTE[] = { "DETALLE", "OPTIONS", "ENGRANAJE", "VISOR", "TODO" };
	for (int v = 0; v < 5; v++) {
		const std::string id = std::string("CU-FT-PREGUNTA-FANTASMA-") + VARIANTE[v];
		b.duelo = std::string("pregunta-") + VARIANTE[v];
		if (g.ctl.vista.pantalla != VD::DUELO && !nuevoDuelo(b)) {
			b.caso(id, false, "no se pudo empezar un duelo");
			return;
		}
		if (g.pausa.abierta) b.apretar(PAD_TOUCH_PAD, 0.2f);
		cub::Mesa m(&g.baseIA, ia::F_BATALLA);
		m.d.turno = 3;
		m.d.l[0].mazo = mazo;
		m.d.l[1].mazo.assign(30, "celtic_guardian");
		m.mon(0, "summoned_skull");
		m.mon(1, "celtic_guardian");
		m.colocar(0, "trap_jammer", 1);
		m.colocar(1, "mirror_force", 1);
		m.d.recalcular();
		inyectar(b, m.d);
		b.esperar(0.3f);
		const bool libre = b.esperarQue(10.0f, [&] { return b.libre(); });
		b.caso(id, libre, "la mesa no quedo libre en la batalla");
		if (!libre) continue;
		const int za = Juego::zonaMesaIA(0, 0);
		b.rivalActiva = "mirror_force";
		// el ataque con el mando: X en Summoned Skull, Atacar, X en el objetivo
		b.irA(Foco{ F_ZONA, za });
		b.apretar(PAD_CROSS, 0.15f);
		int k = -1;
		for (int o = 0; o < (int)g.ctl.opciones.size(); o++)
			if (g.ctl.opciones[(size_t)o].accion == control::AC_ATACAR && g.ctl.opciones[(size_t)o].activa) k = o;
		b.elegirOpcion(k);
		if (g.ctl.estado == control::ELEGIR_OBJETIVO && !g.ctl.validas.empty()) {
			b.irA(Foco{ F_ZONA, g.ctl.validas[0] });
			for (int n = 0; n < (int)(0.1f / g.frameTimer); n++) b.cuadro(PAD_CROSS);
		}
		const bool ataco = b.esperarQue(2.0f, [&] { return d.ef.ataque.hay || g.hayAtaque() || d.ef.preg.tipo != ia::P_NADA; });
		b.caso(id, ataco, "el ataque con el mando no se declaro (estado %s)", control::NOMBRE_ESTADO[g.ctl.estado]);
		if (!ataco) {
			b.rivalActiva.clear();
			b.resolverUI();
			continue;
		}
		// lo que aprieta el usuario mientras la mesa esta bloqueada, y el vigia de la pregunta cada cuadro
		bool vio = false, vioAntesDeMorir = false, fantasma = false, avisoFalso = false, pausaVista = false, visorVisto = false, detalleVisto = false;
		float tSin = 0.0f, tMax = 0.0f, tBoton = 0.0f, tDetras = 0.0f;
		bool optionsTrajo = false;
		float tDetalle = 0.0f;
		int n = 0;
		std::string aviso0 = g.ctl.aviso;
		for (float t = 0.0f; t < 25.0f && !vio; t += g.frameTimer, n++) {
			const bool pendiente = d.ganador < 0 && d.ef.preg.tipo != ia::P_NADA && d.ef.preg.lado == 0;
			const int st = g.ctl.estado;
			const bool menuPregunta = (g.ctl.enPregunta || g.ctl.armando || g.ctl.eleccionDuelo) &&
			                          (st == control::MENU || st == control::ELEGIR_CARTA || st == control::ELEGIR_OBJETIVO || st == control::ELEGIR_ZONA || st == control::ELEGIR_TRIBUTOS);
			const bool aLaVista = menuPregunta && !g.consulta.vis.abierto && !g.pausa.abierta && !g.ctl.selFases.abierto;
			if (pendiente && aLaVista && g.ctl.estado == control::MENU) {
				vio = true;
				vioAntesDeMorir = d.l[0].m[0].id == "summoned_skull";
				break;
			}
			// (con el detalle abierto que abrio el jugador la pregunta sale al cerrarlo)
			tSin = pendiente && !g.ctl.bloqueado && !g.pausa.abierta && !g.consulta.vis.abierto && g.ctl.estado != control::DETALLE && !aLaVista ? tSin + g.frameTimer : 0.0f;
			tDetalle = g.ctl.estado == control::DETALLE ? tDetalle + g.frameTimer : 0.0f;
			tDetras = pendiente && !g.ctl.bloqueado && g.consulta.vis.abierto ? tDetras + g.frameTimer : 0.0f;
			tMax = std::max(tMax, tSin);
			fantasma |= tSin > 0.5f;
			if (g.ctl.aviso != aviso0) {
				aviso0 = g.ctl.aviso;
				avisoFalso |= aviso0 == idioma::tr("Primero responde a la pregunta") && !aLaVista;
			}
			pausaVista |= g.pausa.abierta;
			visorVisto |= g.consulta.vis.abierto;
			detalleVisto |= g.ctl.estado == control::DETALLE;
			uint32_t pad = 0;
			tBoton += g.frameTimer;
			const bool apretar = g.ctl.bloqueado && tBoton > 0.25f && !(g.pausa.abierta && v == 2);
			if (apretar) {
				tBoton = 0.0f;
				switch (v) {
				case 0: pad = detalleVisto ? 0 : PAD_CROSS; break;   // (X una vez: el detalle de Summoned Skull, y a leerlo)
				case 1: pad = PAD_OPTIONS; break;
				case 2: pad = pausaVista ? 0 : PAD_TOUCH_PAD; break;
				case 3: pad = visorVisto ? 0 : PAD_L3; break;
				default: {
					static const uint32_t P[] = { PAD_CROSS, PAD_OPTIONS, PAD_TRIANGLE, PAD_L3, PAD_OPTIONS, PAD_CROSS };
					pad = P[(size_t)(n % 6)];
				}
				}
			}
			// (detalle: lo lee 1.5 s y lo cierra con el circulo)
			if (v == 0 && tDetalle > 1.5f) {
				pad = PAD_CIRCLE;
				tDetalle = 0.0f;
			}
			// (visores: la pregunta llego detras del panel; el usuario aprieta OPTIONS, como en la PS5)
			if (v == 3 && tDetras > 1.0f) {
				pad = PAD_OPTIONS;
				optionsTrajo = true;
				tDetras = 0.0f;
			}
			// (engranaje: el menu del duelo abierto un rato y cerrado con otro toque)
			if (v == 2 && g.pausa.abierta && tBoton > 1.5f) {
				pad = PAD_TOUCH_PAD;
				tBoton = 0.0f;
			}
			if (pad) {
				for (int q = 0; q < (int)(0.1f / g.frameTimer); q++) b.cuadro(pad);
				t += 0.1f;
			} else {
				b.cuadro(0);
			}
		}
		if (g.pausa.abierta) b.apretar(PAD_TOUCH_PAD, 0.2f);
		b.caso(id, vio && !fantasma, "la pregunta para responder al Mirror Force no se vio (vio %d, sin verse %.2f s, estado %s, visor %d, pausa %d, pregunta %d lado %d)", (int)vio,
		       tMax, control::NOMBRE_ESTADO[g.ctl.estado], (int)g.consulta.vis.abierto, (int)g.pausa.abierta, (int)d.ef.preg.tipo, d.ef.preg.lado);
		b.caso(id, !avisoFalso, "OPTIONS dijo 'Primero responde a la pregunta' sin la pregunta a la vista");
		if (v == 2) b.caso(id, pausaVista, "el engranaje no abrio el menu del duelo durante la animacion");
		if (v == 0) b.caso(id, detalleVisto, "X durante la animacion no abrio el detalle");
		if (v == 3) b.caso(id, visorVisto && optionsTrajo, "L3 no abrio los visores durante la animacion o la pregunta no llego detras del panel (visor %d, OPTIONS %d)",
		                   (int)visorVisto, (int)optionsTrajo);
		b.caso("CU-FT-PREGUNTA-ANTES-DEL-CEMENTERIO", !vio || vioAntesDeMorir, "[%s] Summoned Skull se fue al cementerio antes de la pregunta", VARIANTE[v]);
		b.rivalActiva.clear();
		if (!vio) {
			b.resolverUI();
			continue;
		}
		// con la pregunta a la vista: OPTIONS no abre el selector ni la tapa; "No activar" y el duelo sigue
		b.apretar(PAD_OPTIONS, 0.2f);
		b.caso("CU-FT-PREGUNTA-OPTIONS", !sf.abierto && g.ctl.estado == control::MENU && g.ctl.enPregunta && g.ctl.aviso == idioma::tr("Primero responde a la pregunta"),
		       "[%s] OPTIONS con la pregunta a la vista: selector %d, estado %s, aviso '%s'", VARIANTE[v], (int)sf.abierto, control::NOMBRE_ESTADO[g.ctl.estado], g.ctl.aviso.c_str());
		if (g.ctl.estado == control::MENU && g.ctl.enPregunta) b.elegirOpcion((int)g.ctl.opciones.size() - 1);   // (No activar)
		b.resolverUI();
		const bool siguio = b.esperarQue(15.0f, [&] { return b.libre() || d.ganador >= 0; });
		b.caso("CU-FT-PREGUNTA-OPTIONS", siguio && d.l[0].m[0].vacia(), "[%s] contestada la pregunta el duelo no siguio (libre %d, Summoned Skull '%s')", VARIANTE[v], (int)siguio,
		       d.l[0].m[0].id.c_str());
		if (!siguio) continue;
		b.apretar(PAD_OPTIONS, 0.2f);
		b.caso("CU-FT-PREGUNTA-OPTIONS", sf.abierto, "[%s] despues de la pregunta OPTIONS no abrio el selector (aviso '%s')", VARIANTE[v], g.ctl.aviso.c_str());
		if (sf.abierto) b.apretar(PAD_CIRCLE, 0.2f);
	}
}

// (rama classic-arreglo-ciclo-md, la corrida completa de front_total: H-valon-NUNCA se trabo 150 s en el turno del rival)
// La IA equipaba y desequipaba Armor Breaker sin fin: "1 vez por turno equipa O desequipa" se contaba por el uid y la
// carta cambia de uid al pasar de monstruo a equipo y de vuelta.
//  - CU-BE-UNION-UNA-VEZ: equipada este turno no se desequipa; desequipada este turno no se vuelve a equipar; al turno
//    siguiente si.
static void unionUnaVez(Bot& b)
{
	Juego& g = b.g;
	for (int desdeEquipo = 0; desdeEquipo < 2; desdeEquipo++) {
		cub::Mesa m(&g.baseIA);
		const int guerrero = m.mon(0, "celtic_guardian");
		const int ab = m.mon(0, "armor_breaker");
		ia::Duelo& d = m.d;
		if (desdeEquipo) {   // (equipada de un turno anterior: se desequipa y no se vuelve a equipar)
			const int zmt = d.l[0].libreMT();
			d.l[0].m[(size_t)ab] = ia::Monstruo{};
			d.l[0].mt[(size_t)zmt] = ia::MagiaTrampa{ "armor_breaker", false };
			d.l[0].mt[(size_t)zmt].uid = d.nuevoUid();
			d.l[0].mt[(size_t)zmt].eqUid = d.l[0].m[(size_t)guerrero].uid;
			d.recalcular();
			const bool ok = m.activar(0, { 0, ia::EN_MT, zmt });
			m.pasarTodo();
			int z = -1;
			for (int k = 0; k < ia::ZONAS; k++)
				if (d.l[0].m[(size_t)k].id == "armor_breaker") z = k;
			const std::string mot = z >= 0 ? ia::motivoNoActivar(d, 0, { 0, ia::EN_MONSTRUO, z }, false) : "sin monstruo";
			cu::caso("CU-BE-UNION-UNA-VEZ", ok && z >= 0 && mot == "ef.una_vez", "desequipada este turno se puede volver a equipar (desequipo %d, zona %d, motivo '%s')", (int)ok, z,
			         mot.c_str());
			continue;
		}
		const bool ok = m.activar(0, { 0, ia::EN_MONSTRUO, ab });
		m.pasarTodo();
		int zmt = -1;
		for (int k = 0; k < ia::ZONAS; k++)
			if (d.l[0].mt[(size_t)k].id == "armor_breaker") zmt = k;
		const std::string mot = zmt >= 0 ? ia::motivoNoActivar(d, 0, { 0, ia::EN_MT, zmt }, false) : "sin equipo";
		cu::caso("CU-BE-UNION-UNA-VEZ", ok && zmt >= 0 && mot == "ef.una_vez", "equipada este turno se puede desequipar (equipo %d, zona %d, motivo '%s')", (int)ok, zmt, mot.c_str());
		// al turno siguiente (el propio, dos despues) si se desequipa
		if (zmt >= 0) {
			d.ef.usadosTurno.clear();
			const std::string mot2 = ia::motivoNoActivar(d, 0, { 0, ia::EN_MT, zmt }, false);
			cu::caso("CU-BE-UNION-UNA-VEZ", mot2.empty(), "en otro turno la union equipada no se desequipa ('%s')", mot2.c_str());
		}
	}
}
