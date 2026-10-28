/*
 * Yugioh Classic Remaster - textos de la coleccion, el mazo y la pantalla "Construir mazo" (rama classic-constructor)
 * en Espanol / English. La clave es el texto en espanol (como idioma::tr de la rama classic-idioma); T() devuelve el
 * del idioma actual. Con idioma.h presente (despues de unir classic-idioma) el idioma sale de idioma::ingles() y lo
 * que no este en esta tabla pasa por idioma::tr; sin el, todo queda en espanol.
 * Las partes variables (numeros, nombres de cartas) se unen DESPUES de traducir cada pieza.
 */
#pragma once

#include <string>
#include <unordered_map>

#if __has_include("idioma.h")
#include "idioma.h"
#define CONSTRUCTOR_CON_IDIOMA 1
#endif

namespace constructor {

inline bool enIngles()
{
#if defined(CONSTRUCTOR_CON_IDIOMA)
	return idioma::ingles();
#else
	return false;
#endif
}
// sufijo de los archivos de cartas: cartas_ui<suf>.txt, cara<suf>_<id>.png, mini<suf>_<id>.png ("" o "_en")
inline const char* sufijoIdioma() { return enIngles() ? "_en" : ""; }
// sube con cada cambio de idioma (se recargan los datos y las caras)
inline int versionIdioma()
{
#if defined(CONSTRUCTOR_CON_IDIOMA)
	return idioma::version();
#else
	return 0;
#endif
}

inline const std::unordered_map<std::string, std::string>& textosEn()
{
	static const std::unordered_map<std::string, std::string> t = {
		// pantalla
		{ "CONSTRUIR MAZO", "DECK BUILDER" }, { "MI MAZO", "MY DECK" }, { "COLECCIÓN · FORMATO GOAT", "COLLECTION · GOAT FORMAT" },
		{ "TODAS", "ALL" }, { "NORMAL", "NORMAL" }, { "EFECTO", "EFFECT" }, { "RITUAL", "RITUAL" }, { "FUSIÓN", "FUSION" },
		{ "MAGIA", "SPELL" }, { "TRAMPA", "TRAP" },
		{ "INICIAL ", "INITIAL " }, { "INICIAL: TODAS", "INITIAL: ALL" }, { " CARTAS · PÁG ", " CARDS · PAGE " },
		{ "PROHIBIDA", "FORBIDDEN" }, { "LIMITADA", "LIMITED" }, { "SEMI", "SEMI" },
		{ "LIMITADA (1)", "LIMITED (1)" }, { "SEMILIMITADA (2)", "SEMI-LIMITED (2)" }, { "SIN LÍMITE (3)", "UNLIMITED (3)" },
		{ "EN EL MAZO ", "IN DECK " }, { "NIVEL ", "LEVEL " },
		{ "MAZO", "DECK" }, { "MONSTRUOS", "MONSTERS" }, { "MAGIAS", "SPELLS" }, { "TRAMPAS", "TRAPS" }, { "MAZO VÁLIDO", "VALID DECK" },
		{ "El mazo está vacío: Triángulo vuelve a la colección", "The deck is empty: Triangle goes back to the collection" },
		{ "Ninguna carta con estos filtros (L3 los quita)", "No cards match these filters (L3 clears them)" },
		// barra de botones
		{ "Mover", "Move" }, { "Página", "Page" }, { "Agregar", "Add" }, { "Quitar", "Remove" },
		{ "Colección / Mi mazo", "Collection / My deck" }, { "Sin filtros", "Clear filters" }, { "Guardar y salir", "Save & exit" },
		// avisos
		{ "X agrega, Cuadrado quita, Círculo guarda y sale", "X adds, Square removes, Circle saves and exits" },
		{ "Mi mazo: Cuadrado quita una copia", "My deck: Square removes a copy" }, { "Colección", "Collection" },
		{ "No está en el mazo", "Not in the deck" },
		// motivos (mazo.h)
		{ "Carta desconocida", "Unknown card" }, { "Carta desconocida: ", "Unknown card: " },
		{ "Prohibida en el Formato Goat", "Forbidden in the Goat Format" },
		{ "Limitada: 1 copia", "Limited: 1 copy" }, { "Semilimitada: 2 copias", "Semi-limited: 2 copies" }, { "Máximo 3 copias", "Up to 3 copies" },
		{ "Mazo de fusión lleno (15)", "Fusion Deck full (15)" }, { "Mazo principal lleno (60)", "Main Deck full (60)" },
		{ "Faltan ", "Missing " }, { " cartas (mínimo 40)", " cards (minimum 40)" },
		{ "Sobran ", "Too many: " }, { " cartas (máximo 60)", " cards (maximum 60)" },
		{ "Mazo de fusión: máximo 15", "Fusion Deck: maximum 15" },
		{ "Una fusión en el mazo principal", "A Fusion Monster in the Main Deck" },
		{ "Una carta que no es fusión en el mazo de fusión", "A non-Fusion card in the Fusion Deck" },
		{ ": más copias de las permitidas", ": more copies than allowed" },
		// Ajustes
		{ "Construir mazo", "Deck builder" },
	};
	return t;
}

inline std::string T(const std::string& es)
{
	if (!enIngles()) return es;
	const auto& t = textosEn();
	auto it = t.find(es);
	if (it != t.end()) return it->second;
#if defined(CONSTRUCTOR_CON_IDIOMA)
	return idioma::tr(es);
#else
	return es;
#endif
}

}   // namespace constructor
