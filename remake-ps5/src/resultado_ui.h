// Yugioh Classic Remaster (rama classic-resultado): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// La pantalla de resultado (resultado::resumen(), lo llena resultado_juego.h): VICTORIA / DERROTA / EMPATE con el
// motivo, el resumen del duelo (turnos, danno, cartas usadas, MVP), lo desbloqueado (personaje, cartas firma, saga,
// nivel, siguiente rival) y las opciones Revancha / Elegir otro rival / Menu principal.

private:
	void dibujarResultado(const control::VistaDuelo& vd)
	{
		const resultado::Resumen& r = resultado::resumen();
		const bool en = idioma::ingles();
		// rama classic-md-fases: primero el cartel de fin de MD (DuelTextWin2 / DueltextLose / DueltextDraw) y la pantalla
		// aparece en su evento StartUI (2.5 s al ganar, 1.3 s al perder o empatar)
		const float tFin = inicioResultadoMD(vd.ganador);
		dibujarCartelFinMD(vd);
		// rama classic-md-entradas: en el StartUI corre DuelEndTransition (2.0 s, menu_md_ui.h): el negro se cierra
		// (TransIn 0.717 s), la pantalla se arma detras y se abre (TransOut hasta 1.25 s)
		const float tUI = tFin + esperaTransicionFinMD();
		if (vd.t < tUI) {
			dibujarTransicionFinMD(vd.t - tFin);
			return;
		}
		const float a = suave(tUI, tUI + 0.25f, vd.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float cx = (CAMPO_X * s + W) * 0.5f;
		const bool gano = vd.ganador == 0, empate = vd.ganador != 0 && vd.ganador != 1;
		const glm::vec4 acento = gano ? ORO : empate ? CIAN : CARMESI;
		// velo, halo y panel
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.01f, 0.015f, 0.03f, 0.84f)), NADA, 0.0f, glm::vec4(0.0f));
		rz.tapar();   // rama classic-pc
		const float pw = 1120.0f * s, ph = 860.0f * s;
		const float px = cx - pw * 0.5f, py = H * 0.5f - ph * 0.5f + (1.0f - a) * 20.0f * s;
		rect(glm::vec2(cx, H * 0.5f), glm::vec2(pw * 1.3f, ph * 1.25f), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(acento) * 0.12f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		caja(px, py, pw, ph, 30.0f * s, PANEL, A(VIDRIO), A(acento), 22.0f * s, glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s));

		// titulo y motivo
		const float pulso = 1.0f + 0.04f * sinf(t * 3.0f);
		texto(F_NUMEROS, idioma::tr(gano ? "¡VICTORIA!" : vd.ganador == 1 ? "DERROTA" : "EMPATE"), cx, py + 130.0f * s, 86.0f * s * pulso, A(BLANCO), A(acento),
		      glm::vec3(0.8f, 0.0f, 1.0f), 1, 3.0f);
		const char* motivo = resultado::textoMotivo(r.motivo, vd.ganador);
		if (motivo[0])
			texto(F_TITULO, motivo, cx, py + 196.0f * s, 28.0f * s, A(glm::mix(BLANCO, acento, 0.35f)), NADA, glm::vec3(0.0f), 1, 1.0f);
		const std::string& nom = en && !vd.rivalEn.empty() ? vd.rivalEn : vd.rivalEs;
		texto(F_TITULO, std::string(idioma::tr("Rival")) + ": " + nom + (r.enLinea ? std::string("  ·  ") + idioma::tr("En línea") : std::string("  ·  ") + ia::textoIA("ia.nivel", en) + " " + std::to_string(vd.nivel)), cx, py + 238.0f * s,
		      21.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);

		// resumen: cuatro casillas
		const char* const ROT[] = { idioma::t("Turnos", "Turns"), idioma::t("Daño hecho", "Damage dealt"), idioma::t("Daño recibido", "Damage taken"),
			                        idioma::t("Cartas usadas", "Cards used") };
		const int VAL[] = { r.turnos, r.dannoHecho, r.dannoRecibido, r.cartasUsadas };
		const float cw = 230.0f * s, ch = 104.0f * s, sep = 22.0f * s, cy0 = py + 272.0f * s;
		const float cx0 = cx - (4.0f * cw + 3.0f * sep) * 0.5f;
		for (int k = 0; k < 4; k++) {
			const float x = cx0 + (float)k * (cw + sep);
			caja(x, cy0, cw, ch, 10.0f * s, PANEL, A(glm::vec4(0.04f, 0.06f, 0.1f, 0.75f)), A(glm::vec4(glm::vec3(CIAN) * 0.55f, 1.0f)), 14.0f * s,
			     glm::vec4(1.0f * s, 0.8f, 0.0f, 0.0f));
			texto(F_NUMEROS, std::to_string(VAL[k]), x + cw * 0.5f, cy0 + 56.0f * s, 40.0f * s, A(k == 1 ? ORO : k == 2 ? glm::vec4(1.0f, 0.55f, 0.5f, 1.0f) : BLANCO), NADA,
			      glm::vec3(0.3f, 0.0f, 0.4f), 1, 1.0f);
			texto(F_TITULO, ROT[k], x + cw * 0.5f, cy0 + 88.0f * s, 18.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
		}
		float y = cy0 + ch + 48.0f * s;
		if (!r.mvp.empty()) {
			texto(F_TITULO, std::string(idioma::t("Carta MVP", "MVP card")) + ": " + nombreCarta(r.mvp) + "  ·  " + std::to_string(r.mvpDanno) + " " +
			                    idioma::t("de daño", "damage"),
			      cx, y, 23.0f * s, A(glm::vec4(1.0f, 0.92f, 0.7f, 1.0f)), NADA, glm::vec3(0.0f), 1, 1.0f);
			y += 38.0f * s;
		}
		if (!r.cartaClave.empty() && r.cartaClave != r.mvp) {
			texto(F_TITULO, std::string(idioma::t("Carta clave", "Key card")) + ": " + nombreCarta(r.cartaClave), cx, y, 21.0f * s, A(CIAN), NADA, glm::vec3(0.0f), 1, 1.0f);
			y += 36.0f * s;
		}

		// lo desbloqueado
		y += 16.0f * s;
		rect(glm::vec2(cx, y - 22.0f * s), glm::vec2(pw * 0.7f, 2.0f * s), 0.0f, 2.0f * s, LINEA, A(glm::vec4(glm::vec3(acento) * 0.7f, 0.8f)), NADA, 0.0f,
		     glm::vec4(1.0f, 0.2f, 0.0f, 0.0f));
		if (gano && vd.personajeNuevo) {
			dibujarResultadoPersonaje(vd, cx, y + 10.0f * s, a);   // (rama classic-personajes: personaje vencido, firma y saga nueva)
			y += (vd.sagaNueva ? 80.0f : 44.0f) * s;
			if (!r.firmaNuevas.empty()) {
				std::string l;
				for (size_t k = 0; k < r.firmaNuevas.size() && k < 4; k++) l += (k ? "  ·  " : "") + nombreCarta(r.firmaNuevas[k]);
				if (r.firmaNuevas.size() > 4) l += "  ·  +" + std::to_string(r.firmaNuevas.size() - 4);
				texto(F_TEXTO, l, cx, y, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 1, 0.0f);
				y += 38.0f * s;
			}
		} else if (gano && vd.subeNivel) {
			texto(F_NUMEROS, std::string(idioma::tr("NIVEL DESBLOQUEADO")) + ": " + std::to_string(vd.nivelMax), cx, y + 10.0f * s, 30.0f * s, A(ORO),
			      A(glm::vec4(0.5f, 0.3f, 0.05f, 1.0f)), glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
			y += 48.0f * s;
		} else if (gano && vd.modo == control::VistaDuelo::LIBRE && vd.nivelMax >= vd.niveles && vd.nivel >= vd.niveles) {
			texto(F_NUMEROS, idioma::tr("¡ERES EL CAMPEÓN!"), cx, y + 10.0f * s, 30.0f * s, A(ORO), NADA, glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
			y += 48.0f * s;
		}
		if (!r.siguienteEs.empty()) {
			const std::string& sig = en && !r.siguienteEn.empty() ? r.siguienteEn : r.siguienteEs;
			texto(F_TITULO, std::string(idioma::t("Siguiente rival", "Next opponent")) + ": " + sig, cx, y + 4.0f * s, 23.0f * s, A(CIAN), NADA, glm::vec3(0.0f), 1, 1.0f);
		}
		if (r.enLinea) dibujarResultadoEnLinea(vd, cx, py + ph - 200.0f * s, a);   // rama classic-en-linea: ELO y revancha (en_linea_ui.h)
		else texto(F_TITULO, std::string(idioma::tr("Victorias")) + " " + std::to_string(vd.victorias) + "   ·   " + idioma::tr("Derrotas") + " " + std::to_string(vd.derrotas), cx,
		      py + ph - 168.0f * s, 18.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);

		// opciones: Revancha / Elegir otro rival / Menu principal
		const float bw = 300.0f * s, bh = 58.0f * s, bsep = 28.0f * s, by = py + ph - 140.0f * s;
		const float bx0 = cx - (3.0f * bw + 2.0f * bsep) * 0.5f;
		for (int k = 0; k < resultado::NUM_OPCIONES; k++) {
			const bool sel = k == r.opcion && vd.t > tUI + 0.8f;
			const bool apagada = k == resultado::O_REVANCHA && !r.revancha;
			const float bx = bx0 + (float)k * (bw + bsep);
			if (vd.t > tUI + 0.8f) zLista(bx, by, bw, bh, k, r.opcion, true);   // rama classic-pc
			caja(bx, by, bw, bh, 8.0f * s, PANEL, A(sel ? glm::vec4(0.32f, 0.23f, 0.05f, 0.8f) : glm::vec4(glm::vec3(VIDRIO), 0.6f)),
			     A(sel ? ORO : glm::vec4(glm::vec3(CIAN) * (apagada ? 0.3f : 0.7f), 1.0f)), bh * 0.5f, glm::vec4((sel ? 1.6f : 1.1f) * s, 1.1f, 0.0f, 0.0f));
			texto(F_TITULO, resultado::textoOpcion(k, r.enLinea), bx + bw * 0.5f, by + bh * 0.5f + 9.0f * s, 25.0f * s,
			      A(apagada ? GRIS : sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : BLANCO), NADA, glm::vec3(0.0f), 1, 1.0f);
		}
		// ayuda del mando
		float hx = cx - 250.0f * s;
		const float hy = py + ph - 42.0f * s;
		auto ayuda = [&](const std::string& b, const char* txt) {
			hx += iconoBoton(glm::vec2(hx + 14.0f * s, hy), b, 14.0f * s, a) + 12.0f * s;
			hx += texto(F_TITULO, txt, hx, hy + 8.0f * s, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f) + 34.0f * s;
		};
		ayuda("CRUZ", idioma::tr("Opción"));
		ayuda("X", idioma::tr("Elegir"));
		ayuda("O", idioma::tr("Menú principal"));
		dibujarTransicionFinMD(vd.t - tFin);   // (rama classic-md-entradas: el TransOut se abre sobre la pantalla)
	}
