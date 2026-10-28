
Download Ver PS5: https://drive.google.com/file/d/12gWXilzDA6UYHZ_1jW5yFbbTKTfrUY3G/view?usp=drive_link

Download Ver PC: https://drive.google.com/file/d/1a56wvZkwOvRUXh8CH60xqCVlZLphcnYL/view?usp=drive_link


<img width="1500" height="837" alt="01_portada" src="https://github.com/user-attachments/assets/b0708306-2151-4ef8-bda5-1f1984cd3461" />
<img width="1920" height="1080" alt="11_figura_3d_abduccion" src="https://github.com/user-attachments/assets/0e0bad2a-07a3-42dc-a5b1-aac185bf7726" />
<img width="2920" height="1920" alt="10_ataque_rafaga_blanca" src="https://github.com/user-attachments/assets/bdaa3fb8-c99b-40be-95b7-2ffaa3cbfde7" />
<img width="1920" height="938" alt="09_equipo_circulo_y_rayo" src="https://github.com/user-attachments/assets/9859b10c-9635-4ace-99b5-33fd20505e68" />
<img width="1920" height="1620" alt="08_efecto_raigeki" src="https://github.com/user-attachments/assets/cae8d4c5-af67-44be-87fc-a3a3d8be4b4f" />
<img width="1920" height="1620" alt="07_efecto_espadas_de_luz" src="https://github.com/user-attachments/assets/f6141c4b-41d8-4a65-b53a-a0d44c39ce05" />
<img width="960" height="540" alt="06_selector_de_cartas" src="https://github.com/user-attachments/assets/c6de6f04-346b-4b9a-a048-c70f8203e2e1" />
<img width="960" height="540" alt="05_duelo_pregunta_fija" src="https://github.com/user-attachments/assets/f4affe0a-e5e9-4b34-b595-873c4d81d37c" />
<img width="960" height="540" alt="04_duelo_figuras_3d" src="https://github.com/user-attachments/assets/34c4a2f5-a1fa-4c78-8eff-48657d1976ee" />
<img width="1280" height="720" alt="03_constructor_lista_de_cartas" src="https://github.com/user-attachments/assets/cd502ca2-9b01-4302-97bf-ee71e1c9705e" />
<img width="960" height="540" alt="02_constructor_mis_mazos" src="https://github.com/user-attachments/assets/b11314c5-07fe-477c-a158-37113ff3ff55" />
<img width="1000" height="600" alt="12_calculo_de_dano_sobre_cartas" src="https://github.com/user-attachments/assets/639d06de-07a5-424c-b9f7-ecf15796e460" />



# Yugioh Classic Remaster

**English** | [Español](README.md)

A Yu-Gi-Oh! card duel with the rules of the classic era (Goat format, April 2005), with iconic monsters as 3D figures on
a 3D board, a "human-like" AI opponent that learns, *Yu-Gi-Oh! Master Duel*-style presentation and a story mode with the
characters of the original series. Versions for **PS5** (homebrew, title `PPSA77002`, folder `remake-ps5/`) and for
**Windows PC** (`remake-ps5/pc/`, with installer).

> Separate sibling project: **Yugi Reload** (repository `Ps5-Yugioh-Reload`). It is not part of this repository.

> Notice: personal, non-profit project for personal use. It contains references and data from Konami and material
> translated from *Master Duel*; the owner will make this repository private again. **It includes no images, audio or 3D
> models** (assets live outside the repo).

---

## Index

