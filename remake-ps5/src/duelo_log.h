/*
 * Yugioh Classic Remaster (rama classic-ps5-duelo-log): BITACORA DEL DUELO.
 *
 * Un archivo de texto plano (duelo_log.txt, en la carpeta de datos: /data/yugioh_classic en la PS5) con una linea por
 * suceso del duelo, para entender en tiempo real que pone el jugador, que pone el CPU y POR QUE lo hace la IA. Se lee
 * desde el PC por FTP (leer_log_duelo.py: solo lectura). Formato y ejemplos: docs/duelo_log.md.
 *
 *   mm:ss [T<turno> <fase> J|CPU] texto          (J = el jugador, lado 0; CPU = el rival, lado 1)
 *
 * Reglas de oro:
 *  - NO cambia el juego: solo LEE el duelo (ia::Duelo) a traves de los ganchos opcionales de ia.h (ia::ganchosLog).
 *    No gasta azar del motor, no llama a nada que modifique el estado, y las copias que la IA humana imagina (Monte
 *    Carlo) se ignoran: solo se anota el duelo vigilado (el de verdad).
 *  - Barata: una linea = un fwrite + fflush (microsegundos); nada de trabajo en el bucle de render. Con la opcion
 *    "Bitacora del duelo" en No (opciones::bitacora()) no se abre ni se escribe nada y los ganchos quedan apagados.
 *  - No revienta ni bloquea: si la carpeta no se puede escribir, la bitacora simplemente no existe.
 *  - Rotacion: si el archivo pasa de ~2 MB al empezar un duelo, se renombra duelo_log.anterior.txt (el anterior se
 *    pierde) y se empieza otro.
 * Este archivo no depende de Vulkan (lo incluye ia.h al final, asi que lo ven tambien las pruebas sin pantalla).
 */
#pragma once

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "guardado.h"
#include "ia.h"
#include "opciones.h"

namespace duelo_log {

// lo que el juego sabe del duelo y la partida y la IA no (la cabecera)
struct Cabecera {
	std::string rival, variante, personalidad, plan, modo, version;
	int nivel{ 0 };
	bool iaHumana{ false };
};

constexpr long LIMITE_BYTES = 2L * 1024L * 1024L;

struct Estado {
	FILE* f{ nullptr };
	const ia::Duelo* vig{ nullptr };                // el duelo de verdad (las copias de la IA no se anotan)
	std::chrono::steady_clock::time_point t0;
	std::string carpeta;                            // si no esta vacia, manda sobre guardado::carpetaDatos() (pruebas)
	std::map<std::string, std::string> nombres;     // id de carta -> nombre en espanol
	bool nombresListos{ false };
	std::string pendInvoca;                         // tipo de la invocacion que viene (jugada ya aplicada, suceso por llegar)
	std::string pendObj;                            // objetivos / costes de la activacion que viene
	std::vector<ia::Jugada> cand;                   // las candidatas de la ultima decision (ganchos de Cerebro)
	std::array<int, 2> lpPrev{ { 0, 0 } };
	std::array<int, 2> planVisto{ { -2, -2 } };
	int turnoVisto{ -1 };
	long lineas{ 0 };
};
inline Estado& est()
{
	static Estado e;
	return e;
}

inline bool abierta() { return est().f != nullptr; }

inline std::string carpetaLog()
{
	const Estado& e = est();
	if (!e.carpeta.empty()) return e.carpeta;
	if (const char* v = getenv("YUGI_DUELO_LOG_DIR")) return v;
	return guardado::carpetaDatos();
}
inline std::string rutaLog() { return carpetaLog() + "/duelo_log.txt"; }

// los nombres en espanol (cartas_ui.txt: "id|nombre|..."); una sola vez
inline void cargarNombres(const std::string& rutaCartasUi)
{
	Estado& e = est();
	if (e.nombresListos || rutaCartasUi.empty()) return;
	std::ifstream f(rutaCartasUi);
	std::string l;
	while (std::getline(f, l)) {
		const size_t a = l.find('|');
		if (a == std::string::npos) continue;
		const size_t b = l.find('|', a + 1);
		if (b == std::string::npos) continue;
		e.nombres[l.substr(0, a)] = l.substr(a + 1, b - a - 1);
	}
	e.nombresListos = !e.nombres.empty();
}
inline std::string N(const std::string& id)
{
	if (id.empty()) return "?";
	const auto& m = est().nombres;
	const auto it = m.find(id);
	return it == m.end() ? id : it->second;
}

inline const char* nombreFase(int f)
{
	static const char* const F[] = { "Robo", "Principal1", "Batalla", "Principal2", "Final" };
	return F[f < 0 || f > 4 ? 0 : f];
}
inline const char* quien(int lado) { return lado == 0 ? "J" : "CPU"; }

// ------------------------------------------------------------------------------------------------- escritura
inline void escribirCruda(const std::string& s)
{
	Estado& e = est();
	if (!e.f) return;
	if (fwrite(s.data(), 1, s.size(), e.f) != s.size()) {   // (disco lleno / solo lectura: se apaga sin ruido)
		fclose(e.f);
		e.f = nullptr;
		return;
	}
	fflush(e.f);
	e.lineas++;
}
// una linea con el prefijo [T<turno> <fase> J|CPU]
inline void linea(const ia::Duelo& d, int lado, const std::string& txt, int fase = -1)
{
	Estado& e = est();
	if (!e.f) return;
	const int seg = (int)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - e.t0).count();
	char pre[64];
	snprintf(pre, sizeof(pre), "%02d:%02d [T%d %s %s] ", seg / 60, seg % 60, d.turno, nombreFase(fase >= 0 ? fase : (int)d.fase), quien(lado));
	escribirCruda(std::string(pre) + txt + "\n");
}

