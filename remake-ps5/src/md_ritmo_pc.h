	// Rama classic-md-ritmo: capturas en SECUENCIA del ritmo de Master Duel en la vista previa del PC (solo
	// PS5_HOST_REFERENCE, con YUGI_PRUEBA_RITMO=<carpeta> y YUGI_RITMO_ESCENA=coloca|invoca|swords|luz|cadena|combate|
	// combate_alto|combate_figura; rama classic-md-combate: el rival ataca con Giant Soldier of Stone (1300, viaje Low, sin
	// figuras) o Blue-Eyes (3000, High) a la Mystical Elf del jugador, o con Vorse Raider al Celtic Guardian (los dos con
	// figura 3D); la destruida se rompe y viaja al cementerio). Se incluye
	// dentro de la clase del juego y corre en cada cuadro (como front_total_pc.h):
	//  1. a los 2 s empieza un duelo contra Tea sin sorteo (empieza el jugador);
	//  2. con el jugador libre en su fase principal 1, pasa el duelo a la fase principal 1 del rival (sin su IA: la escena
	//     juega por el) con su mano armada;
	//  3. el rival juega la escena: coloca un monstruo boca abajo / invoca uno / activa Swords of Revealing Light /
	//     Light of Intervention / activa Swords y el jugador encadena su Jar of Greed colocado (cadena de 2);
	//  4. saca una foto cada YUGI_RITMO_CADA s (0.2) durante la escena (<carpeta>/<escena>_NN.ppm) y cierra el juego.
	struct PruebaRitmoPC { int etapa{ -1 }; float t{ 0.0f }, tFoto{ 0.0f }; std::string dir, escena; int fotos{ 0 }; bool respondio{ false }; } prt;
	void pruebaRitmoPC()
	{
		if (prt.etapa == -1) {
			const char* v = getenv("YUGI_PRUEBA_RITMO");
			prt.etapa = v ? 0 : -2;
			if (v) prt.dir = std::string(v) + "/";
			const char* e = getenv("YUGI_RITMO_ESCENA");
			prt.escena = e && *e ? e : "coloca";
		}
		if (prt.etapa < 0) return;
		prt.t += frameTimer;
		ia::Duelo& d = dueloIA;
		switch (prt.etapa) {
		case 0: {
			if (prt.t < 2.0f || !mandoListo) return;
			irAElegirRival();
			ctl.vista.modo = VD::HISTORIA;
			for (int s = 0; s < personajes::NUM_SAGAS; s++) {
				const std::vector<int> fila = historia.deSaga(s);
				for (int i = 0; i < (int)fila.size(); i++)
					if (historia.lista[(size_t)fila[(size_t)i]].id == "tea") ctl.vista.saga = s, ctl.vista.idx = i;
			}
			elegirRival(true);
			empezarDueloIA();
			ctl.vista.sorteo = false;
			arrancarDueloIA(0);
			prt.etapa = 1, prt.t = 0.0f;
			return;
		}
		case 1: {
			if (prt.t < 1.0f || d.activo != 0 || d.fase != ia::F_PRINCIPAL1 || ctl.bloqueado || d.ef.preg.tipo != ia::P_NADA) {
				if (prt.t > 60.0f) quit = true;
				return;
			}
			// el turno 2 del rival, su fase principal 1 (sin su IA: la escena juega por el)
			turnoIA = ia::Turno{};
			d.turno = 2;
			d.activo = 1;
			d.fase = ia::F_PRINCIPAL1;
			d.l[1].invocoNormal = false;
			const std::string& e = prt.escena;
			d.l[1].mano = e == "coloca" ? std::vector<std::string>{ "giant_soldier_of_stone", "dark_hole", "trap_hole", "celtic_guardian" }
			              : e == "invoca" ? std::vector<std::string>{ "vorse_raider", "dark_hole", "trap_hole", "giant_soldier_of_stone" }
			              : e == "luz"    ? std::vector<std::string>{ "light_of_intervention", "vorse_raider", "trap_hole", "dark_hole" }
			              : e == "agujero" ? std::vector<std::string>{ "dark_hole", "vorse_raider", "trap_hole", "celtic_guardian" }
			              : e == "raigeki" ? std::vector<std::string>{ "raigeki", "vorse_raider", "trap_hole", "celtic_guardian" }
			              : e == "renacer" ? std::vector<std::string>{ "monster_reborn", "trap_hole", "celtic_guardian" }
			              : (e == "equipo" || e == "equipo_cursor") ? std::vector<std::string>{ "axe_of_despair", "trap_hole", "celtic_guardian" }
			              : e == "campo"   ? std::vector<std::string>{ "yami", "trap_hole", "celtic_guardian" }   // (rama classic-ps5-efectos3)
			                              : std::vector<std::string>{ "swords_of_revealing_light", "vorse_raider", "trap_hole", "dark_hole" };
			// (rama classic-ps5-efectos: escenas de efectos de cartas; "swords_j": la activa el JUGADOR en su turno)
			const bool eligeJ = e.rfind("elegir", 0) == 0;   // (rama classic-ps5-elegir-cartas: el JUGADOR activa y elige entre cartas con el mando)
			const bool delJugador = e == "swords_j" || eligeJ;
			// el jugador: un monstruo boca abajo (Swords lo da vuelta) y, para la cadena, Jar of Greed colocado de antes
			for (auto& m : d.l[0].m) m = ia::Monstruo{};
			for (auto& m : d.l[1].m) m = ia::Monstruo{};
			for (auto& k : d.l[0].mt) k = ia::MagiaTrampa{};
			ia::Monstruo m;
			m.id = "mystical_elf";
			m.bocaAbajo = m.defensa = true;
			m.uid = 9001;
			m.atkEf = d.atk(m.id);
			m.defEf = d.def(m.id);
			d.l[0].m[2] = m;
			if (e == "raigeki" || e == "agujero") {   // (rama classic-ps5-efectos: un monstruo boca arriba mas del jugador)
				ia::Monstruo b;
				b.id = "celtic_guardian";
				b.uid = 9011;
				d.l[0].m[1] = b;
			}
			if (e.rfind("combate", 0) == 0 || e == "espejo" || e == "cilindro") {   // (rama classic-md-combate: los dos boca arriba en ataque, el rival en su batalla)
				d.l[0].m[2].id = e == "combate_figura" ? "celtic_guardian" : "mystical_elf";
				d.l[0].m[2].bocaAbajo = d.l[0].m[2].defensa = false;
				ia::Monstruo a;
				a.id = e == "combate_alto" ? "blue_eyes" : e == "combate_figura" ? "vorse_raider" : "giant_soldier_of_stone";
				a.uid = 9004;
				d.l[1].m[2] = a;
				d.fase = ia::F_BATALLA;
			}
			for (auto& k : d.l[1].mt) k = ia::MagiaTrampa{};
			if (e == "agujero" || e == "equipo" || e == "equipo_cursor" || e == "swords_j") {   // (rama classic-ps5-efectos: un monstruo del rival)
				ia::Monstruo r;
				r.id = "vorse_raider";
				r.uid = 9012;
				if (e == "swords_j") r.id = "giant_soldier_of_stone", r.bocaAbajo = r.defensa = true;
				d.l[1].m[2] = r;
			}
			if (e == "voltea") {   // (rama classic-ps5-equipo-ovni: una figura del rival boca abajo en defensa que se voltea)
				ia::Monstruo r;
				r.id = "celtic_guardian";
				r.uid = 9005;
				r.bocaAbajo = r.defensa = true;
				d.l[1].m[2] = r;
			}
			if (e == "renacer") d.l[1].cementerio = { "vorse_raider" };
			if (e == "espejo" || e == "cilindro") {   // (el jugador tiene Mirror Force / Magic Cylinder colocada y la activa cuando el rival ataca)
				d.l[0].mt[1] = ia::MagiaTrampa{ e == "cilindro" ? "magic_cylinder" : "mirror_force", true };
				d.l[0].mt[1].uid = 9013;
				d.l[0].mt[1].turnoColocada = 0;
			}
			if (delJugador) {   // (el turno es del jugador: Swords en su mano)
				d.l[0].mano = { "swords_of_revealing_light", "trap_hole" };
				d.l[1].mano = { "trap_hole", "dark_hole" };
			}
			// rama classic-ps5-elegir-cartas: buscar en el mazo (Reinforcement of the Army: un Guerrero de nivel 4 o menos),
			// elegir del cementerio (Monster Reborn: los de los dos lados) y elegir de la mano (Graceful Charity: descartar 2)
			if (e == "elegir_mazo") {
				d.l[0].mano = { "reinforcement_of_the_army" };
				d.l[0].mazo.insert(d.l[0].mazo.begin(), { "celtic_guardian", "axe_raider", "marauding_captain", "command_knight", "don_zaloog", "armed_samurai_ben_kei" });
				d.l[0].mazo.insert(d.l[0].mazo.end(), { "celtic_guardian", "axe_raider", "marauding_captain", "command_knight", "don_zaloog" });
			}
			if (e == "elegir_cem") {
				d.l[0].mano = { "monster_reborn" };
				d.l[0].cementerio = { "celtic_guardian", "battle_ox", "vorse_raider", "mystical_elf", "giant_soldier_of_stone", "summoned_skull", "blue_eyes", "dark_magician" };
				d.l[1].cementerio = { "baby_dragon", "vorse_raider" };
			}
			if (e == "elegir_mano") d.l[0].mano = { "graceful_charity", "celtic_guardian", "battle_ox", "mystical_elf", "trap_hole" };
			if (e == "luz") {   // (Light of Intervention, una trampa continua: colocada desde antes y se activa en la zona)
				d.l[1].mano.erase(d.l[1].mano.begin());
				d.l[1].mt[2] = ia::MagiaTrampa{ "light_of_intervention", true };
				d.l[1].mt[2].uid = 9003;
				d.l[1].mt[2].turnoColocada = 0;
			}
			// rama classic-ps5-control2: "pregunta": el jugador tiene Mystical Space Typhoon colocada; el rival activa Swords y
			// el duelo le pregunta al jugador (el guion contesta con el mando: Activar, elegir el objetivo, el cuadrado en
			// medio). "fases": el turno del jugador y el cuadrado abre el selector de fases
			if (e == "pregunta") {
				d.l[0].mt[1] = ia::MagiaTrampa{ "mystical_space_typhoon", true };
				d.l[0].mt[1].uid = 9021;
				d.l[0].mt[1].turnoColocada = 0;
				d.l[1].mt[0] = ia::MagiaTrampa{ "trap_hole", true };
				d.l[1].mt[0].uid = 9022;
				d.l[1].mt[0].turnoColocada = 0;
			}
			if (e == "cadena") {
				d.l[0].mt[1] = ia::MagiaTrampa{ "jar_of_greed", true };
				d.l[0].mt[1].uid = 9002;
				d.l[0].mt[1].turnoColocada = 0;
			}
			d.recalcular();
			sucesosVistos = d.sucesos.size();
			ocultas[0] = ocultas[1] = 0;
			ritmo::estado().limpiar();
			ritmoTomarFoto();
			sincronizarMesaDuelo();
			if (delJugador || e == "fases") {   // (vuelve al turno del jugador, su fase principal 1)
				d.turno = 3;
				d.activo = 0;
				d.l[0].invocoNormal = false;
			}
			ctl.vista.turnoRival = !delJugador && e != "fases";
			prt.etapa = 2, prt.t = 0.0f;
			if (eligeJ) {   // (el jugador activa la carta con el mando: X en la carta, Activar, la zona, y elige con la cruz y X)
				ctl.foco = control::Foco{ control::F_MANO_J, 0 };
				guion.leer(e == "elegir_cem" ? "1.0,x,1.0,x,1.5,x,5.0,der,1.0,der,1.0,der,1.0,x,4.0,x,3.0"
				           : e == "elegir_mazo" ? "1.0,x,1.0,x,1.5,x,6.0,der,1.0,der,1.0,x,4.0"
				                                : "1.0,x,1.0,x,1.5,x,7.0,x,2.0,der,1.0,x,3.0", 0.35f, 1.0f);
				prt.etapa = 3, prt.tFoto = 99.0f;
			}
			if (e == "fases") {   // (rama classic-ps5-control2: sin jugada del rival; el cuadrado abre y cierra el selector)
				guion.leer("c,2.0,der,1.2,c,1.5,c,1.2,o", 0.35f, 1.0f);
				prt.etapa = 3, prt.tFoto = 99.0f;
			}
			fprintf(stderr, "[prueba-ritmo] escena %s: turno del rival preparado\n", e.c_str());
			return;
		}
		case 2: {
			if (prt.t < 1.0f) return;
			const std::string& e = prt.escena;
			ia::Jugada j;
			if (e == "swords_j") {
				j = ia::jugadaActivar({ 0, ia::EN_MANO, 0 });
			} else if (e.rfind("combate", 0) == 0 || e == "espejo" || e == "cilindro") {   // (rama classic-md-combate)
				j.tipo = ia::J_ATACAR;
				j.zona = 2;
				j.objetivo = 2;
			} else if (e == "coloca" || e == "invoca") {
				j.tipo = e == "coloca" ? ia::J_COLOCAR : ia::J_INVOCAR;
				j.mano = 0;
				j.zona = 2;
			} else if (e == "voltea") {
				j.tipo = ia::J_VOLTEAR;
				j.zona = 2;
			} else if (e == "luz") {
				j = ia::jugadaActivar({ 1, ia::EN_MT, 2 });
			} else {
				j = ia::jugadaActivar({ 1, ia::EN_MANO, 0 });
				// (rama classic-ps5-efectos: las que eligen objetivo al activarse: Monster Reborn el monstruo del cementerio,
				// Axe of Despair el monstruo que se equipa)
				if (e == "renacer") j.objs = { { 1, ia::EN_CEMENTERIO, 0 } };
				if (e == "equipo" || e == "equipo_cursor") j.objs = { { 1, ia::EN_MONSTRUO, 2 } };
			}
			const bool ok = d.aplicar(j);
			fprintf(stderr, "[prueba-ritmo] el rival juega %s: %s\n", e.c_str(), ok ? "ok" : "RECHAZADA");
			if (!ok && j.tipo == ia::J_ACTIVAR)   // (rama classic-ps5-efectos: por que no)
				fprintf(stderr, "[prueba-ritmo] motivo: %s\n", ia::motivoNoActivar(d, e == "swords_j" ? 0 : 1, { e == "swords_j" ? 0 : 1, ia::EN_MANO, 0 }, false).c_str());
			prt.etapa = 3, prt.t = 0.0f, prt.tFoto = 99.0f;
			return;
		}
		case 3: {
			// las preguntas del duelo: el rival pasa; el jugador encadena su Jar of Greed en la escena "cadena" (una vez) y pasa
			// (rama classic-ps5-control2, "pregunta": la del jugador la contesta el guion con el mando, mirando la pantalla)
			if (prt.escena == "pregunta" && ctl.enPregunta && ctl.estado == control::MENU && !guion.activo()) {
				guion.leer("1.8,x,2.5,c,2.5,x,1.0", 0.35f, 0.0f);
				fprintf(stderr, "[prueba-ritmo] pregunta a la vista: el guion contesta (Activar, cuadrado, elegir)\n");
			}
			const bool contestaGuion = (prt.escena == "pregunta" || prt.escena.rfind("elegir", 0) == 0) && d.ef.preg.lado == 0;
			if (d.ef.preg.tipo != ia::P_NADA && sucesosVistos >= d.sucesos.size() && esperaEfecto <= 0.0f && !contestaGuion) {
				ia::Jugada j;
				j.tipo = ia::J_PASAR;
				if (d.ef.preg.lado == 0 && (prt.escena == "cadena" || prt.escena == "espejo" || prt.escena == "cilindro") && !prt.respondio && d.ef.preg.tipo == ia::P_RESPUESTA) {
					j = ia::jugadaActivar({ 0, ia::EN_MT, 1 });
					if (prt.escena == "cilindro") j.objs = { { 1, ia::EN_MONSTRUO, 2 } };   // (Magic Cylinder: el monstruo que ataca)
					prt.respondio = true;
				} else if (d.ef.preg.tipo != ia::P_RESPUESTA) {   // (rama classic-ps5-efectos: elegir objetivo / opcion: la IA)
					j = turnoIA.cerebro.decidirPregunta(d);
				}
				const bool ok = d.aplicar(j);
				fprintf(stderr, "[prueba-ritmo] pregunta al %s: %s %s\n", d.ef.preg.lado == 0 ? "jugador" : "rival", ia::NOMBRE_JUGADA[j.tipo], ok ? "ok" : "rechazada");
				if (ctl.enPregunta) {
					ctl.estado = control::NAVEGAR;
					ctl.enPregunta = ctl.armando = ctl.eleccionDuelo = false;
				}
			}
			// rama classic-ps5-equipo-ovni: con el equipo ya puesto, el cursor sobre la carta de equipo (se marca el monstruo)
			if (prt.escena == "equipo_cursor" && prt.t > 4.5f && !vinculosEquipo.empty()) ctl.foco = control::Foco{ control::F_ZONA, vinculosEquipo[0].mt };
			const char* c = getenv("YUGI_RITMO_CADA");
			const float cada = c ? (float)atof(c) : 0.2f;
			prt.tFoto += frameTimer;
			if (prt.tFoto >= cada) {
				prt.tFoto = 0.0f;
				carpetaCapturas = prt.dir;
				char nombre[64];
				snprintf(nombre, sizeof(nombre), "%s_%02d", prt.escena.c_str(), prt.fotos++);
				sacarFoto(nombre);
			}
			const bool alDia = sucesosVistos >= d.sucesos.size() && !ritmo::estado().ocupado() && esperaEfecto <= 0.0f && d.ef.preg.tipo == ia::P_NADA && !hayAtaque();
			const bool largo = prt.escena == "pregunta" || prt.escena == "fases" || prt.escena.rfind("elegir", 0) == 0;   // (rama classic-ps5-control2: el guion tarda)
			if ((alDia && prt.t > (largo ? 9.0f : prt.escena == "equipo_cursor" ? 8.0f : 3.0f) && (!guion.activo() || guion.terminado())) || prt.t > (largo ? 32.0f : prt.escena == "equipo_cursor" ? 14.0f : 12.0f) || prt.fotos >= 110) {
				fprintf(stderr, "[prueba-ritmo] escena %s terminada: %d fotos\n", prt.escena.c_str(), prt.fotos);
				prt.etapa = 4, prt.t = 0.0f;
			}
			return;
		}
		case 4:
			if (prt.t > 1.0f) quit = true;   // (la ultima foto se escribe en otro hilo)
			return;
		default: return;
		}
	}
