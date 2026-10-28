// Prueba logica del control con el mando (src/control.h), sin pantalla ni Vulkan: arma la mesa como el juego
// (los monstruos de escena.h; al tomar el control Ojos Rojos y Ojos Azules vuelven a la mano) y recorre el flujo completo con pulsos de botones:
// elegir carta -> menu -> zona -> invocar; colocar boca abajo; atacar; cambiar posicion; voltear; trampa; campo;
// fase; detalle; cementerio. Imprime el registro del mando ([mando] ...) en stderr y verifica los eventos.
// Compilar y correr (contenedor arch-ps5 o cualquier g++ con C++17):
//   g++ -std=c++17 -Wall -Wextra -I remake-ps5/src remake-ps5/src/pruebas/prueba_mando.cpp -o /tmp/prueba_mando && /tmp/prueba_mando
#include "control.h"
// el duelo de prueba: cada monstruo con el centro de su carta (como el tablero de Blender)
struct MonstruoMesa { const char* carta; float x, z; bool rival; };
static const MonstruoMesa MONSTRUOS[] = {
	{ "blue_eyes", -7.56f, 3.08f, false }, { "celtic_guardian", 7.56f, -3.08f, true }, { "dark_magician", 0.0f, 3.08f, false },
	{ "dark_magician_girl", 7.56f, 3.08f, false }, { "deepsea_shark", -3.78f, -3.08f, true }, { "kuriboh", 3.78f, 3.08f, false },
	{ "oni_tank_t_34", 3.78f, -3.08f, true }, { "red_eyes", -3.78f, 3.08f, false }, { "stone_ogre_grotto", -7.56f, -3.08f, true },
	{ "summoned_skull", 0.0f, -3.08f, true },
};

#include <map>

using namespace control;

static int fallos = 0;
#define VERIFICA(c)                                                                 \
	do {                                                                            \
		if (!(c)) {                                                                 \
			fprintf(stderr, "FALLA (linea %d): %s\n", __LINE__, #c);                 \
			fallos++;                                                               \
		} else {                                                                    \
			fprintf(stderr, "ok: %s\n", #c);                                        \
		}                                                                           \
	} while (0)

static const std::map<std::string, std::string> CLASES = {
	{ "red_eyes", "normal" }, { "blue_eyes", "normal" }, { "gaia", "normal" }, { "dark_hole", "magia" }, { "trap_hole", "trampa" },
	{ "forest", "magia" }, { "sogen", "magia" }, { "kuriboh", "efecto" }, { "dark_magician", "normal" }, { "dark_magician_girl", "efecto" },
	{ "celtic_guardian", "normal" }, { "summoned_skull", "normal" }, { "deepsea_shark", "normal" }, { "oni_tank_t_34", "normal" },
	{ "stone_ogre_grotto", "normal" }, { "swords_of_revealing_light", "magia" }, { "monster_reborn", "magia" }, { "polymerization", "magia" },
	{ "mirror_force", "trampa" }, { "magic_cylinder", "trampa" }, { "raigeki", "magia" },
};

static Mando m;
static bool atacando = false;

static void cuadros(int n)
{
	for (int k = 0; k < n; k++) m.avanzar(1.0f / 60.0f, 0);
}
static void pulsar(uint32_t b)
{
	m.avanzar(1.0f / 60.0f, b);
	cuadros(3);
}
static const Evento* ultimo(TipoEvento t)
{
	for (auto it = m.eventos.rbegin(); it != m.eventos.rend(); ++it) {
		if (it->tipo == t) return &*it;
	}
	return nullptr;
}

// la zona donde esta boca arriba esa carta (-1: en la mano, colocada o en el cementerio)
static int zonaDe(const std::string& carta)
{
	for (int z = 0; z < NUM_ZONAS_MESA; z++) {
		if (m.zonas[z].carta == carta && !m.zonas[z].bocaAbajo) return z;
	}
	return -1;
}

int main()
{
	m.log = true;
	m.clase = [](const std::string& id) {
		auto it = CLASES.find(id);
		return it == CLASES.end() ? std::string() : it->second;
	};
	m.ataqueEnCurso = [] { return atacando; };
	std::vector<MonstruoInicial> mons;
	for (size_t i = 0; i < sizeof(MONSTRUOS) / sizeof(MONSTRUOS[0]); i++) {
		mons.push_back({ MONSTRUOS[i].carta, MONSTRUOS[i].x, MONSTRUOS[i].z, MONSTRUOS[i].rival, strcmp(MONSTRUOS[i].carta, "kuriboh") == 0 });
	}
	const std::vector<CartaInicial> cartas = {
		{ "jugador_magia_0", "swords_of_revealing_light", false }, { "jugador_magia_1", "dark_hole", true }, { "jugador_magia_2", "monster_reborn", false },
		{ "jugador_magia_3", "magic_cylinder", true }, { "jugador_magia_4", "polymerization", false },
		{ "rival_magia_0", "magic_cylinder", false }, { "rival_magia_1", "mirror_force", false }, { "rival_magia_2", "", true },
		{ "rival_magia_3", "raigeki", false }, { "rival_magia_4", "", true }, { "jugador_campo", "forest", true },
		{ "jugador_fusion", "flame_swordsman", false },
	};
	m.aLaMano = { "red_eyes", "blue_eyes", "monster_reborn" };
	m.manoInteractiva = { "red_eyes", "blue_eyes", "monster_reborn", "gaia", "trap_hole", "forest" };
	m.campoOcupado = [] { return false; };
	m.preparar(mons, cartas, { "blue_eyes", "gaia", "pot_of_greed", "monster_reborn", "trap_hole", "time_wizard" });

	// la mesa de la demo: todos en el tablero; Kuriboh en defensa en la 3
	VERIFICA(m.zonas[zonaPorId("jugador_monstruo_0")].carta == "blue_eyes");
	VERIFICA(m.zonas[zonaPorId("jugador_monstruo_1")].carta == "red_eyes");
	VERIFICA(m.zonas[zonaPorId("jugador_monstruo_3")].carta == "kuriboh" && m.zonas[zonaPorId("jugador_monstruo_3")].defensa);
	VERIFICA(m.zonas[zonaPorId("rival_monstruo_2")].carta == "summoned_skull");

	// tocar el mando apaga la demo y reparte: Ojos Rojos, Ojos Azules y Monstruo renacido vuelven a la mano
	m.tocar();
	VERIFICA(!m.demo && ultimo(EV_DEMO_APAGADA) && ultimo(EV_A_LA_MANO));
	VERIFICA(m.zonas[zonaPorId("jugador_monstruo_0")].vacia() && m.zonas[zonaPorId("jugador_monstruo_1")].vacia());
	VERIFICA(m.zonas[zonaPorId("jugador_magia_2")].vacia() && m.manoJ.size() == 6);
	VERIFICA(zonaDe("red_eyes") < 0 && zonaDe("blue_eyes") < 0);
	VERIFICA(m.idFoco() == "jugador_mano_0");

	// 1) Ojos Rojos: elegir -> menu -> Invocar -> zona -> vuela -> rayo
	pulsar(B_X);
	VERIFICA(m.estado == MENU && m.opciones[0].accion == AC_INVOCAR && m.opciones[0].activa);
	pulsar(B_X);
	VERIFICA(m.estado == ELEGIR_ZONA && m.validas.size() == 2);
	VERIFICA(m.idFoco() == "jugador_monstruo_1");   // la libre mas cercana al centro
	pulsar(B_IZQ);
	VERIFICA(m.idFoco() == "jugador_monstruo_0");
	pulsar(B_IZQ);                                  // no hay mas zonas libres a la izquierda
	VERIFICA(m.idFoco() == "jugador_monstruo_0");
	pulsar(B_DER);
	VERIFICA(m.idFoco() == "jugador_monstruo_1");
	pulsar(B_X);
	VERIFICA(m.estado == VOLANDO && m.manoJ.size() == 5);
	cuadros(45);
	VERIFICA(m.estado == NAVEGAR);
	const Evento* inv = ultimo(EV_INVOCAR);
	VERIFICA(inv && inv->carta == "red_eyes" && inv->zona == zonaPorId("jugador_monstruo_1"));
	VERIFICA(zonaDe("red_eyes") == zonaPorId("jugador_monstruo_1"));

	// 2) Ojos Azules: colocar boca abajo (de lado, en defensa); su modelo queda escondido
	pulsar(B_ABAJO);   // de la zona de monstruo 1 a la de magia 1 y a la mano: la carta mas cercana en x
	pulsar(B_ABAJO);
	pulsar(B_IZQ);
	VERIFICA(m.idFoco() == "jugador_mano_0" && m.manoJ[0] == "blue_eyes");
	pulsar(B_X);
	pulsar(B_ABAJO);
	VERIFICA(m.opciones[m.opcion].accion == AC_COLOCAR_MONSTRUO);
	pulsar(B_X);
	VERIFICA(m.estado == ELEGIR_ZONA && m.validas.size() == 1 && m.idFoco() == "jugador_monstruo_0");
	pulsar(B_X);
	cuadros(45);
	{
		const Celda& c = m.zonas[zonaPorId("jugador_monstruo_0")];
		VERIFICA(c.carta == "blue_eyes" && c.bocaAbajo && c.defensa);
		VERIFICA(zonaDe("blue_eyes") < 0);
	}

	// 3) sin zonas libres, Invocar sale apagado
	pulsar(B_ABAJO);
	pulsar(B_ABAJO);
	VERIFICA(m.foco.tipo == F_MANO_J);
	for (int k = 0; k < 8 && m.manoJ[m.foco.i] != "gaia"; k++) pulsar(B_DER);
	pulsar(B_X);
	VERIFICA(m.estado == MENU && !m.opciones[0].activa && m.opciones[m.opcion].accion == AC_CANCELAR);   // arranca en la primera activa (sin "Ver detalle")
	pulsar(B_ARRIBA);
	pulsar(B_ARRIBA);
	VERIFICA(m.opcion == 0);
	pulsar(B_X);   // apagada: solo el aviso
	VERIFICA(m.estado == MENU && m.tAviso < 0.2f);
	pulsar(B_CIRCULO);
	VERIFICA(m.estado == NAVEGAR);

	// 4) atacar: Ojos Rojos -> Craneo convocado
	pulsar(B_ARRIBA);
	pulsar(B_ARRIBA);
	for (int k = 0; k < 12 && m.idFoco() != "jugador_monstruo_1"; k++) pulsar(ZONAS_MESA[m.foco.i].x < -2.7f ? B_DER : B_IZQ);
	pulsar(B_X);
	VERIFICA(m.estado == MENU && m.opciones[0].accion == AC_ATACAR && m.opciones[0].activa);
	pulsar(B_X);
	VERIFICA(m.estado == ELEGIR_OBJETIVO && m.validas.size() == 5);
	VERIFICA(m.idFoco() == "rival_monstruo_3");   // el del rival justo enfrente (x -2.7)
	pulsar(B_DER);
	VERIFICA(m.idFoco() == "rival_monstruo_2");
	pulsar(B_X);
	const Evento* at = ultimo(EV_ATACAR);
	VERIFICA(at && at->carta == "red_eyes" && at->zona2 == zonaPorId("rival_monstruo_2"));
	VERIFICA(m.estado == NAVEGAR && m.idFoco() == "jugador_monstruo_1");

	// 5) mientras ataca no se puede volver a atacar
	atacando = true;
	pulsar(B_X);
	VERIFICA(m.estado == MENU && !m.opciones[0].activa);
	pulsar(B_CIRCULO);
	atacando = false;

	// 6) cambiar a defensa
	pulsar(B_X);
	pulsar(B_ABAJO);
	VERIFICA(m.opciones[m.opcion].accion == AC_POSICION);
	pulsar(B_X);
	const Evento* po = ultimo(EV_POSICION);
	VERIFICA(po && po->carta == "red_eyes" && po->valor == 1 && m.zonas[zonaPorId("jugador_monstruo_1")].defensa);
	pulsar(B_X);   // en defensa: Atacar apagado
	VERIFICA(!m.opciones[0].activa);
	pulsar(B_CIRCULO);

	// 7) voltear a Ojos Azules (invocacion por volteo)
	pulsar(B_IZQ);
	VERIFICA(m.idFoco() == "jugador_monstruo_0");
	pulsar(B_X);
	VERIFICA(m.opciones[0].accion == AC_VOLTEAR_MONSTRUO);
	pulsar(B_X);
	const Evento* vo = ultimo(EV_VOLTEAR_MONSTRUO);
	VERIFICA(vo && vo->carta == "blue_eyes" && m.zonas[zonaPorId("jugador_monstruo_0")].carta == "blue_eyes");

	// 8) trampa: colocar (en la unica zona libre, la de magia 2) y voltear
	pulsar(B_ABAJO);
	pulsar(B_ABAJO);
	VERIFICA(m.foco.tipo == F_MANO_J);
	for (int k = 0; k < 8 && m.manoJ[m.foco.i] != "trap_hole"; k++) pulsar(B_DER);
	pulsar(B_X);
	VERIFICA(m.opciones[0].accion == AC_COLOCAR_MT);
	pulsar(B_X);
	VERIFICA(m.estado == ELEGIR_ZONA && m.validas.size() == 1 && m.idFoco() == "jugador_magia_2");
	const int zt = m.foco.i;
	pulsar(B_X);
	cuadros(45);
	VERIFICA(m.zonas[zt].carta == "trap_hole" && m.zonas[zt].bocaAbajo);
	pulsar(B_X);
	VERIFICA(m.estado == MENU && m.opciones[0].texto == "Voltear");
	pulsar(B_X);
	VERIFICA(!m.zonas[zt].bocaAbajo && ultimo(EV_ACTIVAR_MAGIA));

	// 9) Bosque: desde la mano no se puede (la zona de campo tiene el suyo boca abajo); se activa en su zona, se destruye
	// y despues si se juega el de la mano
	pulsar(B_ABAJO);
	for (int k = 0; k < 8 && m.manoJ[m.foco.i] != "forest"; k++) pulsar(B_DER);
	pulsar(B_X);
	VERIFICA(m.opciones[0].accion == AC_ACTIVAR_CAMPO_MANO && !m.opciones[0].activa);
	pulsar(B_CIRCULO);
	pulsar(B_ARRIBA);
	pulsar(B_ARRIBA);
	for (int k = 0; k < 12 && m.idFoco() != "jugador_campo"; k++) pulsar(B_IZQ);
	VERIFICA(m.idFoco() == "jugador_campo");
	pulsar(B_X);
	VERIFICA(m.opciones[0].accion == AC_ACTIVAR_CAMPO);
	pulsar(B_X);
	VERIFICA(ultimo(EV_ACTIVAR_CAMPO) && !m.zonas[zonaPorId("jugador_campo")].bocaAbajo);
	pulsar(B_X);
	VERIFICA(m.opciones[0].accion == AC_DESTRUIR_CAMPO);
	pulsar(B_X);
	VERIFICA(ultimo(EV_DESTRUIR_CAMPO) && m.zonas[zonaPorId("jugador_campo")].vacia() && m.cementerioJ.back() == "forest");
	pulsar(B_ABAJO);
	pulsar(B_ABAJO);
	for (int k = 0; k < 8 && m.manoJ[m.foco.i] != "forest"; k++) pulsar(B_DER);
	m.eventos.clear();
	pulsar(B_X);
	VERIFICA(m.opciones[0].activa);
	pulsar(B_X);
	cuadros(45);
	VERIFICA(m.zonas[zonaPorId("jugador_campo")].carta == "forest" && ultimo(EV_ACTIVAR_CAMPO));

	// 10) fase siguiente (rama classic-md-fases: OPTIONS -> selector -> X) y detalle de un monstruo del rival (triangulo)
	const int fase0 = m.fase;
	pulsar(B_CUADRADO);
	VERIFICA(m.fase == fase0);   // (el cuadrado ya no pasa de fase)
	m.pulsarOptions();
	pulsar(0);
	VERIFICA(m.selFases.abierto);
	pulsar(B_X);
	VERIFICA(!m.selFases.abierto);
	VERIFICA(m.fase != fase0 && ultimo(EV_FASE) && ultimo(EV_FASE)->valor == m.fase);
	VERIFICA(m.idFoco() == "jugador_campo");
	pulsar(B_ARRIBA);   // del campo a la fila de monstruos del rival (su cementerio, del mismo lado)
	VERIFICA(m.idFoco() == "rival_cementerio");
	pulsar(B_DER);
	VERIFICA(m.idFoco() == "rival_monstruo_4");
	pulsar(B_TRIANGULO);
	VERIFICA(m.estado == DETALLE);
	pulsar(B_CIRCULO);
	VERIFICA(m.estado == NAVEGAR);

	// 11) Mago oscuro al cementerio: su modelo se desvanece
	for (int k = 0; k < 12 && m.idFoco() != "jugador_monstruo_2"; k++) pulsar(m.foco.tipo == F_ZONA && ZONAS_MESA[m.foco.i].rival ? B_ABAJO : (ZONAS_MESA[m.foco.i].x < 0.0f ? B_DER : B_IZQ));
	pulsar(B_X);
	for (int k = 0; k < 8 && m.opciones[m.opcion].accion != AC_CEMENTERIO; k++) pulsar(B_ABAJO);
	pulsar(B_X);
	const Evento* ce = ultimo(EV_AL_CEMENTERIO);
	VERIFICA(ce && ce->carta == "dark_magician" && m.zonas[zonaPorId("jugador_monstruo_2")].vacia());
	VERIFICA(m.cementerioJ.back() == "dark_magician");

	// 12) el guion del PC
	Guion g;
	g.leer("X,abajo*2,1.5,O");
	VERIFICA(g.pasos.size() == 5);
	uint32_t vistos = 0;
	for (int k = 0; k < 400; k++) vistos |= g.avanzar(1.0f / 60.0f);
	VERIFICA(g.terminado() && (vistos & B_X) && (vistos & B_ABAJO) && (vistos & B_CIRCULO));

	fprintf(stderr, "\n%s: %d fallos, %d eventos\n", fallos ? "PRUEBA FALLIDA" : "PRUEBA OK", fallos, (int)m.eventos.size());
	return fallos ? 1 : 0;
}
