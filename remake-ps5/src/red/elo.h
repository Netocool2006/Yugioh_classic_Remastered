#pragma once
// ELO local del modo en linea (rama classic-sala) y el id anonimo del jugador. Se guarda en en_linea.txt con el
// guardado atomico (guardado.h): en la consola /data/yugioh_classic/en_linea.txt, en el PC YUGI_EN_LINEA=<ruta> o
// ./en_linea.txt. Formato "clave=valor": id (32 hex aleatorios, generado en la consola la primera vez: no identifica
// a nadie), elo, partidas, ganadas, perdidas.
// La sala de Cloudflare guarda tambien el ELO por id y manda: si responde, su valor pisa el local (aplicarSala).
// Sin sala (pareja por DHT) el ELO se calcula aca con la misma formula (K = 32).
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include "../guardado.h"
#include "sala_tipos.h"

namespace red {
namespace elo {

static const int INICIAL = 1000;
static const int FACTOR_K = 32;

inline double esperado(int a, int b) { return 1.0 / (1.0 + std::pow(10.0, (b - a) / 400.0)); }
// el ELO nuevo de 'a' tras jugar contra 'b' (redondeado como la sala: Math.round)
inline int nuevo(int a, int b, bool gano, int k = FACTOR_K)
{
	return (int)std::floor(a + k * ((gano ? 1.0 : 0.0) - esperado(a, b)) + 0.5);
}

struct Datos {
	std::string id;
	int elo = INICIAL;
	int partidas = 0, ganadas = 0, perdidas = 0;
};

// donde se guarda (como rutaMazo de mazo.h)
inline std::string ruta()
{
	if (const char* v = getenv("YUGI_EN_LINEA")) return v;   // (PC y pruebas)
#if defined(PS5_HOST_REFERENCE)
	return "en_linea.txt";
#else
	mkdir("/data/yugioh_classic", 0777);
	struct stat st{};
	if (stat("/data/yugioh_classic", &st) == 0 && S_ISDIR(st.st_mode)) return "/data/yugioh_classic/en_linea.txt";
	guardado::avisarSinCarpeta();
	return "en_linea.txt";
#endif
}

inline std::string aTexto(const Datos& d)
{
	std::string s = "# Yugioh Classic Remaster - modo en linea (id anonimo y ELO)\n";
	guardado::agregar(s, "id=%s\nelo=%d\npartidas=%d\nganadas=%d\nperdidas=%d\n", d.id.c_str(), d.elo, d.partidas, d.ganadas, d.perdidas);
	return s;
}
inline Datos deTexto(const std::string& texto)
{
	const Campos c = Campos::leer(texto);
	Datos d;
	d.id = c.obtener("id");
	d.elo = (int)c.numero("elo", INICIAL);
	d.partidas = (int)c.numero("partidas", 0);
	d.ganadas = (int)c.numero("ganadas", 0);
	d.perdidas = (int)c.numero("perdidas", 0);
	// un id que no sea hex de 32 se cambia por uno nuevo
	bool ok = d.id.size() == 32;
	for (char ch : d.id) if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) ok = false;
	if (!ok) d.id = idAleatorio();
	if (d.elo < 100 || d.elo > 4000) d.elo = INICIAL;
	return d;
}

// carga (o crea con un id nuevo y lo guarda) el archivo
inline Datos cargar(const std::string& archivo = ruta())
{
	std::ifstream f(archivo);
	std::stringstream ss;
	if (f) ss << f.rdbuf();
	const bool habia = (bool)f;
	Datos d = deTexto(ss.str());
	if (!habia || ss.str().find("id=" + d.id) == std::string::npos) guardado::guardarAtomico(archivo, aTexto(d));
	return d;
}
inline bool guardar(const Datos& d, const std::string& archivo = ruta()) { return guardado::guardarAtomico(archivo, aTexto(d)); }

// un duelo terminado contra un rival de ELO 'rival' (sin sala)
inline void anotar(Datos& d, int rival, bool gano)
{
	d.elo = nuevo(d.elo, rival, gano);
	d.partidas++;
	(gano ? d.ganadas : d.perdidas)++;
}
// la sala respondio el ELO oficial: manda sobre el local
inline void aplicarSala(Datos& d, int eloSala)
{
	if (eloSala >= 100 && eloSala <= 4000) d.elo = eloSala;
}

}  // namespace elo
}  // namespace red
