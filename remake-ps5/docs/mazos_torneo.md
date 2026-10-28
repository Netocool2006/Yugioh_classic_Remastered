# Mazos de torneo (rama classic-mazos-torneo)

Pedido: "llegar al nivel de mazos de Master Duel con las cartas que ya tenemos", respetando la era clásica (monstruos,
sacrificios, magias, trampas, fusión y ritual; nada de Péndulo, Sincronía, Xyz ni Link).

## Qué hay
- **21 mazos curados a mano** (no generados por tema): 5 inspirados en recetas de Master Duel y 16 arquetipos de
  torneo de la era Goat (abril-septiembre 2005 y alrededores). Las listas están en `mazos_torneo.py` (nombres TCG en
  inglés, `"Nombre*2"` = 2 copias); `python -I mazos_torneo.py` las valida y escribe `datos/mazos_torneo.txt`.
- **Solo cartas de nuestro pool**: las 1690 Goat de `cartas_classic.txt` y las cartas extra (`datos/cartas_extra.txt`),
  sin cartas firma (Marshmallon y Blue-Eyes Ultimate Dragon son firma de personaje y no entran). **Rama
  classic-torneo-extras:** las 10 cartas que faltaban (Command Knight, Cyber Dragon, Foolish Burial, Goblin Zombie, Il
  Blud, Magical Mallet, Izanagi, Yamato-no-Kami, Obnoxious Celtic Guard, Drillroid) se agregaron como cartas extra con su
  efecto en el motor, y las listas ya las usan en lugar de los sustitutos (ver "Cambios de la rama classic-torneo-extras"). **Banlist del juego** (la Goat de
  `cartas_classic.txt`): Chaos Emperor Dragon, Witch of the Black Forest, Monster Reborn, Raigeki, Dark Hole, Change of
  Heart, Harpie's Feather Duster, Confiscation, Painful Choice, Yata-Garasu, Fiber Jar, Magical Scientist... están
  prohibidas y no aparecen; las limitadas van a 1 y las semilimitadas (Upstart Goblin, Emergency Provisions, Good
  Goblin Housekeeping, Nobleman of Crossout, Reinforcement of the Army, Marauding Captain, Vampire Lord, Night Assailant,
  Gravity Bind, Level Limit - Area B, Creature Swap, Abyss Soldier, Chick the Yellow, Manticore) a 2 como mucho.
- Validación de `mazos_torneo.py` (y otra vez en `prueba_torneo.cpp`): 40 cartas justas, máx. 3 copias y límites,
  ninguna fusión en el principal, fusiones con sus materiales y Polymerization **o** con Metamorphosis y un monstruo
  (o ficha de Scapegoat) de su nivel, rituales con su magia, Toons con Toon World, y los que no se invocan de forma
  normal solo si el mazo sabe sacarlos (Black Luster Soldier - Envoy y Chaos Sorcerer por su procedimiento, Horus LV8
  con Horus LV6, Gearfried the Swordmaster con Release Restraint, Toon Summoned Skull/Toon Mermaid con Toon World).

## Cómo los usa el juego (sin diluirlos)
- `datos/mazos_torneo.txt`: `D|clave|plan|nombre ES|nombre EN`, `M|clave|ids` (principal), `X|clave|ids` (fusión),
  `K|clave|ids` (cartas clave) y `A|rival|claves` (a quién va). Lo lee `ia::Base::leerTorneo` (desde `leerVariedad`,
  después de los rivales; `sincronizar.sh` lo copia al título).
- No son variantes de `principal`: van aparte (`Rival::torneo`), así el mazo dinámico, el pool del rival y el índice de
  cartas por nivel no los ven (ninguna carta de torneo se cuela en los mazos de los demás rivales ni sube su ATK).
- `prepararRival` (ia_plan.h) sortea primero si sale un mazo de torneo: **40 % en los genéricos de nivel 7-8, 30 % en
  los personajes (40 % desde el nivel 7)**, nunca el mismo dos duelos seguidos (la memoria guarda el último mazo). Si
  sale, se juega **tal cual** (mismas cartas, sin reequilibrar el ATK ni cambiar nada) y con su plan (casi siempre el
  suyo; a veces otro que su mazo permita, y puede cambiar de plan a mitad de duelo como siempre). Si no, el mazo
  dinámico de siempre. Los niveles 1-6 genéricos y los personajes de nivel 1-4 no tienen ninguno, salvo **Joey** (nivel
  4: Gearfried y Equipos, decisión del usuario).
