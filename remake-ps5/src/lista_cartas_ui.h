	// Yugioh Classic Remaster (rama classic-ps5-elegir-cartas): se incluye DENTRO de la clase Interfaz (interfaz.h).
	// Pedido del usuario (PS5): "cuando haya una carta que te haga escoger entre cartas para sacar, debe sacar las cartas
	// igual que el de Steam. Ahorita saca una lista de nombres, eso no sirve porque uno no se sabe el nombre de todas las
	// cartas". El selector de cartas del duelo de Master Duel (GenericCardListEx / CardSelectionList, "Duel/ScriptableObject/
	// CardSelectionListSetting"): un panel con el TITULO de la pregunta, una fila de CARTAS con su arte que se desplaza con
	// flechas a los lados, la carta con el cursor mas grande y marcada con el cursor de seleccion (GUI_CommonSelectCursor),
	// el contador, y abajo el boton de confirmar / cancelar; la carta enfocada se lee entera (nombre, ATK / DEF y texto) en
	// el visor de la izquierda. Las cartas ocultas del rival se ven de dorso. Cruz: mueve; X: elige; O: cancela donde se puede.
	// Los menus que son opciones (Activar / No activar, si / no, posicion) siguen siendo menu de texto.
	void contenidoLista(Contenido& c)
	{
		if (!ctl || !ctl->listaCartas() || ctl->vista.pantalla != control::VistaDuelo::DUELO) {
			ultimaListaSel = -1;
			return;
		}
		const int n = (int)ctl->idsLista.size();
		const int k = glm::clamp(ctl->opcion, 0, n - 1);
		if (k != ultimaListaSel) {
			ultimaListaSel = k;
			tTexto = 0.0f;   // (el visor "escanea" la carta nueva)
		}
		c = Contenido{};
		const std::string& id = ctl->idsLista[(size_t)k];
		if (id.empty()) {
			c.dorso = true;
			c.rival = true;
			c.titulo = idioma::tr("Carta boca abajo");
			c.subtitulo = idioma::tr("Colocada por el rival");
			c.texto = idioma::tr("El rival guarda estas cartas ocultas.");
			c.color = CARMESI;
		} else {
			c.carta = buscarCarta(id);
		}
	}
	int ultimaListaSel{ -1 };

	void dibujarListaCartas()
	{
		if (!ctl || !ctl->listaCartas() || ctl->vista.pantalla != control::VistaDuelo::DUELO) return;
		const int n = (int)ctl->idsLista.size();
		const int nOp = (int)ctl->opciones.size();
		const bool extra = nOp > n;   // (Cancelar / No / No atacar)
		const bool enBoton = extra && ctl->opcion >= n;
		const int sel = glm::clamp(ctl->opcion, 0, n - 1);
		const float a = suave(0.0f, 0.16f, ctl->tMenu);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float pi = glm::pi<float>();
		// el panel, centrado sobre el campo
		const float cx = (CAMPO_X * s + W) * 0.5f;
		const float pw = glm::min(W - CAMPO_X * s - 70.0f * s, 1260.0f * s), ph = 372.0f * s;
		const float px = cx - pw * 0.5f, py = H * 0.5f - ph * 0.5f + 30.0f * s + (1.0f - a) * 16.0f * s;   // (debajo del retrato del rival)
		usar(setAtlas, false);
		rect(glm::vec2(cx, py + ph * 0.5f), glm::vec2(pw * 1.05f, ph * 1.25f), 0.0f, 0.0f, PARTICULA, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f * a), NADA, 0.0f, glm::vec4(0.0f));
		caja(px, py, pw, ph, 26.0f * s, PANEL, A(VIDRIO), A(glm::mix(CIAN, ORO, 0.55f)), 18.0f * s, glm::vec4(1.5f * s, 0.9f, 0.09f, 20.0f * s));
		rz.tapar();   // rama classic-pc: el selector de cartas se queda con el raton
		// el titulo: la pregunta (con la carta que se activa) y el contador
		std::string titulo = ctl->tituloTexto.empty() ? ctl->aviso : ctl->tituloTexto;
		if (titulo.empty()) titulo = idioma::t("Elige una carta", "Select a card");
		float tt = 26.0f * s;
		while (medir(F_TITULO, titulo, tt, 1.0f) > pw - 220.0f * s && tt > 15.0f * s) tt -= 1.0f * s;
		texto(F_TITULO, titulo, px + 30.0f * s, py + 46.0f * s, tt, A(ORO), NADA, glm::vec3(0.3f, 0.0f, 0.0f), 0, 1.0f);
		rect(glm::vec2(cx, py + 64.0f * s), glm::vec2(pw - 50.0f * s, 2.0f * s), 0.0f, 5.0f * s, LINEA, A(glm::vec4(glm::vec3(ORO), 0.8f)), NADA, 0.0f, glm::vec4(1.5f, 0.2f, 0.0f, 0.0f));
		texto(F_NUMEROS, std::to_string(sel + 1) + " / " + std::to_string(n), px + pw - 34.0f * s, py + 44.0f * s, 24.0f * s, A(BLANCO), A(ORO), glm::vec3(0.3f, 0.0f, 0.5f), 2, 1.5f);
		// la fila de cartas: una ventana alrededor de la elegida (se desplaza con las flechas)
		const glm::vec2 tam(124.0f * s, 180.0f * s);
		const float paso = tam.x + 20.0f * s;
		const int caben = glm::clamp((int)((pw - 150.0f * s) / paso), 1, n);
		const int ini = n <= caben ? 0 : glm::clamp(sel - caben / 2, 0, n - caben);
		const float ancho = (float)caben * paso - 20.0f * s;
		const float x0 = cx - ancho * 0.5f, yc = py + 64.0f * s + (ph - 64.0f * s - 70.0f * s) * 0.5f + 24.0f * s;
		auto carta = [&](int k, bool foco) {
			const int f = k - ini;
			glm::vec2 c(x0 + tam.x * 0.5f + (float)f * paso, yc);
			const float e = foco && !enBoton ? 1.0f + 0.2f * suave(0.0f, 0.12f, tTexto + 0.12f) : 1.0f;
			const glm::vec2 tm = tam * e;
			if (foco && !enBoton) c.y -= 10.0f * s;
			zLista(c.x - tam.x * 0.5f - 4.0f * s, c.y - tam.y * 0.5f, tam.x + 8.0f * s, tam.y, k, enBoton ? -1 : ctl->opcion, true);   // rama classic-pc
			rect(c + glm::vec2(0.0f, 8.0f * s), tm, 0.0f, 16.0f * s, PANEL, A(glm::vec4(0, 0, 0, 0.55f)), NADA, 8.0f * s, glm::vec4(0, 0, 0, 12.0f * s));
			const std::string& id = ctl->idsLista[(size_t)k];
			const int ci = id.empty() ? -1 : buscarCarta(id);
			const float luz = foco && !enBoton ? 1.08f : 0.86f;
			if (ci >= 0 && listaCarta(ci)) {
				usar(cartas[ci].setMini, false);
				const float u = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
				rect(c, tm, 0.0f, 0.0f, IMAGEN, A(glm::vec4(glm::vec3(luz), 1.0f)), A(glm::vec4(0.6f, 0.8f, 1.0f, 1.0f)), 7.0f * s,
				     glm::vec4(foco && !enBoton ? 0.55f : 0.15f, foco && !enBoton ? 1.0f : 0.25f, (float)k * 0.17f, 0), glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
			} else {
				const Dorso dz = usarDorso(false, false, false, 1.0f, (float)k);
				rect(c, tm, 0.0f, 0.0f, DORSO, A(dz.col), A(dz.col2), 7.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
			}
			usar(setAtlas, false);
			if (foco && !enBoton) {   // el cursor de seleccion de MD (GUI_CommonSelectCursor): marco dorado que late
				const float lat = 0.7f + 0.3f * sinf(t * 5.0f);
				caja(c.x - tm.x * 0.5f - 7.0f * s, c.y - tm.y * 0.5f - 7.0f * s, tm.x + 14.0f * s, tm.y + 14.0f * s, 10.0f * s, PANEL, NADA, A(glm::vec4(glm::vec3(ORO) * lat, 1.0f)),
				     12.0f * s, glm::vec4(3.2f * s, 1.4f, 0.0f, 16.0f * s));
				// (la V de MD arriba de la carta enfocada)
				rect(c + glm::vec2(0.0f, -tm.y * 0.5f - 22.0f * s + 3.0f * s * sinf(t * 6.0f)), glm::vec2(26.0f * s), pi, 12.0f * s, FLECHA, A(ORO), A(BLANCO), 0.0f, glm::vec4(0.0f));
			}
		};
		for (int k = ini; k < ini + caben; k++)
			if (k != sel || enBoton) carta(k, false);
		if (!enBoton) carta(sel, true);
		// las flechas de desplazamiento (hay mas cartas a cada lado)
		const float lat = 0.6f + 0.4f * sinf(t * 5.0f);
		if (ini > 0) texto(F_NUMEROS, "<", px + 34.0f * s, yc + 14.0f * s, 52.0f * s, A(glm::vec4(glm::vec3(ORO), lat)), NADA, glm::vec3(0.0f), 1, 1.0f);
		if (ini + caben < n) texto(F_NUMEROS, ">", px + pw - 34.0f * s, yc + 14.0f * s, 52.0f * s, A(glm::vec4(glm::vec3(ORO), lat)), NADA, glm::vec3(0.0f), 1, 1.0f);
		// abajo: el nombre de la carta enfocada y los botones (confirmar / cancelar)
		const std::string& idSel = ctl->idsLista[(size_t)sel];
		const int cs = idSel.empty() ? -1 : buscarCarta(idSel);
		const std::string nom = cs >= 0 ? cartas[cs].vNombre() : idioma::tr("Carta boca abajo");
		float tn = 22.0f * s;
		while (medir(F_TITULO, nom, tn, 1.0f) > pw * 0.5f && tn > 14.0f * s) tn -= 1.0f * s;
		const float yb = py + ph - 30.0f * s;
		texto(F_TITULO, nom, px + 30.0f * s, yb + 7.0f * s, tn, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
		float x = px + pw - 26.0f * s;
		if (extra) {   // (el ultimo: Cancelar / No / No atacar; se elige con la cruz hasta el y X)
			const std::string& ot = ctl->opciones.back().texto;
			const std::string tx = idioma::tr(ot.c_str());
			const float w = medir(F_TITULO, tx, 21.0f * s, 1.0f) + 44.0f * s;
			x -= w;
			zBoton(x, yb - 20.0f * s, w, 40.0f * s, control::B_CIRCULO);   // rama classic-pc: Cancelar / No
			caja(x, yb - 20.0f * s, w, 40.0f * s, 14.0f * s, PANEL, A(enBoton ? glm::vec4(0.32f, 0.23f, 0.05f, 0.8f) : glm::vec4(0.05f, 0.07f, 0.12f, 0.8f)),
			     A(enBoton ? ORO : glm::vec4(0.7f, 0.76f, 0.88f, 1.0f)), 20.0f * s, glm::vec4(1.2f * s, 1.0f, 0.0f, 0.0f));
			texto(F_TITULO, tx, x + w * 0.5f, yb + 7.0f * s, 21.0f * s, A(enBoton ? BLANCO : GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
			x -= 16.0f * s;
			iconoBoton(glm::vec2(x - 14.0f * s, yb), "O", 14.0f * s, a);
			x -= 40.0f * s;
		}
		const std::string ok = idioma::t("Confirmar", "Confirm");
		const float wok = medir(F_TITULO, ok, 21.0f * s, 1.0f);
		x -= wok;
		texto(F_TITULO, ok, x, yb + 7.0f * s, 21.0f * s, A(glm::vec4(0.88f, 0.92f, 0.98f, 1.0f)), NADA, glm::vec3(0.0f), 0, 1.0f);
		iconoBoton(glm::vec2(x - 24.0f * s, yb), "X", 14.0f * s, a);
	}
