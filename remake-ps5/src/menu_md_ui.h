// Yugioh Classic Remaster (rama classic-md-entradas): se incluye DENTRO de interfaz::Interfaz. Tres detalles visuales de
// Yu-Gi-Oh! Master Duel con sus datos (docs/md_entradas.md):
//  1. El MENU DEL DUELO (el engranaje: DuelHUD.OpenSettingMenu -> "Setting/SettingMenuViewController", vista
//     SettingMenuUI). El CONTENIDO es el nuestro (Continuar, Opciones, Construir mazo, Abandonar...: pedido del usuario);
//     de MD vienen la apertura y el cierre (Push / Pop), el cursor y sus esquinas y el pulso del elegido:
//       Push: velo (FullScreen) alfa 0 -> 1 lineal 0.3 s; ventana (SettingMenuArea) alfa 0 -> 1 en 0.3 s tras 0.15 s,
//             desde x +240, z +360 y giro Y -15 grados a su lugar con QuartOut 0.5 s tras 0.15 s; el titulo (TitleArea)
//             baja de y +120 con QuartOut 0.2 s tras 0.4 s.
//       Pop:  velo a 0 en 0.2 s; ventana alfa 1 -> 0, a x +240 / z +360 / -15 grados con CubicIn 0.2 s; titulo CubicIn 0.1 s.
//       Elegido: SelectCursor alfa 1 -> 0.2 y Over 1 -> 0.4 (CubicIn, PingPongLoop 1 s); las esquinas
//             (GUI_CommonSelectCursor_CornerAll) se cierran de 16 px a 0 con QuartOut 0.2 s al cambiar de opcion.
//     Los Tween salen de md_ui/SettingMenuUI.mdui (exportar_clips.py); sin el archivo, los mismos valores de aca.
//     Sonido al abrir: SE_DUEL_SELECT (OpenSettingMenu); al moverse y cerrar, los del menu de MD (ya estaban).
//  2. La TRANSICION del fin del duelo (DuelEndTransition, 2.0 s): en el StartUI del cartel de fin, un negro con un
//     agujero redondo (material SoloTrans_fxp_BK_hole) se cierra en 0.717 s (TransIn), el resultado se arma detras
//     (Wait) y se abre hasta 1.25 s (TransOut), con la curva exacta del radio (md_ui/DuelEndTransition.mdtl).
//  3. Los LP BAJOS (DuelLP.IdleStep): con 1..1000 LP el numero late entre blanco y rojo, color (1, k, k) con
//     k = (sin(t * 6 / EmergencyPeriod) + 1) / 2 y EmergencyPeriod = 0.5 s (un latido cada ~0.52 s).

