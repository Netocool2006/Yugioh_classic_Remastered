## Revision de la certificacion (por que cambiaron los numeros)

Primer reporte del probador: **FALLA 292**; reporte intermedio: **FALLA 128 / SIN PRUEBA 29** (60 de las fallas eran
trampas continuas); despues se publico **OK 1358 / FALLA 0 / SIN PRUEBA 1**. Esa ultima cifra estaba **inflada**. Lo
que cambio, grupo por grupo:

**De 292 a 128: errores del propio probador (no del juego).** La plantilla contaba como efecto de la carta cosas del
contexto armado: los robos del rival (el Pot of Greed que activa el rival en "responde a una activacion"), el danno y
la destruccion del combate del propio contexto, los sacrificios de la invocacion, el robo del turno siguiente en
"fase final del rival". Ademas, el regex no reconocia "Special Summons" en plural y tomaba "100 de danno por cada..."
como si fuera el total. Todo eso se corrigio en la plantilla. Ninguna comprobacion se hizo mas permisiva para que una
carta pasara.

**Las 128 (incluidas las 60 trampas continuas): bug real del front.** Para comprobarlo, el probador actual se compilo
con el `duelo_juego.h` anterior al arreglo: fallan exactamente 123 cartas (60 trampas continuas, 33 trampas normales,
17 monstruos de efecto, 4 magias normales...) y **todas** por "LP mostrados X, reales Y" en el front; ninguna por la
plantilla. El bug: si en una misma jugada se resolvia una cadena y despues habia combate (por ejemplo, responder a un
ataque directo con una trampa que no lo detiene), `S_RESUELVE` ponia los LP finales y el ataque animado volvia a restar
su danno. Despues de un ataque directo de 1200 se veia 5600 en vez de 6800. Las trampas continuas eran mayoria porque
la plantilla prueba cada trampa tambien en "responde a un ataque directo". Arreglo: `lpTras()` en `duelo_juego.h`.
Las demas de las 128 eran de la plantilla: 3 son las expectativas ajustadas de abajo y 1 era el regex de "Special
Summons" (The Shallow Grave).

**Rituales (18 magias + monstruos rituales) y Toons (9).** Antes no habia un contexto para ellos. Se agregaron:
- *Invocacion ritual* (`J_RITUAL`): la magia y su monstruo en la mano y sacrificios en el campo. Se exige que el
  monstruo ritual llegue al campo. Antes no se podian probar porque el probador no cargaba las recetas (lineas `T` de
  rivales.txt y personajes_rivales.txt). Contract with the Abyss y Earth Chant no tenian receta en el motor y no se
  podian usar nunca: se arreglo con `Base::ritualCon()`.
- *Toon* (`J_TOON`): Toon World boca arriba, invocacion y, al turno siguiente, ataque directo. Se exige que llegue,
  que haga danno directo y que **pague 500 LP solo si su texto lo dice** (en Goat, 4 de los 9 pagan). Esto prueba
  **solo la regla Toon**. Cinco Toons tienen un efecto propio que el motor **no tiene**: Toon Cannon Soldier, Toon Dark
  Magician Girl, Toon Gemini Elf, Toon Goblin Attack Force y Toon Masked Sorcerer. Ahora salen **FALLA (efecto no
  implementado)**.

