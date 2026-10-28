	// Yugioh Classic Remaster (rama classic-ps5-cpu-joey): se incluye DENTRO de la clase Interfaz (al final de
	// md_ritmo_ui.h). La carta que el rival ensena antes de jugarla desde la mano (cpu_muestra.h: como Joey the Passion,
	// FUN_0040ad90 / FUN_00418b60 / FUN_00418ef0): en su lugar de la mano, levantada hasta verse entera, se da vuelta
	// (0.267 s) y queda con el marco de seleccion parpadeando (1.067 s). Boca abajo (colocar) no se da vuelta: el dorso.
	// k: la carta de la mano del rival; c, tam: su lugar y tamanno (ya con el levante de ajusteManoRivalMD). true: la dibujo
	bool dibujarMuestraCPU(int k, glm::vec2 c, glm::vec2 tam)
	{
		const ritmo::Viaje* v = nullptr;
		for (const ritmo::Viaje& x : ritmo::estado().viajes)
			if (x.llegada && x.origen == ritmo::O_MANO && x.ladoPila == 1 && x.eligiendo() && x.manoIdx == k) v = &x;
		if (!v) return false;
		const float pi = glm::pi<float>();
		const float t = v->t;
		// levantada hasta verse entera debajo del borde de arriba (la mano del rival esta medio afuera de la pantalla)
		const float sube = suave(0.0f, cpumuestra::VOLTEO, t);
		const glm::vec2 tamM = glm::vec2(150.0f, 218.0f) * s;   // (del tamanno de una carta de la mano del jugador: se lee)
		const glm::vec2 centro(c.x, tamM.y * 0.5f + 18.0f * s);
		const glm::vec2 p = glm::mix(c, centro, sube);
		glm::vec2 tm = glm::mix(tam, tamM, sube);
		// el volteo (Joey: 8 cuadros, en su lugar); el angulo pasa de dorso del reves (180) a la cara derecha (0)
		const float u = glm::clamp(t / cpumuestra::VOLTEO, 0.0f, 1.0f);
		const bool cara = v->caraEnVuelo && u >= 0.5f;
		if (v->caraEnVuelo) tm.x *= glm::max(fabsf(cosf(pi * u)), 0.04f);
		const float ang = v->caraEnVuelo ? (u < 0.5f ? pi : 0.0f) : pi;
		usar(setAtlas, false);
		rect(p + glm::vec2(0.0f, 10.0f * s), tm, ang, 20.0f * s, PANEL, glm::vec4(0, 0, 0, 0.55f), NADA, 9.0f * s, glm::vec4(0, 0, 0, 14.0f * s));
		const int ci = cara ? buscarCarta(v->carta) : -1;
		if (ci >= 0 && listaCarta(ci)) {
			usar(cartas[ci].setMini, false);
			const float uu = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
			rect(p, tm, ang, 0.0f, IMAGEN, glm::vec4(glm::vec3(1.05f), 1.0f), CARMESI, 8.0f * s, glm::vec4(0.5f, 1.0f, 0.2f, 0.0f), glm::vec2(uu, vv),
			     glm::vec2(1.0f - uu, 1.0f - vv));
		} else {
			const Dorso dz = usarDorso(false, false, true, 1.0f, (float)k);
			rect(p, tm, ang, 0.0f, DORSO, dz.col, dz.col2, 9.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		// el marco de seleccion de Joey: parpadea cada 2 cuadros mientras se muestra
		if (t >= cpumuestra::VOLTEO && fmodf(t - cpumuestra::VOLTEO, 2.0f * cpumuestra::PARPADEO) < cpumuestra::PARPADEO) {
			usar(setAtlas, false);
			caja(p.x - tm.x * 0.5f - 6.0f * s, p.y - tm.y * 0.5f - 6.0f * s, tm.x + 12.0f * s, tm.y + 12.0f * s, 10.0f * s, PANEL, NADA, glm::vec4(glm::vec3(ORO), 1.0f),
			     12.0f * s, glm::vec4(3.0f * s, 1.4f, 0.0f, 14.0f * s));
		}
		return true;
	}
	// 0..1: el rival esta ensenando una carta (el cartel fijo TURNO DEL RIVAL se aparta para no taparla)
	float muestraCPUActiva() const
	{
		float m = 0.0f;
		for (const ritmo::Viaje& x : ritmo::estado().viajes)
			if (x.llegada && x.origen == ritmo::O_MANO && x.ladoPila == 1 && x.eligiendo())
				m = glm::max(m, glm::min(suave(0.0f, 0.15f, x.t), 1.0f - suave(x.tElige - 0.15f, x.tElige, x.t)));
		return m;
	}
