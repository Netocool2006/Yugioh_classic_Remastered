# La inteligencia artificial del rival

[English](AI.md) | **Español** · Volver al [README](../README.md) · [Índice de docs](LEEME.md)

Este documento explica cómo piensa el rival de Yugioh Classic Remaster: de qué capas se compone, cómo se combinan en una
decisión, qué cambia entre niveles, cómo se arman sus mazos y planes, cómo responde y ataca, cómo aprende, cómo se mide
si es buena y cómo hacerla más difícil o más divertida. Todo sale de leer el código (`remake-ps5/src/ia.h`,
`ia_plan.h`, `ia_humana.h`, `ia_red.h`, `aprendizaje.h`, `duelo_log.h`), los datos (`remake-ps5/datos/`) y los documentos
`remake-ps5/docs/ia_red.md`, `rivales.md`, `personajes.md`, `mazos_torneo.md` y `duelo_log.md`. Lo que no se pudo
comprobar se marca **no verificado**. Los números de rendimiento son los **registrados en esos docs**: no se volvieron a
medir al escribir esto.

## Índice

1. [Visión general: las capas y cómo se combinan](#1-visión-general-las-capas-y-cómo-se-combinan)
2. [Niveles 1-8, personalidades, ruido y presupuesto](#2-niveles-1-8-personalidades-ruido-y-presupuesto)
3. [Planes y mazos](#3-planes-y-mazos)
4. [Decisiones de combate y de respuesta](#4-decisiones-de-combate-y-de-respuesta)
5. [Red de valor y aprendizaje](#5-red-de-valor-y-aprendizaje)
6. [Cómo medir si la IA es buena, y cómo cambiarla](#6-cómo-medir-si-la-ia-es-buena-y-cómo-cambiarla)
7. [Cómo añadir un plan, un rol, un combo o un personaje](#7-cómo-añadir-un-plan-un-rol-un-combo-o-un-personaje)

---

## 1. Visión general: las capas y cómo se combinan

La IA no es una sola cosa: son cinco capas apiladas. Cada una usa la anterior y se puede apagar por separado (por eso el
juego puede comportarse "como antes" con una opción en No).

| Capa | Qué aporta | Dónde | Se apaga con |
|---|---|---|---|
| 1. **Cerebro clásico** | Genera las jugadas posibles del turno y las puntúa (en puntos aproximadamente en unidades de ATK) según la personalidad; suma ruido según el nivel | `ia.h` (`Cerebro`, `Personalidad`) | no se apaga: es la base de todo |
| 2. **Planes de juego** | Un plan por duelo (agresivo, control, bloqueo...) que cambia la personalidad efectiva, suma o resta puntos a cada jugada, arma el mazo del duelo y puede cambiar a mitad de duelo; memoria entre duelos; dificultad adaptable | `ia_plan.h` | sin `roles_cartas.txt` o sin memoria vuelve al mazo de siempre sin plan |
| 3. **Capa humana** | Búsqueda que imagina jugadas (Monte Carlo con determinización), faroles, lectura del jugador, errores creíbles, tiempo de pensar visible, rasgos por personaje | `ia_humana.h` (`Humana`) | Opciones > IA: Clásica |
| 4. **Red de valor** | Mejora la evaluación del final de cada simulación con una probabilidad de ganar aprendida por autojuego | `ia_red.h`, `datos/ia_red.txt` | Opciones > IA que aprende: No |
| 5. **Aprendizaje del jugador** | Entre duelos ajusta la última capa de la red a los duelos de ESTE jugador | `aprendizaje.h` | Opciones > IA que aprende: No / Olvidar lo aprendido |

### 1.1 Cómo se combinan en una decisión (IA humana, el modo por defecto)

```
 estado del duelo (ia::Duelo) + la memoria del rival (plan, lectura del jugador, racha)
          |
          v
 [1] Cerebro::candidatos / candidatosRespuesta        jugadas posibles con sus puntos (SIN ruido)
          |   principal() / batalla() / activaciones()  + "pasar de fase" (0 puntos) como referencia
          |   [2] ajustarPlan(...)                      el plan del duelo suma o resta puntos
          v
 [3] Humana::ajustarHumano                              faroles, trampas guardadas o calladas, orgullo
          |
          v
   ¿es obvia?  (una sola opción, un letal, una >= 700 puntos por encima de la segunda,
                o una respuesta sin nada que valga la pena)
      si | no
      |   |
      |   v
      |  se toman las K mejores (K = min(7, 3 + nivel/2)) + "pasar"; con probabilidad pErr
      |  se "olvida" a propósito una jugada rara (fusión, ritual, efecto de monstruo)  <- error creíble
      |   |
      |   v   por cada candidata, varias simulaciones (rondas con la misma semilla para todas):
      |      - copia del duelo con lo oculto del jugador ADIVINADO (determinización)
      |      - aplica la candidata y sigue 2 turnos (el actual y el siguiente) con el Cerebro como política
      |        (propio: nivel max(6, nivel+3); jugador: un modelo hecho con su lectura)
      |      - evalúa el final:  valor = evaluarHumana(plan, personalidad, orgullo)
      |                          [4] con red:  (1-mezcla)*valor + mezcla*escala*logit(red)
      |      - poda: las que quedan claramente atrás salen (al menos 2 vivas)
      |   |
      |   v
      |  se elige la mejor en promedio + prior (+ gusto por el riesgo del personaje);
      |  a veces, por error creíble, la segunda si estaba cerca
      v
   Jugada final  +  tiempo de pensar visible  ->  el Turno la aplica cuando se cumple la pausa
          |
          v
 al terminar el duelo: [5] se graba el duelo; el ajuste fino de la red corre FUERA del duelo
```

Puntos clave:

- **IA clásica** (opción "IA: Clásica") es solo la capa 1 (+2): `Cerebro::decidir` suma a cada candidata (menos a "pasar de
  fase") ruido gaussiano `azar.normal() * ruido() * 0.5` y toma la mejor. Es rápida y mucho más débil (la humana le gana
  el 64-68 % en las mediciones de los docs).
- **IA humana** (por defecto): la capa 3 usa la clásica como **política** de las simulaciones y como **orden previo** de las
  candidatas; lo que cambia es que mira hacia delante y evalúa el final.
- La **red de valor** no elige jugadas: solo cambia cuánto vale el final de cada simulación. Los errores creíbles, los
  faroles y el tiempo de pensar siguen igual con o sin red.
- Nunca se bloquea el juego: `Turno` corre la búsqueda **por pedazos** (`rebanadaMs` = 4 ms por cuadro) en el mismo hilo
  (el motor tiene estado estático, así que no se toca desde dos hilos) y mientras tanto se ve "El rival está pensando...".
- Solo se usa **información pública**: la IA nunca mira la mano ni las cartas boca abajo del jugador. Lo oculto se
  **adivina** (determinización). Lo que recuerda del jugador entre duelos son tendencias de lo que hizo en la mesa.

---

## 2. Niveles 1-8, personalidades, ruido y presupuesto

### 2.1 Qué es un nivel

Un nivel (1-8) define **qué mazos usa el rival** (techo de ATK, tributos) y **cuánto se equivoca** (ruido). Los 40 rivales
genéricos son 5 por nivel; cada personaje del modo historia lleva también un nivel 1-8 (`datos/personajes.txt`).

| Nivel | Ruido de decisión (`ruido()`) | Candidatas imaginadas (K) | Nivel de los rollouts propios | Error creíble base | Techo de ATK nv1-4 | ATK medio máx. | Tributos 5-6 / 7+ |
|---|---|---|---|---|---|---|---|
| 1 | 520 | 3 | 6 | 1,50 % | 1400 | 1150 | 1 / 0 |
| 2 | 458 | 4 | 6 | 1,45 % | 1500 | 1250 | 2 / 0 |
| 3 | 396 | 4 | 6 | 1,40 % | 1600 | 1330 | 2 / 1 |
| 4 | 334 | 5 | 7 | 1,35 % | 1700 | 1400 | 3 / 1 |
| 5 | 272 | 5 | 8 | 1,30 % | 1800 | 1467 | 3 / 1 |
| 6 | 210 | 6 | 8 | 1,25 % | 1900 | 1540 | 3 / 2 |
| 7 | 148 | 6 | 8 | 1,20 % | 2000 | 1620 | 4 / 2 |
| 8 | 86 | 7 | 8 | 1,15 % | 2100 | 1700 | 4 / 3 |

Cómo salen las columnas (todas verificadas en el código salvo la de mazos, que viene de `docs/rivales.md`):

- `ruido() = (520 - 62 * (nivel - 1)) * plan.factorRuido` (`ia.h`, `Cerebro::ruido`). Con ruido gaussiano de esa
  desviación x 0,5 sumado a puntos que van de decenas a miles (≈ ATK), el nivel 1 se equivoca seguido y el 8 casi nunca.
  También se usa, x 0,25, para dudar al decidir si activar una respuesta.
- K = `min(7, 3 + nivel / 2)` candidatas que la capa humana imagina (más "pasar").
- Nivel de los rollouts: `min(8, max(6, nivel + 3))`: se imagina jugando con calma, con menos ruido que en vivo.
- Error creíble base: `0,015 - 0,0005 * (nivel - 1)`, multiplicado por la personalidad, la dificultad adaptable y las
  candidatas cercanas (más abajo); tope entre 0,3 % y 4 %.
- El techo de ATK y los tributos son una propiedad de **los mazos** de cada nivel (generados por `rivales_gen.py`), no del
  cerebro. Sin efectos el duelo se decide por ATK/DEF, así que el techo de ATK es la palanca principal de dificultad. En la
  prueba IA contra IA, un rival con 3 o más niveles de ventaja gana el 83-90 % (`docs/rivales.md`).

### 2.2 Qué cambia de un nivel a otro

1. **Mazo**: techo de ATK de los monstruos de nivel 1-4 y cuántos tributos pide (tabla). Los temas suben de simples
   (Insectos, Plantas) a Dragones, Caos y "Goat Control" en el 8.
2. **Ruido**: 520 -> 86 puntos.
3. **Vista**: más candidatas imaginadas (3 -> 7), más presupuesto de simulaciones y de milisegundos (abajo).
4. **Errores creíbles**: apenas cambian (1,5 % -> 1,15 %): a propósito, la IA humana "juega fuerte desde el nivel 1"; lo
   que escala sobre todo es el ruido de la política y la calidad del mazo.
5. **Personaje**: los personajes del anime traen además `Rasgos` propios (abajo) y una dificultad de saga/nivel.

### 2.3 Personalidades

Las 10 personalidades de `ia.h` (`PERSONALIDADES`) son pesos que cambian cómo se puntúan las jugadas:

| Campo | Qué hace |
|---|---|
| `agresividad` | ganas de atacar e invocar en ataque (suma a los ataques) |
| `cautela` | respeto a las cartas boca abajo del rival y a quedar expuesto (`miedo()`; castiga atacar a boca abajo) |
| `farol` | colocar magias/trampas (y monstruos) boca abajo aunque no hagan nada |
| `riesgo` | aceptar intercambios 1x1 y ataques dudosos; el temerario (0,95) se permite jugadas suicidas |
| `paciencia` | guardar cartas, preparar sacrificios, esperar el momento; sube el umbral para responder |
| `umbral` | ganancia mínima para atacar (el "dial 1" del juego original: 0 rechaza el 1x1; el temerario -500) |
| `ritmo` | segundos base de cada jugada (tiempo de pensar) |

Las diez: equilibrado, agresivo, defensivo, farolero, paciente, temerario, calculador y, para personajes, tramposo, control
y oscuro. Cada personalidad tiene además un vector de **afinidad con cada plan** (`AFINIDAD`, sección 3).

La capa humana añade `Rasgos` (`ia_humana.h`): `riesgo` (varianza que busca), `orgullo` (valora tener el monstruo más
fuerte y no esconderlo), `lectura` (cuánto lee al jugador), `farol`, `error` (x errores creíbles), `vista` (x
simulaciones y tiempo) y `pausa` (x tiempo de pensar). Hay una fila por personaje conocido (`RASGOS_PERSONAJES`: Joey
arriesga y se equivoca más (riesgo 0,85, error 1,40); Kaiba es agresivo y orgulloso (orgullo 1,0); Pegasus e Ishizu leen
al jugador (lectura 1,8 y 1,7); Bakura y Weevil farolean; Yugi y Dartz ven lejos (vista 1,25)...) y una por personalidad
para los genéricos (`RASGOS_PERSONALIDAD`).

### 2.4 Presupuesto de tiempo por decisión (IA humana)

| Qué | Fórmula (verificada en `ia_humana.h`) | Rango efectivo |
|---|---|---|
| Simulaciones por decisión | `(60 + 14 * (nivel-1)) * vista * (1 + 0,06 * dificultad) * escala` | entre `simsMin` (60) y 170 |
| Milisegundos por decisión | `(80 + 10 * (nivel-1)) * vista * max(0,5, escala)` | entre `msMin` (80) y `msTope` (150) |
| Rebanada por cuadro | `Turno::rebanadaMs` | 4 ms |
| Horizonte | `Humana::horizonte` | 2 turnos (el actual y el siguiente) |
| "Lo obvio" | `umbralObvia` | 700 puntos por encima de la segunda |

Nota: la cabecera de `ia_humana.h` todavía dice "30-150 ms y 16-170 simulaciones"; el código real (piso `simsMin` = 60 y
`msMin` = 80) manda. Los pisos los baja el entrenador de la red para generar datos más rápido.

Medido (de `docs/ia_red.md`, contenedor con 10 procesos a la vez): ~41-47 ms por decisión de media y ~150-175 ms de máximo;
`docs/ciclos_pruebas.md` (ciclo 2 de reglas oficiales): 14,2 ms de media y 101 ms de máximo. El presupuesto objetivo es ~150 ms.

### 2.5 Tiempo de pensar visible

`Cerebro::tiempoPensar` parte de `personalidad.ritmo` y lo ajusta por tipo de jugada (pasar de fase x0,5-0,6; colocar
trampas x0,55 porque salen seguidas; fusión y ritual x1,6; invocar con tributos x1,4), suma `0,12 * log2(opciones)`, duda
más antes de atacar a las boca abajo si hay miedo, 8 % de las veces se queda pensando 1-2,5 s más y lo multiplica por un
factor log-normal; el resultado queda entre 0,35 y 6 s. La IA humana lo multiplica por su `pausa`, descuenta lo que ya tardó
la búsqueda y tope de 3,0 s para el rival (el icono de Master Duel). Corre con el `dt` del juego, así que la opción de
velocidad de animación lo acorta. Detalle de ritmo: `docs/md_ritmo.md`.

---

## 3. Planes y mazos

### 3.1 Roles de las cartas (`datos/roles_cartas.txt`)

Cada carta tiene letras de rol que generó `roles_gen.py` leyendo su texto en inglés (más una lista a mano `MANO` con las
cartas que el texto no aclara). 882 cartas tienen roles.

| Letra | Rol | Letra | Rol |
|---|---|---|---|
| `q` | quema (daño o pérdida de LP fuera de batalla) | `x` | cambio de control |
| `b` | bloqueo (el rival no puede atacar o queda limitado) | `a` | victoria alternativa (Exodia, Final Countdown, Destiny Board) |
| `w` | muro (monstruo de DEF alta) | `g` | golpeador (ATK alto para su coste) |
| `d` | destruye o quita monstruos | `n` | negación |
| `t` | destruye magias/trampas | `r` | recuperación (vuelve del cementerio) |
| `c` | castigo (trampa que responde al ataque o a la invocación) | `m` | molino (mazo/mano del rival al cementerio) |
| `e` | enjambre (invocaciones especiales, fichas) | `o` | robo (robar, buscar) |
| `k` | combo (Polymerization, rituales, equipos, uniones) | | |

### 3.2 Los 9 planes (`ia_plan.h`, `PLANES`)

Cada plan define los roles que busca (principales valen 2 puntos de encaje, secundarios 1), cuánto cambia la personalidad
efectiva (`dAgr`, `dCaut`, `dFarol`, `dRiesgo`, `dPac`, `dUmbral`), cuántos monstruos lleva el mazo y un bono a cada ataque.

| Plan | Principales | Secundarios | Carácter | Monstruos | Bono de ataque |
|---|---|---|---|---|---|
| agresivo | g | k x e | agresividad +0,30; umbral -200 | 22 | +120 |
| control | d t | n r | paciencia +0,20; umbral +100 | 17 | 0 |
| bloqueo (+ quema) | b q w | a | agresividad -0,35; umbral +250 | 16 | -60 |
| enjambre | e | g r | agresividad +0,15 | 23 | +60 |
| combo | k | r o | paciencia +0,20 | 19 | +20 |
| castigo (trampas) | c | n w | farol +0,30; agresividad -0,20 | 17 | -30 |
| molino / robo | m o | w b | paciencia +0,30 | 17 | -40 |
| tempo (robar monstruos) | x | d g | agresividad +0,15; riesgo +0,15 | 19 | +80 |
| victoria alternativa | a | b o w | agresividad -0,40; umbral +300 | 17 | -100 |

(Los valores completos están en `PLANES[]`; arriba solo los más característicos.)

### 3.3 Cómo se elige el plan de un duelo

`prepararRival` (al elegir al rival) hace, en orden:

1. **¿Mazo de torneo?** `elegirTorneo`: si el rival tiene mazos de torneo (`Rival::torneo`), con probabilidad **40 %**
   (genéricos nivel 7-8), **30 %** (personajes) o **40 %** (personajes de nivel >= 7) saca uno de ellos **tal cual, sin
   reequilibrar**, y nunca el mismo dos duelos seguidos. Su plan: casi siempre el de la lista (`D|clave|plan|...`, peso x6), a veces otro que su
   mazo permita. Hay **21 mazos de torneo** (`datos/mazos_torneo.txt`, ver `docs/mazos_torneo.md`).
2. Si no, **sortea el plan** con pesos (`pesosPlanes`):
   `peso = AFINIDAD[personalidad][plan] * (0,25 + 4 * encaje)^(1 + 0,25 * max(0, dificultad))`, donde `encaje` es la suma
   de puntos de rol de su pool dividida por `2 * cartas`; luego se multiplica por:
   - **no repetir**: el último plan x0,08, el anterior x0,35, el tercero x0,7 (memoria por rival);
   - **contrarrestar al jugador** (`Lectura` x `confianza`; solo calculador, control, oscuro, paciente y tramposo lo
     hacen del todo, el resto x0,35): contra quien ataca siempre sube castigo y bloqueo; contra quien pone muros sube
     combo y tempo; contra el trampero baja agresivo y sube control; contra el farolero sube agresivo;
   - **dificultad baja** (< 0): castigo, control, bloqueo y alternativa pesan menos (hasta x0,4).
   - La victoria alternativa solo entra si el pool tiene 3 piezas de Exodia o Final Countdown / Destiny Board.
3. Arma el **mazo dinámico** para ese plan (3.5).

Durante el duelo: `revisarPlan` (al empezar cada turno propio) **cambia de plan** si el rival tiene <= 1500 LP y el plan no es
de pegar (rematar, hasta 3 cambios), o si tras >= 2 turnos el marcador (`LP/1000 + 0,6 x diferencia de cartas`) es < -2,5 y
peor que al empezar el plan (hasta 2 cambios); sortea otro con los mismos pesos y el juego avisa "¡Cambio de estrategia!".

### 3.4 Variación táctica (`Estilo`, humor, combos, horizonte)

- **Estilo por duelo** (`Estilo::alAzar`): volcar o guardar recursos, colocar trampas antes o después de la batalla (35 %),
  "trampa al revés" (campo vacío a propósito para invitar el ataque; solo castigo, bloqueo y alternativa), orden de
  invocar/activar y atrevimiento con ataques dudosos.
- **Humor por turno**: ruido de ±45 puntos por tipo de jugada, nuevo cada turno.
- **Combos** (`datos/combos.txt`, 43 líneas `C|clave|piezas|mínimo|plan[|torneo]`): una pieza es un id, `@rol`,
  `%subtipo` o `&raza`; el mínimo es cuántas piezas distintas hacen falta en mano + campo para que la IA lo busque; dan
  bono al jugar sus piezas y los mazos dinámicos los meten juntos. Con el campo `torneo`, solo dan bono al jugarse: no arman
  mazos dinámicos de otros rivales.
- **Horizonte de 2 turnos** (`Proyecto`): colocar el monstruo chico que será el sacrificio del grande de la mano, juntar
  materiales de fusión o ritual, retener la trampa clave para el ataque que importa, no gastar quitamonstruos en monstruos
  chicos si el plan es esperar, y no robar de más con el mazo casi vacío (Exodia, Biblioteca).

### 3.5 Mazos dinámicos (`mazoDinamico`)

Se arma desde el **pool** del rival (sus 3 mazos de `rivales.txt` / `personajes_rivales.txt`):

1. **Núcleo**: las cartas que están en todos sus mazos y las que solo él usa (su identidad).
2. **Paquete del plan**: cartas con el rol del plan, de su pool (pesan más) o de mazos de rivales de su nivel o menos (uno más
   con dificultad > 1,5), y los combos de ese plan que su pool o nivel permitan.
3. **Monstruos hasta el objetivo del plan**, luego magias y trampas, del pool por frecuencia y rol, distintos al mazo de la
   vez anterior.
4. **Arreglos** en orden, para que ninguno deshaga el anterior: **techo de ATK medio** (el de sus mazos de siempre + 60 +
   70 x dificultad: la dificultad por nivel no sube), rituales con su magia (o fuera), Toons con Toon World (o fuera),
   fusiones solo con todos sus materiales y con Polymerization (o Metamorphosis), y mínimo 40.

Resultado: siempre válido (40-42 cartas, tamaño 40 y un 20 % de las veces +1 o +2; máx. 3 copias; límites Goat; fusiones
con materiales; nunca cartas firma en genéricos). Si no hay `roles_cartas.txt` (`b.papeles` vacío), el rival usa su mazo de
siempre (`Base::mazoDe`) sin plan.

### 3.6 Cuántos mazos hay

| Qué | Cantidad | Archivo | Generador |
|---|---|---|---|
| Rivales genéricos | 40 (8 niveles x 5) x 3 variantes = **120 mazos** | `datos/rivales.txt` | `rivales_gen.py` (determinista) |
| Mazos de personajes | **141 variantes** (47 personajes; la 0 es "la suya" y sale el 60 % de las veces) | `datos/personajes_rivales.txt` | `personajes_gen.py` |
| Mazos de torneo | **21** (5 de Master Duel, 16 de la era Goat) | `datos/mazos_torneo.txt` | `mazos_torneo.py` (listas a mano) |
| Mazo dinámico | uno nuevo por duelo | (no se guarda; solo el último) | `mazoDinamico` |

`rivales_gen.py` y `personajes_gen.py` son deterministas (semillas CRC32): el mismo `cartas_ui.txt`, `cartas_classic.txt` y
datos de entrada dan siempre el mismo archivo. Los 54 monstruos que no se invocan de forma normal no entran (salvo Toons
con Toon World).

### 3.7 Memoria entre duelos y lectura del jugador

`variedad.txt` (junto a `progreso.txt`):

```
adaptable 1|0     humana 1|0     racha N     aprende 1|0
jugador trampas faroles ataques muros confianza duelos [limpia]
plan <rival> p1 p2 p3        (el más reciente primero)
mazo <rival> id id id ...    (el último)
```

- **Por rival**: sus últimos planes y su último mazo (para no repetirlos).
- **Del jugador** (`Lectura`, todo entre 0 y 1): `trampas` (activa de verdad lo que coloca), `faroles` (coloca y no activa),
  `ataques` (ataca siempre que puede), `muros` (pone monstruos en defensa), `limpia` (destruye las boca abajo del rival con
  MST, Plumero, Tormenta) y `confianza` (0 = no se sabe nada). Solo sale de **sucesos de la mesa**, nunca de su mano ni de lo
  que tiene boca abajo. Se mezcla con lo visto en el duelo en curso (`Observacion`); entre duelos decae un 12 % hacia lo
  neutro y aprende un 35 % del último.
- **Efecto**: `factorMiedo()` sube o baja el respeto a las cartas boca abajo del jugador (rango 0,25-2,2): contra un
  trampero ataca menos a ciegas y limpia antes; a un farolero lo ataca sin miedo. En la capa humana también sesga qué cree
  que hay boca abajo al determinizar.

### 3.8 Dificultad adaptable

`Memoria::racha` cuenta victorias seguidas del jugador (+) o derrotas seguidas (-); abandonar cuenta como derrota según `docs/rivales.md` (no verificado en el código). La
**dificultad** que usa la IA es `racha` acotada a **-1..+4** (apagada en Opciones: 0):

- `factorRuido = clamp(1 - k * dificultad, 0,45, 1,1)` con k = 0,14 si la dificultad es positiva y 0,08 si negativa. Con
  racha +4: ruido x0,45 (más certero); con -1: x1,08.
- Los planes se eligen con `(0,25 + 4 * encaje)^(1 + 0,25 * dificultad)` (más afinados al mazo), el paquete del mazo dinámico
  crece (`K = 7 + hasta(4) + dificultad` cartas), el techo de ATK medio sube `70 x dificultad` y se prefieren cartas que
  usan más rivales; con dificultad negativa, menos planes de castigo/control/bloqueo.
- El presupuesto de simulaciones sube un 6 % por punto y los errores creíbles se mueven entre x0,8 y x1,1.
- **Nunca** cambia el nivel del rival ni su pool de cartas; el jugador que pierde seguido baja como mucho un punto ("nunca a
  modo principiante").

Nota: los comentarios viejos de `ia_plan.h` y `docs/rivales.md` hablan de "+4..-3"; el código actual es -1..+4.

---

## 4. Decisiones de combate y de respuesta

### 4.1 Qué considera el Cerebro en cada fase

`Cerebro::candidatos` genera, según la fase, `principal()` (invocar, colocar, voltear, cambiar posición, fusión, ritual,
colocar magias/trampas, descartar), `batalla()` (atacar) y `activaciones()` (efectos y magias; las de la mano en batalla
-60 salvo urgentes), filtra con el motor de efectos (Cold Wave, Chain Energy, invocaciones prohibidas) y añade "pasar de
fase" con 0 puntos como referencia. Las jugadas que no superan esa referencia no se hacen.

### 4.2 Atacar (`ataquesDe`)

Para cada monstruo que puede atacar:

- **Ataque directo** (sin monstruos rivales, o por efecto): `200 + ATK x (0,7 + 0,5 agresividad) - 0,8 x miedo` (+15 % del
  ATK si el rival tiene <= 2000 LP); si es **letal** vale 100000.
- **Contra un monstruo**: se calcula la ganancia `g` (cartas destruidas y daño hecho menos recibido, "como FUN_005b0f40 del
  original"): ganar vale `450 (CARTA) + 0,5 x ATK/DEF del objetivo + diferencia`; empatar 1x1 es `CARTA + ... - valor
  propio + 100 x riesgo`; perder es `-(valor propio + diferencia)`. Luego `puntos = g - umbral - 0,5 x miedo + 100 x
  agresividad`, y `-600` si `g < -200` salvo el temerario.
- **Monstruos boca abajo que no ha visto**: no conoce sus números; usa una **estimación** (ATK 1100; DEF 1300 + 30 x nivel,
  `verAtk`/`verDef`), suma un pequeño bono por destruirlo (+60) y resta `250 x cautela - 120 x riesgo` ("no se sabe qué hay
  debajo"). El log muestra la estimación y, entre paréntesis, el valor real.
- **Regla nueva (rama `classic-ps5-ia-waboku`)**: contra un monstruo **boca arriba y más fuerte** (ATK/DEF ya con campo,
  equipos y efectos: `atkM`/`defM`, en ataque su ATK, en defensa su DEF) el ataque recibe `-5000` y nunca se elige, **salvo
  que perder tenga razón**: el atacante tiene un efecto al ir al cementerio (`D_AL_CEMENTERIO`). Así la IA ya no suicida
  un monstruo "por si acaso" contra algo que ve más fuerte.
- **`miedo()`** = `cautela x (cartas M/T boca abajo del jugador) x 260 x factorMiedo x 0,72^(ataques sin castigo)`: sube con
  las cartas boca abajo y baja cada vez que ataca y no pasa nada; vuelve al máximo cuando el jugador responde a un ataque
  (Mirror Force, etc.). Si el miedo supera 300, tarda más en atacar.
- **Repetición del ataque** (regla oficial, `elegirRepeticion`): si el número de monstruos cambia tras declarar, vuelve a
  puntuar los objetivos con la misma fórmula y "no atacar" como referencia.

### 4.3 Colocar, invocar y guardar

- Invocar un monstruo en ataque o colocarlo boca abajo en defensa depende de la personalidad, de la **amenaza** del rival
  (`amenaza()`: su mayor ATK visible o 1200 si es boca abajo), del orgullo (en la capa humana) y del plan.
- **Colocar magias/trampas**: una trampa con efecto vale +260; una magia rápida +60; una magia normal -220 ("mejor
  activarla cuando toque"). `Estilo.mtAntes` y `cebo` ajustan cuándo.
- **Descartar** al final del turno (mano por encima de `limiteMano`, 6 normalmente): descarta lo que menos vale, con `valorGuardar` (piezas del plan o de un combo).
- Faroles y trampas guardadas: capa humana (4.5).

### 4.4 Responder (`decidirPregunta` y `candidatosRespuesta`)

En una ventana de respuesta, para cada carta activable el valor `valorActivacion` (puntos ≈ ATK, de `DefCarta::valor()`):

- **Clásica**: `v += azar.normal() x ruido x 0,25`; se activa la mejor si `v > umbral`, con
  `umbral = 120 + 120 x paciencia - 80 x riesgo + ajusteRespuesta`. La paciente espera más; la temeraria gasta antes.
- **Humana**: las mismas puntuaciones (`valor - umbral`) son las candidatas, con "pasar" a 0, y se imaginan igual.
- `ajusteRespuesta` (plan): el castigo activa a gusto (-100); con el mazo casi vacío no se roba de más (hasta +1800); y una
  trampa de castigo/negación se **retiene** (+250 x paciencia) si el ataque es chico (< 1400), hay un solo atacante y sobran
  LP, "para el que importa".
- **Regla nueva `respuestaRedundante`** (rama `classic-ps5-ia-waboku`): se descartan como candidatas las respuestas que no
  sirven:
  - una **protección de ataque** (Waboku, Negate Attack, Threatening Roar) o una **anulación** (Mirror Force, Magic Cylinder,
    Negate Attack, Sakuretsu Armor, Draining Shield, Dark Mirror Force, Threatening Roar) si el ataque ya fue negado o su
    atacante ya no está;
  - si algo **propio en la cadena** ya lo neutraliza (otra anulación; o una protección ante una protección): **no gasta dos
    Waboku ni dos negaciones equivalentes**;
  - una protección si ya hay una resuelta este turno (`sinDannoBatalla`);
  - una protección contra un ataque **inocuo** (`ataqueInocuo`: no haría daño ni destruiría nada y no hay otro atacante
    que proteger).
- **Preguntar antes de activar** (opción del jugador) afecta solo al jugador; la IA siempre decide.
- **Elegir** (objetivos, sacrificios): cada carta tiene `elegirIA` (en general `iaMejor`/`iaPeor`); sus objetivos son lo del
  rival que más vale, lo propio solo si no queda otra, y del cementerio lo más fuerte.

### 4.5 Capa humana: faroles, lectura y errores (`ajustarHumano`)

Sobre las candidatas ya puntuadas:

- **Farol**: colocar boca abajo una magia que no es trampa suma `110` si no tiene trampas puestas, `+120` si tiene el campo
  vacío o el rival tiene un monstruo más fuerte, `+220 x (ataques del jugador - 0,4) x k` si ataca con todo, `+160 x limpia x
  k` si limpia las boca abajo; `-250` con 3 o más ya colocadas; todo x `farolH` (mezcla del farol de la personalidad y del
  rasgo). Con un 35 % de probabilidad dice una frase de farol.
- **Trampas contra el que ataca siempre**: colocar una trampa suma `300 x (ataques - 0,5) x k` si ataca más de 0,55.
- **Orgullo**: colocar boca abajo un monstruo de ATK >= 1800 resta `250 x orgullo`; invocar uno de >= 2000 suma `120 x
  orgullo`; pasarlo a defensa resta `150 x orgullo`. Kaiba (orgullo 1,0) nunca esconde a su monstruo fuerte.
- **Guardar la trampa para el ataque grande**: si el jugador suele atacar con todo (> 0,55), el atacante actual es chico
  y tras él viene uno mayor en más de +300 de ATK, activar la trampa ahora resta `450 x (ataques - 0,5) x k` (contador
  `guardadas`).
- **Callar una trampa marginal**: con LP > 3500 y puntos entre 0 y 220, 50 % x `farolH` de restar 260 (contador `ocultas`).
- **Errores creíbles** (`probError`): no son ruido al azar sino fallos que haría una persona: **no ver un combo raro**
  (fusión, ritual, Toon o efecto de monstruo se quita de las candidatas con probabilidad `0,9 x pErr`), sobrevalorar el
  ataque yendo ganando, arriesgar yendo perdiendo o quedarse con la segunda si estaba cerca. `pErr = base(nivel) x adaptable
  x (1 + 0,12 x (candidatas cercanas - 2)) x (1 + 0,5 x (error del personaje - 1))`, entre 0,3 % y 4 %.
- La **determinización** (`determinizar`) adivina la mano y el orden del mazo del jugador y qué hay boca abajo con un sesgo
  según su `Lectura` (trampero: más probable trampa; farolero: menos) y baja la sospecha con cada ataque sin castigo.

### 4.6 La bitácora: cómo depurar una decisión

La bitácora (`duelo_log.txt`, `src/duelo_log.h`, ver `docs/duelo_log.md`) se engancha a la IA con cuatro ganchos opcionales
en `ia.h` (`ia::ganchosLog`: `suceso`, `jugada`, `candidatas`, `decision`) que solo **leen** el duelo: no gastan azar ni
cambian estado, y las copias que imagina la IA humana se ignoran. Antes de cada jugada del CPU escribe:

```
[CPU plan] <plan> | personalidad <p>, nivel <n>              (la primera vez y cada vez que cambia)
[CPU decide] <qué hace y por qué> (puntos N) | otras: <las 3 mejores alternativas con sus puntos>
```

Ejemplos reales de formato:

```
[CPU decide] ataca con Cebra Oscura: ATK 1800 vs ATK 1250 -> ganaria: ACEPTADO (puntos 1466) | otras: Pasar de fase (0)
[CPU decide] NO ataca, pasa de fase: Hacha del Tigre -> ?: ATK 1300 vs DEF 1450 (boca abajo: valor estimado; real DEF 1400) -> perderia 150 LP: RECHAZADO | otras: ...
[CPU decide] activa Waboku (puntos 680) | responde al ataque de X (ATK 1900), dano estimado 700 | sin proteccion en cadena
[CPU decide] no responde: la mejor activacion posible (Mirror Force, 90 puntos) no supera el umbral
[CPU decide] Invocar X: mejor puntuacion (1666), ATK 1800 vs mayor amenaza rival 1250, en ataque | otras: ...
```

- Los **puntos** son los de la lista de candidatas, **sin el ruido** de decisión.
- Si la jugada elegida puntuaba menos que otra, la línea termina con
  **`[elegida pese a puntuar menos: el ruido o la busqueda de la IA]`**: la búsqueda (capa 3) o el ruido (capa 1) cambiaron el
  orden. Si esto aparece seguido en una decisión que parece mala, mirar la sección 6.
- Cada ataque descartado se enumera ("RECHAZADO") con la cuenta que hizo.

**Receta para depurar "¿por qué hizo eso?"**

1. Activar Opciones > Bitácora del duelo (por defecto Sí) y jugar el duelo.
2. Leerla desde el PC: `python -I remake-ps5/leer_log_duelo.py una -n 120` (o `seguir --filtro "CPU decide"` mientras se
   juega; solo lectura por FTP; ver `docs/duelo_log.md`).
3. Buscar la línea `[CPU decide]` de esa jugada: ¿qué puntuaciones tenía cada alternativa? ¿Qué plan (`[CPU plan]`) y qué
   personalidad? Una jugada con puntos bajos que se eligió = ruido o búsqueda.
4. Reproducirlo sin consola: `prueba_duelo_log.cpp` y `prueba_ia_humana.cpp` corren duelos IA contra IA que escriben la
   misma bitácora; con `Humana::depura` (`DEPURA=1/2/3`: sin errores creíbles / sin faroles ni lectura / ninguna) se aísla qué
   capa causa la jugada.

---

## 5. Red de valor y aprendizaje

(Detalle completo y tablas: `remake-ps5/docs/ia_red.md`; aquí el resumen verificado.)

### 5.1 La red

- **Qué es**: una red de **valor**: mira el duelo desde un lado y da la probabilidad de que ese lado gane. No elige jugadas.
- **Arquitectura**: MLP **85 -> 64 -> 32 -> 1** (ReLU y sigmoide), 7617 pesos (~30 KB en memoria, ~76-80 KB en texto), inferencia
  en C++ puro con arreglos en la pila: ~6 µs por evaluación. Archivo `datos/ia_red.txt` (`red 1 85 64 32`, `escala`, `mezcla`,
  `generacion`, `origen`, `pesos`, `fin <FNV-1a>`: si la suma no cierra, no vale y se juega sin red).
- **85 rasgos**: 33 por lado (LP, tamaños de zonas, monstruos y su ATK/DEF, magias/trampas, mano, papeles de
  `roles_cartas.txt` en mano y campo, amenaza) x 2 lados + 19 globales (turno, fase, plan, ventaja de LP y de cartas).
- **Mezcla**: `valor del final = (1 - mezcla) x evaluarHumana(plan, personalidad) + mezcla x escala x logit(red)` con
  `mezcla` = 0,5 (en el archivo) y `escala` calibrada para que el logit pese como el plan. La red solo ve la **copia
  determinizada**: no hace trampa.
- **Sin red** (archivo ausente o corrupto, u opción "IA que aprende: No") la IA es exactamente la de antes.

### 5.2 Entrenamiento por autojuego

```bash
g++ -std=c++17 -O3 -march=native -I remake-ps5/src remake-ps5/src/pruebas/entrenar_ia_red.cpp -o /tmp/entrenar_ia_red
HILOS=10 DUELOS_CLASICA=20000 DUELOS_HUMANA=1500 PARES=150 \
  /tmp/entrenar_ia_red /compartido/classic-assets/ui/ datos/ datos/ia_red.txt 3 > entrenar.log 2>&1
```

- Procesos con `fork` (no hilos: el motor tiene banderas estáticas), < 1 GB de RAM.
- Duelos con los rivales reales (`rivales.txt` + `personajes_rivales.txt`), dos rivales del mismo nivel, mazos dinámicos, planes
  y una lectura del jugador al azar. Se graba cada posición al empezar cada turno desde los dos lados, con el resultado.
- Generación 0: duelos de la IA **clásica** (rápidos, volumen) y de la humana sin red; las siguientes, de la humana con la
  campeona (presupuesto bajo: `ESCALA_DATOS`, `SIMS_MIN_DATOS`).
- Optimizador: Adam, entropía cruzada, L2 (1e-4), parada temprana con un 10 % apartado, búfer de hasta 900 mil posiciones.
- **Criterio de aceptación (partido)**: pares de duelos con los mismos mazos y semilla, cambiando de lado, presupuesto real.
  La generación 0 se queda si no es más débil que la humana sin red (**>= 50 %**); las siguientes solo si le ganan a la
  campeona **>= 55 %**.
- Medir sin entrenar: `PARTIDO_SOLO=<red.txt> CONTRA=ninguna|clasica|<red.txt> [MEZCLA_FORZADA=x] [HORIZONTE_RED=n]
  [PRIOR_RED=x] PARES=100`. Otras variables del entrenador: `LAMBDA`, `CONTRA_SINRED`, `DATOS_GUARDAR/DATOS_LEER/NO_SALTAR`,
  `ESCALA_A`, `OBVIA_RED`, `MEZCLA_PERSONAJE`.
- Cambiar los rasgos o la forma de la red **invalida** `datos/ia_red.txt`: hay que reentrenar. Y como la red cambia el
  juego de la IA dentro del motor, en línea hay que subir `VERSION_JUEGO`.

### 5.3 Resultados medidos (copiados de `docs/ia_red.md`; ±: intervalo del 95 %)

**Corrida 2 (la que se usa; 20000 duelos de la clásica + 1500 de la humana por generación):**

| Generación | Posiciones | Pérdida de validación | Acierto | Partido | Resultado |
|---|---|---|---|---|---|
| 0 | 803 mil | 0,496 | 75,4 % | contra la humana sin red: 49,7 % ±5,7 | no (pide >= 50 %) |
| 1 | 861 mil | 0,499 | 74,9 % | contra la humana sin red: 51,3 % ±5,7 | **campeona** (`datos/ia_red.txt`) |
| 2 | 900 mil | 0,481 | 75,5 % | contra la generación 1: 50,0 % ±5,7 | no (pide >= 55 %) |

**La campeona medida aparte (200 duelos cada uno):**

| Partido | % de victorias | ms por decisión (medio / máximo) |
|---|---|---|
| humana con red contra humana sin red | 51,5 % ±6,9 | 44,9 / 156,7 |
| humana con red contra la clásica | 64,0 % ±6,9 | 45,3 / 162,9 |
| humana sin red contra la clásica (referencia) | 66,0 % ±6,9 | 43,3 / 174,9 |

**Conclusión del propio documento** (honesta): la red predice el resultado mucho mejor que el azar (75 % de acierto,
pérdida ~0,50 contra 0,69) pero **no hace más fuerte a la IA humana en autojuego** (~50-52 % contra sin red, dentro del
ruido). Diagnósticos: con la hoja valiendo 0 baja a 38,3 % (la evaluación del final sí importa); con el doble de presupuesto
queda en 50,3 % (**la búsqueda está saturada**); el horizonte de 3-4 turnos empeora (45,7 %); el orden previo de candidatas
con la red empeora (42-47,5 %, queda apagado). En una segunda tanda (rasgos v2 de 97 entradas, red 128-64, TD(λ), `OBVIA_RED`)
ninguna variante llegó al 55 % (la mejor ~53 % ±4). Lo que limita a la IA es **la política de los rollouts (el Cerebro) y la
determinización**, no la evaluación ni el tiempo. Lo que sí da la red es la base para adaptarse al jugador (5.4).

### 5.4 Aprender del jugador (solo en el juego real, `aprendizaje.h`)

- **Grabar**: al empezar cada turno de cada duelo contra la IA se guardan los 85 rasgos desde los dos lados y, al final, el
  resultado. `ia_duelos.bin` (carpeta de datos: en la PS5 `/data/yugioh_classic/`) guarda los **últimos 40 duelos**
  (hasta 160 posiciones cada uno, ~100-350 KB), con guardado atómico y suma de control (si se corrompe, se empieza de cero).
  Abandonar (ganador -1) no se guarda.
- **Ajuste fino**: al terminar un duelo queda pendiente; `avanzarDueloIA` lo corre **fuera del duelo** (resultado, menús) en
  rebanadas de ~1,5 ms por cuadro (preparar, entrenar, medir, guardar). Se exigen al menos 3 duelos con resultado. Siempre
  parte de la red **base** y ajusta **solo la última capa** (33 pesos, atados por L2 a la base): con pocos duelos, ajustar
  todo sobreajusta. Usa solo las posiciones vistas **desde el rival** (lo que evalúa la búsqueda); valida en 1 de cada 5 duelos
  y **solo se queda si mejora**; si no, vuelve la base.
- **Resultado**: la red ajustada va a `ia_red_jugador.txt` (~76 KB); si falta, está corrupta o es de otra forma que la base,
  se usa la base. En la prueba, con un jugador fuerte la pérdida en duelos nuevos baja de 0,43 a 0,38.
- **Opciones** (menú Opciones): **IA que aprende: Sí/No** (por defecto Sí; línea `aprende 1|0` de `variedad.txt`) y **Olvidar
  lo aprendido** (borra los duelos grabados y la red del jugador).
- Memoria temporal del ajuste < 2 MB; ninguna copia de la IA humana graba.

---

## 6. Cómo medir si la IA es buena, y cómo cambiarla

### 6.1 Las pruebas (todas sin pantalla; en `remake-ps5/src/pruebas/`)

Compilar y correr (contenedor `arch-ps5`, ver cabeceras de cada archivo):

```bash
g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_ia.cpp -o /tmp/prueba_ia
/tmp/prueba_ia <ui/> remake-ps5/datos/rivales.txt [duelos]
g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_variedad.cpp -o /tmp/prueba_variedad
/tmp/prueba_variedad <ui/> remake-ps5/datos/ [duelos por rival]
g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_ia_humana.cpp -o /tmp/prueba_ia_humana
/tmp/prueba_ia_humana <ui/> remake-ps5/datos/ [pares de duelos] [escala del presupuesto]
```

(`<ui/>` = la carpeta con `cartas_ui.txt` y `cartas_classic.txt`.) El ciclo completo (`ciclo_completo.sh`) las corre todas.

| Prueba | Qué mide | Criterios de falla (verificados en el código) |
|---|---|---|
| `prueba_ia.cpp` | los 120 mazos son válidos (40-60, copias, banlist); duelos IA contra IA (800 por defecto) | jugada ilegal; duelo que no termina; los resultados no varían; el nivel alto no le gana al bajo |
| `prueba_variedad.cpp` | (1) mazos dinámicos válidos de todos los rivales; (2) mismo rival N veces seguidas: **entropía de planes**, planes distintos, repetición del plan seguido, **% de cartas distintas entre mazos consecutivos**, primeras jugadas distintas, cambios de plan a mitad de duelo; (3) la dificultad por nivel se mantiene (el de dif >= 3 gana >= 60 %); (4) lectura: contra el farolero ataca más a ciegas que contra el trampero; (5) adaptable: tras rachas el ruido baja o sube y apagada vale 0; con racha +4 gana al menos 10 puntos más que con -3; (6) la memoria se guarda y se lee igual | entropía media < 1,5 bits; cartas distintas medias < 25 %; repite plan seguido demasiado (> 20 %); duelos sin terminar > 2 %; jugadas ilegales; la lectura o la adaptable no responden |
| `prueba_ia_humana.cpp` | (1) humana contra clásica con los **mismos mazos** (cada IA juega cada mazo y cada lado; % de victorias, por nivel); (2) presupuesto: ms por decisión (todas, las buscadas, el máximo) y simulaciones; (3) diversidad (misma métrica que variedad, clásica contra humana); (4) capa humana: faroles, trampas calladas y guardadas, **errores creíbles por nivel** | jugadas ilegales, duelos sin terminar, una decisión que pasa de 175 ms |
| `prueba_ia_red.cpp` | casos `CU-BE-IA-RED-*`: carga y evaluación < 1 ms, archivos corruptos descartados, grabación con tope de 40, ajuste fino que mejora y no traba, opción apagada = como antes, **fuerza (con red no es más débil y le gana a la clásica)**, tiempo, diversidad | cualquiera de los casos |
| `entrenar_ia_red.cpp` | no es una prueba: el entrenador y el partido de aceptación | (5.2) |

**Cifras registradas en los docs** (no re-medidas al escribir esto): la humana le gana a la clásica el **68,5 %** (137-63 en
200 duelos, 19-oct-2026; por nivel 58-73 %, la peor en el nivel 7), ~35 ms de media por decisión (`docs/rivales.md`); el 27 de 40 (67,5 %) del ciclo 2 de la rama de reglas oficiales; con red, 64 % contra la clásica. Variedad: en
una prueba, 800 duelos dieron 363 resultados distintos.

**Métricas de variedad** (qué significan): *entropía de planes* = incertidumbre (en bits) de qué plan elegirá el mismo rival
duelo tras duelo (máx. log2 9 = 3,17); *planes distintos*; *% de cartas distintas entre mazos seguidos* (cuánto cambia el
mazo del mismo rival); *aperturas distintas* (los dos primeros turnos propios); *cambios de plan*.

### 6.2 Cómo hacer la IA más difícil o más divertida (palancas concretas)

Ordenadas de menos a más invasivas. Todas requieren correr el ciclo de pruebas; las que tocan el motor o la IA, subir
`VERSION_JUEGO`.

| Palanca | Dónde | Efecto |
|---|---|---|
| Nivel de un rival o personaje | `rivales_gen.py` (lista de rivales) / `personajes_datos.py` | cambia su mazo (techo de ATK, tributos), ruido, candidatas y presupuesto |
| **Techo de ATK** y tributos por nivel | `rivales_gen.py` (tabla "por nivel (1..8): techo de ATK de los nivel 1-4, techo del ATK medio del mazo"; ver `docs/rivales.md`) | la palanca principal: sin efectos el duelo se decide por ATK/DEF |
| **Ruido** | `Cerebro::ruido()`: `520 - 62 x (nivel-1)` | más bajo = más certero; afecta a la clásica y a la política de los rollouts |
| **Dificultad adaptable** | Opciones; `Memoria::dificultad()`/`factorRuido()` | rango -1..+4: subir el tope o la pendiente (0,14) la vuelve más dura con quien gana |
| **Errores creíbles** | `Humana::probError` (base 0,015) y `Rasgos::error` | menos = más implacable; más = más "humana" |
| **Presupuesto** | `Humana::escala`, `msTope`, `msMin`, `simsMin`, `simsPresupuesto` | más simulaciones por decisión; ojo: la búsqueda está saturada (el doble no mejora) y el tope práctico es ~150 ms |
| **Horizonte y candidatas** | `Humana::horizonte` (2), `K = min(7, 3 + nivel/2)` | más turnos o candidatas cuestan tiempo; 3-4 turnos empeoró con red |
| **Rasgos del personaje** | `RASGOS_PERSONAJES` | riesgo, orgullo, lectura, farol, vista, pausa por personaje |
| **Personalidad** | `PERSONALIDADES` en `ia.h` | pesos de agresividad, cautela, farol, riesgo, paciencia, umbral |
| **Mazos** | `rivales_gen.py`, `personajes_datos.py`, `mazos_torneo.py` | mejores cartas = IA más fuerte sin tocar el cerebro; **más mazos de torneo** y más probabilidad (`probTorneo`: 30-40 %) |
| **Planes y combos** | `PLANES[]`, `AFINIDAD`, `combos.txt`, `roles_cartas.txt` | más variedad y más identidad por rival |
| **Red de valor** | 5.2 | ver los límites abajo |
| **Más diversión** | subir `Rasgos::farol`/`lectura`, el 8 % de pausas largas, `Estilo` más atrevido, más variantes de mazo por rival | un rival menos predecible aunque no sea más fuerte |

**Límites conocidos** (ahí es donde conviene actuar si se quiere más fuerza):

- **Política de simulación**: los rollouts usan el `Cerebro` (política con puntuaciones fijas, no una búsqueda); es lo que
  limita la fuerza (ver 5.3). Una red de **política** que imite a la búsqueda y reemplace al Cerebro en los rollouts, o más
  candidatas por decisión, o un modelo del jugador más fino en la determinización, son las ideas documentadas (**no
  implementadas**).
- **La búsqueda está saturada**: más presupuesto no sube el porcentaje.
- **Trampas guardadas para el ataque grande**: es una heurística (`guardadas`) que solo se dispara si el jugador ataca con todo
  (> 0,55), el atacante actual es chico y detrás viene uno mayor en > +300; en poca de las decisiones se cumple. La
  bitácora de las pruebas la cuenta (`prueba_ia_humana`: "trampas guardadas para el ataque grande"); **no verificado** cuántas veces
  ocurre en una partida típica ni su efecto en la tasa de victoria.
- **Errores creíbles**: la tasa es baja a propósito (~1,15-1,5 % por decisión imaginada según el nivel en el código;
  **no verificado** el valor medido de 0,8 % que se cita a veces: el código da 1,15-1,5 % antes de los multiplicadores y la
  prueba `prueba_ia_humana` imprime el porcentaje real "de las imaginadas"). Sin errores no hay IA "humana", y con
  más, la IA pierde fuerza.
- **Información oculta**: la IA nunca ve la mano del jugador; en el modo en línea la mano del rival tampoco se esconde aún
  (ver README, 10.3), pero eso es un tema del protocolo, no de la IA.
- Aproximaciones del motor de efectos (Fairy Guardian, Level Conversion Lab...) que la IA hereda (`docs/control_final.md`).

---

## 7. Cómo añadir un plan, un rol, un combo o un personaje

### 7.1 Un rol de carta

1. Editar `roles_gen.py`: añadir la letra a `PAPELES` (descripción), su regla de texto (en la función que lee el texto
   en inglés) y/o las cartas a mano en `MANO`.
2. Regenerar: `python -I roles_gen.py <cartas_ui_en.txt> <cartas_classic.txt> datos/roles_cartas.txt`.
3. Usar la letra en algún plan (`principales`/`secundarios`), combo (`@letra`) o en el cálculo de la red
   (`rasgosRed` de `ia_red.h` cuenta algunos roles de la mano y del campo: **cambiar qué roles ve la red cambia las 85
   entradas y obliga a reentrenar**).
4. Pruebas: `prueba_variedad` (los mazos siguen válidos y variados).

Para corregir un rol de **una** carta: añadirla a `MANO` (gana a la lectura del texto) y regenerar. No editar a mano el
`.txt` generado.

### 7.2 Un combo

1. Añadir una línea a `datos/combos.txt`: `C|clave|pieza pieza ...|mínimo|plan[|torneo]`. Pieza = id, `@rol`, `%subtipo` o
   `&raza`; `mínimo` = piezas distintas en mano + campo para buscarlo; `plan` = el plan que lo mete en el mazo.
2. Con `|torneo`, solo da bono al jugarlo (no arma los mazos dinámicos de otros rivales): úselo para combos de un mazo de
   torneo concreto.
3. `prueba_variedad` valida que el plan exista y que cada pieza sea una carta o un rol conocido.

### 7.3 Un plan nuevo

1. En `ia_plan.h`: añadir el valor al enum `PlanJuego` **antes** de `NUM_PLANES`, una fila en `PLANES[]` (clave, roles
   principales y secundarios, deltas de personalidad, monstruos del mazo, bono de ataque) y una **columna** en cada fila de
   `AFINIDAD[][]` (10 personalidades).
2. Si el plan necesita un comportamiento propio, añadirlo en `ajustarPlan` (qué jugadas suma o resta), `ajusteRespuesta`
   (cuándo retener o gastar una respuesta), `Estilo::alAzar` (si usa "cebo") y `evaluarHumana` (cómo valora el final la IA
   humana: agresivo valora el daño; control y bloqueo, la ventaja de cartas).
3. Si necesita un caso especial de encaje (como la victoria alternativa), añadirlo en `encaje`.
4. Subir el vector de rasgos de la red si el plan aparece en ellos (los 9 planes + "sin plan" son 10 entradas globales de las
   19: **añadir un plan cambia la forma de la red**; reentrenar).
5. Pruebas: `prueba_variedad` (planes distintos, entropía), `prueba_ia`, `prueba_ia_humana`; subir `VERSION_JUEGO`.

### 7.4 Un personaje con su mazo

1. `personajes_datos.py`: fila del personaje con `id`, nombres, saga, nivel, personalidad, entorno, lema y **variantes de
   mazo** (la 0 es "la suya"; nombres TCG en inglés, `"Nombre*2"` = 2 copias).
2. Correr `python -I personajes_gen.py --firma` y luego `python -I personajes_gen.py` (genera `personajes.txt` y
   `personajes_rivales.txt`; mete el núcleo, los materiales de fusión y la magia de ritual, quita lo que no se invoca de forma
   normal y completa hasta 40 con la época y el nivel).
3. Si quiere un **mazo de torneo** para él: lista en `mazos_torneo.py` y una línea `A|<id>|<clave>` (ver `docs/mazos_torneo.md`).
4. Si quiere **rasgos propios**, añadir una fila a `RASGOS_PERSONAJES` en `ia_humana.h` (clave = id del personaje);
   si no, usa los de su personalidad.
5. Diálogos, fondo y retrato: `README.md`, sección 6.3.
6. Pruebas: `prueba_ia` valida los mazos (40-60, copias, banlist), `prueba_variedad` los mazos dinámicos de todos los
   rivales y `prueba_ia_humana` la capa humana.
