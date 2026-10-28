# La red que aprende (paso 3 de la IA humana)

Rama `classic-ia-red`. Código: `src/ia_red.h` (rasgos, red, entrenamiento), `src/aprendizaje.h` (grabar los duelos del
jugador y el ajuste fino), `src/ia_humana.h` (la búsqueda la usa), `src/pruebas/entrenar_ia_red.cpp` (autojuego) y
`src/pruebas/prueba_ia_red.cpp` (casos `CU-BE-IA-RED-*`). Pesos: `datos/ia_red.txt` (llega al título en `ui/`).

## Diseño

La IA humana (pasos 1 y 2) imagina cada decisión importante con Monte Carlo determinizado: juega cada candidata en una
copia del duelo, sigue 1-2 turnos con la política de siempre y **evalúa el final** con `evaluarHumana` (el plan del
duelo, el orgullo: la personalidad). El paso 3 cambia solo esa evaluación:

```
valor del final = (1 - mezcla) * evaluarHumana(...)  +  mezcla * escala * logit(red)
```

- La red es de **valor**: mira el duelo desde un lado y da la probabilidad de que ese lado gane. No elige jugadas.
- Lo demás de la IA humana queda igual: candidatas, faroles, lectura del jugador, errores creíbles, tiempo de pensar
  visible, presupuesto. La red no la vuelve perfecta ni predecible (los errores salen en `elegir`, después de la
  búsqueda; la diversidad se mide igual que antes).
- `mezcla` (0,5) y `escala` vienen en el archivo de pesos. `escala` la calibra el entrenador para que el logit de la red
  tenga el mismo desvío que `evaluarHumana` (las dos partes pesan parecido).
- La red solo ve la **copia determinizada** de la búsqueda (lo oculto del jugador, adivinado): no hace trampa.
- Sin red (archivo ausente o corrupto, u opción "IA que aprende: No") la IA es exactamente la de antes: `Humana::red`
  queda en `nullptr` y no se toca ningún camino.

### Rasgos de entrada (85, todos en 0..~2)

Por cada lado (primero "yo", después el otro), 33:

| # | rasgo |
|---|---|
| 0-2 | LP/8000, min(LP,3000)/3000, min(LP,1000)/1000 |
| 3-7 | mano/6, mazo/40, cementerio/20, desterradas/10, extra/15 |
| 8-15 | monstruos/5, boca abajo/5, en ataque/5, en defensa boca arriba/5, los dos ATK más altos/3000, suma del ATK en ataque/8000, DEF más alta/3000 |
| 16-18 | magias/trampas boca abajo/5, boca arriba/5, magia de campo |
| 19-23 | mano: monstruos, magias, trampas (/6), ATK más alto/3000, de nivel ≤ 4 (/6) |
| 24-31 | papeles (`roles_cartas.txt`) en la mano y lo colocado: quita monstruos (d), quita magias/trampas (t), castigo (c), combo/enjambre (k, e), quema (q), victoria alternativa (a, /5), robo (o), bloqueo/muro (b, w) |
| 32 | amenaza: ATK en ataque / LP del otro |

Y 19 globales: de quién es el turno, la fase (5), turno/40, el **plan** del duelo (9 + "sin plan") y la ventaja de LP
y de cartas (centradas en 0,5).

### Arquitectura

MLP 85 → 64 → 32 → 1 (ReLU, sigmoide). 7.617 pesos (30 KB en memoria; ~80 KB en texto). Inferencia en C++ puro con
arreglos en la pila: **~6 µs** por evaluación (rasgos + red, medido en el contenedor), unas 25 mil evaluaciones por
decisión imaginada como mucho → muy por debajo de la rebanada de 4 ms.

Archivo de texto (`red 1 85 64 32`, `escala`, `mezcla`, `generacion`, `origen`, `pesos` y al final `fin <FNV-1a>`): si
la suma no cierra, el archivo no vale.

## Entrenamiento por autojuego (PC, contenedor)

```
g++ -std=c++17 -O3 -march=native -I remake-ps5/src remake-ps5/src/pruebas/entrenar_ia_red.cpp -o /tmp/entrenar_ia_red
docker exec -d arch-ps5 bash -c 'cd .../remake-ps5 && HILOS=10 DUELOS_CLASICA=20000 DUELOS_HUMANA=1500 PARES=150 \
  /tmp/entrenar_ia_red /compartido/classic-assets/ui/ datos/ datos/ia_red.txt 3 > entrenar.log 2>&1'
```

- Procesos con `fork` (no hilos: el motor tiene banderas estáticas de reentrada, `motorGaap`/`motorEstado`).
- Duelos con los rivales reales (`rivales.txt` + `personajes_rivales.txt`), dos rivales del mismo nivel (1..8 por
  turnos), mazos dinámicos y planes de `ia_plan.h`, y una lectura del jugador al azar (variedad).
- Se graba cada posición al empezar cada turno, desde los dos lados, con el resultado. Generación 0: duelos de la IA
  clásica (rápidos: volumen) y de la humana sin red; las siguientes, de la humana con la campeona (presupuesto bajo:
  `ESCALA_DATOS`, `SIMS_MIN_DATOS`).
- Adam, entropía cruzada, L2, parada temprana con un 10 % apartado; búfer de hasta 900 mil posiciones.
- **Partido**: pares de duelos con los mismos mazos y semilla, cambiando de lado, presupuesto real (escala 1). La
  generación 0 se queda si no es más débil que la humana sin red (≥ 50 %); las siguientes, si le ganan a la campeona
  ≥ 55 %.
- Medir sin entrenar: `PARTIDO_SOLO=<red.txt> CONTRA=ninguna|clasica|<red.txt> [MEZCLA_FORZADA=x] [HORIZONTE_RED=n]
  [PRIOR_RED=x] PARES=100`.
- RAM: < 1 GB (10 procesos de ~60 MB y el búfer).

## Resultados medidos

Todos en el contenedor (10 procesos a la vez: los ms por decisión salen más altos que en un solo proceso). Partido =
pares con los mismos mazos y semilla, cambiando de lado; ±: intervalo del 95 %.

**Corrida 1** (4000 duelos de la clásica + 1200 de la humana por generación; logs en `ciclos_logs/ia_red/`):

| generación | posiciones | pérdida de validación | acierto | contra la humana sin red (300 duelos) |
|---|---|---|---|---|
| 0 | 195 mil | 0,525 | 72,5 % | 50,0 % ±5,7 |
| 1 | 243 mil | 0,515 | 73,3 % | 51,8 % ±5,7 |
| 2 | 291 mil | 0,512 | 73,5 % | 52,0 % ±5,7 |

**Barrido** con la candidata 2 de la corrida 1 (200 duelos cada uno, contra la humana sin red):

| variante | % | ms por decisión |
|---|---|---|
| mezcla 0,5 (la elegida) | 52,0 ±5,7 (300 duelos) | 41 |
| mezcla 1,0 (solo la red, sin el plan) | 50,0 ±6,9 | 44 |
| mezcla 0,25 | 51,5 ±6,9 | 46 |
| mezcla 0,5 y horizonte 1 turno | 47,5 ±6,9 | 31 |
| mezcla 1,0 y horizonte 1 turno | 43,5 ±6,9 | 29 |
| orden previo de las candidatas con la red (`PRIOR_RED` 1,0) | 47,5 ±6,9 | 47 |
| solo el orden previo (mezcla 0, `PRIOR_RED` 1,0) | 42,0 ±6,9 | 49 |
| orden previo suave (`PRIOR_RED` 0,4) | 47,0 ±6,9 | 48 |

El orden previo con la red (`Humana::priorRed`, `ordenarConRed`) empeora: queda en el código apagado (0) y
documentado. Acortar el horizonte con la red ahorra tiempo pero pierde fuerza.

**Corrida 2** (la que se usa; 20000 duelos de la clásica + 1500 de la humana por generación, L2 1e-4):

| generación | posiciones | pérdida de validación | acierto | partido | resultado |
|---|---|---|---|---|---|
| 0 | 803 mil | 0,496 | 75,4 % | contra la humana sin red: 49,7 % ±5,7 | no (pide ≥ 50 %) |
| 1 | 861 mil | 0,499 | 74,9 % | contra la humana sin red: 51,3 % ±5,7 | **campeona** (`datos/ia_red.txt`) |
| 2 | 900 mil | 0,481 | 75,5 % | contra la generación 1: 50,0 % ±5,7 | no (pide ≥ 55 %) |

**La campeona (generación 1), medida aparte** (200 duelos cada uno, otra semilla):

| partido | % de victorias | ms por decisión (medio / máximo) |
|---|---|---|
| humana con red contra humana sin red | 51,5 % ±6,9 | 44,9 / 156,7 |
| humana con red contra la clásica | 64,0 % ±6,9 | 45,3 / 162,9 |
| humana sin red contra la clásica (referencia) | 66,0 % ±6,9 | 43,3 / 174,9 |

