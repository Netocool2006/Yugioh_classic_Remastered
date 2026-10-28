# Ataque (normal y directo) como Master Duel, y rayos de las figuras como Yugi Reload 3D

Rama `classic-ps5-ataque`. Pedido del usuario después de probar en la PS5:

- "el rayo de las figuras 3d son pesimos es una barra la que sale"
- "cuando el oponente no tiene mounstros en su campo la carta debe atacar directo al oponente no al tablero vacio"
- "revisa los juegos que ya estan hechos ... replica al de Steam o copia parte de su codigo; si esta en otro lenguaje
  traducelo al nuestro"

## Referencias usadas

- **Master Duel** (Steam, `D:\Steam\steamapps\common\Yu-Gi-Oh!  Master Duel`): `GameAssembly.dll` con Il2CppDumper
  (`D:\Ghidra-Reload\herramientas\il2cppdumper\dump.cs`, `script.json`) y el desensamblado puntual
  (`md_extraido/ritmo/scripts/desasm.py`, salidas en `md_extraido/ritmo/desasm/desasm_batalla2.txt`); los
  ScriptableObject en `md_extraido/ritmo/otros_settings.txt` (CameraShakerSetting, DuelEffectDefine) y
  `ataque_bezier.txt` (AttackMotionSetting). Nada de MD va al repo.
- **Yugi Reload 3D** (`D:\Yugi-Reload\remake-ps5`): `src/reglas/regla_dragon.h` (rayoLuz, llamarada),
  `src/yugireload.cpp` (dibujarEfectos y efectosPack del Mago Oscuro, relampago, llama, humo, dibujarVfx) y los
  shaders `efecto_billboard.vert`, `efecto_fuego.frag`, `efecto_humo.frag` (copiados tal cual a `shaders/`).
- **Power of Chaos: Joey the Passion** (`D:\Yugi-Reload\datadat-extraido\j\duel\effect\attack.txt`: el ataque usa
  `atk_icon`, `atk_eff` y `dmg_ref`, el reflejo de danno sobre la pantalla): mismo esquema que MD, la carta va al lado
  del que recibe y el golpe se marca en la pantalla y en los LP.

## Que clase / metodo de MD corresponde a cada funcion nuestra

