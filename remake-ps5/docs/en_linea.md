# Duelo en línea: buscar rival en el mundo (rama classic-sala)

El jugador elige **DUELO EN LÍNEA → Buscar rival** y el juego encuentra a otro jugador del mundo que esté buscando, sin
mandar códigos. El duelo va directo entre las dos máquinas (P2P, ramas classic-red-base y classic-red-nat); la búsqueda
la hace `src/red/sala.h`:

1. **Sala en Cloudflare** (`servidor/sala`, Worker gratis, HTTP sin TLS, long-polling): empareja por versión del juego
   y catálogo (obligatorio) y por ELO (ventana que se ensancha con la espera; con pocos jugadores, el primero que
   aparezca). PS5 y PC juegan entre sí.
2. **DHT de BitTorrent** (`src/red/dht.h`), de respaldo y sin ningún servidor: si la sala no responde en 5 s, el juego
   se anuncia en la DHT pública bajo la clave SHA-1("yugioh-classic-remaster/1") y habla directo con los pares que
   encuentra.

ELO local en `/data/yugioh_classic/en_linea.txt` (`src/red/elo.h`, guardado atómico) con un id anónimo aleatorio; si la
sala responde, su ELO manda. La URL de la sala: `URL_SALA` en `sala.h`, o `sala=http://...` en
`/data/yugioh_classic/red.txt` (PC: `./red.txt` o `YUGI_SALA`).

## Uso desde el juego

```cpp
red::Sala sala;                                        // un solo objeto mientras dure el modo en linea
sala.consultarEnLinea();                               // al entrar al menu: el contador
std::vector<red::Candidato> mios = /* nat.h: reunirCandidatos() */;
sala.buscar(sala.perfilLocal(nombreDuelista, VERSION_JUEGO, "ps5", hashCatalogo), mios);
// cada cuadro (no bloquea nunca):
sala.actualizar();
switch (sala.estado()) {
case red::Sala::Buscando:    /* "Buscando rival..." + sala.enLinea() / sala.buscandoAhora() */ break;
case red::Sala::Emparejado:  { red::Pareja p; sala.pareja(p); /* aviso + nat.h perforar(p.candidatos, p.idSesion, p.soyAnfitrion) */ } break;
case red::Sala::SinConexion: /* "Sin conexion a internet" */ break;
case red::Sala::Error:       /* "No se pudo abrir la red" */ break;
}
// al terminar el duelo en linea:
sala.reportarResultado(p.idSesion, gane);
```

## Pantallas sugeridas

Los textos ES/EN ya están en `idioma.h` (bloque «rama classic-sala»).

### 1. Menú DUELO EN LÍNEA

- Opciones: **Buscar rival**, **Jugadores en línea**, **Volver**.
- Pie: «Jugadores en línea: N · Buscando ahora: M» (de `enLinea()`/`buscandoAhora()`; si es -1, «Consultando…»).
- Tu ELO y tus partidas en línea (`datosElo()`).

### 2. Aviso de IP visible (la primera vez y desde Opciones)

Antes de la primera búsqueda, un cartel que hay que aceptar (se guarda en `en_linea.txt`, por ejemplo `aviso_ip=1`):

> «El duelo en línea conecta tu consola directamente con la del rival. Tu dirección IP será visible para tu rival y,
> si la sala no responde, también en la red pública de BitTorrent (DHT) que se usa para encontrar jugadores.»
> **Aceptar** / **Volver**

### 3. Buscando rival

- «Buscando rival…» con un indicador animado y el tiempo de espera.
- «Jugadores en línea: N · Buscando ahora: M».
- Si `usandoDht()`: una línea chica «La sala no responde: buscando por la red P2P».
- Botones: **Jugar contra la IA mientras esperas**, **Cancelar** (`cancelar()`).
- `SinConexion`: «Sin conexión a internet» con **Reintentar** / **Volver**.

### 4. Jugar contra la IA mientras espera

- La búsqueda sigue en segundo plano (`actualizar()` cada cuadro también durante el duelo contra la IA).
- Al emparejar: un cartel arriba **«¡Rival encontrado! <nombre> (ELO <n>, PS5/PC)»** con **Aceptar** (abandona el duelo
  contra la IA sin penalización y va al duelo en línea) o **Rechazar** (cancela y sigue contra la IA). Si no contesta
  en 15 s, se rechaza solo (el otro vuelve a buscar).

### 5. Rival encontrado

- Retrato genérico + «<nombre> · ELO <n> · PS5/PC», «Conectando…» mientras `nat.h` perfora; si no conecta en ~15 s:
  «No se pudo conectar con el rival» y se vuelve a buscar.

### 6. Resultado

- En la pantalla de resultado del duelo en línea: «ELO: 1000 → 1016 (+16)»; si la sala todavía no confirmó, «ELO
  pendiente de confirmar» (llega con `eloConfirmado()`).

## Privacidad

