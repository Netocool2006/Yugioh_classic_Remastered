/*
 * Yugioh Classic Remaster - APRENDER DEL JUGADOR (rama classic-ia-red; paso 3 de la IA humana, ver ia_red.h y
 * docs/ia_red.md). Se incluye al final de ia.h (fuera de namespace ia).
 *
 *  - GRABAR: en cada duelo contra la IA, al empezar cada turno se guardan los rasgos de la red (ia::rasgosRed) desde
 *    los DOS lados (el rival con su plan, el jugador sin plan), cuantizados a un byte (85 bytes + el lado), y al final
 *    el resultado. Los duelos van a <carpeta>/ia_duelos.bin (en la PS5 /data/yugioh_classic/, junto a variedad.txt):
 *    los ultimos MAX_DUELOS (40, ~350 KB como mucho), guardado atomico (guardado.h) con suma de control (si el
 *    archivo se corrompe se empieza de cero, sin romper nada).
 *  - AJUSTE FINO: al terminar un duelo queda pendiente un ajuste; el juego llama avanzar(ms) cada cuadro FUERA del
 *    duelo (resultado, menus) con una rebanada chica (1,5 ms): nunca traba. Siempre parte de la red BASE (la del
 *    autojuego, ui/ia_red.txt) y la entrena unas pasadas con los duelos grabados (Adam, atada a la base: no se
 *    aleja). Al terminar compara con la base sobre duelos apartados (validacion): si no es peor, la guarda en
 *    <carpeta>/ia_red_jugador.txt (atomico, suma de control) y la IA la usa desde el proximo duelo. Si el archivo del
 *    jugador falta, esta corrupto o no es de la misma forma que la base, se usa la base.
 *  - OPCION: "IA que aprende: Si/No" (Memoria::aprende en variedad.txt). No = ni red, ni grabar, ni ajustar: la IA
 *    humana de antes. "Olvidar lo aprendido" borra los duelos grabados y la red del jugador (vuelve la base).
 */
#pragma once

#include "ia.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace aprendizaje {

constexpr int MAX_DUELOS = 40;      // duelos del jugador que se guardan (los mas viejos salen)
constexpr int MAX_POS_DUELO = 160;  // posiciones por duelo (2 por turno: 80 turnos)
constexpr int MIN_DUELOS_AJUSTE = 3;
constexpr int BYTES_POS = ia::RED_ENTRADAS + 1;   // los rasgos y el lado
constexpr uint32_t MAGIA = 0x44524359u;           // "YCRD"
constexpr uint32_t VERSION = 1;

inline void cuantizar(const float* x, uint8_t* q)
{
	for (int i = 0; i < ia::RED_ENTRADAS; i++) {
		const float v = std::max(0.0f, std::min(2.55f, x[i]));
		q[i] = (uint8_t)std::lround(v * 100.0f);
	}
}
inline void descuantizar(const uint8_t* q, float* x)
{
	for (int i = 0; i < ia::RED_ENTRADAS; i++) x[i] = (float)q[i] * 0.01f;
}

struct DueloGrabado {
	int ganador{ -1 };             // 0, 1, 2 (empate) o -1 (sin terminar: no entrena)
	std::vector<uint8_t> pos;      // n * BYTES_POS
	int n() const { return (int)(pos.size() / BYTES_POS); }
	// agrega la posicion de "d" vista por "yo"
	void agregar(const ia::Duelo& d, int yo, int plan)
	{
		float x[ia::RED_ENTRADAS];
		ia::rasgosRed(d, yo, plan, x);
		const size_t k = pos.size();
		pos.resize(k + BYTES_POS);
		cuantizar(x, &pos[k]);
		pos[k + ia::RED_ENTRADAS] = (uint8_t)yo;
	}
	// el resultado de la posicion i para su lado (1 gana, 0 pierde, 0,5 empate)
	float etiqueta(int i) const
	{
		const int yo = pos[(size_t)i * BYTES_POS + ia::RED_ENTRADAS];
		return ganador == 2 ? 0.5f : ganador == yo ? 1.0f : 0.0f;
	}
};

