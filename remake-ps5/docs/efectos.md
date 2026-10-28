# Motor de efectos de cartas (rama classic-efectos)

Yugioh Classic Remaster, reglas de la epoca (Formato Goat, 2005). El duelo autoritativo sigue siendo `ia::Duelo`
(`src/ia.h`); el motor vive dentro de el (secciones "motor de efectos: datos", "motor de efectos" y "las cartas").
Implementacion propia en C++: el diseno de ocgcore/EDOPro se estudio solo como referencia de reglas (nada copiado).

## Contador

**Cartas con su efecto en el motor: 1335** (DefCarta propia); con las que funcionan por regla (Polymerization, magias de ritual, Toons: 28) y las que no tienen efecto (normales y fusiones sin efecto: 412). **El estado real lo da la certificación de la rama classic-probador** (`docs/certificacion_efectos.md` y `.csv` en esa rama): de las 1383 cartas con efecto, OK 1319, FALLA 24 (fusiones con efecto que el motor no tiene: Dark Paladin, Thousand-Eyes Restrict, Dark Flare Knight, XYZ-Dragon Cannon y sus partes...) y SIN PRUEBA 40; además 5 Toons con efecto propio no lo tienen. La lista de pendientes del final solo cuenta las cartas sin DefCarta, por apariciones en los mazos de rivales.txt y personajes_rivales.txt.

## Diseno

### Datos por carta (`DefCarta`, `tablaCartas()`)

Data-driven + codigo por carta. Cada carta con efecto registra una `DefCarta`:

| campo | que es |
|---|---|
| `objetivo`, `nObj` | tipo de objetivo (`O_MT`, `O_MONSTRUO_RIVAL`, `O_CEM_MONSTRUO`, `O_ATACANTE`, `O_INVOCADO`...) y cuantos |
| `costeLP`, `descartes` | costes (LP fijos o la mitad; cartas de la mano) |
| `permanece` | continua / equipo / campo: se queda en el campo tras resolver |
| `ignicion`, `disparo` | monstruos: efecto que se activa a mano (una vez por turno) o que se dispara solo (volteo, invocado, al cementerio, destruido en batalla) |
| `condicion()` | cuando se puede activar (ademas de la velocidad y el lugar) |
| `objetivos()` | candidatos propios si el tipo generico no alcanza |
| `resolver()` | el efecto; puede pedir una eleccion (`preguntar`) y seguir despues (`Eslabon::paso`) |
| `valor()` | IA: cuanto vale activarla ahora (puntos ~ ATK) |
| `alSalir()` | al dejar el campo (Premature Burial destruye al monstruo, Snatch Steal lo devuelve) |
| `modAtk()`, `modDef()`, `impideAtaque()` | efectos continuos (equipos, campos, Gravity Bind...) |
| `espera()`, `finTurno()` | fase de espera / final (Snatch Steal da 1000 LP al rival, Swords cuenta turnos) |

La velocidad de hechizo sale del subtipo de `cartas_classic.txt` (formato sin cambios): magia normal/continua/equipo/
campo/ritual y efectos de monstruo 1; magia rapida y trampas 2; trampas de contraataque 3.

### Activar y la cadena

1. `J_ACTIVAR` (de la mano, una colocada o el efecto de un monstruo) con sus objetivos (`Jugada::objs`) y descartes
   (`Jugada::descartes`). `motivoNoActivar()` dice por que no (turno, fase, velocidad, colocada este turno, condicion,
   objetivos, LP, descartes, zona libre).
2. La carta pasa boca arriba a su zona (de la mano: a una zona libre; la de campo, a su zona y la otra boca arriba se
   va), se pagan los costes y entra como eslabon en `ef.cadena` (pila LIFO). Suceso `S_ACTIVA_MT`/`S_ACTIVA_EFECTO` con
   el numero de eslabon.
3. El otro lado recibe la ocasion de responder (`ef.preg = P_RESPUESTA`) **solo si tiene algo legal** (velocidad >= la
   del ultimo eslabon y >= 2, y su condicion); si no, pasa solo. Quien acaba de pasar le da la ocasion al otro; dos
   pases seguidos resuelven.
4. Se resuelve de arriba hacia abajo. Un eslabon negado no hace nada; una continua/equipo destruida antes de resolver
   no aplica nada; una magia/trampa normal destruida en la cadena resuelve igual (como en las reglas). Al terminar,
   las normales van al cementerio y las continuas se quedan.
5. Si un `resolver` pide elegir (`P_ELEGIR`: descartar con Graceful Charity, desempatar Fissure), la cadena se
   detiene hasta `J_ELEGIR` y sigue desde ese eslabon.
6. Los efectos que se disparan durante una cadena o un combate (`ef.disparos`) arman una cadena nueva al terminar
   (primero los del jugador del turno).

### Ventanas de respuesta (`ef.ventana`)

| ventana | cuando | ejemplos |
|---|---|---|
| `V_INVOCACION_NORMAL`, `V_VOLTEO` | tras invocar normal o por volteo (colocar no abre) | Trap Hole |
| `V_INVOCACION_ESPECIAL` | fusion, ritual, Toon y las de efectos (tras la cadena que las hizo) | Bottomless, Torrential |
| `V_ATAQUE` | tras declarar un ataque: el combate espera (`PEND_BATALLA`) | Mirror Force, Sakuretsu, Waboku |
| `V_ACTIVACION` | una activacion en estado abierto | contraataques, MST |
| `V_FASE_FINAL` | la fase final del turno: ultima ocasion del rival; luego pasa el turno (`PEND_FIN_TURNO`) | MST, Call of the Haunted |

El combate (`Duelo::combate`) se hace al cerrar la ventana de ataque: volteo del defensor, calculo con el ATK/DEF de
verdad (`Monstruo::atkEf/defEf`, recalculado con cada suceso), destruccion y danno (`S_COMBATE`: lo que anima la
mesa). `Duelo::decide()` dice quien tiene que jugar ahora (el que tiene la pregunta o el del turno);
`Duelo::ocupado()` bloquea las jugadas normales mientras haya cadena, pregunta o combate a medias.

### Lo que ve el jugador

- **Activar desde el menu**: magias de la mano ("Activar"), magias/trampas colocadas (su zona: "Activar") y efectos
  de monstruo ("Activar efecto"). Apagado con el motivo si ahora no se puede (en el idioma elegido).
- **Costes y objetivos con el mando**: si los candidatos estan en la mesa o la mano, se eligen con la cruceta y X
  (estado `ELEGIR_CARTA`, el mismo foco de siempre, solo se mueve entre los validos; O vuelve atras); si estan en un
  cementerio, una lista. Con un solo candidato posible (el atacante, el recien invocado) se elige solo.
- **La pregunta del duelo**: cuando el rival hace algo a lo que el jugador puede responder, se abre solo un menu con
  el titulo de lo que paso ("El rival ataca con Summoned Skull", "El rival activó Raigeki", "Fase final del rival")
  y las cartas que puede activar ("Activar: Mirror Force"...) mas "No activar" (tambien con O). Para elegir al
  resolver (Graceful Charity), el foco salta a las cartas validas.
- **La cadena en orden**: cada activacion muestra la carta boca arriba en su zona, su animacion de Master Duel
  (`efectos_cartas.h`; la generica de magia/trampa si no tiene) y `sfx_activar`, con el aviso "Cadena 2: Mirror
  Force"; la presentacion espera a la animacion (hasta 1,8 s) antes de seguir; cada eslabon que resuelve deja la mesa
  al dia (destrucciones, LP) antes del siguiente. Ganancias y pagos de LP se ven en el marcador.
- El rival (IA) contesta las preguntas con su `Cerebro` (pausa corta de "pensar").

### La IA (`Cerebro`)

- `decidirPregunta()`: en una ventana activa la mejor carta si su `valor()` (+ ruido segun el nivel) pasa un umbral
  que depende de la personalidad (paciente espera mas, temeraria gasta antes); si no, pasa. Al elegir: lo que menos
  vale si es descartar/sacrificar; lo que mas vale si es destruir del rival o traer.
- En su fase principal (y de batalla) las activaciones son candidatas como cualquier jugada (`valor()` - 100).
- Objetivos (`objetivosIA`): lo del rival que mas vale; lo propio solo si no queda otra; del cementerio lo mas fuerte.
- Colocar: una trampa con efecto vale mas que un farol; una magia normal con efecto se guarda para activarla.
- Si el rival le responde un ataque (Mirror Force...), vuelve a temer a las cartas boca abajo (`ef.castigos`).

## Cartas implementadas

### Tanda 1: destruccion masiva y dirigida
Raigeki, Dark Hole, Mirror Force, Heavy Storm, Harpie's Feather Duster, Mystical Space Typhoon, Fissure (empate:
elige quien la activa), Tribute to the Doomed (descarte como coste), Sakuretsu Armor, Trap Hole.

### Tanda 2: robo, recuperacion y LP
Pot of Greed, Graceful Charity (elige los 2 descartes), Monster Reborn (cualquier cementerio; vuelve al cementerio de
su duenno), Premature Burial (800 LP, equipo vinculado), Call of the Haunted (trampa continua vinculada), Change of
Heart (vuelve en la fase final), Snatch Steal (equipo: control; el rival gana 1000 LP en cada fase de espera suya),
Dian Keto the Cure Master, Card Destruction, Upstart Goblin.

### Tanda 3: equipos y modificadores
Axe of Despair (+1000), Black Pendant (+500; 500 al rival al ir al cementerio), Megamorph (x2 / mitad del ATK
original segun los LP), United We Stand (+800 por monstruo boca arriba propio), Mage Power (+500 por magia/trampa
propia); campos Forest, Sogen, Yami, Umi, Wasteland, Mountain (+200/-200 por tipo, a los dos lados; una sola de campo
boca arriba); Swords of Revealing Light (voltea, el rival no ataca, se va tras 3 turnos suyos), Messenger of Peace
(1500+ no atacan; 100 LP en tu fase de espera), Gravity Bind (nivel 4+ no atacan). Los rotulos ATK/DEF de la mesa
muestran el valor de verdad (verde si sube, rojo si baja; `Interfaz::statsZona`). La Forest del jugador hace crecer
el bosque 3D de la rama campo-bosque. El menu de un monstruo dice "Un efecto le impide atacar".

### Tanda 4: trampas de batalla, danno y contraataques
Magic Cylinder, Negate Attack (termina la batalla), Waboku, Ring of Destruction (texto de cartas_ui_en.txt: monstruo
del rival en su turno, los dos reciben el ATK original; si los dos llegan a 0, empate; 1 por turno), Barrel Behind the
Door (desvia el danno de un efecto: Ring, Cylinder), Torrential Tribute, Solemn Judgment (mitad de LP; niega
magia/trampa o la invocacion), Magic Jammer (descarte; magias), Seven Tools of the Bandit (1000 LP; trampas), Bottomless
Trap Hole (destierra). Un eslabon negado no resuelve y su carta va al cementerio (aviso "Negado: carta").

### Tanda 5: monstruos con efecto de Goat
Volteo: Man-Eater Bug, Magician of Faith, Old Vindictive Magician, Penguin Soldier, Cyber Jar. Sangan y Witch of the
Black Forest (buscar en el mazo: lista; se baraja), Breaker the Magical Warrior (contador de magia por invocacion
normal, +300; quitarlo destruye una magia/trampa), Tsukuyomi, Jinzo (las trampas no se activan y sus efectos en el
campo se niegan), Black Luster Soldier - Envoy y Chaos Emperor Dragon (invocacion especial desde la mano por su
procedimiento: "Invocación especial" en el menu; no usa la cadena), Airknight Parshath (perforante, roba al hacer
danno), Tribe-Infecting Virus, Sinister Serpent, Exodia (5 piezas en la mano: victoria). Generales: Toons (Toon World
cuesta 1000 LP; no atacan el turno en que llegan; atacan directo si el rival no tiene Toons; los de "pagar 500" pagan
al declarar; si Toon World se va, se destruyen) y Spirits (vuelven a la mano en la fase final del turno en que se
invocaron normal o se voltearon; no se pueden traer del cementerio).

Motor: disparos por invocacion normal (solo normal) y por danno de batalla, contadores en monstruos con ATK por
contador, perforar, coste propio al activar, eleccion propia de la IA por carta.

