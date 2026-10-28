# El jugador humano (rama classic-jugador-humano)

Pedido: las pruebas del front (prueba_front_total, cu_front_total.h, casos de uso) juegan con el mando pero **leen el
estado interno** y siguen caminos fijos; se escaparon dos errores que el usuario vio en la PS5: (1) una carta suya fue
al cementerio sin que se enterara; (2) quedó trancado con «Primero responde a la pregunta» sin pregunta a la vista.
El probador nuevo juega como una persona: **decide solo con lo que la pantalla dibuja**, con tiempos humanos y
apretando botones de más; el estado interno lo usa únicamente el **juez** (los oráculos).

## Piezas

| Archivo | Qué es |
|---|---|
| `src/pruebas/pantalla_sin_gpu.h` | La **interfaz de verdad** (`interfaz.h` y todos sus `*_ui.h`) armada sin GPU sobre el juego sin GPU de `cu_juego.h`. Cada cuadro corre lo mismo que `yugireload.cpp` antes de la GPU (`actualizar()` + `construir()`) y lee los vértices armados: cartas (cara o dorso, rectángulo en pantalla, 2D/3D, alfa), paneles, el recuadro dorado del foco; y los textos e íconos de botones (`[X]`, `[OPTIONS]`, `[TÁCTIL]`…) con su lugar, tamaño y color. Lo tapado por un panel casi opaco dibujado encima no cuenta como visto. |
| `src/interfaz.h` | **Único cambio al juego**: el gancho `alDibujarTexto` en `texto()` e `iconoBoton()`, dentro de `#if defined(YUGI_REGISTRO_DIBUJO)`. El juego no define esa macro: no compila ni cambia nada. |
| `src/pruebas/cu_juego.h` | (prueba) con `CU_INTERFAZ_REAL` no define sus maquetas de `interfaz::` ni los `PAD_*`; la maqueta `nombreCarta` usa el nombre real si se le conecta (`nombreReal`), así los avisos dicen el nombre como en la consola. |
| `src/pruebas/cu_humano.h` | `hu::leer()` (la lectura de un cuadro: menú abierto con sus opciones, la elegida y su título; selector de fases; pausa; aviso; ayudas de botones; la fase), `hu::Juez` (los oráculos) y `hu::Humano` (el jugador). |
| `src/pruebas/prueba_humano.cpp` | Los modos: `fijos`, `duelos`, `general` (fijos + duelos), `cartas` (carta por carta, `HU_LOTE=k/n`), `todo`, `volcar` (imprime lo que lee el humano). Reutiliza las escenas de `prueba_front_total.cpp` (`escenaBase`, `inyectar`, `mazoVariado`, `nuevoDuelo`). |
| `src/pruebas/humano.sh` | Arma la carpeta (cartas .txt, datos, fuentes, Tween/Timeline de MD), compila y corre un modo. `HU_DEBUG=1` imprime lo que había en pantalla en cada falla. |
| `src/pruebas/humano_todo.sh` | Todo el jugador humano como en el ciclo (general + carta por carta en lotes paralelos) con su `resumen.txt`. |
| `src/pruebas/ciclo_completo.sh` | Paso **5d**: suite `HUMANO`; sus TSV (`humano_*.tsv`) entran al resumen del ciclo. |

## Lo que la vista sin GPU no tiene (diferencias con la consola)

- Sin texturas: los sprites de Master Duel (`md_ui.png`) faltan y la interfaz usa sus respaldos dibujados por código
  (mismos textos y lugares; los íconos de fase son letras). Las caras de las cartas se «cargan» un cuadro después de
  pedirlas, como en la consola, con un set falso por carta para saber cuál es cuál.
- Sin escena 3D: no hay figuras, entornos ni efectos del pack; solo la interfaz (que incluye la mesa, las cartas en el
  tablero, los viajes de Master Duel, carteles, cadena, LP, manos, menús).
- La cámara es la de juego (CINE) quieta, 1920×1080, sin respirar.
- El sonido de duelo vive en `yugireload.cpp` (sonidoDuelo): no se juzga acá (lo cubre prueba_audio).

## El jugador (hu::Humano)

- **Percepción solo de pantalla.** Lee la barra de ayuda (ícono + texto: `X=Acciones`, `OPTIONS=Fases`, `O=No activar`…),
  los menús (la fila con el `[X]` es la elegida; las opciones son los textos alineados; las grises = desactivadas), el
  selector de fases (`SELECCIONA LA FASE`, la acción en oro es la elegida), la pausa (`PAUSA`), el aviso de arriba, la
  fase de la columna, sus cartas en la mano (caras abajo) y en la mesa, y el recuadro dorado.
