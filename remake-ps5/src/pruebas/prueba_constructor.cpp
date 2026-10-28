// Prueba del constructor de mazos (rama classic-constructor2) sin pantalla: la biblioteca de mazos (primera vez:
// importa mazo.txt y arma los mazos de inicio), los mazos de inicio validos y con sus cartas de campo, el editor con
// el mando (constructor.h: crear un mazo con el teclado en pantalla, buscar, filtrar, agregar, quitar, validar,
// guardar), cargar de nuevo desde el disco y el duelo real (ia::Duelo::empezar, lo mismo que arrancarDueloIA en
// duelo_juego.h) robando del mazo activo: la mano y el mazo salen de el, barajado, y el extra son sus fusiones.
// Compilar y correr (contenedor arch-ps5):
//   g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_constructor.cpp -o /tmp/prueba_constructor &&
//   /tmp/prueba_constructor <carpeta ui/ con cartas_ui.txt y cartas_classic.txt>
#include "control.h"
#include "constructor.h"

#include <map>

static int fallos = 0;
#define FALLO(...) do { fprintf(stderr, "FALLO: " __VA_ARGS__); fprintf(stderr, "\n"); fallos++; } while (0)
#define CHEQUEAR(c, ...) do { if (!(c)) FALLO(__VA_ARGS__); } while (0)

using namespace constructor;

static void pulsar(Constructor& c, uint32_t b, int veces = 1)
{
	for (int i = 0; i < veces; i++) c.avanzar(1.0f / 60.0f, b);
}
// escribe con el teclado en pantalla (mueve el cursor a cada tecla y aprieta X)
static void escribir(Constructor& c, const std::string& txt)
{
	for (char ch : txt) {
		if (ch == ' ') {
			pulsar(c, C_TRIANGULO);
			continue;
		}
		const char up = (char)toupper((unsigned char)ch);
		int f = -1, k = -1;
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 10; j++)
				if (TECLAS[i][j][0] == up && TECLAS[i][j][1] == 0) f = i, k = j;
		if (f < 0) continue;
		const bool quiereMayus = isupper((unsigned char)ch) != 0;
		if (quiereMayus != c.mayus && isalpha((unsigned char)ch)) pulsar(c, C_R1);
		while (c.teclaF != f) pulsar(c, c.teclaF < f ? C_ABAJO : C_ARRIBA);
		while (c.teclaC != k) pulsar(c, c.teclaC < k ? C_DER : C_IZQ);
		pulsar(c, C_X);
	}
}

