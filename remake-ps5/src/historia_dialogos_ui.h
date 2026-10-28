// Yugioh Classic Remaster (rama classic-historia): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas y
// los retratos de personajes_ui.h). Lo que se ve del modo historia fuera de la mesa (logica en historia_dialogos.h):
//  - INTRO: antes del duelo, el retrato grande del rival a la izquierda, su nombre en una placa y la caja de texto
//    abajo; la linea se escribe letra a letra, X pasa (o la completa), Circulo salta la escena.
//  - FINAL: al completar la historia, el texto del final con el retrato del Faraon, los creditos que suben y lo
//    desbloqueado (Modo Desafio).
//  - RESULTADO (modo historia): la linea de cierre del rival en un globo bajo el panel (cierreEnResultado()).
//  - ELEGIR_RIVAL: los atajos R1 Reglas (tutorial.h) y, con la historia completa, L1 Modo Desafio.

private:
	// parte "txt" en lineas de "ancho" px (por palabras)
	std::vector<std::string> partirLineas(int fu, const std::string& txt, float tam, float ancho) const
	{
		std::vector<std::string> r;
		std::string linea;
		size_t a = 0;
		while (a <= txt.size()) {
			size_t b = txt.find(' ', a);
			if (b == std::string::npos) b = txt.size();
			const std::string palabra = txt.substr(a, b - a);
			const std::string prueba = linea.empty() ? palabra : linea + " " + palabra;
			if (!linea.empty() && medir(fu, prueba, tam) > ancho) {
				r.push_back(linea);
				linea = palabra;
			} else {
				linea = prueba;
			}
			a = b + 1;
		}
		if (!linea.empty()) r.push_back(linea);
		return r;
	}

	// las lineas con solo los primeros "n" bytes visibles (sin cortar un caracter UTF-8); devuelve las y dibujadas
	void textoEscrito(int fu, const std::vector<std::string>& lineas, size_t n, float x, float y, float tam, float inter, glm::vec4 col, int alin = 0)
	{
		struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
		for (size_t k = 0; k < lineas.size(); k++) {
			const std::string& l = lineas[k];
			if (n == 0) break;
			size_t m = std::min(n, l.size());
			while (m < l.size() && ((unsigned char)l[m] & 0xC0) == 0x80) m++;
			texto(fu, l.substr(0, m), x, y + inter * (float)k, tam, col, NADA, glm::vec3(0.0f), alin, 0.0f);
			n = n > l.size() ? n - l.size() - 1 : 0;
		}
	}

	void dibujarEscenaHistoria()
	{
		historia_dialogos::Escena& e = historia_dialogos::escena();
		e.cuadro(dtCuadro);
		if (!e.activa()) {
			dibujarCierreResultado();
			dibujarAtajosHistoria();
			return;
		}
		const float a = suave(0.0f, 0.45f, e.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		rz.tapar();   // rama classic-pc: un clic avanza el dialogo (X)
		zBoton(0.0f, 0.0f, W, H, control::B_X);
		usar(setAtlas, false);
		if (e.tipo == historia_dialogos::FINAL && e.etapa != historia_dialogos::F_TEXTO) {
			dibujarCreditos(e);
			return;
		}
		const bool final = e.tipo == historia_dialogos::FINAL;
		// velo: abajo mas oscuro (la caja), arriba deja ver el lugar del rival
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.01f, 0.012f, 0.025f, final ? 0.86f : 0.42f)), NADA, 0.0f, glm::vec4(0.0f));
		caja(0.0f, H * 0.55f, W, H * 0.45f, 0.0f, PANEL, A(glm::vec4(0.0f, 0.0f, 0.02f, 0.55f)), NADA, 0.0f, glm::vec4(0.0f));
		// el retrato grande a la izquierda (entra deslizandose); en el final, centrado y tenue
		const float entra = suave(0.0f, 0.6f, e.t);
		const float alto = H * (final ? 0.92f : 0.86f);
		const glm::vec2 pie = final ? glm::vec2(W * 0.5f, H + 10.0f * s) : glm::vec2(W * 0.2f - (1.0f - entra) * 160.0f * s, H + 12.0f * s + sinf(t * 1.1f) * 4.0f * s);
		rect(pie - glm::vec2(0.0f, alto * 0.5f), glm::vec2(alto * 0.75f, alto), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(final ? ORO : CARMESI) * 0.22f, 1.0f)), NADA,
		     0.0f, glm::vec4(0.0f));
		pintarRetrato(e.personaje, pie, alto, glm::vec4(glm::vec3(final ? 0.55f : 1.0f), (final ? 0.6f : 1.0f) * a * entra));
		// la caja de texto
		const float bx = final ? W * 0.14f : W * 0.30f, bw = final ? W * 0.72f : W * 0.66f, bh = 250.0f * s, by = H - bh - 46.0f * s;
		const glm::vec4 acento = final ? ORO : CARMESI;
		caja(bx, by, bw, bh, 22.0f * s, PANEL, A(glm::vec4(glm::vec3(VIDRIO), 0.92f)), A(acento), 20.0f * s, glm::vec4(1.6f * s, 1.0f, 0.12f, 18.0f * s));
		// placa con el nombre (en el final, "FIN")
		const std::string nom = final ? std::string(idioma::t("EL FINAL", "THE END")) : e.nombre();
		const float nw = medir(F_NUMEROS, nom, 30.0f * s, 1.5f) + 60.0f * s, nh = 54.0f * s;
		caja(bx + 36.0f * s, by - nh * 0.6f, nw, nh, 12.0f * s, PANEL, A(glm::vec4(0.06f, 0.03f, 0.05f, 0.95f)), A(acento), nh * 0.5f, glm::vec4(1.4f * s, 1.0f, 0.2f, 10.0f * s));
		{
			struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
			texto(F_NUMEROS, nom, bx + 36.0f * s + nw * 0.5f, by - nh * 0.6f + nh * 0.5f + 11.0f * s, 30.0f * s, A(BLANCO), A(acento), glm::vec3(0.4f, 0.0f, 0.5f), 1, 1.5f);
		}
		// la linea, letra a letra
		if (const historia_dialogos::Linea* l = e.actual()) {
			const float tam = 34.0f * s;
			const std::vector<std::string> lineas = partirLineas(F_TEXTO, l->v(), tam, bw - 120.0f * s);
			textoEscrito(F_TEXTO, lineas, e.visibles(), final ? bx + bw * 0.5f : bx + 60.0f * s, by + 82.0f * s, tam, tam * 1.35f, A(BLANCO), final ? 1 : 0);
		}
		// pie de la caja: X sigue (late cuando la linea esta completa), Circulo salta, n / total
		const float py = by + bh - 34.0f * s;
		const float late = e.lineaCompleta() ? 0.6f + 0.4f * sinf(t * 5.0f) : 0.35f;
		float x = bx + bw - 40.0f * s;
		x -= texto(F_TITULO, idioma::t("Seguir", "Next"), x, py + 8.0f * s, 22.0f * s, glm::vec4(glm::vec3(BLANCO), a * late), NADA, glm::vec3(0.0f), 2, 1.0f);
		iconoBoton(glm::vec2(x - 26.0f * s, py), "X", 14.0f * s, a * late);
		if (!final) {
			x -= 90.0f * s;
			x -= texto(F_TITULO, idioma::t("Saltar", "Skip"), x, py + 8.0f * s, 22.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 2, 1.0f);
			iconoBoton(glm::vec2(x - 26.0f * s, py), "O", 14.0f * s, a);
		}
		texto(F_TITULO, std::to_string(e.linea + 1) + " / " + std::to_string(e.total()), bx + 60.0f * s, py + 8.0f * s, 20.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 0, 1.0f);
	}

	// FINAL: creditos que suben y, al acabar, lo desbloqueado
	void dibujarCreditos(const historia_dialogos::Escena& e)
	{
		const historia_dialogos::Dialogos& d = historia_dialogos::base();
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.01f, 0.97f), NADA, 0.0f, glm::vec4(0.0f));
		rect(glm::vec2(W * 0.5f, H * 0.5f), glm::vec2(W * 0.9f, H * 0.9f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(ORO) * 0.08f, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
		if (e.etapa == historia_dialogos::F_CREDITOS) {
			const float vel = (H + 200.0f * s) / e.duracionCreditos() * 0.9f;
			const float y0 = H + 60.0f * s - e.t * vel * 1.6f;
			for (size_t k = 0; k < d.creditos.size(); k++) {
				const float y = y0 + (float)k * 150.0f * s;
				const float al = std::min(suave(H * 0.98f, H * 0.82f, y), suave(H * 0.02f, H * 0.16f, y));
				if (al <= 0.0f) continue;
				if (k == 0) {
					texto(F_NUMEROS, d.creditos[k].v(), W * 0.5f, y, 74.0f * s, glm::vec4(glm::vec3(BLANCO), al), glm::vec4(glm::vec3(ORO), al), glm::vec3(0.7f, 0.0f, 0.9f), 1, 3.0f);
				} else {
					texto(F_TITULO, d.creditos[k].v(), W * 0.5f, y, 34.0f * s, glm::vec4(glm::vec3(k + 1 == d.creditos.size() ? ORO : BLANCO), al), NADA, glm::vec3(0.0f), 1, 1.0f);
				}
			}
			iconoBoton(glm::vec2(W - 210.0f * s, H - 50.0f * s), "X", 14.0f * s, 0.6f);
			texto(F_TITULO, idioma::t("Adelantar", "Skip ahead"), W - 186.0f * s, H - 42.0f * s, 21.0f * s, glm::vec4(glm::vec3(GRIS), 0.6f), NADA, glm::vec3(0.0f), 0, 1.0f);
			return;
		}
		// lo desbloqueado
		const float a = suave(0.0f, 0.5f, e.tLinea);
		const float pw = 1100.0f * s, ph = 380.0f * s, px = W * 0.5f - pw * 0.5f, py = H * 0.5f - ph * 0.5f;
		caja(px, py, pw, ph, 26.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.95f * a), glm::vec4(glm::vec3(ORO), a), 22.0f * s, glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s));
		texto(F_NUMEROS, "YUGIOH CLASSIC REMASTER", W * 0.5f, py + 70.0f * s, 40.0f * s, glm::vec4(glm::vec3(BLANCO), a), glm::vec4(glm::vec3(ORO), a), glm::vec3(0.5f, 0.0f, 0.7f), 1, 2.0f);
		const std::vector<std::string> lineas = partirLineas(F_TITULO, d.desbloqueo.v(), 31.0f * s, pw - 120.0f * s);
		for (size_t k = 0; k < lineas.size(); k++)
			texto(F_TITULO, lineas[k], W * 0.5f, py + 150.0f * s + (float)k * 44.0f * s, 31.0f * s, glm::vec4(glm::vec3(k == 0 ? ORO : BLANCO), a), NADA, glm::vec3(0.0f), 1, 1.0f);
		const float late = 0.6f + 0.4f * sinf(t * 4.0f);
		iconoBoton(glm::vec2(W * 0.5f - 80.0f * s, py + ph - 46.0f * s), "X", 14.0f * s, a * late);
		texto(F_TITULO, idioma::t("Continuar", "Continue"), W * 0.5f - 56.0f * s, py + ph - 38.0f * s, 22.0f * s, glm::vec4(glm::vec3(BLANCO), a * late), NADA, glm::vec3(0.0f), 0, 1.0f);
	}

	// RESULTADO en el modo historia: el rival dice su linea de cierre (globo bajo el panel del resultado)
	void dibujarCierreResultado()
	{
		if (!ctl || !historia_dialogos::cierreEnResultado()) return;
		const control::VistaDuelo& vd = ctl->vista;
		if (vd.pantalla != control::VistaDuelo::RESULTADO || vd.modo != control::VistaDuelo::HISTORIA || vd.personaje.empty() || vd.ganador < 0) return;
		const std::string frase = historia_dialogos::dialogoCierre(vd.personaje, vd.ganador == 0);
		if (frase.empty()) return;
		const float a = suave(0.5f, 1.1f, vd.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float cx = (CAMPO_X * s + W) * 0.5f;
		const float bw = 980.0f * s, bh = 112.0f * s, bx = cx - bw * 0.5f + 70.0f * s, by = H - bh - 26.0f * s;
		usar(setAtlas, false);
		const glm::vec4 acento = vd.ganador == 0 ? ORO : CARMESI;
		caja(bx, by, bw, bh, 16.0f * s, PANEL, A(glm::vec4(glm::vec3(VIDRIO), 0.9f)), A(acento), 18.0f * s, glm::vec4(1.3f * s, 0.9f, 0.1f, 14.0f * s));
		pintarRetrato(vd.personaje, glm::vec2(bx - 30.0f * s, H + 6.0f * s), 250.0f * s, glm::vec4(glm::vec3(vd.ganador == 0 ? 0.8f : 1.0f), a));
		const std::string& nom = idioma::ingles() && !vd.rivalEn.empty() ? vd.rivalEn : vd.rivalEs;
		struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
		texto(F_NUMEROS, nom, bx + 90.0f * s, by + 34.0f * s, 22.0f * s, A(acento), NADA, glm::vec3(0.3f, 0.0f, 0.4f), 0, 1.0f);
		const std::vector<std::string> lineas = partirLineas(F_TEXTO, "\"" + frase + "\"", 24.0f * s, bw - 130.0f * s);
		for (size_t k = 0; k < lineas.size() && k < 2; k++)
			texto(F_TEXTO, lineas[k], bx + 90.0f * s, by + 66.0f * s + (float)k * 30.0f * s, 24.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 0.0f);
	}

	// una tecla con nombre (L1, R1...) con su borde izquierdo en x; devuelve su ancho (iconoBoton no lo devuelve
	// para las teclas con nombre)
	float teclaBoton(float x, float y, const char* b, float a)
	{
		const std::string nombre = std::string(b) == "L1R1" ? "L1 R1" : b;
		const float r = 14.0f * s, w = medir(F_NUMEROS, nombre, 14.0f * s, 1.0f) + 14.0f * s;
		iconoBoton(glm::vec2(x + r, y), b, r, a);
		return w;
	}

	// ELEGIR_RIVAL: R1 Reglas y, con la historia completa, L1 Modo Desafio (arriba a la derecha)
	void dibujarAtajosHistoria()
	{
		if (!ctl || tutorial::estado().abierto) return;
		const control::VistaDuelo& vd = ctl->vista;
		if (vd.pantalla != control::VistaDuelo::ELEGIR_RIVAL) return;
		const float a = suave(0.0f, 0.4f, vd.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const bool desafio = vd.modo == control::VistaDuelo::HISTORIA && historia_dialogos::desafioDisponible();
		const float w = (desafio ? 560.0f : 230.0f) * s, h = 56.0f * s, x0 = W - w - 30.0f * s, y0 = 26.0f * s;
		usar(setAtlas, false);
		caja(x0, y0, w, h, 10.0f * s, PANEL, A(glm::vec4(glm::vec3(VIDRIO), 0.85f)), A(glm::mix(CIAN, ORO, 0.5f)), h * 0.5f, glm::vec4(1.2f * s, 0.9f, 0.1f, 10.0f * s));
		float x = x0 + 26.0f * s;
		const float y = y0 + h * 0.5f;
		x += teclaBoton(x, y, "R1", a) + 10.0f * s;
		x += texto(F_TITULO, idioma::t("Reglas", "Rules"), x, y + 8.0f * s, 22.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f) + 34.0f * s;
		if (desafio) {
			const bool on = historia_dialogos::desafioActivo();
			x += teclaBoton(x, y, "L1", a) + 10.0f * s;
			const std::string txt = std::string(idioma::t("Modo Desafío: ", "Challenge Mode: ")) + (on ? idioma::t("SÍ", "ON") : idioma::t("NO", "OFF"));
			texto(F_TITULO, txt, x, y + 8.0f * s, 22.0f * s, A(on ? ORO : GRIS), NADA, glm::vec3(0.0f), 0, 1.0f);
		}
	}