### Tanda 6: Dioses Egipcios y cartas firma (texto de cartas_ui_en.txt)
Obelisk the Tormentor (nadie lo elige con efectos; sacrifica 2: destruye los monstruos del rival), Slifer the Sky
Dragon (+1000 por carta en la mano; lo que el rival invoca en ataque pierde 2000 y si queda en 0 se destruye), The
Winged Dragon of Ra (al invocarse puedes pagar hasta quedar en 100 LP y gana esa cantidad; paga 1000: destruye un
monstruo; no se puede invocar de forma especial). Los tres: 3 sacrificios, no se colocan, nadie responde a su
invocacion normal y, invocados de forma especial, se van en la fase final. The Seal of Orichalcos (+500, destruye tus
invocados de forma especial, sin fusiones, 1 por duelo), Crush Card Virus, Shrink, Multiply (fichas de Kuriboh),
Mystic Box, Nightmare's Steelcage, Dark Magic Curtain, Kunai with Chain, Marshmallon, The White Stone of Legend,
King's Knight, Jurassic World, Insect Armor with Laser Cannon, Celestial Sword - Eatos.

Motor: elecciones opcionales ("puedes": "No" en la lista, O con el foco), "1 por duelo", sin danno hasta un turno
(Crush Card Virus), fichas, invocado de forma especial, ATK/DEF permanentes y propios, indestructible en batalla.

### Tanda 7 (A): las magias y trampas mas usadas en los mazos de los rivales
Dust Tornado, Scapegoat (fichas), Stop Defense, Jar of Greed, Hinotama, Sparks, Red Medicine, Soul of the Pure, Castle
Walls, Reinforcements, Eatgaboon, House of Adhesive Tape, Enchanted Javelin, Book of Moon, Book of Taiyou,
Reinforcement of the Army, Enemy Controller, Trap Dustshoot, Delinquent Duo, Brain Control, Return from the Different
Dimension, The Nobleman of Crossout, Smashing Ground, Compulsory Evacuation Device, Ceasefire, Widespread Ruin, Creature
Swap, Limiter Removal; campos Gaia Power, Mystic Plasma Zone, Luminous Spark, A Legendary Ocean; equipos Book of Secret
Arts, Mystical Moon, Legendary Sword, Dark Energy, Elf's Light, Machine Conversion Factory, Fusion Sword Murasame Blade,
Dragon Treasure; Dragon's Gunfire, Stamping Destruction.

### Tanda 7 (B): los monstruos con efecto mas usados
Skull Archfiend of Lightning (paga 500 en tu fase de espera), Dancing Fairy, The Bistro Butcher, Zolga, Kycoo the Ghost
Destroyer, Hysteric Fairy, Ancient Gear Golem (perforante; al atacar el rival no activa magias/trampas; no vuelve del
cementerio), Mudora, Dark Magician Girl, Amazoness Paladin, Theban Nightmare, Kaiser Glider, Helping Robo for Combat,
Masked Dragon, Gearfried the Iron Knight, Reflect Bounder, Ninja Grandmaster Sasuke, Kelbek, D.D. Assailant, Blindly
Loyal Goblin, Invader of Darkness, Amphibious Bugroth MK-3, Drillago.

### Tanda 7 (C): siguientes por frecuencia
Ultimate Offering (repetible), Ultimate Obedient Fiend, Skilled White / Dark Magician (contadores de magia),
Uniones Y-Dragon Head y Z-Metal Tank (se equipan y desequipan 1 vez por turno), The Forgiving Maiden, Element Dragon,
Element Valkyrie, Machine Duplication, Arsenal Summoner, Gear Golem, Agido, Armed Dragon LV5, Slate Warrior,
reclutadores (Shining Angel, Flying Kamakiri #1, Mystic Tomato, Giant Rat, UFO Turtle, Mother Grizzly, Pyramid
Turtle), Freed the Brave Wanderer, Guardian Sphinx, Nightmare Penguin, Zombyra the Dark, Flash Assailant, Big Shield
Gardna, Getsu Fuhma, Mirage Dragon, D.D. Survivor, Twin-Headed Behemoth, Maiden of the Aqua. En el menu de un monstruo
que puede atacar directo por un efecto: "Atacar directamente".

### Tanda 7 (D)
Gale Lizard, Nuvia the Wicked, Zaborg y Mobius (al invocarse con sacrificio), Cannon Soldier, Maha Vailo, Maryokutai
(efecto rapido de monstruo), Revival Jam, Abyss Soldier, Don Turtle, Emissary of the Afterlife, Gravekeeper's Spy,
Kuriboh (desde la mano en el paso de danno), Lady Panther, Machine King, Moai Interceptor Cannons, Book of Life, Call
of the Mummy, Dark Zebra, Graceful Dice, Gray Wing (ataca dos veces), Insect Barrier, Lady Assailant of Flames, Mad
Sword Beast, Mine Golem, Shadowknight Archfiend, Element Saurus, The Agent of Creation - Venus, Ryu Kokki, Pitch-Black
Warwolf, Amazoness Swords Woman, Balloon Lizard, Lord of D., Beast Fangs.

### Tanda 7 (E): la linea de Cocoon of Evolution (Weevil)
Cocoon of Evolution (desde la mano se equipa a un Petit Moth propio, "Equipar" en el menu: el Petit Moth pasa a 0/2000;
cuenta tus fases de espera), Larvae Moth (2 turnos), Great Moth (4) y Perfectly Ultimate Great Moth (6): invocacion
especial desde la mano sacrificando ese Petit Moth (el Capullo se va con el). Perfectly Ultimate Great Moth queda
listo en el motor pero falta en classic-assets (cartas_ui.txt / cartas_classic.txt): hasta que el flujo de cartas
firma (personajes_gen.py --firma y preparar_classic.py) la agregue, personajes_gen.py la avisa y no entra al mazo de
Weevil. rivales_gen.py: `ESPECIALES_MOTOR` / `NOMI_MAZO` (las que el motor invoca: Envoy, Chaos Emperor, las
polillas) pueden ir en los mazos de los personajes; los mazos genericos no cambian.

### Tanda 7 (F)
Doble sacrificio (Kaiser Sea Horse, Whirlwind Prodigy, Flame Ruler, Double Coston, Unshaven Angler: valen 2 para un
monstruo de su atributo; `Duelo::valorTributo`), Fake Trap, Light of Intervention (colocar = invocar boca arriba en
defensa; nada pasa a boca abajo), The End of Anubis (niega lo que elige o se activa en el cementerio), Chaos Command
Magician, Frontier Wiseman, Freed the Matchless General, Creeping Doom Manta, Eagle Eye, Dark Elf, Mystic Lamp,
Amazoness Fighter, Stone Statue of the Aztecs, Enraged Battle Ox, Darklord Marie, Blowback Dragon, Violet Crystal,
Raise Body Heat, Gravekeeper's Guard / Spear Soldier, Guardian Statue, Hoshiningen, Senju / Manju of the Thousand
Hands, Royal Magical Library, Skull Dice, Abare Ushioni, Absorbing Kid from the Sky, Guardian Angel Joan, Armed Dragon
LV3, Des Kangaroo, Legendary Jujitsu Master, Hyper Hammerhead, D.D. Warrior Lady, Lekunga, Man-Thro' Tro', Morphing
Jar, Ookazi, Tremendous Fire, Rising Air Current, Reaper of the Cards, Rock Bombardment, Spellbinding Circle, The
Wicked Worm Beast, White Magical Hat, Dark Dust Spirit, The Unfriendly Amazon, Suijin, The Creator Incarnate, Insect
Imitation.

### Tanda 7 (G)
Magical Hats (2 magias/trampas del mazo y tu monstruo boca abajo, mezclados; los sombreros se van al final de la
batalla), Asura Priest (ataca a todos), Mermaid Knight (dos veces con Umi), Pandemonium, B.E.S. Big Core, Roulette
Barrel, Diabound Kernel, Solar Flare Dragon, The Immortal of Thunder, Thestalos, Time Wizard, Amazoness Chain Master,
Birdface, Buster Blader, Chiron the Mage, Dekoichi, Des Koala, Poison Mummy, Harpie Lady 1, Howling Insect, Jinzo #7,
Keldo, Magical Marionette, Mind Control, Molten Zombie, Sacred Crane, Sonic Bird, Levia-Dragon - Daedalus, Bowganian,
uniones Pitch-Dark Dragon y Zombie Tiger.

### Tanda 7 (H)
The Trojan Horse (doble para TIERRA), The Sanctuary in the Sky, Des Wombat, Queen's Double, Shadowslayer, Black Tyranno,
Panther Warrior, Invasion of Flames, D.D. Crazy Beast, Dark Driceratops, Spear Dragon, Electro-Whip, Follow Wind,
Amazoness Archer, Anti-Aircraft Flower, Gravekeeper's Cannonholder, Cobra Jar, Crimson Sentry, Dice Jar, Embodiment of
Apophis, Mask of Darkness, Pinch Hopper, Avatar of the Pot, Barrel Dragon, Bazoo, Chaosrider Gustaph, Berfomet, Decayed
Commander, Desrook / Infernalqueen Archfiend, Granadora, Granmarg, Iron Blacksmith Kotetsu, King Tiger Wanghu, Orgoth,
Sanga of the Thunder, Chain Destruction, Cyber Harpie Lady.

### Tanda 7 (I)
Objetivos de ataque (Hieracosphinx, Magician's Valkyria, The Legendary Fisherman con Umi, Grave Ohja: `motorAtacable`;
el mando solo ofrece los atacables), Eria / Hiita the Charmer (control mientras esten boca arriba), Rocket Warrior,
Spirit Reaper, Fire Princess, Big Tusked Mammoth, Rainbow Flower, Thousand / Two-Thousand Needles, Mystical Knight of
Jackal, Salamandra, Steel Shell, Power of Kaishin, Metalmorph, Kazejin, Masked Sorcerer, Rain of Mercy, Spirit of the
Breeze, Cure Mermaid, The Thing in the Crater, Thousand Knives, Throwstone Unit, White Ninja, 4-Starred Ladybug of Doom,
Witch Doctor of Chaos, Bite Shoes, Hade-Hane, Black Dragon's Chick, Burning Algae, Cockroach Knight, Burning Land,
Catapult Turtle, Chaos Necromancer, Gren Maju Da Eiza, Crimson Ninja, Golem Sentry, Elegant Egotist, Final Flame, Just
Desserts, Gravekeeper's Curse, Goddess of Whim, Gravity Axe - Grarl, Jirai Gumo, Jinzo - Returner, A-Team: Trap Disposal
Unit.

### Tanda 7 (J)
Kaiser Colosseum (`motorPuedeInvocar`: invocaciones normales y procedimientos), Gaap the Divine Soldier (todos en ataque
boca arriba sin volteos, posiciones fijas: `motorGaap`), The Agent of Judgment - Saturn, los Guardianes (Grarl, Kay'est,
Elma, Tryce, Baou, Ceal: solo con su equipo boca arriba) y sus equipos Rod of Silence - Kay'est, Butterfly Dagger - Elma,
Wicked-Breaking Flamberge - Baou, Shooting Star Bow - Ceal; los que niegan a lo que destruyen en batalla
(`motorNegadoEnBatalla`: Dark Ruler Ha Des, Harpie Lady 2, Twin-Headed Wolf, Element Doom / Soldier), Element Magician,
los que ignoran las magias como objetivo (`ignoraMagia`), los niveles LV (Silent Swordsman LV3 / LV5, Horus the Black
Flame Dragon LV4 / LV6, Ultimate Insect LV1 / LV3 / LV5 / LV7) y los enviados al cementerio por un efecto del rival
(`motorDescartado`: Regenerating Mummy, Electric Snake, Elephant Statue of Disaster, Despair / Fear from the Dark,
Penguin Knight; Roc from the Valley of Haze al descartarse).

### Tanda 7 (K)
Uniones Burning Beast / Freezing Beast, Armor Breaker y Des Dendle (con Vampiric Orchis), Dragon Capture Jar y Dragon
Piper, Spirits con efecto de batalla (Great Long Nose, Hino-Kagu-Tsuchi, Fushi No Tori), Gilasaurus, Aussa the Earth
Charmer, Wandering Mummy, Cobraman Sakuzy, Royal Keeper, Little-Winguard, Insect Soldiers of the Sky, Cat's Ear Tribe,
Prickle Fairy, Electric Lizard, Harpie Lady 3, Inferno Hammer, Andro Sphinx, Shinato King of a Higher Plane, Tyrant
Dragon, Berserk Gorilla, Thunder Nyan Nyan (`motorEstado`), Drill Bug, Protector of the Sanctuary, The Agent of Wisdom -
Mercury, Soul-Absorbing Bone Tower, Castle of Dark Illusions, Helpoemer, Armor Exe, Tainted Wisdom.

### Tanda 7 (L)
Magical Labyrinth (equipo repetible: Wall Shadow), Malice Doll of Demise, Strike Ninja (rapido, vuelve en la fase
final), Serpentine Princess, Manticore of Darkness, Dark Master - Zorc (dado), Pandemonium Watchbear, Blast Magician,
Valkyrie Zweite / Dritte, Wall of Revealing Light, Apprentice Magician, Batteryman AA, Card of Safe Return, Chopman,
Crass Clown, Ryu-Kishin Clown, D.D. Scout Plane, Dark Magician of Chaos, Elephant Statue of Blessing, Skull-Mark
Ladybug, Firebird, Griggle, Horus' Servant, Invader of the Throne, Rafflesia Seduction, Karate Man, uniones Kiryu y
Metallizing Parasite - Lunatite, Level Up!, Magical Merchant, Mask of Brutality, Mask of Dispel, Meteor of Destruction,
Poison of the Old Man, Millennium Scorpion, Molten Destruction, Monk Fighter, Night Assailant, Nobleman-Eater Bug,
Nightmare Wheel, Satellite Cannon, Silent Magician LV4, Snake Fang, Stealth Bird, Swarm of Locusts, The Fiend Megacyber.

### Tanda 7 (M)
Motor: cartas ocultas que se muestran (`revelar`, suceso `S_REVELA`: aviso "Revelada" en la mesa; los monstruos quedan
"vistos" para la IA), atributo cambiado (`Monstruo::atributo`, `atributoDe`: campos por atributo y "hay un X boca
arriba"), sustitutos de fusion (`esSustitutoFusion`, `Duelo::cubreSustituto`; el mando los usa con Polymerization),
invocar normal sin sacrificios (Fusilier Dragon: la mitad), el mazo de fusion como lugar para elegir (`EN_EXTRA`), el
ataque recien declarado (`motorAtaqueDeclarado`), nadie sacrifica (`motorSinSacrificios`) y Banisher of the Light.
Cartas: A Man with Wdjat, The Stern Mystic, Cobraman Sakuzy (ahora muestra), Homunculus the Alchemic Being, Mystical
Sheep #1, The Light / Dark / Earth - Hex-Sealed Fusion, Fusilier Dragon, Banisher of the Light, Legendary Flame Lord,
Summoner of Illusions, Maharaghi, Dreamsprite, Maiden with Eyes of Blue, Gravekeeper's Assailant, Glass Slippers,
Prinzessin, Pumpkin Carriage, Sword Hunter, The Kick Man, Cyber Raider, Mask of Restrict, Jigen Bakudan, Magician of
Black Chaos y Fortress Whale (rituales sin efecto).

### Tanda 7 (N)
Motor: lo que se destruye en lugar del monstruo en batalla (`motorSustitutoBatalla`: las uniones, el equipo de
Relinquished, Viser Des), ignicion desde la mano descartandose (`DefCarta::ignicionMano`, el mando: "Activar efecto"),
invocacion con 3 sacrificios (Moisture Creature), campos que responden a los ataques (Golden Castle, Dark Sanctuary en
`motorAtaqueDeclarado`), ajustes de DEF solo en el calculo. Cartas: Destiny Board y los 4 Spirit Message, Dark
Sanctuary, Golden Castle of Stromberg, Bark of Dark Ruler, Relinquished, Moisture Creature, Yado Karu, Dream Clown,
Ceremonial Bell, Orca, The Flute of Summoning Dragon, Thunder Dragon, Tornado Bird, Valkyrie Brunhilde, Gyaku-Gire
Panda, Amazoness Tiger, Vampire Lady / Lord, Viser Des, Watapon, Wave-Motion Cannon, Winged Minion, Woodland Sprite,
An Owl of Luck, Ape Fighter, Catnipped Kitty, Cipher Soldier, Cyber-Stein, Dark Magic Attack, Dark Mimic LV3, Des Feral
Imp, Des Lacooda, Dimension Jar, Don Zaloog, Emissary of the Oasis, Exchange of the Spirit, Fire Sorcerer, Gift of the
Mystical Elf, Gora Turtle, Guardian Eatos, Elemental Mistress Doriado, Deepsea Warrior, Gravekeeper's Vassal, Vile
Germs, The Masked Beast.

### Tanda 7 (O)
Motor: posiciones impuestas por efectos continuos en una sola regla (`posicionForzada`: Gaap, Dragon Capture Jar, Level
Limit - Area B, Insect Princess), Necrovalley (niega lo que elige cartas del cementerio), Bad Reaction to Simochi (en
`Duelo::ganarLP`), magias prohibidas por turnos (Sonic Jammer), mas sustitutos de fusion (Versago, Beastking of the
Swamps). Cartas: Insect Queen, Level Limit - Area B, Necrovalley, Harpie Lady Phoenix Formation, Harpies' Hunting
Ground, Horn of the Unicorn, Mask of the Accursed, Reaper Scythe - Dreadscythe, Insect Princess, Ka-2 Des Scissors,
Mystical Beast of Serket, Kaibaman, Release Restraint, Kryuel, Newdoria, Troop Dragon, Pixie Knight, Mecha-Dog Marron,
The Unhappy Maiden, Nubian Guard, Aswan Apparition, Magical Plant Mandragola, Mask of Weakness, Mirror Wall, Mataza the
Zapper, Mysterious Guard, Otohime, Parasite Paracide, Patrol Robo, Perfect Machine King, Pumpking, White Magician
Pikeru, Witch's Apprentice, Bladefly, Sonic Jammer, Winged Sage Falcos, A Cat of Ill Omen, Armor Break, Call of the
Grave, Bad Reaction to Simochi, Bubonic Vermin, Copycat, Dark Jeroid, Desertapir, Disc Fighter, Wall of Illusion, Steel
Scorpion, Vampire Baby, Tactical Espionage Expert, Ancient Gear Soldier, Minar, Black Luster Soldier, Skull Guardian.

### Tanda 7 (P)
Motor: un monstruo solo puede atacar si tiene a quien (`motorHayObjetivo`: algun atacable o el ataque directo), sin
danno de batalla por turnos (`sinDannoBatallaHasta`), Grave Protector (los destruidos en batalla al mazo), las trampas
del rival tambien respetan a los que no pueden elegir (Gora Turtle of Illusion). Cartas: Ride of the Valkyries,
Valkyrie Erste, Amazoness Archers, Big Eye, Coach Goblin, Contract with Exodia, Exodia Necross, Dark Scorpion - Chick
the Yellow / Meanae the Thorn, Darkbishop Archfiend, Fairy of the Spring, Familiar Knight, Giant Germ, Nimble Momonga,
Lord Poison, Gilford the Lightning, Goblin King, Harpie's Pet Dragon, Muka Muka, King of the Skull Servants, Milus
Radiant, Laser Cannon Armor, Gora Turtle of Illusion, Grave Protector, Gravekeeper's Chief, Great Phantom Thief, Heavy
Mech Support Platform, Hiro's Shadow Scout, Interdimensional Matter Transporter, Jam Breeding Machine, Jowgen the
Spiritualist, Jowls of Dark Demise, Legacy Hunter, Marauding Captain, Medusa Worm, Mid Shield Gardna, Mystic Swordsman
LV2 / LV4 / LV6, Needle Worm, Princess of Tsurugi, Outstanding Dog Marron, Royal Tribute, Mysterious Puppeteer, Leghul,
Ooguchi, Piranha Army, Rocket Jumper, Raregold Armor, Luminous Soldier, Maji-Gire Panda.

### Tanda 7 (Q)
Motor: Chain Energy (`motorPuedePagarMano` / `motorPagarMano`: 500 LP por carta jugada de la mano en invocaciones,
colocaciones, fusiones, rituales y activaciones; sin LP de sobra no se puede; la IA ademas no juega de la mano si le
quedarian 1000 o menos: `motorFiltroIA`), Cost Down (`motorTributos`: niveles menos en la mano este turno; mando, IA e
invocacion), Graverobber (`EstadoEfectos::prestadas`: usarla cuesta 2000; sin usar vuelve al cementerio del rival),
Metalsilver Armor (el rival solo elige al equipado con efectos de un objetivo), Begone Knave (tras el calculo, a la
mano), Kotodama (`motorEstado`), Ultimate Tyranno (ataca a todos; los demas esperan). Cartas: Chain Energy, Cost Down,
Graverobber, Metalsilver Armor, Big Bang Shot, 7 Completed, Ballista of Rampart Smashing, Kiseitai, Infernal Flame
Emperor, Gingerbread House, Sacred Phoenix of Nephthys, Sasuke Samurai #2, Second Goblin, Secret Barrel, Shadow Ghoul,
Wodan, Star Boy, Shadow Tamer, Shield & Sword, Spirit Caller, Little Swordsman of Aile, Toon Table of Contents, Tornado
Wall, Winged Kuriboh, 3-Hump Lacooda, 8-Claws Scorpion, A Feather of the Phoenix, A Feint Plan, A Wingbeat of Giant
Dragon, Acid Rain, Acid Trap Hole, Altar for Tribute, Amazoness Blowpiper, Ameba, Ancient Telescope, Anti Raigeki,
Aqua Chorus, Arsenal Bug, Arsenal Robber, Assault on GHQ, Back to Square One, Backfire, Battery Charger, Begone Knave,
Banner of Courage, Ultimate Tyranno, Kotodama, Servant of Catabolism, Susa Soldier, Torpedo Fish, Atomic Firefly,
Ancient Gear Beast, Spirit of the Pot of Greed, Skull Knight #2, Ultimate Insect LV1.

### Tanda 7 (R)
Motor: tipo declarado (DNA Surgery: `EstadoEfectos::adnTipo` en `razaEs`), efectos de un monstruo negados (Orichalcos
Sword of Sealing: `selladoPorEspada`), equipos sin efecto por un turno (Armored Glass), redirigir el objetivo de una
magia (Fairy's Hand Mirror), invocar en el campo del rival (`DefCarta::alCampoRival`: Lava Golem), ataques que pasan a
ser directos (`Ataque::directo`: Toon Defense, Astral Barrier, Absolute End), el atacado elige el objetivo (Patrician of
Darkness), trampas activables el turno en que se colocan (Temple of the Kings), lo robado por un eslabon se descarta
(Muko), ataques extra por equipo (Armed Samurai - Ben Kei), invocar con menos sacrificios (Behemoth). Cartas: DNA
Surgery, Orichalcos Sword of Sealing, Armored Glass, Fairy's Hand Mirror, Lava Golem, Morphing Jar #2, Muko, Spear Cretin,
Spirit Ryu, Temple of the Kings, Triangle Ecstasy Spark, Toon Defense, Astral Barrier, Absolute End, Patrician of
Darkness, A Deal with Dark Ruler, Berserk Dragon, A Hero Emerges, Adhesion Trap Hole, After the Struggle, Amazoness
Spellcaster, Anti-Spell, Aqua Spirit, Arcane Archer of the Forest, Archfiend's Roar, Armed Dragon LV7, Armed Ninja, Armed
Samurai - Ben Kei, Autonomous Action Unit, Backup Soldier, Behemoth the King of All Animals.

### Tanda 7 (S)
Trap Master, Valkyrie Sechste / Funfte, Virus Cannon, 7, Abyssal Designator, Beast Soul Swap, Beckoning Light, Big Wave
Small Wave, Blade Rabbit, Blast Juggler, Blast with Chain, Blasting the Ruins, Blind Destruction, Block Attack, Blue
Medicine, Blue-Eyes Shining Dragon, Boar Soldier, Bombardment Beetle, Burning Spear, Burst Breath, Burst Stream of
Destruction, Byser Shock, Card of Sanctity, Cemetary Bomb, D.D. Dynamite, Cestus of Dagla, Chaos End, Chaos Greed, Chaos
Sorcerer, Chorus of Sanctuary, Confiscation, Curse of Aging, Curse of Fiend, Curse of Darkness, Coffin Seller,
Blessings of the Nile, Dark Cat with White Tail, Dark Core, Dark Factory of Mass Production, Dark Mimic LV1, Dark
Mirror Force, Dark-Piercing Light, Cave Dragon (`motorPuedeInvocar`), Cannonball Spear Shellfish, Crab Turtle.

### Tanda 7 (T)
Motor: Cold Wave (`ondaFria`: nadie juega magias/trampas hasta su proximo turno; la IA tampoco las propone), DNA
Transplant (`adnAtributo` en `atributoDe`), Dark Room of Nightmare (en `dannoEfecto`), Des Counterblow (tras el calculo),
Continuous Destruction Punch, D.D. Borderline, Chain Burst, Criosphinx, Dragon's Rage (perfora). Cartas: Bottomless
Shifting Sand, Buster Rancher, Cold Wave, Dark Scorpion Burglars / Cliff / Gorg, Dark Snake Syndrome, Dark Spirit of the
Silent, Draining Shield, Darkness Approaches, De-Fusion, De-Spell, Deal of Phantom, Dedication through Light and
Darkness, Des Volstgalph, Desert Sunlight, Destruction Ring, Dimension Distortion, Dimension Fusion, Disappear,
Disarmament, Disturbance Strategy, Divine Wrath, DNA Transplant, Double Snare, Dragon Manipulator, Dragon Seeker,
Dragon's Rage, Des Counterblow, Different Dimension Dragon, Dark Room of Nightmare, Chain Burst, Continuous Destruction
Punch, D.D. Borderline, Criosphinx, Dokurorider.

### Tanda 7 (U)
Goblin Attack Force / Giant Orc (tras atacar, a defensa y fijos: `EstadoEfectos::fijos`), Exiled Force, Fiber Jar, Giant
Trunade, Earthquake, Eternal Rest, Exile of the Wicked, Dummy Golem, Ectoplasmer, Ekibyo Drakmord, Emblem of Dragon
Destroyer, Fusion Sage, Gather Your Mind, Emergency Provisions, Emes the Infinity, Enchanting Fitting Room, Energy
Drain, Enraged Muka Muka, Exhausting Spell, Fairy Box, Fairy King Truesdale, Fairy Meteor Crush, Falling Down, Fiend
Comedian, Final Countdown, Final Destiny, Forced Ceasefire, Fox Fire, Frozen Soul, Fatal Abacus, Final Attack Orders,
Fuhma Shuriken, Fusion Weapon, Fulfillment of the Contract, Gaia Soul, Garuda, Gigantes, Fenrir, Gate Guardian, Gift of
the Martyr, Goblin's Secret Remedy, Goblin Thief, Goblin Fan, Giant Axe Mummy.

### Tanda 7 (V)
Fengsheng Mirror, Flint, Frontline Base, Germ Infection, Gust Fan, Invigoration, Horn of Light, Ghost Knight of Jackal,
Good Goblin Housekeeping, Gravedigger Ghoul, Graverobber's Retribution, Greenkappa, Gryphon's Feather Duster, Gryphon
Wing, Hallowed Life Barrier, Hammer Shot, Hand of Nephthys, Heavy Slump, Hidden Spellbook, Horn of Heaven, Horus the
Black Flame Dragon LV8, Hyena, Inferno, Inferno Fire Blast, Injection Fairy Lily, Knight's Title, Dark Magician Knight,
Lady Ninja Yae, Last Will, Lava / Swamp Battleguard, Great Maju Garzett, Goblin of Greed (sin efectos con descarte),
Kangaroo Champ, Lesser Fiend, Hayabusa Knight, Infinite Dismissal, Labyrinth of Nightmare, Gravekeeper's Servant,
Goddess with the Third Eye / King of the Swamp (sustitutos de fusion), Hungry Burger, Inaba White Rabbit.

### Tanda 7 (W)
Lighten the Load, Lightning Blade, Lightning Vortex, Little Chimera, Lost Guardian, Magical Scientist, Magical Thorn,
Maju Garzett, Malevolent Nuzzler, Malice Dispersion, Mass Driver, Master Monk, Mefist the Infernal General,
Metamorphosis, Meteorain, Michizure, Micro Ray, Mind Crush, Mind Haxorz, Mind Wipe, Minor Goblin Official, Miracle Dig,
Monster Reincarnation, Mucus Yolk, Mushroom Man #2, Mystik Wok, Needle Burrower, Needle Ceiling, Nightmare Horse,
Nobleman of Extermination, Offerings to the Doomed (salta la fase de robo), Opti-Camouflage Armor, Order to Charge,
Paladin of White Dragon, Paralyzing Potion, Penumbral Soldier Lady, Performance of Sword, Phoenix Wing Wind Blast,
Pikeru's Circle of Enchantment, Poison Fangs, Possessed Dark Soul, Pyramid Energy.

### Tanda 7 (X)
Card Shuffle, Checkmate, Delta Attacker, Curse of Anubis, Cursed Seal of the Forbidden Spell (la magia queda sellada el
resto del duelo), Curse of Royal, Covering Fire, Attack and Receive, Blade Rabbit, Ante, Archfiend's Oath, D.D.
Designator.

### Tanda 7 (Y)
Dice Re-Roll (tirarDado: Graceful Dice, Skull Dice, Agido, Dark Master - Zorc), Gravekeeper's Watcher, Great Dezard,
Fushioh Richie, Union Rider, Valkyrie Vierte, Elemental Burst, Fuh-Rin-Ka-Zan, Huge Revolution, Exchange, Fruits of
Kozaky's Studies, Jade Insect Whistle, Dark Designator, Hane-Hane, Minefield Eruption, Invitation to a Dark Sleep, Jar
Robber, Judgment of Anubis, Magic Drain, Lightforce Sword, Magician's Circle, Hero Signal, Dark Scorpion Combination,
Earthshaker, Megarock Dragon, Light of Judgment, Cyclon Laser, Heart of Clear Water, Kishido Spirit, Charm of Shabti,
Array of Revealing Light, Bubble Crash.

### Tanda 7 (Z)
Raimei, Restructer Revolution, Solar Ray, Numinous Healer, Rush Recklessly, Secret Pass to the Treasures, Sebek's
Blessing, Raigeki Break, Remove Trap, Really Eternal Rest, Special Hurricane, Reload, Reckless Greed (salta 2 robos),
Monster Recovery, Ray of Hope, Salvage, Rite of Spirit, Soul Reversal, Soul Resurrection, Silver Bow and Arrow, Ritual
Weapon, Shadow Spell, Sage's Stone, Ojama Delta Hurricane!!, Ojama Trio, Multiplication of Ants, Order to Smash, Share
the Pain, Ninjitsu Art of Decoy, Remove Brainwashing, Ready for Intercepting, Seal of the Ancients, Royal Surrender,
Riryoku Field, My Body as a Shield, Sand Gambler, Rescue Cat, Monster Gate, Painful Choice, Silpheed, Soul of Purity and
Light, Red-Eyes Darkness Dragon, Raging Flame Sprite, Peten the Dark Clown, Sasuke Samurai, Sasuke Samurai #4, Solemn
Wishes, Soul Absorption, Spell Absorption, Skull Invitation, Robbin' Goblin, Soul Release, Rare Metal Dragon.

### Tanda 7 (AA)
Spell Purification, Spell-Stopping Statute, Trap Jammer, Tutan Mask, Spell Reproduction, Spellbook Organization,
Terraforming, The Warrior Returning Alive, The Graveyard in the Fourth Dimension, Spirit Barrier, Spirit of Flames, The
Rock Spirit, Spiritualism, Stray Lambs, Stim Pack, Sword of Dark Destruction, Sword of Deep-Seated, Sword of Dragon's
Soul, Twin Swords of Flashing Light - Tryce, Yellow Luster Shield, Umiiruka, The A. Forces, The Big March of Animals,
Ultimate Baseball Kid, The Dark Door, Vengeful Bog Spirit, Stumbling, Spring of Rebirth, Threatening Roar, Thunder of
Ruler, Time Seal, Teva, Thunder Crash, Two-Pronged Attack, Token Thanksgiving, Type Zero Magic Crusher, Windstorm of
Etaqua, Zero Gravity, Wild Nature's Release, The Reliable Guardian, Thousand Energy, Triangle Power, The Forceful
Sentry, The Inexperienced Spy, The Cheerful Coffin, The Shallow Grave, Tribute Doll, Ultra Evolution Pill, Valkyrion
the Magna Warrior, Terrorking Archfiend, Vilepawn Archfiend, Yomi Ship, The Hunter with 7 Weapons, Twin-Headed Beast.

### Tanda 7 (AB)
Different Dimension Gate, Dimensionhole, Double Attack, Dragonic Attack, Inferno Tempest, Dragged Down into the Grave,
Destruction Punch, Cross Counter, Big Burn, Dark Coffin, Kozaky's Self-Destruct Button, Statue of the Wicked, Deck
Devastation Virus, Dark Necrofear, Sorcerer of Dark Magic, Heart of the Underdog, Legendary Black Belt, Spiral Spear
Strike, Spell Economics, Precious Cards from Beyond, Recycle, Mispolymerization, Needle Wall, Ninjitsu Art of
Transformation, Nutrient Z, Physical Double, Re-Fusion, The Creator, Reasoning, Skull Lair, Spell Vanishing, The
Dragon's Bead, Token Feastevil.

### Tanda 7 (AC)
Infinite Cards, Hieroglyph Lithograph, Enervating Mist (limiteMano: el limite de la mano en la fase final), Spell
Canceller, Silent Swordsman LV7, Imperial Order (no se activan magias), Silent Magician LV8, Royal Decree, Swords of
Concealing Light, Rope of Life, Return of the Doomed, Life Absorbing Machine, Soul Demolition, Rare Metalmorph, Smoke
Grenade of the Thief, Nitro Unit, Different Dimension Capsule, Yata-Garasu, Yamata Dragon, The Unhappy Girl.

### Tanda 7 (AD): negacion de efectos de monstruos
`efectoNegado(d, m)`: Skill Drain (todos los boca arriba), Royal Command (los de volteo) y Gorgon's Eye (los de defensa ese
turno). Un monstruo negado no aporta sus efectos continuos (cadaFuente: modAtk, impideAtaque...; atkPropio), no dispara
(dispararMonstruo y el danno de batalla), no activa efectos en el campo, pierde sus inmunidades a magias/trampas
(ignoraMagia / ignoraTrampa) y Jinzo deja de bloquear trampas. The Emperor's Holiday: los equipos no se cuentan en
cadaFuente. Talisman of Trap / Spell Sealing (con Sealmaster Meisei): no se activan trampas / magias. Mind on Air,
Respect Play y The Eye of Truth: manos a la vista. Self-Destruct Button: empate. El aviso de descartar en la fase final usa
el limite de mano real.

### Tanda 7 (AE)
Mystic Probe, Metal Detector, World Suppression (mtNegadaTurno), Non-Spellcasting Area, Dust Barrier (sinMagias), Toll,
Timidity, Trap of Board Eraser, Tower of Babel, Two-Man Cell Battle, Wall Shadow, Wynn the Wind Charmer, Xing Zhen Hu,
Vampire Genesis, The First / Second / Third Sarcophagus, Spirit of the Pharaoh, The Secret of the Bandit, The Regulation
of Tribe, The Law of the Normal, Shadow of Eyes, Rod of the Mind's Eye, Prohibition, Primal Seed, Penalty Game!, Greed,
Appropriate, Morale Boost, The Spell Absorbing Life.

### Tanda 7 (AF)
Lone Wolf, Mirage of Nightmare, Mokey Mokey Smackdown, Mustering of the Dark Scorpions, Necklace of Command, Non Aggression
Area, Ominous Fortunetelling, Senri Eye, Reversal Quiz, Pyramid of Light, Sphinx Teleia, Reshef the Dark Being, Spell
Shield Type-8, Super Rejuvenation, Swarm of Scarabs, Sword of the Soul-Eater, The Agent of Force - Mars.

### Tanda 7 (AG)
Drop Off, Gamble, Gust, Driving Snow, Fairy Guardian, Chosen One, Blast Held by a Tribute, Impenetrable Formation, Pikeru's
Second Sight, Ring of Magnetism, Taunt, Staunch Defender, Tragedy (blancoForzado en motorAtacable).

### Tanda 7 (AH): niveles, tipos, atributos, contadores de magia y reglas del campo
`nivelM(d, m)` (Demotion -2; se usa donde el motor miraba el nivel de un monstruo del campo), D. Tribe y Dragonic Attack en
razaEs, Scroll of Bewitchment en atributoDe, `contadoresMagia` / `quitarContadoresMagia` (los monstruos de contadores y
Pitch-Black Power Stone: Mega Ton Magical Cannon, Miracle Restoring), Magic Reflector, Anti-Spell Fragrance, Spatial
Collapse, Rivalry of Warlords, Pole Position, Convulsion of Nature, Judgment of the Desert, Solomon's Lawbook.

(Correccion, tanda AR: Archlord Zerato y Mazera DeVille piden "Warrior of Zera" (Guerrero de Zera), que si esta en
los datos (warrior_of_zera); no hacia falta Zera the Mant. Hechas en la tanda AR.)

### Tanda 7 (AI)
Metalmorph (ademas: sacrifica al Zoa / Red-Eyes equipado: Metalzoa / Red-Eyes Black Metal Dragon del mazo), Ocean Dragon
Lord - Neo-Daedalus, Gradius' Option, Mirage Knight (y Dark Flare Knight que lo trae), Theinen the Great Sphinx, Guardian
Dreadscythe, Koitsu (union de Aitsu), Supply, Dramatic Rescue, Spirit's Invitation, Spiritual Energy Settle Machine,
Human Wave Tactics, Pharaoh's Treasure, The Puppet Magic of Dark Ruler.

### Tanda 7 (AJ)
Brain Jacker, Gearfried the Swordmaster (resolverEquipo), Chain Disappearance, Collected Power, Combination Attack,
Formation Union, Diffusion Wave-Motion, Dora of Fate, Forced Requisition, Inspection, Jam Defender.

### Tanda 7 (AK)
White Hole (Dark Hole respeta ese lado), Shift, Weapon Change, Soul Exchange, Shifting Shadows, Weather Report, Kaminote
Blow, Dark Sage (lo trae Time Wizard al acertar).

### Tanda 7 (AL): las ultimas posibles
Amplifier (jinzoBloquea), Bait Doll, Battle-Scarred, Centrifugal Field, Fusion Gate, Double Spell / Serial Spell (copian
magias de una lista: magiaCopiable), Fiend's Hand Mirror, Tailor of the Fickle, Grave Lure, Ground Collapse (zonas
bloqueadas en Lado), Last Turn, Level Conversion Lab (motorTributos), Major Riot, Makyura the Destructor (trampas desde la
mano), Narrow Pass, Neko Mane King, Ordeal of a Traveler, Pineapple Blast, Pyro Clock of Destiny, Question, Reverse Trap
(motorRecalcular), Royal Oppression, Second Coin Toss (tirarMoneda), Spirit Elimination, Timeater (sin fase principal 1).

## Atajos (lo que no es exacto, a proposito)

1. ~~Sin repeticion de ataque~~: resuelto con la regla oficial (rama classic-reglas-oficiales). Si tras la
   declaracion cambia el numero de monstruos del rival (Scapegoat, Magician's Circle...), aparece un monstruo ante un
   ataque directo o el objetivo ya no esta, el motor le pregunta al atacante (`P_ELEGIR` "ef.elegir.repeticion",
   `Duelo::pedirRepeticion`): los objetivos posibles (el de antes primero; `i = -1` es el ataque directo, solo si el
   rival no tiene monstruos o el atacante puede atacar directo) y "No atacar" (`J_PASAR`). Elegir un objetivo es una
   declaracion nueva (`S_ATAQUE` con valor 1, `motorAtaqueDeclarado` y la ventana `V_ATAQUE` otra vez: el rival puede
   responder). "No atacar" deja al monstruo volver a atacar en esa fase (`repetirAtaque`). Si el numero no cambia
   (Waboku, Book of Moon), el ataque sigue. La IA responde con su politica de ataque (`Cerebro::elegirRepeticion`: los
   puntos de `ataquesDe` con el plan, contra "no atacar" = 0; la IA humana y sus simulaciones usan lo mismo; sin
   personalidad, `elegirIA` ataca solo si gana el combate).
2. ~~Posicion de Monster Reborn automatica~~: resuelto, la elige quien la activa ("¿en defensa?"; "No" = ataque). Las
   demas invocaciones especiales por efecto siguen saliendo en ataque (o la mejor posicion para la IA).
3. **Ventanas**: ya hay ventana al empezar la batalla (`V_INICIO_BATALLA`, para el rival) y en el paso de danno
   (`V_PASO_DANNO`, antes del calculo: primero el del turno y luego el rival; solo contraataques y cartas marcadas
   `pasoDanno`: Shrink, Reinforcements, Castle Walls, Limiter Removal; los volteos esperan al calculo). Sigue sin haber
   ventana en la fase de espera ni al final de la batalla.
   Rama classic-reglas-oficiales: **ventana justo despues del danno** (`V_DANNO`, `abrirVentanaDanno`). Cada danno
   (de batalla o de efecto; no los pagos de LP) marca al que lo recibio (`ef.dannoPend`; el de batalla guarda su valor
   para Inferno Tempest). En el primer momento libre despues (al terminar el calculo de danno, al terminar la cadena o
   la jugada) se abre la ventana solo para ese lado (primero el del turno) y solo si tiene una carta "cuando recibes
   danno" que pueda activar (`DefCarta::trasDanno`); si no, el momento pasa. En la ventana solo esas cartas empiezan
   cadena (el rival responde como siempre); al cerrarse sigue lo que esperaba (`PEND_DANNO` guarda el pendiente:
   el combate, los disparos, la fase). Las cartas del catalogo con ese texto son Attack and Receive, Numinous Healer e
   Inferno Tempest (3000+ de danno de batalla de un ataque); se revisaron las demas con "take damage"/"inflicts damage":
   Trap of Board Eraser responde a la activacion (cadena normal), Nutrient Z y Kuriboh van en el calculo, Dark Room of
   Nightmare es continua y las "si inflige danno de batalla" son efectos del atacante (disparos). "Preguntar antes de
   activar" se respeta: Nunca pasa solo, Auto pregunta (solo se abre si hay carta), Siempre no abre ventanas de danno
   sin carta.
4. **Monster Reborn y compania**: los monstruos marcados "no invocables" en rivales.txt no son objetivo; las fusiones y
   rituales del cementerio se consideran invocados como corresponde.
5. **Una sola magia de campo boca arriba en todo el campo**: al activar una, la otra boca arriba se va (regla de la
   epoca); una colocada propia se reemplaza.
6. **Control prestado sin zona libre al volver**: va al cementerio de su duenno.
7. Toon World sigue por la jugada de siempre (`J_ACTIVAR_MT`, magia continua sin efecto propio en el motor).
8. Axe of Despair: sin su efecto de volver al mazo sacrificando un monstruo.
9. Messenger of Peace: en la fase de espera paga los 100 LP solo (no pregunta si prefieres destruirla).
10. Waboku: solo se ofrece en la ventana de un ataque del rival (donde sirve), no en cualquier momento.
11. Solemn Judgment contra una invocacion: se niega en la ventana de la invocacion (el monstruo ya esta en el campo y
    va al cementerio; sus efectos "al ser invocado" se cancelan).
12. Ring of Destruction: se usa el texto de cartas_ui_en.txt (el que ve el jugador), no el de 2005 (cualquier
    monstruo boca arriba, en cualquier turno).
13. Efectos disparados de monstruos (volteo, Sangan...): el objetivo se elige al resolver, no al activarse.
14. Penguin Soldier: siempre devuelve (es opcional) y solo monstruos del rival, hasta 2.
15. Cyber Jar: los monstruos revelados salen boca arriba en ataque (no se elige boca abajo en defensa); sin ventana de
    respuesta para esas invocaciones.
16. Sangan / Witch: sin la restriccion moderna de "no activar cartas con ese nombre" ni "1 por turno" (no existian en
    2005).
17. Black Luster Soldier - Envoy: solo el modo "desterrar un monstruo" (no el de atacar dos veces); los monstruos de
    LUZ y OSCURIDAD que se destierran para invocarlo y a Chaos Emperor los elige el juego (los de menos ATK).
18. Tribe-Infecting Virus: el tipo se declara eligiendo un monstruo boca arriba de ese tipo.
19. Sinister Serpent vuelve siempre (es opcional) y sin la parte moderna de desterrar.
20. Toons: sin los efectos extra de Toon Gemini Elf (descarte) ni Toon Masked Sorcerer (robo); sin Uniones todavia.
21. ~~Costes elegidos por el juego~~: resuelto, Obelisk, Crush Card Virus, Multiply, Hysteric Fairy y la invocacion de
    Envoy / Chaos Emperor piden al jugador los sacrificios / destierros (`Jugada::costes`, `DefCarta::candCostes`); la
    IA paga con lo que menos vale. Enemy Controller sigue eligiendo su sacrificio solo.
22. The Seal of Orichalcos: sin su proteccion (no destruible 1 vez por turno) ni la regla del ataque al de menos ATK.
23. Crush Card Virus: sin la parte del mazo (opcional para el rival). Mystic Box: el monstruo propio se elige al
    resolver. Dark Magic Curtain: sin la restriccion de no invocar mas ese turno. Multiply: las fichas si se pueden
    sacrificar. Celestial Sword - Eatos: sin su efecto al ir al cementerio.
24. Dust Tornado: sin colocar una magia/trampa de la mano despues. Scapegoat: las fichas usan la cara de Scapegoat y no
    hay restriccion de invocar ese turno; se pueden sacrificar. Enemy Controller: el modo lo decide tener un monstruo
    para sacrificar (si lo hay, toma el control). Dragon's Gunfire: el monstruo se elige al resolver ("No" = 800 de
    danno). A Legendary Ocean: sin bajar niveles ni contar como Umi. Murasame: sin su proteccion contra efectos.
