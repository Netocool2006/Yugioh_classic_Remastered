/*
 * Yugioh Classic Remaster (rama classic-md-ritmo): EL RITMO DE MASTER DUEL EN LA MESA. Solo logica (sin Vulkan).
 *
 * Pedido del usuario (PS5): "las cartas del contrincante solo aparecen, no hay un movimiento de su mazo hacia colocar
 * la carta y debe ser un ritmo humano"; "se activo Swords of Revealing Light y ni me entere"; "que las cartas activas
 * brillen fuerte"; "el contador de los 3 turnos no existe". Estudio de Master Duel: docs/md_ritmo.md.
 *
 *  - Viaje: una carta que va de su origen (mano, mazo, cementerio, desterradas, extra) a su zona de la mesa (llegada)
 *    o de su zona a su destino (salida). La del rival que sale de su mano primero se "elige": se levanta un poco en su
 *    mano (T_ELIGE) y luego vuela. Nada aparece de la nada ni se va sin moverse.
 *  - Activacion: la presentacion de Master Duel de toda activacion (magia, trampa, efecto de monstruo; de los dos
 *    lados): la carta se levanta en su zona, se acerca a la camara grande con su arte y un halo, destella y vuelve; el
 *    numero de eslabon (cadena de 2 o mas) queda sobre la carta hasta que resuelve.
 *  - Todos los tiempos son de la velocidad Normal: el juego los avanza con el dt del duelo, que ya va escalado por la
 *    velocidad de animaciones elegida y por R2 (velocidad.h). Lo lee la interfaz (md_ritmo_ui.h); lo maneja el juego
 *    (md_ritmo_juego.h). Es solo presentacion: no toca el duelo (determinismo del modo en linea intacto).
 */
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "control.h"
#include "clips_md.h"   // rama classic-md-fases: clipsmd::CurvaUnity (AnimationCurve de Unity, la evalua como Unity)
#include "ritmo_turno.h"
#include "cpu_muestra.h"   // rama classic-ps5-cpu-joey: el rival ensena la carta antes de jugarla (Joey the Passion)   // rama classic-ps5-ritmo: el ritmo del turno (ritmo_turno_juego.h)

