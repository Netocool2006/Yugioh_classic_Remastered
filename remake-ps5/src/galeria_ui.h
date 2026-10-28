	// ================================================================ GALERIA DE FIGURAS 3D (rama classic-galeria)
	// (se incluye DENTRO de interfaz::Interfaz, despues de constructor_ui.h: usa sus piezas). La logica esta en galeria.h
	// y la figura en su pedestal la dibuja galeria_juego.h ANTES que esto: aca se tapa solo la izquierda (la rejilla) y
	// el visor de la derecha queda a la vista, con el nombre arriba, el candado de las bloqueadas y el detalle (triangulo)
	static constexpr float GAL_PANEL = 800.0f;   // ancho (px de 1080p) del panel de la rejilla; el visor es el resto

	void dibujarGaleria()
	{
		using constructor::T;
		const galeria::Galeria& g = *gal;
		rz.tapar();   // rama classic-pc
		usar(setAtlas, false);
		// ---- panel de la rejilla (opaco) y el borde del visor
		caja(0.0f, 0.0f, GAL_PANEL * s, H, 0.0f, PANEL, glm::vec4(0.012f, 0.018f, 0.036f, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		rect(glm::vec2(GAL_PANEL * 0.5f * s, H * 0.4f), glm::vec2(GAL_PANEL * 1.2f * s, H * 0.9f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(CIAN) * 0.07f, 1.0f), NADA, 0.0f,
		     glm::vec4(0.0f));
		caja(GAL_PANEL * s - 2.0f * s, 0.0f, 3.0f * s, H, 0.0f, PANEL, glm::vec4(glm::vec3(CIAN) * 0.55f, 0.8f), NADA, 0.0f, glm::vec4(0.0f));
		texto(F_NUMEROS, T("GALERÍA DE FIGURAS 3D"), 48.0f * s, 66.0f * s, 30.0f * s, BLANCO, CIAN, glm::vec3(0.5f, 0.0f, 0.6f), 0, 1.8f);
		textoCrudo(F_TITULO, std::to_string(g.entradas.size()) + T(" FIGURAS") + "  ·  " + std::to_string(g.desbloqueadas()) + T(" DESBLOQUEADAS"), 48.0f * s, 98.0f * s,
		           18.0f * s, GRIS);
		const float px = 24.0f * s, py = 116.0f * s, pw = (GAL_PANEL - 48.0f) * s, ph = 888.0f * s;
		panel(px, py, pw, ph, glm::vec4(glm::vec3(CIAN) * 0.6f, 1.0f));
		const float cel = 144.0f * s, celH = 214.0f * s;
		const glm::vec2 tamM(112.0f * s, 163.0f * s);
		const float gx = px + (pw - cel * galeria::COLS) * 0.5f, gy = py + 14.0f * s;
		const int n = (int)g.entradas.size();
		for (int f = 0; f < galeria::FILAS; f++) {
			for (int c = 0; c < galeria::COLS; c++) {
				const int i = (g.primeraFila + f) * galeria::COLS + c;
				if (i >= n) break;
				const galeria::Entrada& e = g.entradas[(size_t)i];
				const mazo::Carta* k = g.cartaDe(e);
				if (!k) continue;
				const bool sel = i == g.cursor, bloq = k->bloqueada;
				const glm::vec2 ce(gx + cel * ((float)c + 0.5f), gy + celH * (float)f + tamM.y * 0.5f + 8.0f * s);
				{   // rama classic-pc
					raton::Zona z;
					z.clic = control::B_X;
					z.e[0] = raton::ejeLista(c, g.cursor % galeria::COLS, control::B_DER, control::B_IZQ);
					z.e[1] = raton::ejeLista(g.primeraFila + f, g.cursor / galeria::COLS, control::B_ABAJO, control::B_ARRIBA);
					zRect(ce.x - tamM.x * 0.5f, ce.y - tamM.y * 0.5f, tamM.x, tamM.y, z);
				}
				const float esc = sel ? 1.0f + 0.06f * suave(0.0f, 0.12f, g.tCursor) : 1.0f;
				dibujarMiniCarta(*k, ce, tamM * esc, bloq ? 0.22f : 0.95f, sel && !bloq, (float)i * 0.13f);
				if (bloq) candado(ce, 1.2f * s, glm::vec4(0.85f, 0.88f, 0.95f, 1.0f));
				// figura con esqueleto (animada): un punto cian
				if (e.esqueleto) {
					usar(setAtlas, false);
					rect(ce + glm::vec2(tamM.x * 0.5f - 9.0f * s, -tamM.y * 0.5f + 9.0f * s), glm::vec2(11.0f * s), 0.0f, 0.0f, ORBE, CIAN, CIAN, 0.0f,
					     glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
				}
				nombreCelda(F_TEXTO, k->nombre, ce.x, ce.y + tamM.y * 0.5f + 19.0f * s, 14.0f * s, cel - 8.0f * s, sel ? ORO : glm::vec4(0.75f, 0.8f, 0.9f, 1.0f));
				if (sel) marcoCursor(ce, tamM * esc, 3.0f);
			}
		}
		const int filas = (n + galeria::COLS - 1) / galeria::COLS;
		if (g.primeraFila > 0) texto(F_TITULO, "▲", px + pw - 22.0f * s, py + 22.0f * s, 16.0f * s, CIAN, NADA, glm::vec3(0.0f), 2, 0.0f);
		if (g.primeraFila + galeria::FILAS < filas) texto(F_TITULO, "▼", px + pw - 22.0f * s, py + ph - 10.0f * s, 16.0f * s, CIAN, NADA, glm::vec3(0.0f), 2, 0.0f);
		if (n == 0) parrafo(F_TITULO, T("No hay figuras 3D cargadas"), px + 30.0f * s, py + 200.0f * s, 22.0f * s, pw - 60.0f * s, 28.0f * s, 3, GRIS);

		// ---- el visor: nombre, datos y el candado de las bloqueadas (la figura ya esta dibujada detras)
		const galeria::Entrada* e = g.actual();
		const mazo::Carta* k = g.carta();
		const float vx = GAL_PANEL * s, vw = W - vx, vc = vx + vw * 0.5f;
		if (e && k) {
			const float a = suave(0.0f, 0.25f, g.tCursor);
			auto A = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * a); };
			const bool bloq = k->bloqueada;
			const std::string nombre = k->nombre;
			textoCrudo(F_TITULO, recortar(F_TITULO, nombre, 38.0f * s, vw - 120.0f * s), vc, 74.0f * s, 38.0f * s, A(BLANCO), 1);
			if (!bloq) {
				std::string sub = k->atributo + " · " + k->raza + " · " + T("NIVEL ") + std::to_string(k->nivel) + "   ATK " + std::to_string(k->atk) + " / DEF " +
				                  std::to_string(k->def);
				textoCrudo(F_TITULO, recortar(F_TITULO, sub, 20.0f * s, vw - 120.0f * s), vc, 108.0f * s, 20.0f * s, A(ORO), 1);
				const std::string at = T("Ataque: ") + T(galeria::nombreAtaque(e->ataque)) + (e->esqueleto ? "  ·  " + T("Animada (esqueleto)") : std::string());
				textoCrudo(F_TITULO, at, vc, 136.0f * s, 17.0f * s, A(glm::vec4(0.7f, 0.85f, 1.0f, 0.9f)), 1);
			} else {
				const std::string v = g.col ? g.col->venceA(*k) + T(" para desbloquear") : std::string();
				const float yy = 880.0f * s;
				candado(glm::vec2(vc, yy - 70.0f * s), 1.6f * s, A(glm::vec4(0.85f, 0.88f, 0.95f, 1.0f)));
				const float tt = 24.0f * s, w = std::min(medirCrudo(F_TITULO, v, tt) + 70.0f * s, vw - 80.0f * s), h = 50.0f * s;
				usar(setAtlas, false);
				caja(vc - w * 0.5f, yy - h * 0.5f, w, h, 16.0f * s, PANEL, A(glm::vec4(0.03f, 0.04f, 0.08f, 0.92f)), A(glm::vec4(0.75f, 0.8f, 0.95f, 1.0f)), h * 0.5f,
				     glm::vec4(1.2f * s, 1.0f, 0.2f, 12.0f * s));
				textoCrudo(F_TITULO, recortar(F_TITULO, v, tt, w - 40.0f * s), vc, yy + 9.0f * s, tt, A(BLANCO), 1);
			}
			if (g.detalle) dibujarDetalleGaleria(*k, g);
		}

		// ---- barra de botones (con un velo para que se lea sobre la figura) y el aviso
		usar(setAtlas, false);
		caja(0.0f, 1014.0f * s, W, H - 1014.0f * s, 0.0f, PANEL, glm::vec4(0.01f, 0.015f, 0.03f, 0.82f), NADA, 0.0f, glm::vec4(0.0f));
		if (g.detalle) barraBotones({ { "CRUZ", "Desplazar" }, { "X", "Atacar" }, { "STICK_R", "Girar y acercar" }, { "T", "Cerrar detalle" }, { "O", "Cerrar detalle" } });
		else barraBotones({ { "CRUZ", "Elegir" }, { "X", "Atacar" }, { "T", "Detalle" }, { "STICK_R", "Girar y acercar" }, { "L2R2", "Zoom" }, { "R3", "Vista inicial" },
		                    { "L1R1", "Página" }, { "O", "Volver" } });
		if (!g.aviso.empty() && g.tAviso < 3.0f) {
			const float a = suave(0.0f, 0.15f, g.tAviso) * (1.0f - suave(2.5f, 3.0f, g.tAviso));
			auto A = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * a); };
			const float tt = 21.0f * s, w = std::min(medirCrudo(F_TITULO, g.aviso, tt) + 60.0f * s, W - 80.0f * s), h = 42.0f * s;
			const float cx = W * 0.5f, y = 960.0f * s;
			usar(setAtlas, false);
			caja(cx - w * 0.5f, y, w, h, 16.0f * s, PANEL, A(glm::vec4(0.03f, 0.04f, 0.08f, 0.97f)), A(ORO), h * 0.5f, glm::vec4(1.2f * s, 1.0f, 0.2f, 12.0f * s));
			textoCrudo(F_TITULO, recortar(F_TITULO, g.aviso, tt, w - 40.0f * s), cx, y + h * 0.5f + 8.0f * s, tt, A(BLANCO), 1);
		}
	}

	// el detalle de la carta (triangulo): a la derecha del visor, la carta grande y su texto
	void dibujarDetalleGaleria(const mazo::Carta& e, const galeria::Galeria& g)
	{
		using constructor::T;
		const float a = suave(0.0f, 0.18f, g.tDetalle);
		const float pw = 470.0f * s, px = W - pw - 24.0f * s + (1.0f - a) * 40.0f * s, py = 160.0f * s, ph = 840.0f * s;
		const glm::vec4 acento = colorClase(e.clase);
		panel(px, py, pw, ph, glm::mix(CIAN, acento, 0.5f));
		const int ui = buscarCarta(e.id);
		const glm::vec2 tam = glm::vec2(250.0f, 363.0f) * s;
		const glm::vec2 centro(px + pw * 0.5f, py + 16.0f * s + tam.y * 0.5f);
		if (e.bloqueada) {
			const Dorso dz = usarDorso(false, false, false, 1.0f, 0.0f);
			rectInclinado(centro, tam, 0.0f, 0.0f, 0.0f, DORSO, dz.col, dz.col2, 8.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
			usar(setAtlas, false);
			candado(centro, 2.2f * s, glm::vec4(0.85f, 0.88f, 0.95f, 1.0f));
		} else if (const CaraHD* h = ui >= 0 ? caraHD(ui) : nullptr) {
			usar(h->set, false);
			const float u = 0.5f / h->tex.width, vv = 0.5f / h->tex.height;
			rectInclinado(centro, tam, 0.0f, 0.0f, 0.0f, IMAGEN, glm::vec4(1.0f), acento, 8.0f * s, glm::vec4(0.5f, 1.0f, (float)ui * 0.13f, 0.0f), glm::vec2(u, vv),
			              glm::vec2(1.0f - u, 1.0f - vv));
		} else if (ui >= 0 && caraLista(ui)) {
			usar(cartas[ui].setCara, false);
			const float u = 0.5f / cartas[ui].cara.width, vv = 0.5f / cartas[ui].cara.height;
			rectInclinado(centro, tam, 0.0f, 0.0f, 0.0f, IMAGEN, glm::vec4(1.0f), acento, 8.0f * s, glm::vec4(0.5f, 1.0f, (float)ui * 0.13f, 0.0f), glm::vec2(u, vv),
			              glm::vec2(1.0f - u, 1.0f - vv));
		} else if (ui >= 0 && miniLista(ui)) {
			usar(cartas[ui].setMini, false);
			const float u = 0.5f / cartas[ui].mini.width, vv = 0.5f / cartas[ui].mini.height;
			rectInclinado(centro, tam, 0.0f, 0.0f, 0.0f, IMAGEN, glm::vec4(1.0f), acento, 8.0f * s, glm::vec4(0.0f), glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
		}
		usar(setAtlas, false);
		float y = py + 16.0f * s + tam.y + 34.0f * s;
		const float tx = px + 22.0f * s, tw = pw - 44.0f * s;
		if (e.bloqueada) {
			texto(F_NUMEROS, T("CARTA FIRMA BLOQUEADA"), tx, y, 17.0f * s, glm::vec4(0.8f, 0.85f, 0.95f, 1.0f), NADA, glm::vec3(0.0f), 0, 1.0f);
			if (g.col) parrafo(F_TITULO, g.col->venceA(e) + T(" para desbloquear"), tx, y + 34.0f * s, 20.0f * s, tw, 25.0f * s, 3, BLANCO);
			return;
		}
		{
			const bool antes = crudo;
			crudo = true;
			const int l = parrafo(F_TITULO, e.nombre, tx, y, 24.0f * s, tw, 25.0f * s, 2, BLANCO);
			crudo = antes;
			y += (float)l * 25.0f * s + 4.0f * s;
		}
		const std::string sub = nombreSubtipo(e.subtipo.empty() ? e.clase : e.subtipo);
		texto(F_TITULO, sub.empty() ? T("MONSTRUOS") : T(sub), tx, y, 17.0f * s, acento, NADA, glm::vec3(0.0f), 0, 1.0f);
		y += 24.0f * s;
		textoCrudo(F_TITULO, e.atributo + " · " + e.raza + " · " + T("NIVEL ") + std::to_string(e.nivel), tx, y, 17.0f * s, GRIS);
		y += 24.0f * s;
		textoCrudo(F_NUMEROS, "ATK " + std::to_string(e.atk) + "   DEF " + std::to_string(e.def), tx, y, 19.0f * s, ORO);
		y += 26.0f * s;
		const float tt = 16.5f * s, il = 20.0f * s, fin = py + ph - 20.0f * s;
		const std::vector<std::string> lineas = cortarLineasCrudo(F_TEXTO, e.texto, tt, tw);
		const int caben = std::max(1, (int)((fin - y) / il));
		const int desde = std::clamp(g.scrollTexto, 0, std::max(0, (int)lineas.size() - caben));
		for (int i = desde; i < (int)lineas.size() && i < desde + caben; i++)
			textoCrudo(F_TEXTO, lineas[(size_t)i], tx, y + (float)(i - desde) * il + tt, tt, glm::vec4(0.82f, 0.87f, 0.95f, 1.0f));
		if ((int)lineas.size() > caben) texto(F_TITULO, desde + caben < (int)lineas.size() ? "▼" : "▲", px + pw - 22.0f * s, fin + 2.0f * s, 16.0f * s, CIAN, NADA, glm::vec3(0.0f), 2, 0.0f);
	}
