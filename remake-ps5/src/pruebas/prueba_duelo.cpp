// Prueba rapida del duelo real (rama classic-integracion) sin pantalla: el jugador juega SOLO con pulsos del mando
// (control.h en modo duelo: menus, sacrificios, zonas, ataques, fases, descartes) contra la IA (ia::Turno::paso), con
// la mesa sincronizada como en el juego (sincronizarLado). Verifica que las jugadas del mando entran, que no se
// cuelga y que los duelos terminan. Rama classic-efectos: el jugador tambien activa magias con efecto desde el menu
// (costes y objetivos con el foco o la lista) y contesta las preguntas del duelo (responder con una trampa o no,
// elegir al resolver); ninguna cadena puede quedar colgada.
// Compilar y correr (contenedor arch-ps5):
//   g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_duelo.cpp -o /tmp/prueba_duelo &&
//   /tmp/prueba_duelo <carpeta ui/ con cartas_ui.txt y rivales.txt> [duelos]
#include "control.h"

using namespace control;

static ia::Base base;
static ia::Duelo duelo(&base);
static Mando ctl;
static int fallos = 0, jugadasMando = 0;
static ia::Turno* rivalT = nullptr;
static int preguntasJ = 0, activacionesJ = 0, respuestasJ = 0;
static bool atendiendo = false;

static void sincronizar()
{
	ctl.sincronizarLado(false, duelo.l[0]);
	ctl.sincronizarLado(true, duelo.l[1]);
	ctl.manoJ = duelo.l[0].mano;
}
static void atender();
static void pulso(uint32_t b)
{
	ctl.avanzar(0.016f, b);
	for (int k = 0; k < 120 && ctl.vuelo.activo; k++) ctl.avanzar(0.016f, 0);   // la carta que vuela
	ctl.eventos.clear();
	sincronizar();
	if (!atendiendo && !ctl.armando) atender();
}
// rama classic-md-fases: la fase siguiente con el selector de fases (OPTIONS -> la primera valida -> X); con una fase
// ya elegida en camino, un pulso la deja seguir
static void faseMando()
{
	if (ctl.faseObjetivo >= 0) {
		pulso(0);
		return;
	}
	ctl.pulsarOptions();
	pulso(0);
	if (!ctl.selFases.abierto) return;
	int k = 0;
	while (k < Mando::SEL_FASES - 1 && !ctl.selFases.valida[(size_t)k]) k++;
	for (int i = 0; i < 4 && ctl.selFases.opcion != k; i++) pulso(B_DER);
	pulso(B_X);
}
// las preguntas del duelo (rama classic-efectos): las del rival las contesta la IA; las del jugador, el mando (la
// mitad de las veces activa lo primero que ofrece el menu; si no, "No activar")
static void atender()
{
	atendiendo = true;
	for (int g = 0; g < 80 && duelo.ganador < 0 && duelo.ef.preg.tipo != ia::P_NADA; g++) {
		if (duelo.decide() == 1) {
			rivalT->paso();
			sincronizar();
			continue;
		}
		const bool antes = ctl.bloqueado;
		ctl.bloqueado = false;
		if (ctl.estado == NAVEGAR) pulso(0);   // (se abre sola)
		if (ctl.estado == MENU) {
			if (!ctl.armando && !ctl.eleccionDuelo) {
				preguntasJ++;
				const bool activa = ctl.opciones.size() > 1 && (g + preguntasJ) % 2 == 0;
				ctl.opcion = activa ? 0 : (int)ctl.opciones.size() - 1;
				respuestasJ += activa;
			} else {
				ctl.opcion = 0;
			}
			pulso(B_X);
		} else if (ctl.estado == ELEGIR_CARTA) {
			pulso(B_X);
		} else if (ctl.estado == DETALLE) {
			pulso(B_CIRCULO);
		}
		ctl.bloqueado = antes;
	}
	if (duelo.ef.preg.tipo != ia::P_NADA && duelo.ganador < 0) {
		fprintf(stderr, "FALLO: pregunta sin contestar (lado %d, tipo %d, estado %s)\n", duelo.ef.preg.lado, duelo.ef.preg.tipo, NOMBRE_ESTADO[ctl.estado]);
		fallos++;
	}
	atendiendo = false;
}
// abre el menu del foco y elige la opcion con esa accion (si esta activa)
static bool elegir(Foco f, Accion a)
{
	ctl.foco = f;
	ctl.estado = NAVEGAR;
	pulso(B_X);
	if (ctl.estado != MENU) return false;
	for (int i = 0; i < (int)ctl.opciones.size(); i++) {
		if (ctl.opciones[i].accion == a && ctl.opciones[i].activa) {
			ctl.opcion = i;
			pulso(B_X);
			return true;
		}
	}
	pulso(B_CIRCULO);
	return false;
}

