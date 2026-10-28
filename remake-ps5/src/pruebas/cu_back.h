// Casos de uso del BACK (rama classic-ciclos): las reglas del duelo en el motor (ia::Duelo) sin pantalla: inicio,
// robo, fases, invocaciones (normal, colocar, sacrificios, Dioses, volteo, posicion, especial, fusion, ritual, union,
// espiritu, toon), limites de mano y de zonas, cadena y velocidades, ventanas de respuesta, batalla y danno,
// repeticion del ataque y condiciones de victoria; la validacion de mazos, el progreso y la memoria del rival.
#pragma once

#include "casos_uso.h"
#include "ia.h"
#include "mazo.h"

namespace cub {

using namespace ia;

// un duelo a medio jugar (como Escena de prueba_efectos.cpp): turno 3 del lado 0, mazos de relleno
struct Mesa {
	Duelo d;
	explicit Mesa(const Base* b, Fase f = F_PRINCIPAL1) : d(b)
	{
		d.l = {};
		d.ef = EstadoEfectos{};
		d.turno = 3;
		d.activo = 0;
		d.fase = f;
		d.ganador = -1;
		for (Lado& x : d.l)
			for (int k = 0; k < 20; k++) x.mazo.push_back(k % 2 ? "celtic_guardian" : "battle_ox");
	}
	int mon(int lado, const std::string& id, bool defensa = false, bool bocaAbajo = false)
	{
		const int z = d.invocarEspecial(lado, id, defensa, -1, bocaAbajo);
		if (z < 0) return z;
		d.l[(size_t)lado].m[(size_t)z].llegoEsteTurno = false;
		d.l[(size_t)lado].m[(size_t)z].especial = false;
		d.ef.disparos.clear();
		d.ef.ventanaTras = Ventana{};
		d.recalcular();
		return z;
	}
	int colocar(int lado, const std::string& id, int turnoColocada = 1)
	{
		Lado& x = d.l[(size_t)lado];
		const int z = x.libreMT();
		if (z < 0) return z;
		x.mt[(size_t)z] = MagiaTrampa{ id, true };
		x.mt[(size_t)z].uid = d.nuevoUid();
		x.mt[(size_t)z].turnoColocada = turnoColocada;
		return z;
	}
	int mano(int lado, const std::string& id)
	{
		d.l[(size_t)lado].mano.push_back(id);
		return (int)d.l[(size_t)lado].mano.size() - 1;
	}
	bool activar(int lado, const Lugar& f, std::vector<Lugar> objs = {})
	{
		Jugada j = jugadaActivar(f);
		j.objs = objs;
		const DefCarta* df = defCarta(cartaEn(d, f));
		j.costes = df ? costesIA(d, lado, *df, f) : std::vector<Lugar>{};
		return d.aplicar(j);
	}
	void pasarTodo1()   // (una sola pregunta: pasar o elegir la primera)
	{
		Jugada j;
		j.tipo = d.ef.preg.tipo == P_ELEGIR ? J_ELEGIR : J_PASAR;
		j.objetivo = 0;
		d.aplicar(j);
	}
	void pasarTodo()
	{
		for (int g = 0; g < 30 && d.ef.preg.tipo != P_NADA; g++) {
			Jugada j;
			j.tipo = d.ef.preg.tipo == P_ELEGIR ? J_ELEGIR : J_PASAR;
			j.objetivo = 0;
			d.aplicar(j);
		}
	}
	bool jugar(TipoJugada t, int mano = -1, int zona = -1, std::vector<int> tributos = {}, int objetivo = -1)
	{
		Jugada j;
		j.tipo = t;
		j.mano = mano;
		j.zona = zona;
		j.tributos = tributos;
		j.objetivo = objetivo;
		const bool ok = d.aplicar(j);
		return ok;
	}
	bool fase()
	{
		const bool ok = jugar(J_FASE);
		pasarTodo();
		return ok;
	}
	// hasta la principal 1 del lado "lado" (pasando fases y turnos; descarta si hace falta)
	void hastaPrincipal(int lado)
	{
		for (int k = 0; k < 40 && !(d.activo == lado && d.fase == F_PRINCIPAL1 && !d.ocupado()) && d.ganador < 0; k++) {
			pasarTodo();
			if (d.activo == lado && d.fase == F_PRINCIPAL1 && !d.ocupado()) break;
			if (d.fase == F_FINAL && (int)d.l[(size_t)d.activo].mano.size() > MAX_MANO) {
				jugar(J_DESCARTAR, 0);
				continue;
			}
			fase();
		}
	}
	int enCementerio(int lado, const std::string& id) const
	{
		int n = 0;
		for (const std::string& s : d.l[(size_t)lado].cementerio) n += s == id;
		return n;
	}
	int zonaDe(int lado, const std::string& id) const
	{
		for (int i = 0; i < ZONAS; i++)
			if (d.l[(size_t)lado].m[(size_t)i].id == id) return i;
		return -1;
	}
	bool enMano(int lado, const std::string& id) const
	{
		const auto& m = d.l[(size_t)lado].mano;
		return std::find(m.begin(), m.end(), id) != m.end();
	}
};
// pasa todas las preguntas de responder (las de elegir quedan)
inline void pasarRival(Mesa& m)
{
	for (int g = 0; g < 30 && m.d.ef.preg.tipo == P_RESPUESTA; g++) {
		Jugada j;
		j.tipo = J_PASAR;
		m.d.aplicar(j);
	}
}
inline Lugar M(int lado, int z) { return { lado, EN_MONSTRUO, z }; }
inline Lugar MT(int lado, int z) { return { lado, EN_MT, z }; }
inline Lugar H(int lado, int k) { return { lado, EN_MANO, k }; }

inline std::vector<std::string> relleno(const char* id, int n)
{
	return std::vector<std::string>((size_t)n, id);
}

// ------------------------------------------------------------------------------------------------- reglas
inline void reglas(const Base& base)
{
	// inicio: 8000 LP, 5 cartas, el extra son las fusiones, el que empieza roba y va a la principal 1
	{
		Duelo d(&base);
		d.empezar(relleno("celtic_guardian", 40), { "flame_swordsman" }, relleno("battle_ox", 40), {}, 7u, 0);
		cu::caso("CU-BE-INICIO", d.l[0].lp == LP_INICIAL && d.l[1].lp == LP_INICIAL, "LP iniciales %d / %d", d.l[0].lp, d.l[1].lp);
		cu::caso("CU-BE-INICIO", d.activo == 0 && d.turno == 1 && d.fase == F_PRINCIPAL1, "empieza %d en el turno %d fase %d", d.activo, d.turno, (int)d.fase);
		cu::caso("CU-BE-INICIO", d.l[1].mano.size() == 5 && d.l[0].extra.size() == 1 && d.l[0].mazo.size() + d.l[0].mano.size() == 40, "manos/extra/mazo al empezar");
		cu::caso("CU-BE-ROBO-PRIMER-TURNO", d.l[0].mano.size() == 6, "el que empieza tiene %d cartas (roba en su primer turno)", (int)d.l[0].mano.size());
		int empiezaJ = 0, empiezaR = 0;
		for (uint32_t s = 1; s <= 40; s++) {
			Duelo x(&base);
			x.empezar(relleno("celtic_guardian", 40), {}, relleno("battle_ox", 40), {}, s * 7919u);
			(x.activo == 0 ? empiezaJ : empiezaR)++;
		}
		cu::caso("CU-BE-INICIO-QUIEN", empiezaJ > 5 && empiezaR > 5, "sin sorteo empieza siempre el mismo (%d / %d)", empiezaJ, empiezaR);
		// sin batalla en el primer turno: principal 1 -> principal 2
		Mesa m(&base);
		m.d = d;
		m.jugar(J_FASE);
		cu::caso("CU-BE-FASES-PRIMER-TURNO", m.d.fase == F_PRINCIPAL2, "en el primer turno la principal 1 fue a la fase %d (sin batalla)", (int)m.d.fase);
		cu::caso("CU-BE-FASES-PRIMER-TURNO", !m.d.puedeAtacar(0, 0), "en el primer turno se puede atacar");
	}
	// fases: principal 1 -> batalla -> principal 2 -> final -> turno del rival (roba 1, espera, principal 1). La fase
	// final pasa sola al turno siguiente si no hay nada que hacer en ella (descartar, responder)
	{
		Mesa m(&base);
		m.mon(0, "celtic_guardian");
		const size_t manoR = m.d.l[1].mano.size(), mazoR = m.d.l[1].mazo.size();
		m.d.sucesos.clear();
		m.fase();
		const Fase f1 = m.d.fase;
		m.fase();
		const Fase f2 = m.d.fase;
		m.fase();
		std::vector<int> fases;
		for (const Suceso& s : m.d.sucesos)
			if (s.tipo == S_FASE) fases.push_back(s.valor);
		const std::vector<int> esperadas = { F_BATALLA, F_PRINCIPAL2, F_FINAL, F_PRINCIPAL1 };
		bool orden = fases.size() >= esperadas.size();
		for (size_t k = 0; orden && k < esperadas.size(); k++) orden = fases[k] == esperadas[k];
		std::string vistas;
		for (int f : fases) vistas += std::to_string(f) + " ";
		cu::caso("CU-BE-FASES-ORDEN", f1 == F_BATALLA && f2 == F_PRINCIPAL2 && orden, "orden de fases %d %d; sucesos de fase: %s", (int)f1, (int)f2, vistas.c_str());
		cu::caso("CU-BE-FASES-PASA-TURNO", m.d.activo == 1 && m.d.turno == 4, "la fase final no paso el turno (activo %d, turno %d)", m.d.activo, m.d.turno);
		cu::caso("CU-BE-ROBO", m.d.l[1].mano.size() == manoR + 1 && m.d.l[1].mazo.size() == mazoR - 1, "el rival no robo 1 al empezar su turno");
		bool suceso = false;
		for (const Suceso& s : m.d.sucesos) suceso |= s.tipo == S_ROBO && s.lado == 1;
		cu::caso("CU-BE-ROBO", suceso, "el robo no dejo su suceso");
		cu::caso("CU-BE-FASES-ESPERA", m.d.fase == F_PRINCIPAL1, "tras el robo y la espera no llego a la principal 1 (fase %d)", (int)m.d.fase);
	}
	// sin cartas para robar: pierde
	{
		Mesa m(&base);
		m.d.l[1].mazo.clear();
		for (int k = 0; k < 4; k++) m.fase();
		cu::caso("CU-BE-VICTORIA-SIN-MAZO", m.d.ganador == 0, "el rival sin mazo no perdio al robar (ganador %d)", m.d.ganador);
	}
	// invocacion normal: 1 por turno
	{
		Mesa m(&base);
		const int a = m.mano(0, "celtic_guardian");
		cu::caso("CU-BE-INVOCAR-NORMAL", m.jugar(J_INVOCAR, a), "no se invoco Celtic Guardian (nivel 4)");
		m.pasarTodo();
		const int z = m.zonaDe(0, "celtic_guardian");
		cu::caso("CU-BE-INVOCAR-NORMAL", z >= 0 && !m.d.l[0].m[(size_t)z].bocaAbajo && !m.d.l[0].m[(size_t)z].defensa, "Celtic Guardian no quedo boca arriba en ataque");
		const int b = m.mano(0, "battle_ox");
		cu::caso("CU-BE-INVOCAR-UNA-POR-TURNO", !m.jugar(J_INVOCAR, b) && !m.jugar(J_COLOCAR, b), "se invoco dos veces en el mismo turno");
		m.d.fase = F_BATALLA;
		cu::caso("CU-BE-INVOCAR-FASE", !m.jugar(J_INVOCAR, b), "se invoco en la fase de batalla");
	}
	// colocar: boca abajo en defensa
	{
		Mesa m(&base);
		const int a = m.mano(0, "battle_ox");
		cu::caso("CU-BE-COLOCAR", m.jugar(J_COLOCAR, a), "no se coloco Battle Ox");
		m.pasarTodo();
		const int z = m.zonaDe(0, "battle_ox");
		cu::caso("CU-BE-COLOCAR", z >= 0 && m.d.l[0].m[(size_t)z].bocaAbajo && m.d.l[0].m[(size_t)z].defensa, "Battle Ox no quedo boca abajo en defensa");
		// volteo: no el mismo turno; el siguiente si
		cu::caso("CU-BE-VOLTEO", !m.jugar(J_VOLTEAR, -1, z), "se volteo el mismo turno en que se coloco");
		m.d.l[0].m[(size_t)z].llegoEsteTurno = false;
		m.d.l[0].m[(size_t)z].cambioPosicion = false;
		cu::caso("CU-BE-VOLTEO", m.jugar(J_VOLTEAR, -1, z), "no se volteo al turno siguiente");
		m.pasarTodo();
		cu::caso("CU-BE-VOLTEO", !m.d.l[0].m[(size_t)z].bocaAbajo && !m.d.l[0].m[(size_t)z].defensa, "el volteo no lo dejo boca arriba en ataque");
		cu::caso("CU-BE-POSICION", !m.jugar(J_POSICION, -1, z), "cambio de posicion el turno en que se volteo");
	}
	// cambio de posicion: no el turno en que llega; una vez por turno
	{
		Mesa m(&base);
		const int z = m.mon(0, "celtic_guardian");
		cu::caso("CU-BE-POSICION", m.jugar(J_POSICION, -1, z) && m.d.l[0].m[(size_t)z].defensa, "no paso a defensa");
		cu::caso("CU-BE-POSICION", !m.jugar(J_POSICION, -1, z), "cambio de posicion dos veces en el turno");
		const int a = m.mano(0, "battle_ox");
		m.jugar(J_INVOCAR, a);
		m.pasarTodo();
		cu::caso("CU-BE-POSICION", !m.jugar(J_POSICION, -1, m.zonaDe(0, "battle_ox")), "cambio de posicion el turno en que se invoco");
	}
	// sacrificios: nivel 5-6 uno, 7+ dos; los sacrificados van al cementerio
	{
		Mesa m(&base);
		const int z0 = m.mon(0, "celtic_guardian");
		const int a = m.mano(0, "summoned_skull");   // nivel 6
		cu::caso("CU-BE-TRIBUTO-1", !m.jugar(J_INVOCAR, a), "Summoned Skull (nivel 6) se invoco sin sacrificio");
		cu::caso("CU-BE-TRIBUTO-1", m.jugar(J_INVOCAR, a, -1, { z0 }), "Summoned Skull no se invoco con 1 sacrificio");
		m.pasarTodo();
		cu::caso("CU-BE-TRIBUTO-1", m.zonaDe(0, "summoned_skull") >= 0 && m.enCementerio(0, "celtic_guardian") == 1, "el sacrificio no fue al cementerio");
	}
	{
		Mesa m(&base);
		const int z0 = m.mon(0, "celtic_guardian"), z1 = m.mon(0, "battle_ox");
		const int a = m.mano(0, "blue_eyes");   // nivel 8
		cu::caso("CU-BE-TRIBUTO-2", !m.jugar(J_INVOCAR, a, -1, { z0 }), "Blue-Eyes se invoco con 1 sacrificio");
		cu::caso("CU-BE-TRIBUTO-2", m.jugar(J_INVOCAR, a, -1, { z0, z1 }), "Blue-Eyes no se invoco con 2 sacrificios");
		m.pasarTodo();
		cu::caso("CU-BE-TRIBUTO-2", m.d.l[0].monstruos() == 1 && m.d.l[0].cementerio.size() == 2, "Blue-Eyes: %d monstruos y %d en el cementerio", m.d.l[0].monstruos(),
		         (int)m.d.l[0].cementerio.size());
		Mesa c(&base);
		const int y0 = c.mon(0, "celtic_guardian"), y1 = c.mon(0, "battle_ox");
		const int b = c.mano(0, "blue_eyes");
		cu::caso("CU-BE-TRIBUTO-COLOCAR", c.jugar(J_COLOCAR, b, -1, { y0, y1 }), "no se coloco Blue-Eyes con 2 sacrificios");
		Mesa e(&base);
		const int w0 = e.mon(0, "celtic_guardian");
		const int k = e.mano(0, "celtic_guardian");
		cu::caso("CU-BE-TRIBUTO-DE-SOBRA", !e.jugar(J_INVOCAR, k, -1, { w0 }), "un nivel 4 se invoco sacrificando");
	}
	// los Dioses: 3 sacrificios y no se colocan
	for (const char* dios : { "slifer_the_sky_dragon", "obelisk_the_tormentor", "the_winged_dragon_of_ra" }) {
		if (!base.carta(dios)) continue;
		Mesa m(&base);
		const int a = m.mon(0, "celtic_guardian"), b = m.mon(0, "battle_ox"), c = m.mon(0, "kuriboh");
		const int k = m.mano(0, dios);
		cu::caso("CU-BE-TRIBUTO-DIOS", !m.jugar(J_INVOCAR, k, -1, { a, b }), "%s se invoco con 2 sacrificios", dios);
		cu::caso("CU-BE-TRIBUTO-DIOS", !m.jugar(J_COLOCAR, k, -1, { a, b, c }), "%s se pudo colocar", dios);
		cu::caso("CU-BE-TRIBUTO-DIOS", m.jugar(J_INVOCAR, k, -1, { a, b, c }), "%s no se invoco con 3 sacrificios", dios);
	}
	// invocacion especial (Monster Reborn) desde el cementerio
	{
		Mesa m(&base);
		m.d.l[1].cementerio.push_back("blue_eyes");
		const int k = m.mano(0, "monster_reborn");
		const bool ok = m.activar(0, H(0, k), { Lugar{ 1, EN_CEMENTERIO, 0 } });
		m.pasarTodo();
		const int z = m.zonaDe(0, "blue_eyes");
		cu::caso("CU-BE-ESPECIAL", ok && z >= 0 && m.d.l[0].m[(size_t)z].especial, "Monster Reborn no invoco de modo especial a Blue-Eyes del cementerio rival");
		cu::caso("CU-BE-ESPECIAL", m.d.l[1].cementerio.empty() && m.enCementerio(0, "monster_reborn") == 1, "el cementerio no quedo bien tras Monster Reborn");
		cu::caso("CU-BE-ESPECIAL-NO-GASTA-NORMAL", !m.d.l[0].invocoNormal, "la invocacion especial gasto la normal del turno");
	}
	// fusion: Polymerization + materiales de la mano o del campo
	{
		Mesa m(&base);
		m.d.l[0].extra = { "flame_swordsman" };
		const int p = m.mano(0, "polymerization");
		const int a = m.mano(0, "flame_manipulator");
		Jugada j;
		j.tipo = J_FUSION;
		j.mano = p;
		j.carta = "flame_swordsman";
		cu::caso("CU-BE-FUSION-SIN-MATERIALES", !m.d.aplicar(j), "Flame Swordsman se fusiono con un solo material");
		const int b = m.mano(0, "masaki_the_legendary_swordsman");
		j.manoExtra = { a, b };
		cu::caso("CU-BE-FUSION", m.d.aplicar(j), "no se fusiono Flame Swordsman con sus materiales de la mano");
		m.pasarTodo();
		cu::caso("CU-BE-FUSION", m.zonaDe(0, "flame_swordsman") >= 0 && m.d.l[0].extra.empty() && m.d.l[0].cementerio.size() == 3 && m.d.l[0].mano.empty(),
		         "tras la fusion: extra %d, cementerio %d, mano %d", (int)m.d.l[0].extra.size(), (int)m.d.l[0].cementerio.size(), (int)m.d.l[0].mano.size());
		cu::caso("CU-BE-FUSION", m.d.l[0].m[(size_t)m.zonaDe(0, "flame_swordsman")].especial, "la fusion no cuenta como invocacion especial");
		Mesa c(&base);
		c.d.l[0].extra = { "flame_swordsman" };
		const int z = c.mon(0, "flame_manipulator");
		const int p2 = c.mano(0, "polymerization");
		const int b2 = c.mano(0, "masaki_the_legendary_swordsman");
		Jugada k;
		k.tipo = J_FUSION;
		k.mano = p2;
		k.carta = "flame_swordsman";
		k.manoExtra = { b2 };
		k.tributos = { z };
		cu::caso("CU-BE-FUSION-CAMPO", c.d.aplicar(k), "no se fusiono con un material del campo");
	}
	// ritual: la magia ritual + el monstruo + sacrificios con nivel suficiente
	{
		Mesa m(&base);
		const int z0 = m.mon(0, "celtic_guardian"), z1 = m.mon(0, "battle_ox");   // 4 + 4 = 8
		const int r = m.mano(0, "black_luster_ritual"), s = m.mano(0, "black_luster_soldier");
		Jugada j;
		j.tipo = J_RITUAL;
		j.mano = r;
		j.mano2 = s;
		j.tributos = { z0 };
		cu::caso("CU-BE-RITUAL-NIVEL", !m.d.aplicar(j), "Black Luster Soldier (8) se invoco con 4 niveles");
		j.tributos = { z0, z1 };
		cu::caso("CU-BE-RITUAL", m.d.aplicar(j), "Black Luster Soldier no se invoco con 8 niveles");
		m.pasarTodo();
		cu::caso("CU-BE-RITUAL", m.zonaDe(0, "black_luster_soldier") >= 0 && m.enCementerio(0, "black_luster_ritual") == 1, "el ritual no quedo en el campo o la magia no fue al cementerio");
	}
	// union: Y-Dragon Head se equipa a X-Head Cannon y se desequipa
	{
		Mesa m(&base);
		m.mon(0, "x_head_cannon");
		const int y = m.mon(0, "y_dragon_head");
		const int atk = m.d.l[0].m[0].atkEf;
		const bool ok = m.activar(0, M(0, y));
		m.pasarTodo();
		cu::caso("CU-BE-UNION", ok && m.d.l[0].m[(size_t)y].vacia() && m.d.l[0].mt[0].id == "y_dragon_head" && m.d.l[0].m[0].atkEf == atk + 400, "la union no se equipo (+400)");
	}
	// espiritu: invocado normal vuelve a la mano en la fase final
	{
		Mesa m(&base);
		const int k = m.mano(0, "susa_soldier");
		if (base.carta("susa_soldier")) {
			const bool ok = m.jugar(J_INVOCAR, k);
			m.pasarTodo();
			for (int f = 0; f < 3; f++) m.fase();
			cu::caso("CU-BE-ESPIRITU", ok && m.enMano(0, "susa_soldier") && m.zonaDe(0, "susa_soldier") < 0, "Susa Soldier (espiritu) no volvio a la mano en la fase final");
		}
	}
	// toon: necesita Toon World boca arriba
	{
		Mesa m(&base);
		const int z0 = m.mon(0, "battle_ox");
		const int k = m.mano(0, "toon_summoned_skull");
		Jugada t;
		t.tipo = J_TOON;
		t.mano = k;
		t.tributos = { z0 };
		cu::caso("CU-BE-TOON", !m.d.aplicar(t), "un Toon se invoco sin Toon World");
		cu::caso("CU-BE-TOON", !m.jugar(J_INVOCAR, k, -1, { z0 }), "Toon Summoned Skull se invoco de modo normal");
		const int w = m.colocar(0, "toon_world");
		m.d.l[0].mt[(size_t)w].bocaAbajo = false;
		cu::caso("CU-BE-TOON", m.d.aplicar(t), "Toon Summoned Skull no se invoco con Toon World y 1 sacrificio");
	}
	// limite de mano: en la fase final con mas de 6 hay que descartar
	{
		Mesa m(&base);
		for (int k = 0; k < 8; k++) m.mano(0, "celtic_guardian");
		m.fase();
		m.fase();
		m.fase();
		cu::caso("CU-BE-LIMITE-MANO", m.d.activo == 0 && m.d.fase == F_FINAL, "con 8 cartas paso el turno sin descartar");
		cu::caso("CU-BE-LIMITE-MANO", !m.fase(), "J_FASE con 8 cartas en la fase final se acepto");
		m.jugar(J_DESCARTAR, 0);
		cu::caso("CU-BE-LIMITE-MANO", m.d.activo == 0, "paso el turno con 7");
		m.jugar(J_DESCARTAR, 0);
		m.pasarTodo();
		cu::caso("CU-BE-LIMITE-MANO", m.d.activo == 1 && m.d.l[0].mano.size() == 6 && m.d.l[0].cementerio.size() == 2, "con 6 no paso el turno (o el descarte no fue al cementerio)");
	}
	// limite de zonas: 5 monstruos y 5 magias/trampas
	{
		Mesa m(&base);
		for (int k = 0; k < 5; k++) m.mon(0, "celtic_guardian");
		const int a = m.mano(0, "battle_ox");
		cu::caso("CU-BE-LIMITE-ZONAS", !m.jugar(J_INVOCAR, a) && !m.jugar(J_COLOCAR, a), "se invoco un sexto monstruo");
		cu::caso("CU-BE-LIMITE-ZONAS", m.d.invocarEspecial(0, "kuriboh", false) < 0, "una invocacion especial entro con las 5 zonas llenas");
		Mesa c(&base);
		for (int k = 0; k < 5; k++) c.colocar(0, "trap_hole");
		const int t = c.mano(0, "mirror_force");
		cu::caso("CU-BE-LIMITE-ZONAS", !c.jugar(J_COLOCAR_MT, t), "se coloco una sexta magia/trampa");
		const int f = c.mano(0, "umi");
		cu::caso("CU-BE-LIMITE-ZONAS", c.jugar(J_COLOCAR_MT, f) && c.d.l[0].campo.id == "umi", "la magia de campo no fue a su zona con las 5 llenas");
	}
	// trampas: no el turno en que se colocan; ventana tras la invocacion (Trap Hole)
	{
		Mesa m(&base);
		const int t = m.colocar(1, "trap_hole", 3);
		const int a = m.mano(0, "summoned_skull");
		const int z0 = m.mon(0, "celtic_guardian");
		m.d.turno = 3;
		m.jugar(J_INVOCAR, a, -1, { z0 });
		cu::caso("CU-BE-VENTANA-INVOCACION", m.d.ef.preg.tipo == P_NADA || m.d.ef.preg.lado != 1, "Trap Hole colocada este turno se ofrecio (turno %d)", m.d.turno);
		(void)t;
		Mesa c(&base);
		c.colocar(1, "trap_hole", 1);
		const int z1 = c.mon(0, "celtic_guardian");
		const int b = c.mano(0, "summoned_skull");
		c.jugar(J_INVOCAR, b, -1, { z1 });
		cu::caso("CU-BE-VENTANA-INVOCACION", c.d.ef.preg.tipo == P_RESPUESTA && c.d.ef.preg.lado == 1, "tras la invocacion el rival no tuvo ventana para Trap Hole");
		c.activar(1, MT(1, 0), { M(0, c.zonaDe(0, "summoned_skull")) });
		c.pasarTodo();
		cu::caso("CU-BE-VENTANA-INVOCACION", c.zonaDe(0, "summoned_skull") < 0 && c.enCementerio(0, "summoned_skull") == 1, "Trap Hole no destruyo a Summoned Skull");
		cu::caso("CU-BE-TRAMPA-MISMO-TURNO", true, "");
		Mesa d(&base);
		const int z = d.colocar(0, "mirror_force", 3);
		cu::caso("CU-BE-TRAMPA-MISMO-TURNO", !d.activar(0, MT(0, z)), "una trampa se activo el turno en que se coloco");
	}
	// cadena: LIFO y velocidades (una magia normal no responde; Magic Jammer niega)
	{
		Mesa m(&base);
		m.mon(1, "celtic_guardian");
		m.colocar(1, "magic_jammer", 1);
		m.mano(1, "celtic_guardian");
		const int k = m.mano(0, "raigeki");
		m.activar(0, H(0, k));
		cu::caso("CU-BE-CADENA-VENTANA", m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.preg.lado == 1, "el rival no tuvo ventana para responder a Raigeki");
		const std::vector<Lugar> resp = activables(m.d, 1, true);
		bool jammer = false;
		for (const Lugar& g : resp) jammer |= cartaEn(m.d, g) == "magic_jammer";
		cu::caso("CU-BE-CADENA-VELOCIDAD", jammer, "Magic Jammer (velocidad 3) no se ofrecio en respuesta a una magia");
		Jugada j = jugadaActivar(MT(1, 0));
		j.descartes = { 0 };
		if (const DefCarta* df = defCarta("magic_jammer")) j.costes = costesIA(m.d, 1, *df, MT(1, 0));
		m.d.aplicar(j);
		// (el orden: Raigeki C1, Magic Jammer C2; se resuelve primero C2)
		std::vector<int> act, res;
		for (const Suceso& s : m.d.sucesos) {
			if (s.tipo == S_ACTIVA_MT) act.push_back(s.valor);
			if (s.tipo == S_RESUELVE) res.push_back(s.valor);
		}
		cu::caso("CU-BE-CADENA-LIFO", act.size() == 2 && act[0] == 1 && act[1] == 2 && !res.empty() && res[0] == 2, "la cadena no fue C1 Raigeki, C2 Magic Jammer y se resolvio primero C2 (%d activaciones, %d resoluciones)",
		         (int)act.size(), (int)res.size());
		m.pasarTodo();
		cu::caso("CU-BE-CADENA-LIFO", m.d.l[1].monstruos() == 1 && m.enCementerio(0, "raigeki") == 1 && m.d.ef.cadena.empty(), "Magic Jammer (C2) no nego a Raigeki (C1)");
		Mesa c(&base);
		c.mon(1, "celtic_guardian");
		c.mano(1, "dark_hole");
		const int r = c.mano(0, "raigeki");
		c.activar(0, H(0, r));
		bool dh = false;
		for (const Lugar& g : activables(c.d, 1, true)) dh |= cartaEn(c.d, g) == "dark_hole";
		cu::caso("CU-BE-CADENA-VELOCIDAD", !dh, "una magia normal (velocidad 1) se ofrecio en respuesta");
		c.pasarTodo();
	}
	// batalla: ATK contra ATK, ATK contra DEF, empate, directo, boca abajo
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "summoned_skull");   // 2500
		const int o = m.mon(1, "celtic_guardian");  // 1400
		cu::caso("CU-BE-BATALLA-ATK", m.jugar(J_ATACAR, -1, a, {}, o), "no se pudo atacar");
		m.pasarTodo();
		cu::caso("CU-BE-BATALLA-ATK", m.d.l[1].lp == LP_INICIAL - 1100 && m.d.l[1].monstruos() == 0 && m.d.l[0].lp == LP_INICIAL, "2500 contra 1400: LP %d / %d", m.d.l[0].lp,
		         m.d.l[1].lp);
		cu::caso("CU-BE-BATALLA-UN-ATAQUE", !m.d.puedeAtacar(0, a), "el monstruo puede atacar dos veces");
	}
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "celtic_guardian");   // 1400
		const int o = m.mon(1, "summoned_skull");    // 2500
		m.jugar(J_ATACAR, -1, a, {}, o);
		m.pasarTodo();
		cu::caso("CU-BE-BATALLA-ATK", m.d.l[0].lp == LP_INICIAL - 1100 && m.d.l[0].monstruos() == 0 && m.d.l[1].monstruos() == 1, "atacar a uno mas fuerte: LP %d", m.d.l[0].lp);
	}
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "battle_ox");   // 1700
		const int o = m.mon(1, "battle_ox");
		m.jugar(J_ATACAR, -1, a, {}, o);
		m.pasarTodo();
		cu::caso("CU-BE-BATALLA-EMPATE", m.d.l[0].monstruos() == 0 && m.d.l[1].monstruos() == 0 && m.d.l[0].lp == LP_INICIAL && m.d.l[1].lp == LP_INICIAL, "ATK iguales: no se destruyeron los dos");
	}
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "battle_ox");                 // 1700
		const int o = m.mon(1, "giant_soldier_of_stone", true);   // DEF 2000
		m.jugar(J_ATACAR, -1, a, {}, o);
		m.pasarTodo();
		cu::caso("CU-BE-BATALLA-DEF", m.d.l[0].lp == LP_INICIAL - 300 && m.d.l[1].monstruos() == 1 && m.d.l[0].monstruos() == 1, "1700 contra DEF 2000: LP %d", m.d.l[0].lp);
		Mesa c(&base, F_BATALLA);
		const int b = c.mon(0, "summoned_skull");
		const int p = c.mon(1, "celtic_guardian", true);   // DEF 1200
		c.jugar(J_ATACAR, -1, b, {}, p);
		c.pasarTodo();
		cu::caso("CU-BE-BATALLA-DEF", c.d.l[1].lp == LP_INICIAL && c.d.l[1].monstruos() == 0, "2500 contra DEF 1200: el defensor recibio danno o no se destruyo");
	}
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "summoned_skull");
		const int o = m.mon(1, "celtic_guardian", true, true);
		m.jugar(J_ATACAR, -1, a, {}, o);
		m.pasarTodo();
		bool volteo = false;
		for (const Suceso& s : m.d.sucesos) volteo |= s.tipo == S_VOLTEA || (s.tipo == S_AL_CEMENTERIO && s.carta == "celtic_guardian");
		cu::caso("CU-BE-BATALLA-BOCA-ABAJO", volteo && m.d.l[1].monstruos() == 0, "atacar a uno boca abajo no lo volteo/destruyo");
	}
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "summoned_skull");
		m.mon(1, "celtic_guardian");
		cu::caso("CU-BE-BATALLA-DIRECTO", !m.jugar(J_ATACAR, -1, a, {}, -1), "ataque directo con monstruos del rival");
		Mesa c(&base, F_BATALLA);
		const int b = c.mon(0, "summoned_skull");
		cu::caso("CU-BE-BATALLA-DIRECTO", c.jugar(J_ATACAR, -1, b, {}, -1), "no ataco directo con el campo rival vacio");
		c.pasarTodo();
		cu::caso("CU-BE-BATALLA-DIRECTO", c.d.l[1].lp == LP_INICIAL - 2500, "ataque directo: LP %d", c.d.l[1].lp);
		Mesa e(&base, F_BATALLA);
		const int z = e.mon(0, "summoned_skull", true);
		cu::caso("CU-BE-BATALLA-DEFENSA-NO-ATACA", !e.jugar(J_ATACAR, -1, z, {}, -1), "un monstruo en defensa ataco");
	}
	// la repeticion del ataque (regla oficial, rama classic-reglas-oficiales): si tras la declaracion cambia el numero de
	// monstruos del rival (Scapegoat), el atacante elige otro objetivo, el mismo o no atacar (pregunta P_ELEGIR
	// "ef.elegir.repeticion"); si no ataca, el monstruo puede volver a atacar en esta fase
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "summoned_skull");
		const int sg = m.colocar(1, "scapegoat", 1);
		m.jugar(J_ATACAR, -1, a, {}, -1);
		if (m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.preg.lado == 1) m.activar(1, MT(1, sg));
		pasarRival(m);
		const bool pregunta = m.d.ef.preg.tipo == P_ELEGIR && m.d.ef.preg.lado == 0 && m.d.ef.preg.texto == "ef.elegir.repeticion" && m.d.ef.preg.opcional;
		cu::caso("CU-BE-REPETICION-ATAQUE", pregunta && m.d.ef.preg.cands.size() == 4, "Scapegoat contra un ataque directo: no se pregunto la repeticion (pregunta %d lado %d, %d candidatos)",
		         m.d.ef.preg.tipo, m.d.ef.preg.lado, (int)m.d.ef.preg.cands.size());
		bool sinDirecto = true;
		for (const Lugar& g : m.d.ef.preg.cands) sinDirecto &= g.i >= 0;
		cu::caso("CU-BE-REPETICION-ATAQUE-DIRECTO", pregunta && sinDirecto, "con fichas en el campo se ofrecio el ataque directo");
		Jugada no;
		no.tipo = J_PASAR;
		cu::caso("CU-BE-REPETICION-ATAQUE", m.d.aplicar(no), "no se pudo elegir no atacar");
		cu::caso("CU-BE-REPETICION-ATAQUE", m.d.l[1].monstruos() == 4 && m.d.l[1].lp == LP_INICIAL && m.d.puedeAtacar(0, a) && !m.d.ocupado(),
		         "no atacar: fichas %d, LP %d, puede volver a atacar %d", m.d.l[1].monstruos(), m.d.l[1].lp, (int)m.d.puedeAtacar(0, a));
		cu::caso("CU-BE-REPETICION-ATAQUE", m.jugar(J_ATACAR, -1, a, {}, 0), "tras no atacar, no pudo volver a declarar el ataque");
		m.pasarTodo();
		cu::caso("CU-BE-REPETICION-ATAQUE", m.d.l[1].monstruos() == 3 && !m.d.puedeAtacar(0, a), "el segundo ataque no destruyo la ficha o el monstruo ataco dos veces");
	}
	{   // el objetivo sigue en el campo: elegir otro objetivo (una ficha) o el mismo
		for (int mismo = 0; mismo < 2; mismo++) {
			Mesa c(&base, F_BATALLA);
			const int b = c.mon(0, "summoned_skull");
			const int cg = c.mon(1, "celtic_guardian");
			const int s2 = c.colocar(1, "scapegoat", 1);
			c.jugar(J_ATACAR, -1, b, {}, cg);
			if (c.d.ef.preg.tipo == P_RESPUESTA && c.d.ef.preg.lado == 1) c.activar(1, MT(1, s2));
			pasarRival(c);
			const bool pregunta = c.d.ef.preg.tipo == P_ELEGIR && c.d.ef.preg.lado == 0 && c.d.ef.preg.texto == "ef.elegir.repeticion" && c.d.ef.preg.cands.size() == 5;
			cu::caso("CU-BE-REPETICION-ATAQUE-ELEGIR", pregunta && c.d.ef.preg.cands[0].i == cg, "con el objetivo en el campo no se pregunto (o no va primero el de antes): pregunta %d, %d candidatos",
			         c.d.ef.preg.tipo, (int)c.d.ef.preg.cands.size());
			if (!pregunta) continue;
			Jugada j;
			j.tipo = J_ELEGIR;
			j.objetivo = mismo ? 0 : 1;
			const int zf = c.d.ef.preg.cands[(size_t)j.objetivo].i;
			c.d.aplicar(j);
			const bool otraVentana = c.d.ef.preg.tipo == P_NADA || (c.d.ef.ventana.tipo == V_ATAQUE && c.d.ef.ataque.hay && c.d.ef.ataque.objetivo == zf);
			cu::caso("CU-BE-REPETICION-ATAQUE-ELEGIR", otraVentana, "la repeticion no declaro el ataque al elegido");
			c.pasarTodo();
			if (mismo)
				cu::caso("CU-BE-REPETICION-ATAQUE-ELEGIR", c.zonaDe(1, "celtic_guardian") < 0 && c.d.l[1].lp == LP_INICIAL - 1100 && c.d.l[1].monstruos() == 4,
				         "elegir el mismo objetivo: Celtic %d, LP %d", c.zonaDe(1, "celtic_guardian"), c.d.l[1].lp);
			else
				cu::caso("CU-BE-REPETICION-ATAQUE-ELEGIR", c.zonaDe(1, "celtic_guardian") >= 0 && c.d.l[1].lp == LP_INICIAL && c.d.l[1].monstruos() == 4,
				         "elegir otro objetivo (una ficha): Celtic %d, LP %d, monstruos %d", c.zonaDe(1, "celtic_guardian"), c.d.l[1].lp, c.d.l[1].monstruos());
			cu::caso("CU-BE-REPETICION-ATAQUE-ELEGIR", !c.d.puedeAtacar(0, b), "tras la repeticion el monstruo puede atacar otra vez");
		}
	}
	{   // sin cambio en el numero de monstruos (Waboku): no hay repeticion, el ataque sigue
		Mesa w(&base, F_BATALLA);
		const int a = w.mon(0, "summoned_skull");
		const int cg = w.mon(1, "celtic_guardian");
		const int wb = w.colocar(1, "waboku", 1);
		w.jugar(J_ATACAR, -1, a, {}, cg);
		if (w.d.ef.preg.tipo == P_RESPUESTA && w.d.ef.preg.lado == 1) w.activar(1, MT(1, wb));
		pasarRival(w);
		cu::caso("CU-BE-REPETICION-ATAQUE", w.d.ef.preg.tipo != P_ELEGIR && !w.d.ef.repeticion.hay, "sin cambio en el numero de monstruos se pregunto la repeticion");
		w.pasarTodo();
		bool combate = false;
		for (const Suceso& x : w.d.sucesos) combate |= x.tipo == S_COMBATE;
		cu::caso("CU-BE-REPETICION-ATAQUE", combate && w.zonaDe(1, "celtic_guardian") >= 0 && w.d.l[1].lp == LP_INICIAL, "con Waboku el ataque no siguio o hizo danno");
	}
	{   // el objetivo se fue y el campo del rival quedo vacio: la repeticion ofrece el ataque directo
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "summoned_skull");
		const int cg = m.mon(1, "celtic_guardian");
		m.d.preguntarSiempre[1] = true;   // (el rival tiene su ventana tras la declaracion)
		m.jugar(J_ATACAR, -1, a, {}, cg);
		m.d.alCementerio(1, cg);   // (como si un efecto lo sacara en la ventana del ataque)
		pasarRival(m);
		const bool pregunta = m.d.ef.preg.tipo == P_ELEGIR && m.d.ef.repeticion.hay && m.d.ef.preg.cands.size() == 1 && m.d.ef.preg.cands[0].i == -1;
		cu::caso("CU-BE-REPETICION-ATAQUE-DIRECTO", pregunta, "sin monstruos del rival la repeticion no ofrece el ataque directo (%d candidatos)", (int)m.d.ef.preg.cands.size());
		m.pasarTodo();
		cu::caso("CU-BE-REPETICION-ATAQUE-DIRECTO", m.d.l[1].lp == LP_INICIAL - 2500, "el ataque directo de la repeticion: LP %d", m.d.l[1].lp);
	}
	// la IA (clasica y humana) responde la repeticion con su politica de ataque
	{
		Cerebro c(PERSONALIDADES[0], 8, 3);
		Humana h;
		h.configurar(true, "", "", 5);
		Cerebro ch(PERSONALIDADES[0], 8, 5);
		{   // letal: el objetivo se fue, campo vacio y el rival con 1000 LP: ataca directo
			Mesa m(&base, F_BATALLA);
			m.d.l[1].lp = 1000;
			const int a = m.mon(0, "summoned_skull");
			const int cg = m.mon(1, "celtic_guardian");
			m.d.preguntarSiempre[1] = true;
			m.jugar(J_ATACAR, -1, a, {}, cg);
			m.d.alCementerio(1, cg);
			pasarRival(m);
			const bool pregunta = m.d.ef.preg.tipo == P_ELEGIR && m.d.ef.repeticion.hay;
			const Jugada j = c.decidir(m.d);
			cu::caso("CU-BE-REPETICION-ATAQUE-IA", pregunta && j.tipo == J_ELEGIR && j.objetivo == 0, "IA clasica: con el letal no ataco directo en la repeticion (jugada %d, objetivo %d)", (int)j.tipo, j.objetivo);
			Jugada jh;
			const bool lista = h.empezar(m.d, ch, jh);
			cu::caso("CU-BE-REPETICION-ATAQUE-IA", pregunta && lista && jh.tipo == J_ELEGIR && jh.objetivo == 0, "IA humana: con el letal no ataco directo en la repeticion (jugada %d, objetivo %d)", (int)jh.tipo, jh.objetivo);
			m.d.aplicar(j);
			m.pasarTodo();
			cu::caso("CU-BE-REPETICION-ATAQUE-IA", m.d.ganador == 0, "la respuesta de la IA no gano el duelo");
			(void)a;
		}
		{   // suicida: el objetivo se fue y en su lugar hay un Blue-Eyes: no ataca (y puede volver a atacar)
			Mesa m(&base, F_BATALLA);
			const int a = m.mon(0, "celtic_guardian");
			const int bo = m.mon(1, "battle_ox");
			m.d.preguntarSiempre[1] = true;
			m.jugar(J_ATACAR, -1, a, {}, bo);
			m.d.alCementerio(1, bo);
			m.mon(1, "blue_eyes");
			pasarRival(m);
			const bool pregunta = m.d.ef.preg.tipo == P_ELEGIR && m.d.ef.repeticion.hay && m.d.ef.preg.cands.size() == 1;
			const Jugada j = c.decidir(m.d);
			cu::caso("CU-BE-REPETICION-ATAQUE-IA", pregunta && j.tipo == J_PASAR, "IA clasica: ataco a un Blue-Eyes con 1400 en la repeticion (jugada %d)", (int)j.tipo);
			Jugada jh;
			const bool lista = h.empezar(m.d, ch, jh);
			cu::caso("CU-BE-REPETICION-ATAQUE-IA", pregunta && lista && jh.tipo == J_PASAR, "IA humana: ataco a un Blue-Eyes con 1400 en la repeticion (jugada %d)", (int)jh.tipo);
			cu::caso("CU-BE-REPETICION-ATAQUE-IA", elegirIA(m.d, 0) < 0, "elegirIA (sin personalidad) ataco al Blue-Eyes");
			m.d.aplicar(j);
			cu::caso("CU-BE-REPETICION-ATAQUE-IA", m.zonaDe(0, "celtic_guardian") >= 0 && m.d.puedeAtacar(0, a), "tras no atacar, Celtic no sigue o no puede volver a atacar");
		}
	}
	// victoria: LP a 0
	{
		Mesa m(&base, F_BATALLA);
		m.d.l[1].lp = 1000;
		const int a = m.mon(0, "summoned_skull");
		m.jugar(J_ATACAR, -1, a, {}, -1);
		m.pasarTodo();
		cu::caso("CU-BE-VICTORIA-LP", m.d.ganador == 0, "el rival con 0 LP no perdio (ganador %d, LP %d)", m.d.ganador, m.d.l[1].lp);
		cu::caso("CU-BE-VICTORIA-FIN", !m.jugar(J_FASE), "se pudo jugar con el duelo terminado");
	}
	// victoria: Exodia (las 5 piezas en la mano)
	{
		Mesa m(&base);
		for (const std::string& p : { "exodia_the_forbidden_one", "left_arm_of_the_forbidden_one", "right_arm_of_the_forbidden_one", "left_leg_of_the_forbidden_one" }) m.mano(0, p);
		m.d.l[0].mazo.push_back("right_leg_of_the_forbidden_one");
		for (int f = 0; f < 4; f++) m.fase();
		m.hastaPrincipal(0);
		cu::caso("CU-BE-VICTORIA-EXODIA", m.d.ganador == 0, "robar la quinta pieza de Exodia no gano el duelo (ganador %d)", m.d.ganador);
	}
}

