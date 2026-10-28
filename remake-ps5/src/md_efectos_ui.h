	// Rama classic-ps5-efectos: efectos de cartas de Master Duel TRADUCIDOS a nuestro C++ (se incluye DENTRO de la clase
	// Interfaz, interfaz.h). Pedido del usuario (PS5): «las espadas de luces reveladoras no se entera uno que llegaron»,
	// «a las cartas activas le dejas esta barra amarilla horrible encima», «replica al de Steam: es el mismo juego».
	// docs/md_efectos.md: cada funcion de aca con su clase / prefab / Timeline de MD.
	//
	//  - haloActivaMD: el brillo de la carta que se puede activar (MD fxp_HL_Select_trpmgc_001, shader fxs_HL_Rollover_001):
	//    la LUZ AMARILLA (1,1,0) QUE SALE DEL BORDE de la carta, tendida en la mesa con ella (perspectiva de la mesa), nunca
	//    encima de la carta. Su ParticleSystem "Light": un marco cada 0.2 s que vive 0.6 s con alfa 0 -> 1 (20 %) -> 0.
	//    (Antes: el sprite md_brillo_marco de md_ui.png, que es la MASCARA llena de EfDuelHighlight00_01_01, dibujado como
	//    imagen en pantalla -> el rectangulo amarillo solido sobre cada magia activa de la foto de la PS5.)
	//  - Swords of Revealing Light: CardRunEffect (CardRunEffectSetting mrk 4354: Ef04354 si la activa el jugador, Ef04354Op
	//    si la activa el rival; SE_EV_GOFUKEN; waitFinish) con su Timeline (1.333 s) y su prefab, pieza por pieza:
	//      Black: la pantalla se oscurece (SpriteRenderer negro, alfa 0 -> 0.5 en 0.2 s, se va de 1.15 a 1.333 s; curva del
	//             AnimationClip "Recorded" del Timeline);
	//      SwordsOfRevealingLight00..05: 6 estelas (trail con SwordsOfRevealingLight01, velocidad 500, gravedad 600, vida
	//             0.3 s, estela 0.2 s) que caen desde arriba de la camara, las de afuera primero (0 / 0.033 / 0.067 s);
	//      SORL_CenterArrow00..05: las 6 espadas clavadas en la linea del medio de la mesa, en arco hacia el lado de quien la
	//             activa (x = +-4.3, +-13.65, +-22.35; z 1.1 / -0.1 / -2.9 de MD), desde 0.25 / 0.30 / 0.333 s durante 0.9 s
	//             (alfa 1 hasta el 80 % y se apaga), color (1,1,0.75), alto = una carta (malla Eff_SRL_01 x 3.2);
	//        Flash: la espada de luz naranja (1,0.594,0) 0.05 s al clavarse (EfSwordsofRevealingLight02, 1.88 x 3.89);
	//        ContactArea: la luz dorada (0.65,0.57,0.09) en el suelo (EFF_flrstr_03_TEX, 10-12, giro +-10 grados) que
	//             titila (sizeOverLifetime 1 <-> 0.7) 1 s;
	//        particle: 20 estrellas (fxt_ple_005, cono de 20 grados radio 4, velocidad 12, gravedad 4, vida 0.2-0.8 s,
	//             tamanno 1-5 que titila) que saltan del punto donde se clava.
	//    Unidades de MD -> mesa: el alto de carta (ritmo::MD_ALTO_CARTA = 8 -> CARTA_ALTO) y, a lo ancho, el paso de zonas
	//    (MD ~8.8 -> el nuestro 2.7 * ESCALA): la unica adaptacion es nuestro tablero.
	// Los sprites: <assets>/md_ritmo/md_efectos.png (herramientas/md_ritmo/exportar_efectos.py, fuera del repo). Sin ellos
	// no se dibuja ni una barra: el halo cae a un borde fino y las espadas a la luz dibujada.

	vks::Texture2D texEfMD;
	VkDescriptorSet setEfMD{ VK_NULL_HANDLE };
	clipsmd::Atlas atlasEfMD;
	bool efMDCargado{ false };
	int spritesEfMDPedidosSinTextura{ 0 };   // (las pruebas lo leen: un sprite de MD pedido sin su textura en el titulo)
	std::string ultimoSpriteEfMDSinTextura;

	void cargarEfectosMD()
	{
		if (efMDCargado) return;
		efMDCargado = true;
		const std::string ruta = rutaAssets + "md_ritmo/md_efectos.png";
		if (cargarConMips(ruta, texEfMD, false)) {
			setEfMD = nuevoSet(texEfMD);
			atlasEfMD.cargar(rutaAssets + "md_ritmo/md_efectos_atlas.txt", (float)texEfMD.width, (float)texEfMD.height);
			fprintf(stderr, "[interfaz] efectos de Master Duel: %zu sprites (%s)\n", atlasEfMD.sprites.size(), ruta.c_str());
		} else {
			fprintf(stderr, "[interfaz] sin los efectos de Master Duel (%s): halo y espadas dibujados\n", ruta.c_str());
		}
	}
	// el sprite n del atlas de efectos (uv con medio texel hacia dentro); false si no esta la textura o el sprite
	bool uvEfMD(const char* n, glm::vec2& uv0, glm::vec2& uv1)
	{
		cargarEfectosMD();
		const clipsmd::Sprite* sp = setEfMD != VK_NULL_HANDLE ? atlasEfMD.sprite(n) : nullptr;
		if (!sp) {
			if (spritesEfMDPedidosSinTextura++ == 0) fprintf(stderr, "[interfaz] AVISO: sprite de Master Duel '%s' pedido sin su textura en el titulo\n", n);
			ultimoSpriteEfMDSinTextura = n;
			return false;
		}
		uv0 = glm::vec2((sp->x + 0.5f) / atlasEfMD.ancho, (sp->y + 0.5f) / atlasEfMD.alto);
		uv1 = glm::vec2((sp->x + sp->w - 0.5f) / atlasEfMD.ancho, (sp->y + sp->h - 0.5f) / atlasEfMD.alto);
		return true;
	}
	// un sprite de luz por 4 puntos del mundo (0 arriba-izq, 1 arriba-der, 2 abajo-der, 3 abajo-izq): suma luz (LUZ)
	bool luzMundoMD(const char* n, const glm::vec3 p[4], glm::vec4 col, bool en3D = true)
	{
		glm::vec2 uv0, uv1;
		if (!uvEfMD(n, uv0, uv1)) return false;
		const glm::vec2 l[4] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
		const glm::vec2 uv[4] = { uv0, { uv1.x, uv0.y }, uv1, { uv0.x, uv1.y } };
		glm::vec4 c[4];
		for (int i = 0; i < 4; i++) c[i] = proyectar(p[i]);
		usar(setEfMD, en3D);
		quad(c, l, uv, col, glm::vec4(0.0f), glm::vec4(1.0f, 1.0f, 0.0f, (float)LUZ), glm::vec4(0.0f));
		usar(setAtlas, en3D);
		return true;
	}
	// tendido en la mesa (centro, tamanno en el mundo: x a lo ancho, y a lo largo de la mesa; ang alrededor de la vertical)
	bool luzSueloMD(const char* n, glm::vec3 c, glm::vec2 tam, float ang, glm::vec4 col)
	{
		const glm::vec3 a(cosf(ang) * tam.x * 0.5f, 0.0f, -sinf(ang) * tam.x * 0.5f), b(sinf(ang) * tam.y * 0.5f, 0.0f, cosf(ang) * tam.y * 0.5f);
		const glm::vec3 p[4] = { c - a - b, c + a - b, c + a + b, c - a + b };
		return luzMundoMD(n, p, col);
	}
	// en pantalla (px), girado ang
	bool luzPantallaMD(const char* n, glm::vec2 c, glm::vec2 tam, float ang, glm::vec4 col)
	{
		glm::vec2 uv0, uv1;
		if (!uvEfMD(n, uv0, uv1)) return false;
		usar(setEfMD, false);
		rect(c, tam, ang, 0.0f, LUZ, col, glm::vec4(0.0f), 0.0f, glm::vec4(0.0f), uv0, uv1);
		usar(setAtlas, false);
		return true;
	}

	// ------------------------------------------------------------------ el brillo de la carta activa / que se activa
	// fxp_HL_Select_trpmgc_001 > trpmgc > Light: ParticleSystem en bucle (duration 0.2), una particula por ciclo, vida 0.6 s,
	// colorOverLifetime alfa 0 -> 1 (0.2) -> 0 (1.0); material fxm_HL_Select_light_001 (fxs_HL_Rollover_001, _Color (1,1,0)).
	// Lo que se ve es la suma de las 3 vivas. "fuerza": 1 = como MD
	static float alfaRolloverMD(float u) { return u < 0.2f ? u / 0.2f : 1.0f - (u - 0.2f) / 0.8f; }
	float pulsoRolloverMD(float fase) const
	{
		float suma = 0.0f;
		for (int j = 0; j < 3; j++) suma += alfaRolloverMD(fmodf(t + fase + 0.2f * (float)j, 0.6f) / 0.6f);
		return suma;
	}
	void haloActivaMD(int z, float fuerza)
	{
		if (fuerza <= 0.0f) return;
		const bool rival = strncmp(ZONAS[z].id, "rival", 5) == 0;
		const float a = pulsoRolloverMD(0.07f * (float)z) / 1.5f;   // (la suma de las 3 vale ~1.5 en promedio)
		const glm::vec4 col(glm::vec3(1.0f, 1.0f, 0.0f) * 1.2f * a * fuerza, 1.0f);
		// la textura: 512 px con la carta de 318 x 457 (EfDuelHighlight00_01_01): el halo, del tamanno de la carta tendida
		const glm::vec2 tam(CARTA_ANCHO * 512.0f / 318.0f, CARTA_ALTO * 512.0f / 457.0f);
		if (luzSueloMD("halo_activa", centroZona(z) + glm::vec3(0.0f, 0.058f, 0.0f), tam, rival ? glm::pi<float>() : 0.0f, col)) return;
		// (sin el atlas de efectos: un borde fino alrededor de la carta, sin relleno)
		const glm::vec3 c = centroZona(z) + glm::vec3(0.0f, 0.058f, 0.0f);
		const float g = 0.035f, ax = CARTA_ANCHO * 0.5f + g, az = CARTA_ALTO * 0.5f + g;
		const glm::vec4 borde(glm::vec3(1.0f, 1.0f, 0.0f) * 0.7f * a * fuerza, 1.0f);
		usar(setAtlas, true);
		for (int k = 0; k < 4; k++) {
			const bool lado = k < 2;
			const float s0 = k % 2 ? 1.0f : -1.0f;
			const glm::vec3 m = lado ? glm::vec3(s0 * ax, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, s0 * az);
			rectSuelo(c + m, lado ? glm::vec2(g, az * 2.0f) : glm::vec2(ax * 2.0f, g), 0.0f, LUZ, borde, glm::vec4(0.0f), 0.0f, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}
	}

	// ------------------------------------------------------------------ CardRunEffect: Swords of Revealing Light
	struct EfectoMD { int tipo{ 0 }; bool rival{ false }; float t{ 0.0f }; };
	enum { EF_MD_ESPADAS = 0 };
	std::vector<EfectoMD> efectosMD;
	static constexpr float T_ESPADAS_MD = 1.3333f;   // (Ef04354Timeline: la ultima pista termina a 0.333 + 1.0)

	void lanzarEspadasMD(bool rival) { efectosMD.push_back({ EF_MD_ESPADAS, rival, 0.0f }); }
	void avanzarEfectosMD(float dt)
	{
		avanzarCardRunMD(dt);   // (md_cardrun_ui.h)
		for (EfectoMD& e : efectosMD) e.t += dt;
		efectosMD.erase(std::remove_if(efectosMD.begin(), efectosMD.end(), [](const EfectoMD& e) { return e.t > T_ESPADAS_MD + 0.05f; }), efectosMD.end());
	}
	bool hayEfectoMD() const { return !efectosMD.empty() || hayCardRunMD(); }

	// una espada: de su prefab (posiciones locales de MD) y de su pista del Timeline
	struct EspadaMD { float x, z, llega; float ex, ey, ez, sale; };
	static const EspadaMD* espadasMD(bool op)
	{
		// Ef04354 (la activa el jugador): CenterArrow (x, z, inicio) y la estela que le cae (SwordsOfRevealingLight: x, y, z, inicio)
		static const EspadaMD J[6] = { { -4.4f, 1.1f, 0.333f, -0.6f, 84.6f, -35.6f, 0.067f }, { 4.2f, 1.1f, 0.333f, 0.6f, 84.6f, -35.6f, 0.067f },
			                           { -14.0f, -0.1f, 0.300f, -1.5f, 84.6f, -35.6f, 0.033f }, { 13.3f, -0.1f, 0.300f, 1.5f, 84.6f, -35.6f, 0.033f },
			                           { 22.1f, -2.9f, 0.250f, 3.37f, 84.6f, -35.6f, 0.0f }, { -22.6f, -2.9f, 0.250f, -3.37f, 84.6f, -35.6f, 0.0f } };
		// Ef04354Op (la activa el rival): las mismas espadas (z -1.0 en las del medio) y otras estelas; la raiz va girada 180
		// grados y corrida -1.6 en z
		static const EspadaMD R[6] = { { -4.4f, 1.1f, 0.32f, -4.8f, 39.1f, -7.4f, 0.07f }, { 4.2f, 1.1f, 0.32f, 4.8f, 39.1f, -7.4f, 0.07f },
			                           { -14.0f, -1.0f, 0.28f, -11.2f, 39.2f, -7.9f, 0.03f }, { 13.3f, -1.0f, 0.28f, 11.2f, 39.2f, -7.9f, 0.03f },
			                           { 22.1f, -2.9f, 0.25f, 15.3f, 38.9f, -5.4f, 0.0f }, { -22.6f, -2.9f, 0.25f, -17.06f, 38.66f, -7.75f, 0.0f } };
		return op ? R : J;
	}
	// de MD (local del prefab) a la mesa: x por el paso de zonas, alto y z por el alto de carta; la de MD mira hacia +z
	// desde el jugador (su camara esta en -z) y la nuestra al reves
	glm::vec3 mesaDesdeMD(float x, float y, float z, bool op) const
	{
		// (rama classic-ps5-efectos2: la misma escala uniforme que las pistas, U_MD_MESA, para que la malla de la espada, la
		// estela, el destello y la luz del suelo caigan en el mismo lugar)
		const float u = 2.7f * ESCALA / 8.8f;
		if (op) return glm::vec3(-x * u, y * u, (z + 1.6f) * u);   // (raiz girada 180 y corrida -1.6)
		return glm::vec3(x * u, y * u, -z * u);
	}
	static float tablaMD(const float* k, int n, float u)   // curva por puntos (t, v) lineal
	{
		if (u <= k[0]) return k[1];
		for (int i = 1; i < n; i++)
			if (u <= k[2 * i]) return glm::mix(k[2 * i - 1], k[2 * i + 1], (u - k[2 * i - 2]) / glm::max(k[2 * i] - k[2 * i - 2], 1e-4f));
		return k[2 * n - 1];
	}
	static float azarMD(uint32_t s)
	{
		s = s * 747796405u + 2891336453u;
		s = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
		return (float)((s >> 22u) ^ s) / 4294967295.0f;
	}
	// cuantos px mide en pantalla un largo L del mundo en el punto p (a lo ancho)
	float pxMundo(glm::vec3 p, float L) const { return glm::length(aPantalla(proyectar(p + glm::vec3(L, 0.0f, 0.0f))) - aPantalla(proyectar(p))); }

	void dibujarEspadasMD(const EfectoMD& e)
	{
		const float te = e.t;
		const bool op = e.rival;
		PistaMD* malla = pistaMD(op ? "ef04354op_espadas" : "ef04354_espadas");
		const bool espadasMalla = malla != nullptr;
		const float pi = glm::pi<float>();
		// Black: alfa 0 -> 0.5 (0.2 s) ... -> 0 (1.15 -> 1.333 s)
		const float negro = 0.5f * glm::smoothstep(0.0f, 0.2f, te) * (1.0f - glm::smoothstep(1.15f, T_ESPADAS_MD, te));
		if (negro > 0.0f) {
			usar(setAtlas, false);
			rect(glm::vec2(W, H) * 0.5f, glm::vec2(W, H) + glm::vec2(4.0f), 0.0f, 0.0f, LUZ, glm::vec4(0.0f, 0.0f, 0.0f, negro), glm::vec4(0.0f), 0.0f,
			     glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
		}
		if (malla) dibujarPistaMD(*malla, te, glm::vec3(0.0f), 1.0f);   // (las 6 espadas clavadas de MD, encima del Black)
		const float k = CARTA_ALTO / ritmo::MD_ALTO_CARTA;
		const EspadaMD* E = espadasMD(op);
		for (int i = 0; i < 6; i++) {
			const EspadaMD& s = E[i];
			const glm::vec3 base = mesaDesdeMD(s.x, 0.0f, s.z, op);
			// la estela que cae (llega cuando aparece la espada)
			if (te >= s.sale && te < s.llega + 0.2f) {
				const glm::vec3 desde = mesaDesdeMD(s.ex, s.ey, s.ez, op);
				auto en = [&](float tt) {
					const float x = glm::clamp((tt - s.sale) / glm::max(s.llega - s.sale, 1e-3f), 0.0f, 1.0f);
					return glm::mix(desde, base, x * (0.7f + 0.3f * x));   // (500 de velocidad + 600 de gravedad: acelera un poco)
				};
				const glm::vec2 cab = aPantalla(proyectar(en(te))), cola = aPantalla(proyectar(en(glm::max(te - 0.2f, s.sale))));
				const glm::vec2 d = cab - cola;
				const float largo = glm::length(d);
				if (largo > 2.0f) {
					const float ancho = glm::clamp(pxMundo(en(te), 0.55f), 10.0f * s_(), 80.0f * s_());
					// (la textura: la punta a la izquierda: va adelante)
					luzPantallaMD("espada_estela", (cab + cola) * 0.5f, glm::vec2(largo, ancho), atan2f(-d.y, -d.x), glm::vec4(1.0f, 1.0f, 0.85f, 1.0f));
				}
			}
			const float u = te - s.llega;
			if (u < 0.0f) continue;
			// ContactArea (1.0 s): la luz dorada en el suelo que titila
			if (u < 1.0f) {
				static const float TIT[] = { 0.0f, 1.0f, 0.06f, 0.73f, 0.11f, 1.0f, 0.17f, 0.72f, 0.22f, 1.0f, 0.28f, 0.72f, 0.36f, 1.0f, 0.40f, 0.72f, 0.45f, 1.0f, 0.53f, 0.73f,
					                         0.62f, 1.0f, 0.67f, 0.69f, 0.74f, 1.0f, 0.79f, 0.69f, 0.84f, 1.0f, 0.87f, 0.72f, 0.94f, 1.0f, 0.98f, 0.71f, 1.0f, 1.0f };
				const float tam = (10.0f + 2.0f * azarMD((uint32_t)i * 13u + 1u)) * k * tablaMD(TIT, 19, u);
				const float a = u < 0.8f ? 1.0f : 1.0f - (u - 0.8f) / 0.2f;
				const float giro = (azarMD((uint32_t)i * 7u + 3u) - 0.5f) * 2.0f * 0.175f;
				if (!luzSueloMD("suelo_luz", base + glm::vec3(0.0f, 0.06f, 0.0f), glm::vec2(tam), giro, glm::vec4(glm::vec3(0.65f, 0.57f, 0.09f) * a, 1.0f))) {
					usar(setAtlas, false);
					rect(aPantalla(proyectar(base)), glm::vec2(pxMundo(base, tam)), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(0.65f, 0.57f, 0.09f) * a, 1.0f),
					     glm::vec4(0.0f), 0.0f, glm::vec4(0.0f));
				}
			}
			// la espada clavada: la malla de MD (Eff_SRL_01, material AREF_LS_01), con sus tiempos y su giro, en la pista
			// ef04354_espadas / ef04354op_espadas (se dibuja una vez para las 6, abajo). Sin la pista: la espada de luz en pie
			if (u < 0.9f && !espadasMalla) {
				const float a = u < 0.72f ? 1.0f : 1.0f - (u - 0.72f) / 0.18f;
				// (la espada ocupa el 80 % del alto del sprite; x1.3: nuestra camara ve la mesa mas lejos que la de MD. El ancho:
				// la guarda de la malla de MD mide el 43 % de su alto y en el sprite la mitad de su ancho)
				const float alto = 8.0f * k / 0.8f * 1.3f;
				const float ancho = alto * 0.8f;
				const glm::vec3 pie = base + glm::vec3(0.0f, -1.12f * k - alto * 0.1f, 0.0f), arriba(0.0f, alto, 0.0f), lado(ancho * 0.5f, 0.0f, 0.0f);
				const glm::vec3 p[4] = { pie + arriba - lado, pie + arriba + lado, pie + lado, pie - lado };
				if (!luzMundoMD("espada_brillo", p, glm::vec4(glm::vec3(1.0f, 1.0f, 0.749f) * a, 1.0f), false)) {
					usar(setAtlas, false);
					const glm::vec2 q0 = aPantalla(proyectar(pie)), q1 = aPantalla(proyectar(pie + arriba));
					rect((q0 + q1) * 0.5f, glm::vec2(8.0f * s_(), glm::length(q1 - q0)), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(1.0f, 1.0f, 0.75f) * a, 1.0f),
					     glm::vec4(0.0f), 0.0f, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
				}
			}
			// Flash (0.05 s): la espada de luz naranja al clavarse
			if (u < 0.05f) {
				const glm::vec3 c = base + glm::vec3(0.0f, 4.0f * k, 0.0f);
				const glm::vec3 arriba(0.0f, 3.89f * k * 0.5f, 0.0f), lado(1.88f * k * 0.5f, 0.0f, 0.0f);
				const glm::vec3 p[4] = { c + arriba - lado, c + arriba + lado, c - arriba + lado, c - arriba - lado };
				luzMundoMD("espada_brillo", p, glm::vec4(glm::vec3(1.0f, 0.594f, 0.0f) * 0.7f, 1.0f), false);
			}
			// particle: 20 estrellas del punto donde se clava (cono de 20 grados hacia arriba, radio 4)
			for (int j = 0; j < 20; j++) {
				const uint32_t sj = (uint32_t)(i * 97 + j * 31 + 5);
				const float vida = 0.2f + 0.6f * azarMD(sj);
				if (u >= vida) continue;
				const float ang = 2.0f * pi * azarMD(sj + 1u), r = 4.0f * k * sqrtf(azarMD(sj + 2u));
				const float abre = glm::radians(20.0f) * (r / (4.0f * k));
				const glm::vec3 dir(sinf(abre) * cosf(ang), cosf(abre), sinf(abre) * sinf(ang));
				const glm::vec3 p0 = base + glm::vec3(r * cosf(ang), 1.0f * k, r * sinf(ang));
				const glm::vec3 p = p0 + dir * (12.0f * k * u) - glm::vec3(0.0f, 0.5f * 4.0f * 9.81f * k * u * u, 0.0f);
				static const float TAM[] = { 0.0f, 1.0f, 0.24f, 0.11f, 0.48f, 0.96f, 0.88f, 0.21f, 1.0f, 1.0f };
				const float tam = (1.0f + 4.0f * azarMD(sj + 3u)) * k * tablaMD(TAM, 5, u / vida);
				const glm::vec2 q = aPantalla(proyectar(p));
				const float px = glm::max(pxMundo(p, tam), 2.0f);
				if (!luzPantallaMD("estrella", q, glm::vec2(px), 0.0f, glm::vec4(1.0f, 1.0f, 0.836f, 1.0f))) {
					usar(setAtlas, false);
					rect(q, glm::vec2(px * 0.5f), 0.0f, 0.0f, PARTICULA, glm::vec4(1.0f, 1.0f, 0.836f, 1.0f), glm::vec4(0.0f), 0.0f, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
				}
			}
		}
		usar(setAtlas, false);
	}
	float s_() const { return s; }

	// desde construir(): despues de las cartas de la mesa y antes de la mano y del HUD (el Black de MD tapa la escena 3D,
	// no la interfaz)
	void dibujarEfectosMD()
	{
		dibujarCardRunMD();   // (md_cardrun_ui.h: Raigeki, Mirror Force, Dark Hole...)
		for (const EfectoMD& e : efectosMD)
			if (e.tipo == EF_MD_ESPADAS) dibujarEspadasMD(e);
	}