| Master Duel (GameAssembly.dll) | Nuestro C++ | Que hace |
|---|---|---|
| `EffectTaskBattleRun.AttackInitStep` (RVA 0x53f8e0): `Engine.IsMyself(dstTeam)` -> `SharedDefinition.directAttackViewportNear/Far` -> `Camera.ViewportToScreenPoint` -> `Util.ScreenPointToFixedHeightWorldPoint(..., directAttackPositionHeight)` | `ataquemd::puntoDirecto`, `ataquemd::puntoPantallaAAltura` (`ataque_md.h`); `prepararAtaqueDirecto` (`ataque_md_juego.h`), llamado por `animarAtaqueDuelo` (`duelo_juego.h`) | El objetivo del ataque directo: Near (0.5, 0.0) abajo al centro si recibe el jugador, Far (0.5, 1.0) arriba al centro si recibe el rival, a 15 unidades de MD (1.875 cartas) de alto |
| `SharedDefinition` (DuelEffectDefine): `DirectAttackPositionHeight` 15, `DirectAttackViewportNear` (0.5, 0), `DirectAttackViewportFar` (0.5, 1) | `ataquemd::ALTO_DIRECTO_MD`, `VIEWPORT_NEAR_Y/FAR_Y/X` | Constantes extraidas (offsets estaticos 0x60, 0x64, 0x6C) |
| `EffectTaskBattleRun.AttackMove` + `ChainedBezierMotion` (StrikeLight00 "Low" 0.45 s / StrikeLight02 "High" 0.65 s) | `Interfaz::cartaZona` (`interfaz.h`, ya de la rama classic-md-combate) con `destinoGolpe()` | El mismo Bezier de MD; con el directo el fin es el punto de MD y no la zona |
| hitTiming OnFinish (el golpe) -> `EffectTaskBattleRun.ShowDamageImpl` -> `DuelLP.ChangeLP` | `avanzarAtaque` (`yugireload.cpp`): en `T_IMPACTO` `cambiarLP`; `dibujarDanio` sobre `destinoGolpe()` | Baja los LP del que recibe en el golpe; el numero sale donde pega |
| `DuelHUD.ShowDamageFrame` (prefab DamageFrame: 0 -> 1 en 0.1 s -> 0 a 1.0 s, color 0.61, 0.09, 0.09), solo si `IsMyself(dstTeam)` | `Interfaz::dibujarMarcoDannoMD` (`ataque_md_ui.h`), `ataquemd::alfaMarcoDanno` | El marco rojo cuando el danno del combate es del jugador |
| `MainCameraOrganizer.Shake("AttackDirectNear" / "AttackDirectFar")` -> `CameraShaker.UpdateShake` (RVA 0x493910) -> `CameraShakerSetting.Info.GetShake` (RVA 0x4a6d20: por eje `Random.Range(min, max) * (1 - t/duration)`; x -0.88..0.88, y 8.8..11, z -8.8..-6.6; valor nuevo cada 3 cuadros, randomDelayGain 0.8; 0.5 s) -> `CameraShaker.shakeOffset` -> `MainCameraOrganizer.UpdateGameCam` (RVA 0x506160) lo SUMA a la POSICION de la camara (ejes del mundo; la rotacion no cambia) | `ataquemd::sacudidaDirecto` (`ataque_md.h`), `sacudidaAtaqueDirecto` (`ataque_md_juego.h`), usada en `updateUniformBuffers` | Empujon de la camara hacia arriba y hacia atras que se apaga lineal en 0.5 s (no un vaiven); unidades de MD a metros por la carta (2.03 m / 8) |
| `EffectTaskBattleRun.ShowDamageImpl` -> `DuelLP.ChangeLP` (un solo `ChangeValue` de `LPCounterSub` por golpe) | `Interfaz::numeroLPActivoMD` (`interfaz.h`): `dibujarDanio` no dibuja su numero si el de MD (`dibujarCambioLPMD`) se esta viendo | Antes salian dos numeros iguales por golpe (-2500 / -2500); ahora uno, como MD |
| `EffectTaskBattleAttack.WaitCardMoveStep`: `ACDuelDirectAtk00` (0.317 s) al declarar el directo | (sin cambio: la declaracion ya la presenta md_ritmo) | |
| `EffectTaskBattleRun.AttackEffectInfo.isDirectAttack` | `ataqueDirecto` (`ataque_md_juego.h`), `Interfaz::Golpe::directo` | La bandera del ataque directo |

Adaptacion (solo geometria): la camara de juego es mas baja que la "Top" de MD (y 95, 70 grados); por el borde de
arriba de la pantalla el rayo casi no baja y no corta la altura de 15 sobre la mesa, asi que el punto lejano queda
detras de la fila de magias del rival, una carta mas alla, a la misma altura. El cercano sale tal cual (z ~ 11.7,
entre el campo del jugador y la camara). El golpe del directo al jugador cae cerca de la camara: el efecto de golpe se achica por la distancia (para verse del
tamanno de un golpe en la mesa, como el fxp_hit de MD) y el destello blanco del combate no se usa en el directo (MD
marca el golpe al jugador con el DamageFrame rojo).

## Rayos de las figuras 3D (Yugi Reload 3D)

Antes `figuras.h` `efectosFiguras` dibujaba dos cilindros de luz (`tramo`) de la boca al objetivo: la "barra". Ahora
`ataque_fig_fx.h` (`ataqueDistanciaFig`):