- En el modo historia el nombre del mazo sale como "Goat Control (torneo)".
- La IA: combos de cada arquetipo en `datos/combos.txt` (bono al jugar sus piezas, ia_plan.h) y papeles nuevos en
  `roles_gen.py` (`MANO.update`: Bad Reaction to Simochi y Rain of Mercy queman, Good Goblin/Provisions/Reload roban,
  Royal Decree niega, Book of Moon quita, Metamorphosis/Necrovalley/The A. Forces son combo...). Dos arreglos de IA
  que salieron al probarlos: **Exodia** se junta en la mano (no invoca ni descarta piezas mientras queden 4+ vivas;
  `valorEnMano` de una pieza 2500) y **no robar de más con el mazo casi vacío** (las cartas de robo pierden puntos
  con 6 o menos en el mazo; antes los mazos de robo perdían por quedarse sin cartas).

## Asignación (nivel / personaje)
| Mazo | Rivales |
|---|---|
| Familia de Sirvientes (MD) | Averno (n8), Yami Bakura (6) |
| Dragón de Llama Negra (MD) | Leyenda (n8), Sacerdote Seto (7) |
| Exodia Codicioso (MD) | Yugi (6) |
| Poder del Fuego (MD) | Yami Marik (6) |
| Coalición Guerrera (MD) | Acero (n7) |
| Goat Control | Caos (n8), Cabra (n8), Quimera (n8), Yami Yugi (8) |
| Chaos Control | Caos (n8), Mahad (6), Yami Yugi (8) |
| Guerreros Toolbox | Acero (n7) |
| Flip Control | Abisal (n7), Cabra (n8), Marik (5) |
| Zombis Vampiro | Averno (n8), Zorc (8) |
| Gearfried y Equipos | Acero (n7), Valon (5), Joey (4) |
| Escorpiones Oscuros | Noche (n7), Bakura Rey de los Ladrones (7) |
| Monarcas | Abisal (n7), Dartz (7) |
| Máquinas Antiguas | Titán (n7), Alister (5) |
| Quema y Bloqueo | Cabra (n8), Yami Marik (6), Rafael (6) |
| Guardianes de la Tumba | Noche (n7), Ishizu (5) |
| Ritual Relinquished | Arcano (n7), Quimera (n8) |
| Señor de los Dragones | Leyenda (n8), Kaiba (5) |
| Toons | Quimera (n8), Pegasus (5) |
| Espíritus | Arcano (n7), Altos Sacerdotes (6) |
| Exodia Biblioteca | Arcano (n7), Gozaburo (6) |

## Fuentes y razonamiento de cada lista (solo listas de cartas)
Las de Master Duel salen de las descripciones de sus recetas (`D:\Yugi-Reload\listas\mazos_master_duel_vs_classic.md`:
el juego no guarda las listas en local, así que se toman la carta estrella y el plan que describe la receta y se
completa con el armazón Goat de la época). Las de torneo siguen las listas que se jugaban en el formato Goat
(TER/abril 2005 y la lista de septiembre de 2005): el "armazón" común (Pot of Greed, Graceful Charity, Heavy Storm,
Delinquent Duo, Snatch Steal, Premature Burial, Mystical Space Typhoon; Mirror Force, Ring of Destruction, Call of
the Haunted, Torrential Tribute, Sakuretsu Armor; Sangan, Breaker, Tribe-Infecting Virus, D.D. Warrior Lady, Exiled
Force, Magician of Faith, Morphing Jar) más el motor de cada arquetipo.

