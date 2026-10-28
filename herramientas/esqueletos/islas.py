# Islas (piezas conectadas) de un .glb: cantidad de vertices y caja (glTF) de las mas grandes. Sirve para encontrar
# armas o accesorios que vienen como pieza aparte (se pueden volver rigidos enteros en rig_auto.py: "isla").
# Uso: python -I islas.py modelo.glb [n=12]
import sys
import os
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from glb_puntos import leer, accesor

j, b = leer(sys.argv[1])
pr = j["meshes"][0]["primitives"][0]
P = accesor(j, b, pr["attributes"]["POSITION"])
I = accesor(j, b, pr["indices"]).reshape(-1)
# vertices con la misma posicion = el mismo (las costuras de UV duplican vertices)
_, uniq = np.unique(np.round(P, 6), axis=0, return_inverse=True)
uniq = uniq.reshape(-1)
padre = np.arange(uniq.max() + 1)

def raiz(x):
    while padre[x] != x:
        padre[x] = padre[padre[x]]
        x = padre[x]
    return x

T = uniq[I].reshape(-1, 3)
for a_, b_, c_ in T:
    ra, rb, rc = raiz(a_), raiz(b_), raiz(c_)
    padre[rb] = ra
    padre[raiz(rc)] = ra
r = np.array([raiz(x) for x in uniq])
ids, cnt = np.unique(r, return_counts=True)
orden = np.argsort(-cnt)
print("islas:", len(ids))
for k in orden[: int(sys.argv[2]) if len(sys.argv) > 2 else 12]:
    q = P[r == ids[k]]
    print("n=%6d  min %s  max %s" % (cnt[k], q.min(0).round(3), q.max(0).round(3)))
