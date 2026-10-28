#!/usr/bin/env bash
# Copia Yugioh Classic Remaster (la carpeta remake-ps5 de una rama) a su propia plantilla Vulkan dentro del
# contenedor arch-ps5 (/work/PS5_ClassicRemaster, titulo PPSA77002: nunca pisa a Yugi Reload, PPSA77001), aplica
# los arreglos a la base (parche_base.py) y compila los shaders.
# Uso: YUGI_SRC=/compartido/wt/<rama>/remake-ps5 bash sincronizar.sh
set -euo pipefail
# YUGI_SRC: carpeta remake-ps5 a compilar; YUGI_ASSETS: los assets que no van al repo (caras de cartas, fx, bosque)
src=${YUGI_SRC:?YUGI_SRC=/compartido/wt/<rama>/remake-ps5}
assets=${YUGI_ASSETS:-/compartido/classic-assets}
t=/work/PS5_ClassicRemaster
grep -q '"titleId": "PPSA77002"' "$t/ps5/sce_sys/param.json" || { echo "la plantilla $t no es PPSA77002: se detiene"; exit 1; }
# 120 Hz: el driver (PS5_Vulkan/driver/ps5vk_wsi.c) ofrece el modo de 119.88 Hz a 3840x2160 solo si el titulo lo
# declara en param.json (attribute3, bits 0x80040); sin eso la consola lo rechaza y queda 59.94 Hz. La tele tiene que
# aceptar 4K a 120 Hz (HDMI 2.1); si no, VideoOut lo rechaza y el driver vuelve solo a 60 Hz
python3 - "$t/ps5/sce_sys/param.json" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["attribute3"] = int(d.get("attribute3", 0)) | 0x80040
d["localizedParameters"]["en-US"]["titleName"] = "Yugioh Classic Remaster"
json.dump(d, open(p, "w"), indent=2)
PY
cp "$src/src/yugireload.cpp" "$t/examples/yugireload/yugireload.cpp"
# interfaz del duelo (HUD): su codigo y stb_truetype (texto SDF)
cp "$src/src/interfaz.h" "$t/examples/yugireload/interfaz.h"
# el control con el mando: mesa, foco, menus y eventos, sin Vulkan
cp "$src/src/control.h" "$t/examples/yugireload/control.h"
# textos ES/EN de la interfaz e idioma elegido (rama classic-idioma)
if [[ -f "$src/src/idioma.h" ]]; then cp "$src/src/idioma.h" "$t/examples/yugireload/idioma.h"; fi
# rama classic-constructor: coleccion y mazo (mazo.h), la pantalla "Construir mazo" (constructor.h, constructor_ui.h) y
# sus ganchos en el duelo (mazo_duelo.h); rama classic-integracion: el duelo real contra la IA (duelo_juego.h, duelo_ui.h)
cp "$src/src/figuras.h" "$t/examples/yugireload/figuras.h"   # rama classic-figuras
cp "$src/src/orientacion_figura.h" "$t/examples/yugireload/orientacion_figura.h"   # rama classic-bp-terminar: las figuras miran a la pantalla
for h in ataque_md.h ataque_md_juego.h ataque_md_ui.h ataque_fig_fx.h ataque_elevacion_juego.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done   # rama classic-ps5-ataque: ataque directo de MD y rayos de Yugi Reload
if [[ -f "$src/src/front_total_pc.h" ]]; then cp "$src/src/front_total_pc.h" "$t/examples/yugireload/front_total_pc.h"; fi   # rama classic-front-total (prueba de pixeles, solo PC)
for h in mazo.h constructor.h constructor_textos.h constructor_ui.h mazo_duelo.h duelo_juego.h duelo_ui.h menu_principal.h menu_ui.h guardado.h portada.h portada_ui.h opciones.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done
cp "$src/src/duelo_log.h" "$t/examples/yugireload/duelo_log.h"   # rama classic-ps5-duelo-log: la bitacora del duelo (ia.h la incluye al final)
for h in duelo_registro.h duelo_cadena.h duelo_visores.h duelo_consulta.h duelo_consulta_ui.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done   # rama classic-duelo-ui
for h in clips_md.h fases_md.h fases_md_juego.h fases_md_ui.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done   # rama classic-md-fases: Master Duel (selector de fases, engranaje, carteles)
for h in moneda.h moneda_juego.h moneda_ui.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done   # rama classic-moneda: el lanzamiento de moneda
for h in md_ritmo.h md_ritmo_juego.h md_ritmo_ui.h md_ritmo_pc.h clips_md.h md_efectos_ui.h md_cardrun_ui.h efectos3_juego.h equipo_ovni_juego.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done   # rama classic-md-ritmo: el ritmo de Master Duel
for h in ritmo_turno.h ritmo_turno_juego.h ritmo_turno_ui.h pregunta_ui.h lista_cartas_ui.h cpu_muestra.h cpu_muestra_ui.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done   # rama classic-ps5-ritmo: el ritmo del turno
# rama classic-md-ritmo: los movimientos de cartas de Master Duel (herramientas/md_ritmo/exportar_movimientos.py; fuera del repo)
if [[ -d "$assets/md_ritmo" ]]; then mkdir -p "$t/ps5/assets/md_ritmo" && rsync -a --delete "$assets/md_ritmo/" "$t/ps5/assets/md_ritmo/"; fi
# carta de campo Bosque: el entorno de bosque hecho por codigo
cp "$src/src/bosque.h" "$t/examples/yugireload/bosque.h"
# rama classic-entornos: el entorno del personaje rival (fondo, suelo, luz, siluetas y particulas)
cp "$src/src/entornos.h" "$t/examples/yugireload/entornos.h"
# rama classic-campos: las cartas de campo cambian el escenario (capa del tablero y particulas)
cp "$src/src/campos.h" "$t/examples/yugireload/campos.h"
# efectos del Niagara Examples Pack horneados a flipbook: el sistema de billboards
cp "$src/src/fx.h" "$t/examples/yugireload/fx.h"
# rama classic-fx-md: animaciones de activacion de magias/trampas, dado y moneda (efectos_cartas.h)
cp "$src/src/efectos_cartas.h" "$t/examples/yugireload/efectos_cartas.h"
# rama classic-probador: la logica de esas animaciones (sin Vulkan: la usa tambien el probador de PC)
cp "$src/src/efectos_logica.h" "$t/examples/yugireload/efectos_logica.h"
# rama classic-ia: la IA del rival (motor del duelo sin efectos, personalidades y rivales.txt)
cp "$src/src/ia.h" "$t/examples/yugireload/ia.h"
# rama classic-variedad: planes de juego, mazos dinamicos, memoria del jugador y dificultad adaptable
if [[ -f "$src/src/ia_plan.h" ]]; then cp "$src/src/ia_plan.h" "$t/examples/yugireload/ia_plan.h"; fi
# rama classic-ia-humana: busqueda que imagina jugadas y capa humana del rival
if [[ -f "$src/src/ia_humana.h" ]]; then cp "$src/src/ia_humana.h" "$t/examples/yugireload/ia_humana.h"; fi
# rama classic-ia-red: la red de valor que aprende y el ajuste fino al jugador
for h in ia_red.h aprendizaje.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done
# rama classic-personajes: personajes del anime (modo historia) y sus retratos
for h in personajes.h personajes_ui.h historia_dialogos.h historia_dialogos_ui.h tutorial.h tutorial_ui.h; do if [[ -f "$src/src/$h" ]]; then cp "$src/src/$h" "$t/examples/yugireload/$h"; fi; done
# rama classic-resultado: velocidad de animaciones y pantalla de resultado
for h in velocidad.h velocidad_juego.h resultado.h resultado_juego.h resultado_ui.h entrada_clave.h entrada_clave_juego.h entrada_clave_ui.h entrada_md.h entrada_md_juego.h entrada_md_ui.h menu_md_ui.h; do if [[ -f "$src/src/$h" ]]; then cp "$src/src/$h" "$t/examples/yugireload/$h"; fi; done
# rama classic-galeria: galeria de figuras 3D (logica, juego e interfaz)
for h in galeria.h galeria_juego.h galeria_ui.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done
# rama classic-extras: Personalizacion (fundas, tapete, controles, estadisticas)
for h in cosmeticos.h cosmeticos_juego.h cosmeticos_ui.h estadisticas.h; do if [[ -f "$src/src/$h" ]]; then cp "$src/src/$h" "$t/examples/yugireload/$h"; fi; done
# modo en linea (ramas classic-red-*, classic-sala): src/red/*.h -> red/
if [[ -d "$src/src/red" ]]; then rm -rf "$t/examples/yugireload/red"; mkdir -p "$t/examples/yugireload/red"; cp "$src"/src/red/*.h "$t/examples/yugireload/red/"; fi
for h in en_linea.h en_linea_juego.h en_linea_ui.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done   # rama classic-en-linea: el DUELO EN LINEA
for h in raton.h raton_juego.h; do cp "$src/src/$h" "$t/examples/yugireload/$h"; done   # rama classic-pc: el raton (zonas de la interfaz; solo lo lee Windows)
cp "$src/third_party/stb_truetype.h" "$t/examples/yugireload/stb_truetype.h"
# rama classic-audio: el mezclador (audio.h) y el decodificador de OGG (stb_vorbis, dominio publico)
cp "$src/src/audio.h" "$src/third_party/stb_vorbis.c" "$t/examples/yugireload/"
rm -rf "$t/examples/yugireload/reglas" "$t/examples/yugireload/escena.h" "$t/examples/yugireload/prueba_cartesiana.h"
rm -f "$t"/shaders/glsl/yugireload/*
cp "$src"/shaders/*.vert "$src"/shaders/*.frag "$t/shaders/glsl/yugireload/"
for s in "$t"/shaders/glsl/yugireload/*.vert "$t"/shaders/glsl/yugireload/*.frag; do
    glslangValidator -V --target-env vulkan1.3 "$s" -o "$s.spv" > /tmp/glslang.log || { cat /tmp/glslang.log; exit 1; }
done
# arreglos a la base: .glb binarios, fotos y video de la consola a pedido, OPTIONS 2 s, nombre de la ventana
cp "$src/third_party/stb_image_write.h" "$t/ps5/src/stb_image_write.h"
python3 "$src/parche_base.py" "$t"
# el tablero (tapete y casillas), dos primitivas para los destellos (esfera y cilindro) y, rama classic-figuras, las
# figuras 3D de las cartas emblematicas (<assets>/models/yugi/<slug>.glb y <slug>_anim.glb, no van al repo; la tabla
# id -> slug es datos/figuras.txt). rsync: solo copia las que cambiaron
mkdir -p "$t/ps5/assets/models/yugi" "$assets/models/yugi"
rsync -a --delete --include='*.glb' --exclude='*' "$assets/models/yugi/" "$t/ps5/assets/models/yugi/"
cp "$assets"/models/tablero.glb "$assets"/models/esfera.glb "$assets"/models/rayo.glb "$t/ps5/assets/models/yugi/"
# interfaz: caras de las cartas y sus datos (no van al repo) y las fuentes libres (OFL, en el repo)
# las caras son ~3 GB: rsync solo copia las que cambiaron (antes rm + cp tardaba 12 min en cada compilacion)
mkdir -p "$t/ps5/assets/ui/fuentes"
rsync -a --delete --include='*.png' --include='cara_hd_*.jpg' --include='portada.jpg' --include='cartas_ui.txt' --exclude='*' "$assets/ui/" "$t/ps5/assets/ui/"
# caras en HD (cara_hd_<id>.jpg, del agente classic-datos): el juego las prefiere a cara_<id>.png y les arma mipmaps
if [[ -f "$assets/ui/cartas_ui_en.txt" ]]; then cp "$assets/ui/cartas_ui_en.txt" "$t/ps5/assets/ui/"; fi   # textos en ingles (classic-idioma)
if [[ -f "$assets/ui/cartas_classic.txt" ]]; then cp "$assets/ui/cartas_classic.txt" "$t/ps5/assets/ui/"; fi   # limites Goat (classic-constructor)
cp "$src"/third_party/fuentes/* "$t/ps5/assets/ui/fuentes/"
# carta de campo Bosque realista: texturas (png) y modelos reducidos (.malla) CC0 de Poly Haven y ambientCG; los
# prepara bosque_insumos.py (no van al repo). Sin ellos el bosque sale con texturas neutras
# entornos por rival: texturas CC0 de Poly Haven (entornos_insumos.py, no van al repo). Sin ellas, texturas neutras
mkdir -p "$t/ps5/assets/entornos"
if ls "$assets"/entornos/*_color.png > /dev/null 2>&1; then
    # rama classic-escenarios: los fondos pre-renderizados (pano/<clave>.jpg, _prof.png y .txt; escenarios/*.py)
    rsync -a --delete --exclude='pano/*_vista.png' --include='pano/' --include='pano/*.jpg' --include='pano/*_prof.png' --include='pano/*.txt' --include='*.png' --exclude='*'         "$assets/entornos/" "$t/ps5/assets/entornos/"
else
    echo "aviso: faltan las texturas de los entornos en $assets/entornos (correr entornos_insumos.py)"
fi
# rama classic-fondos: un fondo fotorrealista por personaje (fondos/<id>.jpg, _prof.png y .txt: cola_fondos.py y
# fondos_post.py, no van al repo) y su tabla (datos/fondos.txt, en el repo). Sin el fondo de un personaje queda el de su tema
mkdir -p "$t/ps5/assets/fondos" "$assets/fondos"
rsync -a --delete --include='*.jpg' --include='*_prof.png' --include='*.txt' --exclude='*' "$assets/fondos/" "$t/ps5/assets/fondos/"
cp "$src/datos/fondos.txt" "$t/ps5/assets/ui/"
# rama classic-campos: texturas CC0 de Poly Haven del tablero de cada carta de campo (campos_insumos.py, no van al repo)
mkdir -p "$t/ps5/assets/campos"
if ls "$assets"/campos/*_color.png > /dev/null 2>&1; then
    rsync -a --delete --include='*.png' --exclude='*' "$assets/campos/" "$t/ps5/assets/campos/"
else
    echo "aviso: faltan las texturas de las cartas de campo en $assets/campos (correr campos_insumos.py)"
fi
rm -rf "$t/ps5/assets/bosque" && mkdir -p "$t/ps5/assets/bosque"
if ls "$assets"/bosque/*_color.png > /dev/null 2>&1; then
    cp "$assets"/bosque/*.png "$t/ps5/assets/bosque/"
    cp "$assets"/bosque/*.malla "$t/ps5/assets/bosque/" 2> /dev/null || true
else
    echo "aviso: faltan los insumos del bosque en $assets/bosque (correr bosque_insumos.py)"
fi
# efectos del Niagara Examples Pack (Fab) horneados (herramientas/unreal/hornear_niagara.bat, no van al repo): atlas
# png + json. Sin ellos no hay explosiones ni chispas del pack
rm -rf "$t/ps5/assets/fx" && mkdir -p "$t/ps5/assets/fx/pack"
if ls "$assets"/fx/*.json > /dev/null 2>&1; then
    cp "$assets"/fx/*.png "$assets"/fx/*.json "$t/ps5/assets/fx/"
    cp "$assets"/fx/pack/*.png "$assets"/fx/pack/*.json "$t/ps5/assets/fx/pack/" 2> /dev/null || true
    echo "efectos del pack: $(ls "$t"/ps5/assets/fx/*.json "$t"/ps5/assets/fx/pack/*.json 2> /dev/null | wc -l) flipbooks"
else
    echo "aviso: faltan los efectos horneados en $assets/fx (herramientas/unreal/hornear_niagara.bat)"
fi
# rama classic-ia: los 40 rivales y sus 120 mazos (rivales_gen.py)
if [[ -f "$src/datos/rivales.txt" ]]; then cp "$src/datos/rivales.txt" "$t/ps5/assets/ui/"; fi
for f in roles_cartas.txt combos.txt mazos_torneo.txt; do if [[ -f "$src/datos/$f" ]]; then cp "$src/datos/$f" "$t/ps5/assets/ui/"; fi; done   # rama classic-variedad
# rama classic-ia-red: los pesos de la red de valor de la IA humana (entrenar_ia_red.cpp; sin el archivo, juega sin red)
if [[ -f "$src/datos/ia_red.txt" ]]; then cp "$src/datos/ia_red.txt" "$t/ps5/assets/ui/"; fi
# rama classic-red-nat: servidores STUN del modo en linea (nombre, puerto e IP de respaldo; src/red/resolver.h)
if [[ -f "$src/datos/red_stun.txt" ]]; then cp "$src/datos/red_stun.txt" "$t/ps5/assets/ui/"; fi
# rama classic-figuras: id de carta emblematica -> slug de su figura 3D (emblematicas_gen.py)
if [[ -f "$src/datos/figuras.txt" ]]; then cp "$src/datos/figuras.txt" "$t/ps5/assets/ui/"; fi
# rama classic-fx-md: efectos de Master Duel (copia del usuario, herramientas/fx_md, no van al repo): flipbooks horneados
# (fx/md), el dado y la moneda 3D (models/yugi) con sus curvas (fx/md); y la tabla carta -> efecto (en el repo)
cp "$src/datos/efectos_cartas.txt" "$t/ps5/assets/ui/"
md="${YUGI_FX_MD:-$assets/fx_master_duel}"
mkdir -p "$t/ps5/assets/fx/md"
if ls "$md"/horneado/*.json > /dev/null 2>&1; then
    cp "$md"/horneado/*.png "$md"/horneado/*.json "$t/ps5/assets/fx/md/"
    cp "$md"/dado_moneda/dado_md.txt "$md"/dado_moneda/moneda_md.txt "$t/ps5/assets/fx/md/" 2> /dev/null || true
    cp "$md"/dado_moneda/dado_md.glb "$md"/dado_moneda/moneda_md.glb "$t/ps5/assets/models/yugi/" 2> /dev/null || true
    echo "efectos de Master Duel: $(ls "$t"/ps5/assets/fx/md/*.json | wc -l) flipbooks"
else
    echo "aviso: faltan los efectos de Master Duel en $md (herramientas/fx_md): solo la activacion por codigo"
fi
# rama classic-md-fases: los datos de la UI del duelo de Master Duel (herramientas/md_fases/exportar_clips.py, copia del
# usuario, no van al repo): Timeline (.mdtl), prefabs (.mdui) y sus sprites (md_ui.png). Sin ellos: los mismos valores en el codigo
rm -rf "$t/ps5/assets/md_ui" && mkdir -p "$t/ps5/assets/md_ui"
if ls "$assets"/md_ui/*.mdtl > /dev/null 2>&1; then
    cp "$assets"/md_ui/*.mdtl "$assets"/md_ui/*.mdui "$assets"/md_ui/md_ui.png "$assets"/md_ui/md_ui_atlas.txt "$t/ps5/assets/md_ui/"
    # rama classic-md-entradas: los sprites de las entradas de fusion y ritual (herramientas/md_fases/exportar_entradas.py)
    cp "$assets"/md_ui/md_entradas.png "$assets"/md_ui/md_entradas_atlas.txt "$t/ps5/assets/md_ui/" 2> /dev/null || echo "aviso: faltan md_entradas.png (fusion/ritual sin sprites de MD)"
    echo "Master Duel (UI del duelo): $(ls "$t"/ps5/assets/md_ui/*.mdtl | wc -l) timelines"
else
    echo "aviso: faltan los datos de la UI de Master Duel en $assets/md_ui (herramientas/md_fases/exportar_clips.py)"
fi
# rama classic-personajes: los 47 personajes y sus mazos (personajes_gen.py) y los retratos (retratos_post.py, no van
# al repo: <assets>/personajes/retrato_<id>.png)
for f in personajes.txt personajes_rivales.txt dialogos.txt; do if [[ -f "$src/datos/$f" ]]; then cp "$src/datos/$f" "$t/ps5/assets/ui/"; fi; done
mkdir -p "$t/ps5/assets/personajes" "$assets/personajes"
rsync -a --delete --include='retrato_*.png' --exclude='*' "$assets/personajes/" "$t/ps5/assets/personajes/"
# rama classic-audio: musica (ogg) y efectos (wav) + audio.txt, sacados de la copia de Master Duel del usuario
# (herramientas/audio, no van al repo). YUGI_AUDIO: su carpeta; sin ella el juego queda mudo
audio=${YUGI_AUDIO:-/compartido/classic-assets/audio}
[[ -f "$audio/audio.txt" ]] || audio="$assets/audio"
rm -rf "$t/ps5/assets/audio" && mkdir -p "$t/ps5/assets/audio"
if [[ -f "$audio/audio.txt" ]]; then
    cp "$audio"/audio.txt "$t/ps5/assets/audio/"
    cp "$audio"/*.wav "$audio"/*.ogg "$t/ps5/assets/audio/" 2> /dev/null || true
    echo "audio: $(ls "$t"/ps5/assets/audio | wc -l) archivos ($(du -sh "$t/ps5/assets/audio" | cut -f1))"
else
    echo "# sin audio" > "$t/ps5/assets/audio/audio.txt"
    echo "aviso: falta el audio en $audio (el juego queda mudo)"
fi
python3 - "$t/ps5/assets.json" <<'PY'
import json, sys
p = sys.argv[1]
d = json.load(open(p))
d["assets"] = [a for a in d["assets"] if a["path"] not in ("models/ps5/lantern", "models/yugi", "ui", "bosque", "fx", "personajes", "audio", "entornos", "fondos", "campos", "md_ritmo", "md_ui")]
d["assets"].append({"path": "personajes", "from": "file", "source": "ps5/assets/personajes",
    "used_by": ["yugireload"], "title": "Retratos de los personajes rivales (modo historia)",
    "author": "Yugi Reload (uso personal)", "licence": "Uso personal", "url": "-"})
d["assets"].append({"path": "audio", "from": "file", "source": "ps5/assets/audio",
    "used_by": ["yugireload"], "title": "Musica y efectos de sonido del duelo",
    "author": "Konami (Yu-Gi-Oh! Master Duel, copia del usuario)", "licence": "Uso personal, sin distribucion",
    "url": "-"})
d["assets"].append({"path": "fx", "from": "file", "source": "ps5/assets/fx",
    "used_by": ["yugireload"], "title": "Efectos del Niagara Examples Pack horneados a flipbook (explosiones, chispas...)",
    "author": "Epic Games (Niagara Examples Pack, Fab); horneado: Yugi Reload", "licence": "Fab Standard License",
    "url": "https://www.fab.com/"})
d["assets"].append({"path": "fondos", "from": "file", "source": "ps5/assets/fondos",
    "used_by": ["yugireload"], "title": "Fondos fotorrealistas por personaje (matte paintings generados con Gemini, escalados con Real-ESRGAN)",
    "author": "Yugi Reload (generados con Gemini); pruebas: Poly Haven (CC0)", "licence": "Uso personal", "url": "-"})
d["assets"].append({"path": "entornos", "from": "file", "source": "ps5/assets/entornos",
    "used_by": ["yugireload"], "title": "Entornos por rival: piedra, adoquin, arenisca, arena, metal, marmol, ladrillo, hormigon, roca, madera...",
    "author": "Poly Haven (Rob Tuytel, Dimitrios Savva y otros)", "licence": "CC0-1.0", "url": "https://polyhaven.com/"})
d["assets"].append({"path": "campos", "from": "file", "source": "ps5/assets/campos",
    "used_by": ["yugireload"], "title": "Tablero de las cartas de campo: cesped, roca, tierra agrietada, pizarra, arena, losas, marmol, metal...",
    "author": "Poly Haven (Rob Tuytel, Dimitrios Savva y otros)", "licence": "CC0-1.0", "url": "https://polyhaven.com/"})
d["assets"].append({"path": "bosque", "from": "file", "source": "ps5/assets/bosque",
    "used_by": ["yugireload"], "title": "Bosque de la carta de campo: corteza, suelo, follaje y modelos escaneados",
    "author": "Poly Haven (Rob Tuytel, Rico Cilliers y otros) y ambientCG (Lennart Demes)", "licence": "CC0-1.0",
    "url": "https://polyhaven.com/ ; https://ambientcg.com/"})
d["assets"].append({"path": "ui", "from": "file", "source": "ps5/assets/ui",
    "used_by": ["yugireload"], "title": "Interfaz del duelo: caras de cartas, datos y fuentes (Chakra Petch, Rajdhani)",
    "author": "Yugi Reload; fuentes: The Chakra Petch Project Authors, Indian Type Foundry", "licence": "Uso personal; fuentes OFL-1.1",
    "url": "https://github.com/google/fonts/tree/main/ofl"})
d["assets"].append({"path": "models/yugi", "from": "file", "source": "ps5/assets/models/yugi",
    "used_by": ["yugireload"], "title": "Tablero de Yugioh Classic Remaster y figuras 3D de las cartas emblematicas",
    "author": "Yugi Reload (proyecto personal: Blender)", "licence": "Uso personal",
    "url": "-"})
import os
for carpeta, titulo in (("md_ritmo", "Movimientos de cartas de Master Duel (BezierMotionSetting exportados)"),
                       ("md_ui", "UI del duelo de Master Duel: Timelines, prefabs y sprites exportados")):   # (ramas classic-md-ritmo / md-fases)
    if os.path.isdir(os.path.join(os.path.dirname(p), "assets", carpeta)):
        d["assets"].append({"path": carpeta, "from": "file", "source": "ps5/assets/" + carpeta, "used_by": ["yugireload"], "title": titulo,
            "author": "Konami (Yu-Gi-Oh! Master Duel, copia del usuario)", "licence": "Uso personal, sin distribucion", "url": "-"})
json.dump(d, open(p, "w"), indent=2, ensure_ascii=False)
PY
for f in icon0.png pic0.png; do   # icono 512x512 y fondo 1920x1080 del titulo (no van al repo)
	if [[ -f "$assets/sce_sys/$f" ]]; then cp "$assets/sce_sys/$f" "$t/ps5/sce_sys/$f"; fi
done
echo "sincronizado en $t"
