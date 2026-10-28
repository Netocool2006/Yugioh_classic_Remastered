# Fases, engranaje y carteles al estilo Master Duel (rama classic-md-fases)

Pedido del usuario: «traer» de Yu-Gi-Oh! Master Duel (su copia de Steam) el cambio de fase, el menú del duelo y las
transiciones con TODOS sus detalles y tiempos, pero con el look de Classic Remaster.

El código de Master Duel (C# compilado a IL2CPP) depende de Unity, así que se traen sus DATOS:
- los AnimationClip y Timeline (curvas exactas);
- los Tween de uGUI (easing, estilo, duración, cadena);
- los campos serializados de sus scripts;
- los sprites.

Los reproduce `src/clips_md.h`. La lógica (qué fases se pueden elegir, el orden de la cadena) se leyó del dump de
Il2CppDumper y del decompilado de Ghidra. Detalle de la extracción: `D:\Docker\compartido\md_extraido\NOTAS.md`.

**Nada de Konami va al repo.** Los datos exportados viven en `<classic-assets>/md_ui/` (`exportar_clips.py`) y
`sincronizar.sh` los copia a `ps5/assets/md_ui/`. Sin ellos, el código cae a los mismos valores escritos a mano (los de
esta tabla).

## Controles nuevos

| Botón | En el duelo | Fuera del duelo |
|---|---|---|
| OPTIONS (corto) | En TU turno, con la mesa libre: el **selector de fases**. En el turno del rival no hace nada; con pregunta, cadena, elección o ataque en curso, avisa y no se abre. Con el selector abierto, OPTIONS o círculo lo cierran. | La pausa, como antes |
| Selector: izq/der (o arriba/abajo) | Elige entre las acciones válidas (las apagadas se saltan) | — |
| Selector: X | Confirma: la fase va por el motor (J_FASE) hasta la elegida | — |
| Panel táctil (PC: G, guion: `tactil`) | El **engranaje** = menú del duelo (Continuar, Opciones, Construir mazo, Abandonar, Salir al menú). Otro toque, círculo u OPTIONS lo cierran. | Construir mazo, como antes |
| Foco en el engranaje + X | Igual que el panel táctil. El engranaje está a la izquierda de la mano (fila de abajo): con la cruceta se llega con «izquierda» desde la primera carta. | — |
| Cuadrado | Igual que OPTIONS corto: abre / cierra el selector de fases (rama classic-ps5-control2, pedido del usuario). Con un visor abierto sigue cambiando de lado; con la pausa abierta no hace nada. La barra de ayuda y la placa FASES muestran los dos botones (`C\|OPTIONS`). | igual |
| Círculo en la batalla | Ya no termina el turno (era el atajo de classic-bp-terminar): se usa «Finalizar turno» del selector | — |

Decisión sobre el panel táctil: antes, en el duelo, abría «Construir mazo» (`mazo_duelo.h`). Ahora en el duelo es el
engranaje, y «Construir mazo» sigue dentro de su menú. En las demás pantallas el panel táctil no cambia.

## Selector de fases

Corresponde a `PhaseSelectWindow` y `PhaseSelect3D` de MD. Tres acciones, con los textos que pidió el usuario
(ES/EN en `idioma.h`):

| Acción | Destino | Válida si (IsPhaseMovable de MD → nuestro motor) |
|---|---|---|
| Iniciar Fase de Batalla | BP | En M1, desde el turno 2, sin Great Long Nose ni D.D. Borderline |
| Ir a M2 | M2 | En BP, sin monstruos obligados a atacar (Amazoness Archers) |
| Finalizar turno | EP | En M1, en BP (con la misma condición) o en M2 |

- **Camino de la fase.** La fase elegida queda como objetivo (`Mando::faseObjetivo`). El motor avanza con J_FASE de a
  una cada vez que el jugador queda libre en el mismo turno. Así pasa por la ventana de inicio de la batalla, la del
  final de la batalla y la de la Fase Final, igual que los cuadrados de antes, y el duelo en línea (`red/`, con reloj
  por turno) manda las mismas jugadas.
- **Carteles intermedios.** No se muestran (`cartelFaseVisible`): de M1 a EP se ve solo el cartel de la Fase Final.
- **Reloj del duelo en línea.** Si el reloj vence (`jugadaAutomaticaRed`), el selector se cierra solo (`puedeElegirFase`).
- **Dibujo** (`fases_md_ui.h: dibujarSelectorFasesMD`). Va a la derecha de la columna de fases, a la altura de la fase
  actual, con una flecha hacia ella. Lleva:
  - el velo y la entrada y salida de MD;
  - el título «SELECCIONA LA FASE» (MD: «Select Phase to Change to»);
  - tres botones redondos con el aro `GUI_PhaseCircle` y los iconos de MD (`GUI_PhaseBP/MP2/EP`) teñidos con nuestra
    paleta;
  - la sombra desplazada, los guiones entre botones, el resplandor `GUI_PhaseButtonCursor2` del elegido y la flecha
    `GUI_PhaseCursorArrow` encima;
  - las no válidas al 50 %;
  - el pie «✕ Confirmar · ○ Cerrar».
- **Placa del HUD** (antes «SIGUIENTE FASE» con el cuadrado). Ahora dice «FASES» con el ícono de OPTIONS y la primera
  acción válida.
- **Barra de ayuda.** `OPTIONS Fases` y `TÁCTIL Menú`; con el selector abierto: `CRUZ Fase · X Confirmar · O Cerrar`.

## Engranaje (DuelMenuButton de MD)

- **Lugar.** Abajo a la izquierda, junto al selector de cámara (pedido del usuario; en MD va arriba a la izquierda).
- **Arte.** Los sprites de MD tal cual: `GUI_T_DuelButtonSubmenu01` normal, `03` apretado o con el menú abierto, y
  `02` como capa «Over» cuando tiene el foco. La capa late con el Tween de MD (alfa 1 → 0.4, CubicIn, PingPongLoop, 1 s).
- **Marcas.** Lleva el aro dorado de nuestro foco y el botón TÁCTIL al pie (el PadIcon de MD).
- **Menú.** El de la pausa de siempre (`duelo_juego.h`), sin cambios de contenido.

## Carteles y transiciones (tiempos de MD)

| Elemento nuestro | De MD | Datos traídos | Antes | Ahora |
|---|---|---|---|---|
| Cartel de turno (TU TURNO / TURNO DEL RIVAL) | `DuelTurnChange00/01` (Timeline), `EffectTaskTurnChange` | Banda: entra x −21.68 → 0 entre 0.517 y 0.833 s. Letras de a una cada 1/60 s desde 0.617 s. Salida: alfa más caída (TU TURNO) o subida (RIVAL) de 5.4 u. Duración 2.2 s / 1.617 s. Sonido a 0.483 / 0.417 s. | Aviso «Tu turno» de 2.6 s y sonido a 0.35 s | Las curvas de MD; el duelo espera el cartel, como EffectTaskTurnChange |
| Cartel de fase | `Duel{Draw,Stanby,Main01,Battle,Main02,End}Phase_{near,far}` | Banda 0.25 → 4 (0.133 s). Grupo de x +6 → 0. Texto que se descubre (0.267 s), alfa 0.78. Sale a la derecha 0.867–0.967 s. En BP, estallido del texto (×1.5 y ×2) desde 0.633 s. | Banda dorada de 1.9 s | 0.967 s (BP 1.0 s) con sus curvas |
| Cartel fijo TURNO DEL RIVAL | (MD no lo tiene) | — | Aparecía de golpe | Entra con fundido (2 × 0.12 s) después del cartel de turno |
| Selector de fases: abrir / cerrar | `PhaseSelectWindow` In / Out, FrameIn / FrameOut | 0.12 s QuartOut / CubicIn; velo 0 → 0.3. Sonidos SE_PHASE_WINDOW_OPEN/CLOSE. | — | Igual |
| LP que bajan o suben | `DuelLP` (countTime) + `LPCounterSub` «ChangeValue» | El valor aparece en el campo (escala 0 → 2 en 0.02 s), se achica a 1.5 (0.58 s), sube (0.8–0.9 s), entra al panel (1.1–1.2 s) y se disuelve (0.2 s). Recién ahí el número cuenta en línea recta 1.0 s, con el color de daño o cura. El número crece a 1.5 (0.1 s) y vuelve 1.5 s después. Con daño, el panel se sacude 0.5 s con la curva de MD (hasta 40 × 20 px). | Rodaba con exp(−3.2 t), el daño flotaba 1.6 s junto al panel y temblaba con un seno | Lo de MD |
| Cartel de inicio del duelo (¡DUELO! / DUEL!) | `DuelTextStart` | Curvas por letra (entrada desde la izquierda, escala a 1 a los 1.4 s) y el estallido final del texto (x2.5) | 2.417 s, antes del primer cartel de turno | No habia | Las curvas de MD (fases_md_ui.h: dibujarCartelInicioMD) |
| Cartel de fin y entrada del resultado | `DuelTextWin2` / `DueltextLose` / `DueltextDraw` | Escala/alfa del texto de victoria; letras de a una en derrota/empate; evento StartUI | Resultado a los 2.5 s (victoria) / 1.3 s (derrota, empate) | Resultado de golpe | El cartel sobre la mesa y el panel en el StartUI de MD (la pantalla y los botones son los de siempre desde el primer cuadro) |

Las fases y los turnos (los carteles, la espera del duelo y los sonidos) usan el mismo camino en el duelo en línea:
`presentarSucesos` de `duelo_juego.h`.

## Archivos

| Archivo | Qué hace |
|---|---|
| `src/clips_md.h` | Reproductor genérico; lo comparte classic-md-ritmo. |
| `src/fases_md.h` | Selector, objetivo de fase, engranaje y relojes de los carteles. Va dentro de `control::Mando`. |
| `src/fases_md_juego.h` | OPTIONS → selector, panel táctil → engranaje, sonidos, cartel de turno. Va dentro del juego, desde `duelo_juego.h`. |
| `src/fases_md_ui.h` | Dibujo, con los sprites de `md_ui.png`. Va dentro de la interfaz. |
| `herramientas/md_fases/*.py` | Extracción (ver NOTAS.md). |

## Casos de uso nuevos (`prueba_front_total.cpp`, modo `extra`)

- `CU-FT-FASES-OPTIONS-ABRIR`, `-AYUDA`, `-INVALIDAS`, `-CANCELAR`, `-CUADRADO`.
- `CU-FT-FASES-OPTIONS-BP`, `-M2`, `-EP`, `-EP-DESDE-M1`, `-EP-DESDE-BP`, `-PRIMER-TURNO`.
- `CU-FT-FASES-OPTIONS-RIVAL-EP` (Scapegoat del rival en la Fase Final), `-PREGUNTA` (Mirror Force), `-ATAQUE`, `-RIVAL`.
- `CU-FT-ENGRANAJE-OPTIONS`, `-TACTIL`, `-CERRAR`, `-CURSOR`, `-RENDIRSE`.

### La pregunta fantasma (rama classic-arreglo-ciclo-md)

En la PS5 una carta del jugador se fue al cementerio sin que viera la pregunta para responder (Mirror Force al atacar)
y OPTIONS decía «Primero responde a la pregunta» sin pregunta a la vista. Con el ritmo de MD la mesa queda bloqueada
más tiempo y lo que el jugador abría en ese rato quedaba delante de la pregunta: el detalle (X o triángulo durante la
animación: la pregunta solo se abre desde la mesa libre) o el panel de visores (L3 / X en una pila: la pregunta se abría
detrás). Ahora:

- el detalle grande de una carta se cierra solo, como el CardInfo de Master Duel (`DuelHUD.CloseCardInfo`), al empezar
  cada movimiento de carta (`EffectTaskCardMove.CommonMove`), cada volteo / cambio de posición (`EffectTaskCardFlipTurn`)
  y cada paso de la cadena (`EffectTaskChainStep`): `control.h cerrarDetalle`, llamado desde `md_ritmo_juego.h` y
  `duelo_juego.h`. Lo que pasa en la mesa no queda tapado y el duelo no se detiene; el panel de visores se queda (se puede
  seguir mirando el cementerio con la pregunta detrás, como pidió classic-arreglo-visor) pero OPTIONS lo cierra y deja la
  pregunta delante (`duelo_consulta.h`);
- OPTIONS solo dice «Primero responde a la pregunta» con la pregunta a la vista (`preguntaALaVista`); si todavía no se
  puede abrir, «Espera a que termine la animación»; con el detalle abierto desde la mesa, OPTIONS lo cierra y sigue;
- en el lanzamiento de moneda OPTIONS abre la pausa (el selector no existe ahí y no había cómo abandonar).

Casos (`src/pruebas/cu_pregunta.h`, modos `extra` y `pregunta`): `CU-FT-PREGUNTA-FANTASMA-DETALLE`, `-OPTIONS`,
`-ENGRANAJE`, `-VISOR`, `-TODO`, `CU-FT-PREGUNTA-ANTES-DEL-CEMENTERIO`, `CU-FT-PREGUNTA-OPTIONS`. Sin el arreglo fallan
DETALLE (22 s con el detalle y la pregunta escondida), OPTIONS (el aviso fantasma) y VISOR (la pregunta detrás del panel).

En la corrida completa salió además un duelo trabado (H-valon-NUNCA, 150 s): la IA equipaba y desequipaba Armor Breaker
sin fin porque «1 vez por turno equipa O desequipa» se contaba por el uid y la unión cambia de uid al pasar de monstruo a
equipo. `ia.h` marca como usada la carta nueva (`CU-BE-UNION-UNA-VEZ`).

Se adaptaron al selector: `cu_front_total.h` (`pasarFase` / `elegirFase` / `abandonar`), `prueba_juego.cpp`,
`prueba_duelo.cpp` y `prueba_mando.cpp`. Los casos `CU-FT-FASES-BP-TERMINAR-*` (atajo del círculo) se retiraron con el
atajo.

## Capturas (vista previa del PC, sin GPU)

`D:\Docker\compartido\capturas_md_fases\`: `selector_fases_es.png` (turno 2: BP y EP activas), `selector_fases_primer_turno_en.png`
(solo EP), `engranaje_menu_es/en.png` (menu del duelo con el panel tactil), `cartel_turno_rival_es.png`, `cartel_tu_turno_en.png`,
`cartel_fase_es/en.png`, y los cuadros completos (5 por segundo) en `duelo_es/` y `duelo_en/` (guion en `lista.txt`, `capturar.sh`).

Inicio y fin: `cartel_inicio_duelo_es.png` (¡DUELO!), `cartel_derrota_es.png` (letras de DueltextLose), `resultado_tras_cartel_es.png` (el panel en el StartUI); cuadros en `inicio_fin_es/` (`lista2.txt`).

## Cuadrado = fases y la pregunta siempre a la vista (rama classic-ps5-control2)

Pedidos del usuario (PS5): «que el botón cuadrado también lleve a seleccionar la fase» y «otra vez salió "responde a la
pregunta" y no sé cuál es la pregunta; no sale por ningún lado visible».

- **Cuadrado** (`duelo_juego.h avanzarPausa`): en el duelo, con la pausa cerrada y sin visor abierto, es un OPTIONS
  corto (el mismo camino: `opcionesAlSelector` → `pulsarOptions`), así que abre y cierra el selector, cierra el detalle y
  da los mismos avisos. Ayuda: `iconoBoton` acepta dos botones juntos (`"C|OPTIONS"`), en la barra y en la placa FASES.
- **Por qué no se veía la pregunta**: (1) cuando el duelo pide ELEGIR una carta de la mesa o de la mano (objetivo de la
  carta con la que se responde, elección de un efecto) la única señal era el aviso de arriba, que se va a los 2.6 s; (2)
  el menú de la pregunta se dibujaba DEBAJO de los carteles de fase / turno y del corte grande del rival (que sale
  justo cuando el rival activa en tu turno y te ofrece responder); (3) una lista larga (elegir del cementerio) se salía
  de la pantalla por abajo; (4) «Primero responde a la pregunta» salía también con una carta volando.
- **Arreglo**: `pregunta_ui.h` deja la pregunta escrita en una placa fija arriba del campo mientras se elige (con
  «PREGUNTA: elige con la cruz y X»); el menú y la placa se dibujan encima de carteles y corte (`dibujarMandoEncima`);
  la lista larga muestra una ventana alrededor de la elegida con ▲ ▼; con una carta volando el aviso es «Espera a que
  termine la animación».
- Capturas: `D:\capturas_ps5_control\` (escenas `pregunta` y `fases` de `md_ritmo_pc.h`, `captura_ritmo.sh`).

## Elegir entre cartas: el selector de cartas de Master Duel (rama classic-ps5-elegir-cartas)

Pedido del usuario (PS5): «cuando haya una carta que te haga escoger entre cartas para sacar, debe sacar las cartas igual
que el de Steam. Ahorita saca una lista de nombres, eso no sirve porque uno no se sabe el nombre de todas las cartas».

- **Antes**: `control.h pedirEleccion` armaba un menú de texto (`AC_ELEGIR_LISTA`, una fila por nombre) cuando los
  candidatos no estaban todos en la mesa o la mano: buscar en el mazo (Reinforcement of the Army, Sangan…), elegir del
  cementerio o de las desterradas (Monster Reborn, Premature Burial…), costes del cementerio, cualquier mezcla.
- **Ahora** (`lista_cartas_ui.h`, MD `GenericCardListEx` / `CardSelectionList`): un panel con el título de la pregunta
  (la pregunta fija con el nombre de la carta que se activa se mantiene), una fila de CARTAS con su arte que se desplaza
  con las flechas (`<` `>`), la carta con el cursor más grande y con el cursor de selección dorado (`GUI_CommonSelectCursor`)
  y la V encima, el contador `n / N`, su nombre y los botones (X Confirmar, O Cancelar / No / No atacar donde corresponda).
  La carta enfocada se lee entera (nombre, ATK / DEF, texto) en el visor de la izquierda (`contenidoLista`).
  Cruz izquierda / derecha mueve, X elige, O cancela; cuadrado y OPTIONS como siempre.
- **Cartas ocultas**: la mano, el mazo y el extra del rival, y sus monstruos / magias y trampas boca abajo, salen de dorso
  («Carta boca abajo») y ya no se nombran en el menú (`candidatoOculto`).
- **Siguen siendo menú de texto**: Activar / No activar, los sí / no (un solo candidato con «puedes», p. ej. la posición
  de Monster Reborn), «Ataque directo». Elegir cartas de la mano o de la mesa sigue con el recuadro sobre las cartas de
  verdad (`ELEGIR_CARTA`) y la placa fija de la pregunta.
- **Pendiente**: el visor de pilas (L3 / X en el cementerio: `duelo_visores.h`) sigue siendo una lista con miniatura y
  nombre; no es una elección. Elegir VARIAS cartas (Graceful Charity) pregunta de una en una, como el motor.
- Capturas: `D:\capturas_ps5_control\elegir\` (escenas `elegir_mazo`, `elegir_cem` y `elegir_mano` de `md_ritmo_pc.h`).