private:
	// ---------------------------------------------------------------------------------------- 1. menu del duelo
	float tMenuMD{ 99.0f };          // s desde que se abrio
	float tCierreMenuMD{ 99.0f };    // s desde que se cerro (el Pop se sigue viendo)
	float tOpcionMD{ 99.0f };        // s desde que cambio la opcion (las esquinas se cierran)
	bool menuAbiertoMD{ false };
	int opcionMD{ -1 };
	std::vector<std::string> opcionesCopiaMD;   // lo que se ve durante el Pop
	int opcionCopiaMD{ 0 };

	clipsmd::Tween tweenMenuMD(const char* obj, const char* label, const char* tipo, int easing, float dur, float demora, std::vector<float> desde,
	                           std::vector<float> hasta) const
	{
		if (const clipsmd::Prefab* p = clipsmd::biblioteca().prefab("SettingMenuUI"))
			if (const clipsmd::Tween* w = p->tween(obj, label, tipo)) return *w;
		clipsmd::Tween w;
		w.obj = obj, w.label = label, w.tipo = tipo, w.easing = easing, w.estilo = clipsmd::ONCE, w.dur = dur, w.demora = demora;
		w.desde = std::move(desde);
		w.hasta = std::move(hasta);
		return w;
	}
	// lo que mueve la ventana en este cuadro
	struct AnimMenuMD { float velo{ 1.0f }, alfa{ 1.0f }, dx{ 0.0f }, escala{ 1.0f }, ancho{ 1.0f }, dyTitulo{ 0.0f }; };
	AnimMenuMD animMenuMD() const
	{
		using namespace clipsmd;
		AnimMenuMD a;
		const bool cerrando = !menuAbiertoMD;
		const float tt = cerrando ? tCierreMenuMD : tMenuMD;
		const char* L = cerrando ? "Pop" : "Push";
		const Tween velo = cerrando ? tweenMenuMD("FullScreen", "Pop", "TweenAlphaTo", LINEAR, 0.2f, 0.0f, {}, { 0.0f })
		                            : tweenMenuMD("FullScreen", "Push", "TweenAlpha", LINEAR, 0.3f, 0.0f, { 0.0f }, { 1.0f });
		const Tween alfa = tweenMenuMD("SettingMenuArea", L, "TweenAlpha", LINEAR, cerrando ? 0.2f : 0.3f, cerrando ? 0.0f : 0.15f, { cerrando ? 1.0f : 0.0f },
		                               { cerrando ? 0.0f : 1.0f });
		const Tween pos = tweenMenuMD("SettingMenuArea", L, "TweenPosition", cerrando ? CUBIC_IN : QUART_OUT, cerrando ? 0.2f : 0.5f, cerrando ? 0.0f : 0.15f,
		                              cerrando ? std::vector<float>{ 0.0f, 0.0f, 0.0f } : std::vector<float>{ 240.0f, 0.0f, 360.0f },
		                              cerrando ? std::vector<float>{ 240.0f, 0.0f, 360.0f } : std::vector<float>{ 0.0f, 0.0f, 0.0f });
		const Tween giro = tweenMenuMD("SettingMenuArea", L, "TweenRotation", cerrando ? CUBIC_IN : QUART_OUT, cerrando ? 0.2f : 0.5f, cerrando ? 0.0f : 0.15f,
		                               cerrando ? std::vector<float>{ 0.0f, 0.0f, 0.0f } : std::vector<float>{ 0.0f, -15.0f, 0.0f },
		                               cerrando ? std::vector<float>{ 0.0f, -15.0f, 0.0f } : std::vector<float>{ 0.0f, 0.0f, 0.0f });
		const Tween titulo = tweenMenuMD("TitleArea", L, "TweenPosition", cerrando ? CUBIC_IN : QUART_OUT, cerrando ? 0.1f : 0.2f, cerrando ? 0.0f : 0.4f,
		                                 cerrando ? std::vector<float>{ 0.0f, 0.0f, 0.0f } : std::vector<float>{ 0.0f, 120.0f, 0.0f },
		                                 cerrando ? std::vector<float>{ 0.0f, 120.0f, 0.0f } : std::vector<float>{ 0.0f, 0.0f, 0.0f });
		a.velo = glm::clamp(velo.valor(tt, 0, 1.0f), 0.0f, 1.0f);
		a.alfa = glm::clamp(alfa.valor(tt, 0, 1.0f), 0.0f, 1.0f);
		a.dx = pos.valor(tt, 0) * s;
		a.escala = 1.0f / (1.0f + pos.valor(tt, 2) / 1200.0f);   // (z de uGUI -> escala por perspectiva)
		a.ancho = cosf(glm::radians(giro.valor(tt, 1)));         // (giro Y: la ventana se ve de costado)
		a.dyTitulo = -titulo.valor(tt, 1) * s;                    // (uGUI: y hacia arriba)
		return a;
	}
	// cada cuadro, desde dibujarPausa: abre/cierra, la opcion elegida y los relojes. false si no hay que dibujar nada
	bool avanzarMenuMD()
	{
		const bool abierto = ctl && ctl->vista.pausa && !ctl->vista.pausaOpciones.empty();
		const float dt = dtCuadro;
		if (abierto && !menuAbiertoMD) tMenuMD = 0.0f, opcionMD = -1;
		if (!abierto && menuAbiertoMD) tCierreMenuMD = 0.0f;
		menuAbiertoMD = abierto;
		tMenuMD += dt;
		tCierreMenuMD += dt;
		tOpcionMD += dt;
		if (abierto) {
			opcionesCopiaMD = ctl->vista.pausaOpciones;
			if (ctl->vista.pausaOpcion != opcionMD) tOpcionMD = 0.0f;
			opcionMD = ctl->vista.pausaOpcion;
			opcionCopiaMD = opcionMD;
			return true;
		}
		const clipsmd::Tween pop = tweenMenuMD("SettingMenuArea", "Pop", "TweenAlpha", clipsmd::LINEAR, 0.2f, 0.0f, { 1.0f }, { 0.0f });
		return ctl && !opcionesCopiaMD.empty() && tCierreMenuMD < pop.total();
	}
	// el elegido: pulso del cursor y de la capa Over, y las esquinas que se cierran
	void cursorMenuMD(float x, float y, float w, float h, float alfa)
	{
		using namespace clipsmd;
		const Tween cur = tweenMenuMD("SettingMenuArea/ButtonGroup/SurrenderButton/SelectCursor", "select", "TweenAlpha", CUBIC_IN, 1.0f, 0.0f, { 1.0f }, { 0.2f });
		const Tween over = tweenMenuMD("SettingMenuArea/ButtonGroup/SurrenderButton/Over", "select", "TweenAlpha", CUBIC_IN, 1.0f, 0.0f, { 1.0f }, { 0.4f });
		const Tween esq = tweenMenuMD("SettingMenuArea/ButtonGroup/SurrenderButton/SelectCursor/Corner", "select", "TweenSize", QUART_OUT, 0.2f, 0.0f, { 16.0f, 16.0f },
		                              { 0.0f, 0.0f });
		Tween curL = cur, overL = over;
		curL.estilo = overL.estilo = PING_PONG_LOOP;   // (el estilo 3 de MD; el respaldo lo necesita)
		const float aCur = curL.valor(tMenuMD, 0, 1.0f), aOver = overL.valor(tMenuMD, 0, 1.0f);
		const float abre = esq.valor(tOpcionMD, 0) * s;
		usar(setAtlas, false);
		caja(x, y, w, h, 6.0f * s, PANEL, glm::vec4(0.32f, 0.23f, 0.05f, 0.6f * (0.55f + 0.45f * aOver) * alfa), glm::vec4(glm::vec3(ORO), aCur * alfa), h * 0.5f,
		     glm::vec4(1.2f * s, 1.1f, 0.0f, 0.0f));
		// las esquinas de MD (su sprite, teñido de oro); sin el atlas, cuatro trazos
		const glm::vec2 c(x + w * 0.5f, y + h * 0.5f), tam(w + 2.0f * abre + 10.0f * s, h + 2.0f * abre + 10.0f * s);
		if (!spriteMD("GUI_CommonSelectCursor_CornerAll", c, tam, glm::vec4(glm::vec3(ORO) * 1.2f, alfa * (0.6f + 0.4f * aCur)))) {
			for (int k = 0; k < 4; k++) {
				const glm::vec2 q = c + glm::vec2((k & 1) ? 1.0f : -1.0f, (k & 2) ? 1.0f : -1.0f) * tam * 0.5f;
				rect(q, glm::vec2(18.0f * s, 3.0f * s), 0.0f, 2.0f * s, LINEA, glm::vec4(glm::vec3(ORO), alfa), NADA, 0.0f, glm::vec4(1.5f, 0.1f, 0.0f, 0.0f));
				rect(q, glm::vec2(18.0f * s, 3.0f * s), 1.5708f, 2.0f * s, LINEA, glm::vec4(glm::vec3(ORO), alfa), NADA, 0.0f, glm::vec4(1.5f, 0.1f, 0.0f, 0.0f));
			}
		}
		usar(setAtlas, false);
	}

	// ------------------------------------------------------------------------------- 2. transicion del fin del duelo
	// el resultado se arma detras del negro: aparece en TransIn (0.717 s del Timeline)
	float esperaTransicionFinMD() const
	{
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline("DuelEndTransition");
		return tl ? 0.7167f * std::min(1.0f, tl->duracion / 1.25f) : 0.7167f;
	}
	// el radio del agujero (curva del material, 3.67 abierto -> 0.15 cerrado -> 3.70), muestreada cada 1/30 s (respaldo)
	static float radioTransicionFinMD(float u)
	{
		static const float R[] = { 3.671f, 2.391f, 1.613f, 1.216f, 1.059f, 0.939f, 0.829f, 0.730f, 0.639f, 0.558f, 0.485f, 0.421f, 0.364f,
			                       0.315f, 0.273f, 0.238f, 0.210f, 0.187f, 0.170f, 0.159f, 0.152f, 0.150f, 0.150f, 0.155f, 0.172f, 0.202f,
			                       0.250f, 0.316f, 0.403f, 0.514f, 0.651f, 0.816f, 1.013f, 1.247f, 1.561f, 1.985f, 2.546f, 3.272f, 3.705f };
		const float k = glm::clamp(u, 0.0f, 1.25f) * 30.0f;
		const int i = std::min((int)k, 37);
		return R[i] + (R[i + 1] - R[i]) * (k - (float)i);
	}
	// tr: s desde el StartUI del cartel de fin
	void dibujarTransicionFinMD(float tr)
	{
		if (tr < 0.0f || tr > 1.25f) return;
		const clipsmd::Timeline* tl = clipsmd::biblioteca().timeline("DuelEndTransition");
		const float r = tl ? tl->valor("BG", "attr2170181182", tr, radioTransicionFinMD(tr)) : radioTransicionFinMD(tr);
		const float abierto = glm::clamp(r / 3.671f, 0.0f, 1.2f);
		const glm::vec2 c((CAMPO_X * s + W) * 0.5f, H * 0.5f);
		const float rad = abierto * 0.5f * sqrtf(W * W + H * H) * 1.08f;   // (3.67 = la pantalla entera a la vista)
		const float lado = rad / 0.9f * 2.0f;                              // (md_agujero: borde suave entre 0.8 y 1.0 del radio)
		usar(setAtlas, false);
		if (spriteEntradaMD("md_agujero", c, glm::vec2(lado), glm::vec4(1.0f))) {
			const float x0 = c.x - lado * 0.5f, x1 = c.x + lado * 0.5f, y0 = c.y - lado * 0.5f, y1 = c.y + lado * 0.5f;
			const glm::vec4 negro(0.0f, 0.0f, 0.0f, 1.0f);
			if (y0 > 0.0f) caja(0.0f, 0.0f, W, y0 + 1.0f, 0.0f, PANEL, negro, NADA, 0.0f, glm::vec4(0.0f));
			if (y1 < H) caja(0.0f, y1 - 1.0f, W, H - y1 + 1.0f, 0.0f, PANEL, negro, NADA, 0.0f, glm::vec4(0.0f));
			if (x0 > 0.0f) caja(0.0f, std::max(y0, 0.0f), x0 + 1.0f, std::min(y1, H) - std::max(y0, 0.0f), 0.0f, PANEL, negro, NADA, 0.0f, glm::vec4(0.0f));
			if (x1 < W) caja(x1 - 1.0f, std::max(y0, 0.0f), W - x1 + 1.0f, std::min(y1, H) - std::max(y0, 0.0f), 0.0f, PANEL, negro, NADA, 0.0f, glm::vec4(0.0f));
			// (nuestro toque: el borde del agujero con una luz dorada)
			spriteEntradaMD("md_anillo_luz", c, glm::vec2(rad * 2.0f / 0.9f), glm::vec4(glm::vec3(ORO) * 1.3f, 0.55f * (1.0f - glm::clamp(abierto, 0.0f, 1.0f))));
		} else {   // (sin el sprite: el mismo tiempo con un fundido a negro)
			caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f - glm::clamp(abierto, 0.0f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		}
		usar(setAtlas, false);
	}

	// ------------------------------------------------------------------------------------------------ 3. LP bajos
	// < 0: sin aviso; si no, k de DuelLP.IdleStep (0 rojo .. 1 blanco)
	float emergenciaLPMD(const Vida& vi) const
	{
		if (vi.valor <= 0 || vi.valor > 1000 || vi.mostrado > 1000.5f) return -1.0f;
		const clipsmd::Prefab* p = clipsmd::biblioteca().prefab("DuelLPNear");
		const float periodo = std::max(0.05f, p ? p->campo("DuelLP.EmergencyPeriod", 0.5f) : 0.5f);
		return (sinf(t * 6.0f / periodo) + 1.0f) * 0.5f;
	}
