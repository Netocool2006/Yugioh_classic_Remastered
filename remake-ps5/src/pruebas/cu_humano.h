// Rama classic-jugador-humano: el JUGADOR HUMANO. Juega con el mando mirando SOLO lo que la pantalla dibuja en cada
// cuadro (pantalla_sin_gpu.h: textos, menus, ayudas de botones, cartas con su cara o dorso y su lugar, el recuadro
// dorado), con tiempos de reaccion humanos y travesuras (OPTIONS, engranaje, triangulo, cuadrado, flechas y X de mas,
// en animaciones, con preguntas pendientes y en el turno del rival). El estado interno del juego (ia::Duelo, control::Mando)
// lo mira solo el JUEZ (los oraculos), nunca el jugador.
//
// Oraculos (casos CU-HU-*; umbrales en docs/jugador_humano.md):
//  - ENTERARSE: cada suceso que cambia el campo del jugador (al cementerio, desterrada, a la mano, descartada, cambio de
//    posicion, volteo, control, robo, danno de LP) y cada activacion del rival se ve en pantalla >= UMBRAL_VISTO s de juego
//    (la carta viajando o en grande, su nombre en un cartel/aviso, el numero del danno o los LP rodando).
//  - VENTANA-VISIBLE: si el motor le pregunta al jugador (pregunta lado 0) y la mesa esta al dia, la pregunta (menu o
//    eleccion) esta en pantalla antes de PLAZO_VENTANA s.
//  - NUNCA-TRANCADO: el duelo avanza (sin progreso PLAZO_TRANCADO s = falla) y nunca "Primero responde a la pregunta" sin
//    pregunta a la vista.
//  - MESA-COHERENTE: con la mesa al dia, lo dibujado (cartas de cada zona, la mano, los LP) es el duelo.
//  - CONTROLES: OPTIONS solo fases, el engranaje abre la pausa (Continuar, Opciones, Construir mazo, Abandonar = derrota),
//    el cuadrado no hace nada, el triangulo abre el detalle, cada boton de la barra de ayuda hace algo.
#pragma once

#include "pantalla_sin_gpu.h"

#include <cstdarg>
#include <unordered_set>

namespace hu {

using pantalla::Carta;
using pantalla::Cuadro;
using pantalla::Texto;
using VD = control::VistaDuelo;

// ---- umbrales (en s de juego a velocidad Normal)
constexpr float UMBRAL_VISTO = 0.4f;     // ENTERARSE: cuanto tiene que verse un suceso (MD: romperse 0.3 s + viaje 0.3 s)
constexpr float UMBRAL_GIRO = 0.25f;     // ENTERARSE: cambio de posicion / volteo (el giro)
constexpr float VENTANA_SUCESO = 6.0f;   // ENTERARSE: desde que la pantalla lo presenta, plazo para verlo
constexpr float NUNCA_PRESENTADO = 25.0f;   // ENTERARSE: el suceso del motor que la pantalla nunca presenta
constexpr float PLAZO_VENTANA = 2.5f;    // VENTANA-VISIBLE: pregunta del jugador sin su menu en pantalla
constexpr float PLAZO_TRANCADO = 45.0f;  // NUNCA-TRANCADO: nada avanza
constexpr float PLAZO_FANTASMA = 1.0f;   // NUNCA-TRANCADO: "Primero responde" sin pregunta a la vista

inline bool oro(const glm::vec4& c) { return c.r > 0.9f && c.g > 0.6f && c.g < 0.9f && c.b < 0.55f; }
inline bool contiene(const std::string& a, const std::string& b) { return !b.empty() && a.find(b) != std::string::npos; }

// ------------------------------------------------------------------------------------------------- lo que se lee
struct Menu {
	bool abierto{ false };
	std::string titulo;
	std::vector<std::string> ops;
	std::vector<bool> activas;
	int sel{ -1 };
};
struct Lectura {
	std::vector<std::pair<std::string, std::string>> ayuda;   // ("X", "Acciones")...
	Menu menu;
	bool selector{ false };
	std::vector<std::string> fases;
	std::vector<bool> fasesOk;
	int faseSel{ -1 };
	bool pausa{ false };
	std::string aviso;     // la pastilla de aviso arriba del campo
	std::string fase;      // "PRINCIPAL 1"... (la columna de fases)
	bool dice(const std::string& b, const std::string& t) const
	{
		for (const auto& a : ayuda)
			if (a.first == b && (t.empty() || a.second == t)) return true;
		return false;
	}
	std::string ayudaDe(const std::string& b) const
	{
		for (const auto& a : ayuda)
			if (a.first == b) return a.second;
		return "";
	}
	bool libre() const { return dice("OPTIONS", "Fases") && dice("CRUZ", "Mover"); }
	bool navegando() const { return dice("CRUZ", "Mover"); }
	bool eligiendo() const { return dice("CRUZ", "Objetivo") || dice("CRUZ", "Monstruo") || dice("CRUZ", "Carta") || dice("CRUZ", "Zona"); }
	bool detalle() const { return dice("O", "Cerrar") && !selector && !menu.abierto && !dice("CRUZ", "Mover"); }
	bool pregunta() const { return menu.abierto && (dice("O", "No activar") || (!dice("O", "") && !pausa)); }
	// (un menu que no se pudo leer entero, p. ej. una lista larga del mazo: la ayuda de los menus igual lo delata)
	bool menuVisible() const { return menu.abierto || (dice("CRUZ", "Opción") && dice("X", "Confirmar")); }
	std::string resumen() const
	{
		std::string r;
		if (pausa) r += "[pausa] ";
		if (selector) r += "[selector] ";
		if (menu.abierto) {
			r += "[menu '" + menu.titulo + "':";
			for (size_t k = 0; k < menu.ops.size(); k++) r += std::string((int)k == menu.sel ? " >" : " ") + menu.ops[k] + (menu.activas[k] ? "" : "(gris)");
			r += "] ";
		}
		if (!aviso.empty()) r += "[aviso '" + aviso + "'] ";
		r += "ayuda:";
		for (const auto& a : ayuda) r += " " + a.first + "=" + a.second;
		return r;
	}
};

// el icono de boton y su texto (el de la derecha; no la etiqueta de la tecla, que va centrada en el icono)
inline const Texto* textoDerecha(const Cuadro& c, const Texto& ic)
{
	const Texto* mejor = nullptr;
	for (const Texto& t : c.textos) {
		if (t.icono || t.tapado || t.col.a < 0.25f || t.tam > ic.tam * 1.3f || t.tam < 16.0f) continue;   // (no las etiquetas chicas de la mesa)
		if (fabsf(t.cy() - ic.y) > std::max(14.0f, ic.tam * 0.7f)) continue;
		if (t.x < ic.x + 6.0f || t.x > ic.x + ic.tam * 3.5f + 20.0f) continue;   // (la etiqueta de la tecla va centrada en el icono)
		if (!mejor || t.x < mejor->x) mejor = &t;
	}
	return mejor;
}

inline Lectura leer(const Cuadro& c)
{
	Lectura L;
	std::vector<const Texto*> iconosMenu;
	for (const Texto& ic : c.textos) {
		if (!ic.icono || ic.tapado || ic.col.a < 0.25f) continue;
		const std::string b = ic.s.substr(1, ic.s.size() - 2);
		const Texto* t = textoDerecha(c, ic);
		if (t) {
			L.ayuda.push_back({ b, t->s });
		} else if (b == "X" && !(ic.y > 900.0f && ic.x > 1300.0f)) {   // (no la barra de ayuda de abajo a la derecha)
			iconosMenu.push_back(&ic);
		}
	}
	L.pausa = c.hay("PAUSA");
	std::sort(iconosMenu.begin(), iconosMenu.end(), [](const Texto* a, const Texto* b) { return a->v > b->v; });
	// un menu: el [X] al final de la fila elegida; las opciones, los textos alineados con la de esa fila
	for (const Texto* ic : iconosMenu) {
		const Texto* fila = nullptr;
		for (const Texto& t : c.textos) {
			if (t.icono || t.tapado || t.col.a < 0.2f) continue;
			if (fabsf(t.cy() - ic->y) > 16.0f || t.x > ic->x - 20.0f || t.x < ic->x - 600.0f) continue;   // (un texto largo puede pasar por debajo del icono)
			if (!fila || t.x > fila->x) fila = &t;   // (el texto de la fila: el vecino de la izquierda del icono)
		}
		if (!fila) continue;
		std::vector<const Texto*> col;
		for (const Texto& t : c.textos)
			if (!t.icono && !t.tapado && t.col.a >= 0.2f && fabsf(t.x - fila->x) < 3.0f && fabsf(t.tam - fila->tam) < 0.6f && fabsf(t.y - fila->y) < 600.0f) col.push_back(&t);
		std::sort(col.begin(), col.end(), [](const Texto* a, const Texto* b) { return a->y < b->y; });
		// (contiguas: el paso de una fila a otra no pasa de 80 px)
		int k0 = -1;
		for (int k = 0; k < (int)col.size(); k++)
			if (col[(size_t)k] == fila) k0 = k;
		int a = k0, b = k0;
		while (a > 0 && col[(size_t)a]->y - col[(size_t)a - 1]->y < 80.0f) a--;
		while (b + 1 < (int)col.size() && col[(size_t)b + 1]->y - col[(size_t)b]->y < 80.0f) b++;
		Menu m;
		m.abierto = true;
		for (int k = a; k <= b; k++) {
			m.ops.push_back(col[(size_t)k]->s);
			m.activas.push_back(col[(size_t)k]->col.a >= 0.7f * fila->col.a && !(col[(size_t)k]->col.r < 0.6f && col[(size_t)k]->col.b < 0.75f));
			if (col[(size_t)k] == fila) m.sel = k - a;
		}
		// el titulo: el texto de arriba de la primera opcion, un poco a la izquierda
		const float y0 = col[(size_t)a]->y;
		const Texto* tit = nullptr;
		for (const Texto& t : c.textos)
			if (!t.icono && !t.tapado && t.col.a >= 0.2f && t.y < y0 - 5.0f && t.y > y0 - 110.0f && t.x <= fila->x + 2.0f && t.x > fila->x - 120.0f && (!tit || t.y > tit->y)) tit = &t;
		if (tit) m.titulo = tit->s;
		L.menu = m;
		break;
	}
	// el selector de fases
	if (c.hay("SELECCIONA LA FASE")) {
		L.selector = true;
		for (const char* f : { "Iniciar Fase de Batalla", "Ir a M2", "Finalizar turno" }) {
			const Texto* t = nullptr;
			for (const Texto& x : c.textos)
				if (!x.tapado && x.s == f && x.y > 120.0f && x.col.a > 0.1f && (!t || x.y > t->y)) t = &x;
			L.fases.push_back(f);
			L.fasesOk.push_back(t && t->col.a > 0.75f && !(t->col.r < 0.6f && t->col.b < 0.75f));   // (las no validas: grises y a media luz)
			if (t && oro(t->col)) L.faseSel = (int)L.fases.size() - 1;
		}
	}
	// el aviso: la pastilla arriba del campo (y 196..292, a la derecha de la columna)
	for (const Texto& t : c.textos)
		if (!t.icono && !t.tapado && t.col.a > 0.25f && t.x > 512.0f && t.y > 200.0f && t.y < 300.0f && t.tam > 20.0f && t.tam < 24.0f) L.aviso = t.s;
	// la fase: el texto bajo "FASE" en la columna
	if (const Texto* f = c.buscar("FASE"))
		for (const Texto& t : c.textos)
			if (!t.icono && !t.tapado && t.y > f->y + 5.0f && t.y < f->y + 40.0f && fabsf(t.x - f->x) < 60.0f && t.s != "FASE") L.fase = t.s;
	return L;
}

// ------------------------------------------------------------------------------------------------- el juez
struct Juez {
	Juego& g;
	pantalla::Pantalla& p;
	std::string contexto{ "-" }, cartaCaso;   // (cartaCaso: el caso CU-HU-CARTA-<id> en curso)
	std::string semilla;
	std::deque<std::string>* botones{ nullptr };
	float t{ 0.0f }, dt{ 1.0f / 30.0f };
	int cuadroN{ 0 };
	FILE* fallas{ nullptr };
	std::map<std::string, int> cuenta;
	int fallasTotal{ 0 };
	// zonas del tablero en pantalla
	std::array<pantalla::Rect, interfaz::NUM_ZONAS> zona{};

