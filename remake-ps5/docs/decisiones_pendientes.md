# Decisiones pendientes

Las fallas que necesitan que decidas una regla o un diseño cuentan como **DECISIÓN** en los ciclos de pruebas
(`cu::decision` en `src/pruebas/cu_back.h`) y no como FALLA. Ninguna regla abierta; queda una cosa para validar en la PS5.

## Para validar en la PS5 (rama classic-ps5-arreglos1, primer turno contra Téa)

1. **"No pude pasar a M2 ni terminar el turno"**: en el PC (juego real y prueba con el mando, 7 duelos contra Téa con
   tu mazo y el plan bloqueo, con Trap Hole y sin ella) el cuadrado siempre pasa de fase y termina el turno; no se
   reprodujo el atasco. Lo más probable: el cuadrado se perdía con la pregunta "¿activar una carta?" o con el detalle
   abiertos (X durante la animación de la trampa abre el detalle). Ahora en tu turno el cuadrado en la pregunta es "No
   activar" y pasa de fase, y con el detalle abierto lo cierra y pasa. Si en la PS5 vuelve a trabarse, una foto de la
   pantalla (y si se ve, el texto del botón SIGUIENTE FASE) alcanza para seguir.
2. **Cartel de la carta del rival**: dura 2,2 s y el duelo espera a que se lea. Si lo quieres más corto o más largo,
   o también en el turno del rival, se cambia en `control::Mando::DURACION_CORTE` y en `duelo_juego.h`.

## Batería desde el front (rama classic-front-total)

No frenan los ciclos y no son fallas. Hay que decidirlas o validarlas a ojo:
1. **Terminar el turno desde la batalla** — RESUELTO (rama classic-bp-terminar, decisión del usuario: agregarlo).
   En la Fase de Batalla el **círculo** termina el turno con un solo botón (como en Master Duel). Se ve en la barra
   de ayuda («O Terminar turno» / «O End turn») y como segunda placa del HUD, igual a la de fase, encima de ella
   («TERMINAR TURNO / Le toca al rival»). El cuadrado sigue yendo a M2. Es solo un atajo: pasa por M2 y por la
   Fase Final del motor (ventanas del rival y efectos de fin de turno) como dos cuadrados; no vale con una
   pregunta, cadena o elección abierta ni con un ataque en curso. Para que el círculo de «atrás» no termine el turno
   sin querer, solo vale si pasaron 0,6 s desde el último círculo y desde que se volvió a la mesa
   (`CU-FT-FASES-BP-TERMINAR*`). Se eligió el círculo porque es el único botón libre en la mesa (X, triángulo,
   cuadrado, L1/R1, L3, R3, L2/R2, OPTIONS y el panel táctil ya tienen uso). Si prefieres otro botón, se cambia.
2. **Figuras 3D**: la batería comprueba automáticamente que la figura sale solo boca arriba, que no sale boca abajo,
   que aparece al voltearse, que desaparece al morir y que no sale con la opción apagada (`CU-FT-FIGURA-*`).
   **Orientación** (regla del usuario): las figuras **miran a la pantalla** (la cámara); solo cuando atacan se
   voltean hacia el objetivo, atacan y vuelven a mirar a la pantalla. No miran al rival en reposo. Ya era así en
   todos los caminos (reposo, defensa, aparición, entrada de carta clave, con esqueleto y rígidas, jugador y
   rival); ahora la cuenta está en `orientacion_figura.h` y la mide `CU-FT-FIGURA-ORIENTACION*`. Las alas, el
   aliento, las armas y las animaciones de ataque y de muerte siguen las reglas de Yugi Reload, pero esto no se
   puede medir con píxeles. Hay que mirarlo en las fotos marcadas
   (`ciclos_logs/<ciclo>/pixeles/*_figuras/ft_*_marcada.png`) o en la PS5.
3. **Seiyaryu**: el pedido la nombra junto a Magic Jammer, Solemn y Trap Jammer, pero no hay una carta con ese
   nombre que encadene. Las cadenas se prueban con Raigeki ← Magic Jammer ← Solemn Judgment (3 eslabones), con tu
   ataque ← Mirror Force ← Trap Jammer, y con el ataque del rival ← tu Mirror Force (`CU-FT-CADENA-*`). Si piensas en
   otra carta, dime cuál.
