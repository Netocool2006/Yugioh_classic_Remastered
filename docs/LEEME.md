# Índice de la documentación / Documentation index

Yugioh Classic Remaster. Resumen general: [`README.md`](../README.md) (español) · [`README.en.md`](../README.en.md)
(English). Los documentos de detalle están en [`remake-ps5/docs/`](../remake-ps5/docs/) y están escritos en español.
The detailed documents live in `remake-ps5/docs/` and are written in Spanish.

Leyenda / Legend: **vigente** = describe el estado actual / describes the current state · **histórico** = registro de una
rama, de un ciclo o de una decisión ya cerrada; útil para entender el porqué, pero puede no coincidir con los números
actuales / a log of a branch, cycle or closed decision; useful for the "why", but numbers may be out of date.

## Duelo, reglas y cartas / Duel, rules and cards

| Documento | Tema / Topic | Estado |
|---|---|---|
| [efectos.md](../remake-ps5/docs/efectos.md) | Motor de efectos (`DefCarta`, cadena, ventanas, IA de respuestas), tandas de cartas, atajos / Effects engine, chain, windows, card batches, shortcuts | vigente (largo; el contador de cartas es histórico: el real está en la certificación) |
| [certificacion_efectos.md](../remake-ps5/docs/certificacion_efectos.md), [.csv](../remake-ps5/docs/certificacion_efectos.csv) | Certificación carta por carta (1395 OK) / Per-card certification | vigente (se regenera con `certificar.sh`) |
| [decisiones_pendientes.md](../remake-ps5/docs/decisiones_pendientes.md) | Decisiones abiertas y resueltas (regla oficial, mazos de torneo, línea) / Open and resolved decisions | vigente + histórico |
| [control_final.md](../remake-ps5/docs/control_final.md) | Control final del 20-10-2026: 30 cartas sin prueba, errores del motor, aproximaciones / Final control | histórico |

## Rivales, personajes y mazos / Opponents, characters and decks

| Documento | Tema / Topic | Estado |
|---|---|---|
| [rivales.md](../remake-ps5/docs/rivales.md) | 40 rivales en 8 niveles, 120 mazos, progresión de dificultad / 40 generic opponents | vigente |
| [personajes.md](../remake-ps5/docs/personajes.md) | Modo historia, 47 personajes, sagas, cartas firma / Story mode, characters | vigente (el recuento de cartas firma está desactualizado: son 81) |
| [mazos_torneo.md](../remake-ps5/docs/mazos_torneo.md) | 21 mazos de torneo curados y sus fuentes / 21 curated tournament decks | vigente (recuento de cartas extra desactualizado: son 13) |
| [constructor.md](../remake-ps5/docs/constructor.md) | Constructor de mazos al estilo Master Duel / Master Duel-style deck builder | vigente |

## Inteligencia artificial / AI

| Documento | Tema / Topic | Estado |
|---|---|---|
| [ia_red.md](../remake-ps5/docs/ia_red.md) | Red de valor, entrenamiento por autojuego, ajuste fino al jugador / Value network, self-play training | vigente |
| [jugador_humano.md](../remake-ps5/docs/jugador_humano.md) | Probador que juega como una persona (oráculos `CU-HU-*`) / Human-like test player | vigente |

## Presentación estilo Master Duel / Master Duel-style presentation

| Documento | Tema / Topic | Estado |
|---|---|---|
| [md_ritmo.md](../remake-ps5/docs/md_ritmo.md) | Tabla de tiempos de Master Duel y de *Joey the Passion*, ritmo del turno, combate / Timing tables | vigente |
| [md_fases.md](../remake-ps5/docs/md_fases.md) | Selector de fases, engranaje, carteles / Phase selector, gear menu, banners | vigente |
| [md_entradas.md](../remake-ps5/docs/md_entradas.md) | Entradas de fusión y ritual, menú del duelo, fin del duelo / Fusion and ritual entrances | vigente |
| [md_efectos.md](../remake-ps5/docs/md_efectos.md) | Efectos de cartas traducidos de MD (`CardRunEffect`, pantallas, sacudidas) y pendientes / Translated card effects | vigente |
| [md_inventario_visual.md](../remake-ps5/docs/md_inventario_visual.md) | Inventario de lo visual del duelo de MD y su estado / Visual inventory | vigente (inventario) |
| [ataque_md.md](../remake-ps5/docs/ataque_md.md) | Ataque directo, ataque de ~3 s como Joey, rayos de las figuras, elevación del atacante (cinco vueltas) / Attack | vigente + histórico (por vueltas) |