	Juez(Juego& j, pantalla::Pantalla& pp) : g(j), p(pp) {}

	void preparar()
	{
		for (size_t i = 0; i < interfaz::NUM_ZONAS; i++) {
			const glm::vec3 c = p.ui.centroZona((int)i);
			pantalla::Rect r;
			r.x0 = r.y0 = 1e9f, r.x1 = r.y1 = -1e9f;
			for (float sx : { -0.5f, 0.5f })
				for (float sz : { -0.5f, 0.5f }) {
					const glm::vec4 q = p.vp * glm::vec4(c + glm::vec3(sx * interfaz::ZONA_ANCHO, 0.0f, sz * interfaz::ZONA_ALTO), 1.0f);
					const glm::vec2 s((q.x / q.w * 0.5f + 0.5f) * pantalla::ANCHO, (q.y / q.w * 0.5f + 0.5f) * pantalla::ALTO);
					r.x0 = std::min(r.x0, s.x), r.y0 = std::min(r.y0, s.y), r.x1 = std::max(r.x1, s.x), r.y1 = std::max(r.y1, s.y);
				}
			zona[i] = r;
		}
	}
	int zonaInterfaz(int zonaMesa) const
	{
		const char* id = control::ZONAS_MESA[zonaMesa].id;
		for (size_t i = 0; i < interfaz::NUM_ZONAS; i++)
			if (strcmp(interfaz::ZONAS[i].id, id) == 0) return (int)i;
		return -1;
	}

	std::string ultimos() const
	{
		std::string r;
		if (!botones) return r;
		for (const std::string& b : *botones) r += (r.empty() ? "" : ",") + b;
		return r;
	}
	// una falla del oraculo: el caso general, el de la carta en curso y la lista con todo para reproducirla
	void falla(const std::string& oraculo, const char* fmt, ...) __attribute__((format(printf, 3, 4)))
	{
		char b[900];
		va_list a;
		va_start(a, fmt);
		vsnprintf(b, sizeof(b), fmt, a);
		va_end(a);
		const std::string id = "CU-HU-" + oraculo;
		const int n = ++cuenta[id + "|" + contexto];
		fallasTotal++;
		char d[1500];
		snprintf(d, sizeof(d), "%s [semilla %s, %s, cuadro %d, t %.1f s, turno %d %s fase %d] botones: %s", b, semilla.c_str(), contexto.c_str(), cuadroN, t, g.dueloIA.turno,
		         g.dueloIA.activo == 0 ? "jugador" : "rival", (int)g.dueloIA.fase, ultimos().c_str());
		if (n <= 2) {
			cu::Resultado& r = cu::de(id);
			r.comprobaciones++;
			if (r.estado != "FALLA") r.estado = "FALLA", r.detalle = d;
			else if (r.detalle.size() < 600) r.detalle += std::string(" || ") + d;
			if (fallas) fprintf(fallas, "%s\t%s\n", id.c_str(), d);
			fprintf(stderr, "FALLA-HU %s: %s\n", id.c_str(), d);
			if (getenv("HU_DEBUG")) {   // (lo que habia en pantalla)
				for (const Texto& t : p.cuadro.textos)
					if (t.col.a > 0.1f) fprintf(stderr, "    T '%s' %.0f,%.0f w %.0f tam %.0f a %.2f tap %d v %zu\n", t.s.c_str(), t.x, t.y, t.w, t.tam, t.col.a, (int)t.tapado, t.v);
				for (const pantalla::Rect& r : p.cuadro.paneles) fprintf(stderr, "    P %.0f,%.0f-%.0f,%.0f a %.2f v %zu\n", r.x0, r.y0, r.x1, r.y1, r.alfa, r.v);
			}
		}
		// (los CONTROLES son del juego entero, no de la carta: no marcan el caso de la carta)
		if (!cartaCaso.empty() && oraculo.rfind("CONTROLES", 0) != 0) {
			cu::Resultado& r = cu::de("CU-HU-CARTA-" + cartaCaso);
			r.comprobaciones++;
			if (r.estado != "FALLA") r.estado = "FALLA", r.detalle = oraculo + ": " + d;
			else if (r.detalle.size() < 400) r.detalle += " || " + oraculo + ": " + b;
		}
	}
	void bien(const std::string& oraculo)
	{
		cu::de("CU-HU-" + oraculo).comprobaciones++;
		if (!cartaCaso.empty()) cu::de("CU-HU-CARTA-" + cartaCaso).comprobaciones++;
	}

	bool enDuelo() const { return g.ctl.vista.pantalla == VD::DUELO && !g.ctl.vista.sorteo; }
	bool alDia() const
	{
		return g.sucesosVistos >= g.dueloIA.sucesos.size() && !g.hayAtaque() && !g.ctl.vuelo.activo && !g.ctl.robo.activo && g.reparto <= 0 && !g.mesaPendiente &&
		       g.esperaEfecto <= 0.0f && !ritmo::estado().ocupado();
	}
	bool pausado() const { return g.pausa.abierta || g.ajustesAbiertos() || g.constructorMazo.abierto; }

