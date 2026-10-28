# Fondos fotorrealistas por personaje (rama classic-fondos)

El usuario vio los fondos en la PS5 y los rechazó todos: "son polígonos; pensé que sería un castillo real como de una
película". Además: "cada personaje debe tener su propio escenario, no puede repetir". Esta rama cambia el fondo por una
**imagen fotorrealista por personaje** (47, ninguna se repite) puesta como telón lejano, con su profundidad para un
paralaje sutil, y **sin nada de geometría por código delante**.

## Por qué así
- **Calidad de película**: un matte painting fotográfico (Gemini, la regla del usuario es que lo visual pasa por
  Gemini) tiene la luz, la atmósfera y el detalle que ni las siluetas por código (classic-entornos) ni las escenas de
  Blender con piezas CC0 (classic-escenarios) alcanzaron.
- **Uno por personaje**: 47 lugares distintos (tabla abajo), elegidos por dónde duela o vive cada uno en la serie.
- **Coste**: el mismo triángulo de pantalla que el cielo de siempre: 5 lecturas de la profundidad + 1 del color por
  píxel; la mayoría de los píxeles los tapa la mesa. Estimado en PS5 a 4K: < 0,3 ms de GPU. Una imagen a la vez en
  memoria (8192 x 3510 RGBA8 con mipmaps ≈ 150 MB; profundidad 2048 x 878 x 2 B ≈ 3,5 MB). Carga ≈ 0,5-1 s al
  cambiar de rival, con el fondo ya fundido a negro.