// ------------------------------------------------------------------------------------------------- atajos conocidos
// los 4 atajos que quedaron anotados en docs/control_final.md: la prueba pide lo que dice el texto de la carta
inline void atajos(const Base& base)
{
	// Fairy Guardian: vuelve al fondo del mazo la magia que el RIVAL mando al cementerio este turno (no la ultima)
	{
		Mesa m(&base);
		const int sw = m.colocar(0, "swords_of_revealing_light", 1);
		m.d.l[0].mt[(size_t)sw].bocaAbajo = false;
		const int ms = m.colocar(1, "mystical_space_typhoon", 1);
		const int fg = m.mon(0, "fairy_guardian");
		const int k = m.mano(0, "pot_of_greed");
		m.activar(0, H(0, k));
		const bool ventana = m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.preg.lado == 1;
		if (ventana) m.activar(1, MT(1, ms), { MT(0, sw) });
		m.pasarTodo();
		const bool armado = ventana && m.enCementerio(0, "swords_of_revealing_light") == 1 && m.enCementerio(0, "pot_of_greed") == 1 && m.d.l[0].cementerio.back() == "pot_of_greed";
		cu::caso("CU-BE-ATAJO-FAIRY-GUARDIAN", armado, "no se armo la escena (MST del rival sobre Swords en respuesta a Pot of Greed)");
		const bool ok = m.activar(0, M(0, fg));
		m.pasarTodo();
		cu::caso("CU-BE-ATAJO-FAIRY-GUARDIAN", ok && !m.d.l[0].mazo.empty() && m.d.l[0].mazo.front() == "swords_of_revealing_light" && m.enCementerio(0, "pot_of_greed") == 1,
		         "Fairy Guardian no devolvio al fondo del mazo la magia que destruyo el rival (fondo: %s)", m.d.l[0].mazo.empty() ? "-" : m.d.l[0].mazo.front().c_str());
		Mesa c(&base);
		const int f2 = c.mon(0, "fairy_guardian");
		c.d.l[0].cementerio.push_back("pot_of_greed");
		cu::caso("CU-BE-ATAJO-FAIRY-GUARDIAN", !motivoNoActivar(c.d, 0, M(0, f2), false).empty(), "Fairy Guardian se activa sin una magia perdida por el rival este turno");
	}
	// Pitch-Black Power Stone: el contador puede ir a cualquier carta que pueda tener contadores de magia (tambien del
	// rival), y elige quien la activa
	{
		Mesa m(&base);
		const int z = m.colocar(0, "pitch_black_power_stone", 1);
		m.d.l[0].mt[(size_t)z].bocaAbajo = false;
		m.d.l[0].mt[(size_t)z].contadores = 3;
		const int b = m.mon(1, "breaker_the_magical_warrior");
		const int antes = m.d.l[1].m[(size_t)b].contadores;
		const std::string motivo = motivoNoActivar(m.d, 0, MT(0, z), false);
		cu::caso("CU-BE-ATAJO-PITCH-BLACK", motivo.empty(), "Pitch-Black Power Stone no puede pasar un contador a Breaker del rival (%s)", motivo.c_str());
		if (motivo.empty()) {
			m.activar(0, MT(0, z));
			m.pasarTodo();
			cu::caso("CU-BE-ATAJO-PITCH-BLACK", m.d.l[1].m[(size_t)b].contadores == antes + 1 && m.d.l[0].mt[(size_t)z].contadores == 2, "el contador no paso a Breaker (%d -> %d)", antes,
			         m.d.l[1].m[(size_t)b].contadores);
		}
		Mesa c(&base);
		const int z2 = c.colocar(0, "pitch_black_power_stone", 1);
		c.d.l[0].mt[(size_t)z2].bocaAbajo = false;
		c.d.l[0].mt[(size_t)z2].contadores = 3;
		const int s1 = c.mon(0, "skilled_dark_magician");
		const int s2 = c.mon(1, "breaker_the_magical_warrior");
		c.activar(0, MT(0, z2));
		pasarRival(c);
		const bool pregunta = c.d.ef.preg.tipo == P_ELEGIR && c.d.ef.preg.lado == 0 && c.d.ef.preg.cands.size() == 2;
		cu::caso("CU-BE-ATAJO-PITCH-BLACK", pregunta, "con dos cartas que pueden recibirlo, no se pregunta a cual (pregunta %d, %d candidatas)", c.d.ef.preg.tipo, (int)c.d.ef.preg.cands.size());
		(void)s1;
		(void)s2;
		c.pasarTodo();
	}
	// Level Conversion Lab: el monstruo de la mano lo elige quien la activa (no el de mas nivel)
	{
		Mesa m(&base);
		const int z = m.colocar(0, "level_conversion_lab", 1);
		m.mano(0, "blue_eyes");
		m.mano(0, "dark_magician");
		m.activar(0, MT(0, z));
		pasarRival(m);
		const bool pregunta = m.d.ef.preg.tipo == P_ELEGIR && m.d.ef.preg.lado == 0 && m.d.ef.preg.cands.size() == 2;
		cu::caso("CU-BE-ATAJO-LEVEL-LAB", pregunta, "Level Conversion Lab no deja elegir el monstruo de la mano (pregunta %d, %d candidatas)", m.d.ef.preg.tipo, (int)m.d.ef.preg.cands.size());
		if (pregunta) {
			int k = 0;
			for (int i = 0; i < (int)m.d.ef.preg.cands.size(); i++)
				if (cartaEn(m.d, m.d.ef.preg.cands[(size_t)i]) == "dark_magician") k = i;
			Jugada j;
			j.tipo = J_ELEGIR;
			j.objetivo = k;
			m.d.aplicar(j);
			m.pasarTodo();
			cu::caso("CU-BE-ATAJO-LEVEL-LAB", m.enMano(0, "blue_eyes") && nivelLaboratorio(m.d, 0, "blue_eyes") == 0 && (m.enCementerio(0, "dark_magician") == 1 || nivelLaboratorio(m.d, 0, "dark_magician") > 0),
			         "el dado no se aplico al monstruo elegido");
		}
	}
	// la ventana justo despues del danno (regla oficial, rama classic-reglas-oficiales): Inferno Tempest (3000+ de danno de
	// batalla de un ataque), Attack and Receive y Numinous Healer se activan solo ahi, solo el que recibio el danno
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "blue_eyes");
		const int it = m.colocar(1, "inferno_tempest", 1);
		m.jugar(J_ATACAR, -1, a, {}, -1);
		for (int g = 0; g < 10 && m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.ventana.tipo != V_DANNO; g++) m.pasarTodo1();
		const bool ventana = m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.preg.lado == 1 && m.d.ef.ventana.tipo == V_DANNO;
		cu::caso("CU-BE-ATAJO-INFERNO-TEMPEST", ventana && m.d.l[1].lp == LP_INICIAL - 3000 && motivoNoActivar(m.d, 1, MT(1, it), true).empty(),
		         "tras 3000 de danno de batalla no se abrio la ventana con Inferno Tempest (pregunta %d lado %d ventana %d, LP %d)", m.d.ef.preg.tipo, m.d.ef.preg.lado, m.d.ef.ventana.tipo, m.d.l[1].lp);
		cu::caso("CU-BE-VENTANA-DANNO", ventana && motivoNoActivar(m.d, 0, MT(1, it), true) != "", "el que no recibio el danno puede activar en la ventana del danno");
		if (ventana) {
			m.d.l[1].mazo.push_back("battle_ox");
			const bool ok = m.activar(1, MT(1, it));
			m.pasarTodo();
			cu::caso("CU-BE-ATAJO-INFERNO-TEMPEST", ok && std::find(m.d.l[1].desterrados.begin(), m.d.l[1].desterrados.end(), "battle_ox") != m.d.l[1].desterrados.end(),
			         "Inferno Tempest no destierra los monstruos del mazo");
		}
		// el momento: si no se activa en esa ventana, despues ya no (ni en la batalla ni en la fase final)
		Mesa c(&base, F_BATALLA);
		const int b = c.mon(0, "blue_eyes");
		const int it2 = c.colocar(1, "inferno_tempest", 1);
		c.jugar(J_ATACAR, -1, b, {}, -1);
		for (int g = 0; g < 10 && c.d.ef.preg.tipo == P_RESPUESTA && c.d.ef.ventana.tipo != V_DANNO; g++) c.pasarTodo1();
		const bool v2 = c.d.ef.ventana.tipo == V_DANNO && c.d.ef.preg.lado == 1;
		c.pasarTodo();
		cu::caso("CU-BE-ATAJO-INFERNO-TEMPEST-MOMENTO", v2 && !c.d.ocupado() && !motivoNoActivar(c.d, 1, MT(1, it2), true).empty(),
		         "tras pasar la ventana del danno, Inferno Tempest se puede activar despues (ventana %d)", (int)v2);
		c.jugar(J_FASE);
		c.pasarTodo();
		c.jugar(J_FASE);
		cu::caso("CU-BE-ATAJO-INFERNO-TEMPEST-MOMENTO", !motivoNoActivar(c.d, 1, MT(1, it2), true).empty() && activables(c.d, 1, true).empty(),
		         "Inferno Tempest se puede activar en la fase %d, despues del danno", (int)c.d.fase);
		c.pasarTodo();
		// con menos de 3000 (Battle Ox, 1700) no hay ventana para Inferno Tempest
		Mesa e(&base, F_BATALLA);
		const int o = e.mon(0, "battle_ox");
		e.colocar(1, "inferno_tempest", 1);
		e.jugar(J_ATACAR, -1, o, {}, -1);
		e.pasarTodo();
		bool vioVentana = false;
		cu::caso("CU-BE-ATAJO-INFERNO-TEMPEST", e.d.l[1].lp == LP_INICIAL - 1700 && !e.d.ocupado(), "con 1700 de danno se abrio una ventana para Inferno Tempest");
		(void)vioVentana;
	}
	// Attack and Receive: danno de batalla; se activa en la ventana y hace 700 al rival
	{
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "summoned_skull");
		const int ar = m.colocar(1, "attack_and_receive", 1);
		const int ar0 = m.colocar(0, "attack_and_receive", 1);
		m.jugar(J_ATACAR, -1, a, {}, -1);
		for (int g = 0; g < 10 && m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.ventana.tipo != V_DANNO; g++) m.pasarTodo1();
		const bool ventana = m.d.ef.ventana.tipo == V_DANNO && m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.preg.lado == 1;
		cu::caso("CU-BE-VENTANA-DANNO-ATTACK-RECEIVE", ventana && m.d.l[1].lp == LP_INICIAL - 2500, "tras el danno de batalla no se abrio la ventana con Attack and Receive (ventana %d)", m.d.ef.ventana.tipo);
		cu::caso("CU-BE-VENTANA-DANNO", ventana && !motivoNoActivar(m.d, 0, MT(0, ar0), true).empty(), "el atacante (sin danno) puede activar su Attack and Receive");
		if (ventana) {
			m.activar(1, MT(1, ar));
			// el atacante recibe 700: su propia ventana, con su Attack and Receive
			for (int g = 0; g < 10 && m.d.ef.preg.tipo == P_RESPUESTA && !(m.d.ef.ventana.tipo == V_DANNO && m.d.ef.preg.lado == 0); g++) m.pasarTodo1();
			const bool suya = m.d.ef.ventana.tipo == V_DANNO && m.d.ef.preg.lado == 0 && m.d.l[0].lp == LP_INICIAL - 700;
			cu::caso("CU-BE-VENTANA-DANNO-ATTACK-RECEIVE", suya, "Attack and Receive no hizo 700 o el atacante no tuvo su ventana (LP %d, ventana %d lado %d)", m.d.l[0].lp, m.d.ef.ventana.tipo, m.d.ef.preg.lado);
			m.pasarTodo();
			cu::caso("CU-BE-VENTANA-DANNO", !m.d.ocupado() && m.d.fase == F_BATALLA, "despues de las ventanas del danno la batalla no sigue libre");
			cu::caso("CU-BE-VENTANA-DANNO-ATTACK-RECEIVE", !motivoNoActivar(m.d, 0, MT(0, ar0), true).empty(), "Attack and Receive se puede activar despues de su ventana");
		}
		// sin carta para ese momento no se abre nada
		Mesa c(&base, F_BATALLA);
		const int b = c.mon(0, "summoned_skull");
		c.colocar(1, "mirror_force", 1);
		c.jugar(J_ATACAR, -1, b, {}, -1);
		for (int g = 0; g < 10 && c.d.ef.preg.tipo == P_RESPUESTA && c.d.ef.ventana.tipo != V_DANNO; g++) c.pasarTodo1();
		cu::caso("CU-BE-VENTANA-DANNO", c.d.ef.ventana.tipo != V_DANNO, "sin carta de ese momento se abrio la ventana del danno");
		c.pasarTodo();
	}
	// Numinous Healer: danno de efecto (Hinotama); solo el que lo recibio
	{
		Mesa m(&base);
		const int nh = m.colocar(1, "numinous_healer", 1);
		const int nh0 = m.colocar(0, "numinous_healer", 1);
		const int k = m.mano(0, "hinotama");
		m.activar(0, H(0, k));
		for (int g = 0; g < 10 && m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.ventana.tipo != V_DANNO; g++) m.pasarTodo1();
		const bool ventana = m.d.ef.ventana.tipo == V_DANNO && m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.preg.lado == 1 && m.d.l[1].lp == LP_INICIAL - 500;
		cu::caso("CU-BE-VENTANA-DANNO-NUMINOUS", ventana, "tras el danno de efecto (Hinotama) no se abrio la ventana con Numinous Healer (ventana %d, LP %d)", m.d.ef.ventana.tipo, m.d.l[1].lp);
		cu::caso("CU-BE-VENTANA-DANNO", ventana && !motivoNoActivar(m.d, 0, MT(0, nh0), true).empty(), "el que no recibio danno puede activar Numinous Healer");
		if (ventana) {
			m.activar(1, MT(1, nh));
			m.pasarTodo();
			cu::caso("CU-BE-VENTANA-DANNO-NUMINOUS", m.d.l[1].lp == LP_INICIAL - 500 + 1000, "Numinous Healer no gano 1000 (LP %d)", m.d.l[1].lp);
		}
		Mesa c(&base);
		const int nh1 = c.colocar(1, "numinous_healer", 1);
		const int k2 = c.mano(0, "hinotama");
		c.activar(0, H(0, k2));
		c.pasarTodo();
		cu::caso("CU-BE-VENTANA-DANNO-NUMINOUS", !c.d.ocupado() && c.d.l[1].lp == LP_INICIAL - 500 && !motivoNoActivar(c.d, 1, MT(1, nh1), true).empty(),
		         "Numinous Healer se puede activar despues de su ventana");
	}
	// la IA usa su carta en la ventana del danno (usarla o perderla)
	{
		Cerebro c(PERSONALIDADES[0], 8, 3);
		Humana h;
		h.configurar(true, "", "", 5);
		Cerebro ch(PERSONALIDADES[0], 8, 5);
		Mesa m(&base, F_BATALLA);
		const int a = m.mon(0, "summoned_skull");
		m.colocar(1, "attack_and_receive", 1);
		m.jugar(J_ATACAR, -1, a, {}, -1);
		for (int g = 0; g < 10 && m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.ventana.tipo != V_DANNO; g++) m.pasarTodo1();
		const bool ventana = m.d.ef.ventana.tipo == V_DANNO && m.d.ef.preg.lado == 1;
		const Jugada j = c.decidir(m.d);
		cu::caso("CU-BE-VENTANA-DANNO-IA", ventana && j.tipo == J_ACTIVAR, "IA clasica: no activo Attack and Receive en su ventana (jugada %d)", (int)j.tipo);
		Jugada jh;
		bool lista = h.empezar(m.d, ch, jh);
		for (int g = 0; g < 400 && !lista; g++) lista = h.trabajar(m.d, ch, 5.0, jh);
		cu::caso("CU-BE-VENTANA-DANNO-IA", ventana && lista && jh.tipo == J_ACTIVAR, "IA humana: no activo Attack and Receive en su ventana (jugada %d)", (int)jh.tipo);
		m.d.aplicar(j);
		m.pasarTodo();
		cu::caso("CU-BE-VENTANA-DANNO-IA", m.d.l[0].lp == LP_INICIAL - 700, "la activacion de la IA no hizo 700 (LP %d)", m.d.l[0].lp);
	}
	// Preguntar antes de activar = Siempre: el motor ofrece al jugador la ventana aunque no tenga nada que activar
	{
		Mesa m(&base);
		m.d.activo = 1;
		m.jugar(J_INVOCAR, m.mano(1, "celtic_guardian"));
		cu::caso("CU-BE-PREGUNTAR-SIEMPRE", m.d.ef.preg.tipo == P_NADA, "sin Siempre y sin nada que activar se pregunto al jugador");
		Mesa c(&base);
		c.d.activo = 1;
		c.d.preguntarSiempre[0] = true;
		c.jugar(J_INVOCAR, c.mano(1, "celtic_guardian"));
		cu::caso("CU-BE-PREGUNTAR-SIEMPRE", c.d.ef.preg.tipo == P_RESPUESTA && c.d.ef.preg.lado == 0 && activables(c.d, 0, true).empty(),
		         "con Siempre no se pregunto al jugador tras la invocacion del rival (pregunta %d lado %d)", c.d.ef.preg.tipo, c.d.ef.preg.lado);
		c.pasarTodo();
		cu::caso("CU-BE-PREGUNTAR-SIEMPRE", c.zonaDe(1, "celtic_guardian") >= 0 && !c.d.ocupado(), "tras pasar el duelo no siguio");
	}
}

