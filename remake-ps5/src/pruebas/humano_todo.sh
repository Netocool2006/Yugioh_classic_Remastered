#!/usr/bin/env bash
# El jugador humano completo como en ciclo_completo.sh (paso 5d), sin el resto del ciclo, dentro del contenedor arch-ps5:
#   bash /compartido/wt/<rama>/remake-ps5/src/pruebas/humano_todo.sh <carpeta de salida> [lotes] [semilla]
# Compila prueba_humano.cpp y corre a la vez: los casos fijos + 6 duelos (humano_general.tsv) y la carta por carta en
# <lotes> procesos (humano_cartas_<k>.tsv). Deja humano.log, *.fallas.txt y resumen.txt.
set -uo pipefail
rama=$(cd "$(dirname "$0")/../.." && pwd)
salida=$1; lotes=${2:-4}; semilla=${3:-1}
mkdir -p "$salida"
inicio=$(date +%s)
HU_TMP=/tmp/hu_todo_$$ HU_SOLO_COMPILAR=1 bash "$rama/src/pruebas/humano.sh" - || exit 3
bin=/tmp/hu_todo_$$/prueba_humano
"$bin" /tmp/hu_todo_$$ "$salida/humano_general.tsv" general 6 "$semilla" > "$salida/prueba_humano.log" 2> "$salida/prueba_humano.err" &
for ((k = 0; k < lotes; k++)); do
	HU_LOTE="$k/$lotes" "$bin" /tmp/hu_todo_$$ "$salida/humano_cartas_$k.tsv" cartas -1 "$semilla" > "$salida/prueba_humano_cartas_$k.log" 2> "$salida/prueba_humano_cartas_$k.err" &
done
wait
cat "$salida"/humano_*.tsv | python3 -c "
import sys
res = {}
for l in sys.stdin:
    p = l.rstrip('\n').split('\t')
    if len(p) < 2: continue
    if p[0] not in res or p[1] == 'FALLA': res[p[0]] = (p[1], p[2] if len(p) > 2 else '')
ok = sum(1 for v in res.values() if v[0] == 'OK'); mal = [k for k, v in res.items() if v[0] == 'FALLA']; dec = [k for k, v in res.items() if v[0] == 'DECISION']
print('HUMANO: %d casos, OK %d, FALLA %d, DECISION %d' % (len(res), ok, len(mal), len(dec)))
for k in sorted(mal): print('  FALLA %s: %s' % (k, res[k][1][:400]))
for k in sorted(dec): print('  DECISION %s: %s' % (k, res[k][1][:300]))
" > "$salida/resumen.txt"
echo "duracion: $(( $(date +%s) - inicio )) s" >> "$salida/resumen.txt"
cat "$salida/resumen.txt" | head -5
rm -rf /tmp/hu_todo_$$
