# Plantilla de clips: CUADRUPEDO (bestias de cuatro patas). Huesos de rig_auto.py: Hips, Spine, Neck, Head, Tail1-3,
# Left/Right Front Up/Low/Foot (patas delanteras), Left/Right Hind Up/Low/Foot (traseras) y, si tiene, Left/RightWing.
# Ejes glTF del personaje (animar.py): +x baja lo que apunta al frente (cuello adelante -> cabeza abajo) y lleva una
# pata que cuelga hacia atras; -x la lleva adelante. +y gira hacia su izquierda; +z sube su lado izquierdo.
# Golpe del ataque en 1.0 s (embestida con mordida).

def patas(fu=0.0, fl=0.0, hu=0.0, hl=0.0):
    """dobla las cuatro patas: delanteras (arriba, abajo) y traseras"""
    return {"LeftFrontUp": [("x", fu)], "RightFrontUp": [("x", fu)], "LeftFrontLow": [("x", fl)], "RightFrontLow": [("x", fl)],
            "LeftHindUp": [("x", hu)], "RightHindUp": [("x", hu)], "LeftHindLow": [("x", hl)], "RightHindLow": [("x", hl)]}


def junta(*ds):
    r = {}
    for d in ds:
        for k, v in d.items():
            r[k] = r.get(k, []) + list(v) if k != "@loc" else v
    return r


def alas(g):
    return {"LeftWing": [("z", g)], "RightWing": [("z", -g)]}


CLIPS = {
    # respira, mueve la cola y las alitas, mira un poco a los lados (2.4 s en bucle)
    "reposo": {"repetir": True, "claves": {
        0.0: junta(alas(0), {"Tail1": [("y", 6)]}),
        0.6: junta(alas(12), {"Spine": [("x", -1.5)], "Neck": [("x", -3)], "Head": [("y", 6)], "Tail1": [("y", -2)], "Tail2": [("y", -6)],
                              "@loc": [0, 0.004, 0]}),
        1.2: junta(alas(0), {"Tail1": [("y", -6)], "Tail2": [("y", 2)]}),
        1.8: junta(alas(12), {"Spine": [("x", -1.5)], "Neck": [("x", -3)], "Head": [("y", -6)], "Tail1": [("y", 2)], "Tail2": [("y", 6)],
                              "@loc": [0, 0.004, 0]}),
        2.4: junta(alas(0), {"Tail1": [("y", 6)]}),
    }},
    # embestida con mordida: se agazapa y baja los cuernos, se lanza estirando las patas traseras, muerde (la cabeza
    # da un tiron hacia arriba) en 1.0 s y vuelve
    "ataque": {"claves": {
        0.0: {},
        0.5: junta(patas(-12, 22, 14, -20), alas(25), {"Neck": [("x", 22)], "Head": [("x", 8)], "Spine": [("x", 4)], "Tail1": [("x", -15)],
                                                         "@loc": [0, -0.03, -0.05]}),
        0.8: junta(patas(-18, 26, 18, -24), alas(30), {"Neck": [("x", 28)], "Head": [("x", 10)], "Spine": [("x", 5)], "Tail1": [("x", -20)],
                                                         "@loc": [0, -0.035, -0.06]}),
        1.0: junta(patas(-38, 10, 32, -8), alas(-20), {"Neck": [("x", 30)], "Head": [("x", -22)], "Spine": [("x", -4)], "Tail1": [("x", -30)],
                                                         "Tail2": [("x", -15)], "@loc": [0, 0.03, 0.14]}),
        1.2: junta(patas(-20, 15, 18, -10), alas(10), {"Neck": [("x", 5)], "Head": [("x", -30)], "Spine": [("x", -6)], "Tail1": [("x", -20)],
                                                         "@loc": [0, 0.015, 0.12]}),
        2.0: {},
    }},
    # golpe recibido: la cabeza se va atras, retrocede, se le doblan un poco las patas delanteras
    "golpe": {"claves": {
        0.0: {},
        0.12: junta(patas(8, 15, -6, 0), alas(30), {"Neck": [("x", -22)], "Head": [("x", -15), ("y", 15)], "Spine": [("z", 6)],
                                                      "Tail1": [("x", 20)], "@loc": [0, -0.02, -0.06]}),
        0.4: junta(patas(4, 8, -3, 0), alas(10), {"Neck": [("x", -8)], "Head": [("y", 6)], "Spine": [("z", 3)], "@loc": [0, -0.01, -0.07]}),
        0.8: {"@loc": [0, 0, -0.04]},
    }},
    # defensa: se planta con la cabeza baja y los cuernos al frente, las alitas pegadas (se sostiene)
    "defensa": {"claves": {
        0.0: {},
        0.4: junta(patas(-10, 25, 12, -18), alas(-15), {"Neck": [("x", 30)], "Head": [("x", 12)], "Tail1": [("x", 25)],
                                                          "@loc": [0, -0.04, -0.02]}),
        1.0: junta(patas(-11, 27, 13, -19), alas(-15), {"Neck": [("x", 31)], "Head": [("x", 12)], "Tail1": [("x", 26)],
                                                          "@loc": [0, -0.042, -0.02]}),
    }},
    # muerte de respaldo (sin ragdoll): se le doblan las patas y cae de costado
    "muerte": {"claves": {
        0.0: {},
        0.4: junta(patas(-10, 40, 15, -30), {"Neck": [("x", 15)], "@loc": [0, -0.08, 0]}),
        1.2: junta(patas(-30, 50, 30, -40), {"Hips": [("z", 80)], "Neck": [("x", 10), ("z", 20)], "Tail1": [("z", 20)],
                                               "@loc": [0, -0.25, 0]}),
    }},
}