### Inspirados en Master Duel
- **Familia de Sirvientes** ("Illustrious Skull Servant Family"): Skull Servant x3 + King of the Skull Servants x3
  (1000 de ATK por cada uno en el cementerio). Llenar el cementerio: Graceful Charity, Card Destruction, Magical
  Merchant x2; Pyramid Turtle x3 recluta al Rey (DEF 0) y a los Sirvientes; Book of Life los recupera. Zombis de
  apoyo: Spirit Reaper x3, Vampire Lord x2, Ryu Kokki. *Sustituciones*: Foolish Burial no está en el pool (lo cubren
  Magical Merchant y Graceful Charity); Wightmare/Wightprince son posteriores.
- **Dragón de Llama Negra** ("☆3 Black Flame Dragon"): Horus LV4/LV6/LV8 3-3-2 + Level Up! x2 + Royal Decree x2
  (Horus LV8 niega magias, Royal Decree las trampas), con Masked Dragon, Luster Dragon y Spear Dragon.
- **Exodia Codicioso** ("Greedy Goblin"): las 5 piezas + Good Goblin Housekeeping x2, Emergency Provisions x2 (en
  respuesta a Good Goblin, roba más), Jar of Greed x3, Reckless Greed, Upstart Goblin x2, Reload x2; muro de bloqueo
  (Messenger of Peace, Level Limit - Area B, Gravity Bind, Threatening Roar, Waboku, Scapegoat). *Sustituciones*:
  Marshmallon (firma) -> Big Shield Gardna; sin Card Destruction ni Morphing Jar (descartan la mano).
- **Poder del Fuego** ("Power to the Fire"): Bad Reaction to Simochi x3 convierte en daño lo que gana el rival: Rain
  of Mercy x3 (los dos ganan 1000), Upstart Goblin x2, Snatch Steal; Fire Princess x3 quema 500 cada vez que ganas
  LP; Magic Cylinder, Just Desserts, Secret Barrel, Ceasefire, Ookazi, Poison of the Old Man y bloqueo (Gravity Bind,
  Level Limit). *Sustitución*: Marshmallon -> Spirit Reaper.
- **Coalición Guerrera** ("Coalition Formed"): The A. Forces x3 (+200 por Guerrero/Lanzador) y United We Stand (+800
  por monstruo), Marauding Captain x2, Reinforcement of the Army x2, Goblin Attack Force x2, Gearfried, Mystic
  Swordsman LV2, Freed. *Sustituciones*: Command Knight no está en el pool -> Blindly Loyal Goblin x2 y Ninja
  Grandmaster Sasuke; Obnoxious Celtic Guard -> D.D. Survivor.

### Era Goat
- **Goat Control**: el mazo que dio nombre al formato. Scapegoat x2 + Metamorphosis (ficha de nivel 1 ->
  Thousand-Eyes Restrict x2 en el extra), Black Luster Soldier - Envoy, Airknight Parshath x2, Tsukuyomi, Jinzo,
  Abyss Soldier, Asura Priest, Kycoo, Spirit Reaper; Book of Moon, Nobleman x2, Smashing Ground, Enemy Controller,
  Swords; Magic Cylinder y Solemn Judgment.
- **Chaos Control** (Chaos Turbo): Chaos Sorcerer x3, Envoy, Dark Magician of Chaos, enlazado con LUZ y OSCURIDAD
  (Shining Angel, Mystic Tomato, Night Assailant). **Chaos Emperor Dragon está prohibido en la banlist del juego**:
  no entra (decisión anotada).
- **Guerreros Toolbox**: Marauding Captain x2, Reinforcement of the Army x2, D.D. Assailant x2, Mystic Swordsman LV2
  x2, Don Zaloog x2, Freed, Zombyra, Gearfried, Envoy. *Sustituciones*: Command Knight -> Blindly Loyal Goblin x2;
  Blade Knight no está en el pool; Obnoxious Celtic Guard -> D.D. Survivor.
- **Flip Control**: Book of Moon x3 + Book of Taiyou, Magician of Faith x2, Penguin Soldier x2, Hane-Hane x2, Old
  Vindictive Magician x2, Night Assailant x2, Stealth Bird, Guardian Sphinx, Cyber Jar, Morphing Jar, Man-Eater Bug,
  Mask of Darkness. (Fiber Jar prohibida.)
