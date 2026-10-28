# Personajes del anime como rivales (rama classic-personajes)

## Qué hay
- **47 personajes** de la serie original (todas las fotos de `D:\Yugi-Reload\Personajes` salvo `portada.webp`, que
  es el logotipo, y `Mago-oscuro.jfif`, que es el ka de Mahad y sirve de segunda referencia para su retrato).
  Los grupos son **un rival cada uno**: Bonz (con Sid y Zigor: la foto es solo de Bonz), Hermanos Paradoja,
  Lumis y Umbra, Los Cinco Grandes y los Altos Sacerdotes. Las fotos de grupo son muy pequeñas para sacar un retrato
  por miembro; cada miembro tiene una variante de mazo cuando se pudo (Cinco Grandes: Crump, Nesbitt; Lumis/Umbra).
  **Odion no tiene foto** en la carpeta: no entra (se puede añadir con una foto).
- **Modo historia** (el modo por defecto): 6 sagas en orden y con dificultad creciente. Una saga se abre al vencer a
  todos los de la anterior; dentro de una saga abierta se elige a cualquiera (están de menor a mayor nivel).
  **Duelo libre**: los 40 rivales genéricos por nivel (rama classic-ia), como antes. Triángulo cambia de modo.
- Cada personaje tiene **3 variantes de mazo**. La 0 es "la suya" (sale el 60 % de las veces) y las otras dos se
  reparten el 40 % restante (`ia::Base::mazoDe`).
- **78 cartas firma** nuevas además de las 1690 Goat (`datos/cartas_firma.txt`).

| Saga | Personajes (nivel 1-8) |
|---|---|
| Reino de los Duelistas | Téa 1, Mokuba 1, Solomon 2, Weevil 2, Rex 2, Bonz 2, Bakura Ryou 2, Mako 3, PaniK 3, Hermanos Paradoja 3, Shadi 3, Mai 3, Joey 4, Bandit Keith 4, Kaiba 5, Pegasus 5 |
| Ciudad Batallas | Espa Roba 3, Duke 3, Arkana 4, Lumis y Umbra 4, Ishizu 5, Marik 5, Yami Bakura 6, Yami Marik 6 |
| Noah | Tristan 2, Serenity 2, Cinco Grandes 5, Gozaburo 6, Noah 6 |
| Despertar de los Dragones | Rebecca 4, Valon 5, Alister 5, Rafael 6, Yugi 6, Dartz 7 |
| Gran Campeonato KC | Leon 5, Zigfried 6 |
| Memorias del Faraón | Mana 4, Kisara 4, Bobasa 4, Aknamkanon 5, Altos Sacerdotes 6, Mahad 6, Sacerdote Seto 7, Bakura Rey de los Ladrones 7, Zorc 8, **Yami Yugi (Atem) 8**, que es el jefe final (Batalla Ceremonial con los 3 Dioses Egipcios) |

Los que casi no duelan en la serie (Téa, Tristan, Mokuba, Serenity, Solomon, Shadi, Bobasa, Kisara, Mana...) son
fáciles o intermedios y llevan un mazo temático coherente con el personaje.

## Personalidad de la IA
Son las de `ia.h` más tres nuevas: **tramposo** (farol al máximo: Weevil, Bandit Keith), **control** (paciente y
cauto, con mucho farol: Marik, Lumis y Umbra, Ishizu... Dartz, Altos Sacerdotes) y **oscuro** (agresivo y con farol:
Yami Bakura, Yami Marik, Arkana, Bakura Rey, Zorc). Joey y Valon son temerarios, Kaiba, Mai y Gozaburo agresivos,
Pegasus, Noah y Yami Yugi calculadores, Rafael y Kisara defensivos, Zigfried farolero, Duke y Espa Roba faroleros.

## Mazos (personajes_datos.py -> personajes_gen.py)
- `personajes_datos.py`: solo datos. Cada variante tiene su núcleo de cartas de la serie (nombre TCG en inglés,
  `"Nombre*2"` = 2 copias) y un tema de relleno con la misma sintaxis que `rivales_gen.py`.
- `personajes_gen.py` (determinista, semillas CRC32): mete el núcleo. Si hay una fusión, añade sus materiales y
  Polymerization; si hay un ritual, su magia. Los monstruos que no se pueden invocar de forma normal se quitan (se
  avisa: Lava Golem, Dark Necrofear, Spirit of the Pharaoh, Harpie Lady Sisters, Guardian Dreadscythe...). Después
  completa con cartas de la época del tema según el nivel (tributos, nivel 1-4 bajo el techo de ATK del nivel,
  fusiones "de paso", campo/equipos, staples Goat según el nivel y básicas) hasta 40. Los mazos son válidos: 40-60
  cartas, máx. 3 copias y límites Goat, y las fusiones solo entran con sus materiales en el mazo.
- Las cartas firma **no salen nunca en los rivales genéricos** (`rivales_gen.py` las excluye de los temas, las
  fusiones de paso y los rituales).
- Salida: `datos/personajes.txt` (`id|nombre_es|nombre_en|saga|nivel|personalidad|entorno|lema_es|lema_en`) y
  `datos/personajes_rivales.txt` (formato de `rivales.txt` + líneas `V|id|variante|nombre ES|nombre EN`; tema
  `personaje:<saga>`). `ia::Base::leer` lee los dos archivos de rivales; los personajes no salen en `mazoRival(nivel)`.

## Cartas firma
- `python -I personajes_gen.py --firma` saca de los mazos las cartas que no están entre las 1690, comprueba en
  YGOPRODeck que existen impresas y escribe `datos/cartas_firma.txt` (nombre|límite|dueños).
