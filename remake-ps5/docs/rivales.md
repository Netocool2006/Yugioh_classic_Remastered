# Rivales de Yugioh Classic Remaster (rama classic-ia)

## Cuántos y por qué
- **40 rivales en 8 niveles (5 por nivel), 3 variantes de mazo cada uno = 120 mazos.** Es la misma forma que la
  tabla de mazos CPU del juego original (`0x5DB158`: 40 entradas, `rng % 5 + nivel * 5`), pero con temas y nombres.
  Con 3 variantes, la personalidad y el ruido de la IA, el mismo rival casi nunca juega dos duelos iguales
  (en la prueba: 800 duelos, 363 resultados distintos).
- Hay temas para todas las razas y atributos de la época, más Unión, Espíritu, Volteo, Toon, Quemar LP, Fusión,
  Ritual y un mazo "Goat Control" (cartas de la época, para cuando lleguen los efectos).
- Generados por `rivales_gen.py` (`python -I`, semillas CRC32 como `deck_gen.py`): es determinista, así que el
  mismo `cartas_ui.txt`/`cartas_classic.txt`/`goat.json` dan siempre el mismo `datos/rivales.txt`. Nada se escribe a
  mano salvo la lista de rivales (clave, nivel, personalidad, tema, nombre y lema en ES y EN).

## Progresión (nivel 1 → 8)
| Nivel | Techo ATK nv1-4 | ATK medio máx. | Tributo 5-6 / 7+ | Rivales |
|---|---|---|---|---|
| 1 | 1400 | 1150 | 1 / 0 | Insectos, Plantas, Peces, Bestias, Rocas |
| 2 | 1500 | 1250 | 2 / 0 | Bestia Alada, Reptiles, Dinosaurios, Hadas, Trueno |
| 3 | 1600 | 1330 | 2 / 1 | Guerreros, Zombis, Piro, Guerrero-Bestia, Unión |
| 4 | 1700 | 1400 | 3 / 1 | Máquinas, Lanzadores, Demonios, Agua, Espíritu |
| 5 | 1800 | **1467** | 3 / 1 | Volteo, Toon, VIENTO, FUEGO, TIERRA |
| 6 | 1900 | 1540 | 3 / 2 | Quemar LP, LUZ, Fusión, Ritual, Dragones |
| 7 | 2000 | 1620 | 4 / 2 | OSCURIDAD, AGUA, Guerreros élite, Archimagos, Máquinas |
| 8 | 2100 | 1700 | 4 / 3 | Dragones, Caos, Goat Control, Fusión maestra, Inframundo |

- 1467 es el ATK medio máximo de los mazos CPU originales (`reequilibrar_atk.py`): queda como la dificultad "normal"
  del nivel 5; por debajo se baja y por arriba se sube. Además la IA tiene menos ruido en cada nivel
  (`Cerebro::ruido()`: 520 puntos a nivel 1, 86 a nivel 8): los novatos se equivocan seguido.
- Sin efectos el duelo se decide por ATK/DEF, así que el techo de ATK es la palanca principal. En la prueba IA vs IA
  el rival con 3+ niveles más gana el 83-90 %.
- `ia::Base::mazoRival(nivel, semilla)` elige uno de los 5 rivales del nivel y una variante (lo que usa el duelo en
  vez del mazo aleatorio de classic-constructor). Avanzar de nivel al ganar queda para la pantalla de progreso.

## Reglas de los mazos
40-60 cartas (casi siempre 40), máx. 3 copias, respeta prohibidas/limitadas/semilimitadas Goat
(`cartas_classic.txt`), mazo de fusión ≤ 15 con solo fusiones cuyos materiales están en el mazo (y entonces
Polymerization). Los monstruos que no se pueden invocar de forma normal (54, del texto en inglés) no entran, salvo
los Toon en el mazo Toon (con Toon World boca arriba). Las magias/trampas ya son las de su tema/época: hoy son
farol boca abajo; cuando haya efectos funcionarán sin regenerar nada.

