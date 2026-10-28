# Efectos de cartas de Master Duel en la mesa (rama classic-ps5-efectos)

Pedido del usuario (probó en la PS5): «la parte de efectos sigue siendo un desastre; las espadas de luces reveladoras no se
entera uno que llegaron»; «a las cartas activas le dejas esta barra amarilla horrible encima»; «replica al de Steam o copia
parte de su código; es el mismo juego: solo difieren nuestro tablero 3D y algunos personajes 3D».

Regla: cada pieza sale de una clase / prefab / Timeline de Master Duel y se traduce a nuestro C++ con la misma secuencia,
tiempos, curvas, capas y colores. Los assets de MD (texturas, sonidos) NUNCA van al repo: se extraen a
`D:\Docker\compartido\md_extraido\` y lo que el juego lee va a `classic-assets` (fuera del repo; `sincronizar.sh` lo copia).

## La "barra amarilla" sobre las cartas activas

| | |
|---|---|
| Qué se veía (foto de la PS5) | un rectángulo amarillo sólido, vertical, más angosto que la carta, encima de cada magia boca arriba |
| Causa | `dibujarHalosActivasMD` (md_ritmo_ui.h) dibujaba el sprite `md_brillo_marco` del atlas `md_ui.png` con `spriteMD` (tipo IMAGEN: tapa lo de abajo), en pantalla y sin perspectiva. Ese sprite es `EfDuelHighlight00_01_01` de MD: **la máscara llena** con forma de carta (blanca, 54 % opaca), no un marco. Teñida de amarillo = bloque sólido. El ancho: `tamZonaMD` (alto proyectado × 0.69) → más angosto que la carta tendida. Además un `rectSuelo` ZONA amarillo con velo alrededor de la zona y la carta aclarada (+0.45). |
| Se reproduce en PC | sí: la captura de PC usa los mismos assets (`captura_ritmo.sh`, escenas `swords` / `luz`) |
| Master Duel | `fxp_HL_Select_trpmgc_001` (sale cuando la magia/trampa se puede activar). ParticleSystem `trpmgc/Light`: en bucle de 0.2 s, una partícula por ciclo, vida 0.6 s, `colorOverLifetime` alfa 0 → 1 (20 %) → 0; material `fxm_HL_Select_light_001`, shader `fxs_HL_Rollover_001`, `_Color` (1,1,0), `_Line` 0.8, `_Gradient_Value_02` 3.63, sobre la máscara `EfDuelHighlight00_01_01` y su difusa `_01_02`: **la luz amarilla que sale del borde de la carta**, tendida con ella; no aclara la carta. MD no deja brillo fijo en las continuas boca arriba (aquí se mantiene en toda magia/trampa boca arriba activa por un pedido anterior del usuario). |
| Ahora | `haloActivaMD` (md_efectos_ui.h): el sprite `halo_activa` (= difusa − máscara + la línea del borde, `herramientas/md_ritmo/exportar_efectos.py`) tendido en la mesa con la carta (`luzSueloMD`, perspectiva de la mesa), del tamaño de la carta (la textura trae la carta en 318 × 457 de 512), con la suma de las 3 partículas vivas de MD (`pulsoRolloverMD`), amarillo (1,1,0), y SOLO SUMA LUZ (tipo nuevo `LUZ` de `interfaz.frag`). Nada encima de la carta: `dibujarHalosActivasMD` quedó vacío y `brilloActivaMD` = 0. Sin el atlas: un borde fino, nunca un relleno. |
| Prueba | `src/pruebas/prueba_sprites_md.py` (en `ciclo_completo.sh`, suite CU-PRUEBA-SPRITES-MD): FALLA si un sprite pedido por nombre (`spriteMD`, `spriteRitmoMD`, `luz*MD`...) no está en el atlas o la textura del título, o si un sprite que se dibuja como imagen es una máscara de un solo color (con el código de antes falla en `md_brillo_marco`). En el juego, `uvEfMD` avisa por stderr el primer sprite pedido sin textura (`spritesEfMDPedidosSinTextura`). |

## Swords of Revealing Light (CardRunEffect de MD)

MD: `CardRunEffectSetting` (data.unity3d) mrk 4354: `Ef04354` si la activa el jugador (player 0), `Ef04354Op` si la activa el
rival (player 1); `seLabel` SE_EV_GOFUKEN, `waitFinish` 1, `blockInput` 1, `cameraShakeType` CardRun04354. Lo corre
`CardRunEffect.OnChainRun` → `_PlayEffect` (el Timeline de la carta) al correr el eslabón. Prefab y Timeline:
`LocalData` `duel/timeline/card/tcg/ef04354/` (volcados con `herramientas/fx_md/volcar_ef.py` y `timeline.py` a
`md_extraido/efectos/swords*/`; la curva del Black con `md_extraido/efectos/clips_sw.py` + `clip_muestras.py`).

| Pieza de MD (Ef04354) | Datos | Nuestro C++ (`md_efectos_ui.h`, `dibujarEspadasMD`) |
|---|---|---|
| Timeline `Ef04354Timeline` | 1.333 s (la última pista: 0.333 + 1.0) | `T_ESPADAS_MD`; `efc::duracion(Espadas)` 1.4 s (antes 3.2): el duelo espera a que termine (`waitFinish`) |
| `Black` (SpriteRenderer negro, delante de la cámara) | alfa 0 → 0.5 en 0.2 s, 0.5, → 0 de 1.15 a 1.333 s (AnimationClip "Recorded") | relleno `LUZ` negro sobre la escena (después de las cartas de la mesa, antes de la mano y el HUD) |
| `SwordsOfRevealingLight00..05` (estelas) | trail con `SwordsOfRevealingLight01`, velocidad 500, gravedad 600, vida 0.3 s, estela 0.2 s; salen de (±0.6 / ±1.5 / ±3.37, 84.6, −35.6) a 0.067 / 0.033 / 0 s (las de afuera primero); Op: de (±4.8 / ±11.2 / −17.06 y 15.3, ~39, ~−7.5) | la estela cae desde arriba de la cámara hasta donde se clava su espada, acelerando; 0.2 s de cola |
| `SORL_CenterArrow00..05` (las espadas clavadas) | malla `Eff_SRL_01` × 3.2 (alto 8 = una carta), color (1,1,0.75), vida 0.9 s (alfa 1 hasta el 80 %); en (−4.4 / 4.2, 1.1), (−14 / 13.3, −0.1), (22.1 / −22.6, −2.9), desde 0.333 / 0.30 / 0.25 s; Op: raíz girada 180° y corrida −1.6 en z, las del medio en z −1.0, desde 0.32 / 0.28 / 0.25 s | la espada de luz (`espada_brillo` = `EfSwordsofRevealingLight02`) de pie en la línea del medio de la mesa, del alto de una carta, la punta enterrada (−1.12), en arco hacia quien la activa |
| `Flash` | `EfSwordsofRevealingLight02` naranja (1,0.594,0) × tinte 0.35·2, 1.88 × 3.89, vida 0.05 s | igual, al clavarse |
| `ContactArea` | `EFF_flrstr_03_TEX` horizontal, (0.65,0.57,0.09), tamaño 10-12, giro ±10°, `sizeOverLifetime` 1 ↔ 0.7 (19 puntos), alfa 1 → 0 del 80 %, 1 s | `luzSueloMD("suelo_luz")` con la misma curva (`tablaMD`) |
| `particle` | 20 estrellas `fxt_ple_005`, cono 20° radio 4, velocidad 12, gravedad 4, vida 0.2-0.8 s, tamaño 1-5, `sizeOverLifetime` 1 → 0.11 → 0.96 → 0.21 → 1, (1,1,0.84) | las 20 por espada, con la misma física y curva |
| `seLabel` | SE_EV_GOFUKEN (`base/se_ev_gofuken`, 2.31 s) | `sfx_ev_gofuken` al empezar (yugireload.cpp `activarEfectoCarta`) |

Antes, `efectos_cartas.h` dibujaba para Swords las plumas de Harpie's Feather Duster (`MdPlumero`) y unas marcas: inventado.

Unidades: MD → mesa con el alto de carta (`ritmo::MD_ALTO_CARTA` 8 → `CARTA_ALTO`) para alto y profundidad, y con el paso de
zonas a lo ancho (MD ~8.8 → 2.7 × ESCALA). La cámara de MD está en −z y la nuestra en +z (se invierte z).

Lo que sigue igual (ya era de MD): la carta llega de la mano (`CardMoveHand2MagicZone`), `CardShow` (0.15 + 0.70 + 0.25 s),
la luz `fxp_chain_cardlight_001`, el volteo de los monstruos del rival (`CardFlipPlateMonster` 0.45 s, después del efecto)
y el contador de turnos `DuelTurnCount` (cronómetro + aguja + número, md_ritmo_ui.h `dibujarContadoresMD`).

## Sonidos de cada efecto (CardRunEffectSetting.seLabel)

Al correr su efecto, como MD, suena el de la carta (`efc::sonidoMD`, efectos_logica.h; WAV en `classic-assets/audio`,
`herramientas/audio/seleccion.json`):

| Carta (mrk) | Timeline de MD | seLabel | clave nuestra |
|---|---|---|---|
| Swords of Revealing Light (4354) | Ef04354 / Ef04354Op | SE_EV_GOFUKEN | sfx_ev_gofuken |
| Dark Hole (4342) | Ef04342 | SE_EV_BLACKHOLE | sfx_ev_blackhole |
| Raigeki (4343) | Ef04343_Far / _Near | SE_EV_RAIGEKI | sfx_ev_raigeki |
| Mirror Force (4887) | Ef04887 / Ef04887Op | SE_EV_MIRRORFORCE | sfx_ev_mirrorforce |
| Monster Reborn (4842) | Ef04842 | SE_EV_MONSTER_REBORN | sfx_ev_monster_reborn |
| Magic Cylinder (5124) | Ef05124_near / _far | SE_EV_MAGIC_CYLINDER | sfx_ev_magic_cylinder |
| Harpie's Feather Duster (4678) | Ef04678 / Op | SE_EV_HARPIESFEATHER_DUSTER_3D | sfx_ev_harpie |
| Thousand Knives (5166) | Ef05166 | SE_EV_THOUSANDKNIVES | sfx_ev_thousandknives |
| Mystical Space Typhoon (4909) | Ef04909 | SE_EV_EF04909 | sfx_ev_typhoon |

## Segunda vuelta: los CardRunEffect de MD traducidos (md_cardrun_ui.h)

Regla del usuario: «replica al de Steam; si está en otro lenguaje tradúcelo». Para cada carta se mira
`CardRunEffectSetting` (data.unity3d): si MD tiene un Timeline para la mrk, se traduce; si no (o si está `enable` 0), va
solo la activación genérica de MD (CardShow + `fxp_chain_cardlight` + `fxp_bff_apply`).

| Carta (mrk) | MD (CardRunEffectSetting) | Nuestro C++ |
|---|---|---|
| Swords of Revealing Light (4354) | Ef04354 / Ef04354Op, SE_EV_GOFUKEN, cameraShake CardRun04354 | `dibujarEspadasMD` (1a vuelta) + sacudida CardRun04354 |
| Raigeki (4343) | centerEffectPath Ef04343_Far (la activa el jugador) / _Near (el rival), SE_EV_RAIGEKI, CardRun04343 | pista 3D `ef04343_far/near` + pantallas BK (negro, alfa 0.72) y w (blanco 0.75 a 0.45 s) + sacudida; sin los rayos propios de antes |
| Mirror Force (4887) | Ef04887 / Ef04887Op, SE_EV_MIRRORFORCE, CardRun04887 | pista 3D `ef04887/op` + pantalla Black (0.5) + sacudida; sin la barrera propia de antes |
| Dark Hole (4342) | Ef04342 (player 2: los dos), SE_EV_BLACKHOLE, CardRun04342 | el remolino de MD horneado (md_agujero_negro, de antes) + pantalla Black (0.4) + sacudida |
| Monster Reborn (4842) | Ef04842 (effectType 26: en el espacio de la cámara), SE_EV_MONSTER_REBORN | pista `ef04842` en modo pantalla (el Ankh de MD al centro, sus destellos) + pantalla Black (0.71, 2.5 s) + sonido; sin el círculo de ritual de antes |
| Magic Cylinder (5124) | Ef05124_near / _far, SE_EV_MAGIC_CYLINDER, CardRun05124 | pantallas BK (0.55) y White (0.46 a 1.2 s) + sacudida + sonido; la pista no (ver Pendiente: tercera vuelta) |
| Harpie's Feather Duster (4678) | Ef04678 / Op, SE_EV_HARPIESFEATHER_DUSTER_3D | las plumas de MD horneadas (md_plumero, de antes) + sonido |
| Mystical Space Typhoon (4909) | Ef04909 sobre la carta elegida (runTiming 3), SE_EV_EF04909 | el tornado de MD horneado (md_tornado, de antes) sobre el objetivo + sonido |
| Thousand Knives (5166) | `enable` 0: MD no lo reproduce | solo la activación genérica (antes: los cuchillos horneados) |
| Heavy Storm, Pot of Greed, Change of Heart, Polymerization, rituales... | sin CardRunEffect | solo la activación genérica (antes: tornados, plumas, aura y círculos propios) |

Lo que se quitó por inventado: la "Aparición" genérica, los rayos de Raigeki (yugireload.cpp), la barrera de Mirror Force,
el círculo de Monster Reborn, las plumas en Pot of Greed, los tornados de Heavy Storm, el aura de Change of Heart, el
tornado y la explosión de Magic Cylinder y el polvo del dado / moneda. Queda `MdDestruir` (el CardBreak de MD,
`fxp_cardbrk_bff`) sobre cada carta que se lleva el efecto.

### Las pistas 3D (`herramientas/md_ritmo/exportar_pistas.py` → `<assets>/md_ritmo/efectos/<ef>.mdp + .png`)

- Simula el prefab con su Timeline (ventanas de las ControlTrack) con el simulador de ParticleSystem de
  `herramientas/fx_md/sim_fx.py` (emisión, bursts, forma, velocidad, fuerza, gravedad, ruido, tamaño / color en la vida,
  flipbook de uv) y guarda, a 30 cuadros por segundo, cada partícula en 3D (espacio del prefab, unidades de MD): los
  billboards y los estirados por la velocidad como partícula (el juego los pone de cara a SU cámara, como Unity) y las
  mallas / partículas tendidas o alineadas al mundo como triángulos 3D. No es un flipbook: la cámara puede ser cualquiera.
- `scalingMode` por eje como Unity (Hierarchy: la escala del mundo; Local: la del nodo).
- Los Shader Graph propios de MD que pintan con una rampa de color se traducen al hornear el atlas (`shader_md`):
  `fxs_04343/04887_Thunder_01` (afuera → adentro con smoothstep sobre el canal elegido), `fxs_SimpleHDR_MASKplus`
  (`_Color_01` → `_Color_02`, alfa con smoothstep), `fxs_ParticleRing` (punto / anillo procedural con `_Emission(_truth)`),
  `fxs_ParticleStar` (estrella procedural), `C_particleBlendAmp / AddAmpSt` (canal o alfa × `_TintColor` HDR),
  `fxs_TexDissolve` (umbral de disolución: modo disolución en `interfaz.frag`), `MASKNoiseScroll / UVDefTwistScroll /
  ExodiaFireShape` (ruido × máscara con su color). El color HDR va como escala en el tinte del material.
- En el juego: `md_cardrun_ui.h` (`dibujarPistaMD`), tipo `LUZ` de `interfaz.frag` con el rectángulo del atlas, tiling,
  mezcla aditiva o con alfa y disolución. El prefab va en el centro de la mesa (centerEffectPath); MD → mesa por el paso de
  zonas (8.8 → 2.7 × ESCALA), z al revés.
- `herramientas/md_ritmo/previa_pista.py` dibuja una pista con la cámara de MD para revisarla sin compilar.

### Tercera vuelta: mallas animadas y efectos en el espacio de la cámara

- `herramientas/md_ritmo/animaciones_md.py`: lee las AnimationTrack del Timeline (PlayableDirector.m_SceneBindings: pista
  → Animator), sus AnimationClip ("Recorded", con `clip_muestras.py`) y, por CRC32 de la ruta relativa al Animator, las
  curvas de cada Transform (pos / euler / escala) y la opacidad del material de las MeshRenderer. `exportar_pistas.py
  --anim` mueve con ellas las mallas (M = padre × TRS animado) y les pone la opacidad.
- Monster Reborn (Ef04842, effectType 26) está armado en el espacio de la cámara de MD: su Black de 952 × 536 (sprite de
  1 px a 100 px por unidad) ES la pantalla, 9.53 × 5.36 unidades, y el Ankh (malla `Ef04842UsModel02` × 4.2 × 0.9 bajo
  `Offset` 0.07) mide 2.5 = el 47 % del alto. No tiene curvas de Transform (su única AnimationTrack es la del Black). El
  juego lo dibuja en modo pantalla (`dibujarPistaMD(..., pantalla)`: x a la derecha, y arriba, alto de la pantalla = 5.36).
- Magic Cylinder (Ef05124): las curvas sí salen (los cilindros aparecen en 0.3 s y se van de 1.7 a 2.0 s; el rojo
  retrocede al tragar, el azul se inclina −7° al escupir) y su textura (`fxs_Fake_ModelAlpha`: `_Texture2D` × `_Emissive`
  con su opacidad; sin la luz de borde verde). Pero MD los pone a 30 de alto junto a su cámara (Root z −22.1); con el
  traslado a la mesa quedan por encima de nuestra cámara y no se ven (captura `cilindro` de la tercera vuelta). Se dejó
  como estaba (oscurecido, destello, sacudida y sonido); las pistas exportadas quedan en `md_extraido/efectos/pistas_sin_usar`.

### Quinta vuelta (rama classic-ps5-efectos2): la cámara de MD, la luz de sus Shader Graph y la espada de Swords

- **Magic Cylinder (Ef05124) con la cámara de duelo de MD (`VISTA_MD`, md_cardrun_ui.h).** Las piezas del prefab se
  proyectan con la vista y la proyección de MD (CameraViewSetting "Top": ojo (0, 95, −37) mirando al centro, FOV vertical
  30°, mano izquierda de Unity) sobre nuestra vista 3D (la pantalla menos el panel de la izquierda). Los cilindros salen
  como en MD: aparecen, el rojo traga y retrocede, el azul se inclina y escupe hacia la mesa; con su textura
  (`fxs_Fake_ModelAlpha`: `_Texture2D` × `_Emissive`, opacidad del material animada) y su luz de borde verde
  (`_RimLightColor` × (1 − N·V)^`_RimLight_Power`). Las mallas con alfa van como en Unity: sin las caras de atrás (Cull
  Back) y de atrás hacia adelante.
- **Mirror Force (Ef04887) también va con la cámara de MD**: su cúpula (`Doom`, una cuarta parte de esfera a 13.7 de alto)
  está armada para esa cámara; en la mesa, con nuestra cámara más cerca, la rodeaba y teñía de azul toda la pantalla.
- **La niebla verde de Ef05124** (`fxs_MASKNoiseScroll`): el umbral del ruido `_Main_Edge1 → _Main_Edge2` y el color
  `_Emissive`; `_Emissive_truth` (×9.7 en verde) es para el HDR con bloom de MD y sin HDR quedaba una mancha verde
  chillona. Los cilindros: la textura sin el ×1.5 de `_Emissive` (su alfa es 0) para no saturar el rosa.
- **El obj de UnityPy trae la v de las uv dada vuelta** respecto de la de Unity (los cilindros caían en la franja de arriba
  de su textura): `exportar_pistas.py` la endereza al leer cada malla.
- **La luz de los Shader Graph con normales.** `exportar_pistas.py` lee las normales del obj (UnityPy las escribe) y las
  guarda en la pista (formato MDP2: un triángulo "tipo 3" con normal por vértice y, por material, la luz traducida). El
  juego la calcula por vértice con la dirección a la cámara (la de la mesa, la de MD o la de la pantalla):
  - `fxs_SimpleRimLight_001` (cúpulas de Mirror Force, IMP_Doom de Raigeki): `_Color` × smoothstep(`_Smooth`,
    `_Smooth` + `_Blur`, 1 − |N·V|), suma luz.
  - `Flit_CardEf` (el Ankh de Monster Reborn, la espada de Swords): textura × `_color` + `_AmbColor` × `_AIntensity` ×
    max(N·`_AddDirection`, 0)^`_Attenuation` + `_FresnelColor` × `_FresnelContrast` × (1 − N·V)^`_FresnelPower`. El
    Ankh de MD tiene `_FresnelContrast` 0: su brillo no es de borde sino la luz de `_AddDirection` (0.8, 0, 0) × 5.
  - `fxs_Fake_ModelAlpha` (los cilindros): textura × `_Emissive` + `_RimLightColor` × (1 − N·V)^`_RimLight_Power`.
  Son lecturas de los parámetros del material (el código del Shader Graph no está en el ejecutable): mapa normal,
  especular y metálico no se usan.
- **La espada clavada de Swords = la malla de MD.** `SORL_CenterArrow00..05` (ParticleSystem en modo malla con
  `Eff_SRL_01`, material `AREF_LS_01` = `Flit_CardEf`, tamaño 0.4 × escala 8, giro 3D 40° / −10°, vida 0.9 s) se exportan
  solas (`exportar_pistas.py --solo SORL_CenterArrow`) a `ef04354_espadas` / `ef04354op_espadas` y `dibujarEspadasMD`
  las dibuja en 3D con sus tiempos en lugar del sprite. Las estelas, el destello, la luz del suelo y las estrellas van a
  la misma escala uniforme de las pistas (paso de zonas).

### Pantallas y sacudidas

- Los SpriteRenderer de pantalla del prefab (Black / BK / w / White) con la curva de alfa de su AnimationClip
  "Recorded" (`md_extraido/efectos/curvas_negro.py`, 20 muestras por segundo), tapan la escena 3D, no la interfaz.
- `CameraShakerSetting` (data.unity3d → `md_extraido/efectos/camera_shaker.json`) y `CameraShakerSetting.Info.GetShake`
  traducido del desensamblado (`md_extraido/efectos/desasm_getshake.txt`, RVA 0x4A6D20): t = tiempo − delay, r = t /
  duración, cae (1 − r); perlin(r · perlinCycle) · perlinPower; cos(t · cosCycle · 270°) · cosPower; sin(r · sinCycle ·
  360°) · sinPower con el signo por eje de `big`; sin(r · sinCycleSub · 360°) · sinPowerSub con el de `lit`; al azar entre
  randomMin / randomMax cada randomDelayFrame cuadros. `getShakeMD` en md_cardrun_ui.h; yugireload.cpp mueve el ojo y la mira
  con `sacudidaMD()` × (distancia de nuestra cámara / 102, la de MD).

## Capturas

`captura_ritmo.sh <remake-ps5> <salida> [escenas]` (src/md_ritmo_pc.h). Escenas nuevas de esta rama: `swords_j` (la activa el
jugador en su turno), `agujero` (Dark Hole del rival), `raigeki`, `renacer` (Monster Reborn), `equipo` (Axe of Despair),
`espejo` (Mirror Force del jugador cuando el rival ataca), `cilindro` (Magic Cylinder del jugador); siguen `swords` (la
activa el rival) y `luz` (trampa continua). Las que eligen objetivo al activarse lo llevan en la jugada (`renacer`, `equipo`,
`cilindro`).
Antes / después: `D:\capturas_ps5_efectos\`.

## Pendiente

- La espada clavada de MD es la malla `Eff_SRL_01` (textura `EfSwordsofRevealingLight01`, shader `Flit_CardEf`, inclinada
  40° / −10° con `startRotation3D`); aquí es la espada de luz `EfSwordsofRevealingLight02` de pie (un sprite).
- Magic Cylinder: las piezas van con la cámara de MD, no con la nuestra; si se cambia de cámara (L1/R1) no la siguen.
  Su cara usa solo la franja de la textura que tocan sus uv (el dibujo de los signos de pregunta no cae en el cilindro).
  Harpie's Feather Duster tiene una malla con huesos (SkinnedMeshRenderer).
- Mirror Force sin `flare04` (un destello pegado a la cámara de MD, hijo de Black: no tiene lugar en nuestra cámara) ni
  `Sonic_01/02` (anillos de disolución cerca de la cámara de MD: salían como un arco blanco). Las cúpulas con luz de borde
  (`fxs_SimpleRimLight`) van como un velo tenue: la luz de borde necesita las normales de la malla.
- El contador de turnos `DuelTurnCount` de MD sale también debajo de la carta mostrada (en la ficha); aquí solo en la mesa.
- En MD el brillo amarillo sale solo en la magia/trampa que se puede activar; aquí, en toda magia/trampa boca arriba activa.
