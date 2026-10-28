// ================================================================ figuras 3D de las cartas emblematicas (rama classic-figuras)
// (se incluye DENTRO de la clase VulkanExample, como duelo_juego.h)
//
// Pedido del usuario: los monstruos de las CARTAS EMBLEMATICAS aparecen como figura 3D de pie sobre su carta (como en
// Yugi Reload) y el resto sigue plano. Es una CAPA OPCIONAL que solo presenta: ia::Duelo y control.h mandan (que hay en
// cada zona, quien ataca, que se destruye); las figuras leen la mesa (ctl.zonas) y el ataque en curso (tAtaque).
//  - datos/figuras.txt (emblematicas_gen.py): id de carta -> slug del modelo. Se carga models/yugi/<slug>_anim.glb si
//    existe (con esqueleto: clips reposo, ataque, golpe, defensa, muerte por nombre; los de Meshy Idle/Parry/dying) y
//    si no models/yugi/<slug>.glb (rigida). Sin el archivo, la carta sigue plana.
//  - Una figura aparece en una zona de monstruo boca arriba (se materializa: desintegracion al reves, borde cian),
//    mira a la camara en reposo (respira, los alados flotan y aletean), en defensa se agacha / se cubre / baja al
//    suelo con las alas recogidas (postura_defensa de Yugi Reload), y si su carta deja la zona sin morir en un ataque
//    (magia, tributo, volver a la mano) se desvanece en su lugar.
//  - El ataque con figura dura mas (el reloj del ataque plano corre a VEL_FIGURAS): el atacante gira hacia el objetivo
//    y, segun su tabla, lanza magia desde la punta del baculo (magos), aliento desde la boca (dragones), rayos (Craneo
//    Convocado) o embiste / va con la espada (el resto); el golpe llega en T_IMPACTO (el danno y las chispas de
//    siempre), el objetivo retrocede, y el destruido cae (clip de muerte con esqueleto; de espaldas, al suelo o
//    aplastado sin esqueleto) y se desintegra antes de que su carta vaya al cementerio.
// Rendimiento (4K a 120 Hz): pocas figuras a la vez (10 zonas), sin pasada extra: van en la pasada de sombra y en la de
// escena con el mismo escena.frag; los huesos se calculan una vez por modelo y cuadro. Ajustes: "Figuras 3D" las
// apaga. PC: YUGI_FIGURAS=0 las apaga; YUGI_FIGURAS_MESA="jugador_monstruo_1=dark_magician,rival_monstruo_2=blue_eyes:def"
// arma esa mesa en un duelo congelado (sin turno de la IA) y YUGI_FIGURAS_ATAQUE="2.5:jugador_monstruo_1>rival_monstruo_2;..."
// lanza esos ataques (destruyen de verdad); YUGI_FIGURAS_MEDIR=1 imprime triangulos y tiempo de CPU.
	enum class AtaqueFig { Embestida, Espada, Magia, Aliento, Rayo };
	enum class CuerpoFig { Bipedo, Cuadrupedo, Alado, Compacto };
	// slug, escala del modelo (la de escena_tablero.py en Yugi Reload; 0 = 2.6 m de alto), cuerpo, ataque, de donde
	// sale el ataque (coordenadas del modelo glTF, frente +Z: boca, punta del baculo) y sus colores; alas (aleteo sin
	// esqueleto, escena.vert): bisagra (fraccion del ancho), altura de la bisagra, abajo, pendiente, corte (modelo)
	struct DatosFigura {
		const char* slug;
		float escala;
		CuerpoFig cuerpo;
		AtaqueFig ataque;
		glm::vec3 salida;
		glm::vec3 halo, nucleo;
		bool alas;
		float bisagra, altura, abajo, pendiente, corte;
	};
	static constexpr float ESCALA_PERSONAJE = 2.6f / 1.8f;   // los de esqueleto (Meshy) miden 1.8 m
	static constexpr float SUELO_FIGURA = 0.06f;            // la carta mas alta bajo un monstruo (5 cm) + 1 cm
	static constexpr float VEL_FIGURAS = 0.45f;             // el ataque plano (1.6 s) dura ~3.6 s con figuras
	static constexpr int MAX_FIGURAS = 96;
	static const DatosFigura* datosFigura(const std::string& slug)
	{
		static const DatosFigura TABLA[] = {
			// Yugi Reload (escena.h, rama atributos-auto, bocas_dragones.h, LANZADORES, ALAS_MEDIDAS)
			{ "blue_eyes", 4.793314f, CuerpoFig::Alado, AtaqueFig::Aliento, { 0.0f, 0.49f, 0.46f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },
			{ "red_eyes", 2.737822f, CuerpoFig::Alado, AtaqueFig::Aliento, { 0.0f, 0.023f, 0.70f }, { 1.7f, 0.28f, 0.08f }, { 1.0f, 0.75f, 0.35f }, true, 0.18f, 0.375f, 0.03f, 0.0f, 100.0f },
			{ "baby_dragon", 1.049288f, CuerpoFig::Alado, AtaqueFig::Aliento, { 0.0f, 0.17f, 0.74f }, { 1.6f, 0.62f, 0.15f }, { 1.0f, 0.9f, 0.55f }, true, 0.18f, 0.25f, 0.0f, 0.371f, 0.015f },
			{ "dark_magician", ESCALA_PERSONAJE, CuerpoFig::Bipedo, AtaqueFig::Magia, { 0.3873f, 1.7372f, 0.1786f }, { 0.18f, 0.45f, 1.6f }, { 0.85f, 0.92f, 1.0f }, false, 0, 0, 0, 0, 0 },
			{ "dark_magician_girl", ESCALA_PERSONAJE, CuerpoFig::Bipedo, AtaqueFig::Magia, { 0.3059f, 1.3721f, 0.0856f }, { 0.55f, 0.18f, 1.5f }, { 0.92f, 0.82f, 1.0f }, false, 0, 0, 0, 0, 0 },
			{ "celtic_guardian", ESCALA_PERSONAJE, CuerpoFig::Bipedo, AtaqueFig::Espada, { 0.0f, 1.2f, 0.3f }, { 0.6f, 1.2f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },
			{ "kuriboh", 0.907142f, CuerpoFig::Compacto, AtaqueFig::Embestida, { 0.0f, 0.5f, 0.4f }, { 1.2f, 0.7f, 0.3f }, { 1.0f, 0.9f, 0.7f }, false, 0, 0, 0, 0, 0 },
			{ "silver_fang", 1.958502f, CuerpoFig::Cuadrupedo, AtaqueFig::Embestida, { 0.0f, 0.7f, 0.25f }, { 0.8f, 0.9f, 1.2f }, { 1.0f, 1.0f, 1.0f }, false, 0, 0, 0, 0, 0 },
			{ "summoned_skull", 3.842787f, CuerpoFig::Bipedo, AtaqueFig::Rayo, { 0.0f, 0.86f, 0.12f }, { 1.25f, 1.05f, 0.35f }, { 1.0f, 1.0f, 0.95f }, false, 0, 0, 0, 0, 0 },
			// lote TRELLIS de 2026-10-19 (rama classic-figuras-lote): modelos normalizados a 1 de alto, pies en 0, de frente +Z;
			// escala = alto logico (tamanos.json) x 2.6/1.8, sin pasar el ancho maximo; con esqueleto Meshy (_anim.glb) miden 1.8
			{ "alligator_s_sword", 2.7444f, CuerpoFig::Bipedo, AtaqueFig::Espada, { 0.064f, 0.364f, 0.4f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.74 m de alto
			{ "alligator_s_sword_dragon", 3.1929f, CuerpoFig::Alado, AtaqueFig::Aliento, { -0.01f, 0.346f, 0.658f }, { 1.7f, 0.45f, 0.1f }, { 1.0f, 0.85f, 0.5f }, false, 0, 0, 0, 0, 0 },   // 3.19 m de alto
			{ "big_shield_gardna", 2.7444f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.041f, 0.502f, 0.278f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.74 m de alto
			{ "blue_eyes_ultimate_dragon", 3.8703f, CuerpoFig::Alado, AtaqueFig::Aliento, { 0.229f, 0.548f, 0.528f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 3.87 m de alto
			{ "buster_blader", 2.8889f, CuerpoFig::Bipedo, AtaqueFig::Espada, { -0.105f, 0.586f, 0.256f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.89 m de alto
			{ "curse_of_dragon", 3.7556f, CuerpoFig::Alado, AtaqueFig::Aliento, { -0.002f, 0.656f, 0.555f }, { 1.7f, 0.45f, 0.1f }, { 1.0f, 0.85f, 0.5f }, false, 0, 0, 0, 0, 0 },   // 3.76 m de alto
			{ "cyber_jar", 1.7333f, CuerpoFig::Compacto, AtaqueFig::Embestida, { -0.013f, 0.447f, 0.499f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 1.73 m de alto
			{ "dark_elf", 2.4556f, CuerpoFig::Bipedo, AtaqueFig::Magia, { 0.026f, 0.515f, 0.139f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 2.46 m de alto
			{ "exodia_the_forbidden_one", 3.7556f, CuerpoFig::Bipedo, AtaqueFig::Rayo, { 0.0f, 0.62f, 0.2f }, { 1.5f, 1.1f, 0.35f }, { 1.0f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 3.76 m de alto
			{ "flame_swordsman", 1.5247f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.0f, 1.2667f, 0.3f }, { 1.7f, 0.45f, 0.1f }, { 1.0f, 0.85f, 0.5f }, false, 0, 0, 0, 0, 0 },   // esqueleto Meshy
			{ "gaia", 3.3316f, CuerpoFig::Cuadrupedo, AtaqueFig::Espada, { -0.126f, 0.528f, 0.57f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.33 m de alto
			{ "gaia_the_dragon_champion", 3.4323f, CuerpoFig::Alado, AtaqueFig::Aliento, { 0.002f, 0.581f, 0.667f }, { 0.5f, 1.4f, 0.4f }, { 0.95f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 3.43 m de alto
			{ "gearfried_the_iron_knight", 2.8889f, CuerpoFig::Bipedo, AtaqueFig::Espada, { 0.027f, 0.734f, 0.118f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.89 m de alto
			{ "gemini_elf", 2.4556f, CuerpoFig::Bipedo, AtaqueFig::Magia, { 0.0f, 0.55f, 0.25f }, { 0.5f, 1.4f, 0.4f }, { 0.95f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 2.46 m de alto
			{ "great_moth", 3.4213f, CuerpoFig::Alado, AtaqueFig::Embestida, { -0.116f, 0.583f, 0.614f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.42 m de alto
			{ "harpie_lady", 2.5278f, CuerpoFig::Alado, AtaqueFig::Embestida, { -0.049f, 0.598f, 0.142f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.53 m de alto
			{ "harpie_lady_sisters", 1.9678f, CuerpoFig::Alado, AtaqueFig::Embestida, { 0.0f, 0.65f, 0.15f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 1.97 m de alto (3 hermanas TRELLIS unidas en fila, 2.2 de ancho)
			{ "harpie_s_pet_dragon", 3.3445f, CuerpoFig::Alado, AtaqueFig::Aliento, { 0.049f, 0.349f, 0.652f }, { 1.7f, 0.45f, 0.1f }, { 1.0f, 0.85f, 0.5f }, false, 0, 0, 0, 0, 0 },   // 3.34 m de alto
			{ "insect_queen", 3.4667f, CuerpoFig::Alado, AtaqueFig::Embestida, { -0.015f, 0.585f, 0.323f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.47 m de alto
			{ "labyrinth_wall", 1.5296f, CuerpoFig::Compacto, AtaqueFig::Embestida, { 0.401f, 0.407f, 1.11f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 1.53 m de alto
			{ "left_arm_of_the_forbidden_one", 2.0222f, CuerpoFig::Compacto, AtaqueFig::Embestida, { -0.127f, 0.336f, 0.347f }, { 1.5f, 1.1f, 0.35f }, { 1.0f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 2.02 m de alto
			{ "left_leg_of_the_forbidden_one", 2.0222f, CuerpoFig::Compacto, AtaqueFig::Embestida, { -0.01f, 0.898f, 0.229f }, { 1.5f, 1.1f, 0.35f }, { 1.0f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 2.02 m de alto
			{ "panther_warrior", 2.7444f, CuerpoFig::Bipedo, AtaqueFig::Espada, { -0.001f, 0.491f, 0.46f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.74 m de alto
			{ "perfectly_ultimate_great_moth", 4.0734f, CuerpoFig::Alado, AtaqueFig::Embestida, { -0.01f, 0.481f, 0.453f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 4.04 m de alto
			{ "red_eyes_black_metal_dragon", 4.1889f, CuerpoFig::Alado, AtaqueFig::Aliento, { 0.036f, 0.498f, 0.539f }, { 1.7f, 0.45f, 0.1f }, { 1.0f, 0.85f, 0.5f }, false, 0, 0, 0, 0, 0 },   // 4.19 m de alto
			{ "right_arm_of_the_forbidden_one", 2.0222f, CuerpoFig::Compacto, AtaqueFig::Embestida, { -0.064f, 0.68f, 0.494f }, { 1.5f, 1.1f, 0.35f }, { 1.0f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 2.02 m de alto
			{ "right_leg_of_the_forbidden_one", 2.0222f, CuerpoFig::Compacto, AtaqueFig::Embestida, { 0.01f, 0.898f, 0.229f }, { 1.5f, 1.1f, 0.35f }, { 1.0f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 2.02 m de alto (espejo de left_leg)
			{ "rocket_warrior", 2.1667f, CuerpoFig::Bipedo, AtaqueFig::Espada, { -0.023f, 0.523f, 0.085f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.17 m de alto
			{ "sangan", 1.3000f, CuerpoFig::Compacto, AtaqueFig::Embestida, { 0.187f, 0.694f, 0.328f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 1.30 m de alto
			{ "swordsman_of_landstar", 0.8827f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.0f, 0.7333f, 0.3f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // esqueleto Meshy
			{ "thousand_dragon", 2.3111f, CuerpoFig::Cuadrupedo, AtaqueFig::Aliento, { 0.074f, 0.252f, 0.708f }, { 1.7f, 0.45f, 0.1f }, { 1.0f, 0.85f, 0.5f }, false, 0, 0, 0, 0, 0 },   // 2.31 m de alto
			{ "time_wizard", 1.4444f, CuerpoFig::Compacto, AtaqueFig::Embestida, { 0.0f, 0.868f, 0.301f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 1.44 m de alto
			{ "twin_headed_thunder_dragon", 2.6633f, CuerpoFig::Cuadrupedo, AtaqueFig::Rayo, { 0.0f, 0.45f, 0.7f }, { 1.25f, 1.05f, 0.35f }, { 1.0f, 1.0f, 0.95f }, false, 0, 0, 0, 0, 0 },   // 2.66 m de alto
			{ "vorse_raider", 2.8889f, CuerpoFig::Bipedo, AtaqueFig::Espada, { -0.072f, 0.748f, 0.241f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.89 m de alto
			{ "witch_of_the_black_forest", 1.4043f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.0f, 1.1667f, 0.3f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // esqueleto Meshy
			// lote TRELLIS de cartas firma de 2026-10-19 (rama classic-figuras-firma): mismas reglas que el lote anterior;
			// los dioses egipcios mas grandes (ancho maximo 5.0-5.6 m, pueden pasar sobre el espacio entre casillas)
			{ "amphibious_bugroth", 2.6757f, CuerpoFig::Compacto, AtaqueFig::Rayo, { 0.0f, 0.62f, 0.55f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 2.68 m de alto
			{ "ape_fighter", 2.7444f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.128f, 0.25f, 0.216f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.74 m de alto
			{ "arcana_knight_joker", 2.8889f, CuerpoFig::Bipedo, AtaqueFig::Espada, { 0.156f, 0.551f, 0.185f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.89 m de alto
			{ "armor_breaker", 1.7333f, CuerpoFig::Compacto, AtaqueFig::Embestida, { 0.143f, 0.577f, 0.406f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 1.73 m de alto
			{ "berfomet", 3.1778f, CuerpoFig::Alado, AtaqueFig::Embestida, { 0.15f, 0.894f, 0.414f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "chimera_the_flying_mythical_beast", 3.1778f, CuerpoFig::Cuadrupedo, AtaqueFig::Embestida, { -0.076f, 0.431f, 0.502f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "copycat", 1.8778f, CuerpoFig::Bipedo, AtaqueFig::Magia, { 0.0f, 0.78f, 0.25f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 1.88 m de alto
			{ "cyber_commander", 2.6000f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.072f, 0.879f, 0.082f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.60 m de alto
			{ "diabound_kernel", 3.7313f, CuerpoFig::Compacto, AtaqueFig::Rayo, { -0.204f, 0.898f, 0.504f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 3.73 m de alto
			{ "fortress_whale", 3.1177f, CuerpoFig::Compacto, AtaqueFig::Embestida, { -0.005f, 0.898f, 0.627f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.12 m de alto
			{ "gaap_the_divine_soldier", 3.1778f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { -0.134f, 0.3f, 0.135f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "gilford_the_lightning", 2.8889f, CuerpoFig::Bipedo, AtaqueFig::Espada, { 0.0f, 0.606f, 0.278f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.89 m de alto
			{ "graveyard_and_the_hand_of_invitation", 2.0222f, CuerpoFig::Compacto, AtaqueFig::Embestida, { -0.056f, 0.599f, 0.268f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.02 m de alto
			{ "guardian_dreadscythe", 2.8889f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.14f, 0.57f, 0.228f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.89 m de alto
			{ "guardian_eatos", 2.7444f, CuerpoFig::Alado, AtaqueFig::Magia, { 0.0f, 0.62f, 0.2f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 2.74 m de alto
			{ "happy_lover", 1.4444f, CuerpoFig::Alado, AtaqueFig::Rayo, { 0.0f, 0.737f, 0.294f }, { 1.6f, 0.4f, 1.0f }, { 1.0f, 0.9f, 0.95f }, false, 0, 0, 0, 0, 0 },   // 1.44 m de alto
			{ "jack_s_knight", 2.7444f, CuerpoFig::Bipedo, AtaqueFig::Espada, { 0.015f, 0.508f, 0.165f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.74 m de alto
			{ "jinzo", 2.7444f, CuerpoFig::Bipedo, AtaqueFig::Rayo, { 0.0f, 0.62f, 0.15f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 2.74 m de alto
			{ "jinzo_returner", 1.2840f, CuerpoFig::Bipedo, AtaqueFig::Rayo, { 0.0f, 1.0667f, 0.3f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // esqueleto Meshy
			{ "kairyu_shin", 3.1338f, CuerpoFig::Compacto, AtaqueFig::Aliento, { 0.121f, 0.466f, 0.611f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 3.13 m de alto
			{ "king_s_knight", 2.7444f, CuerpoFig::Bipedo, AtaqueFig::Espada, { 0.128f, 0.543f, 0.244f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.74 m de alto
			{ "magician_of_black_chaos", 2.8889f, CuerpoFig::Bipedo, AtaqueFig::Magia, { 0.0f, 0.62f, 0.18f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 2.89 m de alto
			{ "magician_of_faith", 2.1667f, CuerpoFig::Bipedo, AtaqueFig::Magia, { 0.214f, 0.838f, 0.259f }, { 1.5f, 1.1f, 0.35f }, { 1.0f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 2.17 m de alto
			{ "magician_s_valkyria", 2.3111f, CuerpoFig::Bipedo, AtaqueFig::Magia, { -0.335f, 0.532f, 0.104f }, { 0.5f, 1.4f, 0.4f }, { 0.95f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 2.31 m de alto
			{ "maiden_with_eyes_of_blue", 2.3111f, CuerpoFig::Bipedo, AtaqueFig::Magia, { 0.0f, 0.6f, 0.2f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 2.31 m de alto
			{ "marshmallon", 1.1556f, CuerpoFig::Compacto, AtaqueFig::Embestida, { 0.0f, 0.329f, 0.468f }, { 1.6f, 0.4f, 1.0f }, { 1.0f, 0.9f, 0.95f }, false, 0, 0, 0, 0, 0 },   // 1.16 m de alto
			{ "megazowler", 2.5303f, CuerpoFig::Cuadrupedo, AtaqueFig::Embestida, { -0.148f, 0.46f, 0.734f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.53 m de alto
			{ "mystical_beast_of_serket", 2.2683f, CuerpoFig::Cuadrupedo, AtaqueFig::Embestida, { -0.339f, 0.252f, 0.841f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 2.27 m de alto
			{ "obelisk_the_tormentor", 5.2000f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.029f, 0.581f, 0.171f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 5.20 m de alto
			{ "orgoth_the_relentless", 3.1778f, CuerpoFig::Bipedo, AtaqueFig::Espada, { -0.135f, 0.699f, 0.189f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "prinzessin", 2.3111f, CuerpoFig::Bipedo, AtaqueFig::Magia, { -0.128f, 0.771f, 0.336f }, { 1.6f, 0.4f, 1.0f }, { 1.0f, 0.9f, 0.95f }, false, 0, 0, 0, 0, 0 },   // 2.31 m de alto
			{ "pumpkin_carriage", 2.3111f, CuerpoFig::Compacto, AtaqueFig::Embestida, { -0.087f, 0.354f, 0.52f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.31 m de alto
			{ "queen_s_knight", 2.6000f, CuerpoFig::Bipedo, AtaqueFig::Espada, { 0.02f, 0.586f, 0.124f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 2.60 m de alto
			{ "satellite_cannon", 2.3111f, CuerpoFig::Compacto, AtaqueFig::Rayo, { 0.0f, 0.492f, 0.736f }, { 1.25f, 1.05f, 0.35f }, { 1.0f, 1.0f, 0.95f }, false, 0, 0, 0, 0, 0 },   // 2.31 m de alto
			{ "slifer_the_sky_dragon", 3.7716f, CuerpoFig::Alado, AtaqueFig::Rayo, { 0.073f, 0.576f, 0.726f }, { 1.25f, 1.05f, 0.35f }, { 1.0f, 1.0f, 0.95f }, false, 0, 0, 0, 0, 0 },   // 3.77 m de alto
			{ "sword_arm_of_dragon", 3.0207f, CuerpoFig::Cuadrupedo, AtaqueFig::Embestida, { 0.122f, 0.459f, 0.629f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.02 m de alto
			{ "the_white_stone_of_legend", 1.1556f, CuerpoFig::Compacto, AtaqueFig::Embestida, { -0.037f, 0.43f, 0.306f }, { 0.35f, 0.65f, 1.7f }, { 0.9f, 0.96f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 1.16 m de alto
			{ "the_winged_dragon_of_ra", 4.5939f, CuerpoFig::Alado, AtaqueFig::Aliento, { -0.05f, 0.68f, 0.3f }, { 1.5f, 1.1f, 0.35f }, { 1.0f, 1.0f, 0.85f }, false, 0, 0, 0, 0, 0 },   // 4.59 m de alto
			{ "toon_dark_magician_girl", 2.0222f, CuerpoFig::Compacto, AtaqueFig::Magia, { 0.11f, 0.77f, 0.504f }, { 1.6f, 0.4f, 1.0f }, { 1.0f, 0.9f, 0.95f }, false, 0, 0, 0, 0, 0 },   // 2.02 m de alto
			{ "ultimate_tyranno", 4.0444f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.158f, 0.267f, 0.442f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 4.04 m de alto
			{ "valkyrie_brunhilde", 3.3222f, CuerpoFig::Cuadrupedo, AtaqueFig::Espada, { 0.068f, 0.882f, 0.561f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.32 m de alto
			{ "valkyrie_dritte", 3.1778f, CuerpoFig::Cuadrupedo, AtaqueFig::Espada, { 0.013f, 0.656f, 0.501f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "valkyrie_erste", 3.1778f, CuerpoFig::Cuadrupedo, AtaqueFig::Espada, { 0.002f, 0.561f, 0.498f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "valkyrie_funfte", 3.1778f, CuerpoFig::Cuadrupedo, AtaqueFig::Espada, { 0.112f, 0.753f, 0.387f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "valkyrie_sechste", 3.1778f, CuerpoFig::Cuadrupedo, AtaqueFig::Espada, { -0.175f, 0.659f, 0.436f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "valkyrie_vierte", 3.1778f, CuerpoFig::Cuadrupedo, AtaqueFig::Espada, { 0.022f, 0.537f, 0.499f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "valkyrie_zweite", 3.1778f, CuerpoFig::Cuadrupedo, AtaqueFig::Espada, { 0.033f, 0.731f, 0.546f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 },   // 3.18 m de alto
			{ "viser_des", 2.3111f, CuerpoFig::Compacto, AtaqueFig::Embestida, { 0.12f, 0.556f, 0.173f }, { 0.7f, 0.25f, 1.5f }, { 0.95f, 0.85f, 1.0f }, false, 0, 0, 0, 0, 0 },   // 2.31 m de alto
		};
		for (const DatosFigura& d : TABLA) {
			if (slug == d.slug) return &d;
		}
		return nullptr;
	}

	struct PoseNodo { glm::vec3 t, s; glm::quat r; };
	struct Figura {
		std::string slug;
		vkglTF::Model modelo;
		DatosFigura datos{};
		bool esqueleto{ false };
		int reposo{ -1 }, ataque{ -1 }, golpe{ -1 }, defensa{ -1 }, muerte{ -1 };
		float escala{ 1.0f }, alto{ 2.6f };   // escala del modelo y alto en el tablero (m)
		glm::vec3 centro{ 0.0f };             // centro de la base (modelo): x, y mas baja, z
		vkglTF::Node* mano{ nullptr };        // (esqueleto) el hueso de la mano del arma
		glm::mat4 agarre{ 1.0f };             // (Espada) de la mano en reposo al arma
		std::vector<PoseNodo> pose;           // (esqueleto) para mezclar clips
		int triangulos{ 0 };
	};
	struct EstadoFigura {
		std::string carta;
		int fig{ -1 };
		bool visible{ false }, muerto{ false };
		float tAparece{ 99.0f }, pesoDef{ 0.0f };
		glm::mat4 ultima{ 1.0f };
		float ultimoDis{ 0.0f };
	};
	struct Fantasma { int fig; glm::mat4 m; float t; };
	struct PipelinesFiguras {
		VkDevice d{ VK_NULL_HANDLE };
		VkPipelineLayout layout{ VK_NULL_HANDLE };
		VkPipeline escena{ VK_NULL_HANDLE }, sombra{ VK_NULL_HANDLE };
		~PipelinesFiguras()
		{
			if (d == VK_NULL_HANDLE) return;
			vkDestroyPipeline(d, escena, nullptr);
			vkDestroyPipeline(d, sombra, nullptr);
			vkDestroyPipelineLayout(d, layout, nullptr);
		}
	};

	std::array<Figura, MAX_FIGURAS> figuras;
	int numFiguras{ 0 };
	std::unordered_map<std::string, int> figuraDeCarta;   // id de carta -> figuras[]
	std::array<EstadoFigura, interfaz::NUM_ZONAS> estFig{};
	std::vector<Fantasma> fantasmas;
	PipelinesFiguras pipesFig;
	vkglTF::Model espadaFig;
	bool hayEspadaFig{ false };
	bool figuras3D{ true };     // Ajustes: "Figuras 3D"
	bool medirFiguras{ false };
	// PC: la mesa y los ataques de prueba
	std::vector<std::pair<std::string, std::string>> pruebaMesaFig;
	struct AtaquePruebaFig { float t; std::string a, o; bool hecho; };
	std::vector<AtaquePruebaFig> pruebaAtaquesFig;
	bool pruebaMesaHecha{ false }, congelarIAFig{ false };
	float medirFigSuma{ 0.0f };
	int medirFigCuadros{ 0 };

	// ---------------------------------------------------------------------------------------------- carga
	static std::string clipCon(const vkglTF::Model& m, std::initializer_list<const char*> nombres, int& clip)
	{
		for (size_t k = 0; k < m.animations.size() && clip < 0; k++) {
			std::string n = m.animations[k].name;
			std::transform(n.begin(), n.end(), n.begin(), ::tolower);
			for (const char* x : nombres) {
				if (n.find(x) != std::string::npos) {
					clip = (int)k;
					break;
				}
			}
		}
		return clip >= 0 ? m.animations[(size_t)clip].name : std::string("-");
	}

	void prepararFiguras()
	{
#if defined(PS5_HOST_REFERENCE)
		if (const char* v = getenv("YUGI_FIGURAS")) figuras3D = atoi(v) != 0;
		medirFiguras = getenv("YUGI_FIGURAS_MEDIR") != nullptr;
		if (const char* v = getenv("YUGI_FIGURAS_MESA")) {
			std::stringstream lista(v);
			std::string par;
			while (std::getline(lista, par, ',')) {
				const size_t i = par.find('=');
				if (i != std::string::npos) pruebaMesaFig.push_back({ par.substr(0, i), par.substr(i + 1) });
			}
		}
		if (const char* v = getenv("YUGI_FIGURAS_ATAQUE")) {
			std::stringstream lista(v);
			std::string a;
			while (std::getline(lista, a, ';')) {
				const size_t p = a.find(':'), g = a.find('>');
				if (p != std::string::npos && g != std::string::npos && g > p) {
					pruebaAtaquesFig.push_back({ (float)atof(a.substr(0, p).c_str()), a.substr(p + 1, g - p - 1), a.substr(g + 1), false });
				}
			}
		}
#endif
		const auto inicio = std::chrono::steady_clock::now();
		std::ifstream tabla(getAssetPath() + "ui/figuras.txt");
		std::string linea;
		const std::string carpeta = getAssetPath() + "models/yugi/";
		while (std::getline(tabla, linea)) {
			if (linea.empty() || linea[0] == '#') continue;
			std::istringstream campos(linea);
			std::string carta, slug;
			if (!(campos >> carta >> slug)) continue;
			int idx = -1;
			for (int k = 0; k < numFiguras; k++) {
				if (figuras[(size_t)k].slug == slug) idx = k;
			}
			if (idx < 0 && numFiguras < MAX_FIGURAS) {
				idx = cargarFigura(carpeta, slug) ? numFiguras++ : -1;
			}
			if (idx >= 0) figuraDeCarta[carta] = idx;
		}
		if (std::ifstream(carpeta + "espada.glb").good()) {
			espadaFig.loadFromFile(carpeta + "espada.glb", vulkanDevice, queue, vkglTF::FileLoadingFlags::PreTransformVertices);
			hayEspadaFig = true;
		}
		for (int k = 0; k < numFiguras; k++) {
			Figura& f = figuras[(size_t)k];
			if (f.esqueleto && f.datos.ataque == AtaqueFig::Espada && hayEspadaFig) empunarFigura(f);
		}
		const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
		fprintf(stderr, "[figuras] %d figuras para %zu cartas emblematicas (%.0f ms)%s\n", numFiguras, figuraDeCarta.size(), ms, figuras3D ? "" : " (apagadas)");
		if (numFiguras > 0) prepararPipelinesFiguras();
		interfazDuelo.figuraEnZona = [this](int z) { return tieneFigura(z); };
	}

	bool cargarFigura(const std::string& carpeta, const std::string& slug)
	{
		Figura& f = figuras[(size_t)numFiguras];
		const std::string anim = carpeta + slug + "_anim.glb", rigida = carpeta + slug + ".glb";
		struct stat st{};
		f.slug = slug;
		f.esqueleto = stat(anim.c_str(), &st) == 0 && st.st_size > 64;
		if (!f.esqueleto && !(stat(rigida.c_str(), &st) == 0 && st.st_size > 64)) {
			fprintf(stderr, "[figuras] %s: sin modelo (queda plana)\n", slug.c_str());
			return false;
		}
		// sin pre-transformar: el esqueleto mueve los vertices en el shader
		f.modelo.loadFromFile(f.esqueleto ? anim : rigida, vulkanDevice, queue, f.esqueleto ? 0u : (uint32_t)vkglTF::FileLoadingFlags::PreTransformVertices);
		const DatosFigura* d = datosFigura(slug);
		f.datos = d ? *d : DatosFigura{ "", 0.0f, CuerpoFig::Bipedo, AtaqueFig::Embestida, { 0.0f, 1.0f, 0.3f }, { 1.0f, 0.8f, 0.5f }, { 1.0f, 1.0f, 0.9f }, false, 0, 0, 0, 0, 0 };
		f.triangulos = f.modelo.indices.count / 3;
		if (f.esqueleto) {
			f.reposo = f.ataque = f.golpe = f.defensa = f.muerte = -1;
			const std::string r = clipCon(f.modelo, { "reposo", "idle" }, f.reposo);
			const std::string a = clipCon(f.modelo, { "ataque", "attack", "slash" }, f.ataque);
			const std::string g = clipCon(f.modelo, { "golpe", "hit" }, f.golpe);
			const std::string df = clipCon(f.modelo, { "defensa", "parry", "block" }, f.defensa);
			const std::string m = clipCon(f.modelo, { "muerte", "dying", "death" }, f.muerte);
			if (f.reposo < 0 && !f.modelo.animations.empty()) f.reposo = 0;
			f.escala = d ? d->escala : ESCALA_PERSONAJE;
			f.alto = 1.8f * f.escala;
			f.centro = glm::vec3(0.0f);
			fprintf(stderr, "[figuras] %s: esqueleto, %d triangulos, clips reposo %s, ataque %s, golpe %s, defensa %s, muerte %s\n", slug.c_str(), f.triangulos,
			        r.c_str(), a.c_str(), g.c_str(), df.c_str(), m.c_str());
		} else {
			const glm::vec3 lo = glm::min(f.modelo.dimensions.min, f.modelo.dimensions.max), hi = glm::max(f.modelo.dimensions.min, f.modelo.dimensions.max);
			const float h = glm::max(hi.y - lo.y, 1e-3f), w = glm::max(glm::max(hi.x - lo.x, hi.z - lo.z), 1e-3f);
			// sin escala en la tabla: 2.6 m de alto sin pasar el ancho de una columna (como armarMatriz de Yugi Reload)
			f.escala = d && d->escala > 0.0f ? d->escala : glm::min(2.6f / h, 2.3f * ESCALA_TABLERO / w);
			f.alto = h * f.escala;
			f.centro = glm::vec3(0.5f * (lo.x + hi.x), lo.y, 0.5f * (lo.z + hi.z));
			fprintf(stderr, "[figuras] %s: rigida, %d triangulos, %.2f m de alto%s\n", slug.c_str(), f.triangulos, f.alto, f.datos.alas ? ", aletea" : "");
		}
		return true;
	}

	// la espada en el puno izquierdo (empunar de Yugi Reload): en reposo dentro del puno y despues pegada al hueso
	void empunarFigura(Figura& f)
	{
		vkglTF::Node* mano = f.modelo.nodeFromName("LeftHand");
		vkglTF::Node* antebrazo = f.modelo.nodeFromName("LeftForeArm");
		if (!mano || !antebrazo) return;
		const glm::mat4 manoReposo = mano->getMatrix();
		const glm::vec3 muneca = glm::vec3(manoReposo[3]);
		const glm::vec3 brazo = glm::normalize(muneca - glm::vec3(antebrazo->getMatrix()[3]));
		const glm::vec3 puno = muneca + brazo * 0.075f;
		const float alto = glm::max(espadaFig.dimensions.max.y - espadaFig.dimensions.min.y, 1e-3f);
		const float largo = 1.05f, agarre = 0.12f;
		const glm::mat4 deseado = glm::translate(glm::mat4(1.0f), puno) * glm::mat4_cast(glm::quat(glm::vec3(0.0f, 1.0f, 0.0f), glm::normalize(glm::vec3(0.0f, -0.55f, 1.0f)))) *
		                          glm::scale(glm::mat4(1.0f), glm::vec3(largo / alto)) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -(espadaFig.dimensions.min.y + agarre * alto), 0.0f));
		f.mano = mano;
		f.agarre = glm::inverse(manoReposo) * deseado;
	}

	void prepararPipelinesFiguras()
	{
		pipesFig.d = device;
		const std::array<VkDescriptorSetLayout, 3> sets = { descriptorSetLayout, vkglTF::descriptorSetLayoutImage, vkglTF::descriptorSetLayoutUbo };
		VkPipelineLayoutCreateInfo li = vks::initializers::pipelineLayoutCreateInfo(sets.data(), static_cast<uint32_t>(sets.size()));
		VkPushConstantRange empuje = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Empuje), 0);
		li.pushConstantRangeCount = 1;
		li.pPushConstantRanges = &empuje;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &li, nullptr, &pipesFig.layout));
		VkPipelineInputAssemblyStateCreateInfo ensamble = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo raster = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState mezcla1 = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo mezcla = vks::initializers::pipelineColorBlendStateCreateInfo(1, &mezcla1);
		VkPipelineDepthStencilStateCreateInfo prof = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo vista = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo muestras = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dinamicos = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dinamico = vks::initializers::pipelineDynamicStateCreateInfo(dinamicos);
		std::array<VkPipelineShaderStageCreateInfo, 2> etapas = {
			loadShader(getShadersPath() + "yugireload/escena_esqueleto.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
			loadShader(getShadersPath() + "yugireload/escena.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
		};
		VkPipelineRenderingCreateInfo render{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &swapChain.colorFormat,
			.depthAttachmentFormat = depthFormat,
			.stencilAttachmentFormat = depthFormat,
		};
		VkGraphicsPipelineCreateInfo ci{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &render,
			.stageCount = static_cast<uint32_t>(etapas.size()),
			.pStages = etapas.data(),
			.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV,
			                                                                   vkglTF::VertexComponent::Joint0, vkglTF::VertexComponent::Weight0 }),
			.pInputAssemblyState = &ensamble,
			.pViewportState = &vista,
			.pRasterizationState = &raster,
			.pMultisampleState = &muestras,
			.pDepthStencilState = &prof,
			.pColorBlendState = &mezcla,
			.pDynamicState = &dinamico,
			.layout = pipesFig.layout,
		};
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, nullptr, &pipesFig.escena));
		// sombra: solo profundidad, con el mismo desplazamiento que la del tablero
		VkPipelineRenderingCreateInfo renderSombra{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 0,
			.depthAttachmentFormat = FORMATO_SOMBRA,
		};
		VkPipelineColorBlendStateCreateInfo sinColor = vks::initializers::pipelineColorBlendStateCreateInfo(0, nullptr);
		raster.depthBiasEnable = VK_TRUE;
		raster.depthBiasConstantFactor = 1.25f;
		raster.depthBiasSlopeFactor = 1.75f;
		VkPipelineShaderStageCreateInfo etapaSombra = loadShader(getShadersPath() + "yugireload/sombra_esqueleto.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		ci.pNext = &renderSombra;
		ci.stageCount = 1;
		ci.pStages = &etapaSombra;
		ci.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Joint0,
		                                                                    vkglTF::VertexComponent::Weight0 });
		ci.pColorBlendState = &sinColor;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &ci, nullptr, &pipesFig.sombra));
	}

	// ---------------------------------------------------------------------------------------------- estado
	int figuraDe(const std::string& carta) const
	{
		const auto it = figuraDeCarta.find(carta);
		return it == figuraDeCarta.end() ? -1 : it->second;
	}
	bool tieneFigura(int z) const
	{
		return figuras3D && z >= 0 && z < (int)interfaz::NUM_ZONAS && estFig[(size_t)z].fig >= 0 && estFig[(size_t)z].visible;
	}
	// el reloj del ataque plano corre mas lento si una figura ataca o recibe (avanzarAtaque)
	// (se decide al empezar el ataque: si el objetivo boca abajo se voltea en el golpe, el ritmo no cambia a mitad)
	// (rama classic-ps5-ataque3s: el ataque dura ~3 s como Joey: con una figura que ataca, solo hasta el golpe el reloj va a
	// ritmo::VEL_FIG_PREVIA (la carga del rayo, 0.75 s, igual con el viaje "High"); despues, al ritmo de la carta plana.
	// Antes todo el ataque iba a VEL_FIGURAS y duraba 2.9 s, ahora 3.25 s)
	float velAtaqueFig{ 1.0f };   // el ritmo hasta el golpe
	float velPostFig{ 1.0f };     // y despues (rama classic-ps5-ataque4)
	// la figura de la zona ataca a distancia (aliento, magia, rayo): su efecto va entero antes del golpe (ataque4)
	bool figuraADistancia(int z) const
	{
		if (!tieneFigura(z)) return false;
		const AtaqueFig at = figuras[(size_t)estFig[(size_t)z].fig].datos.ataque;
		return at == AtaqueFig::Aliento || at == AtaqueFig::Magia || at == AtaqueFig::Rayo;
	}
	float velocidadAtaque()
	{
		if (!hayAtaque()) return 1.0f;
		if (tAtaque == 0.0f) {
			const float previo = ritmo::realAtaque(T_IMPACTO, ataqueAltoMD);   // (s reales del viaje sin figura: 0.45 / 0.65)
			if (figuraADistancia(zonaAtacante)) {
				velAtaqueFig = previo / ritmo::T_FIG_DISTANCIA;
				velPostFig = ritmo::VEL_FIG_POST;
			} else {
				velAtaqueFig = tieneFigura(zonaAtacante) ? previo / (ritmo::T_ATAQUE_BAJO / ritmo::VEL_FIG_PREVIA) : 1.0f;
				velPostFig = 1.0f;
			}
		}
		return tAtaque < T_IMPACTO ? velAtaqueFig : velPostFig;
	}
	// segundos reales desde el empiece del ataque hasta tg (el reloj del ataque)
	float realAtaqueFig(float tg) const
	{
		const float v = glm::max(velAtaqueFig, 0.01f), w = glm::max(velPostFig, 0.05f);
		return ritmo::realAtaque(glm::min(tg, T_IMPACTO), ataqueAltoMD) / v + glm::max(0.0f, tg - T_IMPACTO) / w;
	}

	void avanzarFiguras(float dt)
	{
		pruebaFiguras();
		if (numFiguras == 0) return;
		const auto inicio = std::chrono::steady_clock::now();
		for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
			if (!esMonstruo(z)) continue;
			EstadoFigura& e = estFig[(size_t)z];
			const control::Celda& c = ctl.zonas[(size_t)z];
			const int f = !c.vacia() && !c.bocaAbajo ? figuraDe(c.carta) : -1;
			if (f != e.fig || c.carta != e.carta) {
				// la figura que estaba se va sin morir en un ataque (magia, tributo, a la mano...): se desvanece ahi
				if (e.fig >= 0 && e.visible && !e.muerto && figuras3D) fantasmas.push_back({ e.fig, e.ultima, 0.0f });
				e = EstadoFigura{};
				e.carta = c.carta;
				e.fig = f;
				e.visible = f >= 0;
				e.tAparece = 0.0f;
				e.pesoDef = c.defensa ? 1.0f : 0.0f;
			}
			e.tAparece += dt;
			e.pesoDef = glm::clamp(e.pesoDef + (c.defensa ? dt : -dt) * 2.5f, 0.0f, 1.0f);
			const bool muere = hayAtaque() && ((z == zonaObjetivo && muereO) || (z == zonaAtacante && muereA));
			if (muere && tAtaque > interfaz::Interfaz::G_FIN - 0.08f) e.muerto = true;
			if (e.muerto && !hayAtaque() && f >= 0) {
				e.muerto = false;   // (la demo: la destruida vuelve a aparecer)
				e.tAparece = 0.0f;
			}
		}
		for (Fantasma& g : fantasmas) g.t += dt;
		fantasmas.erase(std::remove_if(fantasmas.begin(), fantasmas.end(), [](const Fantasma& g) { return g.t > 0.9f; }), fantasmas.end());
		if (figuras3D) posarFiguras();
		if (medirFiguras) {
			medirFigSuma += (float)std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - inicio).count();
			if (++medirFigCuadros == 60) {
				int n = 0, tri = 0;
				for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
					if (tieneFigura(z)) {
						n++;
						tri += figuras[(size_t)estFig[(size_t)z].fig].triangulos;
					}
				}
				fprintf(stderr, "[figuras] %d en la mesa, %d triangulos (x2 con la sombra), CPU %.3f ms por cuadro, cuadro %.2f ms\n", n, tri, medirFigSuma / 60.0f,
				        1000.0f * frameTimer);
				medirFigSuma = 0.0f;
				medirFigCuadros = 0;
			}
		}
	}

	// ---------------------------------------------------------------------------------------------- poses
	static void posarClip(vkglTF::Model& m, int clip, float t, bool repetir)
	{
		if (clip < 0 || clip >= (int)m.animations.size()) return;
		const vkglTF::Animation& a = m.animations[(size_t)clip];
		const float largo = glm::max(a.end - a.start, 1e-3f);
		t = repetir ? fmodf(glm::max(t, 0.0f), largo) : glm::clamp(t, 0.0f, largo - 0.001f);
		m.updateAnimation((uint32_t)clip, a.start + t);
	}
	static void guardarPose(vkglTF::Model& m, std::vector<PoseNodo>& p)
	{
		p.resize(m.linearNodes.size());
		for (size_t k = 0; k < m.linearNodes.size(); k++) {
			const vkglTF::Node* n = m.linearNodes[k];
			p[k] = { n->translation, n->scale, n->rotation };
		}
	}
	// lo guardado (a) mezclado con la pose actual (peso w de la actual)
	static void mezclarPose(vkglTF::Model& m, const std::vector<PoseNodo>& a, float w)
	{
		for (size_t k = 0; k < m.linearNodes.size() && k < a.size(); k++) {
			vkglTF::Node* n = m.linearNodes[k];
			n->translation = glm::mix(a[k].t, n->translation, w);
			n->scale = glm::mix(a[k].s, n->scale, w);
			n->rotation = glm::slerp(a[k].r, n->rotation, w);
		}
	}
	void clipEncima(Figura& f, int clip, float t, float w)
	{
		if (clip < 0 || w <= 0.0f) return;
		guardarPose(f.modelo, f.pose);
		posarClip(f.modelo, clip, t, false);
		mezclarPose(f.modelo, f.pose, glm::min(w, 1.0f));
	}

	// La pose de cada modelo con esqueleto (una por modelo y cuadro: si hay dos copias de la misma carta, manda la que
	// esta en el ataque). Tiempos en el reloj del ataque plano (tg): el golpe llega en T_IMPACTO
	void posarFiguras()
	{
		for (int k = 0; k < numFiguras; k++) {
			Figura& f = figuras[(size_t)k];
			if (!f.esqueleto) continue;
			int zona = -1;
			for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
				if (!tieneFigura(z) || estFig[(size_t)z].fig != k) continue;
				if (zona < 0 || (hayAtaque() && (z == zonaAtacante || z == zonaObjetivo))) zona = z;
			}
			if (zona < 0) continue;
			const EstadoFigura& e = estFig[(size_t)zona];
			const float fase = (float)zona * 0.37f;
			posarClip(f.modelo, f.reposo, tiempo + fase, true);
			// en defensa: el punto de mas cobertura de su clip, respirando
			if (f.defensa >= 0 && e.pesoDef > 0.0f) clipEncima(f, f.defensa, 0.5f - 0.03f * (0.5f + 0.5f * sinf(tiempo * 2.6f)), e.pesoDef);
			clipEntradaClave(zona, f);   // rama classic-cartas-clave: con esqueleto, la entrada es su clip de ataque
			if (hayAtaque()) {
				const float tg = tAtaque, v = velocidadAtaque();
				const float real = realAtaqueFig(tg), impactoReal = realAtaqueFig(T_IMPACTO);
				if (zona == zonaAtacante) {
					if (f.ataque >= 0) {
						// el golpe del clip (1 s) cae en el impacto; a distancia, cuando sale el efecto (ataque4)
						const float t = real - ((figuraADistancia(zona) ? YR_SUELTA - YR_INICIO : impactoReal) - 1.0f);
						const float largo = f.modelo.animations[(size_t)f.ataque].end - f.modelo.animations[(size_t)f.ataque].start;
						clipEncima(f, f.ataque, t, suave(0.0f, 0.2f, t) * (1.0f - suave(largo - 0.25f, largo, t)));
					} else if (f.datos.ataque == AtaqueFig::Espada && f.defensa >= 0) {
						// sin clip de ataque: el tajo es la parada de la espada, rapida, en el impacto
						const float t = (real - (impactoReal - 0.35f)) * 1.6f;
						clipEncima(f, f.defensa, t, suave(0.0f, 0.15f, t) * (1.0f - suave(0.9f, 1.4f, t)));
					}
				}
				if (zona == zonaObjetivo || (zona == zonaAtacante && muereA)) {
					const bool muere = zona == zonaObjetivo ? muereO : muereA;
					if (f.golpe >= 0 && tg >= T_IMPACTO) clipEncima(f, f.golpe, real - impactoReal, suave(T_IMPACTO, T_IMPACTO + 0.04f, tg));
					else if (f.defensa >= 0 && zona == zonaObjetivo && tg < T_IMPACTO + 0.2f) clipEncima(f, f.defensa, real - (impactoReal - 0.5f), 0.6f * suave(T_IMPACTO - 0.25f, T_IMPACTO, tg));
					if (muere && f.muerte >= 0 && tg >= ritmo::T_LP + 0.05f) {   // (ataque4: muere despues de los LP)
						// cae (se salta el comienzo de pie del clip) desde la pose del golpe
						const float lpReal = realAtaqueFig(ritmo::T_LP);
						clipEncima(f, f.muerte, (real - (lpReal + 0.05f / v)) * 1.3f + 0.65f, suave(ritmo::T_LP + 0.05f, ritmo::T_LP + 0.15f, tg));
					}
				}
			}
			for (vkglTF::Node* n : f.modelo.nodes) n->update();
		}
	}

	glm::vec3 baseZona(int z) const { return centroZona(z) + glm::vec3(0.0f, SUELO_FIGURA, 0.0f); }

	// angulo (alrededor de Y) que pone el frente del modelo (+Z) mirando hacia d
	static float giroHacia(glm::vec3 d) { return orientacion::giroHacia(d); }

	// La matriz de la figura de la zona z en este instante; dis = desintegracion (< 0 materializandose) y holo
	glm::mat4 matrizFigura(int z, float& dis, float& holo)
	{
		const EstadoFigura& e = estFig[(size_t)z];
		const Figura& f = figuras[(size_t)e.fig];
		const glm::vec3 base = baseZona(z);
		// rama classic-bp-terminar (orientacion_figura.h): de frente a la pantalla (la camara de juego), jugador y rival
		float giro = orientacion::giroReposo(base, OJO);
		glm::vec3 mueve(0.0f);
		float inclina = 0.0f, ladea = 0.0f, aplasta = 1.0f, escala = 1.0f, estrecha = 1.0f, gira = 0.0f;
		const bool alado = f.datos.cuerpo == CuerpoFig::Alado;
		// en reposo: respira; los alados flotan un poco (en defensa bajan al suelo)
		const float vivo = 1.0f - e.pesoDef;
		float vuelo = alado ? (0.32f + 0.07f * sinf(tiempo * 1.7f + (float)z)) * vivo : 0.0f;
		if (!f.esqueleto) aplasta *= 1.0f + 0.012f * sinf(tiempo * 2.3f + (float)z) * vivo;
		// postura de defensa sin esqueleto (o sin clip de defensa): agachado, echado, alas recogidas o compacto
		if (e.pesoDef > 0.0f && !(f.esqueleto && f.defensa >= 0)) {
			const float p = e.pesoDef, rad = glm::pi<float>() / 180.0f;
			switch (f.datos.cuerpo) {
			case CuerpoFig::Bipedo: aplasta *= 1.0f - 0.20f * p; escala *= 1.0f - 0.04f * p; inclina += 11.0f * rad * p; gira += orientacion::GIRA_DEFENSA_BIPEDO * rad * p; ladea += 3.0f * rad * p; break;
			case CuerpoFig::Cuadrupedo: aplasta *= 1.0f - 0.22f * p; inclina += 4.0f * rad * p; break;
			case CuerpoFig::Alado: estrecha *= 1.0f - 0.28f * p; escala *= 1.0f - 0.05f * p; aplasta *= 1.0f - 0.15f * p; inclina += 8.0f * rad * p; break;
			case CuerpoFig::Compacto: escala *= 1.0f - 0.10f * p; aplasta *= 1.0f - 0.12f * p; break;
			}
		}
		// aparece: se materializa y crece un poco desde la carta
		const float ap = suave(0.0f, 0.9f, e.tAparece);
		dis = ap < 1.0f ? -(1.0f - ap) : 0.0f;
		escala *= 0.75f + 0.25f * ap;
		holo = 0.28f + 1.4f * (1.0f - ap);
		poseEntradaClave(z, f, giro, escala, vuelo, holo);   // rama classic-cartas-clave: la entrada de una carta clave (entrada_clave_juego.h)
		if (hayAtaque() && (z == zonaAtacante || z == zonaObjetivo)) {
			const float tg = tAtaque;
			const int otro = z == zonaAtacante ? zonaObjetivo : zonaAtacante;
			// (rama classic-ps5-ataque: el ataque directo va al punto de MD del que recibe, no a su zona vacia: ataque_md_juego.h)
			glm::vec3 haciaOtro = z == zonaAtacante && ataqueDirecto ? destinoAtaqueMundo() : centroZona(otro);
			haciaOtro.y = 0.0f;
			glm::vec3 d = haciaOtro - centroZona(z);
			const float largo = glm::length(d);
			d = largo > 1e-3f ? d / largo : glm::vec3(0.0f, 0.0f, -1.0f);
			// gira hacia el otro (por el lado mas corto), ataca y vuelve a mirar a la pantalla (orientacion_figura.h)
			giro = orientacion::giroAtaque(giro, centroZona(z), haciaOtro, tg);
			if (z == zonaAtacante) {
				holo += 0.9f * suave(0.15f, 0.45f, tg) * (1.0f - suave(0.7f, 1.0f, tg));
				const AtaqueFig at = f.datos.ataque;
				if (at == AtaqueFig::Embestida || at == AtaqueFig::Espada) {
					// va hasta el objetivo (se queda a 1.3 m) y vuelve; los chicos y los de cuatro patas, de un salto
					const float carga = suave(0.08f, 0.25f, tg), va = suave(0.25f, T_IMPACTO, tg), vuelve = suave(0.78f, 1.25f, tg);
					const float avance = glm::max(largo - 1.3f - 0.25f * f.alto, 0.0f) * va * (1.0f - vuelve);
					mueve += d * (avance - 0.3f * carga * (1.0f - va));
					if (f.datos.cuerpo != CuerpoFig::Bipedo || at == AtaqueFig::Embestida) mueve.y += 0.9f * sinf(glm::pi<float>() * va) * (1.0f - vuelve);
					inclina += (-0.12f * carga * (1.0f - va) + 0.3f * va) * (1.0f - vuelve);
				} else {
					// a distancia: se echa atras al cargar y adelante al soltar
					inclina += (-0.08f * suave(0.1f, 0.4f, tg) + 0.18f * suave(0.4f, T_IMPACTO, tg)) * (1.0f - suave(0.8f, 1.2f, tg));
					if (alado) vuelo += 0.35f * suave(0.1f, 0.45f, tg) * (1.0f - suave(0.9f, 1.4f, tg));
				}
			}
			const bool muere = (z == zonaObjetivo && muereO) || (z == zonaAtacante && muereA);
			if (z == zonaObjetivo && tg > T_IMPACTO) {
				// el golpe: retrocede y tiembla
				const float u = tg - T_IMPACTO;
				const float empuje = (muere ? 1.0f : expf(-u * 5.0f)) * suave(0.0f, 0.06f, u);
				mueve += -d * 0.35f * empuje + glm::vec3(sinf(u * 90.0f), 0.0f, cosf(u * 70.0f)) * 0.05f * expf(-u * 8.0f);
				if (!f.esqueleto && !muere) inclina -= 0.18f * expf(-u * 6.0f) * suave(0.0f, 0.05f, u);
			}
			if (muere && tg > ritmo::T_LP + 0.05f) {   // (ataque4: cae despues de que bajan los LP)
				const float u = suave(ritmo::T_LP + 0.05f, ritmo::T_LP + 0.35f, tg);
				if (!f.esqueleto) {
					// sin esqueleto: los alados caen al suelo de costado, los chicos se aplastan, el resto cae de espaldas
					if (alado) {
						vuelo *= 1.0f - u;
						ladea += 1.2f * u;
					} else if (f.datos.cuerpo == CuerpoFig::Compacto) {
						aplasta *= 1.0f - 0.55f * u;
						escala *= 1.0f + 0.15f * u;
					} else {
						inclina -= 1.35f * u * u;
					}
				}
				dis = glm::max(dis, suave(ritmo::T_LP + 0.35f, interfaz::Interfaz::G_FIN - 0.1f, tg));
				holo += 1.2f * u;
			}
		}
		if (e.muerto) dis = 1.0f;
		// rama classic-ps5-ataque5: la figura del atacante declarado se eleva con su carta (interfaz.h ELEV_ALTO)
		const float elev = elevacionDe(z) * interfaz::Interfaz::ELEV_ALTO;
		const glm::mat4 lugar = glm::translate(glm::mat4(1.0f), base + mueve + glm::vec3(0.0f, vuelo + elev, 0.0f));
		const glm::mat4 rot = glm::rotate(glm::mat4(1.0f), giro + gira, glm::vec3(0.0f, 1.0f, 0.0f)) *
		                      glm::rotate(glm::mat4(1.0f), ladea, glm::vec3(0.0f, 0.0f, 1.0f)) * glm::rotate(glm::mat4(1.0f), inclina, glm::vec3(1.0f, 0.0f, 0.0f));
		const glm::mat4 forma = glm::scale(glm::mat4(1.0f), glm::vec3(estrecha * escala, aplasta * escala, escala) * f.escala);
		return lugar * rot * forma * glm::translate(glm::mat4(1.0f), -f.centro);
	}

	// el angulo de las alas (aleteo sin esqueleto) y sus datos para escena.vert (ojoA, ojoB)
	bool alasFigura(int z, const Figura& f, glm::vec4& ojoA, glm::vec4& ojoB) const
	{
		if (!f.datos.alas || f.esqueleto) return false;
		const EstadoFigura& e = estFig[(size_t)z];
		const glm::vec3 lo = glm::min(f.modelo.dimensions.min, f.modelo.dimensions.max), hi = glm::max(f.modelo.dimensions.min, f.modelo.dimensions.max);
		const glm::vec3 tam = hi - lo;
		float hz = glm::clamp(1.9f - 0.22f * f.alto, 0.7f, 1.6f);
		float amp = 0.42f * (1.0f - e.pesoDef);
		if (hayAtaque() && (z == zonaObjetivo ? muereO : (z == zonaAtacante && muereA))) amp *= 1.0f - suave(ritmo::T_LP, ritmo::T_LP + 0.3f, tAtaque);
		if (amp <= 0.0f) return false;
		ojoA = glm::vec4(0.5f * (lo.x + hi.x), f.datos.altura, f.datos.bisagra * tam.x, -1.0f - f.datos.pendiente);
		ojoB = glm::vec4(amp * sinf(tiempo * 2.0f * glm::pi<float>() * hz + (float)z), 0.06f * tam.x, f.datos.abajo, f.datos.corte);
		return true;
	}

	// donde sale el ataque (boca, punta del baculo...) y donde llega
	glm::vec3 salidaFigura(int z)
	{
		const EstadoFigura& e = estFig[(size_t)z];
		Figura& f = figuras[(size_t)e.fig];
		float dis, holo;
		const glm::mat4 M = matrizFigura(z, dis, holo);
		if (f.esqueleto && f.datos.ataque == AtaqueFig::Magia) {
			// la punta del baculo (en el modelo, rigido en LeftHand) llevada por ese hueso
			for (vkglTF::Skin* piel : f.modelo.skins) {
				for (size_t k = 0; k < piel->joints.size(); k++) {
					if (piel->joints[k]->name == "LeftHand") return glm::vec3(M * piel->joints[k]->getMatrix() * piel->inverseBindMatrices[k] * glm::vec4(f.datos.salida, 1.0f));
				}
			}
		}
		return glm::vec3(M * glm::vec4(f.datos.salida, 1.0f));
	}
	glm::vec3 llegadaEn(int z) const
	{
		if (tieneFigura(z)) return baseZona(z) + glm::vec3(0.0f, 0.5f * figuras[(size_t)estFig[(size_t)z].fig].alto, 0.0f);
		return centroZona(z) + glm::vec3(0.0f, 0.35f, 0.0f);
	}

	// ---------------------------------------------------------------------------------------------- dibujo
	void dibujarUnaFigura(VkCommandBuffer cmd, Figura& f, const glm::mat4& M, glm::vec4 efecto, bool imagenes, glm::vec4 ojoA, glm::vec4 ojoB, bool& enEsqueleto)
	{
		const Empuje empuje{ M, efecto, glm::vec4(0.0f), ojoA, ojoB };
		if (f.esqueleto) {
			if (!enEsqueleto) {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagenes ? pipesFig.escena : pipesFig.sombra);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipesFig.layout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
				enEsqueleto = true;
			}
			vkCmdPushConstants(cmd, pipesFig.layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Empuje), &empuje);
			const VkDeviceSize desde = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &f.modelo.vertices.buffer, &desde);
			vkCmdBindIndexBuffer(cmd, f.modelo.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			for (vkglTF::Node* n : f.modelo.linearNodes) {
				if (!n->mesh) continue;
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipesFig.layout, 2, 1, &n->mesh->uniformBuffer.descriptorSet, 0, nullptr);
				for (vkglTF::Primitive* p : n->mesh->primitives) {
					if (imagenes) vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipesFig.layout, 1, 1, &p->material.descriptorSet, 0, nullptr);
					vkCmdDrawIndexed(cmd, p->indexCount, 1, p->firstIndex, 0, 0);
				}
			}
			// la espada en la mano
			if (f.mano && hayEspadaFig) {
				enEsqueleto = false;
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagenes ? pipelines.escena : pipelines.sombra);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
				dibujar(cmd, espadaFig, M * f.mano->getMatrix() * f.agarre, glm::vec4(efecto.x, 0.9f, efecto.z, 0.0f), glm::vec4(0.0f), imagenes);
			}
			return;
		}
		if (enEsqueleto) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagenes ? pipelines.escena : pipelines.sombra);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
			enEsqueleto = false;
		}
		dibujar(cmd, f.modelo, M, efecto, glm::vec4(0.0f), imagenes, ojoA, ojoB);
	}

	// las figuras de la mesa y las que se desvanecen; imagenes = false: pasada de sombra (el pipeline de sombra ya esta)
	void dibujarFiguras(VkCommandBuffer cmd, bool imagenes)
	{
		if (!figuras3D || numFiguras == 0) return;
		bool enEsqueleto = false, algo = false;
		for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
			if (!tieneFigura(z)) continue;
			EstadoFigura& e = estFig[(size_t)z];
			Figura& f = figuras[(size_t)e.fig];
			float dis, holo;
			const glm::mat4 M = matrizFigura(z, dis, holo);
			e.ultima = M;
			e.ultimoDis = dis;
			if (fabsf(dis) >= 0.999f || (!imagenes && fabsf(dis) > 0.5f)) continue;
			if (!algo) {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagenes ? pipelines.escena : pipelines.sombra);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
				algo = true;
			}
			glm::vec4 ojoA(0.0f), ojoB(0.0f);
			alasFigura(z, f, ojoA, ojoB);
			dibujarUnaFigura(cmd, f, M, glm::vec4(holo, 0.55f, dis, 0.0f), imagenes, ojoA, ojoB, enEsqueleto);
		}
		for (const Fantasma& g : fantasmas) {
			const float dis = suave(0.0f, 0.85f, g.t);
			if (dis >= 0.999f || (!imagenes && dis > 0.5f)) continue;
			if (!algo) {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagenes ? pipelines.escena : pipelines.sombra);
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
				algo = true;
			}
			dibujarUnaFigura(cmd, figuras[(size_t)g.fig], g.m, glm::vec4(0.3f + 1.5f * dis, 0.55f, glm::max(dis, 0.001f), 0.0f), imagenes, glm::vec4(0.0f), glm::vec4(0.0f), enEsqueleto);
		}
		if (algo && enEsqueleto) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagenes ? pipelines.escena : pipelines.sombra);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
		}
	}

	// el ataque a distancia (luz que suma): magia del baculo, aliento de la boca o rayos; y la luz al materializarse
	void efectosFiguras(VkCommandBuffer cmd)
	{
		if (!figuras3D || numFiguras == 0) return;
		bool atado = false;
		auto atar = [&] {
			if (atado) return;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentBuffer], 0, nullptr);
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.efectos);
			atado = true;
		};
		// columna de luz al materializarse
		// (rama classic-ps5-equipo-ovni: la abduccion de la carta de campo Bosque, en ataque y al voltearse de defensa boca
		// abajo, en lugar de la bola de luz y las chispas de antes; equipo_ovni_juego.h)
		for (int z = 0; z < (int)interfaz::NUM_ZONAS; z++) {
			if (!tieneFigura(z) || estFig[(size_t)z].tAparece > OVNI_DURA) continue;
			abduccionOvni(cmd, z, estFig[(size_t)z].tAparece);
			atado = false;
		}
		marcaObjetivoFig();
		if (!hayAtaque() || !tieneFigura(zonaAtacante)) return;
		const Figura& f = figuras[(size_t)estFig[(size_t)zonaAtacante].fig];
		const AtaqueFig at = f.datos.ataque;
		if (at == AtaqueFig::Embestida || at == AtaqueFig::Espada) return;
		// rama classic-ps5-ataque: antes aqui iban dos cilindros de luz de la boca al objetivo ("una barra", dijo el usuario
		// en la PS5); ahora el aliento / rayo / magia de Yugi Reload 3D con el pack de Niagara (ataque_fig_fx.h). El
		// directo va al punto de MD del que recibe (ataque_md_juego.h)
		atar();
		const glm::vec3 p0 = salidaFigura(zonaAtacante), p1 = ataqueDirecto ? destinoAtaqueMundo() : llegadaEn(zonaObjetivo);
		ataqueDistanciaFig(cmd, f, p0, p1);
		dibujarVfxFig(cmd);   // (fuego y humo de verdad: transparentes, al final)
	}

	// ---------------------------------------------------------------------------------------------- prueba en el PC
	void pruebaFiguras()
	{
#if defined(PS5_HOST_REFERENCE)
		if (!pruebaMesaFig.empty() && !pruebaMesaHecha && tiempo > 0.4f) {
			pruebaMesaHecha = true;
			congelarIAFig = true;
			if (ctl.vista.pantalla != control::VistaDuelo::DUELO) empezarDueloIA();
			if (ctl.vista.sorteo) {   // (rama classic-ps5-arreglos1: desde que el duelo empieza con el sorteo, la mesa de prueba lo salta)
				ctl.vista.sorteo = false;
				arrancarDueloIA(0);
			}
			for (int lado = 0; lado < 2; lado++) {
				for (ia::Monstruo& m : dueloIA.l[(size_t)lado].m) m = ia::Monstruo{};
			}
			for (const auto& p : pruebaMesaFig) {
				const bool rival = p.first.rfind("rival_", 0) == 0;
				const int i = p.first.back() - '0';
				if (i < 0 || i >= ia::ZONAS) continue;
				std::string carta = p.second;
				bool def = false;
				if (carta.size() > 4 && carta.compare(carta.size() - 4, 4, ":def") == 0) {
					def = true;
					carta.resize(carta.size() - 4);
				}
				ia::Monstruo& m = dueloIA.l[rival ? 1 : 0].m[(size_t)i];
				m.id = carta;
				m.defensa = def;
				m.visto = true;
			}
			sincronizarMesaDuelo();
			fprintf(stderr, "[figuras] mesa de prueba: %zu cartas\n", pruebaMesaFig.size());
		}
		// rama classic-ps5-arreglos1 (capturas): YUGI_CORTE=<carta>[:<segundo>] muestra el cartel de la carta del rival
		static bool corteHecho = false;
		if (const char* c = getenv("YUGI_CORTE")) {
			const std::string v = c;
			const size_t p = v.find(':');
			if (!corteHecho && tiempo >= (p == std::string::npos ? 3.0f : (float)atof(v.c_str() + p + 1))) {
				corteHecho = true;
				ctl.corteCarta = v.substr(0, p);
				ctl.tCorte = 0.0f;
			}
		}
		if (medirFiguras && hayAtaque() && (int)(tiempo * 30.0f) % 10 == 0) {
			fprintf(stderr, "[figuras] t %.2f: ataque en %.2f (ritmo %.2f)\n", tiempo, tAtaque, velAtaqueFig);
		}
		// rama classic-ps5-ataque5: "t:atacante>objetivo!" declara el ataque y lo NIEGA a los 2.2 s (la ventana de respuesta abierta:
		// el atacante se ve elevado; Waboku / Magic Cylinder: S_ATAQUE_NEGADO) y baja sin atacar
		static float negarEn = -1.0f;
		if (negarEn >= 0.0f && tiempo >= negarEn && !hayAtaque()) {
			negarEn = -1.0f;
			ctl.aviso = "Ataque negado";
			ctl.tAviso = 0.0f;
			bajarAtacante();
		}
		for (AtaquePruebaFig& a : pruebaAtaquesFig) {
			if (!a.hecho && tiempo >= a.t && !hayAtaque() && !a.o.empty() && a.o.back() == '!' && negarEn < 0.0f && elevacionLibre()) {
				a.hecho = true;
				elevarAtacante(control::zonaPorId(a.a));
				negarEn = tiempo + 2.2f;
				fprintf(stderr, "[figuras] declarado %s y negado (t %.2f)\n", a.a.c_str(), tiempo);
				continue;
			}
			if (!a.hecho && tiempo >= a.t && !hayAtaque() && !(negarEn >= 0.0f) && elevacionLibre()) {
				a.hecho = true;
				// (rama classic-ps5-ataque: "...>rival" / "...>jugador" es un ataque directo: ataque_fig_fx.h)
				const bool ok = a.o == "rival" || a.o == "jugador" ? atacarDirectoPrueba(control::zonaPorId(a.a)) : atacar(control::zonaPorId(a.a), control::zonaPorId(a.o), false);
				fprintf(stderr, "[figuras] ataque de prueba %s -> %s (t %.2f): %s\n", a.a.c_str(), a.o.c_str(), tiempo, ok ? "ok" : "no");
			}
		}
#endif
	}
