# Yugioh Classic Remaster (rama classic-mazos-torneo): MAZOS DE TORNEO CURADOS. Listas a mano (no generadas por tema)
# de los arquetipos que se jugaban en la era Goat (abril-septiembre 2005) y 5 inspirados en recetas de Master Duel,
# solo con cartas de nuestro pool (las 1690 Goat y las cartas extra de datos/cartas_extra.txt, sin cartas firma) y la
# banlist que usa el juego (cartas_classic.txt). Rama classic-torneo-extras: las 10 cartas que faltaban (Command Knight,
# Cyber Dragon, Foolish Burial, Goblin Zombie, Il Blud, Magical Mallet, Izanagi, Yamato-no-Kami, Obnoxious Celtic Guard,
# Drillroid) ya son cartas extra del juego y las listas las usan en lugar de los sustitutos. Las fuentes y sustituciones de cada lista estan en docs/mazos_torneo.md.
#
# Uso:  python -I mazos_torneo.py [carpeta ui con cartas_ui_en.txt y cartas_classic.txt] [salida]
# Valida cada mazo (40 cartas, max. 3 copias y limites Goat, nada prohibido, ni fusiones en el principal, fusiones con
# sus materiales y Polymerization o con Metamorphosis y un monstruo de su nivel, rituales con su magia, monstruos que
# no se invocan normal solo si el mazo trae como sacarlos) y escribe datos/mazos_torneo.txt:
#   D|clave|plan|nombre ES|nombre EN          el mazo (plan: el de ia_plan.h que lo juega mejor)
#   M|clave|id id ...                         principal (40, tal cual: el cargador NO lo reequilibra)
#   X|clave|id id ...                         fusion
#   K|clave|id id ...                         sus cartas clave (la prueba mide que la IA las juegue)
#   A|rival|clave clave ...                   a que rivales/personajes se asigna (variantes de torneo)
import os, re, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
UI = sys.argv[1] if len(sys.argv) > 1 else r"D:\Docker\compartido\classic-assets\ui"
SALIDA = sys.argv[2] if len(sys.argv) > 2 else os.path.join(AQUI, "datos", "mazos_torneo.txt")

# ------------------------------------------------------------------------------------------------ los mazos
# (clave, plan, nombre ES, nombre EN, principal, fusion, clave, especiales permitidos)
# "Nombre*2" = 2 copias. especiales: monstruos que no se invocan de forma normal y el mazo sabe sacar
GOAT_SPELLS = ["Pot of Greed", "Graceful Charity", "Heavy Storm", "Delinquent Duo", "Snatch Steal", "Premature Burial",
               "Mystical Space Typhoon"]
GOAT_TRAPS = ["Mirror Force", "Ring of Destruction", "Call of the Haunted", "Torrential Tribute"]

