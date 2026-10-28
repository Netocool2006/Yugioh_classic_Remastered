# Inventario de lo visual del duelo de Yu-Gi-Oh! Master Duel

Pedido del usuario (2026-10-21): «estudia sus lógicas de todo lo visual a fondo, una a una, y las replicas aquí». Hay
que mantener nuestro look and feel, pero traer todos los detalles y los tiempos de Master Duel. La velocidad «normal»
es la de MD; la rápida y la lenta son escalas (`velocidad.h`).

Reparto entre ramas:
- **classic-md-fases**: la interfaz y el flujo. Es este archivo.
- **classic-md-ritmo**: las cartas y los efectos. Su detalle está en `docs/md_ritmo.md`, en su rama.

Cómo se trae cada cosa:
- Los datos de MD se exportan con `herramientas/md_fases/exportar_clips.py` a `<assets>/md_ui/` (fuera del repo).
- Se reproducen con `src/clips_md.h`: Timeline/AnimationClip con los tramos cúbicos de Mecanim, Tween con su easing
  (decompilado `Tween.EasingValue`), AnimationCurve y campos serializados.
- La lógica sale del dump de Il2CppDumper y del decompilado de Ghidra (ver `md_extraido/NOTAS.md`).

Estados: **hecho** (con los datos de MD) · **parcial** · **pendiente** · **no aplica** (no existe en la era clásica o en
nuestro juego).

## Interfaz y flujo (classic-md-fases)

