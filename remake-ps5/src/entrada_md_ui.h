// Yugioh Classic Remaster (rama classic-md-entradas): se incluye DENTRO de interfaz::Interfaz (desde entrada_clave_ui.h).
// El dibujo de las entradas de FUSION y RITUAL de Master Duel (entrada_md.h: estado y tiempos; entrada_md_juego.h: la
// deteccion y los sonidos). Todo en pantalla, delante de la mesa, como en MD (sus Timeline se arman frente a la camara):
//  - los materiales se muestran a la camara (Summon*ShowUnitCard0N, 1.333 s);
//  - fusion: fondo negro, el vortice (remolinos rojo y azul con sus colores, giros y achique de MD), los materiales que
//    caen desde arriba (la curva de SummonFusionInCard) y se retuercen hacia el centro, los rayos y anillos de
//    SummonFusionOut, el estallido de PostFusion y, en el StrongSummon, el monstruo de fusion;
//  - ritual: los cinco circulos de RitualCircle (sus colores, giros y tamannos), las ocho llamas una tras otra con su
//    reflejo, las chispas, los materiales que entran al circulo (RitualTrailIn), la columna de luz, el estallido de
//    PostRitual y, en el StrongSummon, el monstruo ritual.
// Los sprites son los de MD (md_ui/md_entradas.png, herramientas/md_fases/exportar_entradas.py; no van al repo); sin
// ellos se dibuja lo mismo con las primitivas propias (halos y lineas). Con carta clave, la secuencia se apaga en el
// StrongSummon y sigue el corte de entrada_clave_ui.h (sin repetir el nombre).

