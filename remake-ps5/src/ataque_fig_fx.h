// rama classic-ps5-ataque (se incluye DENTRO de la clase VulkanExample, como figuras.h): el ATAQUE A DISTANCIA de las
// figuras 3D copiado de Yugi Reload 3D (D:\Yugi-Reload, remake-ps5/src): el usuario probo en la PS5 y "el rayo de las
// figuras 3D son pesimos, es una barra la que sale" (figuras.h efectosFiguras dibujaba 2 cilindros de luz, tramo(), de
// la boca al objetivo). Aqui va lo que ya estaba hecho y aprobado en Yugi Reload, traducido a este motor:
//  - reglas/regla_dragon.h ReglaDragon::rayoLuz (Ojos Azules, "Rafaga Blanca"): esfera de plasma que crece en la boca
//    con filamentos electricos, luz que se junta y chispas; al soltar, fogonazo y onda de choque, haz por capas con dos
//    espirales de relampagos, arcos, anillos que corren, pulsos y chispas; en el objetivo, explosion de luz, arcos que
//    lo envuelven y saltan a la mesa, chispas que rebotan, ondas de choque en la mesa y humo al final.
//  - ReglaDragon::llamarada (los dragones de fuego): lenguas que lamen la boca al cargar; cadena de lenguas de FUEGO DE
//    VERDAD (efecto_fuego.frag de Yugi Reload, procedural) que crece de la boca al objetivo, turbulencia, brasas, humo
//    (efecto_humo.frag) y llamas que envuelven al objetivo; NS_Explosion_Medium / Small y NS_Smoke_Plume del pack.
//  - yugireload.cpp dibujarEfectos + efectosPack del Mago Oscuro (lanzadores): carga electrica en la punta del baculo
//    (NS_Player_Electricity_Looping), y uno de sus rayos (esfera con estela NS_RocketTrail y NS_Explosion_Small,
//    relampago electrico con NS_TeslaCoil en el objetivo, espiral o anillos) con impacto y NS_Spark_Burst.
//  - Rayo (Craneo Convocado, Jinzo, Dragon de Trueno...): el rayo electrico de Yugi Reload (relampago con ramas, dos
//    hilos, NS_TeslaCoil y NS_Spark_Burst).
//  - efectosPack de Yugi Reload: el objetivo marcado en la mesa mientras carga (NS_Marker_Target) y el aura del atacante
//    (NS_Player_Buff/DeBuff_Looping).
// Tiempos: el reloj de Yugi Reload (SUELTA 1.65 s, FIN_EFECTO 2.9 s) se cuelga del golpe de Master Duel: el efecto llega
// al objetivo en el impacto del combate (T_IMPACTO, el danno y los LP), asi la secuencia de MD no cambia (ataque_md.h).
// Los efectos del pack son los horneados TAL CUAL (fx.h, assets/fx: no van al repo).

	// ---- los "efectos de verdad" de Yugi Reload (fuego y humo procedurales): efecto_billboard.vert + efecto_fuego.frag /
	// efecto_humo.frag, alfa premultiplicado, de lejos a cerca al final del ataque (dibujarVfxFig)
	struct VfxFig {
		glm::vec4 a;          // fuego: base, ancho; humo: centro, radio
		glm::vec4 b;          // fuego: punta, intensidad; humo: color, opacidad
		glm::vec4 c;          // x: tipo (0 fuego, 3 humo), y: semilla 0..1
		glm::vec4 d{ 0.0f }, e{ 0.0f }, f{ 0.0f };
	};
	static constexpr int VFX_FIG_FUEGO = 0, VFX_FIG_HUMO = 3;
	std::vector<VfxFig> colaVfxFig;
	VkPipeline pipeFuegoFig{ VK_NULL_HANDLE }, pipeHumoFig{ VK_NULL_HANDLE };

	// (preparePipelines: con la misma base que el haz, sin vertices y sin escribir profundidad)
	void crearPipelinesVfxFig(const VkGraphicsPipelineCreateInfo& base, const VkPipelineVertexInputStateCreateInfo& sinVertices,
	                          const VkPipelineDepthStencilStateCreateInfo& sinEscribir)
	{
		VkPipelineColorBlendAttachmentState premult{};
		premult.colorWriteMask = 0xf;
		premult.blendEnable = VK_TRUE;
		premult.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		premult.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		premult.colorBlendOp = VK_BLEND_OP_ADD;
		premult.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		premult.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		premult.alphaBlendOp = VK_BLEND_OP_ADD;
		VkPipelineColorBlendStateCreateInfo mezcla = vks::initializers::pipelineColorBlendStateCreateInfo(1, &premult);
		const char* frags[2] = { "efecto_fuego", "efecto_humo" };
		VkPipeline* destinos[2] = { &pipeFuegoFig, &pipeHumoFig };
		for (int k = 0; k < 2; k++) {
			std::array<VkPipelineShaderStageCreateInfo, 2> etapas = {
				loadShader(getShadersPath() + "yugireload/efecto_billboard.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
				loadShader(getShadersPath() + "yugireload/" + frags[k] + ".frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
			};
			VkGraphicsPipelineCreateInfo ci = base;
			ci.stageCount = 2;
			ci.pStages = etapas.data();
			ci.pVertexInputState = &sinVertices;
			ci.pColorBlendState = &mezcla;
			ci.pDepthStencilState = &sinEscribir;
			VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, nullptr, destinos[k]));
		}
	}
	void destruirPipelinesVfxFig()
	{
		vkDestroyPipeline(device, pipeFuegoFig, nullptr);
		vkDestroyPipeline(device, pipeHumoFig, nullptr);
	}

	// Pincel::llama de Yugi Reload: una lengua de fuego de la base a la punta
	void llamaFig(glm::vec3 base, glm::vec3 punta, float ancho, float intensidad, uint32_t semilla)
	{
		if (ancho <= 0.0f || intensidad <= 0.0f || glm::distance(base, punta) < 1e-3f || colaVfxFig.size() > 400) return;
		colaVfxFig.push_back({ glm::vec4(base, ancho), glm::vec4(punta, glm::min(intensidad, 1.5f)), glm::vec4((float)VFX_FIG_FUEGO, azar(semilla * 2654435761u + 17u), 0.0f, 0.0f) });
	}
	// Pincel::humo
	void humoFig(glm::vec3 centro, float radio, glm::vec3 color, float opacidad)
	{
		if (radio <= 0.0f || opacidad <= 0.0f || colaVfxFig.size() > 400) return;
		const uint32_t sem = (uint32_t)colaVfxFig.size() * 7919u + 101u;
		colaVfxFig.push_back({ glm::vec4(centro, radio), glm::vec4(color, glm::min(opacidad, 1.0f)), glm::vec4((float)VFX_FIG_HUMO, azar(sem), 0.0f, 0.0f) });
	}
	void dibujarVfxFig(VkCommandBuffer cmd)
	{
		if (colaVfxFig.empty()) return;
		if (pipeFuegoFig == VK_NULL_HANDLE) {
			colaVfxFig.clear();
			return;
		}
		const glm::vec3 ojo(uniformData.camara);
		auto lejania = [&](const VfxFig& e) {
			const glm::vec3 c = (int)e.c.x == VFX_FIG_FUEGO ? 0.5f * (glm::vec3(e.a) + glm::vec3(e.b)) : glm::vec3(e.a);
			return glm::distance(c, ojo);
		};
		std::stable_sort(colaVfxFig.begin(), colaVfxFig.end(), [&](const VfxFig& x, const VfxFig& y) { return lejania(x) > lejania(y); });
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		VkPipeline actual = VK_NULL_HANDLE;
		for (const VfxFig& e : colaVfxFig) {
			const VkPipeline p = (int)e.c.x == VFX_FIG_FUEGO ? pipeFuegoFig : pipeHumoFig;
			if (p != actual) {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
				actual = p;
			}
			vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VfxFig), &e);
			vkCmdDraw(cmd, 6, 1, 0, 0);
		}
		colaVfxFig.clear();
	}

	// ---- relampagos (yugireload.cpp de Yugi Reload: relampago / relampagoSimple), con el color del atacante
	glm::vec3 haloFx{ 1.0f }, nucleoFx{ 1.0f };
	void relampagoFig(VkCommandBuffer cmd, glm::vec3 a, glm::vec3 b, uint32_t semilla, float amplitud, float grosor, int ramas)
	{
		const int N = 16;
		const glm::vec3 d = b - a;
		const float largo = glm::length(d);
		if (largo < 1e-3f) return;
		const glm::vec3 eje = d / largo;
		const glm::vec3 p1 = glm::normalize(glm::cross(eje, glm::vec3(0.0f, 1.0f, 0.0f)) + glm::vec3(1e-4f));
		const glm::vec3 p2 = glm::normalize(glm::cross(eje, p1));
		glm::vec3 pts[N + 1];
		for (int k = 0; k <= N; k++) {
			const float f = (float)k / N;
			const float env = sinf(3.14159f * f);   // quieto en los extremos
			const float ox = (azar(semilla * 97u + (uint32_t)k * 13u) - 0.5f) * 2.0f;
			const float oy = (azar(semilla * 131u + (uint32_t)k * 7u + 5u) - 0.5f) * 2.0f;
			pts[k] = a + d * f + (p1 * ox + p2 * oy) * amplitud * largo * env;
		}
		for (int k = 0; k < N; k++) {
			tramo(cmd, pts[k], pts[k + 1], 0.055f * grosor, haloFx, 0.28f);
			tramo(cmd, pts[k], pts[k + 1], 0.022f * grosor, nucleoFx, 1.6f);
		}
		for (int r = 0; r < ramas; r++) {
			const int k = 2 + (int)(azar(semilla * 31u + (uint32_t)r * 17u) * (N - 4));
			const glm::vec3 desvio = glm::normalize(eje * 0.6f + (p1 * (azar(semilla + (uint32_t)r * 3u) - 0.5f) + p2 * (azar(semilla + (uint32_t)r * 5u + 1u) - 0.5f)) * 2.2f);
			const float l = largo * (0.08f + 0.12f * azar(semilla * 7u + (uint32_t)r));
			relampagoSimpleFig(cmd, pts[k], pts[k] + desvio * l, semilla * 3u + (uint32_t)r, grosor * 0.6f);
		}
	}
	void relampagoSimpleFig(VkCommandBuffer cmd, glm::vec3 a, glm::vec3 b, uint32_t semilla, float grosor)
	{
		const int N = 5;
		const glm::vec3 d = b - a;
		if (glm::length(d) < 1e-3f) return;
		const glm::vec3 eje = glm::normalize(d);
		const glm::vec3 p1 = glm::normalize(glm::cross(eje, glm::vec3(0.0f, 1.0f, 0.0f)) + glm::vec3(1e-4f));
		const glm::vec3 p2 = glm::normalize(glm::cross(eje, p1));
		glm::vec3 prev = a;
		for (int k = 1; k <= N; k++) {
			const float f = (float)k / N;
			glm::vec3 q = a + d * f;
			if (k < N) q += (p1 * (azar(semilla * 11u + (uint32_t)k) - 0.5f) + p2 * (azar(semilla * 19u + (uint32_t)k) - 0.5f)) * glm::length(d) * 0.35f;
			tramo(cmd, prev, q, 0.04f * grosor, haloFx, 0.25f);
			tramo(cmd, prev, q, 0.018f * grosor, nucleoFx, 1.2f);
			prev = q;
		}
	}

	// ---- la situacion del atacante en el reloj de Yugi Reload (combate_api.h Situacion)
	static constexpr float YR_SUELTA = 1.65f, YR_FIN_EFECTO = 2.9f;
	struct SitFx {
		float t{ 0.0f };            // s en el reloj de Yugi Reload (SUELTA = el efecto sale)
		float alto{ 2.6f }, ancho{ 1.5f };
		uint32_t semilla{ 0 };
		glm::vec3 halo{ 1.0f }, nucleo{ 1.0f };
	};
	// el reloj de Yugi Reload colgado del golpe de MD: tg = T_IMPACTO cae en SUELTA + llega (lo que tarda el efecto en
	// llegar al objetivo en Yugi Reload)
	// (rama classic-ps5-ataque4: el ataque entero ANTES del golpe: la carga empieza al empezar el ataque, el efecto sale a
	// 1.2 s y se apaga (FIN_EFECTO) justo en el golpe, ritmo::T_FIG_DISTANCIA; antes el golpe caia cuando el efecto llegaba)
	static constexpr float YR_INICIO = 0.45f;   // el reloj de Yugi Reload al empezar (la esfera de plasma crece desde 0.5)
	float relojYR(float llega) const
	{
		(void)llega;
		return YR_INICIO + realAtaqueFig(tAtaque);
	}
	static bool esLuzFig(const DatosFigura& d) { return d.halo.b > d.halo.r || d.halo.g > d.halo.r * 1.5f; }

	// ReglaDragon::llamarada de Yugi Reload (FUEGO DE VERDAD)
	void llamaradaFig(VkCommandBuffer cmd, const SitFx& s, glm::vec3 desde, glm::vec3 hasta, glm::vec3 dir, glm::vec3 lado, glm::vec3 arriba, float escala)
	{
		const float SUELTA = YR_SUELTA, FIN_EFECTO = YR_FIN_EFECTO;
		const glm::vec3 eje = hasta - desde;
		const float largo = glm::length(eje);
		const glm::vec3 brasa = glm::mix(glm::vec3(1.0f, 0.5f, 0.12f), glm::mix(s.halo, s.nucleo, 0.5f), 0.25f);
		const glm::vec3 gris(0.16f, 0.14f, 0.13f);
		auto trayecto = [&](float f) { return desde + eje * f; };
		// carga: lenguitas que se encienden y lamen la boca
		const float junta = suave(0.6f, SUELTA - 0.1f, s.t) * (1.0f - suave(SUELTA, SUELTA + 0.2f, s.t));
		if (junta > 0.0f) {
			for (int i = 0; i < 6; i++) {
				const float r0 = azar(s.semilla + 40u + (uint32_t)i);
				const float a = 6.2832f * ((float)i / 6.0f + r0) + s.t * (3.0f + 2.0f * r0);
				const glm::vec3 sale = dir * 0.6f + arriba * (0.5f + 0.4f * sinf(s.t * 7.0f + (float)i)) + lado * cosf(a) * 0.5f;
				const float alto = (0.2f + 0.12f * sinf(s.t * 11.0f + 2.0f * (float)i)) * escala * junta;
				llamaFig(desde, desde + sale * alto, 0.1f * escala * junta, 0.5f + 0.5f * junta, s.semilla + 50u + (uint32_t)i);
			}
			for (int i = 0; i < 4; i++) {
				const float q = fmodf(s.t * 1.5f + azar(s.semilla + 60u + (uint32_t)i), 1.0f);
				const glm::vec3 c = desde + arriba * (0.1f + 0.4f * q) * escala + lado * (azar(s.semilla + 70u + (uint32_t)i) - 0.5f) * 0.3f;
				bola(cmd, c, 0.03f * escala, brasa, 0.8f * junta * (1.0f - q));
			}
		}
		if (s.t < SUELTA || s.t > FIN_EFECTO + 0.8f) return;
		const float u = s.t - SUELTA;
		const float frente = suave(0.0f, 0.3f, u);
		const float cola = suave(FIN_EFECTO - 0.35f, FIN_EFECTO + 0.3f, s.t);
		const float fuerza = suave(0.0f, 0.08f, u) * (1.0f - suave(FIN_EFECTO - 0.1f, FIN_EFECTO + 0.6f, s.t));
		// la cadena: lenguas encadenadas que cubren el chorro (mas anchas hacia el objetivo)
		const int CADENA = 14;
		if (fuerza > 0.0f && frente > cola) {
			for (int i = 0; i < CADENA; i++) {
				const float f0 = glm::mix(cola, frente, (float)i / (float)CADENA);
				const float f1 = glm::mix(cola, frente, glm::min(1.0f, ((float)i + 1.6f) / (float)CADENA));
				const float t0 = s.t * 9.0f + (float)i * 1.7f;
				const glm::vec3 temb0 = (lado * sinf(t0) + arriba * cosf(t0 * 1.3f)) * (0.05f + 0.18f * f0) * escala;
				const glm::vec3 temb1 = (lado * sinf(t0 + 1.1f) + arriba * cosf(t0 * 1.3f + 0.8f)) * (0.05f + 0.18f * f1) * escala;
				const glm::vec3 a = trayecto(f0) + temb0;
				const glm::vec3 b = trayecto(f1) + temb1 + arriba * 0.08f * f1 * escala;
				llamaFig(a, b, (0.18f + 0.7f * f0) * escala, fuerza * (1.0f - 0.25f * f0), s.semilla * 31u + 1000u + (uint32_t)i);
			}
		}
		// lenguas sueltas que viajan por el chorro (turbulencia)
		for (int i = 0; i < 24; i++) {
			const float r0 = azar(s.semilla + 100u + (uint32_t)i), r1 = azar(s.semilla * 7u + 200u + (uint32_t)i), r2 = azar(s.semilla * 13u + 300u + (uint32_t)i);
			const float fase = r0 + u * (1.6f + 1.0f * r1);
			const float f = fase - floorf(fase);
			if (f > frente || f < cola) continue;
			const float abre = (0.1f + 0.45f * f) * escala;
			const float a = r2 * 6.2832f + s.t * (4.0f + 3.0f * r1) + f * 5.0f;
			const glm::vec3 turb = lado * cosf(a) * abre * (0.4f + 0.6f * r1) + arriba * (sinf(a) * abre * 0.6f + 0.15f * f * escala);
			const glm::vec3 c = trayecto(f) + turb;
			const float tr = (0.12f + 0.12f * r2) * largo * glm::min(1.0f, f * 4.0f + 0.2f);
			const glm::vec3 punta = c + dir * tr + arriba * 0.15f * f * escala;
			const float apaga = fuerza * suave(0.0f, 0.06f, f) * (1.0f - 0.4f * suave(0.85f, 1.0f, f));
			llamaFig(c, punta, (0.15f + 0.55f * f) * escala, apaga, s.semilla * 17u + 2000u + (uint32_t)i + 97u * (uint32_t)floorf(fase));
		}
		// brasas chicas que escapan del chorro
		for (int i = 0; i < 20; i++) {
			const float r0 = azar(s.semilla + 400u + (uint32_t)i), r1 = azar(s.semilla * 3u + 450u + (uint32_t)i);
			const float vida = 0.6f + 0.5f * r1;
			const float fase = (u + r0 * vida) / vida;
			const float q = fase - floorf(fase);
			const float nace = s.t - q * vida;
			if (nace < SUELTA || nace > FIN_EFECTO) continue;
			const uint32_t n = s.semilla + 461u * (uint32_t)i + 7919u * (uint32_t)floorf(fase);
			const float x = glm::max(cola, frente * azar(n));
			const float a = 6.2832f * azar(n + 1u);
			glm::vec3 c = trayecto(x) + (lado * cosf(a) + arriba * sinf(a)) * (0.3f + 0.6f * q) * escala + dir * 0.8f * q;
			c.y += 0.6f * q - 0.5f * q * q;
			bola(cmd, c, 0.025f + 0.025f * (1.0f - q), brasa, 1.1f * (1.0f - q) * fuerza);
		}
		// humo que sale de las puntas de la llamarada y sube
		const float humea = glm::min(1.0f, fuerza + 0.6f * suave(FIN_EFECTO - 0.2f, FIN_EFECTO + 0.2f, s.t) * (1.0f - suave(FIN_EFECTO + 0.2f, FIN_EFECTO + 0.8f, s.t)));
		for (int i = 0; i < 8; i++) {
			const float r0 = azar(s.semilla + 700u + (uint32_t)i);
			const float q = fmodf(u * 0.9f + r0, 1.0f);
			const float x = glm::clamp(frente * (0.5f + 0.5f * azar(s.semilla + 710u + (uint32_t)i)), 0.0f, 1.0f);
			const glm::vec3 c = trayecto(x) + arriba * (0.4f + 1.4f * q) * escala + lado * (r0 - 0.5f) * escala;
			humoFig(c, (0.25f + 0.5f * q) * escala, gris, 0.4f * humea * (1.0f - q) * frente);
		}
		// NS_Explosion_Medium del pack donde el aliento pega (y otra mas chica despues) y la columna de humo
		flipbooks.pedir(fx::Fx::ExplosionMediana, hasta, 1.5f * escala, s.t - (SUELTA + 0.25f), glm::vec4(1.0f));
		flipbooks.pedir(fx::Fx::ExplosionChica, hasta + lado * 0.3f * s.ancho + arriba * 0.3f * escala, 1.1f * escala, s.t - (SUELTA + 0.75f), glm::vec4(1.0f));
		const float hum = suave(SUELTA + 0.5f, SUELTA + 1.0f, s.t) * (1.0f - suave(FIN_EFECTO, FIN_EFECTO + 0.8f, s.t));
		if (hum > 0.0f) {
			flipbooks.pedir(fx::Fx::ColumnaHumo, glm::vec3(hasta.x, glm::max(0.0f, hasta.y - 0.4f * s.alto), hasta.z), 2.2f * escala, s.t, glm::vec4(0.7f, 0.7f, 0.7f, 0.7f * hum));
		}
		// llamas que envuelven al objetivo
		const float v = s.t - (SUELTA + 0.25f);
		if (v > 0.0f) {
			const float arde = suave(0.0f, 0.15f, v) * (1.0f - suave(FIN_EFECTO - SUELTA - 0.2f, FIN_EFECTO - SUELTA + 0.6f, v));
			if (arde > 0.0f) {
				const glm::vec3 pie(hasta.x, glm::max(0.0f, hasta.y - 0.6f * s.alto), hasta.z);
				for (int i = 0; i < 12; i++) {
					const float r0 = azar(s.semilla + 500u + (uint32_t)i), r1 = azar(s.semilla * 5u + 600u + (uint32_t)i);
					const float a = 6.2832f * ((float)i / 12.0f) + r0 + 0.4f * sinf(s.t * 3.0f + (float)i);
					const float radio = (0.35f + 0.25f * r1) * s.ancho * 0.5f;
					const float altura = s.alto * (0.15f + 0.85f * (float)(i % 3) / 3.0f);
					const glm::vec3 b = pie + glm::vec3(cosf(a) * radio, altura, sinf(a) * radio);
					const float lame = (0.5f + 0.35f * sinf(s.t * 8.0f + 2.3f * (float)i)) * escala;
					llamaFig(b, b + glm::vec3(0.0f, lame, 0.0f) - dir * 0.15f * escala, (0.35f + 0.2f * r1) * escala, arde, s.semilla * 11u + 3000u + (uint32_t)i);
				}
				for (int i = 0; i < 4; i++) {
					const float q = fmodf(v * 0.6f + azar(s.semilla + 650u + (uint32_t)i), 1.0f);
					const glm::vec3 c = hasta + lado * (azar(s.semilla + 660u + (uint32_t)i) - 0.5f) * s.ancho + glm::vec3(0.0f, (0.4f + 1.5f * q) * escala, 0.0f);
					humoFig(c, (0.3f + 0.5f * q) * escala, gris, 0.45f * arde * (1.0f - q));
				}
			}
		}
	}

	// ReglaDragon::rayoLuz de Yugi Reload (Ojos Azules, Rafaga Blanca)
	void rayoLuzFig(VkCommandBuffer cmd, const SitFx& s, glm::vec3 desde, glm::vec3 hasta, glm::vec3 dir, glm::vec3 lado, glm::vec3 arriba, float escala)
	{
		const float SUELTA = YR_SUELTA, FIN_EFECTO = YR_FIN_EFECTO;
		const glm::vec3 blanco(1.0f, 1.0f, 1.0f);
		const glm::vec3 azul = glm::mix(glm::vec3(0.35f, 0.62f, 1.0f), s.halo, 0.45f);
		const glm::vec3 celeste = glm::mix(blanco, glm::mix(s.nucleo, azul, 0.3f), 0.5f);
		haloFx = azul;
		nucleoFx = celeste;
		const float pi2 = 6.2832f;
		const uint32_t cuadro = (uint32_t)(glm::max(0.0f, s.t) * 25.0f);   // los arcos cambian de forma 25 veces/s
		const float R = glm::clamp(0.12f * s.ancho, 0.3f, 0.5f);           // radio del haz
		const float u = s.t - SUELTA;
		// ---------------- CARGA: la esfera de plasma
		if (s.t > 0.5f && s.t < SUELTA + 0.1f) {
			const float crece = suave(0.5f, SUELTA - 0.05f, s.t);
			const float apaga = 1.0f - suave(SUELTA, SUELTA + 0.1f, s.t);
			const float k = suave(0.5f, 0.75f, s.t) * apaga;
			const float parpadeo = 0.8f + 0.2f * azar(cuadro * 7u + 3u);
			const float re = (0.12f + 0.8f * crece) * glm::max(escala, 0.9f) * (1.0f + 0.05f * sinf(s.t * 41.0f));
			const glm::vec3 c = desde + dir * re * 0.9f;
			bola(cmd, c, re * 2.2f, azul, (0.05f + 0.06f * crece) * k * parpadeo);
			bola(cmd, c, re, azul, 0.2f * k * parpadeo);
			bola(cmd, c, re * 0.35f, celeste, 0.5f * k);
			bola(cmd, c, re * 0.14f, blanco, (1.6f + 1.4f * crece) * k);
			const int FIL = 6 + (int)(10.0f * crece);
			for (int i = 0; i < FIL; i++) {
				const float r0 = azar(s.semilla + 40u + (uint32_t)i), r1 = azar(s.semilla * 3u + 70u + (uint32_t)i);
				const float th = pi2 * r0 + s.t * (0.8f + 1.5f * r1) * (i % 2 ? 1.0f : -1.0f);
				const float ph = 1.4f * sinf(s.t * (0.7f + r1) + 5.0f * r0);
				const glm::vec3 v = (lado * cosf(th) + arriba * sinf(th)) * cosf(ph) + dir * sinf(ph);
				const float sale = 0.95f + 0.45f * azar(cuadro * 13u + (uint32_t)i * 7u);
				relampagoFig(cmd, c, c + v * re * sale, s.semilla * 7u + cuadro * 131u + (uint32_t)i, 0.16f, 0.7f + 0.6f * crece, 1);
			}
			for (int i = 0; i < 10; i++) {   // luz que se junta: rayas que caen hacia la esfera
				const float r0 = azar(s.semilla + 120u + (uint32_t)i);
				const float q = fmodf(s.t * 1.7f + r0, 1.0f);
				const float th = pi2 * azar(s.semilla + 130u + (uint32_t)i + 17u * (uint32_t)(s.t * 1.7f + r0));
				const glm::vec3 v = glm::normalize(lado * cosf(th) + arriba * sinf(th) + dir * (r0 - 0.5f));
				const glm::vec3 a = c + v * re * (4.0f - 3.0f * q);
				tramo(cmd, a, glm::mix(a, c, 0.35f), 0.02f * escala, celeste, 0.9f * k * q);
			}
			for (int i = 0; i < 10; i++) {   // chispas que saltan de la superficie
				const float r0 = azar(s.semilla + 150u + (uint32_t)i);
				const float q = fmodf(s.t * 2.3f + r0, 1.0f);
				const uint32_t n = s.semilla + 160u * (uint32_t)i + 977u * (uint32_t)(s.t * 2.3f + r0);
				const float th = pi2 * azar(n), ph = 1.4f * (azar(n + 1u) - 0.5f);
				const glm::vec3 v = (lado * cosf(th) + arriba * sinf(th)) * cosf(ph) + dir * sinf(ph);
				glm::vec3 p = c + v * re * (1.0f + 1.6f * q);
				p.y -= 0.5f * q * q;
				bola(cmd, p, 0.03f * escala, blanco, 1.4f * k * (1.0f - q));
			}
			for (int i = 0; i < 3; i++) {   // arcos al suelo y al cuerpo
				const uint32_t n = cuadro / 2u * 29u + (uint32_t)i * 11u + s.semilla;
				if (azar(n) > 0.25f + 0.55f * crece) continue;
				glm::vec3 b;
				if (i == 0) b = glm::vec3(c.x, 0.02f, c.z) + (lado * (azar(n + 1u) - 0.5f) + dir * azar(n + 2u)) * 1.2f * escala;
				else b = desde - dir * (0.25f + 0.25f * azar(n + 3u)) * s.ancho + lado * (azar(n + 4u) - 0.5f) * 0.5f * s.ancho - arriba * (0.1f + 0.25f * azar(n + 5u)) * s.alto;
				relampagoFig(cmd, c + glm::normalize(b - c) * re * 0.8f, b, n * 3u + cuadro, 0.09f, 1.0f, 2);
			}
		}
		if (s.t < SUELTA || s.t > FIN_EFECTO + 1.6f) return;
		const glm::vec3 eje = hasta - desde;
		const float largo = glm::length(eje);
		const float frente = suave(0.0f, 0.12f, u);
		const float cola = suave(FIN_EFECTO - 0.25f, FIN_EFECTO + 0.1f, s.t);
		const float fuerza = suave(0.0f, 0.04f, u) * (1.0f - suave(FIN_EFECTO - 0.1f, FIN_EFECTO + 0.35f, s.t));
		const float late = 1.0f + 0.08f * sinf(s.t * 47.0f) + 0.05f * sinf(s.t * 23.0f + 1.0f);
		const float r = R * late * (1.0f + 0.5f * expf(-u * 7.0f));
		auto punto = [&](float f, float a, float d) { return desde + eje * f + (lado * cosf(a) + arriba * sinf(a)) * d; };
		// ---------------- DISPARO: el haz
		if (fuerza > 0.0f && frente > cola) {
			const glm::vec3 b = desde + eje * frente;
			// (rama classic-ps5-ataque4: el usuario "quita la barra": sin los 5 cilindros de luz del haz de Yugi Reload; todo lo
			// demas de ReglaDragon::rayoLuz queda tal cual: la punta que avanza, las espirales de relampagos, los arcos, los
			// anillos, los pulsos y las chispas)
			if (frente < 1.0f) bola(cmd, b, r * 1.8f, celeste, 0.6f * fuerza);
			const int VUELTAS = 7;   // dos espirales de relampagos que se enroscan alrededor del haz
			for (int h = 0; h < 2; h++) {
				for (int i = 0; i < VUELTAS; i++) {
					const float f0 = glm::mix(cola, frente, (float)i / (float)VUELTAS), f1 = glm::mix(cola, frente, (float)(i + 1) / (float)VUELTAS);
					const float g0 = 2.2f * (float)i + 3.1416f * (float)h - s.t * 13.0f, g1 = g0 + 2.2f;
					const float d0 = r * (1.05f + 0.35f * azar(cuadro * 5u + (uint32_t)i * 3u + (uint32_t)h));
					const float d1 = r * (1.05f + 0.35f * azar(cuadro * 5u + (uint32_t)i * 3u + (uint32_t)h + 3u));
					relampagoFig(cmd, punto(f0, g0, d0), punto(f1, g1, d1), s.semilla * 5u + cuadro * 61u + (uint32_t)i * 2u + (uint32_t)h, 0.14f, 1.3f, 1);
				}
			}
			for (int i = 0; i < 8; i++) {   // arcos que salen del haz y vuelven a entrar
				const uint32_t n = s.semilla * 11u + (cuadro / 2u) * 97u + (uint32_t)i * 13u;
				const float f0 = glm::mix(cola, frente, azar(n));
				const float df = (0.6f + 0.9f * azar(n + 1u)) / glm::max(largo, 1.0f);
				const float f1 = glm::min(frente, f0 + df);
				const float a0 = pi2 * azar(n + 2u);
				const glm::vec3 p0 = punto(f0, a0, r * 0.6f);
				const glm::vec3 pm = punto(0.5f * (f0 + f1), a0 + 0.4f, r * (1.9f + 1.0f * azar(n + 3u)));
				const glm::vec3 p1 = punto(f1, a0 + 0.8f, r * 0.6f);
				relampagoFig(cmd, p0, pm, n, 0.2f, 1.0f, 1);
				relampagoFig(cmd, pm, p1, n + 7u, 0.2f, 1.0f, 0);
			}
			for (int i = 0; i < 5; i++) {   // anillos que corren por el haz
				const float f = glm::mix(cola, frente, fmodf(u * 1.6f + (float)i / 5.0f, 1.0f));
				const glm::vec3 c = desde + eje * f;
				tramo(cmd, c - dir * 0.04f, c + dir * 0.04f, r * 1.5f, azul, 0.22f * fuerza);
			}
			for (int i = 0; i < 6; i++) {   // pulsos blancos dentro del haz
				const float f = fmodf(azar(s.semilla + 220u + (uint32_t)i) + u * 2.8f, 1.0f);
				if (f > frente || f < cola) continue;
				bola(cmd, desde + eje * f, r * (0.9f + 0.3f * sinf(s.t * 30.0f + (float)i)), celeste, 0.3f * fuerza);
			}
			for (int i = 0; i < 10; i++) {   // chispas que escapan del haz
				const float r0 = azar(s.semilla + 260u + (uint32_t)i);
				const float q = fmodf(u * 2.2f + r0, 1.0f);
				const uint32_t n = s.semilla + 270u * (uint32_t)i + 911u * (uint32_t)(u * 2.2f + r0);
				const float f = glm::mix(cola, frente, azar(n));
				glm::vec3 p = punto(f, pi2 * azar(n + 1u), r * (0.8f + 2.5f * q)) + dir * q * 0.6f;
				p.y -= 0.6f * q * q;
				bola(cmd, p, 0.035f * escala, blanco, 1.3f * fuerza * (1.0f - q));
			}
			if (cola < 0.05f) {   // la boca: brillo sostenido
				bola(cmd, desde, r * 1.8f, azul, 0.35f * fuerza);
				bola(cmd, desde, r * 0.7f, blanco, 1.5f * fuerza);
			}
		}
		if (u < 0.5f) {   // fogonazo en la boca y onda de choque al soltar
			const float q = u / 0.5f, fog = expf(-u * 7.0f);
			bola(cmd, desde, (0.6f + 1.4f * fog) * escala, glm::mix(azul, blanco, 0.3f), 0.5f * fog);
			const float ro = (0.4f + 3.2f * sqrtf(q)) * escala;
			const glm::vec3 c = desde + dir * 0.5f * q;
			const int N = 12;
			for (int i = 0; i < N; i++) {
				const float a0 = pi2 * (float)i / N, a1 = pi2 * (float)(i + 1) / N;
				tramo(cmd, c + (lado * cosf(a0) + arriba * sinf(a0)) * ro, c + (lado * cosf(a1) + arriba * sinf(a1)) * ro, (0.03f + 0.05f * (1.0f - q)) * escala, azul, 0.9f * (1.0f - q));
			}
		}
		// ---------------- IMPACTO en el objetivo
		const float v = s.t - (SUELTA + 0.1f);
		if (v <= 0.0f) return;
		const float pega = suave(0.0f, 0.05f, v) * (1.0f - suave(FIN_EFECTO - SUELTA - 0.25f, FIN_EFECTO - SUELTA + 0.4f, v));
		const glm::vec3 pie(hasta.x, 0.03f, hasta.z);
		if (pega > 0.0f) {
			const float vib = 0.85f + 0.15f * sinf(s.t * 29.0f), estalla = expf(-v * 4.0f);
			bola(cmd, hasta, (1.3f + 1.2f * suave(0.0f, 0.3f, v) + 1.2f * estalla) * escala * vib, azul, (0.1f + 0.35f * estalla) * pega);
			bola(cmd, hasta, 0.9f * escala * vib, celeste, 0.3f * pega);
			bola(cmd, hasta, 0.35f * escala * vib, blanco, 2.0f * pega);
			for (int i = 0; i < 8; i++) {   // arcos que lo envuelven y saltan a la mesa
				const uint32_t n = s.semilla * 13u + (cuadro / 2u) * 53u + (uint32_t)i * 17u;
				if (azar(n + 9u) > 0.8f) continue;
				const float a0 = pi2 * azar(n);
				glm::vec3 b;
				if (i % 2 == 0) {
					const float d = (1.0f + 1.6f * azar(n + 1u)) * escala;
					b = pie + glm::vec3(cosf(a0) * d, 0.0f, sinf(a0) * d);
				} else {
					b = hasta + glm::vec3(cosf(a0), 1.2f * (azar(n + 2u) - 0.4f), sinf(a0)) * (0.5f + 0.4f * azar(n + 3u)) * s.ancho * 0.6f;
				}
				relampagoFig(cmd, hasta, b, n, 0.12f, 1.4f, 2);
			}
			for (int i = 0; i < 12; i++) {   // chispas que rebotan hacia atras
				const float q = fmodf(v * 1.8f + azar(s.semilla + 500u + (uint32_t)i), 1.0f);
				const float a0 = pi2 * azar(s.semilla + 510u + (uint32_t)i + 7u * (uint32_t)(v * 1.8f + azar(s.semilla + 500u + (uint32_t)i)));
				const glm::vec3 sale = lado * cosf(a0) + arriba * sinf(a0) - dir * 0.6f;
				tramo(cmd, hasta + sale * (0.5f + 1.2f * q) * escala, hasta + sale * (0.75f + 1.8f * q) * escala, 0.03f * escala, blanco, 1.2f * pega * (1.0f - q));
			}
		}
		for (int j = 0; j < 2; j++) {   // ondas de choque en la mesa
			const float w = v * 1.3f - 0.45f * (float)j;
			if (w < 0.0f || w > 1.0f || (j > 0 && pega <= 0.0f)) continue;
			const float ro = (0.4f + 4.2f * sqrtf(w)) * escala, br = (j == 0 ? 1.0f : 0.6f) * (1.0f - w);
			const int N = 12;
			for (int i = 0; i < N; i++) {
				const float a0 = pi2 * (float)i / N, a1 = pi2 * (float)(i + 1) / N;
				tramo(cmd, pie + glm::vec3(cosf(a0), 0.0f, sinf(a0)) * ro, pie + glm::vec3(cosf(a1), 0.0f, sinf(a1)) * ro, (0.04f + 0.08f * (1.0f - w)) * escala, azul, 0.8f * br);
			}
		}
		const float h = s.t - (FIN_EFECTO - 0.3f);   // humo que queda al final
		if (h > 0.0f && h < 1.9f) {
			const float k = suave(0.0f, 0.2f, h) * (1.0f - suave(1.0f, 1.9f, h));
			const glm::vec3 gris(0.2f, 0.21f, 0.25f);
			for (int i = 0; i < 6; i++) {
				const float r0 = azar(s.semilla + 950u + (uint32_t)i);
				const float q = fmodf(h * 0.5f + r0 * 0.5f, 1.0f);
				const glm::vec3 c = pie + glm::vec3((r0 - 0.5f) * 1.6f, 0.3f + 1.8f * q, (azar(s.semilla + 960u + (uint32_t)i) - 0.5f) * 1.6f) * escala;
				humoFig(c, (0.35f + 0.6f * q) * escala, gris, 0.45f * k * (1.0f - q));
			}
		}
	}

	// el Mago Oscuro de Yugi Reload (dibujarEfectos + efectosPack): carga, y su rayo (tipo 0 esfera, 1 electrico, 2 espiral,
	// 3 anillos); electrico = el de los "Rayo" (Craneo Convocado, Jinzo...)
	void rayoMagoFig(VkCommandBuffer cmd, const SitFx& s, int tipo, glm::vec3 punta, glm::vec3 destino)
	{
		const float T_SUELTA = YR_SUELTA, T_FIN_RAYO = YR_FIN_EFECTO;
		const glm::vec3 arriba(0.0f, 1.0f, 0.0f);
		haloFx = s.halo;
		nucleoFx = s.nucleo;
		const float t = s.t;
		const uint32_t paso = (uint32_t)(glm::max(t, 0.0f) * 25.0f);
		const glm::vec4 magia(glm::mix(glm::vec3(1.0f), s.halo, 0.4f) * 1.3f, 1.0f);
		const float carga = suave(0.4f, T_SUELTA, t) * (1.0f - suave(T_FIN_RAYO - 0.2f, T_FIN_RAYO + 0.3f, t));
		if (carga > 0.0f) {
			flipbooks.pedir(fx::Fx::Electricidad, punta - arriba * 0.45f, 1.0f, t, glm::vec4(glm::vec3(magia), carga));
			const float lat = 1.0f + 0.25f * sinf(t * 38.0f);
			bola(cmd, punta, 0.30f * carga * lat, s.halo, 0.35f);
			bola(cmd, punta, 0.10f * carga * lat, s.nucleo, 1.4f);
			for (int k = 0; k < 4; k++) {
				const glm::vec3 dir = glm::normalize(glm::vec3(azar(paso * 5u + (uint32_t)k) - 0.5f, azar(paso * 7u + (uint32_t)k + 9u) - 0.5f, azar(paso * 3u + (uint32_t)k + 4u) - 0.5f));
				relampagoSimpleFig(cmd, punta, punta + dir * 0.45f * carga, paso * 13u + (uint32_t)k, 0.8f);
			}
		}
		if (tipo == 0) {   // la esfera viaja antes de soltar: llega en T_SUELTA
			const float viaje = suave(T_SUELTA - 0.35f, T_SUELTA, t);
			if (viaje > 0.0f && viaje < 1.0f) {
				const glm::vec3 p = glm::mix(punta, destino, viaje);
				const float lat = 1.0f + 0.2f * sinf(t * 40.0f);
				bola(cmd, p, 0.34f * lat, s.halo, 0.4f);
				bola(cmd, p, 0.13f * lat, s.nucleo, 1.6f);
				for (int k = 1; k <= 6; k++) bola(cmd, glm::mix(punta, destino, glm::max(0.0f, viaje - 0.05f * (float)k)), 0.2f * (1.0f - (float)k / 7.0f), s.halo, 0.25f);
				flipbooks.pedir(fx::Fx::Estela, p, 0.9f, t, magia, punta - destino);
			}
			flipbooks.pedir(fx::Fx::ExplosionChica, destino, 1.3f, t - T_SUELTA, magia);
		} else {
			flipbooks.pedir(fx::Fx::Chispas, destino, 1.2f, t - T_SUELTA, magia);
		}
		const float grosor = (t < T_SUELTA || t > T_FIN_RAYO) ? 0.0f
		                   : suave(T_SUELTA, T_SUELTA + 0.12f, t) * (1.0f - suave(T_FIN_RAYO - 0.35f, T_FIN_RAYO, t)) * (0.85f + 0.15f * sinf(t * 70.0f));
		if (grosor <= 0.0f) return;
		if (tipo == 1) flipbooks.pedir(fx::Fx::Tesla, destino - arriba * 0.5f, 1.6f, t, glm::vec4(glm::vec3(magia), glm::min(grosor, 1.0f)));
		const glm::vec3 d = destino - punta;
		const float largo = glm::length(d);
		const glm::vec3 eje = d / glm::max(largo, 1e-4f);
		const glm::vec3 p1 = glm::normalize(glm::cross(eje, glm::vec3(0.0f, 1.0f, 0.0f)) + glm::vec3(1e-4f));
		const glm::vec3 p2 = glm::normalize(glm::cross(eje, p1));
		if (tipo == 1) {
			relampagoFig(cmd, punta, destino, paso + 1u, 0.06f, grosor, 3);
			relampagoFig(cmd, punta, destino, paso * 7u + 3u, 0.09f, grosor * 0.55f, 1);   // un segundo hilo mas fino
		} else if (tipo == 2) {   // dos hilos que giran uno alrededor del otro
			const int N = 28;
			for (int h = 0; h < 2; h++) {
				glm::vec3 prev = punta;
				for (int k = 1; k <= N; k++) {
					const float f = (float)k / N;
					const float a = f * 6.2832f * 3.0f + t * 9.0f + (float)h * 3.1416f;
					const float r = 0.22f * sinf(3.1416f * f) * grosor;
					const glm::vec3 q = punta + d * f + (p1 * cosf(a) + p2 * sinf(a)) * r;
					tramo(cmd, prev, q, 0.05f * grosor, s.halo, 0.35f);
					tramo(cmd, prev, q, 0.018f * grosor, s.nucleo, 1.3f);
					prev = q;
				}
			}
		} else if (tipo == 3) {   // ondas en forma de aro unidas por un hilo fino
			const int SEG = 18;   // (ataque4: sin el hilo recto entre los aros: se veia como una barra)
			for (int k = 0; k < 4; k++) {
				const float f = fmodf(t * 1.6f + (float)k * 0.25f, 1.0f);
				const glm::vec3 c = punta + d * f;
				const float r = (0.12f + 0.38f * f) * grosor, brillo = 1.0f - 0.6f * f;
				glm::vec3 prev = c + p1 * r;
				for (int q = 1; q <= SEG; q++) {
					const float a = 6.2832f * (float)q / SEG;
					const glm::vec3 pt = c + (p1 * cosf(a) + p2 * sinf(a)) * r;
					tramo(cmd, prev, pt, 0.05f * grosor, s.halo, 0.4f * brillo);
					tramo(cmd, prev, pt, 0.018f * grosor, s.nucleo, 1.3f * brillo);
					prev = pt;
				}
			}
		}
		// impacto: resplandor y chispas que saltan
		const float lat = 1.0f + 0.3f * sinf(t * 47.0f);
		bola(cmd, destino, 0.45f * grosor * lat, s.halo, 0.3f);
		bola(cmd, destino, 0.14f * grosor * lat, s.nucleo, 1.5f);
		for (int k = 0; k < 6; k++) {
			const glm::vec3 dir = glm::normalize(glm::vec3(azar(paso * 23u + (uint32_t)k) - 0.5f, azar(paso * 29u + (uint32_t)k + 2u) - 0.2f, azar(paso * 31u + (uint32_t)k + 6u) - 0.5f));
			relampagoSimpleFig(cmd, destino, destino + dir * (0.35f + 0.35f * azar(paso + (uint32_t)k * 41u)), paso * 17u + (uint32_t)k, 0.7f);
		}
	}

	// el ataque a distancia de la figura de zonaAtacante (efectosFiguras). El pipeline de efectos ya esta atado
	void ataqueDistanciaFig(VkCommandBuffer cmd, const Figura& f, glm::vec3 p0, glm::vec3 p1)
	{
		const AtaqueFig at = f.datos.ataque;
		SitFx s;
		s.alto = f.alto;
		s.ancho = glm::clamp(0.6f * f.alto, 1.0f, 3.0f);
		s.semilla = (uint32_t)ataquesVistos * 7919u + 13u;
		s.halo = f.datos.halo;
		s.nucleo = f.datos.nucleo;
		const glm::vec3 eje = p1 - p0;
		const float largo = glm::length(eje);
		if (largo < 0.05f) return;
		const glm::vec3 dir = eje / largo;
		glm::vec3 lado = glm::cross(dir, glm::vec3(0.0f, 1.0f, 0.0f));
		lado = glm::length(lado) > 0.01f ? glm::normalize(lado) : glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::vec3 arriba = glm::cross(lado, dir);
		const float escala = glm::clamp(s.alto / 3.0f, 0.6f, 1.5f);
		const bool luz = esLuzFig(f.datos);
		const bool mago = at == AtaqueFig::Magia;
		const float llega = at == AtaqueFig::Aliento ? (luz ? 0.12f : 0.3f) : 0.0f;
		s.t = relojYR(llega);
#if defined(PS5_HOST_REFERENCE)
		if (medirFiguras && (int)(tiempo * 30.0f) % 10 == 0) fprintf(stderr, "[figuras] efecto %s: reloj de Yugi Reload %.2f (%s)\n", f.slug.c_str(), s.t, at == AtaqueFig::Aliento ? (luz ? "rafaga de luz" : "llamarada") : mago ? "magia" : "rayo");
#endif
		// efectosPack de Yugi Reload: el aura del atacante mientras junta fuerza
		const float aura = suave(0.2f, 0.55f, s.t) * (1.0f - suave(YR_SUELTA, YR_SUELTA + 0.4f, s.t));
		if (aura > 0.0f) {
			flipbooks.pedir(mago ? fx::Fx::AuraOscura : fx::Fx::Aura, baseZona(zonaAtacante), 1.15f * glm::max(s.alto, s.ancho), s.t,
			                glm::vec4(glm::mix(glm::vec3(1.0f), s.halo, 0.35f), 0.8f * aura));
		}
		if (at == AtaqueFig::Aliento) {
			if (luz) rayoLuzFig(cmd, s, p0, p1, dir, lado, arriba, escala);
			else llamaradaFig(cmd, s, p0, p1, dir, lado, arriba, escala);
		} else {
			// Magia: uno de los rayos del Mago (sorteado por ataque); Rayo: el electrico
			const int tipo = mago ? (int)(azar(s.semilla * 31u + 7u) * 4.0f) % 4 : 1;
			rayoMagoFig(cmd, s, tipo, p0, p1);
		}
	}

	// el objetivo marcado en la mesa mientras el atacante carga (NS_Marker_Target); se apaga con el golpe
	void marcaObjetivoFig()
	{
		if (!hayAtaque() || ataqueDirecto || !tieneFigura(zonaAtacante)) return;
		const float t = relojYR(0.0f);
		const float marca = suave(0.15f, 0.45f, t) * (1.0f - suave(YR_SUELTA, YR_SUELTA + 0.35f, t));
		if (marca > 0.0f) flipbooks.pedir(fx::Fx::Marca, centroZona(zonaObjetivo), 2.4f, t, glm::vec4(glm::vec3(1.2f), marca));
	}

	// PC (YUGI_FIGURAS_ATAQUE="2.5:jugador_monstruo_2>rival"): ataque directo de prueba de la carta de la zona a, como el
	// del duelo (duelo_juego.h: la zona del centro del que recibe y el punto de MD), con el ATK de la carta de danno
	bool atacarDirectoPrueba(int a)
	{
		if (hayAtaque() || !esMonstruo(a) || ctl.zonas[a].vacia()) return false;
		const bool rival = zonaRival(a);
		const int o = control::zonaPorId(rival ? "jugador_monstruo_2" : "rival_monstruo_2");
		if (o < 0) return false;
		const int atk = interfazDuelo.atkDe(ctl.zonas[a].carta);
		zonaAtacante = a;
		zonaObjetivo = o;
		tAtaque = 0.0f;
		danio = atk;
		danioAlRival = !rival;
		muereA = muereO = false;
		ataqueDemo = false;
		ataquesVistos++;
		ataqueAltoMD = ritmo::ataqueAlto(atk);
		prepararAtaqueDirecto(a, true);
		fprintf(stderr, "[ataque] directo %s (%d) -> %s: punto (%.2f %.2f %.2f)\n", ctl.zonas[a].carta.c_str(), atk, rival ? "jugador" : "rival", puntoDirecto.x, puntoDirecto.y,
		        puntoDirecto.z);
		return true;
	}

	// rama classic-ps5-ataque4: los valores del calculo que se muestran sobre las cartas (Interfaz::dibujarCalculoMD), al
	// empezar el ataque: el ATK del atacante y el ATK o DEF del objetivo como los rotulos de la mesa (equipos, campos y
	// efectos: statsZona; si la carta ya no esta en el duelo, los de la carta)
	int calcA{ -1 }, calcO{ -1 };
	bool calcDef{ false };
	void prepararCalculoAtaque()
	{
		calcA = calcO = -1;
		calcDef = false;
		if (!hayAtaque() || ctl.zonas[zonaAtacante].vacia()) return;
		auto valores = [&](int z, int& a, int& d) {
			const control::Celda& c = ctl.zonas[z];
			a = interfazDuelo.atkDe(c.carta);
			d = interfazDuelo.defDe(c.carta);
			int ea = a, ed = d;   // (los del duelo, con equipos y efectos; si el duelo no la tiene o no los calculo, los de la carta)
			if (interfazDuelo.statsZona && interfazDuelo.statsZona(interfaz::ZONAS[z].id, ea, ed) && (ea > 0 || ed > 0)) a = ea, d = ed;
		};
		int a, d;
		valores(zonaAtacante, a, d);
		calcA = a;
		if (ataqueDirecto || ctl.zonas[zonaObjetivo].vacia()) return;
		valores(zonaObjetivo, a, d);
		calcDef = ctl.zonas[zonaObjetivo].defensa;
		calcO = calcDef ? d : a;
	}