| Elemento | De MD (clase / prefab / clip) | Datos traídos | Tiempo MD | Antes | Estado |
|---|---|---|---|---|---|
| Selector de fases (OPTIONS) | `PhaseSelectWindow`, `PhaseSelect3D.IsPhaseMovable` (bitmask de `Engine.ComGetMovablePhase`) | Ventana In/Out, velo, botones 224 px (icono 55, texto −26, sombra +8), cursor y flecha, guiones, sprites `GUI_Phase*` | In 0.12 s QuartOut, Out 0.12 s CubicIn, velo 0 → 0.3 lineal 0.12 s, cursor 0 → 0.8 CubicOut 1 s | Cuadrado = siguiente fase | hecho (`docs/md_fases.md`) |
| Sonidos del selector | `SE_PHASE_WINDOW_OPEN/CLOSE`, `SE_DUEL_SELECT/DECIDE` | `sfx_fases_abrir/cerrar` (base/se_phase_window_*) | al abrir y al cerrar | — | hecho |
| Engranaje / menú del duelo | `DuelMenuButton`, `SurrenderButton`, `MenuButtons` | Sprites `GUI_T_DuelButtonSubmenu01..04`; Over alfa 1 → 0.4 CubicIn PingPongLoop 1 s | — | OPTIONS = pausa | hecho (esquina abajo a la izquierda, pedido del usuario) |
| Contenido y animación del menú del duelo | `DuelHUD.OpenSettingMenu` → `SettingMenuViewController` / `SettingMenuUI` | Push/Pop (velo, ventana desde x +240 / z +360 / −15°, título), cursor y `Over` que laten, esquinas `CornerAll`, `SE_DUEL_SELECT` al abrir | Push 0.3 / 0.5 s QuartOut (+0.15 s), título 0.2 s (+0.4 s); Pop 0.2 s CubicIn | Nuestra pausa aparecía de golpe | hecho en classic-md-entradas (`docs/md_entradas.md`): el contenido sigue siendo el nuestro |
| Cartel de turno | `DuelTurnChange00/01` (Timeline), `EffectTaskTurnChange` | Banda, letras de a una, salida; `SE_NEXT_TURN_*` | 2.2 s / 1.617 s (curvas 2.05 / 1.55); sonido a 0.483 / 0.417 s | Aviso «Tu turno» 2.6 s, sonido 0.35 s | hecho; el duelo espera el cartel como MD |
| Cartel de fase | `Duel{Draw,Stanby,Main01,Battle,Main02,End}Phase_{near,far}` | Banda 0.25 → 4, grupo +6 → 0, texto que se descubre, estallido de la BP | 0.967 s (BP 1.0 s); `PhaseChangeProcess` 0.98 s | Banda 1.9 s | hecho (el duelo no espera el cartel de fase: el rival sigue jugando como antes) |
| Cartel fijo TURNO DEL RIVAL | — (MD usa el indicador de fase de la mesa) | — | — | Aparecía de golpe | hecho: fundido 0.24 s tras el cartel de turno |
| Indicador de fase (columna DP…EP) | `PhaseButton_c001` (3D en la mesa), `PhaseSelect3D.UpdateStatus/UpdateMovablePhaseIcon` | Prefab volcado | — | Nuestra columna | parcial: queda nuestra columna (look); el selector sale junto a ella |
| LP: bajar y subir | `DuelLP` (countTime 1.0 s, EmergencyPeriod 0.5, colores), `LPCounterSub` «ChangeValue» | Cadena ZoomIn → ZoonOut → Move0 → Move1 → Dissolve (+OnEffectEnd); CMainZoomIn/Out; sacudida «Damage» con su AnimationCurve | Valor 0–1.4 s; cuenta lineal 1.2–2.2 s; número ×1.5 0–0.1 s y vuelve a 1.6–1.7 s; sacudida 0.5 s | Rodaba exp(−3.2 t), flotante 1.6 s | hecho |
| LP: sonido de la cuenta | `SE_LP_COUNT_PLAYER/RIVAL`, `SE_LP_RECOVERY_*` (para al terminar) | — | Durante la cuenta | `sfx_lp` a los 0.15 s | hecho: `sfx_lp_jugador/rival`, `sfx_lp_cura(_rival)` cuando arranca la cuenta (1.2 s, de la cadena de LPCounterSub); el corte al terminar (Sound.Stop) no se replica (el SE dura 1.9 s, casi lo mismo que la cuenta) |
| LP: emergencia (poco LP) | `DuelLP.EmergencyPeriod` 0.5, `DuelLP.IdleStep` | 1..1000 LP: color (1, k, k), k = (sin(t·6/0.5)+1)/2 | latido ~0.52 s | — | hecho en classic-md-entradas |
| Moneda / piedra, papel o tijera | `DuelStartResultCointoss`, `CoinSettings`, `EffectTaskRunCoin` | Rama classic-fx-md: moneda 3D y curvas de MD (`fx/md/moneda_md.txt`) | — | — | hecho en classic-fx-md (no tocado aquí) |
| Cartel de inicio del duelo («¡DUELO!») | `DuelTextStart` (Timeline) | Por letra (D U E L): entra desde la izquierda (pos.x −7…0 u) y se ajusta la escala (2.1/0.3/1.3/0.6 → 1 a los 1.4 s); al final el texto crece a 2.5 (2.3–2.43 s). Nuestras letras ES/EN siguen la curva de la letra de MD que les toca | 2.417 s; el primer cartel de turno espera a que termine | No había cartel | hecho |
| Cartel de fin (victoria / derrota / empate) | `DuelTextWin2` (`SE_DUEL_WIN`, StartUI 2.5 s), `DueltextLose` (`SE_DUEL_LOSE`, StartUI 1.3 s), `DueltextDraw` | Victoria: escala 3 → 0.9 → 1, alfa a los 0.37–0.43 s, brillo a los 1.4 s. Derrota/empate: letras de a una (desde x −0.5, alfa en 0.167 s) | La pantalla de resultado aparece en StartUI (2.5 s / 1.3 s) | Pantalla de resultado de golpe | hecho: la pantalla pasa a RESULTADO enseguida (los botones responden igual) y su panel entra en el StartUI de MD; `DuelEndTransition` (2.0 s) hecho en classic-md-entradas (agujero negro que se cierra en 0.717 s y se abre hasta 1.25 s; el resultado se arma detrás) |
| Pantalla de resultado | `DuelEndMessage`, `DuelEndImage`, `DuelEndOperation` | — | — | Nuestra (classic-resultado) | pendiente |
| Cursor / selección en la mesa | `FieldPadCursorPhase`, `GamepadCursor`, `GUI_CommonSelectCursor_CornerAll` | Sprite en el atlas | — | Nuestro marco dorado | pendiente |
| Pistas de botones | `ShortcutIcon` / `PadIcon` de cada botón | — | — | Barra de ayuda | parcial: OPTIONS/TÁCTIL nuevos; el PadIcon en el engranaje |
| Diálogos de pregunta / elección | `DuelSelectDialog`, `DuelOkDialog`, `DuelInfoDialog`, `CardSelectionList` | Volcados | — | Nuestro menú | pendiente |
| Pila de la cadena (C1, C2…) | `DuelChainStripe` y similares | — (classic-md-ritmo estudia la cadena) | — | Nuestra pila | coordinar con classic-md-ritmo |
| Avisos cortos | `MessageArea`, `DuelInfoDialog` | — | — | Pastilla de vidrio | pendiente |
| Cámara y transiciones de pantalla | `DuelStartCamera01..09`, `DuelEndTransition` | `DuelEndTransition`: curva del radio del agujero (classic-md-entradas) | 2.0 s | Nuestra cámara | parcial: la transición del fin, hecha; la cámara de inicio, pendiente |
| Contador de turnos (Swords) | `IconTurnCounter`, `GUI_TurnCounter`, `GUI_TurnNeedle` | Sprites en el atlas para classic-md-ritmo | — | — | classic-md-ritmo |

## Cartas y efectos (classic-md-ritmo)

Lo completa classic-md-ritmo en `docs/md_ritmo.md` de su rama. Le corresponden: robo; mover, invocar, colocar y
voltear; activación; cadena; brillo de las activas; contadores; ataque y daño; destrucción; cementerio y destierro.
Los tiempos que ya extrajo están en `md_extraido/NOTAS.md` («Ritmo de cartas»).
