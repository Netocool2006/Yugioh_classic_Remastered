// Yugioh Classic Remaster (rama classic-integracion): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
// Lo que se ve del duelo real fuera de la mesa (control::VistaDuelo, lo llena duelo_juego.h):
//  - ELEGIR_RIVAL: nivel (1..8, hasta el desbloqueado), el rival que toca con su lema y personalidad, y los botones.
//  - DUELO: cartel del turno del rival y "pensando..." mientras la IA decide.
//  - RESULTADO: victoria o derrota, nivel desbloqueado y continuar.
// Todos los textos por idioma.h (la tabla) o por los dos idiomas de rivales.txt / ia::textoIA.

public:
	// nombre visible de una carta en el idioma actual (para textos armados fuera de la interfaz)
	std::string nombreCarta(const std::string& id) const
	{
		const int n = buscarCarta(id);
		return n >= 0 ? cartas[n].vNombre() : id;
	}

private:
	void dibujarPantallaDuelo()
	{
		if (!ctl) return;
		if (historia_dialogos::escena().activa() || tutorial::estado().abierto) return;   // rama classic-historia: la escena o las reglas tapan la pantalla
		const control::VistaDuelo& vd = ctl->vista;
		const bool en = idioma::ingles();
		const float a = suave(0.0f, 0.25f, vd.t);
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * a); };
		const float cx = (CAMPO_X * s + W) * 0.5f;
		if (vd.pantalla == control::VistaDuelo::DUELO && vd.sorteo) {   // rama classic-moneda: el lanzamiento de moneda
			dibujarMoneda(vd);
			return;
		}
		if (vd.pantalla == control::VistaDuelo::DUELO) {
			dibujarRetratoRival(vd);   // rama classic-personajes (personajes_ui.h)
			const float aRival = alfaTurnoRivalMD() * (1.0f - muestraCPUActiva());   // rama classic-md-fases: entra con fundido tras el cartel de turno de MD (classic-ps5-cpu-joey: se aparta mientras el rival ensena su carta)
			if (!vd.turnoRival || aRival < 0.01f) return;
			// cartel arriba: TURNO DEL RIVAL y, mientras decide, "pensando..." con tres puntos que laten
			const float w = 520.0f * s, h = 64.0f * s, y = 150.0f * s;
			usar(setAtlas, false);
			caja(cx - w * 0.5f, y, w, h, 18.0f * s, PANEL, glm::vec4(glm::vec3(VIDRIO), 0.8f * aRival), glm::vec4(glm::vec3(CARMESI), aRival), h * 0.5f,
			     glm::vec4(1.4f * s, 1.0f, 0.2f, 14.0f * s));
			texto(F_NUMEROS, ia::textoIA("ia.turno_rival", en), cx, y + 30.0f * s, 24.0f * s, glm::vec4(glm::vec3(BLANCO), aRival), glm::vec4(glm::vec3(CARMESI), aRival),
			      glm::vec3(0.4f, 0.0f, 0.6f), 1, 3.0f);
			if (vd.pensando) {
				const float lat = 0.6f + 0.4f * sinf(t * 6.0f);
				texto(F_TITULO, ia::textoIA("ia.pensando", en), cx, y + 54.0f * s, 17.0f * s, glm::vec4(glm::vec3(GRIS), lat), NADA, glm::vec3(0.0f), 1, 1.0f);
			}
			return;
		}
		if (vd.pantalla != control::VistaDuelo::ELEGIR_RIVAL && vd.pantalla != control::VistaDuelo::RESULTADO) return;
		if (vd.pantalla == control::VistaDuelo::RESULTADO && resultado::resumen().activo) {   // rama classic-resultado (resultado_ui.h)
			dibujarResultado(vd);
			return;
		}
		if (vd.pantalla == control::VistaDuelo::ELEGIR_RIVAL && vd.modo == control::VistaDuelo::HISTORIA && vd.sagas > 0) {
			dibujarHistoria(vd, a);   // rama classic-personajes: modo historia (personajes_ui.h)
			return;
		}
		// velo sobre el tablero
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, A(glm::vec4(0.01f, 0.015f, 0.03f, 0.82f)), NADA, 0.0f, glm::vec4(0.0f));
		const float pw = 900.0f * s, ph = 560.0f * s;
		const float px = cx - pw * 0.5f, py = H * 0.5f - ph * 0.5f + (1.0f - a) * 20.0f * s;
		const glm::vec4 acento = vd.pantalla == control::VistaDuelo::RESULTADO ? (vd.ganador == 0 ? ORO : CARMESI) : CIAN;
		rect(glm::vec2(cx, H * 0.5f), glm::vec2(pw * 1.3f, ph * 1.3f), 0.0f, 0.0f, PARTICULA, A(glm::vec4(glm::vec3(acento) * 0.12f, 1.0f)), NADA, 0.0f, glm::vec4(0.0f));
		caja(px, py, pw, ph, 30.0f * s, PANEL, A(VIDRIO), A(acento), 22.0f * s, glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s));
		float bx = px + 40.0f * s;
		const float by = py + ph - 40.0f * s;
		auto boton = [&](const std::string& b, const char* txt) {
			bx += iconoBoton(glm::vec2(bx + 14.0f * s, by), b, 14.0f * s, a) + 12.0f * s;
			bx += texto(F_TITULO, idioma::tr(txt), bx, by + 8.0f * s, 21.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f) + 34.0f * s;
		};
		rz.tapar();   // rama classic-pc: estas pantallas tapan la mesa
		if (vd.pantalla == control::VistaDuelo::ELEGIR_RIVAL) {
			texto(F_NUMEROS, idioma::tr("ELIGE A TU RIVAL"), cx, py + 70.0f * s, 38.0f * s, A(BLANCO), A(CIAN), glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
			// niveles: 8 orbes (los bloqueados apagados), el elegido grande
			const float ox = cx - 3.5f * 70.0f * s, oy = py + 150.0f * s;
			for (int n = 1; n <= vd.niveles; n++) {
				const bool libre = n <= vd.nivelMax, sel = n == vd.nivel;
				const glm::vec2 c(ox + (float)(n - 1) * 70.0f * s, oy);
				{   // rama classic-pc: el orbe del nivel (arriba / abajo cambian el nivel)
					raton::Zona z;
					z.e[0] = raton::ejeLista(n, vd.nivel, control::B_ARRIBA, control::B_ABAJO);
					zRect(c.x - 32.0f * s, c.y - 32.0f * s, 64.0f * s, 64.0f * s, z);
				}
				const glm::vec4 col = sel ? ORO : libre ? CIAN : glm::vec4(0.3f, 0.34f, 0.42f, 1.0f);
				rect(c, glm::vec2((sel ? 26.0f : 18.0f) * s), 0.0f, 6.0f * s, ORBE, A(col), A(glm::vec4(glm::vec3(col) * 0.4f, 1.0f)), 0.0f, glm::vec4(0.9f, 1.0f, 0.0f, 0.0f));
				texto(F_NUMEROS, std::to_string(n), c.x, c.y + 9.0f * s, (sel ? 24.0f : 18.0f) * s, A(sel ? glm::vec4(0.1f, 0.06f, 0.0f, 1.0f) : glm::vec4(0.05f, 0.08f, 0.12f, 1.0f)),
				      NADA, glm::vec3(0.0f), 1, 0.0f);
			}
			texto(F_TITULO, std::string(ia::textoIA("ia.nivel", en)) + " " + std::to_string(vd.nivel) + " / " + std::to_string(vd.niveles), cx, oy + 62.0f * s, 22.0f * s,
			      A(GRIS), NADA, glm::vec3(0.0f), 1, 2.0f);
			const std::string& nom = en && !vd.rivalEn.empty() ? vd.rivalEn : vd.rivalEs;
			const std::string& lema = en && !vd.lemaEn.empty() ? vd.lemaEn : vd.lemaEs;
			texto(F_NUMEROS, nom.empty() ? std::string(idioma::tr("Sin rivales (falta rivales.txt)")) : nom, cx, oy + 140.0f * s, 46.0f * s, A(ORO), A(glm::vec4(0.5f, 0.3f, 0.05f, 1.0f)),
			      glm::vec3(0.6f, 0.0f, 0.8f), 1, 1.5f);
			if (!lema.empty()) texto(F_TEXTO, "\"" + lema + "\"", cx, oy + 186.0f * s, 22.0f * s, A(BLANCO), NADA, glm::vec3(0.0f), 1, 0.0f);
			if (!vd.personalidad.empty()) {
				const std::string clave = "ia.personalidad." + vd.personalidad;
				texto(F_TITULO, ia::textoIA(clave.c_str(), en), cx, oy + 226.0f * s, 20.0f * s, A(CIAN), NADA, glm::vec3(0.0f), 1, 2.0f);
			}
			texto(F_TITULO, std::string(idioma::tr("Victorias")) + " " + std::to_string(vd.victorias) + "   ·   " + idioma::tr("Derrotas") + " " + std::to_string(vd.derrotas),
			      cx, oy + 270.0f * s, 18.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
			// rama classic-menus: el rival del nivel (izq/der) y su posicion entre los del nivel
			if (vd.rivalesNivel > 1)
				texto(F_TITULO, "<  " + std::to_string(vd.rivalIdx + 1) + " / " + std::to_string(vd.rivalesNivel) + "  >", cx, oy + 100.0f * s, 22.0f * s, A(CIAN), NADA,
				      glm::vec3(0.0f), 1, 1.0f);
			// rama classic-pc: el rival del nivel con el raton (mitad izquierda / derecha de "< n / m >") y el nombre = empezar el duelo
			zBoton(cx - 250.0f * s, oy + 110.0f * s, 500.0f * s, 170.0f * s, control::B_X);
			if (vd.rivalesNivel > 1) {
				zBoton(cx - 120.0f * s, oy + 78.0f * s, 120.0f * s, 40.0f * s, control::B_IZQ);
				zBoton(cx, oy + 78.0f * s, 120.0f * s, 40.0f * s, control::B_DER);
			}
			boton("CRUZ", "Nivel / rival");
			boton("X", "Duelo");
			boton("C", "Otro rival");
			if (vd.sagas > 0) boton("T", "Modo historia");   // (rama classic-personajes)
			boton(idioma::tr("TÁCTIL"), "Construir mazo");
			boton("O", "Volver");
			return;
		}
		// resultado
		zBoton(px, py, pw, ph, control::B_X);   // rama classic-pc: un clic en el panel = continuar
		const bool gano = vd.ganador == 0;
		const float pulso = 1.0f + 0.04f * sinf(t * 3.0f);
		texto(F_NUMEROS, idioma::tr(gano ? "¡VICTORIA!" : vd.ganador == 1 ? "DERROTA" : "EMPATE"), cx, py + 190.0f * s, 82.0f * s * pulso, A(BLANCO), A(acento),
		      glm::vec3(0.8f, 0.0f, 1.0f), 1, 3.0f);
		const std::string& nom = en && !vd.rivalEn.empty() ? vd.rivalEn : vd.rivalEs;
		texto(F_TITULO, std::string(idioma::tr("Rival")) + ": " + nom + "  ·  " + ia::textoIA("ia.nivel", en) + " " + std::to_string(vd.nivel), cx, py + 260.0f * s, 24.0f * s,
		      A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
		if (gano && vd.personajeNuevo) {
			dibujarResultadoPersonaje(vd, cx, py + 330.0f * s, a);   // rama classic-personajes
		} else if (gano && vd.subeNivel) {
			texto(F_NUMEROS, std::string(idioma::tr("NIVEL DESBLOQUEADO")) + ": " + std::to_string(vd.nivelMax), cx, py + 340.0f * s, 30.0f * s, A(ORO), A(glm::vec4(0.5f, 0.3f, 0.05f, 1.0f)),
			      glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
		} else if (gano && vd.nivelMax >= vd.niveles && vd.nivel >= vd.niveles) {
			texto(F_NUMEROS, idioma::tr("¡ERES EL CAMPEÓN!"), cx, py + 340.0f * s, 30.0f * s, A(ORO), NADA, glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
		}
		texto(F_TITULO, std::string(idioma::tr("Victorias")) + " " + std::to_string(vd.victorias) + "   ·   " + idioma::tr("Derrotas") + " " + std::to_string(vd.derrotas), cx,
		      py + 400.0f * s, 18.0f * s, A(GRIS), NADA, glm::vec3(0.0f), 1, 1.0f);
		boton("X", "Continuar");
		boton("O", "Menú principal");   // rama classic-menus
	}

#include "moneda_ui.h"   // rama classic-moneda: el lanzamiento de moneda antes del duelo (antes, piedra, papel o tijera)

	// rama classic-arreglos: el menu de pausa (OPTIONS corto; lo lleva duelo_juego.h en ctl->vista): velo, panel al
	// centro, las opciones (la elegida en dorado) y la ayuda (X elegir, O / OPTIONS volver)
	// rama classic-md-entradas: abre y cierra como el menu del duelo de Master Duel (Push / Pop de SettingMenuUI), con su
	// cursor que late y sus esquinas (menu_md_ui.h). El contenido es el de siempre
	void dibujarPausa()
	{
		if (!avanzarMenuMD()) return;
		const AnimMenuMD an = animMenuMD();
		const std::vector<std::string>& ops = menuAbiertoMD ? ctl->vista.pausaOpciones : opcionesCopiaMD;
		const int opcion = menuAbiertoMD ? ctl->vista.pausaOpcion : opcionCopiaMD;
		const int n = (int)ops.size();
		const float e = an.escala;
		const float fila = 58.0f * s * e, w = 560.0f * s * e * an.ancho, cab = 96.0f * s * e, pie = 70.0f * s * e;
		const float h = cab + fila * (float)n + pie;
		const float cx = W * 0.5f + an.dx, x0 = cx - w * 0.5f, y0 = H * 0.5f - h * 0.5f;
		auto A = [&](glm::vec4 c) { return glm::vec4(glm::vec3(c), c.a * an.alfa); };
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.01f, 0.015f, 0.03f, 0.7f * an.velo), NADA, 0.0f, glm::vec4(0.0f));
		if (an.alfa <= 0.003f) return;
		caja(x0, y0, w, h, 30.0f * s * e, PANEL, A(VIDRIO), A(glm::mix(CIAN, ORO, 0.55f)), 22.0f * s * e, glm::vec4(1.6f * s, 1.1f, 0.12f, 22.0f * s * e));
		rz.tapar();   // rama classic-pc
		if (menuAbiertoMD && an.alfa > 0.6f) {
			for (int k = 0; k < n; k++) zLista(x0 + 24.0f * s * e, y0 + cab + fila * (float)k + 4.0f * s * e, w - 48.0f * s * e, fila - 8.0f * s * e, k, opcion);
		}
		texto(F_NUMEROS, idioma::tr("PAUSA"), cx, y0 + 62.0f * s * e + an.dyTitulo, 38.0f * s * e, A(BLANCO), A(CIAN), glm::vec3(0.5f, 0.0f, 0.6f), 1, 2.0f);
		for (int k = 0; k < n; k++) {
			const bool sel = k == opcion;
			const float fy = y0 + cab + fila * (float)k;
			if (sel) {
				cursorMenuMD(x0 + 24.0f * s * e, fy + 4.0f * s * e, w - 48.0f * s * e, fila - 8.0f * s * e, an.alfa);
				iconoBoton(glm::vec2(x0 + w - 52.0f * s * e, fy + fila * 0.5f), "X", 14.0f * s * e, an.alfa);
			}
			texto(F_TITULO, idioma::tr(ops[(size_t)k].c_str()), x0 + 54.0f * s * e, fy + fila * 0.5f + 9.0f * s * e, 27.0f * s * e,
			      A(sel ? glm::vec4(1.0f, 0.95f, 0.8f, 1.0f) : glm::vec4(0.86f, 0.9f, 0.97f, 1.0f)), NADA, glm::vec3(0.0f), 0, 1.0f);
		}
		float bx = x0 + 40.0f * s * e;
		const float by = y0 + h - pie * 0.5f;
		auto boton = [&](const std::string& b, const char* txt) {
			bx += iconoBoton(glm::vec2(bx + 14.0f * s * e, by), b, 14.0f * s * e, an.alfa) + 12.0f * s * e;
			bx += texto(F_TITULO, idioma::tr(txt), bx, by + 8.0f * s * e, 21.0f * s * e, A(BLANCO), NADA, glm::vec3(0.0f), 0, 1.0f) + 30.0f * s * e;
		};
		boton("CRUZ", "Opción");
		boton("X", "Elegir");
		boton("O", "Volver");
		boton("OPTIONS", "Volver");
	}
