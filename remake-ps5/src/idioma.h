/*
 * Yugioh Classic Remaster - idioma de la interfaz (rama classic-idioma): Espanol (por defecto) o Ingles.
 *
 * - tr("texto en espanol") devuelve el texto en el idioma actual (la clave ES el texto en espanol; si falta en la
 *   tabla, queda en espanol). interfaz.h la aplica sola en texto()/medir()/parrafo(): cualquier literal de la
 *   interfaz o de control.h (menus, avisos, ayuda del mando) que este en la tabla sale traducido.
 * - Las cartas en ingles salen de assets/ui/cartas_ui_en.txt y de las caras cara_en_<id>.png / mini_en_<id>.png
 *   (si falta la inglesa de una carta, se usa la espanola).
 * - La preferencia se guarda en idioma.txt ("es" o "en") junto a capturas.txt (consola: /data/homebrew/<TITULO>/,
 *   es decir getAssetPath() + "../"). En el PC, YUGI_IDIOMA=en|es manda sobre el archivo.
 * - Cambiar el idioma sube version(): la interfaz recarga las caras visibles en el siguiente cuadro (en caliente).
 * - Caras HD: cara_hd_<id>.jpg / cara_hd_en_<id>.jpg en el visor y el detalle (una sola cargada a la vez).
 *
 * COMO REGISTRAR TEXTOS NUEVOS (ramas classic-constructor, classic-ia, classic-base, las que vengan):
 *  1. Lo que se dibuja con interfaz.h texto()/medir()/parrafo() se traduce solo si la frase exacta en espanol esta en
 *     tabla() (abajo): agregar { "texto en espanol", "English text" }. Las partes variables (numeros, nombres de
 *     cartas) se unen DESPUES de traducir cada pieza: std::to_string(n) + idioma::tr(" cartas restantes").
 *  2. Un archivo propio puede tener su tabla (como constructor_textos.h) y caer a idioma::tr en lo que no tenga, o
 *     usar idioma::t("es", "en") en el mismo lugar del literal (sin tocar esta tabla).
 *  3. Datos de cartas: siempre por CartaUI::vNombre()/vAtributo()/vTipo()/vDesc() (nunca .nombre a pelo) o, fuera
 *     de interfaz.h, cartas_ui<idioma::sufijo()>.txt. Para refrescar algo propio al cambiar, comparar idioma::version().
 *  4. Ventana de Ajustes (ImGui): overlay->checkBox(idioma::tr("..."), ...).
 *  Ningun texto visible debe quedar fuera: con Espanol todo en espanol y con English todo en ingles.
 */
#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>

#include "guardado.h"   // rama classic-pulido: guardado atomico y avisos