// ia_duelos.bin: MAGIA, VERSION, RED_ENTRADAS, n; por duelo: ganador (1 byte), posiciones (2 bytes), los bytes; al
// final la suma (FNV-1a de todo lo anterior)
struct Archivo {
	std::vector<DueloGrabado> duelos;
	static void u32(std::string& s, uint32_t v)
	{
		for (int i = 0; i < 4; i++) s += (char)((v >> (8 * i)) & 0xFF);
	}
	static bool leerU32(const std::string& s, size_t& p, uint32_t& v)
	{
		if (p + 4 > s.size()) return false;
		v = 0;
		for (int i = 0; i < 4; i++) v |= (uint32_t)(uint8_t)s[p + (size_t)i] << (8 * i);
		p += 4;
		return true;
	}
	std::string serializar() const
	{
		std::string s;
		u32(s, MAGIA), u32(s, VERSION), u32(s, (uint32_t)ia::RED_ENTRADAS), u32(s, (uint32_t)duelos.size());
		for (const DueloGrabado& d : duelos) {
			s += (char)(int8_t)d.ganador;
			const int n = d.n();
			s += (char)(n & 0xFF), s += (char)((n >> 8) & 0xFF);
			s.append((const char*)d.pos.data(), d.pos.size());
		}
		u32(s, ia::Red::fnv(s.data(), s.size()));
		return s;
	}
	bool deserializar(const std::string& s)
	{
		duelos.clear();
		if (s.size() < 20) return false;
		uint32_t suma = 0;
		size_t q = s.size() - 4;
		if (!leerU32(s, q, suma) || ia::Red::fnv(s.data(), s.size() - 4) != suma) return false;
		size_t p = 0;
		uint32_t m, v, e, n;
		if (!leerU32(s, p, m) || !leerU32(s, p, v) || !leerU32(s, p, e) || !leerU32(s, p, n)) return false;
		if (m != MAGIA || v != VERSION || e != (uint32_t)ia::RED_ENTRADAS || n > (uint32_t)MAX_DUELOS * 4) return false;
		const size_t fin = s.size() - 4;
		for (uint32_t k = 0; k < n; k++) {
			if (p + 3 > fin) return (duelos.clear(), false);
			DueloGrabado d;
			d.ganador = (int8_t)s[p];
			const int np = (uint8_t)s[p + 1] | ((uint8_t)s[p + 2] << 8);
			p += 3;
			if (np > MAX_POS_DUELO || p + (size_t)np * BYTES_POS > fin) return (duelos.clear(), false);
			d.pos.assign((const uint8_t*)s.data() + p, (const uint8_t*)s.data() + p + (size_t)np * BYTES_POS);
			p += (size_t)np * BYTES_POS;
			duelos.push_back(std::move(d));
		}
		if (p != fin) return (duelos.clear(), false);
		return true;
	}
	bool leer(const std::string& ruta)
	{
		std::ifstream f(ruta, std::ios::binary);
		if (!f) return (duelos.clear(), false);
		std::stringstream ss;
		ss << f.rdbuf();
		return deserializar(ss.str());
	}
	bool guardar(const std::string& ruta) const { return guardado::guardarAtomico(ruta, serializar()); }
	// el conjunto de entrenamiento: X[n][RED_ENTRADAS], y[n]; "cada"/"resto": solo los duelos con k % cada == resto
	// (cada = 0: todos); lado >= 0: solo las posiciones vistas desde ese lado
	int conjunto(std::vector<float>& X, std::vector<float>& y, int cada = 0, int resto = 0, bool excluir = false, int lado = -1) const
	{
		X.clear(), y.clear();
		for (size_t k = 0; k < duelos.size(); k++) {
			const DueloGrabado& d = duelos[k];
			if (d.ganador < 0) continue;
			if (cada > 0 && (((int)k % cada == resto) == excluir)) continue;
			for (int i = 0; i < d.n(); i++) {
				if (lado >= 0 && d.pos[(size_t)i * BYTES_POS + ia::RED_ENTRADAS] != lado) continue;
				const size_t a = X.size();
				X.resize(a + ia::RED_ENTRADAS);
				descuantizar(&d.pos[(size_t)i * BYTES_POS], &X[a]);
				y.push_back(d.etiqueta(i));
			}
		}
		return (int)y.size();
	}
};

