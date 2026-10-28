# Yugioh Classic Remaster (rama classic-variedad): clasifica las cartas por su PAPEL en un plan de juego (quema,
# bloqueo, muro, destruccion, castigo, enjambre, combo, robo, molino, cambio de control, victoria alternativa,
# golpeador, negacion...) leyendo el texto en ingles de cartas_ui_en.txt, mas una lista a mano de las conocidas.
# Lo usan los planes de la IA y los mazos dinamicos (src/ia_plan.h). Determinista: misma entrada, misma salida.
#
# Uso:  python -I roles_gen.py [cartas_ui_en.txt] [cartas_classic.txt] [salida]
#   por defecto D:\Docker\compartido\classic-assets\ui\ y datos/roles_cartas.txt junto a este script
# Formato de salida:  id|letras    (una letra por papel; ver PAPELES)
import os, re, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
UI = r"D:\Docker\compartido\classic-assets\ui"
ARG = sys.argv[1:]
F_EN = ARG[0] if len(ARG) > 0 else os.path.join(UI, "cartas_ui_en.txt")
F_CL = ARG[1] if len(ARG) > 1 else os.path.join(UI, "cartas_classic.txt")
SALIDA = ARG[2] if len(ARG) > 2 else os.path.join(AQUI, "datos", "roles_cartas.txt")

PAPELES = {
    "q": "quema (danno o perdida de LP del rival fuera de batalla)",
    "b": "bloqueo (el rival no puede atacar o queda limitado)",
    "w": "muro (monstruo de DEF alta)",
    "d": "destruye o quita monstruos",
    "t": "destruye magias/trampas",
    "c": "castigo (trampa que responde al ataque o a la invocacion)",
    "e": "enjambre (invocaciones especiales, fichas, reclutadores)",
    "k": "combo (Polymerization, rituales, equipos, Limiter Removal, uniones)",
    "o": "robo (robar, buscar cartas)",
    "m": "molino (cartas del mazo o la mano del rival al cementerio)",
    "x": "cambio de control",
    "a": "victoria alternativa (Exodia, Final Countdown, Destiny Board)",
    "g": "golpeador (ATK alto para su coste)",
    "n": "negacion",
    "r": "recuperacion (vuelve del cementerio)",
}

# a mano: las que el texto no deja claras o que son el corazon de un plan
MANO = {
    "swords_of_revealing_light": "b", "gravity_bind": "b", "messenger_of_peace": "b", "level_limit_area_b": "b",
    "nightmare_wheel": "bq", "spirit_barrier": "b", "final_countdown": "ab", "destiny_board": "a",
    "spirit_message_i": "a", "spirit_message_n": "a", "spirit_message_a": "a", "spirit_message_l": "a",
    "exodia_the_forbidden_one": "a", "left_arm_of_the_forbidden_one": "a", "right_arm_of_the_forbidden_one": "a",
    "left_leg_of_the_forbidden_one": "a", "right_leg_of_the_forbidden_one": "a",
    "scapegoat": "eb", "waboku": "b", "threatening_roar": "b", "ceasefire": "q", "magic_cylinder": "cq",
    "limiter_removal": "k", "polymerization": "k", "toon_world": "k", "black_luster_ritual": "k",
    "pot_of_greed": "o", "graceful_charity": "o", "upstart_goblin": "o", "card_destruction": "om", "morphing_jar": "om",
    "needle_worm": "m", "deck_devastation_virus": "m", "confiscation": "m", "delinquent_duo": "m", "the_forceful_sentry": "m",
    "change_of_heart": "x", "snatch_steal": "x", "brain_control": "x", "creature_swap": "x", "mind_control": "x",
    "heavy_storm": "t", "mystical_space_typhoon": "t", "harpie_s_feather_duster": "t", "dust_tornado": "t",
    "mirror_force": "c", "sakuretsu_armor": "c", "trap_hole": "c", "bottomless_trap_hole": "c", "torrential_tribute": "cd",
    "solemn_judgment": "n", "magic_jammer": "n", "seven_tools_of_the_bandit": "n", "lava_golem": "egq", "dark_hole": "d", "raigeki": "d",
    "monster_reborn": "r", "premature_burial": "r", "call_of_the_haunted": "r",
}
# rama classic-mazos-torneo: las piezas de los mazos de torneo (datos/mazos_torneo.txt) que el texto no clasifica
MANO.update({
    "bad_reaction_to_simochi": "q", "rain_of_mercy": "q", "good_goblin_housekeeping": "o", "emergency_provisions": "o",
    "reload": "o", "card_of_sanctity": "o", "spell_economics": "b", "royal_decree": "n", "book_of_moon": "d",
    "book_of_taiyou": "d", "metamorphosis": "k", "magician_of_faith": "r", "penguin_soldier": "d", "hane_hane": "d",
    "lord_of_d": "ek", "the_flute_of_summoning_dragon": "ek", "mustering_of_the_dark_scorpions": "e",
    "don_zaloog": "m", "dark_scorpion_burglars": "m", "necrovalley": "k", "gravekeeper_s_assailant": "d",
    "soul_exchange": "x", "spiritual_energy_settle_machine": "k", "the_a_forces": "k", "king_of_the_skull_servants": "gk",
    "skull_servant": "k", "level_up": "ek", "release_restraint": "ek", "tsukuyomi": "d", "asura_priest": "g",
    "machine_duplication": "ek", "toon_table_of_contents": "o",
})


