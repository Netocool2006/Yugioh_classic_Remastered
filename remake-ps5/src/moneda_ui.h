	// Yugioh Classic Remaster (rama classic-moneda): se incluye DENTRO de interfaz::Interfaz (desde duelo_ui.h).
	// El lanzamiento de moneda antes del duelo (moneda.h), como en Master Duel:
	//  - VS: los dos duelistas entran desde los costados (el rival con su retrato si es un personaje del modo historia;
	//    el jugador y los rivales genericos, que no tienen retrato, con un medallon de duelista dibujado por codigo) y un
	//    "VS" que golpea en el centro.
	//  - GIRO: la moneda (tipo MONEDA del shader de la interfaz: disco de oro con canto estriado, relieve y reflejos;
	//    una cara con el ojo Wedjat y la otra con el logo "Classic Remaster") sube, gira, cae y rebota; brillo y
	//    particulas al caer. Todo por codigo: sin texturas de Konami.
	//  - RESULTADO / ORDEN / RIVAL_DECIDE: el cartel, los botones "Ir primero" / "Ir segundo" y el aviso del rival.
	// Las medidas van en px de 1080 (* s): a 4K se ve igual de nitida (SDF por pixel).

	// texto centrado en x y aplastado en vertical (k) alrededor de yEje: el logo grabado en la moneda mientras gira
	void textoAplastado(int fu, const std::string& txt, float x, float y, float tam, float k, float yEje, glm::vec4 col, float espaciado = 0.0f)
	{
		usar(setAtlas, false);
		const Fuente& f = fuentes[fu];
		const float kk = tam / TAM_BASE;
		x -= medir(fu, txt, tam, espaciado) * 0.5f;
		const std::vector<int> cps = utf8(trd(txt));
		const float arriba = y - tam * 0.72f;
		auto Y = [&](float py) { return yEje + (py - yEje) * k; };
		for (size_t i = 0; i < cps.size(); i++) {
			auto it = f.glifos.find(cps[i]);
			if (it == f.glifos.end()) continue;
			const Glifo& g = it->second;
			if (g.w > 0.0f) {
				const float x0 = x + g.xoff * kk, y0 = y + g.yoff * kk, x1 = x0 + g.w * kk, y1 = y0 + g.h * kk;
				auto ly = [&](float py) { return (py - arriba) / (tam * 0.72f) * 2.0f - 1.0f; };
				const glm::vec4 c[4] = { recorte({ x0, Y(y0) }), recorte({ x1, Y(y0) }), recorte({ x1, Y(y1) }), recorte({ x0, Y(y1) }) };
				const glm::vec2 l[4] = { { 0, ly(y0) }, { 0, ly(y0) }, { 0, ly(y1) }, { 0, ly(y1) } };
				const glm::vec2 uv[4] = { { g.u0, g.v0 }, { g.u1, g.v0 }, { g.u1, g.v1 }, { g.u0, g.v1 } };
				quad(c, l, uv, col, NADA, glm::vec4(0, 0, 0, (float)TEXTO), glm::vec4(0.0f));
			}
			x += g.avance * kk + espaciado * s;
			if (i + 1 < cps.size()) x += stbtt_GetCodepointKernAdvance(&f.info, cps[i], cps[i + 1]) * f.escala * kk;
		}
	}

	// la moneda en "c" (centro), radio R, giro "ang" (cos > 0 cara), destello 0..1
	void pintarMoneda(glm::vec2 c, float R, float ang, float destello, float alfa)
	{
		const float co = cosf(ang), si = sinf(ang);
		usar(setAtlas, false);
		rect(c, glm::vec2(2.0f * R), 0.0f, R * 0.9f, MONEDA, glm::vec4(1.0f, 1.0f, 1.0f, alfa), NADA, 0.0f, glm::vec4(co, si, destello, ang * 0.35f + t * 0.3f));
		// el logo "Classic Remaster" grabado (lado cruz), aplastado con el giro
		const float k = fabsf(co);
		if (co < 0.0f && k > 0.12f) {
			const float fy = c.y - 0.055f * R * si;   // (el mismo centro de la cara que el shader)
			// grabado: un filo de luz abajo y el surco oscuro encima
			const float va = alfa * suave(0.12f, 0.4f, k);
			const glm::vec4 grabado(0.16f, 0.07f, 0.0f, va), filo(1.0f, 0.93f, 0.62f, va * 0.85f);
			const float d = std::max(1.0f, 0.012f * R);
			textoAplastado(F_NUMEROS, "CLASSIC", c.x + d, fy + d, 0.22f * R, k, fy, filo, 0.5f);
			textoAplastado(F_NUMEROS, "CLASSIC", c.x, fy, 0.22f * R, k, fy, grabado, 0.5f);
			textoAplastado(F_NUMEROS, "REMASTER", c.x + d, fy + 0.155f * R + d, 0.15f * R, k, fy, filo, 0.8f);
			textoAplastado(F_NUMEROS, "REMASTER", c.x, fy + 0.155f * R, 0.15f * R, k, fy, grabado, 0.8f);
		}
	}

	// un duelista: su retrato (personaje) o un medallon (jugador / rival generico), con halo y placa con el nombre
	void duelistaMoneda(const std::string& personaje, bool rival, const std::string& nombre, glm::vec2 pie, float alto, float brillo, float alfa)
	{
		const glm::vec4 lado = rival ? CARMESI : CIAN;
		usar(setAtlas, false);
		rect(pie - glm::vec2(0.0f, alto * 0.5f), glm::vec2(alto * 0.95f, alto * 1.1f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(lado) * 0.22f * brillo, alfa), NADA, 0.0f,
		     glm::vec4(0.0f));
		float ancho = 0.0f;
		if (!personaje.empty()) ancho = pintarRetrato(personaje, pie, alto, glm::vec4(glm::vec3(brillo), alfa));
		if (ancho <= 0.0f) {   // sin retrato (el jugador, los rivales genericos): un medallon de duelista dibujado por codigo
			const float R = alto * 0.27f;
			const glm::vec2 c = pie - glm::vec2(0.0f, alto * 0.52f);
			const glm::vec4 oscuro(0.02f, 0.03f, 0.06f, alfa), tenue(glm::vec3(lado) * 0.35f * brillo, alfa);
			usar(setAtlas, false);
			rect(c, glm::vec2(2.0f * R), 0.0f, 18.0f * s, ORBE, glm::vec4(glm::vec3(lado) * brillo, alfa), glm::vec4(0.05f, 0.07f, 0.13f, alfa), 0.0f,
			     glm::vec4(0.9f, 1.0f, 0.0f, 0.0f));
			rect(c, glm::vec2(2.0f * R * 0.86f), 0.0f, 4.0f * s, ORBE, tenue, glm::vec4(0.03f, 0.04f, 0.09f, alfa), 0.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
			// la silueta: hombros y cabeza, con el borde de luz del lado
			const float sw = R * 1.22f, sh = R * 0.62f;
			caja(c.x - sw * 0.5f, c.y + R * 0.22f, sw, sh, 6.0f * s, PANEL, oscuro, glm::vec4(glm::vec3(lado) * brillo, alfa), sh * 0.5f, glm::vec4(1.6f * s, 1.2f, 0.0f, 0.0f));
			rect(c - glm::vec2(0.0f, R * 0.2f), glm::vec2(R * 0.66f), 0.0f, 6.0f * s, ORBE, glm::vec4(glm::vec3(lado) * brillo, alfa), oscuro, 0.0f, glm::vec4(0.4f, 1.0f, 0.0f, 0.0f));
			// destello que recorre el aro
			const float g = t * 0.9f + (rival ? 3.1f : 0.0f);
			rect(c + glm::vec2(cosf(g), sinf(g)) * R * 0.97f, glm::vec2(26.0f * s), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(1.0f, 0.95f, 0.85f) * brillo, alfa), NADA, 0.0f,
			     glm::vec4(0.8f, 0.0f, 0.0f, 0.0f));
			ancho = 2.0f * R;
		}
		const float pw = std::max(ancho * 0.8f, 320.0f * s), ph = 60.0f * s;
		caja(pie.x - pw * 0.5f, pie.y - ph * 0.8f, pw, ph, 14.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.82f * alfa), glm::vec4(glm::vec3(lado), alfa), 14.0f * s,
		     glm::vec4(1.3f * s, 0.9f, 0.10f, 12.0f * s));
		texto(F_NUMEROS, nombre, pie.x, pie.y - ph * 0.8f + 40.0f * s, 27.0f * s, glm::vec4(glm::vec3(BLANCO), alfa), glm::vec4(glm::vec3(lado), alfa), glm::vec3(0.4f, 0.0f, 0.5f), 1,
		      1.0f);
	}

	void dibujarMoneda(const control::VistaDuelo& vd)
	{
		const moneda::Estado& m = vd.moneda;
		const bool en = idioma::ingles();
		const float a = suave(0.0f, 0.25f, vd.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float cx = W * 0.5f;
		rz.tapar();   // rama classic-pc: un clic = X (lanzar / seguir)
		zBoton(0.0f, 0.0f, W, H, control::B_X);
		const bool vs = m.etapa == moneda::VS;
		const float tvs = vs ? m.t : 99.0f;
		// velo y las dos mitades (cian / carmesi) en diagonal
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.01f, 0.012f, 0.03f, 0.88f)), NADA, 0.0f, glm::vec4(0.0f));
		const float entra = suave(0.0f, 0.6f, tvs);
		for (int lado = 0; lado < 2; lado++) {
			const float sg = lado ? 1.0f : -1.0f;
			const glm::vec4 col = lado ? CARMESI : CIAN;
			const glm::vec2 c(cx + sg * (W * 0.27f + (1.0f - entra) * W * 0.5f), H * 0.5f);
			rect(c, glm::vec2(W * 0.5f, H * 1.4f), sg * -0.16f, 0.0f, PANEL, A(glm::vec4(glm::vec3(col) * 0.10f, 0.6f)), A(glm::vec4(glm::vec3(col) * 0.6f, 1.0f)), 0.0f,
			     glm::vec4(1.4f * s, 1.0f, 0.15f, 0.0f));
			rect(c, glm::vec2(W * 0.4f, H * 0.9f), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(col) * 0.10f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		}
		// el haz del centro (la linea que separa los dos lados)
		const float haz = vs ? 1.0f : m.etapa == moneda::GIRO ? 1.0f - 0.75f * suave(0.0f, 0.5f, m.t) : 0.25f;   // (se apaga cuando sale la moneda)
		rect(glm::vec2(cx, H * 0.5f), glm::vec2(H * 1.3f, 10.0f * s), 3.1415927f * 0.5f - 0.16f, 0.0f, LINEA, A(glm::vec4(glm::vec3(ORO) * 0.9f, haz)), NADA, 0.0f,
		     glm::vec4(1.5f, 0.0f, 0.0f, 0.0f));
		// los duelistas
		const bool hayRes = m.etapa >= moneda::RESULTADO;
		const std::string rival = en && !vd.rivalEn.empty() ? vd.rivalEn : vd.rivalEs;
		for (int lado = 0; lado < 2; lado++) {
			const float sg = lado ? 1.0f : -1.0f;
			const float alto = 0.64f * H;
			const glm::vec2 pie(cx + sg * (W * 0.31f + (1.0f - entra) * W * 0.45f), H * 0.93f + sinf(t * 1.1f + (float)lado) * 4.0f * s);
			float br = vs ? 1.0f : 0.8f;
			if (hayRes) br = m.ganador == lado ? 1.12f + 0.06f * sinf(t * 3.0f) : 0.45f;
			duelistaMoneda(lado ? vd.personaje : std::string(), lado == 1, lado ? rival : std::string(idioma::tr("Tú")), pie, alto, br, a * suave(0.0f, 0.3f, tvs));
		}
		// "VS": golpea a los 0.45 s y se va cuando sale la moneda
		const float vsA = vs ? suave(0.4f, 0.5f, m.t) : (m.etapa == moneda::GIRO ? 1.0f - suave(0.0f, 0.3f, m.t) : 0.0f);
		if (vsA > 0.0f) {
			const float golpe = vs ? 1.0f - suave(0.4f, 0.65f, m.t) : 0.0f;
			usar(setAtlas, false);
			rect(glm::vec2(cx, H * 0.5f), glm::vec2((360.0f + 500.0f * golpe) * s), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(1.0f, 0.85f, 0.5f) * (0.35f + 1.2f * golpe), vsA)), NADA,
			     0.0f, glm::vec4(0.6f, 0.0f, 0.0f, 0.0f));
			texto(F_NUMEROS, "VS", cx, H * 0.5f + 40.0f * s * (1.0f + 1.4f * golpe), 120.0f * s * (1.0f + 1.4f * golpe), A(glm::vec4(1.0f, 0.95f, 0.8f, vsA)),
			      A(glm::vec4(glm::vec3(ORO), vsA)), glm::vec3(1.0f, 0.0f, 1.0f), 1, 4.0f);
		}
		// titulo
		texto(F_NUMEROS, idioma::tr("LANZAMIENTO DE MONEDA"), cx, 92.0f * s, 40.0f * s, A(BLANCO), A(ORO), glm::vec3(0.5f, 0.0f, 0.6f), 1, 3.0f);
		if (m.etapa <= moneda::GIRO)
			texto(F_TITULO, idioma::tr("Cara: decides tú · Cruz: decide el rival"), cx, 136.0f * s, 24.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
		if (vs) return;
		// la moneda
		const float tg = m.etapa == moneda::GIRO ? m.t : moneda::T_GIRO;
		const moneda::Pose po = moneda::pose(tg, m.cara);
		const float R = 150.0f * s * po.escala;
		const glm::vec2 reposo(cx, H * 0.45f);
		const glm::vec2 c(reposo.x, reposo.y + po.y * H);
		// sombra en la mesa (mas chica y tenue cuanto mas alta)
		const float alt = std::min(1.0f, std::max(0.0f, -po.y) / 0.2f);
		usar(setAtlas, false);
		rect(glm::vec2(reposo.x, reposo.y + 172.0f * s), glm::vec2(260.0f * s * (1.0f - 0.45f * alt), 34.0f * s * (1.0f - 0.4f * alt)), 0.0f, 22.0f * s, PANEL,
		     A(glm::vec4(0.0f, 0.0f, 0.0f, 0.55f * (1.0f - 0.6f * alt))), NADA, 17.0f * s * (1.0f - 0.4f * alt), glm::vec4(0.0f, 0.0f, 0.0f, 20.0f * s));
		// brillo en reposo (late) y al caer
		const float reposoBrillo = hayRes ? 0.12f + 0.08f * sinf(t * 2.5f) : 0.0f;
		const float destello = std::max(po.brillo, reposoBrillo);
		if (po.cayo) {
			rect(c, glm::vec2(R * (3.0f + 3.0f * (1.0f - po.brillo))), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(1.0f, 0.82f, 0.45f) * (0.25f + 0.9f * po.brillo), 1.0f)), NADA, 0.0f,
			     glm::vec4(0.8f, 0.0f, 0.0f, 0.0f));
		}
		pintarMoneda(c, R, po.angulo, destello, a);
		// particulas al caer: chispas doradas que salen del borde (deterministas: se ven igual en cada cuadro)
		const float tl = m.etapa == moneda::GIRO ? m.t - moneda::T_CAE : m.t + (moneda::T_GIRO - moneda::T_CAE) + (m.etapa > moneda::RESULTADO ? moneda::T_RESULTADO : 0.0f);
		if (po.cayo && tl >= 0.0f && tl < 1.1f) {
			const float u = tl / 1.1f;
			for (int i = 0; i < 40; i++) {
				const float h1 = fmodf(sinf((float)i * 12.9898f) * 43758.5453f, 1.0f), h2 = fmodf(sinf((float)i * 78.233f) * 12345.678f, 1.0f);
				const float ang = (float)i * 2.39996f, vel = 0.6f + 0.4f * fabsf(h1);
				const float d = R * (0.9f + 2.4f * vel * (1.0f - (1.0f - u) * (1.0f - u)));
				const glm::vec2 q = c + glm::vec2(cosf(ang), sinf(ang) * 0.75f) * d + glm::vec2(0.0f, 60.0f * s * u * u);
				const float tam = (6.0f + 10.0f * fabsf(h2)) * s * (1.0f - u * 0.7f);
				rect(q, glm::vec2(tam * 2.4f), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::mix(glm::vec3(1.0f, 0.7f, 0.25f), glm::vec3(1.0f, 0.95f, 0.75f), fabsf(h2)) * 1.6f, 1.0f - u)), NADA,
				     0.0f, glm::vec4(0.7f, 0.0f, 0.0f, 0.0f));
			}
		}
		if (!hayRes) return;
		// el resultado
		const float ra = suave(0.0f, 0.3f, m.etapa == moneda::RESULTADO ? m.t : 1.0f);
		auto B = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * a * ra); };
		const bool gana = m.ganador == 0;
		const float pulso = 1.0f + (m.etapa == moneda::RESULTADO ? 0.25f * (1.0f - suave(0.0f, 0.35f, m.t)) : 0.0f);
		texto(F_NUMEROS, idioma::tr(m.cara == moneda::CARA ? "¡CARA!" : "¡CRUZ!"), cx, reposo.y + 238.0f * s, 46.0f * s * pulso, B(ORO), B(glm::vec4(0.5f, 0.3f, 0.05f, 1.0f)),
		      glm::vec3(0.7f, 0.0f, 0.8f), 1, 4.0f);
		texto(F_NUMEROS, idioma::tr(gana ? "Ganaste el lanzamiento de moneda" : "El rival ganó el lanzamiento"), cx, 150.0f * s, 36.0f * s,
		      B(gana ? glm::vec4(1.0f, 0.86f, 0.45f, 1.0f) : glm::vec4(1.0f, 0.45f, 0.55f, 1.0f)), B(gana ? ORO : CARMESI), glm::vec3(0.45f, 0.12f, 0.0f), 1, 1.5f);
		const float pie = H - 52.0f * s;
		auto ayuda = [&](bool mover) {
			float bx = cx - (mover ? 150.0f : 70.0f) * s;
			if (mover) {
				bx += iconoBoton(glm::vec2(bx, pie), "CRUZ", 14.0f * s, a) + 10.0f * s;
				bx += texto(F_TITULO, idioma::tr("Opción"), bx, pie + 8.0f * s, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f) + 34.0f * s;
			}
			bx += iconoBoton(glm::vec2(bx, pie), "X", 14.0f * s, a) + 10.0f * s;
			texto(F_TITULO, idioma::tr(mover ? "Elegir" : "Continuar"), bx, pie + 8.0f * s, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
		};
		if (m.etapa == moneda::ORDEN) {
			const float oa = suave(0.0f, 0.25f, m.t);
			auto C = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * a * oa); };
			texto(F_TITULO, idioma::tr("Elige: ¿ir primero o segundo?"), cx, H * 0.72f, 27.0f * s, C(BLANCO), NADA, glm::vec3(0.0f), 1, 1.0f);
			static const char* const ORDEN[] = { "Ir primero", "Ir segundo" };
			for (int k = 0; k < 2; k++) {
				const bool sel = k == m.opcion;
				const float lev = sel ? 4.0f * s * sinf(t * 4.0f) : 0.0f;
				const float bw = 330.0f * s, bh = 76.0f * s, bx = cx + (k ? 28.0f * s : -28.0f * s - bw), by = H * 0.76f - lev;
				if (sel)
					rect(glm::vec2(bx + bw * 0.5f, by + bh * 0.5f), glm::vec2(bw * 1.3f, bh * 2.2f), 0.0f, 0.0f, PARTICULA, C(glm::vec4(glm::vec3(ORO) * 0.35f, 1.0f)), NADA, 0.0f,
					     glm::vec4(0.0f));
				caja(bx, by, bw, bh, 10.0f * s, PANEL, C(sel ? glm::vec4(0.36f, 0.25f, 0.05f, 0.85f) : glm::vec4(glm::vec3(VIDRIO), 0.75f)), C(sel ? ORO : glm::vec4(glm::vec3(CIAN) * 0.7f, 1.0f)),
				     bh * 0.5f, glm::vec4((sel ? 1.8f : 1.1f) * s, 1.1f, sel ? 0.25f : 0.0f, 12.0f * s));
				texto(F_NUMEROS, idioma::tr(ORDEN[k]), bx + bw * 0.5f, by + bh * 0.5f + 11.0f * s, 30.0f * s, C(sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : BLANCO), NADA,
				      glm::vec3(0.0f), 1, 1.5f);
				if (sel) iconoBoton(glm::vec2(bx + bw - 34.0f * s, by + bh * 0.5f), "X", 13.0f * s, a * oa);
			}
			ayuda(true);
		} else if (m.etapa == moneda::RIVAL_DECIDE) {
			const float oa = suave(0.0f, 0.3f, m.t);
			auto C = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * a * oa); };
			usar(setAtlas, false);
			const float w = 900.0f * s, h = 96.0f * s, y = H * 0.74f;
			caja(cx - w * 0.5f, y, w, h, 16.0f * s, PANEL, C(glm::vec4(glm::vec3(VIDRIO), 0.85f)), C(CARMESI), h * 0.5f, glm::vec4(1.5f * s, 1.0f, 0.2f, 14.0f * s));
			texto(F_TITULO, rival, cx, y + 34.0f * s, 22.0f * s, C(CARMESI), NADA, glm::vec3(0.0f), 1, 1.0f);
			texto(F_NUMEROS, idioma::tr(m.empieza < 0 ? "El rival está eligiendo…" : m.empieza == 1 ? "El rival decide ir primero" : "El rival decide ir segundo"), cx, y + 74.0f * s, 32.0f * s, C(BLANCO), C(CARMESI),
			      glm::vec3(0.4f, 0.0f, 0.6f), 1, 1.0f);
			ayuda(false);
		}
	}
