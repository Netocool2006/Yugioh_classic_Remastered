// Rama classic-md-entradas: casos de las entradas de FUSION y RITUAL de Master Duel (entrada_md*.h) y del menu del duelo
// (engranaje) / fin del duelo de MD (menu_md_ui.h), en el juego sin GPU de cu_juego.h. Modo "ritmo" (y dentro de "extra").
//  - CU-FT-ANIM-FUSION-SECUENCIA: Polymerization (Dark Magician + Buster Blader -> Dark Paladin): arranca la secuencia de
//    MD con sus 2 materiales, la carta no esta en su zona mientras dura y llega al final; sus sonidos en orden.
//  - CU-FT-ANIM-FUSION-TIEMPO: el duelo espera hasta el StartCard de MD (1.333 + 2.817 s) y no mas de 1.5 s despues.
//  - CU-FT-ANIM-FUSION-RAPIDA: con la velocidad Rapida la misma secuencia dura menos cuadros.
//  - CU-FT-ANIM-FUSION-SIN-MATERIALES: una fusion invocada sin sus materiales (como Monster Reborn) no repite la secuencia.
//  - CU-FT-ANIM-FUSION-CLAVE: una fusion que es carta clave corta la secuencia en el StrongSummon y su entrada de carta
//    clave sigue sin repetir el vortice (entrada::Estado::trasMD).
//  - CU-FT-ANIM-RITUAL-SECUENCIA / -TIEMPO: Black Luster Ritual: Ritual01 (con sacrificios), StartCard a 5.2 s, sonidos.
//  - CU-FT-ANIM-RITUAL-SIN: sin materiales, Ritual02 (StartCard a 2.833 s).
//  - CU-FT-ANIM-RITUAL-RIVAL: el ritual del rival tambien la hace (y del lado del rival).
//  - CU-FT-ENGRANAJE-MD-SONIDO: el engranaje abre el menu del duelo con SE_DUEL_SELECT (sfx_cursor) y cierra con el de MD.
//  - CU-FT-ENGRANAJE-MD-CONTENIDO: el contenido del menu sigue siendo el nuestro.
//  - CU-FT-ENGRANAJE-MD-TWEENS: los Tween de SettingMenuUI (Push 0.3/0.5 s QuartOut desde x 240 y giro -15; Pop 0.2 s).
//  - CU-FT-ENGRANAJE-MD-FIN: DuelEndTransition (radio 3.67 -> 0.15 a los 0.7 s -> 3.70 a 1.25 s; el resultado a 0.717 s).
#pragma once

struct MedidaEntrada {
	bool arranco{ false }, llego{ false }, zonaVaciaDurante{ true }, ladoOk{ true };
	int tipo{ 0 }, materiales{ 0 }, cuadrosActiva{ 0 };
	float espera{ 0.0f }, strong{ 0.0f }, tLlega{ -1.0f }, tTotal{ 0.0f }, tArranca{ 0.0f };
	bool conMateriales{ true }, clave{ false }, trasMD{ false };
	std::vector<std::string> sfx;   // los de la secuencia, en orden
};

