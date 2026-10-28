#!/usr/bin/env bash
# Capturas FIJAS (png) de las figuras 3D de las cartas emblematicas (src/figuras.h) en la vista previa del PC, por
# turno (candado), en UNA sola corrida: guarda los cuadros pedidos (30 por segundo) con la mesa y los ataques de
# prueba de las variables (YUGI_FIGURAS_MESA, YUGI_FIGURAS_ATAQUE). Resolucion: ANCHO x ALTO (1920x1080; sale a la mitad).
# Uso (dentro del contenedor arch-ps5):
#   bash captura_figuras.sh <remake-ps5> <carpeta_salida> <nombre> <cuadro,cuadro,...> [VAR=valor ...]
# Ejemplo: ... captura_figuras.sh /compartido/wt/classic-figuras/remake-ps5 /compartido/capturas_figuras mesa 60,95 \
#              YUGI_FIGURAS_MESA=jugador_monstruo_1=dark_magician,rival_monstruo_2=blue_eyes
set -euo pipefail
src=$1; salida=$2; nombre=$3; cuadros=$4; shift 4
ancho=${ANCHO:-1920}; alto=${ALTO:-1080}
exec 9>/compartido/.compilar.lock
flock 9
cd /work/PS5_ClassicRemaster
if [[ -z "${SIN_COMPILAR:-}" ]]; then
	YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar.log 2>&1 || { cat /tmp/sincronizar.log; exit 1; }
	ninja -C build/ps5-host ps5-samples-host > /tmp/host.log 2>&1 || { grep -E "error" /tmp/host.log | head -30; exit 1; }
	python3 ps5/tools/build-assets.py --install "dist/$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' ps5/sce_sys/param.json)" | tail -1
fi
mkdir -p "$salida"
b=build/ps5-host
ultimo=0; filtro=""
IFS=',' read -ra lista <<< "$cuadros"
for c in "${lista[@]}"; do
	filtro+="${filtro:++}eq(n\\,$c)"
	(( c > ultimo )) && ultimo=$c
done
printf 'frames %s\nsamples yugireload\n' "$((ultimo + 2))" > $b/app/test-run.txt
env YUGI_ANCHO=$ancho YUGI_ALTO=$alto YUGI_FPS=30 "$@" \
PS5_RECORD="ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s $((ancho / 2))x$((alto / 2)) -r 30 -i - -vf 'select=$filtro' -fps_mode passthrough $salida/${nombre}_%02d.png" \
	timeout -s KILL "${TIEMPO_MAX:-240}" $b/ps5-samples-host > /tmp/captura_figuras.log 2>&1 || true   # nunca retener el candado
grep -E "\[figuras\]|\[ataque\]|rror" /tmp/captura_figuras.log | head -40 || true
ls -la "$salida"/${nombre}_*.png || true