	// ---------------------------------------------------------------------------------------- ENTERARSE
	struct Evento {
		size_t k;
		ia::Suceso s;
		float tMotor, tPresenta{ -1.0f }, visto{ 0.0f }, plazo{ 0.0f };
		std::string nombre;
		int lpAntes{ 0 }, lpDespues{ 0 };
		bool salida{ false };   // sale de la mesa (cementerio, desterrada, mano): tiene que verse el viaje o su nombre
	};
	std::vector<Evento> eventos;
	size_t nSucesos{ 0 };
	int lpJ{ 8000 };
	std::unordered_map<std::string, std::vector<pantalla::Rect>> antes;   // las cartas del cuadro anterior (movimiento)
	int vistos{ 0 }, sinVer{ 0 };

	std::string nombreDe(const std::string& id) const
	{
		const int ci = p.ui.buscarCarta(id);
		return ci >= 0 ? p.ui.cartas[(size_t)ci].vNombre() : id;
	}
	bool relevante(const ia::Suceso& s) const
	{
		// (la carta del caso, del jugador: su invocacion / activacion tambien tiene que verse)
		if (s.lado == 0 && !cartaCaso.empty() && s.carta == cartaCaso && (s.tipo == ia::S_INVOCA || s.tipo == ia::S_ACTIVA_MT || s.tipo == ia::S_ACTIVA_EFECTO)) return true;
		if (s.lado == 0)
			return s.tipo == ia::S_AL_CEMENTERIO || s.tipo == ia::S_DESTERRADO || s.tipo == ia::S_A_LA_MANO || s.tipo == ia::S_DESCARTA || s.tipo == ia::S_POSICION ||
			       s.tipo == ia::S_VOLTEA || s.tipo == ia::S_CONTROL || s.tipo == ia::S_ROBO || s.tipo == ia::S_DANNO || (s.tipo == ia::S_LP && s.valor < 0);
		return s.tipo == ia::S_ACTIVA_MT || s.tipo == ia::S_ACTIVA_EFECTO;
	}
	// se ve el suceso en este cuadro?
	bool senal(const Evento& e) const
	{
		const Cuadro& c = p.cuadro;
		if (e.s.tipo == ia::S_DANNO || e.s.tipo == ia::S_LP) {
			const int v = std::abs(e.s.valor);
			const std::string a = std::to_string(v), b = "-" + a;
			for (const Texto& t : c.textos) {
				if (t.icono || t.tapado || t.col.a < 0.25f) continue;
				if (t.s == a || t.s == b) return true;
				// (los LP del jugador rodando: un valor entre el de antes y el de despues)
				if (t.y > 900.0f && t.x < 450.0f && !t.s.empty() && isdigit((unsigned char)t.s[0])) {
					const int x = atoi(t.s.c_str());
					if (x < std::max(e.lpAntes, e.lpDespues) && x > std::min(e.lpAntes, e.lpDespues)) return true;
				}
			}
			return false;
		}
		if (!e.s.carta.empty()) {
			for (const Carta& k : c.cartas) {
				if (k.id != e.s.carta || k.tapada || k.alfa < 0.25f || k.cx() < 500.0f) continue;
				if (!k.en3D && k.cy() < 880.0f) return true;   // en grande / viajando en 2D (cartel, activacion, cadena)
				if (e.s.tipo == ia::S_ROBO && !k.en3D) return true;   // (el robo: en la mano)
				auto it = antes.find(k.id);
				if (it != antes.end())
					for (const pantalla::Rect& r : it->second) {
						const float d = std::hypot(r.cx() - k.cx(), r.cy() - k.cy());
						const float da = r.area() > 1.0f ? fabsf(k.area() / r.area() - 1.0f) : 0.0f;
						if ((d > 1.0f && d < 420.0f) || (d < 420.0f && da > 0.03f)) return true;   // se mueve, crece o gira
					}
				// activacion del rival: su cara a la vista (estaba boca abajo o en la mano)
				if (e.s.lado == 1 && !e.salida) return true;
			}
			if (!e.nombre.empty())
				for (const Texto& t : c.textos)
					if (!t.icono && !t.tapado && t.col.a >= 0.25f && t.x > 500.0f && contiene(t.s, e.nombre)) return true;
		}
		return false;
	}
	std::set<std::string> sacrificadas;   // las cartas que el jugador eligio como sacrificio (en pantalla: ELEGIR_TRIBUTOS)
	void enterarse()
	{
		ia::Duelo& d = g.dueloIA;
		if (g.ctl.estado == control::ELEGIR_TRIBUTOS)
			for (int z : g.ctl.tributos)
				if (z >= 0 && z < control::NUM_ZONAS_MESA) sacrificadas.insert(g.ctl.zonas[(size_t)z].carta);
		if (d.sucesos.size() < nSucesos || g.sucesosVistos > d.sucesos.size()) {   // (otra escena / otro duelo)
			eventos.clear();
			nSucesos = d.sucesos.size();
		}
		int lp = lpJ;
		for (size_t k = nSucesos; k < d.sucesos.size(); k++) {
			const ia::Suceso& s = d.sucesos[k];
			if (!relevante(s)) continue;
			Evento e{ k, s, t };
			e.nombre = s.carta.empty() ? "" : nombreDe(s.carta);
			e.salida = s.tipo == ia::S_AL_CEMENTERIO || s.tipo == ia::S_DESTERRADO || s.tipo == ia::S_A_LA_MANO || s.tipo == ia::S_DESCARTA;
			if (s.tipo == ia::S_DANNO || s.tipo == ia::S_LP) {
				e.lpAntes = lp;
				lp -= std::abs(s.valor);
				e.lpDespues = lp;
			}
			// (robo del jugador sin carta conocida o suceso sin nada que mirar: no se juzga)
			if (s.carta.empty() && s.tipo != ia::S_DANNO && s.tipo != ia::S_LP) continue;
			eventos.push_back(e);
		}
		nSucesos = d.sucesos.size();
		lpJ = d.l[0].lp;
		const bool corre = !pausado();
		for (size_t i = 0; i < eventos.size();) {
			Evento& e = eventos[i];
			if (e.tPresenta < 0.0f && g.sucesosVistos > e.k) e.tPresenta = t;
			if (senal(e)) e.visto += dt;
			if (getenv("HU_DEBUG") && cuadroN % 6 == 0) {
				std::string v;
				for (const Carta& k : p.cuadro.cartas)
					if (k.id == e.s.carta) v += " " + std::to_string((int)k.cx()) + "," + std::to_string((int)k.cy()) + (k.en3D ? "3d" : "2d") + (k.tapada ? "T" : "") + "a" + std::to_string(k.alfa).substr(0, 4);
				fprintf(stderr, "[juez] %s %s: t %.1f presentado %d visto %.2f |%s\n", ia::NOMBRE_SUCESO[e.s.tipo], e.s.carta.c_str(), t - e.tMotor, (int)(e.tPresenta >= 0.0f), e.visto, v.c_str());
			}
			if (corre && e.tPresenta >= 0.0f) e.plazo += dt;
			bool fin = false;
			// (el giro de un cambio de posicion / volteo dura 0.33 s: con UMBRAL_GIRO alcanza)
			const float umbral = e.s.tipo == ia::S_POSICION || e.s.tipo == ia::S_VOLTEA ? UMBRAL_GIRO : UMBRAL_VISTO;
			bool sacrificio = e.s.tipo == ia::S_AL_CEMENTERIO && e.s.lado == 0 && sacrificadas.count(e.s.carta);
			// (o el motor lo dice: al cementerio y enseguida la invocacion del jugador)
			for (size_t q = e.k + 1; q < d.sucesos.size() && q <= e.k + 3 && e.s.tipo == ia::S_AL_CEMENTERIO && e.s.lado == 0; q++)
				if (d.sucesos[q].tipo == ia::S_INVOCA && d.sucesos[q].lado == 0) sacrificio = true;
			// (el coste que el jugador paga al activar su carta: descarte / al cementerio / desterrar justo con su activacion)
			bool coste = false;
			if (e.s.lado == 0 && e.salida)
				for (size_t q = e.k > 3 ? e.k - 3 : 0; q < d.sucesos.size() && q <= e.k + 2; q++)
					if (q != e.k && (d.sucesos[q].tipo == ia::S_ACTIVA_MT || d.sucesos[q].tipo == ia::S_ACTIVA_EFECTO) && d.sucesos[q].lado == 0) coste = true;
			sacrificio = sacrificio || (coste && e.visto < umbral);
			if (e.visto >= umbral) {
				bien("ENTERARSE");
				vistos++;
				fin = true;
			} else if (e.tPresenta >= 0.0f && e.plazo > VENTANA_SUCESO && sacrificio) {
				// (el sacrificio que el jugador eligio: se anota como decision, no como falla)
				cu::decision("CU-HU-ENTERARSE-COSTE", "lo que el propio jugador sacrifica o paga de coste (" + e.s.carta + ", " + ia::NOMBRE_SUCESO[e.s.tipo] +
				                                      ") sale sin viaje ni aviso visible (" + std::to_string(e.visto).substr(0, 4) + " s): decidir si hace falta una animacion");
				fin = true;
			} else if (e.tPresenta >= 0.0f && e.plazo > VENTANA_SUCESO) {
				sinVer++;
				falla("ENTERARSE", "%s %s (%s) del %s se vio %.2f s (minimo %.2f) en los %.0f s tras presentarse (suceso %d)", ia::NOMBRE_SUCESO[e.s.tipo], e.s.carta.c_str(),
				      e.nombre.c_str(), e.s.lado ? "rival" : "jugador", e.visto, UMBRAL_VISTO, VENTANA_SUCESO, (int)e.k);
				fin = true;
			} else if (e.tPresenta < 0.0f && corre && t - e.tMotor > NUNCA_PRESENTADO && enDuelo() && d.ganador < 0) {
				sinVer++;
				falla("ENTERARSE", "%s %s del %s: el motor lo hizo hace %.0f s y la pantalla nunca lo presento (sucesos vistos %d de %d)", ia::NOMBRE_SUCESO[e.s.tipo], e.s.carta.c_str(),
				      e.s.lado ? "rival" : "jugador", t - e.tMotor, (int)g.sucesosVistos, (int)d.sucesos.size());
				fin = true;
			}
			if (fin) eventos.erase(eventos.begin() + (long)i);
			else i++;
		}
		antes.clear();
		for (const Carta& k : p.cuadro.cartas)
			if (!k.id.empty()) antes[k.id].push_back(k);
	}

