// Yugioh Classic Remaster (rama classic-menus): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// El menu principal y sus Opciones (control::VistaDuelo::MENU; lo lleva menu_principal.h): velo sobre la mesa, el
// titulo del juego, un panel con las opciones (la elegida en dorado; en Opciones el valor a la derecha con < >) y la
// ayuda de botones. Todos los textos por idioma::tr (idioma.h).

private:
	void dibujarMenuPrincipal()
	{
		if (!ctl || (ctl->vista.pantalla != control::VistaDuelo::MENU && !ctl->vista.menuEnOpciones) || ctl->vista.menuOpciones.empty()) return;   // (Opciones tambien desde la pausa)
		const control::VistaDuelo& vd = ctl->vista;
		const float a = suave(0.0f, 0.3f, vd.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const int n = (int)vd.menuOpciones.size();
		const bool opc = vd.menuEnOpciones;
		// rama classic-portada: el arte de la puerta egipcia (portada_ui.h); los 5 botones con el texto por idioma
		const bool arte = vd.pantalla == control::VistaDuelo::MENU && fondoMenu(t, a, !opc);
		rz.tapar();   // rama classic-pc: el menu tapa la mesa
		if (arte && !opc) {
			dibujarBotonesPuerta(vd, a);
			return;
		}
		// (rama classic-ia-red: con 14 opciones la fila es mas baja para que el panel entre en la pantalla)
		const float fila = (n > 14 ? 48.0f : n > 12 ? 52.0f : n > 10 ? 58.0f : 64.0f) * s, w = (opc ? 820.0f : 620.0f) * s, cab = 110.0f * s, pie = (vd.menuSalir ? 120.0f : 76.0f) * s;
		const float h = cab + fila * (float)n + pie;
		const float cx = W * 0.5f, x0 = cx - w * 0.5f, y0 = H * 0.56f - h * 0.5f + (1.0f - a) * 20.0f * s;
		usar(setAtlas, false);
		if (arte) {   // Opciones sobre el arte del menu: un velo suave para que se lea el panel
			caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.01f, 0.0f, 0.03f, 0.45f)), NADA, 0.0f, glm::vec4(0.0f));
		} else {
			// velo sobre el tablero y un halo detras del panel
			caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.01f, 0.015f, 0.03f, 0.84f)), NADA, 0.0f, glm::vec4(0.0f));
			rect(glm::vec2(cx, H * 0.5f), glm::vec2(W * 0.9f, H * 0.9f), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(ORO) * 0.10f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
			// titulo del juego
			const float pulso = 1.0f + 0.015f * sinf(t * 2.0f);
			texto(F_NUMEROS, "YUGIOH CLASSIC REMASTER", cx, y0 - 110.0f * s, 64.0f * s * pulso, A(BLANCO), A(ORO), glm::vec3(0.7f, 0.0f, 0.9f), 1, 3.0f);
		}
		// panel
		caja(x0, y0, w, h, 30.0f * s, PANEL, A(VIDRIO), A(glm::mix(CIAN, ORO, 0.55f)), 22.0f * s, glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s));
		texto(F_NUMEROS, idioma::tr(opc ? "OPCIONES" : "MENÚ PRINCIPAL"), cx, y0 + 70.0f * s, 38.0f * s, A(BLANCO), A(CIAN), glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
		for (int k = 0; k < n; k++) {
			const bool sel = k == vd.menuOpcion;
			const float fy = y0 + cab + fila * (float)k;
			if (sel) {
				caja(x0 + 24.0f * s, fy + 4.0f * s, w - 48.0f * s, fila - 8.0f * s, 6.0f * s, PANEL, A(glm::vec4(0.32f, 0.23f, 0.05f, 0.6f)), A(ORO), (fila - 8.0f * s) * 0.5f,
				     glm::vec4(1.2f * s, 1.1f, 0.0f, 0.0f));
			}
			const glm::vec4 col = sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : glm::vec4(0.86f, 0.9f, 0.97f, 1.0f);
			const std::string& val = (size_t)k < vd.menuValores.size() ? vd.menuValores[(size_t)k] : std::string();
			// rama classic-pc: la fila se elige con el raton; en las que tienen valor, el clic cambia el valor (< a la izquierda de su centro, > a la derecha)
			if (opc && !val.empty()) {
				const float vx = x0 + w - 150.0f * s;
				zLista(x0 + 24.0f * s, fy + 4.0f * s, vx - 150.0f * s - (x0 + 24.0f * s), fila - 8.0f * s, k, vd.menuOpcion, false, 0);
				zLista(vx - 150.0f * s, fy + 4.0f * s, 150.0f * s, fila - 8.0f * s, k, vd.menuOpcion, false, control::B_IZQ);
				zLista(vx, fy + 4.0f * s, x0 + w - 24.0f * s - vx, fila - 8.0f * s, k, vd.menuOpcion, false, control::B_DER);
			} else {
				zLista(x0 + 24.0f * s, fy + 4.0f * s, w - 48.0f * s, fila - 8.0f * s, k, vd.menuOpcion);
			}
			if (opc) {
				texto(F_TITULO, idioma::tr(vd.menuOpciones[(size_t)k].c_str()), x0 + 54.0f * s, fy + fila * 0.5f + 10.0f * s, 29.0f * s, A(col), NADA, glm::vec3(0.0f), 0, 1.0f);
				if (!val.empty()) {
					const std::string v = sel ? "<  " + idioma::tr(val) + "  >" : idioma::tr(val);
					texto(F_TITULO, v, x0 + w - 150.0f * s, fy + fila * 0.5f + 10.0f * s, 29.0f * s, A(sel ? ORO : col), NADA, glm::vec3(0.0f), 1, 1.0f);
				} else if (sel) {
					iconoBoton(glm::vec2(x0 + w - 52.0f * s, fy + fila * 0.5f), "X", 14.0f * s, a);
				}
			} else {
				texto(F_TITULO, idioma::tr(vd.menuOpciones[(size_t)k].c_str()), cx, fy + fila * 0.5f + 11.0f * s, 32.0f * s, A(col), NADA, glm::vec3(0.0f), 1, 1.0f);
				if (sel) iconoBoton(glm::vec2(x0 + w - 52.0f * s, fy + fila * 0.5f), "X", 14.0f * s, a);
			}
		}
		float by = y0 + h - pie * 0.5f;
		if (vd.menuSalir) {
			texto(F_TITULO, idioma::tr("¿Salir del juego? Pulsa X otra vez para confirmar"), cx, y0 + h - pie + 34.0f * s, 23.0f * s, A(ORO), NADA, glm::vec3(0.0f), 1, 1.0f);
			by = y0 + h - 36.0f * s;
		}
		float bx = x0 + 40.0f * s;
		auto boton = [&](const std::string& b, const char* txt) {
			bx += iconoBoton(glm::vec2(bx + 14.0f * s, by), b, 14.0f * s, a) + 12.0f * s;
			bx += texto(F_TITULO, idioma::tr(txt), bx, by + 8.0f * s, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f) + 30.0f * s;
		};
		boton("CRUZ", opc ? "Opción / valor" : "Opción");
		boton("X", vd.menuSalir ? "Confirmar" : "Elegir");
		if (opc || vd.menuSalir) boton("O", "Volver");
	}

	// los 5 botones de la puerta (menu_principal.h: MODO DUELO, CONSTRUCCION DE DECK, LISTA DE CARTAS, OPCIONES,
	// SALIR): el texto centrado en cada caja, mayusculas en Rajdhani (condensada); el elegido mas claro. "Salir" pide
	// X otra vez en un cartel sobre el ojo de Horus. Abajo, la ayuda de botones
	void dibujarBotonesPuerta(const control::VistaDuelo& vd, float a)
	{
		const float e = escImagen();
		const int n = std::min((int)vd.menuOpciones.size(), 6);
		for (int k = 0; k < n; k++) {
			const float g = menuEncendido[(size_t)k];
			const glm::vec2 c = aPantalla((BOTON_X0 + BOTON_X1) * 0.5f, botonY(k) + BOTON_ALTO * 0.5f);
			{   // rama classic-pc: el boton de la puerta como zona del raton
				const glm::vec2 p0 = aPantalla(BOTON_X0, botonY(k)), p1 = aPantalla(BOTON_X1, botonY(k) + BOTON_ALTO);
				zLista(p0.x, p0.y, p1.x - p0.x, p1.y - p0.y, k, vd.menuOpcion);
			}
			if (k >= 5) {   // rama classic-en-linea: la sexta no esta pintada en la puerta: piedra oscura con borde dorado
				const glm::vec2 tam = glm::vec2(BOTON_X1 - BOTON_X0, BOTON_ALTO) * e;
				usar(setAtlas, false);
				caja(c.x - tam.x * 0.5f, c.y - tam.y * 0.5f, tam.x, tam.y, 10.0f * e, PANEL, glm::vec4(0.09f, 0.07f, 0.05f, 0.92f * a),
				     glm::vec4(0.78f, 0.62f, 0.3f, a), 14.0f * e, glm::vec4(3.0f * e, 1.1f, 0.15f, 12.0f * e));
			}
			const float tam = 50.0f * e * (1.0f + 0.04f * g);
			const glm::vec4 col = glm::mix(glm::vec4(0.93f, 0.88f, 0.76f, a), glm::vec4(1.0f, 0.98f, 0.9f, a), g);
			// (que no se salga del boton: si el texto es largo, la letra baja)
			const std::string txt = idioma::tr(vd.menuOpciones[(size_t)k]);
			const float ancho = medir(F_TITULO, txt, tam), maximo = (BOTON_X1 - BOTON_X0 - 110.0f) * e;
			const float t2 = ancho > maximo ? tam * maximo / ancho : tam;
			texto(F_TITULO, txt, c.x, c.y + t2 * 0.34f, t2, col, glm::vec4(glm::vec3(ORO), a * g), glm::vec3(0.6f * g, 0.08f, 0.0f), 1, 1.0f);
		}
		if (vd.menuSalir) {
			const glm::vec2 c = aPantalla((BOTON_X0 + BOTON_X1) * 0.5f, botonY(std::max(4, n - 1)) + BOTON_ALTO + 95.0f);
			const std::string txt = idioma::tr("¿Salir del juego? Pulsa X otra vez para confirmar");
			const float w = medir(F_TITULO, txt, 23.0f * s) + 60.0f * s;
			caja(c.x - w * 0.5f, c.y - 28.0f * s, w, 56.0f * s, 20.0f * s, PANEL, glm::vec4(0.05f, 0.02f, 0.08f, 0.88f * a), glm::vec4(glm::vec3(ORO), a), 14.0f * s,
			     glm::vec4(1.4f * s, 1.1f, 0.1f, 14.0f * s));
			texto(F_TITULO, txt, c.x, c.y + 8.0f * s, 23.0f * s, glm::vec4(glm::vec3(ORO), a), NADA, glm::vec3(0.0f), 1, 1.0f);
		}
		float bx = 48.0f * s, by = H - 40.0f * s;
		auto boton = [&](const std::string& b, const char* txt) {
			bx += iconoBoton(glm::vec2(bx + 14.0f * s, by), b, 14.0f * s, a) + 12.0f * s;
			bx += texto(F_TITULO, idioma::tr(txt), bx, by + 8.0f * s, 21.0f * s, glm::vec4(1.0f, 1.0f, 1.0f, a), NADA, glm::vec3(0.0f), 0, 1.0f) + 30.0f * s;
		};
		boton("CRUZ", "Opción");
		boton("X", vd.menuSalir ? "Confirmar" : "Elegir");
		if (vd.menuSalir) boton("O", "Volver");
	}

public:
