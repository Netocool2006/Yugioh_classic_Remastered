	// Yugioh Classic Remaster (rama classic-cartas-clave): se incluye DENTRO de la clase del juego (al final de
	// duelo_juego.h). La entrada especial de las cartas clave (entrada_clave.h: estado y tiempos; entrada_clave_ui.h: el
	// corte 2D). Ganchos de una linea:
	//  - duelo_juego.h, S_INVOCA: esperaEfecto = max(esperaEfecto, empezarEntradaClave(...)): el duelo espera a la
	//    entrada como a una activacion (cero si la carta no es clave: no bloquea nada).
	//  - duelo_juego.h, avanzarDueloIA: avanzarEntradaClave(dt) con el dt del duelo (velocidad.h y R2; la pausa la para).
	//  - yugireload.cpp, updateUniformBuffers: camaraEntradaClave(ojo, mira) (acercamiento, orbita y temblor; tambien
	//    pide los efectos horneados de la entrada).
	//  - figuras.h: poseEntradaClave (matrizFigura: rigidas, crecen girando y caen) y clipEntradaClave (posarFiguras:
	//    con esqueleto, su clip de ataque).
	// PC: YUGI_ENTRADA=0 la apaga (solo para comparar).
	// PC (capturas, rama classic-visual-final): YUGI_CARTA_CLAVE=<id de carta> (o 1 = blue_eyes) salta el sorteo y, a
	// los 2,5 s del duelo, invoca esa carta en el campo del jugador (invocacion especial de verdad: S_INVOCA) para ver
	// su entrada. YUGI_CARTA_CLAVE_RIVAL=1 la invoca el rival. La llama pruebaResultado (resultado_juego.h).
#include "entrada_md_juego.h"   // rama classic-md-entradas: las entradas de fusion y ritual de Master Duel
	bool entradaClaveEncendida{ true };
	bool entradaClaveLeida{ false };
#if defined(PS5_HOST_REFERENCE)
	float tPruebaClave{ 0.0f };
	bool pruebaClaveHecha{ false };
	void pruebaCartaClave(float dt)
	{
		pruebaEntradaMD(dt);   // (rama classic-md-entradas: YUGI_ENTRADA_MD, entrada_md_juego.h)
		const char* e = getenv("YUGI_CARTA_CLAVE");
		if (!e || !*e || pruebaClaveHecha || ctl.vista.pantalla != VD::DUELO) return;
		if (ctl.vista.sorteo) {
			ctl.vista.sorteo = false;
			arrancarDueloIA(0);
			tPruebaClave = 0.0f;
			return;
		}
		tPruebaClave += dt;
		if (tPruebaClave < 2.5f || dueloIA.ganador >= 0) return;
		pruebaClaveHecha = true;
		const std::string id = std::string(e) == "1" ? std::string("blue_eyes") : std::string(e);
		const char* r = getenv("YUGI_CARTA_CLAVE_RIVAL");
		const int lado = r && atoi(r) != 0 ? 1 : 0;
		const int z = dueloIA.invocarEspecial(lado, id, false);
		fprintf(stderr, "[entrada] prueba YUGI_CARTA_CLAVE=%s: lado %d zona %d (clave: %s)\n", id.c_str(), lado, z, esCartaClave(id) ? "si" : "no");
	}
