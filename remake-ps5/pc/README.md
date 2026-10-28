# pc/ - version de Windows

Estado (2026-10-27): jugable con teclado, raton y mando (Xbox / PlayStation). Ver `docs/pc_jugable.md` (incluye la fase 2: raton, iconos por dispositivo,
Alt+Enter, icono del .exe y el juego en linea oculto, y lo que quedo sin verificar).

- `main_windows.cpp`: arranque, ventana, Alt+Enter / F11, mensajes del raton.
- `plataforma_windows.c`: mando (XInput / SDL3), teclado, raton (`raton_plataforma.h`), audio waveOut, dispositivo de entrada.
- Pendiente: el instalador (Inno Setup, rama classic-pc-instalador).