// ------------------------------------------------------------------------------------------------- descripciones
inline const char* posicion(const ia::Monstruo& m)
{
	return m.bocaAbajo ? "boca abajo" : m.defensa ? "defensa" : "ataque";
}
inline int atkDe(const ia::Duelo& d, const ia::Monstruo& m)
{
	return m.atkEf ? m.atkEf : d.atk(m.id) + m.atkPerm + m.atkTurno;
}
inline int defDe(const ia::Duelo& d, const ia::Monstruo& m)
{
	return m.defEf ? m.defEf : d.def(m.id) + m.defPerm + m.defTurno;
}
inline std::string stats(const ia::Duelo& d, const ia::Monstruo& m)
{
	return "ATK " + std::to_string(atkDe(d, m)) + "/DEF " + std::to_string(defDe(d, m));
}
inline std::string zonaTxt(int z) { return "z" + std::to_string(z + 1); }

// la carta de un lugar del duelo ("" si ya no esta)
inline std::string cartaDe(const ia::Duelo& d, const ia::Lugar& p)
{
	if (p.lado < 0 || p.lado > 1) return "";
	const ia::Lado& x = d.l[(size_t)p.lado];
	auto en = [&](const std::vector<std::string>& v) { return p.i >= 0 && p.i < (int)v.size() ? v[(size_t)p.i] : std::string(); };
	switch (p.donde) {
	case ia::EN_MONSTRUO: return p.i >= 0 && p.i < ia::ZONAS ? x.m[(size_t)p.i].id : "";
	case ia::EN_MT: return p.i >= 0 && p.i < ia::ZONAS ? x.mt[(size_t)p.i].id : "";
	case ia::EN_CAMPO: return x.campo.id;
	case ia::EN_MANO: return en(x.mano);
	case ia::EN_CEMENTERIO: return en(x.cementerio);
	case ia::EN_DESTERRADO: return en(x.desterrados);
	case ia::EN_MAZO: return en(x.mazo);
	case ia::EN_EXTRA: return en(x.extra);
	default: return "";
	}
}
inline std::string lugarTxt(const ia::Duelo& d, const ia::Lugar& p)
{
	static const char* const D[] = { "campo", "mag/tr", "mano", "cementerio", "campo", "desterradas", "mazo", "extra" };
	std::string s = N(cartaDe(d, p));
	s += std::string(" (") + quien(p.lado) + " " + D[p.donde < 0 || p.donde > 7 ? 0 : p.donde];
	if (p.donde == ia::EN_MONSTRUO || p.donde == ia::EN_MT) s += " " + zonaTxt(p.i);
	return s + ")";
}

// la carta que mueve una jugada (para describirla)
inline std::string cartaJugada(const ia::Duelo& d, int lado, const ia::Jugada& j)
{
	const ia::Lado& x = d.l[(size_t)lado];
	auto mano = [&](int k) { return k >= 0 && k < (int)x.mano.size() ? x.mano[(size_t)k] : std::string(); };
	switch (j.tipo) {
	case ia::J_ATACAR: case ia::J_VOLTEAR: case ia::J_POSICION:
		return j.zona >= 0 && j.zona < ia::ZONAS ? x.m[(size_t)j.zona].id : j.carta;
	case ia::J_ACTIVAR:
		if (j.mano >= 0) return mano(j.mano);
		if (j.deMonstruo) return j.zona >= 0 && j.zona < ia::ZONAS ? x.m[(size_t)j.zona].id : "";
		if (j.zona == ia::ZONA_CAMPO) return x.campo.id;
		return j.zona >= 0 && j.zona < ia::ZONAS ? x.mt[(size_t)j.zona].id : "";
	default:
		if (!j.carta.empty()) return j.carta;
		return mano(j.mano);
	}
}
inline std::string descJugada(const ia::Duelo& d, int lado, const ia::Jugada& j)
{
	static const char* const T[] = { "Invocar", "Colocar boca abajo", "Voltear", "Cambiar posicion de", "Colocar mag/tr", "Activar", "Fusion", "Ritual",
	                                  "Invocar Toon", "Atacar con", "Pasar de fase", "Descartar", "Activar", "No responder", "Elegir" };
	std::string s = T[(int)j.tipo];
	if (j.tipo == ia::J_FASE || j.tipo == ia::J_PASAR) return s;
	const std::string c = cartaJugada(d, lado, j);
	if (!c.empty()) s += " " + N(c);
	if (j.tipo == ia::J_ATACAR) {
		const ia::Lado& o = d.l[(size_t)(1 - lado)];
		if (j.objetivo >= 0 && j.objetivo < ia::ZONAS && !o.m[(size_t)j.objetivo].vacia())
			s += " -> " + (o.m[(size_t)j.objetivo].bocaAbajo ? std::string("monstruo boca abajo ") + zonaTxt(j.objetivo) : N(o.m[(size_t)j.objetivo].id));
		else s += " -> directo";
	}
	return s;
}

