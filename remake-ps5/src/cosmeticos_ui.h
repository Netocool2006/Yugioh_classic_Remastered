// Yugioh Classic Remaster (rama classic-extras): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// Personalizacion (Opciones > Personalizacion, tambien desde la pausa): Funda, Tapete, Controles y Estadisticas.
//  - Fundas: las 8 tematicas se generan por codigo (cosmeticos::generarFunda) en otro hilo al arrancar y se suben a
//    la GPU en cuanto estan (subirFundas, en dibujar). usarDorso() pone la elegida en las cartas boca abajo del
//    jugador; las del rival siguen con el dorso clasico.
//  - Las pantallas leen cosmeticos::vista()/estado() y estadisticas::datos() (las lleva cosmeticos_juego.h).
// Textos ES/EN con idioma::t (sin tocar la tabla de idioma.h).

private:
	vks::Texture2D fundaTex[cosmeticos::NUM_FUNDAS], fundaMiniTex[cosmeticos::NUM_FUNDAS];
	VkDescriptorSet setFunda[cosmeticos::NUM_FUNDAS]{}, setFundaMini[cosmeticos::NUM_FUNDAS]{};
	bool hayFunda[cosmeticos::NUM_FUNDAS]{};
	int fundaDibujo{ -1 };   // >= 0: la funda que dibuja usarDorso (vistas previas); -1: la elegida
	std::thread hiloFundas;
	std::atomic<bool> fundasHechas{ false };
	std::vector<uint8_t> pixFundas[cosmeticos::NUM_FUNDAS];
	int fundaW{ 0 }, fundaH{ 0 };
	bool fundasSubidas{ false };
	int pantallaPersoAntes{ 0 };

	// al preparar (despues de cargarDorso): las fundas del mismo tamanno que el dorso, en otro hilo
	void empezarFundas()
	{
		if (!hayDorso) return;
		fundaW = (int)dorso.width;
		fundaH = (int)dorso.height;
		const std::string ui = rutaAssets + "ui/";
		hiloFundas = std::thread([this, ui]() {
			const auto t0 = std::chrono::steady_clock::now();
			for (int k = 1; k < cosmeticos::NUM_FUNDAS; k++) cosmeticos::generarFunda(k, ui, fundaW, fundaH, pixFundas[k]);
			fprintf(stderr, "[cosmeticos] %d fundas generadas en %.0f ms\n", cosmeticos::NUM_FUNDAS - 1,
			        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
			fundasHechas = true;
		});
	}
	// en cuanto el hilo termino: texturas (y su mitad para el tablero, como el dorso) y sus descriptores
	void subirFundas()
	{
		if (fundasSubidas || !fundasHechas) return;
		fundasSubidas = true;
		if (hiloFundas.joinable()) hiloFundas.join();
		for (int k = 1; k < cosmeticos::NUM_FUNDAS; k++) {
			std::vector<uint8_t>& px = pixFundas[k];
			const int w = fundaW, h = fundaH;
			if (px.size() != (size_t)w * h * 4) continue;
			fundaTex[k].fromBuffer(px.data(), px.size(), VK_FORMAT_R8G8B8A8_UNORM, w, h, disp, cola, VK_FILTER_LINEAR);
			const int mw = w / 2, mh = h / 2;
			std::vector<uint8_t> mini((size_t)mw * mh * 4);
			for (int y = 0; y < mh; y++)
				for (int x = 0; x < mw; x++)
					for (int c = 0; c < 4; c++) {
						const auto at = [&](int xx, int yy) { return (int)px[((size_t)yy * w + xx) * 4 + c]; };
						mini[((size_t)y * mw + x) * 4 + c] = (uint8_t)((at(2 * x, 2 * y) + at(2 * x + 1, 2 * y) + at(2 * x, 2 * y + 1) + at(2 * x + 1, 2 * y + 1) + 2) / 4);
					}
			fundaMiniTex[k].fromBuffer(mini.data(), mini.size(), VK_FORMAT_R8G8B8A8_UNORM, mw, mh, disp, cola, VK_FILTER_LINEAR);
			setFunda[k] = nuevoSet(fundaTex[k]);
			setFundaMini[k] = nuevoSet(fundaMiniTex[k]);
			hayFunda[k] = true;
			std::vector<uint8_t>().swap(px);
		}
	}
	void liberarFundas()
	{
		if (hiloFundas.joinable()) hiloFundas.join();
		for (int k = 1; k < cosmeticos::NUM_FUNDAS; k++)
			if (hayFunda[k]) {
				fundaTex[k].destroy();
				fundaMiniTex[k].destroy();
			}
	}
	// el descriptor del dorso del jugador (la funda elegida, o la de la vista previa)
	VkDescriptorSet setDorsoJugador(bool mini) const
	{
		const int f = fundaDibujo >= 0 ? fundaDibujo : cosmeticos::fundaVisible();
		if (f > 0 && f < cosmeticos::NUM_FUNDAS && hayFunda[f]) return mini ? setFundaMini[f] : setFunda[f];
		return mini ? setDorsoMini : setDorso;
	}

	// ---------------------------------------------------------------------------------------------- piezas
	void cartaFunda(int k, glm::vec2 c, glm::vec2 tam, float brillo, float alfa)
	{
		usar(setAtlas, false);
		rect(c + glm::vec2(5.0f, 8.0f) * s, tam, 0.0f, 12.0f * s, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f * alfa), NADA, 8.0f * s, glm::vec4(0.0f, 0.0f, 0.0f, 10.0f * s));
		fundaDibujo = k;
		const Dorso dz = usarDorso(false, false, false, brillo, 0.0f);
		rect(c, tam, 0.0f, 0.0f, DORSO, glm::vec4(glm::vec3(dz.col), dz.col.a * alfa), dz.col2, 9.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		fundaDibujo = -1;
		usar(setAtlas, false);
	}
	void candado(glm::vec2 c, float r, float alfa)
	{
		usar(setAtlas, false);
		const glm::vec4 oro(glm::vec3(ORO) * 0.95f, alfa), oscuro(0.05f, 0.04f, 0.06f, 0.92f * alfa);
		rect(c + glm::vec2(0.0f, r * 0.15f), glm::vec2(r * 2.3f), 0.0f, 6.0f * s, ORBE, oscuro, oscuro, 0.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
		rect(c - glm::vec2(0.0f, r * 0.42f), glm::vec2(r * 0.95f, r * 1.1f), 0.0f, 0.0f, PANEL, NADA, oro, r * 0.47f, glm::vec4(2.6f * s, 2.4f, 0.0f, 0.0f));
		rect(c + glm::vec2(0.0f, r * 0.35f), glm::vec2(r * 1.45f, r * 1.0f), 0.0f, 2.0f * s, PANEL, oro, NADA, 3.0f * s, glm::vec4(0.0f));
		rect(c + glm::vec2(0.0f, r * 0.3f), glm::vec2(r * 0.28f), 0.0f, 0.0f, ORBE, oscuro, oscuro, 0.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
	}
	// panel comun: velo, caja, titulo y la ayuda de botones abajo. Devuelve el alfa de aparicion
	float panelPerso(float x0, float y0, float w, float h, const char* titulo)
	{
		const float a = suave(0.0f, 0.25f, cosmeticos::vista().t);
		usar(setAtlas, false);
		// (rama classic-visual-final: velo casi negro y fondo opaco: el menu de Opciones de atras no se transparenta)
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.01f, 0.0f, 0.03f, 0.97f * a), NADA, 0.0f, glm::vec4(0.0f));
		caja(x0, y0 + (1.0f - a) * 20.0f * s, w, h, 30.0f * s, PANEL, glm::vec4(0.02f, 0.03f, 0.06f, a), NADA, 22.0f * s, glm::vec4(0.0f));
		caja(x0, y0 + (1.0f - a) * 20.0f * s, w, h, 30.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), VIDRIO.a * a), glm::vec4(glm::vec3(glm::mix(CIAN, ORO, 0.55f)), a), 22.0f * s,
		     glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s));
		texto(F_NUMEROS, titulo, x0 + w * 0.5f, y0 + 66.0f * s, 38.0f * s, glm::vec4(glm::vec3(BLANCO), a), glm::vec4(glm::vec3(CIAN), a), glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
		return a;
	}
	void ayudaPerso(float x, float y, float a, std::initializer_list<std::pair<const char*, const char*>> botones)
	{
		for (const auto& b : botones) {
			const float wi = iconoBoton(glm::vec2(x + 14.0f * s, y), b.first, 14.0f * s, a);
			x += wi + 12.0f * s;
			x += texto(F_TITULO, b.second, x, y + 8.0f * s, 21.0f * s, glm::vec4(glm::vec3(BLANCO), a), NADA, glm::vec3(0.0f), 0, 1.0f) + 30.0f * s;
		}
	}
	std::string nombreRival(const estadisticas::Rival& r) const { return idioma::ingles() && !r.en.empty() ? r.en : r.es.empty() ? r.clave : r.es; }

	// ---------------------------------------------------------------------------------------------- pantallas
	void dibujarPersonalizacion()
	{
		cosmeticos::VistaPerso& pv = cosmeticos::vista();
		if (pv.pantalla != pantallaPersoAntes) {
			pantallaPersoAntes = pv.pantalla;
			pv.t = 0.0f;
		}
		if (pv.pantalla == cosmeticos::P_CERRADA) return;
		pv.t += dtCuadro;
		pv.tAviso += dtCuadro;
		rz.tapar();   // rama classic-pc: Personalizacion se queda con el raton
		switch (pv.pantalla) {
		case cosmeticos::P_MENU: dibujarPersoMenu(); break;
		case cosmeticos::P_FUNDA: dibujarPersoFundas(); break;
		case cosmeticos::P_TAPETE: dibujarPersoTapetes(); break;
		case cosmeticos::P_CONTROLES: dibujarPersoControles(); break;
		case cosmeticos::P_ESTADISTICAS: dibujarPersoEstadisticas(); break;
		default: break;
		}
	}

	void dibujarPersoMenu()
	{
		const cosmeticos::VistaPerso& pv = cosmeticos::vista();
		const int n = cosmeticos::NUM_PERSO;
		const float fila = 72.0f * s, w = 1180.0f * s, cab = 110.0f * s, pie = 80.0f * s, h = cab + fila * (float)n + pie;
		const float x0 = W * 0.5f - w * 0.5f, y0 = H * 0.5f - h * 0.5f;
		const float a = panelPerso(x0, y0, w, h, idioma::t("PERSONALIZACIÓN", "CUSTOMIZATION"));
		const float lw = 780.0f * s;   // la lista; a la derecha, la funda y el tapete en uso
		for (int k = 0; k < n; k++) {
			const bool sel = k == pv.opcion;
			const float fy = y0 + cab + fila * (float)k;
			zLista(x0 + 24.0f * s, fy + 5.0f * s, lw - 48.0f * s, fila - 10.0f * s, k, pv.opcion);   // rama classic-pc
			if (sel)
				caja(x0 + 24.0f * s, fy + 5.0f * s, lw - 48.0f * s, fila - 10.0f * s, 6.0f * s, PANEL, glm::vec4(0.32f, 0.23f, 0.05f, 0.6f * a), glm::vec4(glm::vec3(ORO), a),
				     (fila - 10.0f * s) * 0.5f, glm::vec4(1.2f * s, 1.1f, 0.0f, 0.0f));
			const glm::vec4 col = sel ? glm::vec4(1.0f, 0.95f, 0.8f, a) : glm::vec4(0.86f, 0.9f, 0.97f, a);
			texto(F_TITULO, cosmeticos::textoOpcionPerso(k), x0 + 56.0f * s, fy + fila * 0.5f + 10.0f * s, 30.0f * s, col, NADA, glm::vec3(0.0f), 0, 1.0f);
			std::string val;
			if (k == cosmeticos::PERSO_FUNDA) val = cosmeticos::nombreFunda(cosmeticos::fundaVisible());
			if (k == cosmeticos::PERSO_TAPETE) val = cosmeticos::nombreTapete(cosmeticos::tapeteVisible());
			if (!val.empty()) texto(F_TITULO, val, x0 + lw - 90.0f * s, fy + fila * 0.5f + 10.0f * s, 26.0f * s, glm::vec4(glm::vec3(sel ? ORO : GRIS), a), NADA, glm::vec3(0.0f), 2, 1.0f);
			if (sel) iconoBoton(glm::vec2(x0 + lw - 60.0f * s, fy + fila * 0.5f), "X", 14.0f * s, a);
		}
		// la funda en uso, grande, y el color del tapete
		const glm::vec2 carta(230.0f * s, 333.0f * s);
		const glm::vec2 cc(x0 + lw + (w - lw) * 0.5f, y0 + cab + carta.y * 0.5f + 10.0f * s);
		cartaFunda(cosmeticos::fundaVisible(), cc, carta, 1.0f, a);
		// (rama classic-visual-final) debajo, el nombre de la funda en una placa (y como se gana, si estuviera bloqueada)
		{
			const int fv = cosmeticos::fundaVisible();
			const std::string nom = cosmeticos::nombreFunda(fv);
			const std::string sub = cosmeticos::estado().fundaLibre[fv] ? std::string() : cosmeticos::venceA(cosmeticos::estado().fundaPorEs[fv], cosmeticos::estado().fundaPorEn[fv]);
			const float tn = 22.0f * s, ts = 16.0f * s;
			const float pw = std::max(carta.x, std::max(medir(F_TITULO, nom, tn, 1.0f), sub.empty() ? 0.0f : medir(F_TITULO, sub, ts, 1.0f)) + 36.0f * s);
			const float ph = sub.empty() ? 36.0f * s : 56.0f * s, py = cc.y + carta.y * 0.5f + 18.0f * s;
			usar(setAtlas, false);
			caja(cc.x - pw * 0.5f, py, pw, ph, 4.0f * s, PANEL, glm::vec4(0.03f, 0.04f, 0.08f, 0.95f * a), glm::vec4(glm::vec3(ORO) * 0.8f, a), ph * 0.5f,
			     glm::vec4(1.2f * s, 1.0f, 0.0f, 0.0f));
			texto(F_TITULO, nom, cc.x, py + (sub.empty() ? ph * 0.5f + 8.0f * s : 25.0f * s), tn, glm::vec4(glm::vec3(ORO), a), NADA, glm::vec3(0.0f), 1, 1.0f);
			if (!sub.empty()) texto(F_TITULO, sub, cc.x, py + 46.0f * s, ts, glm::vec4(glm::vec3(GRIS), a), NADA, glm::vec3(0.0f), 1, 1.0f);
		}
		ayudaPerso(x0 + 40.0f * s, y0 + h - 40.0f * s, a, { { "CRUZ", idioma::t("Opción", "Option") }, { "X", idioma::t("Abrir", "Open") }, { "O", idioma::t("Volver", "Back") } });
	}

	void dibujarPersoFundas()
	{
		const cosmeticos::VistaPerso& pv = cosmeticos::vista();
		const cosmeticos::Estado& e = cosmeticos::estado();
		const float w = 1500.0f * s, h = 940.0f * s, x0 = W * 0.5f - w * 0.5f, y0 = H * 0.5f - h * 0.5f;
		const float a = panelPerso(x0, y0, w, h, idioma::t("FUNDAS", "CARD SLEEVES"));
		const int cols = 5;
		const glm::vec2 tam(170.0f * s, 247.0f * s);
		const float paso = 250.0f * s, filaY = 300.0f * s;
		for (int k = 0; k < cosmeticos::NUM_FUNDAS; k++) {
			const int fx = k % cols, fy = k / cols;
			const int enFila = fy == 0 ? std::min(cols, (int)cosmeticos::NUM_FUNDAS) : (int)cosmeticos::NUM_FUNDAS - cols;
			const glm::vec2 c(x0 + w * 0.5f + ((float)fx - (float)(enFila - 1) * 0.5f) * paso, y0 + 120.0f * s + tam.y * 0.5f + (float)fy * filaY);
			const bool sel = k == pv.sel, libre = e.fundaLibre[k], puesta = k == cosmeticos::fundaVisible();
			{   // rama classic-pc
				raton::Zona z;
				z.clic = control::B_X;
				z.e[0] = raton::ejeLista(fx, pv.sel % cols, control::B_DER, control::B_IZQ);
				z.e[1] = raton::ejeLista(fy, pv.sel / cols, control::B_ABAJO, control::B_ARRIBA);
				zRect(c.x - tam.x * 0.5f, c.y - tam.y * 0.5f, tam.x, tam.y, z);
			}
			const float esc = sel ? 1.08f + 0.015f * sinf(t * 3.0f) : 1.0f;
			if (sel) {
				usar(setAtlas, false);
				rect(c, tam * esc + glm::vec2(18.0f * s), 0.0f, 10.0f * s, PANEL, NADA, glm::vec4(glm::vec3(ORO), a), 14.0f * s, glm::vec4(2.0f * s, 1.4f, 0.3f, 14.0f * s));
			}
			cartaFunda(k, c, tam * esc, libre ? 1.0f : 0.32f, a);
			if (!libre) candado(c, 26.0f * s, a);
			if (puesta) {
				usar(setAtlas, false);
				const glm::vec2 b(c.x, c.y + tam.y * esc * 0.5f - 18.0f * s);
				caja(b.x - 70.0f * s, b.y - 16.0f * s, 140.0f * s, 32.0f * s, 4.0f * s, PANEL, glm::vec4(0.05f, 0.25f, 0.12f, 0.9f * a), glm::vec4(0.4f, 1.0f, 0.6f, a), 16.0f * s,
				     glm::vec4(1.0f * s, 1.0f, 0.0f, 0.0f));
				texto(F_TITULO, idioma::t("EN USO", "IN USE"), b.x, b.y + 7.0f * s, 19.0f * s, glm::vec4(0.8f, 1.0f, 0.85f, a), NADA, glm::vec3(0.0f), 1, 1.5f);
			}
		}
		// la elegida: nombre y estado
		const int k = pv.sel;
		const float ty = y0 + h - 150.0f * s;
		texto(F_TITULO, cosmeticos::nombreFunda(k), x0 + w * 0.5f, ty, 36.0f * s, glm::vec4(glm::vec3(ORO), a), NADA, glm::vec3(0.3f, 0.0f, 0.0f), 1, 1.0f);
		std::string estadoTxt;
		glm::vec4 colE = glm::vec4(glm::vec3(GRIS), a);
		if (!e.fundaLibre[k]) {
			estadoTxt = cosmeticos::venceA(e.fundaPorEs[k], e.fundaPorEn[k]);
			if (!pv.aviso.empty() && pv.tAviso < 1.2f) colE = glm::vec4(1.0f, 0.45f, 0.5f, a);
		} else {
			estadoTxt = k == cosmeticos::fundaVisible() ? idioma::t("Es la funda de tu mazo en los duelos", "This is your deck's sleeve in duels")
			                                            : idioma::t("Pulsa X para usarla en tu mazo", "Press X to use it on your deck");
		}
		texto(F_TITULO, estadoTxt, x0 + w * 0.5f, ty + 46.0f * s, 25.0f * s, colE, NADA, glm::vec3(0.0f), 1, 1.0f);
		ayudaPerso(x0 + 40.0f * s, y0 + h - 40.0f * s, a, { { "CRUZ", idioma::t("Elegir", "Choose") }, { "X", idioma::t("Usar", "Use") }, { "O", idioma::t("Volver", "Back") } });
	}

	void dibujarPersoTapetes()
	{
		const cosmeticos::VistaPerso& pv = cosmeticos::vista();
		const cosmeticos::Estado& e = cosmeticos::estado();
		const int n = cosmeticos::NUM_TAPETES;
		const float fila = 82.0f * s, w = 1400.0f * s, cab = 110.0f * s, pie = 170.0f * s, h = cab + fila * (float)n + pie;
		const float x0 = W * 0.5f - w * 0.5f, y0 = H * 0.5f - h * 0.5f;
		const float a = panelPerso(x0, y0, w, h, idioma::t("TAPETE", "DUEL FIELD"));
		for (int k = 0; k < n; k++) {
			const cosmeticos::Tapete& tp = cosmeticos::TAPETES[k];
			const bool sel = k == pv.sel, libre = e.tapeteLibre[k], puesto = k == cosmeticos::tapeteVisible();
			const float fy = y0 + cab + fila * (float)k;
			zLista(x0 + 24.0f * s, fy + 5.0f * s, w - 48.0f * s, fila - 10.0f * s, k, pv.sel);   // rama classic-pc
			usar(setAtlas, false);
			if (sel)
				caja(x0 + 24.0f * s, fy + 5.0f * s, w - 48.0f * s, fila - 10.0f * s, 6.0f * s, PANEL, glm::vec4(0.32f, 0.23f, 0.05f, 0.6f * a), glm::vec4(glm::vec3(ORO), a),
				     (fila - 10.0f * s) * 0.5f, glm::vec4(1.2f * s, 1.1f, 0.0f, 0.0f));
			// muestra: el color del material, con un brillo que late
			const float late = 0.8f + 0.2f * sinf(t * 2.0f + (float)k);
			const float br = libre ? 1.0f : 0.35f;
			caja(x0 + 56.0f * s, fy + 14.0f * s, 120.0f * s, fila - 28.0f * s, 4.0f * s, PANEL, glm::vec4(tp.r * 0.45f * br, tp.g * 0.45f * br, tp.b * 0.45f * br, a),
			     glm::vec4(tp.r * late * br, tp.g * late * br, tp.b * late * br, a), 10.0f * s, glm::vec4(1.4f * s, 1.0f, 0.2f, 6.0f * s));
			if (!libre) candado(glm::vec2(x0 + 116.0f * s, fy + fila * 0.5f), 15.0f * s, a);
			const glm::vec4 col = !libre ? glm::vec4(0.6f, 0.64f, 0.72f, a) : sel ? glm::vec4(1.0f, 0.95f, 0.8f, a) : glm::vec4(0.86f, 0.9f, 0.97f, a);
			texto(F_TITULO, cosmeticos::nombreTapete(k), x0 + 210.0f * s, fy + fila * 0.5f + 10.0f * s, 29.0f * s, col, NADA, glm::vec3(0.0f), 0, 1.0f);
			std::string der;
			glm::vec4 colD = glm::vec4(glm::vec3(GRIS), a);
			if (puesto) {
				der = idioma::t("EN USO", "IN USE");
				colD = glm::vec4(0.6f, 1.0f, 0.7f, a);
			} else if (!libre) {
				der = cosmeticos::venceA(e.tapetePorEs[k], e.tapetePorEn[k]);
				if (sel && !pv.aviso.empty() && pv.tAviso < 1.2f) colD = glm::vec4(1.0f, 0.45f, 0.5f, a);
			}
			if (!der.empty()) texto(F_TITULO, der, x0 + w - 60.0f * s, fy + fila * 0.5f + 9.0f * s, 23.0f * s, colD, NADA, glm::vec3(0.0f), 2, 1.0f);
		}
		parrafo(F_TEXTO,
		        idioma::t("El automático cambia con el escenario de cada rival. Los fijos cubren tu tapete en todos los duelos; una carta de campo activa sigue mandando sobre él.",
		                  "Automatic changes with each opponent's stage. A fixed one covers your field in every duel; an active Field Spell still takes over."),
		        x0 + 56.0f * s, y0 + h - pie + 50.0f * s, 24.0f * s, w - 112.0f * s, 32.0f * s, 2, glm::vec4(glm::vec3(GRIS), a));
		ayudaPerso(x0 + 40.0f * s, y0 + h - 40.0f * s, a, { { "CRUZ", idioma::t("Elegir", "Choose") }, { "X", idioma::t("Usar", "Use") }, { "O", idioma::t("Volver", "Back") } });
	}

	// ---- controles: el DualSense dibujado y la lista de la pestanna; el boton de la fila elegida late en el mando
	glm::vec2 posBotonMando(int b, glm::vec2 c, float u) const
	{
		static const float P[cosmeticos::NUM_BM][2] = {
			{ -170, -25 },   // cruz
			{ -88, 72 },     // stick izq
			{ 88, 72 },      // stick der
			{ 170, 22 },     // X
			{ 216, -25 },    // O
			{ 170, -72 },    // triangulo
			{ 124, -25 },    // cuadrado
			{ -170, -150 },  // L1
			{ 170, -150 },   // R1
			{ -170, -192 },  // L2
			{ 170, -192 },   // R2
			{ -88, 72 },     // L3 (el stick izq)
			{ 88, 72 },      // R3
			{ 122, -112 },   // OPTIONS
			{ 0, -70 },      // panel tactil
		};
		return c + glm::vec2(P[b][0], P[b][1]) * u;
	}
	void dibujarMando(glm::vec2 c, float u, float a, int resaltado)
	{
		usar(setAtlas, false);
		const glm::vec4 cuerpo(0.86f, 0.88f, 0.92f, a), sombra(0.62f, 0.65f, 0.72f, a), negro(0.08f, 0.09f, 0.12f, a);
		// gatillos y botones superiores
		for (int lado : { -1, 1 }) {
			caja(c.x + lado * 170.0f * u - 55.0f * u, c.y - 212.0f * u, 110.0f * u, 40.0f * u, 3.0f * u, PANEL, negro, sombra, 14.0f * u, glm::vec4(1.0f * u, 1.0f, 0.0f, 0.0f));
			caja(c.x + lado * 170.0f * u - 60.0f * u, c.y - 165.0f * u, 120.0f * u, 28.0f * u, 3.0f * u, PANEL, sombra, cuerpo, 10.0f * u, glm::vec4(1.0f * u, 1.0f, 0.0f, 0.0f));
		}
		// asas
		for (int lado : { -1, 1 }) {
			rect(c + glm::vec2(lado * 195.0f, 95.0f) * u, glm::vec2(150.0f, 260.0f) * u, lado * -0.42f, 3.0f * u, PANEL, cuerpo, sombra, 70.0f * u, glm::vec4(1.5f * u, 1.0f, 0.0f, 0.0f));
		}
		// cuerpo, panel tactil y barra de luz
		caja(c.x - 280.0f * u, c.y - 150.0f * u, 560.0f * u, 250.0f * u, 3.0f * u, PANEL, cuerpo, sombra, 90.0f * u, glm::vec4(1.5f * u, 1.0f, 0.0f, 0.0f));
		caja(c.x - 105.0f * u, c.y - 142.0f * u, 210.0f * u, 135.0f * u, 3.0f * u, PANEL, glm::vec4(0.12f, 0.13f, 0.17f, a), glm::vec4(0.3f, 0.6f, 1.0f, a), 18.0f * u,
		     glm::vec4(1.2f * u, 1.0f, 0.3f, 6.0f * u));
		// cruz, sticks, botones de la derecha
		iconoBoton(posBotonMando(cosmeticos::BM_CRUZ, c, u), "CRUZ", 30.0f * u, a);
		for (int b : { (int)cosmeticos::BM_STICK_I, (int)cosmeticos::BM_STICK_D }) {
			const glm::vec2 p = posBotonMando(b, c, u);
			usar(setAtlas, false);
			rect(p, glm::vec2(78.0f * u), 0.0f, 3.0f * u, ORBE, negro, glm::vec4(0.3f, 0.32f, 0.4f, a), 0.0f, glm::vec4(0.25f, 1.0f, 0.0f, 0.0f));
			rect(p, glm::vec2(50.0f * u), 0.0f, 0.0f, ORBE, glm::vec4(0.22f, 0.24f, 0.3f, a), glm::vec4(0.12f, 0.13f, 0.18f, a), 0.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
			texto(F_NUMEROS, b == cosmeticos::BM_STICK_I ? "L" : "R", p.x, p.y + 7.0f * u, 20.0f * u, glm::vec4(glm::vec3(BLANCO), a), NADA, glm::vec3(0.0f), 1);
		}
		for (int b : { (int)cosmeticos::BM_X, (int)cosmeticos::BM_O, (int)cosmeticos::BM_T, (int)cosmeticos::BM_C }) {
			const char* ic = b == cosmeticos::BM_X ? "X" : b == cosmeticos::BM_O ? "O" : b == cosmeticos::BM_T ? "T" : "C";
			iconoBoton(posBotonMando(b, c, u), ic, 22.0f * u, a);
		}
		usar(setAtlas, false);
		for (int lado : { -1, 1 }) {   // create / options
			const glm::vec2 p = c + glm::vec2(lado * 122.0f, -112.0f) * u;
			rect(p, glm::vec2(16.0f, 30.0f) * u, 0.0f, 2.0f * u, PANEL, negro, NADA, 8.0f * u, glm::vec4(0.0f));
		}
		texto(F_NUMEROS, "L1", c.x - 170.0f * u, c.y - 145.0f * u, 18.0f * u, glm::vec4(glm::vec3(negro), a), NADA, glm::vec3(0.0f), 1);
		texto(F_NUMEROS, "R1", c.x + 170.0f * u, c.y - 145.0f * u, 18.0f * u, glm::vec4(glm::vec3(negro), a), NADA, glm::vec3(0.0f), 1);
		texto(F_NUMEROS, "L2", c.x - 170.0f * u, c.y - 186.0f * u, 18.0f * u, glm::vec4(glm::vec3(BLANCO), a), NADA, glm::vec3(0.0f), 1);
		texto(F_NUMEROS, "R2", c.x + 170.0f * u, c.y - 186.0f * u, 18.0f * u, glm::vec4(glm::vec3(BLANCO), a), NADA, glm::vec3(0.0f), 1);
		// el boton de la fila elegida: un aro dorado que late
		if (resaltado >= 0) {
			const glm::vec2 p = posBotonMando(resaltado, c, u);
			const float late = 0.65f + 0.35f * sinf(t * 5.0f);
			const float r = resaltado == cosmeticos::BM_TACTIL ? 120.0f : resaltado == cosmeticos::BM_CRUZ ? 80.0f : resaltado >= cosmeticos::BM_L1 && resaltado <= cosmeticos::BM_R2 ? 70.0f : 60.0f;
			usar(setAtlas, false);
			rect(p, glm::vec2(r * u), 0.0f, 10.0f * u, PANEL, NADA, glm::vec4(glm::vec3(ORO) * late, a), r * 0.5f * u, glm::vec4(3.0f * u, 1.6f, 0.4f, 12.0f * u));
		}
	}
	void dibujarPersoControles()
	{
		const cosmeticos::VistaPerso& pv = cosmeticos::vista();
		const float w = 1780.0f * s, h = 960.0f * s, x0 = W * 0.5f - w * 0.5f, y0 = H * 0.5f - h * 0.5f;
		const float a = panelPerso(x0, y0, w, h, idioma::t("CONTROLES", "CONTROLS"));
		// pestannas
		float px = x0 + w * 0.5f - 420.0f * s;
		const float py = y0 + 108.0f * s;
		iconoBoton(glm::vec2(px - 40.0f * s, py + 22.0f * s), "L1", 14.0f * s, a);
		for (int p = 0; p < cosmeticos::NUM_PESTANAS; p++) {
			const bool sel = p == pv.pestana;
			usar(setAtlas, false);
			caja(px, py, 260.0f * s, 46.0f * s, 4.0f * s, PANEL, sel ? glm::vec4(0.32f, 0.23f, 0.05f, 0.75f * a) : glm::vec4(0.05f, 0.07f, 0.12f, 0.6f * a),
			     glm::vec4(glm::vec3(sel ? ORO : GRIS) * (sel ? 1.0f : 0.5f), a), 23.0f * s, glm::vec4(1.2f * s, 1.0f, 0.0f, 0.0f));
			texto(F_TITULO, cosmeticos::nombrePestana(p), px + 130.0f * s, py + 32.0f * s, 24.0f * s, glm::vec4(glm::vec3(sel ? ORO : BLANCO), a), NADA, glm::vec3(0.0f), 1, 1.5f);
			px += 290.0f * s;
		}
		iconoBoton(glm::vec2(px + 10.0f * s, py + 22.0f * s), "R1", 14.0f * s, a);
		// el mando a la izquierda
		const std::vector<cosmeticos::Control>& lista = cosmeticos::controles(pv.pestana);
		const int sel = std::max(0, std::min(pv.sel, (int)lista.size() - 1));
		dibujarMando(glm::vec2(x0 + 420.0f * s, y0 + 560.0f * s), 1.25f * s, a, lista.empty() ? -1 : lista[(size_t)sel].boton);
		// la lista a la derecha
		const float lx = x0 + 860.0f * s, lw = w - 900.0f * s, fila = 54.0f * s;
		float y = y0 + 190.0f * s;
		for (int k = 0; k < (int)lista.size(); k++) {
			const cosmeticos::Control& c = lista[(size_t)k];
			const bool es = k == sel;
			usar(setAtlas, false);
			if (es)
				caja(lx - 10.0f * s, y, lw, fila - 4.0f * s, 4.0f * s, PANEL, glm::vec4(0.32f, 0.23f, 0.05f, 0.55f * a), glm::vec4(glm::vec3(ORO), a), (fila - 4.0f * s) * 0.5f,
				     glm::vec4(1.0f * s, 1.0f, 0.0f, 0.0f));
			const std::string ic = c.icono;
			const float cy = y + (fila - 4.0f * s) * 0.5f;
			if (ic == "STICK_L") {
				usar(setAtlas, false);
				rect(glm::vec2(lx + 28.0f * s, cy), glm::vec2(30.0f * s), 0.0f, 5.0f * s, ORBE, glm::vec4(0.7f, 0.76f, 0.88f, a), glm::vec4(0.05f, 0.07f, 0.12f, a), 0.0f,
				     glm::vec4(0.25f, 1.0f, 0.0f, 0.0f));
				texto(F_NUMEROS, "L", lx + 28.0f * s, cy + 5.0f * s, 14.0f * s, glm::vec4(glm::vec3(BLANCO), a), NADA, glm::vec3(0.0f), 1);
			} else {
				iconoBoton(glm::vec2(lx + 28.0f * s, cy), ic, 15.0f * s, a);
			}
			texto(F_TEXTO, idioma::t(c.es, c.en), lx + 150.0f * s, cy + 9.0f * s, 24.0f * s, es ? glm::vec4(1.0f, 0.95f, 0.8f, a) : glm::vec4(glm::vec3(BLANCO), a * 0.92f), NADA,
			      glm::vec3(0.0f), 0, 0.0f);
			y += fila;
		}
		ayudaPerso(x0 + 40.0f * s, y0 + h - 40.0f * s, a,
		           { { "L1R1", idioma::t("Pantalla", "Screen") }, { "CRUZ", idioma::t("Botón", "Button") }, { "O", idioma::t("Volver", "Back") } });
	}

	// ---- estadisticas
	void dibujarPersoEstadisticas()
	{
		const cosmeticos::VistaPerso& pv = cosmeticos::vista();
		const estadisticas::Datos& d = estadisticas::datos();
		const float w = 1700.0f * s, h = 940.0f * s, x0 = W * 0.5f - w * 0.5f, y0 = H * 0.5f - h * 0.5f;
		const float a = panelPerso(x0, y0, w, h, idioma::t("ESTADÍSTICAS", "STATISTICS"));
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		// izquierda: los numeros grandes
		const int vt = d.victoriasTotal, dt = d.derrotasTotal, jug = vt + dt;
		const int pct = jug > 0 ? (int)lroundf(100.0f * (float)vt / (float)jug) : 0;
		struct Num { const char* et; std::string v; glm::vec4 col; };
		std::string racha = d.racha > 0 ? std::to_string(d.racha) + idioma::t(" V", " W") : d.racha < 0 ? std::to_string(-d.racha) + idioma::t(" D", " L") : "0";
		const Num nums[6] = {
			{ idioma::t("Victorias", "Wins"), std::to_string(vt), CIAN },
			{ idioma::t("Derrotas", "Losses"), std::to_string(dt), CARMESI },
			{ idioma::t("% de victorias", "Win rate"), std::to_string(pct) + "%", ORO },
			{ idioma::t("Racha actual", "Current streak"), racha, d.racha >= 0 ? CIAN : CARMESI },
			{ idioma::t("Mejor racha", "Best streak"), std::to_string(d.mejorRacha), ORO },
			{ idioma::t("Empates", "Draws"), std::to_string(d.empates), GRIS },
		};
		const float cx0 = x0 + 50.0f * s, cw = 250.0f * s, ch = 130.0f * s;
		for (int i = 0; i < 6; i++) {
			const float bx = cx0 + (float)(i % 3) * (cw + 20.0f * s), by = y0 + 120.0f * s + (float)(i / 3) * (ch + 20.0f * s);
			usar(setAtlas, false);
			caja(bx, by, cw, ch, 4.0f * s, PANEL, A(glm::vec4(0.03f, 0.05f, 0.1f, 0.7f)), A(glm::vec4(glm::vec3(nums[i].col) * 0.5f, 1.0f)), 14.0f * s, glm::vec4(1.0f * s, 0.8f, 0.0f, 0.0f));
			texto(F_NUMEROS, nums[i].v, bx + cw * 0.5f, by + 72.0f * s, 52.0f * s, A(nums[i].col), NADA, glm::vec3(0.0f), 1);
			texto(F_TITULO, nums[i].et, bx + cw * 0.5f, by + 110.0f * s, 22.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
		}
		float y = y0 + 440.0f * s;
		const float lw = 3.0f * cw + 40.0f * s;
		// rama classic-en-linea: los duelos en linea, aparte (no entran en los numeros de arriba)
		texto(F_TITULO, idioma::t("Duelos en línea", "Online duels"), cx0, y - 40.0f * s, 22.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 0, 1.0f);
		texto(F_TITULO, std::to_string(d.lineaVictorias) + idioma::t(" V  ·  ", " W  ·  ") + std::to_string(d.lineaDerrotas) + idioma::t(" D  ·  ", " L  ·  ") +
		                    std::to_string(d.lineaEmpates) + idioma::t(" E", " D"),
		      cx0 + lw, y - 40.0f * s, 22.0f * s, A(CIAN), NADA, glm::vec3(0.0f), 2, 1.0f);
		// mazo mas usado
		const auto* mz = estadisticas::masAlto(d.mazos);
		texto(F_TITULO, idioma::t("Mazo más usado", "Most used deck"), cx0, y, 24.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 0, 1.0f);
		texto(F_TITULO, mz ? mz->first : std::string("-"), cx0 + lw, y, 26.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 2, 1.0f);
		if (mz) texto(F_TITULO, std::to_string(mz->second) + idioma::t(" duelos", " duels"), cx0 + lw, y + 32.0f * s, 21.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 2, 1.0f);
		y += 90.0f * s;
		// carta mas jugada (con su cara)
		const auto* ca = estadisticas::masAlto(d.cartas);
		texto(F_TITULO, idioma::t("Carta más jugada", "Most played card"), cx0, y, 24.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 0, 1.0f);
		if (ca) {
			const int n = buscarCarta(ca->first);
			const std::string nom = n >= 0 ? cartas[(size_t)n].vNombre() : ca->first;
			{
				struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
				texto(F_TITULO, nom, cx0 + lw - 120.0f * s, y, 26.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 2, 1.0f);
			}
			texto(F_TITULO, std::to_string(ca->second) + idioma::t(" veces", " times"), cx0 + lw - 120.0f * s, y + 32.0f * s, 21.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 2, 1.0f);
			if (n >= 0 && miniLista(n)) {
				usar(cartas[(size_t)n].setMini, false);
				const float u = 0.5f / (float)cartas[(size_t)n].mini.width, vv = 0.5f / (float)cartas[(size_t)n].mini.height;
				rect(glm::vec2(cx0 + lw - 45.0f * s, y + 20.0f * s), glm::vec2(80.0f, 116.0f) * s, 0.0f, 0.0f, IMAGEN, glm::vec4(1.0f, 1.0f, 1.0f, a), glm::vec4(0.6f, 0.8f, 1.0f, 1.0f),
				     5.0f * s, glm::vec4(0.1f, 0.2f, 0.0f, 0.0f), glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
				usar(setAtlas, false);
			}
		} else {
			texto(F_TITULO, "-", cx0 + lw, y, 26.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 2, 1.0f);
		}
		y += 120.0f * s;
		// cartas firma
		texto(F_TITULO, idioma::t("Cartas firma desbloqueadas", "Signature cards unlocked"), cx0, y, 24.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 0, 1.0f);
		texto(F_NUMEROS, std::to_string(d.firmaLibres) + " / " + std::to_string(d.firmaTotal), cx0 + lw, y, 28.0f * s, A(ORO), NADA, glm::vec3(0.0f), 2);
		const float fr = d.firmaTotal > 0 ? (float)d.firmaLibres / (float)d.firmaTotal : 0.0f;
		usar(setAtlas, false);
		caja(cx0, y + 20.0f * s, lw, 14.0f * s, 2.0f * s, PANEL, A(glm::vec4(0.05f, 0.07f, 0.12f, 0.9f)), NADA, 7.0f * s, glm::vec4(0.0f));
		if (fr > 0.0f) caja(cx0, y + 20.0f * s, lw * fr, 14.0f * s, 2.0f * s, PANEL, A(glm::vec4(glm::vec3(ORO) * 0.8f, 1.0f)), NADA, 7.0f * s, glm::vec4(0.0f));
		// derecha: por rival
		const float rx = x0 + 900.0f * s, rw = w - 950.0f * s;
		float ry = y0 + 130.0f * s;
		texto(F_TITULO, idioma::t("POR RIVAL", "BY OPPONENT"), rx, ry, 26.0f * s, A(ORO), NADA, glm::vec3(0.0f), 0, 1.5f);
		const float cV = rx + rw - 230.0f * s, cD = rx + rw - 140.0f * s, cP = rx + rw - 10.0f * s;
		texto(F_TITULO, idioma::t("V", "W"), cV, ry, 22.0f * s, A(CIAN), NADA, glm::vec3(0.0f), 2, 1.0f);
		texto(F_TITULO, idioma::t("D", "L"), cD, ry, 22.0f * s, A(CARMESI), NADA, glm::vec3(0.0f), 2, 1.0f);
		texto(F_TITULO, "%", cP, ry, 22.0f * s, A(ORO), NADA, glm::vec3(0.0f), 2, 1.0f);
		ry += 22.0f * s;
		const int filas = 11, n = (int)d.rivales.size();
		const int desde = std::max(0, std::min(pv.desplazar, n - filas));
		if (n == 0)
			parrafo(F_TEXTO, idioma::t("Todavía no hay duelos registrados por rival. Se anotan al terminar cada duelo.", "No duels recorded by opponent yet. They are recorded after each duel."),
			        rx, ry + 50.0f * s, 24.0f * s, rw, 32.0f * s, 3, A(GRIS));
		for (int i = desde; i < std::min(n, desde + filas); i++) {
			const estadisticas::Rival& r = d.rivales[(size_t)i];
			const float fy = ry + (float)(i - desde) * 52.0f * s;
			usar(setAtlas, false);
			if ((i - desde) % 2 == 0) caja(rx - 10.0f * s, fy + 6.0f * s, rw + 20.0f * s, 48.0f * s, 2.0f * s, PANEL, A(glm::vec4(0.05f, 0.08f, 0.14f, 0.5f)), NADA, 8.0f * s, glm::vec4(0.0f));
			{
				struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
				std::string nom = nombreRival(r);
				float tt = 24.0f * s;
				while (medir(F_TITULO, nom, tt, 1.0f) > cV - rx - 80.0f * s && tt > 15.0f * s) tt -= 1.0f * s;
				texto(F_TITULO, nom, rx, fy + 38.0f * s, tt, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
			}
			const int jr = r.v + r.d;
			texto(F_NUMEROS, std::to_string(r.v), cV, fy + 38.0f * s, 24.0f * s, A(CIAN), NADA, glm::vec3(0.0f), 2);
			texto(F_NUMEROS, std::to_string(r.d), cD, fy + 38.0f * s, 24.0f * s, A(CARMESI), NADA, glm::vec3(0.0f), 2);
			texto(F_NUMEROS, jr > 0 ? std::to_string((int)lroundf(100.0f * (float)r.v / (float)jr)) + "%" : std::string("-"), cP, fy + 38.0f * s, 24.0f * s, A(ORO), NADA,
			      glm::vec3(0.0f), 2);
		}
		if (n > filas)
			texto(F_TITULO, std::to_string(desde + 1) + "-" + std::to_string(std::min(n, desde + filas)) + " / " + std::to_string(n), rx + rw, y0 + h - 70.0f * s, 20.0f * s, A(GRIS),
			      NADA, glm::vec3(0.0f), 2, 1.0f);
		ayudaPerso(x0 + 40.0f * s, y0 + h - 40.0f * s, a, { { "CRUZ", idioma::t("Desplazar", "Scroll") }, { "O", idioma::t("Volver", "Back") } });
	}

public:
