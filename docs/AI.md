# The opponent's artificial intelligence

**English** | [Español](IA.md) · Back to the [README](../README.en.md) · [Docs index](LEEME.md)

This document explains how the opponent of Yugioh Classic Remaster thinks: which layers it is made of, how they combine into
a decision, what changes between levels, how its decks and plans are built, how it responds and attacks, how it learns, how
to measure whether it is any good, and how to make it harder or more fun. Everything comes from reading the code
(`remake-ps5/src/ia.h`, `ia_plan.h`, `ia_humana.h`, `ia_red.h`, `aprendizaje.h`, `duelo_log.h`), the data
(`remake-ps5/datos/`) and the documents `remake-ps5/docs/ia_red.md`, `rivales.md`, `personajes.md`, `mazos_torneo.md` and
`duelo_log.md`. What could not be checked is marked **not verified**. Performance numbers are the ones **recorded in those
docs**: they were not measured again while writing this. (Code identifiers, data files and in-repo docs are in Spanish.)

## Index

1. [Overview: the layers and how they combine](#1-overview-the-layers-and-how-they-combine)
2. [Levels 1-8, personalities, noise and budget](#2-levels-1-8-personalities-noise-and-budget)
3. [Plans and decks](#3-plans-and-decks)
4. [Combat and response decisions](#4-combat-and-response-decisions)
5. [Value network and learning](#5-value-network-and-learning)
6. [How to measure whether the AI is good, and how to change it](#6-how-to-measure-whether-the-ai-is-good-and-how-to-change-it)
7. [How to add a plan, a role, a combo or a character](#7-how-to-add-a-plan-a-role-a-combo-or-a-character)

---

## 1. Overview: the layers and how they combine

The AI is not one thing: it is five stacked layers. Each uses the previous one and can be switched off separately (that is why
the game can behave "like before" with an option set to No).

| Layer | What it adds | Where | Switched off by |
|---|---|---|---|
| 1. **Classic brain** | Generates the possible moves of the turn and scores them (in points, roughly ATK units) according to personality; adds noise according to level | `ia.h` (`Cerebro`, `Personalidad`) | cannot be: it is the base of everything |
| 2. **Game plans** | One plan per duel (aggressive, control, lock...) that changes the effective personality, adds or subtracts points for each move, builds the duel's deck and may change mid-duel; memory between duels; adaptive difficulty | `ia_plan.h` | without `roles_cartas.txt` it falls back to the usual deck, no plan |
| 3. **Human layer** | A search that imagines moves (Monte Carlo with determinization), bluffs, reading the player, believable mistakes, visible thinking time, per-character traits | `ia_humana.h` (`Humana`) | Options > AI: Classic |
| 4. **Value network** | Improves the evaluation of each simulation's end with a win probability learned by self-play | `ia_red.h`, `datos/ia_red.txt` | Options > Learning AI: No |
| 5. **Player learning** | Between duels, fine-tunes the network's last layer to THIS player's duels | `aprendizaje.h` | Options > Learning AI: No / Forget what was learned |

### 1.1 How they combine in one decision (human AI, the default mode)

```
 duel state (ia::Duelo) + the opponent's memory (plan, reading of the player, streak)
          |
          v
 [1] Cerebro::candidatos / candidatosRespuesta        possible moves with their points (WITHOUT noise)
          |   principal() / batalla() / activaciones()  + "pass phase" (0 points) as the reference
          |   [2] ajustarPlan(...)                      the duel's plan adds or subtracts points
          v
 [3] Humana::ajustarHumano                              bluffs, kept or silenced traps, pride
          |
          v
   is it obvious?  (a single option, a lethal, one >= 700 points above the second,
                    or a response with nothing worth doing)
      yes | no
      |   |
      |   v
      |  take the best K (K = min(7, 3 + level/2)) + "pass"; with probability pErr a rare move
      |  (fusion, ritual, monster effect) is deliberately "overlooked"   <- believable mistake
      |   |
      |   v   for every candidate, several simulations (rounds with the same seed for all):
      |      - copy of the duel with the player's hidden information GUESSED (determinization)
      |      - apply the candidate and play on 2 turns (current and next) using the Cerebro as policy
      |        (own side: level max(6, level+3); player: a model built from his reading)
      |      - evaluate the end:  value = evaluarHumana(plan, personality, pride)
      |                           [4] with network:  (1-mix)*value + mix*scale*logit(net)
      |      - prune: clearly trailing candidates leave (at least 2 stay alive)
      |   |
      |   v
      |  pick the best on average + prior (+ the character's taste for risk);
      |  sometimes, as a believable mistake, the second if it was close
      v
   Final move  +  visible thinking time  ->  the Turno applies it when the pause is over
          |
          v
 at the end of the duel: [5] the duel is recorded; the network fine-tuning runs OUTSIDE the duel
```

Key points:

- **Classic AI** (option "AI: Classic") is just layer 1 (+2): `Cerebro::decidir` adds Gaussian noise
  `azar.normal() * ruido() * 0.5` to every candidate (except "pass phase") and takes the best. It is fast and much weaker (the
  human AI beats it 64-68 % in the docs' measurements).
- **Human AI** (default): layer 3 uses the classic one as the **policy** of the simulations and as the **prior order** of the
  candidates; what changes is that it looks ahead and evaluates the end.
- The **value network** does not choose moves: it only changes how much the end of each simulation is worth. Believable
  mistakes, bluffs and thinking time are the same with or without a network.
- The game is never blocked: `Turno` runs the search **in slices** (`rebanadaMs` = 4 ms per frame) on the same thread (the
  engine has static state, so it is not touched from two threads) and meanwhile "The opponent is thinking..." shows.
- Only **public information** is used: the AI never looks at the player's hand or face-down cards. Hidden information is
  **guessed** (determinization). What it remembers about the player between duels are tendencies of what he did on the table.

---

## 2. Levels 1-8, personalities, noise and budget

### 2.1 What a level is

A level (1-8) defines **which decks the opponent uses** (ATK ceiling, tributes) and **how much it errs** (noise). The 40
generic opponents are 5 per level; each story-mode character also carries a 1-8 level (`datos/personajes.txt`).

| Level | Decision noise (`ruido()`) | Imagined candidates (K) | Own rollout level | Base believable error | ATK ceiling lv1-4 | Max mean ATK | Tributes 5-6 / 7+ |
|---|---|---|---|---|---|---|---|
| 1 | 520 | 3 | 6 | 1.50 % | 1400 | 1150 | 1 / 0 |
| 2 | 458 | 4 | 6 | 1.45 % | 1500 | 1250 | 2 / 0 |
| 3 | 396 | 4 | 6 | 1.40 % | 1600 | 1330 | 2 / 1 |
| 4 | 334 | 5 | 7 | 1.35 % | 1700 | 1400 | 3 / 1 |
| 5 | 272 | 5 | 8 | 1.30 % | 1800 | 1467 | 3 / 1 |
| 6 | 210 | 6 | 8 | 1.25 % | 1900 | 1540 | 3 / 2 |
| 7 | 148 | 6 | 8 | 1.20 % | 2000 | 1620 | 4 / 2 |
| 8 | 86 | 7 | 8 | 1.15 % | 2100 | 1700 | 4 / 3 |

How the columns are obtained (all verified in the code except the deck columns, which come from `docs/rivales.md`):

- `ruido() = (520 - 62 * (level - 1)) * plan.factorRuido` (`ia.h`, `Cerebro::ruido`). With Gaussian noise of that deviation
  x 0.5 added to points that range from tens to thousands (≈ ATK), level 1 errs often and level 8 almost never. It is also
  used, x 0.25, to waver when deciding whether to activate a response.
- K = `min(7, 3 + level / 2)` candidates imagined by the human layer (plus "pass").
- Rollout level: `min(8, max(6, level + 3))`: it imagines itself playing calmly, with less noise than live.
- Base believable error: `0.015 - 0.0005 * (level - 1)`, multiplied by personality, adaptive difficulty and close
  candidates (below); clamped between 0.3 % and 4 %.
- The ATK ceiling and tributes are a property of **each level's decks** (generated by `rivales_gen.py`), not of the brain.
  Without effects a duel is decided by ATK/DEF, so the ATK ceiling is the main difficulty lever. In IA-vs-IA tests, an
  opponent 3 or more levels ahead wins 83-90 % (`docs/rivales.md`).

### 2.2 What changes from one level to the next

1. **Deck**: ATK ceiling of level 1-4 monsters and how many tributes it asks for (table). Themes go from simple (Insects,
   Plants) to Dragons, Chaos and "Goat Control" at level 8.
2. **Noise**: 520 -> 86 points.
3. **Vision**: more imagined candidates (3 -> 7), larger simulation and millisecond budget (below).
4. **Believable mistakes**: barely change (1.5 % -> 1.15 %) on purpose; the human AI "plays strong from level 1"; what scales
   mostly is the policy noise and deck quality.
5. **Character**: anime characters also carry their own `Rasgos` (below) and a saga/level difficulty.

### 2.3 Personalities

The 10 personalities in `ia.h` (`PERSONALIDADES`) are weights that change how moves are scored:

| Field | What it does |
|---|---|
| `agresividad` | desire to attack and summon in attack position (adds to attacks) |
| `cautela` | respect for the opponent's face-down cards and for being exposed (`miedo()`; penalizes attacking face-down monsters) |
| `farol` | setting spells/traps (and monsters) face-down even if they do nothing |
| `riesgo` | accepting 1-for-1 trades and doubtful attacks; the reckless one (0.95) allows suicidal moves |
| `paciencia` | keeping cards, preparing sacrifices, waiting for the moment; raises the threshold for responding |
| `umbral` | minimum gain to attack (the original game's "dial 1": 0 refuses 1-for-1; the reckless one -500) |
| `ritmo` | base seconds of each move (thinking time) |

The ten: balanced, aggressive, defensive, bluffer, patient, reckless, calculating and, for characters, tricky, control and
dark. Each personality also has an **affinity vector for each plan** (`AFINIDAD`, section 3).

The human layer adds `Rasgos` (`ia_humana.h`): `riesgo` (variance it seeks), `orgullo` (values having the strongest monster and
not hiding it), `lectura` (how much it reads the player), `farol`, `error` (x believable mistakes), `vista` (x simulations
and search time) and `pausa` (x visible thinking time). There is one row per known character (`RASGOS_PERSONAJES`: Joey
gambles and errs more (risk 0.85, error 1.40); Kaiba is aggressive and proud (pride 1.0); Pegasus and Ishizu read the
player (reading 1.8 and 1.7); Bakura and Weevil bluff; Yugi and Dartz see far (vision 1.25)...) and one per personality for
the generic opponents (`RASGOS_PERSONALIDAD`).

### 2.4 Time budget per decision (human AI)

| What | Formula (verified in `ia_humana.h`) | Effective range |
|---|---|---|
| Simulations per decision | `(60 + 14 * (level-1)) * vista * (1 + 0.06 * difficulty) * escala` | between `simsMin` (60) and 170 |
| Milliseconds per decision | `(80 + 10 * (level-1)) * vista * max(0.5, escala)` | between `msMin` (80) and `msTope` (150) |
| Slice per frame | `Turno::rebanadaMs` | 4 ms |
| Horizon | `Humana::horizonte` | 2 turns (current and next) |
| "Obvious" | `umbralObvia` | 700 points above the second |

Note: the header of `ia_humana.h` still says "30-150 ms and 16-170 simulations"; the real code (floors `simsMin` = 60 and
`msMin` = 80) wins. The trainer lowers those floors to generate data faster.

Measured (from `docs/ia_red.md`, container with 10 processes at once): ~41-47 ms average per decision and ~150-175 ms maximum;
`docs/ciclos_pruebas.md` (cycle 2 of the official-rules branch): 14.2 ms average and 101 ms maximum. The target budget is
~150 ms.

### 2.5 Visible thinking time

`Cerebro::tiempoPensar` starts from `personalidad.ritmo` and adjusts by move type (passing phase x0.5-0.6; setting traps
x0.55 because they come in a row; fusion and ritual x1.6; summoning with tributes x1.4), adds `0.12 * log2(options)`, hesitates
longer before attacking face-down monsters if there is fear, 8 % of the time keeps thinking 1-2.5 s more and multiplies by a
log-normal factor; the result stays between 0.35 and 6 s. The human AI multiplies it by its `pausa`, subtracts what the search
already took, and caps at 3.0 s for the opponent (Master Duel's icon). It runs on the game's `dt`, so the animation-speed
option shortens it. Pacing detail: `docs/md_ritmo.md`.

---

## 3. Plans and decks

### 3.1 Card roles (`datos/roles_cartas.txt`)

Every card has role letters generated by `roles_gen.py` by reading its English text (plus a hand list `MANO` with the cards the
text does not make clear). 882 cards have roles.

| Letter | Role | Letter | Role |
|---|---|---|---|
| `q` | burn (damage or LP loss outside battle) | `x` | control change |
| `b` | lock (the opponent cannot attack or is limited) | `a` | alternate win (Exodia, Final Countdown, Destiny Board) |
| `w` | wall (high-DEF monster) | `g` | beater (high ATK for its cost) |
| `d` | destroys or removes monsters | `n` | negation |
| `t` | destroys spells/traps | `r` | recursion (returns from the graveyard) |
| `c` | punisher (trap that answers an attack or summon) | `m` | mill (opponent's deck/hand to the graveyard) |
| `e` | swarm (special summons, tokens) | `o` | draw (draw, search) |
| `k` | combo (Polymerization, rituals, equips, unions) | | |

### 3.2 The 9 plans (`ia_plan.h`, `PLANES`)

Each plan defines the roles it looks for (primary ones give 2 fit points, secondary 1), how much it changes the effective
personality (`dAgr`, `dCaut`, `dFarol`, `dRiesgo`, `dPac`, `dUmbral`), how many monsters its deck carries and a bonus to each
attack.

| Plan | Primary | Secondary | Character | Monsters | Attack bonus |
|---|---|---|---|---|---|
| aggressive | g | k x e | aggression +0.30; threshold -200 | 22 | +120 |
| control | d t | n r | patience +0.20; threshold +100 | 17 | 0 |
| lock (+ burn) | b q w | a | aggression -0.35; threshold +250 | 16 | -60 |
| swarm | e | g r | aggression +0.15 | 23 | +60 |
| combo | k | r o | patience +0.20 | 19 | +20 |
| punisher (traps) | c | n w | bluff +0.30; aggression -0.20 | 17 | -30 |
| mill / draw | m o | w b | patience +0.30 | 17 | -40 |
| tempo (steal monsters) | x | d g | aggression +0.15; risk +0.15 | 19 | +80 |
| alternate win | a | b o w | aggression -0.40; threshold +300 | 17 | -100 |

(Full values are in `PLANES[]`; only the most characteristic ones are shown.)

### 3.3 How a duel's plan is chosen

`prepararRival` (when the opponent is chosen) does, in order:

1. **Tournament deck?** `elegirTorneo`: if the opponent has tournament decks (`Rival::torneo`), with probability **40 %**
   (generic level 7-8), **30 %** (characters) or **40 %** (characters of level >= 7) it takes one **as is, without
   rebalancing**, never the same one twice in a row. Its plan: almost always the one in the list (`D|key|plan|...`, weight x6),
   sometimes another its deck allows. There are **21 tournament decks** (`datos/mazos_torneo.txt`, see
   `docs/mazos_torneo.md`).
2. Otherwise it **draws the plan** with weights (`pesosPlanes`):
   `weight = AFINIDAD[personality][plan] * (0.25 + 4 * fit)^(1 + 0.25 * max(0, difficulty))`, where `fit` is the sum of role
   points of its pool divided by `2 * cards`; then multiplied by:
   - **do not repeat**: last plan x0.08, the one before x0.35, the third x0.7 (memory per opponent);
   - **counter the player** (`Lectura` x `confianza`; only calculating, control, dark, patient and tricky do it fully, the rest
     x0.35): against someone who always attacks, punisher and lock go up; against someone who walls, combo and tempo go up;
     against the trapper, aggressive goes down and control up; against the bluffer, aggressive goes up;
   - **low difficulty** (< 0): punisher, control, lock and alternate weigh less (down to x0.4).
   - The alternate win only enters if the pool has 3 Exodia pieces or Final Countdown / Destiny Board.
3. It builds the **dynamic deck** for that plan (3.5).

During the duel: `revisarPlan` (at the start of each own turn) **changes plan** if the player has <= 1500 LP and the plan is not
a hitting one (finish him, up to 3 changes), or if after >= 2 turns the score (`LP/1000 + 0.6 x card difference`) is < -2.5 and
worse than when the plan began (up to 2 changes); it draws another with the same weights and the game announces "Strategy
change!".

### 3.4 Tactical variation (`Estilo`, mood, combos, horizon)

- **Style per duel** (`Estilo::alAzar`): dump or save resources, set traps before or after battle (35 %), "reverse trap" (empty
  field on purpose to invite the attack; only punisher, lock and alternate), order of summoning/activating and boldness with
  doubtful attacks.
- **Mood per turn**: ±45 points of noise per move type, new every turn.
- **Combos** (`datos/combos.txt`, 43 lines `C|key|pieces|minimum|plan[|torneo]`): a piece is an id, `@role`, `%subtype` or
  `&race`; the minimum is how many distinct pieces are needed in hand + field for the AI to look for it; they give a bonus when
  their pieces are played and dynamic decks include them together. With the `torneo` field, they only give a bonus when played:
  they do not build other opponents' dynamic decks.
- **2-turn horizon** (`Proyecto`): setting the small monster that will be tomorrow's sacrifice for the big one in hand,
  gathering fusion or ritual materials, holding the key trap for the attack that matters, not spending monster removal on small
  monsters if the plan is to wait, and not drawing extra with an almost empty deck (Exodia, Library).

### 3.5 Dynamic decks (`mazoDinamico`)

Built from the opponent's **pool** (its 3 decks in `rivales.txt` / `personajes_rivales.txt`):

1. **Core**: cards present in all its decks and cards only it uses (its identity).
2. **Plan package**: cards with the plan's role, from its pool (weigh more) or from decks of opponents of its level or lower (one
   more with difficulty > 1.5), and the combos of that plan that its pool or level allows.
3. **Monsters up to the plan's target**, then spells and traps, from the pool by frequency and role, different from the
   previous time's deck.
4. **Fix-ups** in order, so none undoes the previous: **mean-ATK ceiling** (that of its usual decks + 60 + 70 x difficulty: the
   level difficulty does not rise), rituals with their spell (or out), Toons with Toon World (or out), fusions only with all
   their materials and with Polymerization (or Metamorphosis), and at least 40.

Result: always valid (40-42 cards: size 40, and 20 % of the time +1 or +2; max. 3 copies; Goat limits; fusions with materials;
never signature cards in generics). If there is no `roles_cartas.txt` (`b.papeles` empty), the opponent uses its usual deck
(`Base::mazoDe`) with no plan.

### 3.6 How many decks there are

| What | Amount | File | Generator |
|---|---|---|---|
| Generic opponents | 40 (8 levels x 5) x 3 variants = **120 decks** | `datos/rivales.txt` | `rivales_gen.py` (deterministic) |
| Character decks | **141 variants** (47 characters; variant 0 is "their own" and appears 60 % of the time) | `datos/personajes_rivales.txt` | `personajes_gen.py` |
| Tournament decks | **21** (5 from Master Duel, 16 from the Goat era) | `datos/mazos_torneo.txt` | `mazos_torneo.py` (hand-made lists) |
| Dynamic deck | a new one per duel | (not saved; only the last one) | `mazoDinamico` |

`rivales_gen.py` and `personajes_gen.py` are deterministic (CRC32 seeds): the same `cartas_ui.txt`, `cartas_classic.txt` and
input data always give the same file. The 54 monsters that cannot be Normal Summoned are excluded (except Toons with Toon
World).

### 3.7 Memory between duels and reading the player

`variedad.txt` (next to `progreso.txt`):

```
adaptable 1|0     humana 1|0     racha N     aprende 1|0
jugador trampas faroles ataques muros confianza duelos [limpia]
plan <opponent> p1 p2 p3        (most recent first)
mazo <opponent> id id id ...    (the last one)
```

- **Per opponent**: its last plans and its last deck (so as not to repeat them).
- **About the player** (`Lectura`, all between 0 and 1): `trampas` (really activates what he sets), `faroles` (sets and does not
  activate), `ataques` (attacks whenever he can), `muros` (puts monsters in defense), `limpia` (destroys the opponent's face-down
  cards with MST, Duster, Heavy Storm) and `confianza` (0 = nothing known). It comes only from **table events**, never from his
  hand or what he has face-down. It is blended with what was seen in the current duel (`Observacion`); between duels it decays
  12 % toward neutral and learns 35 % of the last one.
- **Effect**: `factorMiedo()` raises or lowers respect for the player's face-down cards (range 0.25-2.2): against a trapper it
  attacks less blindly and cleans up first; a bluffer is attacked without fear. In the human layer it also biases what it
  believes is face-down when determinizing.

### 3.8 Adaptive difficulty

`Memoria::racha` counts the player's consecutive wins (+) or losses (-); abandoning counts as a loss according to
`docs/rivales.md` (not verified in the code). The **difficulty** the AI uses is `racha` clamped to **-1..+4** (off in Options: 0):

- `factorRuido = clamp(1 - k * difficulty, 0.45, 1.1)` with k = 0.14 if difficulty is positive and 0.08 if negative. With
  streak +4: noise x0.45 (more accurate); with -1: x1.08.
- Plans are chosen with `(0.25 + 4 * fit)^(1 + 0.25 * difficulty)` (better matched to the deck), the dynamic deck package grows
  (`K = 7 + up to 4 + difficulty` cards), the mean-ATK ceiling rises `70 x difficulty` and cards used by more opponents are
  preferred; with negative difficulty, fewer punisher/control/lock plans.
- The simulation budget rises 6 % per point and believable mistakes move between x0.8 and x1.1.
- It **never** changes the opponent's level or card pool; a player who keeps losing drops at most one point ("never to
  beginner mode").

Note: old comments in `ia_plan.h` and `docs/rivales.md` speak of "+4..-3"; the current code is -1..+4.

---

## 4. Combat and response decisions

### 4.1 What the brain considers in each phase

`Cerebro::candidatos` generates, depending on the phase, `principal()` (summon, set, flip, change position, fusion, ritual, set
spells/traps, discard), `batalla()` (attack) and `activaciones()` (effects and spells; hand cards in battle -60 unless urgent),
filters through the effects engine (Cold Wave, Chain Energy, forbidden summons) and adds "pass phase" with 0 points as the
reference. Moves that do not beat that reference are not made.

### 4.2 Attacking (`ataquesDe`)

For every monster that can attack:

- **Direct attack** (no enemy monsters, or by effect): `200 + ATK x (0.7 + 0.5 aggression) - 0.8 x fear` (+15 % of ATK if the
  opponent has <= 2000 LP); if **lethal**, 100000.
- **Against a monster**: the gain `g` is computed (cards destroyed and damage dealt minus received, "like FUN_005b0f40 of the
  original"): winning is worth `450 (CARTA) + 0.5 x target ATK/DEF + difference`; a 1-for-1 tie is `CARTA + ... - own value +
  100 x risk`; losing is `-(own value + difference)`. Then `points = g - threshold - 0.5 x fear + 100 x aggression`, and `-600`
  if `g < -200` except for the reckless.
- **Face-down monsters it has not seen**: it does not know their numbers; it uses an **estimate** (ATK 1100; DEF 1300 + 30 x
  level, `verAtk`/`verDef`), adds a small bonus for destroying it (+60) and subtracts `250 x caution - 120 x risk` ("you do not
  know what is underneath"). The log shows the estimate and, in parentheses, the real value.
- **New rule (branch `classic-ps5-ia-waboku`)**: against a **face-up, stronger** monster (ATK/DEF already counting field,
  equips and effects: `atkM`/`defM`; its ATK in attack, its DEF in defense) the attack gets `-5000` and is never chosen,
  **unless losing has a reason**: the attacker has an effect when sent to the graveyard (`D_AL_CEMENTERIO`). So the AI no longer
  throws away a monster "just in case" against something it sees as stronger.
- **`miedo()` (fear)** = `caution x (the player's face-down S/T) x 260 x factorMiedo x 0.72^(attacks without punishment)`: it rises
  with face-down cards and drops every time it attacks and nothing happens; it returns to the maximum when the player responds
  to an attack (Mirror Force, etc.). If fear exceeds 300, it takes longer to attack.
- **Attack replay** (official rule, `elegirRepeticion`): if the number of monsters changes after declaring, it scores the
  targets again with the same formula and "do not attack" as the reference.

### 4.3 Setting, summoning and keeping

- Summoning a monster in attack or setting it face-down in defense depends on personality, on the opponent's **threat**
  (`amenaza()`: its highest visible ATK or 1200 if face-down), on pride (in the human layer) and on the plan.
- **Setting spells/traps**: a trap with an effect is worth +260; a quick-play spell +60; a normal spell -220 ("better to
  activate it when the time comes"). `Estilo.mtAntes` and `cebo` adjust when.
- **Discarding** at end of turn (hand above `limiteMano`, normally 6): discards what is worth least, using `valorGuardar`
  (pieces of the plan or of a combo).
- Bluffs and kept traps: human layer (4.5).

### 4.4 Responding (`decidirPregunta` and `candidatosRespuesta`)

In a response window, for each activatable card the value `valorActivacion` (points ≈ ATK, from `DefCarta::valor()`):

- **Classic**: `v += azar.normal() x ruido x 0.25`; it activates the best one if `v > threshold`, with
  `threshold = 120 + 120 x patience - 80 x risk + ajusteRespuesta`. The patient one waits more; the reckless one spends early.
- **Human**: the same scores (`value - threshold`) are the candidates, with "pass" at 0, and are imagined the same way.
- `ajusteRespuesta` (plan): punisher activates gladly (-100); with an almost empty deck it does not draw extra (up to +1800);
  and a punisher/negation trap is **held back** (+250 x patience) if the attack is small (< 1400), there is a single attacker
  and LP to spare, "for the one that matters".
- **New rule `respuestaRedundante`** (branch `classic-ps5-ia-waboku`): responses that are useless are dropped as candidates:
  - an **attack protection** (Waboku, Negate Attack, Threatening Roar) or an **attack negation** (Mirror Force, Magic Cylinder,
    Negate Attack, Sakuretsu Armor, Draining Shield, Dark Mirror Force, Threatening Roar) if the attack was already negated or
    its attacker is gone;
  - if something **of its own in the chain** already neutralizes it (another negation; or a protection after a protection): it
    **does not spend two Waboku or two equivalent negations**;
  - a protection if one has already resolved this turn (`sinDannoBatalla`);
  - a protection against a **harmless** attack (`ataqueInocuo`: it would deal no damage nor destroy anything and there is no other
    attacker to protect against).
- **Ask before activating** (the player's option) only affects the player; the AI always decides.
- **Choosing** (targets, sacrifices): each card has `elegirIA` (generally `iaMejor`/`iaPeor`); its targets are what is worth most
  of the opponent's, its own only if nothing else remains, and from the graveyard the strongest.

### 4.5 Human layer: bluffs, reading and mistakes (`ajustarHumano`)

On top of the scored candidates:

- **Bluff**: setting a non-trap spell face-down adds `110` if it has no traps set, `+120` if its field is empty or the opponent
  has a stronger monster, `+220 x (player's attacks - 0.4) x k` if he attacks with everything, `+160 x cleans x k` if he cleans
  face-down cards; `-250` with 3 or more already set; all x `farolH` (a blend of the personality's bluff and the trait). With
  35 % probability it says a bluff line.
- **Traps against someone who always attacks**: setting a trap adds `300 x (attacks - 0.5) x k` if he attacks more than 0.55.
- **Pride**: setting a monster of ATK >= 1800 face-down subtracts `250 x pride`; summoning one of >= 2000 adds `120 x pride`;
  switching it to defense subtracts `150 x pride`. Kaiba (pride 1.0) never hides his strong monster.
- **Keeping the trap for the big attack**: if the player usually attacks with everything (> 0.55), the current attacker is small
  and behind it comes one with more than +300 ATK, activating the trap now subtracts `450 x (attacks - 0.5) x k` (counter
  `guardadas`).
- **Silencing a marginal trap**: with LP > 3500 and points between 0 and 220, a 50 % x `farolH` chance of subtracting 260
  (counter `ocultas`).
- **Believable mistakes** (`probError`): not random noise but failures a person would make: **not seeing a rare combo**
  (fusion, ritual, Toon or monster effect is removed from the candidates with probability `0.9 x pErr`), overvaluing the attack
  when ahead, gambling when behind, or taking the second if it was close. `pErr = base(level) x adaptive x (1 + 0.12 x (close
  candidates - 2)) x (1 + 0.5 x (character error - 1))`, between 0.3 % and 4 %.
- **Determinization** (`determinizar`) guesses the player's hand and deck order and what is face-down with a bias from his
  `Lectura` (trapper: more likely a trap; bluffer: less) and lowers suspicion with every attack that goes unpunished.

### 4.6 The duel log: how to debug a decision

The duel log (`duelo_log.txt`, `src/duelo_log.h`, see `docs/duelo_log.md`) hooks into the AI through four optional hooks in
`ia.h` (`ia::ganchosLog`: `suceso`, `jugada`, `candidatas`, `decision`) that only **read** the duel: they spend no randomness
or change state, and the copies the human AI imagines are ignored. Before every CPU move it writes:

```
[CPU plan] <plan> | personalidad <p>, nivel <n>              (the first time and whenever it changes)
[CPU decide] <what it does and why> (puntos N) | otras: <the 3 best alternatives with their points>
```

Real format examples (the log text is in Spanish):

```
[CPU decide] ataca con Cebra Oscura: ATK 1800 vs ATK 1250 -> ganaria: ACEPTADO (puntos 1466) | otras: Pasar de fase (0)
[CPU decide] NO ataca, pasa de fase: Hacha del Tigre -> ?: ATK 1300 vs DEF 1450 (boca abajo: valor estimado; real DEF 1400) -> perderia 150 LP: RECHAZADO | otras: ...
[CPU decide] activa Waboku (puntos 680) | responde al ataque de X (ATK 1900), dano estimado 700 | sin proteccion en cadena
[CPU decide] no responde: la mejor activacion posible (Mirror Force, 90 puntos) no supera el umbral
[CPU decide] Invocar X: mejor puntuacion (1666), ATK 1800 vs mayor amenaza rival 1250, en ataque | otras: ...
```

- The **points** are those of the candidate list, **without** decision noise.
- If the chosen move scored less than another, the line ends with
  **`[elegida pese a puntuar menos: el ruido o la busqueda de la IA]`** ("chosen despite scoring less: noise or the AI's
  search"): the search (layer 3) or the noise (layer 1) changed the order. If this appears repeatedly on a decision that looks
  bad, see section 6.
- Every rejected attack is listed ("RECHAZADO") with the calculation it made.

**Recipe to debug "why did it do that?"**

1. Turn on Options > Duel log (Yes by default) and play the duel.
2. Read it from the PC: `python -I remake-ps5/leer_log_duelo.py una -n 120` (or `seguir --filtro "CPU decide"` while playing;
   read-only over FTP; see `docs/duelo_log.md`).
3. Find the `[CPU decide]` line of that move: what points did each alternative have? What plan (`[CPU plan]`) and personality? A
   move chosen with low points = noise or search.
4. Reproduce it without a console: `prueba_duelo_log.cpp` and `prueba_ia_humana.cpp` run IA-vs-IA duels that write the same log;
   with `Humana::depura` (`DEPURA=1/2/3`: no believable mistakes / no bluffs or reading / neither) you isolate which layer causes
   the move.

---

## 5. Value network and learning

(Full detail and tables: `remake-ps5/docs/ia_red.md`; this is the verified summary.)

### 5.1 The network

- **What it is**: a **value** network: it looks at the duel from one side and gives the probability that side wins. It does not
  choose moves.
- **Architecture**: MLP **85 -> 64 -> 32 -> 1** (ReLU and sigmoid), 7617 weights (~30 KB in memory, ~76-80 KB as text), inference
  in plain C++ with stack arrays: ~6 µs per evaluation. File `datos/ia_red.txt` (`red 1 85 64 32`, `escala`, `mezcla`,
  `generacion`, `origen`, `pesos`, `fin <FNV-1a>`: if the checksum fails, it is not valid and the AI plays without a network).
- **85 features**: 33 per side (LP, zone sizes, monsters and their ATK/DEF, spells/traps, hand, roles from `roles_cartas.txt`
  in hand and field, threat) x 2 sides + 19 global (turn, phase, plan, LP and card advantage).
- **Mix**: `value of the end = (1 - mix) x evaluarHumana(plan, personality) + mix x scale x logit(net)` with `mezcla` = 0.5
  (in the file) and `escala` calibrated so the logit weighs like the plan. The network sees only the **determinized copy**: it
  does not cheat.
- **Without a network** (file missing or corrupt, or option "Learning AI: No") the AI is exactly the previous one.

### 5.2 Self-play training

```bash
g++ -std=c++17 -O3 -march=native -I remake-ps5/src remake-ps5/src/pruebas/entrenar_ia_red.cpp -o /tmp/entrenar_ia_red
HILOS=10 DUELOS_CLASICA=20000 DUELOS_HUMANA=1500 PARES=150 \
  /tmp/entrenar_ia_red /compartido/classic-assets/ui/ datos/ datos/ia_red.txt 3 > entrenar.log 2>&1
```

- Processes with `fork` (not threads: the engine has static flags), < 1 GB of RAM.
- Duels with the real opponents (`rivales.txt` + `personajes_rivales.txt`), two opponents of the same level, dynamic decks, plans
  and a random reading of the player. Each position is recorded at the start of each turn from both sides, with the result.
- Generation 0: duels of the **classic** AI (fast, volume) and of the human one without a network; later ones, of the human with
  the champion (low budget: `ESCALA_DATOS`, `SIMS_MIN_DATOS`).
- Optimizer: Adam, cross-entropy, L2 (1e-4), early stopping with 10 % held out, buffer of up to 900k positions.
- **Acceptance criterion (match)**: duel pairs with the same decks and seed, switching sides, real budget. Generation 0 stays if
  it is not weaker than the human AI without a network (**>= 50 %**); later ones only if they beat the champion **>= 55 %**.
- Measure without training: `PARTIDO_SOLO=<net.txt> CONTRA=ninguna|clasica|<net.txt> [MEZCLA_FORZADA=x] [HORIZONTE_RED=n]
  [PRIOR_RED=x] PARES=100`. Other trainer variables: `LAMBDA`, `CONTRA_SINRED`, `DATOS_GUARDAR/DATOS_LEER/NO_SALTAR`,
  `ESCALA_A`, `OBVIA_RED`, `MEZCLA_PERSONAJE`.
- Changing the features or the network shape **invalidates** `datos/ia_red.txt`: retrain. And since the network changes the AI's
  play inside the engine, bump `VERSION_JUEGO` for online mode.

### 5.3 Measured results (copied from `docs/ia_red.md`; ±: 95 % interval)

**Run 2 (the one in use; 20000 classic duels + 1500 human duels per generation):**

| Generation | Positions | Validation loss | Accuracy | Match | Result |
|---|---|---|---|---|---|
| 0 | 803k | 0.496 | 75.4 % | against human without network: 49.7 % ±5.7 | no (needs >= 50 %) |
| 1 | 861k | 0.499 | 74.9 % | against human without network: 51.3 % ±5.7 | **champion** (`datos/ia_red.txt`) |
| 2 | 900k | 0.481 | 75.5 % | against generation 1: 50.0 % ±5.7 | no (needs >= 55 %) |

**The champion measured separately (200 duels each):**

| Match | % wins | ms per decision (mean / max) |
|---|---|---|
| human with network vs human without network | 51.5 % ±6.9 | 44.9 / 156.7 |
| human with network vs classic | 64.0 % ±6.9 | 45.3 / 162.9 |
| human without network vs classic (reference) | 66.0 % ±6.9 | 43.3 / 174.9 |

**The document's own (honest) conclusion**: the network predicts the result far better than chance (75 % accuracy, loss ~0.50
vs 0.69) but **does not make the human AI stronger in self-play** (~50-52 % against without, within the noise). Diagnostics:
with the leaf worth 0 it drops to 38.3 % (the end evaluation does matter); with double the budget it stays at 50.3 % (**the
search is saturated**); a 3-4 turn horizon is worse (45.7 %); the network-based prior order of candidates is worse (42-47.5 %,
left switched off). In a second batch (v2 features of 97 inputs, 128-64 network, TD(λ), `OBVIA_RED`) no variant reached 55 %
(the best ~53 % ±4). What limits the AI is **the rollout policy (the Cerebro) and determinization**, not evaluation or time. What
the network does give is the base for adapting to the player (5.4).

### 5.4 Learning from the player (real game only, `aprendizaje.h`)

- **Recording**: at the start of every turn of every duel against the AI, the 85 features from both sides are saved and, at the
  end, the result. `ia_duelos.bin` (data folder: on PS5 `/data/yugioh_classic/`) keeps the **last 40 duels** (up to 160 positions
  each, ~100-350 KB), with atomic saving and a checksum (if corrupted, it starts from zero). Abandoning (winner -1) is not saved.
- **Fine-tuning**: when a duel ends it stays pending; `avanzarDueloIA` runs it **outside the duel** (result screen, menus) in
  ~1.5 ms slices per frame (prepare, train, measure, save). At least 3 duels with a result are required. It always starts from
  the **base** network and tunes **only the last layer** (33 weights, tied by L2 to the base): with few duels, tuning
  everything overfits. It uses only positions seen **from the opponent's side** (what the search evaluates); it validates on 1 in
  5 duels and **is kept only if it improves**; otherwise the base returns.
- **Result**: the tuned network goes to `ia_red_jugador.txt` (~76 KB); if missing, corrupt or a different shape from the base,
  the base is used. In the test, with a strong player the loss on new duels drops from 0.43 to 0.38.
- **Options** (Options menu): **Learning AI: Yes/No** (default Yes; line `aprende 1|0` of `variedad.txt`) and **Forget what was
  learned** (deletes the recorded duels and the player's network).
- Temporary tuning memory < 2 MB; no copy of the human AI records.

---

## 6. How to measure whether the AI is good, and how to change it

### 6.1 The tests (all headless; in `remake-ps5/src/pruebas/`)

Compile and run (container `arch-ps5`, see each file's header):

```bash
g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_ia.cpp -o /tmp/prueba_ia
/tmp/prueba_ia <ui/> remake-ps5/datos/rivales.txt [duels]
g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_variedad.cpp -o /tmp/prueba_variedad
/tmp/prueba_variedad <ui/> remake-ps5/datos/ [duels per opponent]
g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_ia_humana.cpp -o /tmp/prueba_ia_humana
/tmp/prueba_ia_humana <ui/> remake-ps5/datos/ [duel pairs] [budget scale]
```

(`<ui/>` = the folder with `cartas_ui.txt` and `cartas_classic.txt`.) The full cycle (`ciclo_completo.sh`) runs them all.

| Test | What it measures | Failure criteria (verified in the code) |
|---|---|---|
| `prueba_ia.cpp` | the 120 decks are valid (40-60, copies, banlist); IA-vs-IA duels (800 by default) | illegal move; duel that does not end; results do not vary; the high level does not beat the low one |
| `prueba_variedad.cpp` | (1) valid dynamic decks for every opponent; (2) same opponent N times in a row: **plan entropy**, distinct plans, repetition of the same plan twice in a row, **% of different cards between consecutive decks**, distinct first moves, mid-duel plan changes; (3) level difficulty is kept (the dif >= 3 side wins >= 60 %); (4) reading: against the bluffer it attacks blindly more than against the trapper; (5) adaptive: after streaks noise goes down or up and off means 0; with streak +4 it wins at least 10 points more than with -3; (6) memory saves and reads back identically | mean entropy < 1.5 bits; mean different cards < 25 %; repeats the plan too often (> 20 %); unfinished duels > 2 %; illegal moves; reading or adaptive not responding |
| `prueba_ia_humana.cpp` | (1) human vs classic with the **same decks** (each AI plays each deck and each side; % wins, by level); (2) budget: ms per decision (all, searched, max) and simulations; (3) diversity (same metric as variety, classic vs human); (4) human layer: bluffs, silenced and kept traps, **believable mistakes by level** | illegal moves, unfinished duels, a decision over 175 ms |
| `prueba_ia_red.cpp` | `CU-BE-IA-RED-*` cases: loading and evaluation < 1 ms, corrupt files discarded, recording capped at 40, fine-tuning that improves and does not stall, option off = as before, **strength (with a network it is not weaker and still beats the classic)**, time, diversity | any of the cases |
| `entrenar_ia_red.cpp` | not a test: the trainer and the acceptance match | (5.2) |

**Figures recorded in the docs** (not re-measured while writing this): the human AI beats the classic **68.5 %** (137-63 over 200
duels, 19-Oct-2026; by level 58-73 %, worst at level 7), ~35 ms average per decision (`docs/rivales.md`); 27 of 40 (67.5 %) in
cycle 2 of the official-rules branch; with the network, 64 % against the classic. Variety: in one test, 800 duels gave 363
distinct results.

**Variety metrics** (what they mean): *plan entropy* = uncertainty (in bits) of which plan the same opponent will pick duel after
duel (max log2 9 = 3.17); *distinct plans*; *% of different cards between consecutive decks* (how much the same opponent's deck
changes); *distinct openings* (the first two own turns); *plan changes*.

### 6.2 How to make the AI harder or more fun (concrete levers)

Ordered from least to most invasive. All require running the test cycle; those that touch the engine or the AI require bumping
`VERSION_JUEGO`.

| Lever | Where | Effect |
|---|---|---|
| An opponent's or character's level | `rivales_gen.py` (opponent list) / `personajes_datos.py` | changes its deck (ATK ceiling, tributes), noise, candidates and budget |
| **ATK ceiling** and tributes per level | `rivales_gen.py` (table "by level (1..8): ATK ceiling of level 1-4, mean-ATK ceiling of the deck"; see `docs/rivales.md`) | the main lever: without effects a duel is decided by ATK/DEF |
| **Noise** | `Cerebro::ruido()`: `520 - 62 x (level-1)` | lower = more accurate; affects the classic AI and the rollout policy |
| **Adaptive difficulty** | Options; `Memoria::dificultad()`/`factorRuido()` | range -1..+4: raising the cap or the slope (0.14) makes it harsher with winners |
| **Believable mistakes** | `Humana::probError` (base 0.015) and `Rasgos::error` | fewer = more ruthless; more = more "human" |
| **Budget** | `Humana::escala`, `msTope`, `msMin`, `simsMin`, `simsPresupuesto` | more simulations per decision; careful: the search is saturated (double does not help) and the practical cap is ~150 ms |
| **Horizon and candidates** | `Humana::horizonte` (2), `K = min(7, 3 + level/2)` | more turns or candidates cost time; 3-4 turns got worse with the network |
| **Character traits** | `RASGOS_PERSONAJES` | risk, pride, reading, bluff, vision, pause per character |
| **Personality** | `PERSONALIDADES` in `ia.h` | weights for aggression, caution, bluff, risk, patience, threshold |
| **Decks** | `rivales_gen.py`, `personajes_datos.py`, `mazos_torneo.py` | better cards = stronger AI without touching the brain; **more tournament decks** and a higher probability (`probTorneo`: 30-40 %) |
| **Plans and combos** | `PLANES[]`, `AFINIDAD`, `combos.txt`, `roles_cartas.txt` | more variety and more identity per opponent |
| **Value network** | 5.2 | see the limits below |
| **More fun** | raise `Rasgos::farol`/`lectura`, the 8 % long pauses, bolder `Estilo`, more deck variants per opponent | a less predictable opponent even if not stronger |

**Known limits** (this is where to act if more strength is wanted):

- **Simulation policy**: rollouts use the `Cerebro` (a policy with fixed scoring, not a search); it is what limits strength (see
  5.3). A **policy network** that imitates the search and replaces the Cerebro in rollouts, or more candidates per decision, or
  a finer player model in determinization, are the documented ideas (**not implemented**).
- **The search is saturated**: more budget does not raise the percentage.
- **Traps kept for the big attack**: it is a heuristic (`guardadas`) that only fires if the player attacks with everything
  (> 0.55), the current attacker is small and one with > +300 ATK follows; it is met in a small share of decisions. The tests
  count it (`prueba_ia_humana`: "trampas guardadas para el ataque grande"); **not verified** how often it happens in a typical
  game or its effect on win rate.
- **Believable mistakes**: the rate is low on purpose (~1.15-1.5 % per imagined decision depending on level in the code; **not
  verified** is the measured 0.8 % figure that is sometimes quoted: the code gives 1.15-1.5 % before the multipliers and
  `prueba_ia_humana` prints the actual percentage "of the imagined ones"). Without mistakes there is no "human" AI, and with more
  the AI loses strength.
- **Hidden information**: the AI never sees the player's hand; in online mode the opponent's hand is not hidden yet either (see
  README, 10.3), but that is a protocol matter, not an AI one.
- Effects-engine approximations (Fairy Guardian, Level Conversion Lab...) that the AI inherits (`docs/control_final.md`).

---

## 7. How to add a plan, a role, a combo or a character

### 7.1 A card role

1. Edit `roles_gen.py`: add the letter to `PAPELES` (description), its text rule (in the function that reads the English text)
   and/or the cards by hand in `MANO`.
2. Regenerate: `python -I roles_gen.py <cartas_ui_en.txt> <cartas_classic.txt> datos/roles_cartas.txt`.
3. Use the letter in some plan (`principales`/`secundarios`), combo (`@letter`) or in the network computation (`rasgosRed` of
   `ia_red.h` counts some roles in hand and field: **changing which roles the network sees changes the 85 inputs and requires
   retraining**).
4. Tests: `prueba_variedad` (decks stay valid and varied).

To correct the role of **one** card: add it to `MANO` (it wins over the text reading) and regenerate. Do not hand-edit the
generated `.txt`.

### 7.2 A combo

1. Add a line to `datos/combos.txt`: `C|key|piece piece ...|minimum|plan[|torneo]`. Piece = id, `@role`, `%subtype` or
   `&race`; `minimum` = distinct pieces in hand + field to look for it; `plan` = the plan that puts it in the deck.
2. With `|torneo`, it only gives a bonus when played (it does not build other opponents' dynamic decks): use it for combos of one
   specific tournament deck.
3. `prueba_variedad` validates that the plan exists and every piece is a known card or role.

### 7.3 A new plan

1. In `ia_plan.h`: add the value to the `PlanJuego` enum **before** `NUM_PLANES`, a row in `PLANES[]` (key, primary and
   secondary roles, personality deltas, deck monsters, attack bonus) and a **column** in every row of `AFINIDAD[][]` (10
   personalities).
2. If the plan needs its own behavior, add it in `ajustarPlan` (which moves it adds or subtracts), `ajusteRespuesta` (when to
   hold or spend a response), `Estilo::alAzar` (if it uses "cebo") and `evaluarHumana` (how the human AI values the end:
   aggressive values damage; control and lock, card advantage).
3. If it needs a special fit case (like the alternate win), add it in `encaje`.
4. Update the network's feature vector if the plan appears in it (the 9 plans + "no plan" are 10 of the 19 global inputs:
   **adding a plan changes the network shape**; retrain).
5. Tests: `prueba_variedad` (distinct plans, entropy), `prueba_ia`, `prueba_ia_humana`; bump `VERSION_JUEGO`.

### 7.4 A character with their deck

1. `personajes_datos.py`: the character's row with `id`, names, saga, level, personality, environment, motto and **deck
   variants** (0 is "their own"; English TCG names, `"Name*2"` = 2 copies).
2. Run `python -I personajes_gen.py --firma` and then `python -I personajes_gen.py` (writes `personajes.txt` and
   `personajes_rivales.txt`; adds the core, fusion materials and the ritual spell, removes what cannot be Normal Summoned and
   fills up to 40 with the era and level).
3. For a **tournament deck** for them: a list in `mazos_torneo.py` and an `A|<id>|<key>` line (see `docs/mazos_torneo.md`).
4. For **their own traits**, add a row to `RASGOS_PERSONAJES` in `ia_humana.h` (key = the character's id); otherwise they use
   those of their personality.
5. Dialogue, background and portrait: `README.en.md`, section 6.3.
6. Tests: `prueba_ia` validates the decks (40-60, copies, banlist), `prueba_variedad` the dynamic decks of all opponents and
   `prueba_ia_humana` the human layer.
