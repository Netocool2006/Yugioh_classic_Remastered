// Casos de uso del FRONT (rama classic-ciclos): portada, menu principal, Opciones, Personalizacion, constructor y
// lista de cartas (entrada y salida), elegir rival (historia, libre, desafio, reglas), dialogos, duelo (moneda,
// reparto, fases, detalle, registro y visores, cadena visible, jugables), pausa, resultado (motivos, revancha,
// desbloqueos, final), idioma ES/EN, mando y guardado. Todo con el juego sin GPU (cu_juego.h) y el mando.
#pragma once

#include <functional>

#include "casos_uso.h"
#include "cu_juego.h"

#include <dirent.h>
#include <sys/stat.h>

namespace cuf {

using VD = control::VistaDuelo;

inline std::string leerArchivo(const std::string& ruta)
{
	std::ifstream f(ruta);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
inline bool existe(const std::string& ruta)
{
	struct stat st{};
	return stat(ruta.c_str(), &st) == 0;
}
inline bool contiene(const std::string& s, const std::string& x) { return s.find(x) != std::string::npos; }
// el texto tiene version inglesa (o es igual en los dos idiomas)
inline bool traducido(const std::string& es)
{
	static const std::set<std::string> iguales = { "Auto", "Normal", "English", "Español", "IA", "OK" };
	if (iguales.count(es)) return true;
	return idioma::tabla().count(es) > 0;
}

// ------------------------------------------------------------------------------------------------- portada
inline void portada(Juego& g)
{
	VD& v = g.ctl.vista;
	cu::caso("CU-FE-PORTADA-ARRANQUE", v.pantalla == VD::PORTADA, "al arrancar la pantalla es %d (esperada la portada)", v.pantalla);
	cu::caso("CU-FE-PORTADA-ARRANQUE", g.sonido.musica == "musica_menu", "musica '%s' en la portada", g.sonido.musica.c_str());
	cu::caso("CU-FE-PORTADA-ARRANQUE", g.interfazDuelo.portadaNegro >= 0.99f, "la portada no empieza desde negro");
	g.esperar(0.2f);
	g.apretar(PAD_CROSS, 0.05f);   // (antes de 0.8 s: no cuenta)
	cu::caso("CU-FE-PORTADA-ESPERA", g.portadaSalida < 0.0f && v.pantalla == VD::PORTADA, "X antes del fundido de entrada ya sale de la portada");
	g.esperar(1.0f);
	g.apretar(PAD_OPTIONS, 0.1f);
	cu::caso("CU-FE-PORTADA-SIN-PAUSA", !g.pausa.abierta, "OPTIONS en la portada abrio la pausa");
	g.esperar(1.0f);
	cu::caso("CU-FE-PORTADA-OPTIONS", v.pantalla == VD::MENU, "OPTIONS en la portada no fue al menu (pantalla %d)", v.pantalla);
	g.irAPortada();
	g.esperar(1.0f);
	g.apretar(PAD_CROSS, 0.1f);
	cu::caso("CU-FE-PORTADA-X", g.portadaSalida >= 0.0f || v.pantalla == VD::MENU, "X no empezo el fundido de salida");
	cu::caso("CU-FE-PORTADA-X", std::find(g.sonido.sfxs.begin(), g.sonido.sfxs.end(), "sfx_menu_confirmar") != g.sonido.sfxs.end(), "X no sono");
	g.esperar(1.0f);
	cu::caso("CU-FE-PORTADA-X", v.pantalla == VD::MENU, "X en la portada no llego al menu (pantalla %d)", v.pantalla);
}

// ------------------------------------------------------------------------------------------------- menu principal
inline void irAlMenu(Juego& g)
{
	if (g.constructorMazo.abierto) g.constructorMazo.abierto = false;
	g.pausa.abierta = false;
	g.irAMenuPrincipal();
	g.esperar(0.2f);
}
inline void elegirMenu(Juego& g, int accion)
{
	const std::vector<int> ops = g.accionesMenu();
	for (int k = 0; k < 10 && ops[(size_t)g.menuP.opcion] != accion; k++) g.apretar(PAD_DOWN, 0.15f);
	g.apretar(PAD_CROSS, 0.3f);
}

inline void menu(Juego& g)
{
	VD& v = g.ctl.vista;
	irAlMenu(g);
	const std::vector<std::string> esperado = { "MODO DUELO", "DUELO EN LÍNEA", "CONSTRUCCIÓN DE DECK", "LISTA DE CARTAS", "OPCIONES", "SALIR" };   // (DUELO EN LINEA: rama classic-en-linea)
	cu::caso("CU-FE-MENU-BOTONES-5", v.menuOpciones == esperado, "el menu muestra %d botones (esperados 6 en orden)", (int)v.menuOpciones.size());
	cu::caso("CU-FE-MENU-MUSICA", g.sonido.musica == "musica_menu", "musica '%s' en el menu", g.sonido.musica.c_str());
	// navegar: arriba desde el primero va al ultimo; abajo desde el ultimo vuelve al primero
	g.menuP.opcion = 0;
	g.mostrarMenu();
	g.apretar(PAD_UP, 0.15f);
	cu::caso("CU-FE-MENU-NAVEGAR", g.menuP.opcion == 5 && v.menuOpcion == 5, "arriba desde MODO DUELO fue a %d", g.menuP.opcion);
	g.apretar(PAD_DOWN, 0.15f);
	cu::caso("CU-FE-MENU-NAVEGAR", g.menuP.opcion == 0, "abajo desde SALIR fue a %d", g.menuP.opcion);
	// la cruz mantenida repite (rama classic-menus)
	g.mantener(PAD_DOWN, 0.55f);   // (un paso al apretar y uno o dos de repeticion: 0.40 s y luego cada 0.09 s)
	cu::caso("CU-FE-MENU-NAVEGAR", g.menuP.opcion == 2 || g.menuP.opcion == 3, "mantener abajo 0.55 s movio a %d (esperado 2 o 3)", g.menuP.opcion);
	// MODO DUELO -> elegir rival
	irAlMenu(g);
	g.menuP.opcion = 0;
	elegirMenu(g, Juego::MP_DUELO);
	cu::caso("CU-FE-MENU-DUELO", v.pantalla == VD::ELEGIR_RIVAL, "MODO DUELO no fue a elegir rival (pantalla %d)", v.pantalla);
	// CONSTRUCCION DE DECK -> el constructor (se abre en el cuadro siguiente)
	irAlMenu(g);
	elegirMenu(g, Juego::MP_CONSTRUIR);
	cu::caso("CU-FE-MENU-CONSTRUIR", g.constructorMazo.abierto && !g.constructorMazo.soloVer, "CONSTRUCCION DE DECK no abrio el constructor");
	cu::caso("CU-FE-MENU-CONSTRUIR", g.sonido.musica == "musica_constructor", "musica '%s' en el constructor", g.sonido.musica.c_str());
	cu::caso("CU-FE-CONS-MIS-MAZOS", g.constructorMazo.pantalla == constructor::P_LISTA && !g.biblioteca.mazos.empty(), "el constructor no abrio en MIS MAZOS");
	// (rama classic-torneo-extras) Triangulo: MAZOS DE TORNEO (los 21, listos para copiar); circulo vuelve a MIS MAZOS
	g.apretar(PAD_TRIANGLE, 0.3f);
	cu::caso("CU-FE-CONS-TORNEO-ABRIR", g.constructorMazo.pantalla == constructor::P_TORNEO && g.constructorMazo.torneos.size() == 21,
	         "Triangulo no abrio MAZOS DE TORNEO (pantalla %d, %d mazos)", (int)g.constructorMazo.pantalla, (int)g.constructorMazo.torneos.size());
	{
		int validos = 0;
		for (const mazo::MazoTorneo& t : g.constructorMazo.torneos) validos += t.mazo.valido(g.coleccion) ? 1 : 0;
		cu::caso("CU-FE-CONS-TORNEO-VALIDOS", validos == 21, "%d de %d mazos de torneo validos en la coleccion del jugador", validos, (int)g.constructorMazo.torneos.size());
	}
	g.apretar(PAD_CIRCLE, 0.3f);
	cu::caso("CU-FE-CONS-TORNEO-VOLVER", g.constructorMazo.abierto && g.constructorMazo.pantalla == constructor::P_LISTA, "circulo en MAZOS DE TORNEO no volvio a MIS MAZOS");
	// circulo en MIS MAZOS: vuelve al menu
	g.apretar(PAD_CIRCLE, 0.3f);
	cu::caso("CU-FE-CONS-CERRAR", !g.constructorMazo.abierto && v.pantalla == VD::MENU, "circulo en MIS MAZOS no volvio al menu");
	cu::caso("CU-FE-CONS-CERRAR", g.sonido.musica == "musica_menu", "al cerrar el constructor suena '%s'", g.sonido.musica.c_str());
	// LISTA DE CARTAS -> la coleccion en modo solo ver
	elegirMenu(g, Juego::MP_LISTA);
	cu::caso("CU-FE-MENU-LISTA", g.constructorMazo.abierto && g.constructorMazo.soloVer, "LISTA DE CARTAS no abrio la coleccion en modo solo ver");
	cu::caso("CU-FE-LISTA-CARTAS", g.constructorMazo.lista.size() > 1000, "la lista de cartas muestra %d cartas", (int)g.constructorMazo.lista.size());
	g.apretar(PAD_CIRCLE, 0.3f);
	cu::caso("CU-FE-LISTA-CARTAS", !g.constructorMazo.abierto, "circulo no cerro la lista de cartas");
	// OPCIONES
	irAlMenu(g);
	elegirMenu(g, Juego::MP_OPCIONES);
	cu::caso("CU-FE-MENU-OPCIONES", g.menuP.enOpciones && v.menuEnOpciones && (int)v.menuOpciones.size() == Juego::NUM_OPCIONES, "OPCIONES no abrio las 14 opciones");
	g.apretar(PAD_CIRCLE, 0.2f);
	cu::caso("CU-FE-MENU-OPCIONES", !g.menuP.enOpciones && v.pantalla == VD::MENU, "circulo en Opciones no volvio al menu");
	// SALIR: pide confirmar; otro boton cancela; X dos veces sale
	elegirMenu(g, Juego::MP_SALIR);
	cu::caso("CU-FE-MENU-SALIR", g.menuP.confirmarSalir && v.menuSalir && !g.quit, "SALIR no pidio confirmar");
	g.apretar(PAD_CIRCLE, 0.2f);
	cu::caso("CU-FE-MENU-SALIR-CANCELA", !g.menuP.confirmarSalir && !g.quit, "circulo no cancelo salir");
	g.apretar(PAD_CROSS, 0.2f);
	g.apretar(PAD_CROSS, 0.2f);
	cu::caso("CU-FE-MENU-SALIR", g.quit, "X dos veces en SALIR no cerro el juego");
	g.quit = false;
	// la pausa no se abre en el menu
	g.apretar(PAD_OPTIONS, 0.2f);
	cu::caso("CU-FE-MENU-SIN-PAUSA", !g.pausa.abierta, "OPTIONS en el menu principal abrio la pausa");
}

// ------------------------------------------------------------------------------------------------- Opciones
inline void irAOpcion(Juego& g, int o)
{
	if (!g.menuP.enOpciones) {
		irAlMenu(g);
		elegirMenu(g, Juego::MP_OPCIONES);
	}
	for (int k = 0; k < 14 && g.menuP.opcionOpc != o; k++) g.apretar(PAD_DOWN, 0.12f);
}
inline std::string valorOpcion(Juego& g, int o)
{
	g.mostrarMenu();
	return o < (int)g.ctl.vista.menuValores.size() ? g.ctl.vista.menuValores[(size_t)o] : std::string("?");
}

inline void opcionesMenu(Juego& g, const std::string& dir)
{
	VD& v = g.ctl.vista;
	const std::string archivo = g.rutaOpciones;
	cu::caso("CU-FE-OPC-RUTA", archivo == dir + "opciones.txt", "opciones.txt en '%s' (esperado junto a progreso.txt)", archivo.c_str());
	irAlMenu(g);
	elegirMenu(g, Juego::MP_OPCIONES);
	const char* const NOMBRES[] = { "Volumen de la música", "Volumen de los efectos", "Permitir cartas prohibidas", "Preguntar antes de activar", "Velocidad de animación",
		                            "Dificultad adaptable", "IA", "IA que aprende", "Olvidar lo aprendido", "Figuras 3D", "Idioma", "Aviso de privacidad (en línea)", "Personalización", "Volver" };
	bool orden = (int)v.menuOpciones.size() == Juego::NUM_OPCIONES;
	for (int o = 0; orden && o < Juego::NUM_OPCIONES; o++) orden = v.menuOpciones[(size_t)o] == NOMBRES[o];
	cu::caso("CU-FE-OPC-LISTA", orden, "las opciones no son las 14 esperadas en orden");
	// musica: der/izq de a 1/10, entre 0 y 10, se guarda al instante
	irAOpcion(g, Juego::OP_MUSICA);
	g.sonido.volMusica = 1.0f;
	g.apretar(PAD_LEFT, 0.12f);
	g.apretar(PAD_LEFT, 0.12f);
	g.apretar(PAD_LEFT, 0.12f);
	cu::caso("CU-FE-OPC-MUSICA", fabsf(g.sonido.volMusica - 0.7f) < 0.01f && valorOpcion(g, Juego::OP_MUSICA) == "7 / 10", "volumen de la musica %.2f ('%s') tras 3 a la izquierda",
	         g.sonido.volMusica.load(), valorOpcion(g, Juego::OP_MUSICA).c_str());
	cu::caso("CU-FE-OPC-MUSICA", contiene(leerArchivo(archivo), "musica=0.70"), "opciones.txt no guardo musica=0.70");
	for (int k = 0; k < 6; k++) g.apretar(PAD_RIGHT, 0.12f);
	cu::caso("CU-FE-OPC-MUSICA", fabsf(g.sonido.volMusica - 1.0f) < 0.01f, "el volumen pasa de 10 (%.2f)", g.sonido.volMusica.load());
	for (int k = 0; k < 12; k++) g.apretar(PAD_LEFT, 0.12f);
	cu::caso("CU-FE-OPC-MUSICA", g.sonido.volMusica.load() == 0.0f, "el volumen baja de 0 (%.2f)", g.sonido.volMusica.load());
	g.sonido.volMusica = 0.8f;
	// efectos: igual y suena de prueba
	irAOpcion(g, Juego::OP_EFECTOS);
	g.sonido.volEfectos = 0.5f;
	g.sonido.sfxs.clear();
	g.apretar(PAD_RIGHT, 0.12f);
	cu::caso("CU-FE-OPC-EFECTOS", fabsf(g.sonido.volEfectos - 0.6f) < 0.01f && contiene(leerArchivo(archivo), "efectos=0.60"), "efectos %.2f tras uno a la derecha", g.sonido.volEfectos.load());
	cu::caso("CU-FE-OPC-EFECTOS", std::find(g.sonido.sfxs.begin(), g.sonido.sfxs.end(), "sfx_menu_confirmar") != g.sonido.sfxs.end(), "el volumen de efectos no suena de prueba");
	// permitir prohibidas: X cambia; el constructor lo aplica a la coleccion
	irAOpcion(g, Juego::OP_PROHIBIDAS);
	const bool antesP = opciones::permitirProhibidas();
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-OPC-PROHIBIDAS", opciones::permitirProhibidas() != antesP && valorOpcion(g, Juego::OP_PROHIBIDAS) == (antesP ? "No" : "Sí"), "X no cambio Permitir prohibidas");
	cu::caso("CU-FE-OPC-PROHIBIDAS", contiene(leerArchivo(archivo), std::string("permitir_prohibidas=") + (antesP ? "0" : "1")), "no se guardo permitir_prohibidas");
	g.leerOpcionBanlist();
	const mazo::Carta* prohibida = nullptr;
	for (const mazo::Carta& k : g.coleccion.cartas)
		if (k.limiteBase == 0 && !k.firma) prohibida = &k;
	cu::caso("CU-FE-OPC-PROHIBIDAS", g.coleccion.permitirProhibidas == opciones::permitirProhibidas(), "la coleccion no aplico la opcion");
	if (prohibida) cu::caso("CU-FE-OPC-PROHIBIDAS", prohibida->limite == (opciones::permitirProhibidas() ? 3 : 0), "%s: limite %d con la opcion en %d", prohibida->id.c_str(), prohibida->limite, (int)opciones::permitirProhibidas());
	irAOpcion(g, Juego::OP_PROHIBIDAS);
	g.apretar(PAD_CROSS, 0.12f);   // (de vuelta)
	g.leerOpcionBanlist();
	if (prohibida) cu::caso("CU-FE-OPC-PROHIBIDAS", prohibida->limite == (opciones::permitirProhibidas() ? 3 : 0), "%s: limite %d al volver la opcion", prohibida->id.c_str(), prohibida->limite);
	// preguntar antes de activar: Auto -> Siempre -> Nunca -> Auto
	irAOpcion(g, Juego::OP_PREGUNTAR);
	opciones::preguntar() = opciones::PREGUNTAR_AUTO;
	const char* const PREG[] = { "Auto", "Siempre", "Nunca" };
	bool ciclo = valorOpcion(g, Juego::OP_PREGUNTAR) == "Auto";
	for (int k = 1; k <= 3; k++) {
		g.apretar(PAD_CROSS, 0.12f);
		ciclo = ciclo && valorOpcion(g, Juego::OP_PREGUNTAR) == PREG[k % 3] && opciones::preguntarActivacion() == k % 3;
	}
	cu::caso("CU-FE-OPC-PREGUNTAR", ciclo, "Preguntar antes de activar no recorre Auto, Siempre, Nunca");
	g.apretar(PAD_LEFT, 0.12f);
	cu::caso("CU-FE-OPC-PREGUNTAR", opciones::preguntarActivacion() == opciones::PREGUNTAR_NUNCA && contiene(leerArchivo(archivo), "preguntar_activacion=2"),
	         "izquierda no vuelve a Nunca o no se guardo");
	g.apretar(PAD_RIGHT, 0.12f);
	// velocidad de animacion: Normal -> Rapida -> Muy rapida; el duelo la usa (velocidad::leer)
	irAOpcion(g, Juego::OP_VELOCIDAD);
	opciones::velocidad() = opciones::VELOCIDAD_NORMAL;
	float factorAntes = 0.0f;
	bool sube = true;
	for (int k = 0; k < 3; k++) {
		g.leerVelocidad();
		sube = sube && velocidad::nivel() == opciones::velocidadAnim() && velocidad::factor() > factorAntes;
		factorAntes = velocidad::factor();
		if (k < 2) g.apretar(PAD_CROSS, 0.12f);
	}
	cu::caso("CU-FE-OPC-VELOCIDAD", sube && valorOpcion(g, Juego::OP_VELOCIDAD) == "Muy rápida", "la velocidad no sube con cada paso (o el duelo no la lee)");
	g.apretar(PAD_CROSS, 0.12f);
	g.leerVelocidad();
	cu::caso("CU-FE-OPC-VELOCIDAD", velocidad::nivel() == 0 && valorOpcion(g, Juego::OP_VELOCIDAD) == "Normal", "la velocidad no vuelve a Normal");
	// dificultad adaptable e IA: en variedad.txt
	irAOpcion(g, Juego::OP_ADAPTABLE);
	const bool adapt = g.memoriaIA.adaptable;
	g.apretar(PAD_CROSS, 0.12f);
	{
		ia::Memoria m;
		m.leer(g.memoriaIA.ruta);
		cu::caso("CU-FE-OPC-ADAPTABLE", g.memoriaIA.adaptable != adapt && m.adaptable == g.memoriaIA.adaptable, "Dificultad adaptable no cambio o no se guardo en variedad.txt");
		cu::caso("CU-FE-OPC-ADAPTABLE", g.memoriaIA.adaptable || g.memoriaIA.dificultad() == 0.0f, "apagada, la dificultad no es 0");
	}
	g.apretar(PAD_CROSS, 0.12f);
	irAOpcion(g, Juego::OP_IA);
	const bool humana = g.memoriaIA.humana;
	g.apretar(PAD_CROSS, 0.12f);
	{
		ia::Memoria m;
		m.leer(g.memoriaIA.ruta);
		cu::caso("CU-FE-OPC-IA", g.memoriaIA.humana != humana && m.humana == g.memoriaIA.humana && valorOpcion(g, Juego::OP_IA) == (g.memoriaIA.humana ? "Humana" : "Clásica"),
		         "IA Clasica/Humana no cambio o no se guardo");
	}
	g.apretar(PAD_CROSS, 0.12f);
	// rama classic-ia-red: IA que aprende (Si/No en variedad.txt; No = sin red) y Olvidar lo aprendido
	irAOpcion(g, Juego::OP_APRENDE);
	const bool aprende = g.memoriaIA.aprende;
	g.apretar(PAD_CROSS, 0.12f);
	{
		ia::Memoria m;
		m.leer(g.memoriaIA.ruta);
		cu::caso("CU-FE-OPC-APRENDE", g.memoriaIA.aprende != aprende && m.aprende == g.memoriaIA.aprende && g.aprendizIA.activo == g.memoriaIA.aprende &&
		                                   valorOpcion(g, Juego::OP_APRENDE) == (g.memoriaIA.aprende ? "Sí" : "No"),
		         "IA que aprende no cambio, no se guardo en variedad.txt o el aprendiz no la siguio");
		cu::caso("CU-FE-OPC-APRENDE", g.memoriaIA.aprende || g.aprendizIA.red() == nullptr, "apagada, la IA sigue usando la red");
	}
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-OPC-APRENDE", g.memoriaIA.aprende == aprende, "IA que aprende no volvio a su valor");
	{
		const std::string jug = g.aprendizIA.rutaJugador(), due = g.aprendizIA.rutaDuelos();
		guardado::guardarAtomico(jug, "basura");
		guardado::guardarAtomico(due, "basura");
		irAOpcion(g, Juego::OP_OLVIDAR);
		g.apretar(PAD_CROSS, 0.12f);
		cu::caso("CU-FE-OPC-OLVIDAR", !existe(jug) && !existe(due) && !g.aprendizIA.hayJugador && g.aprendizIA.grabados.duelos.empty() &&
		                                   valorOpcion(g, Juego::OP_OLVIDAR) == "Hecho",
		         "Olvidar lo aprendido no borro %s / %s o no dice Hecho", jug.c_str(), due.c_str());
	}
	// figuras 3D
	irAOpcion(g, Juego::OP_FIGURAS);
	const bool fig = g.figuras3D;
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-OPC-FIGURAS", g.figuras3D != fig && contiene(leerArchivo(archivo), std::string("figuras_3d=") + (g.figuras3D ? "1" : "0")), "Figuras 3D no cambio o no se guardo");
	g.apretar(PAD_CROSS, 0.12f);
	// idioma: en caliente y en idioma.txt
	irAOpcion(g, Juego::OP_IDIOMA);
	const int ver = idioma::version();
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-OPC-IDIOMA", idioma::ingles() && idioma::version() == ver + 1 && valorOpcion(g, Juego::OP_IDIOMA) == "English", "Idioma no paso a ingles");
	cu::caso("CU-FE-OPC-IDIOMA", leerArchivo(dir + "idioma.txt") == "en\n", "idioma.txt no dice en");
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-OPC-IDIOMA", !idioma::ingles() && leerArchivo(dir + "idioma.txt") == "es\n", "Idioma no volvio a espanol");
	// Personalizacion: X la abre
	irAOpcion(g, Juego::OP_PERSONALIZACION);
	g.apretar(PAD_CROSS, 0.15f);
	cu::caso("CU-FE-OPC-PERSONALIZACION", cosmeticos::vista().pantalla == cosmeticos::P_MENU, "X en Personalizacion no la abrio");
	g.apretar(PAD_CIRCLE, 0.15f);
	cu::caso("CU-FE-PERSO-MENU", cosmeticos::vista().pantalla == cosmeticos::P_CERRADA && g.menuP.enOpciones, "circulo en Personalizacion no volvio a Opciones");
	// Volver (X) guarda y vuelve
	irAOpcion(g, Juego::OP_VOLVER);
	g.apretar(PAD_CROSS, 0.15f);
	cu::caso("CU-FE-OPC-VOLVER", !g.menuP.enOpciones && v.pantalla == VD::MENU, "X en Volver no volvio al menu");
	// guardar y cargar: todo vuelve igual al leer opciones.txt de nuevo; las claves de otras partes se conservan
	{
		std::string s = leerArchivo(archivo);
		guardado::guardarAtomico(archivo, s + "clave_ajena=7\n");
		g.sonido.volMusica = 0.3f;
		g.sonido.volEfectos = 0.9f;
		opciones::prohibidas() = true;
		opciones::preguntar() = 1;
		opciones::velocidad() = 2;
		g.figuras3D = false;
		g.guardarOpciones();
		g.sonido.volMusica = g.sonido.volEfectos = 0.0f;
		opciones::prohibidas() = false;
		opciones::preguntar() = 0;
		opciones::velocidad() = 0;
		g.figuras3D = true;
		g.leerOpciones();
		cu::caso("CU-FE-OPC-GUARDAR-CARGAR", fabsf(g.sonido.volMusica - 0.3f) < 0.01f && fabsf(g.sonido.volEfectos - 0.9f) < 0.01f && opciones::permitirProhibidas() &&
		                                         opciones::preguntarActivacion() == 1 && opciones::velocidadAnim() == 2 && !g.figuras3D,
		         "al leer opciones.txt no vuelven los valores guardados (musica %.2f efectos %.2f prohibidas %d preguntar %d velocidad %d figuras %d)", g.sonido.volMusica.load(),
		         g.sonido.volEfectos.load(), (int)opciones::permitirProhibidas(), opciones::preguntarActivacion(), opciones::velocidadAnim(), (int)g.figuras3D);
		cu::caso("CU-FE-OPC-CLAVES-AJENAS", contiene(leerArchivo(archivo), "clave_ajena=7"), "guardar las opciones borro una clave de otra parte del juego");
		cu::caso("CU-FE-OPC-GUARDADO-ATOMICO", !existe(archivo + ".tmp"), "quedo opciones.txt.tmp");
		opciones::prohibidas() = false;
		opciones::preguntar() = 0;
		opciones::velocidad() = 0;
		g.figuras3D = true;
		g.sonido.volMusica = g.sonido.volEfectos = 0.8f;
		g.guardarOpciones();
	}
}

// ------------------------------------------------------------------------------------------------- Personalizacion
inline void personalizacion(Juego& g, const std::string& dir)
{
	cosmeticos::VistaPerso& pv = cosmeticos::vista();
	cosmeticos::Estado& e = cosmeticos::estado();
	irAOpcion(g, Juego::OP_PERSONALIZACION);
	g.apretar(PAD_CROSS, 0.15f);
	cu::caso("CU-FE-PERSO-MENU", pv.pantalla == cosmeticos::P_MENU && pv.opcion == 0, "Personalizacion no abrio su menu");
	// funda: sin vencer a nadie solo la clasica esta libre
	g.historia.vencidos.clear();
	g.actualizarDesbloqueos();
	pv.opcion = cosmeticos::PERSO_FUNDA;
	g.apretar(PAD_CROSS, 0.15f);
	cu::caso("CU-FE-PERSO-FUNDA", pv.pantalla == cosmeticos::P_FUNDA, "X en Funda no abrio las fundas");
	e.funda = cosmeticos::F_CLASICA;
	pv.sel = 0;
	g.apretar(PAD_RIGHT, 0.12f);
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-PERSO-FUNDA-BLOQUEADA", e.funda == cosmeticos::F_CLASICA && !pv.aviso.empty(), "se equipo una funda bloqueada (o no hubo aviso)");
	cu::caso("CU-FE-PERSO-FUNDA-BLOQUEADA", !e.fundaPorEs[1].empty() && e.fundaPorEs[1] != cosmeticos::FUNDAS[1].personaje, "el aviso no tiene el nombre del personaje ('%s')", e.fundaPorEs[1].c_str());
	g.historia.vencidos.insert(cosmeticos::FUNDAS[1].personaje);
	g.actualizarDesbloqueos();
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-PERSO-FUNDA", e.funda == 1 && cosmeticos::fundaVisible() == 1, "vencido %s, su funda no se equipo", cosmeticos::FUNDAS[1].personaje);
	cu::caso("CU-FE-PERSO-FUNDA", contiene(leerArchivo(dir + "opciones.txt"), std::string("funda=") + cosmeticos::FUNDAS[1].clave), "la funda no se guardo");
	g.apretar(PAD_DOWN, 0.12f);   // (5 columnas: abajo salta 5)
	cu::caso("CU-FE-PERSO-FUNDA", pv.sel == 6, "abajo en la rejilla de fundas fue a %d (esperado 6)", pv.sel);
	g.apretar(PAD_CIRCLE, 0.12f);
	cu::caso("CU-FE-PERSO-MENU", pv.pantalla == cosmeticos::P_MENU, "circulo en fundas no volvio al menu de Personalizacion");
	// tapete
	pv.opcion = cosmeticos::PERSO_TAPETE;
	g.apretar(PAD_CROSS, 0.15f);
	cu::caso("CU-FE-PERSO-TAPETE", pv.pantalla == cosmeticos::P_TAPETE, "X en Tapete no abrio los tapetes");
	pv.sel = 0;
	g.apretar(PAD_DOWN, 0.12f);
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-PERSO-TAPETE-BLOQUEADO", e.tapete == cosmeticos::T_AUTO && !pv.aviso.empty(), "se equipo un tapete bloqueado");
	g.historia.vencidos.insert(cosmeticos::TAPETES[1].personaje);
	g.actualizarDesbloqueos();
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-PERSO-TAPETE", e.tapete == 1 && cosmeticos::tapeteVisible() == 1 && contiene(leerArchivo(dir + "opciones.txt"), "tapete=pradera"), "el tapete no se equipo o no se guardo");
	g.apretar(PAD_CIRCLE, 0.12f);
	// controles: 3 pestannas (R1/L1) y filas
	pv.opcion = cosmeticos::PERSO_CONTROLES;
	g.apretar(PAD_CROSS, 0.15f);
	cu::caso("CU-FE-PERSO-CONTROLES", pv.pantalla == cosmeticos::P_CONTROLES && pv.pestana == cosmeticos::PEST_MENUS, "Controles no abrio en la pestanna de menus (fuera del duelo)");
	g.apretar(PAD_R1, 0.12f);
	const int p1 = pv.pestana;
	g.apretar(PAD_R1, 0.12f);
	const int p2 = pv.pestana;
	g.apretar(PAD_L1, 0.12f);
	cu::caso("CU-FE-PERSO-CONTROLES", p1 == cosmeticos::PEST_DUELO && p2 == cosmeticos::PEST_CONSTRUCTOR && pv.pestana == cosmeticos::PEST_DUELO, "R1/L1 no recorren las 3 pestannas (%d %d %d)", p1, p2, pv.pestana);
	const int n = (int)cosmeticos::controles(pv.pestana).size();
	g.apretar(PAD_UP, 0.12f);
	cu::caso("CU-FE-PERSO-CONTROLES", pv.sel == n - 1, "arriba en la primera fila no fue a la ultima");
	g.apretar(PAD_CIRCLE, 0.12f);
	// estadisticas
	estadisticas::registrar("p:tea", "Téa", "Téa", 0, "Mazo X", { "dark_magician", "dark_magician", "raigeki" });
	pv.opcion = cosmeticos::PERSO_ESTADISTICAS;
	g.apretar(PAD_CROSS, 0.15f);
	const estadisticas::Datos& d = estadisticas::datos();
	cu::caso("CU-FE-PERSO-ESTADISTICAS", pv.pantalla == cosmeticos::P_ESTADISTICAS && d.firmaTotal > 0 && d.firmaLibres <= d.firmaTotal, "Estadisticas no abrio o no cuenta las firma (%d / %d)", d.firmaLibres, d.firmaTotal);
	const auto* top = estadisticas::masAlto(d.cartas);
	cu::caso("CU-FE-PERSO-ESTADISTICAS", top && top->first == "dark_magician" && top->second >= 2, "la carta mas jugada no es dark_magician");
	g.apretar(PAD_CIRCLE, 0.12f);
	pv.opcion = cosmeticos::PERSO_VOLVER;
	g.apretar(PAD_CROSS, 0.12f);
	cu::caso("CU-FE-PERSO-MENU", pv.pantalla == cosmeticos::P_CERRADA, "X en Volver no cerro Personalizacion");
	g.apretar(PAD_CIRCLE, 0.12f);
	// camara recordada (L1/R1 en el duelo la guarda; al arrancar se usa)
	g.recordarCamara(2);
	cu::caso("CU-FE-PERSO-CAMARA", contiene(leerArchivo(dir + "opciones.txt"), "camara=2"), "la camara no se guardo");
	g.modoCamara = 0;
	cosmeticos::leer(dir + "opciones.txt");
	g.aplicarCamaraGuardada();
	cu::caso("CU-FE-PERSO-CAMARA", g.modoCamara == 2, "al arrancar no se uso la camara guardada (%d)", g.modoCamara);
	// lo de la Personalizacion vuelve igual al leer
	e.funda = 0;
	e.tapete = 0;
	cosmeticos::leer(dir + "opciones.txt");
	cu::caso("CU-FE-PERSO-GUARDAR-CARGAR", e.funda == 1 && e.tapete == 1, "al leer opciones.txt la funda/tapete no vuelven (%d %d)", e.funda, e.tapete);
	// dejar todo como estaba
	g.historia.vencidos.clear();
	g.actualizarDesbloqueos();
	e.funda = e.tapete = 0;
	cosmeticos::guardar();
}

// ------------------------------------------------------------------------------------------------- elegir rival
inline void irARivales(Juego& g, int modo)
{
	irAlMenu(g);
	elegirMenu(g, Juego::MP_DUELO);
	if (g.ctl.vista.modo != modo) g.apretar(PAD_TRIANGLE, 0.2f);
}

inline void elegirRival(Juego& g)
{
	VD& v = g.ctl.vista;
	irARivales(g, VD::HISTORIA);
	cu::caso("CU-FE-RIVAL-HISTORIA", v.modo == VD::HISTORIA && v.pantalla == VD::ELEGIR_RIVAL, "MODO DUELO no abrio el modo historia");
	cu::caso("CU-FE-RIVAL-HISTORIA", v.saga == 0 && !v.fila.empty() && v.personaje == v.fila[0] && v.sagaAbierta, "sin vencidos no esta en el primero de la primera saga ('%s')",
	         v.personaje.c_str());
	cu::caso("CU-FE-RIVAL-HISTORIA", g.mazoRivalIA.rival && g.mazoRivalIA.principal.size() >= 40, "el personaje no tiene mazo");
	const std::string p0 = v.personaje;
	g.apretar(PAD_RIGHT, 0.15f);
	cu::caso("CU-FE-RIVAL-HISTORIA-NAVEGAR", v.idx == 1 && v.personaje != p0 && g.mazoRivalIA.rival, "derecha no paso al siguiente personaje");
	g.apretar(PAD_LEFT, 0.15f);
	cu::caso("CU-FE-RIVAL-HISTORIA-NAVEGAR", v.idx == 0 && v.personaje == p0, "izquierda no volvio al primero");
	g.apretar(PAD_DOWN, 0.15f);
	cu::caso("CU-FE-RIVAL-HISTORIA-NAVEGAR", v.saga == 1 && !v.sagaAbierta, "abajo no paso a la saga 2 (cerrada sin vencer la 1)");
	cu::caso("CU-FE-RIVAL-SAGA-CERRADA", !v.filaEstado.empty() && v.filaEstado[0] == 0, "los de la saga cerrada no se ven cerrados");
	g.ctl.aviso.clear();
	g.apretar(PAD_CROSS, 0.2f);
	cu::caso("CU-FE-RIVAL-SAGA-CERRADA", v.pantalla == VD::ELEGIR_RIVAL && !historia_dialogos::escena().activa() && !g.ctl.aviso.empty(), "X en una saga cerrada empezo algo o no aviso");
	cu::caso("CU-FE-IDIOMA-AVISOS", traducido(g.ctl.aviso), "el aviso '%s' no tiene traduccion", g.ctl.aviso.c_str());
	g.apretar(PAD_UP, 0.15f);
	cu::caso("CU-FE-RIVAL-HISTORIA-NAVEGAR", v.saga == 0 && v.sagaAbierta, "arriba no volvio a la saga 1");
	const uint32_t s0 = g.semillaRival;
	g.apretar(PAD_SQUARE, 0.15f);
	cu::caso("CU-FE-RIVAL-VARIANTE", g.semillaRival != s0 && g.mazoRivalIA.principal.size() >= 40, "cuadrado no armo otra variante del mazo");
	// reglas (R1): paginas con la cruz, circulo cierra
	tutorial::Estado& tut = tutorial::estado();
	g.apretar(PAD_R1, 0.15f);
	cu::caso("CU-FE-RIVAL-REGLAS", tut.abierto && tut.pagina == 0, "R1 no abrio las reglas");
	g.apretar(PAD_RIGHT, 0.15f);
	cu::caso("CU-FE-RIVAL-REGLAS", tut.pagina == 1, "derecha no paso de pagina");
	for (int k = 0; k < tutorial::NUM_PAGINAS + 2; k++) g.apretar(PAD_RIGHT, 0.1f);
	cu::caso("CU-FE-RIVAL-REGLAS", tut.pagina == tutorial::NUM_PAGINAS - 1, "las reglas pasan de la ultima pagina");
	g.apretar(PAD_LEFT, 0.1f);
	cu::caso("CU-FE-RIVAL-REGLAS", tut.pagina == tutorial::NUM_PAGINAS - 2, "izquierda no volvio una pagina");
	g.apretar(PAD_CIRCLE, 0.15f);
	cu::caso("CU-FE-RIVAL-REGLAS", !tut.abierto && v.pantalla == VD::ELEGIR_RIVAL, "circulo no cerro las reglas (o salio de elegir rival)");
	// desafio (L1): solo con la historia terminada
	historia_dialogos::desafioDisponible() = false;
	historia_dialogos::desafioActivo() = false;
	g.apretar(PAD_L1, 0.15f);
	cu::caso("CU-FE-RIVAL-DESAFIO", !historia_dialogos::desafioActivo(), "L1 activo el desafio sin terminar la historia");
	historia_dialogos::desafioDisponible() = true;
	g.apretar(PAD_L1, 0.15f);
	cu::caso("CU-FE-RIVAL-DESAFIO", historia_dialogos::desafioActivo(), "L1 no activo el desafio con la historia terminada");
	g.apretar(PAD_L1, 0.15f);
	cu::caso("CU-FE-RIVAL-DESAFIO", !historia_dialogos::desafioActivo(), "L1 no apago el desafio");
	historia_dialogos::desafioDisponible() = false;
	// triangulo: duelo libre (por nivel)
	g.apretar(PAD_TRIANGLE, 0.15f);
	cu::caso("CU-FE-RIVAL-CAMBIO-MODO", v.modo == VD::LIBRE && v.personaje.empty() && !v.rivalEs.empty() && v.rivalesNivel > 0, "triangulo no paso al duelo libre");
	g.apretar(PAD_TRIANGLE, 0.15f);
	cu::caso("CU-FE-RIVAL-CAMBIO-MODO", v.modo == VD::HISTORIA && !v.personaje.empty(), "triangulo no volvio a la historia");
	g.apretar(PAD_TRIANGLE, 0.15f);
	// niveles: hasta el desbloqueado
	v.nivelMax = 1;
	v.nivel = 1;
	g.elegirRival(true);
	g.apretar(PAD_UP, 0.15f);
	cu::caso("CU-FE-RIVAL-LIBRE-NIVELES", v.nivel == 1, "arriba subio a un nivel bloqueado (%d)", v.nivel);
	v.nivelMax = 3;
	g.apretar(PAD_UP, 0.15f);
	g.apretar(PAD_UP, 0.15f);
	g.apretar(PAD_UP, 0.15f);
	cu::caso("CU-FE-RIVAL-LIBRE-NIVELES", v.nivel == 3, "con el nivel 3 desbloqueado arriba llego a %d", v.nivel);
	cu::caso("CU-FE-RIVAL-LIBRE-NIVELES", g.mazoRivalIA.rival && g.mazoRivalIA.rival->nivel == 3, "el rival no es del nivel 3");
	g.apretar(PAD_DOWN, 0.15f);
	cu::caso("CU-FE-RIVAL-LIBRE-NIVELES", v.nivel == 2, "abajo no bajo un nivel");
	for (int k = 0; k < 4; k++) g.apretar(PAD_DOWN, 0.1f);
	cu::caso("CU-FE-RIVAL-LIBRE-NIVELES", v.nivel == 1, "el nivel baja de 1");
	if (v.rivalesNivel > 1) {
		const std::string r0 = v.rivalEs;
		const int i0 = g.rivalIdx;
		g.apretar(PAD_RIGHT, 0.15f);
		cu::caso("CU-FE-RIVAL-LIBRE-RIVAL", g.rivalIdx == (i0 + 1) % v.rivalesNivel && v.rivalIdx == g.rivalIdx, "derecha no paso al siguiente rival del nivel");
		g.apretar(PAD_LEFT, 0.15f);
		cu::caso("CU-FE-RIVAL-LIBRE-RIVAL", g.rivalIdx == i0 && v.rivalEs == r0, "izquierda no volvio al rival anterior");
	} else {
		cu::caso("CU-FE-RIVAL-LIBRE-RIVAL", false, "el nivel 1 tiene un solo rival");
	}
	g.apretar(PAD_SQUARE, 0.15f);
	cu::caso("CU-FE-RIVAL-LIBRE-AZAR", g.rivalIdx >= 0 && g.rivalIdx < v.rivalesNivel && g.mazoRivalIA.rival, "cuadrado no eligio un rival al azar del nivel");
	g.apretar(PAD_CIRCLE, 0.15f);
	cu::caso("CU-FE-RIVAL-VOLVER", v.pantalla == VD::MENU, "circulo en elegir rival no volvio al menu");
}

// ------------------------------------------------------------------------------------------------- dialogos
inline void dialogos(Juego& g)
{
	VD& v = g.ctl.vista;
	historia_dialogos::Escena& e = historia_dialogos::escena();
	// contenido: cada personaje con intro, linea de victoria y de derrota en los dos idiomas
	int sinDialogo = 0;
	std::string faltan;
	for (const personajes::Personaje& x : g.historia.lista) {
		const historia_dialogos::Personaje* p = historia_dialogos::base().de(x.id);
		bool ok = p && !p->intro.empty() && !p->gana.es.empty() && !p->gana.en.empty() && !p->pierde.es.empty() && !p->pierde.en.empty();
		if (p)
			for (const historia_dialogos::Linea& l : p->intro) ok = ok && !l.es.empty() && !l.en.empty();
		if (!ok) {
			sinDialogo++;
			faltan += x.id + " ";
		}
	}
	cu::caso("CU-FE-DIALOGO-CONTENIDO", sinDialogo == 0, "%d personajes sin dialogo completo ES/EN: %s", sinDialogo, faltan.c_str());
	const historia_dialogos::Dialogos& b = historia_dialogos::base();
	bool fin = !b.finalTexto.empty() && !b.creditos.empty() && !b.desbloqueo.es.empty() && !b.desbloqueo.en.empty();
	for (const auto& l : b.finalTexto) fin = fin && !l.es.empty() && !l.en.empty();
	cu::caso("CU-FE-DIALOGO-CONTENIDO", fin, "el final de la historia no tiene texto, creditos o desbloqueo en ES/EN");
	// la intro: X avanza linea por linea (X con la linea a medias la completa) y al final empieza el duelo
	irARivales(g, VD::HISTORIA);
	g.apretar(PAD_CROSS, 0.3f);
	cu::caso("CU-FE-DIALOGO-INTRO", e.activa() && e.tipo == historia_dialogos::INTRO && e.personaje == v.personaje, "X sobre el rival no abrio su dialogo");
	const int total = e.total();
	g.apretar(PAD_CROSS, 0.05f);
	cu::caso("CU-FE-DIALOGO-INTRO", e.linea == 0 && e.lineaCompleta(), "X con la linea a medias no la completo (linea %d)", e.linea);
	int pasos = 0;
	while (e.activa() && pasos++ < 40) g.apretar(PAD_CROSS, 0.25f);
	cu::caso("CU-FE-DIALOGO-INTRO", !e.activa() && v.pantalla == VD::DUELO && v.sorteo, "terminar el dialogo (%d lineas) no empezo el duelo", total);
	// circulo salta la presentacion
	irARivales(g, VD::HISTORIA);
	g.apretar(PAD_CROSS, 0.3f);
	g.apretar(PAD_CIRCLE, 0.2f);
	cu::caso("CU-FE-DIALOGO-SALTAR", !e.activa() && v.pantalla == VD::DUELO, "circulo no salto el dialogo");
	cu::caso("CU-FE-DIALOGO-CIERRE", !historia_dialogos::dialogoCierre(v.personaje, true).empty() && !historia_dialogos::dialogoCierre(v.personaje, false).empty(),
	         "el personaje no tiene linea de cierre");
}

// ------------------------------------------------------------------------------------------------- duelo
// rama classic-md-fases: pasar a la fase siguiente con el mando (antes el cuadrado): OPTIONS abre el selector de fases con
// el cursor en la primera accion valida (la fase siguiente) y X la confirma. false si el selector no se abrio
inline bool pasarFaseCU(Juego& g)
{
	if (!g.ctl.selFases.abierto) g.apretar(PAD_OPTIONS, 0.1f);
	if (!g.ctl.selFases.abierto) return false;
	g.apretar(PAD_CROSS, 0.2f);
	return true;
}
// hasta que el jugador pueda jugar (su turno, libre) o pasen s segundos
inline bool esperarJugador(Juego& g, float s)
{
	for (float t = 0.0f; t < s; t += 0.1f) {
		if (g.dueloIA.ganador >= 0) return false;
		if (g.dueloIA.activo == 0 && g.dueloIA.decide() == 0 && !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && g.dueloIA.fase == ia::F_PRINCIPAL1) return true;
		if (g.dueloIA.decide() == 0 && g.ctl.estado == control::MENU && g.ctl.enPregunta) {   // (pregunta del duelo: no activar)
			g.ctl.opcion = (int)g.ctl.opciones.size() - 1;
			g.apretar(PAD_CROSS, 0.1f);
		}
		if (g.dueloIA.decide() == 0 && g.dueloIA.ef.preg.tipo == ia::P_ELEGIR && g.ctl.estado == control::ELEGIR_CARTA) g.apretar(PAD_CROSS, 0.1f);
		if (g.dueloIA.activo == 0 && g.dueloIA.decide() == 0 && !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && g.dueloIA.fase != ia::F_PRINCIPAL1) {
			if (g.dueloIA.fase == ia::F_FINAL && (int)g.dueloIA.l[0].mano.size() > ia::MAX_MANO) {
				g.ctl.foco = control::Foco{ control::F_MANO_J, 0 };
				g.apretar(PAD_CROSS, 0.2f);
				g.apretar(PAD_CROSS, 0.4f);
			} else {
				pasarFaseCU(g);   // (rama classic-md-fases: el selector de fases; antes el cuadrado)
				g.esperar(0.3f);
			}
		}
		g.esperar(0.1f);
	}
	return false;
}

// rama classic-moneda: el lanzamiento de moneda (antes, piedra, papel o tijera). X adelanta solo lo que ya se vio (la
// entrada VS, la moneda ya caida, el cartel); si gana el jugador elige ir "segundo" (derecha + X) o primero (X) y se
// comprueba que se respeta. Devuelve 0 si gano el jugador, 1 si gano el rival, -1 si no termino
inline bool sono(Juego& g, const char* sfx) { return std::find(g.sonido.sfxs.begin(), g.sonido.sfxs.end(), sfx) != g.sonido.sfxs.end(); }
inline int pasarMoneda(Juego& g, bool segundo)
{
	VD& v = g.ctl.vista;
	moneda::Estado& m = v.moneda;
	for (int k = 0; k < 40 && v.sorteo && m.etapa < moneda::RESULTADO; k++) g.apretar(PAD_CROSS, 0.2f);
	if (!v.sorteo || m.etapa < moneda::RESULTADO) return -1;
	const int cara = m.cara, gan = m.ganador;
	cu::caso("CU-FE-DUELO-MONEDA-REGLA", (cara == moneda::CARA || cara == moneda::CRUZ) && gan == (cara == moneda::CARA ? 0 : 1), "la moneda salio %d y gano %d (cara = jugador)", cara,
	         gan);
	for (int k = 0; k < 20 && v.sorteo && m.etapa == moneda::RESULTADO; k++) g.apretar(PAD_CROSS, 0.2f);
	if (gan == 0) {
		cu::caso("CU-FE-DUELO-MONEDA-GANA", v.sorteo && m.etapa == moneda::ORDEN, "el jugador gano y no le pregunto el orden (etapa %d)", m.etapa);
		if (segundo) g.apretar(PAD_RIGHT, 0.15f);
		g.esperar(0.4f);
		const int quiere = segundo ? 1 : 0;
		g.apretar(PAD_CROSS, 0.2f);
		cu::caso(segundo ? "CU-FE-DUELO-MONEDA-GANA-SEGUNDO" : "CU-FE-DUELO-MONEDA-GANA-PRIMERO", !v.sorteo && m.empieza == quiere && g.dueloIA.activo == quiere,
		         "eligio ir %s y empieza %d", segundo ? "segundo" : "primero", g.dueloIA.activo);
		return 0;
	}
	cu::caso("CU-FE-DUELO-MONEDA-PIERDE", v.sorteo && m.etapa == moneda::RIVAL_DECIDE && (m.empieza == 0 || m.empieza == 1), "gano el rival y no decidio (etapa %d)", m.etapa);
	const int empieza = m.empieza;
	g.esperar(moneda::T_RIVAL + 0.3f);
	cu::caso("CU-FE-DUELO-MONEDA-PIERDE", !v.sorteo && g.dueloIA.activo == empieza, "el rival decidio %d y empieza %d", empieza, g.dueloIA.activo);
	return 1;
}
// s de juego desde que empieza el lanzamiento hasta poder elegir / ver lo que decidio el rival, sin tocar nada
inline float tiempoMoneda(Juego& g)
{
	VD& v = g.ctl.vista;
	float s = 0.0f;
	while (s < 20.0f && v.sorteo && v.moneda.etapa < moneda::ORDEN) {
		g.cuadro(0);
		s += g.frameTimer;
	}
	return s;
}

inline void abandonar(Juego& g);
// el lanzamiento entero (rama classic-moneda): VS, moneda, cara/cruz, orden, rival, moneda justa, mando y velocidad
inline void monedaTodo(Juego& g)
{
	VD& v = g.ctl.vista;
	moneda::Estado& m = v.moneda;
	// 1) gana el jugador (cara) mirando cada momento: VS -> giro -> resultado -> orden; el mando en el orden
	irARivales(g, VD::LIBRE);
	g.monedaForzada = moneda::CARA;
	g.sonido.sfxs.clear();
	g.apretar(PAD_CROSS, 0.1f);
	cu::caso("CU-FE-DUELO-MONEDA", v.pantalla == VD::DUELO && v.sorteo && m.etapa == moneda::VS && m.cara < 0, "X en el rival no empezo el lanzamiento con el VS (etapa %d)", m.etapa);
	g.esperar(moneda::T_VS + 0.1f);
	cu::caso("CU-FE-DUELO-MONEDA", m.etapa == moneda::GIRO && m.cara == moneda::CARA, "tras el VS la moneda no esta girando (etapa %d)", m.etapa);
	g.esperar(0.2f);
	g.apretar(PAD_CROSS, 0.05f);
	cu::caso("CU-FE-DUELO-MONEDA-MANDO", m.etapa == moneda::GIRO && m.t < moneda::T_CAE, "X con la moneda en el aire salto el giro");
	const moneda::Pose aire = moneda::pose(moneda::T_SUBE, m.cara), suelo = moneda::pose(moneda::T_GIRO, m.cara);
	cu::caso("CU-FE-DUELO-MONEDA-ANIMACION", aire.y < -0.1f && !aire.cayo && suelo.cayo && fabsf(suelo.y) < 0.01f && cosf(suelo.angulo) > 0.99f &&
	                                             cosf(moneda::pose(moneda::T_GIRO, moneda::CRUZ).angulo) < -0.99f,
	         "la moneda no sube, cae y muestra la cara que salio");
	g.esperar(moneda::T_GIRO);
	cu::caso("CU-FE-DUELO-MONEDA", m.etapa == moneda::RESULTADO && m.ganador == 0, "la cara no le dio el lanzamiento al jugador (etapa %d, gana %d)", m.etapa, m.ganador);
	cu::caso("CU-FE-DUELO-MONEDA-SONIDO", sono(g, "sfx_impacto") && sono(g, "sfx_voltear") && sono(g, "sfx_colocar") && sono(g, "sfx_clave_aparece"),
	         "faltan los sonidos del VS, el giro, la caida o el brillo");
	g.esperar(moneda::T_RESULTADO + 0.2f);
	cu::caso("CU-FE-DUELO-MONEDA-GANA", m.etapa == moneda::ORDEN && m.opcion == 0, "tras el resultado no se pregunta el orden con el foco en Ir primero (etapa %d)", m.etapa);
	g.esperar(0.4f);
	g.apretar(PAD_RIGHT, 0.15f);
	cu::caso("CU-FE-DUELO-MONEDA-MANDO", m.opcion == 1 && sono(g, "sfx_menu_cursor"), "derecha no paso a Ir segundo");
	g.apretar(PAD_RIGHT, 0.15f);
	cu::caso("CU-FE-DUELO-MONEDA-MANDO", m.opcion == 1, "derecha otra vez se salio de los dos botones");
	g.apretar(PAD_LEFT, 0.15f);
	cu::caso("CU-FE-DUELO-MONEDA-MANDO", m.opcion == 0, "izquierda no volvio a Ir primero");
	g.apretar(PAD_CIRCLE, 0.2f);
	g.apretar(PAD_SQUARE, 0.2f);
	g.esperar(6.0f);
	cu::caso("CU-FE-DUELO-MONEDA-MANDO", v.sorteo && m.etapa == moneda::ORDEN, "el orden se salto sin elegir (circulo, cuadrado o esperar)");
	g.apretar(PAD_RIGHT, 0.15f);
	g.apretar(PAD_CROSS, 0.2f);
	cu::caso("CU-FE-DUELO-MONEDA-GANA-SEGUNDO", !v.sorteo && m.empieza == 1 && g.dueloIA.activo == 1 && sono(g, "sfx_menu_confirmar"), "eligio ir segundo y empieza %d",
	         g.dueloIA.activo);
	abandonar(g);
	// 2) gana el jugador y elige primero; 3) gana el rival (cruz) y decide con su personalidad
	irARivales(g, VD::LIBRE);
	g.monedaForzada = moneda::CARA;
	g.apretar(PAD_CROSS, 0.3f);
	cu::caso("CU-FE-DUELO-MONEDA-GANA-PRIMERO", pasarMoneda(g, false) == 0, "con cara no gano el jugador");
	abandonar(g);
	int decide[2] = { 0, 0 };
	for (int n = 0; n < 4; n++) {
		irARivales(g, VD::LIBRE);
		g.monedaForzada = moneda::CRUZ;
		g.apretar(PAD_CROSS, 0.3f);
		cu::caso("CU-FE-DUELO-MONEDA-PIERDE", pasarMoneda(g, false) == 1, "con cruz no gano el rival");
		if (m.empieza == 0 || m.empieza == 1) decide[m.empieza]++;
		abandonar(g);
	}
	cu::caso("CU-FE-DUELO-MONEDA-PIERDE", decide[0] + decide[1] == 4, "el rival no decidio en los 4 lanzamientos (%d / %d)", decide[0], decide[1]);
	g.monedaForzada = -1;
	// 4) moneda justa: 200 lanzamientos, los dos resultados y ninguno abrumador; y en duelos de verdad al azar
	int caras = 0;
	for (int n = 0; n < 200; n++) caras += g.lanzarMoneda() == moneda::CARA;
	cu::caso("CU-FE-DUELO-MONEDA-JUSTA", caras >= 60 && caras <= 140, "en 200 lanzamientos salieron %d caras y %d cruces", caras, 200 - caras);
	int vistos[2] = { 0, 0 };
	for (int n = 0; n < 24 && !(vistos[0] && vistos[1]); n++) {
		irARivales(g, VD::LIBRE);
		g.apretar(PAD_CROSS, 0.3f);
		const int r = pasarMoneda(g, n % 2 == 1);
		if (r >= 0) vistos[r]++;
		abandonar(g);
	}
	cu::caso("CU-FE-DUELO-MONEDA-JUSTA", vistos[0] > 0 && vistos[1] > 0, "en duelos al azar no salieron los dos resultados (%d / %d)", vistos[0], vistos[1]);
	// 5) con velocidad de animaciones rapida el lanzamiento se acorta
	const int velAntes = velocidad::nivel();
	velocidad::poner(velocidad::NORMAL);
	irARivales(g, VD::LIBRE);
	g.monedaForzada = moneda::CARA;
	g.apretar(PAD_CROSS, 0.05f);
	const float normal = tiempoMoneda(g);
	abandonar(g);
	velocidad::poner(velocidad::MUY_RAPIDA);
	irARivales(g, VD::LIBRE);
	g.apretar(PAD_CROSS, 0.05f);
	const float rapida = tiempoMoneda(g);
	abandonar(g);
	velocidad::poner(velAntes);
	g.monedaForzada = -1;
	cu::caso("CU-FE-DUELO-MONEDA-VELOCIDAD", normal > 4.0f && rapida < normal * 0.6f, "el lanzamiento dura %.2f s normal y %.2f s muy rapido", normal, rapida);
	// 6) los textos en ES y EN; los de piedra, papel o tijera ya no estan
	for (const char* t : { "LANZAMIENTO DE MONEDA", "Cara: decides tú · Cruz: decide el rival", "¡CARA!", "¡CRUZ!", "Tú", "Ganaste el lanzamiento de moneda",
	                       "El rival ganó el lanzamiento", "Elige: ¿ir primero o segundo?", "Ir primero", "Ir segundo", "El rival decide ir primero", "El rival decide ir segundo" }) {
		idioma::actual() = idioma::EN;
		const std::string en = idioma::tr(t);
		idioma::actual() = idioma::ES;
		cu::caso("CU-FE-DUELO-MONEDA-TEXTOS", idioma::tabla().count(t) && en != t && !en.empty() && idioma::tr(t) == std::string(t), "'%s' sin texto en ingles ('%s')", t,
		         en.c_str());
	}
	idioma::actual() = idioma::EN;
	const std::string titulo = idioma::tr("LANZAMIENTO DE MONEDA"), gana = idioma::tr("Ganaste el lanzamiento de moneda");
	idioma::actual() = idioma::ES;
	cu::caso("CU-FE-DUELO-MONEDA-TEXTOS", titulo == "COIN TOSS" && gana == "You won the coin toss", "en ingles: '%s' / '%s'", titulo.c_str(), gana.c_str());
	for (const char* t : { "PIEDRA, PAPEL O TIJERA", "Piedra", "Papel", "Tijera", "¡Empate! Otra vez", "¡Ganas el sorteo!", "Elige tu mano: el ganador decide quién empieza" })
		cu::caso("CU-FE-DUELO-MONEDA-TEXTOS", idioma::tabla().count(t) == 0, "quedo el texto viejo '%s' de piedra, papel o tijera", t);
}

// rama classic-md-fases: en el duelo el menu del duelo (la pausa) se abre con el engranaje (panel tactil); OPTIONS es el
// selector de fases. En el lanzamiento de moneda (sin engranaje) OPTIONS sigue abriendo la pausa
inline void abrirPausaDuelo(Juego& g)
{
	if (g.pausa.abierta) return;
	const bool engranaje = g.ctl.vista.pantalla == VD::DUELO && !g.ctl.vista.sorteo;
	g.apretar(engranaje ? PAD_TOUCH_PAD : PAD_OPTIONS, 0.2f);
	if (g.ctl.selFases.abierto) g.apretar(PAD_CIRCLE, 0.1f);   // (no deberia: el selector no es la pausa)
}
inline void abandonar(Juego& g)
{
	if (g.ctl.vista.pantalla != VD::DUELO) return;
	g.esperar(0.2f);
	abrirPausaDuelo(g);
	if (!g.pausa.abierta) return;
	const std::vector<int> ops = g.opcionesPausa();
	for (int k = 0; k < 8 && ops[(size_t)g.pausa.opcion] != Juego::PA_ABANDONAR; k++) g.apretar(PAD_DOWN, 0.12f);
	g.apretar(PAD_CROSS, 0.3f);
}

inline void duelo(Juego& g)
{
	VD& v = g.ctl.vista;
	monedaTodo(g);   // rama classic-moneda: el lanzamiento de moneda (CU-FE-DUELO-MONEDA*)
	// un duelo de verdad: reparto animado, fases, detalle, jugables, registro y visores
	irARivales(g, VD::LIBRE);
	g.apretar(PAD_CROSS, 0.3f);
	const int serie0 = g.ctl.robo.serie;
	if (pasarMoneda(g, false) < 0) {
		cu::caso("CU-FE-DUELO-MONEDA", false, "el lanzamiento de moneda no termino");
		return;
	}
	cu::caso("CU-FE-DUELO-INICIO", g.dueloIA.l[0].lp == ia::LP_INICIAL && g.dueloIA.l[1].lp == ia::LP_INICIAL && g.interfazDuelo.lp(false) == ia::LP_INICIAL,
	         "el duelo no empieza con 8000 LP");
	cu::caso("CU-FE-DUELO-INICIO", g.mazoDuelo.principal.size() >= 40 && g.dueloIA.l[0].mazo.size() + g.dueloIA.l[0].mano.size() == g.mazoDuelo.principal.size(),
	         "el jugador no juega con su mazo activo");
	{
		int robos = 0, serie = g.ctl.robo.serie;
		robos = serie - serie0;
		// (rama classic-arreglo-ciclo-md: con los tiempos de MD el reparto (5 x 0.8 s + 5 x 0.45 s) y los carteles de inicio y
		// de turno (2.4 + 2.05 s) pasan de los 10 s (600 cuadros) que se esperaban: hasta 25 s)
		for (int k = 0; k < 1500 && (g.reparto > 0 || g.ctl.robo.activo || g.sucesosVistos < g.dueloIA.sucesos.size()); k++) {
			g.cuadro(0);
			if (g.ctl.robo.serie != serie) robos++, serie = g.ctl.robo.serie;
		}
		cu::caso("CU-FE-DUELO-REPARTO", robos >= 10, "el reparto anima %d cartas (esperadas 10 u 11)", robos);
		cu::caso("CU-FE-DUELO-REPARTO", g.ctl.manoJ == g.dueloIA.l[0].mano && g.ctl.manoR == (int)g.dueloIA.l[1].mano.size(), "tras el reparto la mano en pantalla no es la del duelo");
	}
	cu::caso("CU-FE-DUELO-MUSICA", contiene(g.sonido.musica, "musica_duelo"), "en el duelo suena '%s'", g.sonido.musica.c_str());
	// la fase de espera se ve (cartel) al empezar el turno del jugador: robo (0) -> espera (1) -> principal 1 (2)
	{
		bool vioEspera = false, vioM1 = false;
		for (int k = 0; k < 6000 && !(vioEspera && vioM1); k++) {
			const int t0 = g.dueloIA.turno;
			g.cuadro(0);
			if (g.dueloIA.activo == 0 && g.dueloIA.turno == t0) {
				vioEspera |= g.ctl.fase == 1;
				vioM1 |= vioEspera && g.ctl.fase == 2;
			}
			if (g.dueloIA.decide() == 0 && g.ctl.estado == control::MENU && g.ctl.enPregunta) {
				g.ctl.opcion = (int)g.ctl.opciones.size() - 1;
				g.apretar(PAD_CROSS, 0.1f);
			}
			if (g.dueloIA.activo == 0 && !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && g.dueloIA.ganador < 0 && !vioEspera) pasarFaseCU(g);
		}
		cu::caso("CU-FE-DUELO-FASE-ESPERA", vioEspera && vioM1, "la fase de espera no se ve antes de la principal 1 (espera %d, M1 %d)", (int)vioEspera, (int)vioM1);
	}
	if (!esperarJugador(g, 60.0f)) {
		cu::caso("CU-FE-DUELO-TURNO-JUGADOR", false, "el jugador nunca quedo libre en su principal 1");
		abandonar(g);
		return;
	}
	cu::caso("CU-FE-DUELO-TURNO-JUGADOR", g.ctl.fase == Juego::faseInterfaz(ia::F_PRINCIPAL1), "la fase en pantalla (%d) no es la principal 1", g.ctl.fase);
	// jugables: lo que se puede jugar brilla (al menos una carta de la mano en la principal 1 con mano llena)
	g.esperar(0.3f);
	{
		bool hayMonstruo = false;
		for (const std::string& id : g.dueloIA.l[0].mano) {
			const ia::Carta* k = g.baseIA.carta(id);
			hayMonstruo |= k && k->monstruo && ia::Duelo::tributosPara(k) == 0 && !k->noInvocable && (k->clase == "normal" || k->clase == "efecto");
		}
		if (hayMonstruo && !g.dueloIA.l[0].invocoNormal)
			cu::caso("CU-FE-DUELO-JUGABLES", !g.consulta.jug.mano.empty(), "con un monstruo invocable en la mano, nada brilla");
		for (int i : g.consulta.jug.mano) {
			const std::vector<control::Opcion> ops = g.ctl.opcionesPara({ control::F_MANO_J, i });
			bool util = false;
			for (const control::Opcion& o : ops) util |= o.activa && o.accion != control::AC_CANCELAR;
			cu::caso("CU-FE-DUELO-JUGABLES", util, "la carta %d brilla sin tener nada que hacer", i);
		}
	}
	// los textos del menu de cada carta de la mano se ven distintos en ES y en EN (como los dibuja la interfaz: tr)
	{
		std::vector<std::string> vEs, vEn;
		for (int lengua = 0; lengua < 2; lengua++) {
			idioma::actual() = lengua == 0 ? idioma::ES : idioma::EN;
			std::vector<std::string>& v = lengua == 0 ? vEs : vEn;
			for (int i = 0; i < (int)g.ctl.manoJ.size(); i++)
				for (const control::Opcion& o : g.ctl.opcionesPara({ control::F_MANO_J, i })) {
					v.push_back(idioma::tr(o.texto));
					v.push_back(o.motivo.empty() ? std::string() : idioma::tr(o.motivo));
				}
		}
		idioma::actual() = idioma::ES;
		cu::caso("CU-FE-IDIOMA-MENU-CARTA", vEs.size() == vEn.size() && !vEs.empty(), "el menu de las cartas cambia de largo con el idioma");
		for (size_t k = 0; k < vEs.size() && k < vEn.size(); k++)
			if (!vEs[k].empty()) cu::caso("CU-FE-IDIOMA-MENU-CARTA", vEs[k] != vEn[k] || traducido(vEs[k]), "'%s' del menu de la carta se ve igual en ingles", vEs[k].c_str());
	}
	// detalle: triangulo sobre una carta de la mano; el stick derecho desplaza el texto; circulo vuelve
	g.ctl.foco = control::Foco{ control::F_MANO_J, 0 };
	g.apretar(PAD_TRIANGLE, 0.2f);
	cu::caso("CU-FE-DUELO-DETALLE", g.ctl.estado == control::DETALLE && g.ctl.cartaDetalle == g.dueloIA.l[0].mano[0], "triangulo no abrio el detalle de la carta (estado %s)",
	         control::NOMBRE_ESTADO[g.ctl.estado]);
	if (g.ctl.estado == control::DETALLE) {
		const float d0 = g.ctl.desplDetalle;
		g.mando.right_y = 1.0f;
		for (int k = 0; k < 30; k++) {
			g.mando.right_y = 1.0f;
			g.cuadro(0);
		}
		g.mando.right_y = 0.0f;
		cu::caso("CU-FE-DUELO-DETALLE-SCROLL", g.ctl.desplDetalle >= d0, "el stick derecho no desplaza el texto del detalle");
		g.apretar(PAD_CIRCLE, 0.2f);
		cu::caso("CU-FE-DUELO-DETALLE", g.ctl.estado == control::NAVEGAR, "circulo no cerro el detalle");
	}
	// invocar con el mando: la primera de la mano que se pueda invocar (X, Invocar, X en la zona)
	{
		// (si en este turno no hay ninguna que se pueda invocar sin tributos, se prueba en los siguientes del jugador)
		int k = -1;
		for (int intento = 0; intento < 5 && k < 0; intento++) {
			for (int i = 0; i < (int)g.ctl.manoJ.size() && k < 0; i++)
				for (const control::Opcion& o : g.ctl.opcionesPara({ control::F_MANO_J, i }))
					if (o.activa && o.accion == control::AC_INVOCAR && o.jugada.tributos.empty()) k = i;
			if (k >= 0 || intento == 4) break;
			const int t = g.dueloIA.turno;
			for (int n = 0; n < 40 && g.dueloIA.turno == t && g.dueloIA.ganador < 0; n++) {
				if (g.dueloIA.activo == 0 && !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR) pasarFaseCU(g);
				g.esperar(0.3f);
			}
			if (!esperarJugador(g, 90.0f)) break;
		}
		if (k >= 0) {
			const std::string id = g.ctl.manoJ[(size_t)k];
			g.ctl.foco = control::Foco{ control::F_MANO_J, k };
			g.apretar(PAD_CROSS, 0.2f);
			for (int o = 0; o < (int)g.ctl.opciones.size(); o++)
				if (g.ctl.opciones[(size_t)o].accion == control::AC_INVOCAR) g.ctl.opcion = o;
			g.apretar(PAD_CROSS, 0.2f);
			for (int n = 0; n < 4 && g.ctl.estado != control::NAVEGAR; n++) g.apretar(PAD_CROSS, 0.3f);
			g.esperar(2.0f);
			// (si el rival responde, p. ej. Trap Hole, su activacion va al ritmo de MD: se espera a que la mesa se ponga al dia)
			for (int n = 0; n < 40 && (g.sucesosVistos < g.dueloIA.sucesos.size() || g.mesaPendiente || g.esperaEfecto > 0.0f || ritmo::estado().ocupado()); n++)
				g.esperar(0.25f);
			bool esta = false, invocada = false;
			for (const ia::Monstruo& m : g.dueloIA.l[0].m) esta |= m.id == id && !m.bocaAbajo;
			for (const ia::Suceso& x : g.dueloIA.sucesos) invocada |= x.tipo == ia::S_INVOCA && x.lado == 0 && x.carta == id;
			// (el rival puede responder a la invocacion y sacarla del campo, p. ej. Trap Hole: entonces basta con que
			// la invocacion se hiciera y que la mesa coincida con el duelo)
			cu::caso("CU-FE-DUELO-INVOCAR", esta || invocada, "invocar %s con el mando no la puso en el campo", id.c_str());
			bool enMesa = false;
			for (int i = 0; i < ia::ZONAS; i++) enMesa |= g.ctl.zonas[(size_t)Juego::zonaMesaIA(0, i)].carta == id;
			cu::caso("CU-FE-DUELO-INVOCAR", enMesa == esta, "la mesa no muestra %s como el duelo (mesa %d, duelo %d)", id.c_str(), (int)enMesa, (int)esta);
			bool anotada = false;
			for (const registro::Linea& l : g.consulta.reg.lineas) anotada |= l.tipo == ia::S_INVOCA && l.carta == id;
			cu::caso("CU-FE-DUELO-REGISTRO", anotada, "el registro no anoto la invocacion de %s", id.c_str());
		}
	}
	// el registro (L3) y los visores
	g.esperar(0.5f);
	if (g.ctl.estado == control::NAVEGAR && !g.consulta.vis.abierto) {
		g.apretar(PAD_L3, 0.2f);
		cu::caso("CU-FE-DUELO-REGISTRO", g.consulta.vis.abierto && g.consulta.vis.pestana == visores::P_REGISTRO && !g.consulta.reg.lineas.empty(), "L3 no abrio el registro");
		g.apretar(PAD_RIGHT, 0.15f);
		cu::caso("CU-FE-DUELO-VISORES", g.consulta.vis.pestana == visores::P_CEMENTERIO && g.consulta.filas(g.ctl) == (int)g.dueloIA.l[0].cementerio.size(), "el visor del cementerio no coincide");
		g.apretar(PAD_L3, 0.2f);
		cu::caso("CU-FE-DUELO-REGISTRO", !g.consulta.vis.abierto, "L3 no cerro el registro");
	}
	// pausa en el duelo (rama classic-md-fases: el menu del duelo se abre con el engranaje, el panel tactil; OPTIONS es el
	// selector de fases y no la abre)
	g.esperar(0.3f);
	g.apretar(PAD_OPTIONS, 0.2f);
	cu::caso("CU-FE-PAUSA-ABRE", !g.pausa.abierta, "OPTIONS en el duelo abrio la pausa (debe ser el selector de fases)");
	if (g.ctl.selFases.abierto) g.apretar(PAD_CIRCLE, 0.2f);
	g.apretar(PAD_TOUCH_PAD, 0.2f);
	const std::vector<std::string> pausaEsperada = { "Continuar", "Opciones", "Construir mazo", "Abandonar duelo (derrota)", "Salir al menú principal" };
	cu::caso("CU-FE-PAUSA-ABRE", g.pausa.abierta && v.pausa && v.pausaOpciones == pausaEsperada, "el engranaje (panel tactil) no abrio la pausa con sus 5 opciones");
	for (const std::string& s : v.pausaOpciones) cu::caso("CU-FE-IDIOMA-PAUSA", traducido(s), "'%s' de la pausa no tiene traduccion", s.c_str());
	{
		const size_t su = g.dueloIA.sucesos.size();
		const int tu = g.dueloIA.turno;
		const ia::Fase fa = g.dueloIA.fase;
		g.esperar(3.0f);
		cu::caso("CU-FE-PAUSA-DETIENE", g.dueloIA.sucesos.size() == su && g.dueloIA.turno == tu && g.dueloIA.fase == fa, "con la pausa abierta el duelo siguio");
	}
	g.pausa.opcion = 0;
	g.apretar(PAD_CROSS, 0.2f);
	cu::caso("CU-FE-PAUSA-CONTINUAR", !g.pausa.abierta && v.pantalla == VD::DUELO, "Continuar no volvio al duelo");
	g.apretar(PAD_TOUCH_PAD, 0.2f);
	g.pausa.opcion = 1;
	g.apretar(PAD_CROSS, 0.2f);
	cu::caso("CU-FE-PAUSA-OPCIONES", g.menuP.enOpciones && g.pausa.abierta && !v.pausa && v.menuEnOpciones, "Opciones de la pausa no abrio las Opciones");
	g.apretar(PAD_CIRCLE, 0.2f);
	cu::caso("CU-FE-PAUSA-OPCIONES", !g.menuP.enOpciones && g.pausa.abierta && v.pausa && v.pantalla == VD::DUELO, "circulo en las Opciones de la pausa no volvio a la pausa");
	g.apretar(PAD_OPTIONS, 0.2f);
	cu::caso("CU-FE-PAUSA-CERRAR", !g.pausa.abierta, "OPTIONS corto no cerro la pausa");
	// Construir mazo desde la pausa: el constructor y de vuelta al duelo (el mazo nuevo vale para el proximo)
	g.apretar(PAD_TOUCH_PAD, 0.2f);
	g.pausa.opcion = 2;
	g.apretar(PAD_CROSS, 0.2f);
	g.cuadro(0);
	cu::caso("CU-FE-PAUSA-CONSTRUIR", g.constructorMazo.abierto, "Construir mazo de la pausa no abrio el constructor");
	const int turnoAntes = g.dueloIA.turno;
	g.apretar(PAD_CIRCLE, 0.3f);
	cu::caso("CU-FE-PAUSA-CONSTRUIR", !g.constructorMazo.abierto && v.pantalla == VD::DUELO && g.dueloIA.turno == turnoAntes, "al cerrar el constructor no volvio al mismo duelo");
	g.esperar(0.5f);
	// OPTIONS largo (2 s): no es la pausa (son los Ajustes de la plantilla)
	g.mantener(PAD_OPTIONS, 2.1f);
	g.esperar(0.2f);
	cu::caso("CU-FE-PAUSA-LARGO", !g.pausa.abierta, "mantener OPTIONS 2 s abrio la pausa");
	// Abandonar: derrota por rendicion con su pantalla
	const int derrotas = v.derrotas;
	abandonar(g);
	g.esperar(0.3f);
	cu::caso("CU-FE-PAUSA-ABANDONAR", v.pantalla == VD::RESULTADO && v.derrotas == derrotas + 1 && v.ganador == 1, "Abandonar no termino en derrota con su pantalla");
	cu::caso("CU-FE-RESULTADO-MOTIVO", resultado::resumen().motivo == resultado::M_RENDICION, "el motivo de abandonar no es la rendicion (%s)", resultado::nombreMotivo(resultado::resumen().motivo));
	// Salir al menu principal desde la pausa del duelo
	irARivales(g, VD::LIBRE);
	g.apretar(PAD_CROSS, 0.3f);
	pasarMoneda(g, false);
	g.esperar(1.0f);
	const int derr = v.derrotas;
	abrirPausaDuelo(g);
	const std::vector<int> ops = g.opcionesPausa();
	for (int k = 0; k < 8 && ops[(size_t)g.pausa.opcion] != Juego::PA_MENU_PRINCIPAL; k++) g.apretar(PAD_DOWN, 0.12f);
	g.apretar(PAD_CROSS, 0.3f);
	cu::caso("CU-FE-PAUSA-MENU", v.pantalla == VD::MENU && !g.pausa.abierta && v.derrotas == derr, "Salir al menu principal no fue al menu (o conto una derrota)");
	// la pausa en elegir rival cambia de modo
	irARivales(g, VD::HISTORIA);
	g.apretar(PAD_OPTIONS, 0.2f);
	const std::vector<int> ops2 = g.opcionesPausa();
	cu::caso("CU-FE-PAUSA-ELEGIR-RIVAL", g.pausa.abierta && std::find(ops2.begin(), ops2.end(), (int)Juego::PA_DUELO_LIBRE) != ops2.end(), "la pausa en elegir rival no ofrece Duelo libre");
	for (int k = 0; k < 8 && ops2[(size_t)g.pausa.opcion] != Juego::PA_DUELO_LIBRE; k++) g.apretar(PAD_DOWN, 0.12f);
	g.apretar(PAD_CROSS, 0.3f);
	cu::caso("CU-FE-PAUSA-ELEGIR-RIVAL", v.modo == VD::LIBRE && !g.pausa.abierta, "la pausa no cambio a Duelo libre");
}

// ------------------------------------------------------------------------------------------------- Preguntar antes de activar
// el efecto de la opcion en el duelo: Nunca = el juego no se detiene a preguntar (pasa solo); Auto = pregunta (con el
// menu de la pregunta) cuando el jugador tiene algo que activar
inline void preguntarEfecto(Juego& g)
{
	irARivales(g, VD::LIBRE);
	g.apretar(PAD_CROSS, 0.3f);
	pasarMoneda(g, false);
	if (!esperarJugador(g, 90.0f)) {
		cu::caso("CU-FE-OPC-PREGUNTAR-EFECTO", false, "el jugador nunca quedo libre para probar la pregunta");
		abandonar(g);
		return;
	}
	for (int modo : { opciones::PREGUNTAR_AUTO, opciones::PREGUNTAR_NUNCA }) {
		opciones::preguntar() = modo;
		g.dueloIA.ef.preg = ia::Pregunta{};
		g.dueloIA.ef.preg.tipo = ia::P_RESPUESTA;
		g.dueloIA.ef.preg.lado = 0;
		g.esperar(0.5f);
		const bool sigue = g.dueloIA.ef.preg.tipo == ia::P_RESPUESTA && g.dueloIA.ef.preg.lado == 0;
		if (modo == opciones::PREGUNTAR_AUTO)
			cu::caso("CU-FE-OPC-PREGUNTAR-EFECTO", sigue && g.ctl.estado == control::MENU && g.ctl.enPregunta, "con Auto la pregunta del duelo no se mostro al jugador");
		else
			cu::caso("CU-FE-OPC-PREGUNTAR-EFECTO", !sigue, "con Nunca el duelo se detuvo a preguntar (la opcion no tiene efecto)");
		if (sigue) {
			g.ctl.opcion = (int)g.ctl.opciones.size() - 1;
			g.apretar(PAD_CROSS, 0.2f);
		}
	}
	opciones::preguntar() = opciones::PREGUNTAR_SIEMPRE;
	g.esperar(0.3f);
	cu::caso("CU-FE-OPC-PREGUNTAR-EFECTO", g.dueloIA.preguntarSiempre[0] && !g.dueloIA.preguntarSiempre[1], "con Siempre el duelo no pregunta al jugador en cada ventana");
	opciones::preguntar() = opciones::PREGUNTAR_AUTO;
	g.esperar(0.3f);
	cu::caso("CU-FE-OPC-PREGUNTAR-EFECTO", !g.dueloIA.preguntarSiempre[0], "con Auto el duelo sigue preguntando siempre");
	abandonar(g);
}

// ------------------------------------------------------------------------------------------------- reglas oficiales
// (rama classic-reglas-oficiales) lo que ve el jugador: la repeticion del ataque (elegir objetivo o no atacar, con el
// mando) y la ventana justo despues del danno ("Recibiste daño": Attack and Receive), que respeta "Preguntar antes de activar"
inline int ponerMonstruoCU(ia::Duelo& d, int lado, const char* id)
{
	const int z = d.invocarEspecial(lado, id, false);
	if (z < 0) return z;
	d.l[(size_t)lado].m[(size_t)z].llegoEsteTurno = false;
	d.l[(size_t)lado].m[(size_t)z].especial = false;
	d.ef.disparos.clear();
	d.ef.ventanaTras = ia::Ventana{};
	d.recalcular();
	return z;
}
inline int colocarCU(ia::Duelo& d, int lado, const char* id)
{
	ia::Lado& x = d.l[(size_t)lado];
	const int z = x.libreMT();
	if (z < 0) return z;
	x.mt[(size_t)z] = ia::MagiaTrampa{ id, true };
	x.mt[(size_t)z].uid = d.nuevoUid();
	x.mt[(size_t)z].turnoColocada = 1;
	return z;
}
inline bool esperarQue(Juego& g, float s, const std::function<bool()>& f)
{
	for (float t = 0.0f; t < s; t += 0.1f) {
		if (f()) return true;
		g.esperar(0.1f);
	}
	return f();
}
inline void reglasOficiales(Juego& g)
{
	irARivales(g, VD::LIBRE);
	g.apretar(PAD_CROSS, 0.3f);
	pasarMoneda(g, false);
	if (!esperarJugador(g, 90.0f)) {
		cu::caso("CU-FE-DUELO-REPETICION", false, "el jugador nunca quedo libre para probar la repeticion del ataque");
		cu::caso("CU-FE-DUELO-VENTANA-DANNO", false, "el jugador nunca quedo libre para probar la ventana del danno");
		abandonar(g);
		return;
	}
	ia::Duelo& d = g.dueloIA;
	auto mesaLimpia = [&]() {
		for (ia::Lado& x : d.l) {
			x.m = {};
			x.mt = {};
		}
		d.l[0].mano.clear();   // (sin magias rapidas del jugador que pidan responder)
		d.turno = std::max(d.turno, 3);
		d.fase = ia::F_BATALLA;
		d.ef.batallaTerminada = false;
	};
	auto atacar = [&](int z, int objetivo) {
		ia::Jugada j;
		j.tipo = ia::J_ATACAR;
		j.zona = z;
		j.objetivo = objetivo;
		return d.aplicar(j);
	};
	// 1) la repeticion: Summoned Skull ataca directo y el rival responde con Scapegoat
	mesaLimpia();
	const int a = ponerMonstruoCU(d, 0, "summoned_skull");
	const int sg = colocarCU(d, 1, "scapegoat");
	const int lpRival = d.l[1].lp;   // (rama classic-bp-terminar: el rival pudo pagar LP antes, en su turno: no siempre son 8000)
	bool ok = atacar(a, -1);
	if (ok && d.ef.preg.tipo == ia::P_RESPUESTA && d.ef.preg.lado == 1) ok = d.aplicar(ia::jugadaActivar({ 1, ia::EN_MT, sg }));
	const bool pregunta = ok && d.ef.preg.tipo == ia::P_ELEGIR && d.ef.repeticion.hay && d.ef.preg.lado == 0;
	cu::caso("CU-FE-DUELO-REPETICION", pregunta, "Scapegoat tras el ataque directo no dejo la repeticion al jugador (pregunta %d)", d.ef.preg.tipo);
	const bool foco = esperarQue(g, 6.0f, [&]() { return g.ctl.estado == control::ELEGIR_CARTA && g.ctl.eleccionDuelo; });
	cu::caso("CU-FE-DUELO-REPETICION", pregunta && foco, "la repeticion no se mostro como eleccion con el foco (estado %d)", (int)g.ctl.estado);
	if (foco) {
		g.apretar(PAD_CIRCLE, 0.3f);   // (circulo: no atacar)
		cu::caso("CU-FE-DUELO-REPETICION", !d.ef.repeticion.hay && d.l[1].monstruos() == 4 && d.l[1].lp == lpRival && d.puedeAtacar(0, a),
		         "circulo no fue 'no atacar' (repeticion %d, fichas %d, LP del rival %d -> %d, puede atacar %d)", (int)d.ef.repeticion.hay, d.l[1].monstruos(), lpRival, d.l[1].lp,
		         (int)d.puedeAtacar(0, a));
	}
	// 1b) con el ataque directo entre las opciones: la lista con "Ataque directo" y "No atacar"
	// (rama classic-arreglo-ciclo-md: con el ritmo de Master Duel la mesa presenta el Scapegoat y sus 4 fichas viajando
	// varios segundos y la pregunta del duelo se abre al terminar: se espera la mesa libre y 20 s; antes 6 s)
	esperarQue(g, 20.0f, [&]() { return !d.ocupado() && !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR; });
	mesaLimpia();
	const int a2 = ponerMonstruoCU(d, 0, "summoned_skull");
	const int cg = ponerMonstruoCU(d, 1, "celtic_guardian");
	d.preguntarSiempre[1] = true;   // (el rival tiene su ventana tras la declaracion)
	atacar(a2, cg);
	d.alCementerio(1, cg);
	d.preguntarSiempre[1] = false;
	for (int k = 0; k < 6 && d.ef.preg.tipo == ia::P_RESPUESTA && d.ef.preg.lado == 1; k++) {
		ia::Jugada p;
		p.tipo = ia::J_PASAR;
		d.aplicar(p);
	}
	const bool lista = esperarQue(g, 20.0f, [&]() { return g.ctl.estado == control::MENU && g.ctl.eleccionDuelo && g.ctl.opciones.size() == 2; });
	cu::caso("CU-FE-DUELO-REPETICION", lista && g.ctl.opciones[0].texto == "Ataque directo" && g.ctl.opciones[1].texto == "No atacar",
	         "la repeticion sin monstruos no ofrece 'Ataque directo' y 'No atacar' (estado %d, %d opciones)", (int)g.ctl.estado, (int)g.ctl.opciones.size());
	if (lista) {
		const int lp = d.l[1].lp;
		g.ctl.opcion = 0;
		g.apretar(PAD_CROSS, 0.3f);
		esperarQue(g, 12.0f, [&]() { return d.l[1].lp < lp || d.ganador >= 0; });
		cu::caso("CU-FE-DUELO-REPETICION", d.l[1].lp == std::max(0, lp - 2500), "elegir 'Ataque directo' no hizo el danno (LP %d -> %d)", lp, d.l[1].lp);
	}
	if (d.ganador >= 0) return;
	// 2) la ventana del danno: el jugador ataca a un Blue-Eyes, recibe 500 y tiene Attack and Receive
	for (int modo : { opciones::PREGUNTAR_AUTO, opciones::PREGUNTAR_NUNCA }) {
		opciones::preguntar() = modo;
		g.esperar(0.3f);
		esperarQue(g, 20.0f, [&]() { return !d.ocupado() && !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR; });
		mesaLimpia();
		const int s = ponerMonstruoCU(d, 0, "summoned_skull");
		const int be = ponerMonstruoCU(d, 1, "blue_eyes");
		const int ar = colocarCU(d, 0, "attack_and_receive");
		const int lp0 = d.l[0].lp, lp1 = d.l[1].lp;
		atacar(s, be);
		const bool ventana = d.ef.ventana.tipo == ia::V_DANNO && d.ef.preg.tipo == ia::P_RESPUESTA && d.ef.preg.lado == 0 && d.l[0].lp == lp0 - 500;
		if (modo == opciones::PREGUNTAR_AUTO) {
			cu::caso("CU-FE-DUELO-VENTANA-DANNO", ventana, "tras recibir danno de batalla no se abrio la ventana del jugador (ventana %d, pregunta %d lado %d)", d.ef.ventana.tipo, d.ef.preg.tipo, d.ef.preg.lado);
			const bool menu = esperarQue(g, 20.0f, [&]() { return g.ctl.estado == control::MENU && g.ctl.enPregunta && !g.ctl.eleccionDuelo; });
			cu::caso("CU-FE-DUELO-VENTANA-DANNO", menu && g.ctl.tituloTexto == idioma::tr("Recibiste daño") && !g.ctl.opciones.empty() && g.ctl.opciones[0].accion == control::AC_ACTIVAR_EFECTO,
			         "la pregunta tras el danno no dice 'Recibiste daño' con Attack and Receive (estado %d, titulo '%s')", (int)g.ctl.estado, g.ctl.tituloTexto.c_str());
			if (menu) {
				g.ctl.opcion = 0;
				g.apretar(PAD_CROSS, 0.3f);
				esperarQue(g, 8.0f, [&]() { return d.l[1].lp < lp1; });
				cu::caso("CU-FE-DUELO-VENTANA-DANNO", d.l[1].lp == lp1 - 700, "Attack and Receive desde la pregunta no hizo 700 (LP %d -> %d)", lp1, d.l[1].lp);
			}
		} else {
			esperarQue(g, 20.0f, [&]() { return !(d.ef.preg.tipo == ia::P_RESPUESTA && d.ef.preg.lado == 0); });
			cu::caso("CU-FE-DUELO-VENTANA-DANNO", !(d.ef.preg.tipo == ia::P_RESPUESTA && d.ef.preg.lado == 0) && !d.l[0].mt[(size_t)ar].vacia() && d.l[0].mt[(size_t)ar].bocaAbajo,
			         "con Nunca el duelo se detuvo en la ventana del danno o activo la trampa");
		}
		(void)ventana;
	}
	opciones::preguntar() = opciones::PREGUNTAR_AUTO;
	g.esperar(0.3f);
	abandonar(g);
}

// ------------------------------------------------------------------------------------------------- resultado
// un duelo que termina al instante como se pida
inline void dueloQueTermina(Juego& g, int modo, int ganador, bool exodia = false)
{
	irARivales(g, modo);
	g.apretar(PAD_CROSS, 0.3f);
	if (historia_dialogos::escena().activa()) g.apretar(PAD_CIRCLE, 0.2f);
	pasarMoneda(g, false);
	g.esperar(2.0f);
	if (g.ctl.vista.pantalla != VD::DUELO) return;
	if (exodia) {
		for (const char* p : { "exodia_the_forbidden_one", "left_arm_of_the_forbidden_one", "right_arm_of_the_forbidden_one", "left_leg_of_the_forbidden_one",
		                       "right_leg_of_the_forbidden_one" })
			g.dueloIA.l[(size_t)ganador].mano.push_back(p);
	} else {
		g.dueloIA.l[(size_t)(1 - ganador)].lp = 0;
	}
	g.dueloIA.terminar(ganador);
	for (int k = 0; k < 900 && g.ctl.vista.pantalla == VD::DUELO; k++) g.cuadro(0);
}

inline void resultadoPantalla(Juego& g)
{
	VD& v = g.ctl.vista;
	resultado::Resumen& r = resultado::resumen();
	// textos de los motivos en los dos idiomas
	for (int m = resultado::M_LP; m <= resultado::M_EFECTO; m++)
		for (int gan = 0; gan <= 2; gan++) {
			idioma::actual() = idioma::ES;
			const std::string es = resultado::textoMotivo((resultado::Motivo)m, gan);
			idioma::actual() = idioma::EN;
			const std::string en = resultado::textoMotivo((resultado::Motivo)m, gan);
			idioma::actual() = idioma::ES;
			cu::caso("CU-FE-IDIOMA-RESULTADO", !es.empty() && !en.empty() && es != en, "el motivo %s no tiene texto ES/EN", resultado::nombreMotivo((resultado::Motivo)m));
		}
	// victoria en duelo libre al nivel maximo desbloqueado: sube de nivel
	v.nivelMax = 1;
	const int vict = v.victorias, duelos0 = estadisticas::datos().duelos;
	dueloQueTermina(g, VD::LIBRE, 0);
	cu::caso("CU-FE-RESULTADO-PANTALLA", v.pantalla == VD::RESULTADO && r.activo && v.ganador == 0, "ganar no mostro el resultado");
	cu::caso("CU-FE-RESULTADO-MOTIVO", r.motivo == resultado::M_LP, "LP a 0: motivo %s", resultado::nombreMotivo(r.motivo));
	cu::caso("CU-FE-RESULTADO-SUBE-NIVEL", v.subeNivel && v.nivelMax == 2 && v.victorias == vict + 1, "ganar al nivel maximo no subio de nivel (%d)", v.nivelMax);
	cu::caso("CU-FE-RESULTADO-ESTADISTICAS", estadisticas::datos().duelos == duelos0 + 1, "el duelo no se conto en las estadisticas");
	{
		std::ifstream f(g.rutaProgreso);
		std::string cl;
		int val, nmax = 0;
		while (f >> cl >> val)
			if (cl == "nivel_max") nmax = val;
		cu::caso("CU-FE-GUARDADO-PROGRESO", nmax == 2, "progreso.txt no guardo el nivel 2 (%d)", nmax);
	}
	g.sonido.sfxs.clear();
	g.esperar(5.6f);
	cu::caso("CU-FE-RESULTADO-MUSICA", g.sonido.musica == "musica_victoria", "tras la victoria suena '%s'", g.sonido.musica.c_str());
	// Revancha: el mismo rival, otra vez con el lanzamiento de moneda
	const std::string rivalAntes = g.mazoRivalIA.rival ? g.mazoRivalIA.rival->clave : "";
	r.opcion = resultado::O_REVANCHA;
	g.apretar(PAD_CROSS, 0.3f);
	cu::caso("CU-FE-RESULTADO-REVANCHA", v.pantalla == VD::DUELO && v.sorteo && g.mazoRivalIA.rival && g.mazoRivalIA.rival->clave == rivalAntes, "Revancha no repitio el rival '%s'", rivalAntes.c_str());
	abandonar(g);
	g.esperar(1.0f);
	// Elegir otro rival
	g.apretar(PAD_RIGHT, 0.2f);
	cu::caso("CU-FE-RESULTADO-NAVEGAR", r.opcion == resultado::O_OTRO_RIVAL, "derecha no paso a Elegir otro rival (%d)", r.opcion);
	g.apretar(PAD_CROSS, 0.3f);
	cu::caso("CU-FE-RESULTADO-OTRO-RIVAL", v.pantalla == VD::ELEGIR_RIVAL, "Elegir otro rival no fue a elegir rival");
	// derrota y Menu principal
	dueloQueTermina(g, VD::LIBRE, 1);
	cu::caso("CU-FE-RESULTADO-DERROTA", v.pantalla == VD::RESULTADO && v.ganador == 1 && r.motivo == resultado::M_LP, "la derrota no mostro su resultado");
	g.esperar(6.0f);
	cu::caso("CU-FE-RESULTADO-MUSICA", g.sonido.musica == "musica_derrota", "tras la derrota suena '%s'", g.sonido.musica.c_str());
	r.opcion = resultado::O_MENU;
	g.apretar(PAD_CROSS, 0.3f);
	cu::caso("CU-FE-RESULTADO-MENU", v.pantalla == VD::MENU, "Menu principal no fue al menu");
	// Exodia
	dueloQueTermina(g, VD::LIBRE, 0, true);
	cu::caso("CU-FE-RESULTADO-MOTIVO", r.motivo == resultado::M_EXODIA, "con las 5 piezas el motivo es %s", resultado::nombreMotivo(r.motivo));
	g.esperar(1.0f);
	g.apretar(PAD_CIRCLE, 0.3f);
	cu::caso("CU-FE-RESULTADO-CIRCULO", v.pantalla == VD::MENU, "circulo en el resultado no volvio al menu");
	// historia: vencer a un personaje desbloquea sus firma y dice el siguiente rival
	g.historia.vencidos.clear();
	g.historia.guardar();
	g.coleccion.desbloquear(g.historia.vencidos);
	const personajes::Personaje* conFirma = nullptr;
	for (int i : g.historia.deSaga(0))
		if (!g.historia.lista[(size_t)i].firma.empty() && !conFirma) conFirma = &g.historia.lista[(size_t)i];
	irARivales(g, VD::HISTORIA);
	if (conFirma)
		for (int k = 0; k < 12 && v.personaje != conFirma->id; k++) g.apretar(PAD_RIGHT, 0.12f);
	const std::string pj = v.personaje;
	g.apretar(PAD_CROSS, 0.3f);
	if (historia_dialogos::escena().activa()) g.apretar(PAD_CIRCLE, 0.2f);
	pasarMoneda(g, false);
	g.esperar(1.5f);
	g.dueloIA.l[1].lp = 0;
	g.dueloIA.terminar(0);
	for (int k = 0; k < 900 && v.pantalla == VD::DUELO; k++) g.cuadro(0);
	cu::caso("CU-FE-RESULTADO-HISTORIA", v.pantalla == VD::RESULTADO && v.personajeNuevo && g.historia.vencido(pj), "vencer a %s no lo marco vencido", pj.c_str());
	if (conFirma && pj == conFirma->id) {
		cu::caso("CU-FE-RESULTADO-FIRMA", !r.firmaNuevas.empty() && v.firmaNuevas == (int)r.firmaNuevas.size(), "vencer a %s no desbloqueo sus cartas firma", pj.c_str());
		for (const std::string& id : r.firmaNuevas) {
			const mazo::Carta* k = g.coleccion.carta(id);
			cu::caso("CU-FE-RESULTADO-FIRMA", k && !k->bloqueada, "la firma %s sigue bloqueada", id.c_str());
		}
	} else {
		cu::caso("CU-FE-RESULTADO-FIRMA", false, "no se encontro un personaje con cartas firma en la saga 1");
	}
	cu::caso("CU-FE-RESULTADO-SIGUIENTE", !r.siguienteEs.empty() && !r.siguienteEn.empty(), "el resultado no dice el siguiente rival de la historia");
	{
		personajes::Historia h2;
		h2.leer(g.carpetaAssets + "ui/");
		h2.leerProgreso(g.rutaProgreso.substr(0, g.rutaProgreso.size() - 12) + "historia.txt");
		cu::caso("CU-FE-GUARDADO-HISTORIA", h2.vencido(pj), "historia.txt no guardo que %s fue vencido", pj.c_str());
	}
	g.apretar(PAD_CIRCLE, 0.3f);
	// el final de la historia: vencer al ultimo abre el texto, los creditos y el Modo Desafio
	for (const personajes::Personaje& x : g.historia.lista) g.historia.vencidos.insert(x.id);
	const std::string jefe = g.historia.lista.back().id;
	g.historia.vencidos.erase(jefe);
	g.historia.finalVisto = false;
	g.historia.guardar();
	historia_dialogos::desafioDisponible() = false;
	irARivales(g, VD::HISTORIA);
	v.saga = g.historia.lista.back().sagaIdx;
	v.idx = -1;
	g.elegirRival(true);
	cu::caso("CU-FE-RIVAL-HISTORIA", v.personaje == jefe && v.sagaAbierta, "con todos vencidos menos el jefe, no se elige al jefe (%s)", v.personaje.c_str());
	g.apretar(PAD_CROSS, 0.3f);
	if (historia_dialogos::escena().activa()) g.apretar(PAD_CIRCLE, 0.2f);
	pasarMoneda(g, false);
	g.esperar(1.5f);
	g.dueloIA.l[1].lp = 0;
	g.dueloIA.terminar(0);
	for (int k = 0; k < 900 && v.pantalla == VD::DUELO; k++) g.cuadro(0);
	cu::caso("CU-FE-FINAL", g.finalPendiente && g.historia.completa(), "vencer al jefe no dejo el final pendiente");
	g.esperar(1.0f);
	g.apretar(PAD_CROSS, 0.3f);
	historia_dialogos::Escena& e = historia_dialogos::escena();
	cu::caso("CU-FE-FINAL", e.activa() && e.tipo == historia_dialogos::FINAL, "X en el resultado no abrio el final");
	cu::caso("CU-FE-FINAL-DESAFIO", historia_dialogos::desafioDisponible() && g.historia.finalVisto, "el final no desbloqueo el Modo Desafio");
	int pasos = 0;
	bool vioCreditos = false;
	while (e.activa() && pasos++ < 80) {
		vioCreditos |= e.etapa == historia_dialogos::F_CREDITOS;
		g.apretar(PAD_CROSS, 0.3f);
		if (e.etapa == historia_dialogos::F_CREDITOS) g.esperar(1.2f);
	}
	cu::caso("CU-FE-FINAL-CREDITOS", vioCreditos, "el final no paso por los creditos");
	cu::caso("CU-FE-FINAL", !e.activa() && v.pantalla == VD::ELEGIR_RIVAL, "al terminar el final no volvio a elegir rival");
	{
		personajes::Historia h2;
		h2.leer(g.carpetaAssets + "ui/");
		h2.leerProgreso(g.rutaProgreso.substr(0, g.rutaProgreso.size() - 12) + "historia.txt");
		cu::caso("CU-FE-GUARDADO-HISTORIA", h2.finalVisto && h2.completa(), "historia.txt no guardo el final visto");
	}
	// el desafio: el rival al nivel maximo y el jugador con 4000 LP
	historia_dialogos::desafioActivo() = true;
	irARivales(g, VD::HISTORIA);
	g.apretar(PAD_CROSS, 0.3f);
	if (historia_dialogos::escena().activa()) g.apretar(PAD_CIRCLE, 0.2f);
	pasarMoneda(g, false);
	cu::caso("CU-FE-DESAFIO-DUELO", g.dueloIA.l[0].lp == historia_dialogos::LP_DESAFIO, "el desafio no empezo con %d LP (%d)", historia_dialogos::LP_DESAFIO, g.dueloIA.l[0].lp);
	abandonar(g);
	historia_dialogos::desafioActivo() = false;
	historia_dialogos::desafioDisponible() = false;
	g.historia.vencidos.clear();
	g.historia.finalVisto = false;
	g.historia.guardar();
	g.coleccion.desbloquear(g.historia.vencidos);
	irAlMenu(g);
}

// ------------------------------------------------------------------------------------------------- idioma
inline void idiomaTodo(Juego& g)
{
	// menu principal, Opciones y sus valores, pausa
	for (int a : { Juego::MP_DUELO, Juego::MP_EN_LINEA, Juego::MP_CONSTRUIR, Juego::MP_LISTA, Juego::MP_OPCIONES, Juego::MP_SALIR })
		cu::caso("CU-FE-IDIOMA-MENU", traducido(Juego::textoMenu(a)), "'%s' del menu no tiene traduccion", Juego::textoMenu(a));
	for (int o = 0; o < Juego::NUM_OPCIONES; o++) cu::caso("CU-FE-IDIOMA-OPCIONES", traducido(Juego::textoOpcion(o)), "'%s' de Opciones no tiene traduccion", Juego::textoOpcion(o));
	for (const char* s : { "Sí", "No", "Siempre", "Nunca", "Auto", "Muy rápida", "Rápida", "Normal", "Humana", "Clásica" })
		cu::caso("CU-FE-IDIOMA-OPCIONES", traducido(s), "el valor '%s' de Opciones no tiene traduccion", s);
	for (int a = Juego::PA_CONTINUAR; a <= Juego::PA_OPCIONES; a++)
		if (a != Juego::PA_AJUSTES) cu::caso("CU-FE-IDIOMA-PAUSA", traducido(Juego::textoPausa(a)), "'%s' de la pausa no tiene traduccion", Juego::textoPausa(a));
	for (const char* s : { "Empiezas tú", "Empieza el rival", "Vence a todos los de la saga anterior", "No activar", "Cancelar", "Activar" })
		cu::caso("CU-FE-IDIOMA-AVISOS", traducido(s), "el aviso '%s' no tiene traduccion", s);
	for (const char* f : control::NOMBRE_FASE_LARGO) cu::caso("CU-FE-IDIOMA-FASES", traducido(f), "la fase '%s' no tiene traduccion", f);
	// resultado
	for (int o = 0; o < resultado::NUM_OPCIONES; o++) {
		idioma::actual() = idioma::EN;
		const std::string en = resultado::textoOpcion(o);
		idioma::actual() = idioma::ES;
		cu::caso("CU-FE-IDIOMA-RESULTADO", en != resultado::textoOpcion(o), "la opcion %d del resultado no cambia de idioma", o);
	}
	// reglas (tutorial), controles, fundas, tapetes, sagas
	for (int p = 0; p < tutorial::NUM_PAGINAS; p++) {
		const tutorial::Pagina& pg = tutorial::PAGINAS[p];
		cu::caso("CU-FE-IDIOMA-REGLAS", pg.titulo.es && pg.titulo.en && *pg.titulo.es && *pg.titulo.en && strcmp(pg.titulo.es, pg.titulo.en) != 0, "la pagina %d de las reglas sin titulo ES/EN", p);
	}
	for (int b = 0; b < tutorial::NUM_BOTONES; b++)
		cu::caso("CU-FE-IDIOMA-REGLAS", *tutorial::BOTONES[b].texto.es && *tutorial::BOTONES[b].texto.en, "el boton %d de la ayuda sin texto ES/EN", b);
	for (int p = 0; p < cosmeticos::NUM_PESTANAS; p++)
		for (const cosmeticos::Control& c : cosmeticos::controles(p))
			cu::caso("CU-FE-IDIOMA-CONTROLES", c.es && c.en && *c.es && *c.en && strcmp(c.es, c.en) != 0, "un control de la pestanna %d sin texto ES/EN", p);
	for (int k = 0; k < cosmeticos::NUM_FUNDAS; k++) cu::caso("CU-FE-IDIOMA-PERSO", *cosmeticos::FUNDAS[k].es && *cosmeticos::FUNDAS[k].en, "funda %d sin nombre ES/EN", k);
	for (int k = 0; k < cosmeticos::NUM_TAPETES; k++) cu::caso("CU-FE-IDIOMA-PERSO", *cosmeticos::TAPETES[k].es && *cosmeticos::TAPETES[k].en, "tapete %d sin nombre ES/EN", k);
	for (int s = 0; s < personajes::NUM_SAGAS; s++)
		cu::caso("CU-FE-IDIOMA-SAGAS", *personajes::SAGAS[s].es && *personajes::SAGAS[s].en, "la saga %d sin nombre ES/EN", s);
	// personajes y rivales: nombre y lema en los dos idiomas
	for (const personajes::Personaje& x : g.historia.lista)
		cu::caso("CU-FE-IDIOMA-PERSONAJES", !x.nombreEs.empty() && !x.nombreEn.empty() && !x.lemaEs.empty() && !x.lemaEn.empty(), "%s sin nombre o lema ES/EN", x.id.c_str());
	for (const ia::Rival& r : g.baseIA.rivales)
		cu::caso("CU-FE-IDIOMA-RIVALES", !r.nombreEs.empty() && !r.nombreEn.empty() && !r.lemaEs.empty() && !r.lemaEn.empty(), "rival %s sin nombre o lema ES/EN", r.clave.c_str());
	// la coleccion en ingles: la del idioma actual (constructor_textos.h)
	idioma::actual() = idioma::EN;
	mazo::Coleccion en;
	en.leer(g.carpetaAssets + "ui/");
	idioma::actual() = idioma::ES;
	const mazo::Carta* be = en.carta("blue_eyes");
	cu::caso("CU-FE-IDIOMA-CARTAS", be && be->nombre == "Blue-Eyes White Dragon", "con English la coleccion no usa los nombres en ingles ('%s')", be ? be->nombre.c_str() : "-");
	cu::caso("CU-FE-IDIOMA-CARTAS", be && be->atributoClave == "LUZ", "la clave del atributo cambia con el idioma (filtros)");
	// constructor: sus textos (constructor_textos.h) en ingles
	idioma::actual() = idioma::EN;
	const std::string t1 = constructor::T("Máximo 3 copias");
	idioma::actual() = idioma::ES;
	cu::caso("CU-FE-IDIOMA-CONSTRUCTOR", t1 != "Máximo 3 copias", "el constructor no traduce sus avisos");
	// (rama classic-torneo-extras) MAZOS DE TORNEO en ingles: titulo, acciones y nombre de cada mazo
	idioma::actual() = idioma::EN;
	const std::string t2 = constructor::T("MAZOS DE TORNEO"), t3 = constructor::T("Copiar a mis mazos"), t4 = constructor::T("Mazo de torneo copiado: ");
	idioma::actual() = idioma::ES;
	cu::caso("CU-FE-IDIOMA-TORNEO", t2 == "TOURNAMENT DECKS" && t3 == "Copy to my decks" && t4 == "Tournament deck copied: ", "MAZOS DE TORNEO sin traducir ('%s', '%s')", t2.c_str(),
	         t3.c_str());
	int sinEn = 0;
	for (const mazo::MazoTorneo& t : g.constructorMazo.torneos) sinEn += t.nombreEn.empty() || t.nombre(true) != t.nombreEn || t.nombre(false) != t.nombreEs;
	cu::caso("CU-FE-IDIOMA-TORNEO", !g.constructorMazo.torneos.empty() && sinEn == 0, "%d mazos de torneo sin nombre en ingles", sinEn);
}

// ------------------------------------------------------------------------------------------------- mando
inline void mando()
{
	// la cruz: un toque = un paso; mantenida, repite tras 0.40 s y luego cada 0.09 s
	control::Repetidor r;
	int pulsos = r.paso(control::B_ABAJO, control::B_ABAJO, 0, 0, 1.0f / 60.0f) ? 1 : 0;
	for (int k = 1; k < 6; k++) pulsos += r.paso(control::B_ABAJO, 0, 0, 0, 1.0f / 60.0f) ? 1 : 0;
	r.paso(0, 0, 0, 0, 1.0f / 60.0f);
	cu::caso("CU-FE-MANDO-CRUZ-TOQUE", pulsos == 1, "un toque de 0.1 s dio %d pulsos", pulsos);
	control::Repetidor r2;
	pulsos = r2.paso(control::B_ABAJO, control::B_ABAJO, 0, 0, 1.0f / 60.0f) ? 1 : 0;
	for (int k = 1; k < 60; k++) pulsos += r2.paso(control::B_ABAJO, 0, 0, 0, 1.0f / 60.0f) ? 1 : 0;
	cu::caso("CU-FE-MANDO-CRUZ-REPETIR", pulsos >= 6 && pulsos <= 9, "mantener 1 s dio %d pulsos (esperados ~7)", pulsos);
	// el stick izquierdo hace de cruz
	control::Repetidor r3;
	uint32_t b = 0;
	for (int k = 0; k < 3; k++) b |= r3.paso(0, 0, 0.0f, 0.9f, 1.0f / 60.0f);
	cu::caso("CU-FE-MANDO-STICK", (b & control::B_ABAJO) != 0, "el stick abajo no movio");
	b = 0;
	for (int k = 0; k < 3; k++) b |= r3.paso(0, 0, 0.0f, 0.1f, 1.0f / 60.0f);
	cu::caso("CU-FE-MANDO-STICK", b == 0, "el stick suelto siguio moviendo");
	// R2: mantener acelera, un toque salta, y solo si hay animacion o juega el rival
	velocidad::poner(0);
	velocidad::Control v;
	float e = 0.0f;
	for (int k = 0; k < 30; k++) e = v.cuadro(1.0f, true, false, 1.0f / 60.0f);
	cu::caso("CU-FE-MANDO-R2-ACELERA", fabsf(e - velocidad::factor() * velocidad::ACELERA) < 0.01f && v.consume, "mantener R2 con animacion: escala %.2f", e);
	v.reiniciar();
	for (int k = 0; k < 30; k++) e = v.cuadro(1.0f, false, false, 1.0f / 60.0f);
	cu::caso("CU-FE-MANDO-R2-ACELERA", fabsf(e - velocidad::factor()) < 0.01f && !v.consume, "R2 sin animacion acelero (escala %.2f): debe ser zoom", e);
	v.reiniciar();
	for (int k = 0; k < 5; k++) v.cuadro(1.0f, true, false, 1.0f / 60.0f);
	e = v.cuadro(0.0f, true, false, 1.0f / 60.0f);
	cu::caso("CU-FE-MANDO-R2-SALTA", e >= velocidad::SALTO, "un toque de R2 no salto la animacion (escala %.2f)", e);
	// los factores de velocidad suben
	cu::caso("CU-FE-MANDO-VELOCIDAD", velocidad::factor(0) < velocidad::factor(1) && velocidad::factor(1) < velocidad::factor(2), "los factores de velocidad no suben");
}

// ------------------------------------------------------------------------------------------------- guardado
inline void guardadoAtomico(const std::string& dir)
{
	const std::string ruta = dir + "prueba_guardado.txt";
	const int serie = guardado::estado().serie;
	std::string grande;
	for (int k = 0; k < 2000; k++) guardado::agregar(grande, "linea %d con texto largo para el guardado atomico\n", k);
	const bool ok = guardado::guardarAtomico(ruta, grande);
	cu::caso("CU-FE-GUARDADO-ATOMICO", ok && leerArchivo(ruta) == grande && !existe(ruta + ".tmp"), "guardarAtomico no dejo el archivo entero (o quedo el .tmp)");
	cu::caso("CU-FE-GUARDADO-ATOMICO", guardado::estado().serie == serie, "un guardado bueno dio aviso");
	// sobrescribir: el archivo viejo se reemplaza entero
	guardado::guardarAtomico(ruta, "corto\n");
	cu::caso("CU-FE-GUARDADO-ATOMICO", leerArchivo(ruta) == "corto\n", "sobrescribir no reemplazo el archivo entero");
	// fallo: carpeta que no existe -> false, aviso con el nombre del archivo
	const bool mal = guardado::guardarAtomico(dir + "no_existe/x/opciones.txt", "x");
	cu::caso("CU-FE-GUARDADO-AVISO", !mal && guardado::estado().ultimoFallo && guardado::estado().archivo == "opciones.txt" && guardado::estado().serie == serie + 1,
	         "un guardado imposible no dio aviso");
	// sin carpeta de datos: un solo aviso
	guardado::avisarSinCarpeta();
	guardado::avisarSinCarpeta();
	cu::caso("CU-FE-GUARDADO-SIN-CARPETA", guardado::estado().sinCarpeta && guardado::estado().serie == serie + 2, "el aviso de sin carpeta salio %d veces", guardado::estado().serie - serie - 1);
	// estadisticas: guardar y leer
	estadisticas::Datos copia = estadisticas::datos();
	const std::string rutaEst = estadisticas::datos().ruta;   // (copia: leer() reinicia datos() antes de abrir)
	estadisticas::leer(rutaEst);
	const estadisticas::Datos& d = estadisticas::datos();
	cu::caso("CU-FE-GUARDADO-ESTADISTICAS", d.duelos == copia.duelos && d.rivales.size() == copia.rivales.size() && d.cartas.size() == copia.cartas.size() && d.mazos.size() == copia.mazos.size() &&
	                                             d.racha == copia.racha && d.mejorRacha == copia.mejorRacha,
	         "estadisticas.txt no vuelve igual al leerlo: duelos %d/%d rivales %d/%d cartas %d/%d mazos %d/%d racha %d/%d (%s)", d.duelos, copia.duelos, (int)d.rivales.size(),
	         (int)copia.rivales.size(), (int)d.cartas.size(), (int)copia.cartas.size(), (int)d.mazos.size(), (int)copia.mazos.size(), d.racha, copia.racha, copia.ruta.c_str());
}


// ------------------------------------------------------------------------------------------------- primer turno en la PS5
// (rama classic-ps5-arreglos1) el primer turno que jugo el usuario en la PS5 contra Tea (plan bloqueo, preguntar Auto,
// velocidad Normal): invoca un monstruo en ATAQUE con el mando, el rival responde con una trampa (Trap Hole) y despues
// el jugador pasa de fase con el cuadrado hasta terminar el turno. Cada sintoma tiene su caso:
//  - CU-FE-PS5-INVOCAR-ATAQUE: la carta queda boca arriba en ataque (mesa y duelo) y nada la pone en defensa sola
//  - CU-FE-PS5-CORTE-TRAMPA: la trampa del rival en el turno del jugador sale en el cartel grande con su nombre, y el
//    duelo espera a que se lea; el monstruo que se lleva avisa "Al cementerio"
//  - CU-FE-PS5-FASES: con el selector de fases (OPTIONS; antes el cuadrado) se llega a la Batalla / Principal 2 / Final y
//    el turno termina (sin quedar trabado); con la pregunta abierta OPTIONS no la tapa
inline int manoConCU(Juego& g, const char* id)
{
	ia::Lado& x = g.dueloIA.l[0];
	for (int k = 0; k < (int)x.mano.size(); k++)
		if (x.mano[(size_t)k] == id) return k;
	x.mano.push_back(id);
	g.ctl.manoJ = x.mano;
	return (int)x.mano.size() - 1;
}
// un cuadro a la vez hasta que f() (o s segundos)
inline bool cuadrosHasta(Juego& g, float s, const std::function<bool()>& f)
{
	for (float t = 0.0f; t < s; t += g.frameTimer) {
		if (f()) return true;
		g.cuadro(0);
	}
	return f();
}
// invoca la carta k de la mano en ataque con el mando (X, Invocar, X en la zona); devuelve la zona de la mesa
inline int invocarConMandoCU(Juego& g, int k, const std::function<void()>& cadaCuadro = nullptr)
{
	g.ctl.foco = control::Foco{ control::F_MANO_J, k };
	g.apretar(PAD_CROSS, 0.2f);
	bool hay = false;
	for (int o = 0; o < (int)g.ctl.opciones.size(); o++)
		if (g.ctl.opciones[(size_t)o].accion == control::AC_INVOCAR && g.ctl.opciones[(size_t)o].activa) g.ctl.opcion = o, hay = true;
	if (!hay || g.ctl.estado != control::MENU) return -1;
	g.apretar(PAD_CROSS, 0.2f);
	if (g.ctl.estado != control::ELEGIR_ZONA) return -1;
	const int z = g.ctl.foco.i;
	for (int n = 0; n < 6; n++) g.cuadro(PAD_CROSS);   // (X en la zona: la jugada se aplica y la carta vuela)
	for (int n = 0; n < 240 && g.ctl.estado == control::VOLANDO; n++) {
		g.cuadro(0);
		if (cadaCuadro) cadaCuadro();
	}
	return z;
}
// el mazo del usuario (listas/ps5_prueba1/mazo.txt) y el de Tea con el plan bloqueo (variedad.txt) de esa partida
static const char* const MAZO_PS5_J[] = { "shining_abyss", "shining_abyss", "shining_abyss", "archfiend_soldier", "insect_knight", "luster_dragon", "gemini_elf",
	"mad_dog_of_darkness", "flash_assailant", "gaia_soul_the_combustible_collective", "shadowknight_archfiend", "terrorking_archfiend", "armor_exe",
	"rare_metal_dragon", "cave_dragon", "dark_elf", "goblin_attack_force", "toon_goblin_attack_force", "berserk_gorilla", "arsenal_bug", "jirai_gumo",
	"the_unfriendly_amazon", "nuvia_the_wicked", "giant_orc", "boar_soldier", "susa_soldier", "zombyra_the_dark", "7", "7_completed", "gust_fan",
	"spell_absorption", "soul_absorption", "dimensionhole", "lighten_the_load", "soul_of_the_pure", "amplifier", "ring_of_magnetism", "smashing_ground",
	"ante", "shooting_star_bow_ceal", "silver_bow_and_arrow", "fusion_weapon", "ritual_weapon" };
static const char* const MAZO_PS5_TEA[] = { "aqua_madoor", "banisher_of_the_light", "book_of_secret_arts", "castle_walls", "ceremonial_bell",
	"dian_keto_the_cure_master", "dian_keto_the_cure_master", "dust_tornado", "enchanted_javelin", "fairy_s_hand_mirror", "fake_trap", "fire_sorcerer",
	"fissure", "happy_lover", "hinotama", "house_of_adhesive_tape", "jar_of_greed", "luminous_spark", "magical_hats", "magical_labyrinth",
	"magician_s_valkyria", "mystical_elf", "mystical_elf", "mystical_moon", "reinforcements", "shining_angel", "shining_friendship", "shining_friendship",
	"soul_of_the_pure", "sparks", "spirit_of_the_harp", "stop_defense", "swords_of_revealing_light", "the_agent_of_judgment_saturn", "the_forgiving_maiden",
	"trap_hole", "ultimate_offering", "waboku", "waboku", "yami" };

// un duelo contra Tea como el del usuario: empieza el jugador, invoca en ataque con el mando lo que pueda (forzar: con
// Trap Hole colocada del rival y Luster Dragon en la mano) y pasa de fase con el selector de fases (OPTIONS) hasta terminar el turno
inline void turnoTeaPS5(Juego& g, bool forzar, int n)
{
	VD& v = g.ctl.vista;
	irARivales(g, VD::HISTORIA);
	if (n == 0) cu::caso("CU-FE-PS5-INVOCAR-ATAQUE", v.personaje == "tea", "el primer rival de la historia no es Tea ('%s')", v.personaje.c_str());
	g.apretar(PAD_CROSS, 0.3f);
	for (int k = 0; k < 40 && historia_dialogos::escena().activa(); k++) g.apretar(PAD_CROSS, 0.25f);
	g.esperar(0.5f);
	if (!(v.pantalla == VD::DUELO && v.sorteo)) {
		cu::caso("CU-FE-PS5-INVOCAR-ATAQUE", false, "X en Tea no llego al sorteo (pantalla %d)", v.pantalla);
		return;
	}
	g.mazoJugador.principal.assign(std::begin(MAZO_PS5_J), std::end(MAZO_PS5_J));
	g.mazoJugador.fusion.clear();
	g.mazoRivalIA.principal.assign(std::begin(MAZO_PS5_TEA), std::end(MAZO_PS5_TEA));
	g.mazoRivalIA.plan = ia::PL_BLOQUEO;
	v.sorteo = false;
	g.arrancarDueloIA(0);   // (empieza el jugador, como en la PS5)
	ia::Duelo& d = g.dueloIA;
	if (!esperarJugador(g, 60.0f) || d.turno != 1) {
		cu::caso("CU-FE-PS5-INVOCAR-ATAQUE", false, "el jugador no quedo libre en la principal 1 de su primer turno (turno %d)", d.turno);
		abandonar(g);
		return;
	}
	char tag[32];
	snprintf(tag, sizeof(tag), "duelo %d%s", n, forzar ? " (Trap Hole)" : "");
	int th = -1;
	if (forzar) {
		th = colocarCU(d, 1, "trap_hole");
		if (th >= 0) d.l[1].mt[(size_t)th].turnoColocada = 0;   // (colocada en un turno anterior: se puede activar)
		manoConCU(g, "luster_dragon");
		g.esperar(0.3f);
	}
	// lo que se puede invocar en ataque sin sacrificios (la de mas ATK)
	int k = -1, mejor = -1;
	for (int i = 0; i < (int)g.ctl.manoJ.size(); i++)
		for (const control::Opcion& o : g.ctl.opcionesPara({ control::F_MANO_J, i }))
			if (o.activa && o.accion == control::AC_INVOCAR && o.jugada.tributos.empty()) {
				const ia::Carta* c = g.baseIA.carta(g.ctl.manoJ[(size_t)i]);
				const int atk = c ? c->atk : 0;
				if (atk > mejor) mejor = atk, k = i;
			}
	if (forzar)   // (con Trap Hole: Luster Dragon, sin efectos que la nieguen como Shadowknight Archfiend)
		for (int i = 0; i < (int)g.ctl.manoJ.size(); i++)
			if (g.ctl.manoJ[(size_t)i] == "luster_dragon") k = i;
	bool respondio = false, corteVisto = false, cartaDuranteCorte = false;
	auto responder = [&]() {
		if (!forzar || respondio || th < 0 || d.ef.preg.tipo != ia::P_RESPUESTA || d.ef.preg.lado != 1) return;
		ia::Jugada j = ia::jugadaActivar({ 1, ia::EN_MT, th });
		if (const ia::DefCarta* df = ia::defCarta("trap_hole")) j.objs = ia::objetivosDe(d, 1, *df, { 1, ia::EN_MT, th });
		if (j.objs.size() > 1) j.objs.resize(1);
		respondio = d.aplicar(j);
	};
	if (k >= 0) {
		const std::string id = g.ctl.manoJ[(size_t)k];
		const size_t su0 = d.sucesos.size();
		const int z = invocarConMandoCU(g, k, responder);
		cu::caso("CU-FE-PS5-INVOCAR-ATAQUE", z >= 0, "%s: el mando no dejo invocar %s (estado %d)", tag, id.c_str(), (int)g.ctl.estado);
		if (z >= 0) {
			const control::Celda c = g.ctl.zonas[(size_t)z];
			const bool sigue = c.carta == id;   // (pudo irse ya: Nuvia se destruye sola, Trap Hole)
			cu::caso("CU-FE-PS5-INVOCAR-ATAQUE", !sigue || (!c.defensa && !c.bocaAbajo), "%s: al llegar %s no esta boca arriba en ataque (defensa %d, boca abajo %d)", tag,
			         id.c_str(), (int)c.defensa, (int)c.bocaAbajo);
			// la respuesta del rival (la IA, o Trap Hole forzada) y el cartel
			cuadrosHasta(g, 10.0f, [&]() {
				responder();
				if (g.ctl.corteVisible()) {
					corteVisto = true;
					if (g.ctl.tCorte < 0.8f) cartaDuranteCorte |= g.ctl.zonas[(size_t)z].carta == id;
				}
				return !g.ctl.bloqueado && d.decide() == 0 && !g.ctl.corteVisible() && g.ctl.estado == control::NAVEGAR;
			});
			bool activoRival = false, posicion = false;
			for (size_t x = su0; x < d.sucesos.size(); x++) {
				const ia::Suceso& s = d.sucesos[x];
				activoRival |= (s.tipo == ia::S_ACTIVA_MT || s.tipo == ia::S_ACTIVA_EFECTO) && s.lado == 1;
				posicion |= s.tipo == ia::S_POSICION && s.lado == 0;
			}
			if (forzar) cu::caso("CU-FE-PS5-CORTE-TRAMPA", respondio, "%s: el rival no pudo responder con Trap Hole a la invocacion normal", tag);
			if (activoRival) {
				cu::caso("CU-FE-PS5-CORTE-TRAMPA", corteVisto, "%s: la carta del rival en el turno del jugador no salio en el cartel", tag);
				cu::caso("CU-FE-PS5-CORTE-TRAMPA", cartaDuranteCorte, "%s: la carta del jugador desaparecio antes de que se leyera el cartel", tag);
			}
			if (forzar && respondio) {
				cu::caso("CU-FE-PS5-CORTE-TRAMPA", contiene(g.ctl.aviso, idioma::tr("Al cementerio")), "%s: el monstruo destruido no aviso (aviso '%s')", tag, g.ctl.aviso.c_str());
				cu::caso("CU-FE-PS5-CORTE-TRAMPA", !d.l[1].cementerio.empty() && d.l[1].cementerio.back() == "trap_hole" && g.ctl.zonas[(size_t)z].vacia(),
				         "%s: tras Trap Hole la mesa no coincide (cementerio rival %d, zona '%s')", tag, (int)d.l[1].cementerio.size(), g.ctl.zonas[(size_t)z].carta.c_str());
			}
			cu::caso("CU-FE-PS5-INVOCAR-ATAQUE", !posicion, "%s: una carta del jugador cambio de posicion sola", tag);
			// la mesa sigue al duelo: lo que quedo en cada zona esta como dice el duelo (ataque/defensa)
			for (int i = 0; i < ia::ZONAS; i++) {
				const ia::Monstruo& m = d.l[0].m[(size_t)i];
				const control::Celda& cc = g.ctl.zonas[(size_t)Juego::zonaMesaIA(0, i)];
				if (m.vacia()) continue;
				cu::caso("CU-FE-PS5-INVOCAR-ATAQUE", cc.carta == m.id && cc.defensa == m.defensa, "%s: la mesa muestra %s %s y el duelo %s %s", tag, cc.carta.c_str(),
				         cc.defensa ? "en defensa" : "en ataque", m.id.c_str(), m.defensa ? "en defensa" : "en ataque");
			}
		}
	}
	// (rama classic-md-fases: el cuadrado ya no pasa de fase; es OPTIONS, el selector de fases de Master Duel)
	// OPTIONS con la pregunta "activar una carta?" abierta: no cambia de fase y la pregunta sigue a la vista (nunca la
	// pregunta fantasma: "Primero responde a la pregunta" sin pregunta); contestada, el selector pasa de fase. Con el
	// detalle abierto, OPTIONS lo cierra y abre el selector
	const int turno = d.turno;
	if (forzar && d.ganador < 0) {
		cuadrosHasta(g, 10.0f, [&]() { return !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && d.decide() == 0; });
		d.ef.preg = ia::Pregunta{};
		d.ef.preg.tipo = ia::P_RESPUESTA;
		d.ef.preg.lado = 0;
		const bool menu = cuadrosHasta(g, 3.0f, [&]() { return g.ctl.estado == control::MENU && g.ctl.enPregunta; });
		const ia::Fase f0 = d.fase;
		g.apretar(PAD_OPTIONS, 0.2f);
		const bool sigue = !g.ctl.selFases.abierto && d.fase == f0 && g.ctl.estado == control::MENU && g.ctl.enPregunta && d.ef.preg.tipo == ia::P_RESPUESTA;
		cu::caso("CU-FE-PS5-FASES", menu && sigue, "%s: OPTIONS con la pregunta abierta la tapo o cambio de fase (menu %d, selector %d, estado %d, fase %d -> %d)", tag, (int)menu,
		         (int)g.ctl.selFases.abierto, (int)g.ctl.estado, (int)f0, (int)d.fase);
		if (g.ctl.estado == control::MENU && g.ctl.enPregunta) {   // (No activar)
			g.ctl.opcion = (int)g.ctl.opciones.size() - 1;
			g.apretar(PAD_CROSS, 0.2f);
		}
		cuadrosHasta(g, 5.0f, [&]() { return !g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && d.decide() == 0; });
		const int t0 = d.turno;
		const bool abrio = pasarFaseCU(g);   // (en el primer turno: Finalizar turno, la unica valida)
		const bool paso = cuadrosHasta(g, 10.0f, [&]() { return d.fase != f0 || d.turno != t0; });
		cu::caso("CU-FE-PS5-FASES", abrio && paso && d.ef.preg.tipo == ia::P_NADA, "%s: contestada la pregunta el selector no paso de fase (selector %d, fase %d -> %d, pregunta %d)",
		         tag, (int)abrio, (int)f0, (int)d.fase, (int)d.ef.preg.tipo);
		cuadrosHasta(g, 40.0f, [&]() { return d.ganador >= 0 || (!g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && d.decide() == 0 && d.activo == 0); });   // (el turno del rival)
		const ia::Fase f1 = d.fase;
		const int t1 = d.turno;
		if (!g.ctl.manoJ.empty()) g.ctl.foco = control::Foco{ control::F_MANO_J, 0 };
		g.apretar(PAD_TRIANGLE, 0.2f);
		const bool detalle = g.ctl.estado == control::DETALLE;
		g.apretar(PAD_OPTIONS, 0.2f);
		const bool sel = g.ctl.selFases.abierto && g.ctl.estado == control::NAVEGAR;
		if (sel) g.apretar(PAD_CROSS, 0.2f);
		const bool paso2 = cuadrosHasta(g, 10.0f, [&]() { return d.fase != f1 || d.turno != t1; });
		cu::caso("CU-FE-PS5-FASES", detalle && sel && paso2 && g.ctl.estado != control::DETALLE,
		         "%s: OPTIONS con el detalle abierto no abrio el selector o no paso de fase (detalle %d, selector %d, estado %d)", tag, (int)detalle, (int)sel, (int)g.ctl.estado);
	}
	// el selector de fases: Principal 1 -> Batalla/Principal 2 -> Final -> turno del rival, sin quedar trabado
	std::string fases;
	for (int p = 0; p < 6 && d.turno == turno && d.ganador < 0; p++) {
		const bool libre = cuadrosHasta(g, 15.0f, [&]() {
			if (d.decide() == 0 && g.ctl.estado == control::MENU && g.ctl.enPregunta) {   // (pregunta del duelo: no activar)
				g.ctl.opcion = (int)g.ctl.opciones.size() - 1;
				g.apretar(PAD_CROSS, 0.1f);
			}
			return d.turno != turno || (!g.ctl.bloqueado && g.ctl.estado == control::NAVEGAR && d.decide() == 0);
		});
		if (d.turno != turno) break;
		if (!libre) {
			cu::caso("CU-FE-PS5-FASES", false, "%s: el turno quedo trabado en la fase %d (bloqueado %d, estado %d, decide %d, pregunta %d lado %d, corte %d)", tag, (int)d.fase,
			         (int)g.ctl.bloqueado, (int)g.ctl.estado, d.decide(), (int)d.ef.preg.tipo, d.ef.preg.lado, (int)g.ctl.corteVisible());
			break;
		}
		const ia::Fase antes = d.fase;
		pasarFaseCU(g);
		const bool cambio = cuadrosHasta(g, 15.0f, [&]() { return d.fase != antes || d.turno != turno || d.ganador >= 0; });
		fases += std::to_string((int)antes) + ">" + std::to_string((int)d.fase) + " ";
		cu::caso("CU-FE-PS5-FASES", cambio, "%s: el selector de fases no cambio de fase (fase %d, aviso '%s')", tag, (int)antes, g.ctl.aviso.c_str());
		if (!cambio) break;
	}
	cu::caso("CU-FE-PS5-FASES", d.turno != turno || d.ganador >= 0, "%s: con el selector de fases el turno no termino (%s)", tag, fases.c_str());
	fprintf(stderr, "[cu] %s: fases %s, turno %d\n", tag, fases.c_str(), d.turno);
	abandonar(g);
	g.esperar(0.5f);
}
inline void turnoPS5(Juego& g)
{
	opciones::preguntar() = opciones::PREGUNTAR_AUTO;
	turnoTeaPS5(g, true, 0);
	for (int n = 1; n <= 6; n++) turnoTeaPS5(g, false, n);
}

}  // namespace cuf
