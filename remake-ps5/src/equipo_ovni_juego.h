// Rama classic-ps5-equipo-ovni (se incluye dentro de la clase del juego, yugireload.cpp). Dos pedidos del usuario en la PS5:
//  A) "Las cartas que equipan poder, puntos o lo que sea que le de equipo a otra carta deben generar un circulo de
//     equipamiento, asi como el circulo que le pones a los monstruos con la carta de campo Bosque. Ese circulo quiero, y
//     que se sepa, cuando me paro encima de ella con el cursor, la carta a la que esta equipando".
//     El circulo es el del Bosque (dibujarCampo: el aro y el sello bajo los potenciados, efecto_haz) en dorado: bajo el
//     monstruo equipado y bajo la carta de equipo, tenue y permanente mientras dure el vinculo (MagiaTrampa::eqUid del
//     motor: todos los equipos, uniones, Premature Burial...), con un aro que se abre al aparecer. Con el cursor sobre la
//     carta de equipo (o sobre el monstruo) se enciende el circulo de los dos y sale un rayo de union con luces que corren
//     de la carta al monstruo.
//  B) "El efecto de abduccion de ovni que le pones a la carta de campo Bosque cuando la sacas, quiero ese mismo efecto
//     cuando entre una figura 3D, en ataque y volteada de defensa boca abajo": la columna de luz verde y dorada que baja
//     sobre la carta, el aro y el sello, y el estallido de hojas de luz (dibujarCampo, CAMPO_ACTIVANDO), sobre la zona de
//     la figura. Reemplaza la bola de luz y las chispas con que se materializaba (figuras.h efectosFiguras) y el destello
//     de aparicion de las cartas (efectosPack).
struct VinculoEquipo { int mt, mon; };   // zona de la carta de equipo y zona del monstruo equipado
std::vector<VinculoEquipo> vinculosEquipo;
std::array<float, interfaz::NUM_ZONAS> tEquipo{};        // s desde que el vinculo de la carta de esa zona existe
std::array<int, interfaz::NUM_ZONAS> equipoPrevio{};     // a que zona de monstruo estaba vinculada (-1 ninguna)
bool equiposListos{ false };

// los vinculos que se ven en la mesa: la carta de equipo boca arriba y el monstruo boca arriba (la presentacion va
// detras del motor: solo cuenta lo que la mesa ya muestra)
void actualizarEquipos(float dt)
{
	if (!equiposListos) tEquipo.fill(99.0f), equipoPrevio.fill(-1), equiposListos = true;
	vinculosEquipo.clear();
	if (ctl.vista.pantalla == control::VistaDuelo::DUELO) {
		for (int lado = 0; lado < 2; lado++) {
			for (int i = 0; i < ia::ZONAS; i++) {
				const ia::MagiaTrampa& k = dueloIA.l[(size_t)lado].mt[(size_t)i];
				if (k.vacia() || k.bocaAbajo || k.eqUid <= 0) continue;
				const int zmt = zonaMesaIA(lado, i, true);
				if (zmt < 0 || ctl.zonas[(size_t)zmt].vacia() || ctl.zonas[(size_t)zmt].bocaAbajo) continue;
				int la = 0, z = 0;
				if (!dueloIA.porUid(k.eqUid, &la, &z)) continue;
				const int zmo = zonaMesaIA(la, z, false);
				if (zmo < 0 || ctl.zonas[(size_t)zmo].vacia() || ctl.zonas[(size_t)zmo].bocaAbajo) continue;
				vinculosEquipo.push_back({ zmt, zmo });
			}
		}
	}
	std::array<int, interfaz::NUM_ZONAS> actual;
	actual.fill(-1);
	for (const VinculoEquipo& v : vinculosEquipo) actual[(size_t)v.mt] = v.mon;
	for (size_t z = 0; z < interfaz::NUM_ZONAS; z++) {
		if (actual[z] != equipoPrevio[z]) {
			equipoPrevio[z] = actual[z];
			if (actual[z] >= 0) tEquipo[z] = 0.0f;
		} else {
			tEquipo[z] += dt;
		}
	}
}

