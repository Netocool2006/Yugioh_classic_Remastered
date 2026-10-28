# Lee los vertices de un .glb (sin dependencias: json + numpy), con la matriz de cada nodo, en coordenadas del
# modelo glTF (frente = +Z, arriba = +Y). Lo usa atributos.py (tabla de comprobacion).
# El ESCANEO de verdad lo hace el motor al cargar cada modelo (remake-ps5/src/reglas/escaneo.h); atributos.escanear()
# es el mismo calculo en Python: si se cambia uno, cambiar el otro.
import json, struct
import numpy as np


def _matriz(nd):
    if "matrix" in nd:
        return np.array(nd["matrix"], dtype=np.float64).reshape(4, 4).T
    m = np.eye(4)
    if "scale" in nd:
        m = np.diag(list(nd["scale"]) + [1.0]) @ m
    if "rotation" in nd:
        x, y, z, w = nd["rotation"]
        r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                      [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                      [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
        rr = np.eye(4)
        rr[:3, :3] = r
        m = rr @ m
    if "translation" in nd:
        t = np.eye(4)
        t[:3, 3] = nd["translation"]
        m = t @ m
    return m


def vertices(ruta):
    datos = open(ruta, "rb").read()
    largo = struct.unpack_from("<I", datos, 12)[0]
    g = json.loads(datos[20:20 + largo])
    off = 20 + largo
    binario = datos[off + 8:off + 8 + struct.unpack_from("<I", datos, off)[0]]
    pts = []

    def nodo(n, padre):
        nd = g["nodes"][n]
        M = padre @ _matriz(nd)
        if "mesh" in nd:
            for p in g["meshes"][nd["mesh"]]["primitives"]:
                if "POSITION" not in p["attributes"]:
                    continue
                a = g["accessors"][p["attributes"]["POSITION"]]
                v = g["bufferViews"][a["bufferView"]]
                st = v.get("byteStride", 12)
                ini = v.get("byteOffset", 0) + a.get("byteOffset", 0)
                crudo = np.frombuffer(binario, dtype=np.uint8, count=st * (a["count"] - 1) + 12, offset=ini)
                q = np.lib.stride_tricks.as_strided(crudo.view(np.uint8), shape=(a["count"], 12), strides=(st, 1))
                q = np.ascontiguousarray(q).view(np.float32).reshape(-1, 3).astype(np.float64)
                pts.append((M @ np.c_[q, np.ones(len(q))].T).T[:, :3])
        for c in nd.get("children", []):
            nodo(c, M)

    for n in g["scenes"][g.get("scene", 0)]["nodes"]:
        nodo(n, np.eye(4))
    return np.concatenate(pts) if pts else np.zeros((0, 3))
