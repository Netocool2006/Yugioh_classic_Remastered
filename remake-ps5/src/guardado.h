/*
 * Yugioh Classic Remaster - guardado seguro (rama classic-pulido).
 *
 * Todo lo que el juego escribe en /data/yugioh_classic/ (mazos, progreso, historia, variedad, opciones, idioma) pasa
 * por guardarAtomico(ruta, contenido): se escribe entero en <ruta>.tmp, se hace fsync y recien entonces se renombra
 * sobre <ruta> (rename es atomico). Si la consola se apaga a mitad, queda el archivo viejo entero o el nuevo entero,
 * nunca uno vacio o cortado.
 *
 * Avisos al jugador: si /data/yugioh_classic no se pudo crear (se cae a la carpeta del juego, que puede ser de solo
 * lectura) o si falla una escritura, se anota aca (estado()) y la interfaz lo muestra en pantalla en ES/EN
 * (interfaz.h: dibujarAvisoGuardado). Este archivo no depende de nada mas del juego (lo usan tambien las pruebas).
 */
#pragma once

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(_WIN32)
#include <windows.h>   // MoveFileExA
#include <io.h>        // _commit (MinGW no tiene fsync)
#endif

namespace guardado {

// La carpeta escribible del juego (sin la barra final): /data/yugioh_classic en la consola; en Windows la que eligio
// pc/main_windows.cpp (%APPDATA%\YugiohClassic o guardado\ junto al .exe, en YUGI_DATOS_PC)
inline std::string carpetaDatos()
{
#if defined(_WIN32)
	std::string r;
	if (const char* v = getenv("YUGI_DATOS_PC")) r = v;
	while (!r.empty() && (r.back() == '/' || r.back() == '\\')) r.pop_back();
	return r.empty() ? std::string("guardado") : r;
#else
	return "/data/yugioh_classic";
#endif
}

struct Estado {
	bool sinCarpeta{ false };   // /data/yugioh_classic no se pudo crear
	int serie{ 0 };             // sube con cada aviso nuevo (la interfaz lo muestra unos segundos)
	bool ultimoFallo{ false };  // el ultimo aviso fue una escritura fallida (si no, la carpeta)
	std::string archivo;        // el ultimo archivo que no se pudo guardar (solo el nombre)
};
inline Estado& estado()
{
	static Estado e;
	return e;
}

// la carpeta de datos no se pudo crear: se avisa una sola vez
inline void avisarSinCarpeta()
{
	Estado& e = estado();
	if (e.sinCarpeta) return;
	e.sinCarpeta = true;
	e.ultimoFallo = false;
	e.serie++;
	fprintf(stderr, "[guardado] no se pudo crear /data/yugioh_classic: se guarda junto al juego (puede ser de solo lectura)\n");
}

inline void avisarFallo(const std::string& ruta, const char* paso)
{
	Estado& e = estado();
	const size_t barra = ruta.find_last_of('/');
	e.archivo = barra == std::string::npos ? ruta : ruta.substr(barra + 1);
	e.ultimoFallo = true;
	e.serie++;
	fprintf(stderr, "[guardado] no se pudo guardar %s (%s: %s)\n", ruta.c_str(), paso, strerror(errno));
}

// escribe "contenido" en "ruta" de forma atomica (temporal + fsync + rename); false (y aviso) si falla
inline bool guardarAtomico(const std::string& ruta, const std::string& contenido)
{
	if (ruta.empty()) return false;
	const std::string tmp = ruta + ".tmp";
	const int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		avisarFallo(ruta, "abrir");
		return false;
	}
	size_t hecho = 0;
	while (hecho < contenido.size()) {
		const ssize_t n = write(fd, contenido.data() + hecho, contenido.size() - hecho);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) {
			avisarFallo(ruta, "escribir");
			close(fd);
			unlink(tmp.c_str());
			return false;
		}
		hecho += (size_t)n;
	}
#if defined(_WIN32)
	if (_commit(fd) != 0) {   // (el fsync de Windows)
#else
	if (fsync(fd) != 0) {
#endif
		avisarFallo(ruta, "fsync");
		close(fd);
		unlink(tmp.c_str());
		return false;
	}
	if (close(fd) != 0) {
		avisarFallo(ruta, "cerrar");
		unlink(tmp.c_str());
		return false;
	}
#if defined(_WIN32)
	// en Windows rename no pisa un archivo que ya existe: MoveFileEx con reemplazo (tambien atomico en NTFS)
	if (!MoveFileExA(tmp.c_str(), ruta.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
	if (rename(tmp.c_str(), ruta.c_str()) != 0) {
#endif
		avisarFallo(ruta, "renombrar");
		unlink(tmp.c_str());
		return false;
	}
	// el renombre queda firme con el fsync de la carpeta (si el sistema lo permite; si no, no es un error)
	const size_t barra = ruta.find_last_of('/');
	const std::string dir = barra == std::string::npos ? std::string(".") : ruta.substr(0, barra == 0 ? 1 : barra);
#if !defined(_WIN32)   // (Windows no abre carpetas con open; MOVEFILE_WRITE_THROUGH ya lo deja firme)
	const int fdDir = open(dir.c_str(), O_RDONLY);
	if (fdDir >= 0) {
		fsync(fdDir);
		close(fdDir);
	}
#else
	(void)dir;
#endif
	return true;
}

// printf a un std::string (para armar el contenido antes de guardarlo)
inline void agregar(std::string& s, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
inline void agregar(std::string& s, const char* fmt, ...)
{
	char buf[512];
	va_list a;
	va_start(a, fmt);
	const int n = vsnprintf(buf, sizeof(buf), fmt, a);
	va_end(a);
	if (n < 0) return;
	if ((size_t)n < sizeof(buf)) {
		s.append(buf, (size_t)n);
		return;
	}
	std::string grande((size_t)n + 1, '\0');
	va_start(a, fmt);
	vsnprintf(&grande[0], grande.size(), fmt, a);
	va_end(a);
	s.append(grande.data(), (size_t)n);
}

}   // namespace guardado
