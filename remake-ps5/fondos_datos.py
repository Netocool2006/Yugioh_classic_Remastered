# Yugioh Classic Remaster (rama classic-fondos): UN fondo fotorrealista por personaje (47, sin repetir) y 16 por tema
# para los rivales genericos. Cada entrada: id, tema (clave de src/entornos.h: luz base y particulas), lugar ES/EN (lo
# que se ve; docs/fondos.md) y la escena en ingles para Gemini (sin nombres de la franquicia: Gemini rechaza por
# derechos de terceros; se describe el lugar). La luz de cada escena va acorde al tema.
# fondos_gen.py escribe datos/fondos.txt (lo lee el juego) y cola_fondos.py prepara los prompts y corre la cola.

def F(id, tema, es, en, escena, luz, respaldo=""):
    return dict(id=id, tema=tema, es=es, en=en, escena=escena, luz=luz, respaldo=respaldo)


PERSONAJES = [
    # --- Reino de los Duelistas
    F("pegasus", "reino_duelistas_castillo", "Castillo de Pegasus en la isla del Reino de los Duelistas", "Pegasus's castle on Duelist Kingdom island",
      "a colossal European castle of pale stone with many tall slender towers and pointed blue slate roofs, built on the summit "
      "of a lush tropical island; a wide paved stone courtyard and grand stone stairs lead up to its gate; dense jungle and "
      "sea cliffs around it and the open ocean far behind", "golden sunset light from the side, warm glow in the windows"),
    F("kaiba", "dirigible_kaiba", "Cubierta del dirigible de Kaiba Corp sobre el mar de nubes", "Kaiba Corp blimp deck above a sea of clouds",
      "the open-air steel deck of a gigantic futuristic airship flying high above an endless sea of clouds; polished metal "
      "railings, white and blue hull panels and a huge circular duel platform on the deck; other cloud layers to the horizon",
      "late afternoon sun, crisp cold air, blue sky with high cirrus"),
    F("yugi", "tienda_juegos", "Calle de la tienda de juegos Kame en Ciudad Domino", "Kame Game Shop street in Domino City",
      "a quiet Japanese city street with a small two-story family game shop on the corner (wooden facade, glass display window "
      "full of board games and card boxes, a carved wooden turtle emblem above the door; absolutely no letters, words or signs with writing anywhere), utility poles and wires, neighbouring "
      "houses, a modern city skyline far behind", "warm early evening light, sky turning orange, shop lights on"),
    F("joey", "ciudad_batallas", "Muelle del puerto de Ciudad Domino", "Domino City harbour pier",
      "a big industrial harbour pier at the edge of a modern Japanese city: concrete quay, steel bollards, stacked shipping "
      "containers, tall cargo cranes, calm sea and the city skyline across the bay", "dramatic sunset, orange sky reflected on the water"),
    F("mokuba", "dirigible_kaiba", "Jardin de la mansion Kaiba", "Kaiba mansion garden",
      "the front garden of an enormous modern-classical white mansion: trimmed hedges, a marble fountain, a long straight "
      "driveway and lawns, a helipad, tall gates", "bright clear morning, soft shadows"),
    F("tea", "tienda_juegos", "Patio de la preparatoria de Ciudad Domino", "Domino High School courtyard",
      "the courtyard of a Japanese high school: a four-story concrete school building with a clock on its facade, rows of "
      "windows, cherry trees in full bloom, a sports field with a fence", "soft golden afternoon light, cherry petals in the air"),
    F("solomon", "tienda_juegos", "Interior de la tienda de juegos de Solomon", "Inside Solomon's game shop",
      "the cosy interior of an old family-run game and hobby shop: wooden shelves packed with board games, card boxes, dice and "
      "antique toys, a glass counter with old cards, a small shelf of Egyptian souvenirs, warm lamps; no signs, no posters, no lettering anywhere", "warm interior lamp light, dust in the light beams"),
    F("weevil", "isla_bosque", "Bosque brumoso de la isla", "Misty island forest",
      "a dense primeval tropical forest on an island: giant twisted trees, ferns, hanging vines, huge dew-covered spider webs "
      "between the trunks, a clearing of trampled grass in front", "morning mist pierced by shafts of sunlight"),
    F("rex_raptor", "isla_bosque", "Meseta prehistorica volcanica", "Prehistoric volcanic plateau",
      "a rugged prehistoric landscape: black volcanic rock plateau, giant ferns and cycads, a steaming river, a smoking volcano "
      "in the distance", "hazy warm afternoon, smoke and steam in the air"),
    F("bonz", "acantilado_noche", "Cementerio en la isla, de noche", "Island graveyard at night",
      "an old abandoned graveyard on a hillside of an island: crooked tombstones, broken stone crosses, a rusty iron fence, a "
      "dead tree, a cave mouth in the rocks behind", "cold moonlight, low ground fog, very dark blue night"),
    F("ryou_bakura", "acantilado_noche", "Aldea de fantasia medieval al anochecer", "Medieval fantasy village at dusk",
      "a misty medieval fantasy village: half-timbered houses, a stone bridge over a stream, a ruined tower on a hill, lanterns, "
      "dark forest around, like a tabletop role-playing world come to life", "blue dusk with warm lantern light"),
    F("mako", "playa_mar", "Muelle de madera en el oceano", "Wooden pier on the ocean",
      "a long weathered wooden fishing pier stretching into a vast open ocean, rocky shore, crashing waves, a small lighthouse "
      "on a headland, fishing nets and buoys", "stormy sky breaking with golden sunlight on the sea"),
    F("panik", "acantilado_noche", "Claro del bosque con braseros, de noche", "Forest clearing with braziers at night",
      "a dark forest clearing at night surrounded by black pines, a ring of tall iron fire braziers burning, black smoke, a "
      "rocky outcrop behind", "only firelight, black night, heavy smoke"),
    F("hermanos_paradoja", "laberinto", "Laberinto subterraneo de piedra", "Underground stone labyrinth",
      "a vast underground labyrinth of ancient stone corridors and high walls with carved doorways, torches on the walls, "
      "pillars fading into darkness", "warm torchlight, deep shadows, dust in the air"),
    F("shadi", "templo_egipcio", "Ruinas de un templo egipcio bajo la luna", "Egyptian temple ruins under the moon",
      "ruins of an ancient Egyptian temple in the open desert: fallen columns, two tall obelisks, sand dunes", "moonlit night, starry sky, cold blue light"),
    F("mai", "playa_mar", "Pradera en lo alto de un acantilado junto al mar", "Clifftop meadow by the sea",
      "a wild clifftop meadow covered with wildflowers on a volcanic island, sheer cliffs dropping to a turquoise sea, distant "
      "rocky islets", "golden hour, warm wind, glowing grass"),
    F("bandit_keith", "acantilado_noche", "Almacen industrial abandonado, de noche", "Abandoned industrial warehouse at night",
      "an abandoned American industrial yard at night: rusty warehouses, chain-link fences, oil drums, puddles, a water tower, "
      "flickering sodium street lamps", "orange sodium light and cold night, wet ground"),
    # --- Ciudad Batallas
    F("espa_roba", "ciudad_batallas", "Plaza del centro de Ciudad Batallas", "Battle City downtown plaza",
      "a large modern plaza in the business district of a Japanese city, glass skyscrapers, a giant video screen on a building "
      "(blank), trees and benches", "bright midday sun, clear blue sky"),
    F("duke", "rascacielos_noche", "Calle de juegos y neones de noche", "Neon game street at night",
      "a glamorous street of game halls and arcades at night, neon tubes shaped only like dice, playing card suits and stars (absolutely no letters, words or numbers anywhere), "
      "wet asphalt reflecting the lights", "night, colourful neon, light rain reflections"),
    F("arkana", "laberinto", "Mazmorra de un mago", "A magician's dungeon",
      "an underground stone dungeon theatre: gothic arches, iron chains, candelabras, red velvet curtains, a circular stone "
      "stage", "candle light, purple haze"),
    F("lumis_umbra", "rascacielos_noche", "Azotea de un rascacielos de noche", "Skyscraper rooftop at night",
      "the rooftop of a very tall skyscraper at night: helipad markings, steel railings, antennas, the glittering city far below "
      "and other towers around", "night city glow, cold wind, clouds lit from below"),
    F("ishizu", "museo_egipcio", "Exposicion egipcia del Museo de Domino", "Domino Museum Egyptian exhibit",
      "a grand museum hall dedicated to ancient Egypt: polished stone floor, glass cases, sarcophagi, statues of pharaohs and a "
      "huge carved stone tablet on the far wall", "soft spotlights, quiet museum ambience"),
    F("marik", "museo_egipcio", "Santuario subterraneo de los guardianes de la tumba", "Underground sanctuary of the tomb keepers",
      "an underground Egyptian sanctuary carved into rock: massive pillars covered in hieroglyphs, stairs to a hole of daylight "
      "far above, oil lamps", "a shaft of sunlight from above and warm oil lamps"),
    F("yami_marik", "torre_alcatraz", "Torre de duelo en la isla de Alcatraz", "Duel tower on Alcatraz island",
      "a colossal futuristic steel tower rising from a rocky prison island in the middle of the sea, with a huge open-air duel "
      "arena on its top", "blood-red sunset, dark clouds, lightning far away"),
    F("yami_bakura", "reino_sombras", "Paramo con una abadia en ruinas bajo luna roja", "Moor with a ruined abbey under a red moon",
      "a desolate English moor with the ruins of a gothic abbey, broken arches and gravestones, dead trees", "huge blood-red moon, purple night fog"),
    # --- Noah
    F("noah", "mundo_virtual_noah", "Ciudad de cristal del mundo virtual", "Glass city of the virtual world",
      "a futuristic pristine city of white stone and glass towers floating above the clouds, perfectly clean, faint glowing "
      "lines in the sky like a digital grid", "soft cool daylight, slightly unreal clarity"),
    F("gozaburo", "mundo_virtual_noah", "Fortaleza corporativa en la tormenta", "Corporate fortress in the storm",
      "a gigantic dark fortress-like corporate skyscraper of black steel, searchlights, a stormy city below", "night storm, lightning, rain"),
    F("cinco_grandes", "mundo_virtual_noah", "Glaciar artico", "Arctic glacier",
      "an immense arctic glacier landscape: blue ice cliffs, snow fields, icebergs floating in dark water", "low polar sun, icy blue shadows"),
    F("tristan", "mundo_virtual_noah", "Selva tropical con cascada", "Tropical jungle with waterfall",
      "a lush tropical jungle valley with a tall waterfall, giant leaves, mossy rocks, a river", "humid sunlight beams, mist from the waterfall"),
    F("serenity", "mundo_virtual_noah", "Pradera tranquila con cerezos", "Peaceful meadow with cherry trees",
      "a peaceful rolling green meadow with blooming cherry trees, a small stream and gentle hills", "soft spring morning, light haze"),
    # --- Dragones / Doma
    F("dartz", "atlantida_oricalco", "Atlantida y el Oricalco", "Atlantis and the Orichalcos",
      "the ruins of a sunken ancient civilisation risen from the sea: colossal carved stone temples and statues, a giant "
      "green glowing crystal monolith in the centre", "eerie emerald glow, dark storm clouds, sea spray"),
    F("rafael", "atlantida_oricalco", "Isla del naufragio", "Shipwreck island",
      "a lonely tropical island shore with the rusted wreck of a large ship stranded on the rocks, palm trees, rough sea", "overcast, moody light breaking through"),
    F("valon", "atlantida_oricalco", "Carretera del desierto con mesetas", "Desert highway with mesas",
      "a straight asphalt desert highway running between red rock mesas, an old abandoned gas station with blank weathered boards (no signs, no lettering), dust", "harsh late afternoon sun, heat haze"),
    F("alister", "atlantida_oricalco", "Ciudad en ruinas por la guerra", "War-torn ruined city",
      "a war-torn city in ruins: collapsed concrete buildings, burnt cars, smoke columns, a broken tank", "dusk, smoky orange sky"),
    F("rebecca", "tienda_juegos", "Calle soleada de San Francisco", "Sunny San Francisco street",
      "a steep sunny street of a hilly American coastal city with Victorian houses, tram rails, and a red suspension bridge over "
      "the bay in the distance", "bright clear afternoon, sea breeze"),
    # --- Gran Campeonato KC
    F("leon", "estadio_kc", "Estadio del Gran Campeonato en Kaiba Land", "Grand Championship stadium in Kaiba Land",
      "a huge futuristic stadium inside a theme park: tiered empty stands, giant blank screens, a central duel arena, a "
      "dragon-shaped theme park tower outside", "evening, stadium floodlights, fireworks smoke"),
    F("zigfried", "estadio_kc", "Castillo gotico aleman con rosaleda", "German gothic castle with rose garden",
      "a gothic German castle with pointed towers and a terrace of formal rose gardens with red and pink roses, stone "
      "balustrades", "dusk, pink and purple sky, lanterns"),
    # --- Memorias del Faraon
    F("yami_yugi", "templo_egipcio", "Sala de la Ceremonia de Batalla", "Chamber of the Ceremonial Battle",
      "a vast underground Egyptian hall beneath the desert: a giant carved stone tablet with an eye symbol, huge statues, "
      "a sandy arena floor, light from cracks in the ceiling", "golden beams of light in dust"),
    F("aknamkanon", "templo_egipcio", "Sala del trono del faraon", "Pharaoh's throne room",
      "the throne room of an ancient Egyptian palace: painted columns, a golden throne on a dais, tall doors open to the Nile", "warm daylight from high windows"),
    F("sacerdote_seto", "templo_egipcio", "Patio del gran templo con pilonos", "Great temple courtyard with pylons",
      "the courtyard of a colossal ancient Egyptian temple with towering pylons, colossal statues and a row of sphinxes", "sunrise, long shadows, warm haze"),
    F("mahad", "templo_egipcio", "Tumba real subterranea", "Royal underground tomb",
      "a royal Egyptian burial hall deep underground: golden sarcophagus, painted walls, treasure, traps in the floor", "oil lamp glow, deep darkness"),
    F("mana", "templo_egipcio", "Jardines del palacio junto al Nilo", "Palace gardens by the Nile",
      "lush gardens of an ancient Egyptian palace on the bank of the Nile: palm trees, lotus pools, papyrus, sailing boats on the river", "bright warm morning"),
    F("kisara", "templo_egipcio", "Desierto estrellado con un pozo", "Starry desert with a well",
      "a remote moonlit desert with soft dunes, a small ancient stone well and a few palm trees", "starry night, white moonlight"),
    F("bobasa", "templo_egipcio", "Salon de banquetes del palacio", "Palace banquet hall",
      "a grand ancient Egyptian banquet hall: painted lotus columns, long low tables laden with fruit, bread and jars, braziers", "warm torchlight and evening sun"),
    F("altos_sacerdotes", "templo_egipcio", "Santuario de los Articulos del Milenio", "Sanctuary of the Millennium Items",
      "a sacred Egyptian temple sanctuary with seven stone pedestals in a circle, a reflecting pool, massive carved walls", "beams of light from the roof"),
    F("bakura_rey", "reino_sombras", "Kul Elna, la aldea en ruinas", "Kul Elna, the ruined village",
      "a ruined mud-brick village in a desert canyon, broken houses, a dark underground temple entrance", "dusk, red sky, dust storm"),
    F("zorc", "reino_sombras", "Ciudad egipcia en llamas bajo un cielo oscuro", "Egyptian city burning under a dark sky",
      "an ancient Egyptian city on fire under an apocalyptic black sky with a swirling vortex, cracked earth", "fire glow, black and violet sky"),
]

