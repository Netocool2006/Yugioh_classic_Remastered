	// Rama classic-md-fases: el lado del juego del selector de fases, el engranaje y los carteles de Master Duel
	// (control: fases_md.h; dibujo: fases_md_ui.h; docs/md_fases.md). Se incluye dentro de la clase del juego, desde
	// duelo_juego.h, asi lo usan igual yugireload.cpp y los arneses de prueba (cu_juego.h, prueba_juego.cpp).
	//  - OPTIONS corto en el DUELO ya no abre la pausa: va al selector de fases (Mando::pulsarOptions). Fuera del duelo
	//    (elegir rival, resultado) OPTIONS sigue abriendo la pausa.
	//  - El panel tactil en el DUELO (antes: Construir mazo, que sigue en el menu) o X con el foco en el engranaje abren
	//    el menu del duelo (la pausa de siempre: Continuar, Opciones, Construir mazo, Abandonar, Salir); otro toque lo cierra.

	// avanzarPausa, con la pausa cerrada: true si el OPTIONS corto fue al selector de fases
	bool opcionesAlSelector(bool corto)
	{
		// (en el lanzamiento de moneda todavia no hay fases ni engranaje: OPTIONS abre la pausa como fuera del duelo; antes
		// iba al selector, que no se abre en el sorteo, y no habia como abandonar ni ir al menu)
		if (!corto || pausa.abierta || ctl.vista.pantalla != VD::DUELO || ctl.vista.sorteo) return false;
		ctl.pulsarOptions();
		fprintf(stderr, "[fases] OPTIONS: selector de fases (turno %d, fase %d, activo %d)\n", dueloIA.turno, (int)dueloIA.fase, dueloIA.activo);
		return true;
	}
	// el engranaje: el panel tactil, la tecla G / "tactil" del guion, o X con el foco en el (solo en el duelo)
	bool engranajePedido(uint32_t p)
	{
		const bool pedido = ctl.pedidoEngranaje;
		ctl.pedidoEngranaje = false;
		if (ctl.vista.pantalla != VD::DUELO || ctl.vista.sorteo) return false;
		const bool t = pedido || (mando.pressed & PAD_TOUCH_PAD) || (p & control::B_TACTIL);
		if (t) {
			ctl.tEngranaje = 0.0f;
			ctl.cerrarSelectorFases("engranaje");
			fprintf(stderr, "[fases] engranaje (%s)\n", pedido ? "X en el engranaje" : "panel tactil");
		}
		return t;
	}
	// los sonidos de Master Duel que dejo el selector (SE_PHASE_WINDOW_OPEN/CLOSE, SE_DUEL_SELECT/DECIDE)
	void sonidosMD()
	{
		for (const std::string& k : ctl.sfxMD) sonido.sfx(k.c_str());
		ctl.sfxMD.clear();
	}
	// el suceso S_TURNO: el cartel de cambio de turno de MD (DuelTurnChange00/01) con su sonido en el tiempo del
	// Timeline (SoundTrack: SE_NEXT_TURN_PLAYER 0.483 s / SE_NEXT_TURN_RIVAL 0.417 s) y el duelo espera a que termine
	// (EffectTaskTurnChange espera el Timeline antes de seguir)
	void presentarTurnoMD(const ia::Suceso& s)
	{
		const bool rival = s.lado == 1;
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline(rival ? "DuelTurnChange01" : "DuelTurnChange00");
		ctl.cartelTurno(rival, s.valor, tl ? tl->duracion : -1.0f);
		// el primer turno: antes, el cartel de inicio del duelo (DuelTextStart); el de turno espera a que termine
		float antes = 0.0f;
		if (s.valor <= 1) {
			const clipsmd::Timeline* ti = clipsmd::biblioteca().timeline("DuelTextStart");
			ctl.durCartelInicio = ti ? ti->duracion : control::Mando::MD_INICIO;
			ctl.tCartelInicio = 0.0f;
			antes = ctl.durCartelInicio;
			ctl.tCartelTurno = -antes;
			fprintf(stderr, "[fases] cartel de inicio del duelo (%.2f s)\n", antes);
		}
		float tSonido = rival ? 0.417f : 0.483f;
		if (tl && tl->marca(rival ? "SE_NEXT_TURN_RIVAL" : "SE_NEXT_TURN_PLAYER") >= 0.0f) tSonido = tl->marca(rival ? "SE_NEXT_TURN_RIVAL" : "SE_NEXT_TURN_PLAYER");
		sonido.sfx(rival ? "sfx_turno_rival" : "sfx_turno", 0.9f, 0.0f, antes + tSonido);
		esperaEfecto = std::max(esperaEfecto, antes + ctl.duracionCartelTurno());
	}