- **Se mueve con las flechas mirando el recuadro**: hacia la carta que quiere, de lado en la misma fila, arriba/abajo
  entre filas; si una flecha no movió el recuadro, prueba el otro eje.
- **Su turno** (la ayuda dice `OPTIONS Fases`): en las principales prueba cartas de la mano y de su mesa (X, lee el
  menú, elige Invocar/Activar/Voltear/Colocar/Cambiar…; la carta del caso primero); en batalla ataca; pasa de fase con
  OPTIONS y el selector. Preguntas (`O No activar`): a veces activa algo, si no «No activar». Elegir objetivo /
  sacrificio / carta / zona: X donde está el recuadro, si no avanza otra flecha; tras 8 intentos, atrás.
- **Ritmo humano**: reacción 0.3–1.5 s (casi siempre rápido), 8 % de las veces 1.5–3 s; un duelo de cada cuatro con
  un jugador lento. Botón sostenido 0.07–0.17 s.
- **Travesuras** (12 % de las decisiones; 2.4 % mirando sin nada que hacer): cuadrado, engranaje (panel táctil) y
  cerrarla con Continuar / círculo / OPTIONS, triángulo (detalle) y cerrar, OPTIONS, flechas al azar, L3 (registro),
  L1/R1 (cámara), X dos veces rápido, círculo cuando la ayuda lo ofrece. En animaciones, con preguntas pendientes y en
  el turno del rival.
- **Rescate** (para no culpar al juego por un humano torpe): 25 s sin progreso con la decisión del jugador y la mesa al
  día → cierra todo (círculo), contesta lo que se vea, OPTIONS → Finalizar turno. Si así tampoco avanza: falla.
- **Semillas reproducibles**: semilla base (argumento) → duelo `semilla*1000+n`, fijo `semilla*31+n`, carta
  `semilla*7919+índice`. Cada falla lleva semilla, contexto (duelo/carta/escena), cuadro, segundo de juego, turno y fase,
  y los últimos 40 botones con su segundo (`12.3:X`).

## Los oráculos (hu::Juez) y umbrales

Tiempos en s de juego a velocidad Normal (30 cuadros/s).

| Caso | Qué juzga | Umbral |
|---|---|---|
| `CU-HU-ENTERARSE` | Cada suceso que cambia el campo del jugador (al cementerio, desterrada, a la mano, descartada, posición, volteo, control, robo, daño de LP) y cada activación del rival (magia/trampa/efecto), y la invocación/activación de la carta del caso: tiene que **verse**. Señal: la carta (cara) en 2D fuera de la mano (cartel, activación, cadena, viaje), o moviéndose / creciendo / girando en la mesa, o su nombre en un texto (aviso, cartel, pila), o la cara del rival a la vista en su activación; daño: el número del daño o los LP rodando. | ≥ **0.4 s** visto en los 6 s desde que la pantalla lo presenta (MD: romperse 0.3 s + viaje 0.3 s; la versión 0.5 s daba falsos con la destrucción en combate de 0.42 s). Giro de posición / volteo: ≥ **0.25 s** (el giro dura 0.33 s). Suceso del motor que la pantalla nunca presenta en 25 s: falla. El sacrificio que el propio jugador eligió va como DECISION (`CU-HU-ENTERARSE-SACRIFICIO`). |
| `CU-HU-VENTANA-VISIBLE` | Pregunta del motor al jugador (lado 0) con la mesa al día y sin pausa/detalle/selector/visor encima: tiene que haber un menú o una elección en pantalla. | 2.5 s |
| `CU-HU-NUNCA-TRANCADO` | Nada avanza 45 s con la decisión del rival o con una animación que no termina; o con la decisión del jugador, el rescate no lo destraba; «Primero responde a la pregunta» sin pregunta a la vista. | 45 s / rescate a los 25 s / fantasma 1 s |
| `CU-HU-MESA-COHERENTE` | Con la mesa al día 0.6 s: lo dibujado en cada zona de monstruo y de magia/trampa de los dos lados (cara o dorso; las propias boca abajo pueden verse de cara), la mano del jugador (caras), los dorsos de la mano del rival, los LP de los dos paneles = el duelo. Reintenta hasta 3 s de mesa quieta (los LP ruedan). | 3 s |
| `CU-HU-CONTROLES-*` | `CUADRADO` (no cambia nada con la mesa libre), `ENGRANAJE` (el panel táctil abre la pausa con Continuar, Opciones, Construir mazo, Abandonar duelo; se cierra con Continuar/círculo/OPTIONS), `PAUSA-DETIENE` (con la pausa abierta el duelo no sigue: ni sucesos ni la animación del ataque), `DETALLE` (si la ayuda dice `T Detalle`, el triángulo lo abre), `OPTIONS` (nunca abre la pausa; si la ayuda dice `OPTIONS Fases`, abre el selector o da un motivo), `AYUDA` (el círculo que ofrece la ayuda cambia algo), `ABANDONAR` (Abandonar duelo = derrota y pantalla de resultado), `PAUSA-OPCIONES` / `PAUSA-CONSTRUIR` / `PAUSA-VUELVE`. | 0.3–1 s |
| `CU-HU-CARTA-VISOR` | Con el recuadro en la carta del caso, la columna (visor) muestra su nombre. | 0.5 s |
| `CU-HU-AUTOPRUEBA-*` | Que el juez vea fallas a propósito: una carta del jugador al cementerio presentada sin nada en pantalla (ENTERARSE) y una carta cambiada en la mesa dibujada (MESA). | — |