## Personalidades (src/ia.h, `PERSONALIDADES`)
equilibrado, agresivo, defensivo, farolero, paciente, temerario, calculador: pesos de agresividad, cautela
(miedo a las boca abajo), farol, riesgo (intercambios 1x1, ataques dudosos), paciencia (preparar sacrificios,
guardar cartas), umbral de ataque (el "dial 1" del original) y ritmo de pensar.

## Variedad: planes, mazos dinamicos, memoria y dificultad adaptable (rama classic-variedad, src/ia_plan.h)
Contra el mismo rival cada duelo se siente distinto:
- **Plan de juego por duelo**: agresivo, control, bloqueo + quema, enjambre, combo, castigo (trampas), molino/robo,
  tempo (robar monstruos) y victoria alternativa (solo si su mazo trae Exodia, Final Countdown o Destiny Board).
  Se sortea con pesos: afinidad de su personalidad (`AFINIDAD`) x encaje con su pool (papeles de
  `datos/roles_cartas.txt`, que genera `roles_gen.py` del texto en ingles) x no repetir (el ultimo plan x0.08, el
  anterior x0.35, el otro x0.7) x contrarrestar al jugador (sobre todo calculador, control, oscuro, paciente,
  tramposo). El plan cambia la personalidad efectiva (`mezclaPlan`) y suma/resta a cada jugada (`ajustarPlan`). Si
  va perdiendo (marcador = LP/1000 + 0.6 x ventaja de cartas < -2.5 y peor que al empezar el plan, tras 2 turnos) o
  el jugador queda con <= 1500 LP, cambia de plan (como mucho 2 o 3 veces) y lo dice: "¡Cambio de estrategia!".
- **Mazo dinamico** (`mazoDinamico`): nucleo (cartas en todos sus mazos y sus cartas unicas) + paquete del plan
  (7-10 cartas con el papel del plan, de su pool o de mazos de rivales de su nivel o menos; combos del plan de
  `datos/combos.txt`) + su pool por frecuencia; evita el mazo de la vez anterior. Siempre 40-42, limites Goat,
  rituales con su magia, Toons con Toon World, fusiones solo con todos sus materiales y Polymerization, y el ATK
  medio no pasa el de sus mazos de siempre (la dificultad por nivel se mantiene).
