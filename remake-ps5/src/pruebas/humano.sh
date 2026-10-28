#!/usr/bin/env bash
# El jugador humano (rama classic-jugador-humano), dentro del contenedor arch-ps5:
#   bash /compartido/wt/<rama>/remake-ps5/src/pruebas/humano.sh <salida.tsv> [todo|duelos|fijos|volcar] [duelos] [semilla]
# Arma una carpeta con las cartas (classic-assets: solo los .txt, sin imagenes), los datos de la rama, las fuentes y los
# Tween/Timeline de Master Duel; compila prueba_humano.cpp (la interfaz de verdad sin GPU) y la corre.
set -uo pipefail
rama=$(cd "$(dirname "$0")/../.." && pwd)
assets=${YUGI_ASSETS:-/compartido/classic-assets}
tmp=${HU_TMP:-/tmp/hu}
rm -rf "$tmp" && mkdir -p "$tmp/ui/fuentes" "$tmp/md_ui"
cp "$assets"/ui/cartas_ui.txt "$assets"/ui/cartas_ui_en.txt "$assets"/ui/cartas_classic.txt "$tmp/ui/"
cp "$rama"/datos/*.txt "$tmp/ui/"
cp "$rama"/third_party/fuentes/*.ttf "$tmp/ui/fuentes/"
[[ -d "$assets/md_ritmo" ]] && cp -r "$assets/md_ritmo" "$tmp/"
# (sin md_ui.png: sin textura se usan los respaldos por codigo; con ella la interfaz querria subirla a la GPU)
cp "$assets"/md_ui/*.mdui "$assets"/md_ui/*.mdtl "$tmp/md_ui/" 2> /dev/null
base=/work/PS5_ClassicRemaster
bin=$tmp/prueba_humano
g++ -std=c++17 -O2 ${HU_CFLAGS:-} -DPS5_HOST_REFERENCE -I "$rama/src" -I "$rama/third_party" -I "$base/base" -I "$base/external/tinygltf" -I "$base/external/glm" \
	"$rama/src/pruebas/prueba_humano.cpp" -o "$bin" -lvulkan -lpthread || exit 3
[[ -n "${HU_SOLO_COMPILAR:-}" ]] && exit 0
salida=$1
shift
"$bin" "$tmp" "$salida" "$@"
