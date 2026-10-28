# Arreglos a la base de PS5 Vulkan Template que necesita Yugioh Classic Remaster (titulo PPSA77002) (los aplica sincronizar.sh en cada copia;
# cada uno se aplica una sola vez y falla en voz alta si la base cambio y ya no encaja).
# Uso: python3 parche_base.py <carpeta del titulo>
import sys
from pathlib import Path

t = Path(sys.argv[1])


def parchear(archivo, marca, viejo, nuevo):
    p = t / archivo
    s = p.read_text(encoding="utf-8")
    if marca in s:
        return
    if viejo not in s:
        sys.exit("parche_base: %s ya no tiene el texto esperado (%s...)" % (archivo, viejo[:60]))
    p.write_text(s.replace(viejo, nuevo, 1), encoding="utf-8")


# 1) vkglTF solo leia glTF de texto: los .glb (Blender, Meshy) se leen como binarios
parchear("base/VulkanglTFModel.cpp", "LoadBinaryFromFile",
         "bool fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filename);",
         "bool fileLoaded = filename.size() > 4 && filename.compare(filename.size() - 4, 4, \".glb\") == 0\n"
         "\t\t? gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, filename)\n"
         "\t\t: gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filename);")

# 2) JOINTS_0 en uint8 (Meshy): vkglTF los leia siempre como uint16 (indices basura -> el shader se salia del
#    arreglo de huesos)
parchear("base/VulkanglTFModel.cpp", "juntas16",
         '\t\t\t\tif (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {\n',
         "\t\t\t\tstd::vector<uint16_t> juntas16;   // JOINTS_0 en uint8 (Meshy) pasado a uint16\n"
         '\t\t\t\tif (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {\n')
parchear("base/VulkanglTFModel.cpp", "juntas16.assign",
         "\t\t\t\t\tbufferJoints = reinterpret_cast<const uint16_t *>(&(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));\n",
         "\t\t\t\t\tbufferJoints = reinterpret_cast<const uint16_t *>(&(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));\n"
         "\t\t\t\t\tif (jointAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {\n"
         "\t\t\t\t\t\tconst uint8_t *b8 = reinterpret_cast<const uint8_t *>(bufferJoints);\n"
         "\t\t\t\t\t\tjuntas16.assign(b8, b8 + jointAccessor.count * 4);\n"
         "\t\t\t\t\t\tbufferJoints = juntas16.data();\n"
         "\t\t\t\t\t}\n")

# 3) fotos de la consola a pedido: la base solo copiaba la pantalla en el ultimo cuadro de una corrida de prueba
#    (y terminaba); ps5.capturaPedida pide una copia en cualquier cuadro sin terminar el programa
parchear("base/vulkanexamplebase.h", "capturaPedida",
         "\t\tstd::string screenshotPath;\n",
         "\t\tstd::string screenshotPath;\n"
         "\t\t/** @brief Yugi Reload: copiar la pantalla de este cuadro a screenshotPath sin terminar el programa */\n"
         "\t\tbool capturaPedida = false;\n")
parchear("base/vulkanexamplebase.cpp", "capturaPedida",
         "|| ps5Recording()) {\n\t\tpresentWait = ps5CaptureSwapchainImage(presentWait);\n",
         "|| ps5Recording() || ps5.capturaPedida) {\n\t\tpresentWait = ps5CaptureSwapchainImage(presentWait);\n"
         "\t\tps5.capturaPedida = false;\n")
# 4) cuadros de video: si la ruta termina en .jpg, la imagen se comprime a JPG (stb_image_write, dominio
#    publico) en otro hilo, para no frenar el juego (unos 200 KB en vez de 6 MB por cuadro)
parchear("ps5/src/example_ps5.cpp", "stb_image_write.h",
         "#include ",
         "#include <thread>\n#define STB_IMAGE_WRITE_IMPLEMENTATION\n#include \"stb_image_write.h\"\n#include ")
parchear("ps5/src/example_ps5.cpp", "stbi_write_jpg(",
         "\tFILE *file = fopen(ps5.screenshotPath.c_str(), \"wb\");\n",
         "\tif (ps5.screenshotPath.size() > 4 && ps5.screenshotPath.compare(ps5.screenshotPath.size() - 4, 4, \".jpg\") == 0) {\n"
         "\t\t// Yugi Reload: cuadro de video en JPG, comprimido en otro hilo\n"
         "\t\tstd::thread([ruta = ps5.screenshotPath, w = outWidth, h = outHeight, datos = std::move(rgb)]() {\n"
         "\t\t\tstbi_write_jpg(ruta.c_str(), (int)w, (int)h, 3, datos.data(), 82);\n"
         "\t\t\tchmod(ruta.c_str(), 0666);\n"
         "\t\t}).detach();\n"
         "\t\treturn ps5CaptureComplete;\n"
         "\t}\n"
         "\tFILE *file = fopen(ps5.screenshotPath.c_str(), \"wb\");\n")
