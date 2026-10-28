// Yugioh Classic Remaster (rama classic-historia): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// Las reglas del duelo (tutorial.h): un panel con el titulo de la pagina, un esquema de la mesa dibujado con las
// primitivas y las caras reales de las cartas (mini_<id>.png), los puntos de la pagina a la derecha y la ayuda de
// botones (cruz o L1/R1 para pasar de pagina, Circulo para volver). La pagina del mando lista los botones del duelo.

private:
	// una carta del esquema: su cara (mini) o el dorso; ang en radianes; brillo < 1 la apaga
	void cartaEsquema(const char* id, glm::vec2 c, glm::vec2 tam, float ang, float alfa, float brillo = 1.0f, bool bocaAbajo = false)
	{
		usar(setAtlas, false);
		rect(c + glm::vec2(4.0f, 6.0f) * s, tam, ang, 10.0f * s, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f * alfa), NADA, 6.0f * s, glm::vec4(0.0f, 0.0f, 0.0f, 8.0f * s));
		const int n = bocaAbajo || !id ? -1 : buscarCarta(id);
		if (n >= 0 && miniLista(n)) {
			usar(cartas[(size_t)n].setMini, false);
			const float u = 0.5f / (float)cartas[(size_t)n].mini.width, vv = 0.5f / (float)cartas[(size_t)n].mini.height;
			rect(c, tam, ang, 0.0f, IMAGEN, glm::vec4(glm::vec3(brillo), alfa), glm::vec4(0.6f, 0.8f, 1.0f, 1.0f), 5.0f * s, glm::vec4(0.1f, 0.2f, 0.0f, 0.0f), glm::vec2(u, vv),
			     glm::vec2(1.0f - u, 1.0f - vv));
		} else {
			const Dorso dz = usarDorso(false, true, false, brillo, 0.0f);
			rect(c, tam, ang, 0.0f, DORSO, glm::vec4(glm::vec3(dz.col), dz.col.a * alfa), dz.col2, 5.0f * s, dz.extra, uvDorso0(true), uvDorso1(true));
		}
		usar(setAtlas, false);
	}

	// una zona vacia de la mesa (borde que late si esta resaltada)
	void zonaEsquema(glm::vec2 c, glm::vec2 tam, glm::vec4 col, float alfa, bool resalte)
	{
		usar(setAtlas, false);
		const float late = resalte ? 0.65f + 0.35f * sinf(t * 4.0f) : 0.45f;
		rect(c, tam, 0.0f, 4.0f * s, PANEL, glm::vec4(glm::vec3(col) * 0.12f, (resalte ? 0.5f : 0.25f) * alfa), glm::vec4(glm::vec3(col) * late, alfa), 6.0f * s,
		     glm::vec4(1.4f * s, 0.9f, 0.0f, 0.0f));
	}

	void etiquetaEsquema(const char* es, const char* en, glm::vec2 p, float tam, glm::vec4 col, float alfa, int alin = 1)
	{
		struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
		texto(F_TITULO, idioma::t(es, en), p.x, p.y, tam, glm::vec4(glm::vec3(col), alfa), NADA, glm::vec3(0.0f), alin, 1.0f);
	}

	void flechaEsquema(glm::vec2 a, glm::vec2 b, glm::vec4 col, float alfa)
	{
		usar(setAtlas, false);
		const glm::vec2 d = b - a;
		const float l = glm::length(d), ang = atan2f(d.y, d.x);
		rect((a + b) * 0.5f - d / std::max(l, 1.0f) * 10.0f * s, glm::vec2(std::max(0.0f, l - 26.0f * s), 5.0f * s), ang, 2.0f * s, PANEL, glm::vec4(glm::vec3(col), alfa), NADA,
		     2.5f * s, glm::vec4(0.0f));
		// punta: dos trazos
		for (float g : { 0.6f, -0.6f }) {
			const glm::vec2 dir(cosf(ang + glm::pi<float>() - g), sinf(ang + glm::pi<float>() - g));
			rect(b + dir * 11.0f * s, glm::vec2(26.0f * s, 5.0f * s), ang - g, 2.0f * s, PANEL, glm::vec4(glm::vec3(col), alfa), NADA, 2.5f * s, glm::vec4(0.0f));
		}
	}

	// el esquema de la pagina, centrado en c (area de ~ancho x alto px)
	void dibujarEsquema(int esquema, glm::vec2 c, float ancho, float alto, float a)
	{
		const glm::vec2 carta = glm::vec2(64.0f, 92.0f) * s;
		const float ciclo = fmodf(tutorial::estado().tPagina, 4.0f);
		switch (esquema) {
		case tutorial::E_MESA: {
			// la mesa entera: el rival arriba (magias, monstruos) y el jugador abajo (monstruos, magias); a los lados
			// campo y Extra (izq) y mazo y cementerio (der)
			const float cw = carta.x + 16.0f * s, rh = carta.y + 16.0f * s;
			auto pos = [&](int fila, int i) { return c + glm::vec2((float)(i - 2) * cw, ((float)fila - 1.5f) * rh); };
			for (int fila = 0; fila < 4; fila++) {
				const bool monstruo = fila == 1 || fila == 2, rival = fila < 2;
				for (int i = 0; i < 5; i++) zonaEsquema(pos(fila, i), carta, monstruo ? (rival ? CARMESI : CIAN) : glm::vec4(0.35f, 0.9f, 0.6f, 1.0f), a, !rival);
				zonaEsquema(pos(fila, -1) - glm::vec2(10.0f * s, 0.0f), carta, glm::vec4(0.7f, 0.6f, 0.9f, 1.0f), a, false);
				zonaEsquema(pos(fila, 5) + glm::vec2(10.0f * s, 0.0f), carta, glm::vec4(0.7f, 0.6f, 0.9f, 1.0f), a, false);
			}
			cartaEsquema("dark_magician", pos(2, 2), carta, 0.0f, a);
			cartaEsquema(nullptr, pos(3, 1), carta, 0.0f, a, 1.0f, true);
			cartaEsquema("blue_eyes", pos(1, 2), carta, glm::pi<float>(), a);
			for (int k = 0; k < 4; k++) cartaEsquema(nullptr, pos(2, 5) + glm::vec2(10.0f * s - (float)k * 2.0f * s, -(float)k * 2.0f * s), carta, 0.0f, a, 1.0f, true);
			cartaEsquema("kuriboh", pos(3, 5) + glm::vec2(10.0f * s, 0.0f), carta, 0.0f, a, 0.75f);
			cartaEsquema("blue_eyes_ultimate_dragon", pos(3, -1) - glm::vec2(10.0f * s, 0.0f), carta, 0.0f, a, 0.75f);
			const float ly = carta.y * 0.5f + 22.0f * s;
			etiquetaEsquema("MONSTRUOS", "MONSTERS", pos(2, 2) + glm::vec2(0.0f, -carta.y * 0.5f - 5.0f * s), 17.0f * s, CIAN, a);
			etiquetaEsquema("MAGIAS Y TRAMPAS", "SPELLS & TRAPS", pos(3, 2) + glm::vec2(0.0f, ly), 17.0f * s, glm::vec4(0.35f, 0.9f, 0.6f, 1.0f), a);
			etiquetaEsquema("MAZO", "DECK", pos(2, 5) + glm::vec2(10.0f * s, ly), 15.0f * s, GRIS, a);
			etiquetaEsquema("CEMENTERIO", "GRAVEYARD", pos(3, 5) + glm::vec2(10.0f * s, ly), 15.0f * s, GRIS, a);
			etiquetaEsquema("CAMPO", "FIELD", pos(2, -1) - glm::vec2(10.0f * s, -ly), 15.0f * s, GRIS, a);
			etiquetaEsquema("EXTRA", "EXTRA", pos(3, -1) - glm::vec2(10.0f * s, -ly), 15.0f * s, GRIS, a);
			etiquetaEsquema("RIVAL", "OPPONENT", pos(0, 2) + glm::vec2(0.0f, -carta.y * 0.5f - 12.0f * s), 20.0f * s, CARMESI, a);
			etiquetaEsquema("TÚ", "YOU", pos(3, 2) + glm::vec2(0.0f, ly + 30.0f * s), 20.0f * s, CIAN, a);
			struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
			texto(F_NUMEROS, "LP 8000", pos(0, -1).x - 10.0f * s - carta.x * 0.5f, pos(0, 0).y - carta.y * 0.5f - 12.0f * s, 22.0f * s, glm::vec4(glm::vec3(CARMESI), a), NADA,
			      glm::vec3(0.0f), 0, 1.0f);
			texto(F_NUMEROS, "LP 8000", pos(3, -1).x - 10.0f * s - carta.x * 0.5f, pos(3, 0).y + carta.y * 0.5f + 52.0f * s, 22.0f * s, glm::vec4(glm::vec3(CIAN), a), NADA,
			      glm::vec3(0.0f), 0, 1.0f);
			break;
		}
		case tutorial::E_FASES: {
			static const char* const ES[] = { "ROBO", "PRINCIPAL 1", "BATALLA", "PRINCIPAL 2", "FINAL" };
			static const char* const EN[] = { "DRAW", "MAIN 1", "BATTLE", "MAIN 2", "END" };
			const int activa = (int)(tutorial::estado().tPagina / 1.2f) % 5;
			const float paso = ancho / 5.0f, w = paso - 26.0f * s, h = 64.0f * s, y = c.y - alto * 0.18f;
			for (int k = 0; k < 5; k++) {
				const glm::vec2 p(c.x - ancho * 0.5f + paso * ((float)k + 0.5f), y);
				const bool on = k == activa;
				usar(setAtlas, false);
				rect(p, glm::vec2(w, h), 0.0f, 8.0f * s, PANEL, glm::vec4(on ? glm::vec3(0.32f, 0.23f, 0.05f) : glm::vec3(0.05f, 0.07f, 0.12f), 0.9f * a),
				     glm::vec4(glm::vec3(on ? ORO : GRIS), a), h * 0.5f, glm::vec4(1.4f * s, on ? 1.2f : 0.6f, 0.0f, on ? 10.0f * s : 0.0f));
				etiquetaEsquema(ES[k], EN[k], p + glm::vec2(0.0f, 8.0f * s), 19.0f * s, on ? ORO : BLANCO, a);
				if (k < 4) flechaEsquema(p + glm::vec2(w * 0.5f + 2.0f * s, 0.0f), p + glm::vec2(paso - w * 0.5f - 2.0f * s, 0.0f), GRIS, a * 0.8f);
			}
			// la mano y el mazo: en ROBO una carta sale del mazo a la mano
			const glm::vec2 mazo(c.x + ancho * 0.36f, c.y + alto * 0.22f);
			for (int k = 0; k < 4; k++) cartaEsquema(nullptr, mazo + glm::vec2(-(float)k * 2.0f, -(float)k * 2.0f) * s, carta, 0.0f, a, 1.0f, true);
			etiquetaEsquema("MAZO", "DECK", mazo + glm::vec2(0.0f, carta.y * 0.5f + 24.0f * s), 15.0f * s, GRIS, a);
			static const char* const MANO[] = { "dark_magician", "kuriboh", "monster_reborn", "trap_hole", "summoned_skull" };
			for (int k = 0; k < 5; k++)
				cartaEsquema(MANO[k], glm::vec2(c.x - ancho * 0.30f + (float)k * (carta.x + 10.0f * s), c.y + alto * 0.24f), carta, glm::radians(-6.0f + 3.0f * (float)k), a);
			if (activa == 0) {
				const float u = suave(0.0f, 1.0f, fmodf(tutorial::estado().tPagina, 1.2f));
				const glm::vec2 hacia(c.x - ancho * 0.30f + 5.0f * (carta.x + 10.0f * s), c.y + alto * 0.24f);
				cartaEsquema(nullptr, glm::mix(mazo, hacia, u) - glm::vec2(0.0f, sinf(u * glm::pi<float>()) * 50.0f * s), carta, 0.0f, a, 1.0f, true);
			}
			etiquetaEsquema("TU MANO (máx. 6 al final del turno)", "YOUR HAND (max. 6 at end of turn)", glm::vec2(c.x - ancho * 0.30f + 2.0f * (carta.x + 10.0f * s), c.y + alto * 0.24f + carta.y * 0.5f + 30.0f * s),
			                15.0f * s, GRIS, a);
			break;
		}
		case tutorial::E_INVOCAR: {
			// dos sacrificios se van y llega un monstruo de nivel 7+
			const float u = suave(0.8f, 2.2f, ciclo);
			const glm::vec2 t1(c.x - ancho * 0.36f, c.y - alto * 0.12f), t2(c.x - ancho * 0.16f, c.y - alto * 0.12f), dest(c.x + ancho * 0.28f, c.y - alto * 0.12f);
			const glm::vec2 grande = carta * 1.6f;
			cartaEsquema("kuriboh", t1 - glm::vec2(0.0f, u * 30.0f * s), carta * 1.3f, 0.0f, a * (1.0f - u * 0.85f));
			cartaEsquema("dark_magician", t2 - glm::vec2(0.0f, u * 30.0f * s), carta * 1.3f, 0.0f, a * (1.0f - u * 0.85f));
			flechaEsquema(glm::vec2(c.x - ancho * 0.04f, dest.y), glm::vec2(dest.x - grande.x * 0.6f, dest.y), ORO, a);
			cartaEsquema("blue_eyes", dest, grande * (0.85f + 0.15f * u), 0.0f, a * std::max(0.25f, u));
			etiquetaEsquema("2 TRIBUTOS", "2 TRIBUTES", glm::vec2((t1.x + t2.x) * 0.5f, t1.y + carta.y * 0.65f + 30.0f * s), 19.0f * s, CARMESI, a);
			etiquetaEsquema("NIVEL 8", "LEVEL 8", dest + glm::vec2(0.0f, grande.y * 0.5f + 30.0f * s), 19.0f * s, ORO, a);
			// la tabla de tributos
			static const char* const ES[] = { "Nivel 1-4: 0 tributos", "Nivel 5-6: 1 tributo", "Nivel 7+: 2 tributos" };
			static const char* const EN[] = { "Level 1-4: 0 Tributes", "Level 5-6: 1 Tribute", "Level 7+: 2 Tributes" };
			for (int k = 0; k < 3; k++)
				etiquetaEsquema(ES[k], EN[k], glm::vec2(c.x - ancho * 0.30f + (float)k * ancho * 0.30f, c.y + alto * 0.36f), 19.0f * s, k == 2 ? ORO : BLANCO, a);
			break;
		}
		case tutorial::E_POSICIONES: {
			const float y = c.y - alto * 0.04f;
			const glm::vec2 g = carta * 1.45f;
			cartaEsquema("dark_magician", glm::vec2(c.x - ancho * 0.33f, y), g, 0.0f, a);
			cartaEsquema("summoned_skull", glm::vec2(c.x, y), g, glm::half_pi<float>(), a);
			cartaEsquema(nullptr, glm::vec2(c.x + ancho * 0.33f, y), g, glm::half_pi<float>(), a, 1.0f, true);
			const float ly = y + g.y * 0.5f + 40.0f * s;
			etiquetaEsquema("ATAQUE", "ATTACK", glm::vec2(c.x - ancho * 0.33f, ly), 21.0f * s, CARMESI, a);
			etiquetaEsquema("DEFENSA", "DEFENSE", glm::vec2(c.x, ly), 21.0f * s, CIAN, a);
			etiquetaEsquema("BOCA ABAJO", "FACE-DOWN", glm::vec2(c.x + ancho * 0.33f, ly), 21.0f * s, GRIS, a);
			etiquetaEsquema("usa el ATK", "uses ATK", glm::vec2(c.x - ancho * 0.33f, ly + 28.0f * s), 16.0f * s, GRIS, a);
			etiquetaEsquema("usa la DEF", "uses DEF", glm::vec2(c.x, ly + 28.0f * s), 16.0f * s, GRIS, a);
			etiquetaEsquema("en defensa, oculto", "in defense, hidden", glm::vec2(c.x + ancho * 0.33f, ly + 28.0f * s), 16.0f * s, GRIS, a);
			break;
		}
		case tutorial::E_MAGIAS: {
			// la fila de magias y trampas resaltada: una magia boca arriba, una trampa colocada y la zona de campo
			const float cw = carta.x * 1.3f + 18.0f * s, y = c.y - alto * 0.02f;
			for (int i = 0; i < 5; i++) zonaEsquema(glm::vec2(c.x + (float)(i - 2) * cw, y), carta * 1.3f, glm::vec4(0.35f, 0.9f, 0.6f, 1.0f), a, true);
			zonaEsquema(glm::vec2(c.x - 3.0f * cw - 12.0f * s, y), carta * 1.3f, glm::vec4(0.7f, 0.6f, 0.9f, 1.0f), a, true);
			cartaEsquema("monster_reborn", glm::vec2(c.x - 2.0f * cw, y), carta * 1.3f, 0.0f, a);
			const float alza = sinf(t * 2.0f) * 6.0f * s;
			cartaEsquema(nullptr, glm::vec2(c.x, y + alza), carta * 1.3f, 0.0f, a, 1.0f, true);
			cartaEsquema("mirror_force", glm::vec2(c.x + 2.0f * cw, y), carta * 1.3f, 0.0f, a);
			const float ly = y + carta.y * 0.65f + 30.0f * s;
			etiquetaEsquema("MAGIA", "SPELL", glm::vec2(c.x - 2.0f * cw, ly), 18.0f * s, glm::vec4(0.35f, 0.9f, 0.6f, 1.0f), a);
			etiquetaEsquema("TRAMPA COLOCADA", "SET TRAP", glm::vec2(c.x, ly), 18.0f * s, CARMESI, a);
			etiquetaEsquema("TRAMPA ACTIVADA", "ACTIVATED TRAP", glm::vec2(c.x + 2.0f * cw, ly), 18.0f * s, ORO, a);
			etiquetaEsquema("CAMPO", "FIELD", glm::vec2(c.x - 3.0f * cw - 12.0f * s, ly), 16.0f * s, GRIS, a);
			etiquetaEsquema("Una trampa se activa a partir del turno siguiente", "A Trap can be activated from the next turn on", glm::vec2(c.x, c.y - alto * 0.3f), 18.0f * s, BLANCO, a);
			break;
		}
		case tutorial::E_CADENA: {
			// tres eslabones apilados con su numero; se resuelven del 3 al 1 (el que resuelve, en dorado)
			static const char* const CAD[] = { "monster_reborn", "trap_hole", "mystical_space_typhoon" };
			const int resuelve = 2 - std::min(2, (int)(ciclo / 1.2f));
			const glm::vec2 g = carta * 1.35f;
			for (int k = 0; k < 3; k++) {
				const glm::vec2 p(c.x - ancho * 0.18f + (float)k * g.x * 0.62f, c.y + alto * 0.06f - (float)k * 46.0f * s);
				const bool on = k == resuelve;
				cartaEsquema(CAD[k], p, g, glm::radians(-4.0f + 4.0f * (float)k), a, on || k < resuelve ? 1.0f : 0.45f);
				usar(setAtlas, false);
				const glm::vec2 o = p + glm::vec2(-g.x * 0.42f, -g.y * 0.42f);
				rect(o, glm::vec2(22.0f * s), 0.0f, 6.0f * s, ORBE, glm::vec4(glm::vec3(on ? ORO : CIAN), a), glm::vec4(glm::vec3(on ? ORO : CIAN) * 0.4f, a), 0.0f,
				     glm::vec4(1.0f, 1.0f, 0.0f, 0.0f));
				struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
				texto(F_NUMEROS, std::to_string(k + 1), o.x, o.y + 8.0f * s, 22.0f * s, glm::vec4(0.05f, 0.05f, 0.1f, a), NADA, glm::vec3(0.0f), 1, 0.0f);
			}
			const glm::vec2 f0(c.x + ancho * 0.36f, c.y - alto * 0.2f), f1(c.x + ancho * 0.36f, c.y + alto * 0.2f);
			flechaEsquema(f0, f1, ORO, a);
			etiquetaEsquema("RESUELVE", "RESOLVES", f0 - glm::vec2(0.0f, 24.0f * s), 17.0f * s, ORO, a);
			etiquetaEsquema("3  >  2  >  1", "3  >  2  >  1", f1 + glm::vec2(0.0f, 36.0f * s), 20.0f * s, ORO, a);
			etiquetaEsquema("CADENA", "CHAIN", glm::vec2(c.x - ancho * 0.05f, c.y + alto * 0.36f), 20.0f * s, CIAN, a);
			break;
		}
		case tutorial::E_BATALLA: {
			// el atacante (abajo) embiste al defensor (arriba); se destruye el de menos ATK y su duenno pierde la diferencia
			const glm::vec2 g = carta * 1.35f;
			const float u = suave(0.4f, 1.0f, ciclo) * (1.0f - suave(1.2f, 1.6f, ciclo));
			const glm::vec2 atk(c.x - ancho * 0.12f, c.y + alto * 0.2f), def(c.x - ancho * 0.12f, c.y - alto * 0.22f);
			const bool roto = ciclo > 1.2f;
			cartaEsquema("dark_magician", def, g, glm::pi<float>(), a * (roto ? std::max(0.0f, 1.0f - (ciclo - 1.2f) * 1.5f) : 1.0f));
			cartaEsquema("blue_eyes", glm::mix(atk, def + glm::vec2(0.0f, g.y * 0.7f), u * 0.7f), g, 0.0f, a);
			if (ciclo > 1.0f && ciclo < 1.6f) {
				usar(setAtlas, false);
				rect(def + glm::vec2(0.0f, g.y * 0.45f), glm::vec2(260.0f * s), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(1.0f, 0.8f, 0.4f) * (1.6f - ciclo) * 2.0f, 1.0f), NADA, 0.0f,
				     glm::vec4(0.0f));
			}
			struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
			texto(F_NUMEROS, "ATK 3000", atk.x + g.x * 0.65f, atk.y + 8.0f * s, 24.0f * s, glm::vec4(glm::vec3(CIAN), a), NADA, glm::vec3(0.0f), 0, 1.0f);
			texto(F_NUMEROS, "ATK 2500", def.x + g.x * 0.65f, def.y + 8.0f * s, 24.0f * s, glm::vec4(glm::vec3(CARMESI), a), NADA, glm::vec3(0.0f), 0, 1.0f);
			if (roto) {
				const float sube = (ciclo - 1.2f) * 30.0f * s;
				texto(F_NUMEROS, "-500 LP", c.x + ancho * 0.3f, def.y - sube, 40.0f * s, glm::vec4(glm::vec3(CARMESI), a), glm::vec4(glm::vec3(1.0f, 0.6f, 0.6f), a), glm::vec3(0.5f, 0.0f, 0.6f), 1,
				      1.0f);
				etiquetaEsquema("3000 - 2500 = 500 de daño", "3000 - 2500 = 500 damage", glm::vec2(c.x + ancho * 0.3f, def.y + 44.0f * s), 17.0f * s, BLANCO, a);
			}
			break;
		}
		case tutorial::E_FUSION: {
			// fila de arriba: 3 Ojos Azules + Polimerizacion -> Ultimate; fila de abajo: Ritual del Soldado del Brillo Negro
			const glm::vec2 g = carta * 1.15f;
			const float y1 = c.y - alto * 0.2f, y2 = c.y + alto * 0.22f;
			const float x0 = c.x - ancho * 0.42f;
			for (int k = 0; k < 3; k++) cartaEsquema("blue_eyes", glm::vec2(x0 + (float)k * (g.x + 10.0f * s), y1), g, 0.0f, a);
			cartaEsquema("polymerization", glm::vec2(x0 + 3.0f * (g.x + 10.0f * s) + 16.0f * s, y1), g, 0.0f, a);
			flechaEsquema(glm::vec2(x0 + 3.6f * (g.x + 10.0f * s) + 20.0f * s, y1), glm::vec2(c.x + ancho * 0.24f, y1), ORO, a);
			cartaEsquema("blue_eyes_ultimate_dragon", glm::vec2(c.x + ancho * 0.36f, y1), g * 1.25f, 0.0f, a);
			etiquetaEsquema("FUSIÓN", "FUSION", glm::vec2(c.x - ancho * 0.06f, y1 - g.y * 0.5f - 18.0f * s), 19.0f * s, ORO, a);
			cartaEsquema("black_luster_ritual", glm::vec2(x0, y2), g, 0.0f, a);
			cartaEsquema("summoned_skull", glm::vec2(x0 + g.x + 10.0f * s, y2), g, 0.0f, a);
			cartaEsquema("curse_of_dragon", glm::vec2(x0 + 2.0f * (g.x + 10.0f * s), y2), g, 0.0f, a);
			flechaEsquema(glm::vec2(x0 + 2.6f * (g.x + 10.0f * s) + 20.0f * s, y2), glm::vec2(c.x + ancho * 0.24f, y2), CIAN, a);
			cartaEsquema("black_luster_soldier", glm::vec2(c.x + ancho * 0.36f, y2), g * 1.25f, 0.0f, a);
			etiquetaEsquema("RITUAL: niveles 6 + 5 >= 8", "RITUAL: Levels 6 + 5 >= 8", glm::vec2(c.x - ancho * 0.06f, y2 - g.y * 0.5f - 18.0f * s), 19.0f * s, CIAN, a);
			break;
		}
		default: break;
		}
	}

	void dibujarTutorial()
	{
		tutorial::Estado& e = tutorial::estado();
		e.cuadro(dtCuadro);
		if (!e.abierto) return;
		const tutorial::Pagina& pg = tutorial::PAGINAS[e.pagina];
		const float a = suave(0.0f, 0.3f, e.t);
		const float ap = suave(0.0f, 0.25f, e.tPagina);
		const float desliza = (1.0f - ap) * 40.0f * s * (float)e.dir;
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		rz.tapar();   // rama classic-pc: un clic pasa a la pagina siguiente (X); la rueda cambia de pagina (izq / der)
		zBoton(0.0f, 0.0f, W, H, control::B_X);
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.01f, 0.015f, 0.03f, 0.9f)), NADA, 0.0f, glm::vec4(0.0f));
		const float pw = std::min(W - 80.0f * s, 1640.0f * s), ph = std::min(H - 60.0f * s, 960.0f * s);
		const float px = W * 0.5f - pw * 0.5f, py = H * 0.5f - ph * 0.5f;
		rect(glm::vec2(W * 0.5f, H * 0.5f), glm::vec2(pw * 1.2f, ph * 1.25f), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(CIAN) * 0.10f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		caja(px, py, pw, ph, 30.0f * s, PANEL, A(VIDRIO), A(CIAN), 22.0f * s, glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s));
		struct Crudo { bool& b; bool antes; Crudo(bool& v) : b(v), antes(v) { b = true; } ~Crudo() { b = antes; } } crudoAca(crudo);
		// cabecera: REGLAS DEL DUELO, el titulo de la pagina y los puntos de pagina
		texto(F_TITULO, idioma::t("REGLAS DEL DUELO", "DUEL RULES"), px + 50.0f * s, py + 56.0f * s, 22.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 0, 3.0f);
		texto(F_NUMEROS, pg.titulo.v(), px + 50.0f * s + desliza, py + 106.0f * s, 44.0f * s, glm::vec4(glm::vec3(BLANCO), a * ap), glm::vec4(glm::vec3(CIAN), a * ap),
		      glm::vec3(0.5f, 0.0f, 0.6f), 0, 2.0f);
		for (int k = 0; k < tutorial::NUM_PAGINAS; k++) {
			const bool on = k == e.pagina;
			const glm::vec2 c(px + pw - 60.0f * s - (float)(tutorial::NUM_PAGINAS - 1 - k) * 34.0f * s, py + 64.0f * s);
			rect(c, glm::vec2((on ? 18.0f : 11.0f) * s), 0.0f, 5.0f * s, ORBE, A(on ? ORO : glm::vec4(0.35f, 0.4f, 0.5f, 1.0f)), A(glm::vec4(0.1f, 0.1f, 0.15f, 1.0f)), 0.0f,
			     glm::vec4(on ? 1.0f : 0.5f, 1.0f, 0.0f, 0.0f));
		}
		const float cuerpo = py + 140.0f * s, alto = ph - 140.0f * s - 90.0f * s;
		if (pg.esquema == tutorial::E_MANDO) {
			// los botones del duelo en dos columnas
			const int n = tutorial::NUM_BOTONES, mitad = (n + 1) / 2;
			for (int k = 0; k < n; k++) {
				const tutorial::BotonAyuda& b = tutorial::BOTONES[k];
				const float x = px + 80.0f * s + (k < mitad ? 0.0f : pw * 0.5f) + desliza;
				const float y = cuerpo + 50.0f * s + (float)(k % mitad) * 120.0f * s;
				const std::string ic = b.icono;
				const bool redondo = ic == "X" || ic == "O" || ic == "T" || ic == "C" || ic == "CRUZ" || ic == "STICK_R";
				float wi = 0.0f;
				if (redondo) {
					iconoBoton(glm::vec2(x + 24.0f * s, y), ic, 24.0f * s, a * ap);
					wi = 48.0f * s;
				} else {
					wi = teclaBoton(x, y, b.icono, a * ap);
				}
				parrafo(F_TEXTO, b.texto.v(), x + std::max(wi, 110.0f * s) + 24.0f * s, y + 9.0f * s, 25.0f * s, pw * 0.5f - 260.0f * s, 30.0f * s, 2,
				        glm::vec4(glm::vec3(BLANCO), a * ap));
			}
		} else {
			// el esquema a la izquierda y los puntos a la derecha
			const float ew = pw * 0.52f;
			usar(setAtlas, false);
			caja(px + 40.0f * s, cuerpo, ew, alto, 10.0f * s, PANEL, A(glm::vec4(0.02f, 0.05f, 0.09f, 0.75f)), A(glm::vec4(glm::vec3(CIAN) * 0.5f, 1.0f)), 16.0f * s,
			     glm::vec4(1.0f * s, 0.7f, 0.0f, 0.0f));
			dibujarEsquema(pg.esquema, glm::vec2(px + 40.0f * s + ew * 0.5f + desliza, cuerpo + alto * 0.5f), ew - 80.0f * s, alto - 60.0f * s, a * ap);
			float y = cuerpo + 40.0f * s;
			const float tx = px + 40.0f * s + ew + 50.0f * s + desliza, tw = pw - ew - 140.0f * s;
			for (const tutorial::Texto& p : pg.puntos) {
				if (!p.es) break;
				usar(setAtlas, false);
				rect(glm::vec2(tx, y - 8.0f * s), glm::vec2(10.0f * s), glm::radians(45.0f), 3.0f * s, PANEL, glm::vec4(glm::vec3(ORO), a * ap), NADA, 1.5f * s, glm::vec4(0.0f));
				const int lineas = parrafo(F_TEXTO, p.v(), tx + 26.0f * s, y, 26.0f * s, tw, 34.0f * s, 5, glm::vec4(glm::vec3(BLANCO), a * ap));
				y += (float)std::max(1, lineas) * 34.0f * s + 26.0f * s;
			}
		}
		// pie: cruz / L1 R1 pagina, Circulo volver y n / total
		const float by = py + ph - 46.0f * s;
		float bx = px + 50.0f * s;
		iconoBoton(glm::vec2(bx + 14.0f * s, by), "CRUZ", 14.0f * s, a);
		bx += 40.0f * s;
		bx += teclaBoton(bx, by, "L1R1", a) + 12.0f * s;
		bx += texto(F_TITULO, idioma::t("Página", "Page"), bx, by + 8.0f * s, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f) + 34.0f * s;
		iconoBoton(glm::vec2(bx + 14.0f * s, by), "O", 14.0f * s, a);
		bx += 40.0f * s;
		texto(F_TITULO, idioma::t("Volver", "Back"), bx, by + 8.0f * s, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f);
		texto(F_TITULO, std::to_string(e.pagina + 1) + " / " + std::to_string(tutorial::NUM_PAGINAS), px + pw - 50.0f * s, by + 8.0f * s, 21.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 2,
		      1.0f);
	}
