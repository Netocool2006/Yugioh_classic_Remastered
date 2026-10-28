	// Yugioh Classic Remaster (rama classic-md-ritmo): se incluye DENTRO de la clase del juego, despues de duelo_juego.h.
	// El ritmo de Master Duel (md_ritmo.h) aplicado a los sucesos del duelo (presentarSucesos):
	//  - ritmoLlegada: la carta que llega a una zona (invocar, colocar, activar desde la mano, invocacion especial)
	//    viene de su origen; el origen sale de la "foto" de las pilas del duelo tomada con la mesa al dia (lo que habia
	//    en la mano, el cementerio, el extra... antes de la jugada): donde habia una copia mas que ahora.
	//  - ritmoSalida: la carta que deja una zona (cementerio, desterrada, mano, extra) viaja a su pila.
	//  - ritmoActivar: la presentacion de toda activacion (los dos lados) y el numero de eslabon.
	//  - avanzarRitmo: corre los viajes (aterrizan con su destello, sonido y entrada de carta clave) y la activacion
	//    (destella y arranca la animacion de la carta: efectos_cartas.h). esperaEfecto cubre todo lo que falta, asi el
	//    duelo (y las pruebas, que miran esperaEfecto) espera a la mesa: el rival no juega y el reloj en linea no corre.
	//  - contadorZonaMD: los contadores que se ven sobre las cartas (turnos que quedan, contadores de magia).
	struct FotoRitmo { std::vector<std::string> mano, mazo, cementerio, desterrados, extra; };
	FotoRitmo fotoRitmo[2];
	float ritmoUltimaDuracion{ 0.0f };

	static int cuentaRitmo(const std::vector<std::string>& v, const std::string& id) { return (int)std::count(v.begin(), v.end(), id); }
	void ritmoTomarFoto()
	{
		for (int a = 0; a < 2; a++) {
			const ia::Lado& x = dueloIA.l[(size_t)a];
			FotoRitmo& f = fotoRitmo[a];
			f.mano = x.mano;
			f.mazo = x.mazo;
			f.cementerio = x.cementerio;
			f.desterrados = x.desterrados;
			f.extra = x.extra;
		}
	}
	// de donde vino la carta id que llega al lado "lado": la pila de la foto que tiene una copia mas que el duelo de
	// ahora (primero las propias; luego el cementerio / las desterradas del otro: Monster Reborn). La copia se quita de
	// la foto (dos llegadas iguales en la misma jugada no salen del mismo lugar)
	ritmo::Origen ritmoOrigen(int lado, const std::string& id, int& ladoPila, int& manoIdx)
	{
		manoIdx = -1;
		for (int k = 0; k < 2; k++) {
			const int a = k == 0 ? lado : 1 - lado;
			const ia::Lado& x = dueloIA.l[(size_t)a];
			FotoRitmo& f = fotoRitmo[a];
			struct P { std::vector<std::string>* foto; const std::vector<std::string>* ahora; ritmo::Origen o; };
			const P pilas[] = { { &f.mano, &x.mano, ritmo::O_MANO }, { &f.extra, &x.extra, ritmo::O_EXTRA }, { &f.cementerio, &x.cementerio, ritmo::O_CEMENTERIO },
				            { &f.desterrados, &x.desterrados, ritmo::O_DESTERRADO }, { &f.mazo, &x.mazo, ritmo::O_MAZO } };
			for (const P& p : pilas) {
				if (k == 1 && (p.o == ritmo::O_MANO || p.o == ritmo::O_MAZO || p.o == ritmo::O_EXTRA)) continue;
				if (cuentaRitmo(*p.foto, id) <= cuentaRitmo(*p.ahora, id)) continue;
				auto it = std::find(p.foto->begin(), p.foto->end(), id);
				if (p.o == ritmo::O_MANO) manoIdx = (int)(it - p.foto->begin());
				p.foto->erase(it);
				ladoPila = a;
				return p.o;
			}
		}
		ladoPila = lado;
		return ritmo::O_MANO;   // (sin rastro: de la mano, lo mas comun)
	}

	// la zona de la mesa de un suceso (monstruo, magia/trampa o la de campo)
	int zonaMesaSuceso(const ia::Suceso& s) const
	{
		if (s.zona < 0) return -1;
		if (s.magia && s.zona == ia::ZONA_CAMPO) return control::zonaPorId(s.lado ? "rival_campo" : "jugador_campo");
		return zonaMesaIA(s.lado, s.zona, s.magia);
	}

	// la carta del suceso llega a su zona viajando desde su origen. false: ya estaba (la del jugador llego volando de
	// su mano: control.h) o no hay zona; entonces se presenta como antes
	bool ritmoLlegada(const ia::Suceso& s)
	{
		const int z = zonaMesaSuceso(s);
		if (z < 0 || s.carta.empty()) return false;
		control::Celda& c = ctl.zonas[(size_t)z];
		if (!c.vacia() && c.carta == s.carta) return false;
		ritmo::Estado& r = ritmo::estado();
		if (r.llegandoA(z)) return false;
		ritmo::Viaje v;
		v.llegada = true;
		v.lado = s.lado;
		v.zona = z;
		v.carta = s.carta;
		v.sucesoTipo = s.tipo;
		v.bocaAbajo = s.tipo == ia::S_COLOCA || s.tipo == ia::S_COLOCA_MT;
		v.defensa = !s.magia && (s.tipo == ia::S_COLOCA || s.tipo == ia::S_INVOCA) && s.valor == 1;
		v.caraEnVuelo = !v.bocaAbajo;
		v.origen = ritmoOrigen(s.lado, s.carta, v.ladoPila, v.manoIdx);
		if (v.origen == ritmo::O_MANO && v.ladoPila == 1) {
			// el rival "elige" la carta: se levanta en su mano (que todavia la tiene) y luego vuela
			const int n = std::max(1, (int)dueloIA.l[1].mano.size() - ocultas[1] + 1);
			v.manoN = std::min(n, 10);   // (interfaz::MAX_MANO_R)
			v.manoIdx = v.manoIdx < 0 ? v.manoN / 2 : std::min(v.manoIdx, v.manoN - 1);
			v.tElige = cpumuestra::TOTAL;   // (rama classic-ps5-cpu-joey: la ensena como Joey the Passion, 1.33 s; antes T_ELIGE 0.38)
			ctl.manoR = v.manoN;
			if (v.caraEnVuelo) r.mostrar(s.carta);   // (el visor de la izquierda: la carta que ensena; la colocada no se revela)
		} else if (v.origen == ritmo::O_MANO) {
			v.manoN = (int)ctl.manoJ.size() + 1;
			v.manoIdx = v.manoN - 1;
			auto it = std::find(ctl.manoJ.begin(), ctl.manoJ.end(), s.carta);   // (sale de la mano que se ve)
			if (it != ctl.manoJ.end()) ctl.manoJ.erase(it);
		}
		// el movimiento de Master Duel (CardMoveMotionSetting: mano / pila -> zona de monstruo o de magia, con su "poder":
		// los monstruos fuertes suben mas alto y llegan mas lento; el poder por nivel es estimado: 5-6 y 7+)
		v.magia = s.magia;
		const int nv = s.magia ? 0 : dueloIA.nivel(s.carta);
		v.movs = ritmo::cadenaMovimientos(v.origen, true, s.magia, nv >= 7 ? 3 : nv >= 5 ? 2 : 0);
		v.dur = ritmo::duracionMovimientos(v.movs, (v.origen == ritmo::O_MANO ? ritmo::T_MANO_ZONA : ritmo::T_PILA_ZONA) +
		                                               (nv >= 7 ? ritmo::T_PODER_3 : nv >= 5 ? ritmo::T_PODER_2 : 0.0f));
		v.serie = ++r.serie;
		c = control::Celda{};   // (la zona espera a la carta)
		r.viajes.push_back(v);
		ctl.cerrarDetalle("llega una carta");   // (MD: EffectTaskCardMove.CommonMove -> DuelHUD.CloseCardInfo)
		ritmoUltimaDuracion = v.resta();
		esperaEfecto = std::max(esperaEfecto, v.resta() + ritmo::T_ATERRIZA);
		if (v.tElige <= 0.0f) sonido.sfx("sfx_carta_mover", 0.9f, control::ZONAS_MESA[z].x / 12.0f);
		mesaPendiente = true;
		fprintf(stderr, "[ritmo] llega '%s' a %s desde %s%s (%s%s)\n", s.carta.c_str(), control::ZONAS_MESA[z].id, ritmo::NOMBRE_ORIGEN[v.origen],
		        v.ladoPila != s.lado ? " del otro lado" : "", v.bocaAbajo ? "boca abajo" : "boca arriba", v.tElige > 0.0f ? ", el rival la elige de su mano" : "");
		return true;
	}

	// la carta del suceso deja su zona y viaja a su pila (cementerio, desterradas, mano, extra)
	void ritmoSalida(const ia::Suceso& s)
	{
		const int z = zonaMesaSuceso(s);
		if (z < 0) return;
		control::Celda& c = ctl.zonas[(size_t)z];
		if (c.vacia() || (c.carta != s.carta && c.carta != "?")) return;
		ritmo::Estado& r = ritmo::estado();
		ritmo::Viaje v;
		v.llegada = false;
		v.lado = v.ladoPila = s.lado;
		v.zona = z;
		v.carta = s.carta;
		v.bocaAbajo = c.bocaAbajo;
		v.defensa = c.defensa;
		v.origen = s.tipo == ia::S_DESTERRADO ? ritmo::O_DESTERRADO
		           : s.tipo == ia::S_A_LA_MANO ? (s.valor == 1 ? ritmo::O_EXTRA : s.valor == 5 ? ritmo::O_MAZO : ritmo::O_MANO) : ritmo::O_CEMENTERIO;   // (S_A_LA_MANO valor: 1 al extra, 5 al mazo; 0/2/3/4 a la mano)
		// (al cementerio y desterrada boca arriba se ve la cara; a la mano del rival, el dorso; la ficha se desvanece)
		v.caraEnVuelo = !(v.origen == ritmo::O_MANO && s.lado == 1) && !(s.tipo == ia::S_DESTERRADO && s.valor == 2);
		v.magia = s.magia;
		v.movs = ritmo::cadenaMovimientos(v.origen, false, s.magia, 0);   // (MD: Field2GraveNormal, Field2Hand, Field2Deck)
		v.dur = ritmo::duracionMovimientos(v.movs, ritmo::T_SALIDA);
		v.serie = ++r.serie;
		r.viajes.push_back(v);
		c = control::Celda{};
		ctl.cerrarDetalle("sale una carta");   // (MD: EffectTaskCardMove.CommonMove -> DuelHUD.CloseCardInfo)
		// (varias salidas seguidas, como Raigeki, van juntas: solo la ultima hace esperar)
		const size_t k = sucesosVistos + 1;
		const auto& su = dueloIA.sucesos;
		const bool otra = k < su.size() && (su[k].tipo == ia::S_AL_CEMENTERIO || su[k].tipo == ia::S_DESTERRADO || su[k].tipo == ia::S_A_LA_MANO) && su[k].zona >= 0;
		if (!otra) esperaEfecto = std::max(esperaEfecto, ritmo::T_SALIDA * 0.8f);
		// (rama classic-arreglo-ciclo-md: la magia / trampa que resuelve y se va -Polymerization, la magia ritual- se ve
		// entera en su viaje de MD, Field2GraveNormal, antes de lo que sigue: la secuencia de fusion/ritual tapa la mesa)
		if (s.magia && v.origen == ritmo::O_CEMENTERIO) esperaEfecto = std::max(esperaEfecto, v.resta() + 0.08f);
		if (s.tipo == ia::S_DESTERRADO && sonido.hay("sfx_desterrar")) sonido.sfx("sfx_desterrar", 0.9f, control::ZONAS_MESA[z].x / 12.0f);
		mesaPendiente = true;
		fprintf(stderr, "[ritmo] sale '%s' de %s a %s\n", s.carta.c_str(), control::ZONAS_MESA[z].id, ritmo::NOMBRE_ORIGEN[v.origen]);
	}

	// rama classic-arreglo-ciclo-md (jugador humano: Axe of Despair, Megamorph, Black Pendant... se iban sin verse): las
	// magias de equipo del monstruo destruido en combate salen de su zona y viajan al cementerio cuando el destruido se
	// rompe (T_ROMPE), como en MD; antes el combate se las saltaba y la mesa las quitaba al terminar
	std::vector<ia::Suceso> salidasCombate;
	// (y los sacrificios que el jugador eligio para invocar: control.h los saca de la mesa al aplicar la jugada; aca viajan
	// al cementerio con el viaje de MD mientras la invocada vuela a su zona. Antes desaparecian: CU-HU-ENTERARSE-COSTE)
	void ritmoSacrificados()
	{
		if (ctl.sacrificados.empty()) return;
		ritmo::Estado& r = ritmo::estado();
		for (const auto& sc : ctl.sacrificados) {
			const control::Celda& c = sc.second;
			if (sc.first < 0 || c.vacia()) continue;
			ritmo::Viaje v;
			v.llegada = false;
			v.lado = v.ladoPila = control::ZONAS_MESA[sc.first].rival ? 1 : 0;
			v.zona = sc.first;
			v.carta = c.carta;
			v.bocaAbajo = c.bocaAbajo;
			v.defensa = c.defensa;
			v.origen = ritmo::O_CEMENTERIO;
			v.caraEnVuelo = c.carta != "?";
			v.movs = ritmo::cadenaMovimientos(ritmo::O_CEMENTERIO, false, false, 0);
			v.dur = ritmo::duracionMovimientos(v.movs, ritmo::T_SALIDA);
			v.serie = ++r.serie;
			r.viajes.push_back(v);
			ctl.cerrarDetalle("sacrificio");
			esperaEfecto = std::max(esperaEfecto, v.resta());
			fprintf(stderr, "[ritmo] sacrificio '%s' de %s al cementerio\n", c.carta.c_str(), control::ZONAS_MESA[sc.first].id);
		}
		ctl.sacrificados.clear();
	}
	void ritmoSalidasCombate()
	{
		ritmoSacrificados();
		if (salidasCombate.empty() || (hayAtaque() && tAtaque < ritmo::T_ROMPE)) return;
		const std::vector<ia::Suceso> v = salidasCombate;
		salidasCombate.clear();
		for (const ia::Suceso& s : v) ritmoSalida(s);
	}

	// rama classic-arreglo-ciclo-md (jugador humano: descartes forzados, Confiscation, Mind Crush; destierros desde el
	// cementerio, Kycoo; del mazo al cementerio, Assault on GHQ; busquedas a la mano): la carta que pasa de una pila a otra
	// sin zona de la mesa (mano, mazo, cementerio, desterradas) viaja con su cara entre las dos, como en MD, y su nombre sale
	// en el aviso. Antes la pila cambiaba sin nada en pantalla. false: no hay de donde ni adonde (se presenta como antes)
	// rama classic-arreglo-ciclo-md (pedido del usuario: Polymerization / la magia ritual con su viaje al cementerio): en
	// Master Duel la magia de fusion / ritual se queda en su zona mientras dura la invocacion (la secuencia de MD) y al
	// terminar la cadena se va al cementerio con Field2GraveNormal. El motor la manda al cementerio antes del S_INVOCA: aca
	// se pospone (la carta sigue en su zona; de la mano: espera) hasta que la invocacion se presento y la secuencia termino
	struct Pospuesta { ia::Suceso s; size_t tras; int z; control::Celda celda; };
	std::vector<Pospuesta> pospuestas;
	// el indice del S_INVOCA de fusion / ritual de ese lado que sigue a los que van al cementerio desde "desde" (o -1)
	long ritmoFusionSigue(size_t desde, int lado) const
	{
		const auto& su = dueloIA.sucesos;
		for (size_t j = desde + 1; j < su.size(); j++) {
			const ia::Suceso& x = su[j];
			if (x.tipo == ia::S_MODIFICA) continue;
			if (x.lado == lado && (x.tipo == ia::S_AL_CEMENTERIO || x.tipo == ia::S_DESTERRADO)) continue;
			if (x.tipo == ia::S_INVOCA && x.lado == lado && !x.magia) {
				const mazo::Carta* k = coleccion.carta(x.carta);
				if (k && (k->clase == "fusion" || k->clase == "ritual")) return (long)j;
			}
			break;
		}
		return -1;
	}
	// S_AL_CEMENTERIO de una magia / trampa justo antes de su invocacion de fusion / ritual: se pospone (true)
	bool ritmoPosponer(const ia::Suceso& s)
	{
		if (s.tipo != ia::S_AL_CEMENTERIO || s.carta.empty()) return false;
		const ia::Carta* c = dueloIA.carta(s.carta);
		if (!c || c->monstruo) return false;
		const long inv = ritmoFusionSigue(sucesosVistos, s.lado);
		if (inv < 0) return false;
		if (s.zona < 0) {
			// (la magia ritual jugada desde la mano: en MD se activa en el campo -viaja de la mano a una zona de magia y se
			// muestra, CardShow- y ahi se queda durante la invocacion; despues va al cementerio desde esa zona)
			for (int i = 0; i < ia::ZONAS; i++) {
				const int zm = zonaMesaIA(s.lado, i, true);
				if (zm < 0 || !ctl.zonas[(size_t)zm].vacia() || !dueloIA.l[(size_t)s.lado].mt[(size_t)i].vacia() || ritmo::estado().llegandoA(zm)) continue;
				ia::Suceso a = s;
				a.tipo = ia::S_ACTIVA_MT;
				a.zona = i;
				a.magia = true;
				a.valor = 0;
				ritmoActivar(a, zm, false);
				ia::Suceso sal = s;
				sal.zona = i;
				sal.magia = true;
				control::Celda c;
				c.carta = s.carta;
				pospuestas.push_back(Pospuesta{ sal, (size_t)inv, zm, c });
				fprintf(stderr, "[ritmo] '%s' se activa desde la mano en %s y se va al cementerio despues de la invocacion (como MD)\n", s.carta.c_str(), control::ZONAS_MESA[zm].id);
				return true;
			}
		}
		const int z = s.zona >= 0 ? zonaMesaSuceso(s) : -1;
		pospuestas.push_back(Pospuesta{ s, (size_t)inv, z, z >= 0 ? ctl.zonas[(size_t)z] : control::Celda{} });
		fprintf(stderr, "[ritmo] '%s' se va al cementerio despues de la invocacion (como MD)\n", s.carta.c_str());
		return true;
	}
	// cada cuadro: las pospuestas siguen en su zona; terminada la invocacion (presentada y sin la secuencia de MD ni viajes)
	// viajan al cementerio
	void ritmoPospuestas()
	{
		if (pospuestas.empty()) return;
		const bool listo = !entradamd::estado().activa && !ritmo::estado().ocupado() && esperaEfecto <= 0.0f;
		std::vector<Pospuesta> quedan;
		for (Pospuesta& p : pospuestas) {
			if (sucesosVistos <= p.tras || !listo) {
				quedan.push_back(p);
				continue;
			}
			if (p.z >= 0) {
				ctl.zonas[(size_t)p.z] = p.celda;
				ritmoSalida(p.s);
			} else {
				ritmoEntrePilas(p.s, ritmo::O_MANO);
			}
		}
		pospuestas = quedan;
	}
	void ritmoMantenerPospuestas()
	{
		for (const Pospuesta& p : pospuestas)
			if (p.z >= 0 && ctl.zonas[(size_t)p.z].vacia()) ctl.zonas[(size_t)p.z] = p.celda;
	}

	bool ritmoEntrePilas(const ia::Suceso& s, int origenForzado = -1)
	{
		if (s.carta.empty() || s.carta == "?" || s.zona >= 0) return false;
		ritmo::Origen destino;
		const char* aviso;
		switch (s.tipo) {
		case ia::S_DESCARTA: destino = ritmo::O_CEMENTERIO, aviso = "Descartada"; break;
		case ia::S_AL_CEMENTERIO: destino = ritmo::O_CEMENTERIO, aviso = "Al cementerio"; break;
		case ia::S_DESTERRADO: destino = ritmo::O_DESTERRADO, aviso = "Desterrada"; break;
		case ia::S_A_LA_MANO:
			destino = s.valor == 1 ? ritmo::O_EXTRA : s.valor == 5 ? ritmo::O_MAZO : ritmo::O_MANO;   // (valor 1 al extra, 5 al mazo; 2 del cementerio y 3 del mazo, a la mano)
			aviso = destino == ritmo::O_MAZO ? "Al mazo" : destino == ritmo::O_EXTRA ? "Al mazo de fusión" : "A la mano";
			break;
		default: return false;
		}
		// (los materiales de una fusion / un ritual: la secuencia de MD los muestra, entrada_md_juego.h; sin viaje aparte. La
		// magia -Polymerization, la magia ritual- no es material: viaja al cementerio como toda magia que resuelve)
		const ia::Carta* cs = dueloIA.carta(s.carta);
		if ((s.tipo == ia::S_AL_CEMENTERIO || s.tipo == ia::S_DESTERRADO) && cs && cs->monstruo && ritmoFusionSigue(sucesosVistos, s.lado) >= 0) return false;
		int ladoPila = s.lado, manoIdx = -1;
		// (el origen que dice el motor: S_A_LA_MANO valor 3 del mazo, 2 del cementerio; si no, la foto de las pilas)
		if (origenForzado < 0 && s.tipo == ia::S_A_LA_MANO && (s.valor == 3 || s.valor == 2)) origenForzado = s.valor == 3 ? ritmo::O_MAZO : ritmo::O_CEMENTERIO;
		ritmo::Origen origen = origenForzado >= 0 ? (ritmo::Origen)origenForzado : ritmoOrigen(s.lado, s.carta, ladoPila, manoIdx);
		if (s.tipo == ia::S_DESCARTA) origen = ritmo::O_MANO, ladoPila = s.lado;
		if (origen == destino) return false;   // (sin rastro de donde vino)
		auto pila = [](ritmo::Origen o, int lado) {
			const char* n = o == ritmo::O_CEMENTERIO ? "cementerio" : o == ritmo::O_DESTERRADO ? "desterrado" : o == ritmo::O_EXTRA ? "fusion" : "mazo";
			return control::zonaPorId(std::string(lado ? "rival_" : "jugador_") + n);
		};
		ritmo::Estado& r = ritmo::estado();
		ritmo::Viaje v;
		v.soloPila = true;
		v.carta = s.carta;
		v.magia = true;
		v.bocaAbajo = v.defensa = false;
		if (origen == ritmo::O_MANO) {   // de la mano a una pila: "llega" a la pila
			v.llegada = true;
			v.zona = pila(destino, s.lado);
			v.lado = v.ladoPila = s.lado;
			v.origen = ritmo::O_MANO;
			if (s.lado == 1) {
				v.manoN = std::max(1, std::min(ctl.manoR + 1, 10));
				v.manoIdx = v.manoN - 1;
			} else {
				v.manoN = (int)ctl.manoJ.size() + 1;
				v.manoIdx = manoIdx >= 0 ? std::min(manoIdx, v.manoN - 1) : v.manoN - 1;
			}
			v.caraEnVuelo = true;
			v.movs = ritmo::cadenaMovimientos(ritmo::O_MANO, true, true, 0);
			v.dur = ritmo::duracionMovimientos(v.movs, ritmo::T_MANO_ZONA);
		} else {   // de una pila (mazo, cementerio, desterradas) a otra o a la mano: "sale" de su pila
			v.llegada = false;
			v.zona = pila(origen, ladoPila);
			v.lado = ladoPila;
			v.ladoPila = s.lado;
			v.origen = destino;
			v.manoIdx = -1;
			v.caraEnVuelo = !(destino == ritmo::O_MANO && s.lado == 1) && !(destino == ritmo::O_MAZO && origen == ritmo::O_MAZO);
			v.movs = ritmo::cadenaMovimientos(destino, false, true, 0);
			v.dur = ritmo::duracionMovimientos(v.movs, ritmo::T_SALIDA);
		}
		if (v.zona < 0) return false;
		v.serie = ++r.serie;
		r.viajes.push_back(v);
		ctl.cerrarDetalle("una carta pasa de pila");   // (MD: EffectTaskCardMove.CommonMove -> DuelHUD.CloseCardInfo)
		// (el nombre en el aviso: la del rival que va a su mano no se dice)
		if (!(destino == ritmo::O_MANO && s.lado == 1)) {
			ctl.aviso = std::string(idioma::tr(aviso)) + ": " + interfazDuelo.nombreCarta(s.carta);
			ctl.tAviso = 0.0f;
		}
		esperaEfecto = std::max(esperaEfecto, std::max(v.resta(), 0.5f));
		sonido.sfx(s.tipo == ia::S_DESTERRADO && sonido.hay("sfx_desterrar") ? "sfx_desterrar" : "sfx_carta_mover", 0.8f, control::ZONAS_MESA[v.zona].x / 12.0f);
		fprintf(stderr, "[ritmo] entre pilas '%s': %s -> %s (%s)\n", s.carta.c_str(), ritmo::NOMBRE_ORIGEN[origen], ritmo::NOMBRE_ORIGEN[destino], s.lado ? "rival" : "jugador");
		return true;
	}

	// ---- rama classic-md-combate: el combate de Master Duel
	bool ataqueAltoMD{ false };   // el ataque en curso es "High" (ATK >= 2500: viaje de 0.65 s; md_ritmo.h pasoRelojAtaque)
	// el ATK con que ataca la carta de la zona z de la mesa (el del duelo si sigue en el campo; si no, el de la carta)
	int ritmoAtkZona(int z) const
	{
		if (z < 0 || z >= (int)control::NUM_ZONAS_MESA) return 0;
		const control::Celda& c = ctl.zonas[(size_t)z];
		if (c.vacia() || c.carta == "?") return 0;
		const ia::Lado& x = dueloIA.l[control::ZONAS_MESA[z].rival ? 1 : 0];
		for (const ia::Monstruo& m : x.m)
			if (!m.vacia() && m.id == c.carta && m.atkEf > 0) return m.atkEf;
		return dueloIA.atk(c.carta);
	}
	// la carta destruida en combate (MD: CardRoot.ReqBrokenEffect tras la pausa del golpe: fxp_cardbrk_btl_001 y
	// SE_CARDBREAK_02, sin esperar; y CardMoveField2GraveNormal 0.42 s, arco alto) deja su zona y viaja a su cementerio,
	// igual que las destruidas por efecto (ritmoSalida). false: no habia carta (o ya se fue)
	bool ritmoDestruidaCombate(int z)
	{
		if (z < 0 || z >= (int)control::NUM_ZONAS_MESA) return false;
		control::Celda& c = ctl.zonas[(size_t)z];
		if (c.vacia()) return false;
		ritmo::Estado& r = ritmo::estado();
		ritmo::Viaje v;
		v.llegada = false;
		v.lado = v.ladoPila = control::ZONAS_MESA[z].rival ? 1 : 0;
		v.zona = z;
		v.carta = c.carta;
		v.bocaAbajo = false;
		v.defensa = c.defensa;
		v.origen = ritmo::O_CEMENTERIO;
		v.caraEnVuelo = c.carta != "?";
		v.magia = false;
		v.movs = ritmo::cadenaMovimientos(ritmo::O_CEMENTERIO, false, false, 0);   // (MD: CardMoveField2GraveNormal)
		v.dur = ritmo::duracionMovimientos(v.movs, ritmo::T_SALIDA);
		v.serie = ++r.serie;
		r.viajes.push_back(v);
		ctl.cerrarDetalle("destruida en combate");   // (MD: la rotura y el viaje son un EffectTaskCardMove)
		ctl.alCementerio(z);   // (a la pila y la zona queda vacia: la carta va en el aire)
		const float pan = control::ZONAS_MESA[z].x / 12.0f;
		sonido.sfx(sonido.hay("sfx_destruir_batalla") ? "sfx_destruir_batalla" : "sfx_destruir", 1.0f, pan);
		mesaPendiente = true;
		fprintf(stderr, "[ritmo] destruida en combate '%s' (%s): se rompe y viaja al cementerio (%.2f s)\n", v.carta.c_str(), control::ZONAS_MESA[z].id, v.dur);
		return true;
	}

	// la presentacion de una activacion (S_ACTIVA_MT / S_ACTIVA_EFECTO) en la zona z de la mesa (-1: desde la mano o el
	// cementerio, sin zona): la carta llega o se da vuelta, se levanta, se acerca grande, destella y vuelve
	void ritmoActivar(const ia::Suceso& s, int z, bool campo)
	{
		ritmo::Estado& r = ritmo::estado();
		float retraso = 0.0f;
		if (z >= 0 && s.tipo == ia::S_ACTIVA_MT) {
			control::Celda& c = ctl.zonas[(size_t)z];
			if (c.carta != s.carta) {   // (de la mano: viaja a su zona boca arriba)
				if (ritmoLlegada(s)) {
					retraso = ritmoUltimaDuracion;
				} else {
					c = control::Celda{};
					c.carta = s.carta;
					c.tLlega = 0.0f;
				}
			} else if (c.bocaAbajo) {   // (colocada: se da vuelta primero)
				ctl.cerrarDetalle("se voltea");   // (MD: EffectTaskCardFlipTurn -> DuelHUD.CloseCardInfo)
				c.antesBocaAbajo = true;
				c.bocaAbajo = false;
				c.tVolteo = 0.0f;
				retraso = ritmo::T_VOLTEO;
				sonido.sfx("sfx_voltear", 0.9f, control::ZONAS_MESA[z].x / 12.0f);
			}
		} else if (z >= 0 && (ctl.zonas[(size_t)z].vacia() || ctl.zonas[(size_t)z].carta != s.carta)) {
			z = -1;   // (efecto desde la mano / el cementerio: s.zona no es una zona de la mesa)
		}
		ritmo::Activacion a;
		a.carta = s.carta;
		a.lado = s.lado;
		a.zona = z;
		a.eslabon = s.valor;
		a.monstruo = s.tipo == ia::S_ACTIVA_EFECTO;
		a.corte = s.lado == 1 && dueloIA.activo == 0;   // (sale el cartel del rival: ya muestra la carta grande)
		a.retraso = retraso;
		a.serie = ++r.serie;
		(void)campo;
		r.acts.push_back(a);
		r.mostrar(s.carta);   // (el visor de la izquierda: la carta que se activa)
		if (s.valor >= 1) r.cadena.push_back({ z, s.valor, s.carta });
		// el eslabon de la cadena con el sonido de Master Duel (se_duelchain_stack01 / 2nd / 3rd / 4th)
		if (s.valor >= 2) {
			char k[32];
			snprintf(k, sizeof(k), "sfx_cadena_%d", std::min(s.valor, 4));
			if (sonido.hay(k)) sonido.sfx(k, 0.9f, 0.0f, retraso);
		}
		const float efecto = s.tipo == ia::S_ACTIVA_MT ? std::min(efc::duracion(efectosCartas.receta(s.carta)), 1.8f) : 0.5f;
		esperaEfecto = std::max(esperaEfecto, retraso + ritmo::T_ACTIVA * ritmo::T_ACT_DESTELLO + efecto + (s.valor >= 2 ? ritmo::T_ESLABON_EXTRA : 0.0f));
		fprintf(stderr, "[ritmo] activa '%s' (%s, eslabon %d, zona %s, espera %.2f s)\n", s.carta.c_str(), s.lado ? "rival" : "jugador", s.valor,
		        z >= 0 ? control::ZONAS_MESA[z].id : "-", retraso);
	}

	// S_RESUELVE: el eslabon de arriba destella y se va su numero
	void ritmoResuelve()
	{
		ritmo::Estado& r = ritmo::estado();
		ctl.cerrarDetalle("resuelve la cadena");   // (MD: EffectTaskChainStep -> DuelHUD.CloseCardInfo)
		if (r.cadena.empty()) return;
		const int n = r.maxEslabon();
		for (const ritmo::Marca& m : r.cadena)
			if (m.n == n && m.zona >= 0) {
				r.zonaResuelve = m.zona;
				r.tResuelve = 0.0f;
			}
		if (n >= 2 && sonido.hay("sfx_cadena_resuelve")) sonido.sfx("sfx_cadena_resuelve", 0.8f);
		if (n >= 2) esperaEfecto = std::max(esperaEfecto, ritmo::T_RESUELVE);
		r.quitarEslabonMayor();
	}

	// lo que la mesa todavia no tiene que mostrar: las llegadas en vuelo (y la mano del rival con la carta que elige)
	void ritmoOcultarEnVuelo()
	{
		ritmoMantenerPospuestas();   // (rama classic-arreglo-ciclo-md: la magia de fusion / ritual sigue en su zona)
		ritmo::Estado& r = ritmo::estado();
		for (const ritmo::Viaje& v : r.viajes) {
			if (!v.llegada || v.soloPila) continue;
			control::Celda& c = ctl.zonas[(size_t)v.zona];
			if (!c.vacia() && c.carta == v.carta) {
				c = control::Celda{};
				mesaPendiente = true;
			}
			if (v.origen == ritmo::O_MANO && v.ladoPila == 1 && v.eligiendo()) ctl.manoR++;
		}
	}

	// el ATK de las cartas que llegan, la entrada de carta clave... al aterrizar
	void ritmoAterriza(const ritmo::Viaje& v)
	{
		control::Celda& c = ctl.zonas[(size_t)v.zona];
		c = control::Celda{};
		c.carta = v.carta;
		c.bocaAbajo = c.antesBocaAbajo = v.bocaAbajo;
		c.defensa = c.antesDefensa = v.defensa;
		c.tLlega = 0.0f;
		const float pan = control::ZONAS_MESA[v.zona].x / 12.0f;
		if (!v.bocaAbajo && v.lado == 1) ritmo::estado().mostrar(v.carta);   // (la del rival boca arriba, al visor)
		if (v.sucesoTipo == ia::S_INVOCA) {
			tInvocada[v.zona] = 0.0f;   // destello de aparicion
			esperaEfecto = std::max(esperaEfecto, empezarEntradaClave(v.carta, v.lado, v.zona));   // (rama classic-cartas-clave)
			if (v.lado == 1) sonido.sfx("sfx_invocar", 1.0f, pan);   // (la del jugador suena por audio.h)
		}
		fprintf(stderr, "[ritmo] aterriza '%s' en %s\n", v.carta.c_str(), control::ZONAS_MESA[v.zona].id);
	}

	// cada cuadro, con el dt del duelo (ya escalado por la velocidad de animaciones), antes de presentar sucesos
	void avanzarRitmo(float dt)
	{
		ritmo::Estado& r = ritmo::estado();
		static bool leidos = false;   // (los movimientos de Master Duel, una vez: <assets>/md_ritmo/movimientos.txt)
		if (!leidos) {
			leidos = true;
			ritmo::leerMovimientos(getAssetPath() + "md_ritmo/movimientos.txt");
		}
		r.tResuelve += dt;
		r.tVisor += dt;
		if (!r.ocupado() && sucesosVistos >= dueloIA.sucesos.size()) {
			ritmoTomarFoto();
			if (dueloIA.ef.cadena.empty() && !r.cadena.empty()) r.cadena.clear();
		}
		for (size_t i = 0; i < r.viajes.size();) {
			ritmo::Viaje& v = r.viajes[i];
			const bool eligia = v.eligiendo();
			v.t += dt;
			if (eligia && !v.eligiendo()) {   // (la carta elegida deja la mano del rival y vuela)
				ctl.manoR = std::max(0, ctl.manoR - 1);
				sonido.sfx("sfx_carta_mover", 0.9f, control::ZONAS_MESA[v.zona].x / 12.0f);
			}
			if (!v.termino()) {
				i++;
				continue;
			}
			const ritmo::Viaje fin = v;
			r.viajes.erase(r.viajes.begin() + (long)i);
			if (fin.llegada && !fin.soloPila) ritmoAterriza(fin);
		}
		for (size_t i = 0; i < r.acts.size();) {
			ritmo::Activacion& a = r.acts[i];
			a.t += dt;
			if (!a.destello && a.u() >= ritmo::T_ACT_DESTELLO) {
				a.destello = true;
				if (a.zona >= 0) {
					ctl.zonas[(size_t)a.zona].tActiva = 0.0f;
					if (a.monstruo) {
						tInvocada[a.zona] = 0.0f;
					} else {
						activarEfectoCarta(a.carta, a.zona);   // rama classic-fx-md: la animacion de la carta (efectos_cartas.h)
						if (std::string(control::ZONAS_MESA[a.zona].tipo) == "campo" && campos::esCampo(a.carta)) activarCampo(a.carta, a.lado);
					}
				}
			}
			if (a.termino()) r.acts.erase(r.acts.begin() + (long)i);
			else i++;
		}
		if (r.ocupado()) esperaEfecto = std::max(esperaEfecto, r.resta());
	}

	// los contadores sobre una carta de la mesa (como Master Duel): turnos que le quedan (Swords of Revealing Light 3,
	// 2, 1) o contadores (de magia...). turnos: true si es una cuenta de turnos. false si la zona no muestra nada
	bool contadorZonaMD(int zonaMesa, int& n, bool& turnos) const
	{
		if (zonaMesa < 0 || zonaMesa >= (int)control::NUM_ZONAS_MESA || ctl.vista.pantalla != VD::DUELO) return false;
		const std::string id = control::ZONAS_MESA[zonaMesa].id;
		const bool rival = id.rfind("rival_", 0) == 0;
		const ia::Lado& x = dueloIA.l[rival ? 1 : 0];
		const control::Celda& c = ctl.zonas[(size_t)zonaMesa];
		if (c.vacia() || c.bocaAbajo) return false;
		const int i = id.back() - '0';
		turnos = false;
		if (id.find("_magia_") != std::string::npos && i >= 0 && i < ia::ZONAS) {
			const ia::MagiaTrampa& k = x.mt[(size_t)i];
			if (k.vacia() || k.bocaAbajo || k.id != c.carta) return false;
			// cartas que se van tras N turnos (finTurno en ia.h: suman un contador por turno del rival)
			static const struct { const char* id; int turnos; } T[] = { { "swords_of_revealing_light", 3 }, { "nightmare_s_steelcage", 2 }, { "pikeru_s_second_sight", 2 } };
			for (const auto& t : T)
				if (k.id == t.id) {
					n = std::max(0, t.turnos - k.contadores);
					turnos = true;
					return n > 0;
				}
			if (k.id == "tower_of_babel" || k.id == "pitch_black_power_stone") {
				n = k.contadores;
				return n > 0;
			}
			if (k.id == "wave_motion_cannon" && k.contadores > 1) {   // (fases de espera desde que se activo)
				n = k.contadores - 1;
				turnos = true;
				return true;
			}
			return false;
		}
		if (id.find("_monstruo_") != std::string::npos && i >= 0 && i < ia::ZONAS) {
			const ia::Monstruo& m = x.m[(size_t)i];
			if (m.vacia() || m.bocaAbajo || m.id != c.carta || m.contadores <= 0) return false;
			if (!ia::guardaContadores(m.id) && m.id != "balloon_lizard" && m.id != "b_e_s_big_core") return false;
			n = m.contadores;
			return true;
		}
		return false;
	}

#include "ritmo_turno_juego.h"   // rama classic-ps5-ritmo: el ritmo del turno (robo una vez, carteles de fase, pausa de lectura del rival)