# 5) videos rapidos en el PC (solo la vista previa): YUGI_ANCHO/YUGI_ALTO cambian la resolucion (4K por defecto)
#    y YUGI_FPS el paso fijo de la corrida de prueba (60 por defecto)
parchear("base/vulkanexamplebase.cpp", "YUGI_ANCHO",
         "\twidth = 3840;\n\theight = 2160;\n",
         "\twidth = 3840;\n\theight = 2160;\n"
         "#if defined(PS5_HOST_REFERENCE)\n"
         "\tif (const char *a = getenv(\"YUGI_ANCHO\")) { width = (uint32_t)atoi(a); }\n"
         "\tif (const char *a = getenv(\"YUGI_ALTO\")) { height = (uint32_t)atoi(a); }\n"
         "#endif\n")
parchear("base/vulkanexamplebase.cpp", "YUGI_FPS",
         "\t\t\tframeTimer = 1.0f / 60.0f;\n",
         "\t\t\tframeTimer = 1.0f / 60.0f;\n"
         "#if defined(PS5_HOST_REFERENCE)\n"
         "\t\t\tif (const char *f = getenv(\"YUGI_FPS\")) { frameTimer = 1.0f / (float)atof(f); }\n"
         "#endif\n")
print("parche_base: listo")

# Ajustes (la ventana de pruebas) solo manteniendo OPTIONS 2 s; el panel tactil ya no la abre (pedido 2026-10-17:
# se abria sin querer al tocar el mando). Con optionsEnds (lanzador con varias muestras) sigue como antes.
parchear("ps5/src/example_ps5.cpp", "opcionesDesde",
         "\tif ((pad.pressed & (ps5.optionsEnds ? PAD_TOUCH_PAD : PAD_TOUCH_PAD | PAD_OPTIONS)) && !ps5.ownOverlay) {\n"
         "\t\tui.visible = !ui.visible;\n"
         "\t}\n",
         "\tstatic double opcionesDesde = -1.0;   // desde cuando se mantiene OPTIONS (1e300: ya abrio/cerro, hasta soltar)\n"
         "\tif (ps5.ownOverlay) {\n"
         "\t} else if (ps5.optionsEnds) {\n"
         "\t\tif (pad.pressed & PAD_TOUCH_PAD) {\n"
         "\t\t\tui.visible = !ui.visible;\n"
         "\t\t}\n"
         "\t} else if (pad.held & PAD_OPTIONS) {\n"
         "\t\tif (opcionesDesde < 0.0) {\n"
         "\t\t\topcionesDesde = now_seconds();\n"
         "\t\t} else if (now_seconds() - opcionesDesde >= 2.0) {\n"
         "\t\t\tui.visible = !ui.visible;\n"
         "\t\t\topcionesDesde = 1e300;\n"
         "\t\t}\n"
         "\t} else {\n"
         "\t\topcionesDesde = -1.0;\n"
         "\t}\n")

# La ventana de Ajustes se llamaba "Vulkan Example" (la plantilla; la de Yugi Reload, copiada, ya decia "Yugi Reload")
_base = (t / "base/vulkanexamplebase.cpp").read_text(encoding="utf-8")
parchear("base/vulkanexamplebase.cpp", 'ImGui::Begin("Yugioh Classic Remaster"',
         'ImGui::Begin("Yugi Reload",' if 'ImGui::Begin("Yugi Reload",' in _base else 'ImGui::Begin("Vulkan Example",',
         'ImGui::Begin("Yugioh Classic Remaster",')

# 7) modo en linea (ramas classic-red-*, classic-sala): cada muestra se compila dentro de su namespace (wrap.cpp.in) y un
#    encabezado del sistema que se incluye por primera vez ahi queda adentro (net/if.h: "struct timeval" incompleto).
#    Los de sockets se incluyen antes en prelude.h, a nivel de archivo, como pide la plantilla
parchear("ps5/src/prelude.h", "modo en linea",
         "#include <vector>\n",
         "#include <vector>\n"
         "// modo en linea (src/red/*.h de Yugioh Classic Remaster): sockets, interfaces y DNS\n"
         "#if !defined(_WIN32)\n"
         "#include <sys/types.h>\n#include <sys/time.h>\n#include <sys/socket.h>\n#include <netinet/in.h>\n#include <netinet/tcp.h>\n"
         "#include <arpa/inet.h>\n#include <ifaddrs.h>\n#include <net/if.h>\n#include <netdb.h>\n#include <poll.h>\n#include <fcntl.h>\n"
         "#include <errno.h>\n#include <unistd.h>\n#include <sys/stat.h>\n#endif\n")
