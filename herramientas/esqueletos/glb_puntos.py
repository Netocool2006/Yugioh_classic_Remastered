# Lectura minima de un .glb (sin dependencias fuera de numpy): posiciones de todos los vertices en coordenadas glTF
# (aplica la matriz/TRS de los nodos). Y un analisis de apoyos: corta la malla cerca del suelo y agrupa los puntos
# en pies/patas (para ubicar las patas de un esqueleto sin mirar el modelo).
# Uso: python -I glb_puntos.py modelo.glb [alto_corte=0.06]
import json, struct, sys
import numpy as np

TIPOS = {5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8}
N_COMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def leer(ruta):
    d = open(ruta, "rb").read()
    n = struct.unpack("<I", d[12:16])[0]
    j = json.loads(d[20:20 + n])
    binario = d[20 + n + 8:]
    return j, binario


def accesor(j, b, i):
    a = j["accessors"][i]
    v = j["bufferViews"][a["bufferView"]]
    t = TIPOS[a["componentType"]]
    k = N_COMP[a["type"]]
    off = v.get("byteOffset", 0) + a.get("byteOffset", 0)
    paso = v.get("byteStride", 0)
    if paso and paso != np.dtype(t).itemsize * k:
        cr = np.frombuffer(b, np.uint8, a["count"] * paso, off).reshape(a["count"], paso)
        return cr[:, :np.dtype(t).itemsize * k].copy().view(t).reshape(a["count"], k)
    return np.frombuffer(b, t, a["count"] * k, off).reshape(a["count"], k).astype(np.float64 if t == np.float32 else np.int64)


def matriz_nodo(nd):
    if "matrix" in nd:
        return np.array(nd["matrix"]).reshape(4, 4).T
    M = np.eye(4)
    if "scale" in nd:
        M = np.diag(list(nd["scale"]) + [1.0]) @ M
    if "rotation" in nd:
        x, y, z, w = nd["rotation"]
        R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                      [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                      [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
        R4 = np.eye(4)
        R4[:3, :3] = R
        M = R4 @ M
    if "translation" in nd:
        T = np.eye(4)
        T[:3, 3] = nd["translation"]
        M = T @ M
    return M


def puntos(ruta):
    j, b = leer(ruta)
    out = []

    def rec(i, P):
        nd = j["nodes"][i]
        M = P @ matriz_nodo(nd)
        if "mesh" in nd and "skin" not in nd:
            for p in j["meshes"][nd["mesh"]]["primitives"]:
                v = accesor(j, b, p["attributes"]["POSITION"])
                out.append((np.c_[v, np.ones(len(v))] @ M.T)[:, :3])
        elif "mesh" in nd:
            for p in j["meshes"][nd["mesh"]]["primitives"]:
                out.append(accesor(j, b, p["attributes"]["POSITION"]))
        for c in nd.get("children", []):
            rec(c, M)

    for r in j["scenes"][j.get("scene", 0)]["nodes"]:
        rec(r, np.eye(4))
    return np.vstack(out)


def apoyos(P, corte=0.06, radio=0.05):
    """grupos de puntos con y < min+corte (pies): centro (x, z) y cantidad, por union de vecinos a menos de radio"""
    y0 = P[:, 1].min()
    Q = P[P[:, 1] < y0 + corte][:, [0, 2]]
    if len(Q) > 6000:
        Q = Q[np.random.default_rng(1).choice(len(Q), 6000, replace=False)]
    grupo = -np.ones(len(Q), int)
    g = 0
    for i in range(len(Q)):
        if grupo[i] >= 0:
            continue
        pila = [i]
        grupo[i] = g
        while pila:
            k = pila.pop()
            cerca = np.where((grupo < 0) & (np.sum((Q - Q[k]) ** 2, 1) < radio * radio))[0]
            grupo[cerca] = g
            pila += list(cerca)
        g += 1
    res = []
    for k in range(g):
        q = Q[grupo == k]
        res.append((len(q), q.mean(0), q.min(0), q.max(0)))
    return sorted(res, key=lambda r: -r[0])


if __name__ == "__main__":
    P = puntos(sys.argv[1])
    corte = float(sys.argv[2]) if len(sys.argv) > 2 else 0.06
    print("vertices", len(P), "min", P.min(0).round(3), "max", P.max(0).round(3))
    for n, c, lo, hi in apoyos(P, corte)[:8]:
        print("apoyo n=%5d  x=%.3f z=%.3f   x[%.3f,%.3f] z[%.3f,%.3f]" % (n, c[0], c[1], lo[0], hi[0], lo[1], hi[1]))
