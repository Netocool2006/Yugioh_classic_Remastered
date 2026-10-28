	// Yugioh Classic Remaster (rama classic-portada): se incluye DENTRO de la clase del juego (yugireload.cpp), despues
	// de menu_principal.h. La portada (control::VistaDuelo::PORTADA; la dibuja portada_ui.h) sale siempre al arrancar,
	// antes del menu principal: fundido desde negro, "CLASSIC REMASTERED" palpitando, musica del menu y "Pulsa X para
	// comenzar". X u OPTIONS: sonido de confirmar, fundido a negro y al menu principal. No hay salida desde aca (Salir
	// esta en el menu). En el PC con guion de pruebas (YUGI_GUION) no sale (menu_principal.h: prepararMenu).
	static constexpr float PORTADA_ESPERA = 0.8f;   // s antes de aceptar botones (el fundido de entrada)
	static constexpr float PORTADA_SALIDA = 0.6f;   // s del fundido a negro al pasar al menu
	float portadaSalida{ -1.0f };                   // < 0: sin salir; si no, s desde que se pulso X

	void irAPortada()
	{
		irAMenuPrincipal();   // (la mesa vacia, la musica del menu y las opciones)
		ctl.vista.pantalla = VD::PORTADA;
		ctl.vista.t = 0.0f;
		portadaSalida = -1.0f;
		interfazDuelo.portadaNegro = 1.0f;
		fprintf(stderr, "[portada] portada\n");
	}

	// botones en la portada (siempre se queda con el mando); avanza tambien el fundido de salida
	bool avanzarPortada(uint32_t p)
	{
		VD& v = ctl.vista;
		const float dt = std::min(std::max(frameTimer, 0.0f), 0.1f);
		if (portadaSalida >= 0.0f) {
			portadaSalida += dt;
			interfazDuelo.portadaNegro = std::min(1.0f, portadaSalida / PORTADA_SALIDA);
			if (portadaSalida >= PORTADA_SALIDA) {
				irAMenuPrincipal();
				interfazDuelo.portadaNegro = 1.0f;   // (el menu aparece desde negro: portada_ui.h)
				portadaSalida = -1.0f;
			}
			return true;
		}
		interfazDuelo.portadaNegro = 0.0f;
		const bool options = (mando.pressed & PAD_OPTIONS) || (p & control::B_OPTIONS);
		if (v.t > PORTADA_ESPERA && ((p & control::B_X) || options)) {
			fprintf(stderr, "[portada] comenzar (%s)\n", options ? "OPTIONS" : "X");
			sonido.sfx("sfx_menu_confirmar");
			portadaSalida = 0.0f;
		}
		return true;
	}
