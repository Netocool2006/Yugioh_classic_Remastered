#!/usr/bin/env bash
# Rama classic-md-ritmo: capturas en SECUENCIA del ritmo de Master Duel en la vista previa del PC (src/md_ritmo_pc.h).
# Compila el PC de la rama UNA vez (candado) y corre cada escena (coloca, invoca, swords, luz, cadena): el rival la juega y
# el juego saca una foto cada YUGI_RITMO_CADA s; luego las pasa a PNG (<salida>/<escena>_NN.png).
# Uso (dentro del contenedor arch-ps5): bash captura_ritmo.sh <remake-ps5> <carpeta_salida> [escenas...]
set -uo pipefail
src=$1; salida=$2; shift 2
escenas=${*:-coloca invoca swords luz cadena}
ancho=${ANCHO:-1920}; alto=${ALTO:-1080}
mkdir -p "$salida"
(
	exec 9>/compartido/.compilar.lock
	flock 9
	cd /work/PS5_ClassicRemaster
	YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar_ritmo.log 2>&1 || { cat /tmp/sincronizar_ritmo.log; exit 1; }
	ninja -C build/ps5-host ps5-samples-host > /tmp/host_ritmo.log 2>&1 || { grep -E "error" /tmp/host_ritmo.log | head -30; exit 1; }
	python3 ps5/tools/build-assets.py --install "dist/$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' ps5/sce_sys/param.json)" | tail -1
	b=build/ps5-host
	for e in $escenas; do
		dir=/tmp/captura_ritmo_$e
		rm -rf "$dir" && mkdir -p "$dir" && chmod 777 "$dir"
		printf 'frames %s\nsamples yugireload\n' 3000 > $b/app/test-run.txt
		env YUGI_ANCHO=$ancho YUGI_ALTO=$alto YUGI_FPS=30 YUGI_MAZO=$dir/mazo.txt YUGI_PRUEBA_RITMO="$dir" YUGI_RITMO_ESCENA="$e" \
			timeout -s KILL "${TIEMPO_MAX:-240}" $b/ps5-samples-host > "$salida/log_$e.txt" 2>&1 < /dev/null || true
		n=0
		for f in "$dir"/${e}_*.ppm; do
			[[ -f "$f" ]] || continue
			python3 -c "from PIL import Image; import sys; Image.open(sys.argv[1]).save(sys.argv[2])" "$f" "$salida/$(basename "${f%.ppm}").png" && n=$((n + 1))
		done
		echo "$e: $n fotos"
	done
)
