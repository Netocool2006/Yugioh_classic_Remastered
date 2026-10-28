# Ciclos de pruebas (rama classic-ciclos)

Cada ciclo es `src/pruebas/ciclo_completo.sh` corrido completo desde cero dentro del contenedor `arch-ps5`. Compila
el juego, borra y recompila cada prueba, corre la batería, la certificación y los casos de uso (`docs/casos_de_uso.md`).
Los logs de cada ciclo quedan en `D:\Docker\compartido\wt\ciclos_logs\cicloN\` (`resumen.txt` y `resultados.tsv`).
Los casos marcados DECISIÓN no cuentan como falla (`docs/decisiones_pendientes.md`).

## Ciclo 1 (20-10-2026, 07:00 a 07:21)

**7008 casos: OK 6956, FALLA 50, DECISIÓN 2.**

Fallas del juego (6) y lo que se hizo:

| caso | causa | arreglo |
|---|---|---|
| `CU-FE-OPC-PREGUNTAR-EFECTO` | "Preguntar antes de activar" se guardaba, pero el duelo no la leía en ningún lado (Siempre y Nunca hacían lo mismo que Auto) | Nunca: el duelo pasa solo cuando le pregunta al jugador (`duelo_juego.h`). Siempre: el motor le ofrece al jugador cada ventana de respuesta aunque no tenga nada que activar (`Duelo::preguntarSiempre`, `ofrecer` en `ia.h`; la IA humana no lo usa al imaginar) |
| `CU-CARTA-cave_dragon-INVOCAR`, `CU-CARTA-terrorking_archfiend-INVOCAR` | estaban en la lista "no invocables" de `rivales.txt` (línea N), pero su texto solo pone una condición (Cave Dragon: sin otros monstruos; Terrorking: con un Archfiend). Nunca se podían invocar de modo normal | se sacaron de la lista N; la condición ya la revisaba `motorPuedeInvocar` |
| `CU-BE-ATAJO-FAIRY-GUARDIAN` | devolvía la última magia del cementerio, no la que mandó el rival | el motor anota qué magias mandó el rival al cementerio en el turno (`ef.magiasPerdidas`); Fairy Guardian elige entre esas |
| `CU-BE-ATAJO-PITCH-BLACK` | solo pasaba contadores a monstruos propios y al primero | pasa a cualquier monstruo boca arriba que use contadores de magia, de los dos lados, y elige quien la activa (la IA, uno suyo) |
| `CU-BE-ATAJO-LEVEL-LAB` | elegía sola el monstruo de más nivel | lo elige quien la activa (pregunta del motor); la IA sigue eligiendo el de más nivel |

Otras fallas:

- 43 por el runner: a las pruebas se les pasaba la carpeta de cartas sin la barra final (`/tmp/ciclo/ui`) y no leían
  nada (prueba_constructor, prueba_duelo, prueba_variedad, prueba_ia_humana y prueba_audio, más las hojas que cubren).
  La prueba estaba mal: se arregló `ciclo_completo.sh`.
- 1 hoja sin prueba: `CU-BE-PREGUNTAR-SIEMPRE`. Se escribió su prueba junto con el arreglo.

Antes del ciclo 1, al armar las pruebas, se corrigieron pruebas mal planteadas (no el juego): el id de Blue-Eyes es
`blue_eyes`; la fase final pasa sola al turno siguiente si no hay nada que hacer; Toon Alligator es un monstruo normal;
Tsukuyomi se pone boca abajo con su propio efecto; la cadena se resuelve dentro de la misma jugada si nadie más
responde; `estadisticas::leer` reinicia los datos antes de abrir (la prueba le pasaba una referencia a su propia ruta).
También se corrigió el id de Blue-Eyes en la prueba de capturas del resultado (`resultado_juego.h`, solo PC).

## Ciclo 2 (20-10-2026, 07:32 a 07:54)

**7008 casos: OK 7006, FALLA 0, DECISIÓN 2.** Se terminó ahí.

- Compilación del juego (PS5 y PC), las 9 pruebas de siempre, la certificación y los casos de uso: todo con rc 0.
- Certificación: 1385 cartas con efecto, OK 1385, FALLA 0, SIN PRUEBA 0.
- Carta por carta: 1774 DATOS, 1774 CARAS, 1094 INVOCAR, 680 COLOCAR, 1385 EFECTO y 25 CAMPO, todas OK.
- Las 2 DECISIÓN (repetición del ataque e Inferno Tempest) están en `decisiones_pendientes.md`.
- No hizo falta arreglar nada más.

# Ciclos de la rama classic-reglas-oficiales (las 2 decisiones con la regla oficial)

El usuario eligió la opción B en las 2 decisiones de `decisiones_pendientes.md`: repetición del ataque con elección
(otro objetivo, el mismo o no atacar) y ventana justo después del daño (`V_DANNO`) para Inferno Tempest, Attack and
Receive y Numinous Healer. Los 2 casos DECISIÓN pasaron a pruebas normales y se agregaron casos de back, IA y front
(`CU-BE-REPETICION-ATAQUE-*`, `CU-BE-VENTANA-DANNO-*`, `CU-FE-DUELO-REPETICION`, `CU-FE-DUELO-VENTANA-DANNO`).
Antes del ciclo 1 se ajustaron 4 casos de `prueba_efectos.cpp` a la regla nueva (Attack and Receive y Numinous Healer
se prueban en su ventana; Magician's Circle y Scapegoat responden la repetición).

## Ciclo 1 (20-10-2026)

**7016 casos: OK 7014, FALLA 2, DECISIÓN 0.**

- `CU-FE-DUELO-INVOCAR` (y la prueba entera de casos de uso): el duelo del front usa una semilla por reloj; esta vez
  el rival respondió a la invocación (Spear Dragon) y la sacó del campo. El juego estaba bien; la prueba pedía que el
  monstruo siguiera en el campo. Ahora pide que la invocación se haga y que la mesa muestre lo mismo que el duelo.

## Ciclo 2 (20-10-2026)

**7016 casos: OK 7016, FALLA 0, DECISIÓN 0.** Se terminó ahí.

- Certificación: 1385 cartas con efecto, OK 1385, FALLA 0, SIN PRUEBA 0.
- IA humana contra clásica: 67,5 % de victorias (27 de 40); 14,2 ms medios por decisión, máximo 101 ms (presupuesto
  ~150 ms).

## Rama classic-mazos-torneo (20-10-2026)

- **Ciclo 1** (`ciclos_logs/mazos_torneo_1`): 7029 casos, OK 7023, FALLA 6, DECISIÓN 0. Las 6 eran `prueba_variedad`
  ("con la dificultad alta no gana más": 31 % contra 33 %). Causa: los combos nuevos de los mazos de torneo entraban
  en el armado de los mazos dinámicos de todos los rivales. Arreglo: campo `torneo` en `combos.txt` (solo bono en el
  duelo; no arman mazos dinámicos ni suman papel). Vuelve a 43 % contra 25 %.
- **Ciclo 2** (`ciclos_logs/mazos_torneo_2`): **7029 casos, OK 7029, FALLA 0, DECISIÓN 0**. `prueba_torneo`: 21 mazos
  legales, salen tal cual, 73 % de victorias (308/420) contra los temáticos de nivel 8, 0 jugadas ilegales.

## Rama classic-torneo-extras (20-10-2026)

- **Ciclo 1** (`ciclos_logs/torneo_extras_1`, con `YUGI_ASSETS=/compartido/classic-assets-torneo`): **7088 casos, OK
  7088, FALLA 0, DECISIÓN 0** (hojas del mapa: 301). Certificación: 1395 cartas con efecto, OK 1395. `prueba_efectos`:
  881 casos, 0 fallos. `prueba_torneo`: 21 mazos legales con las cartas exactas, 73 % (307/420) contra los temáticos de
  nivel 8, 0 jugadas ilegales.
