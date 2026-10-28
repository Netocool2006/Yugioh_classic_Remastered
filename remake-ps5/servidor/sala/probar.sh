#!/usr/bin/env bash
# Pruebas de la sala dentro del contenedor arch-ps5 (Node en /compartido/node, nada en el sistema):
#   bash probar.sh [binario de prueba_sala]
# Levanta el Worker real en local (`wrangler dev`, workerd), corre pruebas/prueba_sala.mjs (el Durable Object en
# memoria y el Worker por HTTP) y, si se pasa, el cliente C++ (prueba_sala.cpp) contra ese Worker. Apaga el Worker al
# final. Devuelve 0 solo sin fallas.
# (rama classic-arreglo-ciclo-md, ciclo md_ciclo1: 7 CU-RED-SALA-CLIENTE-* fallaron porque el Worker local no estaba
# escuchando cuando corrieron las pruebas: wrangler tardo mas de 90 s en arrancar con el contenedor cargado, o el puerto
# fijo 8795 lo usaba otro ciclo que corria a la vez (y su pkill apagaba el Worker de este). Es ENTORNO, no codigo: ahora
# se busca un puerto libre desde SALA_PUERTO, se espera hasta SALA_ESPERA s (240) a que el Worker conteste 200 y, si no
# arranca, sale una sola falla clara (CU-RED-SALA-ENTORNO) con el final del log de wrangler en vez de las del cliente)
set -uo pipefail
cd "$(dirname "$0")"
export PATH=${NODE_DIR:-/compartido/node/node-v22.11.0-linux-x64}/bin:$PATH WRANGLER_SEND_METRICS=false
command -v node > /dev/null || { echo "FALLA: no hay Node (descomprimir node-v22 en /compartido/node)"; exit 1; }
[[ -d node_modules/wrangler ]] || npm install --no-audit --no-fund > /tmp/sala_npm.log 2>&1 || { cat /tmp/sala_npm.log; exit 1; }
ocupado() { (exec 3<> "/dev/tcp/127.0.0.1/$1") 2> /dev/null; }
puerto=${SALA_PUERTO:-8787}
for _ in $(seq 40); do ocupado "$puerto" || break; puerto=$((puerto + 1)); done
espera=${SALA_ESPERA:-240}
log=/tmp/sala_wrangler_$puerto.log
npx wrangler dev --ip 127.0.0.1 --port "$puerto" > "$log" 2>&1 &
pid=$!
apagar() {
	kill $pid 2> /dev/null
	pkill -f "workerd.*$puerto" 2> /dev/null
	wait $pid 2> /dev/null
}
listo=0
t0=$(date +%s)
while (( $(date +%s) - t0 < espera )); do
	if curl -sf -m 3 "http://127.0.0.1:$puerto/" > /dev/null; then listo=1; break; fi
	kill -0 $pid 2> /dev/null || break   # (wrangler se cayo)
	sleep 1
done
echo "  (Worker local en el puerto $puerto: $([[ $listo -eq 1 ]] && echo "listo en $(( $(date +%s) - t0 )) s" || echo "NO arranco"))"
if [[ $listo -ne 1 ]]; then
	echo "FALLA CU-RED-SALA-ENTORNO: el Worker local (wrangler dev) no contesto en $espera s en el puerto $puerto (entorno: memoria del contenedor o wrangler caido; no es el codigo del cliente)"
	tail -15 "$log" | sed 's/^/    /'
	node pruebas/prueba_sala.mjs   # (la bateria en memoria igual se prueba)
	apagar
	exit 1
fi
node pruebas/prueba_sala.mjs "http://127.0.0.1:$puerto"
rc=$?
if [[ $# -ge 1 ]]; then
	"$1" "http://127.0.0.1:$puerto"
	rc2=$?
	[[ $rc -eq 0 ]] && rc=$rc2
fi
apagar
exit $rc