int main(int argc, char** argv)
{
	const std::string ui = argc > 1 ? argv[1] : "/compartido/classic-assets/ui/";
	const std::string dir = "/tmp/prueba_constructor_datos/";
	if (system(("rm -rf " + dir + " && mkdir -p " + dir).c_str()) != 0) return 1;
	mazo::Coleccion col;
	if (!col.leer(ui)) {
		fprintf(stderr, "sin cartas en %s\n", ui.c_str());
		return 1;
	}
	// un mazo.txt viejo (formato de siempre, sin nombre): se importa como "Mi mazo"
	const mazo::Mazo viejo = mazo::Mazo::porDefecto(col);
	viejo.guardar(dir + "mazo.txt");

	// ---- 1. primera vez: importa mazo.txt y arma los mazos de inicio
	mazo::Biblioteca bib;
	bib.cargar(dir + "mazo.txt", col, false);
	CHEQUEAR(bib.mazos.size() == 1 + mazo::mazosInicio().size(), "esperaba %d mazos, hay %d", (int)(1 + mazo::mazosInicio().size()), (int)bib.mazos.size());
	CHEQUEAR(!bib.mazos.empty() && bib.mazos[0].nombre == "Mi mazo", "mazo.txt no se importo como 'Mi mazo'");
	CHEQUEAR(bib.activo == 0, "el activo deberia ser el importado (valido)");
	const char* const CAMPOS[] = { "mountain", "umi", "yami", "sogen" };
	for (size_t i = 1; i < bib.mazos.size(); i++) {
		const mazo::Mazo& m = bib.mazos[i];
		std::string motivo;
		CHEQUEAR(m.valido(col, &motivo), "mazo de inicio '%s' no valido: %s", m.nombre.c_str(), motivo.c_str());
		CHEQUEAR(m.cuantas(CAMPOS[i - 1]) > 0, "mazo de inicio '%s' sin %s", m.nombre.c_str(), CAMPOS[i - 1]);
		CHEQUEAR(!m.fusion.empty(), "mazo de inicio '%s' sin fusiones", m.nombre.c_str());
		for (const std::string& id : m.fusion) CHEQUEAR(col.esFusion(id), "%s en el extra no es fusion", id.c_str());
		int campo = 0;
		for (const std::string& id : m.principal) campo += col.carta(id)->subtipo == "magia_campo" ? 1 : 0;
		fprintf(stderr, "[prueba] inicio '%s': %d + %d, %d magias de campo\n", m.nombre.c_str(), (int)m.principal.size(), (int)m.fusion.size(), campo);
	}
	CHEQUEAR(bib.mazos.size() > 2 && bib.mazos[2].cuantas("a_legendary_ocean") > 0, "el mazo de agua sin A Legendary Ocean");

	// ---- 2. el editor con el mando: nuevo mazo con nombre por el teclado en pantalla
	Constructor c;
	c.abrir(col, bib);
	CHEQUEAR(c.pantalla == P_LISTA && c.cursorLista == 0, "no abrio en MIS MAZOS sobre el activo");
	pulsar(c, C_ARRIBA);   // (da la vuelta: "+ Nuevo mazo")
	CHEQUEAR(c.cursorLista == (int)bib.mazos.size(), "arriba desde el primero no fue a '+ Nuevo mazo'");
	pulsar(c, C_X);
	CHEQUEAR(c.capa == K_TECLADO && c.modoTeclado == T_NOMBRE_NUEVO, "X en '+ Nuevo mazo' no abrio el teclado");
	escribir(c, "Prueba Campos");
	CHEQUEAR(c.textoTeclado == "Prueba Campos", "el teclado escribio '%s'", c.textoTeclado.c_str());
	pulsar(c, C_OPTIONS);
	CHEQUEAR(c.pantalla == P_EDITOR && c.editando == -1 && c.mazo.nombre == "Prueba Campos", "no entro al editor con el mazo nuevo");
	// guardar vacio: no se puede
	pulsar(c, C_OPTIONS);
	CHEQUEAR(c.aviso.find("No se puede guardar") == 0, "guardo un mazo vacio (aviso '%s')", c.aviso.c_str());

	// buscar "umi" con el teclado (Triangulo -> Nombre -> X)
	pulsar(c, C_TRIANGULO);
	CHEQUEAR(c.capa == K_FILTROS && c.filaFiltro == FF_NOMBRE, "Triangulo no abrio los filtros");
	pulsar(c, C_X);
	escribir(c, "umi");
	pulsar(c, C_OPTIONS);
	CHEQUEAR(c.capa == K_NADA, "el teclado de busqueda no se cerro");
	bool hayUmi = false;
	for (int i : c.lista) hayUmi |= col.cartas[(size_t)i].id == "umi";
	CHEQUEAR(hayUmi, "buscar 'umi' no encontro Umi (%d cartas)", (int)c.lista.size());
	while (c.elegida() && c.elegida()->id != "umi") pulsar(c, C_DER);
	pulsar(c, C_X, 4);   // la cuarta no entra
	CHEQUEAR(c.mazo.cuantas("umi") == 3, "Umi: %d copias (esperaba 3)", c.mazo.cuantas("umi"));
	CHEQUEAR(c.aviso.find("3") != std::string::npos, "no explico el maximo de 3 (aviso '%s')", c.aviso.c_str());

	// filtros: Magia + Campo
	pulsar(c, C_L3);
	pulsar(c, C_TRIANGULO);
	pulsar(c, C_ABAJO);   // Clase
	pulsar(c, C_DER, 5);  // Magia
	pulsar(c, C_ABAJO);   // Subtipo
	pulsar(c, C_DER, 2);  // Campo
	CHEQUEAR(c.filtros.clase == 5 && c.filtros.subtipo == 2, "filtros clase %d subtipo %d", c.filtros.clase, c.filtros.subtipo);
	for (int i : c.lista) CHEQUEAR(col.cartas[(size_t)i].subtipo == "magia_campo", "filtro de campo dejo pasar %s", col.cartas[(size_t)i].id.c_str());
	fprintf(stderr, "[prueba] magias de campo en la coleccion: %d\n", (int)c.lista.size());
	pulsar(c, C_CIRCULO);
	CHEQUEAR(c.capa == K_NADA, "circulo no cerro los filtros");

	// limites Goat: una prohibida y una limitada
	{
		const mazo::Carta* prohibida = nullptr;
		const mazo::Carta* limitada = nullptr;
		for (const mazo::Carta& k : col.cartas) {
			if (!k.firma && k.limiteBase == 0 && !prohibida) prohibida = &k;
			if (!k.firma && k.limiteBase == 1 && k.clase != "fusion" && !limitada) limitada = &k;
		}
		std::string motivo;
		if (prohibida) CHEQUEAR(!c.mazo.agregar(col, prohibida->id, &motivo), "agrego la prohibida %s", prohibida->id.c_str());
		if (limitada) {
			CHEQUEAR(c.mazo.agregar(col, limitada->id), "no agrego la limitada %s", limitada->id.c_str());
			CHEQUEAR(!c.mazo.agregar(col, limitada->id, &motivo), "agrego 2 de la limitada %s", limitada->id.c_str());
			c.mazo.quitar(limitada->id);
		}
		// carta firma bloqueada (nadie vencido)
		for (const mazo::Carta& k : col.cartas) {
			if (!k.firma) continue;
			CHEQUEAR(k.bloqueada, "la firma %s no esta bloqueada", k.id.c_str());
			CHEQUEAR(!c.mazo.agregar(col, k.id, &motivo) && motivo.find("Vence a") != std::string::npos, "firma %s: motivo '%s'", k.id.c_str(), motivo.c_str());
			break;
		}
	}

	// Opciones > "Permitir cartas prohibidas" (opciones.txt): sin banlist Goat, hasta 3 de todo; con No, la banlist entera
	{
		FILE* f = fopen((dir + "opciones.txt").c_str(), "w");
		fprintf(f, "musica 0.80\npermitir_prohibidas=1\n");
		fclose(f);
		CHEQUEAR(constructor::detalle::leerPermitirProhibidas(dir + "opciones.txt"), "no leyo permitir_prohibidas=1");
		CHEQUEAR(!constructor::detalle::leerPermitirProhibidas(dir + "no_existe.txt"), "sin archivo deberia ser No");
		const mazo::Carta* prohibida = nullptr;
		const mazo::Carta* limitada = nullptr;
		for (const mazo::Carta& k : col.cartas) {
			if (!k.firma && k.limiteBase == 0 && k.clase != "fusion" && !prohibida) prohibida = &k;
			if (!k.firma && k.limiteBase == 1 && k.clase != "fusion" && !limitada) limitada = &k;
		}
		mazo::Mazo m = bib.mazos[1];
		while (m.principal.size() > 34) m.principal.pop_back();
		constructor::aplicarOpcionProhibidas(col, dir + "mazo.txt");
		CHEQUEAR(col.permitirProhibidas, "aplicarOpcionProhibidas no la aplico");
		std::string motivo;
		if (prohibida) {
			CHEQUEAR(m.agregar(col, prohibida->id, &motivo) && m.agregar(col, prohibida->id) && m.agregar(col, prohibida->id), "con Si no agrego 3 de la prohibida %s (%s)",
			         prohibida->id.c_str(), motivo.c_str());
			CHEQUEAR(!m.agregar(col, prohibida->id, &motivo), "con Si agrego 4 copias");
		}
		if (limitada) CHEQUEAR(m.agregar(col, limitada->id) && m.agregar(col, limitada->id) && m.agregar(col, limitada->id), "con Si no agrego 3 de la limitada");
		CHEQUEAR(m.valido(col, &motivo), "con Si el mazo con prohibidas no es valido: %s", motivo.c_str());
		col.ponerPermitirProhibidas(false);
		CHEQUEAR(!m.valido(col, &motivo), "con No el mazo con prohibidas sigue valido");
		fprintf(stderr, "[prueba] banlist de nuevo: %s\n", motivo.c_str());
	}

	// agregar una fusion: va sola al extra
	c.quitarFiltros();
	pulsar(c, C_R2, 4);   // clase rapida: Fusion
	CHEQUEAR(c.filtros.clase == 4, "R2 x4 no llego a Fusion (%d)", c.filtros.clase);
	while (c.elegida() && (c.elegida()->firma || c.elegida()->limite == 0)) pulsar(c, C_DER);
	const std::string fus = c.elegida() ? c.elegida()->id : "";
	pulsar(c, C_X);
	CHEQUEAR(!fus.empty() && c.mazo.fusion.size() == 1 && c.mazo.fusion[0] == fus && c.mazo.principal.size() == 3, "la fusion %s no fue al extra", fus.c_str());

	// completar a 40 con monstruos normales (L2/R2: Normal) y quitar uno con cuadrado desde el mazo
	pulsar(c, C_L2, 3);   // Normal
	CHEQUEAR(c.filtros.clase == 1, "L2 x3 no llego a Normal (%d)", c.filtros.clase);
	for (int guard = 0; c.mazo.principal.size() < 41 && guard < 400; guard++) {
		const mazo::Carta* k = c.elegida();
		if (k && c.mazo.sePuedeAgregar(col, k->id)) pulsar(c, C_X);
		else pulsar(c, C_DER);
	}
	CHEQUEAR(c.mazo.principal.size() == 41, "no llego a 41 (%d)", (int)c.mazo.principal.size());
	pulsar(c, C_RS_IZQ);   // al mazo
	CHEQUEAR(c.area == A_MAZO, "stick derecho a la izquierda no fue al mazo");
	pulsar(c, C_CUADRADO);
	CHEQUEAR(c.mazo.principal.size() == 40, "cuadrado no quito (%d)", (int)c.mazo.principal.size());
	std::string motivo;
	CHEQUEAR(c.valido(&motivo), "el mazo armado no es valido: %s", motivo.c_str());

	// circulo con cambios: pregunta; "Guardar y salir"
	pulsar(c, C_CIRCULO);
	CHEQUEAR(c.capa == K_PREGUNTA && c.pregunta == Q_SALIR_EDITOR, "circulo con cambios no pregunto");
	pulsar(c, C_X);
	CHEQUEAR(c.pantalla == P_LISTA && c.capa == K_NADA, "Guardar y salir no volvio a la lista");
	const int nuevo = (int)bib.mazos.size() - 1;
	CHEQUEAR(bib.mazos[(size_t)nuevo].nombre == "Prueba Campos" && bib.mazos[(size_t)nuevo].principal.size() == 40, "el mazo nuevo no quedo guardado");
	CHEQUEAR(c.cursorLista == nuevo, "el cursor no quedo en el mazo nuevo");
	// usarlo en los duelos (cuadrado)
	pulsar(c, C_CUADRADO);
	CHEQUEAR(bib.activo == nuevo, "cuadrado no lo hizo activo");
	// copiar y renombrar por el menu de acciones
	pulsar(c, C_X);
	CHEQUEAR(c.capa == K_PREGUNTA && c.pregunta == Q_ACCIONES, "X no abrio las acciones");
	pulsar(c, C_ABAJO, 3);   // Copiar
	pulsar(c, C_X);
	CHEQUEAR((int)bib.mazos.size() == nuevo + 2 && bib.mazos.back().principal == bib.mazos[(size_t)nuevo].principal, "Copiar no copio");
	pulsar(c, C_X);
	pulsar(c, C_ABAJO, 2);   // Renombrar
	pulsar(c, C_X);
	CHEQUEAR(c.capa == K_TECLADO && c.modoTeclado == T_NOMBRE_MAZO, "Renombrar no abrio el teclado");
	for (int i = 0; i < 30; i++) pulsar(c, C_CUADRADO);
	escribir(c, "Copia");
	pulsar(c, C_OPTIONS);
	CHEQUEAR(bib.mazos.back().nombre == "Copia", "no se renombro ('%s')", bib.mazos.back().nombre.c_str());
	// borrar la copia
	pulsar(c, C_X);
	pulsar(c, C_ABAJO, 4);   // Borrar
	pulsar(c, C_X);
	CHEQUEAR(c.pregunta == Q_BORRAR && c.opcion == 1, "Borrar no pregunto (con Cancelar elegido)");
	pulsar(c, C_ARRIBA);
	pulsar(c, C_X);
	CHEQUEAR((int)bib.mazos.size() == nuevo + 1, "no se borro la copia");
	// un mazo invalido no se puede usar
	{
		mazo::Mazo corto;
		corto.nombre = "Corto";
		corto.principal = { "umi" };
		const int i = bib.agregar(corto);
		CHEQUEAR(!bib.usar(i, col, &motivo), "se pudo usar un mazo de 1 carta");
		fprintf(stderr, "[prueba] mazo invalido: %s\n", motivo.c_str());
		bib.borrar(i);
	}
	pulsar(c, C_CIRCULO);
	CHEQUEAR(!c.abierto, "circulo en la lista no volvio al menu principal");

	// entradas del menu principal
	constructor::abrirListaCartas();
	CHEQUEAR(constructor::tomarPedido() == PEDIDO_LISTA && constructor::tomarPedido() == PEDIDO_NADA, "abrirListaCartas no pidio la lista");
	constructor::abrirConstructorMazo();
	CHEQUEAR(constructor::tomarPedido() == PEDIDO_CONSTRUCTOR, "abrirConstructorMazo no pidio el constructor");

	// ---- 2b. LISTA DE CARTAS (solo ver): rejilla, busqueda y filtros, sin agregar ni guardar; circulo vuelve
	{
		const size_t antes = bib.mazos.size();
		Constructor l;
		l.abrirLista(col, bib);
		CHEQUEAR(l.abierto && l.soloVer && l.pantalla == P_EDITOR && !l.lista.empty(), "la lista de cartas no abrio");
		pulsar(l, C_X);
		pulsar(l, C_OPTIONS);
		CHEQUEAR(l.mazo.principal.empty() && l.mazo.fusion.empty() && !l.cambiado, "la lista de cartas agrego una carta");
		pulsar(l, C_R2, 5);   // Magia
		CHEQUEAR(l.filtros.clase == 5, "R2 en la lista no filtro");
		pulsar(l, C_R1);
		CHEQUEAR(l.pagina() == 1 && l.colCols() == 10, "la lista: pagina %d columnas %d", l.pagina(), l.colCols());
		pulsar(l, C_CIRCULO);
		CHEQUEAR(!l.abierto && bib.mazos.size() == antes, "circulo no cerro la lista de cartas (o guardo algo)");
	}

	// ---- 3. cargar de nuevo desde el disco
	mazo::Biblioteca otra;
	otra.cargar(dir + "mazo.txt", col, false);
	CHEQUEAR(otra.mazos.size() == bib.mazos.size() && otra.activo == bib.activo, "al cargar: %d mazos activo %d (esperaba %d, %d)", (int)otra.mazos.size(), otra.activo,
	         (int)bib.mazos.size(), bib.activo);
	const mazo::Mazo* activo = otra.mazoActivo();
	CHEQUEAR(activo && activo->nombre == "Prueba Campos" && activo->principal == bib.mazos[(size_t)nuevo].principal && activo->fusion == bib.mazos[(size_t)nuevo].fusion,
	         "el mazo activo cargado no es el guardado");
	mazo::Mazo compat;   // mazo.txt (formato de siempre) = el activo
	compat.cargar(dir + "mazo.txt", col);
	CHEQUEAR(activo && compat.principal == activo->principal, "mazo.txt no tiene el mazo activo");

	// ---- 4. el duelo roba del mazo activo (lo mismo que arrancarDueloIA: dueloIA.empezar(mazoDuelo...))
	if (activo) {
		ia::Base base;
		if (!base.leer(ui)) {
			FALLO("ia::Base no leyo %s", ui.c_str());
		} else {
			mazo::Azar az(7);
			const mazo::Mazo rival = mazo::Mazo::aleatorio(col, az);
			std::map<std::string, int> esperado;
			for (const std::string& id : activo->principal) esperado[id]++;
			bool barajado = false;
			for (uint32_t semilla : { 11u, 12u, 13u }) {
				ia::Duelo d(&base);
				d.empezar(activo->principal, activo->fusion, rival.principal, rival.fusion, semilla, 0);
				const ia::Lado& j = d.l[0];
				std::map<std::string, int> visto;
				for (const std::string& id : j.mazo) visto[id]++;
				for (const std::string& id : j.mano) visto[id]++;
				CHEQUEAR(visto == esperado, "mazo + mano del duelo distintos del mazo activo (semilla %u)", semilla);
				CHEQUEAR(j.mano.size() >= 5 && j.mazo.size() + j.mano.size() == activo->principal.size(), "mano %d mazo %d", (int)j.mano.size(), (int)j.mazo.size());
				CHEQUEAR(j.extra == activo->fusion, "el extra del duelo no son las fusiones del mazo");
				barajado |= j.mazo != std::vector<std::string>(activo->principal.begin(), activo->principal.begin() + (long)j.mazo.size());
				// robar: la de arriba del mazo pasa a la mano
				const std::string arriba = j.mazo.back();
				d.l[0].mano.clear();
				fprintf(stderr, "[prueba] duelo (semilla %u): mano %d, quedan %d en el mazo, extra %d; arriba %s\n", semilla, (int)j.mano.size(), (int)j.mazo.size(), (int)j.extra.size(),
				        arriba.c_str());
			}
			CHEQUEAR(barajado, "el mazo del duelo no se baraja");
		}
	}

	// ---- 5. (rama classic-torneo-extras) MAZOS DE TORNEO: Triangulo en MIS MAZOS; se ven y se copian a MIS MAZOS
	{
		Constructor t;
		t.abrir(col, bib);
		t.torneos = mazo::leerMazosTorneo(ui);
		CHEQUEAR(t.torneos.size() == 21, "esperaba 21 mazos de torneo, hay %d", (int)t.torneos.size());
		for (const mazo::MazoTorneo& m : t.torneos) {
			std::string motivo;
			CHEQUEAR(m.mazo.valido(col, &motivo), "mazo de torneo '%s' no valido en la coleccion: %s", m.nombreEs.c_str(), motivo.c_str());
			CHEQUEAR(!m.nombreEs.empty() && !m.nombreEn.empty(), "mazo de torneo '%s' sin nombre ES/EN", m.clave.c_str());
		}
		pulsar(t, C_TRIANGULO);
		CHEQUEAR(t.pantalla == P_TORNEO, "Triangulo no abrio MAZOS DE TORNEO");
		pulsar(t, C_ABAJO, 3);
		const int elegido = t.cursorTorneo;
		CHEQUEAR(elegido == 3, "el cursor de los mazos de torneo no bajo (%d)", elegido);
		const size_t antes = bib.mazos.size();
		pulsar(t, C_X);
		CHEQUEAR(t.capa == K_PREGUNTA && t.pregunta == Q_TORNEO && t.opciones.size() == 3, "X no abrio las acciones del mazo de torneo");
		pulsar(t, C_ABAJO);   // "Copiar y editar"
		pulsar(t, C_X);
		CHEQUEAR(bib.mazos.size() == antes + 1, "no se copio a MIS MAZOS");
		CHEQUEAR(t.pantalla == P_EDITOR && t.editando == (int)antes, "Copiar y editar no abrio el editor sobre la copia");
		if (bib.mazos.size() == antes + 1 && elegido >= 0 && elegido < (int)t.torneos.size()) {
			const mazo::Mazo& copia = bib.mazos.back();
			std::map<std::string, int> a, b;
			for (const std::string& id : copia.principal) a[id]++;
			for (const std::string& id : t.torneos[(size_t)elegido].mazo.principal) b[id]++;
			CHEQUEAR(a == b && copia.fusion.size() == t.torneos[(size_t)elegido].mazo.fusion.size(), "la copia no tiene las mismas cartas");
			CHEQUEAR(copia.nombre == t.torneos[(size_t)elegido].nombreEs, "la copia se llama '%s'", copia.nombre.c_str());
			CHEQUEAR(copia.valido(col), "la copia no es valida");
			// se edita como cualquier mazo: quitar una copia y guardar (41 -> 40 no: queda en 39 y no se guarda)
			t.area = A_MAZO;
			t.cursorMazo = 0;
			t.enExtra = false;
			CHEQUEAR(t.quitarElegida() && !t.guardar(), "con 39 cartas se guardo");
			CHEQUEAR(bib.mazos.back().principal.size() == 40, "el mazo guardado cambio sin guardar");
		}
		pulsar(t, C_CIRCULO);   // (cambios sin guardar: pregunta)
		CHEQUEAR(t.capa == K_PREGUNTA && t.pregunta == Q_SALIR_EDITOR, "no pregunto por los cambios");
		pulsar(t, C_ABAJO);   // "Salir sin guardar"
		pulsar(t, C_X);
		CHEQUEAR(t.pantalla == P_LISTA, "no volvio a MIS MAZOS");
		pulsar(t, C_TRIANGULO);
		pulsar(t, C_CUADRADO);   // (Cuadrado: copia directo)
		CHEQUEAR(bib.mazos.size() == antes + 2 && t.pantalla == P_TORNEO, "Cuadrado no copio el mazo de torneo");
		pulsar(t, C_CIRCULO);
		CHEQUEAR(t.pantalla == P_LISTA, "Circulo no volvio a MIS MAZOS");
		bib.borrar((int)bib.mazos.size() - 1);
		bib.borrar((int)bib.mazos.size() - 1);
		fprintf(stderr, "[prueba] mazos de torneo: %d validos, copiar y editar OK\n", (int)t.torneos.size());
	}

	fprintf(stderr, "%s (%d fallos)\n", fallos ? "PRUEBA FALLIDA" : "PRUEBA OK", fallos);
	return fallos ? 1 : 0;
}