// ------------------------------------------------------------------------------------------------- mazos
inline void mazos(const std::string& ui)
{
	mazo::Coleccion col;
	col.leer(ui);
	const mazo::Mazo base = mazo::Mazo::porDefecto(col);
	std::string motivo;
	cu::caso("CU-BE-MAZO-INICIAL", base.valido(col, &motivo), "el mazo inicial no es valido: %s", motivo.c_str());
	mazo::Mazo m;
	for (int k = 0; k < 39; k++) m.principal.push_back(base.principal[(size_t)k]);
	cu::caso("CU-BE-MAZO-MIN-40", !m.valido(col, &motivo), "un mazo de 39 es valido");
	mazo::Mazo g = base;
	while (g.principal.size() < 61) g.principal.push_back("celtic_guardian");
	cu::caso("CU-BE-MAZO-MAX-60", !g.valido(col, &motivo), "un mazo de 61 es valido");
	mazo::Mazo t = base;
	for (int k = 0; k < 4; k++) t.principal.push_back("battle_ox");
	t.principal.erase(t.principal.begin(), t.principal.begin() + 4);
	cu::caso("CU-BE-MAZO-MAX-3", !t.valido(col, &motivo), "un mazo con 4 copias es valido");
	mazo::Mazo f = base;
	for (int k = 0; k < 16; k++) f.fusion.push_back("flame_swordsman");
	cu::caso("CU-BE-MAZO-EXTRA-15", !f.valido(col, &motivo), "un extra de 16 es valido");
	// banlist Goat: prohibida 0, limitada 1, semilimitada 2
	int prohibidas = 0, limitadas = 0, semi = 0;
	for (const mazo::Carta& k : col.cartas) prohibidas += k.limiteBase == 0, limitadas += k.limiteBase == 1, semi += k.limiteBase == 2;
	cu::caso("CU-BE-MAZO-BANLIST", prohibidas > 5 && limitadas > 10 && semi > 3, "la banlist Goat: %d prohibidas, %d limitadas, %d semilimitadas", prohibidas, limitadas, semi);
	const mazo::Carta* pot = col.carta("pot_of_greed");
	cu::caso("CU-BE-MAZO-BANLIST", pot && pot->limiteBase == 1, "Pot of Greed no es limitada");
}