#endif

	// al presentar una invocacion (S_INVOCA): la entrada si la carta es clave. Devuelve cuanto debe esperar el duelo
	float empezarEntradaClave(const std::string& carta, int lado, int z)
	{
		if (!entradaClaveLeida) {
			entradaClaveLeida = true;
#if defined(PS5_HOST_REFERENCE)
			if (const char* v = getenv("YUGI_ENTRADA")) entradaClaveEncendida = atoi(v) != 0;
#endif
		}
		entrada::Estado& e = entrada::estado();
		if (!entradaClaveEncendida || z < 0 || !esMonstruo(z) || !esCartaClave(carta)) return 0.0f;
		const mazo::Carta* k = coleccion.carta(carta);
		const std::string clave = std::to_string(lado) + ":" + carta;
		const bool corta = std::find(e.vistas.begin(), e.vistas.end(), clave) != e.vistas.end();
		if (!corta) e.vistas.push_back(clave);
		e.activa = true;
		e.carta = carta;
		e.zona = z;
		e.rival = lado == 1;
		e.atributo = k ? k->atributoClave : std::string();
		e.clase = k ? entrada::claseDe(k->clase, k->razaClave, k->atributoClave) : entrada::NORMAL;
		if (e.clase == entrada::DIOS) e.atributo = "DIVINIDAD";
		e.corta = corta;
		e.trasMD = vieneDeEntradaMD(carta, lado);   // (rama classic-md-entradas: la fusion/ritual de MD ya hizo el vortice / circulo)
		e.con3D = figuras3D && figuraDe(carta) >= 0;
		e.t = 0.0f;
		e.tFigura = entrada::duracionCorte(e.clase, e.con3D, corta);
		e.dur = entrada::duracion(e.clase, e.con3D, corta);
		e.saltando = velAnim.saltando > 0.0f;
		if (!e.saltando) sonido.sfx(corta ? "sfx_clave_corta" : entrada::sonidoCorte(e.atributo, e.clase), 1.0f, control::ZONAS_MESA[z].x / 12.0f);
		fprintf(stderr, "[entrada] %s (%s, %s): clase %d, %s, %.1f s%s\n", carta.c_str(), e.rival ? "rival" : "jugador", e.atributo.c_str(), (int)e.clase,
		        corta ? "corta" : e.con3D ? "completa con figura" : "solo el corte 2D", e.dur, e.saltando ? " (saltando)" : "");
		return e.dur;
	}

	// cada cuadro del duelo (dt ya con la velocidad elegida y R2)
	void avanzarEntradaClave(float dt)
	{
		avanzarEntradaMD(dt);   // rama classic-md-entradas (entrada_md_juego.h)
		entrada::Estado& e = entrada::estado();
		if (!e.activa) return;
		const float antes = e.t;
		e.t += dt;
		e.saltando = velAnim.saltando > 0.0f;
		// la figura no aparece hasta que termina el corte (se materializa despues)
		if (e.con3D && e.t < e.tFigura && e.zona >= 0) {
			EstadoFigura& f = estFig[(size_t)e.zona];
			if (f.carta == e.carta) f.tAparece = 0.0f;
		}
		// los sonidos de la figura, cuando el reloj pasa por su momento (saltando: ninguno)
		auto cruza = [&](float x) { return antes < x && e.t >= x; };
		const float pan = control::ZONAS_MESA[(size_t)e.zona].x / 12.0f;
		if (!e.saltando && !e.corta && e.con3D) {
			const float tf = e.tFigura;
			if (cruza(tf)) sonido.sfx("sfx_clave_aparece", 1.0f, pan);
			switch (e.clase) {
			case entrada::DRAGON:
				if (cruza(tf + 0.45f)) sonido.sfx("sfx_clave_rugido", 1.0f, pan);
				break;
			case entrada::MAGO:
				if (cruza(tf + 0.1f)) sonido.sfx("sfx_clave_magia", 1.0f, pan);
				break;
			case entrada::FUSION:
				if (!e.trasMD && cruza(tf - 0.5f)) sonido.sfx("sfx_clave_fusion", 1.0f, pan);
				break;
			case entrada::RITUAL:
				if (!e.trasMD && cruza(tf - 0.5f)) sonido.sfx("sfx_clave_ritual", 1.0f, pan);
				break;
			case entrada::DIOS:
				if (cruza(tf + 0.1f)) sonido.sfx("sfx_clave_trueno", 1.0f, pan);
				if (cruza(tf + 1.2f)) sonido.sfx("sfx_clave_trueno", 0.7f, -pan);
				break;
			default: break;
			}
		}
		if (e.t >= e.dur) {
			e.activa = false;
			fprintf(stderr, "[entrada] %s: fin\n", e.carta.c_str());
		}
	}

	// el alto de la figura de la entrada (m) o el de un monstruo cualquiera
	float altoEntradaClave() const
	{
		const int f = figuraDe(entrada::estado().carta);
		return f >= 0 ? figuras[(size_t)f].alto : 2.6f;
	}

	// updateUniformBuffers (despues del selector de camara): la camara se acerca a la casilla, orbita un poco y, con un
	// Dios Egipcio, tiembla. Fuera del duelo se olvidan las cartas que ya hicieron su entrada (el duelo siguiente
	// empieza de cero)
	void camaraEntradaClave(glm::vec3& ojo, glm::vec3& mira)
	{
		entrada::Estado& e = entrada::estado();
		if (ctl.vista.pantalla != VD::DUELO) {
			e.activa = false;
			e.vistas.clear();
			olvidarEntradaMD();   // (rama classic-md-entradas)
			return;
		}
		if (!e.activa || e.zona < 0) return;
		efectosEntradaClave();
		if (!e.con3D) return;
		const float te = e.t;
		float w;
		if (e.corta) {
			w = 0.35f * suave(0.0f, 0.25f, te) * (1.0f - suave(e.dur - 0.3f, e.dur, te));
		} else {
			w = (0.25f * suave(0.0f, e.tFigura, te) + 0.75f * suave(e.tFigura - 0.2f, e.tFigura + 0.5f, te)) * (1.0f - suave(e.dur - 0.55f, e.dur, te));
		}
		const bool dios = e.clase == entrada::DIOS;
		const float alto = altoEntradaClave();
		const glm::vec3 foco = baseZona(e.zona) + glm::vec3(0.0f, alto * (dios ? 0.55f : 0.5f), 0.0f);
		glm::vec3 d = ojo - foco;
		d.y = 0.0f;
		d = glm::length(d) > 1e-3f ? glm::normalize(d) : glm::vec3(0.0f, 0.0f, 1.0f);
		// orbita de cine: gira despacio alrededor de la figura mientras dura la entrada
		const float orb = glm::radians(14.0f) * (suave(e.tFigura, e.dur, te) - 0.5f) * (e.rival ? -1.0f : 1.0f);
		d = glm::vec3(d.x * cosf(orb) + d.z * sinf(orb), 0.0f, -d.x * sinf(orb) + d.z * cosf(orb));
		const float elev = glm::radians(dios ? 9.0f : 17.0f);
		const float dist = alto * (dios ? 1.9f : 1.7f) + 3.2f;
		const glm::vec3 cerca = foco + (d * cosf(elev) + glm::vec3(0.0f, sinf(elev), 0.0f)) * dist;
		ojo = glm::mix(ojo, cerca, w);
		mira = glm::mix(mira, foco, w);
		// temblor: los Dioses Egipcios (rayos) y el golpe al caer de las rigidas grandes
		if (!e.corta) {
			const float u = te - e.tFigura;
			float amp = 0.0f;
			if (dios) amp = 0.20f * suave(0.0f, 0.2f, u) * (1.0f - suave(1.6f, 2.4f, u));
			else if (alto > 3.0f) amp = 0.10f * expf(-glm::max(u - 0.5f, 0.0f) * 9.0f) * suave(0.45f, 0.5f, u);
			ojo += glm::vec3(sinf(te * 61.0f), cosf(te * 47.0f), sinf(te * 37.0f)) * amp;
		}
	}

	// los efectos horneados de la entrada (fx.h: Niagara y Master Duel), pedidos en cada cuadro
	void efectosEntradaClave()
	{
		using fx::Fx;
		const entrada::Estado& e = entrada::estado();
		if (!flipbooks.listo || !e.con3D) return;
		const glm::vec3 arriba(0.0f, 1.0f, 0.0f), b = baseZona(e.zona);
		const entrada::Color c0 = entrada::colorAtributo(e.atributo);
		const glm::vec3 col(c0.r, c0.g, c0.b);
		const int fi = figuraDe(e.carta);
		const bool rigida = fi < 0 || !figuras[(size_t)fi].esqueleto;
		const float alto = altoEntradaClave();
		if (e.corta) {
			flipbooks.pedir(Fx::Aparicion, b + arriba * 0.06f, 2.6f, e.t, glm::vec4(col * 1.4f, 1.0f));
			flipbooks.pedir(Fx::Chispas, b + arriba * (alto * 0.4f), 2.2f, e.t - 0.1f, glm::vec4(col * 1.3f, 1.0f));
			return;
		}
		const float u = e.t - e.tFigura;   // edad de la figura
		const float sale = 1.0f - suave(e.dur - e.tFigura - 0.45f, e.dur - e.tFigura, u);
		// durante el corte, la casilla se carga de energia del color del atributo
		flipbooks.pedir(Fx::Aura, b, 2.8f, e.t, glm::vec4(col * 1.2f, 0.85f * sale));
		switch (e.trasMD ? entrada::NORMAL : e.clase) {   // (fusion y ritual: el vortice / circulo se abre antes de la figura; tras la de MD, no)
		case entrada::FUSION:
			flipbooks.pedir(Fx::MdFusion, b + arriba * 0.2f, 5.2f, u + 0.6f, glm::vec4(1.0f, 1.0f, 1.0f, sale));
			flipbooks.pedir(Fx::MdFusionMarco, b + arriba * 0.1f, 4.2f, fmodf(glm::max(u + 0.6f, 0.0f), 0.4f),
			                glm::vec4(1.0f, 1.0f, 1.0f, sale * suave(-0.6f, -0.5f, u) * (1.0f - suave(1.0f, 1.6f, u))));
			break;
		case entrada::RITUAL:
			flipbooks.pedir(Fx::MdRitual, b, 5.4f, u + 0.6f, glm::vec4(1.0f, 1.1f, 1.3f, sale));
			flipbooks.pedir(Fx::Marca, b + arriba * 0.04f, 4.6f, u + 0.6f, glm::vec4(0.5f, 0.85f, 1.7f, sale));
			break;
		default: break;
		}
		if (u < 0.0f) return;
		flipbooks.pedir(Fx::TeleportIn, b, 3.4f, u, glm::vec4(col * 1.3f, 1.0f));
		flipbooks.pedir(Fx::Aparicion, b + arriba * 0.06f, 3.0f, u, glm::vec4(col * 1.3f, 1.0f));
		if (rigida) flipbooks.pedir(Fx::Polvo, b + arriba * 0.05f, 2.4f + 0.4f * alto, u - 0.5f, glm::vec4(1.0f, 1.0f, 1.0f, 0.85f));   // cae a la mesa
		else flipbooks.pedir(Fx::Chispas, b + arriba * (alto * 0.5f), 2.6f, u - 0.3f, glm::vec4(col * 1.4f, 1.0f));
		switch (e.clase) {
		case entrada::DRAGON: {
			// rugido y llamarada hacia el cielo
			const float llama = suave(0.55f, 0.7f, u) * (1.0f - suave(1.5f, 2.1f, u));
			flipbooks.pedir(Fx::BolaFuego, b + arriba * (alto * 0.8f), 3.2f, u - 0.5f, glm::vec4(1.3f, 1.0f, 0.8f, 1.0f));
			flipbooks.pedir(Fx::Fuego, b + arriba * (alto * 0.6f), 3.0f, u - 0.55f, glm::vec4(1.4f, 1.1f, 0.8f, llama), arriba);
			flipbooks.pedir(Fx::Fuego, b + arriba * (alto * 0.95f), 2.4f, u - 0.75f, glm::vec4(1.4f, 1.0f, 0.7f, llama * 0.8f), arriba);
			break;
		}
		case entrada::MAGO:
			// circulo magico en la mesa y la carga en lo alto
			flipbooks.pedir(Fx::Marca, b + arriba * 0.04f, 5.0f, u, glm::vec4(col * 1.6f, sale));
			flipbooks.pedir(Fx::MdActivar, b + arriba * 0.15f, 4.4f, u - 0.1f, glm::vec4(col * 1.5f, sale));
			flipbooks.pedir(Fx::Electricidad, b + arriba * (alto * 0.85f), 1.8f, u - 0.4f, glm::vec4(col * 1.3f, sale));
			break;
		case entrada::DIOS: {
			// rayos alrededor, la descarga en lo alto y el aura dorada
			for (int k = 0; k < 6; k++) {
				const float a = (float)k * 1.047f + 0.4f;
				const glm::vec3 p = b + glm::vec3(cosf(a), 0.0f, sinf(a)) * (2.2f + 0.8f * (float)(k & 1));
				flipbooks.pedir(Fx::Tesla, p + arriba * 0.1f, 4.6f, u - 0.12f - 0.2f * (float)k, glm::vec4(1.6f, 1.4f, 0.8f, sale), arriba);
			}
			flipbooks.pedir(Fx::Electricidad, b + arriba * (alto * 0.9f), 3.4f, u - 0.2f, glm::vec4(1.5f, 1.3f, 0.7f, sale));
			flipbooks.pedir(Fx::ExplosionMediana, b + arriba * 0.5f, 4.2f, u - 0.05f, glm::vec4(1.4f, 1.2f, 0.8f, 1.0f));
			flipbooks.pedir(Fx::Aura, b, 4.4f, u, glm::vec4(1.7f, 1.35f, 0.6f, sale));
			break;
		}
		default: break;
		}
	}

	// matrizFigura (figuras.h): la entrada de la figura. Rigida: crece con rebote, gira y cae; con esqueleto, un poco de
	// crecimiento (el clip de ataque lo pone clipEntradaClave). Antes de terminar el corte, oculta
	void poseEntradaClave(int z, const Figura& f, float& giro, float& escala, float& vuelo, float& holo)
	{
		const entrada::Estado& e = entrada::estado();
		if (!e.activa || !e.con3D || e.zona != z || hayAtaque()) return;
		const float u = e.t - e.tFigura;
		if (u < 0.0f) {
			escala *= 0.01f;
			return;
		}
		// (rama classic-bp-terminar: el giro de la entrada se apaga y la figura queda mirando a la pantalla; orientacion_figura.h)
		const bool dios = e.clase == entrada::DIOS;
		giro += orientacion::giroEntrada(e.corta, f.esqueleto, dios, u);
		if (e.corta) {
			const float k = suave(0.0f, 0.7f, u);
			escala *= 0.7f + 0.3f * entrada::rebote(k);
			holo += 0.8f * (1.0f - k);
			return;
		}
		const float k = suave(0.0f, dios ? 1.5f : 0.9f, u);
		if (!f.esqueleto) {
			escala *= 0.25f + 0.75f * entrada::rebote(suave(0.0f, dios ? 1.2f : 0.75f, u));
			vuelo += (1.0f - suave(0.0f, 0.5f, u)) * (dios ? 0.0f : 0.35f * f.alto);
		} else {
			escala *= 0.6f + 0.4f * entrada::rebote(suave(0.0f, 0.6f, u));
		}
		holo += 1.3f * (1.0f - k);
	}

	// posarFiguras (figuras.h): con esqueleto, la entrada es su clip de ataque (sin clip: el reposo de siempre)
	void clipEntradaClave(int z, Figura& f)
	{
		const entrada::Estado& e = entrada::estado();
		if (!e.activa || !e.con3D || e.corta || e.zona != z || f.ataque < 0 || hayAtaque()) return;
		const float u = e.t - e.tFigura - 0.25f;
		if (u < 0.0f) return;
		const vkglTF::Animation& a = f.modelo.animations[(size_t)f.ataque];
		const float largo = a.end - a.start;
		clipEncima(f, f.ataque, u, suave(0.0f, 0.2f, u) * (1.0f - suave(largo - 0.25f, largo, u)));
	}