1. [What it is, final status and numbers](#1-what-it-is-final-status-and-numbers)
2. [Repository layout and module table](#2-repository-layout-and-module-table)
3. [Duel architecture and pacing rules](#3-duel-architecture-and-pacing-rules)
4. [Data: `remake-ps5/datos` and `classic-assets`](#4-data-remake-ps5datos-and-classic-assets)
5. [Build chain (compile, package, upload)](#5-build-chain-compile-package-upload)
6. ["How do I do X" guides](#6-how-do-i-do-x-guides) (the opponent's AI in detail: [`docs/AI.md`](docs/AI.md))
7. [Tests](#7-tests)
8. [PC version (Windows)](#8-pc-version-windows)
9. [Duel log and diagnostic tools](#9-duel-log-and-diagnostic-tools)
10. [Design decisions, lessons and open items](#10-design-decisions-lessons-and-open-items)

Notes on this document: everything below comes from reading the code, the documents in `remake-ps5/docs/` and the data in
the repo. Where something **could not be verified** (because it depends on the console, on external assets or on not
having compiled in this pass) it is said explicitly. Detail documents are indexed in [`docs/LEEME.md`](docs/LEEME.md)
(Spanish/English index). Container paths (`/compartido`, `/work`) belong to the development environment;
`<project folder>` is the repo root. Note: the in-repo docs, comments, UI strings and data files are in Spanish.

---

## 1. What it is, final status and numbers

### 1.1 What it is

- A **complete Yu-Gi-Oh! duel with its own rules engine** (C++17, header-only), with the rules of the era: 8000 LP,
  hand of 5, one normal summon per turn, tributes, fusion, ritual, Toon, response chains, traps, response windows, and the
  Goat forbidden/limited/semi-limited list.
- An **AI opponent**: 8 difficulty levels, personalities, game plans, dynamic decks, Monte Carlo search, a "human layer"
  (believable mistakes, bluffs, thinking time) and a **value neural network** trained by self-play and fine-tuned to the
  player between duels.
- **Master Duel-style presentation** (pacing, turn banner, phase selector, gear menu, activations, chain, combat,
  translated card effects) on a **3D board** with photorealistic backgrounds per character, field cards that change the
  scenery, and **3D figures** of iconic monsters that attack, defend and die.
- **Story mode** (47 characters of the original series in 6 sagas), **free duel** (40 generic opponents), a Master
  Duel-style **deck builder**, **21 curated tournament decks**, customization (sleeves, playmats), 3D figure gallery,
  tutorial, statistics, Spanish/English everywhere.
- **Online duel** (PS5 only) over P2P with lockstep, cloud room matchmaking (Cloudflare Workers) and DHT fallback.
  **The PC version has no online play** (the button reads "not available on PC").

### 1.2 Final status

Finished and playable on PS5 and PC. The documented automated tests close at **0 failures** (see section 7); validation on
the real console is done by the owner by hand (PC tests do not replace it). Known limits and open items: section 10.

### 1.3 Real numbers (counted from the repo data)

| What | Amount | Source |
|---|---|---|
| Playable cards | **1784** = 1690 Goat + 81 character "signature cards" + 13 "extra cards" | `datos/cartas_firma.txt`, `datos/cartas_extra.txt`; "of 1784" in `docs/certificacion_efectos.md` |
| Cards with certified effect | **1395** (OK 1395, FAIL 0, UNTESTED 0) | `docs/certificacion_efectos.md` |
| Monster 3D figures | **92** | `datos/figuras.txt` |
| Iconic ("emblematic") cards | **83** marked `emblematica` (the file has 157 lines including signature cards) | `datos/emblematicas.txt` |
| Story-mode characters | **47** in **6 sagas** (Duelist Kingdom 16, Battle City 8, Noah 5, Dawn of the Dragons 6, KC Grand Championship 2, Memories of the Pharaoh 10) | `datos/personajes.txt` |
| Character decks | **141** variants | `datos/personajes_rivales.txt` (`V` lines) |
| Generic opponents | **40** in 8 levels (5 per level) x 3 variants = **120 decks** | `datos/rivales.txt` (`R` lines) |
| Tournament decks | **21** (5 inspired by Master Duel, 16 from the Goat era) | `datos/mazos_torneo.txt` (`D` lines) |
| Photorealistic backgrounds | **88** entries: 47 characters + 16 generic themes + 25 field cards | `datos/fondos.txt` |
| Field cards that change the scenery | **25** | `docs/campos.md` |
| Story dialogue | ~300 ES/EN lines | `datos/dialogos.txt` |
| Code | ~71,000 lines in `remake-ps5/src` (about 28,400 are the `ia.h` engine) | `wc -l` |

Two inconsistencies between old documents and the data: `docs/personajes.md` says "78 signature cards" and
`docs/mazos_torneo.md` "10 extra cards"; current data says 81 and 13 (added later). The data wins.

**Not verified**: the file count of `classic-assets` (art, audio, models), the exact title size (docs say ~3.6-4.1 GB of
data and a ~4.5 GB image) and the number of baked Niagara effects, because that folder is outside the repo and was not read
in this pass.

---

## 2. Repository layout and module table

```
<project folder>/
  README.md, README.en.md     this document (ES / EN)
  docs/LEEME.md               documentation index
  remake-ps5/                 ALL the Classic code (PS5 + PC)
    src/                      game code (.h headers + yugireload.cpp)
      pruebas/                automated tests, full cycle, certification
      red/                    online mode (UDP, NAT, DHT, room, lockstep)
    shaders/                  GLSL (Vulkan)
    datos/                    game data (text, versioned)
    docs/                     detailed documentation (indexed in docs/LEEME.md)
    pc/                       Windows layer, CMake, Inno Setup installer
    servidor/sala/            Cloudflare Worker for matchmaking
    escenarios/               Blender scenes (pre-rendered backgrounds, historical)
    third_party/              stb_*, OFL fonts, licences
    yugictl/                  resident console agent (PPSA77002 only)
    *.py / *.sh               data generators, Gemini queues, build, upload
  herramientas/               auxiliary tools (Gemini, Blender, skeletons, Unreal, Master Duel, audio)
  remake/diseno/              shared design (tablero.json, tamanos.json, emblematicas.json, componer_cartas.py...)
```

> Cleanup note: some directories inherited from another project (`launcher/`, `pruebas/`, `re-scripts/`, `remake/src`,
> `docs/01..06`) are proposed for removal; the Classic does not use them.

### 2.1 Table "module -> what it does -> file" (`remake-ps5/src/`)

**Engine, rules and AI (no Vulkan, testable headless)**

| Module | What it does | File |
|---|---|---|
| Duel engine and effects | `ia::Duelo`: state, moves, events (`S_*`), chain (LIFO stack), response windows (`V_*`), combat, effects of 1395 cards (`DefCarta`, `tablaCartas()`), `ia::Base` (cards and opponents), `Cerebro` (classic AI) | `ia.h` |
| Plans and variety | game plans, dynamic decks, combos, player memory, adaptive difficulty (streak) | `ia_plan.h` |
| Human AI | determinized Monte Carlo search, bluffs, reading the player, mistakes, thinking time | `ia_humana.h` |
| Value network | MLP 85->64->32->1, 85 features, weights loaded from `datos/ia_red.txt` | `ia_red.h` |
| Player learning | records duels, last-layer fine-tuning between duels | `aprendizaje.h` |
| Player decks | collection, decks, library, Goat limits, starter decks | `mazo.h` |
| Characters and story | reads `personajes.txt`, sagas, unlocks; dialogue | `personajes.h`, `historia_dialogos.h` |
| Result, statistics, speed | end-of-duel logic, statistics, animation scale | `resultado.h`, `estadisticas.h`, `velocidad.h` |
| Options, saving, language | `opciones.txt`, atomic saving (`.tmp` + fsync + rename), ES->EN table | `opciones.h`, `guardado.h`, `idioma.h` |
| Duel log | `duelo_log.txt` with every event and the AI's reasoning | `duelo_log.h` |

**Control and game (engine + presentation glue)**

| Module | What it does | File |
|---|---|---|
| Duel control | table, console-style focus, context menus, gamepad state machine (no Vulkan) | `control.h` |
| Duel in the game | `presentarSucesos`, table-to-engine sync, questions, chain, viewers | `duelo_juego.h`, `duelo_cadena.h`, `duelo_registro.h`, `duelo_consulta*.h`, `duelo_visores.h`, `duelo_ui.h` |
| Deck builder | logic, UI and hooks (Master Duel-like) | `constructor.h`, `constructor_ui.h`, `constructor_textos.h`, `mazo_duelo.h` |
| Menus | main menu (Egyptian door), options, title screen | `menu_principal.h`, `menu_ui.h`, `menu_md_ui.h`, `portada.h`, `portada_ui.h` |
| Customization | sleeves, playmats, controls, statistics | `cosmeticos*.h` |
| 3D gallery, tutorial | figure viewer and rules | `galeria*.h`, `tutorial*.h` |
| Result screen | end of duel with unlocked signature cards | `resultado_juego.h`, `resultado_ui.h` |
| Interface | SDF text, cards, HUD, mouse zones | `interfaz.h` |
| Main program | Vulkan game class, loop, camera, loading, startup | `yugireload.cpp` (name inherited from the template) |

**Master Duel-style presentation (`md_*` and related)**

| Module | What it does | File |
|---|---|---|
| MD data player | evaluates Unity `AnimationCurve`, Bézier and Timeline/Tween | `clips_md.h` |
| Phases, gear, banners | phase selector (OPTIONS), duel menu, turn banner, LP | `fases_md.h`, `fases_md_juego.h`, `fases_md_ui.h` |
| Card pacing | card movements (draw, place, graveyard, flip, activation) with MD's curves | `md_ritmo.h`, `md_ritmo_juego.h`, `md_ritmo_ui.h`, `md_ritmo_pc.h` (capture scenes) |
| Translated effects | 3D tracks of `CardRunEffect`s, halos, effect sprites | `md_cardrun_ui.h`, `md_efectos_ui.h`, `efectos3_juego.h` |
| Special entrances | fusion/ritual summons and key cards | `entrada_md*.h`, `entrada_clave*.h` |
| Turn pacing | draw after the banner, minimum reading time, blocking phase banner | `ritmo_turno.h`, `ritmo_turno_juego.h`, `ritmo_turno_ui.h` |
| Attack | direct attack, Bézier travel, damage, smoke, attacker lift | `ataque_md.h`, `ataque_md_juego.h`, `ataque_md_ui.h`, `ataque_elevacion_juego.h` |
| Figure beams | breath, magic and lightning of the 3D figures | `ataque_fig_fx.h` |
| Spell/trap effects | activation animation (Dark Hole, Raigeki, Mirror Force...) and its Vulkan-free logic | `efectos_cartas.h`, `efectos_logica.h` |
| Opponent shows the card | like *Joey the Passion*, before playing it | `cpu_muestra.h`, `cpu_muestra_ui.h` |
| Equip | equip circle with link beam and abduction | `equipo_ovni_juego.h` |
| Questions and lists | fixed duel question, choosing among cards with artwork | `pregunta_ui.h`, `lista_cartas_ui.h` |
| Coin, die | coin toss | `moneda*.h` |

**3D world and sound**

| Module | What it does | File |
|---|---|---|
| 3D figures | `.glb` loading, skeleton, attack/defense/death, `datosFigura` table | `figuras.h`, `orientacion_figura.h` |
| Environments and backgrounds | photorealistic background with depth, light and particles per character | `entornos.h` |
| Field cards | board and particles per field card | `campos.h` |
| Forest | Forest field card (code-built 3D world) | `bosque.h` |
| Pack effects | baked flipbooks from the Niagara Examples Pack | `fx.h` |
| Audio | own mixer (16 effects + music with crossfade), WAV and OGG | `audio.h` |

**Input and network**

| Module | What it does | File |
|---|---|---|
| Mouse | interface zones -> gamepad pulses (only read on Windows) | `raton.h`, `raton_juego.h`, `pc/raton_plataforma.h` |
| Online mode | menu, turn clock, rematch | `en_linea.h`, `en_linea_juego.h`, `en_linea_ui.h` |
| Network | UDP, reliable channel, lockstep, shuffle commitment, NAT/STUN/UPnP, DHT, room, ELO | `red/*.h` |
| Room server | Cloudflare Worker + Durable Object | `servidor/sala/` |
| Screen tests | full front-end test on the PC preview | `front_total_pc.h` |

### 2.2 Scripts in the `remake-ps5/` root

| Script | Purpose |
|---|---|
| `sincronizar.sh` | copies code and assets into the PS5 template and compiles shaders |
| `probar_rama.sh` | builds a branch (PS5 + PC preview) under a lock and leaves the title ready |
| `parche_base.py` | patches to the Vulkan base (binary glb, JOINTS_0, photos/video, OPTIONS, Win32) |
| `armar_pc.sh` | builds the Windows `.exe` and assembles the playable folder |
| `imagen_titulo.py`, `subir_imagen.py` | exFAT title image and upload to the console |
| `subir_ps5.py`, `vaciar_titulo_ps5.py`, `ciclo_ps5.sh` | folder upload (old, slow method) |
| `fotos_ps5.py`, `esperar_video_ps5.py`, `leer_log_duelo.py` | photos, video and log from the console |
| `preparar_classic.py`, `preparar_ui.py` | card faces and `cartas_ui*.txt`, `cartas_classic.txt` |
| `rivales_gen.py`, `personajes_gen.py`, `personajes_datos.py`, `roles_gen.py`, `mazos_torneo.py`, `emblematicas_gen.py`, `fondos_datos.py` | generate the `datos/` files |
| `cola_classic.py`, `cola_extra.py`, `cola_personajes.py`, `cola_fondos.py` | Gemini queues (card art, portraits, backgrounds) |
| `fondos_post.py`, `retratos_post.py`, `portada_post.py`, `komami_dorso.py` | image post-processing |
| `bosque_insumos.py`, `entornos_insumos.py`, `campos_insumos.py`, `bosque_mallas_blender.py` | CC0 textures (Poly Haven, ambientCG) |
| `captura_*.sh`, `pixeles_ft.py` | captures on the PC preview for review |
| `efectos_pendientes.py` | regenerates the pending-effects list in `docs/efectos.md` |

---

## 3. Duel architecture and pacing rules

### 3.1 Layers

```
 gamepad / keyboard / mouse
          |   (PAD_* bits; the mouse is translated into the same gamepad pulses)
          v
   control.h  ---- table state, focus, menus, state machine (no Vulkan)
          | moves (J_*)                                    ^ events (animations to run)
          v                                                |
   ia::Duelo (ia.h): pure engine (rules, effects, chain) --+   ia::Cerebro / Humana: opponent decisions
          | events (Suceso: S_ROBO, S_INVOCA, S_ATAQUE, S_COMBATE, S_ACTIVA_MT, ...)
          v
   duelo_juego.h: presentarSucesos -- presentation queue in engine order
          |  esperaEfecto / pacing
          v
   interfaz.h + *_ui.h + yugireload.cpp -- draws (Vulkan), sounds (audio.h), 3D figures, effects
```

- **Pure engine.** `ia::Duelo` (`ia.h`) is authoritative: it knows nothing about Vulkan, the UI or `control.h`. A move
  (`Jugada`) produces **events** (`Suceso`: draw, summon, set, flip, position, spell/trap or effect activation,
  graveyard, declared attack, combat, damage, phase, turn, chain, resolution, negation, LP, control, hand, banished,
  discard, modification, reveal). `Duelo::decide()` says who plays now; `Duelo::ocupado()` blocks normal moves while a
  chain, question or half-done combat is pending.
- **Chain and response windows.** Activating pushes a link onto `ef.cadena` (LIFO); the other side gets a window
  (`ef.ventana`: `V_INVOCACION_NORMAL`, `V_VOLTEO`, `V_INVOCACION_ESPECIAL`, `V_ATAQUE`, `V_ACTIVACION`, `V_FASE_FINAL`,
  `V_DISPARO`, `V_INICIO_BATALLA`, `V_PASO_DANNO`, `V_ESPERA`, `V_FIN_BATALLA`, `V_COLOCADO`, `V_DANNO`) **only if it has
  something legal to respond with**; two consecutive passes resolve. Effects triggered during a chain or combat build a
  new chain afterwards. Combat is carried out when the attack window closes. Detail: `remake-ps5/docs/efectos.md`.
- **Presentation.** The UI **decides no rules**: it receives events and presents them in order with `presentarSucesos`
  (`duelo_juego.h`) and `sincronizarMesaEnOrden`: what leaves in a not-yet-presented event stays in its zone, and what
  arrives is not there yet (it arrives travelling when presented). `esperaEfecto` covers every travel and activation:
  **the duel (and the opponent, and the tests) wait until the table is up to date**. Hence logic and presentation are
  separate: the engine can run headless (tests, training, online mode) and presentation only adds time.
- **Online mode.** Only decisions travel (each `Jugada`) with the state hash after applying it; both machines run the same
  engine (lockstep); if the hash differs, desync and the duel ends without a winner. Therefore **the engine cannot change
  between two versions without bumping `VERSION_JUEGO`** (`red/canal.h`).
- **Options that touch presentation but not the engine:** animation speed (Normal = Master Duel; Fast x1.75; Very fast
  x2.75; R2 scales), 3D figures on/off, duel log.

### 3.2 Pacing rules

Project rule: **our look and feel stays; timings, curves, order and sounds come from Master Duel** and, for the opponent,
the gesture of *Joey the Passion*. Timing table (detail and sources in `remake-ps5/docs/md_ritmo.md`, `ataque_md.md`,
`md_fases.md`, `md_entradas.md`):

| Element | Time used | Origin |
|---|---|---|
| Own draw | 0.58 + 0.22 s (0.80); in the turn: 0.25 + 0.28 + 0.50 shown + 0.65 = 1.68 s | MD `CardMoveDeck2HandMyself`, `DrawOperation` |
| Opponent draw | 0.45 s (not shown) | MD `CardMoveField2Hand` |
| Hand -> zone | 0.50 s (MD curves); strong monster 0.67 / 0.88 s | MD `CardMoveHand2MonstorZone*` |
| To graveyard / banished / to hand | 0.42 s / 0.45 s | MD `Field2GraveNormal`, `Field2Hand` |
| Flip | 0.45 s | MD `CardFlipPlate*` |
| Activation | 0.15 + 0.70 + 0.25 = 1.10 s (the card lifts and faces the camera) | MD `CardShow` |
| Chain link 2+ | +0.55 s; resolution 0.60 s | MD `DuelChainStack/Resolve` |
| Turn banner | 2.2 s (own) / 1.617 s (opponent) | MD `DuelTurnChange00/01` |
| Phase banner | ~0.97 s, blocks the opponent | MD `EffectTaskPhaseChange` |
| Phase selector | in 0.12 s (QuartOut), out 0.12 s | MD `PhaseSelectWindow` |
| Opponent "thinks" (cap) | 3.0 s | MD `CpuThinkingIcon.MAXTHINKINGTIME` |
| Opponent shows the card before playing it | flip 0.267 s + 1.067 s with blinking frame = 1.33 s | *Joey the Passion* (30 frames/s, static analysis of the executable) |
| Minimum pause after a visible opponent move | 0.9 s (`ritmoturno::LECTURA_MIN`) | own observation |
| Full attack | **~2.95 s** (High, ATK >= 2500: 3.15 s; with 3D figure: 3.25 s) | *Joey the Passion* (~3 s) |
| Attacker travel | 0.45 s (Low) / 0.65 s (High) | MD `StrikeLight00/02` |
| Pause on target after the hit | 0.50 s | MD `attackExecPostTime` (0.25) and Joey (0.53) |
| Destruction in combat | breaks at 0.70 s and reaches the graveyard at ~1.12 s | MD `ReqBrokenEffect` |
| Attacker lift on declaration | up 0.2 s, down 0.2 s (0.45 of the table) | owner's request; MD's tweens could not be read |
| LP count | 1.0 s linear; damage number ~0.8 s | MD `DuelLP.countTime` |

Joey the Passion, measured in its executable (30 frames/s): turn banner 2.13 s, draw 0.27 s, summon 1.1 s, full attack
~3.0 s (3.2 s with bounce). Master Duel has no fixed waits between CPU moves: pacing comes from its task queue
(`RunEffectWorker`); here `presentarSucesos` with `esperaEfecto` emulates it.

How the data is obtained (Konami's data does **not** go in the repo): classes and constants come from Il2CppDumper over
`GameAssembly.dll`; ScriptableObjects/Timelines/Tweens are read with UnityPy; everything is exported with the scripts in
`herramientas/md_ritmo`, `herramientas/md_fases` and `herramientas/fx_md` to `classic-assets/md_ritmo`, `md_ui` and
`fx_master_duel`, and `sincronizar.sh` copies it into the title. Without that data the game falls back to the same values
hand-written in the code (or to no effect).

---

## 4. Data: `remake-ps5/datos` and `classic-assets`

### 4.1 `remake-ps5/datos/` (versioned, UTF-8 text, `#` = comment)

| File | Format | Written by | Read by |
|---|---|---|---|
| `personajes.txt` | `id\|name_es\|name_en\|saga\|level\|personality\|environment\|motto_es\|motto_en`. Saga order: `reino ciudad noah dragones campeonato faraon`. `entorno` = background key | `personajes_gen.py` (from `personajes_datos.py`) | `personajes.h` |
| `personajes_rivales.txt` | like `rivales.txt` + `V\|id\|variant\|name_es\|name_en` (variant 0 = "their own", 60 % of the time) | `personajes_gen.py` | `ia.h` |
| `rivales.txt` | `R\|key\|level\|personality\|theme\|name_es\|name_en\|motto_es\|motto_en`; `M\|key\|variant\|ids` (main); `X\|...` (fusion); `F\|fusion\|mat+mat`; `T\|ritual\|spell`; `N\|ids` (not normal-summonable) | `rivales_gen.py` (deterministic, CRC32 seeds) | `ia.h` |
| `mazos_torneo.txt` | `D\|key\|plan\|name_es\|name_en`; `M\|key\|ids`; `X\|key\|ids`; `K\|key\|key ids`; `A\|opponent\|keys` (who gets it) | `mazos_torneo.py` (hand-made lists, validated) | `ia::Base::leerTorneo` |
| `combos.txt` | `C\|key\|pieces\|minimum\|plan`; piece = id, `@role`, `%subtype` or `&race` | by hand | `ia_plan.h` |
| `roles_cartas.txt` | `id\|roles` (letters: q burn, b lock, w wall, d removal, t spell/trap removal, c punisher, k combo, e swarm, o draw, a alternate win, r recursion, g beater...) | `roles_gen.py` | `ia_plan.h`, `ia_red.h` |
| `efectos_cartas.txt` | `id\|effect` (activation animation: `agujero_negro raigeki fuerza_espejo renacer olla espadas polimerizacion cambio_corazon tormenta_pesada tifon cilindro_magico plumero mil_cuchillos ritual dado moneda magia trampa`). No entry: generic spell (green) or trap (pink) | by hand | `efectos_cartas.h` |
| `figuras.txt` | `id slug` (card -> model `models/yugi/<slug>.glb` and `<slug>_anim.glb`) | `emblematicas_gen.py` | `figuras.h` |
| `emblematicas.txt` | `id\|name\|class\|emblematica` (or `firma`) | `emblematicas_gen.py` (from `remake/diseno/emblematicas.json` and `cartas_firma.txt`) | tools |
| `cartas_firma.txt` | `TCG name\|limit\|owners` (unlocked by beating an owner) | `personajes_gen.py --firma` | `preparar_classic.py` |
| `cartas_extra.txt` | `TCG name\|limit\|note` (cards outside the 1690, no owner) | by hand | `preparar_classic.py` |
| `dialogos.txt` | `id\|type\|text_es\|text_en`; types `intro` (2-4 lines), `pierde` (opponent line when the player wins), `gana`; id `_final` with types `texto` and `creditos` | by hand | `historia_dialogos.h` |
| `fondos.txt` | `id\|theme\|fallback\|place_es\|place_en` (id = character's environment, theme or `campo_<card>`) | `fondos_datos.py` | `entornos.h` |
| `ia_red.txt` | `red 1 85 64 32`, `escala`, `mezcla`, `generacion`, `origen`, `pesos`, `fin <FNV-1a>` (if the checksum fails, the file is invalid) | `entrenar_ia_red.cpp` | `ia_red.h` |
| `red_stun.txt` | `name port [fallback_ip...]` (STUN servers) | by hand | `red/resolver.h` |

### 4.2 `classic-assets/` (external folder, **not included in the repo**)

`sincronizar.sh` takes it from `YUGI_ASSETS` (default `/compartido/classic-assets`). Structure it expects (read from
`sincronizar.sh`) and how each part is produced:

| Subfolder | Content | How it is produced |
|---|---|---|
| `ui/` | `cartas_ui.txt` (`id\|name\|class\|ATTRIBUTE\|race\|level\|atk\|def\|text`), `cartas_ui_en.txt`, `cartas_classic.txt` (`id\|passcode\|limit\|subtype[\|firma\|owners]`), `cara_<id>.png`, `mini_<id>.png`, `cara_en_*`, `cara_hd_<id>.jpg`, `portada.jpg`, `portada_letras.png`, `portada_brillo.png`, OFL fonts | `preparar_classic.py` (Goat list from YGOPRODeck, Spanish text from the ygopro `cards.cdb` database, artwork remastered with Gemini or provisional, frame via `remake/diseno/componer_cartas.py`); `portada_post.py` |
| `models/yugi/` | `<slug>.glb` (rigid) and `<slug>_anim.glb` (skeleton with idle, attack, hit, defense, death clips), `tablero.glb`, `esfera.glb`, `rayo.glb`, `dado_md.glb`, `moneda_md.glb` | figure pipeline (6.2); `herramientas/blender` |
| `personajes/` | `retrato_<id>.png` (RGBA, 1400 px tall) | `cola_personajes.py` + `retratos_post.py` |
| `fondos/` | `<id>.jpg` (<= 8192 px), `<id>_prof.png` (16-bit depth, log2(m)/12, 65535 = sky), `<id>.txt` (`plano`, `origen`, `cielo`, `niebla`, `tinte`) | `fondos_datos.py` -> `cola_fondos.py` (Gemini) -> `fondos_post.py` (Real-ESRGAN x4 + Depth Anything V2) |
| `entornos/` | CC0 textures (`*_color.png`, `*_normal.png`) and `pano/` (pre-rendered panoramas made with Blender, historical) | `entornos_insumos.py`, `escenarios/*.py` |
| `campos/` | 512 px board textures for each field card | `campos_insumos.py` |
| `bosque/` | Forest textures and `.malla` meshes | `bosque_insumos.py` + `bosque_mallas_blender.py` |
| `fx/` and `fx/pack/` | flipbooks (`.png` + `.json`) of the Niagara Examples Pack | `herramientas/unreal/hornear_niagara.bat` |
| `fx_master_duel/` | `horneado/` (MD flipbooks), `dado_moneda/` | `herramientas/fx_md/hornear_md.sh`, `sim_fx.py` |
| `md_ui/` | `*.mdtl`, `*.mdui`, `md_ui.png`, `md_ui_atlas.txt`, `md_entradas.png/_atlas.txt` | `herramientas/md_fases/exportar_clips.py`, `exportar_entradas.py` |
| `md_ritmo/` | `movimientos.txt` (141 movements), `efectos/<ef>.mdp` + `.png`, `md_efectos.png` | `herramientas/md_ritmo/exportar_movimientos.py`, `exportar_pistas.py`, `exportar_efectos.py` |
| `audio/` | `audio.txt` (`key\|file\|volume\|loop`, loop = `1:<start>:<end>` in samples), `.wav` (PCM 16 bit 48 kHz) and `.ogg` | `herramientas/audio/extraer.py`, `convertir.py`, `seleccion.json` |
| `sce_sys/` | `icon0.png`, `pic0.png`, original title art | title art |

The code tolerates the absence of almost everything (falls back to neutral textures, the theme background, "activation by
code only" or a silent game) and `sincronizar.sh` warns on the console about what is missing.

**Real external dependency**: several art-pipeline scripts (`cola_*.py`, `preparar_classic.py`, `retratos_post.py`,
`fondos_post.py`, `portada_post.py`, `emblematicas_gen.py`) have hard-coded paths into the sibling project's tree
(character photos, Gemini art in `nano-banana/`, `herramientas/pylib`, `esrgan`, `onnx` models). To regenerate art from
scratch, adjust those constants (`BASE`, `PUENTE`, ...) to wherever that material lives.

---

## 5. Build chain (compile, package, upload)

### 5.1 Requirements

- Docker with the **`arch-ps5`** container (Arch Linux with the PS5 payload SDK, `clang`, `g++`, `ninja`, `cmake`,
  `glslangValidator`, `rsync`, `flock`, `python3` with Pillow and `mingw-w64-gcc` for Windows). Mounts: the shared work
  folder as `/compartido` (`/compartido/wt/<branch>/` are the git *worktrees*) and the template at `/work`.
- Base template: *PS5 Vulkan Template* (Sascha Willems, MIT; RADV driver). Two copies: **`/work/PS5_ClassicRemaster`**
  (title `PPSA77002`) and **`/work/PC_ClassicRemaster`** (sources only, for the `.exe`).
- The assets (`classic-assets`) live outside the repo (section 4.2).
- **Build lock**: the templates are shared by all branches, so each script takes `flock /compartido/.compilar.lock`; two
  branches are never built at the same time.
- Not verified in this pass: that the container and template are up today (nothing was executed).

### 5.2 PS5: build a branch

```bash
# inside the arch-ps5 container
bash /compartido/wt/<branch>/remake-ps5/probar_rama.sh /compartido/wt/<branch>/remake-ps5 - 0
# args: <remake-ps5 folder> <output.mp4 | -> <seconds>; with "-" it only compiles (no preview video)
```

`probar_rama.sh`: (1) takes the lock; (2) `sincronizar.sh` copies code and assets to `/work/PS5_ClassicRemaster`, applies
`parche_base.py`, compiles shaders with `glslangValidator -V --target-env vulkan1.3` and rewrites `assets.json`;
(3) `ps5/tools/build.sh` builds `eboot.bin`; (4) `ninja -C build/ps5-host ps5-samples-host` builds the **PC preview**
(windowless Vulkan, *headless* surface, `PS5_HOST_REFERENCE`) used for captures and tests; (5) `rsync` leaves the finished
title in `/compartido/ps5/PPSA77002`. One known warning: pointer comparison in `stb_vorbis.c` (third-party code).

`sincronizar.sh` also edits `param.json` (name "Yugioh Classic Remaster", `attribute3 |= 0x80040` to offer 4K at 120 Hz
if the TV accepts HDMI 2.1) and **refuses to run if the template is not `PPSA77002`**. `parche_base.py` applies each patch
once and fails loudly if the base changed: binary `.glb` in `vkglTF`, uint8 `JOINTS_0` (Meshy), photos and video on
request, OPTIONS 2 s, window name, and `#if defined(_WIN32)` branches for Windows.

### 5.3 PC (Windows)

```bash
bash /compartido/wt/<branch>/remake-ps5/armar_pc.sh /compartido/wt/<branch>/remake-ps5 [output_folder]
# variables: YUGI_TITULO (already built PS5 title), SOLO_EXE=1 (does not touch datos/)
```

Syncs under the lock, copies **only sources** to `/work/PC_ClassicRemaster`, builds `YugiohClassic.exe` with `mingw-w64`
(`pc/CMakeLists.txt`, `pc/toolchain_mingw.cmake`, static link: a single `.exe` with no MinGW DLLs; Vulkan comes from the
driver's `vulkan-1.dll`), assembles the playable folder (assets as **hard links** to the PS5 title's), downloads
`SDL3.dll` (fixed version checked with `pc/SDL3.sha256`, zlib licence), converts `icon0.png` to `yugioh.ico` and writes
`LEEME.txt`.

### 5.4 Title image for the console

Uploading ~11,000 files by FTP takes hours; the title is shipped as **a single exFAT file** that ShadowMount+ mounts:

```
python -I remake-ps5\imagen_titulo.py --verificar         # reads the title folder and writes PPSA77002.exfat
python -I remake-ps5\subir_imagen.py                      # uploads to /data/homebrew/ (resumable, ~7 min for ~4.5 GB)
```

- `imagen_titulo.py`: own exFAT writer (no mounting, no administrator): 512-byte sectors, **64 KiB clusters** (required by
  ShadowMount+'s fast `optimized-1.7` profile), FAT at sector 128, contiguous files, fixed dates (same folder = same
  image). Skips `radv-shader-cache`, `capturas` and hidden files. `--verificar` re-reads and compares; also checked with
  `fsck.exfat -n`. Takes ~7-10 min. Inside the container, prefer `flock /compartido/.compilar.lock`.
- `subir_imagen.py`: uploads to `<name>.exfat.part` and renames when done (ShadowMount does not pick up a `.part`);
  resumable with REST; if an image already exists, renames it to `.anterior` (deletes nothing). Close the game before
  uploading.
- First time (folder -> image): take the old folder out of the paths ShadowMount scans (one title = one source; the
  duplicate is ignored), upload the image, wait ~30 s for the scan and open the game. Detail in
  `remake-ps5/docs/imagen_titulo.md`.
- The image is **read-only**: everything the game writes goes to `/data/yugioh_classic/` (decks, progress, options,
  language, captures, RADV shader cache, duel log).
- Old (slow) method: `subir_ps5.py <folder>` / `ciclo_ps5.sh` by folder. They never touch another title.

No IP in the repo: the FTP tools read the console address from `remake-ps5/consola_ip.txt` (git-ignored). Uploads to the
console are always the owner's decision.

### 5.5 Windows installer (Inno Setup 6.7)

```
powershell -NoProfile -ExecutionPolicy Bypass -File remake-ps5\pc\instalador\armar_instalador.ps1
# parameters: -Origen <assembled game folder> -Salida -Version -Compresion lzma2/max -Solido -ISCC -Icono
```

Packs the `.exe`, `SDL3.dll`, `LEEME.txt`, `opciones_pc.txt`, `licencias\` and `datos\` (assets and shaders). Output is
`YugiohClassic_Instalador.exe` + `YugiohClassic_Instalador-1.bin ... -3.bin` (2 GB slices, **must stay together**). No
administrator needed, Spanish and English, Start menu and optional desktop icon, silent mode with `/VERYSILENT
/SUPPRESSMSGBOXES /DIR=... /TASKS=escritorio`. The uninstaller keeps `%APPDATA%\YugiohClassic` (saves). **The output
contains Konami artwork and Master Duel audio: it never goes in the repo** (`.gitignore` excludes it). Limits: see
section 8.

### 5.6 Room server (optional)

`remake-ps5/servidor/sala` (Cloudflare Workers, free plan; plain HTTP on purpose so the PS5 needs no TLS); deploy with
`wrangler` from the container (Node at `/compartido/node`). Tests: `bash servidor/sala/probar.sh`.

---

## 6. "How do I do X" guides

> Golden rule: after any change, build (5.2) and run the test cycle (section 7). If you touch `ia.h`, bump
> `VERSION_JUEGO` (online mode requires the same engine on both machines).

### 6.1 Add or fix a card and its effect

1. **Card data.** If it is among the 1690 Goat cards it already exists. If not: add it to `datos/cartas_extra.txt`
   (`TCG name|limit|note`; no owner) or to `personajes_datos.py` (if it is a character's signature card) and run
   `python -I preparar_classic.py --solo <id>` (downloads English data from YGOPRODeck by name, Spanish from `cards.cdb`,
   uses the Gemini artwork or a provisional one and recomposes the ES/EN/HD face).
2. **Engine effect.** In `src/ia.h`, sections "motor de efectos: datos" / "motor de efectos" / "las cartas": register a
   `DefCarta` (`objetivo`, `costeLP`, `descartes`, `permanece`, `ignicion`/`disparo`, `condicion()`, `objetivos()`,
   `resolver()`, `valor()` for the AI, `alSalir()`, `modAtk()`/`modDef()`/`impideAtaque()`, `espera()`/`finTurno()`).
   Spell speed comes from the subtype in `cartas_classic.txt`. If the effect needs a choice, use the engine question
   (`P_ELEGIR`) and continue in `Eslabon::paso`.
3. **Animation (optional).** `datos/efectos_cartas.txt` (`id|effect`); without an entry the generic one plays.
4. **Tests.** Add a case in `src/pruebas/prueba_efectos.cpp` (a scenario with the exact condition **and** a situation where
   it must do nothing). Run the **certification** (`bash src/pruebas/certificar.sh [only_id]`): it regenerates
   `docs/certificacion_efectos.md/.csv`; the goal is `SIN PRUEBA 0` (untested 0). If the effect is a known shortcut,
   record it in `src/pruebas/certificacion_notas.md` and in `docs/efectos.md` ("Atajos").
5. When fixing a card, write the failing test first and then the fix (as done with Centrifugal Field, Fairy Guardian,
   etc.: `docs/control_final.md`).
6. Run `efectos_pendientes.py` to refresh the pending list in `docs/efectos.md`.

### 6.2 Add a 3D figure

Art pipeline (each step is done by a tool; the owner reviews on the PC before uploading):

1. **Original artwork** of the card -> **Gemini** (`herramientas/puente-gemini`, queue with a 20 s pause and retries):
   **one single photo of the figure**, faithful to the card. Check each image against its source (Gemini invents things,
   e.g. Exodia in pieces).
2. **Image -> 3D**: TRELLIS on Colab (`herramientas/colab_trellis`, `trellis.py`, `instalar_trellis.py`) or **Meshy**
   (`herramientas/meshy.py`; mind the credits; skeleton figures are `_anim.glb` with Idle/Parry/dying clips used as
   idle/defense/death).
3. **Cleanup in Blender** (`herramientas/blender/`: `quitar_base.py`, `quitar_anillo.py`, `recortar_altura.py`,
   `girar_modelo.py`, `reparar_textura.py`...), `quitar_tapa_gris.py`, `material_mate.py` (TRELLIS leaves it metallic),
   `arma_a_mano.py` (weapon 100 % bound to the hand bone) and, to animate, `herramientas/esqueletos/` (`rig_auto.py`,
   `retarget_quaternius.py`, `animar.py`, `exportar.py`).
4. **Normalize**: models 1 tall, feet at 0, front +Z (glTF). Copy to `classic-assets/models/yugi/<slug>.glb` (and
   `<slug>_anim.glb`).
5. **Register**: run `python -I emblematicas_gen.py` (the card moves to `datos/figuras.txt` by itself and leaves
   `docs/figuras_pendientes.md`) and add a row to the `datosFigura()` table in `src/figuras.h`:
   `slug, scale, CuerpoFig (Bipedo/Cuadrupedo/Alado/Compacto), AtaqueFig (Embestida/Espada/Magia/Aliento/Rayo), attack
   origin {x,y,z} (mouth or staff tip, in model coordinates), halo and core colours, wings (hinge, height, down, slope,
   cut)`. Scale: logical height (`remake/diseno/tamanos.json`) x 2.6/1.8 (skeleton figures are 1.8 m tall).
6. **Automatic rules for every new figure** (agreed with the owner: no figure-by-figure review): **orientation** (they face
   the screen at rest, in defense, on appearance and on key-card entrance; only when attacking do they turn to the target
   and back: `orientacion_figura.h`, measured by `CU-FT-FIGURA-ORIENTACION*`), **wings** (flapping at rest, folded in
   defense), **weapons** (the sword goes to the skeleton's hand: `empunarFigura`), **breath and beam** from the mouth or
   tip (by table), **death** (death clip with skeleton; falls backwards, to the floor or flattened without one) and
   **feet** (floor = `SUELO_FIGURA`). The tools `herramientas/atributos.py`, `escaneo_glb.py` and `orientar_auto.py` scan a
   `.glb` for wings, mouth and orientation (born in the sibling project; **not verified** that their output still feeds
   the `figuras.h` table directly, which is hand-written today).
7. **Visual check.** `captura_figuras.sh` on the PC preview with `YUGI_FIGURAS_MESA`/`YUGI_FIGURAS_ATAQUE` (see the header
   of `figuras.h`). The **Cartesian test** (sweep of angles and poses) belonged to the sibling project
   (`prueba_cartesiana.sh`); **it does not exist in this repo**: the equivalent check here is the `captura_figuras.sh`
   captures and the `CU-FT-FIGURA-*` cases.
8. Tests: `CU-FT-FIGURA-*` (appears only face-up, disappears on death, respects the "3D figures" option).

### 6.3 Add a character to the story

1. `personajes_datos.py`: `id`, ES/EN name, saga (`reino ciudad noah dragones campeonato faraon`), level 1-8, personality
   (aggressive, defensive, patient, tricky, calculating, balanced...), environment, ES/EN motto, photo and **deck variants**
   (0 is "their own"; TCG English names, `"Name*2"` = 2 copies).
2. `python -I personajes_gen.py --firma` writes `datos/cartas_firma.txt` (cards not among the 1690: **the signature card is
   unlocked by beating its owner**) and then `python -I personajes_gen.py` writes `datos/personajes.txt` and
   `datos/personajes_rivales.txt`. Then `preparar_classic.py` for the new signature-card faces.
3. `datos/dialogos.txt`: `intro` (2-4), `pierde` and `gana` lines with the character id, ES and EN.
4. **Background/environment**: a row in `fondos_datos.py` (scene in English **without franchise names**, light, theme) ->
   `python -I fondos_datos.py` -> `cola_fondos.py` -> `fondos_post.py` (6.4). Without its own background, its theme's is used.
5. **Portrait**: `cola_personajes.py` (Gemini, chroma background) + `retratos_post.py` ->
   `classic-assets/personajes/retrato_<id>.png`.
6. If the signature card is an important monster, make its 3D figure (6.2).
7. Tests: `CU-FE-RIVAL-*`, `CU-FE-DIALOGO-*`, `prueba_ia`/`prueba_variedad` (legal decks). Character decks obey the Goat
   banlist except signature cards.

### 6.4 Add a background, field card or environment

- **Per-character or per-theme background**: `fondos_datos.py` defines `id`, theme, ES/EN place and the scene for Gemini
  (cinematic 21:9 photo, subject centred in the 15-75 % band of the height, plain bottom quarter, no people or text). Flow:
  `python -I fondos_datos.py` (writes `datos/fondos.txt`) -> `python -I cola_fondos.py [--solo a,b]` -> `fondos_post.py <id>`
  (crop, Real-ESRGAN x4, depth). Gemini rejects prompts with franchise names ("third-party rights"): describe the place.
- **Environment (light, particles, silhouettes)**: keys in `src/entornos.h` (`entornos::claveDe`, `porTema`); an unknown
  key gives `neutro`. Contract in `docs/entornos.md`. CC0 textures: `entornos_insumos.py`.
- **Field card**: a `campo_<card>` row in `datos/fondos.txt`, textures with `campos_insumos.py` and the card's entry in
  `src/campos.h` (wave colour, particles, theme). `docs/campos.md` lists the 25.

### 6.5 Add a tournament deck

Edit the list in `mazos_torneo.py` (TCG names; `*N` copies), **only with cards from the pool** (1690 Goat + extras, no
signature cards), respecting the game's Goat banlist; run `python -I mazos_torneo.py` (validates: 40 cards, max. 3 copies
and limits, fusions with their materials and Polymerization/Metamorphosis, rituals with their spell, Toons with Toon World)
which writes `datos/mazos_torneo.txt`. The `A|opponent|keys` line decides which opponents get it. If a card is missing:
`cartas_extra.txt` + effect + face (that is how the 10 tournament cards were added). Test: `prueba_torneo` (legal decks, no
illegal moves, win rate). Detail: `docs/mazos_torneo.md`. Careful: combos with the `torneo` field in `combos.txt` do not
build other opponents' dynamic decks.

### 6.6 Add sounds or music

Put the `.wav` (PCM 16 bit, 48 kHz) or `.ogg` in `classic-assets/audio/` and add the line to `audio.txt`
(`key|file|volume|loop`; music with loop points `1:<start>:<end>`). The game asks for the **key** (`sfx_invocar`,
`musica_duelo`...); changing what plays is editing that line. To extract sounds from your own source:
`herramientas/audio/extraer.py` + `convertir.py` + `seleccion.json`. Test: `prueba_audio`.

### 6.7 Change the pacing

Constants are named: `control::DURACION_ROBO`, `DURACION_ROBO_RIVAL`, `DURACION_ANIM`, `ritmo::T_ATAQUE_BAJO/ALTO`,
`T_ROMPE`, `T_POST_GOLPE`, `ritmoturno::LECTURA_MIN`, `ROBO_TURNO_RIVAL`, the opponent's wait when choosing
(`cpu_muestra.h`), `Interfaz::ELEV_ALTO`. Curves come from `md_ritmo/movimientos.txt` (do not edit by hand: re-export with
`exportar_movimientos.py`). When changing a time: update the table in `docs/md_ritmo.md` and the test limits
(`CU-FE-ATAQUE-DURACION-JOEY` fails if the attack in Normal does not last 2.8-3.3 s; the human-player caps in
`cu_humano.h`). The speed option (x1.75, x2.75) scales everything without touching anything else.

### 6.8 Change difficulty and the AI

> Full AI document (layers, levels, plans, decks, combat, network, tests and levers): [`docs/AI.md`](docs/AI.md).

- **Levels 1-8** of the opponents: `rivales_gen.py` (ATK ceiling per level, tributes, themes) and `Cerebro::ruido()` (520
  noise points at level 1, 86 at level 8). Table in `docs/rivales.md`.
- **Personality**: aggressive, defensive, bluffer, patient, reckless, calculating, balanced (`Cerebro` weights).
- **Plans** (`ia_plan.h`): aggressive, control, lock, swarm, combo, punisher, mill, tempo, alternate; card roles in
  `roles_cartas.txt`; combos in `combos.txt`.
- **Adaptive difficulty** (Options): the player's win streak (`Memoria::racha`, from -1 to +4) raises plan strength and
  lowers noise (`factorRuido` 0.45-1.1) **without touching the opponent's level or card pool**.
- **Classic / Human AI** (Options): the human one adds Monte Carlo search, bluffs, reading the player and believable
  mistakes; budget ~150 ms per decision (measured: 14 ms average, 101 ms max).
- **Learning AI**: the **value network** `ia_red.h` mixes its logit with the hand-written evaluation
  (`value = (1-mix)*evaluarHumana + mix*scale*logit(net)`, `mezcla` 0.5). It is **trained by self-play** with
  `src/pruebas/entrenar_ia_red.cpp`:

  ```bash
  g++ -std=c++17 -O3 -march=native -I remake-ps5/src remake-ps5/src/pruebas/entrenar_ia_red.cpp -o /tmp/entrenar_ia_red
  HILOS=10 DUELOS_CLASICA=20000 DUELOS_HUMANA=1500 PARES=150 \
    /tmp/entrenar_ia_red /compartido/classic-assets/ui/ datos/ datos/ia_red.txt 3 > entrenar.log 2>&1
  ```
  Processes with `fork` (the engine has static flags), Adam + cross-entropy + early stopping, buffer of up to 900k
  positions; a new generation is kept only if it beats the champion >= 55 % in duel pairs (generation 0, if it is not
  weaker than the human AI without a network). To measure without training: `PARTIDO_SOLO=<net.txt>
  CONTRA=ninguna|clasica|<net.txt> PARES=100`. Changing the features or the network shape **invalidates**
  `datos/ia_red.txt` (retrain). On the console, between duels, only the **last layer** (33 weights) is fine-tuned with the
  player's recorded duels (`/data/yugioh_classic/ia_duelos.bin` -> `ia_red_jugador.txt`); "Forget what was learned"
  deletes it. Detail: `docs/ia_red.md`.
- Since the AI runs inside the engine, **changing it changes online-mode simulations**: bump `VERSION_JUEGO`.

### 6.9 Add a visual effect translated from Master Duel

1. Locate the effect in the exported MD data (`LocalData` bundle index; `herramientas/fx_md/indice.py`, `volcar_fx.py`,
   `volcar_ef.py`).
2. Particle effects: bake with `herramientas/fx_md/sim_fx.py` / `hornear_md.sh` into `fx_master_duel/horneado` (flipbooks).
3. Per-card `CardRunEffect`s (3D tracks of their `ParticleSystem`s): `herramientas/md_ritmo/exportar_pistas.py` writes
   `md_ritmo/efectos/<ef>.mdp` + `.png`; the game plays them in `md_cardrun_ui.h` (MD duel camera in `VISTA_MD`).
4. Effect sprites: `exportar_efectos.py` -> `md_efectos.png` (`md_efectos_ui.h`).
5. Register the card in `datos/efectos_cartas.txt` (or the new effect in `efectos_cartas.h`) and the sound in `audio.txt`
   (`herramientas/audio/seleccion.json`).
6. Test: `prueba_sprites_md.py` (the MD sprites the UI asks for are in the title and none is a mask) and `md_ritmo_pc.h`
   scenes with `captura_ritmo.sh`. Detail: `docs/md_efectos.md`.

Konami's data is **never** versioned; only the scripts and the translated values.

### 6.10 Translate texts

`src/idioma.h`: `tr("Spanish text")` looks up the table `{ "español", "English" }`; the UI applies it automatically in
`texto()`/`medir()`/`parrafo()`. Variable parts (numbers, card names) are joined **after** translating each piece. Card
data goes through `CartaUI::vNombre()/vAtributo()/vTipo()/vDesc()`; the `_en` files come from `preparar_classic.py`
(`cartas_ui_en.txt`, `cara_en_*`). The language is saved in `idioma.txt` (PC: `YUGI_IDIOMA=en|es`). No visible text may be
left out. Adding another language would require generalizing the ES/EN pair in `idioma.h` (**not done**).

### 6.11 Port to another platform

The platform is isolated in `platform.h` (`ps5/src`): `klog`, gamepad (`pad_player` with `PAD_*` bits), sound
(`audio_start`: 48 kHz, stereo, 16 bits, 256-frame blocks) and time. The game is pure Vulkan 1.3 on top of Sascha
Willems' base, which already supports Windows and Linux. Steps (this is how Windows was done): (1) implement `platform.h`
on the new platform (`pc/plataforma_windows.c`), (2) a dedicated entry point (`pc/main_windows.cpp`), (3) a `CMakeLists`
that builds the same sources, (4) the save folder in **one single function** (`guardado::carpetaDatos()`), and (5) base
patches behind `#if` in `parche_base.py`. Rule: **do not change the engine, the duel or the AI** (lockstep).

---

## 7. Tests

### 7.1 Principles

- Everything is tested **without a screen or console** (engine, real UI without GPU, simulated human player, loopback
  network).
- **0 errors** means: `ciclo_completo.sh` exits with code 0, **0 FALLA** (failures) in the summary, and every leaf of
  `docs/casos_de_uso.md` has at least one test (a leaf without a test counts as a failure). Failures that need an owner
  decision count as **DECISION** (`cu::decision`), not as failures (`docs/decisiones_pendientes.md`).
- A cycle runs **completely from scratch** (every test is rebuilt, nothing differential) inside `arch-ps5`.
- **Parallel worktrees**: each work branch lives in `/compartido/wt/<branch>/` (its own git *worktree*); each agent or
  person edits only in their own; building and testing take the lock (`/compartido/.compilar.lock`); nothing is uploaded
  to the console and no `push` is made unless asked; each finished branch is merged into the integration branch.
- Validation on the PS5 is done by the owner.

### 7.2 How to run

```bash
bash /compartido/wt/<branch>/remake-ps5/src/pruebas/ciclo_completo.sh [output folder]
# leaves one .log per test, casos_uso.tsv, resumen.txt and resultados.tsv; returns 0 only with 0 failures
bash .../src/pruebas/certificar.sh [only_id]        # effect certification
bash .../src/pruebas/humano.sh <output> <mode> ...  # human player (modes: fijos, duelos, general, cartas, todo, volcar, ritmo)
bash .../src/pruebas/humano_todo.sh                 # the whole human player in parallel batches
bash .../src/pruebas/front_total.sh                 # front-end battery
```

`ciclo_completo.sh`: (1) builds the game (`probar_rama.sh`); (2) `prueba_sprites_md.py`; (3) builds and runs each test;
(4) certification; (5) use cases, full front-end, online from the front, human player and pixels; (6) summary.

### 7.3 Suites (`src/pruebas/`)

| Suite | What it tests |
|---|---|
| `prueba_mando.cpp` | gamepad control (`control.h`) |
| `prueba_constructor.cpp` | deck builder |
| `prueba_juego.cpp`, `prueba_duelo.cpp` | the real game path and full duels with the gamepad |
| `prueba_efectos.cpp` | ~881 hand-made effect cases |
| `prueba_certificacion.cpp` | each card with an effect: template + case + front-end presentation (1395 OK) |
| `prueba_ia.cpp`, `prueba_variedad.cpp`, `prueba_ia_humana.cpp`, `prueba_ia_red.cpp` | opponent, deck/plan variety, human vs classic AI, network (`CU-BE-IA-RED-*`) |
| `prueba_torneo.cpp` | 21 legal decks and win rates |
| `prueba_audio.cpp` | mixer and audio files |
| `prueba_red.cpp`, `prueba_red_nat.cpp`, `prueba_sala.cpp`, `prueba_red_front.cpp` | network, NAT, room, ONLINE DUEL from the front (loopback) |
| `prueba_casos_uso.cpp` (+ `cu_front.h`, `cu_back.h`, `cu_cartas.h`, `cu_juego.h`) | the use-case tree of `docs/casos_de_uso.md` (CU-FE front, CU-BE back, CU-CARTA card by card, CU-RED) |
| `prueba_front_total.cpp` (+ `cu_front_total.h`, `cu_ritmo.h`, `cu_ataque_directo.h`) | **front-end** battery: full duels with the gamepad against each opponent and card by card; table pixels on the preview |
| `prueba_humano.cpp` (+ `cu_humano.h`, `pantalla_sin_gpu.h`) | **human player**: decides only from what the screen draws, with human pacing and mischief; `CU-HU-*` oracles (notices what happens, never gets stuck, coherent table, pacing) |
| `prueba_duelo_log.cpp`, `prueba_leer_log.py` | duel log |
| `entrenar_ia_red.cpp` | training (not a test) |

Documented results (`docs/ciclos_pruebas.md`, `control_final.md`, `md_ritmo.md`): cycles of **7,008 to 7,088 cases**
with 0 failures; full front-end 4,724 OK, human player 1,824 OK, use cases 7,025 OK, online from the front 58 OK;
certification 1,395/1,395. These are the numbers **recorded in the docs**: **they were not run again in this pass** (the
work was documentation).

---

## 8. PC version (Windows)

- **Windows layer** (`remake-ps5/pc/`): `plataforma_windows.c` implements `platform.h` (gamepad, keyboard, mouse, audio),
  `main_windows.cpp` (startup, window, Alt+Enter/F11, mouse messages), `raton_plataforma.h`, `compat_windows.h`,
  `CMakeLists.txt` and `toolchain_mingw.cmake` (cross-compilation with `mingw-w64`), `yugioh.rc`/`yugioh.manifest` (icon
  and manifest), `SDL3.sha256`.
- **Gamepad**: XInput (loading `xinput1_4.dll`/`xinput9_1_0.dll` at run time) and **SDL3** (DualSense, DualShock 4, Switch)
  loaded with `LoadLibrary` if `SDL3.dll` sits next to the `.exe`. Mapped to the console's same `PAD_*` bits.
- **Audio**: the same mixer (`audio.h`) over **waveOut** (a thread fills 256-frame blocks at 48 kHz, 6 buffers).
- **Saving**: `%APPDATA%\YugiohClassic\` (decks, progress, options, language, log `yugioh_classic.log`); with
  `portable.txt` next to the `.exe`, in `guardado\`. Saving is atomic (`MoveFileEx`).
- **Data**: `datos\assets\` and `datos\shaders\` next to the `.exe`.
- **Video**: 1920x1080 window (or 90 % of the desktop), `--completa` (fullscreen), `--ventana`, `--ancho N --alto N`,
  `--sin-vsync`, `--consola`; the same options, one per line, in `opciones_pc.txt`. V-sync on by default (monitor
  frequency). Alt+Enter / F11: live fullscreen toggle. Requires a Vulkan 1.3 GPU.
- **Rebuild**: `armar_pc.sh` (5.3). Preview of the same code on Linux: `ninja -C build/ps5-host ps5-samples-host`.
- **No online play on PC** (only under `_WIN32`): the button reads "DUELO EN LÍNEA: NO DISPONIBLE EN PC".

### Controls

| Action | Gamepad (DualSense / Xbox) | Keyboard | Mouse |
|---|---|---|---|
| Move focus | d-pad / left stick | arrows / WASD | move the pointer over a zone |
| Confirm / open action menu | X / A | Enter, Space | left click |
| Cancel / back | circle / B | Esc, Backspace | right click |
| Big detail | triangle / Y | T | middle click |
| Phases (Master Duel selector) | square or short OPTIONS (on your turn) | F or O/Tab | PHASES plate |
| End turn (from battle phase) | circle | Esc | - |
| Camera | L1/R1 (mode), right stick (orbit), L2/R2 (zoom), R3 (recentre) | Q/E, IJKL, 1/3, Z/C | wheel = L1/R1 on the table |
| Duel menu (gear) | touchpad / Create / Back | G | click the gear |
| Options / pause | OPTIONS | Tab, O | - |
| Fullscreen | - | Alt+Enter, F11 | - |

Help icons change according to the last device used (PlayStation, keyboard and mouse, Xbox).

### Installer and its limits

Inno Setup 6.7 wizard (see 5.5). Limits: ~4 GB split into `.exe` + `.bin` files that must stay together; it does not include
`en_linea.txt`; **it contains Konami material**, hence it never goes in the repo; the installer and the `.exe` are **not
signed** (SmartScreen may warn; not verified on a clean machine). Unverified (documented in `docs/pc_jugable.md`): a real
gamepad (XInput/DualSense) and the automatic icon switching, the pointer hiding after 3 s, clicks on some screens
(Customization, Gallery, on-screen keyboard, tutorial, result), fullscreen at 4K/120 Hz and other DPI settings.

---

## 9. Duel log and diagnostic tools

- **Duel log** (`duelo_log.txt`, `src/duelo_log.h`): one line per event and per AI decision, to judge whether the opponent
  plays logically. Options > "Duel log" (Yes by default; `bitacora_duelo=0/1`). It **appends** at the start of each duel;
  past ~2 MB it renames to `duelo_log.anterior.txt`. It does not change the game (optional hooks in `ia.h`, spends no
  randomness; `prueba_duelo_log.cpp` checks the duel is identical with and without the log). On PS5:
  `/data/yugioh_classic/duelo_log.txt`; on PC: the save folder.
- **Format**: `mm:ss [T<turn> <phase> J|CPU] text` (J = player, side 0; CPU = opponent, side 1; phases `Robo, Principal1,
  Batalla, Principal2, Final`; zones `z1..z5`). Lines: `===== Turno N ... =====`, `roba` (draws), `Invocacion Normal`,
  `coloca` (sets), `ACTIVA X [eslabon n] | target | cost | chained after`, `resuelve`, `ATAQUE: A -> B | expected result`,
  `combate real`, destructions and their cause, and the AI's reasoning.
- **Reading from the PC**: `python -I remake-ps5/leer_log_duelo.py una [-n 80]` (download and show) or `... seguir` (like
  `tail -f`). **Read-only** (SIZE and RETR over FTP).
- **Console photos and video**: `fotos_ps5.py [PPSA77002]` downloads captures from `/data/yugioh_classic/capturas/`,
  builds mosaics and per-attack sheets and **deletes them from the console**; `esperar_video_ps5.py`. To request photos:
  upload `capturas.txt` to `/data/yugioh_classic/` (format in `yugireload.cpp`).
- **Captures on the PC preview**: `captura_figuras.sh`, `captura_ritmo.sh`, `captura_pantallas.sh`, `captura_bosque.sh`,
  `captura_fx.sh`, `captura_constructor.sh`, `captura_entornos.sh`, `captura_front_total.sh`, `pixeles_ft.py`.
- **Preview (PC) environment variables**: `YUGI_GUION` (button script), `YUGI_FIGURAS`, `YUGI_FIGURAS_MESA`,
  `YUGI_FIGURAS_ATAQUE`, `YUGI_FIGURAS_MEDIR`, `YUGI_PRUEBA_RITMO`, `YUGI_IDIOMA`, `YUGI_DISPOSITIVO=0|1|2`,
  `YUGI_ANCHO/ALTO`. On Windows: `YUGI_GUION_PC="3:X 7:X 11:C ..."` presses buttons inside the game.
- **`yugictl`**: temporary resident development agent on the console, **for `PPSA77002` only**, never closes or kills
  anything (the game closes by itself with `cerrar.txt`).

---

## 10. Design decisions, lessons and open items

### 10.1 Design decisions (why)

- **Pure engine + separate presentation**: lets everything be tested headless (thousands of duels), train the network,
  play online by lockstep and change presentation without touching rules. The engine lives in one big file (`ia.h`) on
  purpose: a single translation unit, no dependencies.
- **Goat format** (April 2005, 1690 cards): a well-defined era, no Synchro/Xyz/Pendulum/Link; real tournament decks of
  the time. Official rules were chosen in the two open decisions (attack replay with target choice; a window right after
  damage `V_DANNO` for Inferno Tempest and similar).
- **Our look, Master Duel's timings**: curves, times and order are copied from exported data (not the C# code, which
  depends on Unity). The opponent "shows the card" like Joey.
- **Unpredictable AI**: the opponent changes strategy and deck between duels (3 variants per opponent, plans, noise); fun
  beats predictability. In one test, 800 duels gave 363 distinct results.
- **Photorealistic backgrounds** (Gemini matte paintings + depth for parallax) instead of polygons: backgrounds built with
  code geometry were rejected ("a real castle, like in a movie"); a different background per character.
- **A single exFAT file** for the title: FTP by folder hung every 250-300 files.
- **Atomic saving** and everything written to `/data/yugioh_classic/` (the image is read-only).
- **Build lock** and per-branch *worktrees*: the template is shared; prevents crossed builds.
- **Online lockstep**: only decisions + state hash travel; cryptographic shuffle commitment; 3-minute turn clock per
  player; a disconnection longer than 30 s ends the duel without counting.
- **No online play on PC**: the owner's decision for the Windows version.

### 10.2 Lessons and known traps

- **ShadowMount+**: one title = one source (if a folder and an image share the ID, the existing one wins); it needs
  64 KiB clusters and an image on the internal disk for the fast path; it ignores `.part` files; the image is read-only.
  `ftpsrv` answers `226` to `DELE` (use `sendcmd`) and hangs every 250-300 files. The RADV shader cache must live in
  `/data/yugioh_classic/`.
- **When uploading**: close the game first; never touch other titles (the tools refuse another ID).
- **Repeated draw**: the engine draws in the same step in which the previous turn ends; any table sync before presenting
  `S_ROBO` put the card in hand too early (fix: `ritmoTurnoOcultarRobos`). Lesson: every table sync goes through
  `sincronizarMesaEnOrden`.
- **Clock-seeded tests** can fail by chance: fix the seed or relax the condition with judgement.
- **Gemini invents** (Exodia in pieces, names): compare each image to its source; it rejects prompts containing franchise
  names ("third-party rights"); do not run two queues in the same Chrome.
- **Branch merges**: two branches that touch `sincronizar.sh` may lose lines on merge (it happened with the copy of the MD
  headers, commit `69f77b0`): review the merge.
- **Mouse on Windows**: synthetic input with `PostMessage` does not fully work (Windows sends `WM_MOUSELEAVE`); use
  `SetCursorPos`/`mouse_event` with the window on top, and not while the PC is in use.
- **Hard links**: `armar_pc.sh` hard-links the title's assets (NTFS through the Docker mount); if the title changes midway
  (rsync running), the next assembly fixes it (rsync exit code 24 tolerated).

### 10.3 Known limits and open items

- **Unverified on a real console** (the owner validates): what `docs/decisiones_pendientes.md` marks "to validate on the
  PS5" (e.g. the "could not move to M2" stall; not reproduced on the PC).
- **Hide the opponent's hand and deck in the online duel (commitment version 2)**: mandatory **before opening the room to
  the public**; today the committed order travels at the start (lockstep with everything known): it guarantees fairness
  but a modified machine could look at the other's hand. Among friends (LAN, code) it can be played now. A real PS5 <-> PC
  duel is still to be validated (tests play two complete games over loopback).
- **Magic Cylinder** (effect translated from MD): the pieces follow MD's duel camera, not ours (if the camera is changed
  with L1/R1 they do not follow); the full 3D track is pending (`docs/md_efectos.md`).
- **Thin Summoned Skull beam**: the lightning beam of "Rayo"-type figures (Summoned Skull, Jinzo, Thunder Dragon) was
  considered too thin next to the breath attacks; the latest commits on the attack branch thickened it (sparks and running
  orbs: `1f7d603`, `c273f34`). **Not verified**: there is no written record in `docs/` that it is still pending; it is
  listed on the owner's indication.
- Other MD effect approximations: Mirror Force without `flare04` or `Sonic_01/02`; Swords is a standing sprite, not the
  stuck-in mesh; the `DuelTurnCount` counter does not appear under the shown card; the yellow glow shows on every face-up
  active spell/trap (in MD only on those that can be activated); `CardIndividualData.mrkPowerTable` and the attacker-lift
  tweens are estimated.
- Card-effect approximations: Fairy Guardian, Pitch-Black Power Stone and Level Conversion Lab (see
  `docs/control_final.md` and `src/pruebas/certificacion_notas.md`).
- **Duelist name** online: today "Duelista XXXX"; it cannot be typed. The UPnP mapping is not released if the game is
  closed any way other than "Exit".
- The installer is not signed; not verified on a clean machine.
- Names inherited from another project in the code (`yugireload.cpp`, the `yugireload` shader folder) because the template
  requires them: not a defect.

---


