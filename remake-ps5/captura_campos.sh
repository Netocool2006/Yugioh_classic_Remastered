#!/usr/bin/env bash
# Capturas FIJAS (png) de las cartas de campo (src/campos.h, rama classic-campos) en la vista previa del PC, por turno
# (candado). Compila solo el PC de esta rama y guarda el cuadro <cuadro> (30 por segundo) de cada carta pedida: la
# app entra al duelo (GUION, por defecto "0.5,X": Duelo en el modo historia), la carta se activa a los ACTIVA s (4) y
# ~6 s despues todo esta crecido (cuadro >= 330). Una carta con
# ",rival" la activa el rival. Resolucion: ANCHO x ALTO (por defecto 1920x1080; la captura sale a la mitad).
# Uso (dentro del contenedor arch-ps5):
#   bash captura_campos.sh <remake-ps5> <carpeta_salida> <cuadro> <carta>[,rival] ... [-- VAR=valor ...]
# Ejemplo: ... captura_campos.sh /compartido/wt/classic-campos/remake-ps5 /compartido/capturas_campos 200 umi yami,rival
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
	env YUGI_ANCHO=$ancho YUGI_ALTO=$alto YUGI_FPS=30 YUGI_CAMPO="$c" YUGI_CAMPO_BOSQUE=${ACTIVA:-4} YUGI_GUION="${GUION:-0.5,X}" YUGI_CAMPO_MEDIR=1 "${extra[@]}" \
	PS5_RECORD="ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s $((ancho / 2))x$((alto / 2)) -r 30 -i - -vf 'select=eq(n\,$cuadro)' -frames:v 1 -update 1 $salida/${c%%,*}.png" \
		timeout -s KILL "${TIEMPO_MAX:-240}" $b/ps5-samples-host > /tmp/captura_campos.log 2>&1 || true   # nunca retener el candado
	grep -E "\[entornos\]|\[campos\]|\[campo\] con|rror" /tmp/captura_campos.log | head -24 || true
	ls -la "$salida/${c%%,*}.png" || true
done
