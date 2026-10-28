	// Yugioh Classic Remaster (rama classic-duelo-ui): se incluye DENTRO de la clase del juego (yugireload.cpp), despues
	// de duelo_juego.h. Lleva la consulta del duelo (duelo_visores.h): el registro (duelo_registro.h), la pila de la
	// cadena y las cartas jugables (duelo_cadena.h), y el panel de visores con sus botones. Todo sale del duelo real
	// (dueloIA) y de lo que la mesa ya presento (sucesosVistos): no cambia nada del duelo ni de sus tiempos.
	//  - avanzarConsulta(dt): cada cuadro, despues de avanzarDueloIA.
	//  - mandoConsulta(pulsos, dt): en avanzarMando, antes de control.h; true = el panel se quedo con los botones.
	visores::Consulta consulta;

	void avanzarConsulta(float dt)
	{
		visores::Consulta& c = consulta;
		const int pantalla = ctl.vista.pantalla;
		if (pantalla != VD::DUELO || ctl.vista.sorteo) {
			// (fuera del duelo: el panel se cierra; el registro se guarda hasta que empieza otro duelo)
			c.cerrar();
			if (pantalla != VD::RESULTADO) c.vistos = 0;
			c.pantallaAntes = ctl.vista.sorteo ? -1 : pantalla;   // (el sorteo ya es la pantalla del duelo: el duelo nuevo empieza despues)
			return;
		}
		if (c.pantallaAntes != VD::DUELO) {   // un duelo nuevo
			c.reg.limpiar();
			c.pila.limpiar();
			c.jug.clave.clear();
			c.vistos = 0;
		}
		c.pantallaAntes = pantalla;
		const auto& su = dueloIA.sucesos;
		if (c.vistos > sucesosVistos || c.vistos > su.size()) c.vistos = 0;   // (por si el duelo se rearmo)
		for (; c.vistos < sucesosVistos && c.vistos < su.size(); c.vistos++) {
			const ia::Suceso& s = su[c.vistos];
			std::string objetivo;
			if (s.tipo == ia::S_ATAQUE && s.zona2 >= 0) {   // el atacado, como esta en la mesa ahora ("?" boca abajo)
				const int z = zonaMesaIA(1 - s.lado, s.zona2);
				if (z >= 0 && !ctl.zonas[(size_t)z].vacia()) objetivo = ctl.zonas[(size_t)z].bocaAbajo ? std::string("?") : ctl.zonas[(size_t)z].carta;
				if (z >= 0 && objetivo.empty() && !dueloIA.l[(size_t)(1 - s.lado)].m[(size_t)s.zona2].vacia()) {
					const ia::Monstruo& m = dueloIA.l[(size_t)(1 - s.lado)].m[(size_t)s.zona2];
					objetivo = m.bocaAbajo ? std::string("?") : m.id;
				}
			}
			c.reg.anotar(s, objetivo);
			c.pila.suceso(s);
		}
		const bool alDia = sucesosVistos >= su.size() && esperaEfecto <= 0.0f;
		c.pila.avanzar(dt, !dueloIA.ef.cadena.empty(), alDia);
		c.jug.calcular(ctl, dueloIA, sucesosVistos);
	}

	bool mandoConsulta(uint32_t pulsos, float dt)
	{
		visores::Consulta& c = consulta;
		if (ctl.vista.pantalla != VD::DUELO || ctl.vista.sorteo) return false;
		// (rama classic-arreglo-ciclo-md, pregunta fantasma: la pregunta del duelo que llego con el panel abierto queda
		// detras (se puede seguir mirando el cementerio). OPTIONS decia "Primero responde a la pregunta" sin pregunta a la
		// vista: ahora OPTIONS (fases_md_juego.h lo deja pendiente para el mando) cierra el panel y la pregunta queda delante)
		if (c.vis.abierto && ctl.optionsPendiente && ctl.estado != control::DETALLE && dueloIA.ef.preg.tipo != ia::P_NADA && dueloIA.ef.preg.lado == 0) {
			c.cerrar();
			sonido.sfx("sfx_menu_cancelar");
			fprintf(stderr, "[consulta] OPTIONS con la pregunta detras: se cierra el panel\n");
			return false;
		}
		// (rama classic-arreglo-ciclo-md, jugador humano: con el selector de fases abierto X confirma la fase aunque el foco
		// este en un cementerio / desterradas / fusion; antes abria el visor de esa pila y el turno quedaba trancado)
		if (!c.vis.abierto && ctl.estado == control::NAVEGAR && !ctl.selFases.abierto) {
			int p = -1, lado = 0;
			if (pulsos & control::B_L3) {
				p = visores::P_REGISTRO;
			} else if ((pulsos & control::B_X) && ctl.foco.tipo == control::F_ZONA) {
				p = visores::pestanaDeZona(ctl.foco.i);
				lado = control::ZONAS_MESA[ctl.foco.i].rival ? 1 : 0;
			}
			if (p >= 0) {
				c.abrir(p, lado);
				sonido.sfx("sfx_menu_confirmar");
				fprintf(stderr, "[consulta] abre %s (%s)\n", visores::nombrePestana(p, false), lado ? "rival" : "jugador");
				ctl.avanzar(dt, 0);
				return true;
			}
		}
		if (!c.mando(ctl, pulsos, dt)) return false;
		if (!c.vis.abierto) sonido.sfx("sfx_menu_cancelar");
		else if (pulsos & (control::B_DIRECCIONES | control::B_CUADRADO)) sonido.sfx("sfx_menu_cursor");
		ctl.avanzar(dt, 0);   // (la mesa sigue: robos, vuelos y animaciones no se congelan)
		return true;
	}
