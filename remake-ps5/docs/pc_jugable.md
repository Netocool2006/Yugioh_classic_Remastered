# Yugioh Classic Remaster en Windows (versión jugable)

Rama `classic-pc-jugable`. Objetivo: el mismo juego de la PS5 (mismo motor, misma lógica de duelo, mismo código de
la IA: importante para el lockstep del modo en línea PS5 <-> PC) corriendo en una ventana de Windows con GPU.

## Lo que hay hoy (investigación)

| Parte | PS5 | Vista previa de PC (Linux, dentro de `arch-ps5`) |
|---|---|---|
| Gráficos | **Vulkan 1.3** (RADV enlazado en el título, a través de volk) sobre la base de Sascha Willems (`base/`, plantilla "PS5 Vulkan Template", MIT) | el mismo Vulkan con el driver del PC y una superficie *headless* (`PS5_HOST_REFERENCE`); las fotos/videos salen copiando la imagen del swapchain (`ps5CaptureSwapchainImage`) a PPM/JPG o a un ffmpeg (`PS5_RECORD`) |
| Plataforma | `ps5/src/platform.c` (klog, mando, sonido, hora) detrás de `ps5/src/platform.h` | `ps5/src/platform_host.c`: sin mando y sin sonido |
| Mando | `pad_player(0, &mando)` con los bits `PAD_*` de la consola | guiones (`YUGI_GUION`) y `keyPressed` (sin ventana no hay teclado) |
| Audio | mezclador propio (`audio.h`, stb_vorbis) sobre `audio_start` (48 kHz, estéreo, 16 bits, un hilo) | `audio_start` devuelve false: mudo |
| Archivos | assets en `/app0/assets/` (`getAssetPath()`), guardado en `/data/yugioh_classic/` | assets en la carpeta de compilación, guardado junto al juego |

La base de Sascha Willems **ya trae Windows** (ventana Win32, superficie `VK_KHR_win32_surface`, bucle de mensajes):
la plantilla de PS5 solo agregó ramas `VK_EXAMPLE_PS5` encima. No hace falta ninguna capa gráfica ni reescribir el
renderizado: el juego es Vulkan puro.

## Camino elegido (el más corto)

1. **Compilación cruzada en el contenedor** con `mingw-w64-gcc` (paquete de Arch, instalado solo dentro de
   `arch-ps5`; nada en C:). Un `CMakeLists.txt` propio (`remake-ps5/pc/`) compila las **mismas fuentes** que la PS5
   desde la plantilla sincronizada (`/work/PS5_ClassicRemaster`: `base/`, imgui, ktx, volk y
   `examples/yugireload/yugireload.cpp` dentro de su namespace como en la consola). Defines: `VK_EXAMPLE_PS5`
   (la parte "consola" de la base: el mando, `ps5.*`, `quit`) + `_WIN32` / `VK_USE_PLATFORM_WIN32_KHR` (la ventana).
   **No** se define `PS5_HOST_REFERENCE` (eso es la vista previa de pruebas): el .exe se porta como la consola.
   Enlace estático de libstdc++/winpthreads: un solo .exe sin DLL de MinGW. Vulkan llega por volk desde
   `vulkan-1.dll` (viene con el driver de la GPU).
2. **Plataforma de Windows** (`pc/plataforma_windows.c`) que implementa `platform.h` igual que `platform.c` en la
   consola: el juego no cambia una línea para el mando ni para el sonido.
3. **Arranque propio** (`pc/main_windows.cpp`, en lugar de `ps5/src/main.cpp`): lee `opciones_pc.txt` / la línea de
   comandos, crea la ventana con `setupWindow` de la base y corre el único programa (`ps5Samples[0]`).
4. **Parches mínimos a la base** (en `parche_base.py`, todos dentro de `#if defined(_WIN32)`: en la PS5 no compilan):
   el bucle Win32 llama a `ps5HandleInput()` como el de la consola y respeta `quit`; las teclas ya no las procesa la
   base (Esc salía del juego, P pausaba): las lee la plataforma como botones del mando; la ventana no se redimensiona
   arrastrando (resolución fija elegida al arrancar) y el título es el nombre del juego.
5. **Cambios en el código compartido** (mínimos, detrás de `#if`): la carpeta de guardado sale de una sola función
   (`guardado::carpetaDatos()`: `/data/yugioh_classic` en la PS5, `%APPDATA%\YugiohClassic` en Windows) y el guardado
   atómico usa `MoveFileEx` en Windows (`rename` no pisa un archivo existente allí).

### Video