// ------------------------------------------------------------------------------------------------- mazos de torneo
// (rama classic-mazos-torneo; datos/mazos_torneo.txt). La prueba larga (legalidad de cada mazo, duelos y % contra los
// tematicos de nivel 8) es prueba_torneo.cpp; aqui, lo que ve el juego con la base cargada como en el juego
inline void torneo(const Base& base)
{
	cu::caso("CU-BE-TORNEO-JUEGO-CARGA", base.torneos.size() >= 21, "el juego cargo %d mazos de torneo (se esperan 21)", (int)base.torneos.size());
	const Rival* yami = base.rival("yami_yugi");
	const Rival* kaiba = base.rival("kaiba");
	const Rival* cabra = base.rival("cabra");
	cu::caso("CU-BE-TORNEO-JUEGO-CARGA", yami && kaiba && cabra && !yami->torneo.empty() && !kaiba->torneo.empty() && !cabra->torneo.empty(),
	         "Yami Yugi, Kaiba o Cabra sin mazo de torneo");
	int bajos = 0;
	for (const Rival& r : base.rivales)
		if ((r.personaje ? r.nivel < 5 : r.nivel < 7) && !r.torneo.empty() && r.clave != "joey") bajos++;   // (Joey: rama classic-torneo-extras)
	cu::caso("CU-BE-TORNEO-NIVEL-BAJO", bajos == 0, "%d rivales de nivel bajo con mazo de torneo", bajos);
	int salenBajos = 0;
	for (const Rival& r : base.rivales)
		if (r.nivel <= 4 && r.clave != "joey")
			for (uint32_t s = 1; s <= 20; s++) salenBajos += prepararRival(base, r, s * 2654435761u, nullptr).torneo >= 0;
	cu::caso("CU-BE-TORNEO-NIVEL-BAJO", salenBajos == 0, "a un rival de nivel 1-4 le salio un mazo de torneo %d veces", salenBajos);
	if (!cabra) return;

	// sale tal cual y se puede jugar (40 cartas entre mano y mazo, su extra)
	MazoRival m;
	uint32_t sem = 0;
	for (uint32_t s = 1; s <= 400 && m.torneo < 0; s++) m = prepararRival(base, *cabra, s * 7919u, nullptr), sem = s * 7919u;
	cu::caso("CU-BE-TORNEO-TAL-CUAL", m.torneo >= 0, "Cabra nunca saco un mazo de torneo en 400 duelos");
	if (m.torneo < 0) return;
	const MazoTorneo& t = base.torneos[(size_t)m.torneo];
	cu::caso("CU-BE-TORNEO-TAL-CUAL", m.principal == t.principal && m.fusion == t.fusion, "el mazo %s no salio tal cual", t.clave.c_str());
	cu::caso("CU-BE-TORNEO-TAL-CUAL", m.plan >= 0, "el mazo de torneo %s salio sin plan", t.clave.c_str());
	const Rival* rodrigo = base.rival("rodrigo");
	if (rodrigo) {
		Duelo d(&base);
		const MazoRival otro = base.mazoDe(*rodrigo, 5);
		d.empezar(otro.principal, otro.fusion, m.principal, m.fusion, sem);
		cu::caso("CU-BE-TORNEO-TAL-CUAL", d.l[1].mazo.size() + d.l[1].mano.size() == 40 && d.l[1].extra.size() == t.fusion.size(),
		         "el duelo no empezo con el mazo de torneo (%d en mazo+mano, %d en el extra)", (int)(d.l[1].mazo.size() + d.l[1].mano.size()),
		         (int)d.l[1].extra.size());
	}
	// no se repite el mismo dos duelos seguidos (la memoria guarda el ultimo mazo)
	Memoria mem;
	mem.rivales[cabra->clave].mazo = m.principal;
	cu::caso("CU-BE-TORNEO-NO-REPITE", prepararRival(base, *cabra, sem, &mem).torneo != m.torneo, "%s salio dos duelos seguidos", t.clave.c_str());

	// Goat Control: Metamorphosis sobre un nivel 1 trae a Thousand-Eyes Restrict del extra
	{
		Mesa x(&base);
		x.d.l[0].extra = { "thousand_eyes_restrict" };
		const int z = x.mon(0, "magician_of_faith");
		const int k = x.mano(0, "metamorphosis");
		const bool ok = z >= 0 && x.activar(0, H(0, k), { M(0, z) });
		x.pasarTodo();
		cu::caso("CU-BE-TORNEO-METAMORFOSIS", ok && x.zonaDe(0, "thousand_eyes_restrict") >= 0 && x.enCementerio(0, "magician_of_faith") == 1,
		         "Metamorphosis no trajo a Thousand-Eyes Restrict (activo %d)", ok ? 1 : 0);
	}
	// Exodia: la IA no invoca ni descarta las piezas mientras junta las 5
	{
		Mesa x(&base);
		x.d.l[0].mazo = { "left_leg_of_the_forbidden_one", "right_leg_of_the_forbidden_one", "jar_of_greed", "upstart_goblin", "reload", "scapegoat",
		                  "waboku", "gravity_bind" };
		for (const char* id : { "exodia_the_forbidden_one", "left_arm_of_the_forbidden_one", "right_arm_of_the_forbidden_one", "scapegoat", "waboku",
		                        "messenger_of_peace", "upstart_goblin" })
			x.mano(0, id);
		Cerebro c(personalidadPorClave("calculador"), 8, 77);
		c.usarPlan(PL_ALTERNATIVA, Lectura{}, 0, 1);
		std::vector<Jugada> cands;
		c.candidatos(x.d, cands);
		float invocar = -1e9f, pasar = 0.0f;
		for (const Jugada& j : cands) {
			if ((j.tipo == J_INVOCAR || j.tipo == J_COLOCAR) && piezaExodia(j.carta)) invocar = std::max(invocar, j.puntos);
			if (j.tipo == J_FASE) pasar = j.puntos;
		}
		cu::caso("CU-BE-TORNEO-EXODIA-IA", invocar < pasar, "la IA quiere invocar una pieza de Exodia (%.0f contra pasar %.0f)", invocar, pasar);
		x.d.fase = F_FINAL;
		const Jugada dsc = c.decidir(x.d);
		cu::caso("CU-BE-TORNEO-EXODIA-IA", dsc.tipo == J_DESCARTAR && !piezaExodia(dsc.carta), "la IA descarta %s con la mano llena", dsc.carta.c_str());
	}
}

