/*
 * Yugioh Classic Remaster - "Construir mazo" (rama classic-constructor2, al estilo del editor de Master Duel): SOLO
 * logica. La dibuja interfaz.h (constructor_ui.h, incluido dentro de interfaz::Interfaz); la abre el menu principal
 * (o el panel tactil / tecla M en el PC). Ver docs/constructor.md.
 *
 * Dos pantallas:
 *  - MIS MAZOS: la lista de mazos guardados (mazo::Biblioteca, hasta 20) y "+ Nuevo mazo". El activo (el de los
 *    duelos) lleva una estrella; los invalidos, el motivo.
 *      cruz arriba/abajo  elegir          X  acciones (Editar, Usar en los duelos, Renombrar, Copiar, Borrar)
 *      cuadrado           usar en los duelos                  circulo  volver al menu principal
 *      triangulo          MAZOS DE TORNEO (rama classic-torneo-extras)
 *  - MAZOS DE TORNEO: los 21 mazos de torneo (mazos_torneo.txt) como mazos listos, al estilo de los Structure Deck de
 *    Master Duel: se ven (vista previa) y se copian a MIS MAZOS para usarlos o editarlos.
 *      cruz arriba/abajo  elegir          X  acciones (Copiar a mis mazos, Copiar y editar, Volver)
 *      cuadrado           copiar a mis mazos                  circulo  volver a MIS MAZOS
 *  - EDITOR (pantalla dividida): a la izquierda la carta elegida grande con su texto; en el medio el mazo (principal
 *    40-60 y extra hasta 15, con contadores); a la derecha la coleccion en rejilla con la busqueda y los filtros.
 *      cruz / stick izq   mover (pasando del mazo a la coleccion por los bordes)   stick der izq/der: saltar de lado
 *      X                  agregar una copia (las fusiones van solas al extra)      cuadrado: quitar una copia
 *      L1 / R1            pagina de la coleccion          L2 / R2   clase rapida (Todas, Normal, Efecto...)
 *      triangulo          busqueda y filtros              L3        quitar los filtros
 *      stick der arr/ab   desplazar el texto de la carta  OPTIONS   guardar (solo si el mazo es valido)
 *      circulo            volver a la lista (pregunta si hay cambios sin guardar)
 *  - Capas: acciones del mazo, filtros, teclado en pantalla y preguntas (cruz, X elige, circulo cancela).
 *    Teclado: X escribe, cuadrado borra, triangulo espacio, L1/R1 mayusculas, OPTIONS acepta, circulo cancela.
 * La repeticion de la cruz la hace control::Repetidor (como el resto del menu), en mazo_duelo.h.
 */
#pragma once

#include "mazo.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace constructor {

// botones logicos (los mismos bits que control::Boton, mas los de esta pantalla)
enum : uint32_t {
	C_X = 1u << 0, C_CIRCULO = 1u << 1, C_TRIANGULO = 1u << 2, C_CUADRADO = 1u << 3,
	C_ARRIBA = 1u << 4, C_ABAJO = 1u << 5, C_IZQ = 1u << 6, C_DER = 1u << 7,
	C_L1 = 1u << 8, C_R1 = 1u << 9, C_R3 = 1u << 10, C_OPTIONS = 1u << 11, C_L2 = 1u << 12, C_R2 = 1u << 13, C_L3 = 1u << 14,
	C_RS_IZQ = 1u << 15, C_RS_DER = 1u << 16, C_RS_ARRIBA = 1u << 17, C_RS_ABAJO = 1u << 18,
};

// ---- opciones de los filtros (textos en espanol: se traducen con T)
struct Opcion { const char* texto; const char* clave; };
static const Opcion CLASES[] = { { "Todas", "" }, { "Normal", "normal" }, { "Efecto", "efecto" }, { "Ritual", "ritual" },
	                             { "Fusión", "fusion" }, { "Magia", "magia" }, { "Trampa", "trampa" } };
constexpr int NUM_CLASES = 7;
static const Opcion SUBTIPOS[] = { { "Todos", "" }, { "Normal", "normal" }, { "Campo", "campo" }, { "Equipo", "equipo" },
	                               { "Continua", "continua" }, { "Rápida", "rapida" }, { "Contraataque", "contraataque" }, { "Ritual", "ritual" },
	                               { "Volteo", "volteo" }, { "Toon", "toon" }, { "Unión", "union" }, { "Espíritu", "espiritu" } };
constexpr int NUM_SUBTIPOS = 12;
static const char* const RANGOS[] = { "Todos", "0-999", "1000-1499", "1500-1999", "2000-2499", "2500+" };
constexpr int NUM_RANGOS = 6;
static const char* const ORDENES[] = { "Clase y nombre", "Nombre", "ATK (mayor)", "DEF (mayor)", "Nivel (mayor)" };
constexpr int NUM_ORDENES = 5;
enum FilaFiltro { FF_NOMBRE = 0, FF_CLASE, FF_SUBTIPO, FF_ATRIBUTO, FF_NIVEL, FF_TIPO, FF_ATK, FF_DEF, FF_ORDEN, FF_QUITAR, FF_LISTO, NUM_FILAS_FILTRO };
static const char* const FILAS_FILTRO[] = { "Nombre", "Clase", "Subtipo", "Atributo", "Nivel", "Tipo", "ATK", "DEF", "Orden", "Quitar filtros", "Listo" };

struct Filtros {
	std::string texto;   // busqueda por nombre (lo escrito)
	int clase{ 0 }, subtipo{ 0 }, atributo{ 0 }, nivel{ 0 }, tipo{ 0 }, atk{ 0 }, def{ 0 }, orden{ 0 };
	bool activos() const { return !texto.empty() || clase || subtipo || atributo || nivel || tipo || atk || def; }
};