**Conclusión honesta**: la red predice el resultado mejor que nada (75 % de acierto, pérdida 0,50 frente a 0,69 del
azar), pero **no hace más fuerte a la IA humana** en autojuego: con red o sin red queda en ~50-52 % (dentro del ruido),
y contra la clásica igual que antes (64-66 %). Es decir: la evaluación del final de las simulaciones no es lo que limita
a la búsqueda (con solo la red, sin el plan, también da 50 %). Lo que sí da la red es la base para **adaptarse al
jugador** (abajo): el ajuste fino aprende cuánto vale cada posición contra ESE jugador (en la prueba, con un jugador
fuerte la pérdida en duelos nuevos baja de 0,43 a 0,38). Diversidad, tiempo y presupuesto quedan como antes
(`prueba_ia_red`). Ninguna generación posterior le ganó a la campeona por ≥ 55 %.

### Segunda tanda: entrenar más (objetivo ≥ 55 % contra la humana sin red, 400+ duelos)

Herramientas nuevas del entrenador (`entrenar_ia_red.cpp`): `LAMBDA` (objetivo TD(λ): cada posición aprende de lo que
la red cree de la siguiente del mismo lado, y al final del resultado), validación por duelos enteros, `CONTRA_SINRED`
(cada candidata contra la humana sin red; se queda la mejor), `DATOS_GUARDAR`/`DATOS_LEER`/`NO_SALTAR` (reusar las
muestras entre experimentos), `ESCALA_A` (presupuesto distinto para la candidata), `OBVIA_RED` (con red, el umbral de
"lo obvio": imagina más decisiones), `MEZCLA_PERSONAJE` (mezcla según la lectura del personaje), `PARTIDO_SOLO=azar|cero`
(diagnóstico) y cuánto predice la evaluación del plan sola. En `ia_humana.h` quedan `umbralObviaRed` y
`mezclaPersonaje` (apagados: el juego se comporta igual).

Rasgos **v2** probados (en una copia aparte, 97 entradas): por lado, monstruos y ATK más alto en el cementerio,
monstruos de efecto boca arriba, monstruos de nivel ≥ 5 en la mano (amenazas), cartas de recuperación en la mano y
monstruos que todavía pueden atacar.

Diagnósticos (300 duelos cada uno, contra la humana sin red):

| prueba | % | qué dice |
|---|---|---|
| la hoja vale 0 siempre (solo cuentan los finales de verdad) | 38,3 ±5,7 | la evaluación del final SÍ importa |
| la hoja con una red al azar | 40,0 ±5,7 | |
| el doble de presupuesto (sin red) | 50,3 ±5,7 | la búsqueda está **saturada**: más simulaciones no la hacen más fuerte |
| la red con horizonte 3 / 4 turnos | 45,7 / 45,7 ±5,7 | peor (y pasa de 150 ms: 208 ms) |
| pérdida de la evaluación del plan como predictor (logística) | 0,594 | la red predice mucho mejor (0,50-0,52) |

Experimentos (400 duelos cada uno, o 600 con ±4,0):

| experimento | % contra la humana sin red | ms medio / máximo |
|---|---|---|
| A: rasgos v2 + TD(0,7), 20 mil + 2500 duelos | 48,5 ±4,9 | 46,6 / 162 |
| B: v2, solo el resultado (λ = 1), 64-32 | 51,2 ±4,9 | 45,6 / 149 |
| C: v2, TD(0,3) | 51,0 ±4,9 | 45,8 / 149 |
| D: v2, TD(0,7), red 128-64 | 52,0 ±4,9 | 46,6 / 150 |
| F: v2, TD(0,7), 64-32 | 51,8 ±4,9 | 44,3 / 152 |
| D con mezcla 0,7 / 0,8 / 0,9 / 1,0 | 49,7 / 53,0 (y 51,7 repetido) / 52,3 / 51,3 | 47-49 / 150-158 |
| D con `OBVIA_RED` 1500 (imagina más decisiones) | 54,4 ±4,9; repetido con otra semilla 50,5 ±4,0 | 52-54 / 150-157 |
| D con `OBVIA_RED` 1500 y mezcla 0,8 | 53,3 ±4,0 | 50,9 / 150 |
| control: `OBVIA_RED` 1500 con mezcla 0 (imaginar más, sin red) | 49,7 ±4,0 | 53,5 / 152 |
| D con mezcla por personaje (0,8 × lectura) | 50,8 ±4,0 | 47,5 / 153 |
| 3 generaciones más desde D (3000 duelos de la humana con red cada una, TD 0,7, mezcla 0,8) | 51,3 / 51,5 / 53,0 ±4,0 | 49-50 / 153-160 |
| orden previo de las candidatas con la red (primera tanda) | 42-47,5 | |

