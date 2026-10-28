	// Yugioh Classic Remaster (rama classic-md-entradas): se incluye DENTRO de la clase del juego (desde
	// entrada_clave_juego.h). Las entradas de fusion y ritual de Master Duel (entrada_md.h: tiempos y estado;
	// entrada_md_ui.h: el dibujo). Ganchos:
	//  - duelo_juego.h, presentarSucesos, S_INVOCA: si entradaMDAntes(...) arranco la secuencia, el suceso se presenta
	//    DESPUES (el duelo espera esperaEfecto = hasta el StartCard; con carta clave, hasta el StrongSummon).
	//  - avanzarEntradaClave (entrada_clave_juego.h) llama a avanzarEntradaMD(dt): el reloj y los sonidos.
	//  - empezarEntradaClave marca entrada::Estado::trasMD: la carta clave que sale de una fusion/ritual no repite el vortice
	//    ni el circulo (ni sus sonidos).
	// PC: YUGI_ENTRADA_MD=0 la apaga. YUGI_ENTRADA_MD=lp deja los LP en 800 / 500 (aviso de LP bajos).
	// YUGI_ENTRADA_MD=fusion | ritual | <id de una fusion o un ritual> (capturas) salta el
	// sorteo y, a los 2,5 s del duelo, en la fase principal del jugador, pone en su mano la magia y los materiales y hace
	// la invocacion de verdad (Duelo::aplicar J_FUSION / J_RITUAL). YUGI_ENTRADA_MD_SIN=1: ritual sin materiales (Ritual02).
	bool entradaMDEncendida{ true };
	bool entradaMDLeida{ false };

	// es un monstruo (clase de la coleccion)
	bool esMonstruoMD(const std::string& id) const
	{
		const mazo::Carta* c = coleccion.carta(id);
		return c && (c->clase == "normal" || c->clase == "efecto" || c->clase == "fusion" || c->clase == "ritual");
	}

	// S_INVOCA (antes de presentarlo): si es una invocacion por fusion o ritual, arranca la secuencia de MD y devuelve true
	// (el suceso se vuelve a presentar cuando termine la espera). Fusion: la carta es de fusion y justo antes se fueron sus
	// materiales (Polymerization + materiales al cementerio, o las piezas desterradas de una X-Y-Z). Ritual: la carta es
	// ritual y antes se fue la magia ritual o sus sacrificios. Asi un Monster Reborn de una fusion no la repite.
	bool entradaMDAntes(const ia::Suceso& s, size_t idx)
	{
		if (!entradaMDLeida) {
			entradaMDLeida = true;
#if defined(PS5_HOST_REFERENCE)
			if (const char* v = getenv("YUGI_ENTRADA_MD")) entradaMDEncendida = strcmp(v, "0") != 0;
#endif
		}
		entradamd::Estado& e = entradamd::estado();
		const std::string clave = std::to_string(s.lado) + ":" + s.carta;
		if (e.suceso == idx && e.ultima == clave) {   // (el mismo S_INVOCA que vuelve tras la secuencia: se presenta; la marca se borra
			e.suceso = (size_t)-1;                    // para que una tabla nueva con el mismo indice -revancha- no se la salte)
			return false;
		}
		if (!entradaMDEncendida || s.tipo != ia::S_INVOCA || s.magia || s.zona < 0) return false;
		const mazo::Carta* k = coleccion.carta(s.carta);
		if (!k || (k->clase != "fusion" && k->clase != "ritual")) return false;
		const bool fusion = k->clase == "fusion";
		const auto& su = dueloIA.sucesos;
		std::vector<std::string> mats;
		bool magia = false;
		for (size_t j = std::min(idx, su.size()); j-- > 0;) {
			const ia::Suceso& x = su[j];
			if (x.tipo == ia::S_MODIFICA) continue;
			if (x.lado != s.lado || (x.tipo != ia::S_AL_CEMENTERIO && x.tipo != ia::S_DESTERRADO)) break;
			if (esMonstruoMD(x.carta)) mats.insert(mats.begin(), x.carta);
			else magia = true;   // (Polymerization / la magia ritual / una union)
		}
		if (mats.empty() && (fusion || !magia)) return false;
		e = entradamd::Estado{};
		e.activa = true;
		e.tipo = fusion ? entradamd::FUSION : entradamd::RITUAL;
		e.carta = s.carta;
		e.lado = s.lado;
		e.materiales = mats;
		if (e.materiales.size() > 5) e.materiales.resize(5);   // (MD: SummonFusion00..05, ShowUnitCard hasta 6)
		e.conMateriales = !mats.empty();
#if defined(PS5_HOST_REFERENCE)
		if (const char* sin = getenv("YUGI_ENTRADA_MD_SIN"); sin && atoi(sin) != 0 && !fusion) e.conMateriales = false, e.materiales.clear();
#endif
		const int z = zonaMesaIA(s.lado, s.zona);
		e.clave = entradaClaveEncendida && z >= 0 && esCartaClave(s.carta);   // (S_INVOCA sin magia: zona de monstruo)
		e.saltando = velAnim.saltando > 0.0f;
		e.suceso = idx;
		e.ultima = clave;
		e.tUltima = 0.0f;
		entradamd::preparar(e);
		esperaEfecto = std::max(esperaEfecto, e.espera);
		if (!e.saltando)   // (los sonidos del cuadro 0: SE_SMN_CMN_CARD_01 del ritual)
			for (const entradamd::Sonido& x : entradamd::sonidos(e))
				if (x.t <= 0.0f) sonido.sfx(x.clave, 1.0f, 0.0f);
		fprintf(stderr, "[entrada-md] %s de %s (%s): %d materiales, %s; espera %.2f s, fin %.2f s%s\n", fusion ? "fusion" : "ritual", s.carta.c_str(),
		        s.lado ? "rival" : "jugador", (int)e.materiales.size(), e.clave ? "sigue su entrada de carta clave" : "sin carta clave", e.espera, e.fin,
		        e.saltando ? " (saltando)" : "");
		return true;
	}

	// cada cuadro del duelo (dt con la velocidad y R2), desde avanzarEntradaClave
	void avanzarEntradaMD(float dt)
	{
		entradamd::Estado& e = entradamd::estado();
		e.tUltima += dt;
		if (!e.activa) return;
		const float antes = e.t;
		e.t += dt;
		e.saltando = e.saltando || velAnim.saltando > 0.0f;
		if (!e.saltando)
			for (const entradamd::Sonido& x : entradamd::sonidos(e))
				if (antes < x.t && e.t >= x.t && x.t <= e.espera + 0.1f) sonido.sfx(x.clave, 1.0f, 0.0f);
		if (e.t >= e.fin) {
			e.activa = false;
			e.tUltima = 0.0f;
			fprintf(stderr, "[entrada-md] %s: fin\n", e.carta.c_str());
		}
	}

	// la carta clave que llega justo despues de su fusion/ritual (empezarEntradaClave)
	bool vieneDeEntradaMD(const std::string& carta, int lado) const
	{
		const entradamd::Estado& e = entradamd::estado();
		return e.ultima == std::to_string(lado) + ":" + carta && (e.activa || e.tUltima < 3.0f);
	}

	// fuera del duelo (camaraEntradaClave): el duelo siguiente empieza de cero
	void olvidarEntradaMD()
	{
		entradamd::Estado& e = entradamd::estado();
		if (e.activa || !e.ultima.empty()) e = entradamd::Estado{};
	}