// el rayo de union (efectos: esferas y tramos aditivos) y los circulos (efecto_haz)
void dibujarEquipos(VkCommandBuffer cmd)
{
	if (vinculosEquipo.empty()) return;
	const int foco = ctl.foco.tipo == control::F_ZONA ? ctl.foco.i : -1;
	auto resalta = [&](const VinculoEquipo& v) { return foco >= 0 && (foco == v.mt || foco == v.mon); };
	const glm::vec3 oro(1.3f, 0.85f, 0.15f), blanco(0.95f, 1.0f, 0.85f), arriba(0.0f, 1.0f, 0.0f);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.efectos);
	for (const VinculoEquipo& v : vinculosEquipo) {
		if (!resalta(v)) continue;
		const float a = suave(0.0f, 0.25f, tEquipo[(size_t)v.mt]);
		const glm::vec3 p0 = centroZona(v.mt) + arriba * 0.3f, p1 = centroZona(v.mon) + arriba * 0.6f;
		auto punto = [&](float s) { return glm::mix(p0, p1, s) + arriba * (0.9f * sinf(glm::pi<float>() * s)); };
		const int N = 16;
		for (int k = 0; k < N; k++) {
			tramo(cmd, punto((float)k / N), punto((float)(k + 1) / N), 0.13f, oro, 0.30f * a);   // (el halo)
			tramo(cmd, punto((float)k / N), punto((float)(k + 1) / N), 0.06f, oro, 1.1f * a);
		}
		for (int k = 0; k < 5; k++) {   // las luces que corren de la carta de equipo al monstruo
			const float s = fmodf(tiempo * 0.7f + 0.2f * (float)k, 1.0f);
			bola(cmd, punto(s), 0.20f, oro, 0.8f * a);
			bola(cmd, punto(s), 0.07f, blanco, 1.2f * a);
		}
		bola(cmd, p1, 0.45f + 0.05f * sinf(tiempo * 5.0f), oro, 0.22f * a);   // el monstruo que recibe
	}
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.haz);
	std::array<int, interfaz::NUM_ZONAS> cuantos{};
	for (const VinculoEquipo& v : vinculosEquipo) {
		const float t = tEquipo[(size_t)v.mt], a = suave(0.0f, 0.5f, t), f = resalta(v) ? 1.0f : 0.0f;
		const float pulso = 0.38f + 0.10f * sinf(tiempo * 2.4f + (float)v.mt), fuerza = a * (1.0f + 2.6f * f);
		const int n = cuantos[(size_t)v.mon]++;   // (varios equipos en el mismo monstruo: circulos concentricos)
		const glm::vec3 bm = centroZona(v.mon) + glm::vec3(0.0f, 0.06f, 0.0f), bc = centroZona(v.mt) + glm::vec3(0.0f, 0.06f, 0.0f);
		const float Rm = 1.15f + 0.16f * (float)n, Rc = 0.95f;
		haz(cmd, glm::vec4(bm, Rm), glm::vec4(0.0f), glm::vec4(1.0f, 0.13f * (float)v.mt, pulso * fuerza, 0.0f), glm::vec4(oro, 0.05f));
		haz(cmd, glm::vec4(bm, Rm * 1.25f), glm::vec4(0.0f), glm::vec4(2.0f, 0.3f, 0.24f * fuerza, 0.0f), glm::vec4(oro, 0.0f));
		haz(cmd, glm::vec4(bc, Rc), glm::vec4(0.0f), glm::vec4(1.0f, 0.13f * (float)v.mt + 0.5f, pulso * fuerza, 0.0f), glm::vec4(oro, 0.05f));
		haz(cmd, glm::vec4(bc, Rc * 1.25f), glm::vec4(0.0f), glm::vec4(2.0f, 0.3f, 0.22f * fuerza, 0.0f), glm::vec4(oro, 0.0f));
		const float u = t / 1.1f;   // al aparecer: un aro que se abre de cada carta, como el de la carta de campo
		if (u < 1.0f) {
			const float q = (1.0f - u) * (1.0f - u);
			haz(cmd, glm::vec4(bm, 0.9f + 2.2f * u), glm::vec4(0.0f), glm::vec4(1.0f, 0.5f, 1.4f * q, 0.0f), glm::vec4(oro, 0.04f));
			haz(cmd, glm::vec4(bc, 0.8f + 1.8f * u), glm::vec4(0.0f), glm::vec4(1.0f, 0.5f, 1.4f * q, 0.0f), glm::vec4(oro, 0.04f));
		}
	}
}

