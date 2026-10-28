	// Yugioh Classic Remaster (rama classic-ps5-control2): se incluye DENTRO de la clase Interfaz (interfaz.h).
	// Pedido del usuario (PS5): "otra vez salio 'responde a la pregunta' y no se cual es la pregunta; no sale por ningun
	// lado visible". Cuando el duelo le pide al jugador ELEGIR (una carta de la mesa o de la mano con el recuadro, una
	// zona, sacrificios) la unica senal era el aviso de arriba, que se va a los 2.6 s: despues solo quedaba el recuadro y
	// OPTIONS decia "Primero responde a la pregunta". Ahora, mientras dura la eleccion, la pregunta queda escrita en una
	// placa fija arriba del campo con los botones que valen, encima de los carteles y del corte del rival (el menu de la
	// pregunta tambien se dibuja encima: interfaz.h dibujarMandoEncima).
	bool preguntaFijaVisible() const
	{
		if (!ctl || ctl->vista.pantalla != control::VistaDuelo::DUELO) return false;
		const bool eligiendo = ctl->estado == control::ELEGIR_CARTA || ctl->estado == control::ELEGIR_ZONA || ctl->estado == control::ELEGIR_OBJETIVO ||
		                       ctl->estado == control::ELEGIR_TRIBUTOS;
		return eligiendo && (ctl->enPregunta || ctl->armando || ctl->eleccionDuelo);
	}
	void dibujarPreguntaFija()
	{
		if (!preguntaFijaVisible()) return;
		std::string txt = ctl->tituloTexto;
		if (txt.empty()) txt = ctl->aviso;
		if (txt.empty()) txt = idioma::t("Responde: elige una carta", "Answer: choose a card");
		// (activando una carta: de que carta es la eleccion, "Tifon del Espacio Mistico: Elige el objetivo")
		if (ctl->armando && !ctl->armado.carta.empty()) {
			const int ci = buscarCarta(ctl->armado.carta);
			const std::string nom = ci >= 0 ? cartas[ci].vNombre() : std::string();
			if (!nom.empty() && txt.find(nom) == std::string::npos) txt = nom + ": " + txt;
		}
		const float a = 0.9f + 0.1f * sinf(t * 3.0f);
		const float cx = (CAMPO_X * s + W) * 0.5f;
		float tt = 24.0f * s;
		const float maxW = W - CAMPO_X * s - 120.0f * s;
		while (medir(F_TITULO, txt, tt, 1.0f) > maxW && tt > 15.0f * s) tt -= 1.0f * s;
		const float tw = medir(F_TITULO, txt, tt, 1.0f);
		const std::string sub = idioma::t("PREGUNTA: elige con la cruz y X", "QUESTION: choose with the d-pad and X");
		const float ts = 16.0f * s;
		const float w = glm::max(tw, medir(F_NUMEROS, sub, ts, 1.5f)) + 80.0f * s, h = 76.0f * s;
		const float y = (ctl->vista.turnoRival ? 246.0f : 190.0f) * s;
		usar(setAtlas, false);
		caja(cx - w * 0.5f, y, w, h, 20.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.92f), glm::vec4(glm::vec3(ORO) * a, 1.0f), 18.0f * s,
		     glm::vec4(1.6f * s, 1.1f, 0.2f, 14.0f * s));
		texto(F_NUMEROS, sub, cx, y + 26.0f * s, ts, glm::vec4(glm::vec3(ORO), 1.0f), NADA, glm::vec3(0.0f), 1, 1.5f);
		texto(F_TITULO, txt, cx, y + 58.0f * s, tt, BLANCO, NADA, glm::vec3(0.0f), 1, 1.0f);
	}
