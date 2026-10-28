# Control final (Yugioh Classic Remaster, PPSA77002)

Rama `classic-control`, creada desde `classic-integracion` (9fe0a1f), con todo ya unido. Se hizo una sola vez, el
20-10-2026, dentro del contenedor `arch-ps5`. Nada se subió a la PS5.

## 1. Compilación de la integración

`probar_rama.sh … - 0` sobre 9fe0a1f: **COMPILADO** (PS5 y vista previa de PC). El único aviso es el de siempre de
`stb_vorbis.c` (comparación de punteros), que es código de terceros. Al terminar, con los arreglos de este control,
se compiló de nuevo (ver la sección 6).

## 2. Las cartas SIN PRUEBA de la certificación

El CSV del repositorio estaba desactualizado (32 SIN PRUEBA). Al correr de nuevo la certificación sobre 9fe0a1f, el
resultado fue **OK 1355 / FALLA 0 / SIN PRUEBA 30**. Neko Mane King, Gravekeeper's Vassal y Spiritual Energy Settle
Machine ya tenían su caso (rama motor2) y Gamble había pasado a SIN PRUEBA.

Se escribió un caso a mano para cada una de las 30 en `tandaControlFinal` (`src/pruebas/prueba_efectos.cpp`), más
uno nuevo para Spiritual Energy Settle Machine (su efecto al dejar el campo). Cada caso arma la condición exacta
del texto y también comprueba una situación en la que la carta no debe hacer nada:

| carta | condición armada | qué se comprueba |
|---|---|---|
| Centrifugal Field | campo boca arriba, Flame Swordsman destruido por el Raigeki del rival | Masaki vuelve del cementerio; destruida en batalla, no |
| Combination Attack | X-Head Cannon con Y-Dragon Head equipado, ya atacó | la unión vuelve en ataque y X puede atacar otra vez; sin haber atacado, no se activa |
| Convulsion of Nature | boca arriba, robo | se ve la carta de arriba de los dos mazos; al quitarla, ya no |
| Covering Fire | el rival ataca a Celtic; Summoned Skull boca arriba | Celtic llega a 3900 en el paso de daño, Battle Ox muere, 2200 al rival y el bono no dura |
| Dark Sage | Time Wizard + Dark Magician; 30 semillas | si acierta: sacrifica DM, Dark Sage de la mano y una magia del mazo; si falla, nada |
| Dice Re-Roll | 40 semillas, tirada del rival | dado 1-6; repite solo si al rival le conviene y una sola vez por turno |
| Dora of Fate | en el turno del rival, sobre su Battle Ox (nivel 4) | en tu turno, invocar normal un nivel 3 = 2000; un nivel 4 = 0; en tu turno no se activa |
| Dramatic Rescue | Book of Moon del rival sobre Amazoness Swords Woman | la Amazoness vuelve a la mano y Battle Ox se invoca; sobre otro monstruo, no |
| Driving Snow | MST del rival destruye tu Mirror Force | destruye las Swords del rival; antes no se puede activar |
| Fairy Guardian | MST del rival manda tus Swords al cementerio en tu turno | las Swords al fondo del mazo; sin magia perdida, no se activa |
| Fiend's Hand Mirror | MST del rival sobre tu Mirror Force | eliges Trap Hole como nuevo objetivo: Mirror Force queda y Trap Hole se va |
| Flame Ruler / Whirlwind Prodigy | Ryu-Ran (FUEGO 7) / Kazejin (VIENTO 7) | invocan con un solo sacrificio; con el otro atributo, no |
| Formation Union | unión equipada / unión en el campo | se separa en ataque / se equipa a X-Head Cannon; sin uniones, no |
| Inferno Tempest | Blue-Eyes ataca directo (3000) | se destierran los monstruos de los 2 mazos y cementerios y quedan las magias; con 1700, no |
| Level Conversion Lab | Blue-Eyes en la mano; 60 semillas | salen los 6 valores; 1 = al cementerio; 2-6 = ese nivel solo este turno (≤4 sin sacrificio) |
| Light of Judgment | The Sanctuary in the Sky | descartar un no-LUZ se rechaza; con un LUZ: un monstruo o una carta de la mano del rival (elegida) al cementerio; sin el Santuario, no |
| Malice Doll of Demise | sacrificada por Mass Driver y por Ectoplasmer | vuelve en tu siguiente fase de espera; destruida por Raigeki, no |
| Mind on Air | boca arriba, el rival roba | se ve la mano del rival y la tuya no |
| Ninjitsu Art of Transformation | sacrifica a Strike Ninja (nivel 4) | invoca Big Koala (nivel 7) y no Metal Armored Bug (nivel 8); al irse la trampa, Big Koala se destruye; con un no-Ninja, no |
| Pikeru's Second Sight | activada; robos del rival | se ve lo que roba hasta su segunda fase final; después se va |
| Pitch-Black Power Stone | Skilled Dark Magician | llega con 3 contadores; pasa 1 por turno; con 0 se destruye |
| Sasuke Samurai #4 | ataca a Blue-Eyes; 30 semillas | si acierta, Blue-Eyes muere antes del cálculo; si falla, Sasuke muere y recibes 1800 |
| Second Coin Toss | 30 semillas con la moneda directa y 30 con la cantada (Sasuke) | repite solo si sale mal, una vez por turno; decide la segunda moneda |
| Serial Spell | en cadena a tu Pot of Greed | descarta la mano y roba 2 + 2; con la magia del rival, no |
| Shifting Shadows | 3 boca abajo + 1 boca arriba; 20 semillas | paga 300, reordena solo los boca abajo (los mismos, boca abajo en defensa) y una sola vez por turno |
| Spirit Elimination | invocar Silpheed (destierra un VIENTO del cementerio) | se destierra Battle Ox del campo; al turno siguiente, del cementerio |
| Spirit's Invitation | Asura Priest vuelve en la fase final | el monstruo más débil del rival vuelve a su mano; en la espera paga 500, o con 400 LP se destruye |
| Spiritual Energy Settle Machine | Asura Priest invocado | no vuelve; en tu fase final descarta 1; sin mano se destruye y Asura vuelve |
| The Hunter with 7 Weapons | declara Dragón (pregunta) | +1000 contra Baby Dragon (gana, 800); contra Celtic pierde (400) |
| Gamble | rival con 6, tú con 1; 30 semillas | si acierta, roba hasta 5; si falla, se salta el próximo turno; con 5 en la mano del rival, no |