	// ---------------------------------------------------------------------------------------- VENTANA-VISIBLE
	float tSinVentana{ 0.0f };
	bool preguntaJugador() const
	{
		const ia::Duelo& d = g.dueloIA;
		return enDuelo() && d.ganador < 0 && d.ef.preg.tipo != ia::P_NADA && d.ef.preg.lado == 0;
	}
	void ventana(const Lectura& L)
	{
		const bool tapada = pausado() || L.pausa || L.selector || L.detalle() || g.consulta.vis.abierto;
		if (preguntaJugador() && alDia() && !tapada) {
			const bool seVe = L.menuVisible() || L.eligiendo();
			tSinVentana = seVe ? 0.0f : tSinVentana + dt;
			if (seVe && tSinVentana == 0.0f) {
				if (!vioVentana) bien("VENTANA-VISIBLE");
				vioVentana = true;
			}
			if (tSinVentana > PLAZO_VENTANA) {
				falla("VENTANA-VISIBLE", "el motor pregunta al jugador (tipo %d, carta %s, ventana %d) y no hay pregunta en pantalla hace %.1f s: %s", (int)g.dueloIA.ef.preg.tipo,
				      g.dueloIA.ef.preg.carta.c_str(), (int)g.dueloIA.ef.ventana.tipo, tSinVentana, L.resumen().c_str());
				tSinVentana = -30.0f;   // (no repetir enseguida)
			}
		} else {
			if (tSinVentana > 0.0f) tSinVentana = 0.0f;
			if (!preguntaJugador()) vioVentana = false;
			if (tSinVentana < 0.0f) tSinVentana = std::min(0.0f, tSinVentana + dt);
		}
	}
	bool vioVentana{ false };

	// ---------------------------------------------------------------------------------------- NUNCA-TRANCADO
	std::string firma;
	float tQuieto{ 0.0f }, tFantasma{ 0.0f }, tFase{ 0.0f }, tSinPregunta{ 0.0f };
	bool avisoAntes{ false }, avisoFantasma{ false };
	int faseAntes{ -1 };
	bool trancado{ false };
	void trancar(const Lectura& L)
	{
		const ia::Duelo& d = g.dueloIA;
		char b[200];
		snprintf(b, sizeof(b), "%d|%d|%d|%zu|%d|%d|%d|%d|%zu", g.ctl.vista.pantalla, d.turno, (int)d.fase, d.sucesos.size(), (int)d.ef.preg.tipo, g.ctl.estado, d.ganador,
		         (int)g.ctl.vista.sorteo, g.sucesosVistos);
		// (la misma fase del jugador mucho tiempo, aunque se abran y cierren menus: tambien pide el rescate)
		const int ff = d.turno * 10 + (int)d.fase;
		tFase = ff != faseAntes || d.decide() != 0 || !enDuelo() ? 0.0f : tFase + dt;
		faseAntes = ff;
		if (firma != b || !enDuelo() || d.ganador >= 0 || pausado()) {
			firma = b;
			tQuieto = 0.0f;
		} else {
			tQuieto += dt;
		}
		// (con la decision del jugador y la mesa al dia, quien juzga es el rescate del humano: si cerrando todo y con lo que
		// la pantalla ofrece nada avanza, es falla; aca solo la espera del rival o una animacion que no termina)
		const bool delJugador = d.decide() == 0 && alDia();
		if (tQuieto > PLAZO_TRANCADO && !trancado && !delJugador) {
			trancado = true;
			falla("NUNCA-TRANCADO", "%.0f s sin que nada avance (decide %d, pregunta %d lado %d, estado %s, bloqueado %d, al dia %d): %s", tQuieto, d.decide(), (int)d.ef.preg.tipo, d.ef.preg.lado,
			      control::NOMBRE_ESTADO[g.ctl.estado], (int)g.ctl.bloqueado, (int)alDia(), L.resumen().c_str());
		}
		// "Primero responde a la pregunta" sin pregunta a la vista
		// (solo si el aviso APARECE sin pregunta a la vista desde hace 1 s: el que quedo de una pregunta ya contestada no cuenta)
		const bool seVePregunta = L.menuVisible() || L.eligiendo();
		tSinPregunta = seVePregunta ? 0.0f : tSinPregunta + dt;
		const bool hayAviso = contiene(L.aviso, "Primero responde");
		if (hayAviso && !avisoAntes) avisoFantasma = tSinPregunta > 1.0f && !L.pausa;
		if (!hayAviso) avisoFantasma = false;
		avisoAntes = hayAviso;
		const bool fantasma = avisoFantasma && !seVePregunta && !L.pausa;
		tFantasma = fantasma ? tFantasma + dt : 0.0f;
		if (tFantasma > PLAZO_FANTASMA) {
			falla("NUNCA-TRANCADO", "dice 'Primero responde a la pregunta' y no hay pregunta a la vista (pregunta del motor %d lado %d, estado %s): %s", (int)d.ef.preg.tipo, d.ef.preg.lado,
			      control::NOMBRE_ESTADO[g.ctl.estado], L.resumen().c_str());
			tFantasma = -30.0f;
		}
	}

