# Entornos por personaje rival (rama classic-entornos)

Cuando se duela contra un personaje del anime, TODO lo que rodea al tablero (fondo/cielo, suelo, luz, siluetas 3D y
partículas) es el del lugar donde ese personaje duela en la serie. Lo hace `src/entornos.h`; los insumos (texturas
CC0 de Poly Haven) los baja `entornos_insumos.py` a `<classic-assets>/entornos/` y los copia `sincronizar.sh`.

## Contrato con classic-personajes
- Cada personaje lleva un campo **`entorno`** con UNA de las claves de abajo (texto exacto, minúsculas, sin tildes).
- El juego llama `entornos::claveDe(entorno)`; una clave desconocida o vacía da `neutro` (el espacio con estrellas de
  siempre).
- Rivales genéricos (rivales.txt, sin personaje): el entorno sale de su `tema` (`entornos::porTema`, tabla al final).
- La carta de campo manda: mientras está activa se ve su entorno (hoy el Bosque); al destruirla vuelve el del rival.
- Nada del entorno tapa el tablero, el retrato del personaje (arriba a la derecha) ni el HUD.

## Claves
| clave | lugar en la serie | personajes |
|---|---|---|
| `neutro` | (sin personaje: el espacio oscuro con estrellas) | genéricos sin tema |
| `tienda_juegos` | Tienda de juegos Kame / calles de Ciudad Domino, de día | Solomon Muto, Yugi/Yami Yugi, Téa, Tristan |
| `reino_duelistas_castillo` | Castillo de Pegasus, Reino de los Duelistas (patio y torres, atardecer) | Pegasus, Mai Valentine, Kemo y guardias |
| `isla_bosque` | Bosque y pradera de la Isla del Reino de los Duelistas | Weevil Underwood, Rex Raptor, Panik?* |
| `playa_mar` | Costa y mar de la isla (plataforma en el agua) | Mako Tsunami |
| `acantilado_noche` | Acantilados, cuevas y cementerio de la isla, de noche | Bandit Keith, Bonz, Sid, Zygor, Panik |
| `laberinto` | Laberinto subterráneo de los Hermanos Paradoja | Para y Dox |
| `rascacielos_noche` | Azotea de rascacielos de Ciudad Domino de noche (Battle City) | Lumis y Umbra (Mimos), Arkana |
| `ciudad_batallas` | Calles y plazas de Battle City de día | Joey Wheeler, Espa Roba, Duke Devlin, Bonz/Sid de BC, Mokuba (calle), Mai (BC) |
| `dirigible_kaiba` | Arena en la cubierta del dirigible de Kaiba Corp sobre las nubes (finales de BC) | Seto Kaiba, Mokuba, Joey (finales), Mai (finales), Bakura (finales) |
| `torre_alcatraz` | Torre de duelo de la isla Alcatraz de Kaiba, sobre el mar al atardecer | Marik Ishtar (Yami Marik), Odion |
| `museo_egipcio` | Exposición egipcia del Museo de Domino | Ishizu Ishtar, Shadi |
| `mundo_virtual_noah` | Mundo virtual de Noah (datos verdes, cielo digital) | Noah Kaiba, Gozaburo Kaiba, los Cinco Grandes (Gansley, Crump, Johnson, Nesbitt, Leichter) |
| `atlantida_oricalco` | Atlántida / Doma: ruinas y el brillo verde del Oricalco | Dartz, Rafael, Valon, Alister |
| `estadio_kc` | Estadio del Gran Campeonato KC (Kaiba Land) | Zigfried von Schroeder, Leon Wilson, Rebecca Hawkins, Vivian Wong |
| `templo_egipcio` | Palacio y templo del Egipto antiguo, a pleno sol (Memorias del Faraón) | Faraón Atem, Aknamkanon, Seto/sacerdotes (Karim, Shada, Isis, Mahad), Mana, Kisara |
| `reino_sombras` | Reino de las Sombras: Egipto oscuro con Zorc (cielo violeta, niebla negra) | Zorc Necrophades, Bakura Rey Ladrón, Yami Bakura, Pegasus (juego de sombras) |

\* Si un personaje no está en la tabla, usar el lugar donde dueló más en la serie, o `neutro`.

## Rivales genéricos: tema → entorno (`entornos::porTema`)
| tema contiene | entorno |
|---|---|
| Insecto, Planta, Bestia, Guerrero-Bestia, Dinosaurio, Reptil, Roca | `isla_bosque` |
| Pez, Aqua, Serpiente Marina, attr:AGUA | `playa_mar` |
| Zombi, sub:espiritu | `acantilado_noche` |
| Demonio, attr:OSCURIDAD, caos | `reino_sombras` |
| Máquina, Trueno, sub:union | `ciudad_batallas` |
| Dragón, Bestia Alada, attr:VIENTO | `dirigible_kaiba` |
| Lanzador de Conjuros, Hada, attr:LUZ, ritual | `templo_egipcio` |
| Guerrero, sub:volteo, quema, attr:TIERRA, attr:FUEGO | `reino_duelistas_castillo` |
| toon | `tienda_juegos` |
| fusion, goat | `estadio_kc` |
| (otro) | `neutro` |

## Pruebas en el PC
`YUGI_ENTORNO=<clave>` fuerza el entorno al arrancar; `YUGI_ENTORNO_CICLO=<s>` los recorre todos cada `<s>` s.
Capturas fijas: `captura_entornos.sh` (una por entorno, camara de juego).