- El id es aleatorio (32 hex), generado en la consola; el nombre es el de duelista. La sala no guarda IPs (solo en
  memoria, 2 minutos, para el límite de pedidos); guarda el ELO por id y lo borra a los 180 días sin jugar.
- En la DHT se publica IP:puerto (inherente a P2P): por eso el aviso de la pantalla 2.

## En el juego (rama classic-en-linea)

Archivos: `src/en_linea.h` (lo que se ve y las constantes), `src/en_linea_juego.h` (la lógica, dentro de la clase del
juego), `src/en_linea_ui.h` (el dibujo), `src/red/espejo.h` (la vista del invitado). Pruebas: `prueba_red_front.cpp`
(casos `CU-RED-FE-*`) y `prueba_red.cpp` (`CU-RED-RELOJ-AGOTADO`, `CU-RED-REVANCHA`, `CU-RED-ESPEJO*`).

- **Menú principal → DUELO EN LÍNEA** (sexto botón, debajo de los 5 de la puerta): Buscar rival, Red local, Código de
  conexión, Mazo (◄ ►: uno de la biblioteca; tiene que ser válido y legal: 40-60, hasta 3 copias) y Volver. Abajo:
  jugadores en línea / buscando / duelos, tu ELO y tus partidas, tu nombre.
- **Aviso de privacidad** la primera vez («En los duelos P2P el rival puede ver tu dirección IP», Aceptar/Cancelar);
  se recuerda en `opciones.txt` (`aviso_ip=1`) y se vuelve a ver desde Opciones > «Aviso de privacidad (en línea)».
- **Buscar rival**: un hilo reúne los candidatos (STUN, UPnP/NAT-PMP, LAN, IPv6; `red_stun.txt`) del socket del juego
  (puerto 47924; si está ocupado, otro) y la sala busca (sin URL o sin respuesta, la DHT). «Jugar contra la IA mientras
  espero» lleva a elegir rival y la búsqueda sigue: al aparecer el rival, cartel «¡Rival encontrado!» en cualquier
  pantalla (X ir / círculo rechazar, 15 s; sin respuesta se rechaza y se sigue buscando). El duelo contra la IA se
  abandona sin contar. Círculo en elegir rival / resultado vuelve a la búsqueda.
- **Conectar**: otro hilo perfora (`perforar`, 15 s); el anfitrión escucha y el invitado llama. Si no se logra:
  pantalla con el aviso de la red (CGNAT, NAT simétrico) y cómo abrir el puerto UDP 47924 a mano.
- **Red local**: crear (anuncio por broadcast/multicast) o unirse a uno de la lista (los de otra versión, en gris).
- **Código de conexión**: el propio (y el juego escucha como anfitrión) y «Escribir el código del rival» con el teclado
  en pantalla (cruceta, X escribe, cuadrado borra, triángulo acepta, círculo cancela). El que escribe el código llama:
  perfora con la sesión del código (el otro contesta los sondeos desde su socket); una dirección «IP:puerto» escrita a
  mano va directo. Con NAT cerrado del lado del que muestra el código hace falta el puerto abierto (UPnP o a mano).
- **Duelo**: el rival remoto reemplaza a la IA (`DueloRed`); el nombre del rival arriba a la derecha con el indicador de
  conexión (verde / rojo que late) y los dos relojes. La moneda de siempre, con el resultado de la semilla común; el
  que gana elige (30 s; si no, va primero). Pausa: Continuar, Opciones, Abandonar (= rendirse, cuenta como derrota).
- **Reloj del turno** (`en_linea.h`: `RELOJ_TURNO_S` 180, `RELOJ_ALERTA_S` 30, `AGOTAMIENTOS_DERROTA` 3): se rellena
  al empezar cada turno; corre solo para el que decide (con la mesa al día; también con la pausa abierta; no corre con
  el rival caído). Con 30 s: sonido, aviso y borde rojo que late. A 0: la máquina de ESE jugador manda AGOTADO y juega
  sola (pasa de fase, no activa, elige la primera opción en las elecciones obligatorias, descarta la primera con la
  mano llena) hasta que deje de decidir en ese turno; el otro solo lo recibe. Al tercero, derrota (los dos).
- **Desconexión**: cartel «Esperando al rival… (reconectando N / 30 s)»; si vuelve, sigue igual. A los 30 s: «Duelo
  interrumpido: no cuenta» (ni estadísticas ni ELO ni reporte a la sala). Desincronización, rechazo o trampa: igual.
- **Resultado**: la pantalla de siempre con «Rival: X · En línea», «ELO: a -> b (+d)» cuando la sala confirma (si no,
  «ELO pendiente de confirmar»; por red local, código o DHT, sin ELO), Revancha (los dos la piden: otra moneda en el
  mismo canal), «Buscar otro rival» y Menú principal.
- **Estadísticas**: los duelos en línea van aparte (`estadisticas.txt`: `linea_duelos`, `linea_victorias`...); no
  tocan los de la IA, ni la racha, ni el progreso de niveles.
