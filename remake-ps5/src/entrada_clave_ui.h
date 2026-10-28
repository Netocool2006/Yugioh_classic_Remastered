// Yugioh Classic Remaster (rama classic-cartas-clave): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// El corte 2D de la entrada especial de las cartas clave (entrada_clave.h: estado y tiempos, lo lleva
// entrada_clave_juego.h): velo oscuro, bandas de cine, destello y lineas de velocidad del color del atributo, la
// carta en primer plano que se da vuelta, el cielo que se abre (Dioses Egipcios) y el nombre en grande (ES/EN).
// La version corta (la carta ya entro en este duelo): destello, unas lineas y el nombre.

private:
	static float ruidoEntrada(int i)
	{
		uint32_t n = (uint32_t)i * 747796405u + 2891336453u;
		n = ((n >> ((n >> 28u) + 4u)) ^ n) * 277803737u;
		n = (n >> 22u) ^ n;
		return (float)(n & 0xffffffu) / 16777215.0f;
	}

	// lineas de velocidad que salen del centro (c)
	void lineasEntrada(glm::vec2 c, int n, float t, float alfa, glm::vec4 col)
	{
		if (alfa <= 0.0f) return;
		for (int i = 0; i < n; i++) {
			const float ang = ruidoEntrada(i) * 6.2832f;
			const float vel = 1.4f + 1.6f * ruidoEntrada(i + 101);
			const float r = fmodf(ruidoEntrada(i + 202) + t * vel, 1.0f);
			const float rad = (0.18f + 1.1f * r) * W * 0.55f;
			const float largo = (140.0f + 300.0f * ruidoEntrada(i + 303)) * s * (0.6f + r);
			const glm::vec2 dir(cosf(ang), sinf(ang));
			rect(c + dir * rad, glm::vec2(largo, (2.5f + 3.5f * ruidoEntrada(i + 404)) * s), ang, 6.0f * s, LINEA,
			     glm::vec4(glm::vec3(col), col.a * alfa * (1.0f - 0.6f * r)), NADA, 0.0f, glm::vec4(0.0f));
		}
	}

	// la banda con el nombre en grande (y el rotulo de la clase y el ATK/DEF)
	void nombreEntrada(const entrada::Estado& e, const CartaUI* k, float cx, float y, float alfa, float pop, glm::vec4 acento, bool corta)
	{
		if (alfa <= 0.0f) return;
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * alfa); };
		const float x0 = CAMPO_X * s, ancho = W - x0;
		const float alto = (corta ? 96.0f : 132.0f) * s;
		usar(setAtlas, false);
		caja(x0, y - alto * 0.5f, ancho, alto, 0.0f, PANEL, A(glm::vec4(0.01f, 0.012f, 0.03f, 0.72f)), NADA, 0.0f, glm::vec4(0.0f));
		rect(glm::vec2(cx, y - alto * 0.5f), glm::vec2(ancho * 0.9f, 3.0f * s), 0.0f, 3.0f * s, LINEA, A(acento), NADA, 0.0f, glm::vec4(1.5f, 0.1f, 0.0f, 0.0f));
		rect(glm::vec2(cx, y + alto * 0.5f), glm::vec2(ancho * 0.9f, 3.0f * s), 0.0f, 3.0f * s, LINEA, A(acento), NADA, 0.0f, glm::vec4(1.5f, 0.6f, 0.0f, 0.0f));
		const std::string nombre = k ? k->vNombre() : nombreCarta(e.carta);
		float tam = (corta ? 54.0f : 76.0f) * s * pop;
		while (medir(F_TITULO, nombre, tam, 2.0f) > ancho - 120.0f * s && tam > 28.0f * s) tam -= 2.0f * s;
		const float yn = y + (corta ? 18.0f : 22.0f) * s;
		texto(F_TITULO, nombre, cx, yn, tam, A(BLANCO), A(acento), glm::vec3(0.9f, 0.0f, 0.55f), 1, 2.0f);
		if (corta) return;
		std::string rot = idioma::tr(entrada::rotulo(e.clase));
		if (e.rival) rot += std::string("  ·  ") + idioma::tr("Rival");
		texto(F_TITULO, rot, cx, y - alto * 0.5f + 30.0f * s, 22.0f * s, A(acento), NADA, glm::vec3(0.0f), 1, 6.0f);
		if (k && k->clase != "magia" && k->clase != "trampa")
			texto(F_NUMEROS, "ATK " + std::to_string(k->atk) + "   DEF " + std::to_string(k->def), cx, y + alto * 0.5f - 12.0f * s, 22.0f * s, A(GRIS), NADA,
			      glm::vec3(0.0f), 1, 2.0f);
	}

	void dibujarEntradaClave()
	{
		dibujarEntradaMD();   // rama classic-md-entradas: la fusion / el ritual de Master Duel (entrada_md_ui.h)
		const entrada::Estado& e = entrada::estado();
		if (!e.activa || !ctl || ctl->vista.pantalla != control::VistaDuelo::DUELO) return;
		const float te = e.t;
		const int ci = buscarCarta(e.carta);
		const CartaUI* k = ci >= 0 ? &cartas[(size_t)ci] : nullptr;
		const entrada::Color c0 = entrada::colorAtributo(e.atributo);
		const glm::vec4 acento(c0.r, c0.g, c0.b, 1.0f);
		const glm::vec4 claro(glm::mix(glm::vec3(acento), glm::vec3(1.0f), 0.35f), 1.0f);
		const float cx = (CAMPO_X * s + W) * 0.5f, cy = H * 0.42f;
		const float fin = 1.0f - entrada::suave(e.dur - 0.35f, e.dur, te);   // todo se apaga al final
		usar(setAtlas, false);

		if (e.corta) {
			const float a = entrada::suave(0.0f, 0.12f, te) * fin;
			caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(glm::vec3(acento) * 0.8f, 0.30f * expf(-te * 7.0f)), NADA, 0.0f, glm::vec4(0.0f));
			lineasEntrada(glm::vec2(cx, cy), 22, te, 0.7f * a * (1.0f - entrada::suave(0.4f, 0.8f, te)), claro);
			nombreEntrada(e, k, cx, H * 0.80f, a, 1.0f + 0.15f * (1.0f - entrada::suave(0.0f, 0.25f, te)), acento, true);
			return;
		}

		const float tc = e.con3D ? e.tFigura : e.dur;   // fin del corte (sin figura: toda la entrada)
		const bool dios = e.clase == entrada::DIOS;
		// velo oscuro (con figura se abre al terminar el corte para ver la mesa)
		const float velo = 0.74f * entrada::suave(0.0f, 0.18f, te) * (e.con3D ? 1.0f - entrada::suave(tc - 0.15f, tc + 0.35f, te) : fin);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.012f, velo), NADA, 0.0f, glm::vec4(0.0f));
		// el cielo que se abre (Dioses Egipcios): haces dorados desde arriba
		if (dios) {
			const float cielo = entrada::suave(0.25f, 0.8f, te) * fin;
			rect(glm::vec2(cx, -60.0f * s), glm::vec2(W * 0.9f, 700.0f * s), 0.0f, 0.0f, PARTICULA, glm::vec4(1.0f, 0.8f, 0.35f, 0.9f * cielo), NADA, 0.0f,
			     glm::vec4(0.0f));
			for (int i = 0; i < 11; i++) {
				const float ang = glm::radians(90.0f + ((float)i - 5.0f) * 9.0f + 2.5f * sinf(te * 0.9f + (float)i));
				const float largo = H * (1.2f + 0.4f * ruidoEntrada(i + 7));
				const glm::vec2 dir(cosf(ang), sinf(ang));
				const float brillo = 0.55f + 0.45f * sinf(te * 3.0f + (float)i * 1.7f);
				rect(glm::vec2(cx, -40.0f * s) + dir * largo * 0.5f, glm::vec2(largo, (50.0f + 70.0f * ruidoEntrada(i + 31)) * s), ang, 20.0f * s, LINEA,
				     glm::vec4(1.0f, 0.86f, 0.5f, 0.55f * brillo * cielo), NADA, 0.0f, glm::vec4(0.0f));
			}
		}
		// destellos: al empezar, al voltear la carta y al aparecer la figura
		const float tVuelta = 0.55f;
		float fl = 0.5f * expf(-te * 7.0f);
		if (te >= tVuelta) fl += 0.3f * expf(-(te - tVuelta) * 9.0f);
		if (e.con3D && te >= tc) fl += (dios ? 0.6f : 0.4f) * expf(-(te - tc) * 8.0f);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(glm::vec3(claro), fl), NADA, 0.0f, glm::vec4(0.0f));
		// lineas de velocidad
		lineasEntrada(glm::vec2(cx, cy), dios ? 64 : 52, te, entrada::suave(0.0f, 0.15f, te) * (1.0f - entrada::suave(tc - 0.3f, tc + 0.1f, te)), claro);
		// bandas de cine
		const float banda = 0.085f * H * entrada::suave(0.0f, 0.3f, te) * fin;
		caja(0.0f, 0.0f, W, banda, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.93f), NADA, 0.0f, glm::vec4(0.0f));
		caja(0.0f, H - banda, W, banda, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.93f), NADA, 0.0f, glm::vec4(0.0f));

		// la carta en primer plano: entra de costado boca abajo, se da vuelta y se va (con figura) al terminar el corte
		const float entra = entrada::suave(0.0f, 0.38f, te);
		const float vuelta = entrada::suave(0.32f, tVuelta + 0.2f, te);
		const float va = e.con3D ? entrada::suave(tc - 0.3f, tc + 0.05f, te) : 0.0f;
		const float alfa = entra * (1.0f - va) * (e.con3D ? 1.0f : fin);
		if (alfa > 0.0f) {
			const float lado = e.rival ? -1.0f : 1.0f;
			const glm::vec2 centro(cx + lado * (1.0f - entra) * W * 0.55f, cy - 40.0f * s * va);
			const glm::vec2 tam = glm::vec2(440.0f, 638.0f) * s * (dios ? 1.06f : 1.0f) * (0.88f + 0.12f * entra + 0.3f * va) * (1.0f + 0.015f * sinf(te * 4.0f));
			const float angY = glm::pi<float>() * (1.0f - vuelta) + glm::radians(5.0f) * sinf(te * 1.3f);
			const float angX = glm::radians(3.0f) * sinf(te * 1.1f + 1.0f);
			usar(setAtlas, false);
			rect(centro, tam * 1.7f, 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(acento) * 0.55f, alfa), NADA, 0.0f, glm::vec4(0.0f));
			rectInclinado(centro + glm::vec2(0.0f, 16.0f * s), tam, angY > glm::half_pi<float>() ? angY - glm::pi<float>() : angY, angX, 30.0f * s, PANEL,
			              glm::vec4(0.0f, 0.0f, 0.0f, 0.6f * alfa), NADA, 14.0f * s, glm::vec4(0.0f, 0.0f, 0.0f, 22.0f * s));
			if (angY < glm::half_pi<float>() && k && listaCarta(ci)) {
				usar(setCaraGrande(k), false);
				const float uu = 0.5f / k->cara.width, vv = 0.5f / k->cara.height;
				rectInclinado(centro, tam, angY, angX, 0.0f, IMAGEN, glm::vec4(1.0f, 1.0f, 1.0f, alfa), acento, 12.0f * s, glm::vec4(0.9f, 1.0f, (float)ci * 0.13f, 0.0f),
				              glm::vec2(uu, vv), glm::vec2(1.0f - uu, 1.0f - vv));
			} else {
				Dorso dz = usarDorso(false, false, e.rival, 1.0f, 0.0f);
				dz.col.a *= alfa;
				dz.col2.a *= alfa;
				rectInclinado(centro, tam, angY > glm::half_pi<float>() ? angY - glm::pi<float>() : angY, angX, 0.0f, DORSO, dz.col, dz.col2, 12.0f * s, dz.extra,
				              uvDorso0(false), uvDorso1(false));
			}
			usar(setAtlas, false);
		}

		// el nombre en grande
		const float nom = entrada::suave(tVuelta - 0.05f, tVuelta + 0.25f, te) * fin;
		nombreEntrada(e, k, cx, H * 0.80f, nom, 1.0f + 0.25f * (1.0f - entrada::suave(tVuelta, tVuelta + 0.3f, te)), acento, false);
		// R2 salta la entrada
		const float ay = H - banda * 0.5f;
		if (banda > 20.0f * s) {
			float hx = W - 220.0f * s;
			hx += iconoBoton(glm::vec2(hx, ay), "R2", 14.0f * s, fin) + 10.0f * s;
			texto(F_TITULO, idioma::tr("Saltar"), hx, ay + 8.0f * s, 22.0f * s, glm::vec4(glm::vec3(GRIS), fin), NADA, glm::vec3(0.0f), 0, 1.0f);
		}
	}
