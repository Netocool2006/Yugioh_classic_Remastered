// Yugioh Classic Remaster (rama classic-en-linea): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// Lo que se ve del DUELO EN LINEA (control::VistaDuelo::enLinea, lo llena en_linea_juego.h; los textos ya vienen en el
// idioma actual):
//  - VD::EN_LINEA: el panel del menu y sus pantallas (aviso de privacidad, buscando, conectando, red local, codigo de
//    conexion con el teclado en pantalla, avisos de la red, duelo interrumpido), con el estilo del menu principal.
//  - En el duelo en linea: el nombre del rival, el indicador de conexion, los dos relojes del turno (el propio parpadea
//    con 30 s o menos), los agotamientos y el cartel "Esperando al rival... (reconectando N s)".
//  - "¡Rival encontrado!" arriba, en cualquier pantalla, mientras juega contra la IA.
//  - En el resultado: el ELO (si la sala lo confirmo) y el estado de la revancha.

private:
	// la ayuda de botones "boton|texto;..." centrada en y
	void ayudaEnLinea(const std::string& ayuda, float cx, float y, float a)
	{
		std::vector<std::pair<std::string, std::string>> pares;
		size_t i = 0;
		while (i < ayuda.size()) {
			size_t f = ayuda.find(';', i);
			if (f == std::string::npos) f = ayuda.size();
			const std::string par = ayuda.substr(i, f - i);
			const size_t b = par.find('|');
			if (b != std::string::npos) pares.push_back({ par.substr(0, b), par.substr(b + 1) });
			i = f + 1;
		}
		float ancho = 0.0f;
		for (const auto& p : pares) ancho += 28.0f * s + 12.0f * s + medir(F_TITULO, p.second, 21.0f * s) + 30.0f * s;
		float bx = cx - ancho * 0.5f;
		for (const auto& p : pares) {
			bx += iconoBoton(glm::vec2(bx + 14.0f * s, y), p.first, 14.0f * s, a) + 12.0f * s;
			bx += texto(F_TITULO, p.second, bx, y + 8.0f * s, 21.0f * s, glm::vec4(1.0f, 1.0f, 1.0f, a), NADA, glm::vec3(0.0f), 0, 1.0f) + 30.0f * s;
		}
	}

	void dibujarEnLinea()
	{
		if (!ctl) return;
		const control::VistaDuelo& vd = ctl->vista;
		const en_linea::Vista& e = vd.enLinea;
		if (vd.pantalla == control::VistaDuelo::EN_LINEA) dibujarPanelEnLinea(vd, e);
		else if (e.enDuelo && vd.pantalla == control::VistaDuelo::DUELO && !vd.sorteo) dibujarDueloEnLinea(e);
		if (e.rivalEncontrado) dibujarRivalEncontrado(e);
	}

	void dibujarPanelEnLinea(const control::VistaDuelo& vd, const en_linea::Vista& e)
	{
		const float a = suave(0.0f, 0.3f, e.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const bool teclado = e.pantalla == en_linea::TECLADO;
		const bool aviso = e.pantalla == en_linea::AVISO_IP || e.pantalla == en_linea::AVISO_RED || e.pantalla == en_linea::INTERRUMPIDO;
		const glm::vec4 acento = aviso ? (e.pantalla == en_linea::AVISO_IP ? ORO : CARMESI) : glm::mix(CIAN, ORO, 0.55f);
		const int n = (int)e.opciones.size();
		const float w = 1000.0f * s, fila = 60.0f * s, cab = 120.0f * s;
		// alto: lineas (los parrafos largos ocupan mas) + opciones o teclado + pie
		float hLineas = 0.0f;
		for (const std::string& l : e.lineas) hLineas += (medir(F_TEXTO, l, 24.0f * s) > w - 120.0f * s ? 3.0f : 1.0f) * 34.0f * s;
		const float hTeclado = teclado ? (80.0f + 76.0f * (float)((int)strlen(en_linea::TECLAS) / en_linea::TECLAS_FILA + 1)) * s : 0.0f;
		const float h = cab + hLineas + (e.aviso.empty() ? 0.0f : 60.0f * s) + hTeclado + fila * (float)n + 40.0f * s + (e.animando ? 60.0f * s : 0.0f) + 90.0f * s;
		const float cx = W * 0.5f, x0 = cx - w * 0.5f, y0 = std::max(30.0f * s, H * 0.5f - h * 0.5f) + (1.0f - a) * 20.0f * s;
		// el fondo del menu principal (la puerta egipcia, portada_ui.h) con un velo: la mesa no se ve detras
		const bool arte = fondoMenu(t, 1.0f, false);
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, arte ? glm::vec4(0.01f, 0.0f, 0.03f, 0.55f) : glm::vec4(0.01f, 0.015f, 0.03f, 0.97f), NADA, 0.0f, glm::vec4(0.0f));
		rect(glm::vec2(cx, H * 0.5f), glm::vec2(W * 0.9f, H * 0.9f), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(acento) * 0.10f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		caja(x0, y0, w, h, 30.0f * s, PANEL, A(VIDRIO), A(acento), 22.0f * s, glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s));
		const float pulso = 1.0f + 0.012f * sinf(t * 2.0f);
		texto(F_NUMEROS, e.titulo, cx, y0 + 74.0f * s, 38.0f * s * pulso, A(BLANCO), A(aviso ? acento : CIAN), glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
		float y = y0 + cab;
		for (const std::string& l : e.lineas) {
			const bool codigo = e.pantalla == en_linea::CODIGO && l.find('-') != std::string::npos && l.find(' ') == std::string::npos;
			if (codigo) {   // el codigo propio, grande y en dorado
				texto(F_NUMEROS, l, cx, y + 10.0f * s, 30.0f * s, A(ORO), A(glm::vec4(0.4f, 0.25f, 0.05f, 1.0f)), glm::vec3(0.4f, 0.0f, 0.5f), 1, 1.5f);
				y += 50.0f * s;
			} else if (medir(F_TEXTO, l, 24.0f * s) > w - 120.0f * s) {
				const int k = parrafo(F_TEXTO, l, x0 + 60.0f * s, y, 23.0f * s, w - 120.0f * s, 30.0f * s, 4, A(glm::vec4(0.86f, 0.9f, 0.97f, 1.0f)));
				y += (float)k * 30.0f * s + 14.0f * s;
			} else {
				texto(F_TEXTO, l, cx, y, 24.0f * s, A(glm::vec4(0.86f, 0.9f, 0.97f, 1.0f)), NADA, glm::vec3(0.0f), 1, 0.0f);
				y += 34.0f * s;
			}
		}
		if (e.animando) {   // el indicador: tres puntos que giran
			for (int k = 0; k < 8; k++) {
				const float ang = t * 4.0f + (float)k * 0.785f;
				const float lum = 0.25f + 0.75f * (float)k / 7.0f;
				rect(glm::vec2(cx + 20.0f * s * cosf(ang), y + 22.0f * s + 20.0f * s * sinf(ang)), glm::vec2(9.0f * s), 0.0f, 0.0f, ORBE, A(glm::vec4(glm::vec3(CIAN), lum)),
				     A(glm::vec4(glm::vec3(CIAN), lum)), 0.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
			}
			y += 60.0f * s;
		}
		if (!e.aviso.empty()) {
			caja(x0 + 40.0f * s, y, w - 80.0f * s, 48.0f * s, 8.0f * s, PANEL, A(glm::vec4(0.25f, 0.05f, 0.06f, 0.75f)), A(CARMESI), 12.0f * s, glm::vec4(1.0f * s, 1.0f, 0.0f, 0.0f));
			texto(F_TITULO, e.aviso, cx, y + 32.0f * s, 22.0f * s, A(glm::vec4(1.0f, 0.86f, 0.82f, 1.0f)), NADA, glm::vec3(0.0f), 1, 1.0f);
			y += 60.0f * s;
		}
		if (teclado) {
			// lo escrito y las teclas
			caja(x0 + 60.0f * s, y, w - 120.0f * s, 60.0f * s, 8.0f * s, PANEL, A(glm::vec4(0.02f, 0.03f, 0.06f, 0.9f)), A(CIAN), 12.0f * s, glm::vec4(1.0f * s, 1.0f, 0.0f, 0.0f));
			const bool cursor = fmodf(t, 1.0f) < 0.6f;
			texto(F_NUMEROS, e.texto + (cursor ? "_" : " "), cx, y + 42.0f * s, 28.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 1, 1.0f);
			y += 80.0f * s;
			const int nt = (int)strlen(en_linea::TECLAS), f = en_linea::TECLAS_FILA;
			const float tw = 92.0f * s, th = 64.0f * s, sep = 10.0f * s, tx0 = cx - ((float)f * tw + (float)(f - 1) * sep) * 0.5f;
			for (int k = 0; k < nt; k++) {
				const float tx = tx0 + (float)(k % f) * (tw + sep), ty = y + (float)(k / f) * (th + sep + 2.0f * s);
				const bool sel = k == e.tecla;
				caja(tx, ty, tw, th, 6.0f * s, PANEL, A(sel ? glm::vec4(0.32f, 0.23f, 0.05f, 0.8f) : glm::vec4(glm::vec3(VIDRIO), 0.7f)), A(sel ? ORO : glm::vec4(glm::vec3(CIAN) * 0.6f, 1.0f)),
				     10.0f * s, glm::vec4((sel ? 1.5f : 1.0f) * s, 1.0f, 0.0f, 0.0f));
				texto(F_NUMEROS, std::string(1, en_linea::TECLAS[k]), tx + tw * 0.5f, ty + th * 0.5f + 11.0f * s, 30.0f * s, A(sel ? ORO : BLANCO), NADA, glm::vec3(0.0f), 1, 0.0f);
			}
			y += hTeclado - 80.0f * s;
		}
		for (int k = 0; k < n; k++) {
			const bool sel = k == e.opcion;
			const bool apagada = (size_t)k < e.apagadas.size() && e.apagadas[(size_t)k];
			const float fy = y + fila * (float)k;
			if (sel)
				caja(x0 + 60.0f * s, fy + 4.0f * s, w - 120.0f * s, fila - 8.0f * s, 6.0f * s, PANEL, A(glm::vec4(0.32f, 0.23f, 0.05f, 0.6f)), A(ORO), (fila - 8.0f * s) * 0.5f,
				     glm::vec4(1.2f * s, 1.1f, 0.0f, 0.0f));
			const glm::vec4 col = apagada ? GRIS : sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : glm::vec4(0.86f, 0.9f, 0.97f, 1.0f);
			texto(F_TITULO, e.opciones[(size_t)k], cx, fy + fila * 0.5f + 10.0f * s, 29.0f * s, A(col), NADA, glm::vec3(0.0f), 1, 1.0f);
			if (sel && !apagada) iconoBoton(glm::vec2(x0 + w - 92.0f * s, fy + fila * 0.5f), "X", 14.0f * s, a);
		}
		ayudaEnLinea(e.ayuda, cx, y0 + h - 44.0f * s, a);
		(void)vd;
	}

	// los relojes del turno, el rival y la conexion (arriba a la derecha, sobre el tablero)
	void dibujarDueloEnLinea(const en_linea::Vista& e)
	{
		usar(setAtlas, false);
		const float pw = 380.0f * s, ph = 150.0f * s, px = W - pw - 36.0f * s, py = 96.0f * s;
		caja(px, py, pw, ph, 14.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.82f), glm::vec4(glm::vec3(CIAN) * 0.7f, 1.0f), 16.0f * s, glm::vec4(1.2f * s, 1.0f, 0.1f, 12.0f * s));
		// conexion: un punto verde (bien) o rojo que late (caida)
		const bool caida = e.conexion != 0;
		const float lat = caida ? 0.5f + 0.5f * sinf(t * 8.0f) : 1.0f;
		const glm::vec4 cc = caida ? glm::vec4(1.0f, 0.3f, 0.3f, lat) : glm::vec4(0.35f, 0.95f, 0.55f, 1.0f);
		rect(glm::vec2(px + 26.0f * s, py + 30.0f * s), glm::vec2(14.0f * s), 0.0f, 0.0f, ORBE, cc, cc, 0.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
		texto(F_TITULO, std::string(idioma::tr("Rival")) + ": " + e.rival, px + 44.0f * s, py + 38.0f * s, 22.0f * s, BLANCO, NADA, glm::vec3(0.0f), 0, 1.0f);
		auto reloj = [&](float seg) {
			const int x = (int)ceilf(std::max(0.0f, seg));
			char b[16];
			snprintf(b, sizeof b, "%d:%02d", x / 60, x % 60);
			return std::string(b);
		};
		auto marcas = [&](int n) {
			return std::to_string(n) + "/" + std::to_string(en_linea::AGOTAMIENTOS_DERROTA);
		};
		// el reloj propio: dorado; con 30 s o menos, rojo y parpadea
		const bool alerta = e.relojMioCorre && e.relojMio <= en_linea::RELOJ_ALERTA_S;
		const float parp = alerta ? 0.45f + 0.55f * (0.5f + 0.5f * sinf(t * 10.0f)) : 1.0f;
		const glm::vec4 cm = alerta ? glm::vec4(1.0f, 0.35f, 0.35f, parp) : e.relojMioCorre ? ORO : GRIS;
		texto(F_TITULO, idioma::tr("Tu tiempo"), px + 24.0f * s, py + 80.0f * s, 19.0f * s, GRIS, NADA, glm::vec3(0.0f), 0, 1.0f);
		texto(F_NUMEROS, reloj(e.relojMio), px + 24.0f * s, py + 122.0f * s, 34.0f * s, cm, NADA, glm::vec3(0.0f), 0, 1.0f);
		texto(F_TITULO, marcas(e.agotadosMio), px + 140.0f * s, py + 120.0f * s, 18.0f * s, glm::vec4(1.0f, 0.5f, 0.45f, 1.0f), NADA, glm::vec3(0.0f), 0, 0.0f);
		texto(F_TITULO, idioma::tr("Rival"), px + pw - 24.0f * s, py + 80.0f * s, 19.0f * s, GRIS, NADA, glm::vec3(0.0f), 2, 1.0f);
		texto(F_NUMEROS, reloj(e.relojRival), px + pw - 24.0f * s, py + 122.0f * s, 34.0f * s, e.relojRivalCorre ? CIAN : GRIS, NADA, glm::vec3(0.0f), 2, 1.0f);
		texto(F_TITULO, marcas(e.agotadosRival), px + pw - 140.0f * s, py + 120.0f * s, 18.0f * s, glm::vec4(1.0f, 0.5f, 0.45f, 1.0f), NADA, glm::vec3(0.0f), 2, 0.0f);
		if (alerta) {   // un borde rojo que late en toda la pantalla
			caja(0.0f, 0.0f, W, H, 0.0f, PANEL, NADA, glm::vec4(1.0f, 0.2f, 0.2f, 0.35f * parp), 0.0f, glm::vec4(10.0f * s, 1.0f, 0.0f, 0.0f));
		}
		if (caida) {   // el cartel de la reconexion, al centro
			const float cx = W * 0.5f, cw = 760.0f * s, ch = 120.0f * s, cy = H * 0.42f;
			caja(cx - cw * 0.5f, cy, cw, ch, 20.0f * s, PANEL, glm::vec4(0.05f, 0.02f, 0.06f, 0.9f), CARMESI, 22.0f * s, glm::vec4(1.5f * s, 1.1f, 0.1f, 16.0f * s));
			texto(F_NUMEROS, idioma::tr("Esperando al rival…"), cx, cy + 52.0f * s, 34.0f * s, BLANCO, CARMESI, glm::vec3(0.4f, 0.0f, 0.6f), 1, 2.0f);
			texto(F_TITULO, std::string(idioma::tr("Reconectando")) + " " + std::to_string(e.reconectando) + " / " + std::to_string(en_linea::RECONEXION_S) + " s", cx,
			      cy + 94.0f * s, 22.0f * s, GRIS, NADA, glm::vec3(0.0f), 1, 1.0f);
		}
	}

	// "¡Rival encontrado!" arriba al centro (mientras juega contra la IA): X para ir, circulo para rechazar
	void dibujarRivalEncontrado(const en_linea::Vista& e)
	{
		usar(setAtlas, false);
		const float cx = W * 0.5f, cw = 900.0f * s, ch = 150.0f * s, cy = 40.0f * s;
		const float lat = 0.7f + 0.3f * sinf(t * 5.0f);
		caja(cx - cw * 0.5f, cy, cw, ch, 22.0f * s, PANEL, glm::vec4(0.04f, 0.03f, 0.01f, 0.94f), glm::vec4(glm::vec3(ORO), lat), 22.0f * s, glm::vec4(1.8f * s, 1.1f, 0.15f, 18.0f * s));
		texto(F_NUMEROS, idioma::tr("¡Rival encontrado!"), cx, cy + 50.0f * s, 36.0f * s, BLANCO, ORO, glm::vec3(0.5f, 0.0f, 0.7f), 1, 2.0f);
		texto(F_TITULO, e.rivalEncontradoTexto, cx, cy + 88.0f * s, 23.0f * s, glm::vec4(1.0f, 0.92f, 0.7f, 1.0f), NADA, glm::vec3(0.0f), 1, 1.0f);
		// la barra del tiempo que queda para responder
		const float u = std::max(0.0f, std::min(1.0f, e.rivalEncontradoQueda / en_linea::RESPONDER_RIVAL_S));
		caja(cx - cw * 0.4f, cy + 102.0f * s, cw * 0.8f * u, 6.0f * s, 0.0f, PANEL, ORO, NADA, 3.0f * s, glm::vec4(0.0f));
		const std::string ayuda = std::string("X|") + idioma::tr("Ir al duelo en línea") + ";O|" + idioma::tr("Rechazar") + " (" +
		                          std::to_string((int)ceilf(e.rivalEncontradoQueda)) + " s)";
		ayudaEnLinea(ayuda, cx, cy + 128.0f * s, 1.0f);
	}

	// en la pantalla de resultado del duelo en linea (resultado_ui.h): ELO y revancha
	void dibujarResultadoEnLinea(const control::VistaDuelo& vd, float cx, float y, float a)
	{
		const en_linea::Vista& e = vd.enLinea;
		if (!e.eloTexto.empty())
			texto(F_NUMEROS, e.eloTexto, cx, y, 26.0f * s, glm::vec4(1.0f, 0.92f, 0.7f, a), NADA, glm::vec3(0.3f, 0.0f, 0.4f), 1, 1.0f);
		if (!e.revanchaTexto.empty())
			texto(F_TITULO, e.revanchaTexto, cx, y + 34.0f * s, 21.0f * s, glm::vec4(glm::vec3(CIAN), a), NADA, glm::vec3(0.0f), 1, 1.0f);
	}

public:
