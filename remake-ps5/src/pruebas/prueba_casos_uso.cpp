// Prueba de los CASOS DE USO (rama classic-ciclos): cada hoja de docs/casos_de_uso.md con al menos una comprobacion
// automatica. Front (cu_front.h: el juego sin GPU de cu_juego.h con el mando), back (cu_back.h: las reglas en el motor)
// y carta por carta (cu_cartas.h). Escribe la tabla de resultados (TSV) y el resumen. La corre ciclo_completo.sh.
// Compilar y correr (contenedor arch-ps5):
//   g++ -std=c++17 -O1 -DPS5_HOST_REFERENCE -I remake-ps5/src -I /work/PS5_ClassicRemaster/external/tinygltf \
//       -I /work/PS5_ClassicRemaster/external/glm remake-ps5/src/pruebas/prueba_casos_uso.cpp -o /tmp/prueba_casos_uso
//   /tmp/prueba_casos_uso <carpeta con ui/> <caras (classic-assets/ui/)> <modelos (models/yugi/)> <certificacion.csv> <src/> <salida.tsv>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#include "casos_uso.h"
#include "cu_juego.h"
#include "cu_front.h"
#include "cu_ataque_directo.h"   // rama classic-ps5-ataque: el ataque directo va al oponente (no a una zona)
#include "cu_back.h"
#include "cu_cartas.h"
#include "entrada_clave.h"
#include "duelo_cadena.h"
#include "galeria.h"

#include <cstdlib>

// la cadena visible (duelo_cadena.h): los eslabones entran, se marcan negados, salen al resolverse
static void cadenaVisible()
{
	cadena::Pila p;
	p.suceso(ia::Suceso(ia::S_ACTIVA_MT, 0, 0, -1, 1, true, "raigeki"));
	p.suceso(ia::Suceso(ia::S_ACTIVA_MT, 1, 0, -1, 2, true, "magic_jammer"));
	cu::caso("CU-FE-DUELO-CADENA-VISIBLE", p.v.size() == 2 && p.v[0].n == 1 && p.v[1].n == 2 && p.v[1].lado == 1, "la pila no muestra C1 y C2");
	p.suceso(ia::Suceso(ia::S_NEGADO, 0, 0, -1, 0, true, "raigeki"));
	cu::caso("CU-FE-DUELO-CADENA-VISIBLE", p.v[0].negado, "el eslabon negado no se marca");
	p.suceso(ia::Suceso(ia::S_RESUELVE, 1, 0, -1, 2, true, "magic_jammer"));
	p.avanzar(0.3f, true, false);
	p.avanzar(0.3f, true, false);
	cu::caso("CU-FE-DUELO-CADENA-VISIBLE", p.vivos() == 1 && p.v.size() == 1, "C2 resuelto no salio de la pila (%d)", (int)p.v.size());
	p.suceso(ia::Suceso(ia::S_TURNO, 1, -1, -1, 4));
	cu::caso("CU-FE-DUELO-CADENA-VISIBLE", !p.visible(), "la pila no se vacia al cambiar de turno");
}

// entrada de las cartas clave (entrada_clave.h): la clase y su duracion
static void entradaClave(const ia::Base& base, const std::set<std::string>& conFigura)
{
	using namespace entrada;
	cu::caso("CU-FE-DUELO-ENTRADA-CLAVE", claseDe("efecto", "Bestia Divina", "DIVINIDAD") == DIOS && claseDe("fusion", "Guerrero", "FUEGO") == FUSION &&
	                                          claseDe("ritual", "Guerrero", "TIERRA") == RITUAL && claseDe("normal", "Dragón", "LUZ") == DRAGON &&
	                                          claseDe("normal", "Lanzador de Conjuros", "OSCURIDAD") == MAGO,
	         "la clase de la entrada no sale de la carta");
	cu::caso("CU-FE-DUELO-ENTRADA-CLAVE", duracion(DIOS, true, false) > duracion(NORMAL, true, false) && duracion(NORMAL, false, true) <= 1.0f, "las duraciones de la entrada");
	int malas = 0;
	std::string cuales;
	for (const std::string& id : conFigura) {
		const ia::Carta* k = base.carta(id);
		if (!k) {
			malas++;
			cuales += id + " ";
			continue;
		}
		if (!*sonidoCorte(k->atributo, claseDe(k->clase, k->raza, k->atributo))) malas++;
	}
	cu::caso("CU-FE-DUELO-FIGURAS", malas == 0, "%d cartas de figuras.txt no estan en la coleccion: %s", malas, cuales.c_str());
}

static std::string dirDe(const std::string& r) { return r.substr(0, r.find_last_of('/') + 1); }

int main(int argc, char** argv)
{
	if (argc < 7) {
		fprintf(stderr, "uso: prueba_casos_uso <carpeta con ui/> <caras/> <modelos/> <certificacion.csv> <src/> <salida.tsv> [solo: front|back|cartas]\n");
		return 2;
	}
	const std::string assets = std::string(argv[1]) + "/", ui = assets + "ui/", caras = std::string(argv[2]) + "/", modelos = std::string(argv[3]) + "/";
	const std::string csv = argv[4], src = std::string(argv[5]) + "/", salida = argv[6];
	const std::string solo = argc > 7 ? argv[7] : "";
	// los datos del jugador en una carpeta limpia (como /data/yugioh_classic la primera vez)
	const std::string datos = assets + "datos_cu/";
	system(("rm -rf '" + datos + "' && mkdir -p '" + datos + "'").c_str());
	setenv("YUGI_MAZO", (datos + "mazo.txt").c_str(), 1);
	unsetenv("YUGI_GUION");
	unsetenv("YUGI_IDIOMA");
	unsetenv("YUGI_FIGURAS");
	unsetenv("YUGI_CAMARA");
	unsetenv("YUGI_VELOCIDAD");
	unsetenv("YUGI_RESULTADO");
	idioma::cargar(datos + "idioma.txt");

	ia::Base base;
	if (!base.leer(ui)) {
		fprintf(stderr, "sin datos en %s\n", ui.c_str());
		return 2;
	}
	std::set<std::string> conFigura;
	{
		std::ifstream f(ui + "figuras.txt");
		std::string l;
		while (std::getline(f, l)) {
			if (l.empty() || l[0] == '#') continue;
			std::istringstream ss(l);
			std::string id, slug;
			if (!(ss >> id >> slug)) continue;
			conFigura.insert(id);
			cu::caso("CU-FE-DUELO-FIGURAS", cuc::existe(modelos + slug + ".glb"), "falta el modelo %s.glb de %s", slug.c_str(), id.c_str());
		}
		cu::caso("CU-FE-DUELO-FIGURAS", conFigura.size() > 50, "figuras.txt tiene %d cartas", (int)conFigura.size());
	}

	if (solo.empty() || solo == "front") {
		fprintf(stderr, "== front\n");
		Juego* g = new Juego;
		g->conFigura = conFigura;
		g->preparar(assets);
		cu::caso("CU-FE-ARRANQUE", g->baseIAlista && !g->historia.lista.empty() && g->coleccion.cartas.size() > 1700, "el juego no cargo sus datos");
		cu::caso("CU-FE-GUARDADO-RUTAS", g->rutaProgreso == datos + "progreso.txt" && g->rutaOpciones == datos + "opciones.txt", "progreso/opciones fuera de la carpeta de datos");
		cu::caso("CU-FE-CONS-MAZOS-INICIO", g->biblioteca.mazos.size() >= 4 && g->biblioteca.mazoActivo() != nullptr, "la primera vez no se armaron los mazos de inicio");
		if (getenv("CU_SOLO_PS5")) {   // (rama classic-ps5-arreglos1: para depurar solo el primer turno de la PS5)
			cub::primerTurnoPS5(base);
			cuf::turnoPS5(*g);
			return cu::resumen(salida.c_str());
		}
		cuf::portada(*g);
		cuf::menu(*g);
		cuf::opcionesMenu(*g, datos);
		cuf::personalizacion(*g, datos);
		cuf::elegirRival(*g);
		cuf::dialogos(*g);
		cuf::duelo(*g);
		cuf::preguntarEfecto(*g);
		cuf::reglasOficiales(*g);   // (rama classic-reglas-oficiales)
		cuad::todo(*g);             // (rama classic-ps5-ataque: ataque directo como Master Duel, de los dos lados)
		cuf::turnoPS5(*g);          // (rama classic-ps5-arreglos1: el primer turno del usuario en la PS5)
		cuf::resultadoPantalla(*g);
		cuf::idiomaTodo(*g);
		cuf::mando();
		cuf::guardadoAtomico(datos);
		cadenaVisible();
		entradaClave(base, conFigura);
		// las cartas clave (firma de un personaje o con figura): suena su musica
		bool clave = true;
		for (const std::string& id : conFigura) clave = clave && g->esCartaClave(id);
		cu::caso("CU-FE-DUELO-ENTRADA-CLAVE", clave, "una carta con figura no es carta clave");
		// (rama classic-ps5-arreglos1) el tablero del duelo real no trae las cartas horneadas del duelo de prueba ni la casilla
		// de vidrio opaca que las tapaba (en la PS5: recuadro negro con las pestannas naranjas de las cartas de lado)
		{
			const std::string yr = cuf::leerArchivo(src + "yugireload.cpp"), it = cuf::leerArchivo(src + "interfaz.h");
			cu::caso("CU-FE-PS5-TABLERO-LIMPIO", cuf::contiene(yr, "ctl.duelo && nodo->name.rfind(\"Carta_\", 0) == 0") && cuf::contiene(it, "if (h && !ctl->duelo &&"),
			         "el tablero del duelo sigue dibujando las cartas horneadas o la casilla de vidrio que las tapa");
			cu::caso("CU-FE-PS5-CORTE-TRAMPA", cuf::contiene(it, "dibujarCorteMando();"), "la interfaz no dibuja el cartel de la carta del rival");
		}
		// las magias de campo de los tapetes tienen su escenario
		const std::set<std::string> campos = cuc::leerCampos(src + "campos.h");
		for (int k = 1; k < cosmeticos::NUM_TAPETES; k++)
			cu::caso("CU-FE-PERSO-TAPETE", campos.count(cosmeticos::TAPETES[k].campo) > 0, "el tapete %s usa la carta %s sin escenario", cosmeticos::TAPETES[k].clave, cosmeticos::TAPETES[k].campo);
		// la galeria de figuras: una entrada por figura, ordenada, X ataca, triangulo detalle, bloqueadas avisan
		{
			galeria::Galeria gal;
			std::vector<galeria::Entrada> es;
			std::map<std::string, int> porSlug;
			std::ifstream f(ui + "figuras.txt");
			std::string l;
			while (std::getline(f, l)) {
				if (l.empty() || l[0] == '#') continue;
				std::istringstream ss(l);
				std::string id, slug;
				if (!(ss >> id >> slug)) continue;
				if (!porSlug.count(slug)) {
					porSlug[slug] = (int)es.size();
					es.push_back(galeria::Entrada{});
					es.back().fig = (int)es.size() - 1;
				}
				es[(size_t)porSlug[slug]].cartas.push_back(id);
			}
			gal.preparar(&g->coleccion, es);
			gal.abrir("", true);
			cu::caso("CU-FE-GALERIA-ABRIR", gal.abierta && gal.entradas.size() > 40 && gal.desdeLista, "la galeria no abrio con sus figuras (%d)", (int)gal.entradas.size());
			auto vence = [](const mazo::Carta& k) { return std::string("Vence a ") + (k.duennos.empty() ? "?" : k.duennos[0]); };
			const int c0 = gal.cursor;
			uint32_t ev = gal.avanzar(0.016f, constructor::C_DER, 0, 0, 0, 0, vence);
			cu::caso("CU-FE-GALERIA-NAVEGAR", gal.cursor == c0 + 1 && (ev & galeria::EV_MOVIO), "derecha no movio el cursor de la galeria");
			gal.avanzar(0.016f, constructor::C_ABAJO, 0, 0, 0, 0, vence);
			cu::caso("CU-FE-GALERIA-NAVEGAR", gal.cursor == c0 + 1 + galeria::COLS, "abajo no bajo una fila");
			bool vioAtaque = false, vioBloq = false;
			for (int k = 0; k < (int)gal.entradas.size(); k++) {
				gal.cursor = k;
				gal.tAtaque = 99.0f;
				ev = gal.avanzar(0.016f, constructor::C_X, 0, 0, 0, 0, vence);
				if (gal.bloqueada(gal.entradas[(size_t)k])) vioBloq |= (ev & galeria::EV_BLOQUEADA) && !gal.aviso.empty();
				else vioAtaque |= (ev & galeria::EV_ATACA) != 0;
			}
			cu::caso("CU-FE-GALERIA-ATACAR", vioAtaque, "X no hace atacar a ninguna figura");
			cu::caso("CU-FE-GALERIA-BLOQUEADA", vioBloq || gal.desbloqueadas() == (int)gal.entradas.size(), "una figura de carta firma bloqueada no avisa");
			ev = gal.avanzar(0.016f, constructor::C_TRIANGULO, 0, 0, 0, 0, vence);
			cu::caso("CU-FE-GALERIA-DETALLE", gal.detalle, "triangulo no abrio el detalle");
			gal.avanzar(0.016f, constructor::C_CIRCULO, 0, 0, 0, 0, vence);
			gal.avanzar(0.016f, constructor::C_CIRCULO, 0, 0, 0, 0, vence);
			cu::caso("CU-FE-GALERIA-CERRAR", !gal.abierta, "circulo no cerro la galeria");
			bool orden = true;
			for (size_t k = 1; k < gal.entradas.size(); k++) {
				const mazo::Carta* a = g->coleccion.carta(gal.entradas[k - 1].carta);
				const mazo::Carta* b = g->coleccion.carta(gal.entradas[k].carta);
				orden = orden && a && b && !(b->nombre < a->nombre);
			}
			cu::caso("CU-FE-GALERIA-ORDEN", orden, "las figuras no estan ordenadas por nombre");
		}
		(void)dirDe;
	}
	if (solo.empty() || solo == "back") {
		fprintf(stderr, "== back\n");
		cub::reglas(base);
		cub::atajos(base);
		cub::mazos(ui);
		cub::torneo(base);   // (rama classic-mazos-torneo)
		cub::torneoExtras(base);   // (rama classic-torneo-extras)
		cub::primerTurnoPS5(base);   // (rama classic-ps5-arreglos1)
	}
	if (solo.empty() || solo == "cartas") {
		fprintf(stderr, "== cartas\n");
		cuc::cartas(base, ui, caras, csv, src + "campos.h");
	}
	return cu::resumen(salida.c_str());
}
