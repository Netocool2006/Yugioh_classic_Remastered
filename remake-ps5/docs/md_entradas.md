# Entradas de fusión y ritual, menú del duelo y fin del duelo al estilo Master Duel (rama classic-md-entradas)

Pedido: traer de Yu-Gi-Oh! Master Duel (copia de Steam del usuario) las entradas especiales de la invocación por
**fusión** y **ritual** (hasta ahora se usaba la entrada de carta clave antigua) y, del **menú del engranaje**, la
animación de apertura y cierre, la ventana, el cursor y los sonidos, sin cambiar su contenido. También los pendientes de
`md_inventario_visual.md`: `DuelEndTransition` (2 s) y el aviso de LP bajos (0,5 s). Se queda nuestro look and feel; los
tiempos, las curvas, los colores y los sprites son los de MD.

**Nada de Konami va al repo.** Los datos exportados viven en `<classic-assets>/md_ui/` y `sincronizar.sh` los copia:
- `md_entradas.png` y `md_entradas_atlas.txt`: `herramientas/md_fases/exportar_entradas.py`.
- `SettingMenuUI.mdui` y `DuelEndTransition.mdtl`: `MD_SOLO=DuelEndTransition,SettingMenuUI herramientas/md_fases/exportar_clips.py`.
  `MD_SOLO` exporta solo esos archivos y no rehace `md_ui.png`.