## Casos

- **Fijos** (`CU-HU-FIJO-*`): el jugador con dos monstruos en ataque, tres en la mano y, colocadas, Seven Tools of the
  Bandit y Mystical Space Typhoon (para que el juego le ofrezca responder); el rival con un monstruo y la carta:
  `MIRROR-FORCE` (el pedido: atacar contra Mirror Force), `TRAP-HOLE`, `SAKURETSU-ARMOR`, `MAGIC-CYLINDER`,
  `NEGATE-ATTACK`, `BOOK-OF-MOON`, `TORRENTIAL-TRIBUTE`, `DUST-TORNADO` (colocadas: el rival la activa en cuanto puede)
  y `SWORDS-OF-REVEALING-LIGHT`, `RAIGEKI`, `DARK-HOLE`, `MYSTICAL-SPACE-TYPHOON` (en la mano del rival, en su turno: la
  IA decide). El caso falla si algún oráculo falla en la escena. `PREGUNTA-BOTONES`: el rival ataca, el jugador tiene
  Negate Attack y Sakuretsu Armor colocadas; con la pregunta a la vista aprieta engranaje, triángulo, OPTIONS, cuadrado
  y círculo, y tras cada uno la pregunta tiene que seguir (o volver) a la vista; después la contesta.
- **Duelos** (`CU-HU-DUELO-<rival>`): 6 duelos completos contra rivales repartidos (historia, libre, planes, torneo; la
  semilla elige), hasta 14 turnos o 600 s de juego; al tope abandona con el engranaje (prueba Abandonar = derrota).
- **Carta por carta** (`CU-HU-CARTA-<id>`, las 1784 de la base: primero las con efecto —monstruos de efecto, magias y
  trampas—, después fusión/ritual y las normales). Por carta, dos escenas (como `probarCarta` de prueba_front_total):
  - **A, la usa el jugador**: en la mano (fusión con Polymerization y sus materiales o por contacto; ritual con su magia
    y sacrificios; toon con Toon World; condiciones de invocación de cu_cartas.h). El humano la busca con las flechas
    (se juzga el **visor** con su nombre), la juega con el menú, ataca, termina el turno y mira el turno del rival
    respondiendo lo que se le pregunte, hasta su turno siguiente (máx. 100 s de juego; antes 70: rama classic-ps5-ritmo, el turno al ritmo de MD es más largo).
  - **B, la usa el rival**: trampa colocada en la mesa del rival en el turno del jugador (el rival la activa en cuanto
    pueda); magia en la mano del rival y monstruo en su mesa, en el turno del rival (la IA la juega/ataca con ella); el
    jugador responde; hasta el turno siguiente del jugador.
  - Todos los oráculos corren en las dos escenas; una falla marca el caso de la carta con el oráculo y el detalle. La
    salida cuenta cuántas usó el jugador, cuántas el rival, el visor y las cartas con alguna falla.

## Tiempos y memoria