## 3. Errores del motor encontrados y arreglados (ia.h)

1. **Centrifugal Field no hacía nada nunca.** Es una magia de campo y el motor la buscaba en las zonas de
   magia/trampa (`hayBocaArriba`). Ahora la busca en la zona de campo de cualquiera de los dos.
2. **Light of Judgment y Archlord Zerato** dejaban descartar cualquier carta, y el texto pide un monstruo de LUZ.
   La nueva función `descarteVale` lo exige al activar (`activarCarta`), cuando elige la IA (`descartesIA`) y en el menú
   del jugador (`control.h`, ya no ofrece cartas que no valen).
3. **Pikeru's Second Sight** se quedaba para siempre. Ahora cuenta las fases finales del rival y se va en la segunda.
4. **Ninjitsu Art of Transformation** se iba al cementerio al resolverse. Ahora es continua: queda vinculada al
   monstruo que invoca y lo destruye al dejar el campo. Si no hay nada que invocar, se va.
5. **Spiritual Energy Settle Machine**: al dejar el campo, los Spirit boca arriba no volvían a la mano. Ahora vuelven.
6. **Shifting Shadows** se podía usar dos veces en el turno en que se activaba (la activación ya baraja). Ahora la
   activación cuenta como el uso del turno.
7. **Malice Doll of Demise** no volvía si la sacrificaba Ectoplasmer (una magia continua que actúa fuera de la
   cadena). Ahora vuelve.

## 4. Batería completa (una vez, dentro del contenedor)

| prueba | resultado | datos |
|---|---|---|
| prueba_juego | **OK** (0 fallos) | front y visores; se jugaron hasta 5 turnos |
| prueba_efectos | **OK** (0 fallos) | 871 casos; 1360 cartas en el motor, todas con caso |
| prueba_constructor | **OK** (0 fallos) | |
| prueba_duelo | **OK** (0 fallos) | 40 duelos por mando: jugador 8, rival 32, 0 colgados; 604 jugadas por mando, 63 activaciones |
| prueba_mando | **OK** (0 fallos) | |
| prueba_variedad | **OK** (0 fallos) | 240 duelos con planes, 0 sin terminar, 0 jugadas rechazadas; adaptable: rival 50 % / 27 % |
| prueba_ia | **OK** (0 fallos) | 800 duelos IA contra IA, 0 sin terminar, 0 rechazadas, 644 resultados distintos; 120 mazos de 40 rivales |
| prueba_ia_humana (20 pares = 40 duelos) | **OK** (0 fallos) | la humana gana 28 y la clásica 12 (70 %); 14,7 ms de media por decisión (máx. 99 ms); errores por nivel de 0 % a 1,7 % |
| certificación (`certificar.sh`) | **OK** | ver la sección 5 |

No falló nada, así que no hubo que arreglar ninguna prueba desactualizada. El fallo de Kotodama que se había visto
antes en prueba_duelo no volvió a salir: 40 duelos y 0 fallos.

## 5. Certificación de efectos

**1385 cartas con efecto: OK 1385 (100 %), FALLA 0, SIN PRUEBA 0.** La meta era cerca del 100 % y se cumplió.

- Casos a mano: 871, 0 fallos. Escenas presentadas en el front: 1668, 0 con fallas.
- De las OK: 428 por caso y plantilla, 781 solo por su caso a mano y 176 solo por la plantilla.
- Antes de este control: 1355 / 0 / 30 (y el CSV del repositorio todavía decía 1284 / 67 / 32).
- `docs/certificacion_efectos.md` y `docs/certificacion_efectos.csv` están regenerados. La revisión escrita a mano
  (`src/pruebas/certificacion_notas.md`, que va al reporte) tiene una sección nueva, "Control final".

## 6. Pendiente (aproximaciones conocidas, no son fallos)

- ~~**Inferno Tempest**: sin ventana justo después del daño~~: resuelto en la rama classic-reglas-oficiales (ventana
  `V_DANNO`, ver `efectos.md`).
- **Fairy Guardian** devuelve la última magia de tu cementerio. No comprueba que sea la que mandó el rival.
- **Pitch-Black Power Stone** solo pasa contadores a monstruos tuyos que usan contadores de magia.
- **Level Conversion Lab** elige el monstruo de la mano de más nivel. **Light of Judgment**: la IA elige la carta
  con `iaMejor`.
- Las aproximaciones de las ramas anteriores siguen anotadas, una por una, en `certificacion_notas.md`.
- La validación en la PS5 la hace el usuario. Nada de esto se subió a la consola.

Compilación final con los arreglos: **COMPILADO** (el mismo aviso de `stb_vorbis.c`). Commit en la rama `classic-control`, sin push.