	// ---------------------------------------------------------------------------------------- MESA-COHERENTE
	float tAlDia{ 0.0f }, tReintento{ 0.0f }, tEscena{ -99.0f };
	bool mesaMirada{ false };
	// la carta dibujada en una zona del tablero: "" vacia, "~" dorso, o el id
	std::string enZona(int zi, bool* defensa = nullptr) const
	{
		const pantalla::Rect& z = zona[(size_t)zi];
		const Carta* mejor = nullptr;
		for (const Carta& k : p.cuadro.cartas) {
			if (!k.en3D || k.alfa < 0.5f || !z.contiene(k.cx(), k.cy())) continue;
			if (!mejor || k.v > mejor->v) mejor = &k;   // (la de arriba)
		}
		if (defensa && mejor) *defensa = mejor->x1 - mejor->x0 > (mejor->y1 - mejor->y0) * 2.6f;
		return !mejor ? "" : mejor->id.empty() ? "~" : mejor->id;
	}
	void mesa(const Lectura& L)
	{
		const ia::Duelo& d = g.dueloIA;
		// (rama classic-arreglo-ciclo-md: eligiendo la zona para dejar una carta, la interfaz dibuja la carta "fantasma"
		// translucida (alfa 0.30-0.60) en la zona con el foco, tambien sobre el monstruo que se va a sacrificar: es la vista
		// previa de la jugada, no la mesa. Los ~25 MESA-COHERENTE de r3 eran eso: en la escena el humano invocaba)
		const bool quieto = enDuelo() && d.ganador < 0 && alDia() && !pausado() && !L.pausa && !L.detalle() && !g.consulta.vis.abierto && !g.ctl.vuelo.activo &&
		                    g.ctl.estado != control::ELEGIR_ZONA;
		tAlDia = quieto ? tAlDia + dt : 0.0f;
		if (!quieto) mesaMirada = false;
		// (tras armar una escena a mano la interfaz tarda en soltar lo de la escena anterior: 6 s de gracia)
		if (tAlDia < 0.6f || mesaMirada || tAlDia < tReintento || t - tEscena < 6.0f) return;
		mesaMirada = true;
		std::string dif;
		char b[300];
		for (int lado = 0; lado < 2 && dif.empty(); lado++)
			for (int i = 0; i < ia::ZONAS && dif.empty(); i++) {
				for (int mt = 0; mt < 2 && dif.empty(); mt++) {
					const int zi = zonaInterfaz(Juego::zonaMesaIA(lado, i, mt == 1));
					if (zi < 0) continue;
					std::string id, ve = enZona(zi);
					bool abajo = false;
					if (mt) id = d.l[(size_t)lado].mt[(size_t)i].id, abajo = d.l[(size_t)lado].mt[(size_t)i].bocaAbajo;
					else id = d.l[(size_t)lado].m[(size_t)i].id, abajo = d.l[(size_t)lado].m[(size_t)i].bocaAbajo;
					const bool ok = id.empty() ? ve.empty() : abajo ? (ve == "~" || (lado == 0 && ve == id)) : ve == id;
					if (!ok) {
						snprintf(b, sizeof(b), "%s %s %d: se ve '%s', el duelo tiene '%s'%s", lado ? "rival" : "jugador", mt ? "magia/trampa" : "monstruo", i, ve.c_str(), id.c_str(),
						         abajo ? " boca abajo" : "");
						dif = b;
					}
				}
			}
		// la mano del jugador (caras abajo) y la del rival (dorsos arriba)
		if (dif.empty()) {
			std::vector<std::string> mano, motor = d.l[0].mano;
			int dorsosR = 0;
			for (const Carta& k : p.cuadro.cartas) {
				if (k.en3D || k.alfa < 0.5f) continue;
				if (k.cy() > 880.0f && k.cx() > 500.0f && !k.id.empty()) mano.push_back(k.id);
				if (k.cy() < 140.0f && k.cx() > 500.0f && k.id.empty()) dorsosR++;
			}
			std::sort(mano.begin(), mano.end());
			std::sort(motor.begin(), motor.end());
			if (mano != motor) {
				snprintf(b, sizeof(b), "la mano dibujada tiene %d cartas y el duelo %d", (int)mano.size(), (int)motor.size());
				dif = b;
				for (size_t k = 0; k < mano.size() && k < motor.size(); k++)
					if (mano[k] != motor[k]) {
						dif += " (" + mano[k] + " / " + motor[k] + ")";
						break;
					}
			} else if (dorsosR != (int)d.l[1].mano.size() && (int)d.l[1].mano.size() <= interfaz::NUM_MANO_R) {
				snprintf(b, sizeof(b), "la mano del rival dibujada tiene %d dorsos y el duelo %d cartas", dorsosR, (int)d.l[1].mano.size());
				dif = b;
			}
		}
		// los LP de los paneles
		if (dif.empty())
			for (int lado = 0; lado < 2 && dif.empty(); lado++) {
				const float y0 = lado ? 60.0f : 990.0f, y1 = lado ? 150.0f : 1075.0f;
				bool hay = false;
				for (const Texto& t : p.cuadro.textos)
					if (!t.icono && !t.tapado && t.x < 400.0f && t.y > y0 && t.y < y1 && t.s == std::to_string(std::max(0, d.l[(size_t)lado].lp))) hay = true;
				if (!hay) {
					snprintf(b, sizeof(b), "los LP %s del duelo (%d) no estan en su panel", lado ? "del rival" : "del jugador", d.l[(size_t)lado].lp);
					dif = b;
				}
			}
		// (los LP ruedan y las cartas se acomodan: se reintenta hasta 3 s de mesa quieta antes de la falla)
		if (!dif.empty() && tAlDia < 3.0f) {
			mesaMirada = false;
			tReintento = tAlDia + 0.5f;
			return;
		}
		tReintento = 0.0f;
		if (dif.empty()) bien("MESA-COHERENTE");
		else falla("MESA-COHERENTE", "%s", dif.c_str());
	}

	// cada cuadro
	void cuadro()
	{
		t += dt;
		cuadroN++;
		const Lectura L = leer(p.cuadro);
		enterarse();
		ventana(L);
		trancar(L);
		mesa(L);
		danoUnico();
	}
	// rama classic-ps5-ataque: DANNO-UNICO: un golpe de combate muestra UN numero de danno (MD: ShowDamageImpl -> DuelLP, un
	// solo ChangeValue). Falla si durante el ataque se dibuja dos veces el numero de su danno (antes: "-2500" dos veces).
	// (Fuera del combate dos numeros iguales pueden ser de los dos lados: Ring of Destruction, Rain of Mercy)
	void danoUnico()
	{
		if (!g.hayAtaque() || g.danio <= 0) return;
		const std::string suyo = "-" + std::to_string(g.danio);
		std::map<std::string, int> numeros;
		for (const Texto& x : p.cuadro.textos) {
			if (x.col.a <= 0.1f || x.s.size() < 2 || (x.s[0] != '-' && x.s[0] != '+')) continue;
			bool cifras = true;
			for (size_t i = 1; i < x.s.size(); i++) cifras &= x.s[i] >= '0' && x.s[i] <= '9';
			if (cifras && x.s == suyo) numeros[x.s]++;
		}
		for (const auto& n : numeros) {
			if (n.second >= 2) {
				falla("DANNO-UNICO", "el numero de danno %s se dibuja %d veces en el mismo cuadro (MD muestra uno)", n.first.c_str(), n.second);
				return;
			}
		}
		if (!numeros.empty()) bien("DANNO-UNICO");
	}
	void reiniciar()
	{
		eventos.clear();
		sacrificadas.clear();
		nSucesos = g.dueloIA.sucesos.size();
		lpJ = g.dueloIA.l[0].lp;
		tSinVentana = tQuieto = tFantasma = tAlDia = 0.0f;
		trancado = false;
		mesaMirada = false;
		firma.clear();
	}
	// lo que falta juzgar: los sucesos todavia en plazo se miran hasta el final (el que no se vio, falla)
	void cerrarEventos()
	{
		for (const Evento& e : eventos)
			if (e.tPresenta >= 0.0f && e.visto < UMBRAL_VISTO && e.plazo > 1.0f) {
				sinVer++;
				falla("ENTERARSE", "%s %s (%s) del %s se vio %.2f s (minimo %.2f) antes de terminar la escena", ia::NOMBRE_SUCESO[e.s.tipo], e.s.carta.c_str(), e.nombre.c_str(),
				      e.s.lado ? "rival" : "jugador", e.visto, UMBRAL_VISTO);
			}
		eventos.clear();
	}
};

// ------------------------------------------------------------------------------------------------- el jugador
struct Humano {
	Juego& g;
	cuft::Bot& b;
	pantalla::Pantalla& p;
	Juez& j;
	uint32_t azar;
	std::deque<std::string> botones;
	int nBotones{ 0 };
	float travesura{ 0.12f };   // probabilidad de una travesura por decision
	float probResponder{ 0.45f };
	std::string preferida;      // la carta que se busca jugar / activar (carta por carta)
	bool lento{ false };

	Humano(Juego& gg, cuft::Bot& bb, pantalla::Pantalla& pp, Juez& jj, uint32_t semilla) : g(gg), b(bb), p(pp), j(jj), azar(semilla * 2654435761u + 7u)
	{
		j.botones = &botones;
	}
	uint32_t rnd() { return azar = azar * 1664525u + 1013904223u, azar >> 8; }
	float rnd01() { return (float)(rnd() & 0xFFFF) / 65536.0f; }
	int rndN(int n) { return n <= 0 ? 0 : (int)(rnd() % (uint32_t)n); }

	const Cuadro& ve() const { return p.cuadro; }
	Lectura lee() const { return leer(p.cuadro); }