**Correcciones al inflado (despues de que el coordinador lo pidiera):**
1. **Cartas solo marcadas en listas.** Muchos casos de prueba_efectos.cpp hacen `for (id : {...}) probadas[id]++` sin
   armar ni comprobar esas cartas (por ejemplo, "Dragon Seeker" marca Chain Burst, Dragon's Rage, D.D. Borderline...).
   Antes contaban como certificadas por un caso a mano. Ahora un caso cuenta para una carta solo si la usa en su
   escenario (se lee el fuente del caso). Asi dejaron de contar unas 430 cartas: las que no pasan por la plantilla
   quedan **SIN PRUEBA**.
2. **Fusiones con efecto sin implementar.** El conteo dejaba fuera a las fusiones y rituales con efecto en el texto
   que no tienen DefCarta (Dark Paladin, Thousand-Eyes Restrict, Gatling Dragon, los X/Y/Z, etc.: 24). Ahora cuentan
   y salen **FALLA (efecto no implementado)**.

**Lo que sigue siendo una decision de la plantilla (y por que):**
- *No concluyente*: si la carta se activa pero su efecto no se ve en ninguno de los tres escenarios genericos (por
  ejemplo, Acid Rain sin Maquinas, o una trampa continua que espera un suceso), la plantilla queda SIN PRUEBA y no
  FALLA, porque el efecto depende de condiciones que el escenario no tiene. La carta queda OK solo si un caso a mano la
  usa y pasa; si no, queda SIN PRUEBA. Con `CERT_ESTRICTO=1` esas cartas se cuentan como FALLA (sirve para ver cuantas
  dependen solo de su caso a mano).
- *Expectativas ajustadas* (lista de abajo, 4 cartas, cada una con su motivo segun el texto).
- *Toons*: la comprobacion de "pagar 500" sigue al texto de cada Toon. No se compara el numero del texto de Toon Cannon
  Soldier (500, de su efecto de sacrificio, que no esta implementado) con el danno del ataque directo.

### Muestra al azar: 20 cartas que pasaron de FALLA a OK, verificadas a mano con su texto

Semilla 20261019, sobre las 90 cartas que estaban FALLA en el reporte intermedio y estan OK con las reglas corregidas.
Para cada una se leyo el texto (cartas_ui_en.txt) y lo comprobado (lo observado por la plantilla o los VERIFICA del
caso).

| carta | texto (resumen) | que se comprobo | veredicto |
|---|---|---|---|
| Solemn Wishes | +500 LP cada vez que robas (una o varias cartas) | plantilla: sube LP al robar. Al revisarlo aparecio un **bug**: Pot of Greed daba 1000 (500 por carta). Arreglado; caso nuevo: Pot of Greed = +500 y el robo del turno = +500 | correcto tras el arreglo |
| Gray Wing | descarta 1 en la fase principal 1: ataca dos veces | plantilla: descarte en la ignicion; caso: ataca dos veces | correcto |
| Curse of Anubis | los monstruos de efecto a defensa, DEF original 0, posicion fija | caso: Sangan a defensa con DEF 0 y fijo, Battle Ox (normal) no cambia | correcto |
| Destiny Board | un Spirit Message por cada fase final del rival; con los 4 ganas | caso: un mensaje por fase final (3 mensajes y todavia sin ganador) | correcto (la victoria con 4 tiene otro caso) |
| Ominous Fortunetelling | en tu fase de espera, si aciertas el tipo de una carta de la mano del rival: 700 | plantilla: danno al rival en la fase de espera siguiente (al azar) | correcto en la familia; el 700 no se midio |
| Royal Command | los efectos de volteo no se activan | caso: Man-Eater Bug volteado no destruye | correcto |
| Goblin Fan | un nivel 2 o menos invocado por volteo se destruye | caso: el volteado se destruye | correcto |
| Fatal Abacus | 500 al dueno por cada monstruo que va del campo al cementerio | caso: LP del rival -500 | correcto |
| Appropriate | cada vez que el rival roba fuera de la fase de robo, robas 2 | plantilla: con Pot of Greed del rival se robaban **4**. **Bug**: contaba por carta y no por robo. Arreglado; caso nuevo (roba 2) | correcto tras el arreglo |
| Kryuel | destruido en batalla: moneda; si aciertas, destruye 1 del rival | plantilla: destruido en batalla, destruye al atacante | correcto |
| D.D. Assailant | destruido en batalla: destierra al que lo destruyo y a si mismo | caso: los dos desterrados | correcto |
| A-Team: Trap Disposal Unit | se sacrifica para negar una trampa del rival y destruirla | caso: Mirror Force negada (el atacante sigue vivo y hace su danno) | correcto |
| Newdoria | destruido en batalla: destruye 1 monstruo | plantilla y caso | correcto |
| Threatening Roar | el rival no declara ataques este turno | caso: `puedeAtacar` falso | correcto |
| Shift | cambia el blanco del ataque a otro monstruo propio | plantilla: solo vio algo indirecto (un monstruo destruido). **Se agrego un caso**: el ataque a Celtic pasa a Battle Ox y Baby Dragon se destruye (-500 al rival) | correcto (con el caso nuevo) |
| Gravity Bind | los de nivel 4 o mas no atacan | caso: el de nivel 6 no ataca y el de nivel 1 si | correcto |
| Bad Reaction to Simochi | las ganancias de LP del rival se vuelven danno | caso: Red Medicine del rival le quita 500 | correcto |
| Begone, Knave! | el monstruo que hace danno de batalla vuelve a la mano | plantilla: tras el ataque directo, el atacante vuelve a la mano; y caso | correcto |
| Mask of Weakness | el atacante pierde 700 ATK este turno | plantilla (ATK) y caso | correcto |
| Enervating Mist | el limite de mano del rival pasa a 5 | caso: `limiteMano` = 5 | correcto |

Resultado de la muestra: las 20 hacen lo que dice su texto. En 2 (Solemn Wishes y Appropriate) la revision encontro un
bug del motor, que se arreglo con un caso nuevo; lo mismo se hizo con Silent Magician LV4, que tenia el mismo error de
"por carta" en vez de "por robo". 1 (Shift) solo estaba probada de forma indirecta: se le agrego un caso. 1
(Ominous Fortunetelling) queda probada en su familia pero sin medir el numero.

### Casos nuevos para las cartas SIN PRUEBA (tanda del probador en prueba_efectos.cpp)

Despues de la correccion quedaron 259 cartas SIN PRUEBA: cartas que la plantilla no puede ver y que ningun caso a mano
usaba. Se les estan escribiendo casos de a lotes, cada uno con lo que dice su texto (`tandaProbador()` en
prueba_efectos.cpp). Los casos nuevos encontraron estos bugs del motor, que ya estan arreglados (ver "Arreglos
hechos"):
- Soul Reversal no se podia activar nunca y Royal Command no negaba los efectos continuos de los monstruos de volteo:
  buscaban `clase == "volteo"`, pero ese dato esta en el subtipo.
- Ojama Delta Hurricane!! ademas descartaba toda la mano del rival (usaba la resolucion de Huge Revolution).
- Nutrient Z no servia contra un ataque directo.
- Appropriate, Solemn Wishes y Silent Magician LV4 contaban cada carta robada en vez de cada robo.
- Servant of Catabolism estaba en la lista "por reglas del motor", pero esa regla no existia: no podia atacar directo.
- Cross Counter no doblaba el danno de batalla al atacante (usaba la resolucion de Destruction Punch).

Aproximaciones del motor anotadas como dudosas (ver la lista "Dudosas para revisar"): Penalty Game (siempre el
primer efecto), Thunder of Ruler y Shadow of Eyes (momento de activacion mas amplio que el texto) y D.D. Borderline
("nadie ataca" en vez de "no hay fase de batalla").
- Second Goblin no se podia equipar nunca (`uneA` no lo conocia) e Inaba White Rabbit no atacaba directo (estaba en la
  lista "por reglas" sin la regla). Un barrido de las listas `def(id)` de ia.h confirma que la unica otra carta en esa
  situacion es Gravekeeper's Vassal (anotada como dudosa: su danno sigue siendo de batalla).
- Los monstruos "no afectados por magias" solo se protegian de ser objetivo: Raigeki, Dark Hole y las demas magias
  que destruyen en masa los destruian igual. The Legendary Fisherman con Umi no estaba protegido.
- Senri Eye cobraba 1000 LP en vez de 100. The Dragon's Bead se usaba una sola vez (es trampa continua: se queda).
- Master Monk no atacaba dos veces; Ninjitsu Art of Decoy (continua) protegia solo el turno en que se activaba.
- Kaminote Blow destruia en la fase final en vez de al final del paso de danno.
- Datos: Guardian Dreadscythe, Perfectly Ultimate Great Moth y Toon Dark Magician Girl no estaban en la lista de "no
  se invocan normal" (linea N de rivales.txt). Los casos de prueba no leian esa lista (el juego si): ahora la leen.
- Los 5 Toons sin efecto propio (Cannon Soldier, Masked Sorcerer, Gemini Elf, Dark Magician Girl, Goblin Attack Force)
  ya lo tienen (`cartasProbador()` en ia.h), con un caso cada uno. Quedan FALLA las 24 fusiones sin efecto implementado.

### Aproximaciones declaradas en el propio motor (comentarios "atajo" de ia.h)

El codigo de ia.h marca con "(atajo: ...)" las simplificaciones de cada carta. Se revisaron las que dicen "atajo: sin
...", porque declaran que falta una parte del efecto. Esas cartas (53, lista `PARCIALES` en prueba_certificacion.cpp)
salen **FALLA (efecto parcial)** aunque lo que si hacen pase sus casos: el texto pide algo mas. Las que solo
simplifican una eleccion (la moneda sin "cantar", elegir el de mas ATK sin preguntar: 6) quedan como **dudosas**. Las
demas "atajo" (la IA o el motor eligen el objetivo o la carta cuando el texto deja elegir al jugador) no cambian lo que
hace el efecto y no se cuentan como falla.
- Fusiones con efecto sin implementar: 15 ya lo tienen (`cartasProbador()`), con caso. Siguen FALLA 9 sin implementar
  (Dark Paladin, Thousand-Eyes Restrict, Dark Balter, Fiend Skull Dragon, King Dragun, Ojama King, Ryu Senshi, The Last
  Warrior from Another Planet, Arcana Knight Joker: piden negaciones o reglas que el motor no tiene) y 5 parciales.
- Battle-Scarred no le cobraba al rival (miraba la fase de robo, pero el mantenimiento se paga despues).

### Rama classic-motor2: reglas que faltaban (antes de la certificacion final)

Cada carta tiene su caso en `prueba_efectos.cpp` (tandaMotor2Fusiones, tandaMotor2Ventanas, tandaMotor2Parciales,
tandaMotor2Parciales2, tandaMotor2Goblin, tandaMotor2Extras). Las listas `PARCIALES` y `DUDOSAS` de
prueba_certificacion.cpp quedaron sin esas cartas.

- **Las 9 fusiones sin efecto**: Dark Paladin (descarta 1: niega la magia y la destruye; +500 por Dragon), Thousand-Eyes
  Restrict (los demas no atacan ni cambian de posicion; equipa uno del rival), Dark Balter the Terrible (paga 1000: niega
  una magia normal; los que destruye en batalla no activan su efecto), Fiend Skull Dragon (niega los de volteo y las
  trampas que lo eligen), King Dragun (el rival no elige Dragones; invoca un Dragon de la mano), Ojama King (3 zonas del
  rival bloqueadas; las elige su duenno, rama classic-elecciones), Ryu Senshi (paga 1000: niega una trampa normal; las
  magias que lo eligen se destruyen), The Last Warrior from Another Planet (destruye tus demas monstruos; nadie invoca,
  colocar si) y Arcana Knight Joker (descarta del mismo tipo: niega lo que lo elige; la carta la elige su duenno,
  rama classic-elecciones). Las de "solo con estos materiales" no aceptan sustitutos de fusion.
- **X-Y-Z**: se invocan desterrando sus piezas del campo (tambien uniones equipadas), sin Polymerization, y no vuelven del
  cementerio. Opcion "Invocacion especial" en el menu del monstruo y jugada de la IA.
- **Reaper on the Nightmare**: ataque directo: el rival descarta 1 al azar; se destruye al ser elegido por un efecto.
- **Ventanas**: fase de espera (`V_ESPERA`, solo si alguien tiene una carta de ese momento: Curse of Fiend, Thunder of
  Ruler, Aqua Spirit), final de la batalla (`V_FIN_BATALLA`), al colocar un monstruo (`V_COLOCADO`: Shadow of Eyes) y
  repeticion del ataque (si cambia el numero de monstruos del rival, el atacante elige otro objetivo, el mismo o no
  atacar; si no ataca, puede volver a atacar en esa fase: regla oficial, rama classic-reglas-oficiales).
- **Dudosas corregidas**: Penalty Game (eliges el efecto), Thunder of Ruler (solo en la espera del rival: sin fase de
  batalla), Shadow of Eyes, D.D. Borderline (sin fase de batalla), Weather Report (segunda fase de batalla), Goddess of
  Whim (moneda cantada; Abare Ushioni, Fairy Box, Fiend Comedian, Kryuel y Gamble ya la cantaban: tanda 7aq), Gatling
  Dragon y Valkyrie Vierte (elige el jugador), Metalmorph (bono al atacar; su segundo uso elige al equipado), Neko Mane
  King (pasa a la fase final), Gravekeeper's Vassal (danno de efecto), Mokey Mokey King (al dejar el campo de cualquier
  forma) y Mokey Mokey Smackdown (automatica).
- **Otras aproximaciones de ia.h corregidas**: Orichalcos Sword of Sealing (su segundo efecto, desde la espada), Blue-Eyes
  Shining Dragon (niega lo que lo elige), Decayed Commander (descarte con ataque directo), Element Dragon (ataca otra vez
  con un VIENTO), Rare Metalmorph (niega una vez la magia que elige a su Maquina), Vilepawn Archfiend (no se ataca a los
  otros Archfiend) y los dados de Desrook, Infernalqueen y Vilepawn.
- **Parciales completadas**: todas las de la lista salvo las anotadas abajo (Freed busca en la fase de robo; los dados de
  Darkbishop, Shadowknight, Terrorking, Desrook, Infernalqueen y Vilepawn; Axe of Despair / Horn of Light / Celestial
  Sword - Eatos / Blast with Chain al ir al cementerio; De-Fusion invoca los materiales de la receta; Big Eye ordena;
  Temple of the Kings invoca; etc.).
- **Sin cambio por su texto**: Valkyrie Erste (primer efecto) y Ride of the Valkyries (efecto del cementerio) buscan
  "Mischief of the Time Goddess", que no esta en el juego: nunca tienen objetivo. Pandemonium no tiene dado en su texto
  (la anotacion era de Desrook / Infernalqueen). Dragonic Attack ya lo trataba como Dragon (`dragonExtra`).

### Elecciones del jugador (rama classic-elecciones)

Lo que el motor elegia por el jugador y el texto deja elegir: ahora pregunta a su duenno (P_ELEGIR; la IA sigue
eligiendo sola con su `elegirIA`, igual que antes). Un caso por carta en `tandaElecciones` (prueba_efectos.cpp).
- **Ojama King**: las 3 zonas del rival (`ef.elegir.bloquear`). Para poder preguntar, el bloqueo es un disparo al
  llegar boca arriba (invocado, volteado o al cambiar de control): aproximacion, el rival puede responder a ese disparo.
- **Arcana Knight Joker**: la carta del mismo tipo que descarta (`ef.elegir.descartar_tipo`). Aproximacion: el descarte
  se elige y se hace al resolver (antes era coste al activar; si su efecto se niega, no descarta).
- **Orichalcos Sword of Sealing** (el efecto dado): la carta que manda al cementerio, al resolver (misma aproximacion).
  The Seal of Orichalcos no tiene descarte en su texto.
- **Garuda the Wind Spirit**: el monstruo del rival que cambia de posicion (`ef.elegir.cambiar_posicion`), como disparo
  en la fase final del rival (igual que Dark Magician of Chaos).
- **Menu del jugador**: con The Last Warrior from Another Planet boca arriba, "Invocar" sale desactivado con su motivo
  (colocar sigue); con ataques obligados de Amazoness Archers, el boton de fase dice "Primero ataca (Amazoness
  Archers)" y el cuadrado avisa por que no avanza (ES/EN en idioma.h).
