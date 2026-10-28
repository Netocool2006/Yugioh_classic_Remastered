	// Yugioh Classic Remaster (rama classic-resultado): se incluye DENTRO de la clase del juego, despues de duelo_juego.h.
	// La velocidad de las animaciones (velocidad.h) aplicada al duelo:
	//  - avanzarDueloIA corre con dt * escala (esperaEfecto de la cadena, lo que piensa el rival, el sorteo...).
	//  - el ataque plano (tAtaque), la mesa de control.h (robo, vuelo, volteos, avisos) mientras el jugador espera,
	//    las figuras 3D y las activaciones de efectos_cartas.h usan escalaAnim() / dtAnim().
	//  - R2 (velocidad::Control): mantener acelera y un toque salta, solo mientras hay una animacion o juega el rival;
	//    en ese caso R2 no hace zoom (se le quita a la camara en este cuadro).
	velocidad::Control velAnim;
	bool animandoDuelo{ false };   // del cuadro anterior: el duelo no esta al dia (animacion o espera en curso)

	// (solo mientras algo se anima o juega el rival: las figuras en reposo no se aceleran)
	float escalaAnim() const
	{
		if (ctl.vista.pantalla != VD::DUELO) return 1.0f;
		return animandoDuelo || hayAtaque() || ctl.vista.turnoRival ? velAnim.escala : 1.0f;
	}
	float dtAnim(float dt) const { return dt * escalaAnim(); }
	// la mesa (control.h): solo mientras el jugador no tiene el control o una carta vuela / llega a la mano
	float escalaMesa() const
	{
		if (ctl.vista.pantalla != VD::DUELO) return 1.0f;
		return ctl.bloqueado || ctl.vuelo.activo || ctl.robo.activo ? velAnim.escala : 1.0f;
	}

	// al principio de avanzarDueloIA: el dt del duelo
	float velocidadCuadro(float dt)
	{
		if (ctl.vista.pantalla != VD::DUELO) {
			velAnim.escala = 1.0f;
			velAnim.consume = false;
			return dt;
		}
		const bool rival = !ctl.vista.sorteo && dueloIA.ganador < 0 && dueloIA.decide() == 1;
		const float e = velAnim.cuadro(mando.r2, animandoDuelo || ctl.vista.sorteo, rival, dt);
		if (velAnim.consume) mando.r2 = 0.0f;   // (la camara no hace zoom con el R2 de la velocidad)
		return dt * e;
	}

	// la ruta de opciones.txt (junto a progreso.txt)
	std::string rutaOpcionesVelocidad() const
	{
		return rutaProgreso.substr(0, rutaProgreso.size() - std::string("progreso.txt").size()) + "opciones.txt";
	}
	// al empezar cada duelo: la velocidad elegida en Opciones (relee opciones.txt)
	void leerVelocidad()
	{
		velocidad::leer(rutaOpcionesVelocidad());
#if defined(PS5_HOST_REFERENCE)
		if (const char* e = getenv("YUGI_VELOCIDAD")) velocidad::poner(atoi(e));   // (PC: pruebas)
#endif
		velAnim.reiniciar();
		fprintf(stderr, "[velocidad] animaciones: %s (x%.2f)\n", velocidad::nombre(velocidad::nivel()), velocidad::factor());
	}