class Aprendiz {
public:
	bool activo{ true };          // la opcion "IA que aprende"
	std::string carpeta;          // donde van los archivos del jugador (con "/" al final)
	ia::Red base, jugador;
	bool hayJugador{ false };
	Archivo grabados;
	DueloGrabado actual;
	bool grabando{ false };
	int turnoVisto{ -1 };
	// el ajuste fino (por rebanadas)
	struct Ajuste {
		bool pendiente{ false }, enCurso{ false }, evaluando{ false }, guardarPendiente{ false };
		size_t evalCursor{ 0 };
		double sumaBase{ 0.0 }, sumaNueva{ 0.0 };
		ia::Red r;
		ia::Entrenador opt;
		std::vector<float> X, y, Xv, yv;
		std::vector<int> orden;
		size_t cursor{ 0 };
		int epoca{ 0 }, epocas{ 4 }, lote{ 64 };
		uint32_t azar{ 12345u };
	} aj;
	struct Estadisticas {
		int ajustes{ 0 }, aceptados{ 0 }, rechazados{ 0 }, pasos{ 0 };
		float perdidaBase{ 0.0f }, perdidaNueva{ 0.0f };
		double ms{ 0.0 }, msMax{ 0.0 }, msGuardar{ 0.0 };   // (msMax: la rebanada mas larga sin contar la de guardar)
		bool jugadorCorrupto{ false }, duelosCorruptos{ false };
	} st;

	std::string rutaJugador() const { return carpeta + "ia_red_jugador.txt"; }
	std::string rutaDuelos() const { return carpeta + "ia_duelos.bin"; }

	// la red base del titulo y lo del jugador (si esta y vale)
	bool preparar(const std::string& rutaBase, const std::string& carpetaJugador)
	{
		carpeta = carpetaJugador;
		if (!carpeta.empty() && carpeta.back() != '/') carpeta += '/';
		const bool ok = base.leer(rutaBase);
		if (ok) base.origen = "base";
		fprintf(stderr, "[ia-red] red base %s: %s (generacion %d, mezcla %.2f)\n", rutaBase.c_str(), ok ? "cargada" : "NO cargada (la IA humana juega sin red)",
		        base.generacion, base.mezcla);
		leerJugador();
		st.duelosCorruptos = false;
		std::ifstream existe(rutaDuelos(), std::ios::binary);
		if (existe && !grabados.leer(rutaDuelos())) {
			st.duelosCorruptos = true;
			fprintf(stderr, "[ia-red] %s corrupto: se empieza de cero\n", rutaDuelos().c_str());
		}
		return ok;
	}
	void leerJugador()
	{
		hayJugador = false;
		st.jugadorCorrupto = false;
		std::ifstream existe(rutaJugador(), std::ios::binary);
		if (!existe) return;
		ia::Red r;
		if (!r.leer(rutaJugador()) || !base.valida() || r.n1 != base.n1 || r.n2 != base.n2) {
			st.jugadorCorrupto = true;
			fprintf(stderr, "[ia-red] %s no vale (corrupto o de otra red): se usa la base\n", rutaJugador().c_str());
			return;
		}
		jugador = r;
		hayJugador = true;
	}
	// la red que usa la IA (nullptr: sin red, como antes)
	const ia::Red* red() const
	{
		if (!activo || !base.valida()) return nullptr;
		return hayJugador ? &jugador : &base;
	}