- **Pruebas**: prueba_certificacion.cpp y prueba_juego.cpp vuelven a compilar con el duelo_juego.h nuevo: lo que
  yugireload.cpp incluye en la clase y depende de Vulkan o de pantallas (resultado, velocidad, entrada de cartas clave,
  menu principal, portada) va como maquetas en `pruebas/maquetas_juego.h`.

### Control final (rama classic-control)

Las 30 cartas que la certificacion dejaba **SIN PRUEBA** tienen ahora su caso a mano en `tandaControlFinal`
(prueba_efectos.cpp). Cada caso arma la condicion exacta del texto (union equipada, The Sanctuary in the Sky, una carta
del rival que elige a tu Amazoness, 3000 de danno de un solo ataque, etc.) y prueba tambien un caso en el que la carta
no debe hacer nada. Las de azar (Dark Sage con Time Wizard, Dice Re-Roll, Level Conversion Lab, Sasuke Samurai #4,
Second Coin Toss, Gamble, Shifting Shadows) usan semillas fijas: se comprueba el rango (dado 1-6, los 6 valores
salen), que cada resultado tenga su efecto y las reglas de repeticion (una sola vez por turno, solo si salio mal, y
que cuente la segunda moneda).

Errores del motor que aparecieron y se arreglaron en ia.h:
- **Centrifugal Field** nunca se aplicaba: es una magia de campo y el motor la buscaba en las zonas de magia/trampa.
- **Light of Judgment** y **Archlord Zerato** dejaban descartar cualquier carta; el texto pide un monstruo de LUZ.
  Ahora `descarteVale` lo exige al activar, al elegir la IA y en el menu del jugador (control.h).
- **Pikeru's Second Sight** no se iba nunca; ahora dura hasta la segunda fase final del rival.
- **Ninjitsu Art of Transformation** se iba al cementerio al resolverse; ahora se queda vinculada al monstruo que
  invoca y lo destruye al dejar el campo.
- **Spiritual Energy Settle Machine**: al dejar el campo, los Spirit boca arriba vuelven a la mano (no lo hacia).
- **Shifting Shadows**: activarla ya cuenta como el uso del turno (se podia barajar dos veces el mismo turno).
- **Malice Doll of Demise** no volvia si la sacrificaba Ectoplasmer (una magia continua, fuera de la cadena).

Aproximaciones que quedan (anotadas, no cambian el resultado del caso):
- ~~*Inferno Tempest*: sin ventana justo despues del danno~~: resuelto en la rama classic-reglas-oficiales. Hay una
  ventana justo despues del danno (`V_DANNO`) solo para el que lo recibio; Inferno Tempest, Attack and Receive y
  Numinous Healer solo se activan ahi (casos CU-BE-ATAJO-INFERNO-TEMPEST-MOMENTO y CU-BE-VENTANA-DANNO-*).
- *Fairy Guardian* devuelve la ultima magia de tu cementerio (no comprueba cual mando el rival este turno).
- *Pitch-Black Power Stone* pasa el contador a un monstruo tuyo que use contadores de magia (no a cualquier carta).
- *Level Conversion Lab* elige el monstruo de la mano de mas nivel.
