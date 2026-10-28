// Yugioh Classic Remaster (rama classic-duelo-ui): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// Lo que se ve de la consulta del duelo (duelo_visores.h; lo lleva el juego en duelo_consulta.h):
//  - dibujarConsulta(): el panel de pestannas (REGISTRO, CEMENTERIO, DESTERRADAS, EXTRA) con la lista que se recorre a
//    la izquierda y la carta elegida a la derecha; abajo, sus botones.
//  - dibujarCadenaPila(): la pila de la cadena arriba a la izquierda de la mesa: C1 a la izquierda, el ultimo encima; cada uno
//    con su carta, su numero y el color de su duenno. Se resuelve de arriba hacia abajo: el eslabon que resuelve se
//    ilumina y sale hacia arriba.
//  - dibujarJugablesMesa() / brilloJugableMano(): el brillo azul de Master Duel en lo que se puede jugar o activar.
// Textos: idioma::t(es, en) en el lugar (sin tocar la tabla de idioma.h).

public:
	void conectarConsulta(const visores::Consulta* c) { consulta_ = c; }
	bool consultaAbierta() const { return consulta_ && consulta_->vis.abierto && ctl && ctl->vista.pantalla == control::VistaDuelo::DUELO; }

private:
	const visores::Consulta* consulta_{ nullptr };
	const glm::vec4 AZUL_JUGABLE{ 0.32f, 0.62f, 1.0f, 1.0f };

	bool mostrarJugables() const
	{
		return consulta_ && ctl && ctl->duelo && ctl->vista.pantalla == control::VistaDuelo::DUELO && !ctl->vista.sorteo && !consultaAbierta() &&
		       (ctl->estado == control::NAVEGAR || ctl->estado == control::MENU);
	}

	// el halo azul que late detras de una carta de la mano que se puede jugar (k: su indice)
	void brilloJugableMano(int k, glm::vec2 c, glm::vec2 tam, float ang)
	{
		if (!mostrarJugables() || !consulta_->jug.enMano(k)) return;
		const float lat = 0.65f + 0.35f * sinf(t * 4.2f + (float)k * 0.7f);
		usar(setAtlas, false);
		rect(c, tam * 1.22f, ang, 0.0f, PARTICULA, glm::vec4(glm::vec3(AZUL_JUGABLE) * 0.55f * lat, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		rect(c, tam + glm::vec2(12.0f * s), ang, 10.0f * s, PANEL, NADA, glm::vec4(glm::vec3(AZUL_JUGABLE), lat), 12.0f * s,
		     glm::vec4(3.0f * s, 1.4f * lat, 0.0f, 14.0f * s));
	}

	// en la mesa: el marco azul de las zonas propias con algo para hacer (activar, voltear, atacar)
	void dibujarJugablesMesa()
	{
		if (!mostrarJugables()) return;
		usar(setAtlas, true);
		for (int z : consulta_->jug.zonas) {
			if (z < 0 || z >= (int)NUM_ZONAS) continue;
			const float lat = 0.7f + 0.3f * sinf(t * 4.2f + (float)z * 0.9f);
			rectSuelo(centroZona(z) + glm::vec3(0.0f, 0.055f, 0.0f), glm::vec2(ZONA_ANCHO, ZONA_ALTO) * 1.03f, 0.25f, ZONA, AZUL_JUGABLE,
			          glm::vec4(glm::vec3(AZUL_JUGABLE) * 0.35f, 1.0f), 10.0f, glm::vec4(7.0f, 1.0f, lat * 1.2f, 0.0f));
		}
	}

	// ------------------------------------------------------------------------------------------ la cadena
	void dibujarCadenaPila()
	{
		if (!consulta_ || !ctl || ctl->vista.pantalla != control::VistaDuelo::DUELO || consultaAbierta() || ctl->estado == control::DETALLE) return;
		const cadena::Pila& p = consulta_->pila;
		if (p.v.empty()) return;
		const int n = (int)p.v.size();
		// rama classic-visual-final: en fila, en el cielo libre entre la columna de fases y la mano del rival (arriba a la
		// izquierda de la mesa): entera en 16:9, sin tapar las etiquetas de la mesa ni el retrato del rival (arriba a la
		// derecha). C1 a la izquierda; cada eslabon nuevo encima, a su derecha (su ficha C<n> queda a la vista)
		const glm::vec2 tam = glm::vec2(100.0f, 146.0f) * s;
		const float medio = (CAMPO_X + 228.0f) * s, ancho = 300.0f * s;
		const float paso = n > 1 ? glm::min(115.0f * s, ancho / (float)(n - 1)) : 0.0f;
		const float x0 = medio - paso * (float)(n - 1) * 0.5f, yBase = 300.0f * s;
		usar(setAtlas, false);
		caja(medio - 70.0f * s, yBase - tam.y * 0.5f - 52.0f * s, 140.0f * s, 34.0f * s, 6.0f * s, PANEL, glm::vec4(0.02f, 0.03f, 0.06f, 0.8f), glm::vec4(glm::vec3(ORO) * 0.8f, 1.0f),
		     17.0f * s, glm::vec4(1.2f * s, 0.8f, 0.0f, 8.0f * s));
		texto(F_NUMEROS, idioma::t("CADENA", "CHAIN"), medio, yBase - tam.y * 0.5f - 27.0f * s, 22.0f * s, BLANCO, ORO, glm::vec3(0.4f, 0.0f, 0.6f), 1, 3.0f);
		for (int i = 0; i < n; i++) {
			const cadena::Eslabon& e = p.v[(size_t)i];
			const float entra = suave(0.0f, cadena::T_ENTRA, e.t);
			const float sale = e.tFin >= 0.0f ? suave(0.0f, cadena::T_SALE, e.tFin) : 0.0f;
			const float a = entra * (1.0f - sale);
			if (a <= 0.01f) continue;
			const glm::vec4 lado = e.lado ? CARMESI : CIAN;
			glm::vec2 c(x0 + paso * (float)i, yBase - 70.0f * s * sale - 40.0f * s * (1.0f - entra));
			const float esc = 0.9f + 0.1f * entra + 0.15f * sinf(glm::pi<float>() * glm::min(1.0f, sale * 1.6f));
			const float ang = glm::radians(e.lado ? 4.0f : -4.0f) * (1.0f - entra * 0.5f);
			auto A = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * a); };
			// halo (mas fuerte en el ultimo eslabon y al resolverse)
			const bool arriba = i == n - 1 && e.tFin < 0.0f;
			const float brillo = (arriba ? 0.55f + 0.25f * sinf(t * 5.0f) : 0.25f) + 1.2f * sale;
			rect(c, tam * esc * 1.45f, 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(lado) * brillo * 0.6f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
			rect(c, tam * esc + glm::vec2(10.0f * s), ang, 12.0f * s, PANEL, A(glm::vec4(0.0f, 0.0f, 0.0f, 0.6f)), A(lado), 10.0f * s,
			     glm::vec4(2.0f * s, 1.0f + brillo, 0.0f, 14.0f * s));
			const int ci = buscarCarta(e.carta);
			if (ci >= 0 && miniLista(ci)) {
				usar(cartas[ci].setMini, false);
				const float u = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
				rect(c, tam * esc, ang, 0.0f, IMAGEN, A(glm::vec4(glm::vec3(1.0f + 0.5f * sale), 1.0f)), A(lado), 7.0f * s, glm::vec4(0.3f, 0.6f, (float)i * 0.3f, 0.0f),
				     glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
			} else {
				Dorso dz = usarDorso(false, false, e.lado != 0, 1.0f, (float)i);
				dz.col.a *= a;
				rect(c, tam * esc, ang, 0.0f, DORSO, dz.col, dz.col2, 7.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
			}
			usar(setAtlas, false);
			// el numero del eslabon (C1, C2...) en una ficha a la izquierda de la carta
			const glm::vec2 f = c + glm::vec2(-tam.x * esc * 0.5f - 6.0f * s, -tam.y * esc * 0.32f);
			rect(f, glm::vec2(50.0f * s), 0.0f, 6.0f * s, ORBE, A(glm::vec4(glm::vec3(lado) * 0.9f, 1.0f)), A(glm::vec4(0.04f, 0.06f, 0.12f, 1.0f)), 0.0f,
			     glm::vec4(0.25f, 1.0f, 0.0f, 0.0f));
			texto(F_NUMEROS, "C" + std::to_string(e.n), f.x, f.y + 8.0f * s, 22.0f * s, A(BLANCO), A(lado), glm::vec3(0.3f, 0.0f, 0.6f), 1);
			if (e.negado) {   // (negado: una banda carmesi cruzada)
				rect(c, glm::vec2(tam.x * esc * 1.15f, 8.0f * s), glm::radians(-35.0f), 4.0f * s, PANEL, A(CARMESI), NADA, 4.0f * s, glm::vec4(0.0f));
			}
		}
	}

	// ------------------------------------------------------------------------------------------ el panel de consulta
	void dibujarConsulta()
	{
		if (!consultaAbierta() || ctl->estado == control::DETALLE) return;
		const visores::Consulta& q = *consulta_;
		const visores::Estado& e = q.vis;
		const bool en = idioma::ingles();
		const float a = suave(0.0f, 0.18f, e.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float x0 = CAMPO_X * s + 30.0f * s, x1 = W - 30.0f * s, y0 = 70.0f * s + (1.0f - a) * 18.0f * s, y1 = H - 60.0f * s;
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.004f, 0.008f, 0.02f, 0.55f)), NADA, 0.0f, glm::vec4(0.0f));
		caja(x0, y0, x1 - x0, y1 - y0, 30.0f * s, PANEL, A(glm::vec4(glm::vec3(VIDRIO), 0.93f)), A(glm::mix(CIAN, ORO, 0.4f)), 18.0f * s,
		     glm::vec4(1.4f * s, 0.9f, 0.08f, 22.0f * s));
		rz.tapar();   // rama classic-pc: el panel se queda con el raton (pestannas con clic; la rueda recorre la lista)
		// pestannas
		const float tabY = y0 + 22.0f * s, tabH = 52.0f * s, tabW = glm::min(300.0f * s, (x1 - x0 - 60.0f * s) / (float)visores::NUM_PESTANAS);
		for (int k = 0; k < visores::NUM_PESTANAS; k++) {
			const bool sel = k == e.pestana;
			const float tx = x0 + 30.0f * s + tabW * (float)k;
			zLista(tx + 4.0f * s, tabY, tabW - 8.0f * s, tabH, k, e.pestana, true, 0);
			if (sel) {
				caja(tx + 4.0f * s, tabY, tabW - 8.0f * s, tabH, 8.0f * s, PANEL, A(glm::vec4(0.32f, 0.23f, 0.05f, 0.65f)), A(ORO), tabH * 0.5f,
				     glm::vec4(1.4f * s, 1.2f, 0.0f, 0.0f));
			}
			std::string nombre = visores::nombrePestana(k, en);
			int cuenta = -1;
			if (k == visores::P_CEMENTERIO) cuenta = (int)(e.lado ? ctl->cementerioR : ctl->cementerioJ).size();
			if (k == visores::P_DESTERRADAS) cuenta = (int)(e.lado ? ctl->desterradosR : ctl->desterradosJ).size();
			if (k == visores::P_EXTRA) cuenta = (int)(e.lado ? ctl->extraR : ctl->extraJ).size();
			if (cuenta >= 0) nombre += " " + std::to_string(cuenta);
			float tt = 21.0f * s;
			while (medir(F_TITULO, nombre, tt, 2.0f) > tabW - 24.0f * s && tt > 13.0f * s) tt -= 1.0f * s;
			texto(F_TITULO, nombre, tx + tabW * 0.5f, tabY + tabH * 0.5f + 8.0f * s, tt, A(sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : GRIS), sel ? A(ORO) : NADA,
			      glm::vec3(sel ? 0.3f : 0.0f, 0.0f, 0.0f), 1, 2.0f);
			if (k == visores::P_REGISTRO && q.reg.nuevas > 0 && !sel) {   // (lineas nuevas sin mirar)
				rect(glm::vec2(tx + tabW - 22.0f * s, tabY + 12.0f * s), glm::vec2(14.0f * s), 0.0f, 4.0f * s, ORBE, A(CARMESI), A(CARMESI), 0.0f, glm::vec4(0.25f, 1.0f, 0, 0));
			}
		}
		rect(glm::vec2((x0 + x1) * 0.5f, tabY + tabH + 14.0f * s), glm::vec2(x1 - x0 - 60.0f * s, 2.0f * s), 0.0f, 5.0f * s, LINEA, A(glm::vec4(glm::vec3(ORO), 0.8f)), NADA,
		     0.0f, glm::vec4(1.5f, 0.2f, 0.0f, 0.0f));
		const float cuerpoY = tabY + tabH + 34.0f * s, cuerpoH = y1 - 74.0f * s - cuerpoY;
		const float listaX = x0 + 30.0f * s, listaW = (x1 - x0) * (e.pestana == visores::P_REGISTRO ? 0.62f : 0.50f);
		// de quien es (las pestannas de cartas): TU / RIVAL, con el cuadrado
		if (e.pestana != visores::P_REGISTRO) {
			const float ly = cuerpoY + 6.0f * s;
			float lx = listaX;
			for (int ld = 0; ld < 2; ld++) {
				const bool sel = ld == e.lado;
				const glm::vec4 col = ld ? CARMESI : CIAN;
				const std::string nom = ld ? idioma::t("RIVAL", "OPPONENT") : idioma::t("TÚ", "YOU");
				const float w = medir(F_TITULO, nom, 19.0f * s, 2.0f) + 40.0f * s;
				if (!sel) zBoton(lx, ly, w, 38.0f * s, control::B_CUADRADO);   // rama classic-pc
				caja(lx, ly, w, 38.0f * s, 6.0f * s, PANEL, A(glm::vec4(glm::vec3(col) * (sel ? 0.30f : 0.06f), sel ? 0.8f : 0.4f)), A(glm::vec4(glm::vec3(col), sel ? 1.0f : 0.35f)),
				     19.0f * s, glm::vec4(1.2f * s, sel ? 1.2f : 0.5f, 0.0f, 0.0f));
				texto(F_TITULO, nom, lx + w * 0.5f, ly + 26.0f * s, 19.0f * s, A(sel ? BLANCO : GRIS), NADA, glm::vec3(0.0f), 1, 2.0f);
				lx += w + 12.0f * s;
			}
			iconoBoton(glm::vec2(lx + 18.0f * s, ly + 19.0f * s), "C", 13.0f * s, a);
		}
		const float listaY = cuerpoY + (e.pestana == visores::P_REGISTRO ? 0.0f : 56.0f * s), listaH = cuerpoY + cuerpoH - listaY;
		if (e.pestana == visores::P_REGISTRO) dibujarListaRegistro(q, listaX, listaY, listaW, listaH, a);
		else dibujarListaCartas(q, listaX, listaY, listaW, listaH, a);
		// la carta elegida, a la derecha
		const float px = listaX + listaW + 30.0f * s, pw = x1 - 30.0f * s - px;
		dibujarCartaConsulta(q.cartaElegida(*ctl), e.pestana == visores::P_EXTRA && e.lado == 1, px, cuerpoY, pw, cuerpoH, a);
		// botones
		std::vector<std::pair<std::string, std::string>> ayuda = { { "CRUZ", idioma::t("Pestaña / lista", "Tab / list") } };
		if (e.pestana != visores::P_REGISTRO) ayuda.push_back({ "C", idioma::t("Tú / rival", "You / opponent") });
		if (!q.cartaElegida(*ctl).empty()) ayuda.push_back({ "X", idioma::t("Detalle", "Details") });
		ayuda.push_back({ "O", idioma::t("Cerrar", "Close") });
		float bx = x1 - 30.0f * s;
		const float by = y1 - 36.0f * s;
		for (int k = (int)ayuda.size() - 1; k >= 0; k--) {
			const float tw = medir(F_TITULO, ayuda[(size_t)k].second, 19.0f * s, 1.0f);
			bx -= tw;
			texto(F_TITULO, ayuda[(size_t)k].second, bx, by + 7.0f * s, 19.0f * s, A(glm::vec4(0.88f, 0.92f, 0.98f, 1.0f)), NADA, glm::vec3(0.0f), 0, 1.0f);
			bx -= 8.0f * s + 28.0f * s;
			iconoBoton(glm::vec2(bx + 14.0f * s, by), ayuda[(size_t)k].first, 14.0f * s, a);
			bx -= 26.0f * s;
		}
	}

	// el registro: la linea mas nueva arriba; separador en cada turno
	void dibujarListaRegistro(const visores::Consulta& q, float x, float y, float w, float h, float a)
	{
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const auto& ls = q.reg.lineas;
		const int n = (int)ls.size();
		if (n == 0) {
			texto(F_TEXTO, idioma::t("Todavía no pasó nada en el duelo.", "Nothing has happened in the duel yet."), x + 10.0f * s, y + 40.0f * s, 24.0f * s, A(GRIS));
			return;
		}
		const float fila = 46.0f * s;
		const int vis = glm::max(1, (int)(h / fila));
		int primero = glm::clamp(q.vis.sel - vis / 2, 0, glm::max(0, n - vis));
		auto nombre = [this](const std::string& id) { return nombreCarta(id); };
		for (int r = 0; r < vis && primero + r < n; r++) {
			const int idx = primero + r;
			const registro::Linea& l = ls[(size_t)(n - 1 - idx)];
			const float fy = y + fila * (float)r;
			const bool sel = idx == q.vis.sel;
			const glm::vec4 lado = l.lado ? CARMESI : CIAN;
			if (l.tipo == ia::S_TURNO) {
				rect(glm::vec2(x + w * 0.5f, fy + fila * 0.5f), glm::vec2(w - 10.0f * s, fila - 10.0f * s), 0.0f, 4.0f * s, PANEL, A(glm::vec4(glm::vec3(lado) * 0.16f, 0.8f)),
				     A(glm::vec4(glm::vec3(lado), sel ? 1.0f : 0.5f)), 8.0f * s, glm::vec4(1.2f * s, sel ? 1.3f : 0.7f, 0.0f, 0.0f));
				const std::string t0 = registro::Registro::texto(l, nombre) + "  ·  " + (l.lado ? idioma::t("RIVAL", "OPPONENT") : idioma::t("TÚ", "YOU"));
				texto(F_TITULO, t0, x + w * 0.5f, fy + fila * 0.5f + 8.0f * s, 21.0f * s, A(BLANCO), A(lado), glm::vec3(0.3f, 0.0f, 0.0f), 1, 3.0f);
				continue;
			}
			if (sel) {
				caja(x, fy + 3.0f * s, w, fila - 6.0f * s, 6.0f * s, PANEL, A(glm::vec4(0.32f, 0.23f, 0.05f, 0.55f)), A(ORO), (fila - 6.0f * s) * 0.5f,
				     glm::vec4(1.2f * s, 1.1f, 0.0f, 0.0f));
			}
			// turno, quien (barra de su color) y la clase de suceso (punto de color)
			texto(F_NUMEROS, "T" + std::to_string(l.turno), x + 34.0f * s, fy + fila * 0.5f + 7.0f * s, 17.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1);
			rect(glm::vec2(x + 66.0f * s, fy + fila * 0.5f), glm::vec2(5.0f * s, fila - 16.0f * s), 0.0f, 3.0f * s, PANEL, A(lado), NADA, 2.0f * s, glm::vec4(0.0f));
			static const glm::vec4 COL[] = { { 1, 1, 1, 1 }, { 0.75f, 0.8f, 0.9f, 1 }, { 0.55f, 0.95f, 0.75f, 1 }, { 1.0f, 0.78f, 0.32f, 1 },
				                             { 1.0f, 0.45f, 0.35f, 1 }, { 1.0f, 0.3f, 0.45f, 1 }, { 0.72f, 0.6f, 1.0f, 1 }, { 0.7f, 0.75f, 0.85f, 1 } };
			const registro::Clase cl = l.clase();
			rect(glm::vec2(x + 86.0f * s, fy + fila * 0.5f), glm::vec2(11.0f * s), 0.0f, 4.0f * s, ORBE, A(COL[cl]), A(COL[cl]), 0.0f, glm::vec4(0.25f, 1.0f, 0, 0));
			std::string tx = registro::Registro::texto(l, nombre);
			float tt = 22.0f * s;
			while (medir(F_TEXTO, tx, tt) > w - 120.0f * s && tt > 15.0f * s) tt -= 1.0f * s;
			texto(F_TEXTO, tx, x + 104.0f * s, fy + fila * 0.5f + 8.0f * s, tt, A(sel ? glm::vec4(1.0f, 0.95f, 0.85f, 1.0f) : glm::vec4(0.86f, 0.9f, 0.97f, 1.0f)));
		}
		dibujarBarraDesplaza(x + w + 10.0f * s, y, h, primero, vis, n, a);
	}

	void dibujarListaCartas(const visores::Consulta& q, float x, float y, float w, float h, float a)
	{
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const std::vector<std::string> v = visores::lista(*ctl, q.vis.pestana, q.vis.lado);
		const int n = (int)v.size();
		if (n == 0) {
			texto(F_TEXTO, idioma::t("No hay cartas.", "No cards."), x + 10.0f * s, y + 40.0f * s, 24.0f * s, A(GRIS));
			return;
		}
		const float fila = 76.0f * s;
		const int vis = glm::max(1, (int)(h / fila));
		const int primero = glm::clamp(q.vis.sel - vis / 2, 0, glm::max(0, n - vis));
		const glm::vec2 mini = glm::vec2(46.0f, 66.0f) * s;
		for (int r = 0; r < vis && primero + r < n; r++) {
			const int idx = primero + r;
			const float fy = y + fila * (float)r;
			const bool sel = idx == q.vis.sel;
			usar(setAtlas, false);
			if (sel) {
				caja(x, fy + 3.0f * s, w, fila - 6.0f * s, 6.0f * s, PANEL, A(glm::vec4(0.32f, 0.23f, 0.05f, 0.55f)), A(ORO), 14.0f * s, glm::vec4(1.2f * s, 1.1f, 0.0f, 0.0f));
			}
			texto(F_NUMEROS, std::to_string(idx + 1), x + 26.0f * s, fy + fila * 0.5f + 7.0f * s, 17.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1);
			const glm::vec2 c(x + 50.0f * s + mini.x * 0.5f, fy + fila * 0.5f);
			const int ci = v[(size_t)idx].empty() ? -1 : buscarCarta(v[(size_t)idx]);
			if (ci >= 0 && miniLista(ci)) {
				usar(cartas[ci].setMini, false);
				const float u = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
				rect(c, mini, 0.0f, 0.0f, IMAGEN, A(glm::vec4(1.0f)), A(glm::vec4(0.6f, 0.8f, 1.0f, 1.0f)), 4.0f * s, glm::vec4(0.1f, 0.2f, (float)idx * 0.2f, 0.0f),
				     glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
			} else {
				Dorso dz = usarDorso(false, false, q.vis.lado != 0, 1.0f, (float)idx);
				dz.col.a *= a;
				rect(c, mini, 0.0f, 0.0f, DORSO, dz.col, dz.col2, 4.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
			}
			usar(setAtlas, false);
			const float tx = c.x + mini.x * 0.5f + 16.0f * s, tw = x + w - tx - 10.0f * s;
			if (ci < 0) {
				texto(F_TITULO, idioma::t("Carta boca abajo", "Face-down card"), tx, fy + fila * 0.5f + 8.0f * s, 21.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 0, 1.0f);
				continue;
			}
			const CartaUI& k = cartas[ci];
			std::string nom = k.vNombre();
			float tn = 23.0f * s;
			while (medir(F_TITULO, nom, tn, 1.0f) > tw && tn > 14.0f * s) tn -= 1.0f * s;
			texto(F_TITULO, nom, tx, fy + 32.0f * s, tn, A(sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
			texto(F_TEXTO, lineaTipo(k), tx, fy + 60.0f * s, 18.0f * s, A(glm::vec4(glm::vec3(colorAtributo(k.atributo)) * 0.6f + 0.35f, 1.0f)));
		}
		dibujarBarraDesplaza(x + w + 10.0f * s, y, h, primero, vis, n, a);
	}

	// "Mago · Nivel 7 · 2500 / 2100" o "Magia" / "Trampa"
	std::string lineaTipo(const CartaUI& k) const
	{
		if (k.clase == "magia") return idioma::t("Magia", "Spell");
		if (k.clase == "trampa") return idioma::t("Trampa", "Trap");
		return k.vTipo() + "  ·  " + idioma::t("Nivel ", "Level ") + std::to_string(k.nivel) + "  ·  " + std::to_string(k.atk) + " / " + std::to_string(k.def);
	}

	void dibujarBarraDesplaza(float x, float y, float h, int primero, int vis, int n, float a)
	{
		if (n <= vis) return;
		usar(setAtlas, false);
		caja(x - 3.0f * s, y, 6.0f * s, h, 0.0f, PANEL, glm::vec4(1.0f, 1.0f, 1.0f, 0.10f * a), NADA, 3.0f * s, glm::vec4(0.0f));
		const float lh = glm::max(30.0f * s, h * (float)vis / (float)n);
		const float ly = y + (h - lh) * (float)primero / (float)glm::max(1, n - vis);
		caja(x - 3.0f * s, ly, 6.0f * s, lh, 0.0f, PANEL, glm::vec4(glm::vec3(ORO), 0.85f * a), NADA, 3.0f * s, glm::vec4(0.0f));
	}

	// la carta elegida en la consulta: grande, con su nombre, tipo, ATK/DEF y el comienzo del texto
	void dibujarCartaConsulta(const std::string& id, bool oculta, float x, float y, float w, float h, float a)
	{
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const int ci = id.empty() ? -1 : buscarCarta(id);
		const CartaUI* k = ci >= 0 ? &cartas[ci] : nullptr;
		if (!k && !oculta) return;
		const glm::vec4 acento = k ? colorAtributo(k->atributo) : CARMESI;
		const float ch = glm::min(h * 0.62f, w * 1.45f * 0.62f), cw = ch / 1.45f;
		const glm::vec2 c(x + w * 0.5f, y + ch * 0.5f + 10.0f * s);
		usar(setAtlas, false);
		rect(c, glm::vec2(cw, ch) * 1.35f, 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(acento) * 0.25f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		if (k && caraLista(ci)) {
			usar(cartas[ci].setCara, false);
			const float u = 0.5f / k->cara.width, vv = 0.5f / k->cara.height;
			rect(c, glm::vec2(cw, ch), 0.0f, 0.0f, IMAGEN, A(glm::vec4(1.0f)), A(acento), 10.0f * s, glm::vec4(0.5f, 1.0f, (float)ci * 0.13f, 0.0f), glm::vec2(u, vv),
			     glm::vec2(1.0f - u, 1.0f - vv));
		} else {
			Dorso dz = usarDorso(false, false, true, 1.0f, 0.0f);
			dz.col.a *= a;
			rect(c, glm::vec2(cw, ch), 0.0f, 0.0f, DORSO, dz.col, dz.col2, 10.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		usar(setAtlas, false);
		float ty = y + ch + 54.0f * s;
		if (!k) return;
		std::string nom = k->vNombre();
		float tn = 30.0f * s;
		while (medir(F_TITULO, nom, tn) > w && tn > 16.0f * s) tn -= 1.0f * s;
		texto(F_TITULO, nom, x + w * 0.5f, ty, tn, A(BLANCO), A(acento), glm::vec3(0.3f, 0.0f, 0.0f), 1);
		ty += 34.0f * s;
		texto(F_TEXTO, lineaTipo(*k), x + w * 0.5f, ty, 20.0f * s, A(glm::vec4(0.72f, 0.86f, 1.0f, 1.0f)), NADA, glm::vec3(0.0f), 1);
		ty += 34.0f * s;
		const int lineas = (int)((y + h - ty) / (27.0f * s));
		if (lineas > 0) parrafo(F_TEXTO, k->vDesc(), x, ty, 21.0f * s, w, 27.0f * s, lineas, A(glm::vec4(0.82f, 0.86f, 0.93f, 1.0f)));
	}
