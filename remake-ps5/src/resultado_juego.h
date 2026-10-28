	// Yugioh Classic Remaster (rama classic-resultado): se incluye DENTRO de la clase del juego, despues de duelo_juego.h.
	// El final del duelo (punto 10a de la auditoria) y la Fase de Espera visible:
	//  - seguirResultado (cada cuadro del duelo): lleva el resumen (danno, cartas usadas, MVP) con los sucesos ya
	//    presentados, la musica de la carta clave y el cartel de la Fase de Espera.
	//  - prepararResultado (al terminar): el motivo, lo desbloqueado (cartas firma, siguiente rival) y la musica de
	//    victoria / derrota (despues del sfx de siempre).
	//  - avanzarResultado: Revancha (mismo rival, con su lanzamiento de moneda y un plan nuevo) / Elegir otro rival /
	//    Menu principal. Circulo sigue llevando al menu.
	//  - rendirse: Abandonar duelo (pausa) termina el duelo con su pantalla (motivo: rendicion).
	// Los datos van en resultado::resumen() (resultado.h) y los dibuja resultado_ui.h.

	// ---- revancha: el rival del duelo en curso (lo que elegirRival necesita para volver a el)
	struct Revancha {
		bool hay{ false };
		int modo{ 0 }, saga{ 0 }, idx{ 0 }, nivel{ 1 }, rivalIdx{ -1 };
		std::string personaje, rival;
	} revancha;

	// ---- resumen del duelo en curso
	size_t resumenVistos{ 0 };
	bool combateJ{ false };          // el danno que llega es de un ataque del jugador
	std::string atacanteJ;
	std::vector<std::pair<std::string, int>> dannoPorCarta;
	std::vector<std::string> mtColocadasJ;   // magias/trampas que el jugador coloco (al activarlas no cuentan otra vez)
	std::vector<std::string> firmaBloqueadas;   // las firma del personaje que aun no tenia al empezar
	bool claveSonando{ false };      // musica_carta_clave (hasta el final o la musica tensa)
	bool rendido{ false };
	float tEspera{ 0.0f };           // s que le quedan al cartel de la Fase de Espera
	static constexpr float T_ESPERA = 0.967f;   // (rama classic-ps5-ritmo: MD DuelStanbyPhase 0.967 s; antes 0.8)
	bool musicaFinPuesta{ false };

	// en empezarDueloIA (antes del sorteo): este rival es el de la revancha
	void guardarRevancha()
	{
		const VD& v = ctl.vista;
		revancha.hay = mazoRivalIA.rival != nullptr;
		revancha.modo = v.modo;
		revancha.saga = v.saga;
		revancha.idx = v.idx;
		revancha.nivel = v.nivel;
		revancha.rivalIdx = rivalIdx;
		revancha.personaje = v.personaje;
		revancha.rival = mazoRivalIA.rival ? mazoRivalIA.rival->clave : std::string();
	}

	// en arrancarDueloIA: resumen en cero, la velocidad elegida y las firma que se pueden ganar
	void reiniciarResumen()
	{
		resultado::resumen() = resultado::Resumen{};
		resumenVistos = 0;
		combateJ = false;
		atacanteJ.clear();
		dannoPorCarta.clear();
		mtColocadasJ.clear();
		claveSonando = rendido = musicaFinPuesta = false;
		tEspera = 0.0f;
		firmaBloqueadas.clear();
		const VD& v = ctl.vista;
		if (v.modo == VD::HISTORIA && !v.personaje.empty())
			if (const personajes::Personaje* x = historia.buscar(v.personaje))
				for (const std::string& id : x->firma) {
					const mazo::Carta* k = coleccion.carta(id);
					if (k && k->bloqueada && std::find(firmaBloqueadas.begin(), firmaBloqueadas.end(), id) == firmaBloqueadas.end()) firmaBloqueadas.push_back(id);
				}
		leerVelocidad();   // (velocidad_juego.h)
	}

	// carta clave: una carta firma de algun personaje o una emblematica (tiene figura 3D)
	bool esCartaClave(const std::string& id) const
	{
		if (id.empty()) return false;
		for (const personajes::Personaje& x : historia.lista)
			if (std::find(x.firma.begin(), x.firma.end(), id) != x.firma.end()) return true;
		return figuraDe(id) >= 0;
	}

	// los sucesos ya presentados (hasta 'hasta') al resumen
	void contarSucesos(size_t hasta)
	{
		const auto& su = dueloIA.sucesos;
		resultado::Resumen& r = resultado::resumen();
		hasta = std::min(hasta, su.size());
		for (; resumenVistos < hasta; resumenVistos++) {
			const ia::Suceso& s = su[resumenVistos];
			switch (s.tipo) {
			case ia::S_INVOCA:
				combateJ = false;
				if (s.lado != 0) break;
				r.invocaciones++;
				r.cartasUsadas++;
				if (!claveSonando && esCartaClave(s.carta)) {
					claveSonando = true;
					r.cartaClave = s.carta;
					fprintf(stderr, "[resultado] carta clave invocada: %s (musica_carta_clave)\n", s.carta.c_str());
				}
				break;
			case ia::S_COLOCA:
				if (s.lado == 0) r.cartasUsadas++;
				break;
			case ia::S_COLOCA_MT:
				if (s.lado == 0) {
					r.cartasUsadas++;
					mtColocadasJ.push_back(s.carta);
				}
				break;
			case ia::S_ACTIVA_MT:
			case ia::S_ACTIVA_EFECTO:
				combateJ = false;
				if (s.lado != 0) break;
				r.activaciones++;
				if (s.tipo == ia::S_ACTIVA_MT) {
					auto it = std::find(mtColocadasJ.begin(), mtColocadasJ.end(), s.carta);
					if (it != mtColocadasJ.end()) mtColocadasJ.erase(it);   // (ya conto al colocarla)
					else r.cartasUsadas++;
				}
				break;
			case ia::S_COMBATE:
				combateJ = s.lado == 0;
				atacanteJ = s.carta;
				break;
			case ia::S_DANNO:
				if (s.lado == 1) {
					r.dannoHecho += s.valor;
					if (combateJ && !atacanteJ.empty()) {
						auto it = std::find_if(dannoPorCarta.begin(), dannoPorCarta.end(), [&](const std::pair<std::string, int>& p) { return p.first == atacanteJ; });
						if (it == dannoPorCarta.end()) dannoPorCarta.push_back({ atacanteJ, s.valor });
						else it->second += s.valor;
					}
				} else {
					r.dannoRecibido += s.valor;
				}
				break;
			case ia::S_FASE:
			case ia::S_TURNO:
			case ia::S_CADENA:
			case ia::S_RESUELVE:
				combateJ = false;
				break;
			default: break;
			}
		}
	}

	// ---- Fase de Espera (ia::Fase no la tiene: el motor pasa de robo a principal 1). Al presentar la principal 1, el
	// indicador se queda T_ESPERA s en ESPERA (SP, con su cartel y sfx_fase_1) y luego pasa a PRINCIPAL 1
	void mostrarFase(int faseIA)
	{
		if (faseIA == (int)ia::F_PRINCIPAL1) {
			tEspera = T_ESPERA;
			esperaEfecto = std::max(esperaEfecto, T_ESPERA);   // (el duelo espera al cartel)
			ctl.fase = 1;
		} else {
			tEspera = 0.0f;
			ctl.fase = faseInterfaz((ia::Fase)faseIA);
		}
		ctl.tFase = 0.0f;
	}
	int faseVisible(int f) const { return tEspera > 0.0f ? 1 : f; }

	// cada cuadro del duelo (avanzarDueloIA, con el dt ya escalado), despues de presentar
	void seguirResultado(bool alDia, float dt)
	{
		animandoDuelo = !alDia;   // (velocidad_juego.h)
		if (tEspera > 0.0f) {
			tEspera -= dt;
			if (tEspera <= 0.0f) {
				tEspera = 0.0f;
				ctl.fase = faseInterfaz(dueloIA.fase);
				ctl.tFase = 0.0f;
			}
		}
		contarSucesos(sucesosVistos);
	}

	// ---- el final
	resultado::Motivo motivoFin() const
	{
		if (enLineaActivo() && motivoRed() != resultado::M_NINGUNO) return motivoRed();   // rama classic-en-linea: rendicion del rival, tiempo
		if (rendido) return resultado::M_RENDICION;
		const int g = dueloIA.ganador;
		if (g == 2) return dueloIA.l[0].lp <= 0 && dueloIA.l[1].lp <= 0 ? resultado::M_LP_AMBOS : resultado::M_EFECTO;
		if (g != 0 && g != 1) return resultado::M_NINGUNO;
		const ia::Lado& gana = dueloIA.l[(size_t)g];
		const ia::Lado& pierde = dueloIA.l[(size_t)(1 - g)];
		if (pierde.lp <= 0) return resultado::M_LP;
		static const char* const PIEZAS[] = { "exodia_the_forbidden_one", "left_arm_of_the_forbidden_one", "right_arm_of_the_forbidden_one",
			                                  "left_leg_of_the_forbidden_one", "right_leg_of_the_forbidden_one" };
		bool exodia = true;
		for (const char* p : PIEZAS) exodia = exodia && std::find(gana.mano.begin(), gana.mano.end(), p) != gana.mano.end();
		if (exodia) return resultado::M_EXODIA;
		if (ia::mensajesEnCampo(dueloIA, g) >= 4) return resultado::M_DESTINY;
		if (dueloIA.ef.cuentaFinal[(size_t)g] >= 0 && dueloIA.turno > dueloIA.ef.cuentaFinal[(size_t)g]) return resultado::M_CUENTA_FINAL;
		if (pierde.mazo.empty()) return resultado::M_SIN_MAZO;
		return resultado::M_EFECTO;
	}

	// al final de terminarDueloIA (ya con la victoria/derrota contada y la pantalla RESULTADO)
	void prepararResultado()
	{
		const VD& v = ctl.vista;
		contarSucesos(dueloIA.sucesos.size());
		resultado::Resumen& r = resultado::resumen();
		r.activo = true;
		r.ganador = v.ganador;
		r.motivo = motivoFin();
		r.turnos = dueloIA.turno;
		r.mvp.clear();
		r.mvpDanno = 0;
		for (const auto& p : dannoPorCarta)
			if (p.second > r.mvpDanno) {
				r.mvp = p.first;
				r.mvpDanno = p.second;
			}
		r.firmaNuevas.clear();
		if (v.personajeNuevo)
			for (const std::string& id : firmaBloqueadas)
				if (const mazo::Carta* k = coleccion.carta(id))
					if (!k->bloqueada) r.firmaNuevas.push_back(id);
		// el siguiente rival de la historia (el primero sin vencer de las sagas abiertas)
		r.siguienteEs = r.siguienteEn = "";
		if (v.ganador == 0 && v.modo == VD::HISTORIA && !historia.lista.empty()) {
			for (int sg = 0; sg < personajes::NUM_SAGAS && r.siguienteEs.empty(); sg++) {
				if (!historia.sagaAbierta(sg)) break;
				for (int i : historia.deSaga(sg)) {
					const personajes::Personaje& x = historia.lista[(size_t)i];
					if (historia.vencido(x.id)) continue;
					r.siguienteEs = x.nombreEs;
					r.siguienteEn = x.nombreEn;
					break;
				}
			}
		}
		r.revancha = revancha.hay;
		r.enLinea = enLineaActivo();   // rama classic-en-linea: revancha en linea (si los dos la piden) y "Buscar otro rival"
		if (r.enLinea) r.revancha = revanchaRedPosible();
		r.opcion = resultado::O_REVANCHA;
		// sonido: el sfx de siempre y, al terminar, la musica de victoria o de derrota (resultadoMusica)
		claveSonando = false;
		sonido.pararMusica(0.8f);
		musicaActual.clear();
		musicaFinPuesta = false;
		sonido.sfx(v.ganador == 0 ? "sfx_victoria" : v.ganador == 2 && sonido.hay("sfx_empate") ? "sfx_empate" : "sfx_derrota");
		fprintf(stderr, "[resultado] %s por %s: %d turnos, danno %d hecho / %d recibido, %d cartas usadas, MVP %s (%d), %d firma nuevas, siguiente %s\n",
		        v.ganador == 0 ? "victoria" : v.ganador == 1 ? "derrota" : "empate", resultado::nombreMotivo(r.motivo), r.turnos, r.dannoHecho, r.dannoRecibido,
		        r.cartasUsadas, r.mvp.empty() ? "-" : r.mvp.c_str(), r.mvpDanno, (int)r.firmaNuevas.size(), r.siguienteEs.empty() ? "-" : r.siguienteEs.c_str());
		registrarEstadisticas();   // rama classic-extras: por rival, racha, mazo y cartas jugadas (cosmeticos_juego.h)
	}

	// Abandonar duelo (pausa): derrota por rendicion, con su pantalla de resultado
	void rendirse()
	{
		if (enLineaActivo()) {   // rama classic-en-linea: rendirse en linea cuenta como derrota (el rival gana)
			rendirseRed();
			return;
		}
		if (ctl.vista.sorteo) {   // (todavia en el lanzamiento de moneda: el duelo anterior no cuenta)
			ctl.vista.sorteo = false;
			reiniciarResumen();
			dueloIA.ganador = -1;
		}
		rendido = true;
		dueloIA.terminar(1);
		terminarDueloIA();
	}

	// la revancha: el mismo rival (su plan y su mazo se vuelven a armar) y otra vez el lanzamiento de moneda
	void revanchaDuelo()
	{
		VD& v = ctl.vista;
		if (!revancha.hay) {
			irAElegirRival();
			return;
		}
		v.modo = revancha.modo;
		v.saga = revancha.saga;
		v.idx = revancha.idx;
		v.nivel = revancha.nivel;
		rivalIdx = revancha.rivalIdx;
		v.pantalla = VD::ELEGIR_RIVAL;
		v.t = 0.0f;
		elegirRival(true);
		const bool mismo = mazoRivalIA.rival && mazoRivalIA.rival->clave == revancha.rival && (revancha.personaje.empty() || v.personaje == revancha.personaje);
		fprintf(stderr, "[resultado] revancha contra %s%s\n", revancha.rival.c_str(), mismo ? "" : " (no se encontro: elegir rival)");
		if (!mismo) {
			irAElegirRival();
			return;
		}
		musica("musica_menu");
		empezarDueloIA();
	}

	// la pantalla de resultado con el mando (devuelve true: se queda con el mando)
	bool avanzarResultado(uint32_t p)
	{
		VD& v = ctl.vista;
		resultado::Resumen& r = resultado::resumen();
		// la musica de victoria / derrota, cuando termina el sfx
		if (!musicaFinPuesta && v.t > (v.ganador == 0 ? 5.2f : 5.8f)) {
			musicaFinPuesta = true;
			const char* m = v.ganador == 0 ? "musica_victoria" : "musica_derrota";
			if (sonido.hay(m)) musica(m, 2.0f);
		}
		if (v.t < 0.8f) return true;
		const int n = resultado::NUM_OPCIONES;
		const int antes = r.opcion;
		if (p & (control::B_IZQ | control::B_ARRIBA)) r.opcion = (r.opcion + n - 1) % n;
		if (p & (control::B_DER | control::B_ABAJO)) r.opcion = (r.opcion + 1) % n;
		if (!r.revancha && r.opcion == resultado::O_REVANCHA) r.opcion = antes == resultado::O_OTRO_RIVAL ? resultado::O_MENU : resultado::O_OTRO_RIVAL;
		if (r.opcion != antes) sonido.sfx("sfx_menu_cursor", 0.7f);
		if (p & control::B_X) {
			sonido.sfx("sfx_menu_confirmar");
			r.activo = false;
			fprintf(stderr, "[resultado] elige '%s'\n", resultado::textoOpcion(r.opcion));
			switch (r.opcion) {
			case resultado::O_REVANCHA: revanchaDuelo(); break;
			case resultado::O_OTRO_RIVAL: irAElegirRival(); break;
			default: irAMenuPrincipal(); break;
			}
		}
		return true;
	}

