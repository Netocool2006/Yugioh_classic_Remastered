	// rama classic-ps5-ataque5 (se incluye DENTRO de la clase del juego, antes de duelo_juego.h; sin glm: tambien lo usan
	// prueba_juego y prueba_certificacion). Pedido del usuario: "la carta que ataque, antes de atacar, debe elevarse algunos
	// centimetros en el tablero, con el objetivo de si activo o no una carta de contraefecto a ese ataque: si hay mas de 2
	// monstruos en posicion de ataque no se cual va a atacar. La que ataque primero debe elevarse; cuando termine el ataque
	// baja de nuevo, y luego la segunda que ataque se eleva, y asi sucesivamente".
	// Master Duel: CardRoot.StartAttackReady (RVA 0x4b1f40) para el TweenPositionTo "tweenAttackReadyStart" de la carta
	// (CardPlane +0x128) y deja el TweenPosition "Idle" (+0x130) flotando; FinishAttackReady (0x4af4c0) corre el
	// "Finish" (+0x138) que la baja. Esta elevacion hace lo mismo: sube al DECLARAR el ataque (S_ATAQUE, cuando se abre la
	// ventana de respuesta: Mirror Force, Waboku, Magic Cylinder...), flota todo el ataque y baja en el ultimo 0.2 s (el
	// ritmo de ~3 s del ataque no cambia: la subida es la fase de apuntar). Nunca dos elevadas a la vez; si el ataque se
	// niega, se cancela o el atacante desaparece, baja o se quita sin quedarse arriba.
	static constexpr float ELEV_ENTRA = 0.20f, ELEV_SALE = 0.20f;   // s (los de MD no estan en data.unity3d accesibles: ver docs)

	int elevZona{ -1 };       // zona (de ctl.zonas) de la carta elevada; -1: ninguna
	float elevK{ 0.0f };      // 0..1: cuanto esta elevada
	bool elevSube{ false };   // true: declarada (sube o se queda); false: baja

	// el atacante declarado se eleva (si ya habia otra elevada, esa vuelve de golpe a la mesa: nunca dos a la vez)
	void elevarAtacante(int z)
	{
		if (z < 0) return;
		if (elevZona != z) {
			elevZona = z;
			elevK = 0.0f;
		}
		elevSube = true;
	}
	// el ataque termino, se nego o se cancelo: baja
	void bajarAtacante() { elevSube = false; }
	bool elevacionLibre() const { return elevZona < 0; }
	// cada cuadro (dt ya con la velocidad de animaciones)
	void avanzarElevacion(float dt)
	{
		if (elevZona < 0) return;
		if (elevSube) elevK = elevK + dt / ELEV_ENTRA > 1.0f ? 1.0f : elevK + dt / ELEV_ENTRA;
		else elevK = elevK - dt / ELEV_SALE < 0.0f ? 0.0f : elevK - dt / ELEV_SALE;
		if (!elevSube && elevK <= 0.0f) elevZona = -1;
		if (elevZona >= 0 && ctl.zonas[(size_t)elevZona].vacia()) {   // el atacante ya no esta (destruido): nada se queda arriba
			elevZona = -1;
			elevK = 0.0f;
		}
	}
	// la elevacion que muestra la interfaz (suavizada) de la zona z
	float elevacionDe(int z) const
	{
		if (z != elevZona) return 0.0f;
		return elevK * elevK * (3.0f - 2.0f * elevK);
	}
	// el declarado ya no va a atacar (la ventana de respuesta lo quito, se nego sin aviso o se redirigio): baja. Se mira cada
	// cuadro: sin ataque animado, con todo presentado y sin ataque declarado en el motor, nadie se queda arriba
	void vigilarElevacion()
	{
		if (elevZona < 0 || !elevSube || hayAtaque()) return;
		if (sucesosVistos >= dueloIA.sucesos.size() && !dueloIA.ef.ataque.hay && esperaEfecto <= 0.0f) bajarAtacante();
	}