MAZOS = [
    # ------------------------------------------------------------------ inspirados en Master Duel
    ("md_sirvientes", "enjambre", "Familia de Sirvientes", "Skull Servant Family",
     ["Skull Servant*3", "King of the Skull Servants*3", "Pyramid Turtle*3", "Spirit Reaper*3", "Vampire Lord*2",
      "Ryu Kokki", "Sangan", "Magical Merchant*2", "Morphing Jar",
      "Card Destruction", "Book of Life*2", "Swords of Revealing Light", "Pot of Greed",
      "Graceful Charity", "Heavy Storm", "Delinquent Duo", "Premature Burial", "Mystical Space Typhoon",
      "Scapegoat", "Foolish Burial", "Nobleman of Crossout", "Smashing Ground",
      "Call of the Haunted", "Mirror Force", "Ring of Destruction", "Torrential Tribute", "Sakuretsu Armor*2",
      "Magic Cylinder"],
     [], ["King of the Skull Servants", "Graceful Charity", "Foolish Burial", "Book of Life"], []),
    ("md_horus", "control", "Dragón de Llama Negra", "Black Flame Dragon",
     ["Horus the Black Flame Dragon LV4*3", "Horus the Black Flame Dragon LV6*3", "Horus the Black Flame Dragon LV8*2",
      "Masked Dragon*2", "Luster Dragon*2", "Spear Dragon", "Sangan", "Breaker the Magical Warrior",
      "Exiled Force", "Kaiser Glider", "D.D. Warrior Lady",
      "Level Up!*2", "Book of Moon", "Nobleman of Crossout", "Swords of Revealing Light", "Stamping Destruction",
      "Scapegoat"] + GOAT_SPELLS +
     ["Royal Decree*2", "Call of the Haunted", "Mirror Force", "Ring of Destruction", "Torrential Tribute",
      "Sakuretsu Armor*2"],
     [], ["Horus the Black Flame Dragon LV6", "Horus the Black Flame Dragon LV8", "Royal Decree", "Level Up!"],
     ["Horus the Black Flame Dragon LV8"]),
    ("md_exodia_robo", "alternativa", "Exodia Codicioso", "Greedy Goblin Exodia",
     ["Exodia the Forbidden One", "Left Arm of the Forbidden One", "Right Arm of the Forbidden One",
      "Left Leg of the Forbidden One", "Right Leg of the Forbidden One", "Magician of Faith*2", "Sangan",
      "Big Shield Gardna*2",
      "Upstart Goblin*2", "Pot of Greed", "Graceful Charity", "Emergency Provisions*2", "Book of Moon",
      "Swords of Revealing Light", "Messenger of Peace*2", "Level Limit - Area B*2", "Scapegoat*3", "Reload*2",
      "Good Goblin Housekeeping*2", "Jar of Greed*3", "Reckless Greed", "Gravity Bind*2", "Waboku*2",
      "Threatening Roar*2", "Magic Cylinder"],
     [], ["Good Goblin Housekeeping", "Emergency Provisions", "Upstart Goblin", "Jar of Greed", "Exodia the Forbidden One"], []),
    ("md_simochi", "bloqueo", "Poder del Fuego", "Power to the Fire",
     ["Fire Princess*3", "Spirit Reaper*2", "Stealth Bird*2", "Des Koala", "Cannon Soldier", "Magician of Faith*2",
      "Sangan", "Morphing Jar", "Brain Jacker",
      "Rain of Mercy*3", "Upstart Goblin*2", "Snatch Steal", "Poison of the Old Man*2", "Ookazi*2", "Pot of Greed",
      "Graceful Charity", "Swords of Revealing Light", "Level Limit - Area B*2",
      "Bad Reaction to Simochi*3", "Magic Cylinder", "Just Desserts*2", "Secret Barrel", "Ceasefire",
      "Gravity Bind*2", "Ring of Destruction"],
     [], ["Bad Reaction to Simochi", "Rain of Mercy", "Fire Princess", "Magic Cylinder"], []),
    ("md_guerreros_unidos", "agresivo", "Coalición Guerrera", "Warrior Coalition",
     ["Command Knight*3", "Marauding Captain*2", "Goblin Attack Force*2", "Gearfried the Iron Knight*2",
      "Mystic Swordsman LV2*2", "D.D. Assailant", "Freed the Matchless General", "Don Zaloog", "Exiled Force",
      "D.D. Warrior Lady", "Zombyra the Dark", "Big Shield Gardna", "Obnoxious Celtic Guard",
      "Breaker the Magical Warrior",
      "The A. Forces*3", "United We Stand", "Reinforcement of the Army*2", "Mage Power", "Axe of Despair",
      "Pot of Greed", "Graceful Charity", "Heavy Storm", "Snatch Steal", "Premature Burial", "Mystical Space Typhoon",
      "Nobleman of Crossout"] + GOAT_TRAPS + ["Sakuretsu Armor"],
     [], ["The A. Forces", "United We Stand", "Reinforcement of the Army", "Command Knight"], []),
    # ------------------------------------------------------------------ torneo, era Goat
    ("goat_control", "control", "Goat Control", "Goat Control",
     ["Black Luster Soldier - Envoy of the Beginning", "Airknight Parshath*2", "Breaker the Magical Warrior",
      "Tribe-Infecting Virus", "Sangan", "Magician of Faith", "Morphing Jar", "D.D. Warrior Lady", "Exiled Force",
      "Tsukuyomi", "Jinzo", "Abyss Soldier", "Asura Priest", "Kycoo the Ghost Destroyer", "Spirit Reaper"] +
     GOAT_SPELLS + ["Scapegoat*2", "Book of Moon", "Nobleman of Crossout*2", "Metamorphosis", "Smashing Ground",
                    "Swords of Revealing Light", "Enemy Controller"] +
     GOAT_TRAPS + ["Sakuretsu Armor*2", "Magic Cylinder", "Solemn Judgment"],
     ["Thousand-Eyes Restrict*2"], ["Scapegoat", "Metamorphosis", "Thousand-Eyes Restrict", "Black Luster Soldier - Envoy of the Beginning"],
     ["Black Luster Soldier - Envoy of the Beginning"]),
    ("chaos_control", "control", "Chaos Control", "Chaos Control",
     ["Black Luster Soldier - Envoy of the Beginning", "Chaos Sorcerer*3", "Dark Magician of Chaos",
      "Breaker the Magical Warrior", "Tribe-Infecting Virus", "Sangan", "Magician of Faith", "Morphing Jar",
      "D.D. Warrior Lady", "Exiled Force", "Jinzo", "Airknight Parshath", "Night Assailant", "Mystic Tomato",
      "Shining Angel", "Kycoo the Ghost Destroyer"] +
     GOAT_SPELLS + ["Scapegoat*2", "Book of Moon", "Nobleman of Crossout", "Smashing Ground", "Enemy Controller",
                    "Swords of Revealing Light", "Brain Control"] +
     GOAT_TRAPS + ["Sakuretsu Armor*2", "Magic Cylinder"],
     [], ["Chaos Sorcerer", "Black Luster Soldier - Envoy of the Beginning", "Dark Magician of Chaos"],
     ["Black Luster Soldier - Envoy of the Beginning", "Chaos Sorcerer"]),
    ("guerreros_toolbox", "agresivo", "Guerreros Toolbox", "Warrior Toolbox",
     ["Marauding Captain*2", "D.D. Warrior Lady", "Exiled Force", "D.D. Assailant*2", "Mystic Swordsman LV2*2",
      "Command Knight*2", "Don Zaloog*2", "Freed the Matchless General", "Gearfried the Iron Knight",
      "Zombyra the Dark", "Breaker the Magical Warrior", "Black Luster Soldier - Envoy of the Beginning", "Sangan",
      "Tribe-Infecting Virus", "Obnoxious Celtic Guard",
      "Reinforcement of the Army*2"] + GOAT_SPELLS + ["Nobleman of Crossout*2", "Smashing Ground", "Book of Moon",
                                                      "Enemy Controller"] +
     GOAT_TRAPS + ["Sakuretsu Armor*2"],
     [], ["Reinforcement of the Army", "Marauding Captain", "Exiled Force", "D.D. Warrior Lady"],
     ["Black Luster Soldier - Envoy of the Beginning"]),
    ("flip_control", "control", "Flip Control", "Flip Control",
     ["Magician of Faith*2", "Morphing Jar", "Cyber Jar", "Old Vindictive Magician*2", "Penguin Soldier*2",
      "Hane-Hane*2", "Stealth Bird", "Night Assailant*2", "Mask of Darkness", "Guardian Sphinx", "Sangan",
      "Breaker the Magical Warrior", "Tribe-Infecting Virus", "Magical Merchant", "Man-Eater Bug",
      "Book of Moon*3", "Book of Taiyou"] + GOAT_SPELLS + ["Swords of Revealing Light", "Nobleman of Crossout",
                                                           "Card Destruction"] +
     GOAT_TRAPS + ["Sakuretsu Armor", "Ceasefire"],
     [], ["Book of Moon", "Magician of Faith", "Penguin Soldier", "Night Assailant", "Hane-Hane"], []),
    ("zombis", "enjambre", "Zombis Vampiro", "Vampire Zombies",
     ["Pyramid Turtle*3", "Vampire Lord*2", "Ryu Kokki*2", "Spirit Reaper*2", "Goblin Zombie*2",
      "Despair from the Dark", "Il Blud", "Patrician of Darkness", "Dark Dust Spirit", "Master Kyonshee",
      "Vampire Lady", "Sangan", "Tribe-Infecting Virus", "Breaker the Magical Warrior",
      "Book of Life*2", "Card of Safe Return", "Pot of Greed", "Graceful Charity", "Heavy Storm", "Premature Burial",
      "Snatch Steal", "Mystical Space Typhoon", "Delinquent Duo", "Nobleman of Crossout", "Smashing Ground",
      "Call of the Haunted", "Mirror Force", "Ring of Destruction", "Torrential Tribute", "Sakuretsu Armor*2",
      "Magic Cylinder", "Compulsory Evacuation Device"],
     [], ["Pyramid Turtle", "Vampire Lord", "Goblin Zombie", "Book of Life"], []),
    ("gearfried_equipo", "combo", "Gearfried y Equipos", "Gearfried Equip",
     ["Gearfried the Iron Knight*3", "Gearfried the Swordmaster*2", "Marauding Captain*2", "Command Knight*2",
      "Exiled Force", "D.D. Warrior Lady", "Breaker the Magical Warrior", "Mystic Swordsman LV2*2",
      "Zombyra the Dark", "Sangan", "Tribe-Infecting Virus", "Don Zaloog",
      "Release Restraint*2", "Axe of Despair*2", "Malevolent Nuzzler", "Mage Power", "Fairy Meteor Crush",
      "Big Bang Shot", "Lightning Blade", "Reinforcement of the Army*2", "Pot of Greed", "Graceful Charity",
      "Heavy Storm", "Snatch Steal", "Premature Burial", "Mystical Space Typhoon"] +
     GOAT_TRAPS + ["Sakuretsu Armor"],
     [], ["Release Restraint", "Gearfried the Swordmaster", "Axe of Despair", "Gearfried the Iron Knight"],
     ["Gearfried the Swordmaster"]),
    ("escorpiones", "tempo", "Escorpiones Oscuros", "Dark Scorpions",
     ["Don Zaloog*3", "Dark Scorpion Burglars*2", "Dark Scorpion - Chick the Yellow*2",
      "Dark Scorpion - Cliff the Trap Remover*2", "Dark Scorpion - Gorg the Strong*2",
      "Dark Scorpion - Meanae the Thorn*2", "Marauding Captain*2", "Sangan", "Breaker the Magical Warrior",
      "Exiled Force", "Spirit Reaper",
      "Mustering of the Dark Scorpions*2", "Reinforcement of the Army*2"] + GOAT_SPELLS +
     ["Nobleman of Crossout", "Book of Moon", "Swords of Revealing Light",
      "Dark Scorpion Combination"] + GOAT_TRAPS + ["Sakuretsu Armor*2"],
     [], ["Mustering of the Dark Scorpions", "Dark Scorpion Combination", "Don Zaloog", "Dark Scorpion - Meanae the Thorn"], []),
    ("monarcas", "control", "Monarcas", "Monarchs",
     ["Zaborg the Thunder Monarch*2", "Mobius the Frost Monarch*2", "Thestalos the Firestorm Monarch",
      "Granmarg the Rock Monarch", "Breaker the Magical Warrior", "Sangan", "Tribe-Infecting Virus",
      "D.D. Warrior Lady", "Exiled Force", "Magician of Faith", "Morphing Jar", "Mystic Tomato*2",
      "Spirit Reaper", "Black Luster Soldier - Envoy of the Beginning", "Night Assailant"] +
     GOAT_SPELLS + ["Book of Moon*2", "Nobleman of Crossout", "Soul Exchange", "Brain Control", "Scapegoat*2",
                    "Enemy Controller"] +
     GOAT_TRAPS + ["Sakuretsu Armor*2", "Solemn Judgment"],
     [], ["Zaborg the Thunder Monarch", "Mobius the Frost Monarch", "Soul Exchange", "Mystic Tomato"],
     ["Black Luster Soldier - Envoy of the Beginning"]),
    ("maquinas", "agresivo", "Máquinas Antiguas", "Ancient Machines",
     ["Ancient Gear Golem", "Ancient Gear Beast*2", "Jinzo", "Reflect Bounder", "Cyber Dragon*3", "Mechanicalchaser",
      "Cyber Jar", "Robotic Knight*2", "Drillroid*2", "Sangan", "Breaker the Magical Warrior", "Exiled Force",
      "D.D. Warrior Lady", "Tribe-Infecting Virus",
      "Limiter Removal*2", "Machine Duplication", "Pot of Greed", "Graceful Charity", "Heavy Storm",
      "Delinquent Duo", "Snatch Steal", "Premature Burial", "Mystical Space Typhoon", "Nobleman of Crossout*2",
      "Smashing Ground", "Book of Moon", "Swords of Revealing Light"] +
     GOAT_TRAPS + ["Sakuretsu Armor*2"],
     [], ["Limiter Removal", "Cyber Dragon", "Machine Duplication", "Ancient Gear Golem"], []),
    ("quema_bloqueo", "bloqueo", "Quema y Bloqueo", "Burn Stall",
     ["Big Shield Gardna*2", "Stealth Bird*2", "Des Koala*2", "Cannon Soldier", "Morphing Jar", "Magician of Faith*2",
      "Sangan", "Spirit Reaper*2",
      "Wave-Motion Cannon*2", "Level Limit - Area B*2", "Messenger of Peace*2", "Swords of Revealing Light",
      "Pot of Greed", "Graceful Charity", "Delinquent Duo", "Ookazi*2", "Tremendous Fire*2",
      "Poison of the Old Man", "Scapegoat*2",
      "Gravity Bind*2", "Ceasefire", "Just Desserts*2", "Secret Barrel*2", "Magic Cylinder", "Ring of Destruction",
      "Waboku"],
     [], ["Wave-Motion Cannon", "Level Limit - Area B", "Gravity Bind", "Messenger of Peace", "Stealth Bird"], []),
    ("guardianes_tumba", "control", "Guardianes de la Tumba", "Gravekeepers",
     ["Gravekeeper's Spy*3", "Gravekeeper's Spear Soldier*2", "Gravekeeper's Assailant*2", "Gravekeeper's Curse*2",
      "Gravekeeper's Guard*2", "Gravekeeper's Chief", "Gravekeeper's Cannonholder", "Sangan",
      "Breaker the Magical Warrior", "D.D. Warrior Lady", "Exiled Force", "Spirit Reaper",
      "Necrovalley*3", "Royal Tribute", "Pot of Greed", "Graceful Charity", "Heavy Storm", "Delinquent Duo",
      "Snatch Steal", "Book of Moon*2", "Mystical Space Typhoon", "Nobleman of Crossout", "Smashing Ground",
      "Swords of Revealing Light",
      "Mirror Force", "Ring of Destruction", "Torrential Tribute", "Sakuretsu Armor*2", "Rite of Spirit",
      "Solemn Judgment"],
     [], ["Necrovalley", "Gravekeeper's Spy", "Gravekeeper's Chief", "Gravekeeper's Assailant"], []),
    ("relinquished", "combo", "Ritual Relinquished", "Relinquished Ritual",
     ["Relinquished*3", "Sonic Bird*2", "Manju of the Ten Thousand Hands*2", "Senju of the Thousand Hands",
      "Magician of Faith*2", "Sangan", "Breaker the Magical Warrior", "Tribe-Infecting Virus", "D.D. Warrior Lady",
      "Exiled Force", "Mystic Tomato", "Spirit Reaper",
      "Black Illusion Ritual*3"] + GOAT_SPELLS + ["Scapegoat*2", "Book of Moon", "Nobleman of Crossout",
                                                  "Swords of Revealing Light", "Enemy Controller", "Brain Control"] +
     GOAT_TRAPS + ["Sakuretsu Armor*2"],
     [], ["Relinquished", "Black Illusion Ritual", "Sonic Bird", "Manju of the Ten Thousand Hands"], []),
    ("dragones_flauta", "combo", "Señor de los Dragones", "Lord of D. Flute",
     ["Lord of D.*2", "Blue-Eyes White Dragon*3", "Red-Eyes Black Dragon", "Luster Dragon*3", "Luster Dragon #2*2",
      "Spear Dragon*2", "Masked Dragon*2", "Tyrant Dragon", "Kaiser Glider", "Sangan", "Mirage Dragon",
      "Exiled Force", "Twin-Headed Behemoth",
      "The Flute of Summoning Dragon*2", "Stamping Destruction", "Dragon's Gunfire"] + GOAT_SPELLS +
     ["Nobleman of Crossout", "Book of Moon", "Swords of Revealing Light"] + GOAT_TRAPS + ["Sakuretsu Armor"],
     [], ["Lord of D.", "The Flute of Summoning Dragon", "Blue-Eyes White Dragon"], []),
    ("toons", "combo", "Toons", "Toons",
     ["Toon Gemini Elf*3", "Toon Masked Sorcerer*2", "Toon Cannon Soldier*2", "Toon Goblin Attack Force*2",
      "Toon Summoned Skull", "Toon Mermaid", "Toon Alligator", "Sangan", "Breaker the Magical Warrior",
      "Magician of Faith", "Morphing Jar", "D.D. Warrior Lady",
      "Toon World*3", "Toon Table of Contents*3"] + GOAT_SPELLS + ["Scapegoat", "Book of Moon",
                                                                    "Nobleman of Crossout"] +
     GOAT_TRAPS + ["Sakuretsu Armor*2", "Magic Cylinder"],
     [], ["Toon World", "Toon Table of Contents", "Toon Gemini Elf", "Toon Masked Sorcerer"],
     ["Toon Summoned Skull", "Toon Mermaid"]),
    ("espiritus", "tempo", "Espíritus", "Spirits",
     ["Tsukuyomi*3", "Asura Priest*2", "Susa Soldier*2", "Inaba White Rabbit", "Izanagi", "Yamato-no-Kami",
      "Airknight Parshath", "Jinzo", "Kycoo the Ghost Destroyer", "Spirit Reaper", "Magician of Faith", "Sangan",
      "Breaker the Magical Warrior", "Exiled Force", "D.D. Warrior Lady",
      "Creature Swap*2", "Spiritual Energy Settle Machine"] + GOAT_SPELLS + ["Book of Moon*2", "Nobleman of Crossout",
                                                                              "Scapegoat*2"] +
     GOAT_TRAPS + ["Sakuretsu Armor*2"],
     [], ["Tsukuyomi", "Asura Priest", "Izanagi", "Creature Swap"], ["Yamato-no-Kami"]),
    ("exodia_biblioteca", "alternativa", "Exodia Biblioteca", "Library Exodia",
     ["Royal Magical Library*3", "Exodia the Forbidden One", "Left Arm of the Forbidden One",
      "Right Arm of the Forbidden One", "Left Leg of the Forbidden One", "Right Leg of the Forbidden One",
      "Magician of Faith*2", "Sangan", "Big Shield Gardna", "Magical Merchant",
      "Upstart Goblin*2", "Pot of Greed", "Graceful Charity", "Magical Mallet", "Reload*2",
      "Swords of Revealing Light", "Messenger of Peace*2", "Level Limit - Area B*2", "Emergency Provisions*2",
      "Spell Economics", "Scapegoat*3", "Delinquent Duo", "Book of Moon*2",
      "Jar of Greed*3", "Gravity Bind*2", "Waboku"],
     [], ["Royal Magical Library", "Upstart Goblin", "Reload", "Exodia the Forbidden One"], []),
]

