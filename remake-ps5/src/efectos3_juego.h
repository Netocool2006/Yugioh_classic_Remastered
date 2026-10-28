	// Rama classic-ps5-efectos3 (se incluye dentro de la clase del juego, yugireload.cpp). Pedidos del usuario en la PS5:
	//  - «una carta de monstruo se autodestruyo por su efecto: no hizo ruido ni hubo destruccion, solo se fue al cementerio»:
	//    toda destruccion por efecto (ia::Suceso::destruida) se rompe en su zona como MD (CardRoot.ReqBrokenEffect con el
	//    one-shot "CardBreak" del efecto: fxp_cardbrk_bff_001 = Fx::MdDestruir, y SE_CARDBREAK_01 = sfx_destruir) y la carta
	//    viaja al cementerio (CardMoveField2GraveNormal, md_ritmo_juego.h ritmoSalida).
	//  - «cuando entra una carta de campo no hay un sonido FX que de la imponencia del caso, y la musica sigue igual»: la
	//    carta de campo suena (sfx_campo_md) y mientras haya una boca arriba suena musica_campo (duelo_juego.h, musica()).
	std::array<float, interfaz::NUM_ZONAS> tRotaEfecto{};   // s desde que la carta de esa zona se rompio por un efecto
	bool rotasEfectoListas{ false };
	static constexpr float T_ROTA_EFECTO = 1.35f;   // (el flipbook horneado de fxp_cardbrk_bff_001)

	void romperPorEfectoMD(const ia::Suceso& s, int z)
	{
		if (!rotasEfectoListas) tRotaEfecto.fill(99.0f), rotasEfectoListas = true;
		if (z < 0 || z >= (int)interfaz::NUM_ZONAS) return;
		tRotaEfecto[(size_t)z] = 0.0f;
		sonido.sfx("sfx_destruir", 1.0f, control::ZONAS_MESA[z].x / 12.0f);
		fprintf(stderr, "[efectos3] '%s' destruida por un efecto en %s: se rompe (MD CardBreak) y viaja al cementerio\n", s.carta.c_str(), control::ZONAS_MESA[z].id);
	}
	void avanzarRoturasEfectoMD(float dt)
	{
		if (!rotasEfectoListas) tRotaEfecto.fill(99.0f), rotasEfectoListas = true;
		for (float& t : tRotaEfecto) t += dt;
	}
	void pedirRoturasEfectoMD()
	{
		if (!rotasEfectoListas) return;
		const glm::vec3 arriba(0.0f, 1.0f, 0.0f);
		for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++)
			if (tRotaEfecto[(size_t)z] < T_ROTA_EFECTO) flipbooks.pedir(fx::Fx::MdDestruir, centroZona(z) + arriba * 0.3f, 3.2f, tRotaEfecto[(size_t)z], glm::vec4(1.0f));
	}

	// la carta de campo entra: su sonido de MD (el de aterrizaje alto, SE_LAND_ADVANCE_HIGH: MD no tiene uno propio de campo)
	void sonidoCampoMD(int lado)
	{
		if (sonido.hay("sfx_campo_md")) sonido.sfx("sfx_campo_md", 1.0f, lado ? 0.0f : 0.0f);
	}
	// hay una carta de campo boca arriba (de cualquier lado): su musica
	bool campoActivoMD() const
	{
		for (int lado = 0; lado < 2; lado++) {
			const ia::MagiaTrampa& c = dueloIA.l[(size_t)lado].campo;
			if (!c.vacia() && !c.bocaAbajo) return true;
		}
		return false;
	}