	// ---- grabar
	void empezarDuelo()
	{
		actual = DueloGrabado{};
		grabando = activo && base.valida();
		turnoVisto = -1;
	}
	// cada cuadro del duelo: al empezar cada turno, la posicion desde los dos lados
	void mirar(const ia::Duelo& d, int planRival)
	{
		if (!grabando || d.ganador >= 0 || d.turno == turnoVisto) return;
		turnoVisto = d.turno;
		if (actual.n() + 2 > MAX_POS_DUELO) return;
		actual.agregar(d, 1, planRival);
		actual.agregar(d, 0, -1);
	}
	// al terminar: se guarda (los ultimos MAX_DUELOS) y queda pendiente el ajuste. Abandonar: ganador -1 (no se guarda)
	bool terminarDuelo(int ganador)
	{
		if (!grabando) return false;
		grabando = false;
		if (ganador < 0 || actual.n() < 2) return false;
		actual.ganador = ganador;
		grabados.duelos.push_back(std::move(actual));
		actual = DueloGrabado{};
		while ((int)grabados.duelos.size() > MAX_DUELOS) grabados.duelos.erase(grabados.duelos.begin());
		const bool ok = grabados.guardar(rutaDuelos());
		aj.pendiente = true;
		aj.enCurso = false;
		fprintf(stderr, "[ia-red] duelo grabado (%d duelos del jugador)%s\n", (int)grabados.duelos.size(), ok ? "" : ": NO se pudo guardar");
		return ok;
	}

	// ---- ajuste fino por rebanadas: true mientras trabaja
	// (por pasos, cada uno en su cuadro: preparar los datos, entrenar por rebanadas, medir y, al final, guardar)
	bool trabajando() const { return aj.pendiente || aj.enCurso || aj.evaluando || aj.guardarPendiente; }
	bool avanzar(double ms)
	{
		if (!activo || !base.valida()) return (aj.pendiente = aj.enCurso = aj.evaluando = aj.guardarPendiente = false);
		const auto t0 = std::chrono::steady_clock::now();
		auto pasado = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); };
		if (aj.guardarPendiente) {   // (lo unico que no se puede partir: el guardado atomico, como el de variedad.txt)
			aj.guardarPendiente = false;
			if (hayJugador) jugador.guardar(rutaJugador());
			else remove(rutaJugador().c_str());
			st.msGuardar = pasado();
			return false;
		}
		if (aj.evaluando) {   // (la validacion, tambien por rebanadas)
			auto perd = [](float p, float y) { return -(y * std::log(std::max(1e-7f, p)) + (1.0f - y) * std::log(std::max(1e-7f, 1.0f - p))); };
			do {
				if (aj.evalCursor >= aj.yv.size()) {
					terminarAjuste();
					break;
				}
				const float* x = &aj.Xv[aj.evalCursor * ia::RED_ENTRADAS];
				aj.sumaBase += perd(base.valor(x), aj.yv[aj.evalCursor]);
				aj.sumaNueva += perd(aj.r.valor(x), aj.yv[aj.evalCursor]);
				aj.evalCursor++;
			} while (pasado() < ms);
			const double m = pasado();
			st.ms += m, st.msMax = std::max(st.msMax, m);
			return true;
		}
		if (!aj.enCurso) {
			if (!aj.pendiente) return false;
			aj.pendiente = false;
			const bool ok = empezarAjuste();
			const double m = pasado();
			st.ms += m, st.msMax = std::max(st.msMax, m);
			return ok;
		}
		const int n = (int)aj.y.size();
		std::vector<float> xb, yb;
		do {
			if (aj.cursor >= aj.orden.size()) {
				aj.epoca++;
				aj.cursor = 0;
				barajar();
				if (aj.epoca >= aj.epocas) {   // (falta medir y guardar: los proximos cuadros)
					aj.enCurso = false;
					aj.evaluando = true;
					aj.evalCursor = 0;
					aj.sumaBase = aj.sumaNueva = 0.0;
					const double m = pasado();
					st.ms += m, st.msMax = std::max(st.msMax, m);
					return true;
				}
			}
			const int b = std::min(aj.lote, n - (int)aj.cursor);
			xb.resize((size_t)b * ia::RED_ENTRADAS);
			yb.resize((size_t)b);
			for (int k = 0; k < b; k++) {
				const int i = aj.orden[aj.cursor + (size_t)k];
				std::copy(&aj.X[(size_t)i * ia::RED_ENTRADAS], &aj.X[(size_t)i * ia::RED_ENTRADAS] + ia::RED_ENTRADAS, &xb[(size_t)k * ia::RED_ENTRADAS]);
				yb[(size_t)k] = aj.y[(size_t)i];
			}
			aj.opt.paso(aj.r, xb.data(), yb.data(), b, &base);
			aj.cursor += (size_t)b;
			st.pasos++;
		} while (pasado() < ms);
		const double m = pasado();
		st.ms += m, st.msMax = std::max(st.msMax, m);
		return true;
	}
	// todo de una vez (las pruebas)
	void ajustarYa()
	{
		while (avanzar(1e9)) {}
	}

	// borra lo aprendido: los duelos grabados y la red del jugador (vuelve la base)
	void olvidar()
	{
		grabados.duelos.clear();
		actual = DueloGrabado{};
		grabando = false;
		hayJugador = false;
		aj = Ajuste{};
		remove(rutaDuelos().c_str());
		remove(rutaJugador().c_str());
		fprintf(stderr, "[ia-red] lo aprendido del jugador se olvido (vuelve la red base)\n");
	}

