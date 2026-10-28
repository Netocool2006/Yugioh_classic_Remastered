// Yugioh Classic Remaster (rama classic-constructor): se incluye DENTRO de interfaz::Interfaz (usa sus primitivas).
//  1) La cache de caras: con 1690 cartas no se cargan todas al arrancar. Quien dibuja una carta pregunta
//     listaCarta / miniLista / caraLista: eso la marca como usada en este cuadro y, si falta, la pide; procesarCargas
//     (al empezar cada cuadro, fuera del dibujo) carga unas pocas por cuadro y descarga las que hace mas que no se
//     usan cuando se pasa del tope (MAX_CARAS / MAX_MINIS). Solo se descarga lo que no se dibujo en los ultimos
//     cuadros en vuelo (la GPU ya no lo usa); su descriptor queda y se reescribe al volver a cargarla.
//  2) La pantalla "Construir mazo" (constructor.h es la logica; rama classic-constructor2): MIS MAZOS y el editor
//     dividido (detalle, mazo, coleccion), los filtros, el teclado en pantalla, las preguntas y la barra de botones.

public:
	static constexpr int MAX_CARAS = 40, MAX_MINIS = 220;   // ~29 MB y ~41 MB de texturas como mucho
	static constexpr int CARGAS_POR_CUADRO = 8;
	static constexpr float MS_POR_CUADRO = 7.0f;

	// lista para dibujar (cara y mini); si no, la pide
	bool listaCarta(int n) const
	{
		if (n < 0 || n >= (int)cartas.size()) return false;
		const bool c = caraLista(n), m = miniLista(n);
		return c && m;
	}
	bool miniLista(int n) const
	{
		if (n < 0 || n >= (int)cartas.size()) return false;
		CartaUI& k = cartas[n];
		k.usoMini = cuadroN;
		if (!k.tieneMini && !k.falloMini) k.quiereMini = true;
		return k.tieneMini;
	}
	bool caraLista(int n) const
	{
		if (n < 0 || n >= (int)cartas.size()) return false;
		CartaUI& k = cartas[n];
		k.usoCara = cuadroN;
		if (!k.tieneCara && !k.falloCara) k.quiereCara = true;
		return k.tieneCara;
	}