# Rivales genericos (duelo libre): un fondo por tema, distinto de los de los personajes; mientras falte, el respaldo
TEMAS = [
    F("tema_tienda_juegos", "tienda_juegos", "Calle comercial japonesa", "Japanese shopping street",
      "a covered Japanese shopping arcade street with small shops, lanterns and signs without letters", "afternoon light", "yugi"),
    F("tema_reino_duelistas_castillo", "reino_duelistas_castillo", "Murallas de una fortaleza medieval", "Medieval fortress walls",
      "the battlements and courtyard of a massive medieval stone fortress on a hill", "sunset", "pegasus"),
    F("tema_isla_bosque", "isla_bosque", "Pradera y bosque de una isla", "Island meadow and forest",
      "a green meadow at the edge of a dense island forest with rocky hills", "sunny morning", "weevil"),
    F("tema_playa_mar", "playa_mar", "Playa tropical", "Tropical beach",
      "a wide tropical beach with turquoise water and rocks", "bright midday", "mako"),
    F("tema_acantilado_noche", "acantilado_noche", "Acantilados de noche", "Sea cliffs at night",
      "dark rocky sea cliffs with crashing waves and caves", "moonlight, fog", "bonz"),
    F("tema_laberinto", "laberinto", "Catacumbas", "Catacombs",
      "ancient catacombs with stone arches and niches", "torchlight", "hermanos_paradoja"),
    F("tema_rascacielos_noche", "rascacielos_noche", "Ciudad de noche", "City at night",
      "a dense city of skyscrapers seen from a high terrace at night", "night city lights", "lumis_umbra"),
    F("tema_ciudad_batallas", "ciudad_batallas", "Cruce de calles de una gran ciudad", "Big city crossing",
      "a big city street crossing with tall buildings and elevated train tracks", "daylight", "espa_roba"),
    F("tema_dirigible_kaiba", "dirigible_kaiba", "Cumbres sobre las nubes", "Peaks above the clouds",
      "snowy mountain peaks rising above a sea of clouds", "clear high-altitude sun", "kaiba"),
    F("tema_torre_alcatraz", "torre_alcatraz", "Isla prision en el mar", "Prison island at sea",
      "an old fortified prison island in a stormy bay", "sunset storm", "yami_marik"),
    F("tema_museo_egipcio", "museo_egipcio", "Galeria de antiguedades", "Antiquities gallery",
      "a long museum gallery of ancient statues and columns", "soft gallery lighting", "ishizu"),
    F("tema_mundo_virtual_noah", "mundo_virtual_noah", "Paisaje digital", "Digital landscape",
      "an unreal perfectly smooth landscape of glass hills under a glowing grid sky", "cool artificial light", "noah"),
    F("tema_atlantida_oricalco", "atlantida_oricalco", "Ruinas sumergidas", "Sunken ruins",
      "half-sunken ancient stone ruins in a shallow green sea", "eerie green light", "dartz"),
    F("tema_estadio_kc", "estadio_kc", "Estadio vacio", "Empty stadium",
      "a huge empty modern stadium with floodlights", "night floodlights", "leon"),
    F("tema_templo_egipcio", "templo_egipcio", "Templo egipcio a pleno sol", "Egyptian temple in full sun",
      "a grand ancient Egyptian temple hall with columns open to the sun", "harsh desert sun", "sacerdote_seto"),
    F("tema_reino_sombras", "reino_sombras", "Paramo de sombras", "Shadow wasteland",
      "a dark wasteland of black rocks and ruins under a violet stormy sky", "violet gloom", "zorc"),
]