static void turnoJugador()
{
	const ia::Lado& x = duelo.l[0];
	// invocar: el primer monstruo que se pueda (con sacrificios si hacen falta)
	for (int k = 0; k < (int)x.mano.size() && !x.invocoNormal; k++) {
		const ia::Carta* c = duelo.carta(x.mano[k]);
		if (!c || !(c->clase == "normal" || c->clase == "efecto")) continue;
		const int nec = ia::Duelo::tributosPara(c);
		if (x.monstruos() < nec) continue;
		const int antes = x.monstruos();
		const int resueltosAntes = duelo.ef.resueltos;
		if (!elegir({ F_MANO_J, k }, AC_INVOCAR)) continue;
		for (int t = 0; t < nec && ctl.estado == ELEGIR_TRIBUTOS; t++) {
			ctl.foco = { F_ZONA, ctl.validas[t] };
			pulso(B_X);
		}
		if (ctl.estado == ELEGIR_ZONA) pulso(B_X);
		// (rama classic-efectos: una respuesta del rival, como Trap Hole, puede destruirlo enseguida)
		// (rama classic-efectos: una respuesta del rival, como Trap Hole o Compulsory Evacuation, se lo puede llevar)
		const bool respondida = duelo.ef.resueltos > resueltosAntes;
		if (!respondida && (!x.invocoNormal || x.monstruos() != antes - nec + 1)) {   // (Ultimate Offering encadenada da otra invocacion)
			fprintf(stderr, "FALLO: la invocacion por mando no entro (%s, %d sacrificios; monstruos %d -> %d, invoco %d, estado %s)\n", c->id.c_str(), nec, antes,
			        x.monstruos(), (int)x.invocoNormal, NOMBRE_ESTADO[ctl.estado]);
			fallos++;
		}
		jugadasMando++;
		break;
	}
	// rama classic-efectos: activar una magia con efecto desde la mano (objetivos/descartes con X)
	for (int k = 0; k < (int)x.mano.size() && duelo.ganador < 0; k++) {
		const ia::Carta* c = duelo.carta(x.mano[k]);
		if (!c || !c->magia || !ia::defCarta(c->id) || !ia::motivoNoActivar(duelo, 0, { 0, ia::EN_MANO, k }, false).empty()) continue;
		const int antes = duelo.ef.resueltos;
		if (!elegir({ F_MANO_J, k }, AC_ACTIVAR_EFECTO)) {
			fprintf(stderr, "FALLO: no se pudo elegir Activar (%s)\n", c->id.c_str());
			fallos++;
			break;
		}
		for (int g = 0; g < 12 && (ctl.estado == ELEGIR_CARTA || (ctl.estado == MENU && ctl.armando)); g++) {
			ctl.opcion = 0;
			pulso(B_X);
		}
		atender();
		if (duelo.ef.resueltos == antes && duelo.ganador < 0) {
			fprintf(stderr, "FALLO: la activacion por mando no se resolvio (%s, estado %s)\n", c->id.c_str(), NOMBRE_ESTADO[ctl.estado]);
			fallos++;
		}
		activacionesJ++;
		jugadasMando++;
		break;
	}
	// una magia o trampa boca abajo
	for (int k = 0; k < (int)x.mano.size(); k++) {
		const ia::Carta* c = duelo.carta(x.mano[k]);
		if (!c || !(c->magia || c->trampa) || x.libreMT() < 0) continue;
		auto colocadas = [&]() { return x.colocadasMT() + (!x.campo.vacia() && x.campo.bocaAbajo ? 1 : 0); };   // (la de campo: su zona)
		const int antes = colocadas();
		const bool campo = c->subtipo == "magia_campo" && !x.campo.vacia() && x.campo.bocaAbajo;   // (reemplaza la colocada)
		if (elegir({ F_MANO_J, k }, AC_COLOCAR_MT) && ctl.estado == ELEGIR_ZONA) pulso(B_X);
		if (colocadas() != antes + (campo ? 0 : 1)) {
			fprintf(stderr, "FALLO: colocar magia/trampa por mando no entro\n");
			fallos++;
		}
		jugadasMando++;
		break;
	}
	// a la batalla
	for (int g = 0; g < 3 && duelo.fase != ia::F_BATALLA && duelo.activo == 0 && duelo.turno > 1; g++) faseMando();
	if (duelo.fase == ia::F_BATALLA) {
		for (int i = 0; i < ia::ZONAS && duelo.ganador < 0; i++) {
			const ia::Monstruo& m = x.m[i];
			if (m.vacia() || m.bocaAbajo || m.defensa || m.ataco || !duelo.puedeAtacar(0, i)) continue;   // (Swords, Gravity Bind...)
			const int z = zonaPorId("jugador_monstruo_" + std::to_string(i));
			duelo.ef.m2Repeticion = -1;   // (rama classic-motor2: si hay repeticion del ataque queda en 0: puede volver a atacar)
			if (!elegir({ F_ZONA, z }, AC_ATACAR)) {
				fprintf(stderr, "FALLO: no se pudo elegir Atacar\n");
				fallos++;
				continue;
			}
			const int uidAntes = x.m[i].uid;   // (rama classic-efectos: si murio y otro llego a su zona, no cuenta)
			if (ctl.estado == ELEGIR_OBJETIVO) pulso(B_X);
			if (!x.m[i].vacia() && x.m[i].uid == uidAntes && !x.m[i].ataco && duelo.ef.m2Repeticion != 0) {
				fprintf(stderr, "FALLO: el ataque por mando no entro (%s, repeticion %d)\n", x.m[i].id.c_str(), duelo.ef.m2Repeticion);
				fallos++;
			}
			jugadasMando++;
		}
	}
	// pasar hasta el turno del rival (descartando si hace falta)
	for (int g = 0; g < 12 && duelo.activo == 0 && duelo.ganador < 0; g++) {
		if (duelo.fase == ia::F_FINAL && (int)x.mano.size() > ia::MAX_MANO) {
			elegir({ F_MANO_J, 0 }, AC_JUGADA);
		} else {
			faseMando();
		}
	}
	if (duelo.activo == 0 && duelo.ganador < 0) {
		fprintf(stderr, "FALLO: el jugador no pudo pasar el turno (fase %d)\n", (int)duelo.fase);
		fallos++;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2 || !base.leer(argv[1])) {
		fprintf(stderr, "uso: prueba_duelo <carpeta ui/> [duelos]\n");
		return 2;
	}
	const int N = argc > 2 ? atoi(argv[2]) : 40;
	ctl.clase = [](const std::string& id) {
		const ia::Carta* c = base.carta(id);
		return c ? c->clase : std::string();
	};
	ctl.duelo = &duelo;
	ctl.demo = false;
	ctl.jugar = [](const ia::Jugada& j) { return duelo.decide() == 0 && duelo.aplicar(j); };
	int gana[3] = { 0, 0, 0 }, colgados = 0;
	for (int n = 0; n < N; n++) {
		const ia::MazoRival mj = base.mazoRival(1 + n % 8, 1000u + n), mr = base.mazoRival(1 + (n * 3) % 8, 77u + n);
		duelo = ia::Duelo(&base);
		duelo.empezar(mj.principal, mj.fusion, mr.principal, mr.fusion, 5u + n);
		ia::Turno rival(&duelo, 1, mr.rival->personalidad(), mr.rival->nivel, 9u + n);
		rivalT = &rival;
		ctl.preparar({}, {}, {});
		sincronizar();
		int vueltas = 0;
		while (duelo.ganador < 0 && vueltas++ < 400) {
			if (duelo.activo == 0) {
				ctl.bloqueado = false;
				turnoJugador();
			} else {
				ctl.bloqueado = true;
				for (int k = 0; k < 80 && duelo.activo == 1 && duelo.ganador < 0; k++) {
					if (duelo.decide() == 1) rival.paso();
					else atender();   // (el jugador responde en el turno del rival)
				}
				sincronizar();
			}
		}
		if (duelo.ganador < 0) colgados++;
		else gana[std::min(duelo.ganador, 2)]++;
		if (rival.rechazadas) {
			fprintf(stderr, "FALLO: la IA tuvo %d jugadas rechazadas\n", rival.rechazadas);
			fallos++;
		}
	}
	printf("duelos %d: gana jugador %d, rival %d, empate %d, colgados %d; jugadas por mando %d\n", N, gana[0], gana[1], gana[2], colgados, jugadasMando);
	printf("efectos: activaciones del jugador por mando %d, preguntas al jugador %d (respondidas activando %d)\n", activacionesJ, preguntasJ, respuestasJ);
	if (colgados) fallos++;
	printf(fallos ? "PRUEBA CON FALLOS (%d)\n" : "PRUEBA OK (%d fallos)\n", fallos);
	return fallos ? 1 : 0;
}
