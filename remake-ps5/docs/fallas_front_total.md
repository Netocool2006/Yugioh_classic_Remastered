# Fallas de la batería desde el front (rama classic-front-total)

Pedido del usuario: «Realiza todo el set con la batería de pruebas que ya hemos hecho antes, pero desde el front».
Después se amplió: carta por carta, combate, magias y trampas, cementerio, figuras 3D, fases, rendirse, mazo vacío,
constructor y cadenas.

La batería nueva juega con el mando, solo con botones: X, O, cuadrado, triángulo, flechas, OPTIONS, L3, y L2/R2 y el
stick derecho en el constructor.
- Corre sobre el juego sin GPU de `cu_juego.h`, con `control.h` y `duelo_juego.h` de verdad.
- Lo que se dibuja se prueba en la vista previa real del PC, con Vulkan.

Grupos en `resumen.txt`:
- **CU-FT**: duelos, vigilantes, fases, cadenas, constructor y figuras.
- **CU-FT-CARTA**: carta por carta y combate.
- **CU-FT-PIXEL**: píxeles.

## Qué cubre

| Grupo | Casos | Qué hace |
|---|---|---|
| `CU-FT-DUELO-*` | 280 | Un duelo completo con el mando contra cada rival: 47 personajes del modo historia, cada rival del duelo libre por nivel, cada mazo de torneo y cada plan de la IA (9). Se juega dos veces, con «Preguntar antes de activar» en Auto y en Nunca. El bot invoca, coloca, voltea, cambia de posición, activa de la mano y de la mesa, responde cadenas, ataca, pasa BP/M2/EP y descarta. |
| Vigilantes: `CU-FT-MESA-*`, `-POSICION-*`, `-RIVAL-*`, `-ATASCO-*`, `-FASE-*`, `-PREGUNTA-*`, `-ACCION-*`, `-MENU-MOTIVO`, `-NAVEGAR` | ~40 | Cada cuadro y cada acción de los duelos. La mesa (cartas, posiciones, manos, LP, cementerios, desterradas, turno) es igual al motor tras cada animación. Ninguna carta cambia de posición sin un suceso del motor que lo explique, y ese suceso queda en el registro (L3). Toda activación del rival se anuncia (aviso o cadena) y ninguna carta suya va al cementerio en tu turno sin aviso. Nunca hay atasco: el cuadrado pasa de fase y toda pregunta tiene su menú. |
| `CU-FT-CARTA-<id>-JUGAR / -EFECTO / -COLOCADA` | 3205 | Las 1784 cartas de `cartas_classic.txt`, una por una, desde la mano y desde la mesa. El monstruo se invoca o se coloca con sus sacrificios; la fusión, con Polymerization o por contacto; el ritual, con su magia; el toon, con Toon World. La magia se activa de la mano. Magias y trampas también se prueban colocadas boca abajo (se ven así) y activadas desde su zona; si no se puede en tu turno, la trampa se activa en el turno del rival. Se comprueba: el menú igual al motor, el efecto igual que en el motor (repitiendo las mismas jugadas y con la activación de su prueba del motor), las normales que salen de la mesa terminan en el cementerio, y la de campo queda en su zona y cambia el escenario. |
| `CU-FT-COMBATE-<id>` | 1102 | Cada monstruo en 5 combates: ataca a uno en ataque; ataca a uno boca abajo en defensa (se voltea, con rebote si la DEF es mayor o perforación si la carta perfora); y recibe el ataque en defensa boca arriba, boca abajo en defensa (se voltea) y en ataque. Los LP siguen la regla con monstruos normales; con efectos, salen igual que en el motor. Los destruidos van al cementerio de su dueño, y el visor del cementerio (X en la zona) coincide. |
| `CU-FT-FASES-*`, `CU-FT-CANCELAR-*` | 9 | M1 → BP → M2 → fin de turno. Desde la BP se puede terminar el turno sin atacar. En el primer turno, M1 → M2. EP con la mano llena: descartar con el mando. Se cancela el menú, la elección de zona y la de objetivo sin que quede nada hecho. |
| `CU-FT-RENDIRSE`, `CU-FT-MAZO-VACIO-*` | 3 | Abandonar desde la pausa termina en derrota con el motivo «Te rendiste». Quien no puede robar pierde, sea el jugador o el rival, con el motivo «sin mazo». |
| `CU-FT-CADENA-*` | 5 | Raigeki ← Magic Jammer del rival ← Solemn Judgment: 3 eslabones numerados en pantalla (1 tuyo, 2 del rival, 3 tuyo), se resuelven al revés (LIFO) y eslabón por eslabón. Tu ataque ← Mirror Force ← Trap Jammer. Ataque del rival ← tu Mirror Force: los dos jugadores responden. |
| `CU-FT-CONS-*` | 12 | Constructor con el mando: crear un mazo y ponerle nombre con el teclado en pantalla. Tope de 60 (no deja pasar). Con 39 no se graba (mínimo 40). 3 copias, banlist y tope de fusión 15. Grabar, cargar, persiste al reiniciar el juego, y borrar (también persiste). |
| `CU-FT-PIXEL-*` | 50 | 7 configuraciones (tapete Auto con el fondo de Téa, Pradera, Mar, Sombras, Joey, figuras encendidas y apagadas) con la cámara 0. Tras invocar con el mando, las cartas propias y del rival, en ataque y en defensa, muestran su cara y no la casilla vacía (2 fotos: a los 4 s y a los 12 s). |
| `CU-FT-FIGURA-*` | 65 | Las cartas con figura la muestran solo boca arriba, nunca boca abajo. Al voltearse aparece, al morir desaparece y la zona queda vacía. Con la opción apagada no hay ninguna. |