| Figura (`DatosFigura::ataque`) | Yugi Reload 3D | Efecto |
|---|---|---|
| Aliento con halo azul/verde (Ojos Azules, Definitivo, Kairyu-Shin, Gaia) | `ReglaDragon::rayoLuz` | Esfera de plasma con filamentos, haz por capas con espirales de relampagos, arcos, anillos, pulsos, chispas; explosion de luz, arcos a la mesa, ondas de choque, humo |
| Aliento de fuego (Ojos Rojos, Bebe Dragon, Maldicion de Dragon...) | `ReglaDragon::llamarada` + `llama()` / `humo()` (efecto_fuego.frag, efecto_humo.frag) | Lenguas de fuego procedural de la boca al objetivo, brasas, humo, llamas que lo envuelven, NS_Explosion_Medium/Small, NS_Smoke_Plume |
| Magia (Mago Oscuro, Maga Oscura...) | `dibujarEfectos` + `efectosPack` del Mago | Carga electrica (NS_Player_Electricity_Looping), esfera con estela (NS_RocketTrail, NS_Explosion_Small), relampago (NS_TeslaCoil), espiral o anillos; NS_Spark_Burst |
| Rayo (Craneo Convocado, Jinzo, Dragon de Trueno...) | el rayo electrico del Mago | Relampago con ramas y segundo hilo, NS_TeslaCoil, NS_Spark_Burst |
| todos | `efectosPack` de Yugi Reload | NS_Marker_Target en el objetivo mientras carga, NS_Player_Buff/DeBuff_Looping en el atacante |

El reloj de Yugi Reload (SUELTA 1.65 s, FIN_EFECTO 2.9 s) se cuelga del golpe de MD (`relojYR`): el efecto llega al
objetivo en `T_IMPACTO`, cuando bajan los LP; la secuencia de MD no cambia. El directo apunta al punto de MD.

## Pruebas

- `src/pruebas/cu_humano.h` (prueba_humano): CU-HU-DANNO-UNICO falla si en un cuadro se dibujan dos numeros de danno
  iguales (se comprobo que falla con el codigo anterior: "-200 se dibuja 2 veces").
- `src/pruebas/cu_ataque_directo.h` (en `prueba_casos_uso`): CU-FE-ATAQUE-DIRECTO-PUNTO, -JUGADOR y -CPU fallan si
  el objetivo visual del ataque directo es una zona del tablero, si no va hacia el lado del que recibe o si no bajan
  los LP.
- PC: `YUGI_FIGURAS_ATAQUE="2.5:jugador_monstruo_2>rival"` (o `>jugador`) lanza un ataque directo de prueba
  (`atacarDirectoPrueba`, `ataque_fig_fx.h`); capturas con `captura_figuras.sh`.

## El ataque a 3 segundos, como Joey (rama classic-ps5-ataque3s)

Pedido del usuario: "lo quiero como joey el ataque a 3 segundos". Medido en el ejecutable de Power of Chaos: Joey the
Passion (`Joey_Pc.exe`, proyecto Ghidra `D:\Yugi-Reload\ghidra-proyecto`, decompilado sin abrir el juego):

- `FUN_00413bc0` crea el efecto del ataque (carga `duel/effect/attack.txt`: `atk_icon`, `atk_eff`, `dmg_ref`) y lo deja
  en `DAT_005f2fc4`. Los pasos los ponen `FUN_00413a90` (1 elegir atacante), `FUN_00413af0` (2 objetivo),
  `FUN_00413b20` (3 apuntar), `FUN_00413b30` (4 atacar), `FUN_004140b0` (6 danno).
