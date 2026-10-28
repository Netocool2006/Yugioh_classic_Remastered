	// Rama classic-front-total: prueba de PIXELES de la mesa en la vista previa del PC (solo PS5_HOST_REFERENCE, con
	// YUGI_PRUEBA_FT=<carpeta>). Se incluye dentro de la clase del juego (yugireload.cpp) y corre en cada cuadro:
	//  1. a los 2 s elige el rival (YUGI_FT_RIVAL, por defecto Tea: su fondo de la preparatoria con cerezos), el tapete
	//     (YUGI_FT_TAPETE = clave de cosmeticos.h) y la camara 0 (YUGI_FT_CAMARA), y empieza el duelo sin sorteo (empieza el
	//     jugador);
	//  2. en tu fase principal 1 arma la mesa (YUGI_FT_CARTAS = invocar,defensa propia,ataque rival,defensa rival): en tu
	//     mano la carta a invocar; en la mesa un monstruo propio boca arriba en defensa y dos del rival boca arriba (ataque
	//     y defensa);
	//  3. la invoca CON EL MANDO (el guion del PC: izquierda hasta la primera de la mano, X, X Invocar, X en la zona);
	//  4. saca dos fotos (<carpeta>/ft_1.ppm a los 4 s, ft_2.ppm a los 8 s: lo que queda despues de la animacion) y escribe
	//     <carpeta>/zonas.txt con el cuadrilatero de cada zona de monstruo en pixeles (y lo que el motor tiene en ella),
	//     para que pixeles_ft.py mire si se ve la cara de la carta o la casilla vacia; y cierra el juego.
	struct PruebaFTpc { int etapa{ -1 }; float t{ 0.0f }; std::string dir; int fotos{ 0 }, cuadros{ 0 }, zonaB{ -1 }; bool figuras{ false }, vioFigura{ false }, vioBocaAbajo{ false }; } pft;
	void pruebaFrontTotalPC()
	{
		if (pft.etapa == -1) {
			const char* v = getenv("YUGI_PRUEBA_FT");
			pft.etapa = v ? 0 : -2;
			if (v) pft.dir = std::string(v) + "/";
		}
		if (pft.etapa < 0) return;
		pft.t += frameTimer;
		pft.cuadros++;   // (el numero de cuadro: el video del PC, PS5_RECORD, guarda uno cada 15)
		auto env = [](const char* n, const char* def) { const char* v = getenv(n); return std::string(v && *v ? v : def); };
		switch (pft.etapa) {
		case 0: {
			if (pft.t < 2.0f || !mandoListo) return;
			// el tapete y la camara (Personalizacion), el rival del modo historia
			const std::string tapete = env("YUGI_FT_TAPETE", "auto");
			cosmeticos::Estado& ce = cosmeticos::estado();
			for (int k = 0; k < cosmeticos::NUM_TAPETES; k++)
				if (tapete == cosmeticos::TAPETES[k].clave) ce.tapete = k, ce.tapeteLibre[k] = true;
			modoCamara = atoi(env("YUGI_FT_CAMARA", "0").c_str());
			const std::string rival = env("YUGI_FT_RIVAL", "tea");
			irAElegirRival();
			ctl.vista.modo = VD::HISTORIA;
			for (int s = 0; s < personajes::NUM_SAGAS; s++) {
				const std::vector<int> fila = historia.deSaga(s);
				for (int i = 0; i < (int)fila.size(); i++)
					if (historia.lista[(size_t)fila[(size_t)i]].id == rival) ctl.vista.saga = s, ctl.vista.idx = i;
			}
			elegirRival(true);
			empezarDueloIA();
			ctl.vista.sorteo = false;
			arrancarDueloIA(0);
			fprintf(stderr, "[prueba-ft] rival %s, tapete %s, camara %d\n", ctl.vista.personaje.c_str(), tapete.c_str(), modoCamara);
			pft.etapa = 1, pft.t = 0.0f;
			return;
		}
		case 1: {
			if (pft.t < 1.0f || dueloIA.activo != 0 || dueloIA.fase != ia::F_PRINCIPAL1 || ctl.bloqueado || ctl.estado != control::NAVEGAR || dueloIA.ef.preg.tipo != ia::P_NADA) {
				if (pft.t > 60.0f) {
					fprintf(stderr, "[prueba-ft] el jugador no quedo libre en su principal 1 en 60 s\n");
					quit = true;
				}
				return;
			}
			std::vector<std::string> c = ia::partir(env("YUGI_FT_CARTAS", "battle_ox,mystical_elf,giant_soldier_of_stone,vorse_raider"), ',');
			while (c.size() < 4) c.push_back("battle_ox");
			ia::Duelo& d = dueloIA;
			for (ia::Lado& x : d.l) {
				for (ia::Monstruo& m : x.m) m = ia::Monstruo{};
				for (ia::MagiaTrampa& m : x.mt) m = ia::MagiaTrampa{};
			}
			d.l[0].mano = { c[0], "mystical_elf", "celtic_guardian" };
			auto poner = [&](int lado, const std::string& id, bool def, int zona) {
				const int z = d.invocarEspecial(lado, id, def, -1, false);   // (el 4.o es el duenno, no la zona)
				(void)zona;
				if (z >= 0) d.l[(size_t)lado].m[(size_t)z].llegoEsteTurno = false, d.l[(size_t)lado].m[(size_t)z].especial = false;
			};
			poner(0, c[1], true, 4);
			poner(1, c[2], false, 1);
			pft.figuras = env("YUGI_FT_MODO", "") == "figuras";
			{   // (figuras: la defensa del rival boca abajo: sin figura hasta que se voltee)
				const int z = d.invocarEspecial(1, c[3], true, -1, pft.figuras);
				if (z >= 0) d.l[1].m[(size_t)z].llegoEsteTurno = false, d.l[1].m[(size_t)z].especial = false;
			}
			d.ef.disparos.clear();
			d.ef.ventanaTras = ia::Ventana{};
			d.ef.preg = ia::Pregunta{};
			d.recalcular();
			sucesosVistos = d.sucesos.size();
			ctl.preparar({}, {}, {});
			sincronizarMesaDuelo();
			ctl.estado = control::NAVEGAR;
			ctl.foco = control::Foco{ control::F_MANO_J, 0 };   // (la carta a invocar; despues, solo botones)
			guion.leer("x,0.6,x,0.6,x", 0.3f, 0.5f);
			guionTerminado = false;
			fprintf(stderr, "[prueba-ft] mesa armada: invocar %s; defensa propia %s; rival %s (ataque) y %s (defensa)\n", c[0].c_str(), c[1].c_str(), c[2].c_str(), c[3].c_str());
			pft.etapa = 2, pft.t = 0.0f;
			return;
		}
		case 2: {
			if (!guion.terminado()) {
				pft.t = 0.0f;
				return;
			}
			const float cuando = pft.fotos == 0 ? 4.0f : 8.0f;
			if (pft.t < cuando) return;
			carpetaCapturas = pft.dir;
			fotoReposo = true;
			char nombre[32];
			snprintf(nombre, sizeof(nombre), "ft_%d", ++pft.fotos);
			sacarFoto(nombre);
			// las zonas de monstruo en pixeles (como interfaz::cartaZona: centro de la zona, carta de 1 x 1.45 x ESCALA)
			const glm::mat4 vp = uniformData.proyeccion * uniformData.vista;
			FILE* f = fopen((pft.dir + nombre + "_zonas.txt").c_str(), "w");
			if (f) fprintf(f, "#cuadro|%d|%u|%u\n", pft.cuadros, width, height);
			for (int z = 0; f && z < control::NUM_ZONAS_MESA; z++) {
				if (!control::esTipo(z, "monstruo")) continue;
				const control::Celda& k = ctl.zonas[(size_t)z];
				const glm::vec3 c(control::ZONAS_MESA[z].x * control::ESCALA_MESA, 0.045f, control::ZONAS_MESA[z].z * control::ESCALA_MESA);
				const bool girada = k.defensa;
				const float a = 0.5f * (girada ? interfaz::CARTA_ALTO : interfaz::CARTA_ANCHO), l = 0.5f * (girada ? interfaz::CARTA_ANCHO : interfaz::CARTA_ALTO);
				fprintf(f, "%s|%s|%d|%d", control::ZONAS_MESA[z].id, k.carta.empty() ? "-" : k.carta.c_str(), (int)k.bocaAbajo, (int)k.defensa);
				const float esq[4][2] = { { -a, -l }, { a, -l }, { a, l }, { -a, l } };
				for (const auto& e : esq) {
					const glm::vec4 p = vp * glm::vec4(c + glm::vec3(e[0], 0.0f, e[1]), 1.0f);
					fprintf(f, "|%.1f,%.1f", (p.x / p.w * 0.5f + 0.5f) * (float)width, (p.y / p.w * 0.5f + 0.5f) * (float)height);
				}
				fprintf(f, "|fig%d|con%d\n", (int)tieneFigura(z), figuraDe(k.carta) >= 0 ? 1 : 0);   // (la figura 3D: se ve / la carta tiene)
			}
			if (f) fclose(f);
			fprintf(stderr, "[prueba-ft] foto %s (%ux%u)\n", nombre, width, height);
			pft.t = 0.0f;
			if (pft.fotos >= 2) pft.etapa = pft.figuras ? 4 : 3;
			return;
		}
		case 4: {
			// (figuras) la batalla: la carta invocada ataca a la boca abajo del rival: se voltea (con su figura) y muere (sin)
			if (pft.t < 1.0f) return;
			ia::Duelo& d = dueloIA;
			d.turno = 3;
			d.fase = ia::F_BATALLA;
			// (el ataque va a la boca abajo del rival: con las flechas, si el foco del objetivo cae en la otra)
			std::vector<std::string> c = ia::partir(env("YUGI_FT_CARTAS", "battle_ox,mystical_elf,giant_soldier_of_stone,vorse_raider"), ',');
			while (c.size() < 4) c.push_back("battle_ox");
			for (ia::Monstruo& m : d.l[0].m) m.llegoEsteTurno = false, m.ataco = false;
			d.recalcular();
			sucesosVistos = d.sucesos.size();
			ctl.preparar({}, {}, {});
			sincronizarMesaDuelo();
			int za = -1;
			for (int i = 0; i < ia::ZONAS; i++) {
				if (!d.l[0].m[(size_t)i].vacia() && !d.l[0].m[(size_t)i].defensa) za = zonaMesaIA(0, i);
				if (d.l[1].m[(size_t)i].id == c[3]) pft.zonaB = zonaMesaIA(1, i);
			}
			if (za >= 0) ctl.foco = control::Foco{ control::F_ZONA, za };
			int cerca = -1;   // (como control.h enfocarValida: el objetivo mas cercano en x al atacante)
			for (int z = 0; z < control::NUM_ZONAS_MESA && za >= 0; z++)
				if (control::esTipo(z, "monstruo") && control::ZONAS_MESA[z].rival && !ctl.zonas[(size_t)z].vacia() &&
				    (cerca < 0 || fabsf(control::ZONAS_MESA[z].x - control::ZONAS_MESA[za].x) < fabsf(control::ZONAS_MESA[cerca].x - control::ZONAS_MESA[za].x)))
					cerca = z;
			std::string g = "x,0.6,x,0.6,";
			if (cerca >= 0 && pft.zonaB >= 0 && cerca != pft.zonaB) g += control::ZONAS_MESA[pft.zonaB].x > control::ZONAS_MESA[cerca].x ? "der,0.4," : "izq,0.4,";
			g += "x";
			ctl.estado = control::NAVEGAR;
			guion.leer(g.c_str(), 0.3f, 0.5f);
			guionTerminado = false;
			pft.vioBocaAbajo = pft.zonaB >= 0 && ctl.zonas[(size_t)pft.zonaB].bocaAbajo && tieneFigura(pft.zonaB);
			fprintf(stderr, "[prueba-ft] figuras: ataque a la boca abajo (%s)\n", pft.zonaB >= 0 ? control::ZONAS_MESA[pft.zonaB].id : "?");
			pft.etapa = 5, pft.t = 0.0f;
			return;
		}
		case 5: {
			if (pft.zonaB >= 0 && !ctl.zonas[(size_t)pft.zonaB].vacia() && !ctl.zonas[(size_t)pft.zonaB].bocaAbajo && tieneFigura(pft.zonaB)) pft.vioFigura = true;
			if (pft.zonaB >= 0 && ctl.zonas[(size_t)pft.zonaB].bocaAbajo && tieneFigura(pft.zonaB)) pft.vioBocaAbajo = true;
			if (!guion.terminado() || pft.t < 7.0f) return;
			FILE* f = fopen((pft.dir + "figuras.txt").c_str(), "w");
			if (f) {
				const bool muerta = pft.zonaB >= 0 && ctl.zonas[(size_t)pft.zonaB].vacia() && !tieneFigura(pft.zonaB);
				fprintf(f, "VOLTEADA-CON-FIGURA|%d|la carta boca abajo atacada se volteo y mostro su figura\n", (int)pft.vioFigura);
				fprintf(f, "BOCA-ABAJO-SIN-FIGURA|%d|ninguna carta boca abajo mostro figura\n", (int)!pft.vioBocaAbajo);
				fprintf(f, "MUERTA-SIN-FIGURA|%d|la destruida dejo la zona vacia y sin figura (cementerio rival %d)\n", (int)muerta, (int)dueloIA.l[1].cementerio.size());
				fclose(f);
			}
			fprintf(stderr, "[prueba-ft] figuras: volteada con figura %d, boca abajo con figura %d\n", (int)pft.vioFigura, (int)pft.vioBocaAbajo);
			{   // (el estado del duelo al final de la batalla, para revisar)
				std::string t;
				for (size_t k = dueloIA.sucesos.size() > 14 ? dueloIA.sucesos.size() - 14 : 0; k < dueloIA.sucesos.size(); k++)
					t += std::string(ia::NOMBRE_SUCESO[dueloIA.sucesos[k].tipo]) + ":" + dueloIA.sucesos[k].carta + ":" + std::to_string(dueloIA.sucesos[k].lado) + " ";
				fprintf(stderr, "[prueba-ft] sucesos: %s\n[prueba-ft] cementerios %d / %d, LP %d / %d, pantalla %d\n", t.c_str(), (int)dueloIA.l[0].cementerio.size(),
				        (int)dueloIA.l[1].cementerio.size(), dueloIA.l[0].lp, dueloIA.l[1].lp, ctl.vista.pantalla);
			}
			pft.etapa = 3, pft.t = 0.0f;
			return;
		}
		case 3:
			if (pft.t > 1.5f) {
				fprintf(stderr, "[prueba-ft] fin\n");
				quit = true;
			}
			return;
		default: return;
		}
	}
