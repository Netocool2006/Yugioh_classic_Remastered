// Rama classic-md-fases: el cambio de fase y el menu del duelo al estilo de Yu-Gi-Oh! Master Duel (docs/md_fases.md).
// Se incluye DENTRO de la clase control::Mando (control.h), en su parte publica. Solo logica: lo dibuja fases_md_ui.h.
//
//  - OPTIONS (corto) en TU turno abre el selector de fases junto al indicador de fase (en MD: PhaseSelect3D, el boton
//    de fase de la mesa, y PhaseSelectWindow con el mando). Tres acciones, como pidio el usuario: "Iniciar Fase de
//    Batalla", "Ir a M2" y "Finalizar turno"; solo las validas activas (MD: PhaseSelect3D.IsPhaseMovable). Desde M1:
//    BP (no en el primer turno ni con Great Long Nose / D.D. Borderline) o EP; desde BP: M2 o EP; desde M2: EP.
//    Izquierda/derecha elige, X confirma, circulo (u OPTIONS otra vez) cierra.
//  - Elegir una fase deja un OBJETIVO: el motor sigue pasando de a una fase con J_FASE (el mismo camino que antes el
//    cuadrado, asi que las ventanas del rival, el final de la batalla y la Fase Final no cambian y el duelo en linea
//    manda las mismas jugadas), cada vez que el jugador queda libre en el mismo turno, hasta llegar. Los carteles de
//    las fases intermedias no se muestran (en MD tampoco: va directo de M1 al cartel de la Fase Final).
//  - El engranaje (DuelMenuButton de MD) abre el menu del duelo (la pausa de duelo_juego.h): con el panel tactil o con
//    el foco sobre el (F_ENGRANAJE, a la izquierda de la mano) y X. Lo pide "pedidoEngranaje" (duelo_juego.h lo abre).
//  - El cartel de turno (MD: DuelTurnChange00/01, ViewerTaskTurnChange) y el de fase (DuelXxxPhase_near/far) llevan
//    su reloj aca (tCartelTurno, tFase) y los dibuja fases_md_ui.h con los tiempos de los Timeline de MD.

// tiempos de Master Duel (Timeline y Tween de resources.assets; docs/md_fases.md)
static constexpr float MD_VENTANA = 0.12f;          // PhaseSelectWindow: Window In (QuartOut) / Out (CubicIn), ScreenPanel FrameIn/Out
static constexpr float MD_TURNO_J = 2.05f;          // DuelTurnChange00Timeline (TU TURNO)
static constexpr float MD_TURNO_R = 1.55f;          // DuelTurnChange01Timeline (TURNO DEL RIVAL)
static constexpr float MD_FASE = 0.967f;            // DuelPhaseTimeline (DP, SP, M1, M2, EP)
static constexpr float MD_FASE_BP = 1.0f;           // DuelBattlePhaseTimeline (con el estallido del texto)

// las tres acciones del selector: a que fase (indices de interfaz::FASES) y su texto
static constexpr int SEL_FASES = 3;
static constexpr int SEL_DESTINO[SEL_FASES] = { 3, 4, 5 };
static const char* textoSelector(int k)
{
	static const char* const T[SEL_FASES] = { "Iniciar Fase de Batalla", "Ir a M2", "Finalizar turno" };
	return k >= 0 && k < SEL_FASES ? T[k] : "";
}
struct SelectorFases {
	bool abierto{ false };
	int opcion{ 0 };                       // 0 BP, 1 M2, 2 EP
	std::array<bool, SEL_FASES> valida{};
	float t{ 99.0f };                      // s desde que se abrio
	float tCierre{ 99.0f };                // s desde que se cerro (la salida de MD_VENTANA)
	float tElegida{ 99.0f };               // s desde que se movio el cursor (el "select" de 0.2 s de MD)
	int desde{ 2 };                        // la fase en la que se abrio (interfaz::FASES)
} selFases;
int faseObjetivo{ -1 }, faseObjetivoTurno{ -1 };   // a donde va el jugador (interfaz::FASES; -1 = ninguna)
// (rama classic-ps5-efectos3, PS5: «con Goblin Attack Force me pidio que esperara a que terminara la animacion pero no hubo
// ninguna»: el duelo seguia ocupado sin nada en pantalla (el rival decidiendo si responde al ataque, la posicion de fin de
// batalla). OPTIONS ya no avisa: queda en espera y el selector se abre solo en cuanto se puede)
bool selectorEnEspera{ false };
int selectorEnEsperaTurno{ -1 };
bool optionsPendiente{ false };   // un OPTIONS corto (duelo_juego.h: avanzarPausa) para el proximo avanzar
bool pedidoEngranaje{ false };    // X sobre el engranaje: duelo_juego.h abre el menu del duelo
float tEngranaje{ 99.0f };        // s desde el ultimo toque al engranaje (el destello de "click")
std::vector<std::string> sfxMD;   // sonidos de MD que el juego toca y vacia (duelo_juego.h: SE_PHASE_WINDOW_OPEN/CLOSE, SE_DUEL_SELECT/DECIDE)
// el cartel de cambio de turno (lo arranca duelo_juego.h con el suceso S_TURNO)
float tCartelTurno{ 99.0f };
bool cartelTurnoRival{ false };
int cartelTurnoNum{ 0 };
float durCartelTurno{ MD_TURNO_J };   // la del Timeline cargado (clips_md.h), si lo hay
// el cartel de inicio del duelo (MD: DuelTextStart, 2.417 s) antes del primer cartel de turno
static constexpr float MD_INICIO = 2.417f;
float tCartelInicio{ 99.0f }, durCartelInicio{ MD_INICIO };
bool cartelInicioVisible() const { return tCartelInicio < durCartelInicio; }

