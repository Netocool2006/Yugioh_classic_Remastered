# Modo en línea: la base de red (rama classic-red-base)

Duelo P2P entre PS5 con jailbreak y PC (crossplay), sin servidor propio para el duelo. Todo está en `src/red/`
(solo headers, sin hilos: el juego llama `actualizar()` cada cuadro). Lo usan las ramas classic-red-nat (STUN, UPnP,
perforación) y classic-sala (emparejamiento): ellas consiguen la dirección del otro; esta rama juega el duelo.

## Headers

| Header | Qué hace |
|---|---|
| `socket_udp.h` | `red::Direccion` (v4/v6, `texto()`, `desdeTexto()`), `red::SocketUdp` (abrir dual-stack, enviar, recibir con espera, `puertoLocal()`), `direccionesLocales()`, `ahoraMs()`. PS5 (sockets BSD del SDK de payloads), Linux y Windows (Winsock). Extras: `permitirBroadcast()`, `unirMulticastV6()`, `abrirCompartido()`. |
| `sha256.h` | SHA-256 propio (`Sha256`, `sha256()`, `hex()`). |
| `canal.h` | `red::Canal`: mensajes confiables y ordenados sobre un `Transporte` (UDP o simulado). |
| `compromiso.h` | Semilla común, barajado con semilla privada, compromiso por carta con sal, verificación. |
| `duelo_red.h` | `red::DueloRed` (el duelo en lockstep), `ManoIA`, `hashCatalogo()`, `hashEstado()`, `iniciarDueloRed()`. |
| `lan.h` | `AnuncioLan` / `BuscadorLan`: «hay un duelo esperando» en la red local. |

## Canal (`canal.h`)

- Paquete: `"YGCR"`, versión de protocolo, tipo, marcas, sesión (64 bits), seq, ack acumulado y 32 bits de acuses
  selectivos. Máx. 1200 bytes; los mensajes largos van en pedazos.
- Conexión: `HOLA` (nonce, versión del juego, hash del catálogo, nombre) → `HOLA_OK` (sesión al azar, secreta) o
  `RECHAZO` (protocolo, versión, catálogo distintos, u ocupado). Si no coinciden, no se juega.
- Reenvío con espera creciente (150 ms → 1 s), duplicados descartados, adelantados guardados hasta completar el orden.
- Latido cada 1 s. 10 s sin nada: `C_CAIDO` (se sigue intentando); vuelve antes de 30 s: `C_CONECTADO` (aunque llegue
  desde otra dirección o puerto: la sesión hace de clave); 30 s: `C_PERDIDO`. `ADIOS` al cerrar.
- Paquetes de otra magia, protocolo o sesión: se ignoran (`cuentas.ajenos`).
- `VERSION_JUEGO` (`"classic-1.0"`): **hay que subirla cuando cambie el motor (`ia.h`)**, aunque no cambien las cartas.

## Duelo en lockstep (`duelo_red.h`)

- Anfitrión = lado 0, invitado = lado 1, en las dos máquinas (el motor no es simétrico entre lados: ver abajo).
- Viajan solo las decisiones: cada `ia::Jugada` del lado que decide (`Duelo::decide()`), aceptada o rechazada por el
  motor, con el hash del estado después de aplicarla. El otro la aplica, compara el resultado y el hash; si difieren,
  **desincronización**: el duelo termina sin ganador en las dos máquinas y cada una guarda un registro
  (`ConfigDuelo::carpetaRegistro`: semillas, órdenes, estado y todas las jugadas).
- Preparación: `INICIO` (compromisos de la mitad de la semilla común y de la semilla privada, «preguntar antes de
  activar», nombre) → `MITAD` → semilla común = SHA-256(mitades) → la moneda es un bit de la semilla común; el que
  gana elige ir primero o segundo (`debeElegirOrden()` / `elegirOrden()`) → `MAZO` → empieza.
- Fin: `FIN_NORMAL` (ganador del motor), `FIN_ABANDONO` (el otro se cortó 30 s o se fue: SIN ganador, no cuenta;
  rama classic-en-linea), `FIN_RENDICION`, `FIN_DESINCRONIZADO` (sin ganador), `FIN_RECHAZADO` (versión/catálogo),
  `FIN_TRAMPA`, `FIN_TIEMPO` (3 relojes agotados).
- Rama classic-en-linea: `AGOTADO` (lo manda la máquina del que agotó su reloj, antes de su jugada automática; los dos
  cuentan; `agotarTiempo()`) y `REVANCHA` (`pedirRevancha()`; con las dos, todo de nuevo en el mismo canal).
  `VERSION_JUEGO` = `classic-1.1`.
- Uso en el juego:

```cpp
red::ConfigDuelo cfg;            // mazo propio, nombre, preguntarSiempre, carpetaRegistro
auto s = red::iniciarDueloRed(&baseIA, cfg, /*anfitrion*/ true);          // o false + la dirección del anfitrión
// cada cuadro:
s->actualizar();
if (s->duelo.debeElegirOrden()) s->duelo.elegirOrden(irPrimero, red::ahoraMs());
if (s->duelo.meToca()) s->duelo.jugar(jugadaDelMando, red::ahoraMs());    // en vez de dueloIA.aplicar(...)
// s->duelo.duelo() es el ia::Duelo (sucesos para la mesa, como siempre)
```

## Determinismo (lo que se revisó del motor)

- Azar: `ia::Azar` (xorshift propio) sembrado con la semilla común; nada de `rand()`, tiempo ni `<random>` en el motor.
  El barajado de los mazos lo hace cada dueño (`compromiso.h`, SHA-256 en modo contador) y `Duelo::empezar(...,
  barajarMazos = false)` ya no baraja.
- **Arreglado**: Pineapple Blast ordenaba con `std::sort` por un valor `float` con empates: el orden de los empates
  depende de la biblioteca estándar (libc++ en la PS5, MSVC/libstdc++ en el PC). Ahora desempata por uid.
- **Opción local que toca el motor**: «Preguntar antes de activar = Siempre» (`Duelo::preguntarSiempre`) crea o no
  una pregunta del motor. Viaja en el `INICIO` y `DueloRed` lo vuelve a poner para los dos lados antes de cada
  jugada (aunque la interfaz lo cambie, como hace `duelo_juego.h` cada cuadro para el lado 0).
- Lo local que solo toca la interfaz o la IA (dificultad, velocidad, IA humana, tiempos de pensar, `float` de las
  valoraciones de la IA) no viaja: solo viajan las decisiones.
- `unordered_map`: el motor no recorre ninguno (solo `ia_plan.h`, que es de la IA). El hash del catálogo ordena las claves.
- La misma «huella» de 100 duelos con g++/libstdc++ y con clang `-march=znver2 -ffp-contract=fast` (el CPU de la PS5).
- El motor tiene asimetrías por número de lado (p. ej. `if (e.lado == 0) revelar(...)` en Inspection, que además gasta
  azar solo en ese caso): son deterministas porque las dos máquinas usan los mismos números de lado. Por eso el
  invitado no puede ser «lado 0» en su máquina: su pantalla usa la vista en espejo (`red/espejo.h`, rama
  classic-en-linea): una copia del duelo con los lados cambiados para mirar, y `espejoJugada()` al jugar.
  `ia::Duelo::ladoVista` (no entra en el hash) dice a quién se le muestran las cartas que mira un efecto.

## Barajado con compromiso (`compromiso.h`) — alcance real (versión 1)

1. Cada jugador compromete su semilla privada (hash) **antes** de conocer la semilla común.
2. Baraja su mazo con SHA-256(privada, común): nadie elige el orden ni puede probar semillas hasta dar con uno bueno.
3. Manda el compromiso: SHA-256(sal_i, carta_i) por posición (sal_i sale de la privada) y el hash de todo.
4. Cada carta que sale de arriba del mazo (robos) se revela con su sal y el otro la verifica en el momento, mientras el
   mazo siga siendo el comprometido sin las de arriba (una búsqueda, un barajado o una carta devuelta al mazo cortan
   las revelaciones de ese mazo; queda la verificación final).
5. Al terminar (o al rendirse) cada uno revela su semilla privada y el otro verifica todo: compromiso, orden y mazo
   jugado. Mazo arreglado o carta falsa: `trampaDelOtro()` (+ el motivo).

**Lo que NO hace la versión 1**: esconder información. El motor necesita las cartas del rival (robos, efectos que miran
la mano o buscan en el mazo), así que el orden también viaja al empezar: una máquina modificada puede ver la mano y
el mazo del rival. Lo que sí garantiza: el orden es justo y lo jugado es lo comprometido. Ver
`docs/decisiones_pendientes.md`.

## Red local (`lan.h`)

`AnuncioLan::empezar(identidad, puertoDelDuelo, id)` + `actualizar()` cada cuadro: anuncio cada 1 s por broadcast
IPv4 (255.255.255.255 y el broadcast /24 de cada interfaz) y multicast IPv6 (ff02::1), puerto 47321.
`BuscadorLan::lista()`: nombre, dirección del anfitrión, compatible (misma versión y catálogo). Un anuncio que no
se repite en 4 s sale; `detener()` manda el retiro.

## Pruebas

`src/pruebas/prueba_red.cpp` (casos `CU-RED-*`, en `ciclo_completo.sh`): red simulada con pérdida, duplicado y
desorden y tiempo virtual (caídas de 30 s en milisegundos), 200 duelos completos IA contra IA en lockstep, UDP de
verdad en loopback (IPv4 e IPv6) y la red local.