namespace idioma {

enum { ES = 0, EN = 1 };

inline int& actual() { static int v = ES; return v; }
inline int& version() { static int v = 0; return v; }   // sube con cada cambio (la interfaz recarga las caras)
inline bool ingles() { return actual() == EN; }
inline std::string& rutaPreferencia() { static std::string r; return r; }
// el sufijo de los archivos de cartas: cartas_ui<suf>.txt, cara<suf>_<id>.png ("" o "_en")
inline const char* sufijo() { return ingles() ? "_en" : ""; }

// ---------------------------------------------------------------------------------------------- tabla ES -> EN
inline const std::unordered_map<std::string, std::string>& tabla()
{
	static const std::unordered_map<std::string, std::string> t = {
		// fases (columna y cartel)
		{ "FASE", "PHASE" },
		{ "ROBO", "DRAW" }, { "ESPERA", "STANDBY" }, { "PRINCIPAL 1", "MAIN 1" }, { "BATALLA", "BATTLE" },
		{ "PRINCIPAL 2", "MAIN 2" }, { "FINAL", "END" },
		{ "FASE DE ROBO", "DRAW PHASE" }, { "FASE DE ESPERA", "STANDBY PHASE" }, { "FASE PRINCIPAL 1", "MAIN PHASE 1" },
		{ "FASE DE BATALLA", "BATTLE PHASE" }, { "FASE PRINCIPAL 2", "MAIN PHASE 2" }, { "FASE FINAL", "END PHASE" },
		// camaras
		{ "CÁMARA", "CAMERA" }, { "CINE", "CINEMA" }, { "TABLERO", "BOARD" }, { "CENITAL", "TOP-DOWN" },
		{ "LATERAL", "SIDE" }, { "LIBRE", "FREE" },
		// LP y zonas
		{ "RIVAL", "OPPONENT" }, { "JUGADOR", "PLAYER" },
		{ "MAZO", "DECK" }, { "FUSIÓN", "FUSION" }, { "CEMENTERIO", "GRAVEYARD" },
		{ "MANO DEL RIVAL", "OPPONENT'S HAND" }, { " cartas ocultas", " hidden cards" },
		{ "El rival guarda estas cartas boca abajo. Su contenido se revelara cuando las juegue.",
		  "Your opponent keeps these cards face-down. They will be revealed when played." },
		{ "MAZO DEL RIVAL", "OPPONENT'S DECK" }, { "TU MAZO", "YOUR DECK" }, { " cartas restantes", " cards left" },
		{ "Al comenzar cada turno se roba la carta de arriba. Si no quedan cartas para robar, el duelo se pierde.",
		  "At the start of each turn you draw the top card. If there are no cards left to draw, you lose the duel." },
		{ "ZONA DE FUSIÓN", "FUSION ZONE" }, { "Sin monstruos de fusión", "No Fusion Monsters" },
		{ "Aqui esperan los monstruos de fusion hasta que se invoquen con Polimerizacion.",
		  "Fusion Monsters wait here until they are summoned with Polymerization." },
		{ "CEMENTERIO DEL RIVAL", "OPPONENT'S GRAVEYARD" }, { "TU CEMENTERIO", "YOUR GRAVEYARD" },
		{ "Vacío", "Empty" }, { "Vacía", "Empty" },
		{ "Las cartas destruidas, usadas o descartadas terminan aqui, boca arriba.",
		  "Destroyed, used or discarded cards end up here, face-up." },
		{ "CARTA BOCA ABAJO", "FACE-DOWN CARD" },
		{ "Colocada por el rival", "Set by your opponent" }, { "Colocada por ti", "Set by you" },
		{ "Una magia o trampa oculta. Cuidado al atacar.", "A hidden Spell or Trap. Be careful when attacking." },
		{ "Tu carta colocada, lista para activarse.", "Your Set card, ready to be activated." },
		{ "ZONA DE MONSTRUO", "MONSTER ZONE" }, { "ZONA DE MAGIA Y TRAMPA", "SPELL & TRAP ZONE" },
		{ "ZONA DE CAMPO", "FIELD ZONE" },
		{ "Zona del rival sin cartas.", "Opponent's zone with no cards." },
		{ "Puedes colocar una carta aqui.", "You can place a card here." },
		{ "Una carta oculta del rival. Se revelara cuando la juegue o la destruyas.",
		  "A hidden card of your opponent. It will be revealed when played or destroyed." },
		{ " / Efecto", " / Effect" }, { " / Fusión", " / Fusion" }, { " / Ritual", " / Ritual" },
		// menu de acciones (control.h)
		{ "ACCIONES", "ACTIONS" },
		{ "Invocar", "Summon" }, { "Colocar boca abajo", "Set" }, { "Activar", "Activate" }, { "Colocar", "Set" },
		{ "Cancelar", "Cancel" }, { "Voltear (invocación)", "Flip Summon" }, { "Atacar", "Attack" },
		{ "Cambiar a ataque", "Change to Attack" }, { "Cambiar a defensa", "Change to Defense" },
		{ "Enviar al cementerio", "Send to Graveyard" }, { "Voltear", "Flip" }, { "Destruir", "Destroy" },
		// avisos y motivos
		{ "No hay zonas de monstruo libres", "No free Monster Zones" },
		{ "Ya hay una carta de campo: destrúyela primero", "There is already a Field card: destroy it first" },
		{ "No hay zonas de magia y trampa libres", "No free Spell & Trap Zones" },
		{ "En defensa no puede atacar", "Cannot attack in Defense Position" },
		{ "Sin modelo 3D: lo resolverá el motor de reglas", "No 3D model: the rules engine will resolve it" },
		{ "Espera a que termine el ataque", "Wait for the attack to finish" },
		{ "La carta de campo está en plena transición", "The Field card is changing" },
		{ "No se puede", "Not possible" }, { "No hay zonas libres", "No free zones" },
		{ "El rival no tiene monstruos a los que atacar", "Your opponent has no monsters to attack" },
		{ "Demo automática apagada: tienes el control", "Auto demo off: you have control" },
		// barra de ayuda del mando
		{ "Mover", "Move" }, { "Acciones", "Actions" }, { "Ver", "View" }, { "Detalle", "Details" },
		{ "Fase", "Phase" }, { "Cámara", "Camera" }, { "Opción", "Option" }, { "Confirmar", "Confirm" },
		{ "Zona", "Zone" }, { "Atrás", "Back" }, { "Objetivo", "Target" }, { "Cerrar", "Close" },
		// atributos y tipos de respaldo
		{ "Figura 3D", "3D Figure" },
		// Ajustes (ventana de OPTIONS)
		{ "Ajustes", "Settings" }, { "Idioma", "Language" },
		{ "Animar monstruos", "Animate monsters" }, { "Camara que respira", "Breathing camera" },
		{ "Dificultad adaptable", "Adaptive difficulty" },   // (rama classic-variedad)
		{ "IA", "AI" }, { "Humana", "Human" }, { "Clásica", "Classic" },   // (rama classic-ia-humana)
		{ "IA que aprende", "Learning AI" }, { "Olvidar lo aprendido", "Forget what it learned" }, { "Hecho", "Done" },   // (rama classic-ia-red)
		{ "Demo automatica", "Auto demo" }, { "Ataques automaticos", "Auto attacks" }, { "Camara", "Camera" },
		{ "Cine", "Cinema" }, { "Tablero", "Board" }, { "Cenital", "Top-down" }, { "Lateral", "Side" }, { "Libre", "Free" },
		{ "Brillo holografico", "Holo shine" }, { "Ataque por casillas", "Attack by zones" },
		{ "Combinacion", "Combination" },
		{ "Carta de campo: destruir", "Field card: destroy" }, { "Carta de campo: activar", "Field card: activate" },
		{ "Bosque automatico", "Auto Forest" }, { "Entorno: siguiente", "Environment: next" }, { "Mano de figuras nuevas", "Hand of new figures" },
		{ "Robar figura", "Draw figure" }, { "Despejar mis monstruos", "Clear my monsters" },
		{ "Atacar ahora", "Attack now" }, { "Salir", "Quit" },
		{ "Animar", "Animate" }, { "Mano nueva", "New hand" }, { "Robar carta", "Draw card" },
		{ "Cartas de monstruo: %d", "Monster cards: %d" },
		// duelo real contra la IA (rama classic-integracion: control.h, duelo_juego.h, duelo_ui.h)
		{ "Descarta hasta tener 6 cartas en la mano", "Discard down to 6 cards in hand" }, { "Descartar", "Discard" },
		{ "Descarta hasta tener %d cartas en la mano", "Discard down to %d cards in hand" },
		{ "Invocar Toon", "Toon Summon" }, { "sacrificio", "tribute" }, { "sacrificios", "tributes" },
		{ "Se invoca con su magia de ritual", "Summoned with its Ritual Spell" }, { "Fusión", "Fusion" }, { "Ritual", "Ritual" },
		{ "Faltan niveles para el sacrificio", "Not enough levels to tribute" }, { "Espera tu turno", "Wait for your turn" },
		{ "Solo en tu fase principal", "Only in your Main Phase" }, { "Faltan monstruos para el sacrificio", "Not enough monsters to tribute" },
		{ "No se puede invocar de forma normal", "Cannot be Normal Summoned" }, { "Ya invocaste este turno", "Already summoned this turn" }, { "No se puede invocar ahora", "Cannot be Summoned now" }, { "No se puede sacrificar", "Cannot Tribute" }, { "Faltan LP", "Not enough LP" },
		{ "Llegó este turno", "Arrived this turn" }, { "Ya cambió de posición este turno", "Already changed position this turn" },
		{ "Solo en la fase de batalla", "Only in the Battle Phase" }, { "No se ataca en el primer turno", "No attacks on the first turn" },
		{ "Ya atacó este turno", "Already attacked this turn" },
		{ "Elige 1 monstruo para sacrificar", "Choose 1 monster to tribute" }, { "Elige 2 monstruos para sacrificar", "Choose 2 monsters to tribute" },
		{ "Monstruo", "Monster" }, { "Sacrificar", "Tribute" },
		{ "Sin rivales (falta rivales.txt)", "No opponents (rivales.txt missing)" }, { "Empiezas tú", "You go first" },
		{ "Empieza el rival", "Your opponent goes first" }, { "Tu turno", "Your turn" }, { "Turno del rival", "Opponent's turn" },
		{ "Abandonar duelo", "Forfeit duel" }, { "ELIGE A TU RIVAL", "CHOOSE YOUR OPPONENT" }, { "Victorias", "Wins" },
		{ "Derrotas", "Losses" }, { "Nivel", "Level" }, { "Duelo", "Duel" }, { "Otro rival", "Another opponent" }, { "TÁCTIL", "TOUCH" },
		// modo historia: personajes del anime (rama classic-personajes)
		{ "MODO HISTORIA", "STORY MODE" }, { "Saga", "Saga" }, { "Mazo", "Deck" }, { "VENCIDO", "DEFEATED" }, { "DISPONIBLE", "AVAILABLE" },
		{ "BLOQUEADO: vence a todos los de la saga anterior", "LOCKED: defeat everyone in the previous saga" },
		{ "Vence a todos los de la saga anterior", "Defeat everyone in the previous saga" },
		{ "Personajes vencidos", "Characters defeated" }, { "Personaje / saga", "Character / saga" }, { "Duelo libre", "Free duel" },
		{ "Modo historia", "Story mode" }, { "¡PERSONAJE VENCIDO!", "CHARACTER DEFEATED!" },
		{ "cartas firma desbloqueadas", "signature cards unlocked" }, { "NUEVA SAGA", "NEW SAGA" },
		{ "Vence a ", "Defeat " }, { "BLOQUEADA", "LOCKED" }, { "CARTA FIRMA BLOQUEADA", "SIGNATURE CARD LOCKED" },
		{ "Carta firma bloqueada", "Signature card locked" }, { "Un Dios no se puede colocar", "A God cannot be Set" }, { "Primero elige una carta (X)", "Choose a card first (X)" }, { "No hay monstruos del rival que se puedan atacar", "None of your opponent's monsters can be attacked" }, { "Cold Wave: no se pueden colocar ni activar magias y trampas", "Cold Wave: Spells and Traps cannot be Set or activated" }, { "Un efecto le impide cambiar de posición", "An effect prevents it from changing position" }, { "Light of Intervention: no se puede colocar", "Light of Intervention: monsters cannot be Set" },
		{ "Construir mazo", "Build deck" }, { "¡VICTORIA!", "VICTORY!" }, { "DERROTA", "DEFEAT" }, { "EMPATE", "DRAW" },
		{ "Rival", "Opponent" }, { "NIVEL DESBLOQUEADO", "LEVEL UNLOCKED" }, { "¡ERES EL CAMPEÓN!", "YOU ARE THE CHAMPION!" },
		{ "Continuar", "Continue" },
		// menu de pausa y ayuda de fases (rama classic-arreglos: duelo_juego.h, duelo_ui.h, control.h)
		{ "PAUSA", "PAUSE" }, { "Pausa", "Pause" }, { "Registro", "Log" }, { "Volver", "Back" }, { "Abandonar duelo (derrota)", "Forfeit duel (counts as a loss)" },
		{ "Salir al menú principal", "Exit to main menu" }, { "Ir a la Batalla", "Go to Battle" }, { "Ir a Principal 2", "Go to Main 2" },
		{ "Terminar turno", "End turn" }, { "Terminar turno (en espera)", "End turn (queued)" },
		{ "No se puede terminar el turno ahora", "You can't end the turn right now" }, { "Termina el turno en espera", "End of turn queued" },
		{ "Fase (en espera)", "Phase (queued)" }, { "Cambia de fase al terminar la animación", "Phase changes when the animation ends" },
		// motor de efectos (rama classic-efectos: control.h, duelo_juego.h)
		{ "Activar efecto", "Activate effect" }, { "No activar", "Don't activate" }, { "El rival ataca con", "Your opponent attacks with" },
		{ "El rival invocó", "Your opponent summoned" }, { "Invocaste", "You summoned" }, { "El rival activó", "Your opponent activated" },
		{ "Activaste", "You activated" }, { "Fase final del rival", "Opponent's End Phase" }, { "¿Activar una carta?", "Activate a card?" },
		{ "Elige una carta para descartar", "Choose a card to discard" }, { "Elige el objetivo", "Choose the target" },
		{ "Carta", "Card" }, { "Elegir", "Choose" }, { "campo", "field" }, { "mano", "hand" }, { "cementerio", "Graveyard" },
		{ "desterrada", "banished" }, { "mazo", "deck" }, { "fusión", "Extra Deck" }, { "rival", "opponent" },
		{ "Cadena", "Chain" }, { "Un efecto le impide atacar", "An effect prevents it from attacking" },
		{ "Invocación especial", "Special Summon" }, { "Invocación normal (Géminis)", "Gemini Summon" },
		// rama classic-torneo-extras: los mazos de torneo en el constructor
		{ "MAZOS DE TORNEO", "TOURNAMENT DECKS" }, { "Mazos de torneo", "Tournament decks" }, { "Copiar a mis mazos", "Copy to my decks" },
		{ "Copiar y editar", "Copy and edit" }, { "No hay mazos de torneo", "No tournament decks" },
		{ "X: copiar a tus mazos para usarlo o editarlo", "X: copy it to your decks to use or edit it" },
		{ "Mazo de torneo copiado: ", "Tournament deck copied: " },
		{ "Mazo de torneo: Cuadrado lo copia a tus mazos", "Tournament deck: Square copies it to your decks" },
		{ "Elige la carta a desterrar (coste)", "Choose the card to banish (cost)" },
		{ "El rival entra en la fase de batalla", "Your opponent enters the Battle Phase" }, { "Paso de daño", "Damage Step" }, { "Atacar directamente", "Attack directly" }, { "Equipar", "Equip" },
		{ "Elige el monstruo a sacrificar (coste)", "Choose the monster to Tribute (cost)" },
		{ "Ataque", "Attack" }, { "Defensa", "Defense" }, { "No", "No" }, { "Ataque negado", "Attack negated" }, { "Negado", "Negated" },
		{ "Volumen musica", "Music volume" }, { "Volumen efectos", "Effects volume" },
		// elecciones (rama classic-elecciones: control.h, motivos del menu y del boton de fase)
		{ "The Last Warrior from Another Planet: nadie puede invocar", "The Last Warrior from Another Planet: no one can Summon" },
		{ "No puedes invocar este turno", "You cannot Summon this turn" },
		{ "Primero ataca (Amazoness Archers)", "Attack first (Amazoness Archers)" },
		{ "Amazoness Archers: tus monstruos deben atacar este turno", "Amazoness Archers: your monsters must attack this turn" },
		// motor2 (rama classic-motor2: control.h, ventanas y fusiones de contacto)
		{ "Fase de espera del rival", "Opponent's Standby Phase" }, { "Tu fase de espera", "Your Standby Phase" },
		{ "El rival termina la fase de batalla", "Your opponent ends the Battle Phase" }, { "El rival colocó un monstruo", "Your opponent Set a monster" },
		// menu principal y Opciones (rama classic-menus: menu_principal.h, menu_ui.h, duelo_ui.h)
		{ "MENÚ PRINCIPAL", "MAIN MENU" }, { "OPCIONES", "OPTIONS" }, { "Historia", "Story" }, { "Opciones", "Options" },
		{ "Salir del juego", "Quit game" }, { "Volumen de la música", "Music volume" }, { "Volumen de los efectos", "Effects volume" },
		{ "¿Salir del juego? Pulsa X otra vez para confirmar", "Quit the game? Press X again to confirm" },
		{ "Opción / valor", "Option / value" }, { "Nivel / rival", "Level / opponent" }, { "Menú principal", "Main menu" },
		{ "Figuras 3D: %d (ultima: %s)", "3D figures: %d (latest: %s)" },
		// rama classic-duelo-arreglos: boton de fase del HUD (interfaz.h)
		{ "SIGUIENTE FASE", "NEXT PHASE" }, { "TERMINAR TURNO", "END TURN" }, { "Le toca al rival", "Your opponent's turn next" },
		// rama classic-md-fases: selector de fases (OPTIONS), engranaje y carteles de Master Duel (fases_md.h, fases_md_ui.h)
		{ "Iniciar Fase de Batalla", "Start Battle Phase" }, { "Ir a M2", "Go to Main Phase 2" }, { "Finalizar turno", "End Turn" },
		{ "SELECCIONA LA FASE", "SELECT PHASE TO CHANGE TO" }, { "Fases", "Phases" }, { "Menú", "Menu" }, { "Menú del duelo", "Duel menu" },
		{ "FASES", "PHASES" }, { "TU TURNO", "YOUR TURN" }, { "¡DUELO!", "DUEL!" }, { "TURNO DEL RIVAL", "OPPONENT'S TURN" }, { "Turno %d", "Turn %d" },
		{ "Ahora", "Now" }, { "Batalla", "Battle" }, { "Principal 2", "Main 2" }, { "Final", "End" },
		{ "Primero responde a la pregunta", "Answer the question first" }, { "No se puede cambiar de fase durante un ataque", "You can't change phases during an attack" },
		{ "Ahora no se puede cambiar de fase", "You can't change phases now" }, { "Espera a que termine la animación", "Wait for the animation to finish" },
		// rama classic-moneda: el lanzamiento de moneda antes del duelo (moneda_ui.h)
		{ "LANZAMIENTO DE MONEDA", "COIN TOSS" }, { "Cara: decides tú · Cruz: decide el rival", "Heads: you decide · Tails: your opponent decides" },
		{ "¡CARA!", "HEADS!" }, { "¡CRUZ!", "TAILS!" }, { "Tú", "You" },
		{ "Ganaste el lanzamiento de moneda", "You won the coin toss" }, { "El rival ganó el lanzamiento", "Your opponent won the coin toss" },
		{ "Elige: ¿ir primero o segundo?", "Choose: go first or second?" }, { "Ir primero", "Go first" }, { "Ir segundo", "Go second" },
		{ "El rival decide ir primero", "Your opponent decides to go first" }, { "El rival decide ir segundo", "Your opponent decides to go second" },
		{ "Figura 3D nueva: todavía sin datos de carta (preparar_figuras.py).", "New 3D figure: no card data yet." },
		// rama classic-portada: portada y menu principal con el arte (portada_ui.h, menu_ui.h, menu_principal.h)
		{ "Pulsa X para comenzar", "Press X to start" }, { "MODO DUELO", "DUEL MODE" }, { "CONSTRUCCIÓN DE DECK", "DECK BUILDER" },
		{ "LISTA DE CARTAS", "CARD LIST" }, { "SALIR", "QUIT" }, { "Permitir cartas prohibidas", "Allow forbidden cards" },
		{ "Sí", "Yes" }, { "Preguntar antes de activar", "Ask before activating" }, { "Auto", "Auto" }, { "Siempre", "Always" },
		{ "Nunca", "Never" }, { "Velocidad de animación", "Animation speed" }, { "Normal", "Normal" }, { "Rápida", "Fast" },
		{ "Muy rápida", "Very fast" }, { "Figuras 3D", "3D figures" },
		{ "Bitácora del duelo", "Duel log" },   // rama classic-ps5-duelo-log
		// constructor2 (rama classic-constructor2: constructor.h, constructor_ui.h, mazo.h; bloque propio, contiguo)
		{ "MIS MAZOS", "MY DECKS" }, { "PROHIBIDAS PERMITIDAS", "FORBIDDEN ALLOWED" }, { "Goat: ", "Goat: " }, { " (Opciones: sin límite)", " (Options: no limit)" }, { "LISTA DE CARTAS · FORMATO GOAT", "CARD LIST · GOAT FORMAT" }, { "Triángulo: buscar y filtrar · Círculo: volver", "Triangle: search & filter · Circle: back" }, { "+ Nuevo mazo", "+ New deck" }, { "VÁLIDO", "VALID" }, { "NO VÁLIDO", "NOT VALID" }, { "NO VÁLIDO: ", "NOT VALID: " },
		{ "PRINCIPAL", "MAIN" }, { "EXTRA", "EXTRA" }, { "MAZO EXTRA", "EXTRA DECK" }, { "SIN GUARDAR", "UNSAVED" }, { "OPTIONS guarda", "OPTIONS saves" },
		{ "Mazo de los duelos (en uso)", "Deck for duels (in use)" }, { "EN USO", "IN USE" }, { "Válido: Cuadrado lo usa en los duelos", "Valid: Square uses it in duels" },
		{ "Crea un mazo vacío y ponle nombre con el teclado en pantalla. Se guarda cuando tenga entre 40 y 60 cartas.",
		  "Create an empty deck and name it with the on-screen keyboard. It can be saved once it has 40 to 60 cards." },
		{ "Elige una carta para ver su detalle", "Pick a card to see its details" },
		{ "X: acciones del mazo · Cuadrado: usar en los duelos", "X: deck actions · Square: use in duels" },
		{ "X agrega, Cuadrado quita, OPTIONS guarda", "X adds, Square removes, OPTIONS saves" },
		{ "Editar", "Edit" }, { "Usar en los duelos", "Use in duels" }, { "Renombrar", "Rename" }, { "Copiar", "Copy" }, { "Borrar", "Delete" },
		{ "¿Borrar el mazo?", "Delete the deck?" }, { "Hay cambios sin guardar", "There are unsaved changes" },
		{ "Guardar y salir", "Save and exit" }, { "Salir sin guardar", "Exit without saving" }, { "Seguir editando", "Keep editing" },
		{ "Guardar", "Save" }, { "Agregar", "Add" }, { "Quitar", "Remove" }, { "Página", "Page" }, { "Clase", "Class" },
		{ "Buscar y filtrar", "Search & filter" }, { "Mazo / colección", "Deck / collection" }, { "Elegir y cambiar", "Pick and change" },
		{ "Aceptar", "Accept" }, { "Esta fila a Todos", "Reset this row" }, { "Quitar filtros", "Clear filters" }, { "Escribir", "Type" },
		{ "Espacio", "Space" }, { "Mayúsculas", "Caps" },
		{ "Mazo guardado", "Deck saved" }, { "Mazo copiado", "Deck copied" }, { "Mazo renombrado", "Deck renamed" }, { "Mazo borrado: ", "Deck deleted: " },
		{ "Mazo para los duelos: ", "Deck for duels: " }, { "Mazo nuevo", "New deck" }, { " (copia)", " (copy)" }, { "Cambios descartados", "Changes discarded" },
		{ "No se puede guardar: ", "Cannot save: " }, { "No se puede usar: ", "Cannot use: " }, { "Ya hay 20 mazos: borra uno", "There are already 20 decks: delete one" },
		{ "El mazo está vacío", "The deck is empty" }, { "El nombre no puede quedar vacío", "The name cannot be empty" }, { "al mazo extra", "to the Extra Deck" },
		{ "Clase: ", "Class: " }, { "Orden: ", "Sort: " }, { " cartas encontradas", " cards found" },
		{ "Sin filtros · Triángulo: buscar y filtrar", "No filters · Triangle: search & filter" },
		{ " para desbloquear", " to unlock" }, { "carta firma bloqueada", "signature card locked" }, { "prohibida en el Formato Goat", "forbidden in the Goat Format" },
		{ "limitada a 1 copia", "limited to 1 copy" }, { "semilimitada a 2 copias", "semi-limited to 2 copies" }, { "máximo 3 copias", "up to 3 copies" },
		// filtros
		{ "BUSCAR Y FILTRAR", "SEARCH & FILTER" }, { "Nombre", "Name" }, { "Subtipo", "Subtype" }, { "Atributo", "Attribute" }, { "Tipo", "Type" },
		{ "Orden", "Sort" }, { "Listo", "Done" }, { "(cualquiera)", "(any)" }, { "Todas", "All" }, { "Todos", "All" },
		{ "Normal", "Normal" }, { "Efecto", "Effect" }, { "Magia", "Spell" }, { "Trampa", "Trap" }, { "Campo", "Field" }, { "Equipo", "Equip" },
		{ "Continua", "Continuous" }, { "Rápida", "Quick-Play" }, { "Contraataque", "Counter" }, { "Volteo", "Flip" }, { "Toon", "Toon" },
		{ "Unión", "Union" }, { "Espíritu", "Spirit" }, { "ATK", "ATK" }, { "DEF", "DEF" },
		{ "Clase y nombre", "Class and name" }, { "ATK (mayor)", "ATK (highest)" }, { "DEF (mayor)", "DEF (highest)" }, { "Nivel (mayor)", "Level (highest)" },
		// tipos de carta en el detalle
		{ "Magia normal", "Normal Spell" }, { "Magia de campo", "Field Spell" }, { "Magia de equipo", "Equip Spell" }, { "Magia continua", "Continuous Spell" },
		{ "Magia rápida", "Quick-Play Spell" }, { "Magia de ritual", "Ritual Spell" }, { "Trampa normal", "Normal Trap" }, { "Trampa continua", "Continuous Trap" },
		{ "Trampa de contraataque", "Counter Trap" }, { "Monstruo normal", "Normal Monster" }, { "Monstruo de efecto", "Effect Monster" },
		{ "Monstruo de fusión", "Fusion Monster" }, { "Monstruo de ritual", "Ritual Monster" }, { "Monstruo de volteo", "Flip Monster" },
		{ "Monstruo toon", "Toon Monster" }, { "Monstruo de unión", "Union Monster" }, { "Monstruo espíritu", "Spirit Monster" },
		// teclado en pantalla
		{ "Buscar por nombre", "Search by name" }, { "Nombre del mazo nuevo", "New deck name" }, { "Renombrar mazo", "Rename deck" },
		{ "MAYÚS", "CAPS" }, { "ESPACIO", "SPACE" }, { "BORRAR", "DELETE" }, { "ACEPTAR", "OK" },
		// rama classic-pulido: guardado (guardado.h, interfaz.h), dado y moneda (efectos_cartas.h), detalle con desplazamiento
		{ "No se pudo guardar", "Could not save" },
		{ "No se pudo crear /data/yugioh_classic: el progreso podría no guardarse", "Could not create /data/yugioh_classic: progress may not be saved" },
		{ "Dado: ", "Die: " }, { "Moneda: cara", "Coin: heads" }, { "Moneda: cruz", "Coin: tails" },
		{ "Desplazar", "Scroll" }, { "Figuras 3D", "3D figures" },
		// cartas-clave (rama classic-cartas-clave: entrada especial de las cartas clave, entrada_clave_ui.h)
		{ "CARTA CLAVE", "KEY CARD" }, { "DIOS EGIPCIO", "EGYPTIAN GOD" }, { "INVOCACIÓN POR FUSIÓN", "FUSION SUMMON" },
		{ "INVOCACIÓN RITUAL", "RITUAL SUMMON" }, { "Saltar", "Skip" },
		// md-entradas (rama classic-md-entradas: fusion/ritual de Master Duel, menu del duelo y fin del duelo)
		{ "MATERIALES DE FUSIÓN", "FUSION MATERIALS" }, { "SACRIFICIOS DEL RITUAL", "RITUAL TRIBUTES" },
		// galeria
		{ "GALERÍA DE FIGURAS 3D", "3D FIGURE GALLERY" }, { " FIGURAS", " FIGURES" }, { " DESBLOQUEADAS", " UNLOCKED" },
		{ "No hay figuras 3D cargadas", "No 3D figures loaded" }, { "Ataque: ", "Attack: " }, { "Animada (esqueleto)", "Animated (skeleton)" },
		{ "Embestida", "Charge" }, { "Espada", "Sword" }, { "Hechizo", "Spellcast" }, { "Aliento", "Breath" }, { "Rayo", "Lightning" },
		{ "Girar y acercar", "Rotate and zoom" }, { "Cerrar detalle", "Close details" }, { "Zoom", "Zoom" }, { "Vista inicial", "Reset view" },
		{ "Ver figura", "View figure" }, { "Galería 3D", "3D gallery" }, { "Esta carta no tiene figura 3D", "This card has no 3D figure" },
		// extras (rama classic-extras: Opciones > Personalizacion; lo demas va con idioma::t en cosmeticos*.h)
		{ "Personalización", "Customization" },
		// reglas oficiales (rama classic-reglas-oficiales: repeticion del ataque y ventana tras el danno)
		{ "Ataque directo", "Direct attack" }, { "No atacar", "Do not attack" }, { "Recibiste daño", "You took damage" },
		{ "Al cementerio", "To the Graveyard" }, { "Desterrada", "Banished" }, { "A la mano", "To the hand" }, { "Descartada", "Discarded" },
		{ "Al mazo", "To the Deck" }, { "Al mazo de fusión", "To the Fusion Deck" }, { "Robas", "You draw" }, { "Boca abajo", "Face-down" }, { "EL RIVAL ACTIVA UNA TRAMPA", "YOUR OPPONENT ACTIVATES A TRAP" },
		{ "EL RIVAL ACTIVA UNA MAGIA", "YOUR OPPONENT ACTIVATES A SPELL" }, { "EL RIVAL ACTIVA UN EFECTO", "YOUR OPPONENT ACTIVATES AN EFFECT" },
		// duelo en linea (rama classic-sala: buscar rival con src/red/sala.h; pantallas en docs/en_linea.md)
		{ "DUELO EN LÍNEA", "ONLINE DUEL" }, { "DUELO EN LÍNEA: NO DISPONIBLE EN PC", "ONLINE DUEL: NOT AVAILABLE ON PC" }, { "No disponible en PC", "Not available on PC" }, { "Buscar rival", "Find opponent" }, { "Jugadores en línea", "Players online" },
		{ "Jugadores en línea: ", "Players online: " }, { "Buscando ahora: ", "Searching now: " }, { "Consultando…", "Checking…" },
		{ "Buscando rival…", "Looking for an opponent…" }, { "Tiempo de espera: ", "Waiting time: " },
		{ "La sala no responde: buscando por la red P2P", "The lobby is not responding: searching the P2P network" },
		{ "Jugar contra la IA mientras esperas", "Play against the AI while you wait" }, { "Cancelar búsqueda", "Cancel search" },
		{ "¡Rival encontrado!", "Opponent found!" }, { "Rechazar", "Decline" }, { "Conectando…", "Connecting…" },
		{ "No se pudo conectar con el rival", "Could not connect to the opponent" }, { "Sin conexión a internet", "No internet connection" },
		{ "No se pudo abrir la red", "Could not open the network" }, { "Reintentar", "Retry" },
		{ "Tu ELO: ", "Your ELO: " }, { "Partidas en línea: ", "Online matches: " }, { "ELO pendiente de confirmar", "ELO pending confirmation" },
		{ "AVISO: TU IP SERÁ VISIBLE", "NOTICE: YOUR IP WILL BE VISIBLE" },
		{ "El duelo en línea conecta tu consola directamente con la del rival. Tu dirección IP será visible para tu rival y, si la sala no responde, también en la red pública de BitTorrent (DHT) que se usa para encontrar jugadores.",
		  "Online duels connect your console directly to your opponent's. Your IP address will be visible to your opponent and, if the lobby is not responding, also on the public BitTorrent network (DHT) used to find players." },
		// modo en linea: avisos de la conexion directa (rama classic-red-nat, red/nat.h avisoRed y TEXTO_PUERTO_MANUAL)
		{ "Sin conexión a internet.", "No internet connection." },
		{ "Tu proveedor usa CGNAT: la conexión directa puede fallar. Pide una IP pública a tu proveedor o usa IPv6.",
		  "Your ISP uses CGNAT: a direct connection may fail. Ask your ISP for a public IP or use IPv6." },
		{ "Tu router tiene NAT simétrico: la conexión directa puede fallar. Activa UPnP en el router o abre el puerto a mano.",
		  "Your router has a symmetric NAT: a direct connection may fail. Enable UPnP on the router or open the port manually." },
		{ "Para abrirlo a mano: en el router, reenvía el puerto UDP a esta consola. Puerto:",
		  "To open it manually: on the router, forward the UDP port to this console. Port:" },
		// el DUELO EN LINEA en el juego (rama classic-en-linea: en_linea_juego.h, en_linea_ui.h)
		{ "Red local", "Local network" }, { "RED LOCAL", "LOCAL NETWORK" }, { "Código de conexión", "Connection code" },
		{ "CÓDIGO DE CONEXIÓN", "CONNECTION CODE" }, { "CÓDIGO DEL RIVAL", "OPPONENT'S CODE" }, { "Mazo: ", "Deck: " }, { "no válido", "not valid" },
		{ "Tu mazo no es válido: ", "Your deck is not valid: " }, { "Cambiar de mazo", "Change deck" }, { "Nombre: ", "Name: " }, { "Duelos: ", "Duels: " },
		{ "Aviso de privacidad (en línea)", "Privacy notice (online)" }, { "Aceptado", "Accepted" }, { "Pendiente", "Pending" },
		{ "En los duelos P2P el rival puede ver tu dirección IP.", "In P2P duels your opponent can see your IP address." },
		{ "Preparando la conexión…", "Preparing the connection…" }, { "Jugar contra la IA mientras espero", "Play against the AI while I wait" },
		{ "Ir al duelo en línea", "Go to the online duel" }, { "Crear duelo en la red local", "Create a duel on the local network" },
		{ "Nadie anunció un duelo en tu red todavía", "Nobody has announced a duel on your network yet" }, { "Elige un duelo para unirte", "Choose a duel to join" },
		{ "No se pudo abrir la red local", "Could not open the local network" }, { "otra versión", "other version" },
		{ "Otra versión del juego: no se puede unir", "Another game version: cannot join" }, { "Esperando rival en la red local…", "Waiting for an opponent on the local network…" },
		{ "Preparando tu código…", "Preparing your code…" }, { "Tu código (pásaselo al rival para que lo escriba):", "Your code (give it to your opponent to type in):" },
		{ "O escribe el código del rival:", "Or type in your opponent's code:" }, { "Escribir el código del rival", "Type in the opponent's code" },
		{ "Código no válido", "Invalid code" }, { "Tecla", "Key" }, { "Buscar otra vez", "Search again" }, { "Tipo de NAT: ", "NAT type: " },
		{ "Duelo interrumpido: no cuenta", "Duel interrupted: it does not count" },
		{ "El rival se desconectó y no volvió. El duelo no cuenta para ninguno de los dos: ni victoria ni derrota, ni ELO.",
		  "Your opponent disconnected and did not come back. The duel does not count for either of you: no win, no loss, no ELO." },
		{ "Esperando al rival…", "Waiting for the opponent…" }, { "Reconectando", "Reconnecting" }, { "Tu tiempo", "Your time" },
		{ "¡Quedan 30 segundos!", "30 seconds left!" }, { "Se acabó tu tiempo", "Your time ran out" }, { "En línea", "Online" },
		{ "El rival está eligiendo…", "Your opponent is choosing…" }, { "Revancha pedida: esperando al rival…", "Rematch requested: waiting for the opponent…" },
		{ "El rival quiere la revancha", "Your opponent wants a rematch" }, { "El rival se fue: no hay revancha", "Your opponent left: no rematch" },
		{ "Buscar otro rival", "Find another opponent" },
	};
	return t;
}

// el texto en el idioma actual, dado en los dos (para literales de otras ramas que no quieran tocar la tabla)
inline const char* t(const char* es, const char* en) { return ingles() ? en : es; }

inline const std::string& tr(const std::string& es)
{
	if (!ingles()) return es;
	const auto& t = tabla();
	auto it = t.find(es);
	return it == t.end() ? es : it->second;
}
inline const char* tr(const char* es)
{
	if (!ingles() || !es) return es;
	const auto& t = tabla();
	auto it = t.find(es);
	return it == t.end() ? es : it->second.c_str();
}

// rama classic-visual-final: los textos de carta en espanol (cartas_ui.txt) traen terminos de reglas en ingles ("End
// Phase", "Standby Phase"...). Al cargarlos en espanol se cambian por los de la interfaz; los datos EN no se tocan
inline std::string reglasEs(std::string s)
{
	static const char* const R[][2] = {
		{ "Main Phase 1", "Fase Principal 1" }, { "Main Phase 2", "Fase Principal 2" }, { "Main Phase", "Fase Principal" },
		{ "End Phase", "Fase Final" },          { "Standby Phase", "Fase de Espera" },   { "Battle Phase", "Fase de Batalla" },
		{ "Draw Phase", "Fase de Robo" },       { "Damage Step", "Paso de Daño" },       { "Battle Step", "Paso de Batalla" },
	};
	for (const auto& r : R) {
		const size_t n = strlen(r[0]);
		for (size_t p = s.find(r[0]); p != std::string::npos; p = s.find(r[0], p)) {
			s.replace(p, n, r[1]);
			p += strlen(r[1]);
		}
	}
	return s;
}

// ---------------------------------------------------------------------------------------------- preferencia
inline void guardar()
{
	if (rutaPreferencia().empty()) return;
	guardado::guardarAtomico(rutaPreferencia(), ingles() ? "en\n" : "es\n");   // (rama classic-pulido: guardado.h)
}

// lee la preferencia (idioma.txt); sin archivo, espanol. YUGI_IDIOMA=en|es manda (PC)
inline void cargar(const std::string& ruta)
{
	rutaPreferencia() = ruta;
	std::string v;
	std::ifstream f(ruta);
	if (f) std::getline(f, v);
	if (const char* e = getenv("YUGI_IDIOMA")) v = e;
	actual() = (v.size() >= 2 && (v[0] == 'e' || v[0] == 'E') && (v[1] == 'n' || v[1] == 'N')) ? EN : ES;
	fprintf(stderr, "[idioma] %s (%s)\n", ingles() ? "en" : "es", ruta.c_str());
}

inline void poner(int i)
{
	i = i == EN ? EN : ES;
	if (i == actual()) return;
	actual() = i;
	version()++;
	guardar();
}

}  // namespace idioma