void pulsarOptions() { optionsPendiente = true; }
void cartelTurno(bool rival, int numero, float dur = -1.0f)
{
	durCartelTurno = dur > 0.0f ? dur : rival ? MD_TURNO_R : MD_TURNO_J;
	tCartelTurno = 0.0f;
	cartelTurnoRival = rival;
	cartelTurnoNum = numero;
	anotar("cartel de turno %d (%s)", numero, rival ? "rival" : "jugador");
}
float duracionCartelTurno() const { return durCartelTurno; }
bool cartelTurnoVisible() const { return tCartelTurno < duracionCartelTurno(); }
// el cartel de fase que toca ahora ("" si va de paso hacia la fase elegida: no se muestra)
bool cartelFaseVisible(float dur = -1.0f) const
{
	if (demo || fase < 0 || fase > 5) return false;
	if (faseObjetivo >= 0 && fase < faseObjetivo && fase >= 2) return false;   // (fase intermedia del camino)
	return tFase < (dur > 0.0f ? dur : fase == 3 ? MD_FASE_BP : MD_FASE);
}

static int faseUI(ia::Fase f)
{
	switch (f) {
	case ia::F_ROBO: return 0;
	case ia::F_PRINCIPAL1: return 2;
	case ia::F_BATALLA: return 3;
	case ia::F_PRINCIPAL2: return 4;
	default: return 5;
	}
}

// las acciones validas ahora (MD: IsPhaseMovable). Sin duelo (prueba_mando, la mesa libre) cualquiera mas adelante
std::array<bool, SEL_FASES> fasesValidas() const
{
	std::array<bool, SEL_FASES> v{};
	if (!duelo) {
		for (int k = 0; k < SEL_FASES; k++) v[(size_t)k] = SEL_DESTINO[k] > fase;
		return v;
	}
	const ia::Duelo& d = *duelo;
	if (d.activo != 0 || d.ganador >= 0) return v;
	switch (d.fase) {
	case ia::F_PRINCIPAL1:
		v[0] = d.turno > 1 && !(d.efectos && (d.ef.saltaBatalla[0] || d.sinFaseBatallaM2()));
		v[2] = true;
		break;
	case ia::F_BATALLA:
		if (d.efectos && ia::hayForzadosM2(d)) break;   // (Amazoness Archers: deben atacar antes)
		v[1] = v[2] = true;
		break;
	case ia::F_PRINCIPAL2: v[2] = true; break;
	default: break;
	}
	return v;
}

// la pregunta del duelo esta en pantalla (su menu, la eleccion o el armado de la respuesta; o el detalle abierto desde ella)
bool preguntaALaVista() const
{
	if (!enPregunta && !armando && !eleccionDuelo) return false;
	if (estado == DETALLE) return antesDetalle != NAVEGAR;
	return estado != NAVEGAR && estado != VOLANDO;
}
// se puede abrir el selector ahora? (motivo: el aviso si no)
bool puedeElegirFase(std::string* motivo = nullptr) const
{
	auto no = [&](const char* m) {
		if (motivo) *motivo = m;
		return false;
	};
	if (vista.sorteo) return no("");   // (el juego solo manda OPTIONS aca con la pantalla del duelo)
	if (duelo) {
		if (duelo->ganador >= 0) return no("");
		if (duelo->activo != 0) return no("");   // (turno del rival: OPTIONS no hace nada)
		// (en la PS5 quedo "Primero responde a la pregunta" sin pregunta a la vista: solo cuenta la pregunta real del duelo.
		// Si es del jugador y su menu no esta abierto, abrirSelectorFases lo vuelve a abrir; si todavia no se puede abrir
		// (la animacion de la cadena o del ataque sigue) el aviso es esperar, no "responde"; las marcas viejas con la mesa
		// libre no cuentan: control.h las limpia)
		if (duelo->ef.preg.tipo != ia::P_NADA && duelo->ef.preg.lado == 0)
			return no(preguntaALaVista() ? "Primero responde a la pregunta" : "Espera a que termine la animación");
		if (duelo->ef.preg.tipo != ia::P_NADA) return no("Espera a que termine la animación");   // (responde el rival)
		// (rama classic-ps5-control2: solo con la pregunta a la vista; con una carta volando, esperar)
		if (estado != NAVEGAR && (enPregunta || armando || eleccionDuelo))
			return no(preguntaALaVista() ? "Primero responde a la pregunta" : "Espera a que termine la animación");
		if ((ataqueEnCurso && ataqueEnCurso()) || duelo->ef.ataque.hay) return no("No se puede cambiar de fase durante un ataque");
		if (duelo->ocupado() || bloqueado || vuelo.activo) return no("Espera a que termine la animación");
		if (faseObjetivo >= 0) return no("");
	}
	if (estado != NAVEGAR) return no("");
	const std::array<bool, SEL_FASES> v = fasesValidas();
	if (!v[0] && !v[1] && !v[2]) {
		if (duelo && duelo->fase == ia::F_BATALLA && duelo->efectos && ia::hayForzadosM2(*duelo)) return no("Amazoness Archers: tus monstruos deben atacar este turno");
		return no("Ahora no se puede cambiar de fase");
	}
	return true;
}

void abrirSelectorFases()
{
	std::string motivo;
	// con el detalle de una carta abierto desde la mesa, OPTIONS lo cierra y sigue (antes el cuadrado hacia lo mismo)
	if (estado == DETALLE && antesDetalle == NAVEGAR) {
		anotar("OPTIONS cierra el detalle de '%s'", cartaDetalle.c_str());
		cambiarEstado(NAVEGAR);
	}
	if (duelo && duelo->ef.preg.tipo != ia::P_NADA && duelo->ef.preg.lado == 0 && estado == NAVEGAR && !bloqueado) abrirPregunta();
	if (!puedeElegirFase(&motivo)) {
		if (motivo == "Espera a que termine la animación" && duelo && duelo->activo == 0 && duelo->ganador < 0) {
			selectorEnEspera = true;
			selectorEnEsperaTurno = duelo->turno;
			anotar("OPTIONS: el selector de fases queda en espera (el duelo esta ocupado)");
			return;
		}
		if (!motivo.empty()) avisar(motivo);
		anotar("OPTIONS: el selector de fases no se abre (%s)", motivo.empty() ? "no es tu momento" : motivo.c_str());
		return;
	}
	selFases.abierto = true;
	selFases.valida = fasesValidas();
	selFases.t = 0.0f;
	selFases.tElegida = 0.0f;
	selFases.desde = fase;
	sfxMD.push_back("sfx_fases_abrir");   // (PhaseSelectWindow.OpenWindow: SE_PHASE_WINDOW_OPEN)
	// el cursor empieza en la siguiente fase valida (como MD: la fase de despues de la actual)
	selFases.opcion = 0;
	while (selFases.opcion < SEL_FASES - 1 && !selFases.valida[(size_t)selFases.opcion]) selFases.opcion++;
	anotar("selector de fases abierto (fase %d): BP %d, M2 %d, EP %d", fase, (int)selFases.valida[0], (int)selFases.valida[1], (int)selFases.valida[2]);
}
void cerrarSelectorFases(const char* por)
{
	if (!selFases.abierto) return;
	selFases.abierto = false;
	selFases.tCierre = 0.0f;
	sfxMD.push_back("sfx_fases_cerrar");   // (CloseWindow: SE_PHASE_WINDOW_CLOSE)
	anotar("selector de fases cerrado (%s)", por);
}