#if defined(PS5_HOST_REFERENCE)
	// PC (capturas): YUGI_RESULTADO=victoria|derrota|exodia|rendicion salta el sorteo y, a los 3 s del duelo, lo termina
	float tPruebaResultado{ 0.0f };
	void pruebaResultado(float dt)
	{
		pruebaCartaClave(dt);   // (rama classic-visual-final: YUGI_CARTA_CLAVE, entrada_clave_juego.h)
		const char* e = getenv("YUGI_RESULTADO");
		if (!e || ctl.vista.pantalla != VD::DUELO) return;
		if (ctl.vista.sorteo) {
			ctl.vista.sorteo = false;
			arrancarDueloIA(0);
			tPruebaResultado = 0.0f;
			return;
		}
		if (dueloIA.ganador >= 0) return;
		tPruebaResultado += dt;
		if (tPruebaResultado < 3.0f) return;
		const std::string m = e;
		resultado::Resumen& r = resultado::resumen();
		r.cartasUsadas += 7;
		r.invocaciones += 3;
		// (rama classic-visual-final) el marcador de fondo coherente con el resumen: LP = 8000 - el danno de cada lado
		// (0 el que pierde por LP), puestos de una vez en la mesa
		const bool ganaLP = m == "victoria" || (m != "derrota" && m != "exodia" && m != "rendicion");
		const int hecho = ganaLP ? 8000 : 2600, recibido = m == "derrota" ? 8000 : 3100;
		r.dannoHecho += hecho;
		r.dannoRecibido += recibido;
		dannoPorCarta.push_back({ "blue_eyes", 6000 });
		dueloIA.l[0].lp = std::max(0, ia::LP_INICIAL - recibido);
		dueloIA.l[1].lp = std::max(0, ia::LP_INICIAL - hecho);
		interfazDuelo.fijarLP(false, dueloIA.l[0].lp);
		interfazDuelo.fijarLP(true, dueloIA.l[1].lp);
		if (m == "rendicion") {
			rendirse();
		} else if (m == "exodia") {
			for (const char* p : { "exodia_the_forbidden_one", "left_arm_of_the_forbidden_one", "right_arm_of_the_forbidden_one", "left_leg_of_the_forbidden_one",
			                       "right_leg_of_the_forbidden_one" })
				dueloIA.l[0].mano.push_back(p);
			dueloIA.terminar(0);
		} else {
			const int pierde = m == "derrota" ? 0 : 1;
			dueloIA.l[(size_t)pierde].lp = 0;
			dueloIA.terminar(1 - pierde);
		}
	}
#endif
