// Yugioh Classic Remaster (rama classic-portada): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// - La portada (control::VistaDuelo::PORTADA; la lleva portada.h): el arte del titulo a pantalla completa y, encima,
//   las letras de "CLASSIC REMASTERED" como capa aparte que palpita (latido de dos pulsos cada 1,2 s: escala
//   1.00 -> 1.04, brillo y glow dorado; el destello del shader IMAGEN las recorre cada ~4,5 s). Debajo, "Pulsa X para
//   comenzar" parpadeando. Entra desde negro y sale a negro; el menu principal aparece desde negro (portadaNegro).
// - El fondo del menu principal (menu_ui.h): el arte de la puerta egipcia sin el texto de los botones, su "CLASSIC
//   REMASTERED" con el mismo latido, el ojo de Horus brillando suave y el marco encendido del boton elegido.
// Imagenes (portada_post.py, no van al repo) en assets/ui/: portada.jpg (3840x2160, con la silueta oscura de las
// letras), portada_letras.png (RGBA), portada_brillo.png (glow); menu_fondo.png, menu_letras.png, menu_brillo.png y
// menu_sel.png. Las de la portada se liberan al salir; las del menu quedan (se cargan durante la portada).

public:
	float portadaNegro{ 0.0f };   // velo negro (0..1): lo pone portada.h; en el menu se desvanece solo

