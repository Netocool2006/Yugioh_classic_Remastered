#!/usr/bin/env bash
# Hornea los efectos de Master Duel elegidos (volcados con volcar_fx.py / lote.py en $W) a flipbooks en $H
set -e
W=${W:-/d/tools/fx_md/work}; H=${H:-/d/Docker/compartido/classic-assets/fx_master_duel/horneado}
S="/d/tools/venv_unitypy/Scripts/python.exe -I $(dirname $0)/sim_fx.py"
mkdir -p $H $W/prev; cd $W
$S d_fxp_bff_apply_001 $H/md_activar --gris --previa prev/f_activar.png
$S d_fxp_cardbrk_bff_001 $H/md_destruir --previa prev/f_destruir.png
# rama classic-md-combate: la destruida en combate (CardRoot.ReqBrokenEffect). Volcado antes con:
#   volcar_fx.py data.unity3d 7130 d_fxp_cardbrk_btl_001   (fxp_cardbrk_btl_001, resources de masterduel_Data)
$S d_fxp_cardbrk_btl_001 $H/md_destruir_batalla --previa prev/f_destruir_batalla.png
$S prueba_mgctrp $H/md_colocar --previa prev/f_colocar.png
$S md/dark_hole $H/md_agujero_negro --frente --timeline md/dark_hole/timeline.json --previa prev/f_agujero.png
$S md/knives $H/md_cuchillos --frente --timeline md/knives/timeline.json --dur 1.3 --previa prev/f_cuchillos.png
$S md/feather $H/md_plumero --frente --timeline md/feather/timeline.json --previa prev/f_plumero.png
$S md/c4909 $H/md_tornado --frente --timeline md/c4909/timeline.json --dur 0.9 --previa prev/f_tornado.png
$S md/fusion_s1 $H/md_fusion --previa prev/f_fusion.png
$S md/fusion_s1imp $H/md_fusion_marco --previa prev/f_fusion_marco.png
$S md/ritual_s1 $H/md_ritual --previa prev/f_ritual.png
# el tornado se apoya en la mesa: origen abajo al centro
python -c "
import json;f=r'$(cygpath -w $H)/md_tornado.json';d=json.load(open(f));c=d['caja'];d['origen']=[(c[0]+c[2])/2,c[3]];json.dump(d,open(f,'w'),indent=1)"
# el agujero negro va plano sobre la mesa (se traga el campo): vista "arriba" para fx.h
python -c "
import json;f=r'$(cygpath -w $H)/md_agujero_negro.json';d=json.load(open(f));d['vista']='arriba';json.dump(d,open(f,'w'),indent=1)"
