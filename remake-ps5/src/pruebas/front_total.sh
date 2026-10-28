#!/usr/bin/env bash
# La bateria desde el front (rama classic-front-total), dentro del contenedor arch-ps5:
#   bash /compartido/wt/<rama>/remake-ps5/src/pruebas/front_total.sh <salida.tsv> [duelos|cartas|todo] [tope duelos] [tope cartas] [primera carta]
# Arma una carpeta ui/ con las cartas (classic-assets) y los datos de la rama, compila prueba_front_total.cpp y la corre.
set -uo pipefail
rama=$(cd "$(dirname "$0")/../.." && pwd)
assets=${YUGI_ASSETS:-/compartido/classic-assets}
tmp=${FT_TMP:-/tmp/ft}
rm -rf "$tmp" && mkdir -p "$tmp/ui"
cp "$assets"/ui/cartas_ui.txt "$assets"/ui/cartas_ui_en.txt "$assets"/ui/cartas_classic.txt "$tmp/ui/"
cp "$rama"/datos/*.txt "$tmp/ui/"
[[ -d "$assets/md_ritmo" ]] && cp -r "$assets/md_ritmo" "$tmp/"   # rama classic-md-ritmo: los movimientos de cartas de Master Duel
[[ -d "$assets/md_ui" ]] && mkdir -p "$tmp/md_ui" && cp "$assets"/md_ui/*.mdui "$assets"/md_ui/*.mdtl "$tmp/md_ui/" 2> /dev/null   # rama classic-md-entradas: menu del duelo y fin (Tween / Timeline de MD)
base=/work/PS5_ClassicRemaster/external
bin=$tmp/prueba_front_total
g++ -std=c++17 -O2 -DPS5_HOST_REFERENCE -I "$rama/src" -I "$base/tinygltf" -I "$base/glm" "$rama/src/pruebas/prueba_front_total.cpp" -o "$bin" || exit 3
salida=$1
shift
"$bin" "$tmp" "$salida" "$@"