# a quien va cada mazo (variantes de torneo): genericos de nivel 7-8 y personajes fuertes, segun su estilo
ASIGNA = {
    # genericos nivel 7
    "noche": ["escorpiones", "guardianes_tumba"],
    "abisal": ["monarcas", "flip_control"],
    "acero": ["guerreros_toolbox", "gearfried_equipo", "md_guerreros_unidos"],
    "arcano": ["exodia_biblioteca", "relinquished", "espiritus"],
    "titan": ["maquinas"],
    # genericos nivel 8
    "leyenda": ["dragones_flauta", "md_horus"],
    "caos": ["chaos_control", "goat_control"],
    "cabra": ["goat_control", "flip_control", "quema_bloqueo"],
    "quimera": ["relinquished", "toons", "goat_control"],
    "averno": ["zombis", "md_sirvientes"],
    # personajes fuertes (con su estilo de la serie)
    "kaiba": ["dragones_flauta"],
    "pegasus": ["toons"],
    "marik": ["flip_control"],
    "ishizu": ["guardianes_tumba"],
    "alister": ["maquinas"],
    "valon": ["gearfried_equipo"],
    "joey": ["gearfried_equipo"],   # (rama classic-torneo-extras: decision del usuario, aunque Joey es de nivel 4)
    "yami_bakura": ["md_sirvientes"],
    "yami_marik": ["md_simochi", "quema_bloqueo"],
    "yugi": ["md_exodia_robo"],
    "gozaburo": ["exodia_biblioteca"],
    "rafael": ["quema_bloqueo"],
    "mahad": ["chaos_control"],
    "altos_sacerdotes": ["espiritus"],
    "dartz": ["monarcas"],
    "sacerdote_seto": ["md_horus"],
    "bakura_rey": ["escorpiones"],
    "zorc": ["zombis"],
    "yami_yugi": ["goat_control", "chaos_control"],
}

