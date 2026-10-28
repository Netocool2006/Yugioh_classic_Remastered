	// Yugioh Classic Remaster (rama classic-ps5-ritmo): se incluye DENTRO de la clase Interfaz (al final de md_ritmo_ui.h).
	// El robo de la Fase de Robo del jugador como Master Duel (EffectTaskWaitInput -> DrawOperation; tiempos en
	// ritmo_turno.h): la carta sale del mazo (DrawCardMotionFirst), viene grande delante de la camara dandose vuelta
	// (DrawCardMotion), se queda mostrada y baja a su lugar en la mano (DrawCardToHandMotion). Cada tramo avanza con la
	// accelerationCurve de su movimiento de MD si esta en <assets>/md_ritmo/movimientos.txt (si no, suave).
	float tramoRoboMD(const char* mov, float u) const
	{
		u = glm::clamp(u, 0.0f, 1.0f);
		const ritmo::Movimiento* m = ritmo::movimiento(mov);
		if (m && m->acel.hay()) return glm::clamp(m->acel.valor(u), 0.0f, 1.2f);
		return suave(0.0f, 1.0f, u);
	}
	bool dibujarRoboTurnoMD()
	{
		const control::Mando::Robo& ro = ctl->robo;
		const float T = ritmoturno::ROBO_TURNO;
		const float t = ro.t * (ro.dur > 0.0f ? ro.dur : T);   // (s desde que empezo)
		const float t1 = ritmoturno::ROBO_SALE, t2 = t1 + ritmoturno::ROBO_A_CAMARA, t3 = t2 + ritmoturno::ROBO_MUESTRA;
		if (ro.serie != serieRobo) {
			serieRobo = ro.serie;
			const Zona* z = zonaPorId("jugador_mazo");
			roboDesde = z ? aPantalla(proyectar(glm::vec3(z->x * ESCALA, 0.016f * (float)mazoJ + 0.05f, z->z * ESCALA))) : glm::vec2(W * 0.85f, H);
		}
		glm::vec2 mano;
		float aMano, eMano;
		cartaManoJ(ro.indice, mano, aMano, eMano);
		const float pi = glm::pi<float>();
		const glm::vec2 tamMano = glm::vec2(150.0f, 218.0f) * s * eMano;
		const glm::vec2 tamCam = glm::vec2(300.0f, 436.0f) * s;   // (delante de la camara)
		const glm::vec2 alto = roboDesde + glm::vec2(0.0f, -150.0f * s);   // (sale del mazo hacia arriba)
		const glm::vec2 centro((CAMPO_X * s + W) * 0.5f, H * 0.47f);
		glm::vec2 p, tam;
		float ang = 0.0f, giro = 0.0f;   // giro: 0 dorso -> 1 cara
		if (t < t1) {
			const float u = tramoRoboMD("DrawCardMotionFirst", t / t1);
			p = glm::mix(roboDesde, alto, u);
			tam = tamMano * 0.6f;
		} else if (t < t2) {
			const float u = tramoRoboMD("DrawCardMotion", (t - t1) / (t2 - t1));
			p = glm::mix(alto, centro, u);
			tam = glm::mix(tamMano * 0.6f, tamCam, u);
			giro = u;
		} else if (t < t3) {
			const float u = (t - t2) / (t3 - t2);
			p = centro + glm::vec2(0.0f, -6.0f * s * sinf(pi * u));   // (flota mientras se muestra)
			tam = tamCam;
			giro = 1.0f;
		} else {
			const float u = tramoRoboMD("DrawCardToHandMotion", (t - t3) / ritmoturno::ROBO_A_MANO);
			p = glm::mix(centro, mano, u);
			tam = glm::mix(tamCam, tamMano, u);
			ang = glm::mix(0.0f, aMano, u);
			giro = 1.0f;
		}
		const bool cara = giro >= 0.5f;
		tam.x *= glm::max(fabsf(cosf(pi * giro)), 0.04f);
		usar(setAtlas, false);
		rect(p + glm::vec2(0.0f, 14.0f * s), tam, ang, 22.0f * s, PANEL, glm::vec4(0, 0, 0, 0.45f), NADA, 10.0f * s, glm::vec4(0, 0, 0, 14.0f * s));
		const int ci = cara ? buscarCarta(ro.carta) : -1;
		if (ci >= 0 && listaCarta(ci)) {
			usar(cartas[ci].setMini, false);
			const float uu = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
			rect(p, tam, ang, 0.0f, IMAGEN, glm::vec4(glm::vec3(1.05f), 1.0f), CIAN, 8.0f * s, glm::vec4(0.5f, 1.0f, 0.2f, 0.0f), glm::vec2(uu, vv),
			     glm::vec2(1.0f - uu, 1.0f - vv));
		} else {
			const Dorso dz = usarDorso(false, false, false, 1.0f, 0.0f);
			rect(p, tam, ang, 0.0f, DORSO, dz.col, dz.col2, 8.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		usar(setAtlas, false);
		const float luz = t < t1 ? t / t1 : t < t3 ? 1.0f : 1.0f - (t - t3) / ritmoturno::ROBO_A_MANO;
		rect(p, tam * 1.3f, ang, 0.0f, PARTICULA, glm::vec4(glm::vec3(CIAN) * 0.28f * glm::clamp(luz, 0.0f, 1.0f), 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		return true;
	}