// ------------------------------------------------------------------------------------------------- los sucesos
inline std::string nombresZonas(const ia::Duelo& d, int lado, const std::vector<int>& zs)
{
	std::string s;
	for (int z : zs)
		if (z >= 0 && z < ia::ZONAS) s += (s.empty() ? "" : ", ") + N(d.l[(size_t)lado].m[(size_t)z].id);
	return s;
}

// lo que va a pasar si "a" ataca a "o" (con los valores del momento)
inline std::string desenlace(int pa, const ia::Monstruo& o, int po)
{
	if (o.defensa || o.bocaAbajo) {   // (boca abajo siempre esta en defensa)
		if (pa > po) return "destruye al defensor, sin dano";
		if (pa == po) return "no pasa nada";
		return "el atacante pierde " + std::to_string(po - pa) + " LP";
	}
	if (pa > po) return "destruye al defensor, dano " + std::to_string(pa - po);
	if (pa == po) return "ambos destruidos";
	return "el atacante muere, pierde " + std::to_string(po - pa) + " LP";
}

inline void anotarLP(const ia::Duelo& d, int tipo)
{
	Estado& e = est();
	for (int l = 0; l < 2; l++) {
		const int ahora = d.l[(size_t)l].lp;
		if (ahora != e.lpPrev[(size_t)l] && tipo != ia::S_DANNO && tipo != ia::S_LP)
			linea(d, l, std::string("LP ") + quien(l) + ": " + std::to_string(e.lpPrev[(size_t)l]) + " -> " + std::to_string(ahora));
		e.lpPrev[(size_t)l] = ahora;
	}
}

inline void cerrar(const char* motivo);

