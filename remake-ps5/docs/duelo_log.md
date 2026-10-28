# Bitácora del duelo (rama classic-ps5-duelo-log)

Un archivo de texto que el juego escribe mientras se duela, para ver **en tiempo real** qué pone el jugador, qué pone el CPU y
**por qué** lo hace la IA, y así juzgar si su juego es lógico. Se lee desde el PC por FTP, solo lectura.

* Archivo: `/data/yugioh_classic/duelo_log.txt` en la PS5 (en el PC, la carpeta de datos de la vista previa:
  `%APPDATA%\YugiohClassic\duelo_log.txt`, o `guardado\` junto al .exe). Código: `src/duelo_log.h`.
* Opciones > **Bitácora del duelo: Sí/No** (por defecto Sí; clave `bitacora_duelo=0/1` en `opciones.txt`). Con No no se abre
  ni se escribe nada. Vale desde el próximo duelo.
* Se **añade** (append) al empezar cada duelo con una cabecera. Si al empezar un duelo pasa de ~2 MB, se renombra
  `duelo_log.anterior.txt` (el anterior se pierde) y se empieza otro: nunca crece sin límite.
* Una línea por suceso, con `flush` inmediato (un `fwrite` de ~100 bytes: microsegundos; nada se hace en el bucle de render).
  Si la carpeta no se puede escribir, la bitácora simplemente no existe: el juego sigue igual.
* **No cambia el juego**: solo lee el duelo a través de ganchos opcionales de `ia.h` (`ia::ganchosLog`, punteros a función
  apagados salvo mientras se anota un duelo). No gasta azar del motor, no modifica el estado y las copias que la IA humana
  imagina (Monte Carlo) se ignoran: solo se anota el duelo de verdad. `src/pruebas/prueba_duelo_log.cpp` comprueba que el
  mismo duelo con y sin bitácora termina con los mismos sucesos.

## Formato

```
mm:ss [T<turno> <fase> J|CPU] texto
```

* `mm:ss`: tiempo desde que empezó el duelo. `J` = el jugador (lado 0), `CPU` = el rival (lado 1).
* Fase: `Robo`, `Principal1`, `Batalla`, `Principal2`, `Final`. Zonas: `z1`..`z5` (de izquierda a derecha en el orden del motor).
* Los nombres de carta salen de `cartas_ui.txt` (español). Los valores (ATK/DEF) son los del momento: con equipos, campos y
  efectos ya aplicados.
* La mano del jugador se lista entera al robar; la del CPU solo se cuenta (`mano 6`), aunque la carta que roba sí se nombra.

### Qué se anota

| Línea | Cuándo |
|---|---|
| `===== Turno N de J|CPU | LP … | mano … | mazo … | monstruos … =====` | empieza cada turno (resumen) |
| `Fase: Batalla` | cada cambio de fase |
| `roba X (mano n, mazo m)` | robo de cualquiera de los dos |
| `Invocacion Normal: X en z2, ataque, ATK/DEF` | invocación normal; `por sacrificio (sacrifica: A, B)`, `por Fusion (materiales: …)`, `Ritual`, `especial de Toon`, o `Invocacion Especial` si la hizo un efecto |
| `coloca boca abajo en z3 (defensa): X` / `coloca boca abajo en z1: <magia/trampa>` | colocar |
| `voltea X …`, `X (z1) cambia a defensa/ataque` | volteo y cambios de posición |
| `ACTIVA X [eslabon n] | objetivo: … | coste: … | en cadena tras: …` | magias, trampas y efectos; `carta de campo`, `equipo`, `efecto de`; objetivos y costes de la jugada; la cadena que ya había |
| `resuelve X`, `X: NEGADA`, `el ataque es NEGADO` | resolución y negaciones |
| `ATAQUE: A (z1, ATK n) -> B (z2, posición, ATK/DEF) | resultado esperado` | declaración del ataque |
| `combate real: …` | el combate de verdad (tras las respuestas: puede diferir de la declaración) |
| `X (z1): DESTRUIDA EN BATALLA / POR EFECTO -> cementerio`, `va al cementerio (…)` | destrucciones y su causa |
| `recibe 1250 de dano (LP 8000 -> 6750)`, `paga n LP`, `gana n LP`, `LP J: a -> b` | cambios de LP (el último, los que no traen su propio suceso) |
| `toma el control de X`, `vuelve a la mano`, `desterrada`, `descarta`, `revela` | otros |
| `elige <lugar> (pide: X)`, `no responde (pasa)` | elecciones y respuestas del **jugador** |
| `***** FIN DEL DUELO: gana … | motivo: … *****` | fin, con el motivo (LP a 0, mazo vacío, efecto, abandono) |

### Decisiones del CPU

Antes de cada jugada del CPU hay una línea `[CPU decide]` con la opción elegida y el motivo, y (la primera vez y cuando cambia)
`[CPU plan]` con el plan de juego, la personalidad y el nivel.

* `ataca con X: ATK 1800 vs ATK 1250 -> ganaria: ACEPTADO (puntos 1466) | otras: Pasar de fase (0)`
* `NO ataca, pasa de fase: X -> Y: ATK 1800 vs DEF 2000 -> perderia 200 LP: RECHAZADO; …` — cada ataque posible que se descartó.
  Para los boca abajo que el CPU no ha visto se muestra **lo que el CPU cree** (su estimación) y, entre paréntesis, el valor real.
* `activa X (puntos n) | responde al ataque de A (ATK n), dano estimado m | sin proteccion en cadena` (o `cadena de n eslabon(es)`).
* `no responde: la mejor activacion posible (X, n puntos) no supera el umbral` (si no tiene nada que activar no se anota).
* `Invocar X: mejor puntuacion (1666), ATK 1800 vs mayor amenaza rival 1250, en ataque | otras: …` — las 3 mejores alternativas
  con su puntuación; si la elegida puntuaba menos que otra se avisa (el ruido de decisión o la búsqueda de la IA humana).
* `pasa a Batalla: ninguna jugada supera el umbral (referencia 0 puntos)`.

Las puntuaciones son las del `Cerebro` de la IA (en puntos ≈ ATK), sin el ruido de decisión.

## Ejemplo real (prueba_duelo_log, IA contra IA)

```
################################################################################
DUELO NUEVO 2026-10-27 17:26:32 | juego Oct 27 2026 17:25:09
Rival: Máscara la Ilusionista | nivel 5 | personalidad farolero | plan (sin plan) | IA clasica | modo prueba
Empieza: el jugador (J) | LP 8000 - 8000
Mazo J (41 + extra 0): Aguja Asesina, Agujero Trampa, Barrera de Insectos, …
Mano inicial J: Hinotama; Aguja Asesina; Waboku; Fisura; Kuwagata α; Imitación de Insecto;
Mano inicial CPU: 5 cartas
################################################################################
00:00 [T2 Principal1 CPU] [CPU decide] Invocar Cebra Oscura: mejor puntuacion (1666), ATK 1800 vs mayor amenaza rival 1250, en ataque | otras: …
00:00 [T2 Principal1 CPU] Invocacion Normal: Cebra Oscura en z1, ataque, ATK 1800/DEF 400
00:00 [T2 Principal1 CPU] [CPU decide] activa Olla de la Codicia (puntos 1043) | sin proteccion en cadena | otras: Colocar mag/tr Sombreros Mágicos (654); …
00:00 [T2 Principal1 CPU] ACTIVA Olla de la Codicia [eslabon 1]
00:00 [T2 Principal1 CPU] roba Paredes de Castillo (mano 5, mazo 33)
00:00 [T2 Batalla CPU] [CPU decide] ataca con Cebra Oscura: ATK 1800 vs ATK 1250 -> ganaria: ACEPTADO (puntos 1466) | otras: Pasar de fase (0)
00:00 [T2 Batalla CPU] ATAQUE: Cebra Oscura (z1, ATK 1800) -> Kuwagata α (z1, ataque, ATK 1250/DEF 1000) | destruye al defensor, dano 550
00:00 [T2 Batalla J] ACTIVA Waboku [eslabon 1]
00:00 [T4 Batalla J] Kuwagata α (z1): DESTRUIDA EN BATALLA -> cementerio
00:00 [T4 Batalla J] recibe 550 de dano (LP 8000 -> 7450)
00:00 [T6 Batalla CPU] [CPU decide] NO ataca, pasa de fase: Hacha del Tigre -> ?: ATK 1300 vs DEF 1450 (boca abajo: valor estimado; real DEF 1400) -> perderia 150 LP: RECHAZADO | otras: …
00:00 [T7 Principal1 CPU] ACTIVA Artilugio de Evacuación Compulsiva [eslabon 1] | objetivo: Capullo Evolutivo (J campo z2)
00:00 [T10 Principal2 CPU] Invocacion por sacrificio (sacrifica: Hacha del Tigre): Insecto Come-Nobles en z2, ataque, ATK 900/DEF 1200
00:00 [T36 Batalla CPU] ***** FIN DEL DUELO: gana CPU en el turno 36 | motivo: los LP de J llegaron a 0 | LP J 0 - CPU 5150 *****
```

## Leerlo desde el PC: `leer_log_duelo.py`

Python 3, solo FTP de **lectura** (usa únicamente `SIZE` y `RETR` con `REST`; nunca sube, renombra ni borra). La IP sale de
`remake-ps5/consola_ip.txt`, puerto 2121, usuario `anonymous` / `yugi` (igual que `subir_imagen.py`).

```
python -I leer_log_duelo.py una              # el log completo
python -I leer_log_duelo.py una -n 80        # las últimas 80 líneas
python -I leer_log_duelo.py seguir           # como tail -f: cada ~3 s pide solo lo nuevo (REST); Ctrl+C sale
python -I leer_log_duelo.py seguir -n 30     # empieza mostrando las últimas 30
python -I leer_log_duelo.py seguir --filtro "CPU decide"      # solo las decisiones del CPU
python -I leer_log_duelo.py una --anterior   # duelo_log.anterior.txt
```

Si la consola se apaga o el juego se cierra, `seguir` reintenta solo; si el archivo se rota (empieza otro) avisa y sigue desde el
principio. Una línea a medias nunca se imprime hasta que llega completa.

## Pruebas

* `src/pruebas/prueba_duelo_log.cpp`: duelos completos IA contra IA (con y sin la IA humana). Comprueba: existe el archivo, la
  cabecera, robos, invocaciones, ataques, decisiones del CPU, el fin con motivo; el prefijo de cada línea; que con la opción en
  No no se crea ni cambia nada; que el duelo con y sin bitácora es idéntico (misma firma de sucesos); que no se cuelan copias de
  la IA humana (los turnos suben de uno en uno); la rotación a `duelo_log.anterior.txt`; carpeta inexistente; ganchos apagados.
* `src/pruebas/prueba_juego.cpp`: la ruta real del juego (`duelo_juego.h`) deja una bitácora legible (cabecera por duelo, robos,
  turnos, fases, líneas de J y CPU, decisiones del CPU).
* `src/pruebas/prueba_leer_log.py`: `leer_log_duelo.py` contra un servidor FTP falso local (nunca la PS5): `una`, `-n`,
  `--filtro`, `seguir` sin repetir ni imprimir líneas a medias, rotación, y que solo se usan comandos de lectura.

## Ganchos en el código de la IA

Solo en `ia.h` (nada en `ia_plan.h`; ninguna lógica cambiada), todos con un `if (ganchosLog.xxx)`:

| Gancho | Dónde |
|---|---|
| `ganchosLog.suceso(duelo, suceso)` | `Duelo::suceso`, tras anotar el suceso y antes del motor de efectos |
| `ganchosLog.jugada(duelo, jugada)` | `Duelo::aplicar`, al entrar (jugador y CPU) |
| `ganchosLog.candidatas(duelo, lista)` | final de `Cerebro::candidatos` y de `Cerebro::candidatosRespuesta` |
| `ganchosLog.decision(duelo, jugada, cerebro)` | `Turno::jugar`, justo antes de aplicar la jugada del CPU |

`duelo_juego.h` abre la bitácora al empezar el duelo (`duelo_log::empezar`) y la cierra en `terminarDueloIA`.