- **Zombis Vampiro**: Pyramid Turtle x3, Vampire Lord x2, Ryu Kokki x2, Spirit Reaper x2, Despair from the Dark,
  Patrician of Darkness, Dark Dust Spirit, Master Kyonshee, Vampire Lady; Book of Life x2, Card of Safe Return.
  *Sustituciones*: Goblin Zombie e Il Blud no están -> Double Coston x2 y Fear from the Dark.
- **Gearfried y Equipos**: Gearfried the Iron Knight x3 + Release Restraint x2 -> Gearfried the Swordmaster x2
  (destruye un monstruo por cada equipo); Axe of Despair x2, Malevolent Nuzzler, Mage Power, Fairy Meteor Crush, Big
  Bang Shot, Lightning Blade. *Sustitución*: Command Knight -> Blindly Loyal Goblin x2.
- **Escorpiones Oscuros**: Don Zaloog x3 + los cinco Escorpiones (Chick x2, la semilimitada), Mustering of the Dark
  Scorpions x2, Dark Scorpion Combination, Marauding Captain x2 y Reinforcement x2.
- **Monarcas**: Zaborg x2, Mobius x2, Thestalos, Granmarg; los sacrificios de Mystic Tomato, Scapegoat y Soul
  Exchange / Brain Control. (Treeborn Frog y Caius son posteriores.)
- **Máquinas Antiguas**: Ancient Gear Golem, Ancient Gear Beast x2, Jinzo, Reflect Bounder, Mechanicalchaser x3,
  Robotic Knight x2, Drillago x2; Limiter Removal x2 y Machine Duplication. *Sustitución*: **Cyber Dragon no está en
  el pool** (es de Cybernetic Revolution, agosto 2005): el mazo de Máquinas sigue el plan de Limiter Removal; Drillroid
  -> Drillago.
- **Quema y Bloqueo**: Wave-Motion Cannon x2, Level Limit - Area B x2, Messenger of Peace x2, Gravity Bind x2,
  Ceasefire, Just Desserts x2, Secret Barrel x2, Ookazi x2, Tremendous Fire x2, Magic Cylinder, Stealth Bird x2, Des
  Koala x2, Cannon Soldier. *Sustitución*: Marshmallon (firma) -> Big Shield Gardna x2. (Lava Golem no se invoca
  normal y el motor no lo saca por su procedimiento en los mazos: fuera.)
- **Guardianes de la Tumba**: Necrovalley x3, Gravekeeper's Spy x3, Spear Soldier x2, Assailant x2, Curse x2, Guard
  x2, Chief, Cannonholder; Royal Tribute y Rite of Spirit.
- **Ritual Relinquished**: Relinquished x3 + Black Illusion Ritual x3, buscadores Sonic Bird x2 (magia ritual), Manju
  x2 y Senju (monstruo ritual).