Unas 64 veces más rápido que el tiempo de juego (la interfaz sin GPU cuesta ~0.4 ms por cuadro). Carta por carta
≈ 0.9 s por carta; en el ciclo, la general y 3 lotes de cartas corren a la vez (`HU_LOTES`, por defecto 3): ver la
duración medida abajo. Pico de memoria de cada proceso: pocos MB.

## Reproducir una falla

Cada falla (resumen y `<salida>.fallas.txt`) trae semilla del humano, contexto (duelo / carta y escena), cuadro,
segundo de juego, turno, fase y los últimos 40 botones con su segundo. Para una carta:
`HU_DEBUG=1 HU_DESDE=<id> bash src/pruebas/humano.sh /tmp/x.tsv cartas 1 1` (con `HU_DEBUG` imprime lo que había en
pantalla en la falla, lo que lee el humano del selector y cada suceso pendiente con dónde se vio la carta).
**Ojo**: el juego siembra el rival, la moneda y el mazo con el reloj (`duelo_juego.h`, `moneda_juego.h`,
`mazo_duelo.h`); las decisiones del humano son reproducibles con la semilla, pero la IA del rival puede jugar distinto
en otra corrida. Las escenas carta por carta arman la mesa a mano, así que lo que el rival tiene es siempre igual.

## Cobertura carta por carta

Las 1784 cartas de la base (1690 Goat + las de los mazos de torneo), una vez cada una (`CU-HU-CARTA-<id>`), en 4
lotes. En la corrida r3: el jugador la usó (invocó / colocó / activó desde la mano o la mesa con el menú) en 1677 (94 %;
las demás: el menú no la ofreció en esa escena o el humano eligió otra cosa —se listan en el log—); el rival la usó
contra el jugador en 970 (54 %: la IA decide; las trampas colocadas las activa en cuanto puede); el visor mostró su
nombre con el recuadro encima en 1782 de 1784. Por carta se validan los cinco oráculos en las dos escenas, el visor y
que su invocación/activación (del jugador) se vea.

## Corridas (rama classic-jugador-humano, tras unir classic-integracion 85039bb)

| Corrida | Casos | OK | FALLA | DECISION | Duración |
|---|---|---|---|---|---|
| r1 (3 lotes) | 1822 | 1658 | 163 | 1 | 996 s |
| r2 (4 lotes, lectura afinada) | 1822 | 1743 | 78 | 1 | 854 s |
| r3 (4 lotes, rescate + menús largos) | 1822 | 1674 | 147 | 1 | 843 s (14 min) |

(r3 sube por el hallazgo 1: el humano ahora pasa de fase con el foco donde quedó, y ahí el selector se traba.)
Memoria: ~20 MB por proceso; compilar prueba_humano.cpp pide ~1.8 GB y ~80 s (una vez).

## Fallas reales encontradas (para el agente que arregla el juego)

1. **NUNCA-TRANCADO — el selector de fases no confirma con el foco en un cementerio / desterradas / fusión.** Con
   OPTIONS abierto, X abre el visor de esa zona (`duelo_consulta.h mandoConsulta`: `!c.vis.abierto && ctl.estado ==
   NAVEGAR` y `B_X` con `foco` en zona; no mira `ctl.selFases.abierto`) y la fase no cambia; el jugador repite
   OPTIONS → X → visor y queda trancado. ~50 casos en r3 (p. ej. carta change_of_heart, lote 0:
   `HU_DESDE=change_of_heart`; también acid_rain, card_of_sanctity, pot_of_greed, upstart_goblin, mind_control…).
2. **ENTERARSE — las magias de equipo se van al cementerio sin verse** cuando muere su monstruo (combate o efecto):
   0.00 s, sin viaje ni aviso (en el registro no hay `[ritmo] sale <equipo>`). ~30 cartas: axe_of_despair,
   big_bang_shot, black_pendant, blast_with_chain, book_of_secret_arts, butterfly_dagger_elma, call_of_the_haunted,
   celestial_sword_eatos, demotion, elf_s_light, fairy_meteor_crush, fusion_sword_murasame_blade, glass_slippers,
   gravity_axe_grarl, impenetrable_formation, invigoration, kunai_with_chain, malevolent_nuzzler, mask_of_brutality,
   megamorph, metalmorph, metalsilver_armor, orichalcos_sword_of_sealing, ring_of_magnetism, rod_of_the_mind_s_eye,
   scroll_of_bewitchment, shooting_star_bow_ceal, smoke_grenade_of_the_thief, snatch_steal, sword_of_dragon_s_soul,
   twin_swords_of_flashing_light_tryce, wicked_breaking_flamberge_baou.