namespace ritmo {

// ---- tiempos (s, velocidad Normal), extraidos de Master Duel (masterduel_Data/data.unity3d: tablas de movimiento de
// cartas, tramos Bezier con su easing; md_extraido/NOTAS.md y docs/md_ritmo.md)
constexpr float T_ELIGE = 0.38f;         // el rival levanta la carta elegida (MD: la CPU se toma ~0.5-1 s entre acciones)
constexpr float T_MANO_ZONA = 0.50f;     // MD: mano -> zona 0.38 (arco, frena sobre la zona) + 0.12 (cae acelerando)
constexpr float T_PODER_2 = 0.17f;       // MD: nivel 5-6 (Hand2MonsterPower1: 0.55 + 0.12 = 0.67)
constexpr float T_PODER_3 = 0.38f;       // MD: nivel 7+ (Hand2MonsterPower2: 0.76 + 0.12 = 0.88)
constexpr float T_PILA_ZONA = 0.60f;     // MD: del cementerio / desterradas: destello 0.583 y el mismo movimiento (aqui juntos)
constexpr float T_ATERRIZA = 0.22f;      // la carta asentada antes del siguiente suceso (MD: sacudida de las vecinas 0.21)
constexpr float T_SALIDA = 0.42f;        // MD: al cementerio / desterrada 0.42 con arco alto; a la mano / mazo 0.45
constexpr float T_ACTIVA = 1.10f;        // MD: levantar 0.15 + mostrar 0.70 + volver 0.25
constexpr float T_ACT_DESTELLO = 0.78f;  // fraccion de T_ACTIVA: termina de mostrarse y se aplica el efecto (destello)
constexpr float T_ESLABON_EXTRA = 0.55f; // MD: con 2+ eslabones el apilado dura 2.083 en vez de 1.417 (el numero entra)
constexpr float T_RESUELVE = 0.60f;      // MD: resolucion de un eslabon 0.917 (luz a 0.383): espera con cadena de 2+
constexpr float T_VOLTEO = 0.45f;        // MD: volteo 0.45 (salta)
// rama classic-md-combate: el combate de Master Duel (AttackMotionSetting + BezierMotion Strike*; EffectTaskBattleRun;
// CardRoot.ReqBrokenEffect). El reloj del ataque (tAtaque) es el del viaje "Low": impacto a 0.45. El viaje "High"
// (Util.GetAttackLevel: ATK >= 2500, Largest) dura 0.65: hasta el impacto el reloj avanza 0.45/0.65 mas lento y despues
// todo (pausa, rotura, vuelta, fin) se corre 0.20 s
constexpr float T_ATAQUE_BAJO = 0.45f;   // MD: Strike*00/01 (Low) 0.45 s
constexpr float T_ATAQUE_ALTO = 0.65f;   // MD: Strike*02/03 (High) 0.65 s
// rama classic-ps5-ataque3s: "lo quiero como Joey, el ataque a 3 segundos". Power of Chaos: Joey the Passion reparte su
// ataque (FUN_00414110, el efecto de duel/effect/attack.txt; 30 cuadros/s) en: apunta 8 cuadros (0.27 s), viaje 8 (0.27),
// golpe 16 (0.53: atk_eff crece y se apaga), danno 32 (1.07: dmg_ref) y LP y destruidas 32 (1.07) = 96 cuadros, 3.2 s.
// Aqui: el viaje de MD (0.45 / 0.65, golpe = bajada de LP), la pausa en el objetivo = el golpe de Joey (0.50), la vuelta
// (0.45) y la cuenta de LP (1.0, DuelLP de MD) dentro del "danno" de Joey, y el cierre hasta T_FIN_ATAQUE: 2.95 s ("Low"),
// 3.15 s ("High"); las velocidades Rapida / Muy rapida / R2 lo acortan en proporcion (escalaAnim). Antes: pausa 0.25 de MD
// (attackExecPostTime) y fin a 1.30 s
constexpr float T_POST_GOLPE = 0.50f;    // la carta se queda en el objetivo mientras el golpe (Joey: 16 cuadros; MD: 0.25)
constexpr float T_VUELTA = 0.45f;        // MD: vuelve a su zona (Field2Field, 0.45)
constexpr float T_FIN_ATAQUE = 2.95f;    // fin del ataque en el reloj "Low" (Joey: 96 cuadros = 3.2 s; antes 1.30)
constexpr float VEL_FIG_PREVIA = 0.60f;  // con figura 3D que ataca cuerpo a cuerpo: hasta el golpe el reloj va a 0.6 (0.75 s)
// rama classic-ps5-ataque4 (el usuario en la PS5: "primero hace el calculo de danno, luego ataca, debe ser al reves" y "el
// ataque es super corto, como de 1 segundo"): la figura que ataca a distancia (aliento, magia, rayo) hace el ataque COMPLETO
// de Yugi Reload (carga 1.2 s y efecto hasta que se apaga, FIN_EFECTO) y recien entonces llega el golpe: calculo, LP y
// destruidas; despues el cierre va un poco mas rapido. Total ~4 s (Joey: 3.2)
constexpr float T_FIG_DISTANCIA = 2.45f; // s reales hasta el golpe con figura que ataca a distancia
constexpr float VEL_FIG_POST = 1.5f;     // despues del golpe (figura a distancia): 2.5 s del reloj en 1.67 s
// rama classic-ps5-ataque4 (el usuario: "mas tarde, como Joey"): los LP bajan como en Joey (FUN_00414110 paso 7, a los 64
// cuadros = 2.1 s), con la carta ya de vuelta: golpe y humo -> calculo sobre las cartas -> bajada de LP -> destruccion.
// El marco rojo y la sacudida del directo van con la bajada de LP. Antes los LP bajaban en el golpe (0.45, MD)
constexpr float T_LP = 1.95f;            // reloj "Low": 1.95 s reales (High 2.15); con figura a distancia, despues del efecto
constexpr float T_ROMPE = T_LP + 0.20f;  // la destruida se rompe (ReqBrokenEffect) y sale al cementerio, despues de los LP
constexpr float T_ROMPE_FX = 1.35f;      // MD: fxp_cardbrk_btl_001 (dur 1.0 + vida de sus particulas; horneado 1.35 s)
constexpr int ATK_ALTO = 2500;           // MD: Util.GetAttackLevel >= 2500 = Largest -> etiqueta "High"
inline bool ataqueAlto(int atk) { return atk >= ATK_ALTO; }
// el paso del reloj del ataque (en el reloj "Low") para un dt real
inline float pasoRelojAtaque(float tAtaque, float dt, bool alto)
{
	if (!alto || tAtaque >= T_ATAQUE_BAJO) return dt;
	const float lento = dt * T_ATAQUE_BAJO / T_ATAQUE_ALTO;   // (hasta el impacto)
	if (tAtaque + lento <= T_ATAQUE_BAJO) return lento;
	const float realHasta = (T_ATAQUE_BAJO - tAtaque) * T_ATAQUE_ALTO / T_ATAQUE_BAJO;   // (cruza el impacto en este cuadro)
	return (T_ATAQUE_BAJO - tAtaque) + (dt - realHasta);
}
// cuanto tiempo real pasa hasta ese punto del reloj del ataque (pruebas y registro)
inline float realAtaque(float tAtaque, bool alto)
{
	if (!alto) return tAtaque;
	return tAtaque <= T_ATAQUE_BAJO ? tAtaque * T_ATAQUE_ALTO / T_ATAQUE_BAJO : tAtaque + (T_ATAQUE_ALTO - T_ATAQUE_BAJO);
}

enum Origen { O_MANO = 0, O_MAZO, O_CEMENTERIO, O_DESTERRADO, O_EXTRA, O_ZONA };
static const char* const NOMBRE_ORIGEN[] = { "mano", "mazo", "cementerio", "desterradas", "extra", "zona" };

// ---- los movimientos de cartas de Master Duel, TAL CUAL (BezierMotionSetting de masterduel_Data/data.unity3d,
// exportados por herramientas/md_ritmo/exportar_movimientos.py a <assets>/md_ritmo/movimientos.txt, fuera del repo).
// Cada uno: un Bezier de 3 puntos (inicio, via, fin) en animationTime s; cada punto = su base (0: donde esta la carta,
// 1: su destino) + lerp * (destino - inicio) + add (unidades de MD; y = alto). La posicion avanza por la
// accelerationCurve (tiempo -> fraccion del Bezier) y el giro por la rotationAccelerationCurve
struct Movimiento {
	std::string nombre;
	float tiempo{ 0.0f };
	int base[3]{ 0, 1, 1 };
	float lerp[3]{};
	float add[3][3]{};
	clipsmd::CurvaUnity acel, giro;
};
// MD: alto de la carta en sus unidades (CameraViewSetting "Top": camara a 95 de alto y 37 atras, 70 grados, FOV 30; la
// carta ocupa ~1/7 del alto de la vista: ~8 unidades). Estimado: convierte las alturas de MD a las de la mesa
constexpr float MD_ALTO_CARTA = 8.0f;

inline std::map<std::string, Movimiento>& movimientos()
{
	static std::map<std::string, Movimiento> m;
	return m;
}
inline bool leerMovimientos(const std::string& ruta)
{
	std::ifstream f(ruta);
	std::string l;
	int n = 0;
	while (std::getline(f, l)) {
		if (l.empty() || l[0] == '#') continue;
		std::vector<std::string> c;
		size_t a = 0;
		for (size_t b; (b = l.find('|', a)) != std::string::npos; a = b + 1) c.push_back(l.substr(a, b - a));
		c.push_back(l.substr(a));
		if (c.size() < 13) continue;
		Movimiento m;
		m.nombre = c[0];
		m.tiempo = (float)atof(c[1].c_str());
		for (int k = 0; k < 3; k++) m.base[k] = atoi(c[(size_t)(2 + k)].c_str());
		// (orden del archivo: via, fin, inicio)
		const int orden[3] = { 1, 2, 0 };
		for (int k = 0; k < 3; k++) {
			const int q = orden[k];
			m.lerp[q] = (float)atof(c[(size_t)(5 + 2 * k)].c_str());
			sscanf(c[(size_t)(6 + 2 * k)].c_str(), "%f,%f,%f", &m.add[q][0], &m.add[q][1], &m.add[q][2]);
		}
		m.acel.desdeJSON(c[11]);
		m.giro.desdeJSON(c[12]);
		movimientos()[m.nombre] = m;
		n++;
	}
	fprintf(stderr, "[ritmo] %d movimientos de cartas de Master Duel (%s)\n", n, ruta.c_str());
	return n > 0;
}
inline const Movimiento* movimiento(const std::string& n)
{
	auto it = movimientos().find(n);
	return it == movimientos().end() ? nullptr : &it->second;
}
// la cadena de movimientos de MD de cada traslado (CardMoveMotionSetting.infoList: origen -> destino, tipo y "poder").
// poder: 0-1 normal, 2, 3 (MD lo saca de la carta; aca por nivel: 5-6 -> 2, 7+ -> 3, estimado)
inline std::vector<std::string> cadenaMovimientos(Origen o, bool llegada, bool magia, int poder)
{
	const char* pw = poder >= 3 ? "Power2_" : poder == 2 ? "Power1_" : "Power0_";
	if (llegada) {
		if (o == O_MANO) {
			if (magia) return { "CardMoveHand2MagicZone1", "CardMoveHand2MagicZone2" };
			return { std::string("CardMoveHand2MonstorZone") + pw + "1", std::string("CardMoveHand2MonstorZone") + pw + "2" };
		}
		if (magia) return { "CardMoveDeck2MagicZone1", "CardMoveDeck2MagicZone2" };   // (mazo, extra, cementerio, desterradas)
		return { std::string("CardMoveDeck2MonstorZone") + pw + "1", std::string("CardMoveDeck2MonstorZone") + pw + "2" };
	}
	if (o == O_CEMENTERIO || o == O_DESTERRADO) return { "CardMoveField2GraveNormal" };
	if (o == O_MANO) return { "CardMoveField2Hand" };
	return { "CardMoveField2Deck" };   // (al mazo / al extra)
}
inline float duracionMovimientos(const std::vector<std::string>& c, float porDefecto)
{
	float t = 0.0f;
	for (const std::string& n : c) {
		const Movimiento* m = movimiento(n);
		if (!m) return porDefecto;
		t += m->tiempo;
	}
	return t > 0.0f ? t : porDefecto;
}
// la activacion en el campo (CardShow: Move -> Happen -> Back; MD: CardShowMoveFieldNear, CardShowHappen,
// CardShowBackFieldNear) y desde la mano (CardShowMoveHandNear)
inline std::vector<std::string> cadenaActivacion() { return { "CardShowMoveFieldNear", "CardShowHappen", "CardShowBackFieldNear" }; }

struct Viaje {
	bool llegada{ true };     // true: del origen a la zona; false: de la zona al destino (origen = adonde va)
	int lado{ 0 };            // el lado de la zona de la mesa
	int ladoPila{ 0 };        // el lado del origen/destino (Monster Reborn: el cementerio del otro)
	Origen origen{ O_MANO };
	int zona{ -1 };           // zona de la mesa (control::ZONAS_MESA)
	std::string carta;        // id ("" o "?" = dorso)
	bool bocaAbajo{ false }, defensa{ false };   // como queda en la zona (llegada) o como estaba (salida)
	bool caraEnVuelo{ false };// la cara se ve en el aire (si no, el dorso)
	int manoIdx{ -1 }, manoN{ 0 };   // la carta de la mano del rival (para levantarla) y cuantas tenia
	float t{ 0.0f }, tElige{ 0.0f }, dur{ T_MANO_ZONA };
	bool eligeTerminada{ false };
	int serie{ 0 };
	int sucesoTipo{ -1 };     // (llegada) el suceso que la trajo: al aterrizar, su destello / entrada / sonido
	bool magia{ false };      // zona de magia/trampa (o de campo)
	bool soloPila{ false };   // (rama classic-arreglo-ciclo-md) de pila a pila (mano, mazo, cementerio, desterradas): la
	                          // zona es la pila de un extremo y no se escribe nada en ella al llegar
	std::vector<std::string> movs;   // la cadena de movimientos de Master Duel (cadenaMovimientos)
	float u() const { return t <= tElige ? 0.0f : std::min(1.0f, (t - tElige) / std::max(dur, 1e-3f)); }
	bool eligiendo() const { return t < tElige; }
	bool termino() const { return t >= tElige + dur; }
	float resta() const { return std::max(0.0f, tElige + dur - t); }
};

struct Activacion {
	std::string carta;
	int lado{ 0 }, zona{ -1 }, eslabon{ 0 };
	bool monstruo{ false };   // efecto de monstruo (S_ACTIVA_EFECTO)
	bool corte{ false };      // sale el cartel del rival (dibujarCorteMando): sin el primer plano repetido
	float retraso{ 0.0f };    // espera a que la carta llegue / se de vuelta
	float t{ 0.0f };
	bool destello{ false };   // ya destello (y arranco el efecto de la carta)
	int serie{ 0 };
	float u() const { return std::max(0.0f, t - retraso) / T_ACTIVA; }
	bool termino() const { return t >= retraso + T_ACTIVA; }
};

// el numero de eslabon que queda sobre la carta hasta que resuelve
struct Marca { int zona{ -1 }, n{ 0 }; std::string carta; };

struct Estado {
	std::vector<Viaje> viajes;
	std::vector<Activacion> acts;
	std::vector<Marca> cadena;
	int serie{ 0 };
	int zonaResuelve{ -1 };     // el eslabon que acaba de resolver: destello en su zona
	float tResuelve{ 99.0f };
	// Master Duel muestra en el visor de la izquierda la carta que se activa (y la que el rival juega boca arriba)
	std::string visorCarta;
	float tVisor{ 99.0f };
	void mostrar(const std::string& id)
	{
		visorCarta = id;
		tVisor = 0.0f;
	}

