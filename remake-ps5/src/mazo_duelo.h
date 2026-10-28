	// Yugioh Classic Remaster (rama classic-constructor): se incluye DENTRO de la clase del juego (yugireload.cpp).
	// La coleccion (mazo.h), el mazo del jugador (guardado en mazo::rutaMazo), el reparto del duelo y la pantalla
	// "Construir mazo" (constructor.h; la dibuja interfaz.h). Ganchos en el juego: prepararMazos() al preparar el
	// mando (la mano interactiva = 5 robadas del mazo, la fusion a su zona), robarDelMazo() en la fase de robo y
	// avanzarConstructor() al empezar cada cuadro (panel tactil, tecla M en el PC o Ajustes > "Construir mazo").
	mazo::Coleccion coleccion;
	mazo::Biblioteca biblioteca;   // rama classic-constructor2: los mazos guardados (mazos.txt) y cual es el activo
	mazo::Mazo mazoJugador;   // el activo de la biblioteca (el que usa el duelo si es valido)
	mazo::Mazo mazoDuelo;     // con el que se juega: el guardado si es valido; si no, el inicial
	mazo::Mazo mazoRival;     // aleatorio valido de 40
	std::vector<std::string> pilaJ, pilaR, manoInicialJ;   // lo que queda en cada mazo (se roba de back())
	std::string rutaMazoJ;
	constructor::Constructor constructorMazo;
	control::Repetidor repetidorConstructor;
	bool pedidoConstructor{ false };
	bool pedidoLista{ false };   // rama classic-constructor2: LISTA DE CARTAS (solo ver)
	bool l2Antes{ false }, r2Antes{ false };
	float stickDerT{ 0.0f };

	void prepararMazos()
	{
		coleccion.leer(getAssetPath() + "ui/");
		rutaMazoJ = mazo::rutaMazo(getAssetPath());
		leerOpcionBanlist();
		biblioteca.cargar(rutaMazoJ, coleccion, idioma::ingles());   // (la primera vez: importa mazo.txt y los mazos de inicio)
		constructorMazo.torneos = mazo::leerMazosTorneo(getAssetPath() + "ui/");   // (rama classic-torneo-extras: MAZOS DE TORNEO)
		fprintf(stderr, "[mazo] mazos de torneo: %d\n", (int)constructorMazo.torneos.size());
		tomarMazoActivo();
		repartir();
		interfazDuelo.conectarConstructor(&constructorMazo);
#if defined(PS5_HOST_REFERENCE)
		if (getenv("YUGI_CONSTRUCTOR")) pedidoConstructor = true;   // PC: arranca en "Construir mazo"
		if (getenv("YUGI_LISTA_CARTAS")) pedidoLista = true;      // PC: arranca en la lista de cartas
#endif
	}

	// Opciones > "Permitir cartas prohibidas" (opciones.txt junto a mazo.txt): se relee al abrir el constructor y al
	// repartir, asi vale apenas se cambia en Opciones
	void leerOpcionBanlist()
	{
		constructor::aplicarOpcionProhibidas(coleccion, rutaMazoJ);
	}

	// el mazo de los duelos = el activo de la biblioteca (sin ninguno: el mazo inicial)
	void tomarMazoActivo()
	{
		if (const mazo::Mazo* a = biblioteca.mazoActivo()) mazoJugador = *a;
		else mazoJugador = mazo::Mazo::porDefecto(coleccion);
		fprintf(stderr, "[mazo] mazo de los duelos: '%s' (%d + %d)\n", mazoJugador.nombre.c_str(), (int)mazoJugador.principal.size(), (int)mazoJugador.fusion.size());
	}

	// baraja los dos mazos y roba la mano inicial del jugador
	void repartir()
	{
		leerOpcionBanlist();
		std::string motivo;
		if (mazoJugador.valido(coleccion, &motivo)) {
			mazoDuelo = mazoJugador;
		} else {
			fprintf(stderr, "[mazo] el mazo guardado no es valido (%s): se juega con el mazo inicial\n", motivo.c_str());
			mazoDuelo = mazo::Mazo::porDefecto(coleccion);
		}
		mazo::Azar azar((uint32_t)std::chrono::steady_clock::now().time_since_epoch().count() ^ 0x5bd1e995u);
		pilaJ = mazoDuelo.principal;
		mazo::barajar(pilaJ, azar);
		manoInicialJ.clear();
		for (int k = 0; k < mazo::MANO_INICIAL && !pilaJ.empty(); k++) {
			manoInicialJ.push_back(pilaJ.back());
			pilaJ.pop_back();
		}
		mazoRival = mazo::Mazo::aleatorio(coleccion, azar);
		pilaR = mazoRival.principal;
		mazo::barajar(pilaR, azar);
		for (int k = 0; k < interfaz::NUM_MANO_R && !pilaR.empty(); k++) pilaR.pop_back();   // (la mano del rival: 5 ocultas)
		contarMazos();
		fprintf(stderr, "[mazo] duelo: jugador %d en el mazo + %d en la mano, fusion %d; rival %d en el mazo\n", (int)pilaJ.size(), (int)manoInicialJ.size(),
		        (int)mazoDuelo.fusion.size(), (int)pilaR.size());
	}

	void contarMazos() { interfazDuelo.contarMazos((int)pilaJ.size(), (int)pilaR.size(), (int)mazoDuelo.fusion.size()); }

	// fase de robo: la carta de arriba del mazo a la mano (si cabe)
	void robarDelMazo()
	{
		if (pilaJ.empty()) {
			fprintf(stderr, "[mazo] no quedan cartas para robar\n");
			return;
		}
		if (ctl.robar(pilaJ.back(), interfaz::MAX_MANO_J)) {
			pilaJ.pop_back();
			contarMazos();
		}
	}

	// la zona de fusion del jugador muestra la primera fusion de su mazo
	void ponerFusionEnZona()
	{
		const int z = control::zonaPorId("jugador_fusion");
		if (z < 0) return;
		ctl.zonas[z] = control::Celda{};
		if (!mazoDuelo.fusion.empty()) ctl.zonas[z].carta = mazoDuelo.fusion[0];
	}

	// cada cuadro, despues de leer el mando: abre, maneja y cierra "Construir mazo". Abierto se queda con el mando
	// (el resto del cuadro no ve ningun boton)
	void avanzarConstructor()
	{
		if (!mandoListo) {
			return;
		}
		if (!constructorMazo.abierto) {
			// (constructor::abrirConstructorMazo() / abrirListaCartas(): las entradas del menu principal)
			const constructor::PedidoPantalla pedido = constructor::tomarPedido();
			pedidoConstructor |= pedido == constructor::PEDIDO_CONSTRUCTOR;
			pedidoLista |= pedido == constructor::PEDIDO_LISTA;
			if (pedidoConstructor || pedidoLista || ((mando.pressed & PAD_TOUCH_PAD) && !ajustesAbiertos() && ctl.vista.pantalla != control::VistaDuelo::DUELO)) {
				const bool lista = pedidoLista && !pedidoConstructor;
				pedidoConstructor = pedidoLista = false;
				if (coleccion.version != constructor::versionIdioma()) coleccion.leer(getAssetPath() + "ui/");   // (cambio el idioma)
				ui.visible = false;   // (cierra Ajustes si se abrio desde ahi)
				leerOpcionBanlist();
				if (lista) constructorMazo.abrirLista(coleccion, biblioteca);
				else constructorMazo.abrir(coleccion, biblioteca);
				sonido.sfx("sfx_constructor_abrir");   // rama classic-audio
				musica("musica_constructor");
				repetidorConstructor = control::Repetidor{};
				l2Antes = mando.l2 > 0.55f;
				r2Antes = mando.r2 > 0.55f;
				stickDerT = 0.0f;
				mando.pressed = 0;   // (el X que lo abrio no cuenta adentro)
			}
			if (!constructorMazo.abierto) return;
		}
		const float dt = glm::clamp(frameTimer, 0.0f, 0.1f);
		uint32_t extra = teclas | pulsosRaton;   // (PC: el teclado como botones; rama classic-pc: y el raton)
		teclas = 0;
		pulsosRaton = 0;
		if (guion.activo()) extra |= guion.avanzar(dt);   // (PC: YUGI_GUION tambien maneja el constructor)
		const uint32_t recien = botonesDe(mando.pressed) | extra;
		uint32_t p = repetidorConstructor.paso(botonesDe(mando.held), recien, mando.left_x, mando.left_y, dt) | (recien & ~control::B_DIRECCIONES);
		// L2/R2 (gatillos): clase rapida; L3: sin filtros; OPTIONS: guardar; stick derecho: saltar entre el mazo y la
		// coleccion (izq/der) y desplazar el texto de la carta (arriba/abajo), con repeticion
		const bool l2 = mando.l2 > 0.55f, r2 = mando.r2 > 0.55f;
		if (l2 && !l2Antes) p |= constructor::C_L2;
		if (r2 && !r2Antes) p |= constructor::C_R2;
		l2Antes = l2;
		r2Antes = r2;
		if (mando.pressed & PAD_L3) p |= constructor::C_L3;
		if (mando.pressed & PAD_OPTIONS) p |= constructor::C_OPTIONS;
		const float rx = mando.right_x, ry = mando.right_y;
		if (fabsf(rx) > 0.6f || fabsf(ry) > 0.6f) {
			stickDerT -= dt;
			if (stickDerT <= 0.0f) {
				if (fabsf(rx) > fabsf(ry)) p |= rx > 0.0f ? constructor::C_RS_DER : constructor::C_RS_IZQ;
				else p |= ry > 0.0f ? constructor::C_RS_ABAJO : constructor::C_RS_ARRIBA;
				stickDerT = stickDerT < -50.0f ? 0.38f : 0.12f;
			}
		} else {
			stickDerT = -99.0f;
		}
		const size_t antes = constructorMazo.mazo.principal.size() + constructorMazo.mazo.fusion.size();
		constructorMazo.avanzar(dt, p);
		const size_t despues = constructorMazo.mazo.principal.size() + constructorMazo.mazo.fusion.size();
		if (despues > antes) sonido.sfx("sfx_constructor_anadir");   // rama classic-audio
		else if (despues < antes) sonido.sfx("sfx_constructor_quitar");
		else if (p & control::B_DIRECCIONES) sonido.sfx("sfx_menu_cursor", 0.7f);
		if (!constructorMazo.abierto) {
			// se cerro (circulo en la lista de mazos): el mazo activo manda; el duelo se vuelve a repartir con el
			tomarMazoActivo();
			// (rama classic-arreglo-ciclo-md: desde la pausa en pleno duelo el duelo en curso no se toca; el mazo nuevo vale
			// para el proximo, que reparte al empezar: arrancarDueloIA. Antes se rearmaba mazoDuelo y las pilas del duelo)
			if (ctl.vista.pantalla != control::VistaDuelo::DUELO) repartir();
			else fprintf(stderr, "[mazo] constructor cerrado en pleno duelo: el mazo '%s' vale para el proximo duelo\n", mazoJugador.nombre.c_str());
			sonido.sfx("sfx_menu_cancelar");
			musicaDePantalla();
			// (rama classic-ps5-ritmo: en orden: lo que todavia no se presento, el robo de la Fase de Robo, no salta a la mesa)
			if (ctl.vista.pantalla == control::VistaDuelo::DUELO) sincronizarMesaEnOrden(sucesosVistos);
		}
		// el resto del cuadro no ve el mando
		mando.pressed = mando.held = 0;
		mando.left_x = mando.left_y = mando.right_x = mando.right_y = 0.0f;
		mando.l2 = mando.r2 = 0.0f;
	}