- `FUN_00414110` (vtable 0x5da710, un paso por cuadro; el juego corre a 30 cuadros/s, 60 con `-speedy`) y
  `FUN_004142c0` (dibujo):
  - paso 3, apunta: el icono gira hacia el objetivo, +0x10 hasta 0x80 = 8 cuadros;
  - paso 4, de 0x80 a 0x100 el icono viaja con la curva de 0xA52EE8 (8 cuadros; sonido 0x10000006 en 0x90) y de
    0x100 a 0x200 el golpe (`atk_eff` crece y se apaga: 16 cuadros);
  - paso 6, el reflejo del danno (`dmg_ref`): +0x10 hasta 0x200 = 32 cuadros;
  - paso 7, en su primer cuadro baja los LP (`FUN_0047c4f0(-danno)`) y rompe las destruidas (`FUN_0041cee0`); +8
    hasta 0x100 = 32 cuadros.

| Fase | Antes (rama ataque) | Joey (30 cuadros/s) | Ahora (Normal, viaje "Low") |
|---|---|---|---|
| Declara / apunta | carga 0.25 s (dentro del viaje) | 8 cuadros = 0.27 s | carga 0.25 s (igual) |
| Viaje hasta el golpe | 0.45 s (MD Strike Low; High 0.65) | 8 cuadros = 0.27 s | 0.45 s (High 0.65), sin cambio |
| Golpe (la carta en el objetivo) | 0.25 s (MD attackExecPostTime) | 16 cuadros = 0.53 s | 0.50 s (`T_POST_GOLPE`) |
| Danno: numero y LP | en el golpe; LP cuenta 1.0 s | 32 cuadros = 1.07 s (`dmg_ref`); los LP bajan al final | en el golpe (MD); LP cuenta 1.0 s; vuelta 0.45 s |
| LP / destruidas / cierre | la destruida se rompe a 0.70 s | 32 cuadros = 1.07 s | se rompe a 0.95 s; cierre hasta 2.95 s |
| Total carta plana | 1.30 s (High 1.50) | 96 cuadros = 3.2 s | 2.95 s (High 3.15) |
| Total con figura 3D que ataca | 2.9 s (todo a 0.45) | - | 3.25 s (solo la carga del rayo a 0.6) |

Se mantiene lo de MD: golpe = bajada de LP en el impacto, el marco rojo y la sacudida del directo. Rapida (x1.75), Muy
rapida (x2.75) y R2 lo acortan en proporcion (`escalaAnim`). Prueba: CU-FE-ATAQUE-DURACION-JOEY
(`src/pruebas/cu_ataque_directo.h`) falla si en Normal el ataque dura menos de 2.8 s o mas de 3.3 s (con el duelo de
verdad y con las cuentas de la carta plana, el viaje "High" y la figura).

## Cuarta vuelta (rama classic-ps5-ataque4): lo que dijo el usuario en la PS5

| Pedido | Causa | Arreglo |
|---|---|---|
| "SIEMPRE la carta debe pegarle a la otra y SIEMPRE sacar humo la del contrincante" (salvo la figura 3D, que ataca a distancia) | el humo solo salia si la carta del rival se destruia (fxp_cardbrk_btl de MD) | en todo golpe a un monstruo salen bocanadas (NS_SmokePuffLight) y una columna de humo (NS_Smoke_Plume) de la carta del rival; con figura el golpe es en la carta del rival |
| "el efecto que tira sigue siendo pesimo, como una barra" | los efectos del pack que pedia el rayo (NS_TeslaCoil, NS_Player_Electricity_Looping, NS_RocketTrail, auras, marca, teleport, impacto de metal, polvo) **no estan horneados** en classic-assets/fx: no se dibujaban y solo quedaban los cilindros de luz (el nucleo de la Rafaga Blanca, el hilo de los aros del Mago, la columna al materializarse) | sin cilindros: chorro de particulas de luz y chispas (NS_Spark_Burst) en la Rafaga Blanca, aros sin hilo, la materializacion con resplandor y chispas. Solo flipbooks que existen: NS_Fire, NS_Explosion_Medium/Small, NS_Spark_Burst, NS_ExplosionRoil, NS_SmokePuffLight y NS_Smoke_Plume (copiado del horneado de D:\Unreal\YugiDemo\Saved\Horneado) |
| "el ataque es super corto, como de 1 segundo" y "primero hace el calculo de danno, luego ataca" (figuras) | el golpe (LP y destruidas) caia cuando el efecto llegaba al objetivo, a los 0.75 s; el resto del efecto seguia despues | la figura que ataca a distancia hace el ataque entero de Yugi Reload antes del golpe: carga 1.2 s, el efecto sale y llega, y se apaga justo en el golpe (`ritmo::T_FIG_DISTANCIA` = 2.45 s); despues el calculo, los LP y las destruidas (1.67 s). Total ~4.1 s |
| "la resta o suma del calculo de danno no se ve en las cartas, solo en los LP generales" (Joey) | se dibujaba el numero sobre la carta (y el de MD salia en medio del campo) | en el duelo no hay numero sobre la carta; el de MD sale junto al marcador de LP |

