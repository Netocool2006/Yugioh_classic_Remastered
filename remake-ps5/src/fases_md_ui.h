	// Rama classic-md-fases: el dibujo del selector de fases, el engranaje y los carteles de turno y de fase de Yu-Gi-Oh!
	// Master Duel con NUESTRO estilo (vidrio, oro/cian/carmesi) y los DATOS de MD: cada movimiento sale de las curvas de
	// sus Timeline y de sus Tween (clips_md.h, exportados por herramientas/md_fases/exportar_clips.py a <assets>/md_ui/)
	// y los iconos son sus sprites (md_ui.png). Sin esos archivos (no van al repo) cae a los mismos valores escritos aca.
	// Se incluye dentro de la clase de la interfaz (interfaz.h). docs/md_fases.md tiene la tabla MD -> aca.

	vks::Texture2D texMD;
	VkDescriptorSet setMD{ VK_NULL_HANDLE };
	bool mdCargado{ false };
	float tTurnoRivalUI{ 99.0f };   // s desde que se ve el cartel fijo TURNO DEL RIVAL (entra con fundido, sin corte)
	bool turnoRivalAntes{ false };

	// una vez, en actualizar(): los datos de MD y el atlas de sus sprites (un descriptor de la reserva)
	void cargarMD()
	{
		if (mdCargado) return;
		mdCargado = true;
		clipsmd::Biblioteca& b = clipsmd::biblioteca();
		b.cargar(rutaAssets + "md_ui/");
		if (cargarConMips(rutaAssets + "md_ui/md_ui.png", texMD, false)) {
			setMD = nuevoSet(texMD);
			b.atlas.cargar(rutaAssets + "md_ui/md_ui_atlas.txt", (float)texMD.width, (float)texMD.height);
		} else {
			fprintf(stderr, "[interfaz] sin los sprites de Master Duel (%smd_ui/md_ui.png): iconos dibujados por codigo\n", rutaAssets.c_str());
		}
	}
	// un sprite de MD centrado en c, del tamanno "tam" px (col: tinte y alfa). false si no esta
	bool spriteMD(const char* nombre, glm::vec2 c, glm::vec2 tam, glm::vec4 col, float ang = 0.0f)
	{
		if (setMD == VK_NULL_HANDLE) return false;
		const clipsmd::Atlas& a = clipsmd::biblioteca().atlas;
		const clipsmd::Sprite* sp = a.sprite(nombre);
		if (!sp) return false;
		usar(setMD, false);
		rect(c, tam, ang, 0.0f, IMAGEN, col, NADA, 0.0f, glm::vec4(0.0f, 0.0f, 0.0f, 2.0f), glm::vec2(sp->x / a.ancho, sp->y / a.alto),
		     glm::vec2((sp->x + sp->w) / a.ancho, (sp->y + sp->h) / a.alto));
		usar(setAtlas, false);
		return true;
	}
	// el tamanno en px de un sprite a "alto" px de alto (conserva la proporcion)
	glm::vec2 tamSpriteMD(const char* nombre, float alto) const
	{
		const clipsmd::Sprite* sp = clipsmd::biblioteca().atlas.sprite(nombre);
		return sp && sp->h > 0.0f ? glm::vec2(alto * sp->w / sp->h, alto) : glm::vec2(alto);
	}
	// un Tween de un prefab de MD (o el de respaldo con los mismos valores)
	static clipsmd::Tween tweenMD(const char* prefab, const char* obj, const char* label, const char* tipo, int easing, int estilo, float dur, float desde,
	                              float hasta)
	{
		if (const clipsmd::Prefab* p = clipsmd::biblioteca().prefab(prefab))
			if (const clipsmd::Tween* w = p->tween(obj, label, tipo)) return *w;
		clipsmd::Tween w;
		w.obj = obj, w.label = label, w.tipo = tipo, w.easing = easing, w.estilo = estilo, w.dur = dur;
		w.desde = { desde };
		w.hasta = { hasta };
		return w;
	}

	// ---------------------------------------------------------------------------------- el engranaje (DuelMenuButton)
	// Abajo a la izquierda, junto al selector de camara (pedido del usuario; en MD va arriba a la izquierda, 84 px). Su
	// arte de MD tal cual (GUI_T_DuelButtonSubmenu01 normal / 02 resaltado / 03 apretado); con el foco, la capa "Over"
	// late como en MD (TweenAlpha select: 1 -> 0.4, CubicIn, PingPongLoop, 1 s) y al lado, el boton del panel tactil
	glm::vec2 centroEngranaje() const { return glm::vec2((CAMPO_X - 40.0f) * s, (1080.0f - MARGEN - 32.0f) * s); }
	void dibujarEngranajeMD()
	{
		if (!ctl || !ctl->duelo || ctl->vista.pantalla != control::VistaDuelo::DUELO || ctl->vista.sorteo) return;
		const glm::vec2 c = centroEngranaje();
		const float tam = 72.0f * s;   // (84 px de MD a la escala del hueco de la columna de fases)
		const bool foco = ctl->foco.tipo == control::F_ENGRANAJE;
		const bool abierto = ctl->vista.pausa;
		zFoco(c.x - tam * 0.5f, c.y - tam * 0.5f, tam, tam, control::F_ENGRANAJE, -1);   // rama classic-pc
		const float click = 1.0f - suave(0.0f, 0.25f, ctl->tEngranaje);
		usar(setAtlas, false);
		rect(c, glm::vec2(tam * 1.5f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(CIAN) * (0.10f + 0.25f * (foco ? 1.0f : 0.0f) + 0.3f * click), 1.0f), NADA, 0.0f,
		     glm::vec4(0.0f));
		bool arte = spriteMD(abierto || click > 0.5f ? "GUI_T_DuelButtonSubmenu03" : "GUI_T_DuelButtonSubmenu01", c, glm::vec2(tam), glm::vec4(1.0f));
		if (arte && foco) {
			const clipsmd::Tween w = tweenMD("DuelMenuButton", "Button/Over", "select", "TweenAlpha", clipsmd::CUBIC_IN, clipsmd::PING_PONG_LOOP, 1.0f, 1.0f, 0.4f);
			spriteMD("GUI_T_DuelButtonSubmenu02", c, glm::vec2(tam), glm::vec4(1.0f, 1.0f, 1.0f, w.valor(t)));
		}
		if (!arte) {   // (sin el arte de MD: un engranaje de vidrio dibujado)
			rect(c, glm::vec2(tam), 0.0f, 4.0f * s, ORBE, foco ? ORO : CIAN, glm::vec4(0.03f, 0.05f, 0.1f, 1.0f), 0.0f, glm::vec4(0.4f, 0.85f, 0, 0));
			for (int k = 0; k < 8; k++) {
				const float a = (float)k * glm::quarter_pi<float>();
				rect(c + glm::vec2(cosf(a), sinf(a)) * tam * 0.3f, glm::vec2(tam * 0.16f), a, 0.0f, PANEL, foco ? ORO : CIAN, NADA, 2.0f * s, glm::vec4(0.0f));
			}
			rect(c, glm::vec2(tam * 0.5f), 0.0f, 0.0f, ORBE, foco ? ORO : CIAN, glm::vec4(0.03f, 0.05f, 0.1f, 1.0f), 0.0f, glm::vec4(0.6f, 0.85f, 0, 0));
		}
		if (foco) rect(c, glm::vec2(tam * 1.12f), t * 0.8f, 8.0f * s, MARCO, ORO, ORO, tam * 0.56f, glm::vec4(2.0f * s, 6.0f * s, 1.0f, 0.0f));
		// PadIcon de MD (48 px abajo a la derecha del boton): aca el boton del panel tactil
		iconoBoton(c + glm::vec2(tam * 0.36f, tam * 0.42f), "TÁCTIL", 9.0f * s, 0.9f);
	}

	// ------------------------------------------------------------------ el selector de fases (PhaseSelectWindow de MD)
	// Junto al indicador de fase (la columna DP..EP), a la altura de la fase actual. De MD: el velo (ScreenPanel negro
	// 0.44 con FrameIn 0 -> 0.3 lineal 0.12 s), la ventana que entra (Window In: QuartOut 0.12 s; Out: CubicIn 0.12 s),
	// el titulo ("Select Phase to Change to"), los botones redondos de fase (GUI_PhaseButtonBase2 + GUI_PhaseCircle con
	// su icono GUI_PhaseBP / MP2 / EP y el texto debajo, sombra desplazada 8 px), los guiones entre ellos (Arrow 24x2
	// gris 0.44), el elegido con su resplandor (GUI_PhaseButtonCursor2: TweenAlpha 0 -> 0.8 CubicOut 1 s) y la flecha
	// encima (GUI_PhaseCursorArrow a 42 px), y las no validas apagadas (alfa 0.5)
	void dibujarSelectorFasesMD()
	{
		if (!ctl) return;
		const control::Mando::SelectorFases& sf = ctl->selFases;
		const bool cerrando = !sf.abierto && sf.tCierre < control::Mando::MD_VENTANA;
		if (!sf.abierto && !cerrando) return;
		const clipsmd::Tween velo = tweenMD("PhaseSelectWindow", "ScreenPanel", sf.abierto ? "FrameIn" : "FrameOut", "TweenAlpha", clipsmd::LINEAR, 0,
		                                     control::Mando::MD_VENTANA, sf.abierto ? 0.0f : 0.3f, sf.abierto ? 0.3f : 0.0f);
		const clipsmd::Tween ven = tweenMD("PhaseSelectWindow", "Window", sf.abierto ? "In" : "Out", "TweenPosition", sf.abierto ? clipsmd::QUART_OUT : clipsmd::CUBIC_IN,
		                                    0, control::Mando::MD_VENTANA, sf.abierto ? -560.0f : -20.0f, sf.abierto ? -20.0f : -560.0f);
		const float tv = sf.abierto ? sf.t : sf.tCierre;
		const float a = glm::clamp(velo.valor(tv) / 0.3f, 0.0f, 1.0f);   // (la apertura 0..1, del velo de MD)
		// la ventana de MD sube 540 px (de -560 a -20, su alto); aca entra desde la columna de fases la misma fraccion
		const float y0md = ven.desde.empty() ? -560.0f : ven.desde[1 < ven.desde.size() ? 1 : 0];
		const float y1md = ven.hasta.empty() ? -20.0f : ven.hasta[1 < ven.hasta.size() ? 1 : 0];
		const float avance = fabsf(y1md - y0md) > 1.0f ? (ven.valor(tv, 1 < ven.desde.size() ? 1 : 0) - y0md) / (y1md - y0md) : 1.0f;
		const float k = glm::clamp(avance, 0.0f, 1.0f);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		usar(setAtlas, false);
		caja(CAMPO_X * s, 0.0f, W - CAMPO_X * s, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.443f * velo.valor(tv)), NADA, 0.0f, glm::vec4(0.0f));
		// geometria: tres botones (MD: 224 px; aca 150) con su texto, en una ventana de vidrio a la derecha de la columna
		const float boton = 150.0f * s, paso = boton + 70.0f * s, cab = 74.0f * s * 0.8f, pie = 58.0f * s;
		const float w = 3.0f * boton + 2.0f * 70.0f * s + 2.0f * 46.0f * s, h = cab + boton + 120.0f * s + pie;
		const float faseY = 540.0f * s - 82.0f * s * 2.5f + 82.0f * s * (float)glm::clamp(sf.desde, 0, 5);
		const float x0 = (CAMPO_X + 18.0f) * s - (1.0f - k) * w * 0.35f;
		const float y0 = glm::clamp(faseY - h * 0.5f, 20.0f * s, H - h - 120.0f * s);
		caja(x0, y0, w, h, 26.0f * s, PANEL, A(VIDRIO), A(glm::mix(CIAN, ORO, 0.55f)), 18.0f * s, glm::vec4(1.5f * s, 1.0f, 0.10f, 18.0f * s));
		if (sf.abierto) {   // rama classic-pc: el selector se queda con el raton (clic en un boton de fase = elegirla)
			rz.tapar();
			for (int i = 0; i < control::Mando::SEL_FASES; i++)
				if (sf.valida[(size_t)i]) zLista(x0 + w * 0.5f - paso + paso * (float)i - boton * 0.5f, y0 + cab + 50.0f * s, boton, boton + 70.0f * s, i, sf.opcion, true);
		}
		// la flecha que une la ventana con la fase actual de la columna
		rect(glm::vec2((CAMPO_X + 4.0f) * s, faseY), glm::vec2(24.0f * s, 2.0f * s), 0.0f, 4.0f * s, LINEA, A(ORO), NADA, 0.0f, glm::vec4(1.5f, 0.3f, 0, 0));
		// cabecera (Top: negro 0.65, 74 px) con el titulo
		caja(x0 + 10.0f * s, y0 + 10.0f * s, w - 20.0f * s, cab - 10.0f * s, 4.0f * s, PANEL, A(glm::vec4(0.0f, 0.0f, 0.0f, 0.65f)), NADA, 14.0f * s, glm::vec4(0.0f));
		texto(F_NUMEROS, "SELECCIONA LA FASE", x0 + w * 0.5f, y0 + cab * 0.5f + 14.0f * s, 22.0f * s, A(glm::vec4(1.0f, 0.95f, 0.8f, 1.0f)), A(ORO),
		      glm::vec3(0.3f, 0.0f, 0.5f), 1, 2.5f);
		static const char* const ICONO[control::Mando::SEL_FASES] = { "GUI_PhaseBP", "GUI_PhaseMP2", "GUI_PhaseEP" };
		static const char* const ABREV[control::Mando::SEL_FASES] = { "BP", "M2", "EP" };
		const float cy = y0 + cab + 50.0f * s + boton * 0.5f;
		const float cx0 = x0 + w * 0.5f - paso;
		const clipsmd::Tween brillo = tweenMD("PhaseSelectWindow", "Phase4Cursor", "", "TweenAlpha", clipsmd::CUBIC_OUT, 0, 1.0f, 0.0f, 0.8f);
		for (int i = 0; i < control::Mando::SEL_FASES; i++) {
			const glm::vec2 c(cx0 + paso * (float)i, cy);
			const bool valida = sf.valida[(size_t)i], sel = i == sf.opcion && valida;
			const float al = valida ? 1.0f : 0.5f;
			// los guiones de MD entre botones (RootPhaseHyphen/Arrow: 24x2, gris 0.44)
			if (i > 0)
				rect(c - glm::vec2(paso * 0.5f, 0.0f), glm::vec2(24.0f * s, 2.0f * s), 0.0f, 0.0f, PANEL, A(glm::vec4(0.44f, 0.44f, 0.44f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
			// sombra (Phase*BackCircle: +8,-8 px, gris 0.31 alfa 0.39)
			rect(c + glm::vec2(8.0f, 8.0f) * s * (boton / 224.0f / s * s), glm::vec2(boton), 0.0f, 0.0f, ORBE, A(glm::vec4(0.31f, 0.31f, 0.31f, 0.39f)), NADA, 0.0f,
			     glm::vec4(0.0f, 1.0f, 0, 0));
			if (sel) {   // el resplandor del elegido (GUI_PhaseButtonCursor2) que entra en 1 s
				const float b = brillo.valor(sf.tElegida);
				if (!spriteMD("GUI_PhaseButtonCursor2", c, glm::vec2(boton * 194.0f / 174.0f), A(glm::vec4(glm::vec3(ORO), b))))
					rect(c, glm::vec2(boton * 1.15f), 0.0f, 20.0f * s, PARTICULA, A(glm::vec4(glm::vec3(ORO) * 0.5f * b, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
			}
			// el boton: vidrio oscuro con el aro de MD (GUI_PhaseCircle) en cian / oro (elegido) / gris (no valida)
			rect(c, glm::vec2(boton), 0.0f, 0.0f, ORBE, A(glm::vec4(glm::vec3(sel ? ORO : CIAN) * (valida ? 0.9f : 0.35f), al)), A(glm::vec4(0.03f, 0.05f, 0.1f, 0.92f)), 0.0f,
			     glm::vec4(sel ? 0.9f : 0.35f, 0.9f, 0, 0));
			const glm::vec4 aro = !valida ? glm::vec4(0.45f, 0.5f, 0.6f, 0.5f) : sel ? ORO : glm::vec4(glm::vec3(CIAN), 0.9f);
			if (!spriteMD("GUI_PhaseCircle", c, glm::vec2(boton * 176.0f / 174.0f), A(aro)))
				rect(c, glm::vec2(boton), t * 0.5f, 6.0f * s, MARCO, A(aro), A(aro), boton * 0.5f, glm::vec4(2.0f * s, 6.0f * s, 1.0f, 0.0f));
			// icono (55/224 del boton, 23 px arriba del centro) y texto (26 px abajo), como Phase*/Content/Normal
			const float ic = boton * 55.0f / 224.0f * 1.35f;
			const glm::vec4 colIc = !valida ? glm::vec4(0.6f, 0.65f, 0.75f, 0.5f) : sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : BLANCO;
			if (!spriteMD(ICONO[i], c - glm::vec2(0.0f, boton * 23.0f / 224.0f * 1.2f), tamSpriteMD(ICONO[i], ic), A(colIc)))
				texto(F_NUMEROS, ABREV[i], c.x, c.y - boton * 0.05f, 30.0f * s, A(colIc), NADA, glm::vec3(0.0f), 1, 1.0f);
			texto(F_NUMEROS, ABREV[i], c.x, c.y + boton * 26.0f / 224.0f * 1.6f + 6.0f * s, 20.0f * s, A(colIc), sel ? A(ORO) : NADA, glm::vec3(sel ? 0.3f : 0.0f, 0.0f, 0.0f), 1, 2.0f);
			// la accion debajo del boton (el texto pedido por el usuario)
			float tt = 19.0f * s;
			const std::string acc = control::Mando::textoSelector(i);
			while (medir(F_TITULO, acc, tt, 0.5f) > paso - 8.0f * s && tt > 13.0f * s) tt -= 1.0f * s;
			texto(F_TITULO, acc, c.x, cy + boton * 0.5f + 34.0f * s, tt, A(!valida ? glm::vec4(0.55f, 0.6f, 0.7f, 0.5f) : sel ? ORO : BLANCO), NADA, glm::vec3(0.0f), 1, 0.5f);
			// la flecha del cursor encima (Phase*Cursor2: GUI_PhaseCursorArrow, 64 px a 42 px del borde de arriba)
			if (sel) {
				const float bob = sinf(t * 5.0f) * 4.0f * s;
				const glm::vec2 pf = c - glm::vec2(0.0f, boton * 0.5f + 42.0f * s * (boton / (224.0f * s)) + bob);
				if (!spriteMD("GUI_PhaseCursorArrow", pf, tamSpriteMD("GUI_PhaseCursorArrow", 40.0f * s), A(ORO)))
					rect(pf, glm::vec2(26.0f * s), glm::pi<float>(), 10.0f * s, FLECHA, A(ORO), A(glm::vec4(1.0f, 0.8f, 0.5f, 1.0f)), 0.0f, glm::vec4(0.0f));
			}
		}
		// pie: X confirmar / circulo cerrar (ButtonClose de MD)
		const float py = y0 + h - pie * 0.5f;
		float x = x0 + w * 0.5f - 150.0f * s;
		x += iconoBoton(glm::vec2(x, py), "X", 13.0f * s, a) + 8.0f * s;
		texto(F_TITULO, "Confirmar", x, py + 7.0f * s, 19.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
		x = x0 + w * 0.5f + 40.0f * s;
		x += iconoBoton(glm::vec2(x, py), "O", 13.0f * s, a) + 8.0f * s;
		texto(F_TITULO, "Cerrar", x, py + 7.0f * s, 19.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
	}

	// ------------------------------------------------------------- el cartel de fase (DuelXxxPhase_near/far de MD)
	// Sus curvas: el grupo "center" entra desde x = +6 (unidades de Unity) hasta 0; la banda img_Phase_* se estira de
	// 0.25 a 4 (scale.x) y aparece (alfa 0.2 -> 1); el texto (TextGroup) llega de x 2.75 a 1, se descubre (Text:
	// m_SizeDelta.x 0.2 -> 10) y se enciende (m_fontColor.a); al final todo sale a la derecha (o, en la Fase de Batalla,
	// el texto estalla: TextFace escala a 1.5 y TextLine a 2 mientras se apagan). Escala: la banda de MD mide 3.14 u a
	// escala 1 (img_Phase_near 314 px, 100 px por unidad); aca la banda llena (escala 4) mide 760 px
	void dibujarCartelFaseMD()
	{
		if (!ctl) return;
		static const char* const TL[] = { "DuelDrawPhase", "DuelStanbyPhase", "DuelMain01Phase", "DuelBattlePhase", "DuelMain02Phase", "DuelEndPhase" };
		const int f = glm::clamp(ctl->fase, 0, 5);
		const bool rival = ctl->vista.turnoRival;
		const std::string nom = std::string(TL[f]) + (rival ? "_far" : "_near");
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline(nom);
		const float dur = tl ? tl->duracion : (f == 3 ? control::Mando::MD_FASE_BP : control::Mando::MD_FASE);
		if (!ctl->cartelFaseVisible(dur) || ctl->cartelTurnoVisible()) return;
		const float tt = ctl->tFase;
		const std::string banda = rival ? "img_Phase_far" : "img_Phase_near";
		// los valores de MD (o, sin el archivo, los mismos de la curva en sus puntos: docs/md_fases.md)
		auto V = [&](const char* obj, const char* attr, float def) { return tl ? tl->valor(obj, attr, tt, def) : def; };
		const float centroX = V("center", "pos.x", 0.0f);
		const float escala = V(banda.c_str(), "scale.x", 4.0f);
		const float alfaBanda = V(banda.c_str(), "m_Color.a", tt < dur - 0.1f ? 1.0f : 0.0f);
		const float grupoX = V("TextGroup", "pos.x", 1.0f);
		const float revela = V("Text", "m_SizeDelta.x", 10.0f);
		const float alfaTxt = V("Text", "m_fontColor.a", 0.78f) / 0.78f;   // (0.78 es el alfa de reposo del texto de MD)
		const float u = 760.0f * s / (3.14f * 4.0f);                        // px por unidad de Unity
		const float cx = (CAMPO_X * s + W) * 0.5f + centroX * u, cy = H * (rival ? 0.40f : 0.36f);
		const glm::vec4 acento = rival ? CARMESI : ORO;
		const float bw = 3.14f * escala * u, bh = 1.15f * u * 1.6f;
		usar(setAtlas, false);
		if (alfaBanda > 0.01f && bw > 1.0f) {
			rect(glm::vec2(cx, cy), glm::vec2(bw * 1.2f, bh * 2.0f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(acento) * 0.18f * alfaBanda, 1.0f), NADA, 0.0f,
			     glm::vec4(0.0f));
			caja(cx - bw * 0.5f, cy - bh * 0.5f, bw, bh, 26.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.82f * alfaBanda), glm::vec4(glm::vec3(acento), alfaBanda),
			     bh * 0.5f, glm::vec4(1.6f * s, 1.2f, 0.3f, 16.0f * s));
		}
		// el texto: se descubre desde el centro (el rectangulo del texto de MD mide "revela" unidades de ancho)
		const std::string nombre = idioma::tr(control::NOMBRE_FASE_LARGO[f]);
		const float tam = 40.0f * s, esp = 2.0f;
		const float tx = cx + (grupoX - 1.0f) * u;   // (TextGroup reposa en x = 1: se mide desde ahi)
		const float ancho = medir(F_NUMEROS, nombre, tam, esp);
		const float visible = revela * u;
		if (alfaTxt > 0.01f) {
			const std::vector<int> cps = utf8(nombre);
			std::string pre;
			for (size_t i = 0; i < cps.size(); i++) {
				std::string letra;
				const int cp = cps[i];
				if (cp < 0x80) letra = (char)cp;
				else if (cp < 0x800) letra = std::string{ (char)(0xC0 | (cp >> 6)), (char)(0x80 | (cp & 0x3F)) };
				else letra = std::string{ (char)(0xE0 | (cp >> 12)), (char)(0x80 | ((cp >> 6) & 0x3F)), (char)(0x80 | (cp & 0x3F)) };
				const float lx = tx - ancho * 0.5f + medir(F_NUMEROS, pre, tam, esp);
				const float lw = medir(F_NUMEROS, letra, tam, esp);
				pre += letra;
				if (fabsf(lx + lw * 0.5f - tx) > visible * 0.5f) continue;   // (fuera del rectangulo que se descubre)
				crudo = true;
				texto(F_NUMEROS, letra, lx, cy + 14.0f * s, tam, glm::vec4(1.0f, 0.95f, 0.8f, alfaTxt), glm::vec4(glm::vec3(acento), alfaTxt), glm::vec3(0.5f, 0.0f, 0.8f), 0,
				      esp);
				crudo = false;
			}
			texto(F_NUMEROS, FASES[f], tx, cy - 26.0f * s, 15.0f * s, glm::vec4(glm::vec3(GRIS), alfaTxt), NADA, glm::vec3(0.0f), 1, 4.0f);
		}
		// la Fase de Batalla: el estallido del texto (TextFace y TextLine crecen y se apagan)
		if (f == 3 && tl) {
			const float aFace = V("TextFace", "m_fontColor.a", 0.0f), eFace = V("TextFace", "scale.x", 1.0f);
			const float aLine = V("TextLine", "m_fontColor.a", 0.0f), eLine = V("TextLine", "scale.x", 1.0f);
			if (aFace > 0.01f)
				texto(F_NUMEROS, nombre, tx, cy + 14.0f * s * eFace, tam * eFace, glm::vec4(1.0f, 0.95f, 0.8f, aFace), NADA, glm::vec3(0.0f), 1, esp);
			if (aLine > 0.01f)
				texto(F_NUMEROS, nombre, tx, cy + 14.0f * s * eLine, tam * eLine, glm::vec4(glm::vec3(acento), aLine * 0.6f), glm::vec4(glm::vec3(acento), aLine),
				      glm::vec3(0.0f, 0.0f, 1.0f), 1, esp);
		}
	}

	// ------------------------------------------------------------- el cartel de turno (DuelTurnChange00/01 de MD)
	// Sus curvas: la banda img_TunChange_bg* entra desde x = -21.68 u (fuera de pantalla, a la izquierda) entre 0.517 y
	// 0.833 s y se apaga al final; las letras aparecen de a una (pistas de activacion: 1/60 s entre letras desde 0.617
	// s) y se apagan en 0.15 s; el grupo (GameObject) se corre de x -0.5 a 0 y al final cae (TU TURNO, y -5.4) o sube
	// (TURNO DEL RIVAL, y +5.4). La banda de MD mide 5.94 u (594 px a 100 px por unidad); aca 1300 px
	void dibujarCartelTurnoMD()
	{
		if (!ctl || !ctl->cartelTurnoVisible() || ctl->vista.pantalla != control::VistaDuelo::DUELO) return;
		const bool rival = ctl->cartelTurnoRival;
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline(rival ? "DuelTurnChange01" : "DuelTurnChange00");
		const float tt = ctl->tCartelTurno;
		const std::string banda = rival ? "img_TunChange_bg02" : "img_TunChange_bg01";
		auto V = [&](const char* obj, const char* attr, float def) { return tl ? tl->valor(obj, attr, tt, def) : def; };
		const float u = 1300.0f * s / 5.94f;
		const bool hayBanda = tl ? tl->activo(banda, tt, true) : tt > 0.517f;
		const float bandaX = V(banda.c_str(), "pos.x", 0.0f), alfaBanda = V(banda.c_str(), "m_Color.a", 1.0f);
		const float gx = V("GameObject", "pos.x", 0.0f), gy = V("GameObject", "pos.y", 0.0f);
		const float cx = (CAMPO_X * s + W) * 0.5f + gx * u, cy = H * 0.42f - gy * u;
		const glm::vec4 acento = rival ? CARMESI : CIAN;
		const float bw = 5.94f * u, bh = 1.82f * u * 0.62f;
		usar(setAtlas, false);
		if (hayBanda && alfaBanda > 0.01f) {
			const float bx = cx + bandaX * u;
			rect(glm::vec2(bx, cy), glm::vec2(bw * 1.1f, bh * 2.4f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(acento) * 0.22f * alfaBanda, 1.0f), NADA, 0.0f,
			     glm::vec4(0.0f));
			caja(bx - bw * 0.5f, cy - bh * 0.5f, bw, bh, 30.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.86f * alfaBanda), glm::vec4(glm::vec3(acento), alfaBanda), bh * 0.5f,
			     glm::vec4(1.8f * s, 1.3f, 0.35f, 20.0f * s));
			rect(glm::vec2(bx, cy + bh * 0.5f - 3.0f * s), glm::vec2(bw * 0.92f, 3.0f * s), 0.0f, 6.0f * s, LINEA, glm::vec4(glm::vec3(acento), alfaBanda), NADA, 0.0f,
			     glm::vec4(1.5f, 0.3f, 0, 0));
		}
		// las letras, de a una, con los tiempos de las pistas de activacion de MD (las de "TURNCHANGE": 10)
		static const char* const LETRA[] = { "Text (TMP)_T", "Text (TMP)_U", "Text (TMP)_R", "Text (TMP)_N", "Text (TMP)_C", "Text (TMP)_H", "Text (TMP)_A",
			                                 "Text (TMP)_N#2", "Text (TMP)_G", "Text (TMP)_E" };
		const std::string frase = idioma::tr(rival ? "TURNO DEL RIVAL" : "TU TURNO");
		const std::vector<int> cps = utf8(frase);
		const float tam = 64.0f * s, esp = 4.0f;
		const float ancho = medir(F_NUMEROS, frase, tam, esp);
		const float t0 = tl && tl->inicioActiva(LETRA[0]) >= 0.0f ? tl->inicioActiva(LETRA[0]) : 0.617f;
		const float t1 = tl && tl->inicioActiva(LETRA[1]) >= 0.0f ? tl->inicioActiva(LETRA[1]) : 0.633f;
		std::string pre;
		for (size_t i = 0; i < cps.size(); i++) {
			const int cp = cps[i];
			std::string letra;
			if (cp < 0x80) letra = (char)cp;
			else if (cp < 0x800) letra = std::string{ (char)(0xC0 | (cp >> 6)), (char)(0x80 | (cp & 0x3F)) };
			else letra = std::string{ (char)(0xE0 | (cp >> 12)), (char)(0x80 | ((cp >> 6) & 0x3F)), (char)(0x80 | (cp & 0x3F)) };
			const float lx = cx - ancho * 0.5f + medir(F_NUMEROS, pre, tam, esp);
			pre += letra;
			// la letra k de MD (las de mas alla de 10 siguen el mismo paso)
			const size_t k = std::min(i, (size_t)9);
			const float aparece = t0 + (t1 - t0) * (float)i;
			if (tt < aparece) continue;
			const float alfa = tl ? tl->valor(LETRA[k], "m_fontColor.a", tt, 1.0f) : 1.0f - suave(duracionTurnoMD(rival) - 0.2f, duracionTurnoMD(rival), tt);
			if (alfa <= 0.01f) continue;
			crudo = true;
			texto(F_NUMEROS, letra, lx, cy + tam * 0.36f, tam, glm::vec4(1.0f, 0.97f, 0.9f, alfa), glm::vec4(glm::vec3(acento), alfa), glm::vec3(0.6f, 0.0f, 1.0f), 0, esp);
			crudo = false;
		}
		if (tt > t0 && alfaBanda > 0.05f) {
			char b[64];
			snprintf(b, sizeof(b), idioma::tr("Turno %d"), ctl->cartelTurnoNum);
			texto(F_TITULO, b, cx, cy + bh * 0.5f + 30.0f * s, 22.0f * s, glm::vec4(glm::vec3(GRIS), alfaBanda), NADA, glm::vec3(0.0f), 1, 3.0f);
		}
	}
	float duracionTurnoMD(bool rival) const { return rival ? control::Mando::MD_TURNO_R : control::Mando::MD_TURNO_J; }

	// el cartel fijo TURNO DEL RIVAL (duelo_ui.h) entra con fundido despues del cartel de turno de MD, sin corte
	float alfaTurnoRivalMD()
	{
		const bool r = ctl && ctl->vista.turnoRival;
		if ((r && !turnoRivalAntes) || (ctl && ctl->cartelTurnoVisible())) tTurnoRivalUI = t;   // (tTurnoRivalUI: cuando empezo a verse)
		turnoRivalAntes = r;
		if (ctl && ctl->cartelTurnoVisible()) return 0.0f;
		return suave(0.0f, control::Mando::MD_VENTANA * 2.0f, t - tTurnoRivalUI);
	}

	// ------------------------------------------------------------------------- el cambio de LP (DuelLP de MD)
	// De DuelLPNear (DuelLP.countTime = 1 s, lineal: TransitionLPStep) y su LPCounterSub "ChangeValue": el valor (-1000)
	// aparece donde paso (ZoomIn: escala 0 -> 2, CubicIn 0.02 s), espera y se achica (ZoonOut: a 1.5 tras 0.56 s), sube
	// (Move0: a y +80 tras 0.2 s, 0.1 s), baja al panel (Move1: tras 0.2 s, 0.1 s) y se disuelve (Dissolve 0.2 s); al
	// llegar (OnEffectEnd, 1.2 s) el numero cuenta. El numero grande: CMainZoomIn (a 1.5 en 0.1 s) y CMainZoomOut 1.5 s
	// despues. Con danno, el panel se sacude (Content "Damage": su AnimationCurve, 0.5 s, hasta 40 x 20 px)
	const clipsmd::Prefab* prefabLP() const { return clipsmd::biblioteca().prefab("DuelLPNear"); }
	float inicioCuentaLP() const
	{
		const clipsmd::Prefab* p = prefabLP();
		if (!p) return 1.2f;
		const float t0 = p->inicioEnCadena("ChangeValue", "ZoomIn", "ChangeValue", "Dissolve");   // (Move1 llama a la vez Dissolve y OnEffectEnd)
		return t0 >= 0.0f ? t0 : 1.2f;
	}
	float duracionCuentaLP() const
	{
		const clipsmd::Prefab* p = prefabLP();
		return std::max(0.05f, p ? p->campo("DuelLP.countTime", 1.0f) : 1.0f);
	}
	// la sacudida del panel con danno (px de pantalla; y hacia abajo)
	glm::vec2 sacudidaLPMD(const Vida& vi) const
	{
		if (vi.cambio >= 0 || vi.tCambio > 1.0f) return glm::vec2(0.0f);
		const clipsmd::Prefab* p = prefabLP();
		const clipsmd::Tween* w = p ? p->tween("Content", "Damage", "TweenPositionTo") : nullptr;
		if (!w) return glm::vec2(sinf(vi.tCambio * 60.0f) * 4.0f * s * expf(-vi.tCambio * 4.0f), 0.0f);
		if (w->termino(vi.tCambio) || vi.tCambio > w->total()) return glm::vec2(0.0f);
		return glm::vec2(w->valor(vi.tCambio, 0), -w->valor(vi.tCambio, 1)) * s;   // (Unity: y hacia arriba)
	}
	// 0..1: cuanto del color de danno/cura lleva el numero (mientras cuenta; ToIdleStep lo vuelve al normal)
	float colorCuentaLPMD(const Vida& vi) const
	{
		const float t0 = inicioCuentaLP(), dur = duracionCuentaLP();
		return vi.tCambio >= t0 && vi.tCambio <= t0 + dur ? 1.0f : 0.0f;
	}
	// la escala del numero grande (CMainZoomIn -> CMainZoomOut)
	float zoomNumeroLPMD(const Vida& vi) const
	{
		const clipsmd::Prefab* p = prefabLP();
		const clipsmd::Tween* in = p ? p->tween("RectValueShow", "CMainZoomIn") : nullptr;
		const clipsmd::Tween* out = p ? p->tween("RectValueShow", "CMainZoomOut") : nullptr;
		const float dIn = in ? in->total() : 0.1f;
		const float alto = in && !in->hasta.empty() ? in->hasta[0] : 1.5f;
		if (vi.tCambio < dIn) return in ? in->valor(vi.tCambio, 0, 1.0f) : 1.0f + (alto - 1.0f) * vi.tCambio / dIn;
		const float tOut = vi.tCambio - dIn;
		if (!out) return tOut < 1.6f ? (tOut < 1.5f ? alto : alto + (1.0f - alto) * (tOut - 1.5f) / 0.1f) : 1.0f;
		return out->termino(tOut) ? 1.0f : out->valor(tOut, 0, alto);
	}
	// el valor que vuela del lugar del cambio al panel de LP
	void dibujarCambioLPMD()
	{
		if (!ctl || ctl->vista.pantalla != control::VistaDuelo::DUELO) return;
		const clipsmd::Prefab* p = prefabLP();
		const float tIn = 0.0f;
		const float tOut = p ? p->inicioEnCadena("ChangeValue", "ZoomIn", "ChangeValue", "ZoonOut") : 0.02f;
		const float tM0 = p ? p->inicioEnCadena("ChangeValue", "ZoomIn", "ChangeValue", "Move0") : 0.6f;
		const float tM1 = p ? p->inicioEnCadena("ChangeValue", "ZoomIn", "ChangeValue", "Move1") : 0.9f;
		const float tDis = inicioCuentaLP();
		auto tw = [&](const char* l) { return p ? p->tween("ChangeValue", l) : nullptr; };
		for (int lado = 0; lado < 2; lado++) {
			const Vida& vi = lado ? vidaR : vidaJ;
			if (vi.cambio == 0 || vi.tCambio > tDis + 0.25f) continue;
			const float tt = vi.tCambio;
			// escala: ZoomIn (0 -> 2) y ZoonOut (-> 1.5)
			float esc = 2.0f;
			if (const clipsmd::Tween* w = tw("ZoomIn")) esc = tt < tOut ? w->valor(tt - tIn, 0) : (w->hasta.empty() ? 2.0f : w->hasta[0]);
			if (const clipsmd::Tween* w = tw("ZoonOut"))
				if (tt >= tOut) esc = w->valor(tt - tOut, 0, esc);
			// posicion: del lugar del cambio (el centro de ese lado del campo) a +80 sobre el panel y al panel
			const glm::vec2 panel(COL_X * s + 200.0f * s, (lado ? MARGEN + 60.0f : 1080.0f - MARGEN - 60.0f) * s);
			// (rama classic-ps5-ataque4: como Joey, el numero no sale sobre el campo sino junto al marcador de LP de ese lado)
			const glm::vec2 arriba = panel - glm::vec2(0.0f, 80.0f * s);
			const glm::vec2 lugar = arriba + glm::vec2(140.0f * s, 0.0f);
			glm::vec2 pos = lugar;
			if (tt >= tM0) {
				const clipsmd::Tween* w = tw("Move0");
				const float k = w ? w->avance(tt - tM0) : glm::clamp((tt - tM0 - 0.2f) / 0.1f, 0.0f, 1.0f);
				pos = glm::mix(lugar, arriba, k);
			}
			if (tt >= tM1) {
				const clipsmd::Tween* w = tw("Move1");
				const float k = w ? w->avance(tt - tM1) : glm::clamp((tt - tM1 - 0.2f) / 0.1f, 0.0f, 1.0f);
				pos = glm::mix(arriba, panel, k);
			}
			float alfa = 1.0f;
			if (tt >= tDis) {
				const clipsmd::Tween* w = tw("Dissolve");
				alfa = w ? w->valor(tt - tDis, 0, 1.0f) : 1.0f - glm::clamp((tt - tDis) / 0.2f, 0.0f, 1.0f);
			}
			if (alfa <= 0.01f || esc <= 0.01f) continue;
			char b[16];
			snprintf(b, sizeof(b), "%+d", vi.cambio);
			const glm::vec4 col = vi.cambio < 0 ? glm::vec4(1.0f, 0.45f, 0.4f, alfa) : glm::vec4(0.55f, 0.9f, 1.0f, alfa);
			const glm::vec4 borde = vi.cambio < 0 ? glm::vec4(glm::vec3(CARMESI), alfa) : glm::vec4(glm::vec3(CIAN), alfa);
			const float tam = 30.0f * s * esc;
			texto(F_NUMEROS, b, pos.x, pos.y + tam * 0.36f, tam, col, borde, glm::vec3(0.8f, 0.0f, 1.0f), 1, 1.0f);
		}
	}

	// ---------------------------------------------------------------- carteles de inicio y de fin del duelo de MD
	// MD los dibuja con imagenes de letras en ingles (img_DueltexStart_d01..., img_text_Dueltextlose_d01...); aca son
	// nuestras letras (ES / EN) y cada una sigue la curva de la letra de MD que le toca (repartidas a lo largo de la palabra)
	static std::vector<std::string> letrasMD(const std::string& s8)
	{
		std::vector<std::string> r;
		for (size_t i = 0; i < s8.size();) {
			const unsigned char c = (unsigned char)s8[i];
			const size_t n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
			r.push_back(s8.substr(i, n));
			i += n;
		}
		return r;
	}
	// DuelTextStart: cada letra (D U E L) entra desde la izquierda (pos.x de -7 a 0 u) y se ajusta de escala (2.1 / 0.3 /
	// 1.3 / 0.6 -> 1 a los 1.4 s), aparece (m_Color.a) y al final todo el texto (DuelText) crece a 2.5 y se va (2.3-2.43 s)
	void dibujarCartelInicioMD()
	{
		if (!ctl || !ctl->cartelInicioVisible() || ctl->vista.pantalla != control::VistaDuelo::DUELO) return;
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline("DuelTextStart");
		const float tt = ctl->tCartelInicio, dur = ctl->durCartelInicio;
		static const char* const L[] = { "img_DueltexStart_d01", "img_DueltexStart_u01", "img_DueltexStart_e01", "img_DueltexStart_l01" };
		auto V = [&](const char* obj, const char* attr, float def) { return tl ? tl->valor(obj, attr, tt, def) : def; };
		const float grupo = V("DuelText", "scale.x", 1.0f + 1.5f * suave(dur - 0.13f, dur, tt));
		const float salida = 1.0f - suave(dur - 0.12f, dur, tt);
		const std::vector<std::string> letras = letrasMD(idioma::tr("¡DUELO!"));
		const float tam = 120.0f * s, esp = 6.0f;
		const float u = 110.0f * s;   // (px por unidad de Unity de las letras de MD)
		const float cx = (CAMPO_X * s + W) * 0.5f, cy = H * 0.44f;
		const float ancho = medir(F_NUMEROS, idioma::tr("¡DUELO!"), tam, esp);
		usar(setAtlas, false);
		const float aBanda = suave(0.3f, 0.9f, tt) * salida;
		rect(glm::vec2(cx, cy), glm::vec2(W * 0.9f, tam * 2.2f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(ORO) * 0.22f * aBanda, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		caja(cx - W * 0.33f, cy - tam * 0.62f, W * 0.66f, tam * 1.24f, 30.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.80f * aBanda), glm::vec4(glm::vec3(ORO), aBanda),
		     tam * 0.62f, glm::vec4(1.8f * s, 1.3f, 0.35f, 20.0f * s));
		std::string pre;
		for (size_t i = 0; i < letras.size(); i++) {
			const size_t k = std::min<size_t>(3, i * 4 / std::max<size_t>(1, letras.size()));
			const float dx = V(L[k], "pos.x", 0.0f) * u, esc = V(L[k], "scale.x", 1.0f);
			const float a = glm::clamp(V(L[k], "m_Color.a", suave(0.4f, 0.8f, tt)), 0.0f, 1.0f) * salida;
			const float lx = cx - ancho * 0.5f + medir(F_NUMEROS, pre, tam, esp);
			const float lw = medir(F_NUMEROS, letras[i], tam, esp);
			pre += letras[i];
			if (a <= 0.01f) continue;
			const float e = esc * grupo;
			const glm::vec2 c = glm::vec2(cx, cy) + (glm::vec2(lx + lw * 0.5f, cy) - glm::vec2(cx, cy)) * grupo + glm::vec2(dx, 0.0f);
			crudo = true;
			texto(F_NUMEROS, letras[i], c.x, c.y + tam * e * 0.36f, tam * e, glm::vec4(1.0f, 0.95f, 0.8f, a), glm::vec4(glm::vec3(ORO), a), glm::vec3(0.8f, 0.0f, 1.0f), 1, 0.0f);
			crudo = false;
		}
	}
	// el momento en que aparece la pantalla de resultado (el evento StartUI del Timeline de MD: 2.5 s al ganar, 1.3 s al
	// perder o empatar)
	float inicioResultadoMD(int ganador) const
	{
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline(ganador == 0 ? "DuelTextWin2" : ganador == 1 ? "DueltextLose" : "DueltextDraw");
		const float m = tl ? tl->marca("StartUI") : -1.0f;
		return m >= 0.0f ? m : ganador == 0 ? 2.5f : 1.3f;
	}
	// DuelTextWin2: el texto entra de escala 3 a 0.9 (0.63 s) y a 1 (1.77 s), aparece a los 0.37-0.43 s y su brillo a
	// los 1.4 s. DueltextLose / Draw: cada letra entra desde x -0.5 y aparece en 0.167 s, una tras otra (0.75 -> 1.17 s)
	void dibujarCartelFinMD(const control::VistaDuelo& vd)
	{
		const float tt = vd.t;
		const float tUI = inicioResultadoMD(vd.ganador);
		if (tt > tUI + 0.4f) return;
		const float salida = 1.0f - suave(tUI, tUI + 0.4f, tt);
		const bool gano = vd.ganador == 0, perdio = vd.ganador == 1;
		const glm::vec4 acento = gano ? ORO : perdio ? CARMESI : CIAN;
		const float cx = (CAMPO_X * s + W) * 0.5f, cy = H * 0.44f;
		const float tam = 120.0f * s, esp = 6.0f;
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f * suave(0.0f, 0.4f, tt) * salida), NADA, 0.0f, glm::vec4(0.0f));
		rect(glm::vec2(cx, cy), glm::vec2(W * 0.9f, tam * 2.4f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(acento) * 0.25f * salida, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		const std::string frase = idioma::tr(gano ? "¡VICTORIA!" : perdio ? "DERROTA" : "EMPATE");
		if (gano) {
			const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline("DuelTextWin2");
			auto V = [&](const char* obj, const char* attr, float def) { return tl ? tl->valor(obj, attr, tt, def) : def; };
			const bool activo = tl ? tl->activo("img_text_DuelTextWin_02", tt) : tt > 0.2f;
			const float e = V("img_text_DuelTextWin_02", "scale.x", 1.0f), a = glm::clamp(V("img_text_DuelTextWin_02", "m_Color.a", 1.0f), 0.0f, 1.0f) * salida;
			const float brillo = glm::clamp(V("img_text_DuelTextWin_01", "m_Color.a", 0.0f), 0.0f, 1.0f) * salida;
			if (activo && a > 0.01f) {
				texto(F_NUMEROS, frase, cx, cy + tam * e * 0.36f, tam * e, glm::vec4(1.0f, 0.95f, 0.8f, a), glm::vec4(glm::vec3(ORO), a), glm::vec3(0.8f, 0.0f, 1.0f), 1, esp);
				if (brillo > 0.01f)
					texto(F_NUMEROS, frase, cx, cy + tam * e * 0.36f, tam * e, glm::vec4(glm::vec3(ORO), brillo * 0.5f), glm::vec4(1.0f, 0.95f, 0.7f, brillo), glm::vec3(1.0f, 0.0f, 1.0f), 1,
					      esp);
			}
			return;
		}
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline(perdio ? "DueltextLose" : "DueltextDraw");
		static const char* const LP[] = { "img_text_Dueltextlose_d02.png", "img_text_Dueltextlose_e02.png", "img_text_Dueltextlose_f02.png",
			                              "img_text_Dueltextlose_e02_2.png", "img_text_Dueltextlose_a02.png", "img_text_Dueltextlose_t02.png" };
		static const char* const LE[] = { "img_text_Dueltextdraw_d02.png", "img_text_Dueltextdraw_r02.png", "img_text_Dueltextdraw_a02.png", "img_text_Dueltextdraw_w02.png" };
		const char* const* L = perdio ? LP : LE;
		const size_t nL = perdio ? 6 : 4;
		const std::vector<std::string> letras = letrasMD(frase);
		const float ancho = medir(F_NUMEROS, frase, tam, esp);
		const float u = 110.0f * s;
		std::string pre;
		for (size_t i = 0; i < letras.size(); i++) {
			const size_t k = std::min(nL - 1, i * nL / std::max<size_t>(1, letras.size()));
			const float dx = tl ? tl->valor(L[k], "pos.x", tt, 0.0f) * u : -0.5f * u * (1.0f - suave(0.75f, 1.35f, tt));
			const float a = glm::clamp(tl ? tl->valor(L[k], "m_Color.a", tt, 1.0f) : suave(0.75f, 0.92f, tt), 0.0f, 1.0f) * salida;
			const float lx = cx - ancho * 0.5f + medir(F_NUMEROS, pre, tam, esp);
			pre += letras[i];
			if (a <= 0.01f) continue;
			crudo = true;
			texto(F_NUMEROS, letras[i], lx + dx, cy + tam * 0.36f, tam, glm::vec4(1.0f, 0.92f, 0.9f, a), glm::vec4(glm::vec3(acento), a), glm::vec3(0.8f, 0.0f, 1.0f), 0, esp);
			crudo = false;
		}
	}
