	// Yugioh Classic Remaster (rama classic-moneda): se incluye DENTRO de la clase del juego (desde duelo_juego.h).
	// El lanzamiento de moneda antes del duelo (moneda.h): empezarMoneda() lo abre (empezarDueloIA), avanzarMoneda(p)
	// lo lleva cada cuadro con el mando (avanzarPantallaDuelo) y, al decidir quien empieza, arrancarDueloIA(empieza).
	// Sonidos reutilizados de audio.txt: sfx_impacto (el VS), sfx_voltear (el lanzamiento), sfx_carta_mover (el giro),
	// sfx_colocar (la caida y el rebote), sfx_clave_aparece (el brillo del resultado), sfx_menu_cursor / _confirmar.
	// PC: YUGI_MONEDA=cara|cruz fuerza el resultado (capturas); las pruebas usan monedaForzada.
	int monedaForzada{ -1 };   // -1 al azar, moneda::CARA o moneda::CRUZ

	void empezarMoneda()
	{
		VD& v = ctl.vista;
		v.sorteo = true;
		v.moneda.reiniciar();
#if defined(PS5_HOST_REFERENCE)
		if (const char* e = getenv("YUGI_MONEDA")) monedaForzada = std::string(e) == "cara" ? moneda::CARA : std::string(e) == "cruz" ? moneda::CRUZ : -1;
#endif
	}
	uint32_t azarMoneda()
	{
		semillaRival = semillaRival * 1664525u + 1013904223u;
		return (semillaRival >> 8) ^ (uint32_t)std::chrono::steady_clock::now().time_since_epoch().count();
	}
	// la moneda (justa salvo que se fuerce): CARA o CRUZ
	int lanzarMoneda() { return monedaForzada >= 0 ? monedaForzada : moneda::lanzar(azarMoneda()); }
	// el rival que gana el lanzamiento decide con su personalidad (los agresivos casi siempre empiezan; los pacientes a
	// veces prefieren ir segundos) y algo de azar: no siempre lo mismo
	int decisionRival()
	{
		const ia::Personalidad p = mazoRivalIA.rival ? mazoRivalIA.rival->personalidad() : ia::PERSONALIDADES[0];
		const float primero = std::max(0.2f, std::min(0.95f, 0.45f + 0.5f * p.agresividad - 0.3f * p.paciencia));
		return (float)(azarMoneda() & 0xFFFFu) / 65536.0f < primero ? 1 : 0;
	}
	void sonidoMoneda(unsigned bit, const char* clave, float vol = 1.0f)
	{
		moneda::Estado& m = ctl.vista.moneda;
		if (m.sonidos & bit) return;
		m.sonidos |= bit;
		sonido.sfx(clave, vol);
	}
	void etapaMoneda(int e)
	{
		ctl.vista.moneda.etapa = e;
		ctl.vista.moneda.t = 0.0f;
	}
	// el lanzamiento con el mando y el reloj (devuelve true: se queda con el mando). X solo adelanta lo que ya se vio
	// (la entrada, la moneda ya caida, el cartel); la eleccion del orden no se puede saltar
	bool avanzarMoneda(uint32_t p)
	{
		VD& v = ctl.vista;
		if (!v.sorteo) return false;
		moneda::Estado& m = v.moneda;
		const bool x = (p & control::B_X) && m.t > moneda::T_MIN_X;
		switch (m.etapa) {
		case moneda::VS:
			if (m.t > 0.45f) sonidoMoneda(moneda::S_VS, "sfx_impacto", 0.6f);
			if (enLineaActivo()) {   // rama classic-en-linea: la moneda es un bit de la semilla comun (nadie la elige)
				if ((m.t >= moneda::T_VS || x) && monedaRedGanador() >= 0) {
					m.ganador = monedaRedGanador();
					m.cara = m.ganador == 0 ? moneda::CARA : moneda::CRUZ;
					etapaMoneda(moneda::GIRO);
					fprintf(stderr, "[moneda] en linea: gana %s\n", m.ganador == 0 ? "el jugador" : "el rival");
				}
				break;
			}
			if (m.t >= moneda::T_VS || x) {
				m.cara = lanzarMoneda();
				m.ganador = moneda::ganadorDe(m.cara);
				etapaMoneda(moneda::GIRO);
				fprintf(stderr, "[moneda] lanzamiento contra %s\n", mazoRivalIA.rival ? mazoRivalIA.rival->nombreEs.c_str() : "?");
			}
			break;
		case moneda::GIRO:
			sonidoMoneda(moneda::S_LANZA, "sfx_voltear");
			if (m.t > 0.5f) sonidoMoneda(moneda::S_GIRA, "sfx_carta_mover", 0.7f);
			if (m.t >= moneda::T_CAE) sonidoMoneda(moneda::S_CAE, "sfx_colocar");
			if (m.t >= moneda::T_CAE + 0.26f) sonidoMoneda(moneda::S_REBOTA, "sfx_colocar", 0.45f);
			if (m.t >= moneda::T_GIRO || (x && m.t >= moneda::T_CAE)) {
				etapaMoneda(moneda::RESULTADO);
				fprintf(stderr, "[moneda] %s: %s\n", m.cara == moneda::CARA ? "cara" : "cruz", m.ganador == 0 ? "gana el jugador" : "gana el rival");
			}
			break;
		case moneda::RESULTADO:
			sonidoMoneda(moneda::S_BRILLO, "sfx_clave_aparece", 0.8f);
			if (m.t >= moneda::T_RESULTADO || (x && m.t > 0.5f)) {
				if (m.ganador == 0) {
					m.opcion = 0;
					etapaMoneda(moneda::ORDEN);
				} else if (enLineaActivo()) {   // rama classic-en-linea: decide el rival desde su maquina
					m.empieza = -1;
					etapaMoneda(moneda::RIVAL_DECIDE);
				} else {
					m.empieza = decisionRival();
					etapaMoneda(moneda::RIVAL_DECIDE);
					fprintf(stderr, "[moneda] el rival decide ir %s\n", m.empieza == 1 ? "primero" : "segundo");
				}
			}
			break;
		case moneda::ORDEN:
			if (p & (control::B_IZQ | control::B_DER)) {
				m.opcion = (p & control::B_IZQ) ? 0 : 1;
				sonido.sfx("sfx_menu_cursor", 0.7f);
			}
			if (enLineaActivo() && m.empieza >= 0) {   // rama classic-en-linea: ya eligio; espera que el duelo en red arranque
				if (dueloRedJugando()) {
					v.sorteo = false;
					arrancarDueloRed();
				}
				break;
			}
			if (enLineaActivo() && !x && m.t >= en_linea::ELEGIR_ORDEN_S) m.opcion = 0;   // (sin elegir: va primero)
			if (x || (enLineaActivo() && m.t >= en_linea::ELEGIR_ORDEN_S)) {
				sonido.sfx("sfx_menu_confirmar");
				m.empieza = m.opcion == 0 ? 0 : 1;
				fprintf(stderr, "[moneda] el jugador decide ir %s\n", m.empieza == 0 ? "primero" : "segundo");
				if (enLineaActivo()) {
					elegirOrdenRed(m.empieza == 0);
					break;
				}
				v.sorteo = false;
				arrancarDueloIA(m.empieza);
			}
			break;
		default:   // RIVAL_DECIDE: se lee y sigue (X o al terminar)
			if (enLineaActivo()) {   // rama classic-en-linea: lo que eligio el rival llega por la red
				if (m.empieza < 0 && monedaRedEmpieza() >= 0) {
					m.empieza = monedaRedEmpieza();
					m.t = 0.0f;
				}
				if (m.empieza >= 0 && dueloRedJugando() && (m.t >= moneda::T_RIVAL || (x && m.t > 0.6f))) {
					v.sorteo = false;
					arrancarDueloRed();
				}
				break;
			}
			if (m.t >= moneda::T_RIVAL || (x && m.t > 0.6f)) {
				v.sorteo = false;
				arrancarDueloIA(m.empieza);
			}
			break;
		}
		return true;
	}
