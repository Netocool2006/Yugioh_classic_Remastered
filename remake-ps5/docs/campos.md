# Cartas de campo que cambian el escenario (rama classic-campos)

Pedido del usuario: que cada carta de campo cambie el escenario completo al activarse, como ya hacía el Bosque
(le encantó). Son las 25 de `magia_campo` en `cartas_classic.txt`.

## Qué pasa al activarse una carta de campo
1. La carta se levanta, se voltea y brilla; baja la columna de luz y sale la onda (la misma máquina de estados del
   Bosque en `yugireload.cpp`, ahora con `idCampo`/`ladoCampo`; los colores de la carta, la onda y los sellos son los
   del campo: azul Umi, naranja lava, violeta Yami, verde Oricalco...).
2. **Fondo**: el del rival se funde y entra el de la carta (`entornos.h: pedirCampo("campo_<carta>")`, un matte
   painting de Gemini como los 63 de los personajes; su tema da la luz del tablero y las partículas del anillo). Al
   destruirse (`soltarCampo()`) vuelve el del personaje. Lo que se pida mientras tanto (otro rival) queda guardado.
   La imagen se decodifica ahora **en otro hilo** (jpg de 8192 px + mipmaps, ~0,5-1 s de CPU): el duelo no se traba
   al cambiar de fondo (también vale para el cambio de rival).
3. **Tablero**: detrás del frente de la onda el tapete se cubre con el material de la carta (`campos.frag`, una capa
   sobre el tapete, sin escribir profundidad). Las casillas siguen a la vista: dentro el material se oscurece (las
   cartas se leen) y el marco brilla con el color del campo. El canto del tapete queda igual.
4. **Partículas cercanas** (`campos_particula.*`): pocas (60-160), chicas, a ras del tapete y **entre** las
   casillas (sobre una casilla se apagan): no tapan las cartas ni el HUD.
5. Al destruirse: la carta estalla, la onda de retiro recoge el material y vuelve el tapete exacto de antes (con
   ambiente 0 la capa no se dibuja).

## Las 25 cartas
| carta | tablero | partículas | fondo (`campo_<id>`) | tema (luz) |
|---|---|---|---|---|
| Forest | (su mundo 3D propio, `bosque.h`, sin cambios) | hojas, luciérnagas | bosque profundo (generado; el Bosque sigue usando su cielo propio) | isla_bosque |
| Sogen | césped que el viento peina en ráfagas | briznas, polen | praderas infinitas | isla_bosque |
| Mountain | roca con manchas de nieve | polvo de nieve | cumbres rocosas | dirigible_kaiba |
| Umi | agua profunda con olas, reflejo del cielo, destellos y espuma | gotas, burbujas | océano profundo | playa_mar |
| Wasteland | tierra agrietada reseca, polvo que pasa | polvo | páramo y cañones | templo_egipcio |
| Yami | piedra oscura y niebla violeta que se arrastra | niebla, motas violetas | la oscuridad | reino_sombras |
| A Legendary Ocean | agua clara con las ruinas (losas) y cáusticas debajo | gotas, burbujas | ciudad sumergida | atlantida_oricalco |
| Umiiruka | mar crecido turquesa, olas más fuertes y espuma | gotas, burbujas | mar crecido | playa_mar |
| Rising Air Current | mar de nubes que corre con el viento, rayas de viento | polvo blanco, plumas | sobre las nubes | dirigible_kaiba |
| Gaia Power | pasto y roca con venas de vida verdes que laten | motas verdes, hojas | la tierra viva | isla_bosque |
| Luminous Spark | mármol con destellos que titilan | motas, chispas | reino de cristal y luz | templo_egipcio |
| Molten Destruction | tierra quemada en placas con grietas de lava que fluye y late | brasas, humo | volcán en erupción | torre_alcatraz |
| Mystic Plasma Zone | pizarra y metal con filamentos de plasma violeta | chispas de plasma | tormenta de plasma | reino_sombras |
| Chorus of Sanctuary | mármol con incrustaciones de oro que brillan | motas doradas, plumas | santuario blanco | templo_egipcio |
| The Sanctuary in the Sky | mármol con nubes que pasan por encima | motas, niebla blanca | santuario sobre las nubes | dirigible_kaiba |
| Dark Sanctuary | piedra oscura, losas, aro de runas violetas | niebla, motas | santuario oscuro | reino_sombras |
| Necrovalley | losas de arenisca con relieves (jeroglíficos por código) y arena que las cubre | arena | valle de las tumbas | templo_egipcio |
| Pandemonium | piedra negra con grietas rojas encendidas | brasas, humo rojo | castillo infernal | reino_sombras |
| Jurassic World | suelo de selva y pasto con charcos que reflejan | hojas, vapor | valle jurásico | isla_bosque |
| Harpies' Hunting Ground | roca de acantilado con plumas sueltas | plumas, polvo | riscos con nidos | dirigible_kaiba |
| Golden Castle of Stromberg | baldosas de oro pulido que reflejan | motas y chispas doradas | castillo de oro | reino_duelistas_castillo |
| The Seal of Orichalcos | piedra oscura con el sello (doble círculo, hexagrama y glifos) que late en verde | chispas verdes | plaza del sello en la Atlántida | atlantida_oricalco |
| Fusion Gate | metal con un vórtice en espiral azul, violeta y oro | chispas, motas | vórtice dimensional | reino_sombras |
| Centrifugal Field | metal con anillos concéntricos cuyos segmentos giran | chispas cian, polvo | arena centrífuga | rascacielos_noche |
| Array of Revealing Light | mármol y losas con un círculo mágico de luz grabado | motas | sala con el círculo de luz | templo_egipcio |

