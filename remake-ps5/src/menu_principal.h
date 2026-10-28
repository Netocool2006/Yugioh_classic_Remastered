	// Yugioh Classic Remaster (rama classic-menus): se incluye DENTRO de la clase del juego (yugireload.cpp), despues
	// de duelo_juego.h. El menu principal (control::VistaDuelo::MENU; lo dibuja menu_ui.h con el arte de la puerta
	// egipcia, rama classic-portada):
	//  - MODO DUELO (elegir rival; adentro, triangulo cambia entre Historia y Duelo libre), CONSTRUCCION DE DECK,
	//    LISTA DE CARTAS (constructor.h: abrirConstructorMazo / abrirListaCartas), OPCIONES y SALIR (pide X otra vez;
	//    cierra con quit = true: el bucle termina, main vuelve y la plantilla le pide al sistema cerrar el titulo).
	//  - Opciones: volumen de la musica y de los efectos (0..10, al instante; los efectos suenan de prueba), permitir
	//    cartas prohibidas (Si/No: opciones::permitirProhibidas(), opciones.h) e idioma (ES/EN, en caliente). Se
	//    guardan en opciones.txt junto a progreso.txt (el idioma en idioma.txt, como siempre).
	//  - Circulo vuelve: de Opciones al menu (y guarda); en el menu cancela "Salir".
	// Los Ajustes de la plantilla (FPS, render, camara...) son de desarrollo: solo se abren con L1 + R1 apretados
	// mientras se mantiene OPTIONS 2 s (o en el PC). vigilarAjustes() cierra los que se abran sin la combinacion.
	enum AccionMenu { MP_DUELO = 0, MP_HISTORIA, MP_CONSTRUIR, MP_OPCIONES, MP_SALIR, MP_LISTA, MP_EN_LINEA };   // (MP_EN_LINEA: rama classic-en-linea, en_linea_juego.h)
	enum AccionOpciones { OP_MUSICA = 0, OP_EFECTOS, OP_PROHIBIDAS, OP_PREGUNTAR, OP_VELOCIDAD, OP_ADAPTABLE, OP_IA, OP_APRENDE, OP_OLVIDAR, OP_FIGURAS, OP_BITACORA, OP_IDIOMA, OP_AVISO_IP, OP_PERSONALIZACION, OP_VOLVER, NUM_OPCIONES };   // (OP_AVISO_IP: rama classic-en-linea)   // (OP_PERSONALIZACION: rama classic-extras, cosmeticos_juego.h)
	struct MenuPrincipal { int opcion{ 0 }, opcionOpc{ 0 }; bool enOpciones{ false }, confirmarSalir{ false }; } menuP;
	std::string rutaOpciones;
	bool ajustesDesarrollo{ false };   // los Ajustes se abrieron con la combinacion secreta
	bool olvidoHecho{ false };   // (rama classic-ia-red: "Olvidar lo aprendido" ya se uso en esta visita a Opciones)

	std::vector<int> accionesMenu() const
	{
		return { MP_DUELO, MP_EN_LINEA, MP_CONSTRUIR, MP_LISTA, MP_OPCIONES, MP_SALIR };   // (los 5 botones de la puerta y uno mas abajo: menu_ui.h)
	}
	static const char* textoMenu(int a)
	{
		switch (a) {
		case MP_DUELO: return "MODO DUELO";
		case MP_HISTORIA: return "Historia";
#if defined(_WIN32)
		case MP_EN_LINEA: return "DUELO EN LÍNEA: NO DISPONIBLE EN PC";   // rama classic-pc: la version de Windows no trae el juego en linea (maqueta)
#else
		case MP_EN_LINEA: return "DUELO EN LÍNEA";
#endif
		case MP_CONSTRUIR: return "CONSTRUCCIÓN DE DECK";
		case MP_LISTA: return "LISTA DE CARTAS";
		case MP_OPCIONES: return "OPCIONES";
		case MP_SALIR: return "SALIR";
		default: return "";
		}
	}
	static const char* textoOpcion(int o)
	{
		switch (o) {
		case OP_IDIOMA: return "Idioma";
		case OP_MUSICA: return "Volumen de la música";
		case OP_EFECTOS: return "Volumen de los efectos";
		case OP_PROHIBIDAS: return "Permitir cartas prohibidas";
		case OP_PREGUNTAR: return "Preguntar antes de activar";
		case OP_VELOCIDAD: return "Velocidad de animación";
		case OP_ADAPTABLE: return "Dificultad adaptable";
		case OP_IA: return "IA";   // (rama classic-ia-humana: Clasica / Humana)
		case OP_APRENDE: return "IA que aprende";   // (rama classic-ia-red: Si / No; aprendizaje.h)
		case OP_OLVIDAR: return "Olvidar lo aprendido";   // (rama classic-ia-red: borra los duelos grabados y la red del jugador)
		case OP_FIGURAS: return "Figuras 3D";
		case OP_BITACORA: return "Bitácora del duelo";   // (rama classic-ps5-duelo-log: duelo_log.h)
		case OP_AVISO_IP: return "Aviso de privacidad (en línea)";
		case OP_PERSONALIZACION: return "Personalización";
		case OP_VOLVER: return "Volver";
		default: return "";
		}
	}
	static int decimos(float v) { return std::max(0, std::min(10, (int)lroundf(v * 10.0f))); }

	// lo que dibuja menu_ui.h
	void mostrarMenu()
	{
		VD& v = ctl.vista;
		v.menuEnOpciones = menuP.enOpciones;
		v.menuSalir = menuP.confirmarSalir;
		v.menuOpciones.clear();
		v.menuValores.clear();
		if (menuP.enOpciones) {
			v.menuOpcion = menuP.opcionOpc;
			for (int o = 0; o < NUM_OPCIONES; o++) {
				v.menuOpciones.push_back(textoOpcion(o));
				std::string val;
				if (o == OP_IDIOMA) val = idioma::ingles() ? "English" : "Español";
				if (o == OP_MUSICA) val = std::to_string(decimos(sonido.volMusica.load())) + " / 10";
				if (o == OP_EFECTOS) val = std::to_string(decimos(sonido.volEfectos.load())) + " / 10";
				if (o == OP_PROHIBIDAS) val = opciones::permitirProhibidas() ? "Sí" : "No";
				if (o == OP_PREGUNTAR) val = opciones::preguntarActivacion() == opciones::PREGUNTAR_SIEMPRE ? "Siempre" : opciones::preguntarActivacion() == opciones::PREGUNTAR_NUNCA ? "Nunca" : "Auto";
				if (o == OP_VELOCIDAD) val = opciones::velocidadAnim() == opciones::VELOCIDAD_MUY_RAPIDA ? "Muy rápida" : opciones::velocidadAnim() == opciones::VELOCIDAD_RAPIDA ? "Rápida" : "Normal";
				if (o == OP_ADAPTABLE) val = memoriaIA.adaptable ? "Sí" : "No";
				if (o == OP_IA) val = memoriaIA.humana ? "Humana" : "Clásica";
				if (o == OP_APRENDE) val = memoriaIA.aprende ? "Sí" : "No";   // (rama classic-ia-red)
				if (o == OP_OLVIDAR && olvidoHecho) val = "Hecho";
				if (o == OP_FIGURAS) val = figuras3D ? "Sí" : "No";
				if (o == OP_BITACORA) val = opciones::bitacora() ? "Sí" : "No";
#if defined(_WIN32)
				if (o == OP_AVISO_IP) val = "No disponible en PC";   // rama classic-pc
#else
				if (o == OP_AVISO_IP) val = enl.avisoIp ? "Aceptado" : "Pendiente";
#endif
				v.menuValores.push_back(val);
			}
		} else {
			v.menuOpcion = menuP.opcion;
			for (int a : accionesMenu()) {
				v.menuOpciones.push_back(textoMenu(a));
				v.menuValores.push_back("");
			}
		}
	}

	void leerOpciones()
	{
		std::ifstream f(rutaOpciones);
		std::string linea;
		while (std::getline(f, linea)) {   // "clave valor" o "clave=valor"
			for (char& c : linea) if (c == '=') c = ' ';
			std::istringstream l(linea);
			std::string clave;
			float x;
			if (!(l >> clave >> x)) continue;
			if (clave == "preguntar_activacion" || clave == "velocidad_anim") x *= 0.5f;   // (0/1/2 -> 0..1)
			x = std::max(0.0f, std::min(1.0f, x));
			if (clave == "musica") sonido.volMusica = x;
			if (clave == "efectos") sonido.volEfectos = x;
			if (clave == "permitir_prohibidas") opciones::prohibidas() = x > 0.5f;
			if (clave == "preguntar_activacion") opciones::preguntar() = std::max(0, std::min(2, (int)lroundf(x * 2.0f)));
			if (clave == "velocidad_anim") opciones::velocidad() = std::max(0, std::min(2, (int)lroundf(x * 2.0f)));
			if (clave == "bitacora_duelo") opciones::bitacora() = x > 0.5f;   // rama classic-ps5-duelo-log
			if (clave == "aviso_ip") enl.avisoIp = x > 0.5f;   // rama classic-en-linea: el aviso de privacidad ya aceptado
#if defined(PS5_HOST_REFERENCE)
			if (clave == "figuras_3d" && !getenv("YUGI_FIGURAS")) figuras3D = x > 0.5f;   // (PC: YUGI_FIGURAS manda)
#else
			if (clave == "figuras_3d") figuras3D = x > 0.5f;
#endif
		}
	}
	// guarda opciones.txt: lee el archivo, conserva las lineas con claves que no son de aca (de otras ramas), pone las
	// suyas y escribe todo junto en un .tmp que despues reemplaza al archivo (rename: nunca queda a medias)
	void guardarOpciones()
	{
		char buf[64];
		std::vector<std::pair<std::string, std::string>> mias;
		auto poner = [&](const char* clave, const char* fmt, double v) {
			snprintf(buf, sizeof(buf), fmt, v);
			mias.emplace_back(clave, buf);
		};
		poner("musica", "%.2f", sonido.volMusica.load());
		poner("efectos", "%.2f", sonido.volEfectos.load());
		poner("permitir_prohibidas", "%.0f", opciones::permitirProhibidas() ? 1.0 : 0.0);
		poner("preguntar_activacion", "%.0f", (double)opciones::preguntarActivacion());
		poner("velocidad_anim", "%.0f", (double)opciones::velocidadAnim());
		poner("figuras_3d", "%.0f", figuras3D ? 1.0 : 0.0);
		poner("bitacora_duelo", "%.0f", opciones::bitacora() ? 1.0 : 0.0);   // rama classic-ps5-duelo-log
		poner("aviso_ip", "%.0f", enl.avisoIp ? 1.0 : 0.0);
		std::vector<std::string> ajenas;
		{
			std::ifstream f(rutaOpciones);
			std::string linea;
			while (std::getline(f, linea)) {
				const std::string clave = linea.substr(0, linea.find_first_of("= \t"));
				bool mia = false;
				for (const auto& m : mias) mia |= m.first == clave;
				if (!mia && !clave.empty()) ajenas.push_back(linea);
			}
		}
		std::string s;   // (guardado atomico: guardado.h)
		for (const auto& m : mias) guardado::agregar(s, "%s=%s\n", m.first.c_str(), m.second.c_str());
		for (const std::string& l : ajenas) guardado::agregar(s, "%s\n", l.c_str());
		guardado::guardarAtomico(rutaOpciones, s);
	}

	// al arrancar (despues de prepararDuelo): las opciones guardadas y el menu principal
	void prepararMenu()
	{
		rutaOpciones = rutaProgreso.substr(0, rutaProgreso.size() - std::string("progreso.txt").size()) + "opciones.txt";
		leerOpciones();
		prepararCosmeticos();   // rama classic-extras: funda, tapete, camara y estadisticas (cosmeticos_juego.h)
		red::archivoStun() = getAssetPath() + "ui/red_stun.txt";   // rama classic-en-linea: los servidores STUN (datos/red_stun.txt)
#if defined(PS5_HOST_REFERENCE)
		// PC: los guiones de prueba (YUGI_GUION) empiezan en elegir rival como antes, salvo con YUGI_MENU=1
		if (getenv("YUGI_GUION") && !getenv("YUGI_MENU")) return;
		if (getenv("YUGI_GUION")) {   // (con guion no hay portada: los guiones de captura siguen igual)
			irAMenuPrincipal();
			return;
		}
#endif
		irAPortada();   // rama classic-portada: la portada (portada.h) y de ahi al menu
	}

	// al menu principal desde cualquier pantalla (el duelo en curso se descarta sin contar)
	void irAMenuPrincipal()
	{
		VD& v = ctl.vista;
		cerrarRed();   // rama classic-en-linea: la busqueda, la red local y el duelo en linea se cierran (en_linea_juego.h)
		pausa.abierta = false;
		mostrarPausa();
		v.pantalla = VD::MENU;
		v.t = 0.0f;
		v.turnoRival = v.pensando = false;
		v.sorteo = false;   // si se sale durante el lanzamiento de moneda
		ctl.bloqueado = true;
		ctl.preparar({}, {}, {});   // mesa vacia
		ctl.manoR = 0;
		interfazDuelo.contarMazos(0, 0, 0);
		musica("musica_menu");
		menuP.enOpciones = menuP.confirmarSalir = false;
		mostrarMenu();
		fprintf(stderr, "[menu] menu principal\n");
	}

	// botones en Opciones (del menu principal o de la pausa: duelo_juego.h). salir: OPTIONS corto (desde la pausa).
	// Circulo (o X en Volver) guarda y vuelve: al menu principal o a la pausa
	void avanzarOpciones(uint32_t p, bool salir)
	{
		// rama classic-extras: Personalizacion (Funda, Tapete, Controles, Estadisticas) se queda con el mando
		if (cosmeticos::vista().pantalla != cosmeticos::P_CERRADA) {
			if (!salir) {
				avanzarPersonalizacion(p);
				mostrarMenu();
				return;
			}
			cosmeticos::vista().pantalla = cosmeticos::P_CERRADA;
		}
		int& o = menuP.opcionOpc;
		if (p & control::B_ARRIBA) o = (o + NUM_OPCIONES - 1) % NUM_OPCIONES;
		if (p & control::B_ABAJO) o = (o + 1) % NUM_OPCIONES;
		if (p & (control::B_ARRIBA | control::B_ABAJO)) sonido.sfx("sfx_menu_cursor", 0.7f);
		const int paso = (p & control::B_DER) ? 1 : (p & control::B_IZQ) ? -1 : 0;
		const int ciclo = paso != 0 ? paso : (p & control::B_X) ? 1 : 0;   // (las de varias opciones: X tambien avanza)
		if (salir || (p & control::B_CIRCULO) || ((p & control::B_X) && o == OP_VOLVER)) {
			sonido.sfx("sfx_menu_cancelar");
			guardarOpciones();
			menuP.enOpciones = false;
			olvidoHecho = false;   // (rama classic-ia-red)
			if (pausa.abierta) mostrarPausa();   // (de vuelta a la pausa)
		} else if (o == OP_AVISO_IP && (p & control::B_X)
#if defined(_WIN32)
		           && false   // rama classic-pc: sin juego en linea
#endif
		) {   // rama classic-en-linea: el aviso de privacidad otra vez
			if (pausa.abierta) {
				sonido.sfx("sfx_error", 0.5f);
			} else {
				sonido.sfx("sfx_menu_confirmar");
				guardarOpciones();
				menuP.enOpciones = false;
				abrirAvisoIpDesdeOpciones();
				return;
			}
		} else if (o == OP_PERSONALIZACION && (p & control::B_X)) {
			sonido.sfx("sfx_menu_confirmar");
			abrirPersonalizacion();
		} else if (o == OP_IDIOMA && ciclo != 0) {
			idioma::poner(idioma::ingles() ? idioma::ES : idioma::EN);
			sonido.sfx("sfx_menu_confirmar");
		} else if (o == OP_PROHIBIDAS && ciclo != 0) {
			opciones::prohibidas() = !opciones::prohibidas();
			fprintf(stderr, "[menu] permitir cartas prohibidas: %s\n", opciones::permitirProhibidas() ? "si" : "no");
			sonido.sfx("sfx_menu_confirmar");
			guardarOpciones();
		} else if ((o == OP_PREGUNTAR || o == OP_VELOCIDAD) && ciclo != 0) {
			int& v = o == OP_PREGUNTAR ? opciones::preguntar() : opciones::velocidad();
			v = (v + ciclo + 3) % 3;
			fprintf(stderr, "[menu] %s: %d\n", o == OP_PREGUNTAR ? "preguntar antes de activar" : "velocidad de animacion", v);
			sonido.sfx("sfx_menu_confirmar");
			guardarOpciones();
		} else if (o == OP_ADAPTABLE && ciclo != 0) {
			memoriaIA.adaptable = !memoriaIA.adaptable;   // (se guarda en variedad.txt, como antes en los Ajustes)
			memoriaIA.guardar();
			sonido.sfx("sfx_menu_confirmar");
		} else if (o == OP_IA && ciclo != 0) {
			memoriaIA.humana = !memoriaIA.humana;   // (rama classic-ia-humana: en variedad.txt; vale desde el proximo duelo)
			memoriaIA.guardar();
			fprintf(stderr, "[menu] IA: %s\n", memoriaIA.humana ? "humana" : "clasica");
			sonido.sfx("sfx_menu_confirmar");
		} else if (o == OP_APRENDE && ciclo != 0) {
			memoriaIA.aprende = !memoriaIA.aprende;   // (rama classic-ia-red: en variedad.txt; vale desde el proximo duelo)
			aprendizIA.activo = memoriaIA.aprende;
			memoriaIA.guardar();
			fprintf(stderr, "[menu] IA que aprende: %s\n", memoriaIA.aprende ? "si" : "no");
			sonido.sfx("sfx_menu_confirmar");
		} else if (o == OP_OLVIDAR && (p & control::B_X)) {
			aprendizIA.olvidar();   // (rama classic-ia-red: vuelve la red base)
			olvidoHecho = true;
			sonido.sfx("sfx_menu_confirmar");
		} else if (o == OP_BITACORA && ciclo != 0) {   // rama classic-ps5-duelo-log: vale desde el proximo duelo
			opciones::bitacora() = !opciones::bitacora();
			fprintf(stderr, "[menu] bitacora del duelo: %s\n", opciones::bitacora() ? "si" : "no");
			sonido.sfx("sfx_menu_confirmar");
			guardarOpciones();
		} else if (o == OP_FIGURAS && ciclo != 0) {
			figuras3D = !figuras3D;
			sonido.sfx("sfx_menu_confirmar");
			guardarOpciones();
		} else if ((o == OP_MUSICA || o == OP_EFECTOS) && paso != 0) {
			std::atomic<float>& vol = o == OP_MUSICA ? sonido.volMusica : sonido.volEfectos;
			vol = (float)std::max(0, std::min(10, decimos(vol.load()) + paso)) / 10.0f;   // (el mezclador lo aplica al instante)
			sonido.sfx(o == OP_EFECTOS ? "sfx_menu_confirmar" : "sfx_menu_cursor");   // (efectos: suena de prueba con el volumen nuevo)
			guardarOpciones();
		}
		mostrarMenu();
	}

	// botones en el menu principal (siempre se queda con el mando)
	bool avanzarMenuPrincipal(uint32_t p)
	{
		if (menuP.enOpciones) {
			avanzarOpciones(p, false);
			return true;
		}
		const std::vector<int> ops = accionesMenu();
		const int n = (int)ops.size();
		int& k = menuP.opcion;
		k = std::max(0, std::min(k, n - 1));
		if (menuP.confirmarSalir && (p & ~control::B_X)) {
			menuP.confirmarSalir = false;   // cualquier otro boton cancela "Salir del juego"
			sonido.sfx("sfx_menu_cancelar");
			mostrarMenu();
			return true;
		}
		if (p & control::B_ARRIBA) k = (k + n - 1) % n;
		if (p & control::B_ABAJO) k = (k + 1) % n;
		if (p & (control::B_ARRIBA | control::B_ABAJO)) sonido.sfx("sfx_menu_cursor", 0.7f);
		if (p & control::B_X) {
			const int a = ops[(size_t)k];
			fprintf(stderr, "[menu] elige '%s'\n", textoMenu(a));
			sonido.sfx("sfx_menu_confirmar");
			switch (a) {
			case MP_DUELO:   // (Historia o Duelo libre se elige adentro con triangulo; queda el ultimo usado)
				if (historia.lista.empty()) ctl.vista.modo = VD::LIBRE;
				irAElegirRival();
				return true;
			case MP_HISTORIA:
				ctl.vista.modo = VD::HISTORIA;
				irAElegirRival();
				return true;
			case MP_EN_LINEA:   // rama classic-en-linea (en_linea_juego.h)
#if defined(_WIN32)
				sonido.sfx("sfx_error", 0.6f);   // rama classic-pc: sin juego en linea en la version de Windows
#else
				entrarEnLinea();
#endif
				return true;
			case MP_CONSTRUIR: constructor::abrirConstructorMazo(); break;   // (constructor.h: se abre en el cuadro siguiente)
			case MP_LISTA: constructor::abrirListaCartas(); break;          // (la coleccion en modo solo ver)
			case MP_OPCIONES:
				menuP.enOpciones = true;
				menuP.opcionOpc = 0;
				break;
			case MP_SALIR:
				if (menuP.confirmarSalir) {
					fprintf(stderr, "[menu] salir del juego\n");
					guardarOpciones();
					cerrarRed();
					if (enl.upnpUsado) red::liberarUpnp();   // (rama classic-en-linea: el mapeo del router, antes de cerrar)
					quit = true;   // fin del bucle: la plantilla le pide al sistema cerrar el titulo
				} else {
					menuP.confirmarSalir = true;
				}
				break;
			default: break;
			}
		}
		mostrarMenu();
		return true;
	}

	// los Ajustes de la plantilla (OPTIONS 2 s) son de desarrollo: en la consola solo valen si al abrirse estan
	// apretados L1 y R1 (la combinacion secreta); si no, se cierran solos. En el PC (F1) siempre valen.
	void vigilarAjustes()
	{
#if !defined(PS5_HOST_REFERENCE)
		if (!ui.visible) {
			ajustesDesarrollo = false;
			return;
		}
		if (ajustesDesarrollo) return;
		const uint32_t secreta = PAD_L1 | PAD_R1;
		if ((mando.held & secreta) == secreta) {
			ajustesDesarrollo = true;
			fprintf(stderr, "[menu] Ajustes de desarrollo abiertos (L1 + R1 + OPTIONS)\n");
		} else {
			ui.visible = false;
		}
#endif
	}