| Opción | Cómo |
|---|---|
| Ventana | por defecto 1920x1080 (o el 90 % del escritorio si es más chico); `--ancho N --alto N` |
| Pantalla completa | `--completa` (sin bordes, a la resolución del escritorio: 4K si el monitor es 4K); `--ventana` vuelve |
| V-sync | por defecto **activado** (FIFO: 60 Hz, 120 Hz, 144 Hz... lo que dé el monitor); `--sin-vsync` usa MAILBOX/IMMEDIATE |
| 60/120 Hz | con V-sync el juego va a la frecuencia del monitor; el reloj del juego (`frameTimer`) mide cada cuadro como en la PS5, así que la velocidad no depende de los Hz |

Las mismas opciones se pueden dejar escritas en `opciones_pc.txt` (junto al .exe), una por línea.

### Mando

- **XInput** (mandos de Xbox y todo lo que se presente como tal) cargando `xinput1_4.dll` / `xinput9_1_0.dll` en
  tiempo de ejecución: sin dependencia al compilar.
- **DualSense / DualShock 4 / Switch**: con **SDL3** (licencia zlib, compatible) si `SDL3.dll` está junto al .exe.
  Se carga con `LoadLibrary` (sin cabeceras ni .lib al compilar: el ABI de las ~12 funciones usadas es estable);
  `armar_pc.sh` baja la DLL oficial (versión fija, verificada con SHA-256) y su licencia. Con SDL presente, SDL
  maneja también los de Xbox (no se leen dos veces). Sin SDL: solo XInput + teclado.
- Mapeo a los mismos `PAD_*`: A/✕ = `PAD_CROSS`, B/○ = `PAD_CIRCLE`, Y/△ = `PAD_TRIANGLE`, X/□ = `PAD_SQUARE`,
  LB/RB = L1/R1, gatillos = L2/R2 (análogos y bit digital), Start/Options = `PAD_OPTIONS`, Back/Create (o el panel
  táctil con SDL) = `PAD_TOUCH_PAD`, sticks con zona muerta.
- **Teclado** (respaldo, solo con la ventana al frente): flechas = cruceta, **Enter**/Espacio = ✕, **Esc**/Retroceso
  = ○, T = △, F = □, Q/E = L1/R1, 1/3 = L2/R2, Z/C = L3/R3, **Tab**/O = OPTIONS, WASD = stick izquierdo,
  IJKL = stick derecho. La ventana se cierra con la X o Alt+F4 (Esc ya no cierra el juego).
- Íconos: se siguen dibujando los de PlayStation (`interfaz.h: iconoBoton`). Pendiente si se quiere: elegir íconos
  de Xbox cuando el último mando usado fue XInput (habría que pasar un "tipo de mando" de `platform.h` a la interfaz).

### Audio

El **mismo mezclador** (`audio.h`): la plataforma de Windows ofrece `audio_start` con un hilo que llena bloques de 256
cuadros a 48 kHz estéreo 16 bits y los entrega a **waveOut** (winmm, viene con Windows) con 6 búferes (~32 ms). Si el
dispositivo no acepta 48 kHz, Windows convierte.

### Datos

