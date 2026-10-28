#!/usr/bin/env bash
# Ciclo COMPLETO de pruebas (rama classic-ciclos), desde cero y sin nada diferencial: compila el juego (probar_rama.sh,
# PS5 y PC), borra y recompila cada prueba y la corre (en serie: el contenedor tiene 10 GB), corre la certificacion de
# efectos y los casos de uso (prueba_casos_uso.cpp) y junta todo en un resumen: casos totales, OK, FALLA, DECISION y la
# lista de fallas. Dentro del contenedor arch-ps5:
#   bash /compartido/wt/<rama>/remake-ps5/src/pruebas/ciclo_completo.sh [carpeta de salida]
# Salida (por defecto /compartido/wt/ciclos_logs/<fecha>): un .log por prueba, casos_uso.tsv, resumen.txt y
# resultados.tsv (todos los casos: id, estado, detalle). Devuelve 0 solo con 0 fallas.
set -uo pipefail
rama=$(cd "$(dirname "$0")/../.." && pwd)
assets=${YUGI_ASSETS:-/compartido/classic-assets}
salida=${1:-/compartido/wt/ciclos_logs/$(date +%Y%m%d_%H%M%S)}
# (un tmp propio por corrida: dos ramas pueden correr su ciclo a la vez; se borra al final)
tmp=/tmp/ciclo_$(basename "$salida")_$$
bin=$tmp/bin
export CERT_TMP=$tmp/cert CERT_BIN=$tmp/bin/prueba_certificacion
mkdir -p "$salida"
rm -rf "$tmp"
mkdir -p "$tmp/ui" "$tmp/bin"
inicio=$(date +%s)
echo "ciclo completo: $rama -> $salida ($(date '+%F %T'))"
: > "$salida/suites.tsv"

suite() {   # suite <nombre> <rc> <log>: una prueba entera es un caso (CU-PRUEBA-<nombre>)
	local nombre=$1 rc=$2 log=$3 fallos
	fallos=$(grep -cE "^FALLO|FALLA |^  FALLO|PRUEBA CON FALLOS|^  MAL " "$log" 2>/dev/null || true)
	if [[ $rc -eq 0 ]]; then
		printf "CU-PRUEBA-%s\tOK\t%s\n" "$nombre" "$(tail -1 "$log" | tr '\t' ' ' | cut -c1-200)" >> "$salida/suites.tsv"
	else
		printf "CU-PRUEBA-%s\tFALLA\trc %s, %s lineas de falla: %s\n" "$nombre" "$rc" "$fallos" "$(grep -m3 -E "FALLO|FALLA|MAL |error" "$log" | tr '\t\n' '  ' | cut -c1-300)" >> "$salida/suites.tsv"
	fi
	echo "  $nombre: rc $rc ($(( $(date +%s) - inicio )) s)"
}

# 1) el juego (PS5 y vista previa de PC)
bash "$rama/probar_rama.sh" "$rama" - 0 > "$salida/compilar.log" 2>&1
suite COMPILAR-JUEGO $? "$salida/compilar.log"
# rama classic-ps5-efectos: los sprites de Master Duel que pide la interfaz estan en el titulo y ninguno es una mascara
# dibujada como imagen (la "barra amarilla" sobre las cartas activas)
python3 "$rama/src/pruebas/prueba_sprites_md.py" "$rama/src" "${YUGI_TITULO_ASSETS:-/compartido/ps5/PPSA77002/assets}" > "$salida/prueba_sprites_md.log" 2>&1
suite SPRITES-MD $? "$salida/prueba_sprites_md.log"