La orientación, las alas, el aliento, las armas y las animaciones de las figuras no se pueden medir con píxeles. Quedan
en `decisiones_pendientes.md` para revisarlas a ojo.

## Ciclos completos desde 0 (`ciclo_completo.sh`)

| Ciclo | Casos | OK | FALLA | CU-FT | CU-FT-CARTA (+ combate) | CU-FT-PIXEL / FIGURA | Lo de antes (CU-BE, CU-FE, CU-CARTA) |
|---|---|---|---|---|---|---|---|
| 1 (sin arreglos) | 11865 | 11625 | 240 | 1513: 143 duelos atascados + 8 | 3205 + 1102: 58 + 26 | 50 / 65: 0 / 3 | 7082, 0 fallas |
| 2 (arreglos 1-6) | 11863 | 11858 | 5 | 1 (fase en BP con cadena del rival) | 0 | 0 / 2 (el volteo no se veía) + 3 cierres del PC | 7082, 0 fallas |
| 3 (F1-F4 y el arnés del PC) | **11863** | **11863** | **0** | 1511: 0 | 3205 + 1102: 0 | 50 / 65: 0 (sin cierres con error) | 7082, 0 fallas |

Ciclo 3: `ciclos_logs/front_total_ciclo3/resumen.txt`. Lo corrió la copia del commit `c2c8644` (`ciclos_logs/snap3`).

## Fallas encontradas y arreglos

### F1. CRÍTICA (arreglada): en el turno del rival el jugador no podía contestar nada y el duelo quedaba trabado
- **Casos**: `CU-FT-PREGUNTA-RESPUESTA`, 146 de 280 `CU-FT-DUELO-*`, 50 `CU-FT-CARTA-<trampa>-EFECTO` (Mirror Force,
  Trap Hole, Magic Cylinder, Sakuretsu Armor, Solemn, Torrential Tribute, Waboku…), los combates con efectos de volteo
  o de búsqueda en el turno del rival (Sangan, Witch, Big Eye…) y `CU-FT-CADENA-TURNO-RIVAL`.
- **Reproducir**: colocar una trampa (X, Colocar, X), terminar el turno (cuadrado ×2–3) y, cuando el rival ataca, X
  en «No activar»: sale «No se puede», el menú vuelve a abrirse y el rival nunca sigue.
- **Causa**: `duelo_juego.h` `jugarJugador()` rechazaba toda jugada del jugador si el turno era del rival (`activo != 0`),
  incluidas las respuestas que el motor acepta del lado al que le pregunta.
- **Arreglo**: activar, pasar y elegir valen cuando el duelo le pregunta al jugador, también en el turno del rival.
  Nunca se contesta por el rival, porque el motor no sabe quién contesta.

### F2. MEDIA (arreglada): una carta del rival se iba al cementerio en tu turno sin aviso (síntoma 3)
- **Caso**: `CU-FT-RIVAL-CEMENTERIO`, con Swords of Revealing Light al acabarse, en tu fase final.
- **Arreglo**: `presentarSucesos` anuncia «Al cementerio: <carta>» cuando una magia/trampa del rival se va al
  cementerio en tu turno sin haberse activado. El turno se toma del propio suceso.

