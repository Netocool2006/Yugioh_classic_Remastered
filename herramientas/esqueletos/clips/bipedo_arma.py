# Plantilla de clips: BIPEDO CON ARMA DE MANO en la derecha (hacha, espada, maza). Huesos de rig_auto.py (nombres
# estilo Mixamo: Hips, Spine, Spine1, Neck, Head, Left/Right Shoulder/Arm/ForeArm/Hand, Left/Right UpLeg/Leg/Foot) y
# "Arma" hija de RightHand. Ejes en coordenadas glTF del personaje (ver animar.py): +x inclina adelante / baja el
# brazo hacia atras... (un brazo colgando con -x sube por delante), +y gira hacia su izquierda, +z sube su izquierda.
# Tiempos que usa el juego (yugireload.cpp, FiguraAnimada): el golpe del ataque cae en GOLPE_ATAQUE = 1.0 s; el
# clip "golpe" empieza con el impacto; "defensa" se sostiene en su ultimo cuadro; "muerte" la reemplaza el ragdoll.

def rodillas(k):
    """flexiona las dos piernas k grados (muslo adelante, pierna atras, pie plano)"""
    return {"LeftUpLeg": [("x", -k)], "LeftLeg": [("x", 1.8 * k)], "LeftFoot": [("x", -0.8 * k)],
            "RightUpLeg": [("x", -k)], "RightLeg": [("x", 1.8 * k)], "RightFoot": [("x", -0.8 * k)]}


def junta(*ds):
    r = {}
    for d in ds:
        for k, v in d.items():
            r[k] = r.get(k, []) + list(v) if k != "@loc" else v
    return r


CLIPS = {
    # de pie, respirando (2 s en bucle): el pecho sube y baja, el arma se mece apenas
    "reposo": {"repetir": True, "claves": {
        0.0: {},
        1.0: {"Spine1": [("x", -2.5)], "Head": [("x", 2.0)], "RightArm": [("x", -3)], "LeftArm": [("z", 2)],
              "@loc": [0, 0.004, 0]},
        2.0: {},
    }},
    # tajo de arriba abajo: carga (levanta el arma por detras de la cabeza, gira el torso, flexiona), golpe en 1.0 s
    # (el tronco se vuelca adelante, el arma baja con la muneca) y vuelve
    "ataque": {"claves": {
        0.0: {},
        0.55: junta(rodillas(14), {"Spine": [("x", -8), ("y", 12)], "Spine1": [("x", -10), ("y", 8)], "Head": [("x", 10)],
                                   "RightArm": [("x", -70), ("z", -15)], "RightForeArm": [("x", -45)], "RightHand": [("x", -30)],
                                   "LeftArm": [("x", -35), ("z", 15)], "LeftForeArm": [("x", -30)], "@loc": [0, -0.015, -0.01]}),
        0.8: junta(rodillas(18), {"Spine": [("x", -12), ("y", 15)], "Spine1": [("x", -12), ("y", 10)], "Head": [("x", 12)],
                                  "RightArm": [("x", -80), ("z", -18)], "RightForeArm": [("x", -55)], "RightHand": [("x", -40)],
                                  "LeftArm": [("x", -40), ("z", 18)], "LeftForeArm": [("x", -35)], "@loc": [0, -0.02, -0.015]}),
        1.0: junta(rodillas(22), {"Spine": [("x", 22), ("y", -10)], "Spine1": [("x", 15), ("y", -6)], "Head": [("x", -15)],
                                  "RightArm": [("x", 12), ("z", 5)], "RightForeArm": [("x", -5)], "RightHand": [("x", 55)],
                                  "LeftArm": [("x", 20), ("z", 25)], "LeftForeArm": [("x", -20)], "@loc": [0, -0.03, 0.05]}),
        1.25: junta(rodillas(20), {"Spine": [("x", 26), ("y", -12)], "Spine1": [("x", 16), ("y", -8)], "Head": [("x", -16)],
                                   "RightArm": [("x", 22), ("z", 6)], "RightForeArm": [("x", 0)], "RightHand": [("x", 62)],
                                   "LeftArm": [("x", 22), ("z", 28)], "LeftForeArm": [("x", -15)], "@loc": [0, -0.03, 0.055]}),
        2.1: {},
    }},
    # golpe recibido: la cabeza y el pecho se van atras, retrocede, los brazos se abren; vuelve casi a su lugar
    "golpe": {"claves": {
        0.0: {},
        0.12: junta(rodillas(10), {"Spine": [("x", -16)], "Spine1": [("x", -12)], "Head": [("x", -22)],
                                   "LeftArm": [("z", 28), ("x", -15)], "RightArm": [("x", -18), ("z", -15)],
                                   "@loc": [0, -0.01, -0.05]}),
        0.35: junta(rodillas(12), {"Spine": [("x", -8)], "Spine1": [("x", -6)], "Head": [("x", -8)],
                                   "LeftArm": [("z", 15)], "RightArm": [("x", -8)], "@loc": [0, -0.012, -0.06]}),
        0.8: junta(rodillas(6), {"@loc": [0, -0.005, -0.03]}),
    }},
    # defensa: se agacha con el arma cruzada delante del pecho (se sostiene el ultimo cuadro)
    "defensa": {"claves": {
        0.0: {},
        0.4: junta(rodillas(24), {"Spine": [("x", 10)], "Spine1": [("x", 6)], "Head": [("x", -10)],
                                  "RightArm": [("x", -55), ("z", -10)], "RightForeArm": [("x", -15)], "RightHand": [("x", -35), ("y", 20)],
                                  "LeftArm": [("x", -45), ("z", -10)], "LeftForeArm": [("x", -40)], "@loc": [0, -0.035, -0.01]}),
        1.0: junta(rodillas(25), {"Spine": [("x", 11)], "Spine1": [("x", 6)], "Head": [("x", -10)],
                                  "RightArm": [("x", -57), ("z", -10)], "RightForeArm": [("x", -15)], "RightHand": [("x", -35), ("y", 20)],
                                  "LeftArm": [("x", -46), ("z", -10)], "LeftForeArm": [("x", -40)], "@loc": [0, -0.037, -0.01]}),
    }},
    # muerte de respaldo (si no hay ragdoll): le ceden las rodillas y cae de espaldas
    "muerte": {"claves": {
        0.0: {},
        0.3: junta(rodillas(25), {"Spine": [("x", -12)], "Head": [("x", -20)], "LeftArm": [("z", 30)], "RightArm": [("z", -20)],
                                  "@loc": [0, -0.04, -0.03]}),
        1.0: {"Hips": [("x", -80)], "Spine": [("x", -8)], "Head": [("x", -15)], "LeftUpLeg": [("x", -60)], "LeftLeg": [("x", 40)],
              "RightUpLeg": [("x", -45)], "RightLeg": [("x", 30)], "LeftArm": [("z", 60)], "RightArm": [("z", -50)],
              "@loc": [0, -0.33, -0.3]},
        1.6: {"Hips": [("x", -88)], "Spine": [("x", -2)], "Head": [("x", -5)], "LeftUpLeg": [("x", -70)], "LeftLeg": [("x", 35)],
              "RightUpLeg": [("x", -55)], "RightLeg": [("x", 25)], "LeftArm": [("z", 70)], "RightArm": [("z", -60)],
              "@loc": [0, -0.36, -0.34]},
    }},
}