inline void alSuceso(const ia::Duelo& d, const ia::Suceso& s)
{
	Estado& e = est();
	if (!e.f || &d != e.vig) return;
	// el jugador abandono el duelo y se esta empezando otro sobre el mismo objeto: lo viejo se cierra y lo nuevo espera a empezar()
	if (s.tipo == ia::S_TURNO && s.valor == 1 && e.turnoVisto >= 1) {
		cerrar("duelo interrumpido (se empezo otro)");
		return;
	}
	const int ld = s.lado;
	const ia::Lado& x = d.l[(size_t)(ld == 0 || ld == 1 ? ld : 0)];
	auto monstruo = [&](int lado, int z) -> const ia::Monstruo* {
		return (lado == 0 || lado == 1) && z >= 0 && z < ia::ZONAS ? &d.l[(size_t)lado].m[(size_t)z] : nullptr;
	};
	std::string t;
	int lado = ld;
	switch (s.tipo) {
	case ia::S_TURNO: {
		lado = d.activo;
		const ia::Lado& a = d.l[0];
		const ia::Lado& b = d.l[1];
		e.turnoVisto = d.turno;
		t = "===== Turno " + std::to_string(s.valor) + " de " + (lado == 0 ? "J" : "CPU") + " | LP J " + std::to_string(a.lp) + " - CPU " + std::to_string(b.lp) +
		    " | mano J " + std::to_string(a.mano.size()) + ", CPU " + std::to_string(b.mano.size()) + " | mazo J " + std::to_string(a.mazo.size()) + ", CPU " +
		    std::to_string(b.mazo.size()) + " | monstruos J " + std::to_string(a.monstruos()) + ", CPU " + std::to_string(b.monstruos()) + " =====";
		break;
	}
	case ia::S_FASE: lado = d.activo; t = std::string("Fase: ") + nombreFase(s.valor); break;
	case ia::S_ROBO:
		t = "roba " + N(s.carta) + " (mano " + std::to_string(x.mano.size()) + ", mazo " + std::to_string(x.mazo.size()) + ")";
		if (ld == 0) {   // (la mano del jugador entera; la del CPU, solo cuantas tiene)
			t += " [mano:";
			for (const std::string& c : x.mano) t += " " + N(c) + ";";
			t += "]";
		}
		break;
	case ia::S_INVOCA: {
		const ia::Monstruo* m = monstruo(ld, s.zona);
		std::string tipo = e.pendInvoca.empty() ? "Invocacion Especial" : e.pendInvoca;
		e.pendInvoca.clear();
		t = tipo + ": " + N(s.carta);
		if (m) t += std::string(" en ") + zonaTxt(s.zona) + ", " + posicion(*m) + ", " + stats(d, *m);
		break;
	}
	case ia::S_COLOCA: {
		std::string tipo = e.pendInvoca.empty() ? std::string("coloca") : e.pendInvoca;
		e.pendInvoca.clear();
		t = "coloca boca abajo en " + zonaTxt(s.zona) + " (defensa): " + N(s.carta);
		if (tipo != "coloca" && tipo != "Invocacion Normal") t += " [" + tipo + "]";
		break;
	}
	case ia::S_VOLTEA: {
		const ia::Monstruo* m = monstruo(ld, s.zona);
		t = "voltea " + N(s.carta) + " en " + zonaTxt(s.zona) + (s.valor == 1 ? " (atacado)" : s.valor == 2 ? " (por un efecto)" : "");
		if (m) t += ", " + stats(d, *m);
		break;
	}
	case ia::S_POSICION: {
		const ia::Monstruo* m = monstruo(ld, s.zona);
		t = N(s.carta) + " (" + zonaTxt(s.zona) + ") " + (s.valor == 2 ? "queda boca abajo" : s.valor == 1 ? "cambia a defensa" : "cambia a ataque");
		if (m) t += ", " + stats(d, *m);
		break;
	}
	case ia::S_COLOCA_MT:
		t = std::string("coloca boca abajo ") + (s.zona == ia::ZONA_CAMPO ? "en la zona de campo" : "en " + zonaTxt(s.zona)) + ": " + N(s.carta);
		break;
	case ia::S_ACTIVA_MT: case ia::S_ACTIVA_EFECTO: {
		const ia::Carta* c = d.carta(s.carta);
		const std::string clase = s.tipo == ia::S_ACTIVA_EFECTO ? "efecto de " : s.zona == ia::ZONA_CAMPO ? "carta de campo " : c && c->subtipo == "equipo" ? "equipo " : "";
		t = std::string("ACTIVA ") + clase + N(s.carta);
		if (s.valor >= 1) t += " [eslabon " + std::to_string(s.valor) + "]";
		if (!e.pendObj.empty()) t += " | " + e.pendObj;
		e.pendObj.clear();
		if (!d.ef.cadena.empty()) {   // (los eslabones que ya estaban: esta carta responde a ellos)
			t += " | en cadena tras:";
			for (const ia::Eslabon& k : d.ef.cadena) t += " " + N(k.carta) + " (" + quien(k.lado) + ");";
		}
		break;
	}
	case ia::S_RESUELVE: t = "resuelve " + N(s.carta); break;
	case ia::S_NEGADO: t = N(s.carta) + ": NEGADA"; break;
	case ia::S_AL_CEMENTERIO:
		t = N(s.carta) + (s.zona < 0 ? std::string(" (de la mano o el mazo)") : s.magia ? " (mag/tr " + (s.zona == ia::ZONA_CAMPO ? std::string("campo") : zonaTxt(s.zona)) + ")" : " (" + zonaTxt(s.zona) + ")") +
		    (s.valor == 1 ? ": DESTRUIDA EN BATALLA -> cementerio" : s.destruida ? ": DESTRUIDA POR EFECTO -> cementerio" : s.magia ? ": usada o destruida -> cementerio" : ": va al cementerio (sacrificio, coste, descarte o efecto)");
		break;
	case ia::S_ATAQUE: case ia::S_COMBATE: {
		const bool decl = s.tipo == ia::S_ATAQUE;
		const ia::Monstruo* a = monstruo(ld, s.zona);
		const ia::Monstruo* o = monstruo(1 - ld, s.zona2);
		t = std::string(decl ? "ATAQUE: " : "combate real: ") + N(s.carta) + " (" + zonaTxt(s.zona) + (a ? ", ATK " + std::to_string(atkDe(d, *a)) : "") + ")";
		if (o && !o->vacia()) {
			const int po = o->defensa || o->bocaAbajo ? defDe(d, *o) : atkDe(d, *o);
			t += " -> " + (o->bocaAbajo ? std::string("monstruo boca abajo") : N(o->id)) + " (" + zonaTxt(s.zona2) + ", " + posicion(*o) + ", ATK " +
			     std::to_string(atkDe(d, *o)) + "/DEF " + std::to_string(defDe(d, *o)) + ")";
			if (a) t += " | " + desenlace(atkDe(d, *a), *o, po);
		} else {
			t += " -> ataque directo" + (a ? " | dano " + std::to_string(atkDe(d, *a)) : std::string());
		}
		if (s.tipo == ia::S_ATAQUE && s.valor == 1) t += " [repeticion]";
		break;
	}
	case ia::S_ATAQUE_NEGADO: t = "el ataque es NEGADO" + (s.carta.empty() ? std::string() : " (" + N(s.carta) + ")"); break;
	case ia::S_DANNO: {
		const int antes = e.lpPrev[(size_t)lado == 0 || lado == 1 ? (size_t)lado : 0];
		const int ahora = x.lp;
		t = s.zona2 == 1 ? std::string("paga ") + std::to_string(s.valor) + " LP" : "recibe " + std::to_string(s.valor) + " de dano";
		t += " (LP " + std::to_string(antes) + " -> " + std::to_string(ahora) + ")";
		break;
	}
	case ia::S_LP: {
		const int antes = e.lpPrev[(size_t)lado == 0 || lado == 1 ? (size_t)lado : 0];
		t = "gana " + std::to_string(s.valor) + " LP (LP " + std::to_string(antes) + " -> " + std::to_string(x.lp) + ")";
		break;
	}
	case ia::S_CONTROL: t = "toma el control de " + N(s.carta); break;
	case ia::S_A_LA_MANO: t = N(s.carta) + " vuelve a la mano"; break;
	case ia::S_DESTERRADO: t = N(s.carta) + " queda desterrada"; break;
	case ia::S_DESCARTA: t = s.zona2 == 1 ? N(s.carta) + " vuelve al mazo" : "descarta " + N(s.carta); break;
	case ia::S_REVELA: t = "revela " + N(s.carta); break;
	case ia::S_FIN: {
		const int g = s.lado;
		std::string motivo;
		if (g == 0 || g == 1) {
			const int p = 1 - g;
			if (d.l[(size_t)p].lp <= 0) motivo = std::string("los LP de ") + quien(p) + " llegaron a 0";
			else if (d.l[(size_t)p].mazo.empty()) motivo = std::string(quien(p)) + " no tiene cartas en el mazo para robar";
			else motivo = g == 1 ? "el jugador se rindio / abandono el duelo (o una condicion de victoria por efecto)" : "condicion de victoria por efecto (p. ej. Exodia)";
		} else {
			motivo = "empate";
		}
		lado = g == 0 || g == 1 ? g : d.activo;
		t = std::string("***** FIN DEL DUELO: gana ") + (g == 0 ? "J" : g == 1 ? "CPU" : "nadie") + " en el turno " + std::to_string(d.turno) + " | motivo: " + motivo +
		    " | LP J " + std::to_string(d.l[0].lp) + " - CPU " + std::to_string(d.l[1].lp) + " *****";
		break;
	}
	default: anotarLP(d, s.tipo); return;   // (cadena, modifica: internos)
	}
	linea(d, lado, t, s.tipo == ia::S_TURNO ? (int)ia::F_ROBO : -1);
	// los LP que cambiaron sin su propio suceso (costes, efectos) tambien se anotan
	if (s.tipo != ia::S_DANNO && s.tipo != ia::S_LP) anotarLP(d, s.tipo);
	else {
		e.lpPrev[0] = d.l[0].lp;
		e.lpPrev[1] = d.l[1].lp;
	}
}