Mientras falte el fondo propio de una carta se usa su `respaldo` (un fondo ya hecho de la misma familia, ver
`datos/fondos.txt`): así se probó el motor antes de la cola de Gemini.

## Lado (decisión)
La **última** carta de campo activada manda y cambia **todo** el escenario (fondo y tablero entero), sea del
jugador o del rival: en las reglas Goat solo hay una carta de campo a la vez (activar otra manda la anterior al
cementerio). Es lo más legible: un solo mundo a la vez, sin medio tablero de agua y medio de lava. La onda sale de la
zona de campo de quien la activó (la carta del rival mira hacia él). Si llega una carta nueva con otra activa, la
nueva la reemplaza (su onda crece de cero); la destrucción de una carta que ya no manda no hace nada.

## Texturas y coste
- `campos_insumos.py`: 12 texturas CC0 de Poly Haven (ver `third_party/LICENCIAS_campos.md`) a 512x512 en un
  arreglo de 24 capas (color + oclusión, normal GL + rugosidad) con mipmaps: ~34 MB en la GPU. Faltan → neutras.
- GPU (estimado PS5 a 4K): la capa es un cuadro sobre el tapete (lo que se ve de él, ~40 % de la pantalla): 2-4
  lecturas de textura, 4 de sombra y ruido de valor barato por píxel (el agua y las nubes, las más caras: ~12
  lecturas de ruido); + 2 dibujos de 60-160 partículas. Del orden de 0,5-1 ms; con el Bosque no se dibuja (tiene lo
  suyo). El fondo cuesta lo mismo que el del personaje (lo reemplaza).

## Pruebas en el PC
- `YUGI_CAMPO=<carta>[,rival]` elige la carta de la demo; `YUGI_CAMPO_BOSQUE=1` la activa al arrancar;
  `YUGI_CAMPO_CICLO=<s>` activa y destruye en bucle; `YUGI_CAMPO_MEDIR=1` imprime el coste.
- `captura_campos.sh <remake-ps5> <salida> <cuadro> umi yami,rival ...` (en el contenedor, con candado).
- Fondos: `python -I cola_fondos.py --campos` (25, SOLO cuando no corra otra cola en el Chrome del puente) y
  `fondos_post.py` como siempre.

## Estado (2026-10-19)
- Los 25 fondos `campo_<carta>` generados con Gemini (cola_fondos.py --campos) y post-procesados (8192 x 3476 +
  profundidad). Revisados: fotorrealistas, sin personas ni texto, cada uno acorde a su carta (el de Necrovalle tiene
  relieves de figuras, sin escritura legible).
- Capturas de PC dentro del duelo (`captura_campos.sh`, cuadro 340): Umi, Destrucción fundida, Sello de Oricalco,
  Necrovalle, Yami (del rival), Sogen, Océano legendario, Zona de plasma, Santuario en el cielo, Puerta de fusión.
- Pendiente de valorar en la PS5 (el usuario): tono de cada material, intensidad de partículas, coste real de GPU.
