# Plantilla de clips: ALADO con alas de membrana en los brazos (murcielagos, demonios alados, dragones bipedos).
# Huesos de rig_auto.py: Hips, Spine, Neck, Head, Left/Right Shoulder/Arm/ForeArm, dedos del ala Left/Right Wing1..3 y
# Wing1b..3b, Left/Right UpLeg/Leg/Foot, Tail1-3. El juego lo tiene en el aire (vuelo de la regla): el reposo ya es
# un aleteo de verdad (el ala baja empujando el aire y sube plegada). Ejes glTF del personaje (animar.py): +z sube el
# lado izquierdo (ala izquierda arriba = LeftArm +z; ala derecha arriba = RightArm -z), -y lleva el ala izquierda
# hacia adelante (cubrirse), +x inclina adelante. Golpe del ataque (picada) en 1.0 s.

def ala(g, dedos=0.0, adelante=0.0):
    """las dos alas: g grados arriba (+) o abajo (-) en el hombro; dedos: cuanto se pliega la punta (-) o se abre;
    adelante: las lleva hacia el frente (envolverse)"""
    d = {"LeftArm": [("z", g), ("y", -adelante)], "RightArm": [("z", -g), ("y", adelante)]}
    for k in ("1", "2", "3"):
        d["LeftWing%s" % k] = [("z", dedos)]
        d["RightWing%s" % k] = [("z", -dedos)]
        d["LeftWing%sb" % k] = [("z", 0.6 * dedos)]
        d["RightWing%sb" % k] = [("z", -0.6 * dedos)]
    return d


def junta(*ds):
    r = {}
    for d in ds:
        for k, v in d.items():
            r[k] = r.get(k, []) + list(v) if k != "@loc" else v
    return r


PIERNAS_COLGANDO = {"LeftUpLeg": [("x", 12)], "RightUpLeg": [("x", 12)], "LeftLeg": [("x", 10)], "RightLeg": [("x", 10)],
                    "LeftFoot": [("x", 25)], "RightFoot": [("x", 25)]}
GARRAS = {"LeftUpLeg": [("x", -55)], "RightUpLeg": [("x", -55)], "LeftLeg": [("x", 35)], "RightLeg": [("x", 35)],
          "LeftFoot": [("x", -20)], "RightFoot": [("x", -20)]}


def aleteo(t0, cola):
    """un aleteo (0.6 s) desde t0: abajo con fuerza (el cuerpo sube) y arriba plegando"""
    return {
        t0: junta(ala(28, 8), PIERNAS_COLGANDO, {"Tail1": [("x", -10 + cola)], "Tail2": [("y", cola)], "@loc": [0, -0.012, 0]}),
        t0 + 0.25: junta(ala(-32, 12), PIERNAS_COLGANDO, {"Spine": [("x", 4)], "Tail1": [("x", -14)], "Tail2": [("y", -cola)],
                                                          "@loc": [0, 0.015, 0]}),
        t0 + 0.45: junta(ala(5, -18), PIERNAS_COLGANDO, {"Tail1": [("x", -12)], "@loc": [0, 0.004, 0]}),
    }


rep = {}
rep.update(aleteo(0.0, 8))
rep.update(aleteo(0.6, -8))
rep[1.2] = rep[0.0]
CLIPS = {
    "reposo": {"repetir": True, "claves": rep},
    # picada: sube con un aletazo, pliega las alas hacia atras y cae en picada con las garras por delante (golpe en
    # 1.0 s, muerde), abre las alas para frenar y vuelve a su aleteo
    "ataque": {"claves": {
        0.0: junta(ala(28, 8), PIERNAS_COLGANDO, {"@loc": [0, -0.012, 0]}),
        0.3: junta(ala(-35, 12), PIERNAS_COLGANDO, {"Spine": [("x", -10)], "Head": [("x", -10)], "@loc": [0, 0.04, -0.04]}),
        0.55: junta(ala(40, -10), PIERNAS_COLGANDO, {"Spine": [("x", -14)], "Head": [("x", -12)], "@loc": [0, 0.06, -0.06]}),
        0.8: junta(ala(-10, -35, -25), GARRAS, {"Hips": [("x", 22)], "Spine": [("x", 10)], "Head": [("x", -25)], "Tail1": [("x", -25)],
                                                  "@loc": [0, 0.02, 0.06]}),
        1.0: junta(ala(-20, -30, -30), GARRAS, {"Hips": [("x", 28)], "Spine": [("x", 14)], "Neck": [("x", 10)], "Head": [("x", -15)],
                                                  "Tail1": [("x", -30)], "@loc": [0, -0.04, 0.16]}),
        1.25: junta(ala(35, 15), PIERNAS_COLGANDO, {"Hips": [("x", 10)], "Spine": [("x", -5)], "@loc": [0, 0.0, 0.1]}),
        1.55: junta(ala(-30, 12), PIERNAS_COLGANDO, {"@loc": [0, 0.015, 0.04]}),
        2.0: junta(ala(28, 8), PIERNAS_COLGANDO, {"@loc": [0, -0.012, 0]}),
    }},
    # golpe recibido: se le doblan las alas, la cabeza atras, retrocede en el aire
    "golpe": {"claves": {
        0.0: junta(ala(28, 8), PIERNAS_COLGANDO, {"@loc": [0, -0.012, 0]}),
        0.12: junta(ala(45, -25), PIERNAS_COLGANDO, {"Spine": [("x", -22)], "Head": [("x", -25), ("y", 15)], "Hips": [("x", -10)],
                                                      "Tail1": [("x", 15)], "@loc": [0, 0.01, -0.08]}),
        0.45: junta(ala(-20, 5), PIERNAS_COLGANDO, {"Spine": [("x", -8)], "Head": [("x", -8)], "@loc": [0, -0.01, -0.09]}),
        0.8: junta(ala(20, 8), PIERNAS_COLGANDO, {"@loc": [0, -0.01, -0.05]}),
    }},
    # defensa: se envuelve en sus alas (por delante del cuerpo), encogido (se sostiene)
    "defensa": {"claves": {
        0.0: junta(ala(28, 8), PIERNAS_COLGANDO),
        0.45: junta(ala(-15, -20, 70), {"Spine": [("x", 15)], "Head": [("x", 20)], "LeftUpLeg": [("x", -30)], "RightUpLeg": [("x", -30)],
                                        "LeftLeg": [("x", 50)], "RightLeg": [("x", 50)], "@loc": [0, -0.02, 0]}),
        1.0: junta(ala(-16, -21, 72), {"Spine": [("x", 16)], "Head": [("x", 21)], "LeftUpLeg": [("x", -30)], "RightUpLeg": [("x", -30)],
                                       "LeftLeg": [("x", 50)], "RightLeg": [("x", 50)], "@loc": [0, -0.02, 0]}),
    }},
    # muerte de respaldo (sin ragdoll): alas caidas, se desploma de espaldas
    "muerte": {"claves": {
        0.0: {},
        0.4: junta(ala(-40, -20), {"Spine": [("x", -15)], "Head": [("x", -20)], "@loc": [0, -0.05, -0.02]}),
        1.2: junta(ala(-50, -30), {"Hips": [("x", -80)], "Head": [("x", -10)], "@loc": [0, -0.2, -0.2]}),
    }},
}