## Mundo 3D, fondos y figuras / 3D world, backgrounds and figures

| Documento | Tema / Topic | Estado |
|---|---|---|
| [fondos.md](../remake-ps5/docs/fondos.md) | Fondos fotorrealistas por personaje (Gemini + profundidad) / Photorealistic backgrounds | vigente |
| [entornos.md](../remake-ps5/docs/entornos.md) | Claves de entorno por personaje (luz, partículas) / Environment keys | vigente (el fondo ahora es una imagen: ver `fondos.md`) |
| [campos.md](../remake-ps5/docs/campos.md) | 25 cartas de campo que cambian el escenario / Field cards | vigente |
| [escenarios.md](../remake-ps5/docs/escenarios.md) | Fondos pre-renderizados con Blender + Cycles | histórico (sustituido por `fondos.md`) |
| [figuras_pendientes.md](../remake-ps5/docs/figuras_pendientes.md) | Cartas emblemáticas/firma sin figura 3D; lista de las 92 con figura / Figures status | vigente (generado por `emblematicas_gen.py`) |

## Modo en línea / Online mode

| Documento | Tema / Topic | Estado |
|---|---|---|
| [red.md](../remake-ps5/docs/red.md) | Base de red: canal, lockstep, compromiso de barajado / Network base | vigente |
| [en_linea.md](../remake-ps5/docs/en_linea.md) | Búsqueda de rival: sala en Cloudflare + DHT, ELO, pantallas / Matchmaking | vigente |
| [`servidor/sala/LEEME.md`](../remake-ps5/servidor/sala/LEEME.md) | Desplegar y probar la sala / Deploy and test the room | vigente |

## Pruebas / Tests

| Documento | Tema / Topic | Estado |
|---|---|---|
| [casos_de_uso.md](../remake-ps5/docs/casos_de_uso.md) | Árbol de casos de uso (CU-FE, CU-BE, CU-CARTA, CU-RED); cada hoja tiene prueba / Use-case tree | vigente |
| [ciclos_pruebas.md](../remake-ps5/docs/ciclos_pruebas.md) | Resultados de los ciclos completos (7008-7088 casos, 0 fallas) / Cycle results | histórico |
| [fallas_front_total.md](../remake-ps5/docs/fallas_front_total.md) | Fallas halladas por la batería desde el front y su arreglo / Failures found by the front-end battery | histórico |
| [duelo_log.md](../remake-ps5/docs/duelo_log.md) | Bitácora del duelo (`duelo_log.txt`) y su lector / Duel log | vigente |

## PC, instalador y consola / PC, installer and console

| Documento | Tema / Topic | Estado |
|---|---|---|
| [pc_jugable.md](../remake-ps5/docs/pc_jugable.md) | Versión de Windows: plataforma, mando, audio, ratón, Alt+Enter, qué no se verificó / Windows version | vigente |
| [`pc/README.md`](../remake-ps5/pc/README.md) | Resumen de `pc/` | desactualizado: aún dice que falta el instalador (ya existe) |
| [`pc/instalador/LEEME.md`](../remake-ps5/pc/instalador/LEEME.md) | Instalador Inno Setup: armado, opciones, compresión / Installer | vigente |
| [imagen_titulo.md](../remake-ps5/docs/imagen_titulo.md) | Imagen exFAT del título, ShadowMount+, subida y qué escribe el juego en `/data` / Title image | vigente |

## Otros / Other

- [`third_party/LICENCIAS_*.md`](../remake-ps5/third_party/): licencias de bosque, campos, entornos y PC / third-party licences.
- Cómo trabajar (construir, subir, probar, añadir cartas/figuras/personajes): secciones 5 a 7 de [`README.md`](../README.md).