3. **ENTERARSE — descartes forzados por el rival**: la carta de la mano del jugador se va al cementerio sin verse
   (0.00 s): confiscation, delinquent_duo, mind_crush, mefist_the_infernal_general, toon_gemini_elf, hino_kagu_tsuchi.
4. **ENTERARSE — destierros desde el cementerio del jugador** (kycoo_the_ghost_destroyer, dark_blade_the_dragon_knight)
   y **cartas del mazo al cementerio** (assault_on_ghq, valkyrie_sechste, archfiend_s_oath): 0.00 s.
5. **ENTERARSE — a la mano / robo de efecto**: butterfly_dagger_elma y sinister_serpent vuelven a la mano sin verse;
   el robo que se devuelve enseguida (good_goblin_housekeeping, mirage_of_nightmare): 0.00 s; búsquedas al mano
   (dark_factory_of_mass_production, emissary_of_the_afterlife, goblin_zombie): 0.17–0.20 s.
6. **ENTERARSE — cortos**: el giro de un cambio de posición por efecto (book_of_moon, swords_of_concealing_light,
   ready_for_intercepting, tsukuyomi) se ve 0.20–0.23 s (umbral 0.25); el daño de efecto 0.13–0.30 s
   (skull_invitation, des_volstgalph, fatal_abacus); descartes por coste de otras cartas 0.23–0.30 s.
7. **CONTROLES-PAUSA-DETIENE — la pausa no detiene el ataque**: con el engranaje abierto la animación del ataque sigue
   y termina (`avanzarAtaque()` corre siempre en `render()`, antes del chequeo de la pausa); al cerrar, el combate ya
   pasó. ~60 casos (cualquier ataque del rival con la pausa abierta).
8. **A revisar — Construir mazo desde la pausa** en pleno duelo: al volver el registro muestra que se rearma el mazo
   del duelo (`[mazo] duelo: jugador 35 en el mazo + 5 en la mano`) y el turno sigue robando.
9. **A revisar — MESA-COHERENTE en escenas armadas** (~25 en r3): en los primeros segundos de una escena carta por
   carta la mesa dibuja en una zona un monstruo que el duelo no tiene ahí (p. ej. vorse_raider en jugador_monstruo_2
   al empezar BOOK-OF-MOON). No apareció en duelos completos: probablemente lo que queda de la escena anterior al
   inyectar (artefacto de la prueba), pero la forma es la de la «carta fantasma» que vio el usuario.
10. **DECISION — coste / sacrificio propio** (`CU-HU-ENTERARSE-COSTE`): el monstruo que el jugador sacrifica o la carta
    que paga de coste sale sin viaje ni aviso (0.00 s). Es lo que el jugador eligió; decidir si hace falta animación.

Los pedidos del usuario: **atacar contra Mirror Force** (`CU-HU-FIJO-MIRROR-FORCE`: el rival la activa, el jugador ve
la activación y la destrucción y se le ofrece responder) y **pregunta pendiente + engranaje / detalle / OPTIONS /
cuadrado / círculo** (`CU-HU-FIJO-PREGUNTA-BOTONES`: la pregunta siempre vuelve a la vista y se contesta) pasan tras
unir el arreglo de classic-arreglo-ciclo-md; el fantasma «Primero responde a la pregunta» ya no aparece en r3 (en r1,
con el juez anterior, salían 4).

## Arreglos de las fallas de r3 (rama classic-arreglo-ciclo-md)