4. **Light of Intervention**: con ella boca arriba, «Colocar boca abajo» ahora aparece desactivada y dice el motivo.
   Antes se podía elegir y el monstruo quedaba invocado boca arriba. Si prefieres que se pueda elegir y avise, se
   cambia.

## Resueltas el 20-10-2026 (cartas extra de los mazos de torneo)

El usuario confirmó lo que ya hace el juego; no hubo que cambiar nada:
1. **Foolish Burial**: se queda **limitada (1)**.
2. **Il Blud en el cementerio**: cuenta como monstruo de efecto (regla oficial).
3. **Yamato-no-Kami**: destruye una magia/trampa solo cuando destruye en batalla atacando (se deja así).
4. **Izanagi y Yamato-no-Kami** (2008): se quedan; son de mecánica clásica, sin Sincronía, Xyz, Péndulo ni Link.
5. **Assets**: las caras y listas de las 10 cartas ya están copiadas a `classic-assets` (respaldo de las listas
   anteriores en `D:\Docker\compartido\respaldo-assets\antes_torneo_extras_20261020`).

## Resueltas

### Mazos de torneo (rama classic-mazos-torneo) — RESUELTAS (rama classic-torneo-extras)

1. **Cuánto sale un mazo de torneo** (40 % genéricos de nivel 7-8, 30 % personajes, 40 % desde el nivel 7): se dejan.
2. **Personajes de nivel 5-6 con mazo de torneo**: se quedan así.
3. **Joey (nivel 4) con Gearfried y Equipos**: hecho (`A|joey|gearfried_equipo`; `CU-BE-TORNEO-JOEY`).
4. **Chaos Control sin Chaos Emperor Dragon**: se deja así (fiel a la banlist Goat del juego).
5. **Las 10 cartas que faltaban** (Command Knight, Cyber Dragon, Foolish Burial, Goblin Zombie, Il Blud, Magical
   Mallet, Izanagi, Yamato-no-Kami, Obnoxious Celtic Guard, Drillroid): agregadas como cartas extra con datos ES/EN,
   arte de Gemini, caras y efecto en el motor (regla Géminis nueva para Il Blud); las listas de torneo ya son exactas
   (`CU-BE-TORNEO-EXACTAS`, `CU-BE-EXTRA-*`). Marshmallon y Blue-Eyes Ultimate Dragon siguen siendo cartas firma.
6. **Mazos de torneo para el jugador**: en el constructor, Triángulo en MIS MAZOS abre **MAZOS DE TORNEO**; se copian
   a tus mazos para usarlos o editarlos (`CU-FE-CONS-TORNEO-*`).

### 1. Repetición del ataque: elegir el objetivo (`CU-BE-REPETICION-ATAQUE-ELEGIR`) — RESUELTA (opción B, regla oficial)

**Decisión del usuario:** respetar la regla oficial (opción B). Rama `classic-reglas-oficiales`.

**Ahora.** Si después de declarar un ataque cambia el número de monstruos del rival (por ejemplo, Scapegoat), aparece
un monstruo ante un ataque directo o el objetivo ya no está, el duelo le pregunta al atacante (pregunta del motor
`P_ELEGIR`, texto `ef.elegir.repeticion`): otro objetivo válido, el mismo (va primero) o "No atacar". Si el rival se
quedó sin monstruos, se ofrece "Ataque directo". Elegir un objetivo es una declaración nueva (el rival puede volver a
responder). "No atacar" deja al monstruo volver a atacar en esa fase. Si el número no cambia (Waboku), el ataque sigue.
El jugador lo ve con el foco (círculo: no atacar) o con una lista ("Ataque directo", "No atacar"); la IA clásica y la
humana (también en sus simulaciones) responden con su política de ataque. Casos: `CU-BE-REPETICION-ATAQUE`,
`-ELEGIR`, `-DIRECTO`, `-IA` y `CU-FE-DUELO-REPETICION`.

