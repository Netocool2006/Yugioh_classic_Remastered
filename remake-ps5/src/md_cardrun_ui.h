	// Rama classic-ps5-efectos (2a vuelta): los CardRunEffect de Master Duel TRADUCIDOS (se incluye DENTRO de la clase
	// Interfaz, interfaz.h, despues de md_efectos_ui.h). MD: CardRunEffect.OnChainRun -> _PlayEffect reproduce el Timeline
	// de la carta (CardRunEffectSetting: effectPath / centerEffectPath por jugador), su sonido (seLabel) y la sacudida de
	// camara (cameraShakeType -> CameraShakerSetting + CameraShaker.UpdateShake / Info.GetShake). Aca:
	//  - PISTAS: cada ParticleSystem / malla del prefab del Timeline, simulado cuadro a cuadro con los modulos de Unity y
	//    exportado en 3D (herramientas/md_ritmo/exportar_pistas.py -> <assets>/md_ritmo/efectos/<ef>.mdp + .png). Aca se
	//    reproduce en la mesa: los billboards y los estirados de cara a NUESTRA camara (como Unity), lo demas proyectado.
	//    Unidades: las de MD (el prefab va en el centro de la mesa, centerEffectPath) x el paso de zonas (8.8 de MD = 2.7 *
	//    ESCALA de la mesa); z al reves (la camara de MD mira hacia +z desde el jugador, la nuestra hacia -z).
	//  - PANTALLAS: los SpriteRenderer de pantalla del prefab (Black / BK / w / White) con la curva de alfa de su
	//    AnimationClip "Recorded" (md_extraido/efectos/curvas_negro.py), muestreada a 20 por segundo.
	//  - SACUDIDAS: CameraShakerSetting.Info (CardRun04354 / 04342 / 04343 / 04887 / 05124) y GetShake traducido del
	//    desensamblado (md_extraido/efectos/desasm_getshake.txt); la camara se mueve con sacudidaMD() (yugireload.cpp).

	// ------------------------------------------------------------------ pistas
	struct MatPistaMD { float x, y, w, h; int banderas; int aditivo; float tinte[4]; };
	// (MDP2) la luz de los Shader Graph de MD con normales: base rgb, borde rgb (fresnel), potencia, modo (0 sin, 1 (1 - N.V)^p,
	// 2 smoothstep(e0, e1, 1 - N.V)), e0, e1, luz rgb, direccion de la luz, atenuacion ((N.dir)^a: Flit_CardEf)
	struct LuzPistaMD { float base[3]{ 1, 1, 1 }, borde[3]{}, pot{ 1 }, modo{ 0 }, e0{ 0 }, e1{ 1 }, luz[3]{}, dir[3]{}, aten{ 1 }; };
	struct PistaMD {
		bool intentada{ false }, lista{ false };
		float fps{ 30.0f }, dur{ 0.0f };
		std::vector<MatPistaMD> mats;
		std::vector<LuzPistaMD> luces;
		std::vector<std::vector<float>> cuadros;
		vks::Texture2D tex;
		VkDescriptorSet set{ VK_NULL_HANDLE };
	};
	std::map<std::string, PistaMD> pistasMD;
	int pistasMDSinArchivo{ 0 };   // (las pruebas lo leen)
	bool pistasMDSinGPU{ false };  // (pantalla_sin_gpu.h: no se suben texturas)

	PistaMD* pistaMD(const std::string& nombre)
	{
		PistaMD& p = pistasMD[nombre];
		if (p.intentada) return p.lista ? &p : nullptr;
		p.intentada = true;
		const std::string base = rutaAssets + "md_ritmo/efectos/" + nombre;
		std::ifstream f(base + ".mdp", std::ios::binary);
		char magia[4] = {};
		int32_t nm = 0, nc = 0;
		if (!f || !f.read(magia, 4) || (memcmp(magia, "MDP1", 4) != 0 && memcmp(magia, "MDP2", 4) != 0) || !f.read((char*)&nm, 4) || !f.read((char*)&nc, 4) || !f.read((char*)&p.fps, 4) ||
		    !f.read((char*)&p.dur, 4) || nm <= 0 || nm > 256 || nc <= 0 || nc > 2000) {
			pistasMDSinArchivo++;
			fprintf(stderr, "[interfaz] AVISO: falta la pista de Master Duel %s.mdp: el efecto va sin ella\n", base.c_str());
			return nullptr;
		}
		const bool v2 = memcmp(magia, "MDP2", 4) == 0;
		p.mats.resize((size_t)nm);
		p.luces.resize((size_t)nm);
		for (size_t k = 0; k < (size_t)nm; k++) {
			f.read((char*)&p.mats[k], sizeof(MatPistaMD));
			if (v2) f.read((char*)&p.luces[k], sizeof(LuzPistaMD));
		}
		p.cuadros.resize((size_t)nc);
		for (std::vector<float>& c : p.cuadros) {
			int32_t n = 0;
			if (!f.read((char*)&n, 4) || n < 0 || n > 4000000) return nullptr;
			c.resize((size_t)n);
			if (n) f.read((char*)c.data(), (std::streamsize)n * 4);
		}
		if (!f) return nullptr;
		if (!pistasMDSinGPU) {
			if (!cargarConMips(base + ".png", p.tex, true)) return nullptr;
			p.set = nuevoSet(p.tex);
		}
		p.lista = true;
		fprintf(stderr, "[interfaz] pista de Master Duel %s: %d cuadros, %.2f s, %d materiales\n", nombre.c_str(), nc, p.dur, nm);
		return &p;
	}

	static constexpr float U_MD_MESA = 2.7f * ESCALA / 8.8f;   // (unidades de MD -> la mesa: el paso de zonas)
	glm::vec3 mesaMD(const float* p, glm::vec3 ancla) const { return ancla + glm::vec3(p[0], p[1], -p[2]) * U_MD_MESA; }

	struct TriPistaMD { Vertice v[3]; float prof; };
	// como se ve cada pista: en la mesa (VISTA_MESA), en el espacio de la camara de MD (VISTA_PANTALLA: effectType 26,
	// Monster Reborn: su Black de 952 x 536 es la pantalla, 9.53 x 5.36 unidades: x a la derecha, y arriba, sin perspectiva)
	// o con la camara de duelo de MD (VISTA_MD: CameraViewSetting "Top", ojo (0, 95, -37) mirando al centro, FOV vertical 30;
	// Magic Cylinder: sus cilindros van junto a esa camara) sobre nuestra vista 3D
	enum { VISTA_MESA = 0, VISTA_PANTALLA = 1, VISTA_MD = 2 };
	struct CamaraMD { glm::vec3 ojo{ 0.0f, 95.0f, -37.0f }, r, u, f; float tanFov{ tanf(glm::radians(15.0f)) }; };
	static const CamaraMD& camaraMD()
	{
		static CamaraMD c = [] {
			CamaraMD k;
			k.f = glm::normalize(-k.ojo);
			k.r = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), k.f));   // (Unity es de mano izquierda: derecha = arriba x adelante)
			k.u = glm::cross(k.f, k.r);
			return k;
		}();
		return c;
	}
	// un cuadro de la pista a los t s, con el prefab en "ancla"; alfa: un apagado general (0..1)
	void dibujarPistaMD(PistaMD& p, float t, glm::vec3 ancla, float alfa, int vista = VISTA_MESA)
	{
		const bool pantalla = vista == VISTA_PANTALLA;
		const float kp = H / 5.359f;
		const float cx = (CAMPO_X * s + W) * 0.5f;   // (el centro de la vista 3D: a la izquierda va el panel de la carta)
		const float media = (W - CAMPO_X * s) * 0.5f;   // (medio ancho de la vista 3D)
		const CamaraMD& cm = camaraMD();
		auto pos = [&](const float* q, glm::vec4& clip) {
			if (pantalla) {
				clip = recorte(glm::vec2(cx + q[0] * kp, H * 0.5f - q[1] * kp));
				return;
			}
			if (vista == VISTA_MD) {
				const glm::vec3 d = glm::vec3(q[0], q[1], q[2]) - cm.ojo;
				const float z = glm::dot(d, cm.f);
				if (z < 0.1f) {
					clip = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
					return;
				}
				const float sx = glm::dot(d, cm.r) / (z * cm.tanFov * (media / (H * 0.5f))), sy = glm::dot(d, cm.u) / (z * cm.tanFov);
				clip = recorte(glm::vec2(cx + sx * media, H * 0.5f - sy * H * 0.5f));
				return;
			}
			clip = proyectar(mesaMD(q, ancla));
		};
		// px que mide un largo L (unidades de MD) en el punto q
		auto pxDe = [&](const float* q, float L) {
			if (pantalla) return L * kp;
			if (vista == VISTA_MD) {
				const float z = glm::max(glm::dot(glm::vec3(q[0], q[1], q[2]) - cm.ojo, cm.f), 0.1f);
				return L / (z * cm.tanFov) * H * 0.5f;
			}
			return pxMundo(mesaMD(q, ancla), L * U_MD_MESA);
		};
		std::vector<TriPistaMD> opacos;
		// hacia la camara (para la luz de borde), en el espacio del prefab de MD
		const glm::mat4 ivp = glm::inverse(vp);
		glm::vec4 ojoH = ivp * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
		const glm::vec3 ojoMesa = fabsf(ojoH.w) > 1e-6f ? glm::vec3(ojoH) / ojoH.w : glm::vec3(0.0f, 10.0f, 20.0f);
		auto haciaCamara = [&](const float* q) {
			if (pantalla) return glm::vec3(0.0f, 0.0f, -1.0f);
			if (vista == VISTA_MD) return glm::normalize(cm.ojo - glm::vec3(q[0], q[1], q[2]));
			const glm::vec3 w = ojoMesa - mesaMD(q, ancla);
			return glm::normalize(glm::vec3(w.x, w.y, -w.z));
		};
		if (p.cuadros.empty()) return;
		const int k = glm::clamp((int)(t * p.fps), 0, (int)p.cuadros.size() - 1);
		const std::vector<float>& c = p.cuadros[(size_t)k];
		if (p.set != VK_NULL_HANDLE) usar(p.set, false);
		size_t i = 0;
		auto atlasDe = [&](const MatPistaMD& m) { return glm::vec4(m.x, m.y, m.w, m.h); };
		auto extraDe = [&](const MatPistaMD& m) {
			return glm::vec4(0.0f, (m.banderas & 1) ? 1.0f : 0.0f, m.aditivo ? 0.0f : 1.0f, (m.banderas & 2) ? 1.0f : 0.0f);
		};
		auto colorDe = [&](const MatPistaMD& m, const float* col) {
			return glm::vec4(col[0] * m.tinte[0], col[1] * m.tinte[1], col[2] * m.tinte[2], glm::clamp(col[3] * m.tinte[3] * alfa, 0.0f, 4.0f));
		};
		while (i + 2 <= c.size()) {
			const int tipo = (int)c[i], mi = glm::clamp((int)c[i + 1], 0, (int)p.mats.size() - 1);
			const MatPistaMD& m = p.mats[(size_t)mi];
			if (tipo == 0) {   // billboard de cara a la camara
				if (i + 16 > c.size()) break;
				glm::vec4 cc;
				pos(&c[i + 2], cc);
				if (cc.w > 0.05f && p.set != VK_NULL_HANDLE) {
					const float px = pxDe(&c[i + 2], c[i + 5]);
					const glm::vec2 tam(px, px * c[i + 6] / glm::max(c[i + 5], 1e-5f));
					if (tam.x > 0.3f && tam.y > 0.3f && tam.x < 6.0f * W)
						rect(aPantalla(cc), tam, c[i + 7], 0.0f, LUZ, colorDe(m, &c[i + 8]), atlasDe(m), 0.0f, extraDe(m), glm::vec2(c[i + 12], c[i + 13]),
						     glm::vec2(c[i + 14], c[i + 15]));
				}
				i += 16;
			} else if (tipo == 1) {   // estirado por la velocidad: de la cola a la cabeza, de cara a la camara
				if (i + 17 > c.size()) break;
				const glm::vec3 cab = mesaMD(&c[i + 2], ancla);
				const glm::vec3 L(c[i + 5], c[i + 6], -c[i + 7]);
				const glm::vec3 cola = cab - L * U_MD_MESA;
				glm::vec4 c0 = proyectar(cola), c1 = proyectar(cab);
				if (vista != VISTA_MESA) {
					const float qc[3] = { c[i + 2] - c[i + 5], c[i + 3] - c[i + 6], c[i + 4] - c[i + 7] };
					pos(qc, c0);
					pos(&c[i + 2], c1);
				}
				if (c0.w > 0.05f && c1.w > 0.05f && p.set != VK_NULL_HANDLE) {
					const glm::vec2 a = aPantalla(c0), b = aPantalla(c1);
					glm::vec2 d = b - a;
					const float largo = glm::length(d);
					const float media = vista != VISTA_MESA ? pxDe(&c[i + 2], c[i + 8]) : 0.5f * pxMundo(cab, 2.0f * c[i + 8] * U_MD_MESA);
					if (largo > 0.5f && media > 0.2f) {
						d /= largo;
						const glm::vec2 n(-d.y * media, d.x * media);
						const glm::vec4 q[4] = { recorte(a - n), recorte(a + n), recorte(b + n), recorte(b - n) };
						const glm::vec2 l[4] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
						const glm::vec2 uv[4] = { { c[i + 13], c[i + 16] }, { c[i + 15], c[i + 16] }, { c[i + 15], c[i + 14] }, { c[i + 13], c[i + 14] } };
						quad(q, l, uv, colorDe(m, &c[i + 9]), atlasDe(m), glm::vec4(1.0f, 1.0f, 0.0f, (float)LUZ), extraDe(m));
					}
				}
				i += 17;
			} else if (tipo == 3) {   // triangulo 3D con normales: la luz de borde / direccional del Shader Graph de MD, por vertice
				if (i + 30 > c.size()) break;
				const LuzPistaMD& lz = p.luces[(size_t)mi];
				const glm::vec4 base = colorDe(m, &c[i + 26]);
				glm::vec4 q[3];
				glm::vec4 col[3];
				glm::vec2 uv[3];
				bool ok = p.set != VK_NULL_HANDLE;
				for (int j = 0; j < 3 && ok; j++) {
					const float* vj = &c[i + 2 + 8 * j];
					pos(vj, q[j]);
					uv[j] = glm::vec2(vj[3], vj[4]);
					ok = q[j].w > 0.05f;
					const glm::vec3 n = glm::normalize(glm::vec3(vj[5], vj[6], vj[7]) + glm::vec3(1e-6f));
					const float x = 1.0f - fabsf(glm::dot(n, haciaCamara(vj)));
					const float fr = lz.modo < 0.5f ? 0.0f : lz.modo < 1.5f ? powf(glm::max(x, 0.0f), lz.pot) : glm::smoothstep(lz.e0, lz.e1, x);
					const float dl = powf(glm::max(glm::dot(n, glm::vec3(lz.dir[0], lz.dir[1], lz.dir[2])), 0.0f), lz.aten);
					const glm::vec3 rgb = glm::vec3(base) * (glm::vec3(lz.base[0], lz.base[1], lz.base[2]) + glm::vec3(lz.luz[0], lz.luz[1], lz.luz[2]) * dl) +
					                      glm::vec3(lz.borde[0], lz.borde[1], lz.borde[2]) * fr * c[i + 29];
					col[j] = glm::vec4(rgb, base.a);
				}
				if (ok) {
					const glm::vec4 forma(1.0f, 1.0f, 0.0f, (float)LUZ), ex = extraDe(m), atl = atlasDe(m);
					if (m.aditivo) {   // (suma luz: el orden no importa)
						if (v.size() + 3 <= MAX_VERTICES)
							for (int j = 0; j < 3; j++) v.push_back({ q[j], uv[j], glm::vec2(0.0f), col[j], atl, forma, ex });
					} else {
						// (malla con alfa, Cull Back de Unity: fuera las caras de atras; las demas, de atras hacia adelante al final)
						glm::vec2 sp[3];
						for (int j = 0; j < 3; j++) sp[j] = glm::vec2(q[j].x / q[j].w, q[j].y / q[j].w);
						const float cr = (sp[1].x - sp[0].x) * (sp[2].y - sp[0].y) - (sp[1].y - sp[0].y) * (sp[2].x - sp[0].x);
						if (cr <= 0.0f) {
							TriPistaMD tr;
							float z = 0.0f;
							for (int j = 0; j < 3; j++) {
								const float* vj = &c[i + 2 + 8 * j];
								tr.v[j] = { q[j], uv[j], glm::vec2(0.0f), col[j], atl, forma, ex };
								z += pantalla ? vj[2] : vista == VISTA_MD ? glm::dot(glm::vec3(vj[0], vj[1], vj[2]) - cm.ojo, cm.f) : q[j].w;
							}
							tr.prof = z;
							opacos.push_back(tr);
						}
					}
				}
				i += 30;
			} else {   // triangulo 3D (mallas, particulas tendidas / verticales / alineadas al mundo)
				if (i + 21 > c.size()) break;
				glm::vec4 q[4];
				glm::vec2 uv[4];
				bool ok = p.set != VK_NULL_HANDLE;
				for (int j = 0; j < 3 && ok; j++) {
					pos(&c[i + 2 + 5 * j], q[j]);
					uv[j] = glm::vec2(c[i + 5 + 5 * j], c[i + 6 + 5 * j]);
					ok = q[j].w > 0.05f;
				}
				if (ok) {
					q[3] = q[2];
					uv[3] = uv[2];
					const glm::vec2 l[4] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
					quad(q, l, uv, colorDe(m, &c[i + 17]), atlasDe(m), glm::vec4(1.0f, 1.0f, 0.0f, (float)LUZ), extraDe(m));
				}
				i += 21;
			}
		}
		if (!opacos.empty()) {   // (las mallas con alfa, de la mas lejana a la mas cercana)
			std::sort(opacos.begin(), opacos.end(), [](const TriPistaMD& a, const TriPistaMD& b) { return a.prof > b.prof; });
			for (const TriPistaMD& tr : opacos) {
				if (v.size() + 3 > MAX_VERTICES) break;
				for (const Vertice& x : tr.v) v.push_back(x);
			}
		}
		usar(setAtlas, false);
	}

	// ------------------------------------------------------------------ pantallas (Black / BK / w / White)
	struct PantallaMD { glm::vec3 color; float inicio; int n; const float* alfa; };   // alfa: muestras a 20 por segundo
	static float curvaPantallaMD(const PantallaMD& p, float t)
	{
		const float x = (t - p.inicio) * 20.0f;
		if (x <= 0.0f || p.n <= 0) return 0.0f;
		const int k = (int)x;
		if (k >= p.n - 1) return 0.0f;
		return glm::mix(p.alfa[k], p.alfa[k + 1], x - (float)k);
	}

	// ------------------------------------------------------------------ sacudidas (CameraShakerSetting)
	struct SacudidaMD {
		float dur, delay;
		bool perlin; float perlinCycle; glm::vec3 perlinPower;
		bool cosS; float cosCycle; glm::vec3 cosPower;
		bool sinS; float sinCycle; glm::vec3 sinPower;
		bool sinSub; float sinCycleSub; glm::vec3 sinPowerSub;
		bool rnd; glm::vec3 rMin, rMax; int rFrames; float rGain;
	};
	static float ruidoMD(float x, uint32_t sem)   // ruido suave 0..1 (como Mathf.PerlinNoise en una linea)
	{
		const float i = floorf(x), f = x - i;
		const float a = azarMD((uint32_t)(int)i * 2654435761u + sem), b = azarMD((uint32_t)((int)i + 1) * 2654435761u + sem);
		const float s = f * f * (3.0f - 2.0f * f);
		return a + (b - a) * s;
	}
	// CameraShakerSetting.Info.GetShake (desensamblado, RVA 0x4A6D20): t = tiempo - delay; r = t / duracion; cae (1 - r);
	// perlin(r * perlinCycle) * perlinPower; cos(t * cosCycle * 270 grados) * cosPower; sin(r * sinCycle * 360 grados) *
	// sinPower (signo por eje de "big"); sin(r * sinCycleSub * 360 grados) * sinPowerSub (signo de "lit"); al azar cada
	// randomDelayFrame cuadros entre randomMin y randomMax
	static glm::vec3 getShakeMD(const SacudidaMD& s, float tiempo, int cuadro, glm::vec3 big, glm::vec3 lit, uint32_t sem)
	{
		if (tiempo < s.delay) return glm::vec3(0.0f);
		const float t = tiempo - s.delay, r = t / glm::max(s.dur, 1e-3f);
		if (r >= 1.0f) return glm::vec3(0.0f);
		const float cae = 1.0f - r;
		const float gr = glm::pi<float>() / 180.0f;
		glm::vec3 o(0.0f);
		if (s.perlin) {
			const float x = r * s.perlinCycle;
			o += glm::vec3(ruidoMD(x, sem) * 2.0f - 1.0f, ruidoMD(x, sem + 17u) * 2.0f - 1.0f, ruidoMD(x, sem + 31u) * 2.0f - 1.0f) * s.perlinPower * cae;
		}
		if (s.cosS) {
			const float c = cosf(t * s.cosCycle * 270.0f * gr);
			o += glm::vec3(big.x > 0.0f ? c : -c, c, c) * s.cosPower * cae;
		}
		if (s.sinS) {
			const float v = sinf(r * s.sinCycle * 360.0f * gr);
			o += glm::vec3(big.x > 0.0f ? v : -v, big.y > 0.0f ? v : -v, big.z > 0.0f ? v : -v) * s.sinPower * cae;
		}
		if (s.sinSub) {
			const float v = sinf(r * s.sinCycleSub * 360.0f * gr);
			o += glm::vec3(lit.x > 0.0f ? v : -v, lit.y > 0.0f ? v : -v, lit.z > 0.0f ? v : -v) * s.sinPowerSub * cae;
		}
		if (s.rnd) {
			const uint32_t k = (uint32_t)(cuadro / glm::max(s.rFrames, 1)) * 97u + sem;
			const glm::vec3 a(azarMD(k), azarMD(k + 1u), azarMD(k + 2u));
			o += glm::mix(s.rMin, s.rMax, a) * s.rGain * cae;
		}
		return o;
	}

	// ------------------------------------------------------------------ la tabla de MD (CardRunEffectSetting)
	struct CardRunMD {
		const char* receta;
		const char* pista[2];          // (0: la activa el jugador; 1: el rival) — "" sin pista
		PantallaMD pantallas[2];
		const SacudidaMD* sacudida;
		float dur;
		int vista{ VISTA_MESA };   // (VISTA_PANTALLA: effectType 26 de MD; VISTA_MD: con la camara de duelo de MD)
	};
	static const CardRunMD* cardRunMD(const std::string& receta)
	{
		// curvas de alfa de los SpriteRenderer de pantalla (AnimationClip "Recorded" de cada Timeline, 20 por segundo)
		static const float NEGRO_04343[] = { 0.0f, 0.072f, 0.244f, 0.45f, 0.624f, 0.698f, 0.701f, 0.703f, 0.706f, 0.708f, 0.709f, 0.711f, 0.712f, 0.714f, 0.715f, 0.715f, 0.716f, 0.717f,
			                                 0.717f, 0.718f, 0.718f, 0.718f, 0.718f, 0.718f, 0.718f, 0.718f, 0.718f, 0.718f, 0.718f, 0.699f, 0.609f, 0.473f, 0.317f, 0.171f, 0.063f, 0.02f, 0.0f };
		static const float BLANCO_04343[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.751f, 0.392f, 0.034f, 0.0f };
		static const float NEGRO_04887[] = { 0.0f, 0.108f, 0.324f, 0.486f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
			                                 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.486f, 0.324f, 0.108f, 0.0f };
		static const float NEGRO_04842[] = { 0.0f, 0.073f, 0.246f, 0.453f, 0.626f, 0.7f, 0.701f, 0.702f, 0.703f, 0.704f, 0.705f, 0.705f, 0.706f, 0.707f, 0.707f, 0.708f, 0.708f,
			                                 0.708f, 0.709f, 0.709f, 0.709f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f, 0.71f,
			                                 0.71f, 0.71f, 0.71f, 0.709f, 0.69f, 0.653f, 0.599f, 0.534f, 0.46f, 0.382f, 0.302f, 0.225f, 0.153f, 0.092f, 0.043f, 0.011f, 0.0f };
		static const float NEGRO_04342[] = { 0.0f, 0.086f, 0.259f, 0.389f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f,
			                                 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f, 0.384f, 0.342f, 0.282f, 0.212f, 0.141f, 0.077f, 0.027f, 0.002f, 0.0f };
		static const float NEGRO_05124[] = { 0.0f, 0.057f, 0.193f, 0.356f, 0.492f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f,
			                                 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f, 0.549f,
			                                 0.549f, 0.534f, 0.463f, 0.356f, 0.233f, 0.119f, 0.033f, 0.0f };
		static const float BLANCO_05124[] = { 0.0f, 0.251f, 0.458f, 0.285f, 0.092f, 0.0f };   // (de 1.10 s: el clip (5) es 0 hasta ahi)
		// CameraShakerSetting (data.unity3d, md_extraido/efectos/camera_shaker.json)
		static const SacudidaMD S04354{ 0.5f, 0.2f, false, 6.0f, { 2, 2, 1 }, false, 6.0f, { 1, 1, 1 }, true, 6.0f, { 0.4f, 0.2f, 0.4f }, true, 4.0f, { 0.2f, 0.2f, 0.2f }, false, {}, {}, 3, 0.8f };
		static const SacudidaMD S04342{ 1.75f, 0.2f, false, 6.0f, { 2, 2, 1 }, false, 6.0f, { 1, 1, 1 }, true, 10.0f, { 0.6f, 0.2f, 0.6f }, true, 12.0f, { 0.2f, 0.2f, 0.2f }, false, {}, {}, 3, 0.8f };
		static const SacudidaMD S04343{ 0.53f, 0.39f, true, 17.0f, { 6, 5, 12 }, false, 6.0f, { 1, 1, 1 }, false, 6.0f, { 1, 1, 1 }, false, 4.0f, { 1, 1, 1 }, true, { -1.5f, -1.0f, -1.9f }, { 1.5f, 1.0f, 2.3f }, 3, 0.8f };
		static const SacudidaMD S04887{ 0.24f, 0.27f, true, 13.0f, { 4, 2, 7 }, false, 6.0f, { 0.2f, 0.2f, 0.2f }, false, 6.0f, { 1, 1, 1 }, false, 4.0f, { 1, 1, 1 }, false, {}, {}, 3, 0.8f };
		static const SacudidaMD S05124{ 0.2f, 0.22f, true, 13.0f, { 0.3f, 0.3f, 0.3f }, false, 6.0f, { 1, 1, 1 }, false, 6.0f, { 1, 1, 1 }, false, 4.0f, { 1, 1, 1 }, false, {}, {}, 3, 0.8f };
		static const CardRunMD T[] = {
			{ "espadas", { "", "" }, { {}, {} }, &S04354, 1.333f },   // (Ef04354: md_efectos_ui.h; su Black tambien)
			{ "raigeki", { "ef04343_far", "ef04343_near" }, { { glm::vec3(0.0f), 0.0f, 37, NEGRO_04343 }, { glm::vec3(1.0f), 0.0f, 13, BLANCO_04343 } }, &S04343, 1.783f },
			{ "fuerza_espejo", { "ef04887", "ef04887op" }, { { glm::vec3(0.0f), 0.0f, 31, NEGRO_04887 }, {} }, &S04887, 1.5f, VISTA_MD },   // (la cupula de Ef04887 esta armada para la camara de MD: en la mesa rodeaba a la nuestra)
			{ "renacer", { "ef04842", "ef04842" }, { { glm::vec3(0.0f), 0.0f, 51, NEGRO_04842 }, {} }, nullptr, 2.5f, VISTA_PANTALLA },
			{ "agujero_negro", { "", "" }, { { glm::vec3(0.0f), 0.0f, 45, NEGRO_04342 }, {} }, &S04342, 2.167f },
			{ "cilindro_magico", { "ef05124_near", "ef05124_far" },   // (Ef05124: con la camara de duelo de MD)
			  { { glm::vec3(0.0f), 0.0f, 41, NEGRO_05124 }, { glm::vec3(1.0f), 1.1f, 6, BLANCO_05124 } }, &S05124, 2.0f, VISTA_MD },
		};
		for (const CardRunMD& c : T)
			if (receta == c.receta) return &c;
		return nullptr;
	}

	struct CardRunActivo { const CardRunMD* def; bool rival; float t; glm::vec3 big, lit; uint32_t sem; int cuadro; };
	std::vector<CardRunActivo> cardRunsMD;

	// el efecto de la carta empieza (desde el juego, con la receta de efectos_logica.h); false si MD no tiene uno
	bool lanzarCardRunMD(const std::string& receta, bool rival)
	{
		const CardRunMD* d = cardRunMD(receta);
		if (!d) return false;
		if (d->pista[rival ? 1 : 0][0]) pistaMD(d->pista[rival ? 1 : 0]);   // (se carga la primera vez)
		CardRunActivo a{ d, rival, 0.0f, glm::vec3(1.0f), glm::vec3(1.0f), (uint32_t)cardRunsMD.size() * 7919u + (uint32_t)(t * 1000.0f), 0 };
		a.big = glm::vec3(azarMD(a.sem) > 0.5f ? 1.0f : -1.0f, azarMD(a.sem + 1u) > 0.5f ? 1.0f : -1.0f, azarMD(a.sem + 2u) > 0.5f ? 1.0f : -1.0f);
		a.lit = glm::vec3(azarMD(a.sem + 3u) > 0.5f ? 1.0f : -1.0f, azarMD(a.sem + 4u) > 0.5f ? 1.0f : -1.0f, azarMD(a.sem + 5u) > 0.5f ? 1.0f : -1.0f);
		cardRunsMD.push_back(a);
		return true;
	}
	void avanzarCardRunMD(float dt)
	{
		for (CardRunActivo& a : cardRunsMD) a.t += dt, a.cuadro++;
		cardRunsMD.erase(std::remove_if(cardRunsMD.begin(), cardRunsMD.end(), [](const CardRunActivo& a) { return a.t > a.def->dur + 0.6f; }), cardRunsMD.end());
	}
	// la sacudida de camara de MD en este momento (unidades de MD; yugireload.cpp la pasa a la mesa)
	glm::vec3 sacudidaMD() const
	{
		glm::vec3 o(0.0f);
		for (const CardRunActivo& a : cardRunsMD)
			if (a.def->sacudida) o += getShakeMD(*a.def->sacudida, a.t, a.cuadro, a.big, a.lit, a.sem);
		return o;
	}
	bool hayCardRunMD() const { return !cardRunsMD.empty(); }

	// encima: false = en la escena (antes de los rotulos de la mesa); true = solo las de pantalla (MD effectType 26: van en
	// la capa DuelOverlayEffect3D, LayerName 17, encima de la mesa y de sus rotulos 3D, debajo de la interfaz 2D)
	void dibujarCardRunMD(bool encima = false)
	{
		for (CardRunActivo& a : cardRunsMD) {
			if (encima) {
				const char* nom = a.def->pista[a.rival ? 1 : 0];
				if (a.def->vista != VISTA_MESA && nom[0])
					if (PistaMD* p = pistaMD(nom)) dibujarPistaMD(*p, a.t, glm::vec3(0.0f), 1.0f, a.def->vista);
				continue;
			}
			for (const PantallaMD& p : a.def->pantallas) {
				const float al = p.n > 0 ? curvaPantallaMD(p, a.t) : 0.0f;
				if (al <= 0.001f) continue;
				usar(setAtlas, false);
				rect(glm::vec2(W, H) * 0.5f, glm::vec2(W, H) + glm::vec2(4.0f), 0.0f, 0.0f, LUZ, glm::vec4(p.color, al), glm::vec4(0.0f), 0.0f, glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
			}
			const char* nom = a.def->pista[a.rival ? 1 : 0];
			if (!nom[0] || a.def->vista != VISTA_MESA) continue;
			if (PistaMD* p = pistaMD(nom)) dibujarPistaMD(*p, a.t, glm::vec3(0.0f), 1.0f, a.def->vista);   // (centerEffectPath: el centro de la mesa)
		}
	}