# ------------------------------------------------------------------------------------------------ cartas
def slug(n):
    return re.sub(r"[^a-z0-9]+", "_", n.lower()).strip("_")


C, POR_EN = {}, {}
for l in open(os.path.join(UI, "cartas_ui_en.txt"), encoding="utf-8"):
    p = l.rstrip("\n").split("|")
    if len(p) >= 9:
        C[p[0]] = dict(id=p[0], en=p[1], clase=p[2], attr=p[3], raza=p[4], nivel=max(0, int(p[5]) if p[5].lstrip("-").isdigit() else 0),
                       atk=p[6], texto=p[8], limite=3, subtipo=p[2], firma=False)
        POR_EN[p[1].lower()] = p[0]
for l in open(os.path.join(UI, "cartas_classic.txt"), encoding="utf-8"):
    p = l.rstrip("\n").split("|")
    if len(p) >= 4 and p[0] in C:
        C[p[0]]["limite"] = int(p[2])
        C[p[0]]["subtipo"] = p[3]
        C[p[0]]["firma"] = len(p) > 4 and p[4] == "firma"
EXTRA = set()
for l in open(os.path.join(AQUI, "datos", "cartas_extra.txt"), encoding="utf-8").read().splitlines():
    if l.strip() and not l.startswith("#"):
        EXTRA.add(slug(l.split("|")[0]))