#if defined(PS5_HOST_REFERENCE)
	// PC (capturas): YUGI_ENTRADA_MD=fusion | ritual | <id>. Ver arriba
	float tPruebaMD{ 0.0f };
	bool pruebaMDHecha{ false };
	void pruebaEntradaMD(float dt)
	{
		const char* v = getenv("YUGI_ENTRADA_MD");
		if (!v || !*v || strcmp(v, "0") == 0 || strcmp(v, "1") == 0 || pruebaMDHecha || ctl.vista.pantalla != VD::DUELO) return;
		if (ctl.vista.sorteo) {
			ctl.vista.sorteo = false;
			arrancarDueloIA(0);
			tPruebaMD = 0.0f;
			return;
		}
		tPruebaMD += dt;
		if (tPruebaMD < 2.5f || dueloIA.ganador >= 0 || dueloIA.activo != 0 || !dueloIA.principal() || dueloIA.ocupado() || esperaEfecto > 0.0f ||
		    sucesosVistos < dueloIA.sucesos.size() || !dueloIA.base)
			return;
		pruebaMDHecha = true;
		std::string id = v;
		if (id == "lp") {   // (los LP bajos de DuelLP: 800 y 500)
			dueloIA.danno(0, dueloIA.l[0].lp - 800);
			dueloIA.danno(1, dueloIA.l[1].lp - 500);
			fprintf(stderr, "[entrada-md] prueba: LP bajos (800 / 500)\n");
			return;
		}
		if (id == "fusion") id = "dark_paladin";
		if (id == "ritual") id = "black_luster_soldier";
		ia::Lado& x = dueloIA.l[0];
		ia::Jugada j;
		auto aMano = [&](const std::string& c) {
			x.mano.push_back(c);
			return (int)x.mano.size() - 1;
		};
		auto fu = dueloIA.base->fusiones.find(id);
		if (fu != dueloIA.base->fusiones.end()) {
			j.tipo = ia::J_FUSION;
			j.carta = id;
			x.extra.push_back(id);
			j.mano = aMano("polymerization");
			for (const std::string& m : fu->second) j.manoExtra.push_back(aMano(m));
		} else {
			auto ri = dueloIA.base->rituales.find(id);
			if (ri == dueloIA.base->rituales.end()) {
				fprintf(stderr, "[entrada-md] prueba: '%s' no es una fusion ni un ritual con receta\n", id.c_str());
				return;
			}
			j.tipo = ia::J_RITUAL;
			j.carta = id;
			j.mano = aMano(ri->second);
			j.mano2 = aMano(id);
			int suma = 0;
			for (const char* m : { "summoned_skull", "celtic_guardian", "mystical_elf" }) {
				if (suma >= dueloIA.nivel(id)) break;
				j.manoExtra.push_back(aMano(m));
				suma += dueloIA.nivel(m);
			}
		}
		const bool ok = dueloIA.aplicar(j);
		fprintf(stderr, "[entrada-md] prueba YUGI_ENTRADA_MD=%s: %s %s (clave: %s)\n", v, ok ? "invocada" : "RECHAZADA", id.c_str(), esCartaClave(id) ? "si" : "no");
	}
#endif
