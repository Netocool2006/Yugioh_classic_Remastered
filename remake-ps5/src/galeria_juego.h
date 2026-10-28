	// ================================================================ galeria de figuras 3D (rama classic-galeria)
	// (se incluye DENTRO de la clase VulkanExample, despues de figuras.h y mazo_duelo.h). La logica es de galeria.h y lo
	// 2D (rejilla, nombre, detalle, botones) de galeria_ui.h; aca: abrirla desde la Lista de cartas, el mando, y la
	// escena del visor: la figura sobre un pedestal con luz de estudio y su sombra, girando sola, y su ataque (el clip
	// de ataque si tiene esqueleto; si no, el de figuras.h: aliento, magia, rayo, espada o embestida) contra un punto
	// en el aire a su izquierda. Mientras esta abierta el cuadro es SOLO esto (cuadroGaleria): ni mesa ni entorno.
	// Memoria: no carga nada; usa la figura que figuras.h ya tiene en memoria (cargadas una vez para el duelo).
	// Ganchos (yugireload.cpp): avanzarGaleria() en render() antes de avanzarConstructor(), y al principio de
	// updateUniformBuffers() y buildCommandBuffer() el desvio a uniformesGaleria() / cuadroGaleria().
	// PC: YUGI_GALERIA=1 (o =<id de carta>) la abre al arrancar; YUGI_GALERIA_ATAQUE=<s> ataca a los <s> s.
	galeria::Galeria galeriaFig;
	bool galeriaLista{ false };
	control::Repetidor repetidorGaleria;
	float galeriaAtaquePrueba{ -1.0f };
	float galCamDist{ -1.0f }, galCamMira{ 1.5f }, galPedestal{ 1.2f };
	bool galImpactoSono{ false };
	static constexpr float GAL_CENTRO_NDC = 2.0f * (1360.0f / 1920.0f) - 1.0f;   // el centro del visor (x 1360 de 1920)
	static constexpr float GAL_ALTO_PEDESTAL = 0.34f;
	static constexpr float GAL_GIRO_ATAQUE = -1.05f;   // hacia donde ataca (rad): a la izquierda y un poco hacia la camara

	bool galeriaVisible() const { return galeriaFig.abierta; }

	void prepararGaleria()
	{
		galeriaLista = true;
		std::vector<std::vector<std::string>> porFig((size_t)numFiguras);
		for (const auto& p : figuraDeCarta) {
			if (p.second >= 0 && p.second < numFiguras) porFig[(size_t)p.second].push_back(p.first);
		}
		std::vector<galeria::Entrada> v;
		for (int k = 0; k < numFiguras; k++) {
			const Figura& f = figuras[(size_t)k];
			galeria::Entrada e;
			e.cartas = porFig[(size_t)k];
			e.carta = e.cartas.empty() ? std::string() : e.cartas[0];
			e.fig = k;
			e.ataque = (int)f.datos.ataque;
			e.esqueleto = f.esqueleto;
			e.clipAtaque = f.ataque >= 0;
			v.push_back(std::move(e));
		}
		galeriaFig.preparar(&coleccion, std::move(v));
		interfazDuelo.conectarGaleria(&galeriaFig);
		fprintf(stderr, "[galeria] %zu figuras en la galeria (%d desbloqueadas)\n", galeriaFig.entradas.size(), galeriaFig.desbloqueadas());
#if defined(PS5_HOST_REFERENCE)
		if (const char* g = getenv("YUGI_GALERIA")) abrirGaleria(std::string(g) == "1" ? std::string() : std::string(g), false);
		if (const char* g = getenv("YUGI_GALERIA_ATAQUE")) galeriaAtaquePrueba = (float)atof(g);
#endif
	}

	void abrirGaleria(const std::string& carta, bool desdeLista)
	{
		galeriaFig.abrir(carta, desdeLista);
		repetidorGaleria = control::Repetidor{};
		galCamDist = -1.0f;
		galImpactoSono = false;
		ui.visible = false;
		sonido.sfx("sfx_constructor_abrir");
		mando.pressed = 0;
		fprintf(stderr, "[galeria] abierta en %s\n", galeriaFig.actual() ? galeriaFig.actual()->carta.c_str() : "-");
	}

	// cada cuadro, antes del constructor: el pedido de la Lista de cartas (Cuadrado / R3) y, abierta, el mando entero
	void avanzarGaleria()
	{
		if (!mandoListo) return;
		if (!galeriaLista) prepararGaleria();
		if (!galeriaFig.abierta) {
			if (constructorMazo.abierto && constructorMazo.pedidoGaleria != 0) {
				const int pedido = constructorMazo.pedidoGaleria;
				constructorMazo.pedidoGaleria = 0;
				const std::string c = constructorMazo.cartaGaleria;
				const bool tiene = galeriaFig.tieneFigura(c);
				if (pedido == 2 && !tiene) {
					constructorMazo.avisar(constructor::T("Esta carta no tiene figura 3D"));
					sonido.sfx("sfx_error", 0.5f);
				} else {
					abrirGaleria(tiene ? c : std::string(), true);
				}
			}
			if (!galeriaFig.abierta) return;
		}
		const float dt = glm::clamp(frameTimer, 0.0f, 0.1f);
		const uint32_t extra = teclas | pulsosRaton;   // (PC: el teclado como botones; rama classic-pc: y el raton)
		teclas = 0;
		pulsosRaton = 0;
		const uint32_t recien = botonesDe(mando.pressed) | extra;
		const uint32_t p = repetidorGaleria.paso(botonesDe(mando.held), recien, mando.left_x, mando.left_y, dt) | (recien & ~control::B_DIRECCIONES);
		auto venceA = [this](const mazo::Carta& k) { return coleccion.venceA(k); };
		const float antes = galeriaFig.tAtaque;
		uint32_t ev = galeriaFig.avanzar(dt, p, mando.right_x, mando.right_y, mando.l2, mando.r2, venceA);
#if defined(PS5_HOST_REFERENCE)
		if (galeriaAtaquePrueba >= 0.0f && galeriaFig.t >= galeriaAtaquePrueba) {
			galeriaAtaquePrueba = -1.0f;
			ev |= galeriaFig.atacar(venceA);
		}
#endif
		if (ev & galeria::EV_ATACA) {
			galImpactoSono = false;
			sonido.sfx("sfx_atacar");
		}
		const float impacto = T_IMPACTO / VEL_FIGURAS;
		if (!galImpactoSono && galeriaFig.atacando() && antes < impacto && galeriaFig.tAtaque >= impacto) {
			galImpactoSono = true;
			sonido.sfx("sfx_impacto");
		}
		if (ev & galeria::EV_MOVIO) sonido.sfx("sfx_menu_cursor", 0.7f);
		if (ev & galeria::EV_BLOQUEADA) sonido.sfx("sfx_error", 0.5f);
		if (ev & galeria::EV_DETALLE) sonido.sfx("sfx_detalle");
		if (ev & galeria::EV_CERRO) {
			sonido.sfx("sfx_menu_cancelar");
			fprintf(stderr, "[galeria] cerrada\n");
		}
		// el resto del cuadro (constructor, duelo) no ve el mando
		mando.pressed = mando.held = 0;
		mando.left_x = mando.left_y = mando.right_x = mando.right_y = 0.0f;
		mando.l2 = mando.r2 = 0.0f;
	}

	// ---------------------------------------------------------------------------------------------- la figura
	Figura* figuraGaleria()
	{
		const galeria::Entrada* e = galeriaFig.actual();
		return e && e->fig >= 0 && e->fig < numFiguras ? &figuras[(size_t)e->fig] : nullptr;
	}
	static void medidasGaleria(const Figura& f, float& alto, float& ancho)
	{
		const glm::vec3 lo = glm::min(f.modelo.dimensions.min, f.modelo.dimensions.max), hi = glm::max(f.modelo.dimensions.min, f.modelo.dimensions.max);
		alto = glm::max(f.alto, 0.3f);
		ancho = glm::max(hi.x - lo.x, hi.z - lo.z) * f.escala;
		if (!(ancho > 0.05f) || ancho > 40.0f) ancho = 0.6f * alto;
		ancho = glm::clamp(ancho, 0.3f * alto, 2.5f * alto);
	}
	glm::vec3 dirAtaqueGaleria() const { return glm::vec3(sinf(GAL_GIRO_ATAQUE), 0.0f, cosf(GAL_GIRO_ATAQUE)); }
	float relojAtaqueGaleria() const { return galeriaFig.atacando() && !galeriaFig.bloqueadaActual() ? galeriaFig.tAtaque * VEL_FIGURAS : -1.0f; }

	// la matriz de la figura sobre el pedestal (gira sola o con el stick; en el ataque gira hacia el blanco y vuelve)
	glm::mat4 matrizGaleria(const Figura& f, float& dis, float& holo)
	{
		const glm::vec3 base(0.0f, GAL_ALTO_PEDESTAL, 0.0f);
		float alto, ancho;
		medidasGaleria(f, alto, ancho);
		float giro = galeriaFig.giro;
		glm::vec3 mueve(0.0f);
		float inclina = 0.0f, aplasta = 1.0f, escala = 1.0f;
		const bool alado = f.datos.cuerpo == CuerpoFig::Alado;
		float vuelo = alado ? 0.20f + 0.06f * sinf(tiempo * 1.7f) : 0.0f;
		if (!f.esqueleto) aplasta *= 1.0f + 0.012f * sinf(tiempo * 2.3f);
		const float ap = suave(0.0f, 0.9f, galeriaFig.tAparece);
		dis = ap < 1.0f ? -(1.0f - ap) : 0.0f;
		escala *= 0.85f + 0.15f * ap;
		holo = 0.12f + 1.2f * (1.0f - ap);
		const float tg = relojAtaqueGaleria();
		if (tg >= 0.0f) {
			const glm::vec3 d = dirAtaqueGaleria();
			const float mira = suave(0.0f, 0.22f, tg) * (1.0f - suave(1.25f, 1.55f, tg));
			giro += remainderf(GAL_GIRO_ATAQUE - giro, 2.0f * glm::pi<float>()) * mira;
			holo += 0.9f * suave(0.15f, 0.45f, tg) * (1.0f - suave(0.7f, 1.0f, tg));
			const AtaqueFig at = f.datos.ataque;
			if (at == AtaqueFig::Embestida || at == AtaqueFig::Espada) {
				const float carga = suave(0.08f, 0.25f, tg), va = suave(0.25f, T_IMPACTO, tg), vuelve = suave(0.78f, 1.25f, tg);
				const float avance = (1.0f + 0.2f * alto) * va * (1.0f - vuelve);
				mueve += d * (avance - 0.3f * carga * (1.0f - va));
				if (f.datos.cuerpo != CuerpoFig::Bipedo || at == AtaqueFig::Embestida) mueve.y += 0.7f * sinf(glm::pi<float>() * va) * (1.0f - vuelve);
				inclina += (-0.12f * carga * (1.0f - va) + 0.3f * va) * (1.0f - vuelve);
			} else {
				inclina += (-0.08f * suave(0.1f, 0.4f, tg) + 0.18f * suave(0.4f, T_IMPACTO, tg)) * (1.0f - suave(0.8f, 1.2f, tg));
				if (alado) vuelo += 0.35f * suave(0.1f, 0.45f, tg) * (1.0f - suave(0.9f, 1.4f, tg));
			}
		}
		const glm::mat4 lugar = glm::translate(glm::mat4(1.0f), base + mueve + glm::vec3(0.0f, vuelo, 0.0f));
		const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), giro, glm::vec3(0.0f, 1.0f, 0.0f)) * glm::rotate(glm::mat4(1.0f), inclina, glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::mat4 forma = glm::scale(glm::mat4(1.0f), glm::vec3(escala, aplasta * escala, escala) * f.escala);
		return lugar * rot * forma * glm::translate(glm::mat4(1.0f), -f.centro);
	}

	// (con esqueleto) reposo y, al atacar, su clip de ataque (o la parada rapida de la espada), como en el duelo
	void posarGaleria()
	{
		Figura* fp = figuraGaleria();
		if (!fp || !fp->esqueleto) return;
		Figura& f = *fp;
		posarClip(f.modelo, f.reposo, tiempo, true);
		const float tg = relojAtaqueGaleria();
		if (tg >= 0.0f) {
			const float real = galeriaFig.tAtaque, impactoReal = T_IMPACTO / VEL_FIGURAS;
			if (f.ataque >= 0) {
				const float t = real - (impactoReal - 1.0f);
				const float largo = f.modelo.animations[(size_t)f.ataque].end - f.modelo.animations[(size_t)f.ataque].start;
				clipEncima(f, f.ataque, t, suave(0.0f, 0.2f, t) * (1.0f - suave(largo - 0.25f, largo, t)));
			} else if (f.datos.ataque == AtaqueFig::Espada && f.defensa >= 0) {
				const float t = (real - (impactoReal - 0.35f)) * 1.6f;
				clipEncima(f, f.defensa, t, suave(0.0f, 0.15f, t) * (1.0f - suave(0.9f, 1.4f, t)));
			}
		}
		for (vkglTF::Node* n : f.modelo.nodes) n->update();
	}

	// donde sale el ataque (boca, punta del baculo...) con la matriz M de la figura
	glm::vec3 salidaGaleria(Figura& f, const glm::mat4& M)
	{
		if (f.esqueleto && f.datos.ataque == AtaqueFig::Magia) {
			for (vkglTF::Skin* piel : f.modelo.skins) {
				for (size_t k = 0; k < piel->joints.size(); k++) {
					if (piel->joints[k]->name == "LeftHand") return glm::vec3(M * piel->joints[k]->getMatrix() * piel->inverseBindMatrices[k] * glm::vec4(f.datos.salida, 1.0f));
				}
			}
		}
		return glm::vec3(M * glm::vec4(f.datos.salida, 1.0f));
	}

	// imagenes = false: pasada de sombra. Bloqueada: silueta oscura (emisivo casi negro con un borde frio)
	void dibujarFiguraGaleria(VkCommandBuffer cmd, bool imagenes)
	{
		Figura* fp = figuraGaleria();
		if (!fp) return;
		Figura& f = *fp;
		float dis, holo;
		const glm::mat4 M = matrizGaleria(f, dis, holo);
		if (fabsf(dis) >= 0.999f || (!imagenes && fabsf(dis) > 0.5f)) return;
		const bool bloq = galeriaFig.bloqueadaActual();
		glm::vec4 efecto(holo, 0.55f, dis, 0.0f), vida(0.0f), ojoA(0.0f), ojoB(0.0f);
		if (bloq) {
			efecto = glm::vec4(0.0f, 0.0f, dis, 0.03f);
			vida = glm::vec4(0.10f, 0.14f, 0.26f, 0.0f);
		} else if (f.datos.alas && !f.esqueleto) {
			// el aleteo sin esqueleto (alasFigura de figuras.h, sin zona)
			const glm::vec3 lo = glm::min(f.modelo.dimensions.min, f.modelo.dimensions.max), hi = glm::max(f.modelo.dimensions.min, f.modelo.dimensions.max);
			const glm::vec3 tam = hi - lo;
			const float hz = glm::clamp(1.9f - 0.22f * f.alto, 0.7f, 1.6f);
			ojoA = glm::vec4(0.5f * (lo.x + hi.x), f.datos.altura, f.datos.bisagra * tam.x, -1.0f - f.datos.pendiente);
			ojoB = glm::vec4(0.42f * sinf(tiempo * 2.0f * glm::pi<float>() * hz), 0.06f * tam.x, f.datos.abajo, f.datos.corte);
		}
		if (!f.esqueleto) {
			dibujar(cmd, f.modelo, M, efecto, vida, imagenes, ojoA, ojoB);
			return;
		}
		if (pipesFig.escena == VK_NULL_HANDLE) return;
		const Empuje empuje{ M, efecto, vida, ojoA, ojoB };
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagenes ? pipesFig.escena : pipesFig.sombra);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipesFig.layout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		vkCmdPushConstants(cmd, pipesFig.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Empuje), &empuje);
		const VkDeviceSize desde = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &f.modelo.vertices.buffer, &desde);
		vkCmdBindIndexBuffer(cmd, f.modelo.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		for (vkglTF::Node* n : f.modelo.linearNodes) {
			if (!n->mesh) continue;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipesFig.layout, 2, 1, &n->mesh->uniformBuffer.descriptorSet, 0, nullptr);
			for (vkglTF::Primitive* pr : n->mesh->primitives) {
				if (imagenes) vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipesFig.layout, 1, 1, &pr->material.descriptorSet, 0, nullptr);
				vkCmdDrawIndexed(cmd, pr->indexCount, 1, pr->firstIndex, 0, 0);
			}
		}
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagenes ? pipelines.escena : pipelines.sombra);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		if (f.mano && hayEspadaFig) {
			dibujar(cmd, espadaFig, M * f.mano->getMatrix() * f.agarre, bloq ? efecto : glm::vec4(efecto.x, 0.9f, efecto.z, 0.0f), vida, imagenes);
		}
	}

	glm::mat4 matrizPedestal() const { return glm::scale(glm::mat4(1.0f), glm::vec3(galPedestal, GAL_ALTO_PEDESTAL, galPedestal)); }

	// ---------------------------------------------------------------------------------------------- camara y luz
	void uniformesGaleria()
	{
		posarGaleria();
		float alto = 2.6f, ancho = 1.6f;
		if (const Figura* f = figuraGaleria()) medidasGaleria(*f, alto, ancho);
		const float radio = glm::max(0.55f * ancho, 0.9f) + 0.15f;
		const float aspecto = (float)width / (float)height;
		const float fov = glm::radians(34.0f), tv = tanf(fov * 0.5f);
		const float total = alto + GAL_ALTO_PEDESTAL + (figuraGaleria() && figuraGaleria()->datos.cuerpo == CuerpoFig::Alado ? 0.3f : 0.0f);
		// que entre en el visor (debajo del titulo y sobre la barra) y en su ancho (1120 de 1920)
		const float d1 = 0.5f * total * 1.42f / tv;
		const float d2 = 0.5f * glm::max(ancho, 2.0f * radio) * 1.2f / (tv * aspecto * (1120.0f / 1920.0f));
		const float dist = glm::max(d1, d2) * galeriaFig.zoom;
		const float mira = total * 0.47f;
		const float k = galCamDist < 0.0f ? 1.0f : 1.0f - expf(-glm::clamp(frameTimer, 0.0f, 0.1f) * 7.0f);
		galCamDist = galCamDist < 0.0f ? dist : galCamDist + (dist - galCamDist) * k;
		galCamMira += (mira - galCamMira) * k;
		galPedestal += (radio - galPedestal) * k;
		const float elev = glm::radians(9.0f);
		const glm::vec3 objetivo(0.0f, galCamMira, 0.0f);
		const glm::vec3 ojo = objetivo + galCamDist * glm::vec3(0.0f, sinf(elev), cosf(elev));
		uniformData.proyeccion = glm::perspectiveRH(fov, aspecto, 0.1f, 160.0f);
		uniformData.proyeccion[2][0] = -GAL_CENTRO_NDC;   // el centro de la imagen en el centro del visor
		uniformData.proyeccion[1][1] *= -1.0f;
		uniformData.vista = glm::lookAtRH(ojo, objetivo, glm::vec3(0.0f, 1.0f, 0.0f));
		// luz de estudio: el sol arriba a la derecha y adelante; la sombra solo cubre el pedestal y la figura
		const glm::vec3 sol = glm::normalize(glm::vec3(0.55f, 1.0f, 0.65f));
		const float r = glm::max(glm::max(total, ancho), 2.0f * radio) * 0.75f + 1.5f;
		const glm::vec3 centro(0.0f, 0.4f * total, 0.0f);
		const glm::mat4 vistaSol = glm::lookAtRH(centro + sol * 30.0f, centro, glm::vec3(0.0f, 0.0f, -1.0f));
		uniformData.luz = glm::orthoRH(-r, r, -r, r, 1.0f, 60.0f) * vistaSol;
		uniformData.haciaSol = glm::vec4(sol, 0.0f);
		uniformData.camara = glm::vec4(ojo, 1.0f);
		uniformData.pantalla = glm::vec4((float)width, (float)height, tiempo, 0.0f);
		uniformData.bosque = glm::vec4(0.0f, -1.0f, -1.0f, 0.0f);
		uniformData.bosque2 = glm::vec4(0.0f);
		for (glm::vec4& e : uniformData.ent) e = glm::vec4(0.0f);
		memcpy(uniformBuffers[currentBuffer].mapped, &uniformData, sizeof(uniformData));
	}

	// ---------------------------------------------------------------------------------------------- el cuadro
	void sombraGaleria(VkCommandBuffer cmd)
	{
		const VkImageSubresourceRange rango{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		vks::tools::insertImageMemoryBarrier(cmd, sombra.imagen, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
		                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, rango);
		VkRenderingAttachmentInfo profundidad{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = sombra.vista,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = { .depthStencil = { 1.0f, 0 } },
		};
		VkRenderingInfo info{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = { { 0, 0 }, { TAM_SOMBRA, TAM_SOMBRA } },
			.layerCount = 1,
			.pDepthAttachment = &profundidad,
		};
		vkCmdBeginRendering(cmd, &info);
		VkViewport viewport = vks::initializers::viewport((float)TAM_SOMBRA, (float)TAM_SOMBRA, 0.0f, 1.0f);
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor = vks::initializers::rect2D(TAM_SOMBRA, TAM_SOMBRA, 0, 0);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sombra);
		dibujar(cmd, rayo, matrizPedestal(), glm::vec4(0.0f), glm::vec4(0.0f), false);
		dibujarFiguraGaleria(cmd, false);
		vkCmdEndRendering(cmd);
		vks::tools::insertImageMemoryBarrier(cmd, sombra.imagen, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		                                     VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		                                     VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, rango);
	}

	// luz que suma: el disco y el aro del pedestal, la columna al aparecer y el ataque a distancia o el golpe
	void efectosGaleria(VkCommandBuffer cmd)
	{
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.efectos);
		const bool bloq = galeriaFig.bloqueadaActual();
		const glm::vec3 cian = bloq ? glm::vec3(0.55f, 0.6f, 0.8f) : glm::vec3(0.3f, 0.8f, 1.5f);
		const float late = 0.8f + 0.2f * sinf(tiempo * 2.0f);
		const float arriba = GAL_ALTO_PEDESTAL + 0.004f;
		dibujar(cmd, esfera, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, arriba, 0.0f)), glm::vec3(galPedestal * 1.02f, 0.01f, galPedestal * 1.02f)),
		        glm::vec4(0.0f, 0.0f, 0.0f, 0.06f * late), glm::vec4(cian, 1.0f), true);
		const int lados = 40;
		for (int i = 0; i < lados; i++) {
			const float a0 = 2.0f * glm::pi<float>() * (float)i / (float)lados, a1 = 2.0f * glm::pi<float>() * (float)(i + 1) / (float)lados;
			const glm::vec3 p0(galPedestal * cosf(a0), arriba, galPedestal * sinf(a0)), p1(galPedestal * cosf(a1), arriba, galPedestal * sinf(a1));
			tramo(cmd, p0, p1, 0.02f, cian, 1.3f * late);
			tramo(cmd, p0, p1, 0.09f, cian, 0.18f * late);
		}
		Figura* fp = figuraGaleria();
		if (!fp) return;
		Figura& f = *fp;
		float alto, ancho;
		medidasGaleria(f, alto, ancho);
		const glm::vec3 base(0.0f, GAL_ALTO_PEDESTAL, 0.0f);
		// columna de luz al aparecer (cambio de figura)
		if (galeriaFig.tAparece < 1.1f) {
			const float t = galeriaFig.tAparece, vida = 1.0f - suave(0.5f, 1.1f, t);
			tramo(cmd, base, base + glm::vec3(0.0f, alto + 2.5f, 0.0f), 0.55f * suave(0.0f, 0.25f, t) * glm::max(0.6f, 0.4f * ancho), cian, 0.35f * vida);
			tramo(cmd, base, base + glm::vec3(0.0f, alto + 2.5f, 0.0f), 0.18f * suave(0.0f, 0.25f, t), glm::vec3(0.95f), 0.8f * vida);
		}
		const float tg = relojAtaqueGaleria();
		if (tg < 0.0f) return;
		const glm::vec3 d = dirAtaqueGaleria();
		const glm::vec3 halo = f.datos.halo, nucleo = f.datos.nucleo;
		const AtaqueFig at = f.datos.ataque;
		const glm::vec3 p1 = base + d * (1.6f + 0.35f * alto + 0.5f * ancho) + glm::vec3(0.0f, 0.45f * alto, 0.0f);
		if (at == AtaqueFig::Embestida || at == AtaqueFig::Espada) {
			// el golpe: chispas en el punto del choque
			if (tg > T_IMPACTO) {
				const float u = tg - T_IMPACTO, f2 = expf(-u * 6.0f);
				const glm::vec3 pc = base + d * (1.0f + 0.2f * alto + 0.55f * ancho) + glm::vec3(0.0f, 0.5f * alto, 0.0f);
				bola(cmd, pc, 0.3f + 1.0f * u, glm::vec3(1.4f, 0.9f, 0.4f), 0.7f * f2);
				bola(cmd, pc, 0.15f + 0.35f * u, glm::vec3(1.0f, 0.95f, 0.8f), 1.8f * f2);
			}
			return;
		}
		if (tg < 0.12f || tg > 1.0f) return;
		float dis, holo;
		const glm::vec3 p0 = salidaGaleria(f, matrizGaleria(f, dis, holo));
		const float carga = suave(0.12f, 0.42f, tg) * (1.0f - suave(0.5f, 0.6f, tg));
		bola(cmd, p0, 0.12f + 0.35f * carga, halo, 0.6f * carga);
		bola(cmd, p0, 0.05f + 0.12f * carga, nucleo, 1.6f * carga);
		const float sale = suave(0.42f, T_IMPACTO, tg), apaga = 1.0f - suave(0.72f, 0.95f, tg);
		if (sale > 0.0f && apaga > 0.0f) {
			const glm::vec3 frente = glm::mix(p0, p1, sale);
			const float parpadeo = 0.85f + 0.15f * sinf(tiempo * 60.0f);
			if (at == AtaqueFig::Magia) {
				tramo(cmd, p0, frente, 0.22f, halo, 0.55f * apaga * parpadeo);
				tramo(cmd, p0, frente, 0.07f, nucleo, 2.2f * apaga * parpadeo);
				bola(cmd, frente, 0.35f, halo, 0.7f * apaga);
				bola(cmd, frente, 0.14f, nucleo, 2.0f * apaga);
			} else if (at == AtaqueFig::Aliento) {
				for (int k = 0; k < 16; k++) {
					const float u = fmodf((float)k / 16.0f + tiempo * 2.2f, 1.0f) * sale;
					const glm::vec3 p = glm::mix(p0, p1, u) + glm::vec3(azar((uint32_t)k * 7u) - 0.5f, azar((uint32_t)k * 11u) - 0.5f, azar((uint32_t)k * 13u) - 0.5f) * (0.35f * u);
					bola(cmd, p, 0.15f + 0.45f * u, halo, 0.35f * apaga);
					bola(cmd, p, 0.06f + 0.18f * u, nucleo, 0.9f * apaga);
				}
				tramo(cmd, p0, frente, 0.12f, nucleo, 1.4f * apaga);
				bola(cmd, frente, 0.55f, halo, 0.8f * apaga);
			} else {
				for (int r = 0; r < 3; r++) {
					const uint32_t sem = (uint32_t)(tiempo * 20.0f) * 97u + (uint32_t)r * 7919u;
					glm::vec3 desde = p0;
					const int tramos = 7;
					for (int i = 1; i <= tramos; i++) {
						glm::vec3 hasta = glm::mix(p0, frente, (float)i / (float)tramos);
						if (i < tramos) hasta += (glm::vec3(azar(sem + (uint32_t)i * 3u), azar(sem + (uint32_t)i * 5u), azar(sem + (uint32_t)i * 11u)) - 0.5f) * 0.9f;
						tramo(cmd, desde, hasta, 0.05f, nucleo, 2.4f * apaga);
						tramo(cmd, desde, hasta, 0.22f, halo, 0.4f * apaga);
						desde = hasta;
					}
				}
				bola(cmd, frente, 0.45f, halo, 0.8f * apaga);
			}
		}
		if (tg > T_IMPACTO) {
			const float u = tg - T_IMPACTO, f2 = expf(-u * 6.0f);
			bola(cmd, p1, 0.4f + 1.2f * u, halo, 0.7f * f2);
			bola(cmd, p1, 0.2f + 0.4f * u, nucleo, 1.8f * f2);
		}
	}

	// el cuadro entero mientras la galeria esta abierta: fondo, suelo, pedestal, figura, luz y la interfaz encima
	void cuadroGaleria()
	{
		VkCommandBuffer cmd = drawCmdBuffers[currentBuffer];
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBufInfo));
		sombraGaleria(cmd);
		beginDynamicRendering(cmd);
		VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.escena);
		// fondo: una esfera grande vista desde adentro (su degradado, oscurecido a azul noche) y el suelo: un disco claro
		// al centro que se apaga hacia afuera (emisivos: sin sol)
		dibujar(cmd, esfera, glm::scale(glm::mat4(1.0f), glm::vec3(80.0f)), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), glm::vec4(0.012f, 0.032f, 0.03f, 0.0f), true);
		dibujar(cmd, esfera, glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.03f, 0.0f)), glm::vec3(18.0f, 0.03f, 18.0f)), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
		        glm::vec4(0.02f, 0.05f, 0.03f, 0.0f), true);
		// el pedestal (piedra pizarra: emisivo tenue con el borde mas claro; su textura blanca con sol se quemaba) y la figura
		dibujar(cmd, rayo, matrizPedestal(), glm::vec4(0.0f, 0.0f, 0.0f, 0.45f), glm::vec4(0.16f, 0.20f, 0.30f, 0.0f), true);
		dibujarFiguraGaleria(cmd, true);
		efectosGaleria(cmd);
		interfazDuelo.dibujar(cmd, currentBuffer, uniformData.proyeccion * uniformData.vista, (float)width, (float)height);
		drawUI(cmd);
		endDynamicRendering(cmd);
		VK_CHECK_RESULT(vkEndCommandBuffer(cmd));
	}
