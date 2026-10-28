/*
 * Yugioh Classic Remaster (rama classic-en-linea): lo que se ve del modo DUELO EN LINEA. SOLO datos (sin Vulkan ni red):
 * lo llena en_linea_juego.h (dentro de la clase del juego) y lo dibuja en_linea_ui.h (dentro de interfaz::Interfaz).
 * Vive en control::VistaDuelo::enLinea, asi las pruebas sin GPU (cu_juego.h) leen lo mismo que la pantalla.
 *
 * Las constantes del duelo en linea (reloj del turno, alerta, agotamientos, plazos) estan aca para cambiarlas.
 */
#pragma once

#include <string>
#include <vector>

namespace en_linea {

// ---- el reloj del turno (como Master Duel). Cada jugador tiene RELOJ_TURNO_S por turno (se rellena al empezar cada
// turno, de los dos); corre solo mientras ese jugador tiene que decidir. Con RELOJ_ALERTA_S o menos: sonido y
// parpadeo. Si llega a 0 el juego actua solo (pasa de fase / no activa / elige la primera opcion) y cuenta un
// agotamiento; AGOTAMIENTOS_DERROTA en el duelo = derrota. Lo manda siempre la maquina del que agoto su tiempo
static constexpr float RELOJ_TURNO_S = 180.0f;
static constexpr float RELOJ_ALERTA_S = 30.0f;
static constexpr int AGOTAMIENTOS_DERROTA = 3;
// ---- plazos
static constexpr float RESPONDER_RIVAL_S = 15.0f;    // "¡Rival encontrado!" mientras juega contra la IA
static constexpr int CONECTAR_MS = 15000;           // perforacion (nat.h) hasta rendirse: "No se pudo conectar"
static constexpr int REUNIR_MS = 5000;              // STUN + UPnP al entrar (reunirCandidatos)
static constexpr float ELEGIR_ORDEN_S = 30.0f;      // el que gano la moneda: sin elegir en este tiempo, va primero
static constexpr int RECONEXION_S = 30;             // (canal.h: MS_PERDIDA) cartel "reconectando 0-30 s"

enum Pantalla {
	CERRADA = 0,
	MENU,          // DUELO EN LINEA: buscar, red local, codigo, mazo, volver
	AVISO_IP,      // la primera vez (y desde Opciones): tu IP sera visible para el rival
	BUSCANDO,      // buscando rival en la sala / DHT
	CONECTANDO,    // rival encontrado: perforando
	LAN,           // red local: crear duelo o unirse a uno anunciado
	LAN_ESPERA,    // anfitrion en la red local, esperando
	CODIGO,        // codigo de conexion: el propio y escribir el del rival
	TECLADO,       // el teclado en pantalla del codigo
	AVISO_RED,     // no se pudo conectar: CGNAT, NAT simetrico, abrir el puerto
	INTERRUMPIDO,  // el duelo se corto: no cuenta
	ESPERA_RIVAL,  // conectado, esperando que el rival empiece (antes de la moneda)
};

// el teclado del codigo: base32 de Crockford (nat.h) y lo de una direccion escrita a mano
static const char* const TECLAS = "0123456789ABCDEFGHJKMNPQRSTVWXYZ.:[]-";
static constexpr int TECLAS_FILA = 8;

struct Vista {
	int pantalla{ CERRADA };
	float t{ 0.0f };                      // s en la pantalla
	std::string titulo;                   // ya en el idioma actual
	std::vector<std::string> opciones;    // ya en el idioma actual
	std::vector<bool> apagadas;           // la opcion no se puede elegir (gris)
	int opcion{ 0 };
	std::vector<std::string> lineas;      // informacion (contador, ELO, codigo...)
	std::string aviso;                    // un aviso destacado (red, mazo no valido...)
	std::string ayuda;                    // botones de abajo (ya traducidos)
	bool animando{ false };               // indicador "buscando / conectando" que gira
	// teclado
	std::string texto;
	int tecla{ 0 };
	// en el duelo en linea
	bool enDuelo{ false };
	std::string rival;                    // nombre del rival remoto
	int conexion{ 0 };                    // 0 bien, 1 caida (reconectando)
	int reconectando{ 0 };                // s que lleva caida (0..30)
	float relojMio{ RELOJ_TURNO_S }, relojRival{ RELOJ_TURNO_S };
	bool relojMioCorre{ false }, relojRivalCorre{ false };
	int agotadosMio{ 0 }, agotadosRival{ 0 };
	// "¡Rival encontrado!" en pleno duelo contra la IA
	bool rivalEncontrado{ false };
	std::string rivalEncontradoTexto;
	float rivalEncontradoQueda{ 0.0f };
	// resultado del duelo en linea
	std::string eloTexto;                 // "ELO: 1000 -> 1016 (+16)" (solo si la sala confirmo) o pendiente
	std::string revanchaTexto;            // "Esperando al rival..." / "El rival quiere la revancha"
};

}  // namespace en_linea