	void limpiar()
	{
		viajes.clear();
		acts.clear();
		cadena.clear();
		zonaResuelve = -1;
		tResuelve = 99.0f;
		visorCarta.clear();
		tVisor = 99.0f;
	}
	bool ocupado() const { return !viajes.empty() || !acts.empty(); }
	// lo que falta (s) de lo que frena el duelo: viajes y activaciones
	float resta() const
	{
		float r = 0.0f;
		for (const Viaje& v : viajes) r = std::max(r, v.resta() + (v.llegada ? T_ATERRIZA : 0.0f));
		for (const Activacion& a : acts) r = std::max(r, a.retraso + T_ACTIVA - a.t);
		return r;
	}
	// la llegada en vuelo a esa zona (o null)
	const Viaje* llegandoA(int zona) const
	{
		for (const Viaje& v : viajes)
			if (v.llegada && !v.soloPila && v.zona == zona) return &v;
		return nullptr;
	}
	const Activacion* activandoEn(int zona) const
	{
		for (const Activacion& a : acts)
			if (a.zona == zona) return &a;
		return nullptr;
	}
	// la carta de la mano del rival que se esta eligiendo (indice) y cuanto se levanta (0..1); -1 si ninguna
	int manoElegida(float& alza) const
	{
		for (const Viaje& v : viajes) {
			if (!v.llegada || v.origen != O_MANO || v.ladoPila != 1 || !v.eligiendo() || v.manoIdx < 0) continue;
			const float x = v.t / std::max(v.tElige, 1e-3f);
			alza = std::min(1.0f, x * 2.2f);
			return v.manoIdx;
		}
		alza = 0.0f;
		return -1;
	}
	int maxEslabon() const
	{
		int n = 0;
		for (const Marca& m : cadena) n = std::max(n, m.n);
		return n;
	}
	void quitarEslabonMayor()
	{
		if (cadena.empty()) return;
		auto it = std::max_element(cadena.begin(), cadena.end(), [](const Marca& a, const Marca& b) { return a.n < b.n; });
		cadena.erase(it);
	}
};

inline Estado& estado()
{
	static Estado e;
	return e;
}

}   // namespace ritmo
