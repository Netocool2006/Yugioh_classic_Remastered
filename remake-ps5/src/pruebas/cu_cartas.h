// Casos de uso CARTA POR CARTA (rama classic-ciclos): cada una de las ~1775 cartas del juego.
//  - CU-CARTA-<id>-DATOS: la carta en ES (cartas_ui.txt) y EN (cartas_ui_en.txt) con nombre y texto, la misma clase,
//    nivel, ATK y DEF; el atributo y el tipo traducidos igual que en el resto de la coleccion; su linea de
//    cartas_classic.txt (passcode, limite Goat 0-3 y un subtipo que corresponde a la clase).
//  - CU-CARTA-<id>-CARAS: sus 6 caras (cara_, cara_en_, mini_, mini_en_, cara_hd_ y cara_hd_en_).
//  - CU-CARTA-<id>-INVOCAR (monstruos): se invoca como dice su texto (normal con sus sacrificios, colocar, Dioses con
//    3, fusion con Polymerization y su receta o por contacto, ritual con su magia, toon con Toon World, guardianes con
//    su arma). Las que su texto dice "no se puede invocar de modo normal" NO se invocan normal; las que lo dicen con
//    condicion (Cave Dragon, Terrorking Archfiend) se invocan cuando la condicion se cumple.
//  - CU-CARTA-<id>-COLOCAR (magias y trampas): se colocan boca abajo (la de campo en su zona).
//  - CU-CARTA-<id>-EFECTO (las que tienen efecto): el resultado de la certificacion de ESTE ciclo (caso a mano y
//    plantilla en el motor y en el front, prueba_certificacion.cpp).
//  - CU-CARTA-<id>-CAMPO (magias de campo): tienen su escenario en campos.h.
#pragma once

#include "casos_uso.h"
#include "cu_back.h"

#include <fstream>
#include <map>
#include <regex>
#include <sys/stat.h>