	// ---- ritmo humano
	float reaccion()
	{
		const float r = rnd01();
		if (lento || r < 0.08f) return 1.5f + 1.5f * rnd01();   // (a veces se distrae)
		return 0.3f + 1.2f * rnd01() * rnd01();                 // (casi siempre rapido)
	}
	void mirar(float s) { b.esperar(s); }
	void pulsar(uint32_t pad, float espera = -1.0f)
	{
		char x[48];
		snprintf(x, sizeof(x), "%.1f:%s", j.t, cuft::nombreBoton(pad));
		botones.push_back(x);
		if (botones.size() > 40) botones.pop_front();
		nBotones++;
		const float sost = 0.07f + 0.1f * rnd01();
		for (float t = 0.0f; t < sost; t += g.frameTimer) b.cuadro(pad);
		b.cuadro(0);
		mirar(espera >= 0.0f ? espera : reaccion());
	}

	// ---- moverse con las flechas hasta lo que se ve en (x, y)
	bool marcoEn(const pantalla::Rect& r) const
	{
		const Cuadro& c = ve();
		if (!c.hayMarco) return false;
		const float mx = c.marco.cx(), my = c.marco.cy();
		return mx >= r.x0 - 6.0f && mx <= r.x1 + 6.0f && my >= r.y0 - 6.0f && my <= r.y1 + 6.0f;
	}
	// a la carta con ese id (la mano o la mesa propia): flechas mirando el recuadro dorado
	bool irACarta(const std::string& id, bool mano)
	{
		int sinMover = 0;
		for (int intento = 0; intento < 16; intento++) {
			const Carta* k = buscarPropia(id, mano);
			if (!k) return false;
			const pantalla::Rect r = *k;
			if (marcoEn(r)) return true;
			const Cuadro& c = ve();
			if (!c.hayMarco) {
				pulsar(PAD_RIGHT, 0.25f);
				continue;
			}
			const float dx = r.cx() - c.marco.cx(), dy = r.cy() - c.marco.cy();
			if (getenv("HU_DEBUG"))
				fprintf(stderr, "[humano] ir a %s (%s): carta %.0f,%.0f-%.0f,%.0f marco %.0f,%.0f-%.0f,%.0f foco %d/%d\n", id.c_str(), mano ? "mano" : "mesa", r.x0, r.y0, r.x1, r.y1, c.marco.x0,
				        c.marco.y0, c.marco.x1, c.marco.y1, g.ctl.foco.tipo, g.ctl.foco.i);
			// (en la misma fila, de lado; de una fila a otra, arriba / abajo; si la flecha no movio el recuadro, el otro eje)
			// (en la mano el recuadro queda arriba de la carta levantada: ahi solo de lado)
			const bool otraFila = mano && c.marco.cy() > 820.0f ? false : fabsf(dy) > 70.0f && fabsf(dy) > 0.6f * fabsf(dx);
			bool vertical = otraFila;
			if (sinMover >= 2) vertical = !vertical;
			const uint32_t pad = vertical ? (dy > 0 ? PAD_DOWN : PAD_UP) : (dx > 0 ? PAD_RIGHT : PAD_LEFT);
			const float mx = c.marco.cx(), my = c.marco.cy();
			pulsar(pad, 0.12f + 0.25f * rnd01());
			sinMover = fabsf(ve().marco.cx() - mx) + fabsf(ve().marco.cy() - my) < 4.0f ? sinMover + 1 : 0;
			if (sinMover >= 4) return false;
		}
		const Carta* k = buscarPropia(id, mano);
		return k && marcoEn(*k);
	}
	const Carta* buscarPropia(const std::string& id, bool mano) const
	{
		for (const Carta& k : ve().cartas) {
			if (k.id != id || k.tapada || k.alfa < 0.5f) continue;
			if (mano && !k.en3D && k.cy() > 880.0f) return &k;
			if (!mano && k.en3D && k.cy() > 560.0f && k.cx() > 500.0f) return &k;
		}
		return nullptr;
	}
	std::vector<std::string> manoVista() const
	{
		std::vector<std::string> r;
		std::vector<std::pair<float, std::string>> v;
		for (const Carta& k : ve().cartas)
			if (!k.en3D && k.cy() > 880.0f && k.cx() > 500.0f && !k.id.empty() && k.alfa > 0.5f) v.push_back({ k.cx(), k.id });
		std::sort(v.begin(), v.end());
		for (auto& x : v) r.push_back(x.second);
		return r;
	}
	std::vector<std::string> mesaPropia() const
	{
		std::vector<std::string> r;
		for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
			if (strncmp(interfaz::ZONAS[z].id, "jugador_", 8) != 0) continue;
			const std::string tipo = interfaz::ZONAS[z].tipo;
			if (tipo != "monstruo" && tipo != "magia") continue;
			const std::string id = j.enZona(z);
			if (!id.empty() && id != "~") r.push_back(id);
		}
		return r;
	}

	// ---- en un menu: llevar el cursor a la opcion k y X
	bool elegir(int k)
	{
		for (int v = 0; v < 12; v++) {
			const Lectura L = lee();
			if (!L.menu.abierto || L.menu.sel < 0) return false;
			if (L.menu.sel == k) {
				pulsar(PAD_CROSS);
				return true;
			}
			pulsar(L.menu.sel < k ? PAD_DOWN : PAD_UP, 0.1f + 0.2f * rnd01());
		}
		return false;
	}
	static int buscarOp(const Menu& m, std::initializer_list<const char*> quiero)
	{
		for (const char* q : quiero)
			for (int k = 0; k < (int)m.ops.size(); k++)
				if (m.activas[(size_t)k] && contiene(m.ops[(size_t)k], q)) return k;
		return -1;
	}

	// ---- contestar una pregunta del duelo (menu con "No activar" o una eleccion obligatoria)
	void responder(const Lectura& L)
	{
		const Menu& m = L.menu;
		int k = -1;
		const std::string nom = preferida.empty() ? "" : j.nombreDe(preferida);
		if (!nom.empty())
			for (int o = 0; o < (int)m.ops.size(); o++)
				if (m.activas[(size_t)o] && (contiene(m.ops[(size_t)o], nom) || contiene(m.titulo, nom)) && !contiene(m.ops[(size_t)o], "No ")) k = o;
		if (k < 0 && rnd01() < probResponder) {
			std::vector<int> si;
			for (int o = 0; o < (int)m.ops.size(); o++)
				if (m.activas[(size_t)o] && !contiene(m.ops[(size_t)o], "No ") && m.ops[(size_t)o] != "Cancelar") si.push_back(o);
			if (!si.empty()) k = si[(size_t)rndN((int)si.size())];
		}
		if (k < 0) {
			k = buscarOp(m, { "No activar", "No ", "Pasar", "No" });
			if (k < 0 && L.dice("O", "No activar")) {
				pulsar(PAD_CIRCLE);
				return;
			}
		}
		if (k < 0) k = m.sel >= 0 ? m.sel : 0;
		elegir(k);
	}