# Rama classic-campos: un fondo por CARTA DE CAMPO (25). Al activarse la carta, el fondo del rival se cambia por el
# suyo (src/campos.h); al destruirse vuelve el del personaje. id = "campo_<id de la carta>"; el tema da la luz base y
# las particulas; el respaldo es un fondo ya hecho que se usa mientras falte el propio (pruebas).
CAMPOS = [
    F("campo_forest", "isla_bosque", "Bosque: bosque profundo", "Forest: deep forest",
      "the heart of an ancient deep forest: colossal mossy oak and beech trunks, a carpet of ferns and moss, fallen logs, "
      "layer upon layer of trees fading into green mist", "golden god rays through the canopy, dappled light", "weevil"),
    F("campo_mountain", "dirigible_kaiba", "Montana: cumbres rocosas", "Mountain: rocky peaks",
      "a majestic range of jagged granite mountains with snowy summits and steep rocky ridges, a high alpine plateau of "
      "bare rock and patches of snow in front, deep valleys between the peaks", "crisp high-altitude sunlight, thin clouds around the summits", "tema_dirigible_kaiba"),
    F("campo_sogen", "isla_bosque", "Sogen: praderas infinitas", "Sogen: endless grasslands",
      "an endless open grassland steppe of tall swaying green and golden grass under a huge sky, gentle rolling hills, a few "
      "lone trees and wildflowers, distant blue mountains on the horizon", "warm late afternoon sun, big white cumulus clouds, wind in the grass", "serenity"),
    F("campo_umi", "playa_mar", "Umi: oceano profundo", "Umi: the deep ocean",
      "the open deep ocean far from any land: powerful dark blue swells and waves rolling to the horizon, white foam crests, "
      "spray in the air, a dramatic sky over the sea", "silver sunlight breaking through clouds onto the water", "mako"),
    F("campo_wasteland", "templo_egipcio", "Paramo: tierra yerma", "Wasteland: barren land",
      "a vast barren wasteland: cracked dry earth, red rock canyons and eroded mesas, dead bleached trees, scattered "
      "rocks, dust devils in the distance", "harsh hot sun, hazy dusty air, heat shimmer", "valon"),
    F("campo_yami", "reino_sombras", "Yami: la oscuridad", "Yami: the darkness",
      "a realm of pure darkness: black jagged rocks and broken spires emerging from a sea of thick violet fog, a black sky "
      "with a faint swirling violet vortex, dead twisted trees", "almost no light, eerie violet glow from the fog, deep shadows", "tema_reino_sombras"),
    F("campo_a_legendary_ocean", "atlantida_oricalco", "Oceano legendario: ruinas sumergidas", "A Legendary Ocean: sunken ruins",
      "a legendary ocean where a sunken ancient city of white marble columns, arches and a great temple rises half out of "
      "crystal-clear turquoise water, coral and ruins visible beneath the surface, waterfalls pouring from the ruins", "radiant sunlight, caustic light on the stone, sparkling water", "tema_atlantida_oricalco"),
    F("campo_umiiruka", "playa_mar", "Umiiruka: mar crecido", "Umiiruka: the rising sea",
      "a huge rising sea flooding a coastline: towering waves, a tidal surge, rocky islets swallowed by the water, whirlpools "
      "and spray, a stormy horizon", "stormy light with a bright gap in the clouds, wet reflective surfaces", "tema_playa_mar"),
    F("campo_rising_air_current", "dirigible_kaiba", "Corriente de aire ascendente: sobre las nubes", "Rising Air Current: above the clouds",
      "high in the sky above a sea of clouds: towering cumulus columns rising with strong updrafts, floating rocky crags with "
      "wind-bent trees, streaks of wind and mist", "bright clear sun, deep blue sky, luminous clouds", "kaiba"),
    F("campo_gaia_power", "isla_bosque", "Poder de Gaia: la tierra viva", "Gaia Power: the living earth",
      "a primordial living landscape: gigantic ancient trees with roots wrapping huge boulders, lush green valleys, rivers "
      "and waterfalls, mountains covered in forest, an overwhelming feeling of the power of nature", "rich warm sunlight, fertile green haze", "tristan"),
    F("campo_luminous_spark", "templo_egipcio", "Chispa luminosa: el reino de la luz", "Luminous Spark: the realm of light",
      "a radiant landscape of white crystal formations and quartz spires rising from a pale stone plain, glowing light "
      "pouring from the crystals, a dazzling bright sky", "brilliant white and pale gold light, sparkles and lens glow", "tema_templo_egipcio"),
    F("campo_molten_destruction", "torre_alcatraz", "Destruccion fundida: volcan en erupcion", "Molten Destruction: erupting volcano",
      "an erupting volcanic landscape: rivers and lakes of glowing molten lava flowing through black basalt rock, a huge "
      "volcano spewing fire and ash, fissures glowing orange", "fiery orange glow from the lava under a dark smoky sky, embers in the air", "zorc"),
    F("campo_mystic_plasma_zone", "reino_sombras", "Zona de plasma mistico", "Mystic Plasma Zone",
      "an otherworldly plasma storm dimension: violet and magenta plasma lightning arcing between floating dark rocks, a "
      "swirling purple nebula sky, glowing energy rivers on black ground", "electric violet and magenta light, crackling energy", "gozaburo"),
    F("campo_chorus_of_sanctuary", "templo_egipcio", "Coro del santuario", "Chorus of Sanctuary",
      "a serene holy sanctuary: a grand white marble temple with tall columns and golden domes on a hill, broad marble stairs, "
      "white doves, gardens and fountains", "soft heavenly golden light from above, gentle haze", "sacerdote_seto"),
    F("campo_the_sanctuary_in_the_sky", "dirigible_kaiba", "El santuario en el cielo", "The Sanctuary in the Sky",
      "a celestial sanctuary floating above the clouds: a magnificent white marble and gold temple with colonnades and "
      "arches standing on a sea of clouds, pillars of light descending from the heavens", "radiant warm golden light, luminous clouds", "tema_dirigible_kaiba"),
    F("campo_dark_sanctuary", "reino_sombras", "Santuario oscuro", "Dark Sanctuary",
      "a dark forbidden sanctuary: a ruined black stone temple with cracked pillars and a stone altar, ghostly wisps of smoke, "
      "a black sky with a pale ominous moon", "cold ghostly light, deep blackness, faint purple glow", "yami_bakura"),
    F("campo_necrovalley", "templo_egipcio", "Necrovalle: el valle de las tumbas", "Necrovalley: the valley of tombs",
      "a deep desert valley of ancient tombs: towering sandstone cliffs carved with colossal statues and tomb entrances, "
      "pillars covered in carved pictorial reliefs (only pictures, no readable writing), sand drifting over stone", "low golden desert sun, long shadows, dusty air", "shadi"),
    F("campo_pandemonium", "reino_sombras", "Pandemonium: el castillo de los demonios", "Pandemonium: the castle of fiends",
      "a hellish landscape with an enormous dark gothic castle of black spires on a crag, rivers of fire below, a bridge of "
      "black stone, a blood-red sky with black clouds", "infernal red and orange glow, smoke and ash", "bakura_rey"),
    F("campo_jurassic_world", "isla_bosque", "Mundo jurasico", "Jurassic World",
      "a prehistoric jurassic jungle valley: giant ferns, cycads and conifers, a steaming swamp and river, a smoking volcano "
      "in the distance, misty cliffs", "humid hazy warm light, steam and mist", "rex_raptor"),
    F("campo_harpies_hunting_ground", "dirigible_kaiba", "Coto de caza de las arpias", "Harpies Hunting Ground",
      "a wild windswept mountain crag high above the world: jagged rock pinnacles with huge nests of branches and feathers, "
      "a canyon far below, sheer cliffs, storm clouds", "dramatic stormy light with sun rays, strong wind", "mai"),
    F("campo_golden_castle_of_stromberg", "reino_duelistas_castillo", "Castillo dorado de Stromberg", "Golden Castle of Stromberg",
      "a fairy-tale German castle entirely of gold: shining golden towers, spires and walls on a rocky hill above a dark "
      "forest, a golden bridge and gate", "warm sunset light making the gold glow, rich sky", "zigfried"),
    F("campo_the_seal_of_orichalcos", "atlantida_oricalco", "El sello de Oricalco", "The Seal of Orichalcos",
      "an ancient sunken temple of Atlantis at night: a vast circular stone plaza engraved with a giant glowing green circle "
      "and six-pointed star, green light rising from it into the sky, dark ruined pillars and a stormy sea around", "eerie emerald green glow, dark night sky", "dartz"),
    F("campo_fusion_gate", "reino_sombras", "Puerta de fusion: el vortice dimensional", "Fusion Gate: the dimensional vortex",
      "a colossal swirling dimensional vortex in the sky: a spiral of blue, violet and gold energy pulling clouds and light "
      "into its center above a strange flat stone plain with floating rocks", "glowing vortex light, cosmic colours", "tema_mundo_virtual_noah"),
    F("campo_centrifugal_field", "rascacielos_noche", "Campo centrifugo", "Centrifugal Field",
      "a gigantic futuristic circular arena: concentric metal rings and spinning tracks around a central glowing core, "
      "machinery, metal towers, a vast dome of steel girders open to a dusk sky", "cool industrial light with glowing cyan rings", "gozaburo"),
    F("campo_array_of_revealing_light", "templo_egipcio", "Formacion de luz reveladora", "Array of Revealing Light",
      "an ancient stone temple hall open to the sky with a huge glowing circle of light drawn on the floor, columns of white "
      "light rising from it, carved pillars around, dust in the light", "brilliant shafts of white-gold light in a dim hall", "altos_sacerdotes"),
]