# recetas (las de rivales.txt, que salen del texto en ingles) y los que no se invocan de forma normal
FUSION, RITUAL, NOMI = {}, {}, set()
for f in ("rivales.txt", "personajes_rivales.txt"):
    for l in open(os.path.join(AQUI, "datos", f), encoding="utf-8"):
        p = l.rstrip("\n").split("|")
        if p[0] == "F" and len(p) >= 3:
            FUSION[p[1]] = p[2].split("+")
        elif p[0] == "T" and len(p) >= 3:
            RITUAL[p[1]] = p[2]
        elif p[0] == "N" and len(p) >= 2:
            NOMI.update(p[1].split())


def idd(nombre):
    i = POR_EN.get(nombre.lower())
    if not i:
        raise SystemExit("carta que no esta en el pool: %r" % nombre)
    return i


def expandir(lista):
    r = []
    for x in lista:
        n, k = (x.rsplit("*", 1)[0], int(x.rsplit("*", 1)[1])) if "*" in x else (x, 1)
        r += [idd(n)] * k
    return r


def monstruo(i):
    return C[i]["clase"] in ("normal", "efecto", "ritual", "fusion")


def validar(clave, main, extra, especiales):
    err = []
    if len(main) != 40:
        err.append("%d cartas en el principal" % len(main))
    if len(extra) > 15:
        err.append("%d en el de fusion" % len(extra))
    cnt = {}
    for i in main + extra:
        cnt[i] = cnt.get(i, 0) + 1
    for i, n in cnt.items():
        c = C[i]
        if n > min(3, c["limite"]):
            err.append("%s x%d (limite %d)" % (i, n, c["limite"]))
        if c["firma"]:
            err.append("%s es carta firma de un personaje" % i)
    for i in main:
        if C[i]["clase"] == "fusion":
            err.append("fusion en el principal: " + i)
        if i in NOMI and i not in especiales and C[i]["subtipo"] != "toon":
            err.append("no se invoca normal y el mazo no sabe sacarlo: " + i)
        if C[i]["subtipo"] == "toon" and "toon_world" not in cnt:
            err.append("Toon sin Toon World: " + i)
        if C[i]["clase"] == "ritual" and RITUAL.get(i) not in cnt:
            err.append("ritual sin su magia: " + i)
    for i in extra:
        if C[i]["clase"] != "fusion":
            err.append("no es fusion: " + i)
            continue
        con_poly = "polymerization" in cnt and all(m in cnt for m in FUSION.get(i, ["?"]))
        niveles = {C[m]["nivel"] for m in main if monstruo(m)} | ({1} if "scapegoat" in cnt else set())
        con_meta = "metamorphosis" in cnt and C[i]["nivel"] in niveles
        if not (con_poly or con_meta):
            err.append("fusion sin materiales+Polymerization ni Metamorphosis: " + i)
    if "polymerization" in cnt and not extra:
        err.append("Polymerization sin fusiones")
    if err:
        raise SystemExit("mazo %s INVALIDO:\n  " % clave + "\n  ".join(err))


