#!/usr/bin/env bash
# UniRig (VAST-AI-Research, "One Model to Rig Them All", licencia MIT; pesos en HF VAST-AI/UniRig, MIT) en Google Colab
# con GPU L4: esqueleto + pesos automaticos para cada /content/salida/*.glb (los modelos de TRELLIS ya limpios).
# Deja, junto a cada modelo:  <nombre>_skel.fbx (esqueleto), <nombre>_skin.fbx (esqueleto+pesos) y <nombre>_rig.glb
# (malla ORIGINAL de TRELLIS con su textura + esqueleto + pesos: lo que hay que bajar). Registro en /content/unirig.log.
# Uso en una celda:   !bash /content/unirig_colab.sh  > /content/unirig.log 2>&1 ; tail -30 /content/unirig.log
# Opcional: SEMILLA=42 bash unirig_colab.sh  (otra variante del esqueleto si el primero sale raro)
# Requisitos de UniRig: Python 3.11 (bpy==4.2 solo existe para 3.11), CUDA, >8 GB de VRAM (L4: 24 GB). ~5-8 min de
# instalacion la primera vez; ~1 min por modelo.
set -euo pipefail
ENTRADA=${ENTRADA:-/content/salida}
SEMILLA=${SEMILLA:-12345}
cd /content
# librerias del sistema que pide bpy (Blender como modulo) sin pantalla
apt-get -qq update > /dev/null && apt-get -qq install -y libxrender1 libxi6 libxkbcommon0 libsm6 libgl1 libxxf86vm1 libxfixes3 > /dev/null || true
pip install -q uv
export UV_PYTHON_PREFERENCE=managed UV_PYTHON_DOWNLOADS=automatic
unset UV_SYSTEM_PYTHON PIP_TARGET || true
uv python install 3.11
[ -x /content/ur/bin/python ] || uv venv -q -p cpython-3.11 /content/ur
P="--python /content/ur/bin/python"
uv pip install $P -q torch==2.5.1 torchvision==0.20.1 --index-url https://download.pytorch.org/whl/cu124 --extra-index-url https://pypi.org/simple --index-strategy unsafe-best-match
# flash-attn compilado (compilarlo en Colab tarda > 1 h): rueda oficial para py3.11 + torch 2.5 + CUDA 12
uv pip install $P -q "https://github.com/Dao-AILab/flash-attention/releases/download/v2.7.0.post2/flash_attn-2.7.0.post2+cu12torch2.5cxx11abiFALSE-cp311-cp311-linux_x86_64.whl"
[ -d UniRig ] || git clone -q https://github.com/VAST-AI-Research/UniRig
cd /content/UniRig
# requirements.txt sin flash_attn (ya instalado arriba)
grep -v '^flash_attn' requirements.txt > /tmp/req_unirig.txt
uv pip install $P -q -r /tmp/req_unirig.txt
uv pip install $P -q spconv-cu120
uv pip install $P -q torch_scatter torch_cluster -f https://data.pyg.org/whl/torch-2.5.1+cu124.html
uv pip install $P -q numpy==1.26.4
/content/ur/bin/python -c 'import torch, bpy, spconv, flash_attn; print("UNIRIG_ENTORNO_OK", torch.__version__, torch.cuda.get_device_name(0), bpy.app.version_string)'
# los scripts de UniRig llaman a "python": que sea el del entorno 3.11
export PATH=/content/ur/bin:$PATH
export VIRTUAL_ENV=/content/ur
shopt -s nullglob
modelos=("$ENTRADA"/*.glb)
[ ${#modelos[@]} -gt 0 ] || { echo "NO HAY .glb en $ENTRADA"; exit 1; }
for g in "${modelos[@]}"; do
	case "$g" in *_rig.glb) continue ;; esac
	n=$(basename "$g" .glb)
	echo "=== $n ($(date +%H:%M:%S))"
	bash launch/inference/generate_skeleton.sh --input "$g" --output "$ENTRADA/${n}_skel.fbx" --seed "$SEMILLA"
	bash launch/inference/generate_skin.sh --input "$ENTRADA/${n}_skel.fbx" --output "$ENTRADA/${n}_skin.fbx"
	bash launch/inference/merge.sh --source "$ENTRADA/${n}_skin.fbx" --target "$g" --output "$ENTRADA/${n}_rig.glb"
	ls -la "$ENTRADA/${n}_rig.glb" && echo "UNIRIG_LISTO $n"
done
echo "UNIRIG_FIN: bajar $ENTRADA/*_rig.glb (y *_skin.fbx de respaldo)"
