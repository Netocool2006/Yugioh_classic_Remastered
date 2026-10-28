// Yugioh Classic Remaster (rama classic-personajes): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
//  - Retratos de los personajes (assets/personajes/retrato_<id>.png, RGBA ~1400 px de alto): cache de MAX_RETRATOS
//    con mipmaps, cargados a pedido (retrato(id) los pide; cargarRetratos() los carga al empezar el cuadro).
//  - DUELO: el retrato del rival GRANDE arriba a la derecha (el hueco libre de la mesa) con su nombre y lema; aparece
//    con un fundido, se mece un poco y se ilumina en rojo al recibir danno.
//  - ELEGIR_RIVAL en modo historia: la saga, los personajes (vencidos, libres o cerrados), el elegido con su retrato,
//    lema, personalidad, nivel y mazo. Triangulo cambia a duelo libre (los genericos por nivel, duelo_ui.h).
//  - RESULTADO: personaje vencido, cartas firma desbloqueadas y saga nueva.
// Textos por idioma.h (tabla) y los nombres/lemas en los dos idiomas de personajes.txt.

public:
	static constexpr int MAX_RETRATOS = 4;

private:
	struct Retrato { std::string id; vks::Texture2D tex; VkDescriptorSet set{ VK_NULL_HANDLE }; uint64_t uso{ 0 }; bool cargado{ false }; };
	std::array<Retrato, MAX_RETRATOS> retratos{};
	mutable std::string retratoPedido;
	std::unordered_map<std::string, bool> retratoFalta;

	const Retrato* retrato(const std::string& id) const
	{
		if (id.empty()) return nullptr;
		for (const Retrato& r : retratos) {
			if (r.cargado && r.id == id) {
				const_cast<Retrato&>(r).uso = cuadroN;
				return &r;
			}
		}
		if (!retratoFalta.count(id)) retratoPedido = id;
		return nullptr;
	}

	void cargarRetratos()
	{
		const std::string id = retratoPedido;
		retratoPedido.clear();
		if (id.empty() || retratoFalta.count(id)) return;
		int lugar = -1;
		for (int i = 0; i < MAX_RETRATOS; i++) {
			const Retrato& r = retratos[(size_t)i];
			if (r.cargado && r.id == id) return;
			if (r.cargado && r.uso + (uint64_t)maxConcurrentFrames + 2 >= cuadroN) continue;   // (en uso por la GPU)
			if (lugar < 0 || !r.cargado || (retratos[(size_t)lugar].cargado && r.uso < retratos[(size_t)lugar].uso)) lugar = i;
		}
		if (lugar < 0) return;
		Retrato& r = retratos[(size_t)lugar];
		if (r.cargado) r.tex.destroy();
		r.cargado = false;
		if (!cargarConMips(rutaAssets + "personajes/retrato_" + id + ".png", r.tex, true)) {
			retratoFalta[id] = true;
			return;
		}
		if (r.set == VK_NULL_HANDLE) {
			r.set = nuevoSet(r.tex);
		} else {
			apuntarSet(r.set, r.tex);
		}
		r.id = id;
		r.cargado = true;
		r.uso = cuadroN;
	}

	// el retrato con su base (centro abajo) en "pie" y "alto" px; col: brillo (rgb) y alfa. Devuelve el ancho (0 si aun
	// no esta cargado)
	float pintarRetrato(const std::string& id, glm::vec2 pie, float alto, glm::vec4 col, float ang = 0.0f)
	{
		const Retrato* r = retrato(id);
		if (!r || r->tex.height == 0) return 0.0f;
		const float ancho = alto * (float)r->tex.width / (float)r->tex.height;
		usar(r->set, false);
		const float u = 0.5f / (float)r->tex.width, vv = 0.5f / (float)r->tex.height;
		// (IMAGEN con extra.w = 2: usa el alfa de la imagen; sin foil ni destello)
		rect(pie - glm::vec2(0.0f, alto * 0.5f), glm::vec2(ancho, alto), ang, 0.0f, IMAGEN, col, NADA, 0.0f, glm::vec4(0.0f, 0.0f, 0.0f, 2.0f),
		     glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
		usar(setAtlas, false);
		return ancho;
	}

	// ---- DUELO: el retrato del rival arriba a la derecha
	void dibujarRetratoRival(const control::VistaDuelo& vd)
	{
		if (vd.personaje.empty()) return;
		const bool en = idioma::ingles();
		const float a = suave(0.0f, 0.9f, vd.t);
		const float alto = 0.30f * H, der = W - 26.0f * s, arriba = 12.0f * s;
		const float vaiven = sinf(t * 1.1f) * 5.0f * s;
		const float golpe = 1.0f - suave(0.0f, 0.7f, vidaR.tGolpe);   // 1 al recibir danno
		const glm::vec3 brillo = glm::mix(glm::vec3(1.0f), glm::vec3(1.75f, 0.75f, 0.70f), golpe) * (vd.pensando ? 0.92f + 0.08f * sinf(t * 4.0f) : 1.0f);
		const glm::vec2 pie(der - 0.36f * alto, arriba + alto + vaiven * 0.3f);
		// halo del lado rival detras
		usar(setAtlas, false);
		rect(pie - glm::vec2(0.0f, alto * 0.48f), glm::vec2(alto * 0.95f, alto * 1.05f), 0.0f, 0.0f, PARTICULA,
		     glm::vec4(glm::vec3(CARMESI) * (0.16f + 0.25f * golpe), a), NADA, 0.0f, glm::vec4(0.0f));
		const float ancho = pintarRetrato(vd.personaje, pie + glm::vec2(0.0f, vaiven), alto, glm::vec4(brillo, a), glm::radians(0.6f) * sinf(t * 0.7f));
		// nombre y lema (placa al pie del retrato)
		const std::string& nom = en && !vd.rivalEn.empty() ? vd.rivalEn : vd.rivalEs;
		const std::string& lema = en && !vd.lemaEn.empty() ? vd.lemaEn : vd.lemaEs;
		const float pw = std::max(ancho, 300.0f * s) + 24.0f * s, ph = 58.0f * s;
		const float px = der - pw, py = arriba + alto - ph * 0.55f;
		caja(px, py, pw, ph, 14.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.78f * a), glm::vec4(glm::vec3(CARMESI), a), 14.0f * s,
		     glm::vec4(1.2f * s, 0.8f, 0.10f, 12.0f * s));
		texto(F_NUMEROS, nom, der - 14.0f * s, py + 27.0f * s, 23.0f * s, glm::vec4(glm::vec3(BLANCO), a), glm::vec4(glm::vec3(CARMESI), a), glm::vec3(0.4f, 0.0f, 0.5f), 2, 1.0f);
		if (!lema.empty())
			texto(F_TEXTO, "\"" + lema + "\"", der - 14.0f * s, py + 48.0f * s, 15.0f * s, glm::vec4(glm::vec3(GRIS), a), NADA, glm::vec3(0.0f), 2, 0.0f);
	}

	// ---- ELEGIR_RIVAL, modo historia
	void dibujarHistoria(const control::VistaDuelo& vd, float a)
	{
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const bool en = idioma::ingles();
		const float cx = W * 0.5f;
		const float pw = 1400.0f * s, ph = 700.0f * s;
		const float px = cx - pw * 0.5f, py = H * 0.5f - ph * 0.5f + (1.0f - a) * 20.0f * s;
		const glm::vec4 acento = vd.sagaAbierta ? ORO : glm::vec4(0.45f, 0.5f, 0.6f, 1.0f);
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.01f, 0.015f, 0.03f, 0.84f)), NADA, 0.0f, glm::vec4(0.0f));
		rz.tapar();   // rama classic-pc
		rect(glm::vec2(cx, H * 0.5f), glm::vec2(pw * 1.25f, ph * 1.3f), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(acento) * 0.12f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		caja(px, py, pw, ph, 30.0f * s, PANEL, A(VIDRIO), A(acento), 22.0f * s, glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s));
		// retrato grande a la izquierda (gris si la saga esta cerrada)
		const float alto = ph - 40.0f * s;
		const glm::vec3 br = vd.sagaAbierta ? glm::vec3(1.0f) : glm::vec3(0.28f, 0.3f, 0.36f);
		usar(setAtlas, false);
		rect(glm::vec2(px + 270.0f * s, py + ph * 0.5f), glm::vec2(alto * 0.8f, alto), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(acento) * 0.22f, 1.0f)), NADA, 0.0f,
		     glm::vec4(0.0f));
		pintarRetrato(vd.personaje, glm::vec2(px + 270.0f * s, py + ph - 8.0f * s + sinf(t * 1.1f) * 3.0f * s), alto, A(glm::vec4(br, 1.0f)));
		// columna derecha
		const float x0 = px + 560.0f * s, xc = px + 560.0f * s + (pw - 600.0f * s) * 0.5f;
		texto(F_NUMEROS, idioma::tr("MODO HISTORIA"), xc, py + 64.0f * s, 36.0f * s, A(BLANCO), A(CIAN), glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
		const std::string saga = en ? vd.sagaEn : vd.sagaEs;
		texto(F_TITULO, std::string(idioma::tr("Saga")) + " " + std::to_string(vd.saga + 1) + " / " + std::to_string(vd.sagas) + "  ·  " + saga, xc, py + 108.0f * s,
		      25.0f * s, A(acento), NADA, glm::vec3(0.0f), 1, 1.5f);
		// rama classic-pc: la saga (mitad izquierda = anterior, mitad derecha = siguiente) y el nombre = empezar
		zBoton(xc - 330.0f * s, py + 80.0f * s, 330.0f * s, 44.0f * s, control::B_ARRIBA);
		zBoton(xc, py + 80.0f * s, 330.0f * s, 44.0f * s, control::B_ABAJO);
		zBoton(xc - 320.0f * s, py + 215.0f * s, 640.0f * s, 140.0f * s, control::B_X);
		// los personajes de la saga: orbes (oro vencido, cian libre, gris cerrado), el elegido grande
		const int n = (int)vd.fila.size();
		const float paso = std::min(64.0f * s, (pw - 640.0f * s) / std::max(1, n));
		const float ox = xc - 0.5f * paso * (float)(n - 1), oy = py + 160.0f * s;
		for (int i = 0; i < n; i++) {
			const int e = vd.filaEstado[(size_t)i];
			const bool sel = i == vd.idx;
			const glm::vec4 col = e == 2 ? ORO : e == 1 ? CIAN : glm::vec4(0.3f, 0.34f, 0.42f, 1.0f);
			const glm::vec2 c(ox + paso * (float)i, oy);
			zLista(c.x - paso * 0.5f, c.y - 28.0f * s, paso, 56.0f * s, i, vd.idx, true, 0);   // rama classic-pc: el personaje de la saga
			rect(c, glm::vec2((sel ? 24.0f : 15.0f) * s), 0.0f, 6.0f * s, ORBE, A(col), A(glm::vec4(glm::vec3(col) * 0.4f, 1.0f)), 0.0f,
			     glm::vec4(sel ? 1.0f : 0.6f, 1.0f, 0.0f, 0.0f));
		}
		const std::string& nom = en && !vd.rivalEn.empty() ? vd.rivalEn : vd.rivalEs;
		const std::string& lema = en && !vd.lemaEn.empty() ? vd.lemaEn : vd.lemaEs;
		texto(F_NUMEROS, nom, xc, py + 262.0f * s, 46.0f * s, A(vd.sagaAbierta ? ORO : GRIS), A(glm::vec4(0.5f, 0.3f, 0.05f, 1.0f)), glm::vec3(0.6f, 0.0f, 0.8f), 1, 1.5f);
		if (!lema.empty()) texto(F_TEXTO, "\"" + lema + "\"", xc, py + 306.0f * s, 22.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 1, 0.0f);
		std::string info = std::string(ia::textoIA("ia.nivel", en)) + " " + std::to_string(vd.nivelRival);
		if (!vd.personalidad.empty()) {
			const std::string clave = "ia.personalidad." + vd.personalidad;
			info += "  ·  " + std::string(ia::textoIA(clave.c_str(), en));
		}
		texto(F_TITULO, info, xc, py + 348.0f * s, 21.0f * s, A(CIAN), NADA, glm::vec3(0.0f), 1, 2.0f);
		const std::string& var = en ? vd.varianteEn : vd.varianteEs;
		if (!var.empty())
			texto(F_TITULO, std::string(idioma::tr("Mazo")) + ": " + var, xc, py + 382.0f * s, 20.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
		// estado
		const int e = vd.idx >= 0 && vd.idx < n ? vd.filaEstado[(size_t)vd.idx] : 0;
		const char* estado = !vd.sagaAbierta ? "BLOQUEADO: vence a todos los de la saga anterior" : e == 2 ? "VENCIDO" : "DISPONIBLE";
		const float lat = 0.75f + 0.25f * sinf(t * 3.0f);
		texto(F_NUMEROS, idioma::tr(estado), xc, py + 440.0f * s, (vd.sagaAbierta ? 24.0f : 19.0f) * s,
		      A(glm::vec4(glm::vec3(e == 2 ? ORO : vd.sagaAbierta ? CIAN : CARMESI) * (vd.sagaAbierta ? 1.0f : lat), 1.0f)), NADA, glm::vec3(0.3f, 0.0f, 0.4f), 1, 1.5f);
		texto(F_TITULO, std::string(idioma::tr("Personajes vencidos")) + " " + std::to_string(vd.vencidos) + " / " + std::to_string(vd.totalPersonajes) + "   ·   " +
		                    idioma::tr("Victorias") + " " + std::to_string(vd.victorias) + "   ·   " + idioma::tr("Derrotas") + " " + std::to_string(vd.derrotas),
		      xc, py + 486.0f * s, 18.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
		// botones
		float bx = x0;
		const float by = py + ph - 46.0f * s;
		auto boton = [&](const std::string& b, const char* txt) {
			bx += iconoBoton(glm::vec2(bx + 14.0f * s, by), b, 14.0f * s, a) + 12.0f * s;
			bx += texto(F_TITULO, idioma::tr(txt), bx, by + 8.0f * s, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f) + 30.0f * s;
		};
		boton("CRUZ", "Personaje / saga");
		boton("X", "Duelo");
		boton("T", "Duelo libre");
		boton(idioma::tr("TÁCTIL"), "Construir mazo");
		boton("O", "Volver");   // rama classic-menus: al menu principal
	}

	// ---- RESULTADO: lo que trae vencer a un personaje (debajo del titulo)
	void dibujarResultadoPersonaje(const control::VistaDuelo& vd, float cx, float y, float a)
	{
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		if (!vd.personajeNuevo) return;
		std::string l = idioma::tr("¡PERSONAJE VENCIDO!");
		if (vd.firmaNuevas > 0) l += "  ·  " + std::to_string(vd.firmaNuevas) + " " + idioma::tr("cartas firma desbloqueadas");
		texto(F_NUMEROS, l, cx, y, 26.0f * s, A(ORO), A(glm::vec4(0.5f, 0.3f, 0.05f, 1.0f)), glm::vec3(0.5f, 0.0f, 0.6f), 1, 1.5f);
		if (vd.sagaNueva)
			texto(F_NUMEROS, std::string(idioma::tr("NUEVA SAGA")) + ": " + (idioma::ingles() ? vd.sagaEn : vd.sagaEs), cx, y + 40.0f * s, 24.0f * s, A(CIAN), NADA,
			      glm::vec3(0.5f, 0.0f, 0.6f), 1, 1.5f);
	}