25. Skull Archfiend of Lightning: paga solo y sin su tirada de dado. Hysteric Fairy: los 2 sacrificios los elige el
    juego. Kaiser Glider: tambien al ir al cementerio sin ser destruido (sacrificado). Kelbek devuelve al atacante
    aunque Kelbek muera en ese combate.
26. Ultimate Offering: solo en tu fase principal (no en la batalla del rival). Uniones: sin sustituir la destruccion
    del equipado; el monstruo a equipar se elige al resolver. Skilled White/Dark Magician: el juego busca a Buster
    Blader / Dark Magician en mano, mazo y cementerio, en ese orden. Element Dragon: sin el segundo ataque con VIENTO.
    Armed Dragon LV5: el objetivo se elige al resolver y sin la evolucion a LV7. Big Shield Gardna: sin negar magias.
    Twin-Headed Behemoth vuelve siempre que pueda (es opcional). Maiden of the Aqua: cuenta como Umi solo para Amphibious
    Bugroth MK-3. Los reclutadores invocan en ataque (Pyramid Turtle: la mejor posicion).
27. Don Turtle invoca todas las de la mano. Book of Life: el monstruo del rival se elige al resolver. Balloon Lizard:
    el danno va al rival de su controlador (no se mira quien lo destruyo) y sin cadena. Shadowknight Archfiend: sin el
    dado.
31. Metalmorph: sin el bono al atacar. Gravity Axe - Grarl: sin bloquear los cambios de posicion. Goddess of Whim /
    Jirai Gumo: sin "cantar" la moneda. Charmers: si se ponen boca abajo, no devuelven lo tomado (solo al dejar el
    campo). Hade-Hane: solo monstruos del rival. Element Saurus: sin la parte de TIERRA. Mobius: la IA solo destruye las del rival.
32. Kaiser Colosseum: no frena las invocaciones especiales por efectos, fusiones, rituales ni Toons. "No le afectan las
    magias" (Silent Swordsman LV5, Horus LV6, Kay'est, Ultimate Insect LV1) y Silent Swordsman LV3 / Rod of Silence:
    solo no se pueden elegir como objetivo (Raigeki, Dark Hole... si los afectan). Los LV suben solos (es opcional) y sin
    la restriccion del turno en que llegan. Gaap: muestra todos los Demonios de la mano. Saturn: la fase de batalla no
    sigue (aunque se active en la principal 2). Guardian Tryce: sin revivir lo que se sacrifico. Guardian Elma / Butterfly
    Dagger: siempre (es opcional). Regenerating Mummy, Electric Snake...: el rival es el que resuelve el eslabon en curso.
33. Wandering Mummy: no revuelve los boca abajo. Cobraman Sakuzy: no muestra las colocadas del rival. Little-Winguard:
    cambia de posicion en tu fase principal (no en la final). Berserk Gorilla: sin la obligacion de atacar. Andro
    Sphinx: sin su invocacion con Pyramid of Light (no esta en los mazos); el danno usa el ATK original. Tyrant Dragon /
    Andro Sphinx: sin las restricciones para revivirlos. Uniones nuevas: la ficha
    de Des Dendle usa su cara. Hino-Kagu-Tsuchi: esos descartes no cuentan como "por un efecto del rival". Armor Exe:
    cuenta los contadores de los monstruos de contador de magia. Gilasaurus: el rival elige con la pregunta de siempre.
    The Agent of Wisdom - Mercury: se mira la mano al empezar la fase final del rival.
34. Malice Doll: solo cuenta si la manda al cementerio el eslabon de una magia continua. Strike Ninja: vuelve en
    ataque. Valkyrie Zweite: sin recuperar la magia continua; Valkyrie Dritte: sin su busqueda. Wall of Revealing
    Light: paga solo lo justo para frenar al mas fuerte del rival (en multiplos de 1000). Apprentice Magician: el
    contador solo en monstruos de contador de magia. D.D. Scout Plane: solo si se destierra del campo. Firebird: cuenta
    tambien los sacrificados. Kiryu: sin el sacrificio para el ataque directo. Poison of the Old Man: el danno si
    remata o si vas por delante en LP; si no, los LP. Mask of Brutality: paga sola si puede. Blast Magician: quita los
    contadores justos. Satellite Cannon: vuelve a 0 tras cualquier ataque suyo. Silent Magician LV4: sube en la
    siguiente fase de espera con 5 contadores (solo). Chopman: sin mirar a quien puede equiparse la magia.