// ---- teclado en pantalla
static const char* const TECLAS[4][10] = {
	{ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0" },
	{ "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P" },
	{ "A", "S", "D", "F", "G", "H", "J", "K", "L", "Ñ" },
	{ "Z", "X", "C", "V", "B", "N", "M", "-", "'", "." },
};
static const char* const TECLAS_ESP[4] = { "MAYÚS", "ESPACIO", "BORRAR", "ACEPTAR" };
constexpr int FILAS_TECLADO = 5, MAX_TEXTO = 24;
inline int cuantasEnFila(int f) { return f < 4 ? 10 : 4; }

inline std::string minuscula(const std::string& k)
{
	if (k == "Ñ") return "ñ";
	if (k.size() == 1 && k[0] >= 'A' && k[0] <= 'Z') return std::string(1, (char)(k[0] + 32));
	return k;
}
inline void borrarUltimo(std::string& s)
{
	if (s.empty()) return;
	size_t i = s.size() - 1;
	while (i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
	s.erase(i);
}
inline int largoUtf8(const std::string& s)
{
	int n = 0;
	for (unsigned char c : s) n += (c & 0xC0) != 0x80 ? 1 : 0;
	return n;
}

// ---- entradas para el menu principal (rama classic-constructor2; el menu de la rama classic-portada solo las llama).
// Piden abrir la pantalla; la abre el juego al empezar el cuadro siguiente (mazo_duelo.h: avanzarConstructor).
enum PedidoPantalla { PEDIDO_NADA = 0, PEDIDO_CONSTRUCTOR, PEDIDO_LISTA };
inline PedidoPantalla& pedidoPantalla() { static PedidoPantalla p = PEDIDO_NADA; return p; }
// CONSTRUCCION DE DECK: MIS MAZOS y el editor
inline void abrirConstructorMazo() { pedidoPantalla() = PEDIDO_CONSTRUCTOR; }
// LISTA DE CARTAS: la coleccion con busqueda, filtros y detalle, sin mazo; circulo vuelve al menu
inline void abrirListaCartas() { pedidoPantalla() = PEDIDO_LISTA; }
// (lo usa el juego: el pedido pendiente, y lo borra)
inline PedidoPantalla tomarPedido()
{
	const PedidoPantalla p = pedidoPantalla();
	pedidoPantalla() = PEDIDO_NADA;
	return p;
}

namespace detalle {
// Opciones > "Permitir cartas prohibidas" (la pantalla y opciones::permitirProhibidas() son de la rama classic-portada):
// se lee aca la clave de opciones.txt (junto a mazo.txt: /data/yugioh_classic/) para no depender de esa rama.
// "permitir_prohibidas=1" (o "permitir_prohibidas 1"); sin archivo o sin clave, No
inline bool leerPermitirProhibidas(const std::string& rutaOpciones)
{
	std::ifstream f(rutaOpciones);
	std::string l;
	bool si = false;
	while (std::getline(f, l)) {
		if (l.rfind("permitir_prohibidas", 0) != 0) continue;
		const size_t i = l.find_first_of("= ");
		if (i != std::string::npos) si = atoi(l.c_str() + i + 1) != 0;
	}
	return si;
}
}   // namespace detalle
// aplica la opcion a la coleccion (opciones.txt en la carpeta de rutaMazo)
inline void aplicarOpcionProhibidas(mazo::Coleccion& col, const std::string& rutaMazo)
{
	const size_t barra = rutaMazo.find_last_of('/');
	col.ponerPermitirProhibidas(detalle::leerPermitirProhibidas((barra == std::string::npos ? std::string() : rutaMazo.substr(0, barra + 1)) + "opciones.txt"));
}

// rejillas
constexpr int COL_COLUMNAS = 6, COL_FILAS = 4, POR_PAGINA = COL_COLUMNAS * COL_FILAS;
constexpr int MAZO_COLUMNAS = 10, EXTRA_COLUMNAS = 8;

enum Pantalla { P_LISTA = 0, P_EDITOR, P_TORNEO };   // (P_TORNEO: rama classic-torneo-extras)
enum Capa { K_NADA = 0, K_PREGUNTA, K_FILTROS, K_TECLADO };
enum Area { A_COLECCION = 0, A_MAZO };
enum Pregunta { Q_NADA = 0, Q_ACCIONES, Q_BORRAR, Q_SALIR_EDITOR, Q_TORNEO };
enum Teclado { T_BUSCAR = 0, T_NOMBRE_MAZO, T_NOMBRE_NUEVO };
// acciones del mazo (Q_ACCIONES)
enum Accion { AC_EDITAR = 0, AC_USAR, AC_RENOMBRAR, AC_COPIAR, AC_BORRAR, AC_VOLVER };
static const char* const ACCIONES[] = { "Editar", "Usar en los duelos", "Renombrar", "Copiar", "Borrar", "Volver" };

class Constructor {
public:
	mazo::Coleccion* col{ nullptr };
	mazo::Biblioteca* bib{ nullptr };
	bool abierto{ false };
	bool soloVer{ false };   // LISTA DE CARTAS: la coleccion con busqueda, filtros y detalle; sin mazo, sin agregar ni guardar
	// rama classic-galeria: en la lista de cartas, Cuadrado pide la galeria de figuras 3D (1) y R3 "Ver figura" (2);
	// la abre el juego (galeria_juego.h) con la carta elegida
	int pedidoGaleria{ 0 };
	std::string cartaGaleria;
	Pantalla pantalla{ P_LISTA };
	Capa capa{ K_NADA };
	float t{ 0.0f }, tAviso{ 99.0f }, tCursor{ 0.0f }, tCapa{ 0.0f };
	std::string aviso;

	// lista de mazos
	int cursorLista{ 0 };   // 0..mazos.size() (el ultimo = "+ Nuevo mazo")
	// rama classic-torneo-extras: los mazos de torneo (los carga el juego: mazo::leerMazosTorneo) y el elegido
	std::vector<mazo::MazoTorneo> torneos;
	int cursorTorneo{ 0 };

	// editor
	mazo::Mazo mazo;            // el que se esta armando
	int editando{ -1 };         // indice en la biblioteca (-1: nuevo, aun sin guardar)
	bool cambiado{ false };
	Area area{ A_COLECCION };
	int cursor{ 0 };            // indice en "lista" (coleccion)
	bool enExtra{ false };      // cursor del mazo: en el extra (si no, en el principal)
	int cursorMazo{ 0 };        // indice en mazo.principal / mazo.fusion
	std::vector<int> lista;     // indices de col->cartas visibles con los filtros
	Filtros filtros;
	std::vector<std::string> atributos, tipos;   // claves (espanol) de las opciones dinamicas (0 = Todos)
	int scrollTexto{ 0 };       // lineas del texto de la carta desplazadas

	// capas
	Pregunta pregunta{ Q_NADA };
	std::string tituloPregunta;
	std::vector<std::string> opciones;   // (textos en espanol)
	std::vector<int> accionesPregunta;   // Q_ACCIONES: que hace cada opcion
	int opcion{ 0 };
	int filaFiltro{ 0 };
	Teclado modoTeclado{ T_BUSCAR };
	std::string textoTeclado, textoAntes;
	int teclaF{ 1 }, teclaC{ 0 };
	bool mayus{ true };

	// ------------------------------------------------------------------------------------------ abrir / cerrar
	void abrir(mazo::Coleccion& c, mazo::Biblioteca& b)
	{
		col = &c;
		bib = &b;
		abierto = true;
		pantalla = P_LISTA;
		capa = K_NADA;
		t = 0.0f;
		soloVer = false;
		cursorLista = std::clamp(b.activo, 0, (int)b.mazos.size());
		opcionesDinamicas();
		avisar(T("X: acciones del mazo · Cuadrado: usar en los duelos"));
	}
	// LISTA DE CARTAS: la misma rejilla con busqueda, filtros y detalle, sin el mazo (circulo vuelve al menu)
	void abrirLista(mazo::Coleccion& c, mazo::Biblioteca& b)
	{
		col = &c;
		bib = &b;
		abierto = true;
		soloVer = true;
		capa = K_NADA;
		t = 0.0f;
		opcionesDinamicas();
		editarMazo(-1);
		avisar(T("Triángulo: buscar y filtrar · Círculo: volver"));
	}
	void cerrar()
	{
		abierto = false;
		capa = K_NADA;
	}

	// la carta elegida (para el panel de detalle)
	const mazo::Carta* elegida() const
	{
		if (!col || pantalla != P_EDITOR) return nullptr;
		if (area == A_MAZO) {
			const std::vector<std::string>& v = enExtra ? mazo.fusion : mazo.principal;
			if (cursorMazo >= 0 && cursorMazo < (int)v.size()) return col->carta(v[(size_t)cursorMazo]);
			return nullptr;
		}
		if (cursor < 0 || cursor >= (int)lista.size()) return nullptr;
		return &col->cartas[(size_t)lista[(size_t)cursor]];
	}
	// columnas de la rejilla de la coleccion (solo ver: ocupa tambien el lugar del mazo)
	int colCols() const { return soloVer ? 10 : COL_COLUMNAS; }
	int porPagina() const { return colCols() * COL_FILAS; }
	int pagina() const { return cursor / porPagina(); }
	int paginas() const { return std::max(1, ((int)lista.size() + porPagina() - 1) / porPagina()); }
	void conteo(int& monstruos, int& magias, int& trampas) const
	{
		monstruos = magias = trampas = 0;
		for (const std::string& id : mazo.principal) {
			const mazo::Carta* k = col ? col->carta(id) : nullptr;
			if (!k) continue;
			if (k->clase == "magia") magias++;
			else if (k->clase == "trampa") trampas++;
			else monstruos++;
		}
	}
	bool valido(std::string* motivo = nullptr) const { return col && mazo.valido(*col, motivo); }
	bool validoMazo(int i, std::string* motivo = nullptr) const
	{
		return col && bib && i >= 0 && i < (int)bib->mazos.size() && bib->mazos[(size_t)i].valido(*col, motivo);
	}

	// el texto de una opcion de los filtros (ya traducido; los datos de las cartas vienen en el idioma)
	std::string valorFiltro(int fila) const
	{
		switch (fila) {
		case FF_NOMBRE: return filtros.texto.empty() ? T("(cualquiera)") : "\"" + filtros.texto + "\"";
		case FF_CLASE: return T(CLASES[filtros.clase].texto);
		case FF_SUBTIPO: return T(SUBTIPOS[filtros.subtipo].texto);
		case FF_ATRIBUTO: return filtros.atributo ? mostrarAtributo(atributos[(size_t)filtros.atributo]) : T("Todos");
		case FF_NIVEL: return filtros.nivel ? std::to_string(filtros.nivel) : T("Todos");
		case FF_TIPO: return filtros.tipo ? mostrarTipo(tipos[(size_t)filtros.tipo]) : T("Todos");
		case FF_ATK: return T(RANGOS[filtros.atk]);
		case FF_DEF: return T(RANGOS[filtros.def]);
		case FF_ORDEN: return T(ORDENES[filtros.orden]);
		default: return "";
		}
	}
	// el atributo / tipo en el idioma actual (el de la primera carta que lo tiene)
	std::string mostrarAtributo(const std::string& clave) const
	{
		for (const mazo::Carta& k : col->cartas)
			if (k.atributoClave == clave) return k.atributo;
		return clave;
	}
	std::string mostrarTipo(const std::string& clave) const
	{
		for (const mazo::Carta& k : col->cartas)
			if (k.razaClave == clave) return k.raza;
		return clave;
	}

	// ------------------------------------------------------------------------------------------ cada cuadro
	// p: pulsos con los bits de arriba (ya con la repeticion de la cruz)
	void avanzar(float dt, uint32_t p)
	{
		if (!abierto || !col || !bib) return;
		t += dt;
		tAviso += dt;
		tCursor += dt;
		tCapa += dt;
		if (capa == K_PREGUNTA) return avanzarPregunta(p);
		if (capa == K_TECLADO) return avanzarTeclado(p);
		if (capa == K_FILTROS) return avanzarFiltros(p);
		if (pantalla == P_LISTA) return avanzarLista(p);
		if (pantalla == P_TORNEO) return avanzarTorneo(p);
		avanzarEditor(p);
	}

	void avisar(const std::string& s)
	{
		aviso = s;
		tAviso = 0.0f;
	}

	// ---- acciones directas (tambien las usa la prueba de PC)
	void editarMazo(int i)
	{
		pantalla = P_EDITOR;
		capa = K_NADA;
		editando = i;
		mazo = i >= 0 ? bib->mazos[(size_t)i] : mazo::Mazo{};
		cambiado = false;
		area = A_COLECCION;
		cursor = cursorMazo = 0;
		enExtra = false;
		scrollTexto = 0;
		filtrar();
		avisar(T("X agrega, Cuadrado quita, OPTIONS guarda"));
	}
	// guarda el mazo del editor (solo si es valido)
	bool guardar()
	{
		std::string motivo;
		if (!mazo.valido(*col, &motivo)) {
			avisar(T("No se puede guardar: ") + motivo);
			return false;
		}
		if (editando < 0) {
			const int i = bib->agregar(mazo);
			if (i < 0) {
				avisar(T("Ya hay 20 mazos: borra uno"));
				return false;
			}
			editando = i;
			if (!bib->mazoActivo()) bib->usar(i, *col);
		} else {
			bib->mazos[(size_t)editando] = mazo;
			bib->guardarMazo(editando);
		}
		cambiado = false;
		cursorLista = editando;
		avisar(T("Mazo guardado"));
		return true;
	}
	bool usarMazo(int i)
	{
		std::string motivo;
		if (!bib->usar(i, *col, &motivo)) {
			avisar(T("No se puede usar: ") + motivo);
			return false;
		}
		avisar(T("Mazo para los duelos: ") + bib->mazos[(size_t)i].nombre);
		return true;
	}
	void copiarMazo(int i)
	{
		mazo::Mazo m = bib->mazos[(size_t)i];
		m.nombre = (m.nombre + T(" (copia)")).substr(0, 60);
		const int n = bib->agregar(m);
		if (n < 0) {
			avisar(T("Ya hay 20 mazos: borra uno"));
		} else {
			cursorLista = n;
			avisar(T("Mazo copiado"));
		}
	}
	// rama classic-torneo-extras: MAZOS DE TORNEO (Triangulo en MIS MAZOS)
	void abrirTorneos()
	{
		if (torneos.empty()) {
			avisar(T("No hay mazos de torneo"));
			return;
		}
		pantalla = P_TORNEO;
		capa = K_NADA;
		cursorTorneo = std::clamp(cursorTorneo, 0, (int)torneos.size() - 1);
		avisar(T("X: copiar a tus mazos para usarlo o editarlo"));
	}
	// copia el mazo de torneo i a MIS MAZOS (con su nombre en el idioma actual); editar: lo abre en el editor
	int copiarTorneo(int i, bool editar)
	{
		if (i < 0 || i >= (int)torneos.size()) return -1;
		mazo::Mazo m = torneos[(size_t)i].mazo;
		m.nombre = torneos[(size_t)i].nombre(enIngles()).substr(0, 60);
		m.ordenar(*col);
		const int n = bib->agregar(m);
		if (n < 0) {
			avisar(T("Ya hay 20 mazos: borra uno"));
			return -1;
		}
		cursorLista = n;
		if (editar) editarMazo(n);
		avisar(T("Mazo de torneo copiado: ") + m.nombre);
		return n;
	}
	void avanzarTorneo(uint32_t p)
	{
		const int n = (int)torneos.size();
		if (n == 0) {
			pantalla = P_LISTA;
			return;
		}
		if (p & C_ARRIBA) cursorTorneo = (cursorTorneo + n - 1) % n;
		if (p & C_ABAJO) cursorTorneo = (cursorTorneo + 1) % n;
		if (p & (C_ARRIBA | C_ABAJO)) tCursor = 0.0f;
		if (p & C_X) abrirPregunta(Q_TORNEO, torneos[(size_t)cursorTorneo].nombre(enIngles()), { "Copiar a mis mazos", "Copiar y editar", "Volver" });
		if (p & C_CUADRADO) copiarTorneo(cursorTorneo, false);
		if (p & C_CIRCULO) {
			pantalla = P_LISTA;
			capa = K_NADA;
			cursorLista = std::clamp(cursorLista, 0, (int)bib->mazos.size());
		}
	}
	void borrarMazo(int i)
	{
		const std::string nombre = bib->mazos[(size_t)i].nombre;
		bib->borrar(i);
		if (!bib->mazoActivo()) {   // (se borro el activo: el primero valido)
			for (int k = 0; k < (int)bib->mazos.size(); k++)
				if (bib->usar(k, *col)) break;
		}
		cursorLista = std::clamp(cursorLista, 0, (int)bib->mazos.size());
		avisar(T("Mazo borrado: ") + nombre);
	}
	// agrega / quita una copia de la carta elegida
	bool agregarElegida()
	{
		const mazo::Carta* k = elegida();
		if (!k) return false;
		const std::string id = k->id;
		std::string motivo;
		if (!mazo.agregar(*col, id, &motivo)) {
			avisar(motivo);
			return false;
		}
		cambiado = true;
		avisar("+1 " + k->nombre + " (" + std::to_string(mazo.cuantas(id)) + "/" + std::to_string(k->limite) + ")" +
		       (k->clase == "fusion" ? "  ·  " + T("al mazo extra") : ""));
		if (area == A_MAZO) seguirCarta(id);
		return true;
	}
	bool quitarElegida()
	{
		const mazo::Carta* k = elegida();
		if (!k) return false;
		const std::string id = k->id, nombre = k->nombre;
		if (!mazo.quitar(id)) {
			avisar(T("No está en el mazo"));
			return false;
		}
		cambiado = true;
		avisar("-1 " + nombre + " (" + std::to_string(mazo.cuantas(id)) + ")");
		if (area == A_MAZO) {
			const std::vector<std::string>& v = enExtra ? mazo.fusion : mazo.principal;
			if (v.empty()) {
				if (enExtra && !mazo.principal.empty()) {
					enExtra = false;
					cursorMazo = (int)mazo.principal.size() - 1;
				} else if (!enExtra && !mazo.fusion.empty()) {
					enExtra = true;
					cursorMazo = 0;
				} else {
					area = A_COLECCION;
				}
			} else {
				cursorMazo = std::min(cursorMazo, (int)v.size() - 1);
			}
		}
		return true;
	}
	void ponerBusqueda(const std::string& s)
	{
		filtros.texto = s;
		cursor = 0;
		filtrar();
	}

	// ------------------------------------------------------------------------------------------ filtros
	void filtrar()
	{
		lista.clear();
		if (!col) return;
		const std::string busca = mazo::normalizar(filtros.texto);
		for (int i = 0; i < (int)col->cartas.size(); i++) {
			if (pasa(col->cartas[(size_t)i], busca)) lista.push_back(i);
		}
		const auto& cs = col->cartas;
		switch (filtros.orden) {
		case 1: std::stable_sort(lista.begin(), lista.end(), [&](int a, int b) { return cs[(size_t)a].busqueda < cs[(size_t)b].busqueda; }); break;
		case 2: std::stable_sort(lista.begin(), lista.end(), [&](int a, int b) { return cs[(size_t)a].atk > cs[(size_t)b].atk; }); break;
		case 3: std::stable_sort(lista.begin(), lista.end(), [&](int a, int b) { return cs[(size_t)a].def > cs[(size_t)b].def; }); break;
		case 4: std::stable_sort(lista.begin(), lista.end(), [&](int a, int b) { return cs[(size_t)a].nivel > cs[(size_t)b].nivel; }); break;
		default: break;   // (la coleccion ya esta por clase y nombre)
		}
		if (cursor >= (int)lista.size()) cursor = std::max(0, (int)lista.size() - 1);
		if (lista.empty() && area == A_COLECCION && !(mazo.principal.empty() && mazo.fusion.empty())) area = A_MAZO, enExtra = mazo.principal.empty(), cursorMazo = 0;
		tCursor = 0.0f;
	}
	bool pasa(const mazo::Carta& k, const std::string& busca) const
	{
		if (filtros.clase > 0 && k.clase != CLASES[filtros.clase].clave) return false;
		if (filtros.subtipo > 0) {
			const std::string c = SUBTIPOS[filtros.subtipo].clave;
			const std::string& s = k.subtipo;
			const bool sufijo = s.size() > c.size() && s.compare(s.size() - c.size(), c.size(), c) == 0 && s[s.size() - c.size() - 1] == '_';
			if (!(s == c || sufijo)) return false;
		}
		if (filtros.atributo > 0 && k.atributoClave != atributos[(size_t)filtros.atributo]) return false;
		if (filtros.tipo > 0 && k.razaClave != tipos[(size_t)filtros.tipo]) return false;
		const bool monstruo = mazo::esMonstruo(k.clase);
		if (filtros.nivel > 0 && (!monstruo || k.nivel != filtros.nivel)) return false;
		auto enRango = [](int v, int r) {
			static const int LIM[][2] = { { 0, 99999 }, { 0, 999 }, { 1000, 1499 }, { 1500, 1999 }, { 2000, 2499 }, { 2500, 99999 } };
			return v >= LIM[r][0] && v <= LIM[r][1];
		};
		if (filtros.atk > 0 && (!monstruo || !enRango(k.atk, filtros.atk))) return false;
		if (filtros.def > 0 && (!monstruo || !enRango(k.def, filtros.def))) return false;
		// cada palabra buscada tiene que estar en el nombre (sin tildes ni mayusculas)
		size_t a = 0;
		while (a < busca.size()) {
			while (a < busca.size() && busca[a] == ' ') a++;
			size_t b = busca.find(' ', a);
			if (b == std::string::npos) b = busca.size();
			if (b > a && k.busqueda.find(busca.substr(a, b - a)) == std::string::npos) return false;
			a = b;
		}
		return true;
	}
	void quitarFiltros()
	{
		const int orden = filtros.orden;
		filtros = Filtros{};
		filtros.orden = orden;
		cursor = 0;
		filtrar();
	}

private:
	void opcionesDinamicas()
	{
		atributos.assign(1, "");
		tipos.assign(1, "");
		for (const mazo::Carta& k : col->cartas) {
			if (!mazo::esMonstruo(k.clase)) continue;
			if (!k.atributoClave.empty() && std::find(atributos.begin(), atributos.end(), k.atributoClave) == atributos.end()) atributos.push_back(k.atributoClave);
			if (!k.razaClave.empty() && std::find(tipos.begin(), tipos.end(), k.razaClave) == tipos.end()) tipos.push_back(k.razaClave);
		}
		std::sort(atributos.begin() + 1, atributos.end());
		std::sort(tipos.begin() + 1, tipos.end());
	}

	void abrirPregunta(Pregunta q, const std::string& titulo, const std::vector<std::string>& ops, int sel = 0)
	{
		pregunta = q;
		tituloPregunta = titulo;
		opciones = ops;
		opcion = sel;
		capa = K_PREGUNTA;
		tCapa = 0.0f;
	}
	void abrirTeclado(Teclado modo, const std::string& texto)
	{
		modoTeclado = modo;
		textoTeclado = textoAntes = texto;
		teclaF = 1;
		teclaC = 0;
		mayus = texto.empty();
		capa = K_TECLADO;
		tCapa = 0.0f;
	}

	// ---- MIS MAZOS
	void avanzarLista(uint32_t p)
	{
		const int n = (int)bib->mazos.size() + 1;   // (+ "Nuevo mazo")
		if (p & C_ARRIBA) cursorLista = (cursorLista + n - 1) % n;
		if (p & C_ABAJO) cursorLista = (cursorLista + 1) % n;
		if (p & (C_ARRIBA | C_ABAJO)) tCursor = 0.0f;
		const bool nuevo = cursorLista >= (int)bib->mazos.size();
		if (p & C_X) {
			if (nuevo) {
				if ((int)bib->mazos.size() >= mazo::MAX_MAZOS) avisar(T("Ya hay 20 mazos: borra uno"));
				else abrirTeclado(T_NOMBRE_NUEVO, "");
			} else {
				accionesPregunta = { AC_EDITAR, AC_USAR, AC_RENOMBRAR, AC_COPIAR, AC_BORRAR, AC_VOLVER };
				std::vector<std::string> ops;
				for (int a : accionesPregunta) ops.push_back(ACCIONES[a]);
				abrirPregunta(Q_ACCIONES, bib->mazos[(size_t)cursorLista].nombre, ops);
			}
		}
		if ((p & C_CUADRADO) && !nuevo) usarMazo(cursorLista);
		if (p & C_TRIANGULO) abrirTorneos();   // (rama classic-torneo-extras)
		if (p & C_CIRCULO) cerrar();   // al menu principal
	}

	// ---- EDITOR
	void avanzarEditor(uint32_t p)
	{
		const mazo::Carta* antes = elegida();
		moverEditor(p);
		if (p & C_L1) moverPagina(-1);
		if (p & C_R1) moverPagina(1);
		if (p & (C_L2 | C_R2)) {
			filtros.clase = (filtros.clase + ((p & C_R2) ? 1 : NUM_CLASES - 1)) % NUM_CLASES;
			cursor = 0;
			filtrar();
			avisar(T("Clase: ") + T(CLASES[filtros.clase].texto));
		}
		if (p & C_L3) {
			quitarFiltros();
			avisar(T("Sin filtros"));
		}
		if (soloVer) {   // (lista de cartas: sin mazo, sin agregar, quitar ni guardar)
			if (p & (C_CUADRADO | C_R3)) {   // rama classic-galeria
				pedidoGaleria = (p & C_R3) ? 2 : 1;
				const mazo::Carta* k = elegida();
				cartaGaleria = k ? k->id : std::string();
			}
			p &= ~(uint32_t)(C_X | C_CUADRADO | C_OPTIONS | C_RS_IZQ | C_RS_DER);
			if (p & C_CIRCULO) {
				cerrar();
				return;
			}
		}
		if (p & (C_RS_IZQ | C_RS_DER)) cambiarArea((p & C_RS_IZQ) ? A_MAZO : A_COLECCION);
		if (p & C_RS_ABAJO) scrollTexto++;
		if ((p & C_RS_ARRIBA) && scrollTexto > 0) scrollTexto--;
		if (p & C_X) agregarElegida();
		if (p & C_CUADRADO) quitarElegida();
		if (p & C_TRIANGULO) {
			capa = K_FILTROS;
			filaFiltro = 0;
			tCapa = 0.0f;
		}
		if (p & C_OPTIONS) guardar();
		if (p & C_CIRCULO) {
			if (cambiado) abrirPregunta(Q_SALIR_EDITOR, T("Hay cambios sin guardar"), { "Guardar y salir", "Salir sin guardar", "Seguir editando" });
			else volverALista();
		}
		if (elegida() != antes) {
			tCursor = 0.0f;
			scrollTexto = 0;
		}
	}
	void volverALista()
	{
		pantalla = P_LISTA;
		capa = K_NADA;
		if (editando >= 0) cursorLista = editando;
		cursorLista = std::clamp(cursorLista, 0, (int)bib->mazos.size());
	}
	void moverPagina(int d)
	{
		if (lista.empty()) return;
		area = A_COLECCION;
		const int pag = std::clamp(pagina() + d, 0, paginas() - 1);
		cursor = std::min(pag * porPagina() + cursor % porPagina(), (int)lista.size() - 1);
	}
	void cambiarArea(Area a)
	{
		if (a == A_MAZO) {
			if (mazo.principal.empty() && mazo.fusion.empty()) {
				avisar(T("El mazo está vacío"));
				return;
			}
			area = A_MAZO;
			enExtra = enExtra ? !mazo.fusion.empty() : mazo.principal.empty();
			cursorMazo = std::max(0, std::min(cursorMazo, (int)(enExtra ? mazo.fusion : mazo.principal).size() - 1));
		} else if (!lista.empty()) {
			area = A_COLECCION;
		}
	}
	// mantener el cursor del mazo sobre la carta (tras agregar: el mazo se reordena)
	void seguirCarta(const std::string& id)
	{
		const mazo::Carta* k = col->carta(id);
		enExtra = k && k->clase == "fusion";
		const std::vector<std::string>& v = enExtra ? mazo.fusion : mazo.principal;
		auto it = std::find(v.begin(), v.end(), id);
		if (it != v.end()) cursorMazo = (int)(it - v.begin());
	}
	void moverEditor(uint32_t p)
	{
		if (!(p & (C_ARRIBA | C_ABAJO | C_IZQ | C_DER))) return;
		if (area == A_COLECCION) {
			const int n = (int)lista.size();
			if (n == 0) {
				if (p & C_IZQ) cambiarArea(A_MAZO);
				return;
			}
			const int enPag = cursor % porPagina(), pag = cursor / porPagina();
			const int fila = enPag / colCols(), colu = enPag % colCols();
			if ((p & C_IZQ) && colu > 0) {
				cursor--;
			} else if (p & C_IZQ) {   // borde izquierdo: al mazo, a la misma altura
				if (mazo.principal.empty() && mazo.fusion.empty()) return;
				area = A_MAZO;
				const int filasP = ((int)mazo.principal.size() + MAZO_COLUMNAS - 1) / MAZO_COLUMNAS;
				const int f = std::min(fila * 6 / COL_FILAS, std::max(filasP - 1, 0));
				if (mazo.principal.empty() || (fila == COL_FILAS - 1 && !mazo.fusion.empty())) {
					enExtra = true;
					cursorMazo = std::min((int)mazo.fusion.size() - 1, EXTRA_COLUMNAS - 1);
				} else {
					enExtra = false;
					cursorMazo = std::min(f * MAZO_COLUMNAS + MAZO_COLUMNAS - 1, (int)mazo.principal.size() - 1);
				}
				return;
			}
			if (p & C_DER) {
				if (colu < colCols() - 1 && cursor + 1 < n) cursor++;
				else if ((pag + 1) * porPagina() < n) cursor = std::min((pag + 1) * porPagina() + fila * colCols(), n - 1);   // a la pagina siguiente
			}
			if ((p & C_ARRIBA) && cursor >= colCols()) cursor -= colCols();   // (sube tambien a la pagina anterior)
			if (p & C_ABAJO) {
				if (cursor + colCols() < n) cursor += colCols();
				else if (cursor / colCols() < (n - 1) / colCols()) cursor = n - 1;
			}
			cursor = std::clamp(cursor, 0, n - 1);
			return;
		}
		// en el mazo: principal (10 columnas) y abajo el extra (8 columnas)
		const int nP = (int)mazo.principal.size(), nE = (int)mazo.fusion.size();
		const int cols = enExtra ? EXTRA_COLUMNAS : MAZO_COLUMNAS;
		const int n = enExtra ? nE : nP;
		const int fila = cursorMazo / cols, colu = cursorMazo % cols;
		if ((p & C_IZQ) && colu > 0) cursorMazo--;
		if (p & C_DER) {
			if (colu < cols - 1 && cursorMazo + 1 < n) {
				cursorMazo++;
			} else if (!lista.empty()) {   // borde derecho: a la coleccion
				area = A_COLECCION;
				const int filaCol = enExtra ? COL_FILAS - 1 : std::min(fila * COL_FILAS / 6, COL_FILAS - 1);
				cursor = std::min(pagina() * porPagina() + filaCol * colCols(), (int)lista.size() - 1);
				return;
			}
		}
		if (p & C_ARRIBA) {
			if (fila > 0) {
				cursorMazo -= cols;
			} else if (enExtra && nP > 0) {
				enExtra = false;
				cursorMazo = std::min(((nP - 1) / MAZO_COLUMNAS) * MAZO_COLUMNAS + std::min(colu, MAZO_COLUMNAS - 1), nP - 1);
			}
		}
		if (p & C_ABAJO) {
			if (cursorMazo + cols < n) {
				cursorMazo += cols;
			} else if (fila < (n - 1) / cols) {
				cursorMazo = n - 1;
			} else if (!enExtra && nE > 0) {
				enExtra = true;
				cursorMazo = std::min(colu, nE - 1);
			}
		}
	}

	// ---- capas
	void avanzarPregunta(uint32_t p)
	{
		const int n = (int)opciones.size();
		if (p & C_ARRIBA) opcion = (opcion + n - 1) % n;
		if (p & C_ABAJO) opcion = (opcion + 1) % n;
		const bool cancelar = (p & C_CIRCULO) != 0;
		if (!(p & C_X) && !cancelar) return;
		capa = K_NADA;
		const Pregunta q = pregunta;
		pregunta = Q_NADA;
		if (q == Q_ACCIONES) {
			if (cancelar) return;
			switch (accionesPregunta[(size_t)opcion]) {
			case AC_EDITAR: editarMazo(cursorLista); break;
			case AC_USAR: usarMazo(cursorLista); break;
			case AC_RENOMBRAR: abrirTeclado(T_NOMBRE_MAZO, bib->mazos[(size_t)cursorLista].nombre); break;
			case AC_COPIAR: copiarMazo(cursorLista); break;
			case AC_BORRAR: abrirPregunta(Q_BORRAR, T("¿Borrar el mazo?") + " " + bib->mazos[(size_t)cursorLista].nombre, { "Borrar", "Cancelar" }, 1); break;
			default: break;
			}
		} else if (q == Q_TORNEO) {   // (rama classic-torneo-extras)
			if (cancelar || opcion == 2) return;
			copiarTorneo(cursorTorneo, opcion == 1);
		} else if (q == Q_BORRAR) {
			if (!cancelar && opcion == 0) borrarMazo(cursorLista);
		} else if (q == Q_SALIR_EDITOR) {
			if (cancelar || opcion == 2) return;   // seguir editando
			if (opcion == 0) {
				if (guardar()) volverALista();
				return;
			}
			volverALista();   // salir sin guardar
			avisar(T("Cambios descartados"));
		}
	}

	void avanzarFiltros(uint32_t p)
	{
		if (p & C_ARRIBA) filaFiltro = (filaFiltro + NUM_FILAS_FILTRO - 1) % NUM_FILAS_FILTRO;
		if (p & C_ABAJO) filaFiltro = (filaFiltro + 1) % NUM_FILAS_FILTRO;
		const int paso = (p & C_DER) ? 1 : (p & C_IZQ) ? -1 : 0;
		auto ciclar = [&](int& v, int n) { v = (v + paso + n) % n; };
		bool cambio = paso != 0;
		switch (filaFiltro) {
		case FF_CLASE: ciclar(filtros.clase, NUM_CLASES); break;
		case FF_SUBTIPO: ciclar(filtros.subtipo, NUM_SUBTIPOS); break;
		case FF_ATRIBUTO: ciclar(filtros.atributo, (int)atributos.size()); break;
		case FF_NIVEL: ciclar(filtros.nivel, 13); break;
		case FF_TIPO: ciclar(filtros.tipo, (int)tipos.size()); break;
		case FF_ATK: ciclar(filtros.atk, NUM_RANGOS); break;
		case FF_DEF: ciclar(filtros.def, NUM_RANGOS); break;
		case FF_ORDEN: ciclar(filtros.orden, NUM_ORDENES); break;
		default: cambio = false; break;
		}
		if (p & C_X) {
			if (filaFiltro == FF_NOMBRE) {
				abrirTeclado(T_BUSCAR, filtros.texto);
				return;
			}
			if (filaFiltro == FF_QUITAR) quitarFiltros();
			if (filaFiltro == FF_LISTO) {
				capa = K_NADA;
				if (!lista.empty()) area = A_COLECCION;
				return;
			}
		}
		if (p & C_CUADRADO) {   // (cuadrado: esta fila a "Todos")
			switch (filaFiltro) {
			case FF_NOMBRE: filtros.texto.clear(); break;
			case FF_CLASE: filtros.clase = 0; break;
			case FF_SUBTIPO: filtros.subtipo = 0; break;
			case FF_ATRIBUTO: filtros.atributo = 0; break;
			case FF_NIVEL: filtros.nivel = 0; break;
			case FF_TIPO: filtros.tipo = 0; break;
			case FF_ATK: filtros.atk = 0; break;
			case FF_DEF: filtros.def = 0; break;
			case FF_ORDEN: filtros.orden = 0; break;
			default: break;
			}
			cambio = true;
		}
		if (p & C_L3) quitarFiltros();
		if (cambio) {
			cursor = 0;
			filtrar();
		}
		if (p & (C_CIRCULO | C_TRIANGULO)) {
			capa = K_NADA;
			if (!lista.empty()) area = A_COLECCION;
		}
	}

	void avanzarTeclado(uint32_t p)
	{
		const int nF = cuantasEnFila(teclaF);
		if (p & C_IZQ) teclaC = (teclaC + nF - 1) % nF;
		if (p & C_DER) teclaC = (teclaC + 1) % nF;
		if (p & (C_ARRIBA | C_ABAJO)) {
			const int f2 = (teclaF + ((p & C_ABAJO) ? 1 : FILAS_TECLADO - 1)) % FILAS_TECLADO;
			teclaC = std::min(teclaC * cuantasEnFila(f2) / nF, cuantasEnFila(f2) - 1);
			teclaF = f2;
		}
		const std::string antes = textoTeclado;
		bool aceptar = (p & C_OPTIONS) != 0;
		if (p & (C_L1 | C_R1)) mayus = !mayus;
		if (p & C_CUADRADO) borrarUltimo(textoTeclado);
		if ((p & C_TRIANGULO) && largoUtf8(textoTeclado) < MAX_TEXTO) textoTeclado += ' ';
		if (p & C_X) {
			if (teclaF < 4) {
				if (largoUtf8(textoTeclado) < MAX_TEXTO) {
					const std::string k = TECLAS[teclaF][teclaC];
					textoTeclado += mayus ? k : minuscula(k);
					if (modoTeclado != T_BUSCAR && mayus && largoUtf8(textoTeclado) == 1) mayus = false;   // (como un movil)
				}
			} else if (teclaC == 0) {
				mayus = !mayus;
			} else if (teclaC == 1) {
				if (largoUtf8(textoTeclado) < MAX_TEXTO) textoTeclado += ' ';
			} else if (teclaC == 2) {
				borrarUltimo(textoTeclado);
			} else {
				aceptar = true;
			}
		}
		if (modoTeclado == T_BUSCAR && textoTeclado != antes) ponerBusqueda(textoTeclado);   // (busca mientras se escribe)
		if (p & C_CIRCULO) {
			if (modoTeclado == T_BUSCAR) ponerBusqueda(textoAntes);
			capa = modoTeclado == T_BUSCAR ? K_FILTROS : K_NADA;
			return;
		}
		if (!aceptar) return;
		// sin espacios en los bordes
		std::string s = textoTeclado;
		while (!s.empty() && s.back() == ' ') s.pop_back();
		while (!s.empty() && s[0] == ' ') s.erase(0, 1);
		if (modoTeclado == T_BUSCAR) {
			ponerBusqueda(s);
			capa = K_NADA;
			if (!lista.empty()) area = A_COLECCION;
			avisar(std::to_string(lista.size()) + T(" cartas encontradas"));
			return;
		}
		if (s.empty()) {
			avisar(T("El nombre no puede quedar vacío"));
			return;
		}
		capa = K_NADA;
		if (modoTeclado == T_NOMBRE_MAZO) {
			bib->mazos[(size_t)cursorLista].nombre = s;
			bib->guardarMazo(cursorLista);
			avisar(T("Mazo renombrado"));
		} else {   // nuevo: al editor (se guarda cuando sea valido)
			editarMazo(-1);
			mazo.nombre = s;
			cambiado = true;
		}
	}
};

}   // namespace constructor
