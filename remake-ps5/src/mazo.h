/*
 * Yugioh Classic Remaster - la coleccion y el mazo del jugador (rama classic-constructor). SOLO logica: sin Vulkan ni
 * la interfaz (la pantalla del constructor esta en constructor.h y su dibujo en constructor_ui.h).
 *
 * Datos (assets/ui/, los prepara la rama classic-datos):
 *   cartas_ui.txt        id|nombre|clase|ATRIBUTO|raza|nivel|atk|def|texto   (clase normal/efecto/fusion/ritual/magia/trampa)
 *   cartas_classic.txt   id|passcode|limite|subtipo  (limite 0 prohibida, 1 limitada, 2 semilimitada, 3; sin linea o
 *                        sin archivo: 3). Lista prohibida/limitada del Formato Goat (abril 2005)
 *
 * Coleccion: el jugador tiene de cada carta tantas copias como permite su limite (3, o 2/1/0 segun la lista).
 * Mazo: principal 40-60 cartas (sin fusiones) y mazo de fusion aparte, hasta 15 (DECISION: en 2005 no habia tope
 * oficial para el mazo de fusion; se usa 15 para que quepa en la pantalla y siga la regla posterior). Las copias de
 * una carta (principal + fusion) no pasan de su limite.
 * El mazo se guarda en texto plano (una linea por carta: "principal <id> <n>" o "fusion <id> <n>"), ver rutaMazo().
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unordered_map>
#include <vector>

#include "constructor_textos.h"   // textos ES/EN (T)
#include "guardado.h"             // rama classic-pulido: guardado atomico y aviso sin carpeta de datos

namespace mazo {

using constructor::T;

constexpr int MIN_PRINCIPAL = 40, MAX_PRINCIPAL = 60, MAX_FUSION = 15, MANO_INICIAL = 5;

struct Carta {
	std::string id, nombre, clase, atributo, raza, texto, subtipo, passcode;
	std::string atributoClave, razaClave;   // (en espanol siempre: los filtros no cambian con el idioma)
	std::string busqueda;                   // nombre normalizado (mayusculas sin tildes) para buscar
	int nivel{ 0 }, atk{ 0 }, def{ 0 }, limite{ 3 };
	// rama classic-personajes: carta firma de un personaje (cartas_classic.txt ...|firma|duenno,duenno). Bloqueada hasta
	// vencer a uno de sus duennos en el modo historia: mientras, limite = 0 (no se puede usar)
	bool firma{ false }, bloqueada{ false };
	int limiteBase{ 3 };
	std::vector<std::string> duennos;
	char inicial{ '#' };   // A..Z (sin tildes) o '#'
};

inline std::vector<std::string> partir(const std::string& linea, char sep = '|')
{
	std::vector<std::string> c;
	size_t a = 0, b;
	while ((b = linea.find(sep, a)) != std::string::npos) {
		c.push_back(linea.substr(a, b - a));
		a = b + 1;
	}
	c.push_back(linea.substr(a));
	return c;
}

// la primera letra del nombre, sin tilde y en mayuscula (UTF-8 de las vocales con tilde, la enie y la dieresis)
inline char inicialDe(const std::string& n)
{
	for (size_t i = 0; i < n.size(); i++) {
		unsigned char c = (unsigned char)n[i];
		if (c >= 'a' && c <= 'z') return (char)(c - 32);
		if (c >= 'A' && c <= 'Z') return (char)c;
		if (c >= '0' && c <= '9') return '#';
		if (c == 0xC3 && i + 1 < n.size()) {
			const unsigned char d = (unsigned char)n[i + 1] | 0x20;   // (mayuscula -> minuscula: +0x20)
			if (d == 0xA1) return 'A';
			if (d == 0xA9) return 'E';
			if (d == 0xAD) return 'I';
			if (d == 0xB3) return 'O';
			if (d == 0xBA || d == 0xBC) return 'U';
			if (d == 0xB1) return 'N';
			return '#';
		}
		if (c == 0xC2) { i++; continue; }   // signos de apertura (¡, ¿)
		if (c == '"' || c == '\'' || c == ' ' || c == '(' || c == '-') continue;
		return '#';
	}
	return '#';
}

// para buscar por nombre: mayusculas sin tildes (UTF-8 de las vocales con tilde, la enie y la dieresis); lo demas
// que no sea letra o numero, un espacio
inline std::string normalizar(const std::string& n)
{
	std::string r;
	for (size_t i = 0; i < n.size(); i++) {
		unsigned char c = (unsigned char)n[i];
		if (c >= 'a' && c <= 'z') r += (char)(c - 32);
		else if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) r += (char)c;
		else if (c == 0xC3 && i + 1 < n.size()) {
			const unsigned char d = (unsigned char)n[++i] | 0x20;
			r += d == 0xA1 || d == 0xA0 || d == 0xA4 ? 'A' : d == 0xA9 || d == 0xA8 || d == 0xAB ? 'E' : d == 0xAD || d == 0xAC || d == 0xAF ? 'I'
			   : d == 0xB3 || d == 0xB2 || d == 0xB6 ? 'O' : d == 0xBA || d == 0xB9 || d == 0xBC ? 'U' : d == 0xB1 ? 'N' : ' ';
		} else if (c >= 0x80) {
			while (i + 1 < n.size() && ((unsigned char)n[i + 1] & 0xC0) == 0x80) i++;
			r += ' ';
		} else {
			r += ' ';
		}
	}
	return r;
}

// orden de las clases en la coleccion y en el filtro
inline int ordenClase(const std::string& c)
{
	static const char* const C[] = { "normal", "efecto", "ritual", "fusion", "magia", "trampa" };
	for (int i = 0; i < 6; i++) {
		if (c == C[i]) return i;
	}
	return 6;
}
inline bool esMonstruo(const std::string& clase) { return clase == "normal" || clase == "efecto" || clase == "ritual" || clase == "fusion"; }

class Coleccion {
public:
	std::vector<Carta> cartas;   // ordenadas: por clase y por nombre

	int version{ -1 };   // versionIdioma() con que se leyo (al cambiar el idioma se vuelve a leer)
	// rama classic-personajes: personajes vencidos en el modo historia (desbloquean sus cartas firma) y el nombre de
	// un personaje en el idioma actual (para "Vence a X"); los pone el juego (duelo_juego.h)
	std::set<std::string> vencidos;
	std::function<std::string(const std::string&)> nombrePersonaje;

	// aplica los bloqueos de las cartas firma (tras leer y cada vez que cambian los vencidos)
	// rama classic-constructor2: Opciones > "Permitir cartas prohibidas" (opciones.txt, permitir_prohibidas=1): sin la
	// banlist Goat (las prohibidas se pueden usar y limitada / semilimitada no cuentan: siempre hasta 3). limiteBase
	// guarda el estado Goat de cada carta (se sigue mostrando)
	bool permitirProhibidas{ false };

	// aplica los bloqueos de las cartas firma y la banlist (tras leer y cada vez que cambian los vencidos o la opcion)
	void desbloquear(const std::set<std::string>& v)
	{
		vencidos = v;
		int bloq = 0;
		for (Carta& k : cartas) {
			k.bloqueada = false;
			if (k.firma) {
				k.bloqueada = true;
				for (const std::string& d : k.duennos)
					if (vencidos.count(d)) k.bloqueada = false;
			}
			k.limite = k.bloqueada ? 0 : permitirProhibidas ? 3 : k.limiteBase;
			bloq += k.bloqueada ? 1 : 0;
		}
		fprintf(stderr, "[mazo] cartas firma bloqueadas: %d%s\n", bloq, permitirProhibidas ? " (banlist Goat desactivada en Opciones)" : "");
	}
	void ponerPermitirProhibidas(bool si)
	{
		if (si == permitirProhibidas) return;
		permitirProhibidas = si;
		desbloquear(vencidos);
	}
	// "Vence a X" (o a X / Y) de una carta firma bloqueada
	std::string venceA(const Carta& k) const
	{
		std::string r;
		for (const std::string& d : k.duennos) {
			if (!r.empty()) r += " / ";
			r += nombrePersonaje ? nombrePersonaje(d) : d;
		}
		return T("Vence a ") + r;
	}

	// lee cartas_ui.txt y cartas_classic.txt de la carpeta ui (con "/" al final); en ingles los textos salen de
	// cartas_ui_en.txt (la carta que falte ahi queda en espanol)
	bool leer(const std::string& carpetaUi)
	{
		cartas.clear();
		indice.clear();
		version = constructor::versionIdioma();
		std::string l;
		for (int pasada = 0; pasada < 2; pasada++) {
			const std::string suf = pasada ? constructor::sufijoIdioma() : "";
			if (pasada && suf.empty()) break;
			std::ifstream f(carpetaUi + "cartas_ui" + suf + ".txt");
			while (std::getline(f, l)) {
				if (!l.empty() && l.back() == '\r') l.pop_back();
				if (l.empty() || l[0] == '#') continue;
				const std::vector<std::string> c = partir(l);
				if (c.size() < 9 || c[0].empty()) continue;
				if (pasada) {   // (ingles: solo los textos de las que ya estan)
					auto it = indice.find(c[0]);
					if (it == indice.end()) continue;
					Carta& k = cartas[it->second];
					k.nombre = c[1]; k.atributo = c[3]; k.raza = c[4]; k.texto = c[8];
					k.inicial = inicialDe(k.nombre);
					k.busqueda = normalizar(k.nombre);
					continue;
				}
				if (indice.count(c[0])) continue;
				Carta k;
				k.id = c[0]; k.nombre = c[1]; k.clase = c[2]; k.atributo = c[3]; k.raza = c[4];
				k.nivel = atoi(c[5].c_str()); k.atk = atoi(c[6].c_str()); k.def = atoi(c[7].c_str()); k.texto = idioma::reglasEs(c[8]);   // (rama classic-visual-final)
				k.inicial = inicialDe(k.nombre);
				k.atributoClave = k.atributo;
				k.razaClave = k.raza;
				k.busqueda = normalizar(k.nombre);
				indice[k.id] = (int)cartas.size();
				cartas.push_back(std::move(k));
			}
		}
		std::ifstream g(carpetaUi + "cartas_classic.txt");
		int limitadas = 0;
		while (std::getline(g, l)) {
			if (!l.empty() && l.back() == '\r') l.pop_back();
			if (l.empty() || l[0] == '#') continue;
			const std::vector<std::string> c = partir(l);
			auto it = indice.find(c[0]);
			if (it == indice.end()) continue;
			Carta& k = cartas[it->second];
			if (c.size() > 1) k.passcode = c[1];
			if (c.size() > 2 && !c[2].empty()) k.limite = std::clamp(atoi(c[2].c_str()), 0, 3);
			if (c.size() > 3) k.subtipo = c[3];
			k.limiteBase = k.limite;
			if (c.size() > 5 && c[4] == "firma") {
				k.firma = true;
				k.duennos = partir(c[5], ',');
			}
			limitadas += k.limite < 3 ? 1 : 0;
		}
		std::stable_sort(cartas.begin(), cartas.end(), [](const Carta& a, const Carta& b) {
			const int ca = ordenClase(a.clase), cb = ordenClase(b.clase);
			return ca != cb ? ca < cb : a.nombre < b.nombre;
		});
		indice.clear();
		for (size_t i = 0; i < cartas.size(); i++) indice[cartas[i].id] = (int)i;
		fprintf(stderr, "[mazo] coleccion: %d cartas (%d con limite < 3)\n", (int)cartas.size(), limitadas);
		desbloquear(vencidos);   // (rama classic-personajes: cartas firma)
		return !cartas.empty();
	}

	int buscar(const std::string& id) const
	{
		auto it = indice.find(id);
		return it == indice.end() ? -1 : it->second;
	}
	const Carta* carta(const std::string& id) const
	{
		const int n = buscar(id);
		return n >= 0 ? &cartas[n] : nullptr;
	}
	// copias que tiene el jugador (= su limite en el Formato Goat)
	int copias(const std::string& id) const
	{
		const Carta* k = carta(id);
		return k ? k->limite : 0;
	}
	bool esFusion(const std::string& id) const
	{
		const Carta* k = carta(id);
		return k && k->clase == "fusion";
	}

private:
	std::unordered_map<std::string, int> indice;
};

// generador simple (sin <random>: igual en el PC y en la consola)
struct Azar {
	uint32_t s{ 0x9E3779B9u };
	explicit Azar(uint32_t semilla = 0x9E3779B9u) : s(semilla ? semilla : 1u) {}
	uint32_t siguiente()
	{
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	}
	int hasta(int n) { return n > 0 ? (int)(siguiente() % (uint32_t)n) : 0; }
};

template <class T> void barajar(std::vector<T>& v, Azar& a)
{
	for (int i = (int)v.size() - 1; i > 0; i--) std::swap(v[i], v[a.hasta(i + 1)]);
}

struct Mazo {
	std::vector<std::string> principal, fusion;   // una entrada por copia
	std::string nombre;                           // (rama classic-constructor2: varios mazos con nombre)

	int cuantas(const std::string& id) const
	{
		return (int)(std::count(principal.begin(), principal.end(), id) + std::count(fusion.begin(), fusion.end(), id));
	}
	// se puede agregar una copia mas? (motivo: por que no)
	bool sePuedeAgregar(const Coleccion& col, const std::string& id, std::string* motivo = nullptr) const
	{
		const Carta* k = col.carta(id);
		auto no = [&](const std::string& m) {
			if (motivo) *motivo = m;
			return false;
		};
		if (!k) return no(T("Carta desconocida"));
		if (k->bloqueada) return no(T("Carta firma bloqueada") + ": " + col.venceA(*k) + T(" para desbloquear"));
		if (k->limite == 0) return no(T("Prohibida en el Formato Goat"));
		if (cuantas(id) >= k->limite) return no(T(k->limite == 1 ? "Limitada: 1 copia" : k->limite == 2 ? "Semilimitada: 2 copias" : "Máximo 3 copias"));
		if (k->clase == "fusion") {
			if ((int)fusion.size() >= MAX_FUSION) return no(T("Mazo de fusión lleno (15)"));
		} else if ((int)principal.size() >= MAX_PRINCIPAL) {
			return no(T("Mazo principal lleno (60)"));
		}
		return true;
	}
	bool agregar(const Coleccion& col, const std::string& id, std::string* motivo = nullptr)
	{
		if (!sePuedeAgregar(col, id, motivo)) return false;
		(col.esFusion(id) ? fusion : principal).push_back(id);
		ordenar(col);
		return true;
	}
	bool quitar(const std::string& id)
	{
		for (auto* v : { &principal, &fusion }) {
			auto it = std::find(v->begin(), v->end(), id);
			if (it != v->end()) {
				v->erase(it);
				return true;
			}
		}
		return false;
	}
	// por clase y nombre, como la coleccion
	void ordenar(const Coleccion& col)
	{
		auto cmp = [&](const std::string& a, const std::string& b) { return col.buscar(a) < col.buscar(b); };
		std::stable_sort(principal.begin(), principal.end(), cmp);
		std::stable_sort(fusion.begin(), fusion.end(), cmp);
	}
	// valido para jugar? (motivo: el primer problema)
	bool valido(const Coleccion& col, std::string* motivo = nullptr) const
	{
		auto no = [&](const std::string& m) {
			if (motivo) *motivo = m;
			return false;
		};
		const int n = (int)principal.size();
		if (n < MIN_PRINCIPAL) return no(T("Faltan ") + std::to_string(MIN_PRINCIPAL - n) + T(" cartas (mínimo 40)"));
		if (n > MAX_PRINCIPAL) return no(T("Sobran ") + std::to_string(n - MAX_PRINCIPAL) + T(" cartas (máximo 60)"));
		if ((int)fusion.size() > MAX_FUSION) return no(T("Mazo de fusión: máximo 15"));
		std::unordered_map<std::string, int> c;
		for (const std::string& id : principal) {
			if (col.esFusion(id)) return no(T("Una fusión en el mazo principal"));
			c[id]++;
		}
		for (const std::string& id : fusion) {
			if (!col.esFusion(id)) return no(T("Una carta que no es fusión en el mazo de fusión"));
			c[id]++;
		}
		for (const auto& p : c) {
			const Carta* k = col.carta(p.first);
			if (!k) return no(T("Carta desconocida: ") + p.first);
			if (k->bloqueada) return no(k->nombre + ": " + T("carta firma bloqueada") + " (" + col.venceA(*k) + T(" para desbloquear") + ")");
			if (k->limite == 0) return no(k->nombre + ": " + T("prohibida en el Formato Goat"));
			if (p.second > k->limite)
				return no(k->nombre + ": " + T(k->limite == 1 ? "limitada a 1 copia" : k->limite == 2 ? "semilimitada a 2 copias" : "máximo 3 copias"));
		}
		return true;
	}

	// ---- archivo
	bool guardar(const std::string& ruta) const
	{
		std::string s;   // (rama classic-pulido: guardado atomico, guardado.h)
		guardado::agregar(s, "# Yugioh Classic Remaster - mazo del jugador (principal %d, fusion %d)\n", (int)principal.size(), (int)fusion.size());
		if (!nombre.empty()) guardado::agregar(s, "nombre %s\n", nombre.c_str());   // (las versiones viejas la saltan: no es una carta)
		for (int lado = 0; lado < 2; lado++) {
			const std::vector<std::string>& v = lado ? fusion : principal;
			for (size_t i = 0; i < v.size();) {
				size_t j = i;
				while (j < v.size() && v[j] == v[i]) j++;
				guardado::agregar(s, "%s %s %d\n", lado ? "fusion" : "principal", v[i].c_str(), (int)(j - i));
				i = j;
			}
		}
		if (!guardado::guardarAtomico(ruta, s)) return false;
		fprintf(stderr, "[mazo] guardado en %s (%d + %d)\n", ruta.c_str(), (int)principal.size(), (int)fusion.size());
		return true;
	}
	// solo las cartas que estan en la coleccion (y donde corresponden: las fusiones al mazo de fusion)
	bool cargar(const std::string& ruta, const Coleccion& col)
	{
		std::ifstream f(ruta);
		if (!f) return false;
		principal.clear();
		fusion.clear();
		nombre.clear();
		std::string l;
		while (std::getline(f, l)) {
			if (!l.empty() && l.back() == '\r') l.pop_back();
			if (l.empty() || l[0] == '#') continue;
			if (l.rfind("nombre ", 0) == 0) {
				nombre = l.substr(7);
				continue;
			}
			std::istringstream s(l);
			std::string lado, id;
			int n = 1;
			if (!(s >> lado >> id)) continue;
			s >> n;
			if (col.buscar(id) < 0) continue;
			for (int k = 0; k < std::clamp(n, 0, 3); k++) (col.esFusion(id) ? fusion : principal).push_back(id);
		}
		ordenar(col);
		fprintf(stderr, "[mazo] cargado de %s (%d + %d)\n", ruta.c_str(), (int)principal.size(), (int)fusion.size());
		return true;
	}

	// ---- mazos armados por el juego
	// El mazo inicial: monstruos normales y de efecto de mas ataque, luego magias y trampas, hasta 40 (con las copias
	// que permite cada limite) y todas las fusiones que entren. Si la coleccion no alcanza, lo que haya
	static Mazo porDefecto(const Coleccion& col)
	{
		Mazo m;
		std::vector<const Carta*> mons, mt;
		for (const Carta& k : col.cartas) {
			if (k.limite == 0) continue;
			if (k.clase == "fusion") {
				if ((int)m.fusion.size() < MAX_FUSION) m.fusion.push_back(k.id);
			} else if (k.clase == "normal" || k.clase == "efecto") {
				if (k.nivel <= 4) mons.push_back(&k);   // (sin tributos: se puede jugar sin motor de reglas)
			} else if (k.clase == "magia" || k.clase == "trampa") {
				mt.push_back(&k);
			}
		}
		std::stable_sort(mons.begin(), mons.end(), [](const Carta* a, const Carta* b) { return a->atk > b->atk; });
		auto llenar = [&](const std::vector<const Carta*>& v, int hasta) {
			for (int ronda = 1; ronda <= 3; ronda++) {   // una copia de cada, despues la segunda, la tercera
				for (const Carta* k : v) {
					if ((int)m.principal.size() >= hasta) return;
					if (k->limite >= ronda) m.principal.push_back(k->id);
				}
			}
		};
		llenar(mons, 24);
		llenar(mt, MIN_PRINCIPAL);
		// si faltan (pocas magias/trampas): mas monstruos, y si aun faltan, los de nivel alto
		llenar(mons, MIN_PRINCIPAL);
		if ((int)m.principal.size() < MIN_PRINCIPAL) {
			std::vector<const Carta*> resto;
			for (const Carta& k : col.cartas) {
				if (k.limite > 0 && k.clase != "fusion" && std::find(mons.begin(), mons.end(), &k) == mons.end() &&
				    std::find(mt.begin(), mt.end(), &k) == mt.end()) resto.push_back(&k);
			}
			llenar(resto, MIN_PRINCIPAL);
		}
		m.ordenar(col);
		return m;
	}
	// Un mazo aleatorio valido de 40 (el del rival): ~22 monstruos y el resto magias y trampas, sin pasar los limites
	static Mazo aleatorio(const Coleccion& col, Azar& a)
	{
		Mazo m;
		std::vector<std::string> mons, mt, fus;
		for (const Carta& k : col.cartas) {
			for (int c = 0; c < k.limite; c++) {
				if (k.clase == "fusion") fus.push_back(k.id);
				else if (k.clase == "normal" || k.clase == "efecto") (k.nivel <= 6 ? mons : mt).push_back(k.id);
				else if (k.clase == "magia" || k.clase == "trampa") mt.push_back(k.id);
				else mt.push_back(k.id);   // (rituales: con las magias, pocas)
			}
		}
		barajar(mons, a);
		barajar(mt, a);
		barajar(fus, a);
		for (const std::string& id : mons) {
			if ((int)m.principal.size() >= 22) break;
			m.principal.push_back(id);
		}
		for (auto* v : { &mt, &mons }) {
			for (const std::string& id : *v) {
				if ((int)m.principal.size() >= MIN_PRINCIPAL) break;
				if (m.cuantas(id) < col.copias(id)) m.principal.push_back(id);
			}
		}
		for (const std::string& id : fus) {
			if ((int)m.fusion.size() >= 5) break;
			if (m.cuantas(id) < col.copias(id)) m.fusion.push_back(id);
		}
		m.ordenar(col);
		return m;
	}
};

// Donde se guarda el mazo. En la consola: /data/yugioh_classic/mazo.txt (fuera de la carpeta del juego: sobrevive a
// las reinstalaciones); si no se puede crear, junto al juego (<assets>/../mazo.txt = /data/homebrew/<titulo>/). En el
// PC: YUGI_MAZO=<ruta> o ./mazo.txt
inline std::string rutaMazo(const std::string& assets)
{
#if defined(PS5_HOST_REFERENCE)
	(void)assets;
	if (const char* v = getenv("YUGI_MAZO")) return v;
	return "mazo.txt";
#else
	const std::string datos = guardado::carpetaDatos();   // (/data/yugioh_classic; Windows: pc/main_windows.cpp)
	mkdir(datos.c_str(), 0777);
	struct stat st{};
	if (stat(datos.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return datos + "/mazo.txt";
	guardado::avisarSinCarpeta();   // (rama classic-pulido: se avisa en pantalla)
	return assets + "../mazo.txt";
#endif
}


// ---------------------------------------------------------------------------------------------- mazos de inicio
// (rama classic-constructor2) Cuatro mazos tematicos prearmados con su carta de campo, para probar los campos desde el
// principio. Cada uno es una lista de "id copias"; lo que no este en la coleccion, sea firma (bloqueable) o pase el
// limite Goat se salta, y si faltan cartas para 40 se completa con monstruos del mismo tema (nivel <= 4, mas ATK).
// Asi siempre salen validos. Las fusiones van a su mazo.
struct Inicio {
	const char *es, *en;
	const char* lista;                                    // "id n,id n,..."
	std::function<bool(const Carta&)> tema;               // para completar
};
inline const std::vector<Inicio>& mazosInicio()
{
	static const std::vector<Inicio> v = {
		{ "Dragones de la Montaña", "Mountain Dragons",
		  "mountain 3,luster_dragon 3,spear_dragon 3,masked_dragon 3,armed_dragon_lv3 2,horus_the_black_flame_dragon_lv4 2,blackland_fire_dragon 2,"
		  "koumori_dragon 2,mirage_dragon 1,element_dragon 1,baby_dragon 1,armed_dragon_lv5 1,luster_dragon_2 1,"
		  "dragon_treasure 2,pot_of_greed 1,heavy_storm 1,mystical_space_typhoon 1,swords_of_revealing_light 1,nobleman_of_crossout 1,smashing_ground 1,"
		  "mirror_force 1,trap_hole 2,sakuretsu_armor 2,magic_cylinder 1,call_of_the_haunted 1,"
		  "gaia_the_dragon_champion 1,fiend_skull_dragon 1,black_skull_dragon 1,thousand_dragon 1",
		  [](const Carta& k) { return k.razaClave == "Dragón"; } },
		{ "Océano Legendario", "Legendary Ocean",
		  "umi 2,a_legendary_ocean 2,7_colored_fish 3,giant_red_seasnake 2,abyss_soldier 2,mermaid_knight 2,great_white 2,sea_serpent_warrior_of_darkness 2,"
		  "cure_mermaid 2,space_mambo 2,unshaven_angler 1,terrorking_salmon 1,the_legendary_fisherman 1,maiden_of_the_aqua 1,"
		  "pot_of_greed 1,heavy_storm 1,mystical_space_typhoon 1,snatch_steal 1,fissure 1,book_of_moon 2,"
		  "torrential_tribute 1,mirror_force 1,trap_hole 2,waboku 1,sakuretsu_armor 2,"
		  "deepsea_shark 1,humanoid_worm_drake 1,roaring_ocean_snake 1",
		  [](const Carta& k) { return k.atributoClave == "AGUA"; } },
		{ "Sombras de Yami", "Shadows of Yami",
		  "yami 3,archfiend_soldier 3,dark_elf 2,giant_orc 2,skilled_dark_magician 2,opticlops 2,la_jinn_the_mystical_genie_of_the_lamp 2,whiptail_crow 2,"
		  "nuvia_the_wicked 1,terrorking_archfiend 1,man_eating_treasure_chest 2,breaker_the_magical_warrior 1,a_man_with_wdjat 1,"
		  "pot_of_greed 1,graceful_charity 1,black_pendant 2,axe_of_despair 2,premature_burial 1,heavy_storm 1,"
		  "ring_of_destruction 1,sakuretsu_armor 2,dark_mirror_force 1,solemn_judgment 1,"
		  "bickuribox 1,dark_balter_the_terrible 1,sanwitch 1,skull_knight 1,reaper_on_the_nightmare 1",
		  [](const Carta& k) { return k.atributoClave == "OSCURIDAD" && (k.razaClave == "Demonio" || k.razaClave == "Lanzador de Conjuros"); } },
		{ "Guerreros de Sogen", "Sogen Warriors",
		  "sogen 3,reinforcement_of_the_army 2,pot_of_greed 1,heavy_storm 1,mystical_space_typhoon 1,swords_of_revealing_light 1,nobleman_of_crossout 1,"
		  "axe_of_despair 1,rush_recklessly 2,mirror_force 1,trap_hole 2,sakuretsu_armor 2,magic_cylinder 1,"
		  "flame_swordsman 1,karbonala_warrior 1,giltia_the_d_knight 1,warrior_of_tradition 1,dark_blade_the_dragon_knight 1",
		  [](const Carta& k) { return k.razaClave == "Guerrero" || k.razaClave == "Guerrero-Bestia"; } },
	};
	return v;
}
inline Mazo armarInicio(const Coleccion& col, const Inicio& ini, bool ingles)
{
	Mazo m;
	m.nombre = ingles ? ini.en : ini.es;
	// (siempre con la banlist Goat, aunque Opciones la desactive: asi el mazo vale con las dos)
	auto sirve = [&](const Carta* k) { return k && !k->firma && !k->bloqueada && k->limiteBase > 0 && m.cuantas(k->id) < k->limiteBase; };
	for (const std::string& par : partir(ini.lista, ',')) {
		std::istringstream e(par);
		std::string id;
		int n = 1;
		e >> id >> n;
		const Carta* k = col.carta(id);
		if (!sirve(k)) {
			fprintf(stderr, "[mazo] mazo de inicio '%s': sin %s\n", ini.es, id.c_str());
			continue;
		}
		for (int i = 0; i < n && sirve(k); i++) m.agregar(col, id);
	}
	// completar hasta 40 con el tema (nivel <= 4, de mas ATK)
	std::vector<const Carta*> c;
	for (const Carta& k : col.cartas) {
		if (sirve(&k) && (k.clase == "normal" || k.clase == "efecto") && k.nivel <= 4 && ini.tema(k)) c.push_back(&k);
	}
	std::stable_sort(c.begin(), c.end(), [](const Carta* a, const Carta* b) { return a->atk > b->atk; });
	for (int ronda = 0; ronda < 3 && (int)m.principal.size() < MIN_PRINCIPAL; ronda++) {
		for (const Carta* k : c) {
			if ((int)m.principal.size() >= MIN_PRINCIPAL) break;
			if (sirve(k)) m.agregar(col, k->id);
		}
	}
	if ((int)m.principal.size() < MIN_PRINCIPAL) {   // (sin tema suficiente: el mazo inicial completa)
		const Mazo d = Mazo::porDefecto(col);
		for (const std::string& id : d.principal) {
			if ((int)m.principal.size() >= MIN_PRINCIPAL) break;
			const Carta* k = col.carta(id);
			if (sirve(k)) m.agregar(col, id);
		}
	}
	m.ordenar(col);
	return m;
}

// ---------------------------------------------------------------------------------------------- varios mazos
// (rama classic-constructor2) Los mazos del jugador. En la carpeta de mazo.txt (consola: /data/yugioh_classic/):
//   mazos.txt          indice: "activo <n>" y una linea "mazo <archivo>" por mazo, en orden
//   mazos/<archivo>    cada mazo en el formato de mazo.txt (con una linea "nombre ...")
//   mazo.txt           copia del mazo activo (el formato de siempre: lo leen las versiones anteriores)
// La primera vez se importa mazo.txt (si existe) como "Mi mazo" y se agregan los mazos de inicio.
constexpr int MAX_MAZOS = 20;   // (como Master Duel)
// rama classic-torneo-extras: los mazos de torneo (datos/mazos_torneo.txt, mazos_torneo.py: D|clave|plan|nombre ES|
// nombre EN, M|clave|ids, X|clave|ids). En el constructor son mazos listos que el jugador copia a sus mazos y edita
struct MazoTorneo {
	std::string clave, nombreEs, nombreEn;
	Mazo mazo;   // (sin nombre: se pone al copiarlo, en el idioma de ese momento)
	const std::string& nombre(bool ingles) const { return ingles && !nombreEn.empty() ? nombreEn : nombreEs; }
};
inline std::vector<MazoTorneo> leerMazosTorneo(const std::string& carpetaUi)
{
	std::vector<MazoTorneo> r;
	std::ifstream f(carpetaUi + "mazos_torneo.txt");
	std::string l;
	auto buscar = [&r](const std::string& clave) -> MazoTorneo* {
		for (MazoTorneo& t : r)
			if (t.clave == clave) return &t;
		return nullptr;
	};
	while (std::getline(f, l)) {
		if (!l.empty() && l.back() == '\r') l.pop_back();
		if (l.size() < 2 || l[1] != '|') continue;
		const std::vector<std::string> c = partir(l);
		if (c.size() < 2) continue;
		if (l[0] == 'D' && c.size() >= 5 && !buscar(c[1])) {
			MazoTorneo t;
			t.clave = c[1];
			t.nombreEs = c[3];
			t.nombreEn = c[4];
			r.push_back(t);
		} else if ((l[0] == 'M' || l[0] == 'X') && c.size() >= 3) {
			MazoTorneo* t = buscar(c[1]);
			if (!t) continue;
			std::vector<std::string>& v = l[0] == 'M' ? t->mazo.principal : t->mazo.fusion;
			v.clear();
			for (const std::string& id : partir(c[2], ' '))
				if (!id.empty()) v.push_back(id);
		}
	}
	r.erase(std::remove_if(r.begin(), r.end(), [](const MazoTorneo& t) { return t.mazo.principal.empty(); }), r.end());
	return r;
}

struct Biblioteca {
	std::vector<Mazo> mazos;
	std::vector<std::string> archivos;
	int activo{ -1 };
	std::string carpeta, rutaCompat;   // carpeta con "/" al final; mazo.txt

	std::string rutaIndice() const { return carpeta + "mazos.txt"; }
	std::string rutaArchivo(int i) const { return carpeta + "mazos/" + archivos[(size_t)i]; }

	bool guardarIndice() const
	{
		std::string s;
		guardado::agregar(s, "# Yugioh Classic Remaster - mazos del jugador (el activo es el que usa el duelo)\nactivo %d\n", activo);
		for (const std::string& a : archivos) guardado::agregar(s, "mazo %s\n", a.c_str());
		return guardado::guardarAtomico(rutaIndice(), s);
	}
	// escribe el mazo i (y mazo.txt si es el activo)
	bool guardarMazo(int i)
	{
		if (i < 0 || i >= (int)mazos.size()) return false;
		const bool ok = mazos[(size_t)i].guardar(rutaArchivo(i));
		if (i == activo && !rutaCompat.empty()) mazos[(size_t)i].guardar(rutaCompat);
		return ok;
	}
	std::string archivoNuevo() const
	{
		for (int n = 1;; n++) {
			const std::string a = "mazo_" + std::to_string(n) + ".txt";
			if (std::find(archivos.begin(), archivos.end(), a) == archivos.end()) return a;
		}
	}
	// agrega un mazo (y lo escribe); devuelve su indice o -1 si ya hay MAX_MAZOS
	int agregar(const Mazo& m)
	{
		if ((int)mazos.size() >= MAX_MAZOS) return -1;
		mazos.push_back(m);
		archivos.push_back(archivoNuevo());
		const int i = (int)mazos.size() - 1;
		guardarMazo(i);
		guardarIndice();
		return i;
	}
	bool borrar(int i)
	{
		if (i < 0 || i >= (int)mazos.size()) return false;
		remove(rutaArchivo(i).c_str());
		mazos.erase(mazos.begin() + i);
		archivos.erase(archivos.begin() + i);
		if (activo == i) activo = -1;
		else if (activo > i) activo--;
		guardarIndice();
		return true;
	}
	// elegir el mazo de los duelos: solo uno valido (motivo: por que no)
	bool usar(int i, const Coleccion& col, std::string* motivo = nullptr)
	{
		if (i < 0 || i >= (int)mazos.size()) return false;
		if (!mazos[(size_t)i].valido(col, motivo)) return false;
		activo = i;
		guardarIndice();
		if (!rutaCompat.empty()) mazos[(size_t)i].guardar(rutaCompat);
		return true;
	}
	const Mazo* mazoActivo() const { return activo >= 0 && activo < (int)mazos.size() ? &mazos[(size_t)activo] : nullptr; }

	// lee los mazos; la primera vez los arma (importa mazo.txt y agrega los de inicio)
	void cargar(const std::string& rutaMazoTxt, const Coleccion& col, bool ingles)
	{
		rutaCompat = rutaMazoTxt;
		const size_t barra = rutaMazoTxt.find_last_of('/');
		carpeta = barra == std::string::npos ? std::string() : rutaMazoTxt.substr(0, barra + 1);
		mkdir((carpeta + "mazos").c_str(), 0777);
		mazos.clear();
		archivos.clear();
		activo = -1;
		std::ifstream f(rutaIndice());
		if (f) {
			std::string l;
			while (std::getline(f, l)) {
				if (!l.empty() && l.back() == '\r') l.pop_back();
				if (l.rfind("activo ", 0) == 0) activo = atoi(l.c_str() + 7);
				if (l.rfind("mazo ", 0) != 0) continue;
				Mazo m;
				const std::string a = l.substr(5);
				if (!m.cargar(carpeta + "mazos/" + a, col)) continue;
				if (m.nombre.empty()) m.nombre = (ingles ? "Deck " : "Mazo ") + std::to_string(mazos.size() + 1);
				mazos.push_back(m);
				archivos.push_back(a);
			}
			if (activo >= (int)mazos.size()) activo = -1;
			fprintf(stderr, "[mazo] %d mazos en %s, activo %d\n", (int)mazos.size(), rutaIndice().c_str(), activo);
			return;
		}
		// primera vez
		Mazo viejo;
		if (viejo.cargar(rutaMazoTxt, col) && !(viejo.principal.empty() && viejo.fusion.empty())) {
			if (viejo.nombre.empty()) viejo.nombre = ingles ? "My deck" : "Mi mazo";
			agregar(viejo);
			if (viejo.valido(col)) activo = 0;
		}
		for (const Inicio& ini : mazosInicio()) {
			const Mazo m = armarInicio(col, ini, ingles);
			std::string motivo;
			if (!m.valido(col, &motivo)) {
				fprintf(stderr, "[mazo] mazo de inicio '%s' no valido: %s\n", ini.es, motivo.c_str());
				continue;
			}
			const int i = agregar(m);
			if (activo < 0) activo = i;
		}
		guardarIndice();
		if (const Mazo* a = mazoActivo()) a->guardar(rutaCompat);
		fprintf(stderr, "[mazo] primera vez: %d mazos (activo %d)\n", (int)mazos.size(), activo);
	}
};

}   // namespace mazo
