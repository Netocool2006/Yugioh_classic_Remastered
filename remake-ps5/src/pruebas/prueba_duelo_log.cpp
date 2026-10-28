// Prueba de la bitacora del duelo (src/duelo_log.h, rama classic-ps5-duelo-log), sin pantalla: juega duelos completos
// IA contra IA (el lado 1 es "el CPU" con plan y, en uno de ellos, la IA humana que imagina copias del duelo) y comprueba
//   1) el archivo existe, tiene la cabecera y contiene robos, invocaciones, ataques, decisiones del CPU y el fin con motivo;
//   2) con "Bitacora del duelo = No" no se escribe nada (ni se crea el archivo);
//   3) la bitacora NO cambia el juego: el mismo duelo con y sin bitacora termina igual, con los mismos sucesos;
//   4) la rotacion (mas de 2 MB -> duelo_log.anterior.txt) y que sin carpeta escribible no pasa nada;
//   5) los ganchos de ia.h quedan apagados al terminar.
// Compilar y correr (contenedor arch-ps5; segundos):
//   g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_duelo_log.cpp -o /tmp/prueba_duelo_log &&
//   /tmp/prueba_duelo_log <carpeta ui/ con cartas_ui.txt> remake-ps5/datos/rivales.txt <carpeta temporal> [muestra.txt]
#include "ia.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>

using namespace ia;

static int fallos = 0;
#define FALLO(...) do { fprintf(stderr, "FALLO: " __VA_ARGS__); fprintf(stderr, "\n"); fallos++; } while (0)