- **Variacion tactica**: un estilo por duelo (volcar o guardar, trampas antes o despues de la batalla, "trampa al
  reves" con el campo vacio, orden de jugadas), un humor por turno, combos conocidos con bono y horizonte de 2 turnos
  (colocar el sacrificio del grande de la mano, juntar materiales, retener Mirror Force y compania para el ataque que
  importa, no gastar quitamonstruos en monstruos chicos si el plan es de esperar).
- **Memoria** (`variedad.txt` junto a `progreso.txt`): por rival, sus ultimos planes y su ultimo mazo; del jugador,
  `Lectura` (trampas que de verdad activa, faroles = colocadas que no activa, cuanto ataca, muros), solo de lo que se
  vio en la mesa. Se mezcla con lo visto en el duelo en curso; entre duelos decae un 12 % hacia lo neutro y aprende
  un 35 % del ultimo. Efecto: contra el trampero ataca menos a ciegas (no arriesga a sus monstruos buenos) y limpia
  sus boca abajo antes (MST, Plumero); al farolero lo ataca sin miedo.
- **Dificultad adaptable** (Ajustes > "Dificultad adaptable", activada por defecto; se guarda en `variedad.txt`):
  racha del jugador acotada a +4..-3 (victorias seguidas +, derrotas seguidas -; abandonar cuenta como derrota). Con
  racha +: menos ruido en la decision (hasta x0.45), planes que encajan mejor con el mazo, paquete mas grande y ATK
  medio hasta +280 sobre el de sus mazos; con racha -: mas ruido (hasta x1.42), menos planes de castigo/control/bloqueo
  y ATK medio algo menor. Nunca cambia el nivel del rival ni su pool. Apagada: todo como con racha 0.
- Prueba: `src/pruebas/prueba_variedad.cpp` (validez de los mazos, diversidad, nivel, lectura, adaptable, memoria).

## IA humana (rama classic-ia-humana, `src/ia_humana.h`)

Opciones → **IA: Clásica / Humana** (por defecto Humana; `humana 1|0` en `variedad.txt`). La clásica es el Cerebro
de siempre. La humana usa el mismo Cerebro como política y le suma dos capas:

1. **Búsqueda que imagina jugadas** (Monte Carlo con determinización). En cada decisión no obvia (invocar/colocar,
   atacar y a quién, activar, responder) toma las 4-7 mejores candidatas de la política (con el plan ya sumado) y
   siempre "pasar"; copia el `Duelo` y adivina lo oculto del jugador (mano, orden del mazo y lo boca abajo salen de sus
   cartas no vistas; lo boca abajo es más probablemente trampa si el jugador es "trampero", menos si es "farolero" y
   menos tras cada ataque propio sin castigo); juega la candidata y sigue 1-2 turnos con la política de `ia.h` (el
   jugador simulado se arma con su lectura); evalúa con el plan (agresivo: daño; control/bloqueo: ventaja de cartas;
   orgullo de Kaiba: tener el monstruo más fuerte). Rondas con la misma semilla para todas las candidatas y poda de las
   que quedan atrás. Presupuesto por decisión: **80-150 ms** y **60-170 simulaciones** según nivel, dificultad adaptable
   y "vista" del personaje. En el juego corre **por pedazos de 4 ms por cuadro** (el indicador "pensando" sigue
   encendido); nunca en otro hilo, así no toca el motor desde dos lados.
2. **Capa humana**: faroles (magias boca abajo para frenar ataques; más si el jugador ataca con todo o limpia las boca
   abajo), callar una trampa marginal con LP de sobra, guardar la trampa para el ataque grande si el jugador ataca con
   todo, errores creíbles, pocos desde el nivel 1 (~1,5 %; ~1,15 % en el 8; la personalidad y la dificultad adaptable solo los mueven un poco, y la adaptable baja como mucho a -1) (no ver un combo raro, sobrevalorar el ataque yendo ganando, arriesgar yendo perdiendo,
   quedarse con la segunda), tiempo de pensar visible según la dificultad de la posición (lo ya pensado se descuenta;
   corre con el dt escalado por la velocidad de animaciones) y **rasgos** por personaje (`RASGOS_PERSONAJES`: riesgo,
   orgullo, lectura, farol, error, vista, pausa) o por personalidad para los genéricos.
   Memoria nueva: `Lectura::limpia` (cuánto destruye el jugador las boca abajo), 8º número de la línea `jugador`.

Medición (`src/pruebas/prueba_ia_humana.cpp`, 200 duelos humana contra clásica con los mismos mazos, cada IA juega
los dos rivales de cada par del MISMO nivel, cada IA juega cada mazo una vez; 19-oct-2026, tras "fuerte desde el nivel 1"): la humana gana el 68,5 % (137-63) y le gana a la clásica en todos los niveles (n1 73 %, n2 73 %, n3 69 %, n4 69 %, n5 66 %, n6 70 %, n7 58 %, n8 66 %), 35,5 ms de media por decisión imaginada (96 simulaciones), máximo 146 ms, errores creíbles 0,7 %, 0 jugadas ilegales; la prueba falla si la humana gana menos del 55 %, si hay jugadas
ilegales o si una decisión pasa de 175 ms. Variables de afinado: `DEPURA`, `KPRIOR`, `HORIZONTE`, `WCARTAS`,
`NIVELSIM`, `OBVIA`, `SIN_DIVERSIDAD`.

## Mazos de torneo (rama classic-mazos-torneo)
21 mazos curados a mano (5 inspirados en Master Duel y 16 arquetipos de la era Goat) en `datos/mazos_torneo.txt`
(`mazos_torneo.py` los valida). Van a los genéricos de nivel 7-8 y a los personajes fuertes como variantes aparte
(`Rival::torneo`): si `prepararRival` saca uno (40 % genéricos, 30-40 % personajes, nunca el mismo dos duelos seguidos)
se juega tal cual, sin reequilibrar ni diluir, con su plan. El mazo dinámico y los pools no los ven. Detalle, fuentes,
sustituciones y medición en `docs/mazos_torneo.md`; prueba: `src/pruebas/prueba_torneo.cpp`.