**Resultado**: ninguna variante llega al 55 % (la mejor, ~53 % ±4, dentro del ruido de las demás: la misma variante
repetida da de 51,7 a 54,4). Por qué: la evaluación del final importa (sin ella cae a 38 %), pero la del plan ya
alcanza; una red que predice mucho mejor el resultado (pérdida 0,50 contra 0,59) no cambia qué candidata gana en la
búsqueda, y la búsqueda está saturada (el doble de presupuesto no suma nada). Lo que limita a la IA humana es la
política de los rollouts (el Cerebro) y la determinización, no la evaluación ni el tiempo. Se queda la red de la primera
tanda (generación 1, 85 rasgos, 64-32, mezcla 0,5): no empeora nada (51-52 % contra sin red, 64 % contra la clásica,
misma variedad y tiempo) y es la que pasa el ciclo completo; las variantes v2/128-64 no ganan con claridad y cuestan
más tiempo por evaluación (~15 µs en vez de ~6).

Para seguir: una red de **política** que imite a la búsqueda (para los rollouts, en lugar del Cerebro) es lo que
podría subir la fuerza; también más candidatas por decisión o un modelo del jugador más fino en la determinización.

## Aprender del jugador en la PS5

- **Grabar** (`aprendizaje::Aprendiz`, en `duelo_juego.h`): al empezar cada turno de cada duelo contra la IA, los 85
  rasgos desde los dos lados (85 bytes + el lado), y al final el resultado. `/data/yugioh_classic/ia_duelos.bin`: los
  últimos 40 duelos (como mucho 160 posiciones cada uno, ~100-350 KB), guardado atómico (`guardado.h`) con suma de
  control; si se corrompe, se empieza de cero.
- **Ajuste fino** entre duelos: al terminar un duelo queda pendiente; `avanzarDueloIA` lo corre **fuera del duelo**
  (resultado, menús) en rebanadas de 1,5 ms por cuadro: preparar, entrenar, medir y guardar, cada paso en su cuadro.
  Siempre parte de la red **base** y ajusta solo la **última capa** (33 pesos, atados a la base): con unos pocos
  duelos, ajustar todo sobreajusta (medido: la pérdida en duelos nuevos del mismo jugador empeoraba). Se usan solo las
  posiciones vistas **desde el rival** (lo que evalúa la búsqueda): con las dos miradas, una red simétrica se anula
  (un jugador fuerte sube un lado y baja el otro). Valida en 1 de cada 5 duelos y se queda solo si mejora; si no,
  vuelve la base.
- La red ajustada va a `/data/yugioh_classic/ia_red_jugador.txt`; si falta, está corrupta o es de otra forma que la
  base, se usa la base. En el PC se prueba igual con una carpeta temporal (`prueba_ia_red`).
- **Opciones**: "IA que aprende: Sí/No" (`Memoria::aprende`, línea `aprende 1|0` de `variedad.txt`; por defecto Sí) y
  "Olvidar lo aprendido" (borra los duelos grabados y la red del jugador). Textos ES/EN en `idioma.h`. Con 14 opciones
  la fila del panel baja a 52 px para que entre en la pantalla.

## Tamaños

| qué | tamaño |
|---|---|
| `ui/ia_red.txt` (base) | ~76 KB |
| red en memoria | 30 KB (×2 con la del jugador) |
| `ia_duelos.bin` (40 duelos) | ~100-350 KB |
| `ia_red_jugador.txt` | ~76 KB |
| ajuste fino: memoria temporal | < 2 MB |

## Ciclos completos de la rama

| ciclo | casos | FALLA | qué pasó |
|---|---|---|---|
| ia_red_c1 | 13890 | 9 | un espacio perdido en `ciclo_completo.sh` (prueba_torneo no corría) y `CU-RED-FE-RELOJ-3` intermitente (3 de 3 bien al repetirla) |
| ia_red_c2 | 13890 | 0 | |
| ia_red_c3 | 13890 | 6 | `prueba_juego`: miraba los visores con el rival todavía jugando (máquina cargada); ahora espera hasta 60 s |
| ia_red_c4 | 13891 | 4 | jugador humano: el rival (Swords of Revealing Light + Messenger of Peace) lo dejó con la mano llena y el jugador de prueba no sabía descartar en la Fase Final; y el volteo de sus monstruos por Swords se veía 0,2 s. Arreglado: descarta como una persona y la jugada siguiente del rival espera 0,6 s tras el volteo |
| ia_red_c5 | 13890 | **0** (0 DECISION) | |