// cada cuadro con el selector abierto (o con un OPTIONS): true si se quedo con los botones
bool avanzarSelectorFases(uint32_t p)
{
	if (!selFases.abierto) {
		if (!(p & B_OPTIONS)) return false;
		abrirSelectorFases();
		return selFases.abierto;
	}
	// el duelo cambio por debajo (el reloj del duelo en linea paso de fase, termino el duelo...): se cierra
	if (!puedeElegirFase() && !(duelo && faseObjetivo < 0 && fasesValidas() == selFases.valida && duelo->activo == 0 && !duelo->ocupado())) {
		cerrarSelectorFases("el duelo cambio");
		return false;
	}
	if (p & (B_CIRCULO | B_OPTIONS)) {
		cerrarSelectorFases(p & B_OPTIONS ? "OPTIONS" : "circulo");
		return true;
	}
	int o = selFases.opcion;
	if (p & (B_IZQ | B_ARRIBA)) {
		for (int k = o - 1; k >= 0; k--) {
			if (selFases.valida[(size_t)k]) {
				o = k;
				break;
			}
		}
	}
	if (p & (B_DER | B_ABAJO)) {
		for (int k = o + 1; k < SEL_FASES; k++) {
			if (selFases.valida[(size_t)k]) {
				o = k;
				break;
			}
		}
	}
	if (o != selFases.opcion) {
		selFases.opcion = o;
		selFases.tElegida = 0.0f;
		sfxMD.push_back("sfx_cursor");   // (SE_DUEL_SELECT)
		anotar("selector de fases: %s", textoSelector(o));
	}
	if (p & B_X) {
		if (!selFases.valida[(size_t)selFases.opcion]) {
			avisar("Ahora no se puede cambiar de fase");
			return true;
		}
		const int destino = SEL_DESTINO[selFases.opcion];
		sfxMD.push_back("sfx_confirmar");   // (SE_DUEL_DECIDE)
		cerrarSelectorFases("eleccion");
		irAFase(destino);
	}
	return true;   // (abierto se queda con todos los botones: el foco no se mueve debajo)
}

// pasa de fase hacia "destino" (interfaz::FASES): la primera ahora, el resto en seguirFaseObjetivo
void irAFase(int destino)
{
	anotar("ir a la fase %d desde la %d", destino, fase);
	if (!duelo) {
		fase = destino;
		tFase = 0.0f;
		emitir({ EV_FASE, -1, -1, fase, "" });
		return;
	}
	faseObjetivo = destino;
	faseObjetivoTurno = duelo->turno;
	if (!siguienteFase()) faseObjetivo = -1;
	seguirFaseObjetivo();
}

// cada cuadro: si hay una fase elegida y el jugador quedo libre en el mismo turno, el siguiente J_FASE
void seguirFaseObjetivo()
{
	if (selectorEnEspera) {   // (rama classic-ps5-efectos3: el OPTIONS que quedo en espera)
		if (!duelo || duelo->ganador >= 0 || duelo->activo != 0 || duelo->turno != selectorEnEsperaTurno || selFases.abierto ||
		    (duelo->ef.preg.tipo != ia::P_NADA && duelo->ef.preg.lado == 0)) {
			selectorEnEspera = false;
		} else if (estado == NAVEGAR && puedeElegirFase()) {
			selectorEnEspera = false;
			anotar("OPTIONS en espera: se abre el selector de fases");
			abrirSelectorFases();
		}
	}
	if (faseObjetivo < 0) return;
	if (!duelo || duelo->ganador >= 0 || duelo->turno != faseObjetivoTurno || duelo->activo != 0 || faseUI(duelo->fase) >= faseObjetivo) {
		anotar("fase elegida %d: %s", faseObjetivo, duelo && duelo->turno == faseObjetivoTurno && duelo->ganador < 0 ? "llego" : "termino el turno");
		faseObjetivo = -1;
		return;
	}
	if (bloqueado || duelo->ocupado() || estado != NAVEGAR || vuelo.activo || (ataqueEnCurso && ataqueEnCurso())) return;   // (espera: ventanas, animaciones)
	anotar("fase elegida %d: sigue (fase %d)", faseObjetivo, faseUI(duelo->fase));
	const int antes = (int)duelo->fase;
	if (!siguienteFase() || ((int)duelo->fase == antes && !duelo->ocupado())) faseObjetivo = -1;   // (el motor no lo acepta: ya aviso)
}

// el boton de fase del HUD: "" si OPTIONS ahora no abre el selector (la placa y la barra de ayuda)
bool selectorDisponible() const { return !selFases.abierto && puedeElegirFase(); }