Joey, por el ejecutable (`Joey_Pc.exe`): `FUN_0047c4f0(-danno)` solo cambia el contador de LP (y suena); el paso 6
(`dmg_ref`, `FUN_004140b0`) solo existe cuando el objetivo sobrevive sin que nadie reciba danno (el "rebote") y se dibuja en
la posicion de la carta (`FUN_0045c140(jugador, 0, zona)`); si hay danno o destruccion va directo al paso 7 (LP y
destruidas). Asi el ataque de Joey dura 64 cuadros (2.1 s) con danno y 96 (3.2 s) con rebote; ninguno escribe numeros
sobre las cartas.

## Quinta vuelta (rama classic-ps5-ataque5): el atacante se eleva al declarar

Pedido del usuario: "la carta que ataque, antes de atacar, debe elevarse algunos centimetros en el tablero (...) para saber
cual ataca mientras decido si activo una carta de contraefecto; cuando termine el ataque baja, y luego la segunda se eleva".

| Master Duel | Nuestro C++ |
|---|---|
| `EffectTaskBattleAttack` declara el ataque y `CardRoot.StartAttackReady` (RVA 0x4b1f40) corre el `TweenPositionTo` "tweenAttackReadyStart" de la carta (`CardPlane` +0x128) y deja el `TweenPosition` "tweenAttackReadyIdle" (+0x130) flotando; `FinishAttackReady` (RVA 0x4af4c0) corre el "tweenAttackReadyFinish" (+0x138) que la baja | `ataque_elevacion_juego.h`: `elevarAtacante` al presentar S_ATAQUE (la declaracion, con la ventana de respuesta abierta), `avanzarElevacion` cada cuadro, `bajarAtacante` al negarse (S_ATAQUE_NEGADO), al cancelarse (`vigilarElevacion`) y en el ultimo 0.2 s del ataque |

Los valores de esos tres tweens (posicion destino, duracion, curva) estan en el prefab de la carta, dentro de `data.unity3d`;
no pude leerlos (la busqueda de `TweenPositionTo` con etiqueta de ataque en el bundle no devolvio nada), asi que se usa el
respaldo pedido: subida de 0.2 s y bajada de 0.2 s (smoothstep), `Interfaz::ELEV_ALTO` = 0.45 de nuestra mesa (la carta
mide 2.03: ~22% de su alto, ~3 cm si la carta fuera de verdad), sombra suave en la mesa (mas grande y tenue cuanto mas
arriba) y brillo ligero. La figura 3D sube con su carta (`figuras.h`). El ritmo de ~3 s no cambia: la subida es la fase de
apuntar (el duelo espera 0.2 s en la declaracion) y la bajada ocurre dentro del ultimo 0.2 s de `T_FIN_ATAQUE`.

Reglas: nunca dos elevadas (la siguiente declaracion espera a que la anterior termine de bajar; una redireccion cambia de
carta de golpe); ninguna queda arriba (ataque negado, cancelado, atacante destruido o redirigido: `bajarAtacante` o la
zona vacia).