static MedidaEntrada medirEntrada(Juego& g, int zonaMesa, const std::string& carta, int lado, float tope = 14.0f)
{
	ia::Duelo& d = g.dueloIA;
	MedidaEntrada r;
	const size_t s0 = g.sonido.sfxs.size();
	for (float t = 0.0f; t < tope; t += g.frameTimer) {
		bool r0 = false;
		contestarRitmo(g, nullptr, r0);
		g.cuadro(0);
		r.tTotal = t;
		const entradamd::Estado& e = entradamd::estado();
		if (e.activa && e.carta == carta) {
			if (!r.arranco) {
				r.arranco = true;
				r.tArranca = t;   // (rama classic-arreglo-ciclo-md: el tiempo se mide desde la secuencia de MD; antes puede ir
				                  // la activacion de la magia -Polymerization / la ritual desde la mano, como en MD-)
				r.tipo = (int)e.tipo;
				r.materiales = (int)e.materiales.size();
				r.espera = e.espera;
				r.strong = e.strong;
				r.conMateriales = e.conMateriales;
				r.clave = e.clave;
				r.ladoOk = e.lado == lado;
			}
			r.cuadrosActiva++;
			const control::Celda& c = g.ctl.zonas[(size_t)zonaMesa];
			if (e.t < e.espera - 0.05f && !c.vacia() && c.carta == carta) r.zonaVaciaDurante = false;
		}
		if (entrada::estado().carta == carta) r.trasMD = r.trasMD || entrada::estado().trasMD;
		const control::Celda& c = g.ctl.zonas[(size_t)zonaMesa];
		if (r.tLlega < 0.0f && !c.vacia() && c.carta == carta && !entradamd::estado().activa) r.tLlega = t - r.tArranca;
		const bool alDia = g.sucesosVistos >= d.sucesos.size() && !ritmo::estado().ocupado() && g.esperaEfecto <= 0.0f && !g.hayAtaque() && !g.ctl.robo.activo;
		if (alDia && d.ef.preg.tipo == ia::P_NADA && !entradamd::estado().activa && !entrada::estado().activa && t > 0.5f) break;
	}
	r.llego = !g.ctl.zonas[(size_t)zonaMesa].vacia() && g.ctl.zonas[(size_t)zonaMesa].carta == carta;
	for (size_t k = s0; k < g.sonido.sfxs.size(); k++)
		if (g.sonido.sfxs[k].rfind("sfx_md_", 0) == 0) r.sfx.push_back(g.sonido.sfxs[k]);
	return r;
}

// la mesa: fase principal 1 del lado que juega (sin la IA del rival)
static cub::Mesa mesaEntradas(Juego& g, int lado)
{
	cub::Mesa m(&g.baseIA, ia::F_PRINCIPAL1);
	m.d.turno = 4;
	m.d.activo = lado;
	return m;
}
static bool enOrden(const std::vector<std::string>& v, std::initializer_list<const char*> q)
{
	size_t i = 0;
	for (const std::string& x : v)
		if (i < q.size() && x == *(q.begin() + i)) i++;
	return i == q.size();
}
static std::string unir(const std::vector<std::string>& v)
{
	std::string r;
	for (const std::string& x : v) r += (r.empty() ? "" : ",") + x;
	return r;
}

// una fusion con Polymerization (materiales de la mano) del lado "lado"
static MedidaEntrada fusionarMD(Bot& b, int lado, const std::string& fusion, bool* ok = nullptr)
{
	Juego& g = b.g;
	cub::Mesa m = mesaEntradas(g, lado);
	ia::Jugada j;
	j.tipo = ia::J_FUSION;
	j.carta = fusion;
	j.zona = 2;
	m.d.l[(size_t)lado].extra = { fusion };
	j.mano = m.mano(lado, "polymerization");
	auto it = g.baseIA.fusiones.find(fusion);
	if (it != g.baseIA.fusiones.end())
		for (const std::string& x : it->second) j.manoExtra.push_back(m.mano(lado, x));
	ponerRitmo(b, m.d);
	const bool a = g.dueloIA.aplicar(j);
	if (ok) *ok = a;
	return medirEntrada(g, Juego::zonaMesaIA(lado, 2), fusion, lado);
}
static MedidaEntrada ritualMD(Bot& b, int lado, const std::string& ritual, bool* ok = nullptr)
{
	Juego& g = b.g;
	cub::Mesa m = mesaEntradas(g, lado);
	ia::Jugada j;
	j.tipo = ia::J_RITUAL;
	j.carta = ritual;
	j.zona = 2;
	auto it = g.baseIA.rituales.find(ritual);
	j.mano = m.mano(lado, it != g.baseIA.rituales.end() ? it->second : std::string("black_luster_ritual"));
	j.mano2 = m.mano(lado, ritual);
	j.manoExtra = { m.mano(lado, "summoned_skull"), m.mano(lado, "celtic_guardian") };
	ponerRitmo(b, m.d);
	const bool a = g.dueloIA.aplicar(j);
	if (ok) *ok = a;
	return medirEntrada(g, Juego::zonaMesaIA(lado, 2), ritual, lado);
}