def main():
    out = ["# mazos_torneo.txt - generado por remake-ps5/mazos_torneo.py (listas a mano; no editar). Ver docs/mazos_torneo.md",
           "# D|clave|plan|nombre ES|nombre EN ; M|clave|ids (principal, tal cual) ; X|clave|ids (fusion) ; K|clave|ids clave ; A|rival|claves"]
    claves = set()
    PLANES = {"agresivo", "control", "bloqueo", "enjambre", "combo", "castigo", "molino", "tempo", "alternativa"}
    resumen = []
    for clave, plan, es, en, main_, extra_, k, esp in MAZOS:
        assert plan in PLANES, plan
        main_l, extra_l = expandir(main_), expandir(extra_)
        validar(clave, main_l, extra_l, {idd(x) for x in esp})
        claves.add(clave)
        main_l.sort(key=lambda i: (not monstruo(i), C[i]["clase"], i))
        out.append("D|%s|%s|%s|%s" % (clave, plan, es, en))
        out.append("M|%s|%s" % (clave, " ".join(main_l)))
        out.append("X|%s|%s" % (clave, " ".join(sorted(extra_l))))
        out.append("K|%s|%s" % (clave, " ".join(idd(x) for x in k)))
        ms = [i for i in main_l if monstruo(i)]
        resumen.append("# %-20s %-11s %2d monstruos, %2d magias, %2d trampas, %d fusiones" % (
            clave, plan, len(ms), sum(C[i]["clase"] == "magia" for i in main_l), sum(C[i]["clase"] == "trampa" for i in main_l), len(extra_l)))
    for r, ks in ASIGNA.items():
        for k in ks:
            assert k in claves, k
        out.append("A|%s|%s" % (r, " ".join(ks)))
    out += resumen
    open(SALIDA, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    print("mazos de torneo: %d, rivales con mazo de torneo: %d -> %s" % (len(MAZOS), len(ASIGNA), SALIDA))
    for l in resumen:
        print(l)


if __name__ == "__main__":
    main()
