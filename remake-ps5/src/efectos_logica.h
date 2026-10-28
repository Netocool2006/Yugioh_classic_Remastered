// Yugioh Classic Remaster - la LOGICA de las animaciones de activacion de magias y trampas (rama classic-probador):
// que receta toca a cada carta, la cola de animaciones activas y su avance hasta terminar. Sin Vulkan ni glm (lo
// que se dibuja esta en efectos_cartas.h, efc::Motor, que hereda de esta): asi el probador de PC
// (pruebas/prueba_certificacion.cpp) verifica que cada activacion del duelo encola su animacion y que termina.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace efc {

enum class Receta : int {
	Magia = 0,        // activacion generica de magia
	Trampa,           // activacion generica de trampa
	AgujeroNegro,     // Dark Hole (Master Duel ef04342: remolino galactico) + destruccion de todos los monstruos
	Raigeki,          // rayos sobre los monstruos del rival + destruccion
	FuerzaEspejo,     // Mirror Force: barrera + destruccion de los monstruos del rival
	Renacer,          // Monster Reborn: circulo de invocacion + materializacion
	Olla,             // Pot of Greed: destellos dorados sobre la carta y el mazo
	Espadas,          // Swords of Revealing Light: marcas doradas sobre los monstruos del rival
	Polimerizacion,   // fusion de Master Duel (fxp_somldg_fusion)
	CambioCorazon,    // Change of Heart: aura sobre un monstruo del rival
	TormentaPesada,   // Heavy Storm: tornados sobre todas las magias/trampas
	Tifon,            // Mystical Space Typhoon (Master Duel ef04909: tornado) sobre una magia/trampa rival
	CilindroMagico,   // Magic Cylinder
	Plumero,          // Harpie's Feather Duster (Master Duel ef04678: plumas doradas)
	MilCuchillos,     // Thousand Knives (Master Duel ef05166)
	Ritual,           // rituales (fxp_somldg_ritual)
	Dado,             // tirada de dado 3D (Graceful Dice, Skull Dice...)
	Moneda,           // lanzamiento de moneda 3D (Fairy Box, Second Coin Toss...)
	NUM
};

inline const char* nombre(Receta r)
{
	static const char* const N[] = { "magia", "trampa", "agujero_negro", "raigeki", "fuerza_espejo", "renacer", "olla", "espadas",
		                             "polimerizacion", "cambio_corazon", "tormenta_pesada", "tifon", "cilindro_magico", "plumero",
		                             "mil_cuchillos", "ritual", "dado", "moneda" };
	return N[(int)r];
}

// cuanto dura cada animacion (s)
inline float duracion(Receta r)
{
	switch (r) {
	case Receta::AgujeroNegro: return 3.2f;
	case Receta::Raigeki: return 1.8f;          // (rama classic-ps5-efectos: Ef04343 de MD, 1.783 s)
	case Receta::FuerzaEspejo: return 1.5f;     // (Ef04887, 1.5 s)
	case Receta::Renacer: return 2.5f;          // (Ef04842, 2.5 s)
	case Receta::CilindroMagico: return 2.0f;   // (Ef05124, 2.0 s)
	case Receta::TormentaPesada: return 1.5f;
	case Receta::Dado: return 4.3f;
	case Receta::Moneda: return 4.6f;
	case Receta::Espadas: return 1.4f;   // (rama classic-ps5-efectos: Ef04354Timeline de MD dura 1.333 s, waitFinish)
	default: return 2.4f;
	}
}

// rama classic-ps5-efectos: el sonido de Master Duel de cada efecto (CardRunEffectSetting.seLabel, al correr el eslabon);
// "" si MD no tiene uno propio. Los WAV van en classic-assets/audio (herramientas/audio/seleccion.json)
inline const char* sonidoMD(Receta r)
{
	switch (r) {
	case Receta::Espadas: return "sfx_ev_gofuken";              // SE_EV_GOFUKEN (Ef04354)
	case Receta::AgujeroNegro: return "sfx_ev_blackhole";       // SE_EV_BLACKHOLE (Ef04342)
	case Receta::Raigeki: return "sfx_ev_raigeki";              // SE_EV_RAIGEKI (Ef04343)
	case Receta::FuerzaEspejo: return "sfx_ev_mirrorforce";     // SE_EV_MIRRORFORCE (Ef04887)
	case Receta::Renacer: return "sfx_ev_monster_reborn";       // SE_EV_MONSTER_REBORN (Ef04842)
	case Receta::CilindroMagico: return "sfx_ev_magic_cylinder";// SE_EV_MAGIC_CYLINDER (Ef05124)
	case Receta::Plumero: return "sfx_ev_harpie";               // SE_EV_HARPIESFEATHER_DUSTER_3D (Ef04678)
	case Receta::MilCuchillos: return "sfx_ev_thousandknives";  // SE_EV_THOUSANDKNIVES (Ef05166)
	case Receta::Tifon: return "sfx_ev_typhoon";                // SE_EV_EF04909 (Ef04909)
	default: return "";
	}
}