static std::string leer(const std::string& ruta)
{
	std::ifstream f(ruta, std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}
static bool existe(const std::string& ruta)
{
	struct stat st;
	return stat(ruta.c_str(), &st) == 0;
}
static long tam(const std::string& ruta)
{
	struct stat st;
	return stat(ruta.c_str(), &st) == 0 ? (long)st.st_size : -1;
}
static int cuenta(const std::string& t, const std::string& x)
{
	int n = 0;
	for (size_t p = t.find(x); p != std::string::npos; p = t.find(x, p + x.size())) n++;
	return n;
}

struct Resultado {
	int ganador{ -2 }, turno{ 0 }, lp0{ 0 }, lp1{ 0 };
	size_t sucesos{ 0 };
	uint64_t firma{ 1469598103934665603ull };   // FNV de los sucesos (tipo, lado, zonas, valor, carta)
	bool operator==(const Resultado& o) const { return ganador == o.ganador && turno == o.turno && lp0 == o.lp0 && lp1 == o.lp1 && sucesos == o.sucesos && firma == o.firma; }
};

// un duelo completo con el CPU (lado 1) contra el "jugador" (lado 0, tambien IA); bitacora: se anota con duelo_log
static Resultado jugar(const Base& b, const Rival& rj, const Rival& rc, uint32_t s, bool humana, bool conLog, const std::string& modo)
{
	const MazoRival mj = b.mazoDe(rj, s), mc = b.mazoDe(rc, s ^ 0x55u);
	Duelo d(&b);
	d.empezar(mj.principal, mj.fusion, mc.principal, mc.fusion, s);
	Turno tj(&d, 0, rj.personalidad(), rj.nivel, s), tc(&d, 1, rc.personalidad(), rc.nivel, s + 1);
	tc.cerebro.usarPlan(mc.plan, Lectura{}, 0, 1);
	tj.usarHumana(false, rj.clave, s);
	tc.usarHumana(humana, rc.clave, s + 1);
	if (conLog) {
		duelo_log::Cabecera cab;
		cab.rival = rc.nombreEs;
		cab.personalidad = rc.clavePersonalidad;
		cab.nivel = rc.nivel;
		cab.plan = mc.plan >= 0 ? PLANES[mc.plan].clave : "";
		cab.iaHumana = humana;
		cab.modo = modo;
		duelo_log::empezar(d, cab);
	}
	int pasos = 0;
	while (d.ganador < 0 && pasos < 20000 && d.turno < 300) {
		(d.decide() == 0 ? tj : tc).paso();
		pasos++;
	}
	if (conLog) duelo_log::terminar();
	Resultado r;
	r.ganador = d.ganador, r.turno = d.turno, r.lp0 = d.l[0].lp, r.lp1 = d.l[1].lp, r.sucesos = d.sucesos.size();
	for (const Suceso& su : d.sucesos) {
		const std::string k = std::to_string((int)su.tipo) + "," + std::to_string(su.lado) + "," + std::to_string(su.zona) + "," + std::to_string(su.zona2) + "," +
		                      std::to_string(su.valor) + "," + su.carta + ";";
		for (unsigned char ch : k) r.firma = (r.firma ^ ch) * 1099511628211ull;
	}
	return r;
}

int main(int argc, char** argv)
{
	if (argc < 4) {
		fprintf(stderr, "uso: prueba_duelo_log <carpeta ui/> <rivales.txt> <carpeta temporal> [muestra.txt]\n");
		return 2;
	}
	const std::string ui = argv[1], tmp = argv[3];
	Base base;
	if (!base.leerCartas(ui) || !base.leerRivales(argv[2])) return 1;
	duelo_log::est().carpeta = tmp;
	duelo_log::cargarNombres(ui + "cartas_ui.txt");
	if (duelo_log::est().nombres.empty()) FALLO("no se cargaron los nombres de cartas_ui.txt");
	const std::string ruta = duelo_log::rutaLog(), anterior = tmp + "/duelo_log.anterior.txt";
	remove(ruta.c_str());
	remove(anterior.c_str());
	const Rival& rj = base.rivales[0];
	const Rival& rc = base.rivales[base.rivales.size() / 2];

	// 2) con la opcion en No: nada se escribe
	opciones::bitacora() = false;
	const Resultado sin = jugar(base, rj, rc, 777u, false, true, "prueba");
	if (existe(ruta)) FALLO("con la bitacora en No se creo %s", ruta.c_str());
	if (ia::ganchosLog.suceso || ia::ganchosLog.jugada || ia::ganchosLog.candidatas || ia::ganchosLog.decision) FALLO("con la bitacora en No quedaron ganchos instalados");

	// 1) y 3) con la opcion en Si: el mismo duelo, el mismo resultado, y el archivo
	opciones::bitacora() = true;
	const Resultado con = jugar(base, rj, rc, 777u, false, true, "prueba");
	if (!(sin == con))
		FALLO("la bitacora cambio el duelo: sin log gana %d turno %d LP %d/%d (%zu sucesos), con log gana %d turno %d LP %d/%d (%zu sucesos)", sin.ganador, sin.turno, sin.lp0, sin.lp1,
		      sin.sucesos, con.ganador, con.turno, con.lp0, con.lp1, con.sucesos);
	if (ia::ganchosLog.suceso || ia::ganchosLog.jugada || ia::ganchosLog.candidatas || ia::ganchosLog.decision) FALLO("al terminar quedaron ganchos instalados");
	if (!existe(ruta)) {
		FALLO("no existe %s", ruta.c_str());
		return 1;
	}
	std::string t = leer(ruta);
	auto debe = [&](const char* x, int minimo) {
		const int n = cuenta(t, x);
		if (n < minimo) FALLO("el log tiene %d veces '%s' (se esperaban %d o mas)", n, x, minimo);
		return n;
	};
	debe("DUELO NUEVO", 1);
	debe("Mazo J (", 1);
	debe("Mazo CPU (", 1);
	debe("Mano inicial J:", 1);
	const int robos = debe(" roba ", 6);
	const int invocaciones = debe("Invocacion", 2);
	const int ataques = debe("ATAQUE: ", 1);
	const int decisiones = debe("[CPU decide]", 5);
	debe("[CPU plan]", 1);
	debe(" CPU] ", 10);
	debe(" J] ", 10);
	debe("===== Turno ", 4);
	debe("Fase: Batalla", 1);
	debe("FIN DEL DUELO", 1);
	debe("motivo:", 1);
	debe("DESTRUIDA EN BATALLA", 1);
	debe("(LP ", 1);
	if (cuenta(t, "decide] ataca con") + cuenta(t, "decide] NO ataca") < 1) FALLO("el log no explica ni un ataque ni un 'no ataca' del CPU");
	if (cuenta(t, "RECHAZADO") + cuenta(t, "ACEPTADO") < 1) FALLO("el log no tiene ACEPTADO/RECHAZADO");
	// cada linea de suceso empieza con "mm:ss [T<n> <fase> J|CPU]"
	int malas = 0, lineas = 0;
	{
		std::istringstream is(t);
		std::string l;
		while (std::getline(is, l)) {
			if (l.size() < 14 || l[2] != ':' || l[5] != ' ' || l[6] != '[' || l[7] != 'T') continue;   // (cabecera y marcos)
			lineas++;
			const size_t c = l.find("] ");
			const std::string pre = c == std::string::npos ? std::string() : l.substr(0, c);
			const bool finJ = pre.size() > 2 && pre.compare(pre.size() - 2, 2, " J") == 0, finCpu = pre.size() > 4 && pre.compare(pre.size() - 4, 4, " CPU") == 0;
			if (!finJ && !finCpu) malas++;
		}
	}
	if (malas) FALLO("%d lineas con el prefijo mal formado", malas);
	if (lineas < 50) FALLO("el log tiene solo %d lineas de sucesos", lineas);
	// un nombre en espanol de verdad (no el id)
	if (cuenta(t, "Dragon") + cuenta(t, "Dragón") + cuenta(t, "Guerrero") + cuenta(t, "Mago") + cuenta(t, "Bestia") + cuenta(t, "Guardi") == 0) FALLO("parece que los nombres no estan en espanol");
	const long tamUno = tam(ruta);
	const std::string muestra = t;

	// con la IA humana (imagina copias del duelo: no deben salir en la bitacora) -> se agrega al mismo archivo (append)
	const Resultado hum = jugar(base, rj, rc, 4242u, true, true, "prueba IA humana");
	(void)hum;
	t = leer(ruta);
	if (cuenta(t, "DUELO NUEVO") != 2) FALLO("el segundo duelo no se agrego (append): %d cabeceras", cuenta(t, "DUELO NUEVO"));
	if (tam(ruta) <= tamUno) FALLO("el segundo duelo no agrego lineas");
	// ninguna copia de la IA humana debe contaminar: los turnos del log suben de 1 en 1 en cada duelo
	{
		std::istringstream is(t);
		std::string l;
		int ultimo = 0, saltos = 0;
		while (std::getline(is, l)) {
			if (l.rfind("DUELO NUEVO", 0) == 0) ultimo = 0;
			const size_t p = l.find("===== Turno ");
			if (p == std::string::npos) continue;
			const int n = atoi(l.c_str() + p + 12);
			if (n != ultimo + 1) saltos++;
			ultimo = n;
		}
		if (saltos) FALLO("los turnos del log no suben de uno en uno (%d saltos): se colaron copias de la IA", saltos);
	}

	// en No otra vez: el archivo no cambia
	const long antes = tam(ruta);
	opciones::bitacora() = false;
	jugar(base, rj, rc, 99u, false, true, "prueba");
	if (tam(ruta) != antes) FALLO("con la bitacora en No el archivo cambio de tamano");
	opciones::bitacora() = true;

	// 4) rotacion: mas de 2 MB -> duelo_log.anterior.txt y un archivo nuevo
	{
		std::ofstream g(ruta, std::ios::binary | std::ios::trunc);
		const std::string bloque(1024, 'x');
		for (int k = 0; k < 2100; k++) g << bloque << "\n";
	}
	jugar(base, rj, rc, 31337u, false, true, "prueba");
	if (!existe(anterior) || tam(anterior) < 2L * 1024L * 1024L) FALLO("la rotacion no dejo duelo_log.anterior.txt");
	if (tam(ruta) <= 0 || tam(ruta) > 1024L * 1024L) FALLO("despues de rotar el log nuevo tiene %ld bytes", tam(ruta));
	if (cuenta(leer(ruta), "xxxxxxxxxx") != 0) FALLO("el log nuevo conserva lo viejo");

	// una carpeta que no existe: no revienta
	duelo_log::est().carpeta = tmp + "/no_existe_esta_carpeta";
	const Resultado sinCarpeta = jugar(base, rj, rc, 777u, false, true, "prueba");
	if (!(sinCarpeta == sin)) FALLO("sin carpeta escribible el duelo cambio");

	printf("bitacora: %d lineas de sucesos, %d robos, %d invocaciones, %d ataques, %d decisiones del CPU, %ld bytes (primer duelo: %d turnos, gana %d)\n", lineas, robos, invocaciones,
	       ataques, decisiones, tamUno, con.turno, con.ganador);
	if (argc > 4) {   // una muestra para el informe
		std::ofstream m(argv[4], std::ios::binary);
		m << muestra;
	}
	printf(fallos ? "PRUEBA CON FALLOS (%d)\n" : "PRUEBA OK (%d fallos)\n", fallos);
	return fallos ? 1 : 0;
}
