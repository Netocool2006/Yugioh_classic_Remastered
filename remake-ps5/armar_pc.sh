#!/usr/bin/env bash
# Yugioh Classic Remaster: arma la version de Windows jugable (docs/pc_jugable.md) en /compartido/pc/YugiohClassic
# (D:\Docker\compartido\pc\YugiohClassic) a partir de la rama y del titulo de PS5 ya armado (/compartido/ps5/PPSA77002).
#  1. Con el candado de compilacion: sincroniza la rama en la plantilla (sincronizar.sh, lo mismo que la PS5) y copia
#     solo las FUENTES (sin los ~4 GB de assets) a una plantilla propia (/work/PC_ClassicRemaster); suelta el candado.
#  2. Compila YugiohClassic.exe con mingw-w64 (pc/CMakeLists.txt): el mismo codigo del juego que la consola.
#  3. Arma la carpeta: el .exe, datos/assets (ENLACES DUROS a los del titulo de PS5: no ocupan espacio de nuevo),
#     datos/shaders (los de esta rama), SDL3.dll (zlib, para DualSense/DualShock 4; version fija con SHA-256) y LEEME.
# Uso (dentro del contenedor arch-ps5):
#   bash /compartido/wt/<rama>/remake-ps5/armar_pc.sh /compartido/wt/<rama>/remake-ps5 [carpeta_salida]
# Variables: YUGI_TITULO (por defecto /compartido/ps5/PPSA77002), SOLO_EXE=1 (no toca datos/).
set -euo pipefail
src=$1
salida=${2:-/compartido/pc/YugiohClassic}
titulo=${YUGI_TITULO:-/compartido/ps5/PPSA77002}
t=/work/PS5_ClassicRemaster
pc=/work/PC_ClassicRemaster
build=$pc/build-win64
command -v x86_64-w64-mingw32-g++ > /dev/null || { echo "falta mingw-w64: pacman -S --needed mingw-w64-gcc"; exit 1; }

# 1) fuentes sincronizadas (con el candado: la plantilla es de todas las ramas)
(
	exec 9>/compartido/.compilar.lock
	flock 9
	YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar_pc.log 2>&1 || { cat /tmp/sincronizar_pc.log; exit 1; }
	mkdir -p "$pc"
	for d in base external ps5/src ps5/third_party examples/yugireload shaders/glsl/base shaders/glsl/yugireload; do
		mkdir -p "$pc/$d"
		rsync -a --delete "$t/$d/" "$pc/$d/"
	done
)
echo "fuentes listas en $pc"

# 2) el .exe (la carpeta de compilacion es de todas las ramas: si la armo otra rama, se borra su cache de CMake)
if [[ -f "$build/CMakeCache.txt" ]] && ! grep -q "^CMAKE_HOME_DIRECTORY:INTERNAL=$src/pc\$" "$build/CMakeCache.txt"; then
	rm -rf "$build/CMakeCache.txt" "$build/CMakeFiles"
fi
# el icono del .exe: sce_sys/icon0.png de la consola (no va al repo) -> yugioh.ico (varios tamanos)
icono_dir=$pc/icono
mkdir -p "$icono_dir"
icono_png=${YUGI_ICONO_PNG:-$titulo/sce_sys/icon0.png}
if [[ -f "$icono_png" ]]; then
	python3 - "$icono_png" "$icono_dir/yugioh.ico" <<'PY' || echo "aviso: no se pudo armar el icono (PIL): el .exe sale sin icono propio"
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGBA")
im.save(sys.argv[2], sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
PY
fi
cmake -G Ninja -S "$src/pc" -B "$build" -DCMAKE_TOOLCHAIN_FILE="$src/pc/toolchain_mingw.cmake" -DYUGI_PLANTILLA="$pc" -DYUGI_ICONO_DIR="$icono_dir" 	-DCMAKE_BUILD_TYPE=Release > /tmp/cmake_pc.log 2>&1 || { tail -30 /tmp/cmake_pc.log; exit 1; }
ninja -C "$build" > /tmp/ninja_pc.log 2>&1 || { grep -E "error|Error" /tmp/ninja_pc.log | head -40; exit 1; }
grep -E "warning:" /tmp/ninja_pc.log | head -10 || true
echo "COMPILADO $build/YugiohClassic.exe ($(du -h "$build/YugiohClassic.exe" | cut -f1))"

# 3) la carpeta jugable
mkdir -p "$salida/datos"
cp "$build/YugiohClassic.exe" "$salida/YugiohClassic.exe.nuevo"
mv -f "$salida/YugiohClassic.exe.nuevo" "$salida/YugiohClassic.exe"
if [[ "${SOLO_EXE:-0}" != "1" ]]; then
	[[ -d "$titulo/assets/ui" ]] || { echo "falta el titulo de PS5 en $titulo (probar_rama.sh lo arma)"; exit 1; }
	# assets: enlaces duros a los del titulo (mismo disco NTFS por el montaje de Docker). rsync --link-dest: los que no
	# cambiaron quedan enlazados; los que cambiaron se copian; los que ya no estan se borran
	# (24: archivos que desaparecieron a mitad porque probar_rama.sh estaba copiando el titulo; el siguiente armado los toma)
	rsync -a --delete --link-dest="$titulo/assets/" "$titulo/assets/" "$salida/datos/assets/" || [[ $? == 24 ]]
	mkdir -p "$salida/datos/shaders/glsl"
	rsync -a --delete --include='*/' --include='*.spv' --exclude='*' "$pc/shaders/glsl/" "$salida/datos/shaders/glsl/"
	cp "$titulo/LEGAL.txt" "$salida/datos/" 2> /dev/null || true
	rsync -a --delete "$titulo/licenses/" "$salida/datos/licenses/" 2> /dev/null || true
fi

# SDL3 (zlib): DualSense / DualShock 4 / Switch en PC. Version fija, verificada; se guarda una copia en cache
sdl_ver=3.2.10
sdl_sha=${SDL_SHA256:-}
cache=/compartido/pc/.cache
mkdir -p "$cache"
zip="$cache/SDL3-$sdl_ver-win32-x64.zip"
if [[ ! -f "$zip" ]]; then
	curl -fsSL -o "$zip.tmp" "https://github.com/libsdl-org/SDL/releases/download/release-$sdl_ver/SDL3-$sdl_ver-win32-x64.zip" \
		&& mv "$zip.tmp" "$zip" || { rm -f "$zip.tmp"; echo "aviso: no se pudo bajar SDL3 (sin DualSense: solo XInput y teclado)"; }
fi
if [[ -f "$zip" ]]; then
	sha=$(sha256sum "$zip" | cut -d' ' -f1)
	esperado=$(cat "$src/pc/SDL3.sha256" 2> /dev/null | cut -d' ' -f1 || true)
	if [[ -n "$esperado" && "$sha" != "$esperado" ]]; then
		echo "aviso: SDL3 $sdl_ver no coincide con pc/SDL3.sha256 ($sha): no se copia"
	else
		rm -rf /tmp/sdl3_pc && mkdir -p /tmp/sdl3_pc
		(cd /tmp/sdl3_pc && bsdtar -xf "$zip" 2> /dev/null || python3 -c "import zipfile,sys; zipfile.ZipFile(sys.argv[1]).extractall('.')" "$zip")
		cp "$(find /tmp/sdl3_pc -name SDL3.dll | head -1)" "$salida/SDL3.dll"
		lic=$(find /tmp/sdl3_pc -iname 'LICENSE*' | head -1)
		mkdir -p "$salida/licencias"
		[[ -n "$lic" ]] && cp "$lic" "$salida/licencias/SDL3_LICENSE.txt"
		echo "SDL3 $sdl_ver copiada ($sha)"
	fi
fi

cat > "$salida/LEEME.txt" <<'LEEME'
Yugioh Classic Remaster - version de Windows (proyecto personal, sin fines de lucro)

Arrancar: YugiohClassic.exe (necesita una GPU con Vulkan 1.3 y su driver al dia).

Opciones (en la linea de comandos o en opciones_pc.txt, una por linea):
  --completa            pantalla completa a la resolucion del escritorio (4K si el monitor es 4K)
  --ancho 1920 --alto 1080   tamano de la ventana
  --sin-vsync           sin V-sync (por defecto va con V-sync a la frecuencia del monitor: 60/120/144 Hz)
  --consola             mensajes en una consola

Mando: Xbox (XInput) siempre; DualSense / DualShock 4 / Switch con SDL3.dll (incluida).
Teclado: flechas = cruceta, Enter/Espacio = X, Esc/Retroceso = circulo, T = triangulo, F = cuadrado,
         Q/E = L1/R1, 1/3 = L2/R2, Z/C = L3/R3, Tab u O = OPTIONS, WASD = stick izquierdo, IJKL = stick derecho.
Raton: apunta y haz clic (clic izquierdo = X / elegir, derecho = volver, central = detalle, rueda = desplazar o L1/R1).
Pantalla completa: Alt+Enter o F11 (en caliente).
Cerrar: la X de la ventana o Alt+F4 (o Salir en el menu).

Guardado: %APPDATA%\YugiohClassic (o, si existe portable.txt junto al .exe, la carpeta guardado\ de aqui).
Registro: yugioh_classic.log en la carpeta de guardado.
LEEME
if [[ ! -f "$salida/opciones_pc.txt" ]]; then
	printf '# opciones de la version de Windows (ver LEEME.txt); por ejemplo:\n# --completa\n# --ancho 2560\n# --alto 1440\n' > "$salida/opciones_pc.txt"
fi
echo "carpeta jugable: $salida ($(du -sh --apparent-size "$salida/datos" 2> /dev/null | cut -f1) de datos, enlazados al titulo)"
