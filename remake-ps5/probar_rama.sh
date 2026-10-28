#!/usr/bin/env bash
# Yugioh Classic Remaster: compila UNA rama (PS5 y vista previa de PC) por turno (candado) en su propia plantilla
# (/work/PS5_ClassicRemaster, titulo PPSA77002), deja el titulo listo para subir en /compartido/ps5/PPSA77002 (copia
# atomica) y, si se pide, graba un video de PC.
# Uso (dentro del contenedor arch-ps5):
#   bash /compartido/wt/<rama>/remake-ps5/probar_rama.sh /compartido/wt/<rama>/remake-ps5 <salida.mp4|-> <segundos> [VAR=valor ...]
# Sin <salida.mp4> ("-") solo compila.
set -euo pipefail
src=$1; salida=$2; segundos=${3:-9}; shift 3 || true
exec 9>/compartido/.compilar.lock
flock 9
cd /work/PS5_ClassicRemaster
YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar.log 2>&1 || { cat /tmp/sincronizar.log; exit 1; }
bash ps5/tools/build.sh > /tmp/build.log 2>&1 || { grep -E "error" /tmp/build.log | head -30; exit 1; }
ninja -C build/ps5-host ps5-samples-host > /tmp/host.log 2>&1 || { grep -E "error" /tmp/host.log | head -30; exit 1; }
grep -E "warning:" /tmp/build.log /tmp/host.log | grep -v "unused-command-line" | head -10 || true
echo "COMPILADO $src"
# el titulo para subir a la PS5 (subir_ps5.py D:/Docker/compartido/ps5/PPSA77002): copia nueva y cambio de nombre
# rsync: el titulo pesa ~3 GB y casi todo son caras que no cambian (una copia completa tardaba minutos)
mkdir -p /compartido/ps5/PPSA77002
rsync -a --delete dist/PPSA77002/ /compartido/ps5/PPSA77002/
echo "titulo listo en /compartido/ps5/PPSA77002"
if [[ "$salida" != "-" ]]; then
	mkdir -p "$(dirname "$salida")"
	# (video_pc.sh es del juego original: se usa una copia que apunta a esta plantilla)
	sed "s|/work/PS5_YugiReload|/work/PS5_ClassicRemaster|" /compartido/video_pc.sh > /tmp/video_classic.sh
	env "$@" bash /tmp/video_classic.sh "$segundos" "$salida" | tail -1
fi
