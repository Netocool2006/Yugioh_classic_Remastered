	// Yugioh Classic Remaster (rama classic-en-linea): se incluye DENTRO de la clase del juego (yugireload.cpp y
	// pruebas/cu_juego.h), despues de menu_principal.h. El modo DUELO EN LINEA (docs/en_linea.md):
	//  - Menu (VD::EN_LINEA, lo dibuja en_linea_ui.h): Buscar rival (sala de Cloudflare; sin URL o sin respuesta, la DHT:
	//    red/sala.h), Red local (red/lan.h), Codigo de conexion (red/nat.h: el propio y el teclado para el del rival) y el
	//    mazo con el que se juega. La primera vez, el aviso de privacidad (opciones.txt aviso_ip=1).
	//  - Buscando: contador de la sala, tiempo de espera, ELO; "Jugar contra la IA mientras espero": la busqueda sigue y,
	//    al aparecer rival, un cartel en pleno duelo (X ir / circulo rechazar, 15 s); el duelo contra la IA no cuenta.
	//  - Conexion: un hilo reune los candidatos (STUN, UPnP) y otro perfora (nat.h) sin bloquear el cuadro; el duelo va
	//    por red::DueloRed (lockstep) sobre el socket del juego. El rival remoto reemplaza a la IA: dueloIA pasa a ser la
	//    VISTA del duelo en red (red/espejo.h: el invitado es el lado 1 del motor pero se ve abajo) y las jugadas del
	//    mando van a DueloRed::jugar (con espejoJugada para el invitado).
	//  - Reloj del turno (en_linea.h): corre solo para el que decide; si se agota, ESTA maquina manda AGOTADO y la jugada
	//    automatica (el otro solo la recibe). 3 agotamientos = derrota.
	//  - Desconexion: cartel "Esperando al rival... (reconectando N s)"; a los 30 s el duelo termina SIN contar (ni
	//    estadisticas ni ELO). La rendicion cuenta como derrota normal.
	//  - Resultado: ELO antes/despues cuando la sala lo confirma; revancha en linea si los dos la piden.
	// Las pruebas (pruebas/prueba_red_front.cpp) cambian enl.* (sin STUN ni UPnP, reloj virtual, sala local) antes de entrar.

	// el transporte del duelo: el socket del juego, contestando los sondeos tardios de la perforacion (nat.h)
	struct TransporteJuego : red::Transporte {
		red::SocketUdp* s{ nullptr };
		uint64_t idSondeo{ 0 };
		int sondeos{ 0 };
		bool enviar(const red::Direccion& a, const uint8_t* d, size_t n) override { return s && s->enviar(a, d, n); }
		int recibir(red::Direccion& de, uint8_t* buf, size_t cap) override
		{
			for (int k = 0; k < 64 && s; k++) {
				const int n = s->recibir(de, buf, cap, 0);
				if (n <= 0) return n;
				if (red::esSondeo(buf, (size_t)n)) {
					sondeos += red::responderSondeo(*s, buf, (size_t)n, de, idSondeo) ? 1 : 0;
					continue;
				}
				return n;
			}
			return 0;
		}
	};
	struct SesionRed {
		std::shared_ptr<red::SocketUdp> socket;
		TransporteJuego transporte;
		red::DueloRed duelo;
		bool deSala{ false };       // la pareja vino de la sala (el resultado se reporta: ELO)
		uint64_t idSesion{ 0 };
		uint64_t inicio{ 0 };       // (ms) para rendirse si nunca conecta
		explicit SesionRed(const ia::Base* b) : duelo(b) {}
	};
	// un trabajo de red en un hilo (STUN/UPnP o perforacion): el cuadro solo mira "lista"
	struct TareaRed {
		std::atomic<bool> lista{ false };
		red::InfoRed info;
		bool ok{ false };
		red::Direccion elegido;
	};
	enum FaseRed { FR_NADA = 0, FR_REUNIENDO, FR_BUSCANDO, FR_PERFORANDO, FR_SESION, FR_CODIGO };
	enum SiguienteRed { SR_BUSCAR = 0, SR_CODIGO };
	enum OpcionEnLinea { OL_BUSCAR = 0, OL_LAN, OL_CODIGO, OL_MAZO, OL_VOLVER };
	struct EnLinea {
		// ---- configuracion (las pruebas la cambian antes de entrar)
		bool stun{ true }, upnp{ true }, v6{ true };
		uint16_t puerto{ red::PUERTO_JUEGO };
		std::vector<red::Direccion> localesExtra;   // candidatos de mas (pruebas: 127.0.0.1)
		red::SalaConfig sala;
		uint16_t puertoLan{ red::PUERTO_LAN };
		std::vector<red::Direccion> lanExtra;       // destinos de mas del anuncio (pruebas: 127.0.0.1)
		bool relojVirtual{ false };                 // (pruebas) los ms avanzan con los cuadros
		uint64_t msVirtual{ 1000 };
		std::string nombre;                         // vacio: "Duelista XXXX" (del id anonimo)
		// ---- estado
		bool avisoIp{ false };                      // el aviso de privacidad ya se acepto (opciones.txt aviso_ip=1)
		bool avisoDesdeOpciones{ false };
		int fase{ FR_NADA }, siguiente{ SR_BUSCAR };
		std::unique_ptr<red::Sala> salaObj;
		std::shared_ptr<red::SocketUdp> socket;
		std::shared_ptr<TareaRed> tarea;
		red::InfoRed info;
		bool infoLista{ false };
		uint64_t idCodigo{ 0 };
		std::string codigo;
		std::unique_ptr<SesionRed> sesion;
		red::Pareja pareja;
		uint64_t inicioBusqueda{ 0 };
		red::AnuncioLan anuncio;
		red::BuscadorLan buscador;
		bool lanAbierta{ false }, anunciando{ false };
		bool upnpUsado{ false };                    // se abrio el puerto en el router (liberarUpnp al salir del juego)
		int mazo{ -1 };                             // indice en la biblioteca (-1: el activo)
		bool conIA{ false };                        // juega contra la IA mientras busca
		bool rivalEncontrado{ false };
		float rivalQueda{ 0.0f };
		std::string avisoRed, motivoCorte, aviso;
		float tAviso{ 99.0f };
		// ---- duelo
		bool enDuelo{ false };
		uint32_t pasoVisto{ 0 };
		int revanchasVistas{ 0 };
		float reloj[2]{ en_linea::RELOJ_TURNO_S, en_linea::RELOJ_TURNO_S };   // [0] el mio, [1] el del rival
		int turnoReloj{ -1 };
		bool alertaSonada{ false }, agotadoTurno{ false }, alDia{ false };
		int eloAntes{ -1 }, confirmacionesAntes{ 0 };
		bool reportado{ false }, rendidoYo{ false };
		resultado::Motivo motivo{ resultado::M_NINGUNO };
	} enl;

	uint64_t ahoraRed() const { return enl.relojVirtual ? enl.msVirtual : red::ahoraMs(); }
	bool enLineaActivo() const { return enl.enDuelo && enl.sesion != nullptr; }
	red::Sala& salaRed()
	{
		if (!enl.salaObj) enl.salaObj.reset(new red::Sala(enl.sala));
		return *enl.salaObj;
	}
	std::string nombreRed()
	{
		if (!enl.nombre.empty()) return enl.nombre;
#if defined(PS5_HOST_REFERENCE)
		if (const char* n = getenv("YUGI_NOMBRE")) return n;
#endif
		std::string id = salaRed().datosElo().id.substr(0, 4);
		for (char& c : id) c = (char)toupper((unsigned char)c);
		return "Duelista " + id;
	}
	static const char* plataformaRed()
	{
#if defined(PS5_HOST_REFERENCE) || defined(_WIN32)
		return "pc";
#else
		return "ps5";
#endif
	}
	red::Identidad identidadRed()
	{
		red::Identidad id;
		id.catalogo = red::hashCatalogo(baseIA);
		id.nombre = nombreRed();
		return id;
	}

	// ---- el mazo del duelo en linea (uno de la biblioteca; tiene que ser valido y legal para la red)
	const mazo::Mazo* mazoRed() const
	{
		if (enl.mazo >= 0 && enl.mazo < (int)biblioteca.mazos.size()) return &biblioteca.mazos[(size_t)enl.mazo];
		return &mazoJugador;
	}
	bool mazoRedValido(std::string* motivo = nullptr) const
	{
		const mazo::Mazo* m = mazoRed();
		if (!m->valido(coleccion, motivo)) return false;
		if (!red::mazoLegal(baseIA, m->principal, m->fusion)) {
			if (motivo) *motivo = idioma::t("no es legal para el duelo en línea (40-60 cartas, hasta 3 copias)", "not legal for online duels (40-60 cards, up to 3 copies)");
			return false;
		}
		return true;
	}

	// ---- pantallas
	void irAEnLinea(int pantalla)
	{
		VD& v = ctl.vista;
		pausa.abierta = false;
		mostrarPausa();
		v.pantalla = VD::EN_LINEA;
		v.t = 0.0f;
		v.sorteo = false;
		v.turnoRival = v.pensando = false;
		ctl.bloqueado = true;
		ctl.preparar({}, {}, {});
		ctl.manoR = 0;
		interfazDuelo.contarMazos(0, 0, 0);
		musica("musica_menu");
		v.enLinea.pantalla = pantalla;
		v.enLinea.t = 0.0f;
		v.enLinea.opcion = 0;
		v.enLinea.enDuelo = false;
		fprintf(stderr, "[en linea] pantalla %d\n", pantalla);
	}
	// desde el menu principal
	void entrarEnLinea()
	{
		salaRed().consultarEnLinea();
		if (enl.mazo < 0) enl.mazo = biblioteca.activo;
		irAEnLinea(enl.avisoIp ? en_linea::MENU : en_linea::AVISO_IP);
	}
	// el aviso de privacidad desde Opciones (menu_principal.h)
	void abrirAvisoIpDesdeOpciones()
	{
		enl.avisoDesdeOpciones = true;
		irAEnLinea(en_linea::AVISO_IP);
	}
	void avisoEnLinea(const std::string& a)
	{
		enl.aviso = a;
		enl.tAviso = 0.0f;
	}

	// ---- cerrar todo lo de red (al menu principal, al salir del juego)
	void cerrarSesionRed(bool avisar = true)
	{
		if (enl.sesion) {
			if (avisar) enl.sesion->duelo.salir(ahoraRed());   // (ADIOS: si el duelo seguia, el otro ve un abandono: no cuenta)
			enl.sesion.reset();
		}
		enl.enDuelo = false;
		ctl.vista.enLinea.enDuelo = false;
	}
	void cerrarRed()
	{
		cerrarSesionRed();
		if (enl.salaObj) enl.salaObj->cancelar();
		if (enl.anunciando) enl.anuncio.detener();
		if (enl.lanAbierta) enl.buscador.detener();
		enl.anunciando = enl.lanAbierta = false;
		enl.tarea.reset();
		enl.socket.reset();
		enl.infoLista = false;   // (la proxima vez se vuelven a reunir los candidatos; el mapeo UPnP se borra al salir del juego)
		enl.fase = FR_NADA;
		enl.conIA = enl.rivalEncontrado = false;
		ctl.vista.enLinea.pantalla = en_linea::CERRADA;
		ctl.vista.enLinea.rivalEncontrado = false;
	}

	// el socket del juego (puerto 47924; si esta ocupado, otro)
	bool abrirSocketRed()
	{
		if (enl.socket && enl.socket->abierto()) return true;
		auto s = std::make_shared<red::SocketUdp>();
		if (!s->abrir(enl.puerto, enl.v6) && !s->abrir(0, enl.v6) && !s->abrir(0, false)) {
			avisoEnLinea(idioma::tr("No se pudo abrir la red"));
			return false;
		}
		enl.socket = s;
		fprintf(stderr, "[en linea] socket del juego en el puerto %u\n", (unsigned)s->puertoLocal());
		return true;
	}
	// los candidatos (STUN, UPnP, LAN, IPv6) en un hilo; al terminar sigue con enl.siguiente
	void reunirRed(int siguiente)
	{
		enl.siguiente = siguiente;
		if (enl.infoLista) {
			seguirTrasReunir();
			return;
		}
		if (!abrirSocketRed()) return;
		auto t = std::make_shared<TareaRed>();
		auto s = enl.socket;
		red::OpcionesReunir op;
		op.usarUpnp = enl.upnp;
		op.resolverStun = enl.stun;
		op.archivoStun = red::archivoStun();
		std::vector<red::Direccion> extra = enl.localesExtra;
		std::thread([t, s, op, extra] {
			t->info = red::reunirCandidatosCon(*s, en_linea::REUNIR_MS, op);
			for (red::Direccion d : extra) {
				d.puerto = s->puertoLocal();
				red::Candidato c;
				c.dir = d;
				c.tipo = red::TipoCandidato::Local;
				c.prioridad = red::prioridadTipo(c.tipo) + 50;
				t->info.candidatos.insert(t->info.candidatos.begin(), c);
			}
			t->lista = true;
		}).detach();
		enl.tarea = t;
		enl.fase = FR_REUNIENDO;
	}
	void seguirTrasReunir()
	{
		if (enl.siguiente == SR_BUSCAR) {
			buscarRival();
		} else {
			mostrarCodigoPropio();
		}
	}
	void buscarRival()
	{
		red::Sala& sala = salaRed();
		sala.buscar(sala.perfilLocal(nombreRed(), red::VERSION_JUEGO, plataformaRed(), red::hex(red::hashCatalogo(baseIA))), enl.info.candidatos);
		enl.fase = FR_BUSCANDO;
		enl.inicioBusqueda = ahoraRed();
		enl.rivalEncontrado = false;
		fprintf(stderr, "[en linea] buscando rival (%d candidatos, NAT %s)\n", (int)enl.info.candidatos.size(), red::nombreNat(enl.info.nat));
	}

	// la configuracion del duelo en red (el mazo elegido, el nombre, "preguntar antes de activar")
	red::ConfigDuelo configRed()
	{
		red::ConfigDuelo c;
		c.principal = mazoRed()->principal;
		c.fusion = mazoRed()->fusion;
		c.nombre = nombreRed();
		c.preguntarSiempre = opciones::preguntarActivacion() == opciones::PREGUNTAR_SIEMPRE;
		c.agotamientosDerrota = en_linea::AGOTAMIENTOS_DERROTA;
		const std::string dir = rutaProgreso.substr(0, rutaProgreso.size() - std::string("progreso.txt").size());
		c.carpetaRegistro = dir;
		return c;
	}
	// la sesion del duelo en el socket del juego: anfitrion (escucha) o invitado (llama a "a")
	void crearSesionRed(bool anfitrion, const red::Direccion& a, uint64_t idSondeo, bool deSala)
	{
		enl.sesion.reset(new SesionRed(&baseIA));
		SesionRed& s = *enl.sesion;
		s.socket = enl.socket;
		s.transporte.s = enl.socket.get();
		s.transporte.idSondeo = idSondeo;
		s.deSala = deSala;
		s.idSesion = idSondeo;
		s.inicio = ahoraRed();
		const red::ConfigDuelo c = configRed();
		if (anfitrion) s.duelo.anfitrion(&s.transporte, c, ahoraRed());
		else s.duelo.invitado(&s.transporte, a, c, ahoraRed());
		enl.fase = FR_SESION;
		enl.revanchasVistas = 0;
		fprintf(stderr, "[en linea] sesion %s%s%s\n", anfitrion ? "anfitrion" : "invitado -> ", anfitrion ? "" : a.texto().c_str(), deSala ? " (sala)" : "");
	}
	// rival de la sala: perforar en un hilo y despues la sesion
	void conectarConPareja()
	{
		if (!abrirSocketRed()) return;
		if (enl.salaObj) enl.salaObj->pareja(enl.pareja);
		auto t = std::make_shared<TareaRed>();
		auto s = enl.socket;
		const std::vector<red::Candidato> cands = enl.pareja.candidatos;
		const uint64_t id = enl.pareja.idSesion;
		std::thread([t, s, cands, id] {
			t->ok = red::perforar(*s, cands, id, en_linea::CONECTAR_MS, t->elegido);
			t->lista = true;
		}).detach();
		enl.tarea = t;
		enl.fase = FR_PERFORANDO;
		irAEnLinea(en_linea::CONECTANDO);
		fprintf(stderr, "[en linea] rival %s (ELO %d, %s): perforando (%s)\n", enl.pareja.rival.nombre.c_str(), enl.pareja.rival.elo, enl.pareja.rival.plataforma.c_str(),
		        enl.pareja.soyAnfitrion ? "anfitrion" : "invitado");
	}
	// el codigo propio (y se escucha como anfitrion: el que escribe el codigo llama)
	void mostrarCodigoPropio()
	{
		if (!enl.idCodigo) enl.idCodigo = red::nuevoIdSesion();
		enl.codigo = red::codigoConexion(enl.info, enl.idCodigo);
		crearSesionRed(true, red::Direccion{}, enl.idCodigo, false);
		enl.fase = FR_CODIGO;
		fprintf(stderr, "[en linea] codigo propio: %s\n", enl.codigo.c_str());
	}
	// el codigo del rival: este lado llama (perfora con la sesion del codigo; una direccion a mano va directo)
	void usarCodigoRival(const std::string& texto)
	{
		std::vector<red::Candidato> cands;
		uint64_t id = 0;
		if (!red::leerCodigo(texto, cands, id)) {
			avisoEnLinea(idioma::tr("Código no válido"));
			sonido.sfx("sfx_error");
			return;
		}
		cerrarSesionRed(false);   // (la sesion que escuchaba suelta el socket)
		if (!abrirSocketRed()) return;
		auto t = std::make_shared<TareaRed>();
		auto s = enl.socket;
		std::thread([t, s, cands, id] {
			if (id == 0) {   // (direccion escrita a mano: sin perforacion)
				t->elegido = cands[0].dir;
				t->ok = true;
			} else {
				t->ok = red::perforar(*s, cands, id, en_linea::CONECTAR_MS, t->elegido);
			}
			t->lista = true;
		}).detach();
		enl.tarea = t;
		enl.pareja = red::Pareja{};
		enl.pareja.idSesion = id;
		enl.pareja.soyAnfitrion = false;
		enl.fase = FR_PERFORANDO;
		irAEnLinea(en_linea::CONECTANDO);
		fprintf(stderr, "[en linea] codigo del rival: %d candidatos, sesion %llx\n", (int)cands.size(), (unsigned long long)id);
	}
	// red local
	void abrirLan()
	{
		if (!enl.lanAbierta) enl.lanAbierta = enl.buscador.empezar(identidadRed(), enl.puertoLan);
		irAEnLinea(en_linea::LAN);
	}
	void crearDueloLan()
	{
		if (!abrirSocketRed()) return;
		crearSesionRed(true, red::Direccion{}, 0, false);
		enl.anuncio.extra = enl.lanExtra;
		enl.anuncio.puertoLan = enl.puertoLan;
		enl.anunciando = enl.anuncio.empezar(identidadRed(), enl.socket->puertoLocal(), red::nuevoIdSesion());
		irAEnLinea(en_linea::LAN_ESPERA);
	}
	void unirseLan(const red::Anuncio& a)
	{
		if (!a.compatible) {
			avisoEnLinea(idioma::tr("Otra versión del juego: no se puede unir"));
			sonido.sfx("sfx_error");
			return;
		}
		if (!abrirSocketRed()) return;
		crearSesionRed(false, a.anfitrion, 0, false);
		irAEnLinea(en_linea::ESPERA_RIVAL);
	}

	// no se pudo conectar: los avisos de la red (CGNAT, NAT simetrico, como abrir el puerto)
	void falloConexion(const char* porque)
	{
		cerrarSesionRed(false);
		if (enl.salaObj) enl.salaObj->cancelar();
		const char* a = red::avisoRed(enl.info);
		enl.avisoRed = a ? idioma::tr(a) : std::string();
		enl.motivoCorte = porque;
		irAEnLinea(en_linea::AVISO_RED);
		sonido.sfx("sfx_error");
		fprintf(stderr, "[en linea] no se pudo conectar: %s (NAT %s)\n", porque, red::nombreNat(enl.info.nat));
	}

	// ---- el duelo en red
	void empezarMonedaRed()
	{
		SesionRed& s = *enl.sesion;
		VD& v = ctl.vista;
		if (enl.anunciando) {
			enl.anuncio.detener();
			enl.anunciando = false;
		}
		enl.conIA = enl.rivalEncontrado = false;
		mazoRivalIA = ia::MazoRival{};
		turnoIA = ia::Turno{};
		variedadPendiente = false;
		const std::string nombre = s.duelo.canal().otro().nombre.empty() ? std::string(idioma::tr("Rival")) : s.duelo.canal().otro().nombre;
		v.rivalEs = v.rivalEn = nombre;
		v.lemaEs = v.lemaEn = v.personalidad = v.personaje = v.entorno = v.varianteEs = v.varianteEn = "";
		v.nivelRival = 0;
		v.modo = VD::LIBRE;
		ctl.preparar({}, {}, {});
		ctl.manoR = 0;
		ctl.robo.activo = false;
		interfazDuelo.contarMazos(0, 0, 0);
		v.pantalla = VD::DUELO;
		v.t = 0.0f;
		v.ganador = -1;
		v.subeNivel = false;
		v.turnoRival = v.pensando = false;
		dueloIA = ia::Duelo(&baseIA);
		empezarMoneda();
		monedaForzada = -1;
		ctl.bloqueado = true;
		pausa.abierta = false;
		mostrarPausa();
		resultado::resumen().activo = false;
		enl.enDuelo = true;
		enl.reportado = enl.rendidoYo = false;
		enl.motivo = resultado::M_NINGUNO;
		enl.revanchasVistas = s.duelo.revanchas();
		v.enLinea.pantalla = en_linea::CERRADA;
		v.enLinea.enDuelo = true;
		v.enLinea.rival = nombre;
		v.enLinea.eloTexto = v.enLinea.revanchaTexto = "";
		musica("musica_menu");
		fprintf(stderr, "[en linea] duelo contra %s (soy el lado %d): moneda\n", nombre.c_str(), s.duelo.yo());
	}
	// la moneda ya dijo quien empieza y el duelo en red arranco: la mesa
	void arrancarDueloRed()
	{
		SesionRed& s = *enl.sesion;
		dueloIA = red::vistaDe(s.duelo.duelo(), s.duelo.yo());
		enl.pasoVisto = s.duelo.pasos();
		sucesosVistos = 0;
		mesaPendiente = true;
		esperaEfecto = 0.0f;
		reiniciarResumen();
		rendido = false;
		estadoCampo = CAMPO_VACIO;
		entornosRival.soltarCampo();
		interfazDuelo.cambiarLP(false, ia::LP_INICIAL);
		interfazDuelo.cambiarLP(true, ia::LP_INICIAL);
		ctl.preparar({}, {}, {});
		VD& v = ctl.vista;
		v.pantalla = VD::DUELO;
		v.t = 0.0f;
		v.ganador = -1;
		ocultas[0] = (int)dueloIA.l[0].mano.size();
		ocultas[1] = (int)dueloIA.l[1].mano.size();
		reparto = std::min(ocultas[0], ia::MANO_INICIAL) + std::min(ocultas[1], ia::MANO_INICIAL);
		repartoLado = dueloIA.activo;
		ctl.robo.activo = false;
		sincronizarMesaDuelo();
		sonido.sfx("sfx_duelo_inicio");
		musicaActual.clear();
		musicaDePantalla();
		ctl.aviso = dueloIA.activo == 0 ? "Empiezas tú" : "Empieza el rival";
		ctl.tAviso = 0.0f;
		enl.reloj[0] = enl.reloj[1] = en_linea::RELOJ_TURNO_S;
		enl.turnoReloj = -1;
		enl.alertaSonada = enl.agotadoTurno = false;
		fprintf(stderr, "[en linea] duelo en red: empieza %s\n", dueloIA.activo == 0 ? "el jugador" : "el rival");
	}
	// la vista del duelo en red (dueloIA) al dia con el de verdad
	void refrescarVistaRed()
	{
		if (!enl.sesion) return;
		const red::DueloRed& d = enl.sesion->duelo;
		if (d.estado() != red::D_JUGANDO && d.estado() != red::D_TERMINADO) return;
		if (d.pasos() == enl.pasoVisto && dueloIA.sucesos.size() == d.duelo().sucesos.size()) return;
		dueloIA = red::vistaDe(d.duelo(), d.yo());
		enl.pasoVisto = d.pasos();
	}
	// una jugada del jugador (control.h -> jugarJugador): al duelo en red, del lado real
	bool jugarRed(const ia::Jugada& j)
	{
		SesionRed& s = *enl.sesion;
		if (!s.duelo.meToca()) return false;
		const ia::Jugada real = s.duelo.yo() == 1 ? red::espejoJugada(j) : j;
		const bool ok = s.duelo.jugar(real, ahoraRed());
		refrescarVistaRed();
		if (ok) sonidoJugada(j);
		fprintf(stderr, "[en linea] jugador: %s %s (paso %u)\n", ia::NOMBRE_JUGADA[j.tipo], ok ? "ok" : "rechazada", s.duelo.pasos());
		return ok;
	}
	// el reloj se agoto: la jugada que haria el jugador sin mirar (pasa de fase / no activa / elige la primera)
	void jugadaAutomaticaRed()
	{
		SesionRed& s = *enl.sesion;
		const ia::Duelo& d = s.duelo.duelo();
		ia::Jugada j;   // (J_FASE)
		if (d.ef.preg.tipo == ia::P_RESPUESTA) {
			j.tipo = ia::J_PASAR;
		} else if (d.ef.preg.tipo == ia::P_ELEGIR) {
			j.tipo = d.ef.preg.opcional ? ia::J_PASAR : ia::J_ELEGIR;
			j.objetivo = 0;
		}
		s.duelo.jugarConRespaldo(j, ahoraRed());
		refrescarVistaRed();
		if (ctl.estado != control::NAVEGAR) {   // (el menu que tenia abierto ya no vale)
			ctl.estado = control::NAVEGAR;
			ctl.enPregunta = ctl.armando = ctl.eleccionDuelo = false;
		}
	}
	// el reloj del turno (en_linea.h): el propio corre mientras decido (con la mesa al dia); el del rival solo se muestra
	void avanzarRelojRed(float dt)
	{
		SesionRed& s = *enl.sesion;
		const red::DueloRed& r = s.duelo;
		if (r.estado() != red::D_JUGANDO || ctl.vista.sorteo || ctl.vista.pantalla != VD::DUELO) return;
		if (r.duelo().turno != enl.turnoReloj) {
			enl.turnoReloj = r.duelo().turno;
			enl.reloj[0] = enl.reloj[1] = en_linea::RELOJ_TURNO_S;
			enl.alertaSonada = enl.agotadoTurno = false;
		}
		if (r.esperandoReconexion()) return;   // (con el rival caido no corre ninguno)
		const bool mio = r.meToca();
		if (mio && (enl.alDia || pausa.abierta || enl.reloj[0] <= 0.0f)) enl.reloj[0] = std::max(0.0f, enl.reloj[0] - dt);
		if (!mio && r.duelo().decide() != r.yo()) enl.reloj[1] = std::max(0.0f, enl.reloj[1] - dt);
		if (mio && enl.reloj[0] <= en_linea::RELOJ_ALERTA_S && !enl.alertaSonada) {
			enl.alertaSonada = true;
			sonido.sfx("sfx_error", 0.8f);
			ctl.aviso = "¡Quedan 30 segundos!";
			ctl.tAviso = 0.0f;
		}
		if (mio && enl.reloj[0] <= 0.0f) {
			if (!enl.agotadoTurno) {
				enl.agotadoTurno = true;
				s.duelo.agotarTiempo(ahoraRed());
				ctl.aviso = "Se acabó tu tiempo";
				ctl.tAviso = 0.0f;
				fprintf(stderr, "[en linea] reloj agotado (%d de %d)\n", s.duelo.agotados(s.duelo.yo()), en_linea::AGOTAMIENTOS_DERROTA);
			}
			if (s.duelo.meToca()) jugadaAutomaticaRed();
		}
	}
	// el duelo en red termino sin pasar por la mesa (rendicion, tiempo, abandono...)
	void finRed()
	{
		SesionRed& s = *enl.sesion;
		const red::DueloRed& r = s.duelo;
		const red::FinDuelo f = r.fin();
		VD& v = ctl.vista;
		if (v.pantalla == VD::RESULTADO || (v.pantalla == VD::EN_LINEA && v.enLinea.pantalla == en_linea::INTERRUMPIDO)) return;
		if (f == red::FIN_NORMAL) return;   // (lo termina la mesa al presentar el ultimo suceso: terminarDueloIA)
		if (f == red::FIN_RENDICION || f == red::FIN_TIEMPO) {
			refrescarVistaRed();
			dueloIA.ganador = r.ganador() == r.yo() ? 0 : 1;
			enl.motivo = f == red::FIN_TIEMPO ? resultado::M_TIEMPO : enl.rendidoYo ? resultado::M_RENDICION : resultado::M_RENDICION_RIVAL;
			v.sorteo = false;
			terminarDueloRed();
			return;
		}
		// abandono, desincronizacion, rechazo, trampa: no cuenta para ninguno
		enl.motivoCorte = r.motivo();
		fprintf(stderr, "[en linea] duelo interrumpido (%s): %s\n", red::NOMBRE_FIN[f], r.motivo().c_str());
		const bool rechazado = f == red::FIN_RECHAZADO && v.pantalla == VD::EN_LINEA;
		cerrarSesionRed(false);
		if (rechazado) {
			falloConexion("rechazado");
			return;
		}
		irAEnLinea(en_linea::INTERRUMPIDO);
		sonido.sfx("sfx_error");
	}
	// la mesa termino el duelo (terminarDueloIA) o lo termino la red: resultado, ELO y estadisticas en linea
	void terminarDueloRed()
	{
		VD& v = ctl.vista;
		SesionRed& s = *enl.sesion;
		v.ganador = dueloIA.ganador;
		v.subeNivel = v.personajeNuevo = v.sagaNueva = false;
		v.firmaNuevas = 0;
		if (s.deSala && !enl.reportado && (v.ganador == 0 || v.ganador == 1)) {
			red::Sala& sala = salaRed();
			enl.eloAntes = sala.datosElo().elo;
			enl.confirmacionesAntes = sala.confirmaciones();
			sala.reportarResultado(s.idSesion, v.ganador == 0);
			enl.reportado = true;
		}
		v.pantalla = VD::RESULTADO;
		v.t = 0.0f;
		v.turnoRival = v.pensando = false;
		ctl.estado = control::NAVEGAR;
		prepararResultado();
		fprintf(stderr, "[en linea] fin: gana %s (%s)\n", v.ganador == 0 ? "el jugador" : v.ganador == 1 ? "el rival" : "nadie", red::NOMBRE_FIN[s.duelo.fin()]);
	}
	// el resultado del duelo en linea con el mando: revancha (los dos), buscar otro rival o menu
	bool avanzarResultadoRed(uint32_t p)
	{
		VD& v = ctl.vista;
		resultado::Resumen& r = resultado::resumen();
		if (v.t < 0.8f) return true;
		if (p & control::B_CIRCULO) {   // (circulo: al menu principal, como en el resultado contra la IA)
			sonido.sfx("sfx_menu_cancelar");
			r.activo = false;
			irAMenuPrincipal();
			return true;
		}
		const int n = resultado::NUM_OPCIONES, antes = r.opcion;
		r.revancha = enl.sesion && enl.sesion->duelo.revanchaPosible();   // (ya pedida: sigue elegida, X no hace nada)
		if (p & (control::B_IZQ | control::B_ARRIBA)) r.opcion = (r.opcion + n - 1) % n;
		if (p & (control::B_DER | control::B_ABAJO)) r.opcion = (r.opcion + 1) % n;
		if (!r.revancha && r.opcion == resultado::O_REVANCHA) r.opcion = antes == resultado::O_OTRO_RIVAL ? resultado::O_MENU : resultado::O_OTRO_RIVAL;
		if (r.opcion != antes) sonido.sfx("sfx_menu_cursor", 0.7f);
		if (p & control::B_X) {
			sonido.sfx("sfx_menu_confirmar");
			fprintf(stderr, "[en linea] resultado: elige %d\n", r.opcion);
			switch (r.opcion) {
			case resultado::O_REVANCHA: enl.sesion->duelo.pedirRevancha(ahoraRed()); break;
			case resultado::O_OTRO_RIVAL:
				r.activo = false;
				cerrarSesionRed();
				irAEnLinea(en_linea::MENU);
				break;
			default:
				r.activo = false;
				irAMenuPrincipal();
				break;
			}
		}
		return true;
	}

	// ---- lo que preguntan duelo_juego.h, moneda_juego.h y resultado_juego.h (las pruebas sin red tienen maquetas)
	bool dueloRedJugando() const { return enl.sesion && enl.sesion->duelo.estado() == red::D_JUGANDO; }
	void alDiaRed(bool alDia) { enl.alDia = alDia; }
	int monedaRedGanador() const   // 0 la gane yo, 1 el rival, -1 todavia no se sabe
	{
		if (!enl.sesion || enl.sesion->duelo.ganoMoneda() < 0) return -1;
		return enl.sesion->duelo.ganoMoneda() == enl.sesion->duelo.yo() ? 0 : 1;
	}
	int monedaRedEmpieza() const   // 0 empiezo yo, 1 el rival, -1 todavia no se sabe
	{
		if (!enl.sesion || enl.sesion->duelo.empieza() < 0) return -1;
		return enl.sesion->duelo.empieza() == enl.sesion->duelo.yo() ? 0 : 1;
	}
	void elegirOrdenRed(bool primero)
	{
		if (enl.sesion) enl.sesion->duelo.elegirOrden(primero, ahoraRed());
	}
	resultado::Motivo motivoRed() const { return enl.motivo; }
	bool revanchaRedPosible() const { return enl.sesion && enl.sesion->duelo.revanchaPosible(); }
	void rendirseRed()
	{
		if (enl.sesion && enl.sesion->duelo.estado() == red::D_TERMINADO) return;   // (ya termino: el resultado llega solo)
		if (dueloRedJugando()) {
			enl.rendidoYo = true;
			rendido = true;
			enl.sesion->duelo.rendirse(ahoraRed());
			finRed();
		} else {   // (todavia en la moneda: no hay duelo; se sale sin contar, el otro lo ve como abandono)
			ctl.vista.sorteo = false;
			irAMenuPrincipal();
		}
	}
	// circulo en elegir rival / resultado mientras juega contra la IA esperando: vuelve a la busqueda
	bool volverABusquedaRed()
	{
		if (!enl.conIA) return false;
		enl.conIA = false;
		resultado::resumen().activo = false;
		irAEnLinea(en_linea::BUSCANDO);
		return true;
	}

	// ---- el cartel "¡Rival encontrado!" mientras juega contra la IA (X ir, circulo rechazar)
	bool avanzarAvisoRival(uint32_t p)
	{
		if (!enl.rivalEncontrado) return false;
		if (p & control::B_X) {
			sonido.sfx("sfx_menu_confirmar");
			aceptarRivalEncontrado();
			return true;
		}
		if (p & control::B_CIRCULO) {
			sonido.sfx("sfx_menu_cancelar");
			rechazarRivalEncontrado();
			return true;
		}
		return false;
	}
	void aceptarRivalEncontrado()
	{
		fprintf(stderr, "[en linea] rival aceptado: el duelo contra la IA se abandona sin contar\n");
		enl.rivalEncontrado = enl.conIA = false;
		variedadPendiente = false;
		resultado::resumen().activo = false;
		historia_dialogos::escena().cerrar();
		dueloIA.ganador = -1;
		conectarConPareja();
	}
	void rechazarRivalEncontrado()
	{
		fprintf(stderr, "[en linea] rival rechazado: se sigue buscando\n");
		enl.rivalEncontrado = false;
		if (enl.salaObj) enl.salaObj->cancelar();
		buscarRival();
	}

	// ---- cada cuadro (siempre: tambien con la pausa abierta, la red no se detiene)
	void avanzarRed(float dt)
	{
		if (enl.relojVirtual) enl.msVirtual += (uint64_t)lroundf(dt * 1000.0f);
		VD& v = ctl.vista;
		en_linea::Vista& e = v.enLinea;
		e.t += dt;
		enl.tAviso += dt;
		if (enl.salaObj) enl.salaObj->actualizar();
		if (enl.anunciando) enl.anuncio.actualizar(ahoraRed());
		if (enl.lanAbierta) enl.buscador.actualizar(ahoraRed());
		// los trabajos en hilos
		if (enl.tarea && enl.tarea->lista) {
			std::shared_ptr<TareaRed> t = enl.tarea;
			enl.tarea.reset();
			if (enl.fase == FR_REUNIENDO) {
				enl.info = t->info;
				enl.infoLista = true;
				enl.upnpUsado |= enl.info.upnpOk;
				fprintf(stderr, "[en linea] candidatos: %d, NAT %s%s%s\n", (int)enl.info.candidatos.size(), red::nombreNat(enl.info.nat), enl.info.upnpOk ? ", UPnP" : "",
				        enl.info.ipv6Ok ? ", IPv6" : "");
				seguirTrasReunir();
			} else if (enl.fase == FR_PERFORANDO) {
				if (t->ok) crearSesionRed(enl.pareja.soyAnfitrion, t->elegido, enl.pareja.idSesion, enl.pareja.idSesion != 0 && !enl.pareja.rival.id.empty() && !enl.pareja.porDht);
				else falloConexion("la perforacion no encontro camino");
				if (enl.sesion) irAEnLinea(en_linea::ESPERA_RIVAL);
			}
		}
		// buscando: rival encontrado
		if (enl.fase == FR_BUSCANDO && enl.salaObj && enl.salaObj->estado() == red::Sala::Emparejado && enl.salaObj->pareja(enl.pareja)) {
			if (enl.conIA) {
				if (!enl.rivalEncontrado) {
					enl.rivalEncontrado = true;
					enl.rivalQueda = en_linea::RESPONDER_RIVAL_S;
					sonido.sfx("sfx_turno", 1.0f);
					fprintf(stderr, "[en linea] rival encontrado mientras juega contra la IA: %s\n", enl.pareja.rival.nombre.c_str());
				}
				enl.rivalQueda -= dt;
				if (enl.rivalQueda <= 0.0f) rechazarRivalEncontrado();
			} else if (v.pantalla == VD::EN_LINEA && e.pantalla == en_linea::BUSCANDO) {
				conectarConPareja();
			}
		}
		// la sesion
		if (enl.sesion) {
			SesionRed& s = *enl.sesion;
			s.duelo.actualizar(ahoraRed());
			const red::EstadoDuelo st = s.duelo.estado();
			const bool esperando = v.pantalla == VD::EN_LINEA &&
			                       (e.pantalla == en_linea::ESPERA_RIVAL || e.pantalla == en_linea::LAN_ESPERA || e.pantalla == en_linea::CODIGO || e.pantalla == en_linea::TECLADO);
			if (esperando && st != red::D_CONECTANDO && st != red::D_TERMINADO) {
				empezarMonedaRed();
			} else if (esperando && st == red::D_CONECTANDO && e.pantalla == en_linea::ESPERA_RIVAL && ahoraRed() - s.inicio > (uint64_t)en_linea::CONECTAR_MS + 5000) {
				falloConexion("el rival no contesto");
			} else if (st == red::D_TERMINADO && !esperando) {
				finRed();
			}
			if (enl.sesion && enl.enDuelo) {
				SesionRed& s2 = *enl.sesion;
				if (s2.duelo.revanchas() != enl.revanchasVistas) {   // (las dos revanchas pedidas: otra vez la moneda)
					resultado::resumen().activo = false;
					empezarMonedaRed();
				}
				refrescarVistaRed();
				avanzarRelojRed(dt);
			}
		}
		if (enl.sesion && enl.enDuelo && enl.sesion->duelo.estado() == red::D_TERMINADO && v.pantalla == VD::DUELO) finRed();
		mostrarEnLinea();
	}

	// ---- lo que dibuja en_linea_ui.h
	// la ayuda de botones: "boton|texto;..." (en_linea_ui.h dibuja el icono del boton y el texto ya traducido)
	static std::string ayudaRed(std::initializer_list<const char*> pares)
	{
		std::string r;
		bool boton = true;
		for (const char* x : pares) {
			r += boton ? (r.empty() ? "" : ";") + std::string(x) + "|" : std::string(idioma::tr(x));
			boton = !boton;
		}
		return r;
	}
	static std::string segundosTexto(uint64_t ms)
	{
		const int s = (int)(ms / 1000);
		char b[16];
		snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
		return b;
	}
	void mostrarEnLinea()
	{
		VD& v = ctl.vista;
		en_linea::Vista& e = v.enLinea;
		e.rivalEncontrado = enl.rivalEncontrado;
		if (enl.rivalEncontrado) {
			e.rivalEncontradoTexto = enl.pareja.rival.nombre + "  ·  ELO " + std::to_string(enl.pareja.rival.elo) + "  ·  " +
			                         (enl.pareja.rival.plataforma == "pc" ? "PC" : "PS5");
			e.rivalEncontradoQueda = enl.rivalQueda;
		}
		// en el duelo
		e.enDuelo = enLineaActivo() && (v.pantalla == VD::DUELO || v.pantalla == VD::RESULTADO);
		if (enl.sesion) {
			const red::DueloRed& r = enl.sesion->duelo;
			e.conexion = r.esperandoReconexion() ? 1 : 0;
			e.reconectando = e.conexion ? (int)std::min<uint64_t>(30, r.canal().silencio(ahoraRed()) / 1000) : 0;
			e.relojMio = enl.reloj[0];
			e.relojRival = enl.reloj[1];
			e.relojMioCorre = r.estado() == red::D_JUGANDO && r.meToca();
			e.relojRivalCorre = r.estado() == red::D_JUGANDO && !r.meToca();
			e.agotadosMio = r.agotados(r.yo());
			e.agotadosRival = r.agotados(1 - r.yo());
			// resultado: ELO (solo si la sala lo confirmo) y la revancha
			e.eloTexto.clear();
			if (enl.reportado && enl.salaObj) {
				if (enl.salaObj->confirmaciones() > enl.confirmacionesAntes) {
					const int d = enl.salaObj->eloConfirmado() - enl.eloAntes;
					e.eloTexto = "ELO: " + std::to_string(enl.eloAntes) + " -> " + std::to_string(enl.salaObj->eloConfirmado()) + " (" + (d >= 0 ? "+" : "") +
					             std::to_string(d) + ")";
				} else {
					e.eloTexto = idioma::tr("ELO pendiente de confirmar");
				}
			}
			e.revanchaTexto.clear();
			if (v.pantalla == VD::RESULTADO) {
				if (r.revanchaMia()) e.revanchaTexto = idioma::tr("Revancha pedida: esperando al rival…");
				else if (r.revanchaDelOtro()) e.revanchaTexto = idioma::tr("El rival quiere la revancha");
				else if (!r.revanchaPosible()) e.revanchaTexto = idioma::tr("El rival se fue: no hay revancha");
			}
		}
		if (v.pantalla != VD::EN_LINEA) return;
		e.opciones.clear();
		e.apagadas.clear();
		e.lineas.clear();
		e.aviso = enl.tAviso < 4.0f ? enl.aviso : std::string();
		e.animando = false;
		auto op = [&](const std::string& t, bool apagada = false) {
			e.opciones.push_back(t);
			e.apagadas.push_back(apagada);
		};
		red::Sala* sala = enl.salaObj.get();
		auto contador = [&]() {
			if (!sala) return;
			const int n = sala->enLinea(), b = sala->buscandoAhora(), d = sala->duelosEnCurso();
			if (n < 0) e.lineas.push_back(std::string(idioma::tr("Jugadores en línea: ")) + idioma::tr("Consultando…"));
			else
				e.lineas.push_back(std::string(idioma::tr("Jugadores en línea: ")) + std::to_string(n) + "  ·  " + idioma::tr("Buscando ahora: ") + std::to_string(std::max(0, b)) +
				                   "  ·  " + idioma::tr("Duelos: ") + std::to_string(std::max(0, d)));
		};
		auto elo = [&]() {
			if (!sala) return;
			e.lineas.push_back(std::string(idioma::tr("Tu ELO: ")) + std::to_string(sala->datosElo().elo) + "  ·  " + idioma::tr("Partidas en línea: ") +
			                   std::to_string(sala->datosElo().partidas));
		};
		switch (e.pantalla) {
		case en_linea::MENU: {
			e.titulo = idioma::tr("DUELO EN LÍNEA");
			op(idioma::tr("Buscar rival"));
			op(idioma::tr("Red local"));
			op(idioma::tr("Código de conexión"));
			std::string motivo;
			const bool ok = mazoRedValido(&motivo);
			op(std::string(idioma::tr("Mazo: ")) + mazoRed()->nombre + (ok ? "" : "  (" + std::string(idioma::tr("no válido")) + ")"));
			op(idioma::tr("Volver"));
			contador();
			elo();
			e.lineas.push_back(std::string(idioma::tr("Nombre: ")) + nombreRed());
			if (!ok && e.aviso.empty()) e.aviso = std::string(idioma::tr("Tu mazo no es válido: ")) + motivo;
			e.ayuda = ayudaRed({ "X", "Elegir", "CRUZ", "Cambiar de mazo", "O", "Volver" });
			break;
		}
		case en_linea::AVISO_IP:
			e.titulo = idioma::tr("AVISO: TU IP SERÁ VISIBLE");
			e.lineas.push_back(idioma::tr("En los duelos P2P el rival puede ver tu dirección IP."));
			e.lineas.push_back(idioma::tr("El duelo en línea conecta tu consola directamente con la del rival. Tu dirección IP será visible para tu rival y, si la sala no responde, también en la red pública de BitTorrent (DHT) que se usa para encontrar jugadores."));
			op(idioma::tr("Aceptar"));
			op(idioma::tr("Cancelar"));
			e.ayuda = ayudaRed({ "X", "Elegir", "O", "Cancelar" });
			break;
		case en_linea::BUSCANDO: {
			e.animando = true;
			const bool sin = sala && enl.fase == FR_BUSCANDO && sala->estado() == red::Sala::SinConexion;
			const bool error = sala && enl.fase == FR_BUSCANDO && sala->estado() == red::Sala::Error;
			e.titulo = idioma::tr(enl.fase == FR_REUNIENDO ? "Preparando la conexión…" : sin ? "Sin conexión a internet" : error ? "No se pudo abrir la red" : "Buscando rival…");
			if (enl.fase == FR_BUSCANDO) e.lineas.push_back(std::string(idioma::tr("Tiempo de espera: ")) + segundosTexto(ahoraRed() - enl.inicioBusqueda));
			contador();
			elo();
			if (sala && sala->usandoDht()) e.lineas.push_back(idioma::tr("La sala no responde: buscando por la red P2P"));
			if (sin || error) {
				e.animando = false;
				op(idioma::tr("Reintentar"));
				op(idioma::tr("Volver"));
			} else {
				op(idioma::tr("Jugar contra la IA mientras espero"));
				op(idioma::tr("Cancelar búsqueda"));
			}
			e.ayuda = ayudaRed({ "X", "Elegir", "O", "Cancelar" });
			break;
		}
		case en_linea::CONECTANDO:
		case en_linea::ESPERA_RIVAL:
			e.animando = true;
			e.titulo = enl.pareja.rival.nombre.empty() ? idioma::tr("Conectando…") : idioma::tr("¡Rival encontrado!");
			if (!enl.pareja.rival.nombre.empty())
				e.lineas.push_back(enl.pareja.rival.nombre + "  ·  ELO " + std::to_string(enl.pareja.rival.elo) + "  ·  " + (enl.pareja.rival.plataforma == "pc" ? "PC" : "PS5"));
			e.lineas.push_back(idioma::tr("Conectando…"));
			op(idioma::tr("Cancelar"));
			e.ayuda = ayudaRed({ "O", "Cancelar" });
			break;
		case en_linea::LAN: {
			e.titulo = idioma::tr("RED LOCAL");
			op(idioma::tr("Crear duelo en la red local"));
			if (enl.lanAbierta)
				for (const red::Anuncio& a : enl.buscador.lista())
					op(a.nombre + (a.compatible ? "" : "  (" + std::string(idioma::tr("otra versión")) + ")"), !a.compatible);
			e.lineas.push_back(idioma::tr(enl.lanAbierta ? (enl.buscador.lista().empty() ? "Nadie anunció un duelo en tu red todavía" : "Elige un duelo para unirte")
			                                              : "No se pudo abrir la red local"));
			op(idioma::tr("Volver"));
			e.ayuda = ayudaRed({ "X", "Elegir", "O", "Volver" });
			break;
		}
		case en_linea::LAN_ESPERA:
			e.animando = true;
			e.titulo = idioma::tr("Esperando rival en la red local…");
			e.lineas.push_back(std::string(idioma::tr("Nombre: ")) + nombreRed());
			op(idioma::tr("Cancelar"));
			e.ayuda = ayudaRed({ "O", "Cancelar" });
			break;
		case en_linea::CODIGO:
			e.titulo = idioma::tr("CÓDIGO DE CONEXIÓN");
			if (enl.fase == FR_REUNIENDO) {
				e.animando = true;
				e.lineas.push_back(idioma::tr("Preparando tu código…"));
			} else {
				e.lineas.push_back(idioma::tr("Tu código (pásaselo al rival para que lo escriba):"));
				e.lineas.push_back(enl.codigo);
				e.lineas.push_back(idioma::tr("O escribe el código del rival:"));
				if (const char* a = red::avisoRed(enl.info)) e.lineas.push_back(idioma::tr(a));
			}
			op(idioma::tr("Escribir el código del rival"), enl.fase == FR_REUNIENDO);
			op(idioma::tr("Volver"));
			e.ayuda = ayudaRed({ "X", "Elegir", "O", "Volver" });
			break;
		case en_linea::TECLADO:
			e.titulo = idioma::tr("CÓDIGO DEL RIVAL");
			e.ayuda = ayudaRed({ "CRUZ", "Tecla", "X", "Escribir", "C", "Borrar", "T", "Aceptar", "O", "Cancelar" });
			break;
		case en_linea::AVISO_RED:
			e.titulo = idioma::tr("No se pudo conectar con el rival");
			if (!enl.avisoRed.empty()) e.lineas.push_back(enl.avisoRed);
			e.lineas.push_back(std::string(idioma::tr(red::TEXTO_PUERTO_MANUAL)) + " " + std::to_string(red::PUERTO_JUEGO));
			e.lineas.push_back(std::string(idioma::tr("Tipo de NAT: ")) + red::nombreNat(enl.info.nat));
			op(idioma::tr("Buscar otra vez"));
			op(idioma::tr("Volver"));
			e.ayuda = ayudaRed({ "X", "Elegir", "O", "Volver" });
			break;
		case en_linea::INTERRUMPIDO:
			e.titulo = idioma::tr("Duelo interrumpido: no cuenta");
			e.lineas.push_back(idioma::tr("El rival se desconectó y no volvió. El duelo no cuenta para ninguno de los dos: ni victoria ni derrota, ni ELO."));
			if (!enl.motivoCorte.empty()) e.lineas.push_back("(" + enl.motivoCorte + ")");
			op(idioma::tr("Continuar"));
			e.ayuda = ayudaRed({ "X", "Continuar" });
			break;
		default: break;
		}
		if (e.opcion >= (int)e.opciones.size()) e.opcion = std::max(0, (int)e.opciones.size() - 1);
	}

	// ---- el mando en las pantallas del modo en linea (avanzarPantallaDuelo). Siempre se queda con el mando
	bool avanzarMenuEnLinea(uint32_t p)
	{
		VD& v = ctl.vista;
		en_linea::Vista& e = v.enLinea;
		const int n = (int)e.opciones.size();
		if (e.pantalla == en_linea::TECLADO) return avanzarTecladoRed(p);
		if (n > 0) {
			const int antes = e.opcion;
			if (p & control::B_ARRIBA) e.opcion = (e.opcion + n - 1) % n;
			if (p & control::B_ABAJO) e.opcion = (e.opcion + 1) % n;
			if (e.opcion != antes) sonido.sfx("sfx_menu_cursor", 0.7f);
		}
		const bool x = (p & control::B_X) != 0, o = (p & control::B_CIRCULO) != 0;
		const int k = e.opcion;
		switch (e.pantalla) {
		case en_linea::AVISO_IP:
			if ((x && k == 0) || (x && k == 1) || o) {
				const bool acepta = x && k == 0;
				sonido.sfx(acepta ? "sfx_menu_confirmar" : "sfx_menu_cancelar");
				enl.avisoIp = acepta;
				guardarOpciones();
				fprintf(stderr, "[en linea] aviso de privacidad: %s\n", acepta ? "aceptado" : "cancelado");
				if (enl.avisoDesdeOpciones) {
					enl.avisoDesdeOpciones = false;
					irAMenuPrincipal();
					menuP.enOpciones = true;
					menuP.opcionOpc = OP_AVISO_IP;
					mostrarMenu();
				} else if (acepta) {
					irAEnLinea(en_linea::MENU);
				} else {
					irAMenuPrincipal();
				}
			}
			return true;
		case en_linea::MENU:
			if ((p & (control::B_IZQ | control::B_DER)) || (x && k == OL_MAZO)) {
				const int nm = (int)biblioteca.mazos.size();
				if (nm > 0 && (k == OL_MAZO || (p & (control::B_IZQ | control::B_DER)))) {
					enl.mazo = ((enl.mazo < 0 ? 0 : enl.mazo) + ((p & control::B_IZQ) ? nm - 1 : 1)) % nm;
					sonido.sfx("sfx_menu_cursor", 0.7f);
				}
				return true;
			}
			if (o || (x && k == OL_VOLVER)) {
				sonido.sfx("sfx_menu_cancelar");
				irAMenuPrincipal();
				return true;
			}
			if (x) {
				std::string motivo;
				if (!mazoRedValido(&motivo)) {
					avisoEnLinea(std::string(idioma::tr("Tu mazo no es válido: ")) + motivo);
					sonido.sfx("sfx_error");
					return true;
				}
				sonido.sfx("sfx_menu_confirmar");
				if (k == OL_BUSCAR) {
					irAEnLinea(en_linea::BUSCANDO);
					reunirRed(SR_BUSCAR);
				} else if (k == OL_LAN) {
					abrirLan();
				} else if (k == OL_CODIGO) {
					irAEnLinea(en_linea::CODIGO);
					reunirRed(SR_CODIGO);
				}
			}
			return true;
		case en_linea::BUSCANDO: {
			const bool fallo = enl.salaObj && enl.fase == FR_BUSCANDO && (enl.salaObj->estado() == red::Sala::SinConexion || enl.salaObj->estado() == red::Sala::Error);
			if (o || (x && k == 1)) {
				sonido.sfx("sfx_menu_cancelar");
				if (enl.salaObj) enl.salaObj->cancelar();
				enl.fase = FR_NADA;
				irAEnLinea(en_linea::MENU);
			} else if (x && k == 0 && fallo) {
				sonido.sfx("sfx_menu_confirmar");
				buscarRival();
			} else if (x && k == 0 && enl.fase == FR_BUSCANDO) {   // contra la IA mientras espera (la busqueda sigue)
				sonido.sfx("sfx_menu_confirmar");
				enl.conIA = true;
				v.modo = VD::LIBRE;
				irAElegirRival();
				fprintf(stderr, "[en linea] juega contra la IA mientras espera\n");
			}
			return true;
		}
		case en_linea::CONECTANDO:
		case en_linea::ESPERA_RIVAL:
		case en_linea::LAN_ESPERA:
			if (o || x) {
				sonido.sfx("sfx_menu_cancelar");
				cerrarSesionRed();
				if (enl.salaObj) enl.salaObj->cancelar();
				enl.tarea.reset();
				if (enl.anunciando) enl.anuncio.detener();
				enl.anunciando = false;
				enl.fase = FR_NADA;
				irAEnLinea(e.pantalla == en_linea::LAN_ESPERA ? en_linea::LAN : en_linea::MENU);
			}
			return true;
		case en_linea::LAN:
			if (o || (x && k == n - 1)) {
				sonido.sfx("sfx_menu_cancelar");
				enl.buscador.detener();
				enl.lanAbierta = false;
				irAEnLinea(en_linea::MENU);
			} else if (x && k == 0) {
				sonido.sfx("sfx_menu_confirmar");
				crearDueloLan();
			} else if (x && enl.lanAbierta && k - 1 < (int)enl.buscador.lista().size()) {
				sonido.sfx("sfx_menu_confirmar");
				unirseLan(enl.buscador.lista()[(size_t)(k - 1)]);
			}
			return true;
		case en_linea::CODIGO:
			if (o || (x && k == 1)) {
				sonido.sfx("sfx_menu_cancelar");
				cerrarSesionRed(false);
				enl.fase = FR_NADA;
				irAEnLinea(en_linea::MENU);
			} else if (x && k == 0 && enl.fase == FR_CODIGO) {
				sonido.sfx("sfx_menu_confirmar");
				e.texto.clear();
				e.tecla = 0;
				e.pantalla = en_linea::TECLADO;
				e.t = 0.0f;
			}
			return true;
		case en_linea::AVISO_RED:
			if (o || (x && k == 1)) {
				sonido.sfx("sfx_menu_cancelar");
				irAEnLinea(en_linea::MENU);
			} else if (x && k == 0) {
				sonido.sfx("sfx_menu_confirmar");
				irAEnLinea(en_linea::BUSCANDO);
				reunirRed(SR_BUSCAR);
			}
			return true;
		case en_linea::INTERRUMPIDO:
			if (x || o) {
				sonido.sfx("sfx_menu_confirmar");
				irAEnLinea(en_linea::MENU);
			}
			return true;
		default: return true;
		}
	}
	// el teclado en pantalla del codigo: cruceta mueve, X escribe, cuadrado borra, triangulo acepta, circulo cancela
	bool avanzarTecladoRed(uint32_t p)
	{
		en_linea::Vista& e = ctl.vista.enLinea;
		const int n = (int)strlen(en_linea::TECLAS), f = en_linea::TECLAS_FILA;
		const int antes = e.tecla;
		if (p & control::B_DER) e.tecla = (e.tecla + 1) % n;
		if (p & control::B_IZQ) e.tecla = (e.tecla + n - 1) % n;
		if (p & control::B_ABAJO) e.tecla = std::min(n - 1, e.tecla + f);
		if (p & control::B_ARRIBA) e.tecla = std::max(0, e.tecla - f);
		if (e.tecla != antes) sonido.sfx("sfx_menu_cursor", 0.6f);
		if ((p & control::B_X) && e.texto.size() < 120) {
			e.texto += en_linea::TECLAS[e.tecla];
			sonido.sfx("sfx_menu_cursor", 0.8f);
		}
		if ((p & control::B_CUADRADO) && !e.texto.empty()) {
			e.texto.pop_back();
			sonido.sfx("sfx_menu_cancelar", 0.6f);
		}
		if (p & control::B_CIRCULO) {
			sonido.sfx("sfx_menu_cancelar");
			e.pantalla = en_linea::CODIGO;
			e.opcion = 0;
		} else if ((p & control::B_TRIANGULO) && !e.texto.empty()) {
			sonido.sfx("sfx_menu_confirmar");
			usarCodigoRival(e.texto);
		}
		return true;
	}