// la jugada que se va a aplicar (del jugador o del CPU): solo guarda lo que el suceso no sabe (tributos, materiales,
// objetivos y costes), en el estado de ANTES de aplicarla
inline void alJugar(const ia::Duelo& d, const ia::Jugada& j)
{
	Estado& e = est();
	if (!e.f || &d != e.vig) return;
	const int lado = d.decide();
	e.pendInvoca.clear();
	e.pendObj.clear();
	switch (j.tipo) {
	case ia::J_INVOCAR: case ia::J_COLOCAR:
		e.pendInvoca = j.tributos.empty() ? "Invocacion Normal" : "Invocacion por sacrificio (sacrifica: " + nombresZonas(d, lado, j.tributos) + ")";
		break;
	case ia::J_TOON: e.pendInvoca = "Invocacion especial de Toon (Toon World)" + (j.tributos.empty() ? std::string() : " (sacrifica: " + nombresZonas(d, lado, j.tributos) + ")"); break;
	case ia::J_FUSION: case ia::J_RITUAL: {
		std::string mats;
		const ia::Lado& x = d.l[(size_t)lado];
		if (j.tipo == ia::J_RITUAL && j.mano2 >= 0 && j.mano2 < (int)x.mano.size()) mats += N(x.mano[(size_t)j.mano2]) + " (monstruo ritual); ";
		for (int k : j.manoExtra)
			if (k >= 0 && k < (int)x.mano.size()) mats += N(x.mano[(size_t)k]) + "; ";
		mats += nombresZonas(d, lado, j.tributos);
		e.pendInvoca = std::string(j.tipo == ia::J_FUSION ? "Invocacion por Fusion" : "Invocacion Ritual") + " (materiales: " + mats + ")";
		break;
	}
	case ia::J_ACTIVAR: {
		std::string o;
		for (const ia::Lugar& p : j.objs) o += (o.empty() ? "" : "; ") + lugarTxt(d, p);
		if (!o.empty()) e.pendObj = "objetivo: " + o;
		std::string c;
		for (int k : j.descartes)
			if (k >= 0 && k < (int)d.l[(size_t)lado].mano.size()) c += (c.empty() ? "" : ", ") + N(d.l[(size_t)lado].mano[(size_t)k]);
		for (const ia::Lugar& p : j.costes) c += (c.empty() ? "" : ", ") + lugarTxt(d, p);
		if (!c.empty()) e.pendObj += std::string(e.pendObj.empty() ? "" : " | ") + "coste: " + c;
		break;
	}
	case ia::J_ELEGIR: {
		const auto& cs = d.ef.preg.cands;
		if (j.objetivo >= 0 && j.objetivo < (int)cs.size())
			linea(d, d.ef.preg.lado, "elige " + lugarTxt(d, cs[(size_t)j.objetivo]) + " (pide: " + N(d.ef.preg.carta) + ")");
		break;
	}
	case ia::J_PASAR:
		if (d.ef.preg.tipo == ia::P_RESPUESTA) {
			if (d.ef.preg.lado == 0) linea(d, 0, "no responde (pasa)");   // (el CPU lo explica su [CPU decide])
		}
		else if (d.ef.preg.tipo == ia::P_ELEGIR) linea(d, d.ef.preg.lado, "no elige nada (opcional)");
		break;
	default: break;
	}
}