- `preparar_classic.py` las añade con el **mismo proceso** que las 1690 (datos EN de YGOPRODeck con caché en
  `fuente_classic/firma/`, español oficial de `cards.cdb` es-ES por passcode: las 78 lo tienen, caras ES/EN/HD, arte de
  Gemini o provisional) y las marca en `cartas_classic.txt`: `id|passcode|límite|subtipo|firma|dueño,dueño`.
- Límite 3, salvo los **Dioses Egipcios (límite 1)**. Sin motor de efectos se juegan como monstruos con su ATK/DEF
  (piden 3 sacrificios, y el duelo ya los pide: `ia.h`, nivel 7+ pide 2 salvo los Dioses). Obelisk 4000/4000 impreso. **Slifer: 3000/3000** (en el
  anime son 1000 por carta en la mano; 3 es lo normal). **Ra: 4000/4000** (como su versión impresa Immortal Phoenix
  y en la línea de lo que suma con 3 sacrificios). Las caras muestran ese valor.
- **Sustituciones** (solo existen en el anime): Cinderella -> Prinzessin; Duos (ka de Seto) -> Dark Blade; Mago de
  las Ilusiones (Mahad) -> Dark Magician; ka de Karim/Shada -> Gaap the Divine Soldier / Kaiser Sea Horse; Revolver
  Dragon (Keith) -> Barrel Dragon/Blowback Dragon; Air Fortress Ziggurat y Gorlag (Alister) -> Perfect Machine King /
  Machine King / Satellite Cannon; armaduras de Valon (Big Bang Blow, Active Guard...) -> Armor Exe, Armor Breaker,
  Metalsilver/Raregold Armor; Mirror Knight Calling, Orichalcos Deuteros/Tritos/Kyutora (Dartz) -> The Seal of
  Orichalcos + Orichalcos Sword of Sealing y Levia-Dragon; Zorc Necrophades -> Dark Master - Zorc (con Contract
  with the Dark Master); Diabound -> Diabound Kernel; Portrait's Secret (Bakura) fuera. Se quitaron cartas genéricas
  que no son "firma" (Battle Warrior, Cyber Shield, Gernia, Marie the Fallen One...) para no inflar la lista.
- **Cartas firma bloqueadas**: el jugador tiene una carta firma desde que vence a su dueño en el modo historia
  (`personajes::Historia::cartaDesbloqueada(duennos)`, progreso en `historia.txt`). `mazo.h` lee el 5º/6º campo de
  `cartas_classic.txt` y deja en 0 copias las que no están desbloqueadas (`Coleccion::desbloquear`, que se llama al
  arrancar y al vencer a un personaje); en el constructor salen como "Vence a X".

## Entornos
El campo `entorno` usa las claves del `docs/entornos.md` de la rama **classic-entornos** (17 claves), que es la
lista que leen las dos ramas. Aquí no se crea otro archivo para que no choquen al unir las ramas. El juego deja la
clave del rival en `control::VistaDuelo::entorno` (vacía en un genérico), para que `entornos::claveDe` la use.
Por personaje: tienda_juegos (Téa, Solomon, Bakura Ryou, Rebecca, Yugi), isla_bosque (Weevil, Rex),
playa_mar (Mako), acantilado_noche (Bonz, PaniK, Bandit Keith), laberinto (Paradoja), reino_duelistas_castillo
(Pegasus), dirigible_kaiba (Mokuba, Kaiba, Mai, Marik), ciudad_batallas (Joey, Espa Roba, Duke),
rascacielos_noche (Arkana, Lumis y Umbra), museo_egipcio (Shadi, Ishizu), torre_alcatraz (Yami Marik),
reino_sombras (Yami Bakura, Bakura Rey, Zorc), mundo_virtual_noah (Tristan, Serenity, Cinco Grandes, Gozaburo,
Noah), atlantida_oricalco (Valon, Alister, Rafael, Dartz), estadio_kc (Leon, Zigfried) y templo_egipcio (el resto
de Memorias del Faraón y Yami Yugi).

## Retratos
- `cola_personajes.py --preparar` deja en `nano-banana\classic\personajes\<id>\` la foto (x4 con Real-ESRGAN anime si
  es pequeña), `prompt.txt` (retrato anime fiel, medio cuerpo, 4K, fondo de croma verde o magenta para los que
  llevan verde) y `prompt_neutro.txt` (sin nombres, para los rechazos por derechos). También deja las 78
  ilustraciones de las cartas firma en `nano-banana\classic\arte\<id>\` (mismo prompt que `cola_classic.py`).
- `cola_personajes.py` usa el motor de `cola_gemini.py`, pero con su propio estado y registro. No hay que correrla a la
  vez que otra cola en el mismo Chrome. Al terminar cada retrato corre `retratos_post.py --solo <id>`: recorte con
  isnet-anime + llave de croma + despill, y deja `classic-assets\personajes\retrato_<id>.png` (RGBA, 1400 px de
  alto). Mientras no hay resultado de Gemini, el retrato provisional sale de la foto original.
- En el juego (`personajes_ui.h`): cache de 4 retratos con mipmaps cargados a pedido. Durante el duelo el retrato va
  **arriba a la derecha** (30 % del alto de la pantalla), con una placa con el nombre y el lema. Aparece con
  fundido, se mece despacio, se pone rojizo al recibir daño y late un poco mientras el rival piensa. En la pantalla
  del modo historia se ve grande, a la izquierda.