struct Activo {
	Receta receta{ Receta::Magia };
	std::string carta;
	int zona{ -1 };
	bool rival{ false };
	float t{ 0.0f };
	int resultado{ 0 };    // dado 1..6, moneda 0 cara / 1 cruz
	int objetivo{ -1 };    // zona elegida (Change of Heart, MST, Thousand Knives)
};

class Logica {
public:
	std::vector<Activo> activos;
	bool log{ false };
	int nTabla{ 0 };   // cartas con animacion propia (efectos_cartas.txt)

	// carpetaUi: efectos_cartas.txt y cartas_classic.txt
	void cargarTabla(const std::string& carpetaUi)
	{
		nTabla = 0;
		log = getenv("YUGI_FX_LOG") != nullptr;
		std::ifstream c(carpetaUi + "cartas_classic.txt");
		std::string l;
		while (std::getline(c, l)) {
			std::vector<std::string> k = partir(l);
			if (k.size() >= 4) {
				subtipo[k[0]] = k[3];
				porPasscode[k[1]] = k[0];
			}
		}
		std::ifstream t(carpetaUi + "efectos_cartas.txt");
		while (std::getline(t, l)) {
			if (l.empty() || l[0] == '#') continue;
			std::vector<std::string> k = partir(l);
			if (k.size() < 2) continue;
			std::string id = k[0];
			if (porPasscode.count(id)) id = porPasscode[id];
			for (int r = 0; r < (int)Receta::NUM; r++) {
				if (k[1] == nombre((Receta)r)) {
					tabla[id] = (Receta)r;
					nTabla++;
				}
			}
		}
	}

	Receta receta(const std::string& carta) const
	{
		auto it = tabla.find(carta);
		if (it != tabla.end()) return it->second;
		auto s = subtipo.find(carta);
		if (s != subtipo.end() && s->second.rfind("trampa", 0) == 0) return Receta::Trampa;
		if (s != subtipo.end() && s->second == "magia_ritual") return Receta::Ritual;
		return Receta::Magia;
	}
	// una magia/trampa se activo en la zona (de cualquier lado). El mismo pedido dos veces seguidas (el control y el
	// duelo avisan los dos) se ignora
	void activar(const std::string& carta, int zona, bool rival, uint32_t semilla)
	{
		for (const Activo& a : activos) {
			if (a.zona == zona && a.carta == carta && a.t < 0.8f) return;
		}
		Activo a;
		a.receta = receta(carta);
		a.carta = carta;
		a.zona = zona;
		a.rival = rival;
		semilla = semilla * 1664525u + 1013904223u;
		a.resultado = a.receta == Receta::Dado ? 1 + (int)((semilla >> 8) % 6u) : (int)((semilla >> 9) & 1u);
		activos.push_back(a);
		if (log) fprintf(stderr, "[efectos] %s (zona %d, %s): %s\n", carta.c_str(), zona, rival ? "rival" : "jugador", nombre(a.receta));
	}
	void avanzar(float dt)
	{
		for (Activo& a : activos) a.t += dt;
		std::vector<Activo> quedan;
		for (Activo& a : activos) {
			if (a.t < duracion(a.receta)) quedan.push_back(a);
		}
		activos.swap(quedan);
	}
	bool hay() const { return !activos.empty(); }

protected:
	std::unordered_map<std::string, Receta> tabla;
	std::unordered_map<std::string, std::string> subtipo, porPasscode;

	std::string subtipoDe(const std::string& carta) const
	{
		auto s = subtipo.find(carta);
		return s == subtipo.end() ? std::string() : s->second;
	}
	static std::vector<std::string> partir(const std::string& l)
	{
		std::vector<std::string> k;
		std::string x;
		std::istringstream s(l);
		while (std::getline(s, x, '|')) {
			while (!x.empty() && (x.back() == '\r' || x.back() == ' ')) x.pop_back();
			k.push_back(x);
		}
		return k;
	}
};

}   // namespace efc
