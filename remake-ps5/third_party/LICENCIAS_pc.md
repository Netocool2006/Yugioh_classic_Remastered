# Licencias de la version de Windows (remake-ps5/pc, docs/pc_jugable.md)

| Que | Para que | Licencia | Como llega |
|---|---|---|---|
| SDL3 3.2.10 (`SDL3.dll`, libsdl.org) | DualSense / DualShock 4 / Switch (y Xbox) en PC | zlib (compatible; permite redistribuir la DLL) | `armar_pc.sh` baja la DLL oficial (`SDL3-3.2.10-win32-x64.zip`, SHA-256 en `pc/SDL3.sha256`) y la deja junto al .exe con `licencias/SDL3_LICENSE.txt`. No va al repo; el juego la carga con `LoadLibrary` (sin cabeceras ni .lib al compilar) y sin ella usa XInput |
| mingw-w64 (GCC 16, libstdc++, libgcc, winpthreads) | compilador cruzado (en el contenedor `arch-ps5`) y el runtime enlazado estatico | GPL-3.0 con *GCC Runtime Library Exception* (libstdc++/libgcc: se puede distribuir el .exe sin condiciones); winpthreads: MIT/BSD | `pacman -S mingw-w64-gcc` dentro del contenedor |
| XInput, waveOut (winmm), Vulkan loader (`vulkan-1.dll`) | mando de Xbox, sonido, GPU | parte de Windows / del driver de la GPU | del sistema |

El resto del codigo (base de Sascha Willems, imgui, ktx, volk, stb) es el mismo de la PS5: ver `ps5/licenses` del titulo.