def num(s):
    try:
        return max(0, int(s))
    except ValueError:
        return 0


def main():
    sub = {}
    for l in open(F_CL, encoding="utf-8"):
        p = l.rstrip("\n").split("|")
        if len(p) >= 4:
            sub[p[0]] = p[3]
    out = ["# roles_cartas.txt - generado por remake-ps5/roles_gen.py (no editar a mano). id|papeles",
           "# " + " ; ".join("%s=%s" % kv for kv in PAPELES.items())]
    n = 0
    for l in open(F_EN, encoding="utf-8"):
        p = l.rstrip("\n").split("|")
        if len(p) < 9:
            continue
        i, clase, nivel, atk, df, t = p[0], p[2], num(p[5]), num(p[6]), num(p[7]), p[8].lower()
        st = sub.get(i, clase)
        r = set(MANO.get(i, ""))
        mons = clase in ("normal", "efecto", "ritual", "fusion")
        if re.search(r"inflict \d+ damage to your opponent|inflict damage to your opponent|your opponent loses \d+|"
                     r"inflict \d+ (points of )?damage to your opponent|inflict damage equal", t) and "battle damage" not in t:
            r.add("q")
        if re.search(r"cannot declare an attack|cannot attack|monsters cannot attack|end the battle phase", t):
            r.add("b")
        if mons and df >= 1800 and df >= atk and nivel <= 4:
            r.add("w")
        if re.search(r"destroy (1|one|all|up to \d|that|the) [^.]*monster|destroy that target|destroy all monsters|"
                     r"destroy all face-up|return (it|that target|1 monster)[^.]*to the hand|banish (that|1|the) ", t) and \
                not re.search(r"destroy this card", t[:40]):
            r.add("d")
        if re.search(r"destroy (1|all|up to \d)[^.]*spell(/| or | and )trap|spell/trap", t) and "destroy" in t:
            r.add("t")
        if clase == "trampa" and re.search(r"declares an attack|summons? (a|1|a monster|exactly)|is (normal |flip |special )?summoned|battle damage", t):
            r.add("c")
        if re.search(r"special summon (1|up to|\d|as many)[^.]*from your (deck|hand)|token", t) or "special summon" in t and mons:
            r.add("e")
        if st in ("magia_ritual", "union", "toon") or clase in ("ritual",) or (st == "magia_equipo" and re.search(r"gains? \d+ atk", t)):
            r.add("k")
        if re.search(r"\bdraw (1|2|3|\d|a|one|two|three) cards?|add 1 [^.]* from your deck to your hand", t):
            r.add("o")
        if re.search(r"send the top \d+ cards? of your opponent's deck|opponent discards|discard 1 random card from your opponent", t):
            r.add("m")
        if re.search(r"take control", t):
            r.add("x")
        if re.search(r"you win the duel", t):
            r.add("a")
        if mons and clase in ("normal", "efecto") and ((nivel <= 4 and atk >= 1700) or (nivel <= 6 and atk >= 2300) or atk >= 2800):
            r.add("g")
        if st == "trampa_contraataque" or re.search(r"negate the activation", t):
            r.add("n")
        if re.search(r"from (either player's|your) graveyard[^.]*special summon|special summon[^.]*from (either player's |your )?graveyard|"
                     r"add 1 [^.]*from your graveyard to your hand", t):
            r.add("r")
        if r:
            out.append("%s|%s" % (i, "".join(sorted(r))))
            n += 1
    os.makedirs(os.path.dirname(SALIDA), exist_ok=True)
    open(SALIDA, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    cuenta = {k: sum(1 for x in out[2:] if k in x.split("|")[1]) for k in PAPELES}
    print("cartas con papel: %d -> %s" % (n, SALIDA))
    print(" ".join("%s=%d" % kv for kv in cuenta.items()))


if __name__ == "__main__":
    main()