- **Señor de los Dragones**: Lord of D. x2 + The Flute of Summoning Dragon x2 con Blue-Eyes x3, Red-Eyes, Luster Dragon
  x3, Luster Dragon #2 x2, Spear Dragon x2, Masked Dragon x2, Tyrant Dragon, Mirage Dragon, Kaiser Glider.
  *Sustitución*: Blue-Eyes Ultimate Dragon es carta firma -> sin Polymerization (Dragon's Gunfire en su lugar).
- **Toons**: Toon World x3 + Toon Table of Contents x3, Toon Gemini Elf x3, Toon Masked Sorcerer x2, Toon Cannon
  Soldier x2, Toon Goblin Attack Force x2, Toon Summoned Skull, Toon Mermaid, Toon Alligator.
- **Espíritus**: Tsukuyomi x3, Asura Priest x2, Susa Soldier x2, Inaba White Rabbit, Creature Swap x2 y Spiritual
  Energy Settle Machine, con el armazón de control (Airknight, Jinzo, Kycoo, Spirit Reaper, Mystic Tomato).
  *Sustituciones*: Yata-Garasu prohibido; Izanagi y Yamato-no-Kami no están en el pool. Se probó con más Espíritus
  (Otohime, Maharaghi, Hino-Kagu-Tsuchi, Great Long Nose) y ganaba el 35 %: se dejaron los 8 mejores.
- **Exodia Biblioteca**: Royal Magical Library x3 + las 5 piezas, Upstart Goblin x2, Reload x2, Emergency Provisions
  x2, Spell Economics, Jar of Greed x3 y bloqueo (Messenger, Level Limit, Gravity Bind, Scapegoat x3, Waboku,
  Threatening Roar). *Sustituciones*: Magical Mallet no está en el pool; sin Card Destruction, Card of Sanctity ni
  Morphing Jar (mandan las piezas de la mano al cementerio o al destierro).

## Medición (prueba_torneo.cpp, 20 duelos por mazo contra los temáticos de nivel 8, IA clásica, nivel 8)
Ver la tabla del último ciclo en `docs/ciclos_pruebas.md` o en el log `prueba_torneo.log` del ciclo. Al cerrar la rama:
los de torneo ganan el **73 %** (308/420) a los temáticos de nivel 8, 0 jugadas ilegales, 0 duelos sin terminar;
todos los mazos juegan sus cartas clave. Por mazo: Simochi, Horus, Quema y Bloqueo 95-100 %; Guerreros, Gravekeepers 85-90 %; Goat Control 70 %, Chaos Control 75 %; Exodia 45-60 %; Escorpiones Oscuros el más flojo (25 %). Los combos de torneo de `combos.txt` llevan el campo `torneo`: solo dan bono en el duelo y no arman mazos dinámicos (así no cambian a los demás rivales: `prueba_variedad` lo mide).

## Cambios de la rama classic-torneo-extras
Decisiones del usuario sobre `docs/decisiones_pendientes.md` (todas resueltas): las probabilidades, los personajes de
nivel 5-6 y Chaos Control sin Chaos Emperor Dragon se quedan; Joey lleva Gearfried y Equipos; las 10 cartas que faltaban
se agregan al juego, y los mazos de torneo se ofrecen al jugador en el constructor.

- **Cartas exactas** (las 10 son cartas extra: `datos/cartas_extra.txt`, datos ES/EN oficiales de YGOPRODeck y la base
  es-ES de ygopro, arte remasterizado por Gemini con el prompt de siempre (`cola_extra.py`), caras ES/EN/HD con
  `preparar_classic.py`, efecto en `ia.h` (`cartasTorneoExtras`) y su caso en `prueba_efectos.cpp`):
  - Familia de Sirvientes: + Foolish Burial (limitada) en lugar de Enemy Controller.
  - Coalición Guerrera: Command Knight x3 en lugar de Blindly Loyal Goblin x2 y Ninja Grandmaster Sasuke; Obnoxious
    Celtic Guard en lugar de D.D. Survivor.
  - Guerreros Toolbox: Command Knight x2 (en lugar de Blindly Loyal Goblin x2) y Obnoxious Celtic Guard (en lugar de
    D.D. Survivor).
  - Zombis Vampiro: Goblin Zombie x2 e Il Blud en lugar de Double Coston x2 y Fear from the Dark.
  - Gearfried y Equipos: Command Knight x2 en lugar de Blindly Loyal Goblin x2.
  - Máquinas Antiguas: Cyber Dragon x3 (en lugar de 2 Mechanicalchaser y Cyber Raider) y Drillroid x2 (en lugar de
    Drillago x2).
  - Espíritus: Izanagi y Yamato-no-Kami en lugar de Mystic Tomato y Morphing Jar.
  - Exodia Biblioteca: Magical Mallet en lugar de Threatening Roar.
- **Regla Géminis** (Il Blud, subtipo `geminis`): boca arriba en el campo es un monstruo normal; con la invocación normal
  del turno se invoca de nuevo (en el duelo: "Invocación normal (Géminis)" sobre la carta) y gana su efecto; boca abajo
  lo pierde. Se puede responder como a cualquier invocación normal (Torrential Tribute...).
- **Joey**: `A|joey|gearfried_equipo` (sale con la probabilidad de los personajes, 30 %).
- **Constructor**: Triángulo en MIS MAZOS abre **MAZOS DE TORNEO** (los 21, con vista previa); X: Copiar a mis mazos /
  Copiar y editar; Cuadrado copia directo. La copia es un mazo más de MIS MAZOS (ver `docs/constructor.md`).