static void entradasMD(Bot& b)
{
	Juego& g = b.g;
	if (g.ctl.vista.pantalla != VD::DUELO && !nuevoDuelo(b)) {
		b.caso("CU-FT-ANIM-FUSION-SECUENCIA", false, "no se pudo empezar el duelo para las pruebas de las entradas");
		return;
	}
	b.rivalPorBot = true;
	b.grabar = false;
	b.duelo = "entradas-md";
	velocidad::poner(velocidad::NORMAL);
	g.leerVelocidad();
	entrada::estado() = entrada::Estado{};
	const float startF = entradamd::MUESTRA + entradamd::F_START;

	// fusion del jugador
	{
		bool ok = false;
		const MedidaEntrada r = fusionarMD(b, 0, "dark_paladin", &ok);
		const char* C = "CU-FT-ANIM-FUSION-SECUENCIA";
		b.caso(C, ok, "no se pudo hacer la fusion (Polymerization + Dark Magician + Buster Blader)");
		b.caso(C, r.arranco && r.tipo == entradamd::FUSION && r.materiales == 2, "no arranco la fusion de MD (arranco %d, tipo %d, materiales %d)", (int)r.arranco, r.tipo,
		       r.materiales);
		b.caso(C, r.zonaVaciaDurante, "Dark Paladin ya estaba en su zona durante la secuencia");
		b.caso(C, r.llego, "Dark Paladin no quedo en su zona al terminar");
		b.caso(C, enOrden(r.sfx, { "sfx_md_carta", "sfx_md_fusion_1", "sfx_md_fusion_2" }), "sonidos fuera de orden: %s", unir(r.sfx).c_str());
		b.caso("CU-FT-ANIM-FUSION-TIEMPO", fabsf(r.espera - startF) < 0.01f && r.tLlega >= startF - 0.05f && r.tLlega <= startF + 1.5f,
		       "la carta llego a los %.2f s (StartCard de MD %.2f s; espera %.2f)", r.tLlega, startF, r.espera);
		// con la velocidad Rapida
		velocidad::poner(velocidad::RAPIDA);
		g.leerVelocidad();
		const MedidaEntrada rr = fusionarMD(b, 0, "dark_paladin");
		velocidad::poner(velocidad::NORMAL);
		g.leerVelocidad();
		b.caso("CU-FT-ANIM-FUSION-RAPIDA", rr.arranco && rr.cuadrosActiva > 0 && rr.cuadrosActiva < r.cuadrosActiva, "con la velocidad Rapida no se acorto (%d cuadros contra %d)",
		       rr.cuadrosActiva, r.cuadrosActiva);
	}
	// una fusion que llega sin sus materiales (Monster Reborn, invocacion especial): sin la secuencia
	{
		cub::Mesa m = mesaEntradas(g, 0);
		ponerRitmo(b, m.d);
		const int z = g.dueloIA.invocarEspecial(0, "dark_paladin", false);
		const MedidaEntrada r = medirEntrada(g, Juego::zonaMesaIA(0, z < 0 ? 2 : z), "dark_paladin", 0, 6.0f);
		b.caso("CU-FT-ANIM-FUSION-SIN-MATERIALES", z >= 0 && !r.arranco && r.llego, "la invocacion especial sin materiales hizo la secuencia de fusion (zona %d, arranco %d)", z,
		       (int)r.arranco);
	}
	// una fusion que es carta clave: corta en el StrongSummon y sigue su entrada (sin repetir el vortice)
	{
		std::string clave;
		for (const auto& kv : g.baseIA.fusiones)
			if (g.esCartaClave(kv.first) && (clave.empty() || kv.first < clave)) clave = kv.first;
		if (clave.empty()) {
			b.caso("CU-FT-ANIM-FUSION-CLAVE", true, "(sin fusiones clave en este arnes)");
		} else {
			bool ok = false;
			const MedidaEntrada r = fusionarMD(b, 0, clave, &ok);
			b.caso("CU-FT-ANIM-FUSION-CLAVE", ok && r.arranco && r.clave && fabsf(r.espera - r.strong) < 0.01f,
			       "%s: la secuencia no corto en el StrongSummon (ok %d, arranco %d, clave %d, espera %.2f, strong %.2f)", clave.c_str(), (int)ok, (int)r.arranco, (int)r.clave, r.espera,
			       r.strong);
			b.caso("CU-FT-ANIM-FUSION-CLAVE", r.trasMD && r.llego, "%s: la entrada de carta clave no siguio a la de MD (trasMD %d, llego %d)", clave.c_str(), (int)r.trasMD, (int)r.llego);
		}
	}
	// ritual del jugador (Ritual01) y sin materiales (Ritual02)
	{
		bool ok = false;
		const MedidaEntrada r = ritualMD(b, 0, "black_luster_soldier", &ok);
		const char* C = "CU-FT-ANIM-RITUAL-SECUENCIA";
		b.caso(C, ok, "no se pudo hacer el ritual de Black Luster Soldier");
		b.caso(C, r.arranco && r.tipo == entradamd::RITUAL && r.conMateriales && r.materiales == 2, "no arranco el ritual de MD (arranco %d, tipo %d, materiales %d)", (int)r.arranco,
		       r.tipo, r.materiales);
		b.caso(C, r.zonaVaciaDurante && r.llego, "el monstruo ritual aparecio antes o no llego (vacia %d, llego %d)", (int)r.zonaVaciaDurante, (int)r.llego);
		if (!r.clave)
			b.caso(C, enOrden(r.sfx, { "sfx_md_carta", "sfx_md_ritual_1", "sfx_md_ritual_2", "sfx_md_ritual_3", "sfx_md_ritual_4" }), "sonidos fuera de orden: %s", unir(r.sfx).c_str());
		const float st = r.clave ? entradamd::RITUAL01.strong : entradamd::RITUAL01.start;
		b.caso("CU-FT-ANIM-RITUAL-TIEMPO", fabsf(r.espera - st) < 0.01f && r.tLlega >= st - 0.05f && r.tLlega <= st + 1.5f + (r.clave ? 6.0f : 0.0f),
		       "el monstruo ritual llego a los %.2f s (MD %.2f s)", r.tLlega, st);
		setenv("YUGI_ENTRADA_MD_SIN", "1", 1);
		const MedidaEntrada r2 = ritualMD(b, 0, "black_luster_soldier");
		unsetenv("YUGI_ENTRADA_MD_SIN");
		const float st2 = r2.clave ? entradamd::RITUAL02.strong : entradamd::RITUAL02.start;
		b.caso("CU-FT-ANIM-RITUAL-SIN", r2.arranco && !r2.conMateriales && fabsf(r2.espera - st2) < 0.01f, "sin materiales no uso Ritual02 (con %d, espera %.2f, MD %.2f)",
		       (int)r2.conMateriales, r2.espera, st2);
	}
	// ritual del rival
	{
		bool ok = false;
		const MedidaEntrada r = ritualMD(b, 1, "black_luster_soldier", &ok);
		b.caso("CU-FT-ANIM-RITUAL-RIVAL", ok && r.arranco && r.ladoOk && r.tipo == entradamd::RITUAL && r.llego, "el ritual del rival no hizo la secuencia (ok %d, arranco %d, lado %d, llego %d)",
		       (int)ok, (int)r.arranco, (int)r.ladoOk, (int)r.llego);
	}

	// ---- el menu del duelo (engranaje) y el fin del duelo de MD
	{
		cub::Mesa m = mesaEntradas(g, 0);
		ponerRitmo(b, m.d);
		b.esperarQue(8.0f, [&] { return b.libre(); });
		const size_t s0 = g.sonido.sfxs.size();
		b.apretar(PAD_TOUCH_PAD, 0.2f);
		bool abrio = false;
		for (size_t k = s0; k < g.sonido.sfxs.size(); k++) abrio = abrio || g.sonido.sfxs[k] == "sfx_cursor";
		b.caso("CU-FT-ENGRANAJE-MD-SONIDO", g.pausa.abierta && abrio, "el engranaje no abrio el menu con SE_DUEL_SELECT (abierta %d, sonido %d)", (int)g.pausa.abierta, (int)abrio);
		const std::vector<int> ops = g.opcionesPausa();
		const std::vector<int> esperadas = { Juego::PA_CONTINUAR, Juego::PA_OPCIONES, Juego::PA_CONSTRUIR, Juego::PA_ABANDONAR, Juego::PA_MENU_PRINCIPAL };
		b.caso("CU-FT-ENGRANAJE-MD-CONTENIDO", ops == esperadas, "el contenido del menu del duelo cambio (%d opciones)", (int)ops.size());
		const size_t s1 = g.sonido.sfxs.size();
		b.apretar(PAD_CIRCLE, 0.2f);
		bool cerro = false;
		for (size_t k = s1; k < g.sonido.sfxs.size(); k++) cerro = cerro || g.sonido.sfxs[k] == "sfx_menu_cancelar";
		b.caso("CU-FT-ENGRANAJE-MD-SONIDO", !g.pausa.abierta && cerro, "circulo no cerro el menu con su sonido (abierta %d, sonido %d)", (int)g.pausa.abierta, (int)cerro);
	}
	{
		clipsmd::Biblioteca& bi = clipsmd::biblioteca();
		if (!bi.prefab("SettingMenuUI")) bi.cargar(g.carpetaAssets + "md_ui/");
		const clipsmd::Prefab* p = bi.prefab("SettingMenuUI");
		if (!p) {
			b.caso("CU-FT-ENGRANAJE-MD-TWEENS", true, "(sin md_ui/SettingMenuUI.mdui en este arnes: valores escritos en menu_md_ui.h)");
		} else {
			const clipsmd::Tween* velo = p->tween("FullScreen", "Push", "TweenAlpha");
			const clipsmd::Tween* pos = p->tween("SettingMenuArea", "Push", "TweenPosition");
			const clipsmd::Tween* giro = p->tween("SettingMenuArea", "Push", "TweenRotation");
			const clipsmd::Tween* pop = p->tween("SettingMenuArea", "Pop", "TweenPosition");
			const clipsmd::Tween* esq = p->tween("SettingMenuArea/ButtonGroup/SurrenderButton/SelectCursor/Corner", "select", "TweenSize");
			const bool okT = velo && fabsf(velo->dur - 0.3f) < 1e-3f && pos && fabsf(pos->dur - 0.5f) < 1e-3f && pos->easing == clipsmd::QUART_OUT && pos->desde.size() >= 3 &&
			                 fabsf(pos->desde[0] - 240.0f) < 1e-3f && fabsf(pos->desde[2] - 360.0f) < 1e-3f && giro && giro->desde.size() >= 2 &&
			                 fabsf(giro->desde[1] + 15.0f) < 1e-3f && pop && fabsf(pop->dur - 0.2f) < 1e-3f && pop->easing == clipsmd::CUBIC_IN && esq && fabsf(esq->dur - 0.2f) < 1e-3f;
			b.caso("CU-FT-ENGRANAJE-MD-TWEENS", okT, "los Tween de SettingMenuUI no son los esperados (Push 0.3/0.5 QuartOut desde 240,0,360 y -15; Pop 0.2 CubicIn; esquinas 0.2)");
		}
		const clipsmd::Timeline* tl = bi.timeline("DuelEndTransition");
		if (!tl) {
			b.caso("CU-FT-ENGRANAJE-MD-FIN", true, "(sin md_ui/DuelEndTransition.mdtl en este arnes: curva escrita en menu_md_ui.h)");
		} else {
			const float r0 = tl->valor("BG", "attr2170181182", 0.0f, -1.0f), r1 = tl->valor("BG", "attr2170181182", 0.7f, -1.0f),
			            r2 = tl->valor("BG", "attr2170181182", 1.25f, -1.0f);
			b.caso("CU-FT-ENGRANAJE-MD-FIN", fabsf(tl->duracion - 1.25f) < 0.01f && fabsf(r0 - 3.671f) < 0.01f && fabsf(r1 - 0.15f) < 0.01f && fabsf(r2 - 3.705f) < 0.02f,
			       "DuelEndTransition: duracion %.3f, radio %.3f / %.3f / %.3f (MD 1.25: 3.671 / 0.150 / 3.705)", tl->duracion, r0, r1, r2);
		}
	}
}