// la abduccion de la carta de campo Bosque (dibujarCampo, CAMPO_ACTIVANDO) sobre la zona z; t = s desde que entro
static constexpr float OVNI_DURA = 2.5f;
void abduccionOvni(VkCommandBuffer cmd, int z, float t)
{
	const glm::vec3 c = centroZona(z), mesa = c + glm::vec3(0.0f, 0.06f, 0.0f);
	const glm::vec3 verde(0.35f, 1.25f, 0.30f), dorado(1.3f, 1.0f, 0.35f), blanco(0.95f, 1.0f, 0.85f);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.efectos);
	const float ini = 0.35f, dur = 1.6f;   // estallido de hojas de luz que salen de la zona
	if (t > ini && t < ini + dur) {
		const float u = t - ini;
		const uint32_t sem = (uint32_t)z * 17u;
		for (uint32_t k = 0; k < 28; k++) {
			const float a = 6.2832f * azar(k * 3u + 7u + sem);
			const glm::vec3 vel(cosf(a) * (2.0f + 3.0f * azar(k * 5u + sem)), 2.5f + 3.5f * azar(k * 7u + 1u + sem), sinf(a) * (2.0f + 3.0f * azar(k * 5u + sem)));
			glm::vec3 p = mesa + vel * u + glm::vec3(0.0f, -3.0f * u * u, 0.0f);
			p.y = glm::max(p.y, 0.03f);
			const float vida = 1.0f - u / dur;
			bola(cmd, p, 0.08f, k % 3 ? verde : dorado, 0.35f * vida);
			bola(cmd, p, 0.03f, blanco, 1.2f * vida);
			tramo(cmd, p, p - vel * 0.05f, 0.02f, verde, 0.5f * vida);
		}
	}
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.haz);
	if (t < 0.6f) {
		const float u = t / 0.6f;
		haz(cmd, glm::vec4(mesa, 0.9f + 1.1f * u), glm::vec4(0.0f), glm::vec4(1.0f, 0.3f, 1.6f * (1.0f - u) * (1.0f - u), 0.0f), glm::vec4(verde, 0.04f));
	}
	// la columna de luz verde y dorada que baja sobre la zona (la abduccion)
	const float luz = suave(0.0f, 0.3f, t) * (1.0f - suave(1.3f, 2.0f, t));
	if (luz > 0.0f) {
		const glm::vec3 cielo = c + glm::vec3(0.0f, 10.0f, 0.0f);
		const float frente = suave(0.0f, 0.4f, t);
		haz(cmd, glm::vec4(cielo, 0.15f), glm::vec4(mesa, 1.15f), glm::vec4(0.0f, 0.61f, 0.65f * luz, frente), glm::vec4(verde, 0.0f));
		haz(cmd, glm::vec4(cielo, 0.08f), glm::vec4(mesa, 0.5f), glm::vec4(0.0f, 0.23f, 0.5f * luz, frente), glm::vec4(dorado, 0.0f));
	}
	const float sello = suave(0.15f, 0.5f, t) * (1.0f - suave(1.7f, 2.5f, t));
	if (sello > 0.0f) {
		haz(cmd, glm::vec4(mesa, 1.7f), glm::vec4(0.0f), glm::vec4(2.0f, 0.4f, 1.2f * sello, 0.0f), glm::vec4(glm::mix(verde, dorado, 0.3f), 0.0f));
	}
}