// ------------------------------------------------------------------------------------------------- torneo: extras
// (rama classic-torneo-extras) las 10 cartas que faltaban (cartas extra con su efecto en el motor), las listas exactas
// y Joey con Gearfried y Equipos. Los casos largos de cada carta estan en prueba_efectos.cpp (tandaTorneoExtras)
inline void torneoExtras(const Base& base)
{
	// las listas usan las cartas exactas (no los sustitutos)
	{
		auto tiene = [&](const char* clave, const char* id) {
			for (const MazoTorneo& t : base.torneos)
				if (t.clave == clave) return std::find(t.principal.begin(), t.principal.end(), id) != t.principal.end();
			return false;
		};
		struct E { const char* mazo; const char* carta; };
		static const E ESPERADAS[] = { { "md_sirvientes", "foolish_burial" },       { "md_guerreros_unidos", "command_knight" },
			                           { "md_guerreros_unidos", "obnoxious_celtic_guard" }, { "guerreros_toolbox", "command_knight" },
			                           { "guerreros_toolbox", "obnoxious_celtic_guard" }, { "zombis", "goblin_zombie" },
			                           { "zombis", "il_blud" },                    { "gearfried_equipo", "command_knight" },
			                           { "maquinas", "cyber_dragon" },             { "maquinas", "drillroid" },
			                           { "espiritus", "izanagi" },                 { "espiritus", "yamato_no_kami" },
			                           { "exodia_biblioteca", "magical_mallet" } };
		int faltan = 0;
		std::string cuales;
		for (const E& e : ESPERADAS)
			if (!tiene(e.mazo, e.carta)) faltan++, cuales += std::string(" ") + e.mazo + ":" + e.carta;
		cu::caso("CU-BE-TORNEO-EXACTAS", faltan == 0, "%d cartas exactas que faltan en las listas:%s", faltan, cuales.c_str());
		const bool sust = tiene("zombis", "double_coston") || tiene("maquinas", "drillago") || tiene("md_guerreros_unidos", "d_d_survivor") ||
		                  tiene("guerreros_toolbox", "blindly_loyal_goblin");
		cu::caso("CU-BE-TORNEO-EXACTAS", !sust, "una lista sigue con un sustituto (Double Coston, Drillago, D.D. Survivor o Blindly Loyal Goblin)");
		int sinCarta = 0;
		for (const char* id : { "command_knight", "cyber_dragon", "foolish_burial", "goblin_zombie", "il_blud", "magical_mallet", "izanagi", "yamato_no_kami",
		                        "obnoxious_celtic_guard", "drillroid" })
			sinCarta += base.carta(id) == nullptr || defCarta(id) == nullptr;
		cu::caso("CU-BE-TORNEO-EXACTAS", sinCarta == 0, "%d de las 10 cartas extra sin datos o sin efecto en el motor", sinCarta);
		const Carta* fb = base.carta("foolish_burial");
		cu::caso("CU-BE-TORNEO-EXACTAS", fb && fb->limite == 1, "Foolish Burial deberia ser limitada (1)");
	}
	// Joey (nivel 4) lleva Gearfried y Equipos (decision del usuario) y le sale tal cual
	{
		const Rival* joey = base.rival("joey");
		int k = -1;
		for (size_t i = 0; i < base.torneos.size(); i++)
			if (base.torneos[i].clave == "gearfried_equipo") k = (int)i;
		const bool asignado = joey && k >= 0 && std::find(joey->torneo.begin(), joey->torneo.end(), k) != joey->torneo.end();
		cu::caso("CU-BE-TORNEO-JOEY", asignado, "Joey no tiene el mazo de torneo Gearfried y Equipos");
		if (asignado) {
			int sale = 0, talCual = 0;
			for (uint32_t s = 1; s <= 60; s++) {
				const MazoRival m = prepararRival(base, *joey, s * 7919u, nullptr);
				if (m.torneo != k) continue;
				sale++;
				talCual += m.principal == base.torneos[(size_t)k].principal;
			}
			cu::caso("CU-BE-TORNEO-JOEY", sale > 0 && talCual == sale, "en 60 duelos a Joey le salio %d veces (tal cual %d)", sale, talCual);
		}
	}
	// Command Knight: +400 a tus Guerreros; con otro monstruo no se le ataca
	{
		Mesa x(&base, F_BATALLA);
		const int a = x.mon(0, "summoned_skull");
		const int ck = x.mon(1, "command_knight");
		x.mon(1, "celtic_guardian");
		const bool bloqueado = !x.jugar(J_ATACAR, -1, a, {}, ck);
		cu::caso("CU-BE-EXTRA-COMMAND-KNIGHT", x.d.l[1].m[(size_t)ck].atkEf == 1600 && bloqueado, "ATK %d, se le pudo atacar %d", x.d.l[1].m[(size_t)ck].atkEf, bloqueado ? 0 : 1);
	}
	// Cyber Dragon: desde la mano si solo el rival controla monstruos
	{
		Mesa x(&base);
		x.mon(1, "battle_ox");
		const bool ok = x.activar(0, H(0, x.mano(0, "cyber_dragon")));
		x.pasarTodo();
		cu::caso("CU-BE-EXTRA-CYBER-DRAGON", ok && x.zonaDe(0, "cyber_dragon") >= 0, "Cyber Dragon no se invoco de forma especial");
	}
	// Foolish Burial: un monstruo del mazo al cementerio
	{
		Mesa x(&base);
		x.d.l[0].mazo = { "skull_servant" };
		const bool ok = x.activar(0, H(0, x.mano(0, "foolish_burial")));
		x.pasarTodo();
		cu::caso("CU-BE-EXTRA-FOOLISH-BURIAL", ok && x.enCementerio(0, "skull_servant") == 1 && x.d.l[0].mazo.empty(), "no mando el monstruo del mazo al cementerio");
	}
	// Goblin Zombie: destruido, busca un Zombi de 1200 DEF o menos
	{
		Mesa x(&base);
		x.mon(1, "goblin_zombie");
		x.d.l[1].mazo = { "spirit_reaper", "vampire_lord" };
		const bool ok = x.activar(0, H(0, x.mano(0, "raigeki")));
		x.pasarTodo();
		cu::caso("CU-BE-EXTRA-GOBLIN-ZOMBIE", ok && x.enMano(1, "spirit_reaper") && !x.enMano(1, "vampire_lord"), "no busco el Zombi de 1200 DEF o menos");
	}
	// Il Blud: la regla Geminis (invocarlo de nuevo gasta la invocacion normal) y su efecto
	{
		Mesa x(&base);
		const int z = x.mon(0, "il_blud");
		x.d.l[0].cementerio = { "vampire_lord" };
		const bool antes = motivoNoActivar(x.d, 0, M(0, z), false).empty() && !geminisActivo(x.d.l[0].m[(size_t)z]);
		const bool gem = x.d.aplicar(jugadaActivar(M(0, z)));
		x.pasarTodo();
		cu::caso("CU-BE-GEMINIS-REGLA", antes && gem && geminisActivo(x.d.l[0].m[(size_t)z]) && x.d.l[0].invocoNormal,
		         "la invocacion de Geminis no funciono o no gasto la normal del turno");
		const bool ef = x.d.aplicar(jugadaActivar(M(0, z)));
		x.pasarTodo();
		const int v = x.zonaDe(0, "vampire_lord");
		x.d.alCementerio(0, z);
		x.pasarTodo();
		cu::caso("CU-BE-EXTRA-IL-BLUD", ef && v >= 0 && x.zonaDe(0, "vampire_lord") < 0, "Il Blud no invoco al Zombi o no se destruyo al irse");
	}
	// Magical Mallet: elegir cartas de la mano (opcional, una a una), al mazo y robar las mismas
	{
		Mesa x(&base);
		const int k = x.mano(0, "magical_mallet");
		x.mano(0, "kuriboh");
		x.mano(0, "dark_hole");
		const bool ok = x.activar(0, H(0, k));
		pasarRival(x);
		const bool pregunta = x.d.ef.preg.tipo == P_ELEGIR && x.d.ef.preg.opcional;
		x.pasarTodo1();   // (elige la primera)
		Jugada no;
		no.tipo = J_PASAR;
		x.d.aplicar(no);   // (ya no mas)
		x.pasarTodo();
		cu::caso("CU-BE-EXTRA-MAGICAL-MALLET", ok && pregunta && x.d.l[0].mano.size() == 2 && x.d.l[0].mazo.size() == 20, "mano %d mazo %d",
		         (int)x.d.l[0].mano.size(), (int)x.d.l[0].mazo.size());
	}
	// Izanagi: desde la mano desterrando un Spirit de la mano
	{
		Mesa x(&base);
		const int k = x.mano(0, "izanagi");
		x.mano(0, "tsukuyomi");
		const bool ok = x.activar(0, H(0, k));
		x.pasarTodo();
		cu::caso("CU-BE-EXTRA-IZANAGI", ok && x.zonaDe(0, "izanagi") >= 0 && x.d.l[0].desterrados.size() == 1, "Izanagi no se invoco desterrando el Spirit");
		cu::caso("CU-BE-EXTRA-IZANAGI", izanagiDe(x.d, 0) >= 0, "con Izanagi en el campo los Spirit no tienen opcion de quedarse");
	}
	// Yamato-no-Kami: no se invoca normal; desterrando un Spirit del cementerio; vuelve a la mano al final del turno
	{
		Mesa x(&base);
		x.d.l[0].cementerio = { "asura_priest" };
		const int k = x.mano(0, "yamato_no_kami");
		const bool normal = x.jugar(J_INVOCAR, k);
		const bool ok = x.activar(0, H(0, k));
		x.pasarTodo();
		const int z = x.zonaDe(0, "yamato_no_kami");
		cu::caso("CU-BE-EXTRA-YAMATO", !normal && ok && z >= 0 && x.d.l[0].m[(size_t)z].vuelveFinTurno && x.d.l[0].desterrados.size() == 1,
		         "Yamato-no-Kami: normal %d, procedimiento %d, vuelve %d", normal ? 1 : 0, ok ? 1 : 0, z >= 0 && x.d.l[0].m[(size_t)z].vuelveFinTurno ? 1 : 0);
	}
	// Obnoxious Celtic Guard y Drillroid
	{
		Mesa x(&base, F_BATALLA);
		const int a = x.mon(0, "summoned_skull");
		const int o = x.mon(1, "obnoxious_celtic_guard");
		x.jugar(J_ATACAR, -1, a, {}, o);
		x.pasarTodo();
		cu::caso("CU-BE-EXTRA-CELTIC-GUARD", x.zonaDe(1, "obnoxious_celtic_guard") >= 0, "un monstruo de 2500 ATK lo destruyo en batalla");
		Mesa y(&base, F_BATALLA);
		const int b = y.mon(0, "drillroid");
		const int p = y.mon(1, "big_shield_gardna", true, true);
		y.jugar(J_ATACAR, -1, b, {}, p);
		y.pasarTodo();
		cu::caso("CU-BE-EXTRA-DRILLROID", y.zonaDe(1, "big_shield_gardna") < 0 && y.d.l[0].lp == LP_INICIAL, "Drillroid no destruyo al de defensa antes del calculo");
	}
}