# 2) los datos de las pruebas: las cartas (assets) y los datos de la rama, en una carpeta nueva
cp "$assets"/ui/cartas_ui.txt "$assets"/ui/cartas_ui_en.txt "$assets"/ui/cartas_classic.txt "$tmp/ui/"
cp "$rama"/datos/*.txt "$tmp/ui/"
ui="$tmp/ui"
base=/work/PS5_ClassicRemaster/external
cd "$tmp"

compilar() {   # compilar <nombre> <comando...>
	local nombre=$1
	shift
	rm -f "$bin/$nombre"
	"$@" > "$salida/$nombre.compilar.log" 2>&1
	local rc=$?
	if [[ $rc -ne 0 ]]; then
		cp "$salida/$nombre.compilar.log" "$salida/$nombre.log"
		suite "$nombre" "$rc" "$salida/$nombre.log"
		return 1
	fi
	return 0
}
correr() {   # correr <nombre> <args...>
	local nombre=$1
	shift
	"$bin/$nombre" "$@" > "$salida/$nombre.log" 2>&1
	suite "$nombre" $? "$salida/$nombre.log"
}

# 3) las pruebas de siempre, cada una compilada de nuevo
G="g++ -std=c++17 -O2 -I $rama/src"
compilar prueba_mando $G "$rama/src/pruebas/prueba_mando.cpp" -o "$bin/prueba_mando" && correr prueba_mando
compilar prueba_constructor $G "$rama/src/pruebas/prueba_constructor.cpp" -o "$bin/prueba_constructor" && correr prueba_constructor "$ui/"
compilar prueba_juego $G "$rama/src/pruebas/prueba_juego.cpp" -o "$bin/prueba_juego" && correr prueba_juego "$ui/"
compilar prueba_duelo $G "$rama/src/pruebas/prueba_duelo.cpp" -o "$bin/prueba_duelo" && correr prueba_duelo "$ui/" 40
compilar prueba_efectos $G "$rama/src/pruebas/prueba_efectos.cpp" -o "$bin/prueba_efectos" && correr prueba_efectos "$ui/"
compilar prueba_ia $G "$rama/src/pruebas/prueba_ia.cpp" -o "$bin/prueba_ia" && correr prueba_ia "$ui/" "$rama/datos/rivales.txt"
compilar prueba_variedad $G "$rama/src/pruebas/prueba_variedad.cpp" -o "$bin/prueba_variedad" && correr prueba_variedad "$ui/" "$rama/datos/"
compilar prueba_ia_humana $G "$rama/src/pruebas/prueba_ia_humana.cpp" -o "$bin/prueba_ia_humana" && correr prueba_ia_humana "$ui/" "$rama/datos/" 20
compilar prueba_ia_red $G "$rama/src/pruebas/prueba_ia_red.cpp" -o "$bin/prueba_ia_red" && correr prueba_ia_red "$ui/" "$rama/datos/" 12 "$tmp/ia_red"   # (rama classic-ia-red: la red que aprende)
compilar prueba_torneo $G "$rama/src/pruebas/prueba_torneo.cpp" -o "$bin/prueba_torneo" && correr prueba_torneo "$ui/" "$rama/datos/" 20 1   # (rama classic-mazos-torneo)
compilar prueba_red $G "$rama/src/pruebas/prueba_red.cpp" -o "$bin/prueba_red" && correr prueba_red "$ui/" 200   # (rama classic-red-base: modo en linea)
# rama classic-sala: la busqueda de rival en linea (bencode/KRPC, mini-red DHT, ELO) y el cliente C++ contra el Worker de
# Cloudflare levantado en local (servidor/sala/probar.sh: wrangler dev + prueba_sala.mjs; Node en /compartido/node)
compilar prueba_sala $G -pthread "$rama/src/pruebas/prueba_sala.cpp" -o "$bin/prueba_sala" &&
	{ SALA_PUERTO=8795 bash "$rama/servidor/sala/probar.sh" "$bin/prueba_sala" > "$salida/prueba_sala.log" 2>&1; suite prueba_sala $? "$salida/prueba_sala.log"; }
compilar prueba_red_nat $G "$rama/src/pruebas/prueba_red_nat.cpp" -o "$bin/prueba_red_nat" -lpthread && correr prueba_red_nat   # (rama classic-red-nat: STUN, UPnP, codigo y NAT simulados)
compilar prueba_audio clang++ -std=c++17 -O1 -I "$rama/src" -I "$rama/third_party" "$rama/src/pruebas/prueba_audio.cpp" -o "$bin/prueba_audio" &&
	correr prueba_audio "$assets/audio/" "$tmp/mezcla.wav" 4

# 4) la certificacion de efectos (binario nuevo; escribe docs/certificacion_efectos.md/.csv de la rama)
rm -f "$CERT_BIN"
rm -rf "$CERT_TMP"
bash "$rama/src/pruebas/certificar.sh" > "$salida/certificacion.log" 2>&1
rc=$?
cp "$CERT_TMP/certificacion.log" "$salida/certificacion_detalle.log" 2>/dev/null
grep -q "SIN PRUEBA 0" "$salida/certificacion.log" || [[ $rc -ne 0 ]] || rc=1
suite CERTIFICACION $rc "$salida/certificacion.log"

# 5) los casos de uso (front, back y carta por carta, con la certificacion de este ciclo)
if compilar prueba_casos_uso g++ -std=c++17 -O1 -DPS5_HOST_REFERENCE -I "$rama/src" -I "$base/tinygltf" -I "$base/glm" "$rama/src/pruebas/prueba_casos_uso.cpp" -o "$bin/prueba_casos_uso"; then
	"$bin/prueba_casos_uso" "$tmp" "$assets/ui" "$assets/models/yugi" "$rama/docs/certificacion_efectos.csv" "$rama/src" "$salida/casos_uso.tsv" > "$salida/prueba_casos_uso.log" 2> "$salida/prueba_casos_uso.err"
	suite CASOS-DE-USO $? "$salida/prueba_casos_uso.log"
fi

# 5b) rama classic-front-total: la bateria DESDE EL FRONT (cu_front_total.h): duelos completos con el mando contra cada
# rival y carta por carta, y la prueba de pixeles de la mesa en la vista previa del PC (captura_front_total.sh)
if compilar prueba_front_total g++ -std=c++17 -O2 -DPS5_HOST_REFERENCE -I "$rama/src" -I "$base/tinygltf" -I "$base/glm" "$rama/src/pruebas/prueba_front_total.cpp" -o "$bin/prueba_front_total"; then
	"$bin/prueba_front_total" "$tmp" "$salida/front_total.tsv" todo > "$salida/prueba_front_total.log" 2> "$salida/prueba_front_total.err"
	suite FRONT-TOTAL $? "$salida/prueba_front_total.log"
fi
# 5c) rama classic-en-linea: el DUELO EN LINEA desde el front: dos juegos sin GPU, cada uno con su bot de mando, por UDP
# en loopback (red local, codigo, DHT de una mini red, moneda, espejo, reloj, caidas, revancha, rendicion; CU-RED-FE-*)
if compilar prueba_red_front g++ -std=c++17 -O2 -DPS5_HOST_REFERENCE -pthread -I "$rama/src" -I "$base/tinygltf" -I "$base/glm" "$rama/src/pruebas/prueba_red_front.cpp" -o "$bin/prueba_red_front"; then
	"$bin/prueba_red_front" "$tmp" "$salida/red_front.tsv" > "$salida/prueba_red_front.log" 2>&1
	suite prueba_red_front $? "$salida/prueba_red_front.log"
fi
# 5d) rama classic-jugador-humano: el JUGADOR HUMANO (prueba_humano.cpp, docs/jugador_humano.md): juega mirando solo lo
# que la interfaz de verdad dibuja (sin GPU), con ritmo humano y travesuras; los oraculos CU-HU-* (enterarse, ventana,
# nunca trancado, mesa coherente, controles), los casos fijos, duelos completos y carta por carta (CU-HU-CARTA-<id>, en
# HU_LOTES procesos a la vez: poca RAM cada uno, ~3 MB de pico)
hu_lotes=${HU_LOTES:-4}
mkdir -p "$tmp/hu/ui/fuentes" "$tmp/hu/md_ui"
cp "$ui"/*.txt "$tmp/hu/ui/" && cp "$rama"/third_party/fuentes/*.ttf "$tmp/hu/ui/fuentes/"
[[ -d "$assets/md_ritmo" ]] && cp -r "$assets/md_ritmo" "$tmp/hu/"
cp "$assets"/md_ui/*.mdui "$assets"/md_ui/*.mdtl "$tmp/hu/md_ui/" 2> /dev/null
if compilar prueba_humano g++ -std=c++17 -O2 -DPS5_HOST_REFERENCE -I "$rama/src" -I "$rama/third_party" -I /work/PS5_ClassicRemaster/base -I "$base/tinygltf" -I "$base/glm" \
	"$rama/src/pruebas/prueba_humano.cpp" -o "$bin/prueba_humano" -lvulkan -lpthread; then
	"$bin/prueba_humano" "$tmp/hu" "$salida/humano_general.tsv" general 6 1 > "$salida/prueba_humano.log" 2> "$salida/prueba_humano.err" &
	for ((k = 0; k < hu_lotes; k++)); do
		HU_LOTE="$k/$hu_lotes" "$bin/prueba_humano" "$tmp/hu" "$salida/humano_cartas_$k.tsv" cartas -1 1 > "$salida/prueba_humano_cartas_$k.log" 2> "$salida/prueba_humano_cartas_$k.err" &
	done
	rc=0
	for p in $(jobs -p); do wait "$p" || rc=1; done
	cat "$salida"/prueba_humano.log "$salida"/prueba_humano_cartas_*.log > "$salida/humano.log"
	suite HUMANO $rc "$salida/humano.log"
fi
bash "$rama/captura_front_total.sh" "$rama" "$salida/pixeles" "$salida/pixeles.tsv" > "$salida/pixeles.log" 2>&1
suite PIXELES $(grep -q "	FALLA	" "$salida/pixeles.tsv" 2>/dev/null && echo 1 || echo 0) "$salida/pixeles.log"

# 6) el resumen: casos de uso + pruebas enteras + hojas del mapa que cubren las pruebas de siempre + cobertura del mapa
python3 - "$rama" "$salida" <<'PY'
import re, sys, os
rama, salida = sys.argv[1], sys.argv[2]
res = {}
orden = []
def poner(i, e, d):
    if i not in res: orden.append(i)
    if i in res and res[i][0] == "FALLA": return
    res[i] = (e, d)
import glob
for archivo in ["casos_uso.tsv", "front_total.tsv", "pixeles.tsv", "red_front.tsv"] + sorted(os.path.basename(x) for x in glob.glob(os.path.join(salida, "humano_*.tsv"))) + ["suites.tsv"]:   # (front_total y pixeles: rama classic-front-total; red_front: classic-en-linea)
    ruta = os.path.join(salida, archivo)
    if os.path.exists(ruta):
        for l in open(ruta, encoding="utf-8", errors="replace"):
            p = l.rstrip("\n").split("\t")
            if len(p) >= 2: poner(p[0], p[1], p[2] if len(p) > 2 else "")
# hojas cubiertas por una prueba de siempre (src/pruebas/casos_suites.txt: ID|prueba)
for l in open(os.path.join(rama, "src/pruebas/casos_suites.txt"), encoding="utf-8"):
    l = l.strip()
    if not l or l.startswith("#"): continue
    i, s = l.split("|")[:2]
    r = res.get("CU-PRUEBA-" + s)
    if r is None: poner(i, "FALLA", "la prueba " + s + " no corrio")
    else: poner(i, r[0], ("por " + s + ": " + r[1])[:200])
# el mapa: cada hoja (- `CU-...`) tiene que tener su prueba
mapa = open(os.path.join(rama, "docs/casos_de_uso.md"), encoding="utf-8").read()
hojas = re.findall(r"^\s*- `(CU-[A-Z0-9-]+)`", mapa, re.M)
for h in hojas:
    if h not in res: poner(h, "FALLA", "hoja del mapa sin prueba automatica")
ok = sum(1 for i in orden if res[i][0] == "OK")
mal = [i for i in orden if res[i][0] == "FALLA"]
dec = [i for i in orden if res[i][0] == "DECISION"]
niveles = {}
for i in orden:
    k = "CU-CARTA" if i.startswith("CU-CARTA-") else "CU-PRUEBA" if i.startswith("CU-PRUEBA-") else "CU-FT-CARTA" if i.startswith("CU-FT-CARTA-") else "CU-FT-PIXEL" if i.startswith("CU-FT-PIXEL-") else "CU-HU-CARTA" if i.startswith("CU-HU-CARTA-") else "-".join(i.split("-")[:2])
    niveles[k] = niveles.get(k, 0) + 1
with open(os.path.join(salida, "resultados.tsv"), "w", encoding="utf-8") as f:
    for i in orden: f.write("%s\t%s\t%s\n" % (i, res[i][0], res[i][1]))
with open(os.path.join(salida, "resumen.txt"), "w", encoding="utf-8") as f:
    f.write("CICLO: %d casos, OK %d, FALLA %d, DECISION %d (excluidas del conteo de fallas)\n" % (len(orden), ok, len(mal), len(dec)))
    f.write("hojas del mapa: %d; por grupo: %s\n" % (len(hojas), ", ".join("%s %d" % kv for kv in sorted(niveles.items()))))
    for i in mal: f.write("  FALLA %s: %s\n" % (i, res[i][1][:300]))
    for i in dec: f.write("  DECISION %s: %s\n" % (i, res[i][1][:300]))
print(open(os.path.join(salida, "resumen.txt"), encoding="utf-8").read())
sys.exit(1 if mal else 0)
PY
rc=$?
cd / && rm -rf "$tmp"
echo "duracion: $(( $(date +%s) - inicio )) s"
exit $rc
