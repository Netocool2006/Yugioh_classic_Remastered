	// Yugioh Classic Remaster (rama classic-ps5-ritmo): EL RITMO DEL TURNO aplicado al duelo. Se incluye DENTRO de la clase
	// del juego, al final de md_ritmo_juego.h (asi lo tienen yugireload.cpp y los arneses de prueba). Constantes y
	// fuentes: ritmo_turno.h; tablas (antes / Master Duel / Joey the Passion / ahora): docs/md_ritmo.md "Ritmo del turno".
	// Lo llama duelo_juego.h en cuatro lugares:
	//  - sincronizarMesaEnOrden -> ritmoTurnoOcultarRobos: lo robado en un S_ROBO todavia sin presentar no esta en la mano
	//    dibujada (ni falta en el mazo). Antes el motor, al terminar el turno del rival, ya habia robado la carta del
	//    jugador y la primera sincronizacion de la mesa la ponia en su mano; en la fase de robo se animaba otra vez.
	//  - S_ROBO -> ritmoTurnoCartelRobo / ritmoTurnoDuracionRobo: el inicio del turno como Master Duel
	//    (EffectTaskTurnChange -> cartel de la Fase de Robo -> el robo DrawCardMotion*, 1.18 s).
	//  - S_FASE -> ritmoTurnoEsperaFase: el duelo espera el cartel de fase (MD: PhaseChangeProcess del Timeline, 0.98 s).
	//  - el rival -> ritmoTurnoRivalPuede / ritmoTurnoJugoRival: la pausa de lectura tras cada jugada visible.
	// Solo presentacion: el motor (y el duelo en linea) no cambian.
	size_t ritmoTurnoCartelK{ (size_t)-1 };   // el S_ROBO cuyo cartel de la Fase de Robo ya salio
	bool ritmoTurnoFaseRobo{ false };          // la columna de fases muestra la Fase de Robo hasta el siguiente S_FASE
	float ritmoTurnoLectura{ 99.0f };          // s de mesa al dia desde la ultima jugada visible del rival

	// el S_ROBO k es el robo del inicio del turno (sigue al S_TURNO de su lado)
	bool ritmoTurnoEsInicio(size_t k) const
	{
		const auto& su = dueloIA.sucesos;
		if (k >= su.size() || su[k].tipo != ia::S_ROBO) return false;
		for (size_t q = k; q-- > 0 && q + 4 > k;) {
			if (su[q].tipo == ia::S_TURNO) return su[q].lado == su[k].lado;
			if (su[q].tipo == ia::S_ROBO) return false;
		}
		return false;
	}

	void ritmoTurnoOcultarRobos(size_t desde)
	{
		const auto& su = dueloIA.sucesos;
		int pend[2]{ 0, 0 };
		std::vector<std::string> cartasJ;   // (las del jugador, de la ultima a la primera)
		for (size_t k = su.size(); k-- > desde;)
			if (su[k].tipo == ia::S_ROBO) {
				const int lado = su[k].lado == 1 ? 1 : 0;
				pend[lado]++;
				if (lado == 0) cartasJ.push_back(su[k].carta);
			}
		int extra[2]{ 0, 0 };
		for (int lado = 0; lado < 2; lado++) extra[lado] = std::max(0, pend[lado] - ocultas[lado]);   // (en el reparto ya estan ocultas)
		for (int n = 0; n < extra[0] && n < (int)cartasJ.size(); n++) {
			for (size_t i = ctl.manoJ.size(); i-- > 0;)
				if (ctl.manoJ[i] == cartasJ[(size_t)n]) {
					ctl.manoJ.erase(ctl.manoJ.begin() + (long)i);
					break;
				}
		}
		ctl.manoR = std::max(0, ctl.manoR - extra[1]);
		if (extra[0] > 0 || extra[1] > 0)
			interfazDuelo.contarMazos((int)dueloIA.l[0].mazo.size() + ocultas[0] + extra[0], (int)dueloIA.l[1].mazo.size() + ocultas[1] + extra[1],
			                          (int)dueloIA.l[0].extra.size());
		// la columna de fases (y su cartel) es la de lo PRESENTADO: con cambios de fase o de turno sin presentar, el motor
		// ya va por otra (antes, al terminar el turno el cartel de la Fase Final decia "FASE PRINCIPAL 1": la del turno
		// siguiente, que el motor ya habia empezado)
		bool adelante = false;
		for (size_t k = desde; k < su.size() && !adelante; k++) adelante = su[k].tipo == ia::S_FASE || su[k].tipo == ia::S_TURNO;
		if (adelante)
			for (size_t k = std::min(desde, su.size()); k-- > 0;) {
				if (su[k].tipo == ia::S_FASE) {
					ctl.fase = faseVisible(faseInterfaz((ia::Fase)std::max(0, std::min(su[k].valor, 4))));
					break;
				}
				// (S_TURNO: la columna se queda en la Fase Final hasta el cartel de la Fase de Robo)
			}
		if (ritmoTurnoFaseRobo) ctl.fase = 0;   // (el cartel de la Fase de Robo: la columna no salta a la fase del motor)
	}

	// S_ROBO: el robo del inicio del turno primero muestra el cartel de la Fase de Robo (MD: DuelDrawPhase_near/far
	// 0.967 s; Joey the Passion: el cartel de turno y el de la Draw Phase, 64 + 16 cuadros). true: el suceso espera
	bool ritmoTurnoCartelRobo(size_t k)
	{
		if (!ritmoTurnoEsInicio(k)) return false;
		// (se espera a los carteles mismos, no solo al reloj del duelo: con la pausa abierta el cartel se detiene)
		if (ctl.cartelTurnoVisible() || ctl.cartelInicioVisible()) {
			esperaEfecto = std::max(esperaEfecto, 0.05f);
			return true;
		}
		if (ritmoTurnoCartelK == k) {
			if (ritmoTurnoFaseRobo && ctl.fase == 0 && ctl.cartelFaseVisible()) {
				esperaEfecto = std::max(esperaEfecto, 0.05f);
				return true;
			}
			return false;
		}
		ritmoTurnoCartelK = k;
		ritmoTurnoFaseRobo = true;
		ctl.fase = 0;
		ctl.tFase = 0.0f;
		const bool rival = dueloIA.sucesos[k].lado == 1;
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline(rival ? "DuelDrawPhase_far" : "DuelDrawPhase_near");
		esperaEfecto = std::max(esperaEfecto, tl ? tl->duracion : control::Mando::MD_FASE);
		fprintf(stderr, "[ritmo] turno: cartel de la Fase de Robo (%s)\n", rival ? "rival" : "jugador");
		return true;
	}
	// la duracion del robo animado (0: la de control.h). El del inicio del turno: el de la fase de robo de MD
	float ritmoTurnoDuracionRobo(size_t k) const
	{
		if (!ritmoTurnoEsInicio(k)) return 0.0f;
		return dueloIA.sucesos[k].lado == 1 ? ritmoturno::ROBO_TURNO_RIVAL : ritmoturno::ROBO_TURNO;
	}

	// S_FASE (despues de mostrarFase): si el cartel se ve, el duelo lo espera entero (antes el rival seguia jugando y el
	// cartel de la Fase Final lo cortaba el de turno a los 0.5 s). La Principal 1 trae antes la Fase de Espera
	void ritmoTurnoEsperaFase(int faseIA)
	{
		ritmoTurnoFaseRobo = false;
		if (!ctl.cartelFaseVisible()) return;
		const float dur = ctl.fase == 3 ? control::Mando::MD_FASE_BP : control::Mando::MD_FASE;
		esperaEfecto = std::max(esperaEfecto, faseIA == (int)ia::F_PRINCIPAL1 ? tEspera + control::Mando::MD_FASE : dur);
	}

	// el rival: tras una jugada visible ya presentada, el rival no juega antes de LECTURA_MIN (lo que piensa corre a la
	// vez: si ya penso mas, no espera de mas). turnoIA.espera <= dt: la jugada saldria en este cuadro
	bool ritmoTurnoRivalPuede(float dt)
	{
		if (!ritmo::estado().ocupado()) ritmoTurnoLectura += dt;   // (desde que la mesa quedo quieta: viajes y activaciones terminados)
		if (ritmoTurnoLectura >= ritmoturno::LECTURA_MIN) return true;
		return !(turnoIA.espera >= 0.0f && turnoIA.espera <= dt);
	}
	void ritmoTurnoJugoRival()
	{
		const int t = (int)turnoIA.ultima.tipo;
		const bool visible = t != ia::J_FASE && t != ia::J_PASAR && t != ia::J_ELEGIR;
		ritmoTurnoLectura = visible ? 0.0f : 99.0f;
	}