Sin esos archivos, el código usa los mismos valores, escritos a mano. El método y los volcados crudos están en
`D:\Docker\compartido\md_extraido\NOTAS.md` (sección «Entradas de fusión y ritual, menú del duelo») y en
`md_extraido\entradas\` y `md_extraido\engranaje\`.

## Cómo lo hace Master Duel

Lo que sigue sale de varias fuentes:
- `dump.cs`.
- El desensamblado de `FusionEffect.PlayFusionEffect`, `RitualEffect.PlayRitualEffect` y `DuelHUD.OpenSettingMenu`.
- Los Timeline de `LocalData`: `duel/timeline/duel/universal/summon/summonfusion|summonritual`.
- Sus `ParticleSystem`.

| Paso | Fusión (`FusionEffect`) | Ritual (`RitualEffect`) |
|---|---|---|
| Elección del Timeline | `FusionEffectSetting`: `SummonFusion0N_01`, con N = número de materiales | `SummonRitual01` con materiales, `SummonRitual02` sin ellos |
| Materiales a la cámara | `SummonFusionShowUnitCard0N`: 1,333 s, **antes** del principal. `SE_SMN_CMN_CARD_01` a 0,067 s | `SummonRitualShowUnitCard0N`: 1,333 s, **a la vez** que el principal (los círculos empiezan a 1,33 s) |
| Fondo | Pista `black` 0–2,0 s y el remolino `SummonFusionBG` (`fxt_Ef07445_swirl_001`) | Velo |
| Remolino / círculo | `SummonFusionVortex` (1,2 s). Rojo `guruguruRed(B)`: (1, 0,25, 0), 110 u, −π rad/s, tamaño ×0,55. Azul `guruguruBlue(B)`: (0, 0,29, 0,6), 100 u, −π/2 rad/s, ×0,64. Alfa 0 → 1 (6 % / 15 %) → 1 (67 %) → 0 | `RitualCircle01..05`: 01/02 a 1,33 s (3,4 s), 03/04/05 a 1,72 s (3,02 s). Azules y violeta. Giran +0,39 / −0,39 / +0,196 / −0,087 rad/s y se asientan a 0,5 de su tamaño en el 2–8 % de su vida |
| Los materiales entran | `SummonFusionInCard0N`: caen de y 50 a 0 en 0,5 s (ease-out, inclinados de 60° a 0°) y se retuercen hasta 1,33 s | `RitualTrailIn` 2,38–3,76 s (`SE_SMN_RITUAL_02_01` a 2,447 s). `CenterFlare` a 3,26 s |
| Fuego / rayos | `SummonFusionOut`: rayos 0,47–1,67 s, humo naranja 0,62–1,67 s, anillos 1,83–2,46 s | 8 llamas (`RitualFire01..08`), una cada 0,067 s desde 1,83 s, hasta 4,73 s. Su reflejo rojo (0,69, 0,05, 0,09). 20 chispas a 2,32 s |
| Columna | — | `Cylinder` 3,5–5,5 s: azul (0,23, 0,48, 1) y violeta (0,63, 0, 1), dos pulsos cada 0,5 s |
| Estallido | `SummonFusionPostFusion` 1,17–3,04 s; el estallido a 1,79 s (0,5 s) | `SummonRitualPostRitual` 3,87–5,89 s; explosión rosa (0,93, 0,4, 1) de 190 u, de 0,09 a 1 en 0,3 s, a 4,73 s |
| Sonidos | `SE_SMN_FUSION_01_02` a 0 s, `SE_SMN_FUSION_02` a 1,8 s | `SE_SMN_RITUAL_01` a 1,367 s, `_03` a 3,583 s, `_04` a 4,767 s |
| `StrongSummon` (sale el monstruo) | 1,8 s | 4,7 s (Ritual02: 2,37 s) |
| `StartCard` (la carta va a su zona) | 2,817 s | 5,2 s (Ritual02: 2,833 s) |
| Total | 1,333 + 3,04 = 4,37 s | 5,89 s (Ritual02: 3,55 s) |

## Cómo queda en nuestro juego

### Fusión y ritual

Archivos:
- `entrada_md.h`: los tiempos.
- `entrada_md_juego.h`: la detección y los sonidos.
- `entrada_md_ui.h`: el dibujo.

**Detección.** `S_INVOCA` de una carta de clase fusión o ritual. Justo antes tienen que haberse ido sus materiales:
- Fusión: Polymerization y los materiales al cementerio, o las piezas desterradas de una X-Y-Z.
- Ritual: la magia ritual o los sacrificios.

Por eso una fusión revivida con Monster Reborn no repite la secuencia.

**Espera.** El suceso se presenta **después** de la secuencia:
- `esperaEfecto` llega hasta el `StartCard`, como `EffectTaskRunSummon` espera el Timeline.
- Después la carta viaja a su zona con el ritmo de MD (`md_ritmo_juego.h`).
- R2 y la velocidad de animaciones la aceleran; saltando, no suena.

**Dibujo.** Va en pantalla, delante de la mesa, como los Timeline de MD, que se arman frente a la cámara:
- Los materiales en fila, con el rótulo «MATERIALES DE FUSIÓN» o «SACRIFICIOS DEL RITUAL».
- Los sprites de MD, teñidos con sus colores: `EfFusion009..012`, `EfThunder002`, `tlt_RitualCircle01..05` y `tlt_RitualFire01`.
- En el `StrongSummon`, el monstruo con nuestra banda de nombre (rótulo «INVOCACIÓN POR FUSIÓN» o «RITUAL»).

**Cartas clave** (emblemáticas y firmas) que ya tienen su entrada. Se combinan sin duplicar:
- La secuencia de MD se corta en su `StrongSummon`: el duelo espera hasta ahí y la secuencia se apaga en 0,25 s.
- La entrada de carta clave hace de «invocación fuerte»: la carta en primer plano, su nombre y la figura 3D.
- `entrada::Estado::trasMD` evita que la entrada de carta clave repita el vórtice o el círculo
  (`MdFusion`/`MdRitual`) y sus sonidos (`sfx_clave_fusion` / `sfx_clave_ritual`).

**Sonidos nuevos** (`seleccion.json` y `audio.txt`):

| Clave | Sonido de MD |
|---|---|
| `sfx_md_carta` | `se_smn_cmn_card_01` |
| `sfx_md_fusion_1` | `se_smn_fusion_01_02` |
| `sfx_md_fusion_2` | `se_smn_fusion_02` |
| `sfx_md_ritual_1..4` | `se_smn_ritual_01`, `_02_01`, `_03`, `_04` |

**Lo que no es exacto:**
- MD dibuja algunas piezas en 3D: los rayos, el humo y la columna del ritual. Aquí son planos, con los mismos tiempos y colores.
- Las posiciones de los materiales que caen en el vórtice son nuestras. La curva de caída y la torsión sí son las de MD.

### Menú del engranaje (`menu_md_ui.h`; dibujo en `duelo_ui.h: dibujarPausa`)

El engranaje de MD (`DuelHUD.OpenSettingMenu`) abre `Setting/SettingMenuViewController`, cuya vista es `SettingMenuUI`.

**Contenido.** Sigue siendo el nuestro: Continuar, Opciones, Construir mazo, Abandonar duelo y Salir al menú principal.

**Lo que viene de MD:**

| Momento | Datos de MD |
|---|---|
| Abrir (`Push`) | Velo (`FullScreen`) de 0 a 1 en 0,3 s, lineal.<br>Ventana (`SettingMenuArea`): alfa en 0,3 s; desde x +240, z +360 (escala por perspectiva) y giro Y −15° hasta su lugar, con QuartOut en 0,5 s. Ambas tras 0,15 s.<br>Título (`TitleArea`): baja desde y +120 con QuartOut en 0,2 s, tras 0,4 s. |
| Cerrar (`Pop`) | Velo a 0 en 0,2 s.<br>Ventana a x +240 / z +360 / −15° con CubicIn en 0,2 s.<br>Título en 0,1 s.<br>El menú se sigue viendo mientras se va. |
| Opción elegida | `SelectCursor` de 1 a 0,2 y `Over` de 1 a 0,4, ambos CubicIn y PingPongLoop de 1 s.<br>Las esquinas `GUI_CommonSelectCursor_CornerAll` se cierran de 16 px a 0 con QuartOut en 0,2 s al cambiar de opción. |
| Sonidos | Al abrir en el duelo: `SE_DUEL_SELECT` (`sfx_cursor`), como `OpenSettingMenu`. Antes sonaba `sfx_menu_confirmar`.<br>Al moverse y al cerrar: los del menú de MD que ya estaban (`se_menu_select_01`, `se_menu_cancel`). |

### Fin del duelo (`DuelEndTransition`, 2,0 s)

Corre en el `StartUI` del cartel de fin:
1. Un negro con un agujero redondo se cierra en 0,717 s (`TransIn`). Es el material `SoloTrans_fxp_BK_hole`.
2. La pantalla de resultado se arma detrás (`Wait`).
3. El agujero se abre hasta 1,25 s (`TransOut`).

El radio sigue la curva exacta de MD: 3,67 → 0,15 a los 0,7 s → 3,70. Nuestro toque es el borde dorado del agujero. El
panel de resultado aparece ahora a `StartUI` + 0,717 s.

### LP bajos (`DuelLP.IdleStep`)

Con 1 a 1000 LP, y mientras no se cuentan, el número late entre blanco y rojo:
- Color (1, k, k), con k = (sin(t · 6 / `EmergencyPeriod`) + 1) / 2.
- `EmergencyPeriod` = 0,5 s, así que late cada ~0,52 s.
- El umbral 1000 (`0x3e8`) y el 6 salen del desensamblado. El período sale de `DuelLPNear.mdui`.

## Casos de uso nuevos (`src/pruebas/cu_entradas.h`, modos `ritmo` y `extra` de `prueba_front_total`)

**Fusión:**
- `CU-FT-ANIM-FUSION-SECUENCIA`
- `CU-FT-ANIM-FUSION-TIEMPO`
- `CU-FT-ANIM-FUSION-RAPIDA`
- `CU-FT-ANIM-FUSION-SIN-MATERIALES`
- `CU-FT-ANIM-FUSION-CLAVE`

**Ritual:**
- `CU-FT-ANIM-RITUAL-SECUENCIA`
- `CU-FT-ANIM-RITUAL-TIEMPO`
- `CU-FT-ANIM-RITUAL-SIN`
- `CU-FT-ANIM-RITUAL-RIVAL`

**Engranaje:**
- `CU-FT-ENGRANAJE-MD-SONIDO`
- `CU-FT-ENGRANAJE-MD-CONTENIDO`
- `CU-FT-ENGRANAJE-MD-TWEENS`
- `CU-FT-ENGRANAJE-MD-FIN`

`cu_juego.h` ahora corre la lógica real de `entrada_md_juego.h`. La entrada de carta clave sigue siendo una maqueta.

## Capturas (vista previa del PC)

Están en `D:\Docker\compartido\capturas_md_entradas\`, generadas con `captura_pantallas.sh` y `lista.txt`, a un cuadro
cada 0,2 s.

Para reproducirlas en el PC, `YUGI_ENTRADA_MD` admite estos valores:
- `fusion`: Dark Paladin.
- `ritual`: Black Luster Soldier.
- El id de cualquier fusión o ritual con receta.
- `lp`: deja los LP en 800 / 500.

`YUGI_ENTRADA_MD_SIN=1` hace el ritual sin materiales.