private:
	vks::Texture2D texEntradasMD;
	VkDescriptorSet setEntradasMD{ VK_NULL_HANDLE };
	bool entradasMDCargadas{ false };
	clipsmd::Atlas atlasEntradasMD;

	void cargarEntradasMD()
	{
		if (entradasMDCargadas) return;
		entradasMDCargadas = true;
		if (cargarConMips(rutaAssets + "md_ui/md_entradas.png", texEntradasMD, false)) {
			setEntradasMD = nuevoSet(texEntradasMD);
			atlasEntradasMD.cargar(rutaAssets + "md_ui/md_entradas_atlas.txt", (float)texEntradasMD.width, (float)texEntradasMD.height);
		} else {
			fprintf(stderr, "[interfaz] sin los sprites de fusion/ritual de Master Duel (%smd_ui/md_entradas.png): halos dibujados\n", rutaAssets.c_str());
		}
	}
	// un sprite de md_entradas.png (mascara blanca con alfa) tenido con col, centrado en c, de lado "tam" px
	bool spriteEntradaMD(const char* nombre, glm::vec2 c, glm::vec2 tam, glm::vec4 col, float ang = 0.0f)
	{
		if (setEntradasMD == VK_NULL_HANDLE || col.a <= 0.003f) return setEntradasMD != VK_NULL_HANDLE;
		const clipsmd::Sprite* sp = atlasEntradasMD.sprite(nombre);
		if (!sp) return false;
		const clipsmd::Atlas& a = atlasEntradasMD;
		usar(setEntradasMD, false);
		rect(c, tam, ang, 0.0f, IMAGEN, glm::vec4(glm::min(glm::vec3(col), glm::vec3(4.0f)), col.a), NADA, 0.0f, glm::vec4(0.0f, 0.0f, 0.0f, 2.0f),
		     glm::vec2((sp->x + 0.5f) / a.ancho, (sp->y + 0.5f) / a.alto), glm::vec2((sp->x + sp->w - 0.5f) / a.ancho, (sp->y + sp->h - 0.5f) / a.alto));
		usar(setAtlas, false);
		return true;
	}
	// un halo (respaldo sin sprites, y las luces suaves)
	void haloMD(glm::vec2 c, float r, glm::vec3 col, float a)
	{
		if (a <= 0.003f) return;
		usar(setAtlas, false);
		rect(c, glm::vec2(r * 2.0f), 0.0f, 0.0f, PARTICULA, glm::vec4(col * a, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
	}
	// una carta (cara grande si esta cargada; si no, el dorso). angX: inclinacion hacia atras (la caida de la fusion)
	void cartaEntradaMD(const std::string& id, glm::vec2 c, glm::vec2 tam, float angZ, float angX, float alfa, glm::vec4 borde, bool rival)
	{
		if (alfa <= 0.003f) return;
		const int ci = buscarCarta(id);
		const CartaUI* k = ci >= 0 ? &cartas[(size_t)ci] : nullptr;
		if (k && listaCarta(ci)) {
			usar(setCaraGrande(k), false);
			const float uu = 0.5f / k->cara.width, vv = 0.5f / k->cara.height;
			if (angX != 0.0f)
				rectInclinado(c, tam, 0.0f, angX, 0.0f, IMAGEN, glm::vec4(1.0f, 1.0f, 1.0f, alfa), borde, 10.0f * s, glm::vec4(0.5f, 0.6f, (float)ci * 0.13f, 0.0f),
				              glm::vec2(uu, vv), glm::vec2(1.0f - uu, 1.0f - vv));
			else
				rect(c, tam, angZ, 0.0f, IMAGEN, glm::vec4(1.0f, 1.0f, 1.0f, alfa), borde, 10.0f * s, glm::vec4(0.5f, 0.6f, (float)ci * 0.13f, 0.0f), glm::vec2(uu, vv),
				     glm::vec2(1.0f - uu, 1.0f - vv));
		} else {
			Dorso dz = usarDorso(false, false, rival, 1.0f, 0.0f);
			dz.col.a *= alfa;
			dz.col2.a *= alfa;
			if (angX != 0.0f) rectInclinado(c, tam, 0.0f, angX, 0.0f, DORSO, dz.col, dz.col2, 10.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
			else rect(c, tam, angZ, 0.0f, DORSO, dz.col, dz.col2, 10.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		usar(setAtlas, false);
	}
	// donde se muestra el material i de n (en fila, al centro de la mesa) y su tamanno
	glm::vec2 tamMaterialMD(int n) const
	{
		const float ancho = (W - CAMPO_X * s) * 0.86f;
		const float alto = std::min(400.0f * s, ancho / (float)std::max(n, 1) / 0.69f / 1.12f);
		return glm::vec2(alto * 0.6875f, alto);
	}
	glm::vec2 posMaterialMD(int i, int n, glm::vec2 c) const
	{
		const glm::vec2 tam = tamMaterialMD(n);
		return c + glm::vec2(((float)i - 0.5f * (float)(n - 1)) * tam.x * 1.12f, 0.0f);
	}
	// Summon*ShowUnitCard0N: los materiales a la camara (1.333 s; la luz recorre el dorso desde 0.33 s: CardBack01)
	void muestraMaterialesMD(const entradamd::Estado& e, glm::vec2 c, float te, float salida, bool fusion, glm::vec4 acento)
	{
		const int n = (int)e.materiales.size();
		if (n == 0 || te >= entradamd::MUESTRA) return;
		const glm::vec2 tam = tamMaterialMD(n);
		const float va = entradamd::suave(entradamd::MUESTRA - 0.22f, entradamd::MUESTRA, te);
		const float rot = entradamd::suave(0.0f, 0.2f, te) * (1.0f - va) * salida;
		texto(F_TITULO, idioma::tr(fusion ? "MATERIALES DE FUSIÓN" : "SACRIFICIOS DEL RITUAL"), c.x, c.y - tam.y * 0.5f - 34.0f * s, 26.0f * s,
		      glm::vec4(glm::vec3(acento), rot), NADA, glm::vec3(0.0f), 1, 6.0f);
		for (int i = 0; i < n; i++) {
			const float ti = te - 0.05f * (float)i;
			const float entra = entradamd::suave(0.0f, 0.2f, ti);
			const float a = entra * (1.0f - va) * salida;
			glm::vec2 p = posMaterialMD(i, n, c) + glm::vec2(0.0f, (1.0f - entra) * 46.0f * s);
			if (fusion) p.y -= va * H * 0.35f;   // (sube: de ahi cae al vortice, SummonFusionInCard)
			const float luz = entradamd::suave(0.33f, 0.5f, ti) * (1.0f - entradamd::suave(0.9f, 1.33f, ti));
			haloMD(p, tam.y * 0.75f, glm::vec3(acento) * (0.25f + 0.35f * luz), a);
			cartaEntradaMD(e.materiales[(size_t)i], p, tam * (0.92f + 0.08f * entra), 0.0f, 0.0f, a, glm::vec4(glm::vec3(acento) * (0.4f + luz), 1.0f), e.lado == 1);
		}
	}
	// el StrongSummon (sin carta clave): el monstruo sale del centro con su nombre; en el StartCard se achica y se va
	void revelaMonstruoMD(const entradamd::Estado& e, glm::vec2 c, float u, float salida, glm::vec4 acento, const char* rotulo)
	{
		if (u < 0.0f || e.clave) return;
		const float ida = e.espera - e.strong;
		const float entra = entradamd::suave(0.0f, 0.3f, u);
		const float va = entradamd::suave(ida - 0.05f, ida + 0.2f, u);
		const float a = entra * (1.0f - va) * salida;
		if (a <= 0.003f) return;
		const glm::vec2 tam = glm::vec2(390.0f, 568.0f) * s * (0.25f + 0.75f * entrada::rebote(entradamd::suave(0.0f, 0.35f, u))) * (1.0f - 0.4f * va);
		haloMD(c, tam.y * 0.9f, glm::vec3(acento) * 0.7f, a);
		haloMD(c, tam.y * 0.45f, glm::vec3(1.0f) * 0.5f * expf(-u * 5.0f), 1.0f);
		cartaEntradaMD(e.carta, c + glm::vec2(0.0f, -va * 60.0f * s), tam, 0.0f, 0.0f, a, glm::vec4(glm::vec3(acento), 1.0f), e.lado == 1);
		// la banda con el nombre (nuestro estilo, como la entrada de carta clave)
		const int ci = buscarCarta(e.carta);
		const std::string nombre = ci >= 0 ? cartas[(size_t)ci].vNombre() : nombreCarta(e.carta);
		const float an = entradamd::suave(0.1f, 0.35f, u) * (1.0f - va) * salida;
		const float x0 = CAMPO_X * s, ancho = W - x0, y = H * 0.84f, alto = 104.0f * s;
		usar(setAtlas, false);
		caja(x0, y - alto * 0.5f, ancho, alto, 0.0f, PANEL, glm::vec4(0.01f, 0.012f, 0.03f, 0.72f * an), NADA, 0.0f, glm::vec4(0.0f));
		rect(glm::vec2(c.x, y - alto * 0.5f), glm::vec2(ancho * 0.9f, 3.0f * s), 0.0f, 3.0f * s, LINEA, glm::vec4(glm::vec3(acento), an), NADA, 0.0f,
		     glm::vec4(1.5f, 0.1f, 0.0f, 0.0f));
		rect(glm::vec2(c.x, y + alto * 0.5f), glm::vec2(ancho * 0.9f, 3.0f * s), 0.0f, 3.0f * s, LINEA, glm::vec4(glm::vec3(acento), an), NADA, 0.0f,
		     glm::vec4(1.5f, 0.6f, 0.0f, 0.0f));
		float tn = 58.0f * s;
		while (medir(F_TITULO, nombre, tn, 2.0f) > ancho - 120.0f * s && tn > 26.0f * s) tn -= 2.0f * s;
		std::string rot = idioma::tr(rotulo);
		if (e.lado == 1) rot += std::string("  ·  ") + idioma::tr("Rival");
		texto(F_TITULO, rot, c.x, y - alto * 0.5f + 28.0f * s, 20.0f * s, glm::vec4(glm::vec3(acento), an), NADA, glm::vec3(0.0f), 1, 6.0f);
		texto(F_TITULO, nombre, c.x, y + 30.0f * s, tn, glm::vec4(glm::vec3(BLANCO), an), glm::vec4(glm::vec3(acento), an), glm::vec3(0.9f, 0.0f, 0.55f), 1, 2.0f);
	}

	// --------------------------------------------------------------------------------------------------- fusion
	void dibujarFusionMD(const entradamd::Estado& e, glm::vec2 c, float te, float salida)
	{
		using namespace entradamd;
		const glm::vec4 acento(0.78f, 0.52f, 1.0f, 1.0f);   // (violeta de la fusion, con nuestra paleta)
		const float U = H * 0.0085f;                        // unidades de las particulas de MD -> px
		const float m = te - MUESTRA;                       // tiempo del Timeline SummonFusion0N_01
		// fondo: el velo de la muestra y el negro de MD (pista "black" 0-2.0 s); se aclara al salir el monstruo
		float velo = 0.62f * suave(0.0f, 0.15f, te);
		if (m >= 0.0f) velo = std::max(velo, 0.62f + 0.3f * suave(0.0f, 0.15f, m) * (1.0f - suave(F_STRONG, F_NEGRO, m)));
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.01f, velo * salida), NADA, 0.0f, glm::vec4(0.0f));
		muestraMaterialesMD(e, c, te, salida, true, acento);
		if (m < 0.0f) return;
		// el remolino de fondo (SummonFusionBG: fxt_Ef07445_swirl_001), lento, mientras dura el negro
		const float fondo = suave(0.0f, 0.3f, m) * (1.0f - suave(F_STRONG, F_NEGRO, m)) * salida;
		spriteEntradaMD("md_fusion_espiral", c, glm::vec2(130.0f * U), glm::vec4(0.35f, 0.22f, 0.75f, 0.45f * fondo), -0.6f * m);
		// el vortice (SummonFusionVortex 1.2 s: guruguruRed(B) y guruguruBlue(B))
		if (m <= F_VORTICE) {
			const float u = m / F_VORTICE;
			const float aR = alfaVida(u, 0.06f, 0.67f) * salida, aB = alfaVida(u, 0.15f, 0.67f) * salida;
			const float tamR = 110.0f * U * (1.0f - 0.45f * u), tamB = 101.0f * U * (1.0f - 0.36f * u);
			const bool hay = spriteEntradaMD("md_vortice_aro", c, glm::vec2(tamB), glm::vec4(0.0f, 0.29f * 1.8f, 0.6f * 1.8f, aB), 0.9f - 1.5708f * m);
			spriteEntradaMD("md_vortice_aro", c, glm::vec2(tamB * 1.02f), glm::vec4(0.0f, 0.33f * 1.8f, 0.68f * 1.8f, aB), -2.2f - 1.5708f * m);
			spriteEntradaMD("md_vortice", c, glm::vec2(tamR), glm::vec4(1.0f * 1.6f, 0.25f * 1.6f, 0.0f, aR), 3.1416f - 3.1416f * m);
			spriteEntradaMD("md_vortice", c, glm::vec2(tamR * 0.7f), glm::vec4(1.0f * 1.6f, 0.25f * 1.6f, 0.0f, 0.8f * aR), -3.1416f * m);
			if (!hay) {
				haloMD(c, tamR * 0.5f, glm::vec3(1.0f, 0.25f, 0.0f), aR);
				haloMD(c, tamB * 0.4f, glm::vec3(0.0f, 0.29f, 0.6f) * 1.8f, aB);
			}
		}
		// los materiales caen desde arriba y se retuercen hacia el centro (SummonFusionInCard0N, 1.333 s)
		const int n = (int)e.materiales.size();
		if (m <= F_ENTRA && n > 0) {
			const glm::vec2 tam = tamMaterialMD(n) * 0.85f;
			const float caida = caidaFusion(m), tor = torsionFusion(m);
			for (int i = 0; i < n; i++) {
				const glm::vec2 base = posMaterialMD(i, n, c) * glm::vec2(1.0f, 1.0f) + glm::vec2(0.0f, 0.0f);
				glm::vec2 p = glm::mix(glm::vec2(c.x + (base.x - c.x) * 0.8f, c.y), c, tor);
				p.y -= caida * H * 0.62f;
				const float a = (1.0f - tor * tor) * salida;
				const float giro = tor * 2.6f * 3.1416f * ((i & 1) ? 1.0f : -1.0f);
				const glm::vec2 t2 = tam * (1.0f - 0.82f * tor);
				haloMD(p, t2.y * 0.6f, glm::vec3(1.0f, 0.45f, 0.15f) * (0.3f + 0.6f * tor), a);
				cartaEntradaMD(e.materiales[(size_t)i], p, t2, giro, caida > 0.0f ? glm::radians(60.0f) * caida : 0.0f, a,
				               glm::vec4(glm::mix(glm::vec3(acento), glm::vec3(1.0f, 0.4f, 0.1f), tor), 1.0f), e.lado == 1);
			}
		}
		// SummonFusionOut: rayos (0.47-1.67 s) y el humo naranja del centro (centerMoyamoya 0.62-1.67 s)
		if (m >= 0.467f && m <= 1.667f) {
			const float u = (m - 0.467f) / 1.2f;
			for (int k = 0; k < 4; k++) {
				const float a = alfaVida(u, 0.08f, 0.7f) * (0.45f + 0.55f * fabsf(sinf(m * 37.0f + (float)k * 1.9f))) * salida;
				const float ang = (float)k * 1.5708f + 0.4f + 0.5f * sinf(m * 3.0f + (float)k);
				if (!spriteEntradaMD("md_fusion_rayo", c + glm::vec2(cosf(ang), sinf(ang)) * 18.0f * U, glm::vec2(34.0f * U), glm::vec4(0.47f, 0.98f, 1.0f, a) * 1.4f, ang + 1.5708f))
					haloMD(c + glm::vec2(cosf(ang), sinf(ang)) * 18.0f * U, 6.0f * U, glm::vec3(0.47f, 0.98f, 1.0f), a);
			}
		}
		if (m >= 0.617f && m <= 1.667f) {
			const float u = (m - 0.617f) / 1.05f;
			spriteEntradaMD("md_vortice", c, glm::vec2(60.0f * U * u), glm::vec4(1.0f * 1.5f, 0.33f * 1.5f, 0.0f, alfaVida(u, 0.07f, 0.79f) * salida), 0.785f * m);
		}
		// PostFusion: el estallido (Activation 0.62-1.12 s del clip que empieza a 1.17 s) y su destello
		const float te2 = m - (F_POST + F_ESTALLIDO);
		if (te2 >= 0.0f && te2 <= 0.5f) {
			const float u = te2 / 0.5f;
			const float a = (1.0f - u) * (1.0f - u) * salida;
			if (!spriteEntradaMD("md_fusion_estallido", c, glm::vec2(90.0f * U * (0.35f + 1.3f * (1.0f - (1.0f - u) * (1.0f - u)))), glm::vec4(1.5f, 1.3f, 1.8f, a)))
				haloMD(c, 40.0f * U * (0.3f + u), glm::vec3(1.0f, 0.9f, 1.0f), a);
			caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(1.0f, 0.95f, 1.0f, 0.35f * expf(-te2 * 9.0f) * salida), NADA, 0.0f, glm::vec4(0.0f));
		}
		// los anillos del final de SummonFusionOut (1.83-2.46 s)
		if (m >= 1.833f && m <= F_OUT) {
			const float u = (m - 1.833f) / (F_OUT - 1.833f);
			for (int k = 0; k < 2; k++) {
				const float uk = glm::clamp(u * 1.3f - 0.3f * (float)k, 0.0f, 1.0f);
				spriteEntradaMD("md_fusion_aro", c, glm::vec2(70.0f * U * (0.4f + 1.1f * uk)), glm::vec4(0.6f, 0.75f, 1.6f, (1.0f - uk) * 0.9f * salida), 0.6f * m + (float)k);
			}
		}
		// el StrongSummon: el monstruo de fusion
		revelaMonstruoMD(e, c, m - F_STRONG, salida, acento, "INVOCACIÓN POR FUSIÓN");
	}

	// --------------------------------------------------------------------------------------------------- ritual
	void dibujarRitualMD(const entradamd::Estado& e, glm::vec2 c, float te, float salida)
	{
		using namespace entradamd;
		const TiemposRitual& T = e.conMateriales ? RITUAL01 : RITUAL02;
		const glm::vec4 acento = CIAN;
		const float U = H * 0.011f;
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.012f, 0.8f * suave(0.0f, 0.25f, te) * salida), NADA, 0.0f, glm::vec4(0.0f));
		muestraMaterialesMD(e, c, te, salida, false, acento);
		const glm::vec2 cc = c + glm::vec2(0.0f, 10.0f * s);
		// los cinco circulos (RitualCircle01..05): sprite, inicio, vida, tamanno de MD, color, giro (rad/s)
		struct Circ { const char* sp; float t0, dur, tam; glm::vec3 col; float giro; int modo; };
		const Circ C[5] = { { "md_ritual_circulo4", T.circulos2, T.durCirc2, 100.0f, { 0.0f, 0.36f, 1.0f }, -0.0873f, 4 },
			                { "md_ritual_circulo3", T.circulos2, T.durCirc2, 100.0f, { 0.48f, 0.67f, 1.0f }, 0.1963f, 3 },
			                { "md_ritual_circulo2", T.circulos + 0.01f, T.durCirc, 56.0f, { 0.3f, 0.33f, 1.0f }, -0.3927f, 2 },
			                { "md_ritual_circulo5", T.circulos2, T.durCirc2, 35.5f, { 0.35f, 0.14f, 0.61f }, 0.0f, 5 },
			                { "md_ritual_circulo1", T.circulos, T.durCirc, 33.0f, { 0.14f, 0.45f, 1.0f }, 0.3927f, 1 } };
		for (const Circ& k : C) {
			const float u = (te - k.t0) / k.dur;
			if (u <= 0.0f || u >= 1.0f) continue;
			float tam = k.tam, brillo = 1.0f;
			switch (k.modo) {
			case 1: tam *= std::min(1.0f, u / 0.06f); brillo = u < 0.04f ? 0.56f : u < 0.07f ? 1.0f : u < 0.11f ? 1.0f - (u - 0.07f) / 0.04f * 0.47f : 0.53f; break;
			case 2: tam *= tamCirculo(u, 0.77f, 0.08f); brillo = std::min(u / 0.06f, 1.0f) * 0.65f; break;
			case 3: tam *= tamCirculo(u, 0.68f, 0.02f); brillo = std::min(u / 0.06f, 1.0f) * 0.99f * 0.5f / 0.5f; break;
			case 4: tam *= tamCirculo(u, 0.58f, 0.05f); brillo = std::min(u / 0.06f, 1.0f) * 0.68f; break;
			default: brillo = alfaVida(u, 0.06f, 0.97f); break;
			}
			if (u > 0.96f) brillo *= 1.0f - (u - 0.96f) / 0.04f;
			const float a = brillo * salida;
			if (!spriteEntradaMD(k.sp, cc, glm::vec2(tam * U), glm::vec4(k.col * 2.2f, a), k.giro * (te - k.t0)))
				haloMD(cc, tam * U * 0.5f, k.col * 1.5f, a * 0.6f);
		}
		// las ocho llamas (RitualFire01..08: una cada 0.067 s) con su reflejo rojo (Reflection01..08) y las chispas
		for (int k = 0; k < 8; k++) {
			const float t0 = T.fuego + 0.0667f * (float)k, dur = T.durFuego - 0.0667f * (float)k;
			const float u = (te - t0) / dur;
			if (u <= 0.0f || u >= 1.0f) continue;
			const float ang = 1.5708f + (float)k * 0.7854f;
			const glm::vec2 p = cc + glm::vec2(cosf(ang), sinf(ang)) * 21.0f * U;
			const float a = alfaVida(u, 0.04f, 0.95f) * salida;
			const float baile = 1.0f + 0.12f * sinf(te * 19.0f + (float)k * 2.1f);
			const float ur = (te - t0) / 2.0f;
			const float refl = ur < 0.09f ? 1.0f - ur / 0.09f * 0.5f : ur < 0.6f ? 0.5f : std::max(0.0f, 0.5f * (1.0f - (ur - 0.6f) / 0.4f));
			haloMD(p + glm::vec2(0.0f, 2.0f * U), 8.0f * U, glm::vec3(0.69f, 0.05f, 0.09f) * 2.0f, refl * salida);
			if (!spriteEntradaMD("md_ritual_fuego", p - glm::vec2(0.0f, 3.5f * U * baile), glm::vec2(5.5f * U, 9.0f * U * baile), glm::vec4(1.6f, 0.75f, 0.3f, a), 0.0f))
				haloMD(p - glm::vec2(0.0f, 3.0f * U), 3.5f * U * baile, glm::vec3(1.0f, 0.55f, 0.2f), a);
			haloMD(p - glm::vec2(0.0f, 2.0f * U), 2.2f * U * baile, glm::vec3(1.0f, 0.85f, 0.5f), a * 0.8f);
		}
		if (te >= T.chispas && te <= T.chispas + 1.78f) {
			for (int i = 0; i < 20; i++) {
				const float vida = 0.2f + 1.4f * ruidoEntrada(i + 900);
				const float u = (te - T.chispas - 0.05f) / vida;
				if (u <= 0.0f || u >= 1.0f) continue;
				const float ang = ruidoEntrada(i + 920) * 6.2832f;
				const float vel = (0.1f + 2.9f * ruidoEntrada(i + 940)) * 6.0f * U;
				const glm::vec2 p0 = cc + glm::vec2(cosf(ang), sinf(ang)) * 21.0f * U;
				const glm::vec2 p = p0 + glm::vec2(cosf(ang) * 0.3f, -1.0f) * vel * (te - T.chispas);
				haloMD(p, (0.7f + 0.5f * ruidoEntrada(i + 960)) * 0.9f * U, glm::vec3(1.0f, 0.27f, 0.31f) * 2.0f, (u < 0.8f ? 1.0f : (1.0f - u) / 0.2f) * salida);
			}
		}
		// RitualTrailIn (01): los materiales entran al circulo; CenterFlare
		const int n = (int)e.materiales.size();
		if (T.trail >= 0.0f && n > 0 && te >= T.trail && te <= T.trail + 1.383f) {
			for (int i = 0; i < n; i++) {
				const glm::vec2 de = posMaterialMD(i, n, c);
				const float u = suave(0.0f, 1.0f, (te - T.trail - 0.05f * (float)i) / 0.9f);
				const glm::vec2 p = glm::mix(de, cc, u * u);
				const float a = alfaVida(u, 0.1f, 0.85f) * salida;
				const glm::vec2 d = cc - de;
				rect(glm::mix(de, p, 0.6f), glm::vec2(glm::length(p - de) * 0.7f + 1.0f, 14.0f * s), atan2f(d.y, d.x), 8.0f * s, LINEA, glm::vec4(0.5f, 0.8f, 1.0f, 0.7f * a),
				     NADA, 0.0f, glm::vec4(1.5f, 0.2f, 0.0f, 0.0f));
				haloMD(p, 3.0f * U, glm::vec3(0.6f, 0.85f, 1.0f) * 1.6f, a);
			}
		}
		if (T.flare >= 0.0f && te >= T.flare && te <= T.flare + 0.5f) {
			const float u = (te - T.flare) / 0.5f;
			haloMD(cc, 14.0f * U * (0.5f + u), glm::vec3(0.7f, 0.9f, 1.0f) * 1.6f, (1.0f - u) * salida);
		}
		// la columna de luz (Cylinder 2 s: tlp_RitualCylinder01_a azul, 03_b violeta; dos pulsos por cada 0.5 s)
		if (te >= T.columna && te <= T.columna + 2.0f) {
			const float u = (te - T.columna) / 2.0f;
			const float pulso = 0.65f + 0.35f * fabsf(sinf(3.1416f * (te - T.columna) / 0.25f));
			const float a = alfaVida(u, 0.08f, 0.8f) * pulso * salida;
			rect(glm::vec2(cc.x, H * 0.5f), glm::vec2(H * 1.4f, 26.0f * U), 1.5708f, 20.0f * s, LINEA, glm::vec4(0.23f, 0.48f, 1.0f, 0.75f * a), NADA, 0.0f,
			     glm::vec4(1.5f, 0.3f, 0.0f, 0.0f));
			rect(glm::vec2(cc.x, H * 0.5f), glm::vec2(H * 1.4f, 11.0f * U), 1.5708f, 10.0f * s, LINEA, glm::vec4(0.63f, 0.2f, 1.0f, 0.85f * a), NADA, 0.0f,
			     glm::vec4(1.5f, 0.3f, 0.0f, 0.0f));
			haloMD(cc, 20.0f * U, glm::vec3(0.4f, 0.55f, 1.0f), 0.8f * a);
		}
		// PostRitual: el estallido rosa (explosion 190 u, 0.09 -> 1 en 0.3 s) y el destello
		if (te >= T.estallido && te <= T.estallido + 0.3f) {
			const float u = (te - T.estallido) / 0.3f;
			const float a = alfaVida(u, 0.06f, 0.46f) * salida;
			if (!spriteEntradaMD("md_fusion_estallido", cc, glm::vec2(190.0f * U * 0.6f * (0.09f + 0.91f * u)), glm::vec4(0.93f * 1.6f, 0.4f * 1.6f, 1.6f, a)))
				haloMD(cc, 190.0f * U * 0.3f * (0.09f + 0.91f * u), glm::vec3(0.93f, 0.4f, 1.0f), a);
			caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(1.0f, 0.9f, 1.0f, 0.3f * (1.0f - u) * salida), NADA, 0.0f, glm::vec4(0.0f));
		}
		revelaMonstruoMD(e, c, te - T.strong, salida, acento, "INVOCACIÓN RITUAL");
	}

	void dibujarEntradaMD()
	{
		const entradamd::Estado& e = entradamd::estado();
		if (!e.activa || !ctl || ctl->vista.pantalla != control::VistaDuelo::DUELO) return;
		cargarEntradasMD();
		const float te = e.t;
		const glm::vec2 c((CAMPO_X * s + W) * 0.5f, H * 0.45f);
		// con carta clave se apaga en el StrongSummon (sigue su corte); sin ella, al final de la secuencia
		const float salida = e.clave ? 1.0f - entradamd::suave(e.strong, e.strong + 0.25f, te) : 1.0f - entradamd::suave(e.fin - 0.3f, e.fin, te);
		if (e.tipo == entradamd::FUSION) dibujarFusionMD(e, c, te, salida);
		else dibujarRitualMD(e, c, te, salida);
		usar(setAtlas, false);
	}