35. Homunculus: el atributo pasa al siguiente (LUZ, OSCURIDAD, TIERRA, AGUA, FUEGO, VIENTO), sin elegirlo; el atributo
    cambiado cuenta para los campos y "hay un X boca arriba" (no para todos los efectos). Fusilier Dragon: el mando lo
    invoca sin sacrificios solo si no tienes con que sacrificar. La IA no usa sustitutos con Polymerization (si las
    Hex-Sealed por su efecto); con Hex-Sealed el juego elige los materiales. Maharaghi: solo muestra la de arriba (se
    queda arriba; la del rival no se muestra). Dreamsprite: el juego elige a quien se desvia. Maiden with Eyes of Blue:
    siempre niega el primer ataque del turno y saca a Blue-Eyes (sin su efecto al ser elegida por cartas). Gravekeeper's
    Assailant: cambia la posicion del atacado. Cyber Raider: el equipo del rival se destruye, el tuyo pasa a el. Sword
    Hunter: los equipados van al cementerio de su controlador. Mask of Restrict: invocaciones normales y rituales (no
    los costes de efectos). Banisher of the Light: lo que pasa por el cementerio directo en algunos efectos no se
    destierra. Chain Energy: pendiente.
36. Destiny Board: el mensaje se pone solo (con Dark Sanctuary, siempre como monstruo, sin que sea inmune). Dark
    Sanctuary: sin "cantar" la moneda. Golden Castle of Stromberg: sin la proteccion de Pumpkin Carriage. Bark of Dark
    Ruler: paga lo justo para que gane su Demonio. Relinquished: el monstruo equipado va al cementerio de su
    controlador. Moisture Creature: el mando pide 3 sacrificios si tienes 3 monstruos y el rival magias/trampas; la IA
    lo invoca con 2. Ceremonial Bell: muestra las manos solo al llegar. Orca: el sacrificio sale al resolver. Vampire
    Lady / Lord: se declara el tipo con mas cartas en el mazo del rival y va la de mas arriba. Dimension Jar: cada uno
    destierra los 3 mas fuertes. Don Zaloog: descarta si hay mano; si no, 2 del mazo. Valkyrie Brunhilde: sin su efecto
    al ser atacada. Guardian Eatos: sin su segundo efecto. Thunder Dragon: busca siempre 2 si hay.
37. Insect Queen: el sacrificio para atacar lo elige el juego; la ficha usa la cara de Basic Insect. Necrovalley: niega lo
    que elige cartas del cementerio (no frena los costes de desterrar). Kryuel: sin cantar la moneda. Pixie Knight /
    Nubian Guard / Aswan Apparition: va la ultima que cumple. Mirror Wall: la mitad del ATK original; paga sola si
    puede. Mysterious Guard: sin el segundo objetivo. Parasite Paracide: sin cambiar la raza de los del rival. Steel
    Scorpion: se destruye en la fase final del segundo turno de su duenno. Mecha-Dog Marron: el danno sin cadena.
38. Ride of the Valkyries: su efecto del cementerio solo se ofrece al ir alli tras resolver. Valkyrie Brunhilde: sin
    su primer efecto. Mischief of the Time Goddess: se activa en tu batalla sin un ataque en curso y la termina. Amazoness
    Archers: sin la obligacion de atacar. Big Eye: muestra las 5 sin reordenarlas. Coach Goblin: siempre que pueda.
    Exodia Necross: si lo destruyen las magias/trampas. Dark Scorpion - Chick the Yellow: solo devolver a la mano.
    Darkbishop Archfiend: sin su proteccion con el dado. Familiar Knight: el juego invoca el mas fuerte de nivel 4 de
    cada mano. Gilford: el mando pide 3 sacrificios si tienes 3 monstruos y el rival alguno; la IA lo invoca con 2.
    Great Phantom Thief: declara la carta repetida de la mano del rival. Jam Breeding Machine: la ficha usa la cara de
    Humanoid Slime; sin la restriccion de invocar. Jowgen: sin prohibir las invocaciones especiales. Jowls of Dark
    Demise: sin el ataque directo. Mid Shield Gardna: sin negar las magias. Mystic Swordsman: destruir al boca abajo
    cuenta para subir de nivel; sube solo. LV6: sin ponerlo arriba del mazo del rival.
39. Chain Energy: con LP justos no se puede jugar (no se paga hasta 0). Cost Down: solo cambia los sacrificios (no el
    nivel en el campo). Graverobber: usada, va a tu cementerio; si se coloca, se queda. Infernal Flame Emperor: el juego
    destierra lo justo para las magias/trampas del rival. Gingerbread House: siempre. Sasuke Samurai #2: nadie activa
    magias/trampas el resto del turno. Second Goblin: sin su cambio de posicion. 7 Completed: siempre ATK. Amazoness
    Blowpiper: al de mas ATK. Arsenal Robber: la de mas arriba. Skull Knight #2: el del mazo sale solo (sin cadena). Kiseitai: sin zona de magia/trampa libre, combate normal.
40. DNA Surgery: el tipo se declara eligiendo un monstruo del campo; cuenta donde el motor pregunta el tipo de un
    monstruo boca arriba por su nombre. Orichalcos Sword of Sealing: niega efectos activados y disparados del equipado
    (no los continuos); sin su segundo efecto. Lava Golem: los sacrificios los elige el juego. Morphing Jar #2: los que
    salen, boca abajo; Muko: responde a las cartas que roban de la lista. Spear Cretin: cada uno revive el de mas ATK.
    Spirit Ryu: el bono dura el turno. Temple of the Kings: sin su segundo efecto. Triangle Ecstasy Spark: sin negar
    las trampas ya activas. Astral Barrier: el juego la usa cuando el atacado perderia y el danno no mata. Patrician of
    Darkness: el juego elige el de mas defensa. Aqua Spirit: sin su efecto en la espera del rival.
41. Valkyrie Funfte / Sechste: sin su segundo efecto (Funfte) y Sechste trae la primera Valkyrie del mazo. Abyssal
    Designator: el juego declara el tipo y atributo del monstruo mas comun del mazo del rival. 7: el +700 cuando va del
    campo al cementerio por cualquier via. Blast Juggler: en tu fase principal (no en la de espera). Blast with Chain: sin
    su efecto al destruirse. Blue-Eyes Shining Dragon: sin negar lo que lo elige. Beast Soul Swap: sale el primero del
    mismo nivel. Curse of Fiend: sin bloquear los cambios de posicion. Card of Sanctity: las magias/trampas desterradas
    sin pasar por su salida del campo.
42. Dark Scorpion Cliff / Gorg: solo su primer efecto. Dark Spirit of the Silent: solo niega (sin el ataque de otro).
    De-Fusion: sin invocar los materiales. Des Volstgalph: sin su bono con las magias. Double Snare: destruye a Jinzo.
    Criosphinx: el descarte es al azar. Different Dimension Dragon: sin su proteccion contra efectos que no lo eligen.
43. Dummy Golem: el rival da el de menos valor. Ectoplasmer: sacrifica el de menos ATK. Fairy Box / Fiend Comedian: sin
    cantar la moneda. Falling Down: sin su efecto con Archfiends. Garuda / Gigantes / Fenrir: solo su invocacion. Gaia
    Soul: sacrifica los Piros que puede (hasta 2). Fiber Jar: las magias/trampas vuelven al mazo sin pasar por su salida
    del campo. Giant Orc: fijo hasta el final de tu proximo turno (como Goblin Attack Force).
44. Good Goblin Housekeeping: va debajo del mazo la de menos valor. Horn of Heaven: destruye al invocado tras su
    invocacion (no la niega). Hand of Nephthys: el otro sacrificio lo elige el juego. Horn of Light: sin volver al mazo.
    Inferno: sin su danno. Gravekeeper's Servant: el atacante manda la de arriba (sin impedir el ataque si no puede).
    King of the Swamp: sustituye a cualquier material (no solo a los nombrados).
45. Mind Crush: se declara la carta mas repetida de la mano del rival (siempre acierta). Lighten the Load / Mass
    Driver: tambien hacen su efecto al activarse (si se puede). Lost Guardian: la DEF se suma a la impresa. Michizure:
    se activa como trampa normal el turno en que un monstruo tuyo fue al cementerio (elige un monstruo). Pikeru's
    Circle: anula todo el danno a ti ese turno. Pyramid Energy: siempre +200 ATK. Mushroom Man #2: sin pasar el
    control. Mass Driver: el sacrificio lo elige el juego. Miracle Dig: vuelven los 3 ultimos desterrados.
46. Card Shuffle: siempre baraja el mazo del rival. Cursed Seal: el descarte no tiene que ser una magia. Curse of Royal:
    niega lo que elige 1 magia/trampa del campo. Attack and Receive: ~~en cualquier momento del turno en que recibiste
    danno~~ (resuelto: solo en la ventana justo despues del danno, `V_DANNO`). Blade Rabbit / Ante: el juego elige (el mas fuerte del rival / la de mayor nivel de cada mano).
    Archfiend's Oath: declara la mas repetida que queda en tu mazo. D.D. Designator: declara la mas repetida de la
    mano del rival (siempre acierta).
47. Dice Re-Roll: se repite sola la primera tirada que le conviene a quien la activo (solo en Graceful Dice, Skull Dice,
    Agido y Zorc). Gravekeeper's Watcher: niega una lista fija de cartas que hacen descartar. Fushioh Richie: sin
    voltearse boca abajo; al voltearse trae el Zombi de mas ATK. Union Rider: la union tomada va a tu cementerio y se
    puede desequipar a tu campo; toma la de mas valor. Valkyrie Vierte: al ser destruida en batalla trae sola la
    Valkyrie de mas ATK del mazo. Fuh-Rin-Ka-Zan: destruye los monstruos del rival (o sus magias/trampas si no tiene).
    Exchange / Fruits of Kozaky / Jade Insect Whistle / Dark Designator / Invitation to a Dark Sleep / Magician's Circle /
    Earthshaker / Light of Judgment / Bubble Crash / Array of Revealing Light: el juego elige (ver el comentario de
    cada carta en ia.h). Magic Drain: el rival descarta una magia si tiene. Lightforce Sword: vuelve al empezar su
    4o turno. Dark Scorpion Combination: danno normal (no 400). Megarock Dragon: destierra todas las Rocas. Heart of
    Clear Water: solo en batalla y sin destruirse si sube el ATK. Light of Judgment: el descarte no tiene que ser LUZ.
48. Numinous Healer: ~~en cualquier momento del turno en que recibiste danno~~ (resuelto: solo en la ventana `V_DANNO`). Shadow Spell: el monstruo si puede cambiar
    de posicion. Ojama Trio / Multiplication of Ants: las fichas usan la cara de la carta, se pueden sacrificar y (las
    hormigas) no son Insectos. Order to Smash: destruye las 2 primeras magias/trampas. Share the Pain / Soul Release /
    Painful Choice / Rescue Cat / Rite of Spirit / Soul Resurrection / Salvage: el juego elige (el de menos valor del
    rival, sus ultimos monstruos, al azar, los de mas ATK). Rescue Cat: sus Bestias no tienen los efectos negados.
    Remove Brainwashing: devuelve el control solo al activarse. Riryoku Field / My Body as a Shield: lo que elige un
    monstruo / una lista fija de cartas que destruyen. Peten: siempre trae otro. Ninjitsu Art of Decoy: solo este
    turno. Silpheed / Soul of Purity / Megarock: destierran los de menos ATK.
49. Spell Reproduction: trae la ultima magia del cementerio y descarta las 2 ultimas magias de la mano. Spellbook
    Organization: pone una magia arriba. Sword of Deep-Seated: no vuelve al mazo. Sword of Dragon's Soul: sin destruir
    Dragones. Type Zero Magic Crusher: el descarte no tiene que ser magia. Stray Lambs: sin prohibir otras
    invocaciones. Thunder of Ruler: el rival no ataca ese turno (como Threatening Roar). Two-Pronged Attack / Forceful
    Sentry / Cheerful Coffin / Shallow Grave / Hunter with 7 Weapons: el juego elige. Terrorking / Vilepawn Archfiend:
    sin sus protecciones con dado. Stumbling / Spring of Rebirth: no los niega Jinzo.
