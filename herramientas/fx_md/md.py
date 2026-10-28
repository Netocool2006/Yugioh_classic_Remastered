# utilidades: abrir un archivo (bundle o data.unity3d) y tener los objetos por (archivo, path_id)
import UnityPy
def abrir(ruta):
    env=UnityPy.load(ruta)
    return env
def por_archivo(env):
    d={}
    for o in env.objects:
        d.setdefault(o.assets_file.name,{})[o.path_id]=o
    return d
def nombre(o):
    try: return o.peek_name()
    except Exception: return '?'
