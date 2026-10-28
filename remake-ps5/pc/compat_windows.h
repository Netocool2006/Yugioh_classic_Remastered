/*
 * Yugioh Classic Remaster - lo POSIX que el juego usa y MinGW no trae igual (solo la version de Windows).
 *
 * pc/CMakeLists.txt lo incluye primero en cada archivo (-include): asi las cabeceras del sistema quedan a nivel de
 * archivo antes de que el juego las incluya dentro de su namespace (ps5/src/wrap.cpp.in) y el codigo compartido no
 * cambia por Windows:
 *  - mkdir(ruta, modo): MinGW solo tiene mkdir(ruta)
 *  - setenv: MinGW solo tiene _putenv_s
 *  - fsync: _commit
 *  - las macros de windows.h que chocan con nombres del juego o de la base (min, max, small)
 */
#pragma once

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __cplusplus
// toda la biblioteca estandar a nivel de archivo: el juego incluye algunas (<utility> en ia.h...) dentro de su
// namespace y en la consola llegaban antes por otro lado (prelude.h); con MinGW algunas no
#include <bits/stdc++.h>
#endif

#ifdef __cplusplus
// mkdir con permisos (los permisos no existen en Windows: se ignoran)
inline int mkdir(const char* ruta, int /*modo*/)
{
	return _mkdir(ruta);
}
inline int setenv(const char* nombre, const char* valor, int pisar)
{
	if (!pisar && getenv(nombre)) return 0;
	return _putenv_s(nombre, valor);
}
inline int fsync(int fd)
{
	return _commit(fd);
}
#endif

// windows.h (rpcndr.h) define "small" como char: platform.h tiene un parametro "small"
#ifdef small
#undef small
#endif
// wingdi.h: PLANES (GetDeviceCaps) es la tabla de planes de la IA (ia_plan.h)
#ifdef PLANES
#undef PLANES
#endif

#endif