private:
	void barajar()
	{
		ia::Azar a(aj.azar = ia::mezclar(aj.azar, 77u));
		ia::barajar(aj.orden, a);
	}
	bool empezarAjuste()
	{
		int conResultado = 0;
		for (const DueloGrabado& d : grabados.duelos) conResultado += d.ganador >= 0;
		if (conResultado < MIN_DUELOS_AJUSTE) return false;
		// validacion: 1 de cada 5 duelos (con pocos duelos, todos: no hay de donde apartar)
		const int cada = conResultado >= 10 ? 5 : 0;
		// solo lo visto desde el RIVAL (lado 1, el que evalua la busqueda): lo que importa es cuanto vale cada posicion
		// para la IA contra ESTE jugador (con las dos miradas, una red simetrica se anularia: el jugador fuerte sube un
		// lado y baja el otro)
		grabados.conjunto(aj.X, aj.y, cada, 0, true, 1);
		if (cada) grabados.conjunto(aj.Xv, aj.yv, cada, 0, false, 1);
		else aj.Xv = aj.X, aj.yv = aj.y;
		if (aj.y.empty() || aj.yv.empty()) return false;
		aj.r = base;
		aj.opt = ia::Entrenador{};
		aj.opt.lr = 2e-3f;
		aj.opt.ancla = 5e-3f;
		aj.opt.soloSalida = true;   // (solo la ultima capa: 33 pesos para unos pocos duelos; las capas de abajo son las del autojuego)
		aj.orden.resize(aj.y.size());
		for (size_t i = 0; i < aj.orden.size(); i++) aj.orden[i] = (int)i;
		aj.cursor = 0;
		aj.epoca = 0;
		barajar();
		aj.enCurso = true;
		return true;
	}
	void terminarAjuste()
	{
		aj.enCurso = aj.evaluando = false;
		st.ajustes++;
		const double nv = (double)std::max<size_t>(1, aj.yv.size());
		st.perdidaBase = (float)(aj.sumaBase / nv);
		st.perdidaNueva = (float)(aj.sumaNueva / nv);
		// (se queda solo si mejora de verdad sobre los duelos apartados: con ruido, la base)
		const bool vale = aj.r.finita() && st.perdidaNueva <= st.perdidaBase - 0.001f;
		if (vale) {
			jugador = aj.r;
			jugador.origen = "jugador";
			hayJugador = true;
			st.aceptados++;
		} else {
			hayJugador = false;
			st.rechazados++;
		}
		aj.guardarPendiente = true;
		fprintf(stderr, "[ia-red] ajuste fino con %d posiciones: perdida %.4f -> %.4f (%s)\n", (int)aj.y.size(), st.perdidaBase, st.perdidaNueva,
		        vale ? "se queda la del jugador" : "no mejora: se usa la base");
		aj.X.clear(), aj.y.clear(), aj.Xv.clear(), aj.yv.clear(), aj.orden.clear();
		aj.X.shrink_to_fit(), aj.Xv.shrink_to_fit();
	}
};

}   // namespace aprendizaje
