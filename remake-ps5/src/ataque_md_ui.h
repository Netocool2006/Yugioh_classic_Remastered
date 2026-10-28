// rama classic-ps5-ataque (se incluye DENTRO de la clase Interfaz): el marco rojo del danno de Master Duel
// (DuelHUD.ShowDamageFrame, prefab DamageFrame: alfa 0 -> 1 en 0.1 s -> 0 a 1.0 s, color (0.61, 0.09, 0.09)). MD lo
// muestra cuando el danno del combate es del jugador (EffectTaskBattleRun.AttackMoveStep: Engine.IsMyself(dstTeam));
// con el ataque directo del rival es el golpe que "llega a la pantalla". Ver ataque_md.h.

	void dibujarMarcoDannoMD()
	{
		if (golpe.t < G_LP || golpe.danio <= 0 || !golpe.alJugador || golpe.objetivo < 0) return;
		const float a = ataquemd::alfaMarcoDanno(golpe.t - G_LP);   // (ataque4: con la bajada de LP, como Joey)
		if (a <= 0.0f) return;
		const glm::vec3 c(ataquemd::MARCO_COLOR[0], ataquemd::MARCO_COLOR[1], ataquemd::MARCO_COLOR[2]);
		// el marco: un degradado suave del borde hacia el centro (16 capas finas cada vez mas angostas; con 4 capas gruesas se
		// veian franjas, parecia un fallo de la pantalla)
		const int N = 16;
		for (int k = 0; k < N; k++) {
			const float g = 0.12f * H * (1.0f - (float)k / (float)N);   // ancho de la capa
			const glm::vec4 col(c, a * 0.07f);
			caja(0.0f, 0.0f, W, g, 0.0f, PANEL, col, NADA, 0.0f, glm::vec4(0.0f));
			caja(0.0f, H - g, W, g, 0.0f, PANEL, col, NADA, 0.0f, glm::vec4(0.0f));
			caja(0.0f, g, g, H - 2.0f * g, 0.0f, PANEL, col, NADA, 0.0f, glm::vec4(0.0f));
			caja(W - g, g, g, H - 2.0f * g, 0.0f, PANEL, col, NADA, 0.0f, glm::vec4(0.0f));
		}
	}

	// rama classic-ps5-ataque4 (el usuario: "encima de las cartas"): el calculo de danno sobre las cartas que chocan, despues
	// del golpe y antes de que bajen los LP: el ATK del atacante, el ATK o DEF del objetivo y la diferencia en rojo entre los
	// dos. Va junto al borde de cada carta que da a la camara (no la tapa a ella ni a su figura). En el directo: el ATK y la
	// diferencia (todo el ATK) en el punto del golpe. Un solo numero de danno en el marcador de LP (fases_md_ui.h)
	void dibujarCalculoMD()
	{
		if (golpe.t < 0.0f || golpe.valorA < 0 || golpe.atacante < 0) return;
		const float t = golpe.t;
		const float a = suave(G_IMPACTO + 0.10f, G_IMPACTO + 0.30f, t) * (1.0f - suave(G_LP + 0.35f, G_LP + 0.65f, t));
		if (a <= 0.01f) return;
		const glm::vec3 alFrente(0.0f, 0.05f, 0.62f * CARTA_ALTO);   // (la camara mira desde +z: el borde de la carta hacia ella)
		auto enPantalla = [&](glm::vec3 p) { return aPantalla(proyectar(p)); };
		const glm::vec4 blanco(1.0f, 1.0f, 1.0f, a), borde(0.05f, 0.05f, 0.08f, a);
		const float tn = 46.0f * s, tt = 24.0f * s;
		auto etiqueta = [&](glm::vec2 p, const char* tag, int valor) {
			texto(F_TITULO, tag, p.x, p.y - tn * 0.55f, tt, glm::vec4(0.85f, 0.9f, 1.0f, a), borde, glm::vec3(0.6f, 0.0f, 1.0f), 1);
			texto(F_NUMEROS, std::to_string(valor), p.x, p.y + tn * 0.35f, tn, blanco, borde, glm::vec3(0.6f, 0.0f, 1.0f), 1);
		};
		const glm::vec2 pa = enPantalla(centroZona(golpe.atacante) + alFrente);
		etiqueta(pa, "ATK", golpe.valorA);
		glm::vec2 po;
		int dif;
		if (golpe.directo) {
			po = enPantalla(golpe.punto);
			dif = golpe.valorA;
		} else {
			po = enPantalla(centroZona(golpe.objetivo) + alFrente);
			etiqueta(po, golpe.defensaO ? "DEF" : "ATK", golpe.valorO);
			dif = abs(golpe.valorA - golpe.valorO);
		}
		// la diferencia en rojo (como el numero de danno de MD), a la derecha del valor del objetivo: sin tapar ningun numero
		const glm::vec2 m = po + glm::vec2(70.0f * s + 30.0f * s, -tn * 0.1f);
		const float pulso = 1.0f + 0.25f * expf(-glm::max(0.0f, t - (G_IMPACTO + 0.2f)) * 6.0f);
		texto(F_NUMEROS, (dif > 0 ? "-" : "") + std::to_string(dif), m.x, m.y + 22.0f * s, 56.0f * s * pulso, glm::vec4(1.0f, 0.9f, 0.55f, a),
		      glm::vec4(CARMESI.r, CARMESI.g, CARMESI.b, a), glm::vec3(0.6f, 0.0f, 1.0f), 0);
	}