	// ---- las travesuras: botones de mas en cualquier momento (y lo que dicen los controles)
	void travesuraAhora()
	{
		const int q = rndN(9);
		const Lectura L0 = lee();
		const ia::Duelo& d = g.dueloIA;
		if (q == 0) {   // el cuadrado: como OPTIONS (rama classic-ps5-control2), el selector de fases; si no, nada
			const std::string antes = firmaControles();
			const bool sinNada = L0.libre() && j.alDia() && !L0.menu.abierto && !L0.selector && !L0.pausa;
			pulsar(PAD_SQUARE, 0.5f);
			if (lee().selector) {
				j.bien("CONTROLES-CUADRADO");
				pulsar(PAD_CIRCLE, 0.4f);
			} else if (sinNada && j.alDia()) {
				const std::string despues = firmaControles();
				if (antes == despues) j.bien("CONTROLES-CUADRADO");
				else j.falla("CONTROLES-CUADRADO", "el cuadrado cambio algo: %s -> %s", antes.c_str(), despues.c_str());
			}
		} else if (q == 1) {   // el engranaje (panel tactil): la pausa con sus opciones
			if (!j.enDuelo() || L0.pausa) return;
			pulsar(PAD_TOUCH_PAD, 0.3f);
			Lectura L = lee();
			for (int k = 0; k < 10 && !(L.pausa && buscarOp(L.menu, { "Abandonar duelo" }) >= 0); k++) {   // (la pausa de MD entra animada)
				mirar(0.1f);
				L = lee();
			}
			const bool ok = L.pausa && buscarOp(L.menu, { "Continuar" }) >= 0 && buscarOp(L.menu, { "Opciones" }) >= 0 && buscarOp(L.menu, { "Construir mazo" }) >= 0 &&
			                buscarOp(L.menu, { "Abandonar duelo" }) >= 0;
			if (ok) j.bien("CONTROLES-ENGRANAJE");
			else j.falla("CONTROLES-ENGRANAJE", "el panel tactil no mostro la pausa con Continuar, Opciones, Construir mazo y Abandonar: %s", L.resumen().c_str());
			// la pausa detiene el duelo
			const size_t n0 = d.sucesos.size();
			const float ta = g.tAtaque;
			mirar(0.5f + rnd01());
			if (L.pausa) {
				if (d.sucesos.size() == n0 && (ta < 0.0f || g.tAtaque == ta)) j.bien("CONTROLES-PAUSA-DETIENE");
				else j.falla("CONTROLES-PAUSA-DETIENE", "con la pausa abierta el duelo siguio (sucesos %d -> %d; la animacion del ataque %.2f -> %.2f s)", (int)n0, (int)d.sucesos.size(), ta, g.tAtaque);
			}
			// cerrarla: Continuar, circulo u OPTIONS
			const Lectura L2 = lee();
			if (L2.pausa) {
				const int r = rndN(3);
				if (r == 0) elegir(buscarOp(L2.menu, { "Continuar" }));
				else pulsar(r == 1 ? PAD_CIRCLE : PAD_OPTIONS);
				mirar(0.3f);
				if (lee().pausa) j.falla("CONTROLES-ENGRANAJE", "la pausa no se cerro con %s", r == 0 ? "Continuar" : r == 1 ? "circulo" : "OPTIONS");
			}
		} else if (q == 2) {   // el triangulo: el detalle de lo que tiene el recuadro
			const bool hayDetalle = L0.dice("T", "Detalle");
			// (rama classic-ps5-ritmo: la carta del recuadro; si un ataque la destruye justo al apretar, ya no hay que mirar)
			auto enFoco = [&]() -> std::string {
				const control::Foco& f = g.ctl.foco;
				if (f.tipo == control::F_ZONA && f.i >= 0 && f.i < (int)control::NUM_ZONAS_MESA) return g.ctl.zonas[(size_t)f.i].carta;
				if (f.tipo == control::F_MANO_J && f.i >= 0 && f.i < (int)g.ctl.manoJ.size()) return g.ctl.manoJ[(size_t)f.i];
				return "-";
			};
			const std::string focoAntes = enFoco();
			// (rama classic-arreglo-ciclo-md: como Master Duel, el detalle se cierra solo al empezar un movimiento de carta
			// -EffectTaskCardMove.CommonMove -> DuelHUD.CloseCardInfo-: si el triangulo lo abrio en una animacion, puede
			// cerrarse antes de los 0.5 s. Cuenta que se abra en ese plazo)
			// (se mira cada cuadro desde que se aprieta: lo que se dibuja en pantalla)
			bool abrio = false;
			{
				char x[48];
				snprintf(x, sizeof(x), "%.1f:%s", j.t, cuft::nombreBoton(PAD_TRIANGLE));
				botones.push_back(x);
				if (botones.size() > 40) botones.pop_front();
				nBotones++;
				const float sost = 0.07f + 0.1f * rnd01();
				for (float t = 0.0f; t < sost; t += g.frameTimer) {
					b.cuadro(PAD_TRIANGLE);
					abrio = abrio || lee().detalle();
				}
				b.cuadro(0);
			}
			Lectura L = lee();
			abrio = abrio || L.detalle();
			for (int k = 0; k < 5 && !abrio; k++) {
				mirar(0.1f);
				L = lee();
				abrio = L.detalle();
			}
			if (hayDetalle && !L0.menu.abierto && (abrio || enFoco() == focoAntes)) {
				if (abrio) j.bien("CONTROLES-DETALLE");
				else if (j.enDuelo() && !L.pausa) j.falla("CONTROLES-DETALLE", "la ayuda decia 'T Detalle' y el triangulo no abrio el detalle: %s", L.resumen().c_str());
			}
			if (L.detalle()) {
				mirar(0.4f + rnd01());
				pulsar(rnd01() < 0.7f ? PAD_CIRCLE : PAD_TRIANGLE);
			}
		} else if (q == 3) {   // OPTIONS: solo fases (nunca la pausa)
			const bool fases = L0.dice("OPTIONS", "Fases") && !L0.menu.abierto;
			pulsar(PAD_OPTIONS, 0.5f);
			const Lectura L = lee();
			if (L.pausa && !L0.pausa) j.falla("CONTROLES-OPTIONS", "OPTIONS abrio la pausa (tiene que ser solo fases)");
			else if (fases) {
				if (L.selector || !L.aviso.empty()) j.bien("CONTROLES-OPTIONS");
				else j.falla("CONTROLES-OPTIONS", "la ayuda decia 'OPTIONS Fases' y OPTIONS no abrio el selector ni dio un motivo: %s", L.resumen().c_str());
			}
			if (L.selector) pulsar(PAD_CIRCLE);
		} else if (q == 4) {   // flechas al azar
			static const uint32_t F[4] = { PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT };
			for (int k = 0, n = 1 + rndN(3); k < n; k++) pulsar(F[rndN(4)], 0.1f + 0.2f * rnd01());
		} else if (q == 5) {   // L3: el registro, y cerrarlo
			pulsar(PAD_L3, 0.4f + rnd01());
			if (g.consulta.vis.abierto || lee().dice("O", "Cerrar")) pulsar(PAD_CIRCLE);
		} else if (q == 6) {   // la camara
			pulsar(rnd01() < 0.5f ? PAD_R1 : PAD_L1, 0.3f);
			pulsar(rnd01() < 0.5f ? PAD_R1 : PAD_L1, 0.3f);
		} else if (q == 7) {   // X de mas (dos veces rapido)
			if (!L0.menu.abierto) {
				pulsar(PAD_CROSS, 0.08f);
				pulsar(PAD_CROSS, 0.2f);
				const Lectura L = lee();
				if (L.menu.abierto && !L.pregunta()) pulsar(PAD_CIRCLE);
			}
		} else {   // circulo (atras) cuando la ayuda lo ofrece: tiene que hacer algo
			const std::string que = L0.ayudaDe("O");
			if (que.empty() || L0.pregunta()) return;
			const std::string antes = L0.resumen();
			pulsar(PAD_CIRCLE, 0.4f);
			const std::string despues = lee().resumen();
			if (despues != antes) j.bien("CONTROLES-AYUDA");
			else j.falla("CONTROLES-AYUDA", "la ayuda decia 'O %s' y el circulo no cambio nada: %s", que.c_str(), antes.c_str());
		}
	}
	std::string firmaControles() const
	{
		const ia::Duelo& d = g.dueloIA;
		char x[160];
		snprintf(x, sizeof(x), "turno %d fase %d sucesos %zu estado %s pausa %d selector %d", d.turno, (int)d.fase, d.sucesos.size(), control::NOMBRE_ESTADO[g.ctl.estado], (int)g.pausa.abierta,
		         (int)g.ctl.selFases.abierto);
		return x;
	}

	// ---- el selector de fases: a la accion con ese texto
	bool faseA(const char* accion)
	{
		Lectura L = lee();
		if (!L.selector) {
			pulsar(PAD_OPTIONS, 0.3f);
			L = lee();
			if (!L.selector) return false;
		}
		// (la ventana de MD entra animada: se espera a que se lea entera)
		for (int q = 0; q < 10; q++) {
			bool alguna = false;
			for (bool b : L.fasesOk) alguna |= b;
			if (alguna && L.faseSel >= 0) break;
			mirar(0.1f);
			L = lee();
			if (!L.selector) return false;
		}
		int k = -1;
		for (int i = 0; i < (int)L.fases.size(); i++)
			if (L.fases[(size_t)i] == accion && L.fasesOk[(size_t)i]) k = i;
		if (k < 0)
			for (int i = (int)L.fases.size(); i-- > 0;)
				if (L.fasesOk[(size_t)i]) k = i;   // (la ultima valida: terminar)
		if (k < 0) {
			pulsar(PAD_CIRCLE);
			return false;
		}
		if (getenv("HU_DEBUG")) fprintf(stderr, "[humano] selector: quiero %s -> %d; %s\n", accion, k, L.resumen().c_str());
		for (int v = 0; v < 6; v++) {
			L = lee();
			if (!L.selector) return false;
			if (L.faseSel == k) break;
			pulsar(L.faseSel < k ? PAD_RIGHT : PAD_LEFT, 0.12f + 0.2f * rnd01());
		}
		pulsar(PAD_CROSS);
		return true;
	}

	// ---- elegir (objetivo, sacrificio, carta, zona): X donde esta el recuadro; si no avanza, otra flecha
	int fallosElegir{ 0 };
	void elegirAlgo(const Lectura& L)
	{
		const std::string antes = L.resumen() + firmaControles();
		if (fallosElegir > 0 && fallosElegir % 2 == 1) {
			static const uint32_t F[4] = { PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT };
			pulsar(F[rndN(4)], 0.2f);
		}
		pulsar(PAD_CROSS);
		fallosElegir = (lee().resumen() + firmaControles()) == antes ? fallosElegir + 1 : 0;
		if (fallosElegir > 8 && L.ayudaDe("O") != "") {
			pulsar(PAD_CIRCLE);
			fallosElegir = 0;
		}
	}

