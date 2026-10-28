// rama classic-ps5-ataque (se incluye DENTRO de la clase VulkanExample, y en el juego sin GPU de las pruebas): el
// ATAQUE DIRECTO como en Master Duel (ataque_md.h). Pedido del usuario: "cuando el oponente no tiene monstruos en su
// campo la carta debe atacar directo al oponente, no al tablero vacio". Antes el ataque directo iba a la zona del
// centro del rival (vacia): la carta (o la figura 3D, con su rayo) pegaba al tablero.
//  - prepararAtaqueDirecto (al empezar el ataque, duelo_juego.h animarAtaqueDuelo): el punto de MD
//    (EffectTaskBattleRun.AttackInitStep): abajo al centro de la pantalla si el que recibe es el jugador (la carta del
//    rival vuela hacia la camara), arriba al centro / detras del campo del rival si recibe el rival.
//  - destinoAtaqueMundo(): a donde va el ataque en curso (la carta plana: interfaz.h; la figura 3D y su rayo:
//    figuras.h / ataque_fig_fx.h; el golpe y la camara: yugireload.cpp).
//  - sacudidaAtaqueDirecto(): la camara en el golpe (CameraShakerSetting "AttackDirectNear" / "AttackDirectFar").
// La camara de juego es la de yugireload.cpp (OJO, OBJETIVO con la mira 7 grados arriba, 62 grados horizontales en
// 16:9); se rehace aqui con los mismos numeros para que el juego sin GPU (pruebas) calcule el mismo punto.

	bool ataqueDirecto{ false };
	glm::vec3 puntoDirecto{ 0.0f };
	uint32_t semillaDirecto{ 0 };

	static bool zonaDelRivalMD(int z) { return z >= 0 && z < control::NUM_ZONAS_MESA && control::ZONAS_MESA[z].rival; }
	static glm::vec3 centroZonaMD(int z)
	{
		return glm::vec3(control::ZONAS_MESA[z].x * ataquemd::ESCALA, 0.0f, control::ZONAS_MESA[z].z * ataquemd::ESCALA);
	}

	// la camara de juego (yugireload.cpp updateUniformBuffers, sin el vaiven ni el acercamiento del combate)
	static void camaraJuegoMD(glm::vec3& ojo, glm::vec3& mira, float& fovVertical, float& aspecto)
	{
		ojo = orientacion::ojoJuego();
		mira = glm::vec3(0.0f, 0.0f, -0.6f) * ataquemd::ESCALA;
		const float extra = glm::radians(7.0f);   // (MIRA_EXTRA_GRADOS)
		const glm::vec3 d = mira - ojo;
		const float h = glm::length(glm::vec2(d.x, d.z));
		const float p = atan2f(d.y, h) + extra;
		mira = ojo + glm::vec3(d.x / h, 0.0f, d.z / h) * (glm::length(d) * cosf(p)) + glm::vec3(0.0f, glm::length(d) * sinf(p), 0.0f);
		aspecto = 16.0f / 9.0f;
		fovVertical = 2.0f * atanf(tanf(glm::radians(62.0f) * 0.5f) / aspecto);   // (FOV_HORIZONTAL)
	}

	// EffectTaskBattleRun.AttackInitStep: si es directo, el objetivo del viaje es el punto de MD del que recibe
	void prepararAtaqueDirecto(int atacante, bool directo)
	{
		ataqueDirecto = directo && atacante >= 0;
		if (!ataqueDirecto) return;
		glm::vec3 ojo, mira;
		float fov, aspecto;
		camaraJuegoMD(ojo, mira, fov, aspecto);
		const bool alJugador = zonaDelRivalMD(atacante);   // ataca el rival: recibe el jugador (MD "Myself": Near)
		puntoDirecto = ataquemd::puntoDirecto(alJugador, ojo, mira, fov, aspecto);
		semillaDirecto = (uint32_t)ataquesVistos * 2654435761u + (uint32_t)atacante;
	}

	// a donde va el ataque en curso: la zona del objetivo o el punto del ataque directo
	glm::vec3 destinoAtaqueMundo() const
	{
		if (ataqueDirecto) return puntoDirecto;
		return zonaObjetivo >= 0 ? centroZonaMD(zonaObjetivo) : glm::vec3(0.0f);
	}

	// MainCameraOrganizer.Shake("AttackDirectNear"/"AttackDirectFar") en el golpe: UpdateGameCam suma shakeOffset a la
	// POSICION de la camara (ejes del mundo; la rotacion no cambia: se corren ojo y mira juntos); desdeGolpe: s desde el golpe
	void sacudidaAtaqueDirecto(glm::vec3& ojo, glm::vec3& mira, float desdeGolpe) const
	{
		if (!ataqueDirecto) return;
		const glm::vec3 m = ataquemd::sacudidaDirecto(desdeGolpe, semillaDirecto, ataquemd::escalaSacudidaMD());
		ojo += m;
		mira += m;
	}
