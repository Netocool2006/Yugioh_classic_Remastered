# Yugioh Classic Remaster: la version de Windows jugable (docs/pc_jugable.md).
#
# Compila las MISMAS fuentes que la PS5, tomadas de la plantilla ya sincronizada (sincronizar.sh:
# /work/PS5_ClassicRemaster): la base de Sascha Willems (con su ventana Win32), imgui, ktx, volk y el juego
# (examples/yugireload/yugireload.cpp, en su namespace como en la consola: ps5/src/wrap.cpp.in). Lo unico propio de
# Windows es esta carpeta: el arranque (main_windows.cpp, en lugar de ps5/src/main.cpp), platform.h en Windows
# (plataforma_windows.c) y lo POSIX que MinGW no trae igual (compat_windows.h).
# Uso (lo hace armar_pc.sh): cmake -G Ninja -S pc -B <build> -DCMAKE_TOOLCHAIN_FILE=pc/toolchain_mingw.cmake
#   -DYUGI_PLANTILLA=/work/PS5_ClassicRemaster && ninja -C <build>
cmake_minimum_required(VERSION 3.20)
project(yugioh_classic_windows C CXX RC)

set(YUGI_PLANTILLA "/work/PS5_ClassicRemaster" CACHE PATH "la plantilla de la PS5 ya sincronizada")
# carpeta con yugioh.ico (armar_pc.sh lo arma del icono de la consola, que no va al repo); sin el, el .exe sale sin icono propio
set(YUGI_ICONO_DIR "" CACHE PATH "carpeta con yugioh.ico")
set(T ${YUGI_PLANTILLA})

# el juego dentro de su namespace, como en la consola
set(ID yugireload)
set(SOURCE ${T}/examples/yugireload/yugireload.cpp)
configure_file(${T}/ps5/src/wrap.cpp.in ${CMAKE_BINARY_DIR}/wrap/yugireload.cpp @ONLY)

file(GLOB base_sources ${T}/base/*.cpp)
list(FILTER base_sources EXCLUDE REGEX "VulkanAndroid|VulkanOHOS")
set(third_party_sources
	${T}/external/imgui/imgui.cpp
	${T}/external/imgui/imgui_demo.cpp
	${T}/external/imgui/imgui_draw.cpp
	${T}/external/imgui/imgui_widgets.cpp
	${T}/external/ktx/lib/texture.c
	${T}/external/ktx/lib/hashlist.c
	${T}/external/ktx/lib/checkheader.c
	${T}/external/ktx/lib/swap.c
	${T}/external/ktx/lib/memstream.c
	${T}/external/ktx/lib/filestream.c
	${T}/external/ktx/lib/vkloader.c
	${T}/ps5/third_party/volk/volk.c)
set_source_files_properties(${third_party_sources} PROPERTIES COMPILE_OPTIONS "-w")

add_executable(YugiohClassic WIN32
	${base_sources}
	${third_party_sources}
	${CMAKE_BINARY_DIR}/wrap/yugireload.cpp
	${T}/ps5/src/samples.cpp
	${T}/ps5/src/example_ps5.cpp
	main_windows.cpp
	plataforma_windows.c
	yugioh.rc)
target_compile_definitions(YugiohClassic PRIVATE
	VK_EXAMPLE_PS5
	"PS5_TITLE_NAME=\"Yugioh Classic Remaster\""
	VK_USE_PLATFORM_WIN32_KHR
	VK_NO_PROTOTYPES
	NDEBUG
	NOMINMAX
	_WIN32_WINNT=0x0A00
	WIN32_LEAN_AND_MEAN
	_USE_MATH_DEFINES
	VK_EXAMPLE_ASSETS_DIR="datos/assets/"
	VK_EXAMPLE_SHADERS_DIR="datos/shaders/")
if(YUGI_ICONO_DIR AND EXISTS ${YUGI_ICONO_DIR}/yugioh.ico)
	target_compile_definitions(YugiohClassic PRIVATE YUGI_TIENE_ICONO)
	target_include_directories(YugiohClassic PRIVATE ${YUGI_ICONO_DIR})
	set_source_files_properties(yugioh.rc PROPERTIES OBJECT_DEPENDS ${YUGI_ICONO_DIR}/yugioh.ico)
endif()
target_include_directories(YugiohClassic PRIVATE
	${T}/external
	${T}/external/glm
	${T}/external/imgui
	${T}/external/tinygltf
	${T}/external/ktx/include
	${T}/external/ktx/other_include
	${T}/base
	${T}/ps5/third_party/volk
	${T}/ps5/src
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/mingw)
set_target_properties(YugiohClassic PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON C_STANDARD 11)
# compat_windows.h primero (las cabeceras del sistema a nivel de archivo, antes del namespace del juego) y volk
# despues, como en la consola: los comandos de Vulkan son los punteros de volk (vulkan-1.dll del driver)
target_compile_options(YugiohClassic PRIVATE
	$<$<NOT:$<COMPILE_LANGUAGE:RC>>:-O2>
	$<$<NOT:$<COMPILE_LANGUAGE:RC>>:-g0>
	"$<$<NOT:$<COMPILE_LANGUAGE:RC>>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/compat_windows.h>"
	"$<$<NOT:$<COMPILE_LANGUAGE:RC>>:SHELL:-include ${T}/ps5/third_party/volk/volk.h>")
# un solo .exe: libstdc++, libgcc y winpthreads estaticos (sin DLL de MinGW al lado)
target_link_options(YugiohClassic PRIVATE -static -static-libgcc -static-libstdc++ -s)
# winmm: waveOut; shell32: SHGetFolderPath; ws2_32: la red (rama classic-red-base, socket_udp.h)
target_link_libraries(YugiohClassic PRIVATE winmm shell32 ws2_32 ole32)