// ------------------------------------------------------------------------------------------------- decisiones del CPU
inline void alProponer(const ia::Duelo& d, const std::vector<ia::Jugada>& c)
{
	Estado& e = est();
	if (!e.f || &d != e.vig) return;
	e.cand = c;
}

inline const char* siguienteFase(const ia::Duelo& d)
{
	switch (d.fase) {
	case ia::F_PRINCIPAL1: return "Batalla (o Final si no puede atacar)";
	case ia::F_BATALLA: return "Principal 2";
	case ia::F_PRINCIPAL2: return "Fase Final";
	default: return "la fase siguiente";
	}
}

// "ATK 1800 vs DEF 2000 -> el atacante pierde 200 LP": lo que el CPU CREE (boca abajo sin ver: su estimacion)
inline std::string cuentaAtaque(const ia::Duelo& d, const ia::Cerebro& c, int lado, int za, int zo, bool& pierde)
{
	const ia::Monstruo& a = d.l[(size_t)lado].m[(size_t)za];
	const int pa = d.atkM(a);
	pierde = false;
	if (zo < 0 || zo >= ia::ZONAS || d.l[(size_t)(1 - lado)].m[(size_t)zo].vacia()) return "ATK " + std::to_string(pa) + " directo";
	const ia::Monstruo& o = d.l[(size_t)(1 - lado)].m[(size_t)zo];
	const bool oculto = o.bocaAbajo && !o.visto;
	const bool def = o.defensa || o.bocaAbajo;
	const int po = def ? c.verDef(d, o) : c.verAtk(d, o);
	std::string s = "ATK " + std::to_string(pa) + " vs " + (def ? "DEF " : "ATK ") + std::to_string(po) + (oculto ? " (boca abajo: valor estimado; real DEF " + std::to_string(defDe(d, o)) + ")" : "");
	if (def) {
		pierde = pa < po;
		s += pa > po ? " -> ganaria" : pa == po ? " -> sin efecto" : " -> perderia " + std::to_string(po - pa) + " LP";
	} else {
		pierde = pa <= po;
		s += pa > po ? " -> ganaria" : pa == po ? " -> ambos destruidos" : " -> perderia " + std::to_string(po - pa) + " LP y el monstruo";
	}
	return s;
}

inline bool mismaJugada(const ia::Jugada& a, const ia::Jugada& b)
{
	return a.tipo == b.tipo && a.mano == b.mano && a.zona == b.zona && a.objetivo == b.objetivo && a.carta == b.carta && a.tributos == b.tributos;
}

inline std::string alternativas(const ia::Duelo& d, int lado, const ia::Jugada& elegida)
{
	const Estado& e = est();
	std::vector<const ia::Jugada*> v;
	for (const ia::Jugada& j : e.cand) v.push_back(&j);
	std::stable_sort(v.begin(), v.end(), [](const ia::Jugada* a, const ia::Jugada* b) { return a->puntos > b->puntos; });
	std::string s;
	int n = 0;
	for (const ia::Jugada* j : v) {
		if (mismaJugada(*j, elegida)) continue;
		if (n++ >= 3) break;
		char p[24];
		snprintf(p, sizeof(p), " (%.0f)", j->puntos);
		s += (s.empty() ? "" : "; ") + descJugada(d, lado, *j) + p;
	}
	return s;
}