50. Different Dimension Gate: destierra tu peor monstruo y el mejor del rival. Dimensionhole: vuelve al empezar tu
    turno. Double Attack / Spell Vanishing / The Dragon's Bead: el descarte puede ser cualquier carta. Dragonic Attack:
    no vuelve Dragon. Cross Counter: sin doblar el danno. Dark Coffin: el rival descarta al azar (o, sin mano, pierde
    su peor monstruo). Kozaky's Button / Dark Coffin / Statue: cuentan cualquier envio de la colocada al cementerio.
    Deck Devastation Virus: sin las que robe despues. Dark Necrofear / Sorcerer of Dark Magic: sin sus efectos.
    Spiral Spear Strike: solo el perforante. Recycle: siempre que tenga mas de 1500 LP. Re-Fusion: sin equiparse.
    Reasoning: el rival declara el nivel mas comun de tu mazo. Skull Lair: elige el mejor del rival. Physical Double:
    la ficha solo copia ATK/DEF y se va en la fase final. Needle Wall: con un 6 no pasa nada.
51. Spell Canceller / Silent Swordsman LV7 / Imperial Order: impiden activar magias, pero no niegan las continuas ya
    boca arriba. Royal Decree: como Jinzo (no se activan trampas). Swords of Concealing Light: los monstruos si pueden
    cambiar de posicion. Return of the Doomed: devuelve el monstruo en el momento. Soul Demolition: solo destierras
    tu (el de mas ATK). Rare Metalmorph: sin negar magias. Smoke Grenade: el rival descarta su mejor carta cada vez que
    sale equipada. Nitro Unit: el ATK original. The Unhappy Girl: no bloquea al rival. Los avisos de la interfaz siguen
    diciendo "6 cartas" aunque el limite cambie (corregido en la tanda AD).
52. Negacion de efectos: los efectos escritos a mano en el motor para cartas concretas (ataque directo, perforar,
    indestructibles en batalla, el calculo de danno, los sucesos por id...) no miran efectoNegado; solo lo hacen las
    vias generales (DefCarta, disparos, activaciones, inmunidades y Jinzo). Spell Canceller / Silent Swordsman LV7 siguen
    impidiendo magias aunque esten negados. Manos a la vista: se revelan al empezar cada turno y al robar o volver
    cartas a la mano.
53. Mystic Probe / Metal Detector / World Suppression: se activan con cualquier carta de ese tipo boca arriba (no solo al
    activarse una). Non-Spellcasting Area / Dust Barrier: solo cuentan como inmunidad a las magias que eligen (ignoraMagia).
    Trap of Board Eraser: anula todo el danno a ti ese turno. Two-Man Cell Battle: invoca sola el Normal de nivel 4 de
    mas ATK. Wynn: el control no vuelve. Xing Zhen Hu: las 2 primeras colocadas del rival. Sarcofagos: el segundo y el
    tercero se ponen solos (sin elegir); Spirit of the Pharaoh trae los Zombis sin preguntar. The Regulation of Tribe /
    Prohibition: declara el tipo mas comun en el campo del rival / la carta mas repetida de su mazo. Primal Seed: las 2
    ultimas desterradas. Penalty Game!: siempre se salta el robo. Shadow of Eyes / The Spell Absorbing Life: sin volteos
    (ni suceso de volteo). Vampire Genesis: descarta el Zombi de mas nivel.
54. Lone Wolf: sin la inmunidad a efectos de monstruo. Necklace of Command: siempre roba. Non Aggression Area: se activa
    en tu turno (no solo en la espera) y solo impide invocar normal / colocar. Ominous Fortunetelling: siempre dice
    "monstruo". Senri Eye: paga solo con mas de 3000 LP. Reversal Quiz: el tipo se adivina al azar. Reshef / Spell Shield
    Type-8: el descarte de Reshef puede ser cualquier carta (con una magia en la mano). Swarm of Scarabs: sin volverse
    boca abajo; destruye el mejor del rival. Sphinx Teleia: sin la prohibicion de atacar.
55. Drop Off: se activa despues de la fase de robo, en ese turno. Gamble: perder = sin robar, invocar normal ni atacar
    tu proximo turno. Gust / Driving Snow / Fairy Guardian: cuenta cualquier magia/trampa tuya que vaya al cementerio
    mientras resuelve una cadena del rival; Fairy Guardian devuelve la ultima magia de tu cementerio. Chosen One: el
    juego elige el monstruo de mas ATK y las 2 primeras no-monstruo. Taunt / Staunch Defender / Ring of Magnetism: el
    rival no puede atacar a otros, pero no se le obliga a atacar (ni se le impide el ataque directo si no hay monstruos).
    Tragedy: con cualquier monstruo del rival que cambio a defensa este turno.
56. nivelM: los efectos que miran el nivel con el id de la carta (en la mano, en el cementerio, al elegir objetivos con
    cartaEn) siguen viendo el nivel impreso. Scroll of Bewitchment: declara el atributo mas comun entre tus otros
    monstruos (o LUZ). Pitch-Black Power Stone: pasa el contador al primer monstruo tuyo que use contadores. Spatial
    Collapse: solo limita invocar normal y activar desde la mano. Rivalry of Warlords: se queda el tipo mas comun de cada
    lado (empate: el primero). Pole Position: solo como inmunidad a las magias que eligen. Solomon's Lawbook: se salta
    todo lo que el motor hace en la fase de espera de ese turno.
57. Theinen / Guardian Dreadscythe / Mirage Knight: se invocan solos cuando se cumple su condicion (Theinen paga 500 + 500
    y gana +3000; Dreadscythe sin equipar la guadania ni su restriccion). Koitsu: +3000 tambien a la DEF. Supply: los 2
    monstruos de mas ATK del cementerio que no son de fusion. Dramatic Rescue: invoca el monstruo de mas ATK de tu mano.
    Spirit's Invitation: el rival devuelve su peor monstruo. Spiritual Energy Settle Machine: descarta la ultima carta de
    tu mano. Pharaoh's Treasure: trae la carta de mas valor. Puppet Magic: el Demonio de mas ATK que se pueda.
58. Brain Jacker / Gearfried the Swordmaster: eligen el mejor monstruo del rival. Collected Power: sin destruir los
    equipos que no valgan. Formation Union: primero separa una union equipada; si no hay, equipa la primera union a su
    primer candidato. Diffusion Wave-Motion: tantos ataques como monstruos del rival (no obligados a cada uno). Forced
    Requisition: el rival descarta al azar. Inspection: paga solo con mas de 3000 LP. Jam Defender: ver 60 (boca arriba solo
    se vuelve a usar en tu fase principal).
59. Shift: pasa el ataque al tuyo que mejor aguanta, o la magia/trampa al de menos valor. Weapon Change: paga solo con
    mas de 2000 LP y elige el que mas gana. Soul Exchange: tomas el monstruo hasta la fase final (lo puedes sacrificar o
    usar de otra forma) y no atacas ese turno. Weather Report: sin la batalla doble. Dark Sage: trae la ultima magia del
    mazo.
60. Amplifier: solo al activar trampas (los efectos continuos de las trampas siguen negados por Jinzo). Bait Doll: la
    trampa colocada siempre se destruye. Battle-Scarred: copia todo lo que pagas en tu fase de espera. Double Spell /
    Serial Spell: solo copian magias de una lista que no eligen ni preguntan. Fiend's Hand Mirror: pasa la magia a otra
    magia/trampa (primero del que la lanzo). Tailor of the Fickle: el juego elige el equipo y el destino. Grave Lure: marca
    el nombre de la carta, no la copia exacta. Ground Collapse: bloquea las 2 primeras zonas vacias del rival. Last Turn:
    el combate se decide por ATK. Level Conversion Lab: elige el monstruo de mas nivel; el nivel nuevo solo cuenta para los
    sacrificios. Major Riot: cada uno invoca sus monstruos de nivel 4 o menos de mas ATK. Makyura: todas las trampas de la
    mano ese turno (no solo una). Neko Mane King: el rival ya no ataca ni invoca normal (no salta a la fase final). Ordeal
    of a Traveler / Question: el rival adivina al azar. Pineapple Blast: el rival conserva sus mejores monstruos. Pyro
    Clock: solo adelanta las cuentas de la lista. Royal Oppression: tras activarse, el juego paga solo por su controlador
    (rival con 1500+ ATK, si tiene mas de 2000 LP). Second Coin Toss: solo en Time Wizard, Gamble y Sasuke Samurai #4.
    Spirit Elimination: solo en los destierros de coste y de los procedimientos. Jam Defender / Skull Lair / Fusion Gate y
    las demas "repetibles" ya boca arriba solo se pueden usar en tu fase principal (no en respuesta).
28. Chaos Command Magician: en los efectos de monstruo que eligen al resolver, simplemente no aparece como
    candidato. Freed the Matchless General: sin su busqueda en la fase de robo. Spellbinding Circle: el monstruo si
    puede cambiar de posicion. Abare Ushioni: sin "cantar" la moneda. Armed Dragon LV3 sube solo (es opcional). The
    Unfriendly Amazon: el sacrificio lo elige el juego. Suijin: el ATK queda en 0 hasta el final del turno (no solo
    en ese calculo). Lekunga: la ficha usa la cara de Lekunga.
29. Magical Hats: si el monstruo atacado se esconde, el ataque sigue contra el (sin repetir el ataque); las del mazo
    las elige el jugador (la IA: las primeras). Time Wizard: sin "cantar" la moneda. Diabound Kernel: sin su efecto
    rapido. Solar Flare Dragon: sin su proteccion contra ataques. Pitch-Dark Dragon / Zombie Tiger: solo el bono de
    ATK/DEF. Mind Control: el monstruo si se puede sacrificar.
30. Panther Warrior: el sacrificio para atacar lo elige el juego. Cobra Jar: la ficha usa su cara y sin el danno al
    destruirla. Bazoo / Gustaph: el bono dura hasta el final de este turno; los desterrados se eligen al resolver.
    Decayed Commander: sin el descarte. Granadora: los 2000 tambien si se sacrifica. Desrook / Infernalqueen: sin el
    dado.

