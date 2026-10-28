#!/usr/bin/env bash
# Rama classic-front-total: la prueba de PIXELES de la mesa en la vista previa del PC. Compila el PC de la rama UNA vez
# (candado) y corre cada configuracion (rival, tapete, camara y cartas) con YUGI_PRUEBA_FT (src/pruebas/front_total_pc.h):
# el juego arma la mesa, invoca con el mando, saca dos fotos con las zonas en pixeles y se cierra. Despues
# pixeles_ft.py mira si cada carta boca arriba (propia y del rival, en ataque y en defensa) se ve o si queda la casilla.
# Uso (dentro del contenedor arch-ps5):
#   bash captura_front_total.sh <remake-ps5> <carpeta_salida> [salida.tsv]
# Escribe <carpeta_salida>/<config>/ft_1.ppm, ft_2.ppm, *_zonas.txt, *_marcada.png, log.txt y el TSV de casos
# (CU-FT-PIXEL-...; por defecto <carpeta_salida>/pixeles.tsv).
set -uo pipefail
src=$1; salida=$2; tsv=${3:-$2/pixeles.tsv}
ancho=${ANCHO:-1920}; alto=${ALTO:-1080}
mkdir -p "$salida"
: > "$tsv"
(
	exec 9>/compartido/.compilar.lock
	flock 9
	cd /work/PS5_ClassicRemaster
	YUGI_SRC="$src" bash "$src/sincronizar.sh" > /tmp/sincronizar_ft.log 2>&1 || { cat /tmp/sincronizar_ft.log; exit 1; }
	ninja -C build/ps5-host ps5-samples-host > /tmp/host_ft.log 2>&1 || { grep -E "error" /tmp/host_ft.log | head -30; exit 1; }
	python3 ps5/tools/build-assets.py --install "dist/$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' ps5/sce_sys/param.json)" | tail -1
	b=build/ps5-host
	# config|rival|tapete|camara|cartas (invocar, defensa propia, ataque rival, defensa rival)|VAR=valor ...
	while IFS='|' read -r nombre rival tapete camara cartas vars; do
		[[ -z "$nombre" || "$nombre" == \#* ]] && continue
		dir="$salida/$nombre"
		rm -rf "$dir" && mkdir -p "$dir" && chmod 777 "$dir"
		rm -rf /tmp/captura_ft && mkdir -p /tmp/captura_ft
		printf 'frames %s\nsamples yugireload\n' 2400 > $b/app/test-run.txt
		env YUGI_ANCHO=$ancho YUGI_ALTO=$alto YUGI_FPS=30 YUGI_MAZO=/tmp/captura_ft/mazo.txt YUGI_PRUEBA_FT="$dir" YUGI_FT_RIVAL="$rival" YUGI_FT_TAPETE="$tapete" \
			YUGI_FT_CAMARA="$camara" YUGI_FT_CARTAS="$cartas" \
			PS5_RECORD="ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s $((ancho / 2))x$((alto / 2)) -r 30 -i - -vf 'select=eq(mod(n\,15)\,14)' -fps_mode vfr -start_number 1 $dir/c_%04d.png" \
			$vars timeout -s KILL "${TIEMPO_MAX:-240}" $b/ps5-samples-host > "$dir/log.txt" 2>&1 < /dev/null || true   # nunca retener el candado
		echo "$nombre: $(ls "$dir"/*_zonas.txt 2>/dev/null | wc -l) fotos, $(ls "$dir"/c_*.png 2>/dev/null | wc -l) cuadros"
	done <<'LISTA'
tea_auto_sin_figura|tea|auto|0|battle_ox,mystical_elf,giant_soldier_of_stone,vorse_raider|
tea_auto_figuras|tea|auto|0|celtic_guardian,summoned_skull,dark_magician,baby_dragon|YUGI_FT_MODO=figuras
pradera|tea|pradera|0|battle_ox,mystical_elf,giant_soldier_of_stone,vorse_raider|
mar|tea|mar|0|battle_ox,mystical_elf,giant_soldier_of_stone,vorse_raider|
sombras_figuras|tea|sombras|0|celtic_guardian,summoned_skull,dark_magician,baby_dragon|YUGI_FT_MODO=figuras
joey_auto|joey|auto|0|battle_ox,mystical_elf,giant_soldier_of_stone,vorse_raider|
figuras_apagadas|tea|auto|0|celtic_guardian,summoned_skull,dark_magician,baby_dragon|YUGI_FT_MODO=figuras YUGI_FIGURAS=0
LISTA
)
for d in "$salida"/*/; do
	n=$(basename "$d")
	python3 "$src/pixeles_ft.py" "$d" "$n" "$tsv" | sed "s/^/  /"
done
echo "casos: $(wc -l < "$tsv"), fallas: $(grep -c "	FALLA	" "$tsv")"