| Falla r3 | Causa | Arreglo |
|---|---|---|
| 1. NUNCA-TRANCADO con el selector (~50) | Juego: `mandoConsulta` abría el visor de la pila con X aunque el selector de fases estuviera abierto | `duelo_consulta.h`: con el selector abierto X confirma la fase |
| 2. Equipos al cementerio sin verse (~30) | Juego: el combate se saltaba los `S_AL_CEMENTERIO` de los equipos y la mesa los quitaba al terminar | `md_ritmo_juego.h ritmoSalidasCombate`: viajan al cementerio cuando el destruido se rompe (T_ROMPE) |
| 3. Descartes forzados sin verse | Juego: `S_DESCARTA` no se presentaba | viaje de pila a pila (`ritmoEntrePilas`): de la mano al cementerio con la cara, y aviso «Descartada: X» |
| 4. Destierros del cementerio, mazo→cementerio, a la mano, robo devuelto, búsquedas | Juego: sin zona de la mesa no había presentación | el mismo viaje de pila a pila + aviso (Desterrada / Al cementerio / A la mano / Al mazo); el robo que ya se devolvió: aviso «Robas: X» y espera |
| 5. Giro a boca abajo 0.20–0.23 s; daño de efecto 0.13–0.30 s | Juego: al ponerse boca abajo la cara se ve medio volteo de MD (0.45 s); los daños seguidos pisaban el valor de MD antes de leerse; el daño del efecto que dispara el combate se sumaba al del combate | aviso «Boca abajo: X»; `lpConEspera` (el duelo espera 0.6 s a que se lea el valor); el segundo daño del combate se presenta aparte. El arnés (`cu_juego.h`) ahora quita los LP en el golpe como `yugireload.cpp` (sin eso veía −2100 en vez de −1600 y −500) |
| 6. La pausa no detiene el ataque (~60) | Juego: `avanzarAtaque()` corría siempre | `yugireload.cpp` / `cu_juego.h`: con la pausa o el constructor abiertos el ataque espera |
| 7a. Construir mazo desde la pausa rearmaba el mazo del duelo | Juego: `repartir()` al cerrar el constructor | en pleno duelo solo se toma el mazo activo para el próximo duelo |
| 7b. MESA-COHERENTE al armar escenas (~25) | Prueba: la carta «fantasma» translúcida (alfa 0.30–0.60) que la interfaz dibuja al elegir la zona (también sobre el monstruo a sacrificar) | `cu_humano.h`: con `ELEGIR_ZONA` la mesa no se juzga (es la vista previa de la jugada) |
| 8. DECISION ENTERARSE-COSTE (sacrificios) | Juego: `control.h` quitaba los sacrificios de la mesa sin viaje | `control.h sacrificados` + `ritmoSacrificados`: viajan al cementerio con el viaje de MD |
| (nuevo) cartas tapadas por el detalle | Juego: el detalle grande (X/triángulo en una animación) tapaba la mitad derecha de la mesa mientras se destruía la carta del jugador; es el «se fue al cementerio sin que me enterara» de la PS5 | Como Master Duel (GameAssembly: `EffectTaskCardMove.CommonMove`, `EffectTaskCardFlipTurn.WaitFlipTurn` y `EffectTaskChainStep.WaitLoadStep` llaman a `DuelHUD.CloseCardInfo`): el detalle se cierra solo al empezar cada movimiento de carta, volteo o paso de cadena (`control.h cerrarDetalle`). El juez `CONTROLES-DETALLE` cuenta que el triángulo lo abra en 0.5 s (puede cerrarlo el siguiente movimiento, como en MD) |
| DECISION Polymerization / magia ritual al cementerio | Juego: el motor la manda al cementerio antes de la invocación y la secuencia de fusión/ritual la tapaba | Como MD: se queda en su zona durante la invocación y después viaja con Field2GraveNormal; la ritual jugada desde la mano primero se activa en una zona de magia (viaje + CardShow). `CU-FT-ANIM-*-TIEMPO` mide desde que arranca la secuencia |
| búsquedas del mazo a la mano | Juego: `S_A_LA_MANO` valor 3 (del mazo) se leía como «al mazo» | valor 3 del mazo y 2 del cementerio a la mano; 5 al mazo |
| (nuevo) barra de ayuda ilegible | Juego: con la mano llena la barra bajaba a letra de 15 px en 4 filas | `interfaz.h planAyuda`: mínimo 16 px |
| (nuevo) duelo trancado en la Fase Final | Juego: con cartas de más el duelo espera el descarte, OPTIONS ya no ofrece fases y, con el foco fuera de la mano, no había nada a la vista | `control.h`: el foco va a la mano con el aviso «Descarta hasta tener N cartas» |
| (efecto del arreglo 3/4) fusión y ritual llegaban 3 s tarde | Juego: los materiales viajaban aparte antes de la secuencia de MD | `ritmoEntrePilas`: los materiales de una fusión/ritual los muestra la secuencia de MD |

Corrida tras los arreglos (rama classic-arreglo-ciclo-md, ed1ca83): **1821 casos, OK 1820, FALLA 0, DECISION 1**
(`CU-HU-ENTERARSE-COSTE`: la Polymerization propia que se va al cementerio tras resolver, 0.16 s), 1062 s.