namespace cuc {

using namespace ia;

struct LineaUI { std::vector<std::string> c; };

inline std::map<std::string, std::vector<std::string>> leerTabla(const std::string& ruta)
{
	std::map<std::string, std::vector<std::string>> t;
	std::ifstream f(ruta);
	std::string l;
	while (std::getline(f, l)) {
		if (!l.empty() && l.back() == '\r') l.pop_back();
		if (l.empty() || l[0] == '#') continue;
		std::vector<std::string> c = ia::partir(l, '|');
		if (!c.empty()) t[c[0]] = c;
	}
	return t;
}
inline bool existe(const std::string& r)
{
	struct stat st{};
	return stat(r.c_str(), &st) == 0 && st.st_size > 0;
}
inline std::string minus(std::string s)
{
	for (char& ch : s) ch = (char)tolower((unsigned char)ch);
	return s;
}

// el texto dice que no se puede invocar de modo normal (y si es con condicion)
inline int textoNoNormal(const std::string& en)
{
	const std::string t = minus(en);
	static const std::regex frase("(cannot|can't) be normal summoned[^.]*\\.");
	std::smatch m;
	if (!std::regex_search(t, m, frase)) return 0;
	const std::string s = m.str();
	return s.find("unless") != std::string::npos || s.find("while") != std::string::npos ? 2 : 1;
}

// las del motor con condicion para invocar (motorPuedeInvocar): lo que hay que poner antes
inline void condicionInvocar(cub::Mesa& m, const std::string& id)
{
	static const char* const GUARDIANES[][2] = { { "guardian_grarl", "gravity_axe_grarl" },
		                                         { "guardian_kay_est", "rod_of_silence_kay_est" },
		                                         { "guardian_elma", "butterfly_dagger_elma" },
		                                         { "guardian_tryce", "twin_swords_of_flashing_light_tryce" },
		                                         { "guardian_baou", "wicked_breaking_flamberge_baou" },
		                                         { "guardian_ceal", "shooting_star_bow_ceal" } };
	for (const auto& g : GUARDIANES)
		if (id == g[0]) {
			const int z = m.colocar(0, g[1]);
			m.d.l[0].mt[(size_t)z].bocaAbajo = false;
		}
}

// invocar el monstruo "id" como dice su texto. Devuelve "" si salio bien o el motivo
inline std::string probarInvocar(const Base& base, const Carta& c, const std::string& textoEn)
{
	const std::string& id = c.id;
	const bool divina = Duelo::divina(&c);
	char buf[300];
	if (c.clase == "fusion") {
		auto it = base.fusiones.find(id);
		if (it == base.fusiones.end()) return "sin receta de fusion (rivales.txt F)";
		cub::Mesa m(&base);
		m.d.l[0].extra = { id };
		Jugada j;
		j.tipo = J_FUSION;
		j.carta = id;
		if (fusionContacto(id)) {
			for (const std::string& mat : it->second) {
				const int z = m.d.invocarEspecial(0, mat, false);
				if (z < 0) return "no entra el material " + mat;
			}
			j.mano = -1;
		} else {
			j.mano = m.mano(0, "polymerization");
			for (const std::string& mat : it->second) j.manoExtra.push_back(m.mano(0, mat));
		}
		if (!m.d.aplicar(j)) {
			snprintf(buf, sizeof(buf), "J_FUSION rechazada (%d materiales%s)", (int)it->second.size(), fusionContacto(id) ? ", por contacto" : "");
			return buf;
		}
		return m.zonaDe(0, id) >= 0 ? "" : "la fusion no quedo en el campo";
	}
	if (c.clase == "ritual") {
		std::string magia;
		auto it = base.rituales.find(id);
		if (it != base.rituales.end()) magia = it->second;
		if (magia.empty())
			for (const Carta& k : base.cartas)
				if (k.subtipo == "magia_ritual" && base.ritualCon(id, k.id)) magia = k.id;
		if (magia.empty()) return "sin magia ritual que lo invoque";
		cub::Mesa m(&base);
		Jugada j;
		j.tipo = J_RITUAL;
		int nivel = 0;
		while (nivel < c.nivel) {
			j.tributos.push_back(m.mon(0, "battle_ox"));
			nivel += 4;
		}
		j.mano = m.mano(0, magia);
		j.mano2 = m.mano(0, id);
		if (!m.d.aplicar(j)) return "J_RITUAL rechazada con " + magia;
		return m.zonaDe(0, id) >= 0 ? "" : "el ritual no quedo en el campo";
	}
	if (c.subtipo == "toon") {
		cub::Mesa m(&base);
		const int w = m.colocar(0, "toon_world");
		m.d.l[0].mt[(size_t)w].bocaAbajo = false;
		Jugada j;
		j.tipo = J_TOON;
		const int n = motorTributos(m.d, 0, &c);
		for (int k = 0; k < n; k++) j.tributos.push_back(m.mon(0, "battle_ox"));
		j.mano = m.mano(0, id);
		if (!m.d.aplicar(j)) return "J_TOON rechazada con Toon World boca arriba";
		return m.zonaDe(0, id) >= 0 ? "" : "el toon no quedo en el campo";
	}
	const int noNormal = textoNoNormal(textoEn);
	if (noNormal == 1) {   // no se invoca normal: ni con 0, 1, 2 o 3 sacrificios
		for (int n = 0; n <= 3; n++) {
			cub::Mesa m(&base);
			std::vector<int> tr;
			for (int k = 0; k < n; k++) tr.push_back(m.mon(0, "battle_ox"));
			const int h = m.mano(0, id);
			if (m.d.puedeInvocarNormal(0, h, tr)) {
				snprintf(buf, sizeof(buf), "su texto dice que no se invoca de modo normal y se invoco con %d sacrificios", n);
				return buf;
			}
		}
		return "";
	}
	// normal: con sus sacrificios (y la condicion del motor si la tiene)
	const int n = divina ? 3 : Duelo::tributosPara(&c);
	for (int modo = 0; modo < 2; modo++) {
		if (modo == 1 && divina) break;   // (un Dios no se coloca)
		cub::Mesa m(&base);
		condicionInvocar(m, id);
		if (id == "terrorking_archfiend") m.mon(0, "archfiend_soldier");
		std::vector<int> tr;
		for (int k = 0; k < n; k++) tr.push_back(m.mon(0, "battle_ox"));
		const int h = m.mano(0, id);
		Jugada j;
		j.tipo = modo == 0 ? J_INVOCAR : J_COLOCAR;
		j.mano = h;
		j.tributos = tr;
		if (!m.d.aplicar(j)) {
			snprintf(buf, sizeof(buf), "%s con %d sacrificios rechazada", modo == 0 ? "invocar" : "colocar", n);
			return buf;
		}
		// (lo que cuenta es como entro: su suceso; despues su propio efecto puede cambiarla, Tsukuyomi se pone boca abajo)
		bool entro = false;
		for (const Suceso& s : m.d.sucesos) entro |= s.tipo == (modo == 0 ? S_INVOCA : S_COLOCA) && s.carta == id;
		if (!entro) return modo == 0 ? "invocada pero no entro boca arriba" : "colocada pero no entro boca abajo";
	}
	if (noNormal == 2 && id == "cave_dragon") {   // con un monstruo propio: no
		cub::Mesa m(&base);
		m.mon(0, "battle_ox");
		const int h = m.mano(0, id);
		if (m.d.puedeInvocarNormal(0, h, {})) return "Cave Dragon se invoco controlando un monstruo";
	}
	if (id == "terrorking_archfiend") {   // sin otro Archfiend: no
		cub::Mesa m(&base);
		const int h = m.mano(0, id);
		if (m.d.puedeInvocarNormal(0, h, {})) return "Terrorking Archfiend se invoco sin un Archfiend";
	}
	return "";
}

inline std::string probarColocar(const Base& base, const Carta& c)
{
	cub::Mesa m(&base);
	const int h = m.mano(0, c.id);
	Jugada j;
	j.tipo = J_COLOCAR_MT;
	j.mano = h;
	if (!m.d.aplicar(j)) return "J_COLOCAR_MT rechazada";
	if (c.subtipo == "magia_campo") return m.d.l[0].campo.id == c.id && m.d.l[0].campo.bocaAbajo ? "" : "la magia de campo no quedo boca abajo en su zona";
	for (const MagiaTrampa& x : m.d.l[0].mt)
		if (x.id == c.id) return x.bocaAbajo ? "" : "quedo boca arriba";
	return "no esta en la zona de magia/trampa";
}

// los resultados de la certificacion (docs/certificacion_efectos.csv de este ciclo): id -> estado
inline std::map<std::string, std::string> leerCertificacion(const std::string& csv)
{
	std::map<std::string, std::string> r;
	std::ifstream f(csv);
	std::string l;
	std::getline(f, l);
	while (std::getline(f, l)) {
		const std::vector<std::string> c = ia::partir(l, ';');
		if (c.size() > 5) r[c[0]] = c[5];
	}
	return r;
}

// las cartas de campos.h (la tabla usa Vulkan: se lee del fuente)
inline std::set<std::string> leerCampos(const std::string& fuente)
{
	std::set<std::string> r;
	std::ifstream f(fuente);
	std::string l;
	static const std::regex fila("^\\s*\\{ \"([a-z0-9_]+)\", S_");
	std::smatch m;
	while (std::getline(f, l))
		if (std::regex_search(l, m, fila)) r.insert(m[1].str());
	return r;
}

inline void cartas(const Base& base, const std::string& ui, const std::string& caras, const std::string& csvCert, const std::string& fuenteCampos)
{
	const auto es = leerTabla(ui + "cartas_ui.txt");
	const auto en = leerTabla(ui + "cartas_ui_en.txt");
	const auto classic = leerTabla(ui + "cartas_classic.txt");
	const auto cert = leerCertificacion(csvCert);
	const auto campos = leerCampos(fuenteCampos);
	cu::caso("CU-CARTA-CERTIFICACION-LEIDA", cert.size() > 1300, "la certificacion de este ciclo tiene %d cartas (%s)", (int)cert.size(), csvCert.c_str());
	// atributo y tipo: la traduccion de la mayoria (cada valor en ES con un solo valor en EN)
	std::map<std::string, std::map<std::string, int>> atrib, tipo;
	for (const auto& p : es) {
		auto e = en.find(p.first);
		if (e == en.end() || p.second.size() < 9 || e->second.size() < 9) continue;
		atrib[p.second[3]][e->second[3]]++;
		tipo[p.second[4]][e->second[4]]++;
	}
	auto mayoria = [](const std::map<std::string, int>& m) {
		std::string r;
		int n = -1;
		for (const auto& x : m)
			if (x.second > n) r = x.first, n = x.second;
		return r;
	};
	static const std::set<std::string> SUB_EFECTO = { "efecto", "volteo", "espiritu", "union", "toon", "geminis", "firma" };   // (geminis: rama classic-torneo-extras)
	int nCartas = 0;
	for (const auto& p : es) {
		const std::string& id = p.first;
		const std::vector<std::string>& a = p.second;
		nCartas++;
		const std::string cid = "CU-CARTA-" + id;
		// ---- datos
		{
			const std::string k = cid + "-DATOS";
			cu::caso(k, a.size() == 9 && !a[1].empty() && !a[8].empty(), "la linea ES no tiene 9 campos con nombre y texto");
			auto e = en.find(id);
			if (!cu::caso(k, e != en.end() && e->second.size() == 9, "no esta en cartas_ui_en.txt")) continue;
			const std::vector<std::string>& b = e->second;
			cu::caso(k, !b[1].empty() && !b[8].empty(), "la linea EN sin nombre o texto");
			cu::caso(k, a[2] == b[2] && a[5] == b[5] && a[6] == b[6] && a[7] == b[7], "ES/EN distintos: clase %s/%s nivel %s/%s ATK %s/%s DEF %s/%s", a[2].c_str(), b[2].c_str(), a[5].c_str(),
			         b[5].c_str(), a[6].c_str(), b[6].c_str(), a[7].c_str(), b[7].c_str());
			cu::caso(k, mayoria(atrib[a[3]]) == b[3], "el atributo %s esta traducido como %s (en las demas: %s)", a[3].c_str(), b[3].c_str(), mayoria(atrib[a[3]]).c_str());
			cu::caso(k, mayoria(tipo[a[4]]) == b[4], "el tipo %s esta traducido como %s (en las demas: %s)", a[4].c_str(), b[4].c_str(), mayoria(tipo[a[4]]).c_str());
			const bool monstruo = a[2] == "normal" || a[2] == "efecto" || a[2] == "fusion" || a[2] == "ritual";
			if (monstruo) {
				const int nivel = atoi(a[5].c_str()), atk = atoi(a[6].c_str()), def = atoi(a[7].c_str());
				cu::caso(k, nivel >= 1 && nivel <= 12 && atk >= -1 && def >= -1 && atk <= 10000 && def <= 10000, "nivel %d ATK %d DEF %d fuera de rango", nivel, atk, def);
			}
			auto cl = classic.find(id);
			if (cu::caso(k, cl != classic.end() && cl->second.size() >= 4, "no esta en cartas_classic.txt")) {
				const std::vector<std::string>& c = cl->second;
				const int lim = atoi(c[2].c_str());
				cu::caso(k, !c[1].empty() && c[1].find_first_not_of("0123456789") == std::string::npos, "passcode '%s'", c[1].c_str());
				cu::caso(k, lim >= 0 && lim <= 3 && c[2].size() == 1, "limite Goat '%s'", c[2].c_str());
				const std::string& sub = c[3];
				bool vale = false;
				if (a[2] == "normal") vale = sub == "normal";
				if (a[2] == "efecto") vale = SUB_EFECTO.count(sub) > 0;
				if (a[2] == "fusion") vale = sub == "fusion";
				if (a[2] == "ritual") vale = sub == "ritual";
				if (a[2] == "magia") vale = sub.rfind("magia_", 0) == 0;
				if (a[2] == "trampa") vale = sub.rfind("trampa_", 0) == 0;
				cu::caso(k, vale, "subtipo '%s' no corresponde a la clase '%s'", sub.c_str(), a[2].c_str());
			}
			const Carta* kb = base.carta(id);
			cu::caso(k, kb != nullptr, "el motor (ia::Base) no la tiene");
		}
		// ---- caras
		{
			const std::string k = cid + "-CARAS";
			for (const std::string& f : { "cara_" + id + ".png", "cara_en_" + id + ".png", "mini_" + id + ".png", "mini_en_" + id + ".png", "cara_hd_" + id + ".jpg", "cara_hd_en_" + id + ".jpg" })
				cu::caso(k, existe(caras + f), "falta %s", f.c_str());
		}
		const Carta* c = base.carta(id);
		if (!c) continue;
		// ---- invocar / colocar
		if (c->monstruo) {
			const std::string motivo = probarInvocar(base, *c, en.count(id) ? en.at(id)[8] : std::string());
			cu::caso(cid + "-INVOCAR", motivo.empty(), "%s", motivo.c_str());
			// el texto y la marca "no invocable" (rivales.txt N) dicen lo mismo
			const int nn = c->clase == "normal" || c->clase == "efecto" ? textoNoNormal(en.count(id) ? en.at(id)[8] : "") : 0;
			if (c->subtipo != "toon")
				cu::caso(cid + "-INVOCAR", (nn == 1) == c->noInvocable, "su texto %s y el motor lo marca %s", nn == 1 ? "prohibe invocarla de modo normal" : nn == 2 ? "lo permite con condicion" : "permite invocarla",
				         c->noInvocable ? "no invocable" : "invocable");
		} else {
			const std::string motivo = probarColocar(base, *c);
			cu::caso(cid + "-COLOCAR", motivo.empty(), "%s", motivo.c_str());
		}
		// ---- efecto: la certificacion de este ciclo
		auto ce = cert.find(id);
		if (ce != cert.end()) cu::caso(cid + "-EFECTO", ce->second == "OK", "certificacion: %s", ce->second.c_str());
		else if (c->clase == "efecto" || c->magia || c->trampa) cu::caso(cid + "-EFECTO", false, "carta con efecto que no esta en la certificacion");
		// ---- campo
		if (c->subtipo == "magia_campo") cu::caso(cid + "-CAMPO", campos.count(id) > 0, "la magia de campo no tiene escenario en campos.h");
	}
	cu::caso("CU-CARTA-TOTAL", nCartas >= 1770, "solo %d cartas en cartas_ui.txt", nCartas);
	cu::caso("CU-CARTA-TOTAL", es.size() == en.size() && es.size() == classic.size(), "ES %d, EN %d, classic %d cartas", (int)es.size(), (int)en.size(), (int)classic.size());
}

}  // namespace cuc
