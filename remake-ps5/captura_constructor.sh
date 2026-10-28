#!/usr/bin/env bash
# Capturas FIJAS (png) del constructor de mazos (rama classic-constructor2) en la vista previa del PC, por turno
# (candado). Compila solo el PC de esta rama, arranca en "Construir mazo" (YUGI_CONSTRUCTOR=1) con los mazos de la
# primera vez (YUGI_MAZO en una carpeta vacia: importa nada y arma los mazos de inicio), aprieta el guion y guarda el
# cuadro <cuadro> (30 por segundo). MODO=YUGI_LISTA_CARTAS arranca en la lista de cartas (solo ver).
# Uso (dentro del contenedor arch-ps5):
#   bash captura_constructor.sh <remake-ps5> <carpeta_salida> <nombre> <cuadro> "<guion>" [VAR=valor ...]
# Ejemplo: ... captura_constructor.sh /compartido/wt/classic-constructor2/remake-ps5 /compartido/capturas_constructor lista 90 ""
set -euo pipefail
src=$1; salida=$2; nombre=$3; cuadro=$4; guion=$5; shift 5
ancho=${ANCHO:-1920}; alto=${ALTO:-1080}
exec 9>/compartido/.compilar.lock
flock 9
cd /work/PS5_ClassicRemaster
YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar.log 2>&1 || { cat /tmp/sincronizar.log; exit 1; }
ninja -C build/ps5-host ps5-samples-host > /tmp/host.log 2>&1 || { grep -E "error" /tmp/host.log | head -30; exit 1; }
python3 ps5/tools/build-assets.py --install "dist/$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' ps5/sce_sys/param.json)" | tail -1
mkdir -p "$salida"
rm -rf /tmp/captura_mazos && mkdir -p /tmp/captura_mazos
b=build/ps5-host
printf 'frames %s\nsamples yugireload\n' "$((cuadro + 2))" > $b/app/test-run.txt
env YUGI_ANCHO=$ancho YUGI_ALTO=$alto YUGI_FPS=30 ${MODO:-YUGI_CONSTRUCTOR}=1 YUGI_MAZO=/tmp/captura_mazos/mazo.txt YUGI_GUION="$guion" "$@" \
PS5_RECORD="ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s $((ancho / 2))x$((alto / 2)) -r 30 -i - -vf 'select=eq(n\,$cuadro)' -frames:v 1 -update 1 $salida/$nombre.png" \
	timeout -s KILL "${TIEMPO_MAX:-240}" $b/ps5-samples-host > /tmp/captura_constructor.log 2>&1 || true   # nunca retener el candado
grep -E "\[mazo\]|rror" /tmp/captura_constructor.log | grep -v "guardado en\|cargado de" | head -20 || true
ls -la "$salida/$nombre.png" || true