inline void alDecidir(const ia::Duelo& d, const ia::Jugada& j, const ia::Cerebro& c)
{
	Estado& e = est();
	if (!e.f || &d != e.vig) return;
	const int lado = d.decide();
	// el plan de la IA (cuando cambia)
	const int plan = c.plan.activo() ? c.plan.plan : -1;
	if (plan != e.planVisto[(size_t)(lado & 1)]) {
		e.planVisto[(size_t)(lado & 1)] = plan;
		linea(d, lado, std::string("[CPU plan] ") + (plan >= 0 ? ia::PLANES[plan].clave : "(sin plan)") + " | personalidad " + c.p.clave + ", nivel " + std::to_string(c.nivel));
	}
	// los puntos de la elegida son los de la lista de candidatas (sin el ruido de decision) y se avisa si otra puntuaba mas
	float base = j.puntos, mejorOtra = -1e9f;
	bool enLista = false;
	for (const ia::Jugada& k : e.cand) {
		if (mismaJugada(k, j)) base = k.puntos, enLista = true;
		else mejorOtra = std::max(mejorOtra, k.puntos);
	}
	char pt[32];
	snprintf(pt, sizeof(pt), "%.0f", base);
	std::string alt = alternativas(d, lado, j);
	if (enLista && mejorOtra > base + 0.5f) alt += (alt.empty() ? "" : " ") + std::string("[elegida pese a puntuar menos: el ruido o la busqueda de la IA]");
	const std::string altTxt = alt.empty() ? std::string() : " | otras: " + alt;
	std::string r;
	switch (j.tipo) {
	case ia::J_ATACAR: {
		bool pierde = false;
		const std::string cuenta = cuentaAtaque(d, c, lado, j.zona, j.objetivo, pierde);
		r = "ataca con " + N(cartaJugada(d, lado, j)) + ": " + cuenta + (pierde ? ": ACEPTADO aun asi (puntos " : ": ACEPTADO (puntos ") + pt + ")";
		break;
	}
	case ia::J_FASE:
		if (d.fase == ia::F_BATALLA && d.ef.preg.tipo == ia::P_NADA) {
			std::string no;
			int n = 0;
			for (int z = 0; z < ia::ZONAS && n < 6; z++) {
				if (!d.puedeAtacar(lado, z)) continue;
				const ia::Lado& rival = d.l[(size_t)(1 - lado)];
				if (rival.monstruos() == 0) {
					bool p = false;
					no += (no.empty() ? "" : "; ") + N(d.l[(size_t)lado].m[(size_t)z].id) + ": " + cuentaAtaque(d, c, lado, z, -1, p) + ": RECHAZADO";
					n++;
					continue;
				}
				for (int o = 0; o < ia::ZONAS && n < 6; o++) {
					if (rival.m[(size_t)o].vacia()) continue;
					bool p = false;
					no += (no.empty() ? "" : "; ") + N(d.l[(size_t)lado].m[(size_t)z].id) + " -> " + N(rival.m[(size_t)o].bocaAbajo && !rival.m[(size_t)o].visto ? std::string("?") : rival.m[(size_t)o].id) +
					      ": " + cuentaAtaque(d, c, lado, z, o, p) + ": RECHAZADO";
					n++;
				}
			}
			r = no.empty() ? "pasa a " + std::string(siguienteFase(d)) + ": no tiene ataques posibles" : "NO ataca, pasa de fase: " + no;
		} else {
			r = std::string("pasa a ") + siguienteFase(d) + ": ninguna jugada supera el umbral (referencia 0 puntos)";
		}
		break;
	case ia::J_PASAR:
		if (d.ef.preg.tipo == ia::P_RESPUESTA) {
			std::string mejor;
			float pm = -1e9f;
			for (const ia::Jugada& k : e.cand)
				if (k.tipo != ia::J_PASAR && k.puntos > pm) pm = k.puntos, mejor = descJugada(d, lado, k);
			char b[24];
			snprintf(b, sizeof(b), "%.0f", pm);
			if (mejor.empty()) {   // (nada que activar: ni se anota, seria ruido)
				e.cand.clear();
				return;
			}
			r = "no responde: la mejor activacion posible (" + mejor + ", " + b + " puntos) no supera el umbral";
		} else {
			r = "no elige nada (opcional)";
		}
		break;
	case ia::J_ACTIVAR: {
		r = "activa " + N(cartaJugada(d, lado, j)) + " (puntos " + pt + ")";
		if (d.ef.ataque.hay) {
			const ia::Monstruo& a = d.l[(size_t)d.ef.ataque.lado].m[(size_t)(d.ef.ataque.zona < 0 ? 0 : d.ef.ataque.zona)];
			const ia::Lado& rv = d.l[(size_t)(1 - d.ef.ataque.lado)];
			int dano = d.atkM(a);
			if (d.ef.ataque.objetivo >= 0 && d.ef.ataque.objetivo < ia::ZONAS && !rv.m[(size_t)d.ef.ataque.objetivo].vacia()) {
				const ia::Monstruo& o = rv.m[(size_t)d.ef.ataque.objetivo];
				dano = o.defensa || o.bocaAbajo ? 0 : std::max(0, d.atkM(a) - d.atkM(o));
			}
			r += " | responde al ataque de " + N(a.id) + " (ATK " + std::to_string(d.atkM(a)) + "), dano estimado " + std::to_string(dano);
		}
		r += d.ef.cadena.empty() ? " | sin proteccion en cadena" : " | cadena de " + std::to_string(d.ef.cadena.size()) + " eslabon(es) hasta ahora";
		break;
	}
	case ia::J_INVOCAR: case ia::J_COLOCAR: case ia::J_TOON: case ia::J_FUSION: case ia::J_RITUAL:
		r = descJugada(d, lado, j) + (j.tributos.empty() ? "" : " (sacrifica " + nombresZonas(d, lado, j.tributos) + ")") + ": mejor puntuacion (" + pt + ")";
		if (j.tipo == ia::J_INVOCAR) {
			const ia::Carta* k = d.carta(cartaJugada(d, lado, j));
			if (k) r += ", ATK " + std::to_string(k->atk) + " vs mayor amenaza rival " + std::to_string(c.amenaza(d, 1 - lado)) + ", en ataque";
		}
		if (j.tipo == ia::J_COLOCAR) r += ", boca abajo en defensa";
		break;
	default: r = descJugada(d, lado, j) + ": mejor puntuacion (" + pt + ")"; break;
	}
	if (j.tipo != ia::J_FASE || d.fase == ia::F_BATALLA) r += altTxt;
	linea(d, lado, std::string(lado == 1 ? "[CPU decide] " : "[IA-J decide] ") + r);   // (lado 0 solo en las pruebas IA contra IA)
	e.cand.clear();
}

