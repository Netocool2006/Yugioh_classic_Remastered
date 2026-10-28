#!/usr/bin/env bash
# Capturas FIJAS (png, sin video) de los efectos del Niagara Examples Pack (rama niagara-horneado) en la vista previa
# del PC, por turno (candado). Compila solo el PC de esta rama y guarda cuadros fijos (30 por segundo) por pedido:
#   <nombre>:<cuadro>[:VAR=valor,VAR=valor...]     <cuadro> = N (ese cuadro) o A-B/P (de A a B cada P: nombre_<n>.png)
# Uso (dentro del contenedor arch-ps5):
#   bash captura_fx.sh <remake-ps5> <carpeta_salida> fuego:75:YUGI_DUELO=2 mago:40-120/20:YUGI_DUELO=0 ...
# El ataque empieza enseguida (YUGI_ATAQUE_YA=1, a los 0.3 s). Cada captura con limite de tiempo (TIEMPO_MAX, 240 s):
# el render del PC es por CPU y el candado se comparte con otros agentes.
set -euo pipefail
src=$1; salida=$2; shift 2
exec 9>/compartido/.compilar.lock
flock 9
cd /work/PS5_ClassicRemaster
YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar.log 2>&1 || { cat /tmp/sincronizar.log; exit 1; }
grep -E "efectos del pack|aviso" /tmp/sincronizar.log || true
ninja -C build/ps5-host ps5-samples-host > /tmp/host.log 2>&1 || { grep -E "error" /tmp/host.log | head -30; exit 1; }
python3 ps5/tools/build-assets.py --install "dist/$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' ps5/sce_sys/param.json)" | tail -1
mkdir -p "$salida"
b=build/ps5-host
for pedido in "$@"; do
	IFS=: read -r nombre cuadro vars <<< "$pedido"
	extra=()
	if [[ -n "${vars:-}" ]]; then IFS=, read -r -a extra <<< "$vars"; fi
	if [[ "$cuadro" == *-* ]]; then
		desde=${cuadro%%-*}; resto=${cuadro#*-}; hasta=${resto%%/*}; paso=${resto#*/}
		filtro="select=between(n\,$desde\,$hasta)*not(mod(n-$desde\,$paso))"
		destino="-fps_mode passthrough -frame_pts 1 $salida/${nombre}_%03d.png"
		ultimo=$hasta
	else
		filtro="select=eq(n\,$cuadro)"
		destino="-frames:v 1 -update 1 $salida/$nombre.png"
		ultimo=$cuadro
	fi
	printf 'frames %s\nsamples yugireload\n' "$((ultimo + 2))" > $b/app/test-run.txt
	env YUGI_ANCHO=1920 YUGI_ALTO=1080 YUGI_FPS=30 YUGI_ATAQUE_YA=1 YUGI_FX_MEDIR=1 "${extra[@]}" \
	PS5_RECORD="ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s 960x540 -r 30 -i - -vf '$filtro' $destino" \
		timeout -s KILL "${TIEMPO_MAX:-240}" $b/ps5-samples-host > /tmp/captura_fx.log 2>&1 || true   # nunca retener el candado
	grep -E "\[fx\] [0-9]|rror" /tmp/captura_fx.log | head -10 || true
	ls "$salida/$nombre"*.png 2> /dev/null | tail -3 || true
done