// ------------------------------------------------------------------------------------------------- primer turno en la PS5
// (rama classic-ps5-arreglos1) lo que paso en el primer turno del usuario contra Tea, en el motor:
//  - CU-BE-PS5-TRAP-HOLE: invocacion normal en ataque, el rival responde con Trap Hole: el monstruo va al cementerio del
//    jugador, la trampa al del rival, el duelo queda libre (sin preguntas) y la Principal 1 pasa a la 2 y a la Final
//  - CU-BE-PS5-SIN-CAMBIO-POSICION: invocado en ataque, sin respuesta del rival sigue en ataque (nada lo pone en defensa)
//  - CU-BE-PS5-NUVIA: Nuvia the Wicked invocada normal se destruye sola y el turno sigue normal
inline void primerTurnoPS5(const Base& base)
{
	{
		Mesa m(&base);
		m.d.turno = 1;
		const int th = m.colocar(1, "trap_hole", 0);
		const int k = m.mano(0, "luster_dragon");
		const bool inv = m.jugar(J_INVOCAR, k, 2);
		const int z = m.zonaDe(0, "luster_dragon");
		cu::caso("CU-BE-PS5-SIN-CAMBIO-POSICION", inv && z >= 0 && !m.d.l[0].m[(size_t)z].defensa && !m.d.l[0].m[(size_t)z].bocaAbajo,
		         "la invocacion normal no dejo a Luster Dragon boca arriba en ataque");
		const bool ventana = m.d.ef.preg.tipo == P_RESPUESTA && m.d.ef.preg.lado == 1;
		bool act = false;
		if (ventana) {
			Jugada j = jugadaActivar(MT(1, th));
			if (const DefCarta* df = defCarta("trap_hole")) j.objs = objetivosDe(m.d, 1, *df, MT(1, th));
			if (j.objs.size() > 1) j.objs.resize(1);
			act = m.d.aplicar(j);
		}
		m.pasarTodo();
		cu::caso("CU-BE-PS5-TRAP-HOLE", ventana && act, "el rival no tuvo ventana o no pudo activar Trap Hole tras la invocacion normal (ventana %d)", (int)ventana);
		cu::caso("CU-BE-PS5-TRAP-HOLE", m.enCementerio(0, "luster_dragon") == 1 && m.enCementerio(1, "trap_hole") == 1 && m.zonaDe(0, "luster_dragon") < 0,
		         "tras Trap Hole: Luster Dragon %d en el cementerio, Trap Hole %d", m.enCementerio(0, "luster_dragon"), m.enCementerio(1, "trap_hole"));
		cu::caso("CU-BE-PS5-TRAP-HOLE", !m.d.ocupado() && m.d.decide() == 0, "tras Trap Hole el duelo sigue ocupado (pregunta %d lado %d)", (int)m.d.ef.preg.tipo, m.d.ef.preg.lado);
		bool posicion = false;
		for (const Suceso& s : m.d.sucesos) posicion |= s.tipo == S_POSICION && s.lado == 0;
		cu::caso("CU-BE-PS5-SIN-CAMBIO-POSICION", !posicion, "una carta del jugador cambio de posicion sin que la tocara");
		const bool f1 = m.jugar(J_FASE);
		pasarRival(m);
		const Fase tras1 = m.d.fase;
		const bool f2 = m.jugar(J_FASE);
		pasarRival(m);
		cu::caso("CU-BE-PS5-TRAP-HOLE", f1 && tras1 == F_PRINCIPAL2 && f2 && (m.d.fase == F_FINAL || m.d.activo == 1),
		         "tras Trap Hole el turno 1 no pasa a la Principal 2 y a la Final (fase %d -> %d)", (int)tras1, (int)m.d.fase);
	}
	{
		Mesa m(&base);
		const int k = m.mano(0, "luster_dragon");
		m.jugar(J_INVOCAR, k, 1);
		pasarRival(m);
		const int z = m.zonaDe(0, "luster_dragon");
		cu::caso("CU-BE-PS5-SIN-CAMBIO-POSICION", z >= 0 && !m.d.l[0].m[(size_t)z].defensa, "sin respuesta del rival Luster Dragon no sigue en ataque");
	}
	{
		Mesa m(&base);
		const int k = m.mano(0, "nuvia_the_wicked");
		const bool inv = m.jugar(J_INVOCAR, k, 2);
		m.pasarTodo();
		cu::caso("CU-BE-PS5-NUVIA", inv && m.zonaDe(0, "nuvia_the_wicked") < 0 && m.enCementerio(0, "nuvia_the_wicked") == 1, "Nuvia invocada normal no se destruyo sola");
		const Fase f0 = m.d.fase;
		const bool f = m.jugar(J_FASE);
		pasarRival(m);
		cu::caso("CU-BE-PS5-NUVIA", f && m.d.fase != f0 && !m.d.ocupado(), "tras Nuvia el jugador no puede pasar de fase (fase %d -> %d)", (int)f0, (int)m.d.fase);
	}
}

}  // namespace cub
