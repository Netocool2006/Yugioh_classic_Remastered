# El ritmo de Master Duel en la mesa (rama classic-md-ritmo)

Pedido del usuario (probó en la PS5): «las cartas del contrincante solo aparecen, no hay un movimiento de su mazo hacia
colocar la carta y debe ser un ritmo humano»; «se activó Swords of Revealing Light y ni me enteré»; «que las cartas que
están activas brillen fuerte»; «el contador de los 3 turnos no existe»; «encuentra el código del de Steam y tráelo».

Regla: **se queda nuestro look and feel; los detalles (tiempos, curvas, orden, sonidos) vienen de Master Duel**. Los
datos de MD (assets de Konami) NUNCA van al repo: se extraen a `D:\Docker\compartido\md_extraido\` (crudos, `NOTAS.md`
con el método) y lo que el juego lee va a `<assets>/md_ritmo/` (classic-assets, fuera del repo; `sincronizar.sh` lo copia).

## Cómo se sacó de Master Duel

- `GameAssembly.dll` + `global-metadata.dat` (sin cifrar) → Il2CppDumper (`D:\Ghidra-Reload\herramientas\il2cppdumper`):
  `dump.cs` (clases, campos, constantes) y `DummyDll`.
- Los ScriptableObjects y Timelines del duelo están en `masterduel_Data/data.unity3d` (`resources.assets`), sin typetree:
  se leen con UnityPy + `TypeTreeGeneratorAPI` cargando el `DummyDll` (scripts en `md_extraido/ritmo/scripts/`).
- Efectos y sprites: `LocalData/` (índice de bundles en `D:\tools\fx_md\work\indice.json`), con los volcadores de
  `herramientas/fx_md/` y `herramientas/md_fases/`.
- Los movimientos de cartas: `herramientas/md_ritmo/exportar_movimientos.py` → `<assets>/md_ritmo/movimientos.txt` (90
  `BezierMotionSetting`: tiempo, puntos inicio/vía/fin y sus dos AnimationCurve). El juego los evalúa **tal cual**:
  `clipsmd::CurvaUnity` (de `clips_md.h`, rama classic-md-fases: Hermite / Bézier con pesos como Unity).
- Sonidos: los del catálogo de MD ya convertido (`classic-assets/audio_master_duel`), elegidos en
  `herramientas/audio/seleccion.json`.

## Cada elemento: de MD a nuestro C++

| Elemento nuestro | Master Duel (clase / prefab / clip) | Datos traídos | Estado |
|---|---|---|---|
| Robo del jugador (`control.h` DURACION_ROBO) | `CardMoveMotionSetting` Deck→Hand (Myself): `CardMoveDeck2HandMyself1` 0.58 + `…2` 0.22 | 0.80 s (antes 0.42) | hecho |
| Robo del rival (DURACION_ROBO_RIVAL) | Deck→Hand (rival): `CardMoveField2Hand` 0.45 | 0.45 s (antes 0.42) | hecho |
| Mano → zona (jugador, `dibujarVuelo` + `vueloMD`) | `CardMoveHand2MonstorZonePower0_1/_2`, `CardMoveHand2MagicZone1/2` | Bézier y curvas exactas; 0.50 s (antes 0.60) | hecho |
| Mano → zona del RIVAL (`ritmoLlegada`) | mismos movimientos; la CPU antes "elige" | curvas exactas; elige 0.38 s (estimado: MD ~0.5-1 s entre acciones) | hecho |
| Monstruos fuertes | `…Power1_*` 0.67 s, `…Power2_*` 0.88 s (suben a +16/+24) | poder por nivel: 5-6 → 2, 7+ → 3 (estimado: MD lo saca de `CardIndividualData.mrkPowerTable`) | hecho |
| Invocación especial desde cementerio / extra / mazo / desterradas | `CardMoveDeck2MonstorZone*`, `CardMoveDeck2MagicZone*` + destello de salida `DuelfFromGrave01TL` / `FromExclude01TL` (0.583) | curvas exactas; destello de la pila | hecho |
| Al cementerio / desterrada (`ritmoSalida`) | `CardMoveField2GraveNormal` 0.42 (vía +20: arco alto) | curvas exactas; varias salidas juntas (Raigeki) van a la vez | hecho |
| Volver a la mano / al mazo / al extra | `CardMoveField2Hand` 0.45, `CardMoveField2Deck` 0.45 | curvas exactas | hecho |
| Volteo en el campo (DURACION_ANIM) | `CardFlipPlateMonster/Magic` 0.45 (salta +10) | 0.45 s (antes 0.55) | hecho |
| Activación (`ritmoActivar`, `alzaRitmoMD`) | `CardShow`: `CardShowMoveFieldNear` 0.15 (+5) → `CardShowHappen` 0.70 (+5→+6, mira a la cámara) → `CardShowBackFieldNear` 0.25 | curvas exactas; la carta se levanta en su zona y se para mirando a la cámara; total 1.10 s | hecho |
| Luz de la activación | `fxp_chain_cardlight_001` a 0.65 s del apilado, 0.5 s (`DuelChainStack01TL`) | tiempos | hecho (halo propio) |
| Efecto al aplicarse | `fxp_bff_apply_001` (ya horneado: `efectos_cartas.h`, md_activar) | arranca al terminar `CardShowHappen` (0.85 s) | hecho |
| Número de eslabón | `DuelChainStack02TL` 2.083 s: `ChainNumEf` a 0.75 s durante 1.333 | entra a 0.75 s; +0.55 s de espera con 2+ eslabones | hecho |
| Resolución de eslabón | `DuelChainResolve01TL` 0.917 (sonido a 0.217, luz a 0.383) | espera 0.60 s con cadena de 2+; destello en la zona; `se_duelchain_resolve01` | hecho |
| Sonidos de cadena | `se_duelchain_stack01_2nd/3rd/4th`, `se_duelchain_resolve01`, `se_card_exclusion` | `sfx_cadena_2..4`, `sfx_cadena_resuelve`, `sfx_desterrar` | hecho |
| Brillo de magias/trampas activas (`dibujarActivasMD`) | `fxp_HL_Select_trpmgc_001` (shader `fxs_HL_Rollover_001`): amarillo (1,1,0), un marco cada 0.2 s que vive 0.6 s (alfa 0→1 al 20 %→0) | color y pulso exactos; en MD solo sale si se puede activar: aquí, por pedido del usuario, en toda magia/trampa boca arriba que sigue en el campo | hecho (marco dibujado; sprite `EfDuelHighlight00_01_01` pendiente del atlas compartido) |
| Contador de turnos (Swords 3→2→1, Nightmare's Steelcage, Pikeru's Second Sight, Wave-Motion Cannon) | `DuelTurnCount`: cronómetro `GUI_TurnCounter` + aguja `GUI_TurnNeedle` (vuelta de -360° en 0.4 s tras 0.2 s; el número late 1→1.2 en 0.3 s) | animación exacta | hecho (dibujado; sprites pendientes del atlas) |
| Contadores de carta (magia: Skilled Magicians, Breaker, Royal Magical Library, Tower of Babel, Pitch-Black Power Stone…) | `IconCommonCounter` en la esquina superior izquierda; `icon_counter_magic`; CountChange escala 1.5→1 en 0.2 s | posición y animación | hecho (dibujado; ícono pendiente del atlas) |
| Combate, daño, destrucción, ritmo del rival | ver "Ritmo global" | | hecho (ver tabla) |

Swords of Revealing Light en el motor (`ia.h`): `finTurno` suma un contador al terminar cada turno del rival de su
dueño y la manda al cementerio al llegar a 3 (se va en la fase final del 3er turno del rival). Lo comprueba
CU-FT-ANIM-SWORDS-TURNOS (el contador al empezar cada turno: 3, 2, 1; luego ya no está).

## Ritmo global (Normal = Master Duel)

Cada tiempo: valor de MD, de dónde salió, el nuestro antes y después (la velocidad Rápida / Muy rápida y R2 lo escalan).

| Tiempo | Master Duel | Fuente | Antes | Ahora |
|---|---|---|---|---|
| Robo propio | 0.58 + 0.22 s | CardMoveDeck2HandMyself1/2 | 0.42 | 0.80 (`control::DURACION_ROBO`) |
| Robo del rival | 0.45 s | CardMoveField2Hand | 0.42 | 0.45 (`DURACION_ROBO_RIVAL`) |
| Mano → zona | 0.38 + 0.12 s (fuertes 0.67 / 0.88) | CardMoveHand2MonstorZonePower*, Hand2MagicZone* | 0.60 | 0.50 (curvas de MD) |
| Pila → zona | 0.50 s + destello 0.583 | CardMoveDeck2*, DuelfFromGrave01TL | aparecía | 0.60 |
| Al cementerio / desterrada | 0.42 s | CardMoveField2GraveNormal | desaparecía | 0.42 |
| A la mano / mazo | 0.45 s | CardMoveField2Hand / Field2Deck | desaparecía | 0.45 |
| Volteo | 0.45 s | CardFlipPlateMonster/Magic | 0.55 | 0.45 (`DURACION_ANIM`) |
| Activación | 0.15 + 0.70 + 0.25 s | CardShowMoveFieldNear / Happen / BackFieldNear | destello 1.8 máx. | 1.10 + la animación de la carta |
| Eslabón 2+ | +0.67 s (2.083 contra 1.417) | DuelChainStack02TL | 0 | +0.55 |
| Resolución en cadena | 0.917 s (luz a 0.383) | DuelChainResolve01TL | 0.35 | 0.60 con 2+ eslabones |
| El rival elige de su mano | sin constante (la CPU no espera en C#; el ritmo lo dan las animaciones) | Engine.CpuThinkUpdate (desasm) | aparecía | 0.38 (estimado) |
| Lo que piensa el rival (tope) | 3.0 s | CpuThinkingIcon.MAXTHINKINGTIME | 5.0 | 3.0 (`ia_humana.h`) |
| Ataque: viaje hasta el impacto | 0.45 s Low; 0.65 s High (ATK ≥ 2500) | AttackMotionSetting + BezierMotion StrikeLight00 / 02 (curvas tal cual) | 0.55 | 0.45 / 0.65 (`ritmo::T_ATAQUE_BAJO/ALTO`, classic-md-combate) |
| Pausa tras el impacto | 0.25 s | EffectTaskBattleRun.attackExecPostTime | — | incluida |
| Vuelta del atacante | ~0.45 s (inferido) tras la pausa | CardRoot.ReqMoveFlipTurnEffect | — | de 0.70 a 1.15 (`G_VUELVE`) |
| Destrucción en combate | tras la pausa del golpe, sin esperar: estallido `fxp_cardbrk_btl_001` + `SE_CARDBREAK_02` y viaje `CardMoveField2GraveNormal` 0.42 | CardRoot.ReqBrokenEffect(waitEffect=false) (desasm) | se desvanecía en su zona (0.90) | se rompe a 0.70 y llega al cementerio a 1.12 (High: 0.90 → 1.32) |
| Número de daño | escala 2 en 0.02 s, 1.5 a los 0.56, sube 80 px tras 0.2, se va en 0.2 | prefab ChangeValue (LPCounterSub) | 1.05 s | ~0.8 s (`dibujarDanio`) |
| Fin del ataque | ≈1.15-1.35 s | derivado | 1.6 | 1.30 (`G_FIN`) |
| Bajada de LP, carteles, fases | 1.0 s lineal, ... | DuelLP.countTime | (rama classic-md-fases) | — |

## Pendiente

- La amplitud de la vía del ataque (MD: 25 atrás / 20 arriba, High 35 / 25, en unidades de su cámara) se dibuja con la
  de antes (0.7 / 1.1 de la mesa) en la proporción de MD: el significado exacto del offset `it1` no se confirmó.
- El "poder" del ataque se toma del ATK del duelo (`atkEf`); MD pasa el ATK que muestra la carta (igual salvo casos raros).
- La subida de la carta al declarar el ataque (~0.2 s) y la vuelta del atacante (~0.45 s) son estimadas: los tweens de
  `CardRoot.StartAttackReady` no aparecieron en los datos.
- El "poder" de cada carta (`CardIndividualData.mrkPowerTable`) se aproxima por nivel.
- La unidad de alto de MD en la mesa (`MD_ALTO_CARTA` = 8) es estimada a partir de su cámara.
- (Hecho en classic-md-entradas, `docs/md_entradas.md`: las invocaciones por fusión y ritual de MD, 4.37 s / 5.89 s; las
  cartas clave siguen con su entrada en el `StrongSummon`.)

## El orden de los sucesos

Todo pasa por `presentarSucesos` (`duelo_juego.h`) en el orden del motor, con `sincronizarMesaEnOrden`: lo que se va
en un suceso sin presentar sigue en su zona y, ahora, lo que LLEGA en un suceso sin presentar todavía no está (llega
viajando cuando se presenta); las cartas en vuelo no están en su zona (`ritmoOcultarEnVuelo`). `esperaEfecto` cubre
cada viaje y cada activación: el duelo (y el rival, y las pruebas) esperan a la mesa. Es solo presentación: el motor y
el modo en línea no cambian (el reloj del turno en línea corre solo con la mesa al día: `alDiaRed`).

La velocidad de animaciones (Normal = Master Duel; Rápida x1.75, Muy rápida x2.75) y R2 escalan todo: los viajes y
las activaciones avanzan con el dt del duelo.

## Combate (rama classic-md-combate)

Pedido: «en MD la carta destruida en combate viaja al cementerio con su animación; aquí todavía se desvanece en su zona».

| Elemento | Master Duel | Nuestro C++ |
|---|---|---|
| Nivel del ataque | `Util.GetAttackLevel(ATK)`: <1000 Small, ≥1000 Medium, ≥1700 Large, ≥2500 Largest; "High" solo si Largest | `ritmo::ataqueAlto` (ATK ≥ 2500), `ataqueAltoMD` (`animarAtaqueDuelo` / `atacar`) |
| Viaje del atacante | `StrikeLight00` (Low) 0.45 s, `StrikeLight02` (High) 0.65 s: Bezier inicio → vía (atrás y arriba) → fin (casi sobre el objetivo); `accelerationCurve` (0.71, 0.31) / (0.81, 0.46) y `rotationAccelerationCurve` | `cartaZona` (interfaz.h) evalúa las dos curvas de MD (`movimientos.txt`) en un Bezier de 3 puntos; el reloj del ataque es el del Low y con High avanza 0.45/0.65 más lento hasta el impacto (`ritmo::pasoRelojAtaque`): todo lo de después (daño, pausa, rotura, vuelta, fin) se corre 0.20 s |
| Pausa tras el golpe | `attackExecPostTime` 0.25 s | el atacante espera sobre el objetivo y vuelve de 0.70 a 1.15 |
| Rotura | `CardRoot.ReqBrokenEffect`: one-shot "CardBreak" = `fxp_cardbrk_btl_001` + `SE_CARDBREAK_02`, sin esperar | `ritmoDestruidaCombate` (md_ritmo_juego.h) a `ritmo::T_ROMPE` (0.70): estallido `Fx::MdDestruirBatalla` (horneado con `sim_fx.py`, 1.35 s) y `sfx_destruir_batalla` |
| Viaje al cementerio | `CardMoveField2GraveNormal` 0.42 s (vía +20: arco alto), igual que por efecto | el mismo `ritmo::Viaje` de las destruidas por efecto (dibujarViajesMD); la zona queda vacía y la carta entra a la pila |

Datos (fuera del repo): `md_extraido/ritmo/ataque_bezier.json` → `exportar_movimientos.py settings.json ataque_bezier.json
<assets>/md_ritmo/movimientos.txt` (ahora 141 movimientos: los 90 de cartas + los del ataque); `fxp_cardbrk_btl_001`
(`data.unity3d` pid 7130) volcado con `volcar_fx.py` a `D:	oolsx_md\work\d_fxp_cardbrk_btl_001` y horneado a
`classic-assets/fx_master_duel/horneado/md_destruir_batalla` (`hornear_md.sh`); `se_cardbreak_02` →
`classic-assets/audio/sfx_destruir_batalla.wav` (`seleccion.json`).

## Pruebas

`src/pruebas/cu_ritmo.h` (modo `ritmo` de `prueba_front_total`, también en `extra`/`todo`): CU-FT-ANIM-RIVAL-COLOCA,
-INVOCA, -MAGIA, -ROBO, CU-FT-ANIM-ACTIVA-SWORDS, -LUZ, -CAMPO, CU-FT-ANIM-CADENA, CU-FT-ANIM-ORDEN, CU-FT-ANIM-RAPIDA,
CU-FT-ANIM-SWORDS-CONTADOR, CU-FT-ANIM-SWORDS-TURNOS; CU-FT-ANIM-COMBATE-VIAJE, -BAJO, -ALTO, -AMBOS, -DATOS (classic-md-combate).

Capturas en secuencia (vista previa del PC): `captura_ritmo.sh <remake-ps5> <salida> [coloca invoca swords luz cadena]`
(`src/md_ritmo_pc.h`, YUGI_PRUEBA_RITMO; YUGI_RITMO_CADA = segundos entre fotos). Las del pedido están en
`D:\Docker\compartido\capturas_md_ritmo\` (`coloca_NN`, `invoca_NN`, `swords_NN`, `luz_NN`, `cadena_NN`, una cada 0.1 s). Las del combate (escenas `combate` y
`combate_alto`): `D:\Docker\compartido\capturas_md_combate\`.

## Ritmo del turno (rama classic-ps5-ritmo)

Pedido del usuario (PS5): «el timing o velocidad de juego no es como la del juego de Joey the Passion o la de Steam, es
demasiado rápida»; «cuando está a punto de terminar el turno del CPU me das la carta del mazo a mi mano y luego, cuando
ya me toca, me la vuelves a dar»; «replica al de Steam: es el mismo juego». Las pruebas (13890 casos) no lo veían.

### Cómo mide MD el ritmo (GameAssembly.dll, VA; datos fuera del repo)

MD no tiene esperas fijas entre jugadas de la CPU: el ritmo sale de su **cola de tareas** y de lo que cada tarea espera.

| MD (clase / método) | Qué hace | Tiempo | Nuestro C++ |
|---|---|---|---|
| `RunEffectWorker.ExecDuelStep` / `DequeueTask` | cola de `EffectTask`, una por vez; las instantáneas se encadenan en el mismo cuadro | — | `presentarSucesos` (duelo_juego.h) con `esperaEfecto` |
| `EngineBusyIdTable` + `RunEffectWorker.IsBusyEffect` | el motor no sigue mientras la tarea que pidió siga en la cola (salvo CardMove, BattleInit, ChainEnd…) | — | el rival y el jugador solo juegan con la mesa al día (`alDia`) |
| `Engine.CpuThinkUpdate` | la CPU piensa en el hilo nativo; sin `CpuWait` en C#; ícono a los 3 s | 0 | `ia_humana.h` (pensar 0.5–3 s, tope de MD) + `ritmoTurnoRivalPuede` (lectura) |
| `EffectTaskTurnChange` | espera las zonas listas y el Timeline `DuelTurnChange00/01` | 2.2 / 1.617 s | `presentarTurnoMD` (fases_md_juego.h) |
| `EffectTaskPhaseChange` (`playingPhaseChangeEffect`, evento PhaseChangeProcess) | abre `ACDuel<Fase>_near/far` y espera a que termine; todas las fases que manda el motor (Draw y Standby en cada turno) | ≈0.98 s | `ritmoTurnoEsperaFase` (antes no esperaba) y `ritmoTurnoCartelRobo` (cartel de la Fase de Robo, antes no había) |
| `EffectTaskWaitInput` → `DrawOperation` (robo propio) | DeckCenterMotion 0.40 → DrawCardMotionFirst 0.25 → DrawCardMotion 0.28 (delante de la cámara) → carta mostrada hasta un toque (≤10 s) → DrawCardToHandMotion 0.65 → DeckBackMotion 0.40 | ≈1.6 s + el toque | `dibujarRoboTurnoMD` (ritmo_turno_ui.h): 0.25 + 0.28 + 0.50 mostrada + 0.65 = 1.68 s (sin toque: pausa fija) |
| robo del rival (CardMove Deck→Hand) | `CardMoveField2Hand`, no se muestra | 0.45 s | `ritmoturno::ROBO_TURNO_RIVAL` |
| `EffectTaskCardMove` / `EffectTaskRunSummon` | el movimiento no bloquea; la invocación espera el aterrizaje | 0.50 / 0.67 / 0.88 s | `ritmoLlegada` (ya estaba) |
| `EffectTaskLifeDamage` | cambia los LP y termina a los 0.3 s (la cuenta de 1.0 s sigue sola) | 0.3 s | `lpConEspera` 0.6 s (más pausado, se deja) |
| `EffectTaskWaitFrame` | param/30 s | — | — |

### Joey the Passion (Joey_Pc.exe, decompilado en re-output; 30 cuadros/s)

Tampoco tiene pausa de «pensar»: la IA corre en el mismo paso y el motor espera a que ninguna animación esté ocupada.
Cartel de turno 64 cuadros (2.13 s) + Draw Phase 16 (0.53 s); cartel de fase 16 (Battle 24 = 0.80 s); robo / mover
carta 8 cuadros (0.27 s); la carta de la CPU que se juega boca arriba se muestra antes ~1.33 s (FUN_0040ad90);
invocación 1.1 s, colocar 0.73 s; activación ~0.57 s; ataque completo ~3.0 s; número de daño 1.07 s + cuenta.

### Turno típico: antes / MD / Joey the Passion / ahora

Medido con el cronista del jugador humano (`src/pruebas/cu_ritmo_humano.h`, `humano.sh <salida> ritmo 3 1`: 3 duelos,
velocidad Normal, s de juego; la cronología cuadro a cuadro queda en `<salida>.ritmo.txt`).

| Paso | Antes | Master Duel | Joey the Passion | Ahora |
|---|---|---|---|---|
| Cartel de turno (jugador / rival) | 2.23 / 1.63 | 2.2 / 1.617 | 2.13 | 2.23 / 1.63 |
| Cartel de la Fase de Robo | no había | 0.98 | 0.53 | 0.97 |
| Robo del inicio del turno (jugador) | 0.80, **y la carta ya estaba en la mano desde el final del turno del rival** | ≈1.6 + toque | 0.27 | 1.68, una sola vez, después del cartel |
| Robo del inicio del turno (rival) | 0.45, el dorso ya estaba en su mano | 0.45 | 0.27 | 0.45, una sola vez |
| Fase de Espera + Principal 1 | 0.77 + 1.0 (la de Espera no se esperaba entera) | 0.98 + 0.98 | 0.53 + 0.53 | 0.97 + 0.97 |
| Cartel de fase de la CPU (BP, M2, EP) | 0.40–0.60: la CPU seguía jugando y el de turno lo cortaba; el de la Fase Final decía «FASE PRINCIPAL 1» | 0.98, bloquea | 0.53 (BP 0.80), bloquea | 0.97–1.0, bloquea, con su nombre |
| CPU: pensar antes de cada jugada | 0.5–4.3 (media 2.7) | 0 (solo animaciones) | 0 | igual (más la lectura) |
| CPU: colocar / invocar (presentarse) | 1.13 | 0.50 + 0.38 de elegir | 0.73 / 1.1 (+1.33 mostrada) | 1.13 |
| CPU: activar | 2.4–8 | 1.1 + cadena | 0.57–0.8 | igual |
| Ataque (animación) | 1.63–1.83 | 1.15–1.35 | ≈3.0 | 1.63 |
| Pausa tras una jugada visible de la CPU | mín. 0.37 | ~0.5–1 (observación) | — | mín. 0.9 (`ritmoturno::LECTURA_MIN`), media 2.3 |
| Turno completo del rival (media de los duelos medidos) | 21.7 | — | — | 31.2 |

Corridas del cronista: antes (215f1d6) 3 duelos, 40 fallas de los oráculos nuevos (robo temprano en cada cambio de
turno, carteles cortados); después 4 duelos, 0 fallas.

### Robo repetido: causa y arreglo

El motor (`ia.h empezarTurno`) roba en el mismo paso en que termina el turno anterior. `sincronizarMesaDuelo` copia la
mano del motor tal cual (`ctl.manoJ = mano` menos `ocultas`, que solo cubre el reparto inicial), y cualquier
sincronización de la mesa antes de presentar el `S_ROBO` (la del final de `presentarSucesos`, la de un `S_RESUELVE`…)
ponía la carta en la mano mientras el rival terminaba su turno (y sonaba `sfx_robar` porque la mano crecía). Después,
al presentarse el `S_ROBO` tras el cartel de turno, se animaba otra vez del mazo a la mano. Lo mismo con el dorso del
rival. Arreglo (`ritmo_turno_juego.h ritmoTurnoOcultarRobos`, llamado al final de `sincronizarMesaEnOrden`): lo robado
en un `S_ROBO` sin presentar no está en la mano dibujada ni falta en el mazo; la columna de fases muestra la fase
presentada, no la del motor.

### Casos (cronista del jugador humano, en todos los modos de prueba_humano: corre en humano_todo.sh y en el ciclo 5d)

- `CU-HU-ROBO-UNA-VEZ`: cada robo se anima una vez y la carta (cara del jugador; dorso del rival en su Fase de Robo) no
  está en la mano dibujada antes de su animación. Sin el arreglo falla en cada cambio de turno.
- `CU-HU-ROBO-TRAS-CARTEL`: el robo del inicio del turno se anima con el cartel de turno ya terminado.
- `CU-HU-RITMO-LECTURA`: entre una jugada visible de la CPU ya presentada y la siguiente pasan ≥ 0.9 s.
- `CU-HU-RITMO-CARTEL-FASE`: la CPU no juega con un cartel de fase a la vista.

Otros arreglos que salieron de las corridas: al cerrar «Construir mazo» desde la pausa la mesa se sincronizaba sin
orden y el robo pendiente aparecía en la mano (`mazo_duelo.h`: `sincronizarMesaEnOrden`); con la pausa abierta el
cartel de turno se detiene y el reloj del duelo no, así que el robo espera al cartel mismo (`ritmoTurnoCartelRobo`).
Topes de las escenas del jugador humano: carta por carta 70 → 100 s y casos fijos 90 → 120 s (el turno a este ritmo
dura más y una escena se cortaba con un suceso a medio ver); `CONTROLES-DETALLE` no juzga si la carta del recuadro
cambió al apretar (la destruyó un ataque).

La opción Velocidad de animaciones (Normal = MD; Rápida ×1.75, Muy rápida ×2.75; R2) escala todo esto.

Corrida r2 (2911742, en serie): jugador humano 1824 casos OK (0 FALLA, 0 DECISION, 1438 s), prueba_front_total 4724
OK, prueba_casos_uso 7025 OK, prueba_red_front 58 OK.

## El rival enseña la carta antes de jugarla (rama classic-ps5-cpu-joey)

Decisión del usuario: «Como Joey: enseñar la carta». Antes de colocar, invocar o activar una carta desde la mano, el
rival la enseña como en Yu-Gi-Oh! Power of Chaos: Joey the Passion, y después la juega con el movimiento de Master Duel.

Lo medido en Joey the Passion (Joey_Pc.exe a 30 cuadros/s, decompilado completo con Ghidra en modo solo lectura):

| Joey (función) | Qué hace | Tiempo |
|---|---|---|
| `FUN_0040ad90` (paso 0x19 del duelo) | pone la carta de la mano boca arriba (bit 0x8000), modo 1 (+0x18 = 1) con +0x1c = 0x200 | — |
| `FUN_00418b60` (la carta) | el volteo corre +0x14 de 0 a 0x100 de a 0x20 por cuadro, **en su lugar** (no se mueve ni se agranda); después +0x1c baja 0x10 por cuadro | 8 cuadros (0.267 s) + 32 cuadros (1.067 s) |
| `FUN_00418ef0` (su dibujo) | en modo 1 dibuja el marco de selección que parpadea con `(+0x1c >> 5) & 1` (cambia cada 2 cuadros) | — |
| `FUN_0041d050` | el motor no sigue mientras la carta está ocupada | 1.33 s en total |

Nuestro C++ (`cpu_muestra.h`, `cpu_muestra_ui.h`, gancho en `md_ritmo_juego.h ritmoLlegada`, que ya cubre invocar,
colocar y activar desde la mano):
- la espera de «elegir» del rival pasa de 0.38 s (estimado) a 1.33 s (Joey);
- la carta se levanta en su lugar de la mano hasta verse entera (nuestra mano del rival está medio fuera de la pantalla
  por arriba) con el tamaño de una carta de la mano del jugador, se da vuelta en 0.267 s y queda derecha (se lee) con el
  marco dorado parpadeando cada 2 cuadros durante 1.067 s; luego vuela con el movimiento de MD;
- **colocar** (boca abajo): no se da vuelta, se ve el dorso con el marco (no se revela lo oculto);
- el visor de la izquierda muestra la carta enseñada (solo las boca arriba);
- el cartel fijo «TURNO DEL RIVAL» se aparta mientras tanto; la pregunta fija, el selector y los carteles de fase se
  dibujan después (encima);
- la velocidad de animaciones (Rápida ×1.75, Muy rápida ×2.75) y R2 lo acortan en proporción (corre con el dt del duelo).

Capturas: `D:\capturas_ps5_cpu\` (escenas `coloca`, `invoca` y `swords` de `md_ritmo_pc.h`, una foto cada 0.1 s).