## Cómo se hace una imagen
1. `fondos_datos.py`: la escena de cada personaje en inglés (sin nombres de la franquicia: Gemini rechaza "por
   derechos de terceros"), su luz, su tema (la clave de entornos.h: partículas y luz base) y el lugar en ES/EN.
   `python -I fondos_datos.py` escribe `datos/fondos.txt` (`id|tema|respaldo|lugar_es|lugar_en`).
2. `cola_fondos.py` (con `herramientas/puente-gemini/cola_gemini.py`: pausa de 20 s, límite y silencio con reintento
   cada 30 min): prompt de solo texto -> `nano-banana\classic\fondos\<id>\resultado.png`. Prompt: fotografía de cine
   (matte painting), 21:9, el sujeto centrado en la banda del 15 % al 75 % de la altura, el cuarto de abajo liso (queda
   tras la mesa), sin personas, texto ni logos; prohibido el aspecto de dibujo/3D/low-poly. (Gemini da 3168 x 1344.)
   Orden: Pegasus, Kaiba, Yugi, Joey, Dartz, Marik, el resto y al final los 16 de tema.
   `--preparar` solo escribe los prompts; `--solo a,b,c` y `--max N` para pilotos. NO correr junto a otra cola en el
   mismo Chrome.
3. `fondos_post.py <id>` (lo lanza la cola tras cada imagen, uno por vez): si es más alta que 21:9 recorta cielo de
   arriba; Real-ESRGAN x4 (`realesrgan-x4plus`, el de fotos) si mide < 6000 px y LANCZOS a <= 8192; Depth Anything V2
   (ONNX, CPU) -> distancia log-lineal entre 45 m (tras la mesa) y 4000 m, el cielo sin distancia; colores del cielo,
   del horizonte (niebla del tablero) y de la luz (tinte). Deja en `classic-assets\fondos\`: `<id>.jpg`,
   `<id>_prof.png` (16 bits: log2(m)/12; 65535 = cielo) y `<id>.txt`. `--prueba` hace `prueba_plaza` y
   `prueba_valle` desde fotos CC0 de Poly Haven (piazza_san_marco, neurathen_rock_castle) para probar el motor.

## En el motor (`src/entornos.h`, `shaders/entornos_cielo.frag`)
- `personajes.txt`: el campo `entorno` de cada personaje es ahora **su id**. `Entornos::pedir(string)` acepta un id
  (busca su fila en `ui/fondos.txt`: tema + fondo) o una clave de tema como antes. Si el fondo del id aún no existe,
  usa su `respaldo`; si no hay ninguno, queda el entorno del tema de antes.
- **Proyección**: la imagen se envuelve en un cilindro alrededor de la cámara de juego: `u` por acimut (110° de ancho,
  centrado detrás del rival), `v` lineal en tan(elevación) con el borde de abajo a -28° y la altura que da el aspecto
  de la imagen. Es como un objetivo descentrado: las verticales quedan verticales. Con 21:9, desde la cámara de juego
  la mesa tapa el 25 % de abajo (hasta -18°) y arriba se ve hasta el 85 % (+9,7°): se ve la banda del 15 % al 75 % de
  la imagen; lo de arriba aparece al subir la cámara. `plano <ancho> <abajo>` del `.txt` lo ajusta por fondo. Fuera de los costados la imagen se espeja (cámaras lateral y libre), sobre el
  borde de arriba se funde con el color del cielo, y detrás de la cámara la costura se esconde en ese color.
- **Paralaje**: la misma reproyección por punto fijo de classic-escenarios con la profundidad de Depth Anything:
  exacta desde la cámara de juego; al moverse (respirar, CINE, libre) cada cosa se desplaza según su distancia.
- **Sin polígonos**: con fondo propio no se dibuja la obra del tema (suelo, siluetas, árboles) ni sus llamas; quedan
  solo las partículas sutiles del tema (polvo, brasas, niebla, hojas...).
- **Luz**: la niebla del tablero toma el color del horizonte de la imagen; la luz del sol y el relleno, mitad tema y
  mitad el tinte de la imagen (lo más claro y el cielo).
- PC: `YUGI_ENTORNO=<id>` (p. ej. `pegasus` o `prueba_plaza`) fija el fondo; `captura_entornos.sh` acepta ids.

## Rivales genéricos (duelo libre)
Tienen su set aparte de 16 fondos, uno por tema (`tema_<clave>`, lugares genéricos que no repiten ninguno de los 47),
generados al final de la cola. Mientras falte el suyo usan el de un personaje de ese tema (`respaldo`, temporal).

## Los 47 escenarios
| personaje | lugar |
|---|---|
| Pegasus | Castillo de pálida piedra con torres en la cima de la isla del Reino de los Duelistas, atardecer |
| Seto Kaiba | Cubierta del dirigible de Kaiba Corp sobre el mar de nubes |
| Yugi Muto | Calle de la tienda de juegos Kame en Ciudad Domino, al anochecer |
| Joey Wheeler | Muelle del puerto de Ciudad Domino (su duelo bajo control de Marik), atardecer |
| Mokuba Kaiba | Jardín de la mansión Kaiba |
| Téa Gardner | Patio de la preparatoria de Domino con cerezos |
| Solomon Muto | Interior de su tienda de juegos |
| Weevil Underwood | Bosque brumoso de la isla con telarañas |
| Rex Raptor | Meseta prehistórica volcánica |
| Bonz | Cementerio de la isla, de noche |
| Bakura Ryou | Aldea de fantasía medieval (Monster World) al anochecer |
| Mako Tsunami | Muelle de madera en el océano, tormenta que abre |
| PaniK | Claro del bosque con braseros, de noche |
| Hermanos Paradoja | Laberinto subterráneo de piedra |
| Shadi | Ruinas de un templo egipcio bajo la luna |
| Mai Valentine | Pradera en un acantilado sobre el mar |
| Bandit Keith | Almacén industrial abandonado (EE. UU.), de noche |
| Espa Roba | Plaza del centro de Ciudad Batallas |
| Duke Devlin | Calle de juegos con neones de dados y cartas, de noche |
| Arkana | Mazmorra-teatro de un mago |
| Lumis y Umbra | Azotea de un rascacielos de noche |
| Ishizu Ishtar | Exposición egipcia del Museo de Domino |
| Marik Ishtar | Santuario subterráneo de los guardianes de la tumba |
| Yami Marik | Torre de duelo en la isla de Alcatraz |
| Yami Bakura | Páramo con abadía en ruinas bajo luna roja |
| Noah Kaiba | Ciudad de cristal del mundo virtual |
| Gozaburo Kaiba | Fortaleza corporativa en la tormenta |
| Los Cinco Grandes | Glaciar ártico (mundo virtual) |
| Tristan Taylor | Selva con cascada (mundo virtual) |
| Serenity Wheeler | Pradera con cerezos (mundo virtual) |
| Dartz | Atlántida y el monolito del Oricalco |
| Rafael | Isla del naufragio |
| Valon | Carretera del desierto con mesetas |
| Alister | Ciudad en ruinas por la guerra |
| Rebecca Hawkins | Calle soleada de San Francisco |
| Leon von Schroeder | Estadio del Gran Campeonato en Kaiba Land |
| Zigfried von Schroeder | Castillo gótico alemán con rosaleda |
| Yami Yugi | Sala de la Ceremonia de Batalla |
| Faraón Aknamkanon | Sala del trono del faraón |
| Sacerdote Seto | Patio del gran templo con pilonos |
| Sacerdote Mahad | Tumba real subterránea |
| Mana | Jardines del palacio junto al Nilo |
| Kisara | Desierto estrellado con un pozo |
| Bobasa | Salón de banquetes del palacio |
| Altos Sacerdotes | Santuario de los Artículos del Milenio |
| Bakura, Rey de los Ladrones | Kul Elna, la aldea en ruinas |
| Zorc Necrophades | Ciudad egipcia en llamas bajo un cielo oscuro |

## Estado (2026-10-19)
- Los 63 generados con Gemini (47 personajes + 16 de tema), 21:9 a 3168 x 1344, con post-proceso (8192 x 3474,
  profundidad 2048 x 868). Revisión visual: fotorrealistas, sin personas. Rehechos por tener texto: Yugi (letrero),
  Solomon (cartel de la tienda), Duke (neones con letras), Valon (cartel de la gasolinera). Las versiones descartadas
  quedan como `resultado_v1_con_texto.png`.
- Pendiente de valorar en la PS5: el encuadre (`plano 110 -28`) por fondo; si alguno queda alto o bajo se cambia el
  segundo número en su `.txt` (más negativo = la imagen sube en pantalla).
