	// Yugioh Classic Remaster (rama classic-md-ritmo): se incluye DENTRO de la clase Interfaz (interfaz.h). Dibuja el ritmo
	// de Master Duel (md_ritmo.h) con el estilo de la casa (vidrio, cian del jugador, carmesi del rival, oro):
	//  - dibujarViajesMD: las cartas que viajan (mano / mazo / cementerio / extra / desterradas <-> zona), en arco, con
	//    su vuelta en el aire y la estela de chispas.
	//  - la carta del rival elegida en su mano se levanta (ajusteManoRivalMD, desde dibujarManoRival).
	//  - dibujarActivacionesMD: la activacion (los dos lados): la carta se levanta en su zona (alzaRitmoMD, desde
	//    cartaZona), se acerca grande con su arte y un halo del color de su clase, destella con chispas y vuelve; el numero
	//    de eslabon queda sobre la carta mientras haya cadena de 2 o mas; destello al resolver.
	//  - dibujarActivasMD: toda magia/trampa boca arriba que sigue en el campo (continua, equipo, campo, Swords...) brilla
	//    fuerte y late (marco de luz en su zona + halo), distinta de una boca abajo o ya resuelta.
	//  - dibujarContadoresMD: el contador sobre la carta (turnos que quedan: Swords 3 -> 2 -> 1; contadores de magia).
	std::function<bool(int, int&, bool&)> contadorZona;   // (lo pone el juego: contadorZonaMD de md_ritmo_juego.h)
	int destelloVistoMD{ 0 };
	// los sprites de Master Duel del atlas compartido (md_ui.png de classic-md-fases: md_turnos, md_turnos_aguja,
	// md_contador_magia, md_brillo_marco). Sin el atlas (o sin el sprite) devuelven false y se dibujan
	// las piezas con primitivas
	bool spriteRitmoMD(const char* n, glm::vec2 c, glm::vec2 tam, glm::vec4 col, float ang = 0.0f) { return spriteMD(n, c, tam, col, ang); }
	glm::vec2 tamSpriteRitmoMD(const char* n, float alto) const { return tamSpriteMD(n, alto); }


	// colores de la activacion (los del cartel del rival: dibujarCorteMando)
	glm::vec4 acentoMD(const std::string& carta, bool monstruo) const
	{
		const int ci = buscarCarta(carta);
		const CartaUI* k = ci >= 0 ? &cartas[ci] : nullptr;
		if (!monstruo && k && k->atributo == "TRAMPA") return glm::vec4(1.0f, 0.38f, 0.72f, 1.0f);
		if (!monstruo && k && k->atributo == "MAGIA") return glm::vec4(0.25f, 0.95f, 0.75f, 1.0f);
		return glm::vec4(1.0f, 0.62f, 0.22f, 1.0f);
	}
	// la zona en pantalla y el tamanno de una carta tendida en ella
	glm::vec2 pantallaZonaMD(int z, float alto = 0.05f) const { return aPantalla(proyectar(centroZona(z) + glm::vec3(0.0f, alto, 0.0f))); }
	glm::vec2 tamZonaMD(int z) const
	{
		const glm::vec3 zc = centroZona(z) + glm::vec3(0.0f, 0.05f, 0.0f);
		const float h = glm::max(glm::length(aPantalla(proyectar(zc + glm::vec3(0.0f, 0.0f, CARTA_ALTO * 0.5f))) - aPantalla(proyectar(zc - glm::vec3(0.0f, 0.0f, CARTA_ALTO * 0.5f)))),
		                         30.0f * s);
		return glm::vec2(h * 150.0f / 218.0f, h);
	}
	// la carta k de la mano del rival si tuviera n cartas (cartaManoR sin el foco)
	void manoRivalMD(int k, int n, glm::vec2& c, float& ang) const
	{
		const float cx = (CAMPO_X * s + W) * 0.5f;
		const float d = (float)k - (float)(n - 1) * 0.5f;
		c = glm::vec2(cx - d * 100.0f * s, (14.0f + 26.0f * focoR) * s - d * d * 3.0f * s);
		ang = glm::radians(180.0f - d * 3.0f);
	}
	// de donde sale / adonde va un viaje: posicion, tamanno y giro en pantalla
	void extremoMD(const ritmo::Viaje& v, glm::vec2& p, glm::vec2& tam, float& ang) const
	{
		const bool rivalPila = v.ladoPila == 1;
		ang = rivalPila ? glm::pi<float>() : 0.0f;
		if (v.origen == ritmo::O_MANO) {
			if (rivalPila) {
				manoRivalMD(std::max(0, v.manoIdx), std::max(1, v.manoN), p, ang);
				p.y += 40.0f * s;   // (la elegida, levantada)
				tam = glm::vec2(120.0f, 174.0f) * s;
			} else {
				p = v.manoIdx >= 0 && v.manoIdx < MAX_MANO_J && posManoJ[v.manoIdx].x > 0.0f ? posManoJ[v.manoIdx] : glm::vec2((CAMPO_X * s + W) * 0.5f, H - 60.0f * s);
				tam = glm::vec2(150.0f, 218.0f) * s;
			}
			return;
		}
		const char* pila = v.origen == ritmo::O_CEMENTERIO ? "cementerio" : v.origen == ritmo::O_DESTERRADO ? "desterrado" : v.origen == ritmo::O_EXTRA ? "fusion" : "mazo";
		const int z = control::zonaPorId(std::string(rivalPila ? "rival_" : "jugador_") + pila);
		if (z < 0) {
			p = glm::vec2(W * 0.5f, rivalPila ? 0.0f : H);
			tam = glm::vec2(120.0f, 174.0f) * s;
			return;
		}
		p = pantallaZonaMD(z, 0.1f);
		tam = tamZonaMD(z);
	}
	// una carta en pantalla: la cara (mini) o el dorso
	void cartaPantallaMD(glm::vec2 p, glm::vec2 tam, float ang, const std::string& carta, bool cara, bool rival, float alfa, float brillo, glm::vec4 borde)
	{
		usar(setAtlas, false);
		rect(p + glm::vec2(0.0f, 12.0f * s), tam, ang, 18.0f * s, PANEL, glm::vec4(0, 0, 0, 0.45f * alfa), NADA, 9.0f * s, glm::vec4(0, 0, 0, 12.0f * s));
		const int ci = cara && carta != "?" ? buscarCarta(carta) : -1;
		if (ci >= 0 && listaCarta(ci)) {
			usar(cartas[ci].setMini, false);
			const float uu = 0.5f / cartas[ci].mini.width, vv = 0.5f / cartas[ci].mini.height;
			rect(p, tam, ang, 0.0f, IMAGEN, glm::vec4(glm::vec3(brillo), alfa), borde, 8.0f * s, glm::vec4(0.6f, 1.0f, 0.3f, 0.0f), glm::vec2(uu, vv), glm::vec2(1.0f - uu, 1.0f - vv));
		} else {
			Dorso dz = usarDorso(false, false, rival, brillo, 0.0f);
			dz.col.a *= alfa;
			rect(p, tam, ang, 0.0f, DORSO, dz.col, dz.col2, 8.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
		}
		usar(setAtlas, false);
	}

	// la carta que el rival eligio de su mano se levanta (antes de volar a la mesa)
	void ajusteManoRivalMD(int k, glm::vec2& c, glm::vec2& tam)
	{
		float alza = 0.0f;
		if (ritmo::estado().manoElegida(alza) != k) return;
		const float u = suave(0.0f, 1.0f, alza);
		c.y += 46.0f * s * u;
		tam *= 1.0f + 0.12f * u;
		rect(c, tam * 1.35f, glm::pi<float>(), 0.0f, PARTICULA, glm::vec4(glm::vec3(CARMESI) * 0.5f * u, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
	}

	// la pila de un viaje (o -1: la mano)
	int zonaPilaMD(const ritmo::Viaje& v) const
	{
		if (v.origen == ritmo::O_MANO) return -1;
		const char* pila = v.origen == ritmo::O_CEMENTERIO ? "cementerio" : v.origen == ritmo::O_DESTERRADO ? "desterrado" : v.origen == ritmo::O_EXTRA ? "fusion" : "mazo";
		return control::zonaPorId(std::string(v.ladoPila == 1 ? "rival_" : "jugador_") + pila);
	}
	// cuanto sube en pantalla una carta a hMD de alto sobre la zona z (unidades de MD -> de la mesa: MD_ALTO_CARTA)
	glm::vec2 alturaPantallaMD(int z, float hMD) const
	{
		return pantallaZonaMD(z, 0.05f + hMD * CARTA_ALTO / ritmo::MD_ALTO_CARTA) - pantallaZonaMD(z, 0.05f);
	}
	// cuanto mas grande se ve a ese alto (mas cerca de la camara)
	float escalaAlturaMD(int z, float hMD) const
	{
		const float y = 0.05f + hMD * CARTA_ALTO / ritmo::MD_ALTO_CARTA;
		const glm::vec3 c = centroZona(z);
		const float l0 = glm::length(aPantalla(proyectar(c + glm::vec3(-CARTA_ANCHO * 0.5f, 0.05f, 0.0f))) - aPantalla(proyectar(c + glm::vec3(CARTA_ANCHO * 0.5f, 0.05f, 0.0f))));
		const float l1 = glm::length(aPantalla(proyectar(c + glm::vec3(-CARTA_ANCHO * 0.5f, y, 0.0f))) - aPantalla(proyectar(c + glm::vec3(CARTA_ANCHO * 0.5f, y, 0.0f))));
		return l0 > 1e-3f ? glm::clamp(l1 / l0, 0.5f, 3.0f) : 1.0f;
	}
	// una cadena de movimientos de MD a los "tiempo" s: posicion en el suelo de la pantalla (p) y alto (h, de MD), cuanto
	// del camino en el suelo va (g) y la fraccion del giro (giro). Cada tramo: Bezier de 3 puntos (inicio = donde quedo
	// el tramo anterior, base 0 = ahi, 1 = el destino; + lerp hacia el destino + alto)
	bool evalMovimientosMD(const std::vector<std::string>& movs, float tiempo, glm::vec2 desde, glm::vec2 destino, glm::vec2& p, float& h, float& g, float& giro) const
	{
		if (movs.empty()) return false;
		glm::vec2 sxy = desde;
		float sh = 0.0f, t0 = 0.0f;
		const float largo = glm::max(glm::length(destino - desde), 1e-3f);
		for (size_t i = 0; i < movs.size(); i++) {
			const ritmo::Movimiento* m = ritmo::movimiento(movs[i]);
			if (!m || m->tiempo <= 0.0f) return false;
			glm::vec2 P[3];
			float H[3];
			for (int k = 0; k < 3; k++) {
				const glm::vec2 bxy = m->base[k] == 0 ? sxy : destino;
				const float bh = m->base[k] == 0 ? sh : 0.0f;
				P[k] = bxy + m->lerp[k] * (destino - sxy);
				H[k] = bh + m->lerp[k] * (0.0f - sh) + m->add[k][1];
			}
			if (tiempo <= t0 + m->tiempo || i + 1 == movs.size()) {
				const float x = glm::clamp((tiempo - t0) / m->tiempo, 0.0f, 1.0f);
				const float w = m->acel.hay() ? m->acel.valor(x) : x;
				const float a = (1.0f - w) * (1.0f - w), b = 2.0f * (1.0f - w) * w, c = w * w;
				p = a * P[0] + b * P[1] + c * P[2];
				h = a * H[0] + b * H[1] + c * H[2];
				g = glm::clamp(1.0f - glm::length(destino - p) / largo, 0.0f, 1.0f);
				const float r = m->giro.hay() ? m->giro.valor(x) : x;
				giro = i == 0 ? glm::clamp(r, 0.0f, 1.0f) : 1.0f;   // (el giro se hace en el primer tramo; los demas asientan)
				return true;
			}
			sxy = P[2];
			sh = H[2];
			t0 += m->tiempo;
		}
		return false;
	}

	// la carta del jugador que vuela de su mano a la zona (control.h, dibujarVuelo) con el movimiento de Master Duel
	void vueloMD(const control::Mando::Vuelo& vu, glm::vec2 desde, glm::vec2 hasta, glm::vec2 tam0, glm::vec2 tam1, glm::vec2& p, glm::vec2& tam) const
	{
		if (vu.zona < 0) return;
		const bool magia = std::string(ZONAS[vu.zona].tipo) != "monstruo";
		const std::vector<std::string> movs = ritmo::cadenaMovimientos(ritmo::O_MANO, true, magia, 0);
		const float dur = ritmo::duracionMovimientos(movs, 0.0f);
		if (dur <= 0.0f) return;
		float h = 0.0f, g = 0.0f, giro = 0.0f;
		glm::vec2 q;
		if (!evalMovimientosMD(movs, vu.t * dur, desde, hasta, q, h, g, giro)) return;
		p = q + alturaPantallaMD(vu.zona, h);
		tam = glm::mix(tam0, tam1, g) * escalaAlturaMD(vu.zona, h);
	}

	void dibujarViajesMD()
	{
		const float pi = glm::pi<float>();
		for (const ritmo::Viaje& v : ritmo::estado().viajes) {
			if (v.eligiendo() || v.zona < 0) continue;
			const float u = suave(0.0f, 1.0f, v.u());
			glm::vec2 pp, tp;
			float ap;
			extremoMD(v, pp, tp, ap);
			const glm::vec2 pz = pantallaZonaMD(v.zona);
			const glm::vec2 tz = tamZonaMD(v.zona);
			const bool rival = v.lado == 1;
			const float az = (rival ? pi : 0.0f) + (v.defensa ? -0.5f * pi : 0.0f);
			glm::vec2 p0 = pp, p1 = pz, t0 = tp, t1 = tz;
			float a0 = ap, a1 = az;
			bool cara0, cara1;
			if (v.llegada) {
				cara0 = v.origen == ritmo::O_CEMENTERIO || v.origen == ritmo::O_DESTERRADO || (v.origen == ritmo::O_MANO && v.ladoPila == 0);
				cara1 = !v.bocaAbajo;
			} else {
				std::swap(p0, p1);
				std::swap(t0, t1);
				a0 = (rival ? pi : 0.0f) + (v.defensa ? -0.5f * pi : 0.0f);
				a1 = ap;
				cara0 = cara1 = v.caraEnVuelo;
			}
			// el movimiento de Master Duel tal cual (md_ritmo.h: BezierMotionSetting con su accelerationCurve y su
			// rotationAccelerationCurve, evaluadas como Unity por clipsmd::CurvaUnity). Cada punto: su lugar en el suelo de la
			// pantalla y su alto (unidades de MD); el alto se ve como lo proyecta la camara sobre la zona
			glm::vec2 p, tam;
			float ang, g = u, giro = u, hMD = 0.0f;
			const int zAlto = v.llegada ? v.zona : zonaPilaMD(v);
			if (evalMovimientosMD(v.movs, v.t - v.tElige, p0, p1, p, hMD, g, giro)) {
				p += alturaPantallaMD(zAlto >= 0 ? zAlto : v.zona, hMD);
				tam = glm::mix(t0, t1, g) * escalaAlturaMD(zAlto >= 0 ? zAlto : v.zona, hMD);
				ang = glm::mix(a0, a1, giro);
			} else {   // (sin los datos de MD: el arco de siempre)
				p = glm::mix(p0, p1, u) + glm::vec2(0.0f, -150.0f * s * sinf(pi * u));
				tam = glm::mix(t0, t1, u) * (1.0f + 0.2f * sinf(pi * u));
				ang = glm::mix(a0, a1, u);
			}
			// (al salir de una pila: el destello de MD, DuelfFromGrave01TL / FromExclude01TL)
			if (v.llegada && v.origen != ritmo::O_MANO && v.u() < 0.35f) {
				usar(setAtlas, false);
				const float f = 1.0f - v.u() / 0.35f;
				rect(p0, glm::vec2(glm::max(t0.x, t0.y)) * (1.4f + 0.8f * (1.0f - f)), 0.0f, 0.0f, PARTICULA,
				     glm::vec4(glm::vec3(v.origen == ritmo::O_CEMENTERIO ? glm::vec3(0.75f, 0.45f, 1.0f) : glm::vec3(0.6f, 0.9f, 1.0f)) * f, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
			}
			const bool voltea = cara0 != cara1;   // (se da vuelta con el giro del movimiento)
			if (voltea) tam.x *= glm::max(fabsf(cosf(pi * giro)), 0.04f);
			const bool cara = voltea ? (giro < 0.5f ? cara0 : cara1) : cara0;
			const float alfa = v.llegada ? 1.0f : 1.0f - suave(0.75f, 1.0f, v.u());
			const glm::vec4 borde = rival ? CARMESI : ORO;
			cartaPantallaMD(p, tam, ang, v.carta, cara, rival, alfa, 1.05f + 0.25f * sinf(pi * u), borde);
			rect(p, tam * 1.35f, ang, 0.0f, PARTICULA, glm::vec4(glm::vec3(borde) * 0.28f * sinf(pi * u) * alfa, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
			for (int k = 0; k < 2; k++) {
				const glm::vec3 col = rival ? glm::mix(glm::vec3(1.0f, 0.35f, 0.4f), glm::vec3(1.0f, 0.8f, 0.8f), azar()) : glm::mix(glm::vec3(1.0f, 0.75f, 0.25f), glm::vec3(1.0f, 0.95f, 0.7f), azar());
				chispa(p + glm::vec2(azar() - 0.5f, azar() - 0.5f) * tam, glm::vec2((azar() - 0.5f) * 60.0f, -(20.0f + 60.0f * azar())) * s, 0.35f + 0.4f * azar(),
				       (1.5f + 2.0f * azar()) * s, col, azar() > 0.8f ? 1.0f : 0.0f);
			}
		}
	}

	// la carta de la zona i que se esta activando: Master Duel CardShow (CardShowMoveFieldNear 0.15 s: sube a +5; CardShowHappen
	// 0.70 s: +5 -> +6 mirando a la camara; CardShowBackFieldNear 0.25 s: vuelve a la zona), con sus curvas. Devuelve el
	// alto (de la mesa); "mira": cuanto se para mirando a la camara (0..1); brillo: la luz de la carta (fxp_chain_cardlight:
	// a 0.65 s del apilado, 0.5 s)
	float alzaRitmoMD(int i, float& brillo, float& mira) const
	{
		mira = 0.0f;
		const ritmo::Activacion* a = ritmo::estado().activandoEn(i);
		if (!a || a->t < a->retraso) return 0.0f;
		const float ta = a->t - a->retraso;
		glm::vec2 p;
		float h = 0.0f, g = 0.0f, giro = 0.0f;
		const std::vector<std::string> movs = ritmo::cadenaActivacion();
		if (!evalMovimientosMD(movs, ta, glm::vec2(0.0f), glm::vec2(0.0f), p, h, g, giro)) {
			const float u = a->u();
			h = 6.0f * suave(0.0f, 0.14f, u) * (1.0f - suave(0.77f, 1.0f, u));
		}
		const float ida = ritmo::movimiento(movs[0]) ? ritmo::movimiento(movs[0])->tiempo : 0.15f;
		const float vuelta = ritmo::T_ACTIVA - (ritmo::movimiento(movs[2]) ? ritmo::movimiento(movs[2])->tiempo : 0.25f);
		mira = suave(0.0f, ida, ta) * (1.0f - suave(vuelta, ritmo::T_ACTIVA, ta));
		const float luz = ta >= 0.65f && ta < 1.15f ? sinf(glm::pi<float>() * (ta - 0.65f) / 0.5f) : 0.0f;
		brillo += 0.6f * mira + 1.4f * luz;
		return h * CARTA_ALTO / ritmo::MD_ALTO_CARTA;
	}

	// una magia/trampa boca arriba que sigue en el campo: activa (continua, equipo, campo, Swords...)
	bool activaMD(int i) const
	{
		if (!ctl || !ctl->duelo || i < 0 || i >= (int)NUM_ZONAS) return false;
		const std::string tipo = ZONAS[i].tipo;
		if (tipo != "magia" && tipo != "campo") return false;
		const control::Celda& k = ctl->zonas[(size_t)i];
		if (k.vacia() || k.bocaAbajo || k.tVolteo < control::DURACION_ANIM) return false;
		return !ritmo::estado().activandoEn(i);   // (mientras se presenta su activacion, la presentacion manda)
	}
	// el brillo de la carta activa en cartaZona: ninguno (rama classic-ps5-efectos: MD no aclara la carta; su brillo es la
	// luz amarilla de afuera del borde, haloActivaMD de md_efectos_ui.h)
	float brilloActivaMD(int i) const { (void)i; return 0.0f; }

	// el brillo de las cartas activas, tendido en la mesa con la carta (se dibuja antes que ellas). Rama classic-ps5-efectos:
	// antes era un marco de zona amarillo con velo y, encima de la carta, el sprite md_brillo_marco (la MASCARA llena de
	// EfDuelHighlight00_01_01) dibujado como imagen: el rectangulo amarillo solido de la foto de la PS5. Ahora es el de MD
	// (fxp_HL_Select_trpmgc_001): la luz amarilla que sale del borde de la carta, con su pulso (haloActivaMD)
	void dibujarActivasMD()
	{
		if (!ctl || !ctl->duelo) return;
		for (int i = 0; i < (int)NUM_ZONAS; i++) {
			if (!activaMD(i)) continue;
			const control::Celda& k = ctl->zonas[(size_t)i];
			const float entra = suave(0.0f, 0.5f, glm::min(k.tActiva, k.tLlega) > 50.0f ? 1.0f : glm::max(k.tActiva, 0.0f));
			haloActivaMD(i, entra);
		}
		usar(setAtlas, true);
	}
	// (rama classic-ps5-efectos: ya no hay nada encima de la carta activa; se deja para no tocar a quien la llama)
	void dibujarHalosActivasMD() {}

	// la luz de la activacion (fxp_chain_cardlight_001 de MD: a 0.65 s del apilado durante 0.5 s) alrededor de la carta
	// levantada, del color de su clase, y el numero de eslabon que entra (ChainNumEf: a 0.75 s, con 2+ eslabones)
	void dibujarActivacionesMD()
	{
		ritmo::Estado& r = ritmo::estado();
		for (const ritmo::Activacion& a : r.acts) {
			if (a.t < a.retraso || a.zona < 0) continue;
			const float ta = a.t - a.retraso;
			float b = 0.0f, mira = 0.0f;
			const float alto = alzaRitmoMD(a.zona, b, mira);
			const glm::vec4 acento = acentoMD(a.carta, a.monstruo);
			const glm::vec2 p = pantallaZonaMD(a.zona, 0.05f + alto);
			const glm::vec2 tz = tamZonaMD(a.zona) * escalaAlturaMD(a.zona, alto * ritmo::MD_ALTO_CARTA / CARTA_ALTO);
			const float luz = ta >= 0.65f && ta < 1.15f ? sinf(glm::pi<float>() * (ta - 0.65f) / 0.5f) : 0.0f;
			usar(setAtlas, false);
			rect(p, glm::vec2(glm::max(tz.x, tz.y)) * (1.3f + 0.9f * luz), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(acento) * (0.25f * mira + 0.9f * luz), 1.0f), NADA, 0.0f,
			     glm::vec4(0.0f));
			if (a.eslabon >= 2 && ta >= 0.75f) {
				const float e = suave(0.75f, 0.95f, ta);
				fichaEslabonMD(p + glm::vec2(-tz.x * 0.5f, -tz.y * 0.55f), a.eslabon, a.lado == 1, 1.0f + 0.6f * (1.0f - e), e);
			}
		}
	}

	// el numero de eslabon (como Master Duel: un disco con el numero sobre la carta)
	void fichaEslabonMD(glm::vec2 c, int n, bool rival, float esc, float alfa)
	{
		const glm::vec4 lado = rival ? CARMESI : CIAN;
		auto A = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * alfa); };
		usar(setAtlas, false);
		rect(c, glm::vec2(70.0f * s * esc), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(ORO) * 0.5f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		rect(c, glm::vec2(40.0f * s * esc), 0.0f, 6.0f * s, ORBE, A(glm::vec4(glm::vec3(lado) * 0.95f, 1.0f)), A(glm::vec4(0.04f, 0.06f, 0.12f, 1.0f)), 0.0f,
		     glm::vec4(0.25f, 1.0f, 0.0f, 0.0f));
		texto(F_NUMEROS, std::to_string(n), c.x, c.y + 10.0f * s * esc, 28.0f * s * esc, A(BLANCO), A(ORO), glm::vec3(0.5f, 0.0f, 0.8f), 1);
	}

	// los numeros de eslabon sobre las cartas de la mesa (cadena de 2 o mas) y el destello del que resuelve
	void dibujarCadenaMesaMD()
	{
		const ritmo::Estado& r = ritmo::estado();
		if (r.maxEslabon() >= 2) {
			for (const ritmo::Marca& m : r.cadena) {
				if (m.zona < 0) continue;
				const ritmo::Activacion* a = r.activandoEn(m.zona);
				if (a) continue;   // (todavia llegando, o presentandose: la ficha va con la carta levantada)
				const bool rival = strncmp(ZONAS[m.zona].id, "rival", 5) == 0;
				const glm::vec2 p = pantallaZonaMD(m.zona, 0.5f);
				fichaEslabonMD(p + glm::vec2(0.0f, -6.0f * s), m.n, rival, 1.0f + 0.06f * sinf(t * 5.0f), 1.0f);
			}
		}
		if (r.zonaResuelve >= 0 && r.tResuelve < 0.7f) {
			const float u = r.tResuelve / 0.7f;
			const glm::vec2 p = pantallaZonaMD(r.zonaResuelve, 0.3f);
			const glm::vec2 tz = tamZonaMD(r.zonaResuelve);
			usar(setAtlas, false);
			rect(p, glm::vec2(glm::max(tz.x, tz.y)) * (1.2f + 2.4f * u), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(1.0f, 0.95f, 0.8f) * 1.2f * (1.0f - u), 1.0f), NADA, 0.0f,
			     glm::vec4(0.0f));
		}
	}

	// el contador sobre la carta, como Master Duel: en la esquina de arriba a la izquierda; el numero entra a escala 1.5 y
	// baja a 1 en 0.2 s al cambiar. Turnos (Swords of Revealing Light 3 -> 2 -> 1): un cronometro cuya aguja da una vuelta
	// en 0.4 s (tras 0.2 s) y el numero late 1 -> 1.2 en 0.3 s. Contadores de magia: la estrella con el numero
	int contadorVistoMD[NUM_ZONAS]{};
	float tContadorMD[NUM_ZONAS]{};   // (cuando cambio: t de la interfaz)
	void dibujarContadoresMD()
	{
		if (!ctl || !ctl->duelo || !contadorZona) return;
		const float pi = glm::pi<float>();
		for (int i = 0; i < (int)NUM_ZONAS; i++) {
			int n = 0;
			bool turnos = false;
			// (rama classic-ps5-efectos: ni mientras la carta se presenta ni durante su CardRunEffect: en MD el contador sale
			// cuando el efecto ya se aplico; antes el cronometro tapaba la carta levantada)
			if (!contadorZona(i, n, turnos) || ritmo::estado().activandoEn(i) || hayEfectoMD()) {
				contadorVistoMD[i] = 0;
				continue;
			}
			if (n != contadorVistoMD[i]) contadorVistoMD[i] = n, tContadorMD[i] = t;
			const float tc = t - tContadorMD[i];
			const glm::vec2 tz = tamZonaMD(i);
			const bool rival = strncmp(ZONAS[i].id, "rival", 5) == 0;
			// (arriba a la izquierda de la carta tal como se ve; la del rival esta girada: su esquina queda abajo a la derecha)
			const glm::vec2 p = pantallaZonaMD(i, 0.1f) + glm::vec2(-tz.x * 0.46f, -tz.y * 0.44f) * (rival ? -1.0f : 1.0f);
			const glm::vec4 col = turnos ? glm::vec4(1.0f, 0.82f, 0.30f, 1.0f) : glm::vec4(0.55f, 0.75f, 1.0f, 1.0f);
			const float pop = glm::mix(1.5f, 1.0f, suave(0.0f, 0.2f, tc)) * (turnos ? 1.0f + 0.2f * sinf(pi * glm::clamp(tc / 0.3f, 0.0f, 1.0f)) : 1.0f);
			const float r0 = glm::clamp(tz.y * 0.30f, 26.0f * s, 42.0f * s);
			usar(setAtlas, false);
			rect(p, glm::vec2(r0 * 2.3f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(col) * 0.5f, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
			// los sprites de Master Duel (atlas md_ui de classic-md-fases): el cronometro GUI_TurnCounter con su aguja
			// GUI_TurnNeedle, o el icono del contador de magia (icon_counter_magic); sin el atlas, dibujados
			if (turnos && spriteRitmoMD("md_turnos", p, tamSpriteRitmoMD("md_turnos", r0 * 2.0f), glm::vec4(1.0f))) {
				const float giro = -2.0f * pi * suave(0.2f, 0.6f, tc);
				spriteRitmoMD("md_turnos_aguja", p + glm::vec2(0.0f, r0 * 0.12f), tamSpriteRitmoMD("md_turnos_aguja", r0 * 1.2f), glm::vec4(1.0f), giro);
				texto(F_NUMEROS, std::to_string(n), p.x, p.y + r0 * 0.5f * pop, r0 * 0.95f * pop, BLANCO, glm::vec4(0, 0, 0, 1), glm::vec3(0.5f, 0.0f, 0.8f), 1);
				continue;
			}
			if (!turnos && spriteRitmoMD("md_contador_magia", p, glm::vec2(r0 * 1.3f), glm::vec4(1.0f))) {
				texto(F_NUMEROS, std::to_string(n), p.x + r0 * 0.55f, p.y + r0 * 0.75f, r0 * 0.9f * pop, BLANCO, glm::vec4(0, 0, 0, 1), glm::vec3(0.5f, 0.0f, 0.8f), 1);
				continue;
			}
			rect(p, glm::vec2(r0 * 1.12f), 0.0f, 6.0f * s, ORBE, glm::vec4(0.05f, 0.07f, 0.14f, 0.95f), col, 0.0f, glm::vec4(0.25f, 1.0f, 0.0f, 0.0f));
			if (turnos) {   // cronometro: el boton de arriba y la aguja
				rect(p + glm::vec2(0.0f, -r0 * 0.62f), glm::vec2(r0 * 0.28f, r0 * 0.16f), 0.0f, 2.0f * s, PANEL, col, NADA, 2.0f * s, glm::vec4(0.0f));
				const float giro = -2.0f * pi * suave(0.2f, 0.6f, tc);
				const glm::vec2 d(sinf(giro + pi * 0.25f), -cosf(giro + pi * 0.25f));
				rect(p + d * r0 * 0.3f, glm::vec2(r0 * 0.07f, r0 * 0.6f), atan2f(d.x, -d.y), 1.0f * s, PANEL, glm::vec4(glm::vec3(col) * 0.6f, 0.8f), NADA, 1.0f * s, glm::vec4(0.0f));
			} else {   // contador de magia: la estrella
				rect(p, glm::vec2(r0 * 0.8f), t * 0.4f, 6.0f * s, ESTRELLA, glm::vec4(glm::vec3(col) * 0.55f, 1.0f), glm::vec4(glm::vec3(col) * 0.3f, 1.0f), 0.0f,
				     glm::vec4(0.8f, 0, 0, 0));
			}
			texto(F_NUMEROS, std::to_string(n), p.x, p.y + r0 * 0.36f * pop, r0 * 1.0f * pop, BLANCO, col, glm::vec3(0.5f, 0.0f, 0.8f), 1);
			if (turnos) texto(F_TITULO, idioma::t("TURNOS", "TURNS"), p.x, p.y + r0 * 1.05f, r0 * 0.42f, col, NADA, glm::vec3(0.0f), 1, 1.5f);
		}
	}

	// todo lo de encima (2D), desde dibujarMandoEncima
	void dibujarRitmoMD()
	{
		if (!ctl) return;
		dibujarHalosActivasMD();
		dibujarContadoresMD();
		dibujarViajesMD();
		dibujarActivacionesMD();
		dibujarCadenaMesaMD();
	}

	// el visor de la izquierda muestra la carta que se activa o que el rival juega boca arriba (como Master Duel), mientras
	// el jugador no tiene el control y hasta 2.5 s despues
	void contenidoRitmoMD(Contenido& c) const
	{
		const ritmo::Estado& r = ritmo::estado();
		if (!ctl || !ctl->duelo || r.visorCarta.empty() || r.tVisor > 2.5f || !ctl->bloqueado) return;
		const int ci = buscarCarta(r.visorCarta);
		if (ci < 0) return;
		c = Contenido{};
		c.carta = ci;
	}

#include "ritmo_turno_ui.h"   // rama classic-ps5-ritmo: el robo de la Fase de Robo como Master Duel
#include "cpu_muestra_ui.h"   // rama classic-ps5-cpu-joey: el rival ensena la carta antes de jugarla