// ------------------------------------------------------------------------------------------------- empezar / terminar
inline void apagarGanchos()
{
	ia::ganchosLog = ia::GanchosLog{};
}

inline void cerrar(const char* motivo)
{
	Estado& e = est();
	if (e.f) {
		if (motivo && e.vig) {
			linea(*e.vig, e.vig->activo, std::string("----- ") + motivo + " -----");
		}
		fclose(e.f);
		e.f = nullptr;
	}
	apagarGanchos();
	e.vig = nullptr;
}

inline void rotar(const std::string& ruta)
{
	struct stat st;
	if (stat(ruta.c_str(), &st) != 0 || st.st_size <= LIMITE_BYTES) return;
	const std::string vieja = carpetaLog() + "/duelo_log.anterior.txt";
	remove(vieja.c_str());   // (en Windows rename no pisa)
	rename(ruta.c_str(), vieja.c_str());
}

// Al empezar un duelo (despues de Duelo::empezar): abre la bitacora (append), escribe la cabecera y repasa los
// sucesos que el motor ya emitio (el turno 1 y su robo). Con la opcion en No, o si no se puede escribir, no hace nada.
inline bool empezar(const ia::Duelo& d, const Cabecera& c)
{
	Estado& e = est();
	if (e.f) cerrar("duelo interrumpido (se empezo otro)");
	else apagarGanchos();
	e.vig = nullptr;
	if (!opciones::bitacora()) return false;
	const std::string ruta = rutaLog();
	rotar(ruta);
	e.f = fopen(ruta.c_str(), "ab");
	if (!e.f) return false;
	e.vig = &d;
	e.t0 = std::chrono::steady_clock::now();
	e.pendInvoca.clear();
	e.pendObj.clear();
	e.cand.clear();
	e.planVisto = { { -2, -2 } };
	e.turnoVisto = -1;
	e.lineas = 0;
	e.lpPrev = { { d.l[0].lp, d.l[1].lp } };
	char fecha[64] = "";
	const time_t ahora = time(nullptr);
	if (struct tm* tmv = localtime(&ahora)) strftime(fecha, sizeof(fecha), "%Y-%m-%d %H:%M:%S", tmv);
	auto mazo = [&](const std::vector<std::string>& cartas) {
		std::map<std::string, int> cuenta;
		for (const std::string& k : cartas) cuenta[N(k)]++;
		std::string s;
		for (const auto& kv : cuenta) s += (s.empty() ? "" : ", ") + (kv.second > 1 ? std::to_string(kv.second) + "x " : std::string()) + kv.first;
		return s;
	};
	// (el mazo y el extra ya se barajaron y repartieron: la mano inicial esta en l[].mano, el resto en mazo)
	std::vector<std::string> m0 = d.l[0].mazo, m1 = d.l[1].mazo;
	m0.insert(m0.end(), d.l[0].mano.begin(), d.l[0].mano.end());
	m1.insert(m1.end(), d.l[1].mano.begin(), d.l[1].mano.end());
	std::string s = "\n################################################################################\n";
	s += std::string("DUELO NUEVO ") + fecha + " | juego " + (c.version.empty() ? std::string(__DATE__ " " __TIME__) : c.version) + "\n";
	s += "Rival: " + c.rival + (c.variante.empty() ? "" : " (" + c.variante + ")") + " | nivel " + std::to_string(c.nivel) + " | personalidad " + c.personalidad +
	     " | plan " + (c.plan.empty() ? "(sin plan)" : c.plan) + " | IA " + (c.iaHumana ? "humana" : "clasica") + (c.modo.empty() ? "" : " | modo " + c.modo) + "\n";
	s += "Empieza: " + std::string(d.activo == 0 ? "el jugador (J)" : "el rival (CPU)") + " | LP " + std::to_string(d.l[0].lp) + " - " + std::to_string(d.l[1].lp) + "\n";
	s += "Mazo J (" + std::to_string(m0.size()) + " + extra " + std::to_string(d.l[0].extra.size()) + "): " + mazo(m0) + "\n";
	s += "Mazo CPU (" + std::to_string(m1.size()) + " + extra " + std::to_string(d.l[1].extra.size()) + "): " + mazo(m1) + "\n";
	s += "Mano inicial J:";
	for (const std::string& k : d.l[0].mano) s += " " + N(k) + ";";
	s += "\nMano inicial CPU: " + std::to_string(d.l[1].mano.size()) + " cartas\n";
	s += "################################################################################\n";
	escribirCruda(s);
	ia::GanchosLog g;
	g.suceso = &alSuceso;
	g.jugada = &alJugar;
	g.candidatas = &alProponer;
	g.decision = &alDecidir;
	ia::ganchosLog = g;
	for (const ia::Suceso& su : d.sucesos) alSuceso(d, su);   // (el primer turno ya paso dentro de Duelo::empezar)
	return true;
}

inline void terminar(const char* motivo = nullptr) { cerrar(motivo); }

}   // namespace duelo_log
