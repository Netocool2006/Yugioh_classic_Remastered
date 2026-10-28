# Exporta el personaje animado al .glb que lee el juego (<carta>_anim.glb): una malla con su esqueleto y una animacion
# por clip (reposo, ataque, golpe, defensa, muerte), muestreada a 30 cuadros por segundo. La malla queda en las mismas
# coordenadas que la figura rigida (<carta>.glb): el juego la dibuja con la misma matriz.
# Uso: blender -b -P exportar.py -- final.blend salida.glb
import bpy, sys

a = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.open_mainfile(filepath=a[0])
esc = bpy.context.scene
esc.render.fps = 30
CLIPS = ["reposo", "ataque", "golpe", "defensa", "muerte"]
for act in list(bpy.data.actions):
    if act.name not in CLIPS:
        bpy.data.actions.remove(act)
faltan = [c for c in CLIPS if c not in bpy.data.actions]
if faltan:
    sys.exit("faltan clips: %s" % faltan)
arm = next(o for o in esc.objects if o.type == "ARMATURE")
arm.animation_data_create()
arm.animation_data.action = None
for t in list(arm.animation_data.nla_tracks):
    arm.animation_data.nla_tracks.remove(t)
# cada clip en su pista NLA (asi el exportador hace una animacion por clip, con su nombre)
for c in CLIPS:
    act = bpy.data.actions[c]
    p = arm.animation_data.nla_tracks.new()
    p.name = c
    tira = p.strips.new(c, int(act.frame_range[0]), act)
    if hasattr(tira, "action_slot") and len(getattr(act, "slots", [])):
        tira.action_slot = act.slots[0]
    p.mute = True
bpy.ops.export_scene.gltf(filepath=a[1], export_format="GLB", export_animations=True,
                          export_animation_mode="NLA_TRACKS", export_force_sampling=True, export_frame_step=1,
                          export_skins=True, export_def_bones=True, export_all_influences=False,
                          export_apply=False, export_yup=True, export_image_format="AUTO")
print("YUGI exportado", a[1])