private:
	void cargarImagenCarta(int n, bool cara)
	{
		CartaUI& k = cartas[n];
		bool& tiene = cara ? k.tieneCara : k.tieneMini;
		bool& fallo = cara ? k.falloCara : k.falloMini;
		(cara ? k.quiereCara : k.quiereMini) = false;
		if (tiene || fallo) return;
		vks::Texture2D& tex = cara ? k.cara : k.mini;
		// en ingles cara_en_<id> / mini_en_<id>; si falta, la espanola
		const std::string base = rutaAssets + (cara ? "ui/cara" : "ui/mini");
		const std::string suf = constructor::sufijoIdioma();
		bool ok = false;
		if (!suf.empty()) {
			struct stat st{};
			const std::string en = base + suf + "_" + k.id + ".png";
			ok = stat(en.c_str(), &st) == 0 && (cara ? cargarConMips(en, tex, true) : cargarImagen(en, tex));
		}
		// la cara con mipmaps (classic-base: nitida chica e inclinada en el tablero, con el filtro anisotropo)
		if (!ok && !(cara ? cargarConMips(base + "_" + k.id + ".png", tex, true) : cargarImagen(base + "_" + k.id + ".png", tex))) {
			fallo = true;   // (sin imagen: el dorso; no se reintenta)
			return;
		}
		VkDescriptorSet& set = cara ? k.setCara : k.setMini;
		if (set == VK_NULL_HANDLE) {
			set = nuevoSet(tex);
		} else {
			VkDescriptorImageInfo img{ muestreador, tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkWriteDescriptorSet w = vks::initializers::writeDescriptorSet(set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &img);
			vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
		}
		tiene = true;
		k.lista = k.tieneCara && k.tieneMini;
	}

	int versionCaras{ 0 };   // versionIdioma() de las caras cargadas

	void procesarCargas()
	{
		cuadroN++;
		if (versionCaras != constructor::versionIdioma()) {
			// cambio el idioma: fuera todas las caras (se vuelven a pedir con el sufijo nuevo)
			versionCaras = constructor::versionIdioma();
			vkDeviceWaitIdle(dev);
			for (CartaUI& k : cartas) {
				if (k.tieneCara) k.cara.destroy();
				if (k.tieneMini) k.mini.destroy();
				k.tieneCara = k.tieneMini = k.lista = false;
				k.falloCara = k.falloMini = false;
			}
			for (CaraHD& h : hd) {
				if (h.carta >= 0) h.tex.destroy();
				h.carta = -1;
			}
			hdFalta.clear();
		}
		cargarHD();
		const auto t0 = std::chrono::steady_clock::now();
		int hechas = 0;
		// primero las minis (la rejilla), despues las caras; solo lo pedido en el ultimo cuadro
		for (int pasada = 0; pasada < 2; pasada++) {
			const bool cara = pasada == 1;
			for (int n = 0; n < (int)cartas.size(); n++) {
				CartaUI& k = cartas[n];
				const bool quiere = cara ? k.quiereCara : k.quiereMini;
				if (!quiere) continue;
				if ((cara ? k.usoCara : k.usoMini) + 2 < cuadroN) {   // ya no se dibuja: se olvida el pedido
					(cara ? k.quiereCara : k.quiereMini) = false;
					continue;
				}
				if (hechas >= CARGAS_POR_CUADRO ||
				    std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - t0).count() > MS_POR_CUADRO) {
					break;
				}
				cargarImagenCarta(n, cara);
				hechas++;
			}
		}
		expulsar(true, MAX_CARAS);
		expulsar(false, MAX_MINIS);
	}

	void expulsar(bool cara, int maximo)
	{
		int cargadas = 0;
		for (const CartaUI& k : cartas) cargadas += (cara ? k.tieneCara : k.tieneMini) ? 1 : 0;
		while (cargadas > maximo) {
			int peor = -1;
			for (int n = 0; n < (int)cartas.size(); n++) {
				const CartaUI& k = cartas[n];
				if (!(cara ? k.tieneCara : k.tieneMini)) continue;
				const uint64_t uso = cara ? k.usoCara : k.usoMini;
				if (uso + (uint64_t)maxConcurrentFrames + 2 >= cuadroN) continue;   // (la GPU puede estar usandola)
				if (peor < 0 || uso < (cara ? cartas[peor].usoCara : cartas[peor].usoMini)) peor = n;
			}
			if (peor < 0) return;
			CartaUI& k = cartas[peor];
			(cara ? k.cara : k.mini).destroy();
			(cara ? k.tieneCara : k.tieneMini) = false;
			k.lista = false;
			cargadas--;
		}
	}

	// ---- caras HD de la carta grande del constructor (el juego sera 4K): cara_hd_<id>.jpg (ingles cara_hd_en_<id>.jpg;
	// si falta, la espanola; si no hay HD, la cara_ normal). Cache propia de MAX_HD; un descriptor fijo por casillero
	static constexpr int MAX_HD = 6;
	struct CaraHD { int carta{ -1 }; vks::Texture2D tex; VkDescriptorSet set{ VK_NULL_HANDLE }; uint64_t uso{ 0 }; };
	std::array<CaraHD, MAX_HD> hd{};
	mutable int hdPedida{ -1 };
	std::unordered_map<int, bool> hdFalta;   // cartas sin HD (no se reintenta)

	const CaraHD* caraHD(int n) const
	{
		for (const CaraHD& h : hd) {
			if (h.carta == n) {
				const_cast<CaraHD&>(h).uso = cuadroN;
				return &h;
			}
		}
		if (!hdFalta.count(n)) hdPedida = n;
		return nullptr;
	}
	void cargarHD()
	{
		const int n = hdPedida;
		hdPedida = -1;
		if (n < 0 || n >= (int)cartas.size() || hdFalta.count(n)) return;
		int lugar = -1;
		for (int i = 0; i < MAX_HD; i++) {
			if (hd[i].carta == n) return;
			if (hd[i].carta >= 0 && hd[i].uso + (uint64_t)maxConcurrentFrames + 2 >= cuadroN) continue;   // (en uso por la GPU)
			if (lugar < 0 || hd[i].carta < 0 || (hd[lugar].carta >= 0 && hd[i].uso < hd[lugar].uso)) lugar = i;
		}
		if (lugar < 0) return;
		const std::string base = rutaAssets + "ui/cara_hd", suf = constructor::sufijoIdioma();
		std::string ruta;
		struct stat st{};
		for (const std::string& r : { base + suf + "_" + cartas[n].id + ".jpg", base + "_" + cartas[n].id + ".jpg" }) {
			if (stat(r.c_str(), &st) == 0) {
				ruta = r;
				break;
			}
		}
		if (ruta.empty()) {
			hdFalta[n] = true;
			return;
		}
		CaraHD& h = hd[lugar];
		if (h.carta >= 0) h.tex.destroy();
		h.carta = -1;
		if (!cargarConMips(ruta, h.tex, true)) {   // (con mipmaps: se ve a 4K inclinada)
			hdFalta[n] = true;
			return;
		}
		if (h.set == VK_NULL_HANDLE) {
			h.set = nuevoSet(h.tex);
		} else {
			VkDescriptorImageInfo img{ muestreador, h.tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkWriteDescriptorSet w = vks::initializers::writeDescriptorSet(h.set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &img);
			vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);
		}
		h.carta = n;
		h.uso = cuadroN;
	}

	// ------------------------------------------------------------------------------------------ Construir mazo
	// (rama classic-constructor2) MIS MAZOS y el EDITOR a pantalla dividida (detalle | mazo | coleccion), con las capas
	// de filtros, teclado en pantalla y preguntas. La logica esta en constructor.h.
	glm::vec4 colorClase(const std::string& c) const
	{
		if (c == "normal") return glm::vec4(0.95f, 0.80f, 0.45f, 1.0f);
		if (c == "efecto") return glm::vec4(1.0f, 0.55f, 0.30f, 1.0f);
		if (c == "ritual") return glm::vec4(0.40f, 0.62f, 1.0f, 1.0f);
		if (c == "fusion") return glm::vec4(0.75f, 0.48f, 1.0f, 1.0f);
		if (c == "magia") return glm::vec4(0.25f, 0.95f, 0.75f, 1.0f);
		if (c == "trampa") return glm::vec4(1.0f, 0.38f, 0.72f, 1.0f);
		return GRIS;
	}
	// texto del usuario o de las cartas: sin pasar por la tabla de traduccion (ya viene en su idioma)
	float textoCrudo(int fu, const std::string& txt, float x, float y, float tam, glm::vec4 col, int alin = 0)
	{
		const bool antes = crudo;
		crudo = true;
		const float w = texto(fu, txt, x, y, tam, col, NADA, glm::vec3(0.0f), alin, 0.0f);
		crudo = antes;
		return w;
	}
	float medirCrudo(int fu, const std::string& txt, float tam)
	{
		const bool antes = crudo;
		crudo = true;
		const float w = medir(fu, txt, tam);
		crudo = antes;
		return w;
	}
	// recorta con "…" para que entre en el ancho
	std::string recortar(int fu, std::string txt, float tam, float ancho)
	{
		if (medirCrudo(fu, txt, tam) <= ancho) return txt;
		while (!txt.empty() && medirCrudo(fu, txt + "…", tam) > ancho) constructor::borrarUltimo(txt);
		return txt + "…";
	}
	// rama classic-visual-final: el nombre bajo una carta de una rejilla (centrado en x, linea base y). Si no entra en una
	// linea, va en 2 lineas (achicando la letra hasta 72%); solo si ni asi entra, la 2a linea termina en "…"
	void nombreCelda(int fu, const std::string& txt, float x, float y, float tam, float ancho, glm::vec4 col)
	{
		if (medirCrudo(fu, txt, tam) <= ancho) {
			textoCrudo(fu, txt, x, y, tam, col, 1);
			return;
		}
		for (float t2 = tam * 0.92f;; t2 -= 0.5f * s) {
			std::vector<std::string> l = cortarLineasCrudo(fu, txt, t2, ancho);
			bool entra = l.size() <= 2;
			for (const std::string& li : l) entra = entra && medirCrudo(fu, li, t2) <= ancho;
			if (!entra && t2 > tam * 0.72f) continue;
			if (l.size() > 2) {
				for (size_t i = 2; i < l.size(); i++) l[1] += " " + l[i];
				l.resize(2);
			}
			const float y0 = y - (l.size() > 1 ? t2 * 0.22f : 0.0f);
			for (size_t i = 0; i < l.size(); i++) textoCrudo(fu, recortar(fu, l[i], t2, ancho), x, y0 + (float)i * t2 * 1.02f, t2, col, 1);
			return;
		}
	}
	// lineas de un parrafo cortado por palabras (para desplazar el texto de la carta)
	std::vector<std::string> cortarLineasCrudo(int fu, const std::string& txt, float tam, float ancho)
	{
		std::vector<std::string> r;
		std::string linea;
		size_t a = 0;
		while (a <= txt.size()) {
			size_t b = txt.find(' ', a);
			if (b == std::string::npos) b = txt.size();
			const std::string palabra = txt.substr(a, b - a);
			a = b + 1;
			const std::string prueba = linea.empty() ? palabra : linea + " " + palabra;
			if (!linea.empty() && medirCrudo(fu, prueba, tam) > ancho) {
				r.push_back(linea);
				linea = palabra;
			} else {
				linea = prueba;
			}
			if (b == txt.size()) break;
		}
		if (!linea.empty()) r.push_back(linea);
		return r;
	}
	// "Magia de campo", "Monstruo de efecto"... (en espanol; se traduce con T)
	static const char* nombreSubtipo(const std::string& st)
	{
		static const char* const T_[][2] = { { "magia_normal", "Magia normal" }, { "magia_campo", "Magia de campo" }, { "magia_equipo", "Magia de equipo" },
			                                 { "magia_continua", "Magia continua" }, { "magia_rapida", "Magia rápida" }, { "magia_ritual", "Magia de ritual" },
			                                 { "trampa_normal", "Trampa normal" }, { "trampa_continua", "Trampa continua" },
			                                 { "trampa_contraataque", "Trampa de contraataque" }, { "normal", "Monstruo normal" }, { "efecto", "Monstruo de efecto" },
			                                 { "fusion", "Monstruo de fusión" }, { "ritual", "Monstruo de ritual" }, { "volteo", "Monstruo de volteo" },
			                                 { "toon", "Monstruo toon" }, { "union", "Monstruo de unión" }, { "espiritu", "Monstruo espíritu" } };
		for (const auto& p : T_)
			if (st == p[0]) return p[1];
		return "";
	}

	// una carta chica (la mini) con su ficha de color si aun no llego; brillo < 1 = apagada
	void dibujarMiniCarta(const mazo::Carta& mc, glm::vec2 centro, glm::vec2 tam, float brillo, bool sel, float semilla)
	{
		const int ui = buscarCarta(mc.id);
		usar(setAtlas, false);
		rect(centro + glm::vec2(0.0f, 4.0f * s), tam, 0.0f, 10.0f * s, PANEL, glm::vec4(0, 0, 0, 0.55f), NADA, 5.0f * s, glm::vec4(0, 0, 0, 8.0f * s));
		if (ui >= 0 && miniLista(ui)) {
			usar(cartas[ui].setMini, false);
			const float u = 0.5f / cartas[ui].mini.width, vv = 0.5f / cartas[ui].mini.height;
			rect(centro, tam, 0.0f, 0.0f, IMAGEN, glm::vec4(glm::vec3(sel ? 1.1f : brillo), 1.0f), glm::vec4(0.6f, 0.8f, 1.0f, 1.0f), 4.0f * s,
			     glm::vec4(sel ? 0.45f : 0.0f, sel ? 1.0f : 0.0f, semilla, 0.0f), glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
		} else {
			const glm::vec4 cc = colorClase(mc.clase);
			caja(centro.x - tam.x * 0.5f, centro.y - tam.y * 0.5f, tam.x, tam.y, 0.0f, PANEL, glm::vec4(glm::vec3(cc) * 0.18f * brillo, 1.0f), cc, 4.0f * s,
			     glm::vec4(1.0f * s, 0.5f, 0.0f, 0.0f));
			if (tam.x > 90.0f * s) {
				const bool antes = crudo;
				crudo = true;
				parrafo(F_TITULO, mc.nombre, centro.x - tam.x * 0.5f + 8.0f * s, centro.y - 14.0f * s, 15.0f * s, tam.x - 16.0f * s, 17.0f * s, 3, BLANCO);
				crudo = antes;
			}
		}
		usar(setAtlas, false);
	}
	void candado(glm::vec2 c, float k, glm::vec4 col)
	{
		usar(setAtlas, false);
		rect(c + glm::vec2(0.0f, -10.0f * k), glm::vec2(20.0f * k), 0.0f, 0.0f, PANEL, NADA, col, 10.0f * k, glm::vec4(3.0f * k, 2.4f, 0.0f, 0.0f));
		caja(c.x - 16.0f * k, c.y - 4.0f * k, 32.0f * k, 25.0f * k, 3.0f * k, PANEL, glm::vec4(0.05f, 0.06f, 0.1f, 0.95f), col, 4.0f * k, glm::vec4(1.4f * k, 1.0f, 0.0f, 0.0f));
		rect(c + glm::vec2(0.0f, 8.0f * k), glm::vec2(5.0f * k, 8.0f * k), 0.0f, 0.0f, PANEL, col, NADA, 2.0f * k, glm::vec4(0.0f));
	}
	void etiqueta(float x, float y, const std::string& txt, float tam, glm::vec4 col)
	{
		const float w = medir(F_NUMEROS, txt, tam, 1.0f) + 12.0f * s;
		usar(setAtlas, false);
		caja(x, y, w, tam + 7.0f * s, 3.0f * s, PANEL, glm::vec4(0.05f, 0.03f, 0.05f, 0.92f), col, 5.0f * s, glm::vec4(1.0f * s, 1.0f, 0.0f, 0.0f));
		texto(F_NUMEROS, txt, x + w * 0.5f, y + tam + 1.0f * s, tam, col, NADA, glm::vec3(0.0f), 1, 1.0f);
	}
	void marcoCursor(glm::vec2 centro, glm::vec2 tam, float grosor)
	{
		const float pulso = 0.6f + 0.4f * sinf(t * 5.0f);
		usar(setAtlas, false);
		caja(centro.x - tam.x * 0.5f - 4.0f * s, centro.y - tam.y * 0.5f - 4.0f * s, tam.x + 8.0f * s, tam.y + 8.0f * s, 12.0f * s, PANEL, NADA, ORO, 8.0f * s,
		     glm::vec4(grosor * s, 1.0f + pulso, 0.6f, 0.0f));
	}
	void panel(float x, float y, float w, float h, glm::vec4 borde)
	{
		usar(setAtlas, false);
		caja(x, y, w, h, 18.0f * s, PANEL, VIDRIO, borde, 14.0f * s, glm::vec4(1.3f * s, 0.8f, 0.07f, 14.0f * s));
	}
	// barra de botones abajo: pares (boton, texto en espanol)
	void barraBotones(const std::vector<std::pair<const char*, const char*>>& b)
	{
		float xx = 44.0f * s;
		const float yy = 1046.0f * s, r = 13.0f * s;
		for (const auto& p : b) {
			xx += iconoBoton(glm::vec2(xx + r, yy), p.first, r) + 8.0f * s;
			xx += texto(F_TITULO, constructor::T(p.second), xx, yy + 7.0f * s, 19.0f * s, glm::vec4(0.88f, 0.92f, 0.98f, 1.0f), NADA, glm::vec3(0.0f), 0, 1.0f) + 26.0f * s;
		}
	}

	// rama classic-pc: una carta de las rejillas del editor como zona del raton. Con el foco en su area se llega con la cruceta
	// (columna con izq / der, fila con arriba / abajo); si el foco esta en la otra area, el primer paso es saltar a esta
	// (stick derecho: C_RS_*; "saltar" es el boton que lleva al area de esta zona)
	void zCeldaConstructor(glm::vec2 ce, glm::vec2 tam, int col, int fila, bool enArea, int curCol, int curFila, uint32_t saltar)
	{
		raton::Zona z;
		z.clic = control::B_X;   // agregar / quitar
		if (enArea) {
			z.e[0] = raton::ejeLista(col, curCol, control::B_DER, control::B_IZQ);
			z.e[1] = raton::ejeLista(fila, curFila, control::B_ABAJO, control::B_ARRIBA);
		} else {
			z.e[0] = raton::ejeLista(1, 0, saltar, saltar);
		}
		zRect(ce.x - tam.x * 0.5f, ce.y - tam.y * 0.5f, tam.x, tam.y, z);
	}

	void dibujarConstructor()
	{
		const constructor::Constructor& c = *cons;
		if (!c.col || !c.bib) return;
		rz.tapar();   // rama classic-pc: el constructor tapa el duelo; cada capa (filtros, teclado, pregunta) tapa a la de abajo
		usar(setAtlas, false);
		// fondo opaco (el duelo sigue debajo) con un velo de color
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.012f, 0.018f, 0.036f, 1.0f), NADA, 0.0f, glm::vec4(0.0f));
		rect(glm::vec2(W * 0.5f, H * 0.42f), glm::vec2(W * 1.1f, H * 0.9f), 0.0f, 0.0f, PARTICULA, glm::vec4(glm::vec3(CIAN) * 0.06f, 1.0f), NADA, 0.0f,
		     glm::vec4(0.0f));
		if (c.pantalla == constructor::P_LISTA || c.pantalla == constructor::P_TORNEO) dibujarListaMazos(c);   // (P_TORNEO: rama classic-torneo-extras)
		else dibujarEditor(c);
		if (c.capa == constructor::K_FILTROS || (c.capa == constructor::K_TECLADO && c.modoTeclado == constructor::T_BUSCAR)) dibujarFiltros(c);
		if (c.capa == constructor::K_TECLADO) dibujarTeclado(c);
		if (c.capa == constructor::K_PREGUNTA) dibujarPregunta(c);
		// aviso
		if (!c.aviso.empty() && c.tAviso < 3.0f) {
			const float a = suave(0.0f, 0.15f, c.tAviso) * (1.0f - suave(2.5f, 3.0f, c.tAviso));
			auto A = [&](glm::vec4 col) { return glm::vec4(glm::vec3(col), col.a * a); };
			const float tt = 21.0f * s, w = std::min(medirCrudo(F_TITULO, c.aviso, tt) + 60.0f * s, W - 80.0f * s), h = 42.0f * s;
			const float cx = W * 0.5f, y = 984.0f * s;
			usar(setAtlas, false);
			caja(cx - w * 0.5f, y, w, h, 16.0f * s, PANEL, A(glm::vec4(0.03f, 0.04f, 0.08f, 0.97f)), A(ORO), h * 0.5f, glm::vec4(1.2f * s, 1.0f, 0.2f, 12.0f * s));
			textoCrudo(F_TITULO, recortar(F_TITULO, c.aviso, tt, w - 40.0f * s), cx, y + h * 0.5f + 8.0f * s, tt, A(BLANCO), 1);
		}
	}

	// ---- MIS MAZOS
	void dibujarListaMazos(const constructor::Constructor& c)
	{
		using constructor::T;
		const mazo::Biblioteca& b = *c.bib;
		// (rama classic-torneo-extras) MAZOS DE TORNEO: la misma lista con los mazos de torneo, sin "+ Nuevo mazo" ni "EN USO"
		const bool tor = c.pantalla == constructor::P_TORNEO;
		const bool en = constructor::enIngles();
		const int cursorL = tor ? c.cursorTorneo : c.cursorLista;
		auto mazoFila = [&](int i) -> const mazo::Mazo& { return tor ? c.torneos[(size_t)i].mazo : b.mazos[(size_t)i]; };
		auto nombreFila = [&](int i) -> const std::string& { return tor ? c.torneos[(size_t)i].nombre(en) : b.mazos[(size_t)i].nombre; };
		texto(F_NUMEROS, T("CONSTRUIR MAZO"), 60.0f * s, 70.0f * s, 34.0f * s, BLANCO, CIAN, glm::vec3(0.5f, 0.0f, 0.6f), 0, 2.0f);
		if (tor)
			texto(F_TITULO, T("MAZOS DE TORNEO") + "  ·  " + std::to_string(c.torneos.size()), 60.0f * s, 104.0f * s, 20.0f * s, ORO, NADA, glm::vec3(0.0f), 0, 1.5f);
		else
			texto(F_TITULO, T("MIS MAZOS") + "  ·  " + std::to_string(b.mazos.size()) + " / " + std::to_string(mazo::MAX_MAZOS), 60.0f * s, 104.0f * s, 20.0f * s, GRIS,
			      NADA, glm::vec3(0.0f), 0, 1.5f);
		// lista
		const float lx = 60.0f * s, ly = 130.0f * s, lw = 820.0f * s, fila = 66.0f * s;
		const int n = tor ? (int)c.torneos.size() : (int)b.mazos.size() + 1, visibles = 12;
		const int desde = std::clamp(cursorL - visibles / 2, 0, std::max(0, n - visibles));
		panel(lx - 16.0f * s, ly - 14.0f * s, lw + 32.0f * s, fila * visibles + 28.0f * s, glm::vec4(glm::vec3(CIAN) * 0.6f, 1.0f));
		for (int i = desde; i < std::min(n, desde + visibles); i++) {
			const float y = ly + (float)(i - desde) * fila;
			const bool sel = i == cursorL;
			zLista(lx, y, lw, fila - 8.0f * s, i, cursorL);   // rama classic-pc
			usar(setAtlas, false);
			caja(lx, y, lw, fila - 8.0f * s, 6.0f * s, PANEL, sel ? glm::vec4(0.16f, 0.13f, 0.06f, 0.95f) : glm::vec4(0.04f, 0.06f, 0.11f, 0.9f),
			     sel ? ORO : glm::vec4(0.25f, 0.32f, 0.45f, 1.0f), 10.0f * s, glm::vec4((sel ? 2.0f : 1.0f) * s, sel ? 1.4f : 0.6f, 0.0f, 0.0f));
			const float base = y + (fila - 8.0f * s) * 0.5f + 8.0f * s;
			if (!tor && i == n - 1) {
				texto(F_TITULO, T("+ Nuevo mazo"), lx + 54.0f * s, base, 23.0f * s, sel ? ORO : CIAN, NADA, glm::vec3(0.0f), 0, 1.0f);
				continue;
			}
			const mazo::Mazo& m = mazoFila(i);
			std::string motivo;
			const bool ok = m.valido(*c.col, &motivo);
			if (!tor && i == b.activo) {   // (el mazo de los duelos: un orbe dorado y "EN USO")
				usar(setAtlas, false);
				rect(glm::vec2(lx + 28.0f * s, y + (fila - 8.0f * s) * 0.5f), glm::vec2(18.0f * s), 0.0f, 6.0f * s, ORBE, ORO, glm::vec4(1.0f, 0.95f, 0.7f, 1.0f), 0.0f, glm::vec4(0.3f, 1.0f, 0.0f, 0.0f));
				etiqueta(lx + 600.0f * s, y + (fila - 8.0f * s) * 0.5f - 11.0f * s, T("EN USO"), 14.0f * s, ORO);
			}
			textoCrudo(F_TITULO, recortar(F_TITULO, nombreFila(i), 23.0f * s, 400.0f * s), lx + 54.0f * s, base, 23.0f * s, sel ? BLANCO : glm::vec4(0.85f, 0.9f, 0.97f, 1.0f));
			textoCrudo(F_NUMEROS, std::to_string(m.principal.size()) + " + " + std::to_string(m.fusion.size()), lx + 500.0f * s, base, 19.0f * s, GRIS);
			const glm::vec4 verde(0.45f, 1.0f, 0.62f, 1.0f);
			texto(F_TITULO, ok ? T("VÁLIDO") : T("NO VÁLIDO"), lx + lw - 20.0f * s, base, 18.0f * s, ok ? verde : ORO, NADA, glm::vec3(0.0f), 2, 1.0f);
		}
		// vista previa del elegido
		const float px = 930.0f * s, py = 116.0f * s, pw = W - px - 50.0f * s, ph = 820.0f * s;
		panel(px, py, pw, ph, glm::vec4(glm::vec3(ORO) * 0.6f, 1.0f));
		if (tor ? cursorL < 0 || cursorL >= n : c.cursorLista >= (int)b.mazos.size()) {
			parrafo(F_TITULO, T("Crea un mazo vacío y ponle nombre con el teclado en pantalla. Se guarda cuando tenga entre 40 y 60 cartas."), px + 40.0f * s,
			        py + 70.0f * s, 24.0f * s, pw - 80.0f * s, 32.0f * s, 6, GRIS);
		} else {
			const mazo::Mazo& m = mazoFila(cursorL);
			textoCrudo(F_TITULO, recortar(F_TITULO, nombreFila(cursorL), 30.0f * s, pw - 80.0f * s), px + 36.0f * s, py + 52.0f * s, 30.0f * s, BLANCO);
			int mo = 0, mg = 0, tr = 0;
			for (const std::string& id : m.principal) {
				const mazo::Carta* k = c.col->carta(id);
				if (!k) continue;
				if (k->clase == "magia") mg++;
				else if (k->clase == "trampa") tr++;
				else mo++;
			}
			float xx = px + 36.0f * s;
			const float yb = py + 92.0f * s;
			auto dato = [&](const char* rot, const std::string& val, glm::vec4 col) {
				xx += texto(F_TITULO, T(rot), xx, yb, 18.0f * s, GRIS, NADA, glm::vec3(0.0f), 0, 1.0f) + 7.0f * s;
				xx += textoCrudo(F_NUMEROS, val, xx, yb, 20.0f * s, col) + 26.0f * s;
			};
			dato("PRINCIPAL", std::to_string(m.principal.size()), BLANCO);
			dato("MONSTRUOS", std::to_string(mo), colorClase("efecto"));
			dato("MAGIAS", std::to_string(mg), colorClase("magia"));
			dato("TRAMPAS", std::to_string(tr), colorClase("trampa"));
			dato("EXTRA", std::to_string(m.fusion.size()), colorClase("fusion"));
			std::string motivo;
			const bool ok = m.valido(*c.col, &motivo);
			const std::string estado = tor ? (ok ? T("Mazo de torneo: Cuadrado lo copia a tus mazos") : motivo)
			                           : c.cursorLista == b.activo ? T("Mazo de los duelos (en uso)") : ok ? T("Válido: Cuadrado lo usa en los duelos") : motivo;
			textoCrudo(F_TITULO, recortar(F_TITULO, estado, 19.0f * s, pw - 72.0f * s), px + 36.0f * s, py + 126.0f * s, 19.0f * s,
			           ok ? glm::vec4(0.45f, 1.0f, 0.62f, 1.0f) : ORO);
			// las cartas distintas del mazo (las de campo y el extra incluidos)
			std::vector<std::string> distintas;
			for (const auto* v : { &m.principal, &m.fusion })
				for (const std::string& id : *v)
					if (std::find(distintas.begin(), distintas.end(), id) == distintas.end()) distintas.push_back(id);
			const int cols = 10;
			const float cw = (pw - 60.0f * s) / (float)cols, chh = cw * 1.45f;
			const glm::vec2 tam(cw - 8.0f * s, (cw - 8.0f * s) * 1.452f);
			for (int i = 0; i < (int)distintas.size() && i < cols * 4; i++) {
				const mazo::Carta* k = c.col->carta(distintas[(size_t)i]);
				if (!k) continue;
				const glm::vec2 ce(px + 30.0f * s + cw * ((float)(i % cols) + 0.5f), py + 160.0f * s + chh * ((float)(i / cols) + 0.5f));
				dibujarMiniCarta(*k, ce, tam, 0.95f, false, (float)i * 0.13f);
				const int cu = m.cuantas(k->id);
				if (cu > 1) etiqueta(ce.x + tam.x * 0.5f - 26.0f * s, ce.y + tam.y * 0.5f - 24.0f * s, "x" + std::to_string(cu), 13.0f * s, ORO);
			}
		}
		if (c.capa == constructor::K_NADA && tor) barraBotones({ { "CRUZ", "Elegir" }, { "X", "Acciones" }, { "C", "Copiar a mis mazos" }, { "O", "Volver" } });
		else if (c.capa == constructor::K_NADA)
			barraBotones({ { "CRUZ", "Elegir" }, { "X", "Acciones" }, { "C", "Usar en los duelos" }, { "T", "Mazos de torneo" }, { "O", "Menú principal" } });
	}

	// ---- EDITOR
	void dibujarEditor(const constructor::Constructor& c)
	{
		using constructor::T;
		const mazo::Carta* e = c.elegida();
		// ---- detalle de la carta (izquierda)
		{
			const float px = 24.0f * s, py = 24.0f * s, pw = 400.0f * s, ph = 916.0f * s;
			const glm::vec4 acento = e ? colorClase(e->clase) : CIAN;
			panel(px, py, pw, ph, glm::mix(CIAN, acento, 0.5f));
			if (e) {
				const int ui = buscarCarta(e->id);
				const glm::vec2 tam = glm::vec2(280.0f, 407.0f) * s;
				const glm::vec2 centro(px + pw * 0.5f, py + 18.0f * s + tam.y * 0.5f);
				const float angY = glm::radians(4.0f) * sinf(t * 0.6f), angX = glm::radians(2.0f) * sinf(t * 0.45f + 1.0f);
				if (const CaraHD* h = ui >= 0 ? caraHD(ui) : nullptr) {
					usar(h->set, false);
					const float u = 0.5f / h->tex.width, vv = 0.5f / h->tex.height;
					rectInclinado(centro, tam, angY, angX, 0.0f, IMAGEN, glm::vec4(1.0f), acento, 8.0f * s, glm::vec4(0.5f, 1.0f, (float)ui * 0.13f, 0.0f),
					              glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
				} else if (ui >= 0 && caraLista(ui)) {
					usar(cartas[ui].setCara, false);
					const float u = 0.5f / cartas[ui].cara.width, vv = 0.5f / cartas[ui].cara.height;
					rectInclinado(centro, tam, angY, angX, 0.0f, IMAGEN, glm::vec4(1.0f), acento, 8.0f * s, glm::vec4(0.5f, 1.0f, (float)ui * 0.13f, 0.0f),
					              glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
				} else if (ui >= 0 && miniLista(ui)) {
					usar(cartas[ui].setMini, false);
					const float u = 0.5f / cartas[ui].mini.width, vv = 0.5f / cartas[ui].mini.height;
					rectInclinado(centro, tam, angY, angX, 0.0f, IMAGEN, glm::vec4(1.0f), acento, 8.0f * s, glm::vec4(0.0f), glm::vec2(u, vv), glm::vec2(1.0f - u, 1.0f - vv));
				} else {
					const Dorso dz = usarDorso(false, false, false, 1.0f, 0.0f);
					rectInclinado(centro, tam, angY, angX, 0.0f, DORSO, dz.col, dz.col2, 8.0f * s, dz.extra, uvDorso0(false), uvDorso1(false));
				}
				usar(setAtlas, false);
				if (e->bloqueada) candado(centro, 2.4f * s, glm::vec4(0.85f, 0.88f, 0.95f, 1.0f));
				float y = py + 18.0f * s + tam.y + 36.0f * s;
				const float tx = px + 22.0f * s, tw = pw - 44.0f * s;
				{
					const bool antes = crudo;
					crudo = true;
					const int l = parrafo(F_TITULO, e->nombre, tx, y, 25.0f * s, tw, 26.0f * s, 2, BLANCO);
					crudo = antes;
					y += (float)l * 26.0f * s + 4.0f * s;
				}
				// clase y subtipo; monstruos: atributo, tipo, nivel, ATK/DEF
				const std::string sub = nombreSubtipo(e->subtipo.empty() ? e->clase : e->subtipo);
				texto(F_TITULO, sub.empty() ? T(e->clase == "magia" ? "MAGIA" : e->clase == "trampa" ? "TRAMPA" : "MONSTRUOS") : T(sub), tx, y, 18.0f * s, acento, NADA,
				      glm::vec3(0.0f), 0, 1.0f);
				y += 25.0f * s;
				if (mazo::esMonstruo(e->clase)) {
					textoCrudo(F_TITULO, e->atributo + " · " + e->raza + " · " + T("NIVEL ") + std::to_string(e->nivel), tx, y, 18.0f * s, GRIS);
					y += 25.0f * s;
					textoCrudo(F_NUMEROS, "ATK " + std::to_string(e->atk) + "   DEF " + std::to_string(e->def), tx, y, 20.0f * s, ORO);
					y += 27.0f * s;
				}
				// el texto completo, con desplazamiento (stick derecho)
				const float tt = 17.0f * s, il = 20.5f * s, fin = py + ph - 58.0f * s;
				const std::vector<std::string> lineas = cortarLineasCrudo(F_TEXTO, e->texto, tt, tw);
				const int caben = std::max(1, (int)((fin - y) / il));
				const int desde = std::clamp(c.scrollTexto, 0, std::max(0, (int)lineas.size() - caben));
				for (int i = desde; i < (int)lineas.size() && i < desde + caben; i++)
					textoCrudo(F_TEXTO, lineas[(size_t)i], tx, y + (float)(i - desde) * il + tt, tt, glm::vec4(0.82f, 0.87f, 0.95f, 1.0f));
				if ((int)lineas.size() > caben) {
					texto(F_TITULO, desde + caben < (int)lineas.size() ? "▼" : "▲", px + pw - 22.0f * s, fin + 2.0f * s, 16.0f * s, CIAN, NADA, glm::vec3(0.0f), 2, 0.0f);
				}
				// copias / limite o el candado
				static const char* const LIM[] = { "PROHIBIDA", "LIMITADA (1)", "SEMILIMITADA (2)", "SIN LÍMITE (3)" };
				std::string cop;
				glm::vec4 colCop = GRIS;
				if (e->bloqueada) {
					cop = c.col->venceA(*e) + T(" para desbloquear");
					colCop = glm::vec4(0.8f, 0.85f, 0.95f, 1.0f);
				} else {
					cop = (c.soloVer ? std::string() : T("EN EL MAZO ") + std::to_string(c.mazo.cuantas(e->id)) + "/" + std::to_string(e->limite) + "  ·  ") +
					      T("Goat: ") + T(LIM[std::clamp(e->limiteBase, 0, 3)]) + (c.col->permitirProhibidas && e->limiteBase < 3 ? T(" (Opciones: sin límite)") : std::string());
					colCop = e->limiteBase == 0 ? CARMESI : e->limiteBase < 3 ? ORO : GRIS;
				}
				if (e->bloqueada) texto(F_NUMEROS, T("CARTA FIRMA BLOQUEADA"), tx, py + ph - 38.0f * s, 15.0f * s, colCop, NADA, glm::vec3(0.0f), 0, 1.0f);
				textoCrudo(F_TITULO, recortar(F_TITULO, cop, 17.0f * s, tw), tx, py + ph - 16.0f * s, 17.0f * s, colCop);
			} else {
				parrafo(F_TITULO, T("Elige una carta para ver su detalle"), px + 30.0f * s, py + 200.0f * s, 22.0f * s, pw - 60.0f * s, 28.0f * s, 3, GRIS);
			}
		}

		// ---- el mazo (medio)
		if (!c.soloVer) {
			const float dx = 440.0f * s, dy = 24.0f * s, dw = 650.0f * s, dh = 916.0f * s;
			const bool foco = c.area == constructor::A_MAZO;
			panel(dx, dy, dw, dh, foco ? ORO : glm::vec4(0.3f, 0.4f, 0.55f, 1.0f));
			// nombre del mazo y contadores
			textoCrudo(F_TITULO, recortar(F_TITULO, c.mazo.nombre.empty() ? T("Mazo nuevo") : c.mazo.nombre, 24.0f * s, dw - 160.0f * s) + (c.cambiado ? " *" : ""),
			           dx + 20.0f * s, dy + 36.0f * s, 24.0f * s, BLANCO);
			if (c.cambiado) texto(F_TITULO, T("SIN GUARDAR"), dx + dw - 18.0f * s, dy + 34.0f * s, 15.0f * s, ORO, NADA, glm::vec3(0.0f), 2, 1.0f);
			int mo = 0, mg = 0, tr = 0;
			c.conteo(mo, mg, tr);
			const int n = (int)c.mazo.principal.size();
			const bool nOk = n >= mazo::MIN_PRINCIPAL && n <= mazo::MAX_PRINCIPAL;
			float xx = dx + 20.0f * s;
			const float yb = dy + 68.0f * s;
			auto dato = [&](const char* rot, const std::string& val, glm::vec4 col) {
				xx += texto(F_TITULO, T(rot), xx, yb, 16.0f * s, GRIS, NADA, glm::vec3(0.0f), 0, 1.0f) + 6.0f * s;
				xx += textoCrudo(F_NUMEROS, val, xx, yb, 19.0f * s, col) + 18.0f * s;
			};
			dato("PRINCIPAL", std::to_string(n) + " / 40-60", nOk ? glm::vec4(0.45f, 1.0f, 0.62f, 1.0f) : ORO);
			dato("MONSTRUOS", std::to_string(mo), colorClase("efecto"));
			dato("MAGIAS", std::to_string(mg), colorClase("magia"));
			dato("TRAMPAS", std::to_string(tr), colorClase("trampa"));
			// principal: 10 x 6
			const float cel = 63.0f * s, celH = 91.0f * s;
			const glm::vec2 tamC(57.0f * s, 83.0f * s);
			const float gx = dx + (dw - cel * constructor::MAZO_COLUMNAS) * 0.5f, gy = dy + 84.0f * s;
			for (int i = 0; i < 60; i++) {
				const glm::vec2 ce(gx + cel * ((float)(i % constructor::MAZO_COLUMNAS) + 0.5f), gy + celH * ((float)(i / constructor::MAZO_COLUMNAS) + 0.5f));
				if (i >= n) {   // casillero vacio
					usar(setAtlas, false);
					caja(ce.x - tamC.x * 0.5f, ce.y - tamC.y * 0.5f, tamC.x, tamC.y, 0.0f, PANEL, glm::vec4(0.03f, 0.05f, 0.09f, 0.6f),
					     i < mazo::MIN_PRINCIPAL ? glm::vec4(0.25f, 0.32f, 0.45f, 0.8f) : glm::vec4(0.15f, 0.2f, 0.3f, 0.5f), 4.0f * s,
					     glm::vec4(1.0f * s, 0.4f, 0.0f, 0.0f));
					continue;
				}
				const mazo::Carta* k = c.col->carta(c.mazo.principal[(size_t)i]);
				if (!k) continue;
				const bool sel = foco && !c.enExtra && c.cursorMazo == i;
				zCeldaConstructor(ce, tamC, i % constructor::MAZO_COLUMNAS, i / constructor::MAZO_COLUMNAS, foco && !c.enExtra,
				                  c.cursorMazo % constructor::MAZO_COLUMNAS, c.cursorMazo / constructor::MAZO_COLUMNAS, constructor::C_RS_IZQ);   // rama classic-pc
				dibujarMiniCarta(*k, ce, tamC * (sel ? 1.08f : 1.0f), 0.95f, sel, (float)i * 0.13f);
				if (k->bloqueada || k->limiteBase < 3) {
					const glm::vec4 cl = k->bloqueada ? glm::vec4(0.75f, 0.8f, 0.9f, 1.0f) : k->limiteBase == 0 ? CARMESI : k->limiteBase == 1 ? ORO : glm::vec4(1.0f, 0.62f, 0.25f, 1.0f);
					usar(setAtlas, false);
					rect(ce + glm::vec2(-tamC.x * 0.5f + 8.0f * s, -tamC.y * 0.5f + 8.0f * s), glm::vec2(12.0f * s), 0.0f, 0.0f, ORBE, cl, cl, 0.0f, glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
				}
				if (sel) marcoCursor(ce, tamC * 1.08f, 2.4f);
			}
			// extra: 8 x 2
			const int nE = (int)c.mazo.fusion.size();
			const float ey = gy + celH * 6.0f + 34.0f * s;
			texto(F_TITULO, T("MAZO EXTRA"), dx + 20.0f * s, ey - 8.0f * s, 16.0f * s, GRIS, NADA, glm::vec3(0.0f), 0, 1.0f);
			textoCrudo(F_NUMEROS, std::to_string(nE) + " / 15", dx + 20.0f * s + medir(F_TITULO, T("MAZO EXTRA"), 16.0f * s, 1.0f) + 10.0f * s, ey - 8.0f * s, 18.0f * s,
			           colorClase("fusion"));
			const float gxE = dx + (dw - cel * constructor::EXTRA_COLUMNAS) * 0.5f;
			for (int i = 0; i < 16; i++) {
				const glm::vec2 ce(gxE + cel * ((float)(i % constructor::EXTRA_COLUMNAS) + 0.5f), ey + 6.0f * s + celH * ((float)(i / constructor::EXTRA_COLUMNAS) + 0.5f));
				if (i >= 15) break;
				if (i >= nE) {
					usar(setAtlas, false);
					caja(ce.x - tamC.x * 0.5f, ce.y - tamC.y * 0.5f, tamC.x, tamC.y, 0.0f, PANEL, glm::vec4(0.05f, 0.03f, 0.09f, 0.6f), glm::vec4(0.3f, 0.22f, 0.45f, 0.7f),
					     4.0f * s, glm::vec4(1.0f * s, 0.4f, 0.0f, 0.0f));
					continue;
				}
				const mazo::Carta* k = c.col->carta(c.mazo.fusion[(size_t)i]);
				if (!k) continue;
				const bool sel = foco && c.enExtra && c.cursorMazo == i;
				zCeldaConstructor(ce, tamC, i % constructor::EXTRA_COLUMNAS, 6 + i / constructor::EXTRA_COLUMNAS, foco && c.enExtra,
				                  c.cursorMazo % constructor::EXTRA_COLUMNAS, 6 + c.cursorMazo / constructor::EXTRA_COLUMNAS, constructor::C_RS_IZQ);   // rama classic-pc
				dibujarMiniCarta(*k, ce, tamC * (sel ? 1.08f : 1.0f), 0.95f, sel, (float)i * 0.17f);
				if (sel) marcoCursor(ce, tamC * 1.08f, 2.4f);
			}
		}

		// ---- la coleccion (derecha)
		{
			const float cx = (c.soloVer ? 440.0f : 1106.0f) * s, cy = 24.0f * s, cw = (c.soloVer ? 1456.0f : 790.0f) * s, ch = 916.0f * s;
			const bool foco = c.area == constructor::A_COLECCION;
			panel(cx, cy, cw, ch, foco ? ORO : glm::vec4(0.3f, 0.4f, 0.55f, 1.0f));
			texto(F_TITULO, T(c.soloVer ? "LISTA DE CARTAS · FORMATO GOAT" : "COLECCIÓN · FORMATO GOAT") + (c.col->permitirProhibidas ? "  ·  " + T("PROHIBIDAS PERMITIDAS") : std::string()), cx + 20.0f * s, cy + 36.0f * s, 21.0f * s, BLANCO, NADA, glm::vec3(0.0f), 0, 1.5f);
			textoCrudo(F_NUMEROS, std::to_string(c.lista.size()) + T(" CARTAS · PÁG ") + std::to_string(c.pagina() + 1) + "/" + std::to_string(c.paginas()), cx + cw - 20.0f * s,
			           cy + 36.0f * s, 16.0f * s, GRIS, 2);
			// resumen de la busqueda y los filtros activos
			{
				std::string r;
				auto mas = [&](const std::string& x) { r += (r.empty() ? "" : "  ·  ") + x; };
				if (!c.filtros.texto.empty()) mas("\"" + c.filtros.texto + "\"");
				if (c.filtros.clase) mas(T(constructor::CLASES[c.filtros.clase].texto));
				if (c.filtros.subtipo) mas(T(constructor::SUBTIPOS[c.filtros.subtipo].texto));
				if (c.filtros.atributo) mas(c.valorFiltro(constructor::FF_ATRIBUTO));
				if (c.filtros.nivel) mas(T("NIVEL ") + std::to_string(c.filtros.nivel));
				if (c.filtros.tipo) mas(c.valorFiltro(constructor::FF_TIPO));
				if (c.filtros.atk) mas("ATK " + c.valorFiltro(constructor::FF_ATK));
				if (c.filtros.def) mas("DEF " + c.valorFiltro(constructor::FF_DEF));
				if (c.filtros.orden) mas(T("Orden: ") + c.valorFiltro(constructor::FF_ORDEN));
				if (r.empty()) r = T("Sin filtros · Triángulo: buscar y filtrar");
				textoCrudo(F_TITULO, recortar(F_TITULO, r, 17.0f * s, cw - 40.0f * s), cx + 20.0f * s, cy + 66.0f * s, 17.0f * s, c.filtros.activos() ? CIAN : GRIS);
			}
			const float cel = 128.0f * s, celH = 204.0f * s;
			const glm::vec2 tamM(118.0f * s, 171.0f * s);
			const float gx = cx + (cw - cel * c.colCols()) * 0.5f, gy = cy + 86.0f * s;
			const int desde = c.pagina() * c.porPagina();
			for (int i = 0; i < c.porPagina(); i++) {
				const int li = desde + i;
				if (li >= (int)c.lista.size()) break;
				const mazo::Carta& mc = c.col->cartas[(size_t)c.lista[(size_t)li]];
				const glm::vec2 ce(gx + cel * ((float)(i % c.colCols()) + 0.5f), gy + celH * ((float)(i / c.colCols()) + 0.5f) - 8.0f * s);
				const bool sel = foco && li == c.cursor;
				zCeldaConstructor(ce, tamM, i % c.colCols(), i / c.colCols(), foco, (c.cursor - desde) % c.colCols(), (c.cursor - desde) / c.colCols(), constructor::C_RS_DER);   // rama classic-pc
				const int enMazo = c.mazo.cuantas(mc.id);
				const bool agotada = mc.limite == 0 || enMazo >= mc.limite;
				const float esc = sel ? 1.0f + 0.05f * suave(0.0f, 0.12f, c.tCursor) : 1.0f;
				dibujarMiniCarta(mc, ce, tamM * esc, agotada ? 0.4f : 0.95f, sel, (float)i * 0.13f);
				if (mc.bloqueada) candado(ce, 1.3f * s, glm::vec4(0.85f, 0.88f, 0.95f, 1.0f));
				if (mc.bloqueada || mc.limiteBase < 3) {
					static const char* const LIM[] = { "PROHIBIDA", "LIMITADA", "SEMI" };
					const char* etq = mc.bloqueada ? "BLOQUEADA" : LIM[mc.limiteBase];
					const glm::vec4 cl = mc.bloqueada ? glm::vec4(0.75f, 0.8f, 0.9f, 1.0f) : mc.limiteBase == 0 ? CARMESI : mc.limiteBase == 1 ? ORO : glm::vec4(1.0f, 0.62f, 0.25f, 1.0f);
					etiqueta(ce.x - tamM.x * 0.5f + 3.0f * s, ce.y - tamM.y * 0.5f + 3.0f * s, T(etq), 11.0f * s, cl);
				}
				// copias en el mazo / permitidas
				if (!c.soloVer && (enMazo > 0 || sel)) {
					const std::string nn = std::to_string(enMazo) + "/" + std::to_string(mc.limite);
					const float w = medir(F_NUMEROS, nn, 15.0f * s, 1.0f) + 14.0f * s, h = 22.0f * s;
					const float bx = ce.x + tamM.x * 0.5f - w - 3.0f * s, by = ce.y + tamM.y * 0.5f - h - 3.0f * s;
					usar(setAtlas, false);
					caja(bx, by, w, h, 5.0f * s, PANEL, glm::vec4(0.02f, 0.04f, 0.08f, 0.94f), enMazo > 0 ? ORO : GRIS, h * 0.5f, glm::vec4(1.2f * s, 1.0f, 0.0f, 0.0f));
					textoCrudo(F_NUMEROS, nn, bx + w * 0.5f, by + h * 0.5f + 5.0f * s, 15.0f * s, enMazo > 0 ? ORO : BLANCO, 1);
				}
				// el nombre debajo
				nombreCelda(F_TEXTO, mc.nombre, ce.x, ce.y + tamM.y * 0.5f + 18.0f * s, 14.0f * s, cel - 6.0f * s, sel ? ORO : glm::vec4(0.75f, 0.8f, 0.9f, 1.0f));
				if (sel) marcoCursor(ce, tamM * esc, 3.0f);
			}
			if (c.lista.empty())
				texto(F_TITULO, T("Ninguna carta con estos filtros (L3 los quita)"), cx + cw * 0.5f, cy + ch * 0.45f, 21.0f * s, GRIS, NADA, glm::vec3(0.0f), 1, 1.0f);
		}

		// ---- validez
		if (!c.soloVer) {
			std::string motivo;
			const bool ok = c.valido(&motivo);
			const glm::vec4 verde(0.45f, 1.0f, 0.62f, 1.0f);
			const std::string txt = ok ? T("MAZO VÁLIDO") + std::string("  ·  ") + T("OPTIONS guarda") : T("NO VÁLIDO: ") + motivo;
			if (c.tAviso >= 3.0f || c.aviso.empty())
				textoCrudo(F_TITULO, recortar(F_TITULO, txt, 20.0f * s, W - 100.0f * s), W * 0.5f, 1012.0f * s - 2.0f * s, 20.0f * s, ok ? verde : ORO, 1);
		}
		if (c.capa == constructor::K_NADA && c.soloVer) {
			// rama classic-galeria: Cuadrado abre la galeria de figuras 3D y R3 "Ver figura" (si la carta tiene)
			const bool fig = gal && e && gal->tieneFigura(e->id);
			if (fig) barraBotones({ { "CRUZ", "Mover" }, { "R3", "Ver figura" }, { "C", "Galería 3D" }, { "T", "Buscar y filtrar" }, { "L1R1", "Página" }, { "L2", "Clase" }, { "L3", "Sin filtros" }, { "O", "Volver" } });
			else barraBotones({ { "CRUZ", "Mover" }, { "C", "Galería 3D" }, { "T", "Buscar y filtrar" }, { "L1R1", "Página" }, { "L2", "Clase" }, { "L3", "Sin filtros" }, { "O", "Volver" } });
		} else if (c.capa == constructor::K_NADA) barraBotones({ { "CRUZ", "Mover" }, { "X", "Agregar" }, { "C", "Quitar" }, { "T", "Buscar y filtrar" }, { "L1R1", "Página" }, { "L2", "Clase" },
		               { "STICK_R", "Mazo / colección" }, { "OPTIONS", "Guardar" }, { "O", "Volver" } });
	}

	// ---- capa de filtros
	void dibujarFiltros(const constructor::Constructor& c)
	{
		using constructor::T;
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f), NADA, 0.0f, glm::vec4(0.0f));
		if (c.capa == constructor::K_FILTROS) rz.tapar();   // rama classic-pc
		const float w = 760.0f * s, fila = 52.0f * s, h = 110.0f * s + fila * (float)constructor::NUM_FILAS_FILTRO;
		const float x = (W - w) * 0.5f, y = (H - h) * 0.5f - 30.0f * s;
		panel(x, y, w, h, CIAN);
		texto(F_NUMEROS, T("BUSCAR Y FILTRAR"), x + 30.0f * s, y + 48.0f * s, 26.0f * s, BLANCO, CIAN, glm::vec3(0.4f, 0.0f, 0.5f), 0, 1.5f);
		textoCrudo(F_NUMEROS, std::to_string(c.lista.size()) + T(" cartas encontradas"), x + w - 30.0f * s, y + 46.0f * s, 17.0f * s, GRIS, 2);
		for (int i = 0; i < constructor::NUM_FILAS_FILTRO; i++) {
			const float yy = y + 76.0f * s + fila * (float)i;
			const bool sel = c.capa == constructor::K_FILTROS && i == c.filaFiltro;
			if (c.capa == constructor::K_FILTROS) {   // rama classic-pc: la fila; en las de valor, mitad izquierda = anterior, derecha = siguiente
				const bool bot = i == constructor::FF_QUITAR || i == constructor::FF_LISTO;
				if (bot || i == constructor::FF_NOMBRE) zLista(x + 18.0f * s, yy, w - 36.0f * s, fila - 6.0f * s, i, c.filaFiltro);
				else {
					zLista(x + 18.0f * s, yy, (w - 36.0f * s) * 0.5f, fila - 6.0f * s, i, c.filaFiltro, false, control::B_IZQ);
					zLista(x + 18.0f * s + (w - 36.0f * s) * 0.5f, yy, (w - 36.0f * s) * 0.5f, fila - 6.0f * s, i, c.filaFiltro, false, control::B_DER);
				}
			}
			usar(setAtlas, false);
			if (sel)
				caja(x + 18.0f * s, yy, w - 36.0f * s, fila - 6.0f * s, 4.0f * s, PANEL, glm::vec4(0.16f, 0.13f, 0.06f, 0.95f), ORO, 8.0f * s, glm::vec4(1.8f * s, 1.3f, 0.0f, 0.0f));
			const float base = yy + (fila - 6.0f * s) * 0.5f + 7.0f * s;
			const bool boton = i == constructor::FF_QUITAR || i == constructor::FF_LISTO;
			texto(F_TITULO, T(constructor::FILAS_FILTRO[i]), boton ? x + w * 0.5f : x + 40.0f * s, base, 21.0f * s, sel ? ORO : boton ? CIAN : BLANCO, NADA, glm::vec3(0.0f),
			      boton ? 1 : 0, 1.0f);
			if (boton) continue;
			const std::string v = c.valorFiltro(i);
			const float vx = x + w - 60.0f * s;
			textoCrudo(F_TITULO, recortar(F_TITULO, v, 21.0f * s, 380.0f * s), vx, base, 21.0f * s, sel ? BLANCO : GRIS, 2);
			if (sel && i != constructor::FF_NOMBRE) {
				texto(F_TITULO, "<", vx - medirCrudo(F_TITULO, recortar(F_TITULO, v, 21.0f * s, 380.0f * s), 21.0f * s) - 22.0f * s, base, 21.0f * s, ORO, NADA, glm::vec3(0.0f), 0, 0.0f);
				texto(F_TITULO, ">", vx + 14.0f * s, base, 21.0f * s, ORO, NADA, glm::vec3(0.0f), 0, 0.0f);
			}
		}
		if (c.capa == constructor::K_FILTROS)
			barraBotones({ { "CRUZ", "Elegir y cambiar" }, { "X", "Aceptar" }, { "C", "Esta fila a Todos" }, { "L3", "Quitar filtros" }, { "O", "Cerrar" } });
	}

	// ---- teclado en pantalla
	void dibujarTeclado(const constructor::Constructor& c)
	{
		using constructor::T;
		const float tk = 70.0f * s, sep = 8.0f * s;
		const float w = tk * 10.0f + sep * 9.0f + 60.0f * s, h = 520.0f * s;
		const float x = (W - w) * 0.5f, y = c.modoTeclado == constructor::T_BUSCAR ? H - h - 70.0f * s : (H - h) * 0.5f;
		if (c.modoTeclado != constructor::T_BUSCAR) {
			usar(setAtlas, false);
			caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f), NADA, 0.0f, glm::vec4(0.0f));
		}
		rz.tapar();   // rama classic-pc
		panel(x, y, w, h, ORO);
		const char* titulo = c.modoTeclado == constructor::T_BUSCAR ? "Buscar por nombre" : c.modoTeclado == constructor::T_NOMBRE_NUEVO ? "Nombre del mazo nuevo" : "Renombrar mazo";
		texto(F_TITULO, T(titulo), x + 30.0f * s, y + 42.0f * s, 22.0f * s, ORO, NADA, glm::vec3(0.0f), 0, 1.0f);
		// el campo de texto con el cursor
		usar(setAtlas, false);
		caja(x + 30.0f * s, y + 58.0f * s, w - 60.0f * s, 56.0f * s, 4.0f * s, PANEL, glm::vec4(0.01f, 0.02f, 0.04f, 0.95f), CIAN, 8.0f * s, glm::vec4(1.4f * s, 1.0f, 0.0f, 0.0f));
		const float tw = textoCrudo(F_TITULO, c.textoTeclado, x + 46.0f * s, y + 96.0f * s, 27.0f * s, BLANCO);
		if (fmodf(c.t, 1.0f) < 0.6f) {
			usar(setAtlas, false);
			rect(glm::vec2(x + 50.0f * s + tw, y + 86.0f * s), glm::vec2(3.0f * s, 30.0f * s), 0.0f, 0.0f, PANEL, ORO, NADA, 1.0f * s, glm::vec4(0.0f));
		}
		textoCrudo(F_NUMEROS, std::to_string(constructor::largoUtf8(c.textoTeclado)) + "/" + std::to_string(constructor::MAX_TEXTO), x + w - 40.0f * s, y + 94.0f * s, 15.0f * s, GRIS, 2);
		// teclas
		const float kx = x + 30.0f * s, ky = y + 136.0f * s;
		for (int f = 0; f < constructor::FILAS_TECLADO; f++) {
			const int n = constructor::cuantasEnFila(f);
			const float anchoFila = tk * 10.0f + sep * 9.0f;
			const float kw = f < 4 ? tk : (anchoFila - sep * 3.0f) / 4.0f;
			for (int k = 0; k < n; k++) {
				const float px = kx + (kw + sep) * (float)k, py = ky + (tk + sep) * (float)f;
				const bool sel = f == c.teclaF && k == c.teclaC;
				{   // rama classic-pc: la tecla (columna con izq / der, fila con arriba / abajo)
					raton::Zona z;
					z.clic = control::B_X;   // escribir la letra
					z.e[0] = raton::ejeLista(k, c.teclaC, control::B_DER, control::B_IZQ);
					z.e[1] = raton::ejeLista(f, c.teclaF, control::B_ABAJO, control::B_ARRIBA);
					zRect(px, py, kw, tk, z);
				}
				std::string et = f < 4 ? std::string(constructor::TECLAS[f][k]) : T(constructor::TECLAS_ESP[k]);
				if (f < 4 && !c.mayus) et = constructor::minuscula(et);
				const bool activa = f == 4 && k == 0 && c.mayus;
				usar(setAtlas, false);
				caja(px, py, kw, tk, 3.0f * s, PANEL, sel ? glm::vec4(0.22f, 0.17f, 0.06f, 0.98f) : glm::vec4(0.05f, 0.07f, 0.12f, 0.95f),
				     sel ? ORO : activa ? CIAN : glm::vec4(0.3f, 0.38f, 0.52f, 1.0f), 8.0f * s, glm::vec4((sel ? 2.2f : 1.0f) * s, sel ? 1.4f : 0.6f, 0.0f, 0.0f));
				if (f < 4) textoCrudo(F_TITULO, et, px + kw * 0.5f, py + tk * 0.5f + 10.0f * s, 28.0f * s, sel ? ORO : BLANCO, 1);
				else texto(F_TITULO, et, px + kw * 0.5f, py + tk * 0.5f + 8.0f * s, 20.0f * s, sel ? ORO : activa ? CIAN : BLANCO, NADA, glm::vec3(0.0f), 1, 1.0f);
			}
		}
		barraBotones({ { "X", "Escribir" }, { "C", "Borrar" }, { "T", "Espacio" }, { "L1R1", "Mayúsculas" }, { "OPTIONS", "Aceptar" }, { "O", "Cancelar" } });
	}

	// ---- pregunta / acciones
	void dibujarPregunta(const constructor::Constructor& c)
	{
		using constructor::T;
		usar(setAtlas, false);
		caja(0.0f, 0.0f, W, H, 0.0f, PANEL, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f), NADA, 0.0f, glm::vec4(0.0f));
		const int n = (int)c.opciones.size();
		const float w = 620.0f * s, fila = 54.0f * s, h = 96.0f * s + fila * (float)n;
		const float x = (W - w) * 0.5f, y = (H - h) * 0.5f;
		const float a = suave(0.0f, 0.15f, c.tCapa);
		rz.tapar();   // rama classic-pc
		panel(x, y + (1.0f - a) * 12.0f * s, w, h, glm::mix(CIAN, ORO, 0.6f));
		textoCrudo(F_TITULO, recortar(F_TITULO, c.tituloPregunta, 24.0f * s, w - 60.0f * s), x + w * 0.5f, y + 50.0f * s, 24.0f * s, ORO, 1);
		for (int i = 0; i < n; i++) {
			const float yy = y + 74.0f * s + fila * (float)i;
			const bool sel = i == c.opcion;
			zLista(x + 30.0f * s, yy, w - 60.0f * s, fila - 8.0f * s, i, c.opcion);   // rama classic-pc
			usar(setAtlas, false);
			caja(x + 30.0f * s, yy, w - 60.0f * s, fila - 8.0f * s, 4.0f * s, PANEL, sel ? glm::vec4(0.18f, 0.14f, 0.06f, 0.97f) : glm::vec4(0.04f, 0.06f, 0.11f, 0.9f),
			     sel ? ORO : glm::vec4(0.28f, 0.35f, 0.5f, 1.0f), 10.0f * s, glm::vec4((sel ? 2.0f : 1.0f) * s, sel ? 1.4f : 0.6f, 0.0f, 0.0f));
			texto(F_TITULO, T(c.opciones[(size_t)i]), x + w * 0.5f, yy + (fila - 8.0f * s) * 0.5f + 8.0f * s, 22.0f * s, sel ? BLANCO : GRIS, NADA, glm::vec3(0.0f), 1, 1.0f);
		}
		barraBotones({ { "CRUZ", "Elegir" }, { "X", "Aceptar" }, { "O", "Cancelar" } });
	}
