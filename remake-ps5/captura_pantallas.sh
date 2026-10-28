#!/usr/bin/env bash
# Capturas (png) de las pantallas del juego en la vista previa del PC, por turno (candado). Compila solo el PC de la
# rama UNA vez y corre cada pedido de <lista>: una linea "nombre|segundos|guion|VAR=valor VAR=valor" (# comenta).
# Cada corrida guarda un cuadro por segundo en <salida>/<nombre>/s_<n>.png (s_001 = el primer segundo) para elegir
# despues. Cada corrida usa un YUGI_MAZO en una carpeta vacia (progreso nuevo: la primera vez).
# Uso (dentro del contenedor arch-ps5):
#   bash captura_pantallas.sh <remake-ps5> <carpeta_salida> <lista>
# Ejemplo de lista:
#   portada|4||
#   menu|3|0.5|YUGI_MENU=1
#   opciones_en|4|abajo*3,X|YUGI_MENU=1 YUGI_IDIOMA=en
set -euo pipefail
src=$1; salida=$2; lista=$3
ancho=${ANCHO:-1920}; alto=${ALTO:-1080}
exec 9>/compartido/.compilar.lock
flock 9
cd /work/PS5_ClassicRemaster
YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar.log 2>&1 || { cat /tmp/sincronizar.log; exit 1; }
ninja -C build/ps5-host ps5-samples-host > /tmp/host.log 2>&1 || { grep -E "error" /tmp/host.log | head -30; exit 1; }
python3 ps5/tools/build-assets.py --install "dist/$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' ps5/sce_sys/param.json)" | tail -1
b=build/ps5-host
while IFS='|' read -r nombre segundos guion vars; do
	[[ -z "$nombre" || "$nombre" == \#* ]] && continue
	dir="$salida/$nombre"
	rm -rf "$dir" && mkdir -p "$dir"
	rm -rf /tmp/captura_pant && mkdir -p /tmp/captura_pant
	printf 'frames %s\nsamples yugireload\n' "$((segundos * 30 + 2))" > $b/app/test-run.txt
	extra=()
	[[ -n "$guion" ]] && extra+=("YUGI_GUION=$guion")
	read -r -a vv <<< "$vars"
	env YUGI_ANCHO=$ancho YUGI_ALTO=$alto YUGI_FPS=30 YUGI_MAZO=/tmp/captura_pant/mazo.txt "${extra[@]}" "${vv[@]}" \
	PS5_RECORD="ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s $((ancho / 2))x$((alto / 2)) -r 30 -i - -vf 'select=eq(mod(n\,30)\,29)' -fps_mode vfr -start_number 1 $dir/s_%03d.png" \
		timeout -s KILL "${TIEMPO_MAX:-300}" $b/ps5-samples-host > "$dir/log.txt" 2>&1 < /dev/null || true   # nunca retener el candado
	echo "$nombre: $(ls "$dir"/*.png 2>/dev/null | wc -l) png"
done < "$lista"