private:
	struct ImagenPortada { vks::Texture2D tex; VkDescriptorSet set{ VK_NULL_HANDLE }; };
	enum { PI_FONDO = 0, PI_LETRAS, PI_BRILLO, MI_FONDO, MI_LETRAS, MI_BRILLO, MI_SEL, NUM_IMAGENES_PORTADA };
	std::array<ImagenPortada, NUM_IMAGENES_PORTADA> portadaImg{};
	bool portadaCargada{ false }, portadaFalta{ false }, menuCargado{ false }, menuFalta{ false };
	uint64_t portadaUso{ 0 };
	std::array<float, 8> menuEncendido{};   // brillo de cada boton del menu (fundido corto al mover la cruz)
	// rectangulos (px de las imagenes de 3840x2160; los imprime portada_post.py)
	static constexpr float PORTADA_W = 3840.0f, PORTADA_H = 2160.0f;
	struct RectImg { float x, y, w, h; };
	static constexpr RectImg PORTADA_LETRAS{ 1362.0f, 1283.0f, 1166.0f, 131.0f }, PORTADA_BRILLO{ 1272.0f, 1193.0f, 1346.0f, 311.0f };
	static constexpr RectImg MENU_LETRAS{ 1447.0f, 545.0f, 953.0f, 247.0f }, MENU_BRILLO{ 1357.0f, 455.0f, 1133.0f, 427.0f };
	// botones de la puerta: x0..x1, la "y" de arriba de cada uno y el alto; menu_sel.png los cubre con 60 px de margen
	static constexpr float BOTON_X0 = 1656.0f, BOTON_X1 = 2186.0f, BOTON_ALTO = 116.0f, SEL_MARGEN = 60.0f;
	static constexpr float BOTONES_Y[5] = { 869.0f, 1019.0f, 1169.0f, 1318.0f, 1468.0f };
	// rama classic-en-linea: el menu tiene 6 opciones; la sexta va debajo de las 5 de la puerta (con su caja dibujada)
	static float botonY(int k) { return k < 5 ? BOTONES_Y[k] : BOTONES_Y[4] + (BOTONES_Y[4] - BOTONES_Y[3]) * (float)(k - 4); }
	static constexpr float HORUS_X = 1911.0f, HORUS_Y = 1790.0f;   // el ojo de Horus de abajo
	static constexpr float LATIDO = 1.2f;                           // s por latido

	bool cargarImagenes(int desde, int hasta, const char* const* nombres)
	{
		for (int k = desde; k < hasta; k++) {
			ImagenPortada& im = portadaImg[(size_t)k];
			if (!cargarConMips(rutaAssets + "ui/" + nombres[k - desde], im.tex, true)) {
				for (int j = desde; j < k; j++) portadaImg[(size_t)j].tex.destroy();
				return false;
			}
			if (im.set == VK_NULL_HANDLE) {
				im.set = nuevoSet(im.tex);
			} else {
				apuntarSet(im.set, im.tex);
			}
		}
		return true;
	}

	// al empezar el cuadro (actualizar): carga al entrar en la portada (y el menu, un cuadro despues), libera la
	// portada al salir (cuando la GPU ya no la usa), desvanece el velo negro del menu y funde el boton encendido
	void cargarPortada(float dt)
	{
		if (!ctl) return;
		const control::VistaDuelo& vd = ctl->vista;
		const bool en = vd.pantalla == control::VistaDuelo::PORTADA, menu = vd.pantalla == control::VistaDuelo::MENU;
		if (!en) portadaNegro = std::max(0.0f, portadaNegro - dt / 0.5f);
		if (en && !portadaCargada && !portadaFalta) {
			static const char* nombres[3] = { "portada.jpg", "portada_letras.png", "portada_brillo.png" };
			portadaFalta = !cargarImagenes(PI_FONDO, PI_BRILLO + 1, nombres);
			portadaCargada = !portadaFalta;
		} else if ((menu || (en && vd.t > 0.1f)) && !menuCargado && !menuFalta) {
			static const char* nombres[4] = { "menu_fondo.png", "menu_letras.png", "menu_brillo.png", "menu_sel.png" };
			menuFalta = !cargarImagenes(MI_FONDO, MI_SEL + 1, nombres);
			menuCargado = !menuFalta;
		}
		if (en) portadaUso = cuadroN;
		if (!en && portadaCargada && portadaUso + (uint64_t)maxConcurrentFrames + 2 < cuadroN) {
			for (int k = PI_FONDO; k <= PI_BRILLO; k++) portadaImg[(size_t)k].tex.destroy();
			portadaCargada = false;
		}
		for (int k = 0; k < (int)menuEncendido.size(); k++) {
			const float obj = (menu && !vd.menuEnOpciones && k == vd.menuOpcion) ? 1.0f : 0.0f;
			menuEncendido[(size_t)k] += glm::clamp(obj - menuEncendido[(size_t)k], -dt / 0.15f, dt / 0.15f);
		}
	}

	// latido: dos pulsos (fuerte y otro mas suave) y reposo, 0..1
	static float latido(float t)
	{
		const float f = fmodf(t, LATIDO) / LATIDO;
		auto pulso = [](float x, float c, float w) { return expf(-((x - c) / w) * ((x - c) / w)); };
		return std::min(1.0f, pulso(f, 0.10f, 0.055f) + 0.6f * pulso(f, 0.30f, 0.065f));
	}

	// la imagen de 3840x2160 cubre la pantalla (si no es 16:9, recorta lo que sobra)
	float escImagen() const { return std::max(W / PORTADA_W, H / PORTADA_H); }
	glm::vec2 aPantalla(float x, float y) const
	{
		const float e = escImagen();
		return glm::vec2(W, H) * 0.5f + (glm::vec2(x, y) - glm::vec2(PORTADA_W, PORTADA_H) * 0.5f) * e;
	}
	void imagenPortada(int k, glm::vec2 centro, glm::vec2 tam, glm::vec4 col, float destello, float alfaImagen)
	{
		const vks::Texture2D& tx = portadaImg[(size_t)k].tex;
		const glm::vec2 m(0.5f / (float)tx.width, 0.5f / (float)tx.height);
		usar(portadaImg[(size_t)k].set, false);
		rect(centro, tam, 0.0f, 0.0f, IMAGEN, col, NADA, 0.0f, glm::vec4(0.0f, destello, 0.0f, alfaImagen), m, glm::vec2(1.0f) - m);
	}
	void imagenCompleta(int k, float alfa)
	{
		imagenPortada(k, glm::vec2(W, H) * 0.5f, glm::vec2(PORTADA_W, PORTADA_H) * escImagen(), glm::vec4(1.0f, 1.0f, 1.0f, alfa), 0.0f, 0.0f);
	}
	// "CLASSIC REMASTERED" palpitando: glow dorado detras (sube y baja, un poco mas grande en el pulso) y las letras
	// (escala 1.00 -> 1.04, brillo y el destello del shader); chispas doradas en las puntas del latido
	void letrasLatiendo(int kLetras, int kBrillo, RectImg L, RectImg B, float t, float alfa)
	{
		const float lt = latido(t), e = escImagen();
		imagenPortada(kBrillo, aPantalla(B.x + B.w * 0.5f, B.y + B.h * 0.5f), glm::vec2(B.w, B.h) * e * (1.0f + 0.08f * lt),
		              glm::vec4(glm::vec3(1.0f), (0.18f + 0.62f * lt) * alfa), 0.0f, 2.0f);
		imagenPortada(kLetras, aPantalla(L.x + L.w * 0.5f, L.y + L.h * 0.5f), glm::vec2(L.w, L.h) * e * (1.0f + 0.04f * lt),
		              glm::vec4(glm::vec3(1.0f + 0.32f * lt), alfa), 1.0f, 2.0f);
		usar(setAtlas, false);
		for (int k = 0; k < 6; k++) {
			const float fx = 0.08f + 0.168f * (float)k;
			const glm::vec2 p = aPantalla(L.x + L.w * fx, L.y + L.h * (k % 2 ? 0.2f : 0.85f));
			rect(p, glm::vec2(70.0f * e * (0.6f + lt)), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(ORO), 0.55f * lt * alfa), NADA, 0.0f, glm::vec4(0.8f, 0.0f, 0.0f, 0.0f));
		}
	}

	// el fondo del menu principal y de Opciones (menu_ui.h); false si faltan las imagenes
	bool fondoMenu(float t, float alfa, bool botonEncendido)
	{
		if (!menuCargado) return false;
		const float e = escImagen();
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, alfa), NADA, 0.0f, glm::vec4(0.0f));
		imagenCompleta(MI_FONDO, alfa);
		letrasLatiendo(MI_LETRAS, MI_BRILLO, MENU_LETRAS, MENU_BRILLO, t, alfa);
		// el ojo de Horus: un brillo morado y dorado que respira despacio (solo suma luz)
		const float r = 0.5f + 0.5f * sinf(t * 1.7f);
		rect(aPantalla(HORUS_X, HORUS_Y), glm::vec2(420.0f * e * (0.9f + 0.15f * r)), 0.0f, 0.0f, PARTICULA, glm::vec4(0.75f, 0.45f, 1.0f, (0.22f + 0.22f * r) * alfa), NADA, 0.0f, glm::vec4(0.0f));
		rect(aPantalla(HORUS_X, HORUS_Y - 6.0f), glm::vec2(150.0f * e), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(ORO), (0.25f + 0.3f * r) * alfa), NADA, 0.0f, glm::vec4(0.3f, 0.0f, 0.0f, 0.0f));
		// el marco encendido del boton elegido (pulsa) y su fundido al mover la cruz
		if (botonEncendido) {
			const float pulso = 0.82f + 0.18f * sinf(t * 4.2f);
			for (int k = 0; k < 6; k++) {
				const float g = menuEncendido[(size_t)k];
				if (g <= 0.001f) continue;
				const glm::vec2 c = aPantalla((BOTON_X0 + BOTON_X1) * 0.5f, botonY(k) + BOTON_ALTO * 0.5f);
				const glm::vec2 tam = glm::vec2(BOTON_X1 - BOTON_X0 + 2.0f * SEL_MARGEN, BOTON_ALTO + 2.0f * SEL_MARGEN) * e;
				imagenPortada(MI_SEL, c, tam, glm::vec4(glm::vec3(1.0f + 0.25f * pulso), g * pulso * alfa), 0.0f, 2.0f);
				usar(setAtlas, false);
			}
		}
		usar(setAtlas, false);
		return true;
	}

	void dibujarPortada()
	{
		if (!ctl) return;
		const control::VistaDuelo& vd = ctl->vista;
		if (vd.pantalla != control::VistaDuelo::PORTADA) {
			if (portadaNegro > 0.0f) {   // el menu (o lo que venga) aparece desde negro
				usar(setAtlas, false);
				caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, portadaNegro), NADA, 0.0f, glm::vec4(0.0f));
			}
			return;
		}
		rz.tapar();   // rama classic-pc: un clic en cualquier parte = X
		zBoton(0.0f, 0.0f, W, H, control::B_X);
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		if (portadaCargada) {
			imagenCompleta(PI_FONDO, 1.0f);
			letrasLatiendo(PI_LETRAS, PI_BRILLO, PORTADA_LETRAS, PORTADA_BRILLO, vd.t, 1.0f);
		} else {
			texto(F_NUMEROS, "YU-GI-OH! CLASSIC REMASTERED", W * 0.5f, H * 0.5f, 90.0f * s * (1.0f + 0.04f * latido(vd.t)), BLANCO, ORO, glm::vec3(0.7f, 0.0f, 0.9f), 1, 3.0f);
		}
		usar(setAtlas, false);
		// "Pulsa X para comenzar": banda oscura abajo y el texto que parpadea (despues del fundido de entrada)
		const float aTxt = suave(1.0f, 1.8f, vd.t);
		if (aTxt > 0.0f) {
			const float parpadeo = 0.35f + 0.65f * (0.5f + 0.5f * cosf(vd.t * 3.6f));
			caja(0.0f, H * 0.86f, W, H * 0.14f, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f * aTxt), NADA, 0.0f, glm::vec4(0.0f));
			// (rama classic-visual-final) placa oscura fija detras, sombra y contorno: se lee sobre cualquier parte del arte;
			// el pulso queda en el texto y en el brillo del borde de la placa
			std::string txt = idioma::tr("Pulsa X para comenzar");
			if (dispositivo == 1) txt = idioma::ingles() ? "Click or press Enter to start" : "Haz clic o pulsa Enter para comenzar";   // rama classic-pc
			else if (dispositivo == 2) txt = idioma::ingles() ? "Press A to start" : "Pulsa A para comenzar";
			const float tam = 46.0f * s, tw = medir(F_NUMEROS, txt, tam, 2.0f), pw = tw + 110.0f * s, ph = 78.0f * s;
			const float cy = H * 0.93f;
			caja(W * 0.5f - pw * 0.5f, cy - ph * 0.5f, pw, ph, 18.0f * s, PANEL, glm::vec4(0.02f, 0.015f, 0.03f, 0.88f * aTxt),
			     glm::vec4(glm::vec3(ORO) * (0.45f + 0.55f * parpadeo), aTxt), ph * 0.5f, glm::vec4(1.6f * s, 1.0f + parpadeo, 0.0f, 16.0f * s));
			const float vis = 0.55f + 0.45f * parpadeo;
			texto(F_NUMEROS, txt, W * 0.5f + 3.0f * s, cy + 14.0f * s + 4.0f * s, tam, glm::vec4(0.0f, 0.0f, 0.0f, 0.85f * aTxt * vis), NADA, glm::vec3(0.0f), 1, 2.0f);
			texto(F_NUMEROS, txt, W * 0.5f, cy + 14.0f * s, tam, glm::vec4(1.0f, 0.96f, 0.84f, aTxt * vis), glm::vec4(glm::vec3(ORO), aTxt * vis),
			      glm::vec3(0.5f, 0.16f, 0.5f), 1, 2.0f);
		}
		// fundido desde negro al entrar y a negro al salir
		const float negro = std::max(portadaNegro, 1.0f - suave(0.0f, 1.2f, vd.t));
		if (negro > 0.0f) caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, negro), NADA, 0.0f, glm::vec4(0.0f));
	}

public:
