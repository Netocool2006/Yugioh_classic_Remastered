#!/usr/bin/env bash
# Capturas FIJAS (png) de los entornos por rival (src/entornos.h) en la vista previa del PC, por turno (candado).
# Compila solo el PC de esta rama y guarda el cuadro <cuadro> (30 por segundo) de cada entorno pedido con la camara
# de juego (o la que digan las variables extra). Resolucion: ANCHO x ALTO (por defecto 1920x1080; la captura sale a
# la mitad).
# Uso (dentro del contenedor arch-ps5):
#   bash captura_entornos.sh <remake-ps5> <carpeta_salida> <cuadro> <clave> ... [-- VAR=valor ...]
# Ejemplo: ... captura_entornos.sh /compartido/wt/classic-entornos/remake-ps5 /compartido/capturas_entornos 45 \
#              reino_duelistas_castillo dirigible_kaiba -- YUGI_ENTORNO_MEDIR=1
set -euo pipefail
src=$1; salida=$2; cuadro=$3; shift 3
claves=(); extra=()
while [[ $# -gt 0 ]]; do
	if [[ "$1" == "--" ]]; then shift; extra=("$@"); break; fi
	claves+=("$1"); shift
done
ancho=${ANCHO:-1920}; alto=${ALTO:-1080}
exec 9>/compartido/.compilar.lock
flock 9
cd /work/PS5_ClassicRemaster
YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar.log 2>&1 || { cat /tmp/sincronizar.log; exit 1; }
ninja -C build/ps5-host ps5-samples-host > /tmp/host.log 2>&1 || { grep -E "error" /tmp/host.log | head -30; exit 1; }
python3 ps5/tools/build-assets.py --install "dist/$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' ps5/sce_sys/param.json)" | tail -1
mkdir -p "$salida"
b=build/ps5-host
for c in "${claves[@]}"; do
	printf 'frames %s\nsamples yugireload\n' "$((cuadro + 2))" > $b/app/test-run.txt
	env YUGI_ANCHO=$ancho YUGI_ALTO=$alto YUGI_FPS=30 YUGI_ENTORNO="$c" "${extra[@]}" \
	PS5_RECORD="ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s $((ancho / 2))x$((alto / 2)) -r 30 -i - -vf 'select=eq(n\,$cuadro)' -frames:v 1 -update 1 $salida/$c.png" \
		timeout -s KILL "${TIEMPO_MAX:-240}" $b/ps5-samples-host > /tmp/captura_entornos.log 2>&1 || true   # nunca retener el candado
	grep -E "\[entornos\]|rror" /tmp/captura_entornos.log | head -24 || true
	ls -la "$salida/$c.png" || true
done