### Tanda 7 (AM): declarar sin mirar lo oculto
Mind Crush, Great Phantom Thief, D.D. Designator, Archfiend's Oath, Prohibition, Dark Designator (un nombre), Array of
Revealing Light, The Regulation of Tribe, The Hunter with 7 Weapons (un tipo), Abyssal Designator (tipo y atributo),
Reasoning (el rival declara un nivel) y Vampire Lady / Lord (monstruo, magia o trampa; luego el rival elige la carta que
manda al cementerio) declaran con la pregunta generica: el jugador elige en pantalla entre su mazo, su cementerio y lo
que el rival ha ensenado (cementerio, desterradas, campo boca arriba); la IA elige con elegirIA usando solo esa misma
informacion (iaDeclararCarta / iaDeclararTipo). Mind Crush y D.D. Designator ya pueden fallar (y castigan al que declara).
Sustituye a los atajos 45 (Mind Crush), 46 (Archfiend's Oath, D.D. Designator), 49 (Hunter), 53 (Regulation, Prohibition),
56 y anteriores (Array of Revealing Light, Reasoning, Vampire Lady, Abyssal Designator, Dark Designator).

### Tanda 7 (AN): el jugador elige
Con la pregunta generica (el jugador en pantalla; la IA con elegirIA o la eleccion por defecto): Blade Rabbit, Swarm of
Scarabs (que monstruo se destruye), Brain Jacker, Invitation to a Dark Sleep, Wynn the Wind Charmer (que monstruo),
Two-Pronged Attack (tus 2 y el suyo), Skull Lair (el objetivo), Exchange (cada uno elige de la mano del otro), Ante (cada
uno elige la que muestra), Shift, Fiend's Hand Mirror (el nuevo objetivo), Tailor of the Fickle (el equipo y su destino),
Painful Choice (tus 5; el rival elige la tuya), Chosen One (tus 3 cartas), Fusion Gate (que fusion), Light of Judgment
(que carta suya), Earthshaker (tus 2 atributos; el rival elige uno), Share the Pain (el rival elige su monstruo),
Pineapple Blast (el rival elige los que se quedan), Last Turn (tu monstruo; el rival el suyo), Different Dimension Gate,
Order to Smash, Supply, Salvage, Rite of Spirit, Soul Resurrection, Re-Fusion, The Creator. Se quito un disparo doble de
Blade Rabbit (ya existia por Dream Clown).

### Tanda 7 (AO): opcionales con si/no
Los mantenimientos de la fase de espera (Messenger of Peace, Skull Archfiend of Lightning, Desrook / Infernalqueen /
Shadowknight / Vilepawn / Terrorking Archfiend, Mask of Brutality, Mirror Wall, Imperial Order, Spirit's Invitation) pasan
por un disparo con la pregunta "¿pagar para mantenerla? (No: se destruye)" (la IA paga si le sobran 1000 LP). Los usos
de la espera (Weapon Change, Senri Eye, Recycle, Inspection) preguntan si se usan (y que monstruo / que carta). Theinen,
Guardian Dreadscythe, Mirage Knight y Peten preguntan si se invocan. Royal Oppression pregunta si se pagan los 800 en cada
invocacion especial del rival. Tras la fase de espera se abre una ventana para estos disparos. Las cartas envueltas
guardan su resolver / elegirIA de antes (resolverAntes / elegirAntes) para el resto de sus efectos.

### Tanda 7 (AP): LV y Behemoth con si/no; continuas que responden; la negacion alcanza lo escrito a mano
Los LV (Silent Swordsman, Horus, Ultimate Insect, Silent Magician, Mystic Swordsman, Dark Mimic) preguntan "¿subir de
nivel?" (subirNivel pide un disparo; la IA sube siempre) y Twin-Headed Behemoth pregunta si vuelve. DefCarta
repetibleRespuesta: Jam Defender, Skull Lair y Soul Demolition boca arriba se pueden usar en respuesta (en la ventana
que pida su condicion). Negacion: motorAtaqueDirecto, motorPuedeAtacar, motorDannoBatalla y motorAntesCalculo comparan
el id efectivo del monstruo (vacio si esta negado), y la indestructibilidad, el perforar y Piranha Army / Susa Soldier
miran efectoNegado. Quedan sin negar los sucesos por id (motorSuceso*) y motorTrasCombate.

### Tanda 7 (AQ): cierre de la negacion; monedas cantadas y adivinanzas
Negacion: los sucesos por nombre de un monstruo que sigue en el campo (invocado, volteado, cambio de posicion, ataque,
control) miran negadoEnZona, y motorTrasCombate / motorTrasCombate2 usan el id efectivo del atacante y del defensor.
Monedas: Time Wizard, Fiend Comedian, Gamble, Abare Ushioni y Fairy Box preguntan "¿cara? (No: cruz)" antes de tirar
(la IA canta cara; la probabilidad es la misma); Second Coin Toss repite si se falla. Adivinanzas: Reversal Quiz y
Ominous Fortunetelling preguntan la clase (monstruo / magia / trampa; la IA dice monstruo, sin mirar); en Question el
rival elige el nombre entre los monstruos de tu cementerio ordenados por nombre (la IA, sin mirar). Siguen sin cantar
(la moneda va igual al azar): Jirai Gumo, Kryuel, Sasuke Samurai #4 (fuera de la cadena) y Ordeal of a Traveler (el
rival adivina al azar).

### Tanda 7 (AR)
Archlord Zerato (procedimiento: sacrifica un Warrior of Zera boca arriba con The Sanctuary in the Sky en el campo; con el
Santuario en tu campo, descarta un LUZ: destruye los monstruos del rival; atajo: el descarte puede ser cualquier carta
con un LUZ en la mano) y Mazera DeVille (sacrifica un Warrior of Zera con Pandemonium en el campo; al invocarse con
Pandemonium en tu campo, el rival descarta 3 al azar). Kryuel, Jirai Gumo y Sasuke Samurai #4 cantan la moneda con la
pregunta (Jirai Gumo y Sasuke #4 al declararse el ataque, por un disparo; Sasuke #4 deja la marca y el rival se destruye
antes del calculo). Ordeal of a Traveler pregunta al atacante la clase (la IA dice monstruo).

### Tanda 7 (AS): cartas extra (Zera the Mant y Zera Ritual)
Cartas fuera de las 1690 Goat y sin duenno en el anime: datos/cartas_extra.txt (nombre TCG|limite|nota). preparar_classic.py
las trata como las firma (YGOPRODeck por nombre, espanol oficial de cards.cdb, caras ES/EN/HD con arte provisional) pero
en cartas_classic.txt van como las Goat (sin desbloqueo). Deja <cache>/extra.json para rivales_gen.py, que saca su
receta de ritual (T|zera_the_mant|zera_ritual) pero no las mete en los mazos generados (asi los mazos no cambian).
Decision: Zera the Mant no tiene duenno (en Monster World es un personaje del juego de rol, no una carta de duelo).
Zera the Mant no tiene efecto: se invoca por ritual con Zera Ritual (niveles 8 o mas), con el ritual normal del motor.

### Carta firma extra: Mischief of the Time Goddess (Leon)
Carta firma de Leon (variante "Valquirias de su hermano", con Valkyrie Erste y Ride of the Valkyries: personajes_datos.py
-> personajes_gen.py --firma -> preparar_classic.py; arte Gemini en nano-banana/classic/arte/mischief_of_the_time_goddess).
Magia rapida: nadie responde a su activacion (motivoNoActivar) y solo como eslabon 1; en tu batalla, si todos tus
monstruos son Valkyries boca arriba (min. 1), al cerrar la cadena (motorDiosaTiempo) se salta al comienzo de la batalla
de tu proximo turno: sin principal 2 ni fase final (Ride of the Valkyries no devuelve tus monstruos), el turno del
rival entero, y sin robo, espera ni principal 1; ese turno no activas otra. Completa el combo: Ride of the Valkyries
desde el cementerio (se destierra: Mischief del mazo a la mano; efecto 1, al ir alli) y Valkyrie Erste invocada por
Ride (Mischief del cementerio a la mano; efecto 2).

### Combos cortados: Swift Gaia the Fierce Knight y Sentinel of the Seas
Las dos cartas que nombraban otras del juego sin estar (listas/combos_cortados.md). Swift Gaia the Fierce Knight: carta
extra (datos/cartas_extra.txt, era Goat): se invoca normal sin sacrificios si es la unica carta de la mano
(puedeInvocarNormal, la IA y control.h como Fusilier Dragon); Spiral Spear Strike lo hace perforar (motorPerforaLado).
Sentinel of the Seas: carta firma de Mako (variante Oceano, con Amphibious Bugroth); rivales_gen.py traduce el nombre
viejo de su texto ("Defender of the Sea", NOMBRE_VIEJO) y personajes_gen.py arma la receta
F|amphibious_bugroth|ground_attacker_bugroth+sentinel_of_the_seas con Polymerization en el mazo.

### Mazos de torneo: las 10 cartas que faltaban (rama classic-torneo-extras)
Cartas extra (`datos/cartas_extra.txt`) para que las listas de torneo sean exactas; su codigo esta en
`cartasTorneoExtras` (ia.h) y sus casos en `prueba_efectos.cpp` (`tandaTorneoExtras`):
- **Command Knight**: tus Guerreros +400 ATK (`modAtk`); con otro monstruo tuyo no se le ataca (`motorAtacable`); si
  solo hay Command Knights, ninguno se ataca y el ataque es directo (`motorAtaqueDirecto`).
- **Cyber Dragon**: procedimiento desde la mano si solo el rival controla monstruos.
- **Foolish Burial**: un monstruo del mazo al cementerio (la IA prefiere los Sirvientes y los que sirven alli).
- **Goblin Zombie**: danno de batalla al rival -> la de arriba de su mazo al cementerio (`motorDannoBatalla`, efecto 3);
  del campo al cementerio -> un Zombi de 1200 DEF o menos del mazo a la mano.
- **Il Blud** y la **regla Geminis** (subtipo `geminis` en cartas_classic.txt): boca arriba es normal; J_ACTIVAR sobre
  el (sin cadena) gasta la invocacion normal del turno, lo marca `Monstruo::geminado` y abre la ventana de invocacion
  normal (Torrential Tribute, Bottomless...); boca abajo vuelve a ser normal (`motorRecalcular`). Su efecto (1 vez por
  turno): un Zombi de la mano o de cualquier cementerio (Necrovalley lo impide); si Il Blud deja el campo, se destruyen
  (`EstadoEfectos::ilBlud`, `motorSaleMonstruo`). El jugador ve "Invocacion normal (Geminis)" en el menu de la carta.
- **Magical Mallet**: pregunta opcional una a una (No: terminar); las elegidas al mazo, baraja y roba las mismas.
- **Izanagi**: procedimiento desterrando un Spirit de la mano (coste de la mano: `activarCarta` vuelve a buscar la carta
  por si cambio su indice). En la fase final, cada Spirit que deberia volver pregunta a su controlador (disparo
  `EF_IZANAGI`); la IA lo deja.
- **Yamato-no-Kami** (Spirit, N en rivales.txt): procedimiento desterrando un Spirit del cementerio; vuelve a la mano en
  la fase final del turno en que se invoca (`vuelveFinTurno` al invocarse por su procedimiento); si destruye en batalla
  (atacando), puedes destruir 1 magia/trampa del rival.
- **Obnoxious Celtic Guard**: no lo destruye en batalla uno de 1900 ATK o mas (`motorSinDestruirBatallaY`).
- **Drillroid**: atacando a uno en defensa (tambien el volteado por el ataque), lo destruye antes del calculo
  (`motorAntesCalculo`; los efectos de volteo se disparan igual).

## Atajos mas visibles para el jugador (propuesta de orden para quitarlos)

1. **[HECHO en la tanda AM] Cartas que "adivinan" con informacion oculta** (se nota y es injusto): Mind Crush y D.D. Designator siempre aciertan
   porque declaran la carta mas repetida de la mano del rival; Archfiend's Oath, Prohibition, The Regulation of Tribe,
   Reasoning y Array of Revealing Light declaran mirando mazos o manos. Propuesta: una pregunta "declara una carta /
   un tipo / un nivel" para el jugador y, para la IA, declarar sin mirar lo oculto (lo mas comun de su propio mazo o al
   azar). Es lo primero a quitar.
2. **[HECHO en gran parte en la tanda AN] Elecciones que el juego hace por el jugador** (lado 0): Blade Rabbit, Ante, Exchange, Brain Jacker, Gearfried the
   Swordmaster, Shift, Tailor of the Fickle, Fiend's Hand Mirror, Swarm of Scarabs, Two-Pronged Attack, Skull Lair,
   Painful Choice, Chosen One, Fusion Gate, Ground Collapse, Hand of Nephthys, Hysteric Fairy, Dreamsprite... El motor ya
   tiene la pregunta generica (`preguntar`, P_ELEGIR): pasarlas a pregunta cuando el lado es el del jugador y dejar la
   eleccion automatica solo a la IA.
3. **[HECHO en las tandas AO y AP] Efectos opcionales que se aplican solos**: pagos de mantenimiento sin opcion (Messenger of Peace, Skull Archfiend,
   Imperial Order...), LV que suben solos, Twin-Headed Behemoth, Theinen / Guardian Dreadscythe / Mirage Knight / Peten
   que se invocan solos, Royal Oppression que paga sola, Weapon Change, Senri Eye, Recycle, Inspection. Propuesta: una
   pregunta si/no para el jugador.
4. **[HECHO en la tanda AP para Jam Defender, Skull Lair y Soul Demolition] Cartas continuas "repetibles" que no responden**: ya boca arriba, Jam Defender, Skull Lair, Royal Oppression,
   Soul Demolition... solo se usan en tu fase principal. Propuesta: permitir `repetible` como respuesta cuando su
   condicion lo pide (ventana de ataque / de invocacion).
5. **[HECHO en las tandas AP y AQ] Negacion parcial**: Skill Drain y compania no apagan los efectos escritos a mano por id (ataque directo, perforar,
   indestructibles, ajustes del calculo). Se nota con cartas comunes; propuesta: pasar esos ids por `efectoNegado`.
6. **[HECHO en la tanda AQ] Monedas y dados sin "cantar"** (Time Wizard, Jirai Gumo, Kryuel, Fiend Comedian...) y adivinanzas al azar
   (Question, Ordeal of a Traveler, Reversal Quiz): poco visibles; quitarlos al final.

## Pruebas

- `src/pruebas/prueba_efectos.cpp`: un escenario por carta (escenario -> esperado) y la cadena (Sakuretsu <- MST:
  resuelve al reves), la IA usando Raigeki y respondiendo con Mirror Force; lista las cartas del motor sin caso.
- `src/pruebas/prueba_ia.cpp`: 300+ duelos IA contra IA con efectos: ninguno colgado, cero jugadas ilegales.
- `src/pruebas/prueba_duelo.cpp`: el jugador por mando activa magias (objetivos y descartes con X), contesta las
  preguntas del duelo (la mitad activando) y ningun duelo queda colgado.

## Pendientes (por frecuencia en los mazos)

Generada por efectos_pendientes.py a partir de ia.h, cartas_classic.txt y los mazos. Se pueden colocar como farol; el menu dice "Efecto aún no disponible".

| mazos | id | nombre | subtipo |
|---|---|---|---|