PROMPT = (
    "Create an ultra-photorealistic cinematic photograph (a matte painting for a big-budget fantasy film) of this place: "
    "{escena}.\n\n"
    "- Lighting and mood: {luz}; volumetric atmosphere, filmic colour grading, real materials and textures with very fine detail.\n"
    "- It must look like a REAL photograph shot on a full-frame cinema camera with a 24 mm lens. Strictly no illustration, "
    "no painting brushstrokes, no anime, no cartoon, no video-game or 3D-render look, no low-poly.\n"
    "- Ultra-wide panoramic landscape, aspect ratio 21:9, the highest resolution possible.\n"
    "- Composition: wide establishing shot from a slightly elevated viewpoint, level horizon. The main subject is centred and "
    "fills the middle band of the frame (from 15% to 75% of the height, nothing important above or below); the bottom quarter "
    "is plain ground, floor or water with no important details (it will be hidden). Symmetric, calm composition, everything "
    "in focus.\n"
    "- Empty scene: no people, no animals, no creatures, no vehicles in the foreground, no text, no letters, no logos, no "
    "watermark, no frame, no border.\n")


def prompt(f):
    return PROMPT.format(escena=f["escena"], luz=f["luz"])


TODOS = PERSONAJES + TEMAS + CAMPOS
assert len(PERSONAJES) == 47 and len(CAMPOS) == 25 and len({f["id"] for f in TODOS}) == len(TODOS)


if __name__ == "__main__":
    # datos/fondos.txt (lo lee src/entornos.h desde ui/fondos.txt): id|tema|respaldo|lugar_es|lugar_en
    import os
    ruta = os.path.join(os.path.dirname(os.path.abspath(__file__)), "datos", "fondos.txt")
    with open(ruta, "w", encoding="utf-8", newline="\n") as f:
        f.write("# fondos.txt - generado por remake-ps5/fondos_datos.py. id|tema|respaldo|lugar_es|lugar_en\n")
        f.write("# un fondo fotorrealista por personaje (su id = su campo entorno en personajes.txt) y uno por tema (rivales genericos) y uno por carta de campo (campo_<carta>, rama classic-campos)\n")
        for x in TODOS:
            f.write("|".join((x["id"], x["tema"], x["respaldo"], x["es"], x["en"])) + "\n")
    print("escrito %s (%d fondos)" % (ruta, len(TODOS)))