### 2. Inferno Tempest: el momento de la activación (`CU-BE-ATAJO-INFERNO-TEMPEST-MOMENTO`) — RESUELTA (opción B, regla oficial)

**Decisión del usuario:** respetar la regla oficial (opción B). Rama `classic-reglas-oficiales`.

**Ahora.** Justo después de aplicar daño (de batalla o de efecto) se abre una ventana (`V_DANNO`) solo para quien lo
recibió y solo si tiene una carta "cuando recibes daño" que pueda activar. Inferno Tempest (3000 o más de daño de
batalla de un ataque), Attack and Receive y Numinous Healer solo se pueden activar en esa ventana; después, ya no. Se
revisó el catálogo: no hay más cartas con ese momento (Trap of Board Eraser responde a la activación, Nutrient Z y
Kuriboh van en el cálculo, Dark Room of Nightmare es continua). "Preguntar antes de activar" se respeta (Nunca: pasa
solo; Auto y Siempre: pregunta, porque la ventana solo se abre si hay una carta para ella). Casos:
`CU-BE-ATAJO-INFERNO-TEMPEST`, `CU-BE-ATAJO-INFERNO-TEMPEST-MOMENTO`, `CU-BE-VENTANA-DANNO*` y
`CU-FE-DUELO-VENTANA-DANNO`.

## Modo en línea (rama classic-red-base, `docs/red.md`)

Estado tras la rama classic-en-linea (el modo en línea ya está en el juego; `docs/en_linea.md`):
1. **OBLIGATORIO ANTES DE ABRIR LA SALA AL PÚBLICO: esconder la mano y el mazo del rival (compromiso versión 2)**.
   Por ahora el duelo sale con lockstep y todo conocido: el orden comprometido viaja al empezar porque el motor (`ia.h`)
   necesita las cartas del rival (robos, descartes al azar, efectos que miran la mano, búsquedas en el mazo, Pot of
   Greed del rival...). El compromiso garantiza que el orden es justo, que cada robo es la carta comprometida y que al
   final todo cuadra; lo que NO impide es que una máquina modificada mire la mano y el mazo del otro. Para esconderlo el
   motor tiene que aceptar cartas desconocidas («?») en el mazo y la mano del rival y la máquina del dueño resolver y
   anunciar lo que cada efecto revela (los accesos a `mazo`/`mano` del otro lado en `ia.h` por un punto único que pide
   la carta al dueño). Decidido: es obligatorio antes de publicar la URL de la sala; entre amigos (red local, código)
   se puede jugar ya.
2. ~~Vista en espejo para el invitado~~: hecha (`red/espejo.h`; `ia::Duelo::ladoVista` para las cartas que mira un
   efecto). Queda: dos revelaciones que gastan azar solo con `lado == 0` (una carta al azar de la mano del rival en dos
   efectos de la fase de espera) siguen sin mostrarse al invitado; cambiarlas cambia el azar del motor contra la IA.
3. ~~Abandono sin árbitro~~: decidido: la desconexión de más de 30 s termina el duelo SIN contar para ninguno (ni
   estadísticas ni ELO). Riesgo: el que va perdiendo puede cortar la red para no perder; si se abusa, medir en la sala
   cuántos abandonos tiene cada id.
4. ~~Tiempo por jugada~~: decidido y hecho: reloj por turno de 3 min por jugador (`en_linea.h`), alerta a 30 s, jugada
   automática al agotarse y 3 agotamientos = derrota.
5. ~~Menú~~: hecho (DUELO EN LÍNEA en el menú principal).
6. **Nombre de duelista**: hoy es «Duelista XXXX» (del id anónimo); falta una opción para escribirlo (el teclado del
   constructor sirve).
7. **Liberar el puerto del router**: el mapeo UPnP (2 h) se borra al salir con «Salir» del menú; si el juego se cierra
   de otra forma queda hasta que vence.
8. **Validar en la PS5 y en el PC**: probado en el contenedor (Linux, g++ y clang con el CPU de la PS5) y compilado para
   la PS5 y para Windows (mingw: el socket, el canal y la red local corrieron en Windows). Falta un duelo real PS5 ↔ PC (las pruebas juegan dos juegos completos en loopback: `prueba_red_front.cpp`).
