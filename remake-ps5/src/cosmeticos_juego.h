	// Yugioh Classic Remaster (rama classic-extras): se incluye DENTRO de la clase del juego (yugireload.cpp), despues de
	// menu_principal.h. Personalizacion (Opciones > Personalizacion: Funda, Tapete, Controles, Estadisticas):
	//  - prepararCosmeticos (al arrancar, desde prepararMenu): funda, tapete y camara de opciones.txt, estadisticas.txt
	//    y los desbloqueos (personaje vencido en la historia; sin historia, todo libre).
	//  - avanzarPersonalizacion: el mando en sus pantallas (lo llama avanzarOpciones de menu_principal.h).
	//  - registrarEstadisticas: al terminar cada duelo (gancho en prepararResultado, resultado_juego.h).
	//  - recordarCamara: L1/R1 en el duelo (cambiarCamara) guarda camara=N en opciones.txt; al arrancar se usa.
	//  - tapeteMesa: el tapete fijo elegido (un material de campos.h, cobertura completa) si no manda una carta de campo.
	// Los datos van en cosmeticos::estado()/vista() y estadisticas::datos(); los dibuja cosmeticos_ui.h.

	void prepararCosmeticos()
	{
		const std::string dir = rutaOpciones.substr(0, rutaOpciones.size() - std::string("opciones.txt").size());
		cosmeticos::leer(rutaOpciones);
		estadisticas::leer(dir + "estadisticas.txt");
		actualizarDesbloqueos();
		aplicarCamaraGuardada();
	}

	// funda / tapete libres: su personaje ya fue vencido en la historia (sin personajes cargados, todo libre)
	void actualizarDesbloqueos()
	{
		cosmeticos::Estado& e = cosmeticos::estado();
		auto poner = [&](const char* id, bool& libre, std::string& es, std::string& en) {
			es = en = id;
			libre = !*id || historia.lista.empty() || historia.vencido(id);
			if (const personajes::Personaje* x = *id ? historia.buscar(id) : nullptr) {
				es = x->nombreEs;
				en = x->nombreEn;
			}
		};
		for (int k = 0; k < cosmeticos::NUM_FUNDAS; k++) poner(cosmeticos::FUNDAS[k].personaje, e.fundaLibre[k], e.fundaPorEs[k], e.fundaPorEn[k]);
		for (int k = 0; k < cosmeticos::NUM_TAPETES; k++) poner(cosmeticos::TAPETES[k].personaje, e.tapeteLibre[k], e.tapetePorEs[k], e.tapetePorEn[k]);
	}

	// la camara del ultimo duelo (opciones.txt camara=N), sin transicion
	void aplicarCamaraGuardada()
	{
#if defined(PS5_HOST_REFERENCE)
		if (getenv("YUGI_CAMARA")) return;   // (PC: la variable manda)
#endif
		const int c = cosmeticos::estado().camara;
		if (c < 0 || c >= interfaz::NUM_CAMARAS || c == modoCamara) return;
		modoCamara = c;
		if (modoCamara == interfaz::CAM_LIBRE) libreDesde(Vista{ OJO, OBJETIVO }, libreGiro, libreElev, libreDist);
		tCambioCamara = 99.0f;
		fprintf(stderr, "[cosmeticos] camara guardada: %s\n", interfaz::CLAVE_CAMARA[c]);
	}
	void recordarCamara(int modo)
	{
		cosmeticos::Estado& e = cosmeticos::estado();
		if (e.camara == modo || e.ruta.empty()) return;
		e.camara = modo;
		cosmeticos::guardar();
	}

	// ---- tapete fijo (nullptr: el automatico, o manda la carta de campo / el Bosque)
	const campos::Campo* tapeteMesa() const
	{
		const int k = cosmeticos::tapeteVisible();
		if (k == cosmeticos::T_AUTO) return nullptr;
		const VD& v = ctl.vista;
		if (v.pantalla != VD::DUELO && v.pantalla != VD::RESULTADO) return nullptr;
		if (bosqueVisible() || (estadoCampo == CAMPO_ACTIVO && campoMesa())) return nullptr;
		return campos::buscar(cosmeticos::TAPETES[k].campo);
	}

	// ---- estadisticas: al terminar cada duelo
	void registrarEstadisticas()
	{
		const VD& v = ctl.vista;
		if (enLineaActivo()) {   // rama classic-en-linea: aparte de los duelos contra la IA
			estadisticas::registrarEnLinea(v.ganador);
			return;
		}
		std::string clave, es, en;
		if (v.modo == VD::HISTORIA && !v.personaje.empty())
			if (const personajes::Personaje* x = historia.buscar(v.personaje)) {
				clave = "p:" + x->id;
				es = x->nombreEs;
				en = x->nombreEn;
			}
		if (clave.empty() && mazoRivalIA.rival) {
			clave = mazoRivalIA.rival->clave;
			es = mazoRivalIA.rival->nombreEs;
			en = mazoRivalIA.rival->nombreEn;
		}
		// las cartas que jugo el jugador (invocadas, colocadas o activadas; una magia colocada y luego activada, una vez)
		std::vector<std::string> jugadas, colocadas;
		for (const ia::Suceso& s : dueloIA.sucesos) {
			if (s.lado != 0 || s.carta.empty()) continue;
			if (s.tipo == ia::S_INVOCA || s.tipo == ia::S_COLOCA) {
				jugadas.push_back(s.carta);
			} else if (s.tipo == ia::S_COLOCA_MT) {
				jugadas.push_back(s.carta);
				colocadas.push_back(s.carta);
			} else if (s.tipo == ia::S_ACTIVA_MT) {
				auto it = std::find(colocadas.begin(), colocadas.end(), s.carta);
				if (it != colocadas.end()) colocadas.erase(it);
				else jugadas.push_back(s.carta);
			}
		}
		estadisticas::registrar(clave, es, en, v.ganador, mazoJugador.nombre, jugadas);
		actualizarDesbloqueos();   // (vencer a un personaje libera su funda o su tapete)
	}

	// los datos de la pantalla que vienen del juego
	void prepararPantallaEstadisticas()
	{
		estadisticas::Datos& d = estadisticas::datos();
		d.victoriasTotal = std::max(ctl.vista.victorias, d.victorias);
		d.derrotasTotal = std::max(ctl.vista.derrotas, d.derrotas);
		std::vector<std::string> ids;
		for (const personajes::Personaje& x : historia.lista)
			for (const std::string& id : x.firma)
				if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
		d.firmaTotal = (int)ids.size();
		d.firmaLibres = 0;
		for (const std::string& id : ids) {
			const mazo::Carta* k = coleccion.carta(id);
			d.firmaLibres += k && !k->bloqueada ? 1 : 0;
		}
		std::stable_sort(d.rivales.begin(), d.rivales.end(), [](const estadisticas::Rival& a, const estadisticas::Rival& b) { return a.duelos() > b.duelos(); });
	}

	// ---- el mando en Personalizacion (true: se quedo con el mando)
	void abrirPersonalizacion()
	{
		cosmeticos::VistaPerso& pv = cosmeticos::vista();
		pv.pantalla = cosmeticos::P_MENU;
		pv.opcion = 0;
		pv.t = 0.0f;
		actualizarDesbloqueos();
		fprintf(stderr, "[cosmeticos] personalizacion abierta\n");
	}
	void irPerso(int pantalla)
	{
		cosmeticos::VistaPerso& pv = cosmeticos::vista();
		pv.pantalla = pantalla;
		pv.t = 0.0f;
		pv.desplazar = 0;
		pv.aviso.clear();
		pv.tAviso = 99.0f;
		if (pantalla == cosmeticos::P_FUNDA) pv.sel = cosmeticos::estado().funda;
		if (pantalla == cosmeticos::P_TAPETE) pv.sel = cosmeticos::estado().tapete;
		if (pantalla == cosmeticos::P_CONTROLES) {
			pv.sel = 0;
			pv.pestana = ctl.vista.pantalla == VD::DUELO ? cosmeticos::PEST_DUELO : cosmeticos::PEST_MENUS;
		}
		if (pantalla == cosmeticos::P_ESTADISTICAS) prepararPantallaEstadisticas();
	}
	void avanzarPersonalizacion(uint32_t p)
	{
		cosmeticos::VistaPerso& pv = cosmeticos::vista();
		cosmeticos::Estado& e = cosmeticos::estado();
		auto cursor = [&]() { sonido.sfx("sfx_menu_cursor", 0.7f); };
		if (pv.pantalla == cosmeticos::P_MENU) {
			int& o = pv.opcion;
			if (p & control::B_ARRIBA) o = (o + cosmeticos::NUM_PERSO - 1) % cosmeticos::NUM_PERSO;
			if (p & control::B_ABAJO) o = (o + 1) % cosmeticos::NUM_PERSO;
			if (p & (control::B_ARRIBA | control::B_ABAJO)) cursor();
			if ((p & control::B_CIRCULO) || ((p & control::B_X) && o == cosmeticos::PERSO_VOLVER)) {
				sonido.sfx("sfx_menu_cancelar");
				pv.pantalla = cosmeticos::P_CERRADA;
			} else if (p & control::B_X) {
				sonido.sfx("sfx_menu_confirmar");
				irPerso(o == cosmeticos::PERSO_FUNDA ? cosmeticos::P_FUNDA : o == cosmeticos::PERSO_TAPETE ? cosmeticos::P_TAPETE : o == cosmeticos::PERSO_CONTROLES ? cosmeticos::P_CONTROLES : cosmeticos::P_ESTADISTICAS);
			}
			return;
		}
		if (p & control::B_CIRCULO) {
			sonido.sfx("sfx_menu_cancelar");
			pv.pantalla = cosmeticos::P_MENU;
			pv.t = 0.0f;
			return;
		}
		if (pv.pantalla == cosmeticos::P_FUNDA || pv.pantalla == cosmeticos::P_TAPETE) {
			const bool fu = pv.pantalla == cosmeticos::P_FUNDA;
			const int n = fu ? (int)cosmeticos::NUM_FUNDAS : (int)cosmeticos::NUM_TAPETES, cols = fu ? 5 : 1;
			int& k = pv.sel;
			const int antes = k;
			if (p & control::B_IZQ) k = std::max(0, k - 1);
			if (p & control::B_DER) k = std::min(n - 1, k + 1);
			if (p & control::B_ARRIBA) k = fu ? (k - cols >= 0 ? k - cols : k) : (k + n - 1) % n;
			if (p & control::B_ABAJO) k = fu ? std::min(n - 1, k + cols) : (k + 1) % n;
			if (k != antes) {
				cursor();
				pv.aviso.clear();
			}
			if (p & control::B_X) {
				const bool libre = fu ? e.fundaLibre[k] : e.tapeteLibre[k];
				if (libre) {
					(fu ? e.funda : e.tapete) = k;
					cosmeticos::guardar();
					sonido.sfx("sfx_menu_confirmar");
					fprintf(stderr, "[cosmeticos] %s: %s\n", fu ? "funda" : "tapete", fu ? cosmeticos::FUNDAS[k].clave : cosmeticos::TAPETES[k].clave);
				} else {
					sonido.sfx("sfx_menu_cancelar");
					pv.aviso = "x";   // (la interfaz arma el texto en el idioma actual)
					pv.tAviso = 0.0f;
				}
			}
			return;
		}
		if (pv.pantalla == cosmeticos::P_CONTROLES) {
			const int antesP = pv.pestana;
			if (p & (control::B_R1 | control::B_DER)) pv.pestana = (pv.pestana + 1) % cosmeticos::NUM_PESTANAS;
			if (p & (control::B_L1 | control::B_IZQ)) pv.pestana = (pv.pestana + cosmeticos::NUM_PESTANAS - 1) % cosmeticos::NUM_PESTANAS;
			if (pv.pestana != antesP) {
				pv.sel = 0;
				cursor();
			}
			const int n = (int)cosmeticos::controles(pv.pestana).size();
			if (p & control::B_ARRIBA) pv.sel = (pv.sel + n - 1) % n;
			if (p & control::B_ABAJO) pv.sel = (pv.sel + 1) % n;
			if (p & (control::B_ARRIBA | control::B_ABAJO)) cursor();
			return;
		}
		if (pv.pantalla == cosmeticos::P_ESTADISTICAS) {
			const int n = (int)estadisticas::datos().rivales.size();
			const int antes = pv.desplazar;
			if (p & control::B_ARRIBA) pv.desplazar = std::max(0, pv.desplazar - 1);
			if (p & control::B_ABAJO) pv.desplazar = std::max(0, std::min(n - 8, pv.desplazar + 1));
			if (pv.desplazar != antes) cursor();
		}
	}