	// ---- un turno propio con la mesa libre (lo que se ve): cartas de la mano y de la mesa, fases
	std::string faseVista;
	std::set<std::string> probadas;   // (fase|carta|opcion) ya intentadas en esta fase
	int accionesFase{ 0 };
	bool quiereBatalla{ true };
	// devuelve true si hizo algo
	bool jugarLibre(const Lectura& L)
	{
		if (L.fase != faseVista) {
			faseVista = L.fase;
			probadas.clear();
			accionesFase = 0;
			quiereBatalla = rnd01() < 0.85f;
		}
		const bool batalla = contiene(L.fase, "BATALLA");
		const bool principal = contiene(L.fase, "PRINCIPAL");
		const int maximo = batalla ? 5 : 4;
		if ((principal || batalla) && accionesFase < maximo) {
			// candidatas: la preferida primero; despues la mano y la mesa propia
			std::vector<std::pair<std::string, bool>> cand;
			if (!preferida.empty()) {
				if (buscarPropia(preferida, true)) cand.push_back({ preferida, true });
				if (buscarPropia(preferida, false)) cand.push_back({ preferida, false });
			}
			if (cand.empty() || rnd01() < 0.3f) {
				std::vector<std::pair<std::string, bool>> otras;
				if (!batalla)
					for (const std::string& id : manoVista()) otras.push_back({ id, true });
				for (const std::string& id : mesaPropia()) otras.push_back({ id, false });
				for (size_t k = otras.size(); k > 1; k--) std::swap(otras[k - 1], otras[(size_t)rndN((int)k)]);
				for (auto& o : otras) cand.push_back(o);
			}
			for (const auto& c : cand) {
				const std::string clave = L.fase + "|" + c.first + "|" + (c.second ? "m" : "z");
				if (probadas.count(clave)) continue;
				probadas.insert(clave);
				accionesFase++;
				if (!irACarta(c.first, c.second)) return true;
				pulsar(PAD_CROSS);
				const Lectura M = lee();
				if (!M.menu.abierto) {
					if (M.detalle() || g.consulta.vis.abierto) pulsar(PAD_CIRCLE);
					return true;
				}
				int k = -1;
				const bool esPreferida = c.first == preferida;
				if (batalla) k = buscarOp(M.menu, { "Atacar", "Ataque directo" });
				else if (esPreferida) k = buscarOp(M.menu, { "Invocar por", "Fusión", "Ritual", "Activar", "Invocar", "Voltear", "Colocar" });
				else if (rnd01() < 0.75f) k = buscarOp(M.menu, { "Invocar", "Activar", "Voltear", "Colocar", "Cambiar a" });
				if (k < 0) {
					pulsar(PAD_CIRCLE);
					return true;
				}
				elegir(k);
				return true;
			}
		}
		// a la fase siguiente con OPTIONS (el selector)
		const char* quiero = contiene(L.fase, "PRINCIPAL 1") && quiereBatalla ? "Iniciar Fase de Batalla" : batalla && rnd01() < 0.3f ? "Ir a M2" : "Finalizar turno";
		faseA(quiero);
		return true;
	}

	// ---- NUNCA-TRANCADO: sin progreso con la decision del jugador, cerrar todo y usar lo que la pantalla ofrece
	void rescate()
	{
		const std::string antes = lee().resumen();
		for (int k = 0; k < 4; k++) {
			const Lectura L = lee();
			if (L.pausa || L.selector || L.detalle() || g.consulta.vis.abierto || (L.menu.abierto && !L.pregunta()) || L.dice("O", "Cancelar") || L.dice("O", "Atrás")) pulsar(PAD_CIRCLE);
			else break;
		}
		for (int k = 0; k < 6 && j.tQuieto > 1.0f; k++) {
			const Lectura L = lee();
			if (L.pregunta()) responder(L);
			else if (L.menu.abierto) elegir(L.menu.sel >= 0 ? L.menu.sel : 0);
			else if (L.eligiendo()) elegirAlgo(L);
			else if (L.menuVisible()) pulsar(PAD_CROSS);
			else if (L.dice("OPTIONS", "Fases")) faseA("Finalizar turno");
			else if (L.detalle() || L.pausa || L.selector || g.consulta.vis.abierto || L.dice("O", "Cerrar")) pulsar(PAD_CIRCLE);
			else pulsar(k % 2 ? PAD_CROSS : PAD_OPTIONS);
		}
		mirar(4.0f);
		if (j.tQuieto > 10.0f && j.enDuelo() && g.dueloIA.ganador < 0) {
			j.trancado = true;
			j.falla("NUNCA-TRANCADO", "%.0f s sin avanzar con la decision del jugador y nada de lo que la pantalla ofrece lo destraba (pregunta del motor %d lado %d, estado %s): antes %s | despues %s",
			        j.tQuieto, (int)g.dueloIA.ef.preg.tipo, g.dueloIA.ef.preg.lado, control::NOMBRE_ESTADO[g.ctl.estado], antes.c_str(), lee().resumen().c_str());
		}
	}

	// ---- una decision: lo que se ve manda
	void paso()
	{
		if ((j.tQuieto > 25.0f || j.tFase > 120.0f) && !j.trancado) {
			rescate();
			j.tFase = 0.0f;
			return;
		}
		const Lectura L = lee();
		// (mirando, sin nada que hacer, las decisiones son seguidas: la travesura, mas rara)
		const bool mirando = !L.libre() && !L.menu.abierto && !L.eligiendo() && !L.pausa && !L.selector;
		if (rnd01() < (mirando ? travesura * 0.2f : travesura) && j.enDuelo()) {
			travesuraAhora();
			return;
		}
		if (L.pausa) {
			const int k = buscarOp(L.menu, { "Continuar" });
			if (k >= 0 && rnd01() < 0.5f) elegir(k);
			else pulsar(PAD_CIRCLE);
			return;
		}
		if (L.selector) {
			pulsar(PAD_CIRCLE);
			return;
		}
		if (L.menu.abierto) {
			if (L.pregunta()) responder(L);
			else if (L.dice("O", "Cancelar") || L.dice("O", "")) pulsar(PAD_CIRCLE);
			else elegir(L.menu.sel >= 0 ? L.menu.sel : 0);
			return;
		}
		if (L.eligiendo()) {
			elegirAlgo(L);
			return;
		}
		if (L.menuVisible()) {   // (un menu que no se leyo entero: la opcion que esta elegida)
			pulsar(PAD_CROSS);
			return;
		}
		fallosElegir = 0;
		if (L.detalle() || g.consulta.vis.abierto) {
			pulsar(PAD_CIRCLE);
			return;
		}
		// (rama classic-ia-red) la Fase Final con cartas de mas (p. ej. el rival lo encerro con Swords of Revealing Light y
		// Messenger of Peace y no pudo jugar nada): el juego avisa "Descarta hasta tener N cartas" y no deja terminar; una
		// persona cuenta sus cartas, va a una de la mano y la descarta (el menu "Descartar" lo elige el paso siguiente)
		if (j.enDuelo() && g.dueloIA.ganador < 0 && g.dueloIA.activo == 0 && g.dueloIA.decide() == 0 && g.dueloIA.fase == ia::F_FINAL) {
			const std::vector<std::string> mano = manoVista();
			if ((int)mano.size() > ia::limiteMano(g.dueloIA, 0)) {
				if (irACarta(mano[(size_t)rndN((int)mano.size())], true)) pulsar(PAD_CROSS);
				return;
			}
		}
		if (L.libre()) {
			jugarLibre(L);
			return;
		}
		// nada para hacer (el rival juega, una animacion): mirar
		mirar(0.2f + 0.4f * rnd01());
	}

	// ---- el sorteo (la moneda): X cuando lo pide, a veces cambiar de opcion
	bool sorteo(float maximo = 30.0f)
	{
		for (float t = 0.0f; t < maximo && g.ctl.vista.pantalla == VD::DUELO && g.ctl.vista.sorteo; t += 0.5f) {
			if (rnd01() < 0.3f) pulsar(rnd01() < 0.5f ? PAD_LEFT : PAD_RIGHT, 0.2f);
			pulsar(PAD_CROSS);
		}
		return g.ctl.vista.pantalla == VD::DUELO && !g.ctl.vista.sorteo;
	}
};

}  // namespace hu