### F3. MEDIA (arreglada): el menú ofrecía acciones que el motor rechazaba («No se puede»)
- **Casos y arreglos** (en `control.h`, el menú ahora sigue al motor y dice el motivo; textos ES/EN en `idioma.h`):
  - `CU-FT-ACCION-POSICION`: Koumori Dragon, Blackland Fire Dragon y Armed Dragon LV5 con Gaap o Level Limit. Ahora sale
    «Un efecto le impide cambiar de posición», y Second Goblin permite el cambio extra.
  - `CU-FT-ACCION-COLOCAR`: con Light of Intervention, «Colocar boca abajo» invocaba boca arriba sin avisar. Ahora la
    opción aparece desactivada con su motivo (queda en decisiones por si se prefiere de otra forma).
  - `CU-FT-ACCION-COLOCAR-MT`: con Cold Wave o Chain Energy no se puede colocar ni activar. Ahora el menú lo dice.
  - `CU-FT-ACCION-ATACAR`: «Atacar» activo sin ningún monstruo del rival atacable. Ahora sale «No hay monstruos del
    rival que se puedan atacar».
  - `CU-FT-ACCION-ACTIVAR-*` (Mystical Space Typhoon, Call of the Haunted): rechazos causados por F1. Desaparecieron
    al arreglarla.

### F4. MEDIA (arreglada): el monstruo boca abajo destruido en un ataque desaparecía sin mostrarse (y sin su figura)
- **Casos**: `CU-FT-FIGURA-*-VOLTEADA-CON-FIGURA` en el PC (Baby Dragon boca abajo atacado).
- **Causa**: al presentar el combate, la mesa saltaba al estado final del motor: la carta ya estaba en el cementerio y
  nunca se veía dada vuelta.
- **Arreglo**: en `S_COMBATE`, el atacado boca abajo se da vuelta en la mesa (con su animación y su figura). Los que
  mueren siguen en su zona hasta el golpe.

### F5. Del arnés, no del juego
- Los mazos de torneo los sortea el rival, y la variedad no repite el del duelo anterior. Si no sale en 250 sorteos con
  el cuadrado, se juega con el que salió y queda anotado.
- La fase con una cadena del rival en el medio: el tiempo que el rival piensa y que se ven los carteles no cuenta para
  el límite de 8 s del jugador.
- Las cartas que su efecto deja en la mesa (Swords, Different Dimension Capsule, Metalmorph…) no tienen que ir al
  cementerio.
- Gaap pone boca arriba en ataque a los boca abajo al llegar, así que ya no hay volteo al atacar.
- 84 cartas no tuvieron ocasión de activar su efecto en el escenario: rituales sin sus materiales, condiciones muy
  particulares, Polymerization sola. Están listadas en `prueba_front_total.err` («sin ocasion»), tienen su caso
  `-JUGAR` y la paridad del menú con el motor. No cuentan como efecto probado.

### Los 4 síntomas de la PS5

| Síntoma | Qué vio la batería |
|---|---|
| 1. Carta invocada que se desvanece (queda la casilla) | Con la rama `classic-ps5-arreglos1` unida, las 7 configuraciones de píxeles muestran la cara de cada carta propia y del rival, en ataque y en defensa (`CU-FT-PIXEL-*` OK, a los 4 y a los 12 s). |
| 2. Pasó sola de ataque a defensa | Ningún cambio de posición sin suceso en el motor ni en la mesa (`CU-FT-POSICION-*`), en 280 duelos y 5485 combates. Los casos parecidos eran F3 (colocar con Light of Intervention) y F4. |
| 3. Trampa del rival al cementerio sin aviso | F2 (arreglada). Toda activación del rival se ve (`CU-FT-RIVAL-AVISO`). |
| 4. No pudo pasar a M2 ni terminar el turno | En tu turno el cuadrado siempre pasa (`CU-FT-FASE-*`). En el turno del rival, el duelo se trababa por F1 (arreglada). |

## Del arnés del PC (arreglado): `free(): invalid pointer` al cerrar después de la batalla de figuras

La prueba del PC (`front_total_pc.h`) armaba la mesa con `invocarEspecial(lado, id, defensa, ZONA)`, pero el cuarto
parámetro es el **dueño**, no la zona. Por eso la carta destruida iba al cementerio de un «lado 3» inexistente: se
escribía fuera de la memoria y el cierre fallaba. El juego no tiene el problema; ahora se pasa -1.