# 6) Version de Windows jugable (rama classic-pc-jugable, pc/ y docs/pc_jugable.md): la base de Sascha Willems ya trae
#    la ventana Win32; con VK_EXAMPLE_PS5 + _WIN32 el bucle de Windows hace lo que el de la consola (ps5HandleInput,
#    quit). Todo dentro de #if defined(_WIN32): en la PS5 no compila nada nuevo.
parchear("base/vulkanexamplebase.h", "Windows (pc/main_windows.cpp)",
         "\tvoid ps5HandleInput();\n",
         "\tvoid ps5HandleInput();\n"
         "#if defined(_WIN32)\n"
         "\t/** @brief Windows (pc/main_windows.cpp): el juego (Salir) termina el bucle como en la consola */\n"
         "\tbool quit = false;\n"
         "#endif\n")
parchear("base/vulkanexamplebase.cpp", "ps5.framesDrawn++;\n\t\t\tif (quit)",
         "\t\tif (prepared && !IsIconic(window)) {\n\t\t\tnextFrame();\n\t\t}\n",
         "\t\tif (prepared && !IsIconic(window)) {\n"
         "#if defined(VK_EXAMPLE_PS5)\n"
         "\t\t\t// Windows (pc/): el mando y el teclado como en la consola (example_ps5.cpp) y quit del juego\n"
         "\t\t\tps5HandleInput();\n"
         "\t\t\tnextFrame();\n"
         "\t\t\tps5.framesDrawn++;\n"
         "\t\t\tif (quit) {\n"
         "\t\t\t\tbreak;\n"
         "\t\t\t}\n"
         "#else\n"
         "\t\t\tnextFrame();\n"
         "#endif\n"
         "\t\t}\n"
         "#if defined(VK_EXAMPLE_PS5)\n"
         "\t\telse {\n"
         "\t\t\tSleep(10);   // minimizada: sin girar en vacio\n"
         "\t\t}\n"
         "#endif\n")
# las teclas las lee la plataforma como botones del mando (pc/plataforma_windows.c): la base ya no las procesa (Esc
# cerraba el juego, P pausaba, F1 abria Ajustes y keyPressed las mandaba otra vez al juego)
parchear("base/vulkanexamplebase.cpp", "las lee pc/plataforma_windows.c",
         "\tcase WM_KEYDOWN:\n\t\tswitch (wParam)\n",
         "\tcase WM_KEYDOWN:\n"
         "#if defined(VK_EXAMPLE_PS5)\n"
         "\t\tbreak;   // Windows: las lee pc/plataforma_windows.c como botones del mando\n"
         "#endif\n"
         "\t\tswitch (wParam)\n")
# ventana de tamano fijo (la resolucion se elige al arrancar: el juego no espera que cambie a mitad)
parchear("base/vulkanexamplebase.cpp", "WS_THICKFRAME | WS_MAXIMIZEBOX",
         "\t\tdwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;\n",
         "#if defined(VK_EXAMPLE_PS5)\n"
         "\t\tdwStyle = (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX)) | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;\n"
         "#else\n"
         "\t\tdwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;\n"
         "#endif\n")
# el titulo de la ventana: solo el nombre del juego (no la GPU)
parchear("base/vulkanexamplebase.cpp", "return title;   // Windows",
         "\tstd::string windowTitle{ title + \" - \" + deviceProperties.deviceName };\n",
         "#if defined(VK_EXAMPLE_PS5)\n"
         "\treturn title;   // Windows (pc/): el nombre del juego\n"
         "#endif\n"
         "\tstd::string windowTitle{ title + \" - \" + deviceProperties.deviceName };\n")
# el icono del .exe (pc/yugioh.rc, recurso 1) si lo tiene
parchear("base/vulkanexamplebase.cpp", "MAKEINTRESOURCE(1)",
         "\t\t.hIcon = LoadIcon(NULL, IDI_APPLICATION),\n",
         "#if defined(VK_EXAMPLE_PS5)\n"
         "\t\t.hIcon = LoadIcon(hinstance, MAKEINTRESOURCE(1)) ? LoadIcon(hinstance, MAKEINTRESOURCE(1)) : LoadIcon(NULL, IDI_APPLICATION),\n"
         "#else\n"
         "\t\t.hIcon = LoadIcon(NULL, IDI_APPLICATION),\n"
         "#endif\n")
print("parche_base: Windows listo")