- **Assets**: `datos/assets/` y `datos/shaders/` junto al .exe (`vks::tools::resourcePath`). El .exe los busca ahí.
- **Guardado**: `%APPDATA%\YugiohClassic\` (mazos, progreso, opciones, idioma; sobrevive a reemplazar la carpeta del
  juego). Si existe `portable.txt` junto al .exe, se guarda en `guardado\` junto al .exe. Si no se puede crear
  ninguna, el juego avisa en pantalla como en la consola.
- **Registro**: `yugioh_classic.log` en la carpeta de guardado (lo que en la consola va a klog).
- `remake-ps5/armar_pc.sh` (dentro de `arch-ps5`) compila el .exe con el candado de compilación y arma
  `D:\Docker\compartido\pc\YugiohClassic\` desde el título de PS5 (`/compartido/ps5/PPSA77002`): los assets van como
  **enlaces duros** (NTFS por el montaje de Docker): no ocupan espacio de nuevo; si un archivo del título cambia, el
  siguiente `armar_pc.sh` lo actualiza. Los shaders salen de la plantilla recién sincronizada (los de esta rama).

## Lo que no se toca

- Nada del motor, del duelo ni de la IA: el TU `yugireload.cpp` es el mismo; el lockstep en red ve el mismo código.
- La PS5 sigue compilando igual (`probar_rama.sh`); los cambios de la base solo existen con `_WIN32`.
- Nada de assets de Konami en el repo: `armar_pc.sh` los toma del título ya armado fuera del repo.

## Pendiente / ideas

- Íconos de Xbox según el mando (ver arriba).
- Cambiar de ventana a pantalla completa en caliente (Alt+Enter): hoy se elige al arrancar (el juego no espera que
  cambie la resolución a mitad).
- Ícono del .exe (el de `sce_sys/icon0.png` no va al repo; se podría convertir en `armar_pc.sh`).
- La red (rama `classic-red-base`) ya trae Winsock en `src/red/socket_udp.h`: al unirse, enlazar `-lws2_32`
  (ya está en `pc/CMakeLists.txt`).

## Verificación (2026-10-21)

- `YugiohClassic.exe` (6 MB, un solo archivo) arranca en Windows 11 con la GPU del PC: ventana 1600x900 con V-sync,
  portada → menú principal → MODO DUELO → Téa Gardner (diálogo) → moneda → "Ir primero" → mesa del duelo con la
  mano, el tablero, el fondo y la UI; al pasar de fase llega el turno del rival ("El rival está pensando...").
- Sonido: `waveOut` abierto, 57 efectos y 9 pistas cargados (mismo `audio.h`).
- Guardado: mazos, progreso, idioma, historia y estadísticas en `%APPDATA%\YugiohClassic`; el registro también.
- Pruebas sin manos: `YUGI_GUION_PC="3:X 7:X 11:C ..."` (segundos desde el primer cuadro : botón X/O/T/C/ARRIBA/
  ABAJO/IZQ/DER/L1/R1/L3/R3/OPTIONS/TOUCH) aprieta botones dentro del juego sin mandar teclas al escritorio; las fotos
  de la ventana salen con `PrintWindow` (PW_RENDERFULLCONTENT) aunque esté tapada.
- Falta probar a mano: un DualSense/Xbox reales (SDL3/XInput), pantalla completa 4K y 120 Hz en un monitor que los dé.

## Fase 2: raton, iconos por dispositivo, pantalla completa en caliente, icono y sin juego en linea

### Raton (clic para mover y confirmar; sin arrastrar)

Todo vive en codigo portable y solo lo lee Windows:

- `src/raton.h`: la **zona** (rectangulo en px de pantalla + hasta dos **ejes** "estoy en `cur`, la zona esta en `pos`, `mas` / `menos` son los botones que
  suben / bajan"), el registro por cuadro y el `Puntero`. Una lista vertical es un eje con ARRIBA / ABAJO; una rejilla, dos; la mesa del duelo lleva en
  cambio el foco directo (`fTipo` / `fI`).
- La interfaz (`interfaz.h` y los `*_ui.h`) **registra las zonas al dibujar** (`zLista`, `zBoton`, `zFoco`, `zRect`): son una linea junto a cada fila, boton o
  carta. Las pantallas que tapan (portada, menu, pausa, elegir rival, resultado, constructor, filtros, teclado, pregunta, selector de fases, selector de
  cartas, detalle, consulta, galeria, Personalizacion, dialogos, tutorial, moneda) llaman a `rz.tapar()`: lo registrado debajo desaparece, y la mesa del duelo
  (mano, 18 zonas del tablero, engranaje, placa de fase) solo se registra si nada la tapa. Gana la zona dibujada **mas arriba**.
  Cada icono de boton de la ayuda (`iconoBoton`) es ademas una zona: clic = apretar ese boton.
- `src/raton_juego.h` (`raton::Juego`): del puntero a los **mismos pulsos del mando**. Mover el puntero sobre una zona lleva el foco hasta ella (un pulso de
  la cruceta cada 55 ms; en la mesa, el foco directo si el estado lo permite); clic izquierdo = llegar y apretar el boton de la zona (X casi siempre; en filas con
  valor, mitad izquierda = IZQ y derecha = DER); clic derecho = circulo; clic central = triangulo; rueda = arriba / abajo en menus, listas y detalle, y
  L1 / R1 (camara) en la mesa. Si un eje no avanza (nivel bloqueado, opcion apagada) se abandona tras 3 pulsos. Si despues el teclado o el mando mueven el foco,
  el puntero no lo vuelve a tirar hasta que se mueva.
- Windows: `pc/raton_plataforma.h` / `plataforma_windows.c` (`raton_mensaje`, `raton_tomar`); `main_windows.cpp` manda los mensajes de la ventana (el raton ya no
  llega a la camara de la base). El puntero se **oculta 3 s despues de mover el raton si luego se uso mando o teclado** y reaparece al moverlo.

### Iconos de la ayuda segun el ultimo dispositivo
`Interfaz::dispositivo` (0 PlayStation, 1 teclado y raton, 2 Xbox) lo pone la plataforma: el mando manda sobre el teclado, y un mando de SDL cuyo nombre es
DualSense / DualShock / PS4 / PS5 / Wireless Controller / Sony es PlayStation; XInput es Xbox. Teclado: Enter, Esc, T, F, Flechas, IJKL, Q, E, Tab, P, Z, C.
Xbox: A, B, Y, X en su color, LB, RB, LT, RT, MENU, VIEW, LS, RS. El arranque usa teclado y raton.

### Pantalla completa en caliente, icono, sin juego en linea
- **Alt+Enter** o **F11**: sin bordes al tamano del monitor y de vuelta (la base recrea la cadena de imagenes con WM_SIZE; el juego usa width / height de cada cuadro).
- **Icono del .exe**: `armar_pc.sh` convierte `sce_sys/icon0.png` del titulo (fuera del repo) a `yugioh.ico` (16..256) con PIL y `windres` lo compila (`pc/yugioh.rc`,
  `-DYUGI_ICONO_DIR`). Sin PIL o sin el PNG, el .exe sale sin icono propio.
- **En linea oculto en PC** (solo `_WIN32`; la PS5 no cambia): el boton de la puerta dice "DUELO EN LINEA: NO DISPONIBLE EN PC" y suena el error; en Opciones, "Aviso de
  privacidad (en linea)" dice "No disponible en PC".

### Verificacion de la fase 2 (2026-10-27, ventana de 1280x720, entrada REAL del raton con SetCursorPos + mouse_event)
- Compila con mingw (`armar_pc.sh`, salida `D:\Docker\compartido\pc\YugiohClassic_dev`, con `portable.txt` para no tocar el guardado real) y la vista previa
  Linux del mismo codigo (`ninja ps5-samples-host`) tambien. El registro `[raton] (x,y) ... botones 0x.. zona [rect] ejes (pos/cur) foco ..` muestra cada pulso.
- Probado con raton real: portada (clic = X), menu principal (hover lleva el foco, clic elige), Opciones (clic a cada lado del valor, clic derecho vuelve), elegir rival
  libre (clic en el nombre = duelo; orbe de nivel bloqueado se abandona tras 3 pulsos), modo historia (orbes, icono T = duelo libre), dialogo y moneda (clic = X),
  mesa del duelo (hover en la mano y en el tablero, clic = menu de acciones, clic en "Colocar", clic en una zona valida = la carta se coloca: `colocar_mt ok`),
  placa FASES y selector de fases (hover + clic derecho cierra), engranaje -> pausa (filas, Opciones desde la pausa, Continuar), detalle (clic central, rueda
  desplaza, clic cierra), rueda en la mesa = L1 (camara), constructor (lista de mazos, acciones, editor: hover y salto entre coleccion y mazo, filtros por mitades),
  Alt+Enter (1280x720 <-> 1920x1080 en caliente, sin reiniciar), icono del .exe (extraido: arte del juego), iconos de teclado y, con `YUGI_DISPOSITIVO=0|1|2`, de
  Xbox / PlayStation (el texto de la portada tambien: "Haz clic o pulsa Enter" / "Pulsa A").
- Sin verificar: un mando real (XInput / DualSense: el cambio automatico de iconos y que el mando manda sobre el teclado), el puntero que se oculta a los 3 s
  (necesita teclado real; la logica es `puntero_oculto()` en `plataforma_windows.c`), clic en las zonas de las pantallas Personalizacion (fundas, tapetes), Galeria 3D, teclado en
  pantalla, tutorial ("Reglas") y resultado (solo se compilan y se leyeron sus coordenadas), la rueda en las listas largas del constructor (hay zonas por fila, no por
  pagina), pantalla completa en un monitor 4K / 120 Hz, y el juego en otra pantalla / DPI distinto. En un cuadro con el puntero sobre dos capas la ultima dibujada gana.
- Limitaciones: los clics en el texto de las etiquetas de la ayuda no hacen nada (solo el icono del boton); las zonas del tablero son la caja de las cuatro
  esquinas de cada zona (con la perspectiva se solapan un poco en el fondo); una lista con una fila apagada no se alcanza (se abandona tras 3 pulsos).
- Ojo con las pruebas: la entrada sintetica con PostMessage no sirve del todo (Windows manda WM_MOUSELEAVE enseguida si el cursor real no esta encima); usar
  SetCursorPos / mouse_event con la ventana arriba, y no mientras el usuario usa el PC (le mueve el cursor).
