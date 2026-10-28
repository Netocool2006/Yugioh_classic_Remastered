/*
 * Yugioh Classic Remaster (rama classic-portada): opciones del jugador que leen otras partes del juego.
 * - permitirProhibidas(): "Permitir cartas prohibidas" de Opciones (por defecto No). Con Si, el constructor deja usar
 *   las cartas prohibidas de la banlist Goat (siempre maximo 3 copias) y no aplica los limites de limitada/semilimitada;
 *   con No se aplica la banlist Goat completa.
 * - preguntarActivacion(): "Preguntar antes de activar" como en Master Duel: PREGUNTAR_AUTO (pregunta solo si hay
 *   algo que se pueda activar con sentido), PREGUNTAR_SIEMPRE (en cada ventana) o PREGUNTAR_NUNCA (no pregunta).
 * - velocidadAnim(): "Velocidad de animacion": 0 Normal, 1 Rapida, 2 Muy rapida (la logica: velocidad.h, rama
 *   classic-resultado).
 * Se guardan en opciones.txt (permitir_prohibidas=0/1, preguntar_activacion=0/1/2, velocidad_anim=0/1/2, junto a
 * musica, efectos y figuras_3d) y las lee/escribe menu_principal.h (Opciones del menu principal y de la pausa).
 */
#pragma once

namespace opciones {

inline bool& prohibidas() { static bool v = false; return v; }   // (lo pone menu_principal.h)
inline bool permitirProhibidas() { return prohibidas(); }

enum { PREGUNTAR_AUTO = 0, PREGUNTAR_SIEMPRE = 1, PREGUNTAR_NUNCA = 2 };
inline int& preguntar() { static int v = PREGUNTAR_AUTO; return v; }   // (lo pone menu_principal.h)
inline int preguntarActivacion() { return preguntar(); }

enum { VELOCIDAD_NORMAL = 0, VELOCIDAD_RAPIDA = 1, VELOCIDAD_MUY_RAPIDA = 2 };
inline int& velocidad() { static int v = VELOCIDAD_NORMAL; return v; }   // (lo pone menu_principal.h)
inline int velocidadAnim() { return velocidad(); }

// rama classic-ps5-duelo-log: "Bitacora del duelo" (Si por defecto): el juego escribe duelo_log.txt (duelo_log.h). Clave
// bitacora_duelo=0/1 en opciones.txt. En No no se abre ni se escribe nada.
inline bool& bitacora() { static bool v = true; return v; }   // (lo pone menu_principal.h)

}  // namespace opciones
