#!/usr/bin/env bash
# Capturas FIJAS (png, sin video) del bosque de la carta de campo en la vista previa del PC, por turno (candado).
# Compila solo el PC de esta rama y guarda el cuadro <cuadro> (30 cuadros por segundo) de cada camara pedida
# (camara LIBRE: giro y elevacion en radianes, distancia en metros al centro de la mesa).
# Uso (dentro del contenedor arch-ps5):
#   bash captura_bosque.sh <remake-ps5> <carpeta_salida> <cuadro> <nombre>=<giro,elev,dist> ... [-- VAR=valor ...]
# Ejemplo: ... captura_bosque.sh /compartido/wt/bosque-real/remake-ps5 /compartido/capturas_bosque 240 \
#              lado=1.2,0.25,24 fondo=3.1,0.3,30 -- YUGI_CAMPO_MEDIR=1
set -euo pipefail
src=$1; salida=$2; cuadro=$3; shift 3
camaras=(); extra=()
while [[ $# -gt 0 ]]; do
	if [[ "$1" == "--" ]]; then shift; extra=("$@"); break; fi
	camaras+=("$1"); shift
done
exec 9>/compartido/.compilar.lock
flock 9
cd /work/PS5_ClassicRemaster
YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar.log 2>&1 || { cat /tmp/sincronizar.log; exit 1; }
ninja -C build/ps5-host ps5-samples-host > /tmp/host.log 2>&1 || { grep -E "error" /tmp/host.log | head -30; exit 1; }
# los assets del titulo (la vista previa los lee de dist/<titulo>/assets): con los del bosque recien sincronizados
python3 ps5/tools/build-assets.py --install "dist/$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' ps5/sce_sys/param.json)" | tail -1
mkdir -p "$salida"
b=build/ps5-host
for c in "${camaras[@]}"; do
	nombre=${c%%=*}; cam=${c#*=}
	printf 'frames %s\nsamples yugireload\n' "$((cuadro + 2))" > $b/app/test-run.txt   # la app lo consume
	# la captura sale a la mitad de la resolucion (YUGI_ANCHO x YUGI_ALTO / 2); ffmpeg guarda un solo cuadro.
	# Con limite de tiempo (TIEMPO_MAX, 300 s): el render del PC es por CPU y comparte el candado con otros agentes
	env YUGI_ANCHO=3840 YUGI_ALTO=2160 YUGI_FPS=30 YUGI_CAMPO_BOSQUE=1 YUGI_CAMARA=libre YUGI_CAM_LIBRE="$cam" "${extra[@]}" \
	PS5_RECORD="ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s 1920x1080 -r 30 -i - -vf 'select=eq(n\\,$cuadro)' -frames:v 1 -update 1 $salida/$nombre.png" \
		timeout -s KILL "${TIEMPO_MAX:-300}" $b/ps5-samples-host > /tmp/captura_bosque.log 2>&1 || true   # nunca retener el candado
	grep -E "\[bosque\]|\[campo\]|rror" /tmp/captura_bosque.log | head -20 || true
	ls -la "$salida/$nombre.png"
done
