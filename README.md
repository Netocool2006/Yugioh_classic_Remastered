# Yugioh Classic Remaster

[English](README.en.md) | **Español**

Duelo de cartas de Yu-Gi-Oh! con las reglas de la era clásica (formato Goat, abril de 2005), con monstruos emblemáticos
como figuras 3D sobre un tablero 3D, un rival con inteligencia artificial "humana" que aprende, presentación al estilo
de *Yu-Gi-Oh! Master Duel* y un modo historia con los personajes de la serie original. Versiones para **PS5**
(homebrew, título `PPSA77002`, carpeta `remake-ps5/`) y para **PC Windows** (`remake-ps5/pc/`, con instalador).

> Proyecto hermano separado: **Yugi Reload** (repositorio `Ps5-Yugioh-Reload`). No forma parte de este repositorio.

> Aviso: proyecto personal, sin fines de lucro, de uso personal. Contiene referencias y datos de Konami y material
> traducido de *Master Duel*; el dueño volverá este repositorio privado. **No incluye imágenes, audios ni modelos 3D**
> (los assets viven fuera del repo). Ver [Aviso legal](#11-aviso-legal).

---

## Índice

1. [Qué es, estado final y números](#1-qué-es-estado-final-y-números)
2. [Estructura del repositorio y tabla de módulos](#2-estructura-del-repositorio-y-tabla-de-módulos)
3. [Arquitectura del duelo y reglas de ritmo](#3-arquitectura-del-duelo-y-reglas-de-ritmo)
4. [Datos: `remake-ps5/datos` y `classic-assets`](#4-datos-remake-ps5datos-y-classic-assets)
5. [Cadena de construcción (compilar, empaquetar, subir)](#5-cadena-de-construcción-compilar-empaquetar-subir)
6. [Guías "cómo hago X"](#6-guías-cómo-hago-x) (la IA del rival, en detalle: [`docs/IA.md`](docs/IA.md))
7. [Pruebas](#7-pruebas)
8. [Versión PC (Windows)](#8-versión-pc-windows)
9. [Bitácora del duelo y herramientas de diagnóstico](#9-bitácora-del-duelo-y-herramientas-de-diagnóstico)
10. [Decisiones de diseño, lecciones y pendientes](#10-decisiones-de-diseño-lecciones-y-pendientes)
11. [Aviso legal](#11-aviso-legal)

Notas sobre este documento: todo lo que sigue sale de leer el código y los documentos de `remake-ps5/docs/` y de los
datos del repo. Donde algo **no se pudo verificar** (porque depende de la consola, de assets externos o de no haber
compilado en esta pasada) se dice explícitamente. Los documentos de detalle están indexados en
[`docs/LEEME.md`](docs/LEEME.md); la inteligencia artificial del rival tiene su propio capítulo en [`docs/IA.md`](docs/IA.md). Las rutas del contenedor (`/compartido`, `/work`) son las del entorno de desarrollo;
`<carpeta del proyecto>` es la raíz del repo.

---

## 1. Qué es, estado final y números

### 1.1 Qué es

- Un duelo de Yu-Gi-Oh! **completo, con motor de reglas propio** (C++17, solo cabeceras), con las reglas de la época:
  8000 LP, mano de 5, una invocación normal por turno, tributos, fusión, ritual, Toon, cadena de respuestas, trampas,
  ventanas de respuesta, y la lista de prohibidas/limitadas/semilimitadas Goat.
- Un **rival con IA**: 8 niveles de dificultad, personalidades, planes de juego, mazos dinámicos, búsqueda con Monte
  Carlo, "capa humana" (errores creíbles, faroles, tiempo de pensar) y una **red neuronal de valor** que aprende por
  autojuego y se ajusta al jugador entre duelos.
- **Presentación al estilo Master Duel** (ritmo, cartel de turno, selector de fases, engranaje, activaciones, cadena,
  combate, efectos de cartas traducidos), sobre un **tablero 3D** con fondos fotorrealistas por personaje, cartas de
  campo que cambian el escenario y **figuras 3D** de monstruos emblemáticos que atacan, se defienden y mueren.
- **Modo historia** (47 personajes de la serie original en 6 sagas), **duelo libre** (40 rivales genéricos),
  **constructor de mazos** al estilo Master Duel, **21 mazos de torneo** curados, personalización (fundas, tapetes),
  galería de figuras 3D, tutorial, estadísticas, español/inglés en todo.
- **Duelo en línea** (solo PS5) por P2P con lockstep, emparejamiento por sala en la nube (Cloudflare Workers) y respaldo
  por DHT. **La versión de PC no incluye juego en línea** (el botón dice "no disponible en PC").

### 1.2 Estado final

Terminado y jugable en PS5 y en PC. Las pruebas automáticas documentadas cierran en **0 fallas** (ver sección 7); la
validación en la consola real la hace el dueño a mano (las pruebas del PC no la reemplazan). Pendientes y límites
conocidos: sección 10.

### 1.3 Números reales (contados de los datos del repo)

| Qué | Cantidad | De dónde sale |
|---|---|---|
| Cartas jugables | **1784** = 1690 Goat + 81 "cartas firma" de personajes + 13 "cartas extra" | `datos/cartas_firma.txt`, `datos/cartas_extra.txt`; "de 1784" en `docs/certificacion_efectos.md` |
| Cartas con efecto certificado | **1395** (OK 1395, FALLA 0, SIN PRUEBA 0) | `docs/certificacion_efectos.md` |
| Figuras 3D de monstruos | **92** | `datos/figuras.txt` |
| Cartas emblemáticas | **83** marcadas "emblematica" (el archivo trae 157 líneas con las firma) | `datos/emblematicas.txt` |
| Personajes del modo historia | **47** en **6 sagas** (Reino de los Duelistas 16, Ciudad Batallas 8, Noah 5, Despertar de los Dragones 6, Gran Campeonato 2, Memorias del Faraón 10) | `datos/personajes.txt` |
| Mazos de personajes | **141** variantes | `datos/personajes_rivales.txt` (líneas `V`) |
| Rivales genéricos | **40** en 8 niveles (5 por nivel) x 3 variantes = **120 mazos** | `datos/rivales.txt` (líneas `R`) |
| Mazos de torneo | **21** (5 inspirados en Master Duel, 16 de la era Goat) | `datos/mazos_torneo.txt` (líneas `D`) |
| Fondos fotorrealistas | **88** entradas: 47 personajes + 16 temas genéricos + 25 cartas de campo | `datos/fondos.txt` |
| Cartas de campo que cambian el escenario | **25** | `docs/campos.md` |
| Diálogos del modo historia | ~300 líneas ES/EN | `datos/dialogos.txt` |
| Código | ~71 000 líneas en `remake-ps5/src` (unas 28 400 son el motor `ia.h`) | `wc -l` |

Dos incoherencias entre documentos viejos y los datos: `docs/personajes.md` dice "78 cartas firma" y
`docs/mazos_torneo.md` "10 cartas extra"; los datos actuales dicen 81 y 13 (se agregaron después). Manda el dato.

**No verificado**: el número de archivos de `classic-assets` (arte, audio, modelos), el peso exacto del título (los docs
dicen ~3,6-4,1 GB de datos y una imagen de ~4,5 GB) y la cantidad de efectos Niagara horneados, porque esa carpeta está
fuera del repo y no se leyó en esta pasada.

---

## 2. Estructura del repositorio y tabla de módulos

```
<carpeta del proyecto>/
  README.md, README.en.md     este documento (ES / EN)
  docs/LEEME.md               índice de la documentación
  remake-ps5/                 TODO el código del Classic (PS5 + PC)
    src/                      código del juego (cabeceras .h + yugireload.cpp)
      pruebas/                pruebas automáticas, ciclo completo, certificación
      red/                    modo en línea (UDP, NAT, DHT, sala, lockstep)
    shaders/                  GLSL (Vulkan)
    datos/                    datos del juego (texto, versionados)
    docs/                     documentación detallada (indexada en docs/LEEME.md)
    pc/                       capa de Windows, CMake, instalador Inno Setup
    servidor/sala/            Worker de Cloudflare para el emparejamiento
    escenarios/               escenas de Blender (fondos pre-renderizados, históricas)
    third_party/              stb_*, fuentes OFL, licencias
    yugictl/                  agente residente de la consola (solo para PPSA77002)
    *.py / *.sh               generadores de datos, colas de Gemini, construcción, subida
  herramientas/               herramientas auxiliares (Gemini, Blender, esqueletos, Unreal, Master Duel, audio)
  remake/diseno/              diseño compartido (tablero.json, tamanos.json, emblematicas.json, componer_cartas.py...)
```

> Nota de limpieza: algunos directorios heredados de otro proyecto (`launcher/`, `pruebas/`, `re-scripts/`, `remake/src`,
> `docs/01..06`) están propuestos para quitarse; el Classic no los usa.

### 2.1 Tabla "módulo -> qué hace -> archivo" (`remake-ps5/src/`)

**Motor, reglas e IA (sin Vulkan, se prueban sin pantalla)**

| Módulo | Qué hace | Archivo |
|---|---|---|
| Motor del duelo y efectos | `ia::Duelo`: estado, jugadas, sucesos (`S_*`), cadena (pila LIFO), ventanas de respuesta (`V_*`), combate, efectos de 1395 cartas (`DefCarta`, `tablaCartas()`), `ia::Base` (cartas y rivales), `Cerebro` (IA clásica) | `ia.h` |
| Planes y variedad | planes de juego, mazos dinámicos, combos, memoria del jugador, dificultad adaptable (racha) | `ia_plan.h` |
| IA humana | búsqueda Monte Carlo determinizada, faroles, lectura del jugador, errores, tiempo de pensar | `ia_humana.h` |
| Red de valor | MLP 85->64->32->1, 85 rasgos, carga de pesos `datos/ia_red.txt` | `ia_red.h` |
| Aprendizaje del jugador | graba duelos, ajuste fino de la última capa entre duelos | `aprendizaje.h` |
| Mazos del jugador | colección, mazos, biblioteca, límites Goat, mazos de inicio | `mazo.h` |
| Personajes e historia | lectura de `personajes.txt`, sagas, desbloqueos; diálogos | `personajes.h`, `historia_dialogos.h` |
| Resultado, estadísticas, velocidad | lógica del fin de duelo, estadísticas, escala de animación | `resultado.h`, `estadisticas.h`, `velocidad.h` |
| Opciones, guardado, idioma | `opciones.txt`, guardado atómico (`.tmp` + fsync + rename), tabla ES->EN | `opciones.h`, `guardado.h`, `idioma.h` |
| Bitácora del duelo | `duelo_log.txt` con cada suceso y la razón de la IA | `duelo_log.h` |

**Control y juego (integra motor + presentación)**

| Módulo | Qué hace | Archivo |
|---|---|---|
| Control del duelo | mesa, foco tipo consola, menús contextuales, máquina de estados con el mando (sin Vulkan) | `control.h` |
| Duelo en el juego | `presentarSucesos`, sincronización de la mesa con el motor, preguntas, cadena, visores | `duelo_juego.h`, `duelo_cadena.h`, `duelo_registro.h`, `duelo_consulta*.h`, `duelo_visores.h`, `duelo_ui.h` |
| Constructor de mazos | lógica, interfaz y ganchos (como Master Duel) | `constructor.h`, `constructor_ui.h`, `constructor_textos.h`, `mazo_duelo.h` |
| Menús | menú principal (puerta egipcia), opciones, portada | `menu_principal.h`, `menu_ui.h`, `menu_md_ui.h`, `portada.h`, `portada_ui.h` |
| Personalización | fundas, tapetes, controles, estadísticas | `cosmeticos*.h` |
| Galería 3D, tutorial | visor de figuras y reglas | `galeria*.h`, `tutorial*.h` |
| Pantalla de resultado | fin del duelo con cartas firma desbloqueadas | `resultado_juego.h`, `resultado_ui.h` |
| Interfaz | texto SDF, cartas, HUD, zonas del ratón | `interfaz.h` |
| Programa principal | clase del juego Vulkan, bucle, cámara, carga, arranque | `yugireload.cpp` (nombre heredado de la plantilla) |

**Presentación al estilo Master Duel (`md_*` y afines)**

| Módulo | Qué hace | Archivo |
|---|---|---|
| Reproductor de datos de MD | evalúa `AnimationCurve`, Bézier y Timeline/Tween de Unity | `clips_md.h` |
| Fases, engranaje, carteles | selector de fases (OPTIONS), menú del duelo, cartel de turno, LP | `fases_md.h`, `fases_md_juego.h`, `fases_md_ui.h` |
| Ritmo de cartas | movimientos de cartas (robo, colocar, cementerio, volteo, activación) con las curvas de MD | `md_ritmo.h`, `md_ritmo_juego.h`, `md_ritmo_ui.h`, `md_ritmo_pc.h` (escenas de captura) |
| Efectos traducidos | pistas 3D de los `CardRunEffect`, halos, sprites de efectos | `md_cardrun_ui.h`, `md_efectos_ui.h`, `efectos3_juego.h` |
| Entradas especiales | invocación por fusión/ritual y cartas clave | `entrada_md*.h`, `entrada_clave*.h` |
| Ritmo del turno | robo tras el cartel, lectura mínima, cartel de fases que bloquea | `ritmo_turno.h`, `ritmo_turno_juego.h`, `ritmo_turno_ui.h` |
| Ataque | ataque directo, viaje Bézier, daño, humo, elevación del atacante | `ataque_md.h`, `ataque_md_juego.h`, `ataque_md_ui.h`, `ataque_elevacion_juego.h` |
| Rayos de las figuras | aliento, magia y rayo de las figuras 3D | `ataque_fig_fx.h` |
| Efectos de magias/trampas | animación al activar (agujero negro, raigeki, espejo...) y su lógica sin Vulkan | `efectos_cartas.h`, `efectos_logica.h` |
| El rival enseña la carta | como *Joey the Passion*, antes de jugarla | `cpu_muestra.h`, `cpu_muestra_ui.h` |
| Equipo | círculo de equipamiento con rayo de unión y abducción | `equipo_ovni_juego.h` |
| Preguntas y listas | pregunta fija del duelo, elegir entre cartas con arte | `pregunta_ui.h`, `lista_cartas_ui.h` |
| Moneda, dado | lanzamiento de moneda | `moneda*.h` |

**Mundo 3D y sonido**

| Módulo | Qué hace | Archivo |
|---|---|---|
| Figuras 3D | carga `.glb`, esqueleto, ataque/defensa/muerte, tabla `datosFigura` | `figuras.h`, `orientacion_figura.h` |
| Entornos y fondos | fondo fotorrealista con profundidad, luz y partículas por personaje | `entornos.h` |
| Cartas de campo | tablero y partículas por carta de campo | `campos.h` |
| Bosque | carta de campo Bosque (mundo 3D por código) | `bosque.h` |
| Efectos del pack | flipbooks horneados del Niagara Examples Pack | `fx.h` |
| Audio | mezclador propio (16 efectos + música con fundido), WAV y OGG | `audio.h` |

**Entrada y red**

| Módulo | Qué hace | Archivo |
|---|---|---|
| Ratón | zonas de la interfaz -> pulsos del mando (solo lo lee Windows) | `raton.h`, `raton_juego.h`, `pc/raton_plataforma.h` |
| Modo en línea | menú, reloj de turno, revancha | `en_linea.h`, `en_linea_juego.h`, `en_linea_ui.h` |
| Red | UDP, canal confiable, lockstep, compromiso de barajado, NAT/STUN/UPnP, DHT, sala, ELO | `red/*.h` |
| Servidor de sala | Worker + Durable Object de Cloudflare | `servidor/sala/` |
| Pruebas con pantalla | front total en la vista previa de PC | `front_total_pc.h` |

### 2.2 Scripts de `remake-ps5/` (raíz)

| Script | Para qué |
|---|---|
| `sincronizar.sh` | copia el código y los assets a la plantilla PS5 y compila shaders |
| `probar_rama.sh` | compila una rama (PS5 + vista previa de PC) con candado y deja el título listo |
| `parche_base.py` | parches a la base Vulkan (glb binarios, JOINTS_0, fotos/video, OPTIONS, Win32) |
| `armar_pc.sh` | compila el `.exe` de Windows y arma la carpeta jugable |
| `imagen_titulo.py`, `subir_imagen.py` | imagen exFAT del título y subida a la consola |
| `subir_ps5.py`, `vaciar_titulo_ps5.py`, `ciclo_ps5.sh` | subida por carpeta (método antiguo, lento) |
| `fotos_ps5.py`, `esperar_video_ps5.py`, `leer_log_duelo.py` | fotos, video y bitácora desde la consola |
| `preparar_classic.py`, `preparar_ui.py` | caras de las cartas y `cartas_ui*.txt`, `cartas_classic.txt` |
| `rivales_gen.py`, `personajes_gen.py`, `personajes_datos.py`, `roles_gen.py`, `mazos_torneo.py`, `emblematicas_gen.py`, `fondos_datos.py` | generan los archivos de `datos/` |
| `cola_classic.py`, `cola_extra.py`, `cola_personajes.py`, `cola_fondos.py` | colas de Gemini (arte de cartas, retratos, fondos) |
| `fondos_post.py`, `retratos_post.py`, `portada_post.py`, `komami_dorso.py` | postproceso de imágenes |
| `bosque_insumos.py`, `entornos_insumos.py`, `campos_insumos.py`, `bosque_mallas_blender.py` | texturas CC0 (Poly Haven, ambientCG) |
| `captura_*.sh`, `pixeles_ft.py` | capturas en la vista previa de PC para revisar |
| `efectos_pendientes.py` | regenera la lista de efectos pendientes en `docs/efectos.md` |

---

## 3. Arquitectura del duelo y reglas de ritmo

### 3.1 Capas

```
 mando / teclado / ratón
          |   (bits PAD_*; el ratón se traduce a los mismos pulsos del mando)
          v
   control.h  ---- estado de la mesa, foco, menús, máquina de estados (sin Vulkan)
          | jugadas (J_*)                                  ^ eventos (animaciones a ejecutar)
          v                                                |
   ia::Duelo (ia.h): motor puro (reglas, efectos, cadena) -+   ia::Cerebro / Humana: decide el rival
          | sucesos (Suceso: S_ROBO, S_INVOCA, S_ATAQUE, S_COMBATE, S_ACTIVA_MT, ...)
          v
   duelo_juego.h: presentarSucesos -- cola de presentación en el orden del motor
          |  esperaEfecto / ritmo
          v
   interfaz.h + *_ui.h + yugireload.cpp -- dibuja (Vulkan), suena (audio.h), figuras 3D, efectos
```

- **Motor puro.** `ia::Duelo` (`ia.h`) es autoritativo: no sabe de Vulkan, de la interfaz ni de `control.h`. Una jugada
  (`Jugada`) produce **sucesos** (`Suceso`: robo, invocación, colocación, volteo, posición, activación de magia/trampa
  o de efecto, cementerio, ataque declarado, combate, daño, fase, turno, cadena, resolución, negación, LP, control,
  mano, desterrado, descarte, modificación, revelación). `Duelo::decide()` dice quién juega ahora; `Duelo::ocupado()`
  bloquea jugadas normales mientras hay cadena, pregunta o combate a medias.
- **Cadena y ventanas de respuesta.** Activar mete un eslabón en `ef.cadena` (pila LIFO); el otro lado recibe una
  ventana (`ef.ventana`: `V_INVOCACION_NORMAL`, `V_VOLTEO`, `V_INVOCACION_ESPECIAL`, `V_ATAQUE`, `V_ACTIVACION`,
  `V_FASE_FINAL`, `V_DISPARO`, `V_INICIO_BATALLA`, `V_PASO_DANNO`, `V_ESPERA`, `V_FIN_BATALLA`, `V_COLOCADO`,
  `V_DANNO`) **solo si tiene algo legal que responder**; dos pases seguidos resuelven. Los efectos disparados durante una
  cadena o combate arman una cadena nueva al terminar. El combate se hace al cerrar la ventana de ataque. Detalle:
  `remake-ps5/docs/efectos.md`.
- **Presentación.** La interfaz **no decide reglas**: recibe los sucesos y los presenta en orden con
  `presentarSucesos` (`duelo_juego.h`) y `sincronizarMesaEnOrden`: lo que se va en un suceso aún sin presentar sigue en
  su zona y lo que llega todavía no está (llega viajando cuando se presenta). `esperaEfecto` cubre cada viaje y cada
  activación: **el duelo (y el rival, y las pruebas) esperan a que la mesa esté al día**. Por eso lógica y presentación
  están separadas: el motor puede correr sin pantalla (pruebas, entrenamiento, modo en línea) y la presentación solo
  añade tiempo.
- **Modo en línea.** Solo viajan las decisiones (cada `Jugada`) con el hash del estado después de aplicarla; ambas
  máquinas ejecutan el mismo motor (lockstep); si el hash difiere, desincronización y fin sin ganador. Por eso **el
  motor no puede cambiar entre dos versiones sin subir `VERSION_JUEGO`** (`red/canal.h`).
- **Opciones que tocan la presentación y no el motor:** velocidad de animación (Normal = Master Duel; Rápida x1.75;
  Muy rápida x2.75; R2 escala), figuras 3D sí/no, bitácora.

### 3.2 Reglas de ritmo

Regla del proyecto: **se queda nuestro look and feel; los tiempos, curvas, orden y sonidos vienen de Master Duel** y, en
el rival, además, el gesto de *Joey the Passion*. Tabla de tiempos (detalle y fuentes en `remake-ps5/docs/md_ritmo.md`,
`ataque_md.md`, `md_fases.md`, `md_entradas.md`):

| Elemento | Tiempo usado | Origen |
|---|---|---|
| Robo propio | 0.58 + 0.22 s (0.80); en el turno: 0.25 + 0.28 + 0.50 mostrada + 0.65 = 1.68 s | MD `CardMoveDeck2HandMyself`, `DrawOperation` |
| Robo del rival | 0.45 s (no se muestra) | MD `CardMoveField2Hand` |
| Mano -> zona | 0.50 s (curvas de MD); monstruo fuerte 0.67 / 0.88 s | MD `CardMoveHand2MonstorZone*` |
| Al cementerio / desterrada / a la mano | 0.42 s / 0.45 s | MD `Field2GraveNormal`, `Field2Hand` |
| Volteo | 0.45 s | MD `CardFlipPlate*` |
| Activación | 0.15 + 0.70 + 0.25 = 1.10 s (la carta se levanta y mira a la cámara) | MD `CardShow` |
| Eslabón de cadena 2+ | +0.55 s; resolución 0.60 s | MD `DuelChainStack/Resolve` |
| Cartel de turno | 2.2 s (propio) / 1.617 s (rival) | MD `DuelTurnChange00/01` |
| Cartel de fase | ~0.97 s, bloquea al rival | MD `EffectTaskPhaseChange` |
| Selector de fases | entra 0.12 s (QuartOut), sale 0.12 s | MD `PhaseSelectWindow` |
| Rival "piensa" (tope) | 3.0 s | MD `CpuThinkingIcon.MAXTHINKINGTIME` |
| Rival enseña la carta antes de jugarla | volteo 0.267 s + 1.067 s con marco parpadeante = 1.33 s | *Joey the Passion* (30 cuadros/s, análisis estático del ejecutable) |
| Pausa mínima tras una jugada visible del rival | 0.9 s (`ritmoturno::LECTURA_MIN`) | observación propia |
| Ataque completo | **~2.95 s** (High, ATK >= 2500: 3.15 s; con figura 3D: 3.25 s) | *Joey the Passion* (~3 s) |
| Viaje del atacante | 0.45 s (Low) / 0.65 s (High) | MD `StrikeLight00/02` |
| Pausa en el objetivo tras el golpe | 0.50 s | MD `attackExecPostTime` (0.25) y Joey (0.53) |
| Destrucción en combate | se rompe a 0.70 s y llega al cementerio a ~1.12 s | MD `ReqBrokenEffect` |
| Elevación del atacante al declarar | sube 0.2 s, baja 0.2 s (0.45 de la mesa) | pedido del dueño; los tweens de MD no se pudieron leer |
| Cuenta de LP | 1.0 s lineal; número de daño ~0.8 s | MD `DuelLP.countTime` |

Joey the Passion, medido en su ejecutable (30 cuadros/s): cartel de turno 2.13 s, robo 0.27 s, invocación 1.1 s, ataque
completo ~3.0 s (3.2 s con rebote). Master Duel no tiene esperas fijas entre jugadas de la CPU: el ritmo sale de su cola
de tareas (`RunEffectWorker`); aquí lo emula `presentarSucesos` con `esperaEfecto`.

Cómo se traen los datos (los de Konami **no** van al repo): las clases y constantes salen de Il2CppDumper sobre
`GameAssembly.dll`; los ScriptableObject/Timeline/Tween se leen con UnityPy; todo se exporta con los scripts de
`herramientas/md_ritmo`, `herramientas/md_fases` y `herramientas/fx_md` a `classic-assets/md_ritmo`, `md_ui` y
`fx_master_duel`, y `sincronizar.sh` lo copia al título. Sin esos datos el juego cae a los mismos valores escritos a
mano en el código (o sin el efecto).

---

## 4. Datos: `remake-ps5/datos` y `classic-assets`

### 4.1 `remake-ps5/datos/` (versionados, texto UTF-8, `#` = comentario)

| Archivo | Formato | Lo escribe | Lo lee |
|---|---|---|---|
| `personajes.txt` | `id\|nombre_es\|nombre_en\|saga\|nivel\|personalidad\|entorno\|lema_es\|lema_en`. Sagas en orden: `reino ciudad noah dragones campeonato faraon`. `entorno` = clave del fondo | `personajes_gen.py` (de `personajes_datos.py`) | `personajes.h` |
| `personajes_rivales.txt` | como `rivales.txt` + `V\|id\|variante\|nombre_es\|nombre_en` (variante 0 = "la suya", 60 % de las veces) | `personajes_gen.py` | `ia.h` |
| `rivales.txt` | `R\|clave\|nivel\|personalidad\|tema\|nombre_es\|nombre_en\|lema_es\|lema_en`; `M\|clave\|variante\|ids` (principal); `X\|...` (fusión); `F\|fusión\|mat+mat`; `T\|ritual\|magia`; `N\|ids` (no invocables de modo normal) | `rivales_gen.py` (determinista, semillas CRC32) | `ia.h` |
| `mazos_torneo.txt` | `D\|clave\|plan\|nombre_es\|nombre_en`; `M\|clave\|ids`; `X\|clave\|ids`; `K\|clave\|ids clave`; `A\|rival\|claves` (a quién va) | `mazos_torneo.py` (listas a mano, validadas) | `ia::Base::leerTorneo` |
| `combos.txt` | `C\|clave\|piezas\|mínimo\|plan`; pieza = id, `@papel`, `%subtipo` o `&raza` | a mano | `ia_plan.h` |
| `roles_cartas.txt` | `id\|papeles` (letras: q quema, b bloqueo, w muro, d destruye, t quita magias/trampas, c castigo, k combo, e enjambre, o robo, a victoria alternativa, r recursión, g golpeador...) | `roles_gen.py` | `ia_plan.h`, `ia_red.h` |
| `efectos_cartas.txt` | `id\|efecto` (animación de activación: `agujero_negro raigeki fuerza_espejo renacer olla espadas polimerizacion cambio_corazon tormenta_pesada tifon cilindro_magico plumero mil_cuchillos ritual dado moneda magia trampa`). Sin entrada: genérica de magia (verde) o trampa (rosa) | a mano | `efectos_cartas.h` |
| `figuras.txt` | `id slug` (carta -> modelo `models/yugi/<slug>.glb` y `<slug>_anim.glb`) | `emblematicas_gen.py` | `figuras.h` |
| `emblematicas.txt` | `id\|nombre\|clase\|emblematica` (o `firma`) | `emblematicas_gen.py` (de `remake/diseno/emblematicas.json` y `cartas_firma.txt`) | herramientas |
| `cartas_firma.txt` | `nombre TCG\|límite\|dueños` (se desbloquea al vencer a un dueño) | `personajes_gen.py --firma` | `preparar_classic.py` |
| `cartas_extra.txt` | `nombre TCG\|límite\|nota` (cartas fuera de las 1690, sin dueño) | a mano | `preparar_classic.py` |
| `dialogos.txt` | `id\|tipo\|texto_es\|texto_en`; tipos `intro` (2-4 líneas), `pierde`, `gana`; id `_final` con tipos `texto` y `creditos` | a mano | `historia_dialogos.h` |
| `fondos.txt` | `id\|tema\|respaldo\|lugar_es\|lugar_en` (id = entorno del personaje, tema o `campo_<carta>`) | `fondos_datos.py` | `entornos.h` |
| `ia_red.txt` | `red 1 85 64 32`, `escala`, `mezcla`, `generacion`, `origen`, `pesos`, `fin <FNV-1a>` (si la suma no cierra, el archivo no vale) | `entrenar_ia_red.cpp` | `ia_red.h` |
| `red_stun.txt` | `nombre puerto [ip_respaldo...]` (servidores STUN) | a mano | `red/resolver.h` |

### 4.2 `classic-assets/` (carpeta externa, **no incluida en el repo**)

`sincronizar.sh` la toma de `YUGI_ASSETS` (por defecto `/compartido/classic-assets`). Estructura que espera (leída de
`sincronizar.sh`) y cómo se genera cada parte:

| Subcarpeta | Contenido | Cómo se genera |
|---|---|---|
| `ui/` | `cartas_ui.txt` (`id\|nombre\|clase\|ATRIBUTO\|raza\|nivel\|atk\|def\|texto`), `cartas_ui_en.txt`, `cartas_classic.txt` (`id\|passcode\|límite\|subtipo[\|firma\|dueños]`), `cara_<id>.png`, `mini_<id>.png`, `cara_en_*`, `cara_hd_<id>.jpg`, `portada.jpg`, `portada_letras.png`, `portada_brillo.png`, fuentes OFL | `preparar_classic.py` (lista Goat de YGOPRODeck, textos ES de la base ygopro `cards.cdb`, ilustración remasterizada con Gemini o provisional, marco con `remake/diseno/componer_cartas.py`); `portada_post.py` |
| `models/yugi/` | `<slug>.glb` (rígida) y `<slug>_anim.glb` (esqueleto con clips reposo, ataque, golpe, defensa, muerte), `tablero.glb`, `esfera.glb`, `rayo.glb`, `dado_md.glb`, `moneda_md.glb` | pipeline de figuras (6.2); `herramientas/blender` |
| `personajes/` | `retrato_<id>.png` (RGBA, 1400 px de alto) | `cola_personajes.py` + `retratos_post.py` |
| `fondos/` | `<id>.jpg` (<= 8192 px), `<id>_prof.png` (profundidad 16 bits, log2(m)/12, 65535 = cielo), `<id>.txt` (`plano`, `origen`, `cielo`, `niebla`, `tinte`) | `fondos_datos.py` -> `cola_fondos.py` (Gemini) -> `fondos_post.py` (Real-ESRGAN x4 + Depth Anything V2) |
| `entornos/` | texturas CC0 (`*_color.png`, `*_normal.png`) y `pano/` (paneles pre-renderizados con Blender, históricos) | `entornos_insumos.py`, `escenarios/*.py` |
| `campos/` | texturas 512 px del tablero de cada carta de campo | `campos_insumos.py` |
| `bosque/` | texturas y mallas `.malla` del Bosque | `bosque_insumos.py` + `bosque_mallas_blender.py` |
| `fx/` y `fx/pack/` | flipbooks (`.png` + `.json`) del Niagara Examples Pack | `herramientas/unreal/hornear_niagara.bat` |
| `fx_master_duel/` | `horneado/` (flipbooks de MD), `dado_moneda/` | `herramientas/fx_md/hornear_md.sh`, `sim_fx.py` |
| `md_ui/` | `*.mdtl`, `*.mdui`, `md_ui.png`, `md_ui_atlas.txt`, `md_entradas.png/_atlas.txt` | `herramientas/md_fases/exportar_clips.py`, `exportar_entradas.py` |
| `md_ritmo/` | `movimientos.txt` (141 movimientos), `efectos/<ef>.mdp` + `.png`, `md_efectos.png` | `herramientas/md_ritmo/exportar_movimientos.py`, `exportar_pistas.py`, `exportar_efectos.py` |
| `audio/` | `audio.txt` (`clave\|archivo\|volumen\|bucle`, bucle = `1:<inicio>:<fin>` en muestras), `.wav` (PCM 16 bit 48 kHz) y `.ogg` | `herramientas/audio/extraer.py`, `convertir.py`, `seleccion.json` |
| `sce_sys/` | `icon0.png`, `pic0.png`, portada original | arte del título |

El código tolera la ausencia de casi todo (cae a texturas neutras, al fondo del tema, a "solo activación por código" o a
juego mudo) y `sincronizar.sh` avisa por consola de lo que falte.

**Dependencia externa real**: varios scripts del pipeline de arte (`cola_*.py`, `preparar_classic.py`,
`retratos_post.py`, `fondos_post.py`, `portada_post.py`, `emblematicas_gen.py`) tienen rutas codificadas hacia el árbol
del proyecto hermano (fotos de los personajes, arte de Gemini en `nano-banana/`, `herramientas/pylib`, `esrgan`, modelos
`onnx`). Para regenerar arte desde cero hay que ajustar esas constantes (`BASE`, `PUENTE`, ...) a donde viva ese material.

---

## 5. Cadena de construcción (compilar, empaquetar, subir)

### 5.1 Requisitos

- Docker con el contenedor **`arch-ps5`** (Arch Linux con el SDK de payloads de PS5, `clang`, `g++`, `ninja`, `cmake`,
  `glslangValidator`, `rsync`, `flock`, `python3` con Pillow y `mingw-w64-gcc` para Windows). Montajes: la carpeta de
  trabajo compartida como `/compartido` (aquí `/compartido/wt/<rama>/` son los *worktrees* de git) y la plantilla en `/work`.
- Plantilla base: *PS5 Vulkan Template* (Sascha Willems, MIT; driver RADV). Dos copias: **`/work/PS5_ClassicRemaster`**
  (título `PPSA77002`) y **`/work/PC_ClassicRemaster`** (solo fuentes, para el `.exe`).
- Los assets (`classic-assets`) fuera del repo (sección 4.2).
- **Candado de compilación**: las plantillas son de todas las ramas, así que cada script toma
  `flock /compartido/.compilar.lock`; nunca se compilan dos ramas a la vez.
- Lo que no se verificó en esta pasada: que el contenedor y la plantilla estén hoy en pie (no se ejecutó nada).

### 5.2 PS5: compilar una rama

```bash
# dentro del contenedor arch-ps5
bash /compartido/wt/<rama>/remake-ps5/probar_rama.sh /compartido/wt/<rama>/remake-ps5 - 0
# args: <carpeta remake-ps5> <salida.mp4 | -> <segundos>; con "-" solo compila (sin video de la vista previa)
```

`probar_rama.sh`: (1) toma el candado; (2) `sincronizar.sh` copia el código y los assets a `/work/PS5_ClassicRemaster`,
aplica `parche_base.py`, compila shaders con `glslangValidator -V --target-env vulkan1.3` y reescribe `assets.json`;
(3) `ps5/tools/build.sh` compila el `eboot.bin`; (4) `ninja -C build/ps5-host ps5-samples-host` compila la **vista previa
de PC** (Vulkan sin ventana, superficie *headless*, `PS5_HOST_REFERENCE`) usada para capturas y pruebas; (5) `rsync` deja
el título armado en `/compartido/ps5/PPSA77002`. Un aviso conocido: comparación de punteros en `stb_vorbis.c` (código de
terceros).

`sincronizar.sh` también ajusta `param.json` (nombre "Yugioh Classic Remaster", `attribute3 |= 0x80040` para ofrecer
4K a 120 Hz si el televisor acepta HDMI 2.1) y **se niega a correr si la plantilla no es `PPSA77002`**.
`parche_base.py` aplica cada parche una sola vez y falla en voz alta si la base cambió: `.glb` binarios en `vkglTF`,
`JOINTS_0` en uint8 (Meshy), fotos y video a pedido, OPTIONS 2 s, nombre de ventana, y ramas `#if defined(_WIN32)`
para Windows.

### 5.3 PC (Windows)

```bash
bash /compartido/wt/<rama>/remake-ps5/armar_pc.sh /compartido/wt/<rama>/remake-ps5 [carpeta_salida]
# variables: YUGI_TITULO (título PS5 ya armado), SOLO_EXE=1 (no toca datos/)
```

Sincroniza con el candado, copia **solo las fuentes** a `/work/PC_ClassicRemaster`, compila `YugiohClassic.exe` con
`mingw-w64` (`pc/CMakeLists.txt`, `pc/toolchain_mingw.cmake`, enlace estático: un solo `.exe` sin DLL de MinGW; Vulkan
llega por `vulkan-1.dll` del driver), arma la carpeta jugable (assets como **enlaces duros** a los del título de PS5),
baja `SDL3.dll` (versión fija verificada con `pc/SDL3.sha256`, licencia zlib), convierte `icon0.png` a `yugioh.ico` y
escribe `LEEME.txt`.

### 5.4 Imagen del título para la consola

Subir ~11 000 archivos por FTP tarda horas; se distribuye **un solo archivo exFAT** que ShadowMount+ monta:

```
python -I remake-ps5\imagen_titulo.py --verificar         # lee la carpeta del título y escribe PPSA77002.exfat
python -I remake-ps5\subir_imagen.py                      # sube a /data/homebrew/ (reanudable, ~7 min para ~4,5 GB)
```

- `imagen_titulo.py`: escritor exFAT propio (sin montar, sin administrador): sector 512, **cluster de 64 KiB** (exigido
  por el perfil rápido `optimized-1.7` de ShadowMount+), FAT en el sector 128, archivos contiguos, fechas fijas (misma
  carpeta = misma imagen). Omite `radv-shader-cache`, `capturas` y ocultos. `--verificar` relee y compara; también se
  comprobó con `fsck.exfat -n`. Tarda ~7-10 min. Dentro del contenedor conviene `flock /compartido/.compilar.lock`.
- `subir_imagen.py`: sube a `<nombre>.exfat.part` y renombra al terminar (ShadowMount no toma un `.part`); reanudable con
  REST; si ya hay una imagen, la renombra a `.anterior` (no borra nada). Cerrar el juego antes de subir.
- Primera vez (pasar de carpeta a imagen): quitar la carpeta vieja de las rutas que escanea ShadowMount (un título = una
  fuente; el duplicado se ignora), subir la imagen, esperar ~30 s al escaneo y abrir el juego. Detalle en
  `remake-ps5/docs/imagen_titulo.md`.
- La imagen es de **solo lectura**: todo lo que el juego escribe va a `/data/yugioh_classic/` (mazos, progreso, opciones,
  idioma, capturas, caché de shaders RADV, bitácora).
- Método antiguo (lento): `subir_ps5.py <carpeta>` / `ciclo_ps5.sh` por carpeta. Nunca tocan otro título.

Sin IP en el repo: las herramientas FTP leen la dirección de la consola de `remake-ps5/consola_ip.txt` (archivo ignorado
por git). Las subidas a la consola son siempre una decisión del dueño.

### 5.5 Instalador de Windows (Inno Setup 6.7)

```
powershell -NoProfile -ExecutionPolicy Bypass -File remake-ps5\pc\instalador\armar_instalador.ps1
# parámetros: -Origen <carpeta del juego armada> -Salida -Version -Compresion lzma2/max -Solido -ISCC -Icono
```

Empaca `.exe`, `SDL3.dll`, `LEEME.txt`, `opciones_pc.txt`, `licencias\` y `datos\` (assets y shaders). Entrega
`YugiohClassic_Instalador.exe` + `YugiohClassic_Instalador-1.bin ... -3.bin` (cortes de 2 GB, **deben quedar juntos**). Sin
administrador, español e inglés, menú Inicio y escritorio opcional, silencioso con `/VERYSILENT /SUPPRESSMSGBOXES
/DIR=... /TASKS=escritorio`. El desinstalador conserva `%APPDATA%\YugiohClassic` (partidas). **La salida contiene arte de
Konami y audio de Master Duel: nunca va al repo** (`.gitignore` lo excluye). Límites: ver sección 8.

### 5.6 Servidor de sala (opcional)

`remake-ps5/servidor/sala` (Cloudflare Workers, plan gratis; HTTP sin TLS a propósito para no necesitar TLS en la PS5);
desplegar con `wrangler` desde el contenedor (Node en `/compartido/node`). Pruebas: `bash servidor/sala/probar.sh`.

---

## 6. Guías "cómo hago X"

> Regla de oro: después de cualquier cambio, compilar (5.2) y correr el ciclo de pruebas (sección 7). Si se toca `ia.h`,
> subir `VERSION_JUEGO` (el modo en línea exige el mismo motor en las dos máquinas).

### 6.1 Añadir o arreglar una carta y su efecto

1. **Datos de la carta.** Si está entre las 1690 Goat ya existe. Si no: agregarla a `datos/cartas_extra.txt`
   (`nombre TCG|límite|nota`; sin dueño) o a `personajes_datos.py` (si es firma de un personaje) y correr
   `python -I preparar_classic.py --solo <id>` (descarga datos en inglés de YGOPRODeck por nombre, el español de
   `cards.cdb`, usa la ilustración de Gemini o una provisional y recompone la cara ES/EN/HD).
2. **Efecto en el motor.** En `src/ia.h`, secciones "motor de efectos: datos" / "motor de efectos" / "las cartas":
   registrar una `DefCarta` (`objetivo`, `costeLP`, `descartes`, `permanece`, `ignicion`/`disparo`, `condicion()`,
   `objetivos()`, `resolver()`, `valor()` para la IA, `alSalir()`, `modAtk()`/`modDef()`/`impideAtaque()`,
   `espera()`/`finTurno()`). La velocidad de hechizo sale del subtipo de `cartas_classic.txt`. Si el efecto pide elegir,
   usar la pregunta del motor (`P_ELEGIR`) y seguir en `Eslabon::paso`.
3. **Animación (opcional).** `datos/efectos_cartas.txt` (`id|efecto`); sin entrada sale la genérica.
4. **Pruebas.** Añadir un caso en `src/pruebas/prueba_efectos.cpp` (escenario con la condición exacta **y** una situación
   donde no debe hacer nada). Correr la **certificación** (`bash src/pruebas/certificar.sh [solo_id]`): regenera
   `docs/certificacion_efectos.md/.csv`; el objetivo es `SIN PRUEBA 0`. Si el efecto es un atajo conocido, anotarlo en
   `src/pruebas/certificacion_notas.md` y en `docs/efectos.md` ("Atajos").
5. Si se arregla una carta, escribir primero la prueba que falla y luego el arreglo (así se hizo con Centrifugal Field,
   Fairy Guardian, etc.: `docs/control_final.md`).
6. Correr `efectos_pendientes.py` para actualizar la lista de pendientes de `docs/efectos.md`.

### 6.2 Añadir una figura 3D

Pipeline de arte (cada paso lo hace una herramienta; el dueño revisa en el PC antes de subir):

1. **Arte original** de la carta -> **Gemini** (`herramientas/puente-gemini`, cola con pausa de 20 s y reintentos): **una
   sola foto de la figura**, fiel a la carta. Verificar cada imagen contra su fuente (Gemini inventa: p. ej. Exodia en
   piezas).
2. **Imagen -> 3D**: TRELLIS en Colab (`herramientas/colab_trellis`, `trellis.py`, `instalar_trellis.py`) o **Meshy**
   (`herramientas/meshy.py`; cuida los créditos; las figuras con esqueleto son `_anim.glb` con clips Idle/Parry/dying
   usados como reposo/defensa/muerte).
3. **Limpieza en Blender** (`herramientas/blender/`: `quitar_base.py`, `quitar_anillo.py`, `recortar_altura.py`,
   `girar_modelo.py`, `reparar_textura.py`...), `quitar_tapa_gris.py`, `material_mate.py` (TRELLIS deja metálico),
   `arma_a_mano.py` (arma 100 % al hueso de la mano) y, para animar, `herramientas/esqueletos/` (`rig_auto.py`,
   `retarget_quaternius.py`, `animar.py`, `exportar.py`).
4. **Normalizar**: modelos a 1 de alto, pies en 0, frente +Z (glTF). Copiar a `classic-assets/models/yugi/<slug>.glb`
   (y `<slug>_anim.glb`).
5. **Registrar**: correr `python -I emblematicas_gen.py` (la carta pasa sola a `datos/figuras.txt` y sale de
   `docs/figuras_pendientes.md`) y añadir una fila a la tabla `datosFigura()` de `src/figuras.h`:
   `slug, escala, CuerpoFig (Bipedo/Cuadrupedo/Alado/Compacto), AtaqueFig (Embestida/Espada/Magia/Aliento/Rayo),
   salida del ataque {x,y,z} (boca o punta del báculo, en coordenadas del modelo), colores halo y núcleo, alas (bisagra,
   altura, abajo, pendiente, corte)`. Escala: alto lógico (`remake/diseno/tamanos.json`) x 2.6/1.8 (las de esqueleto miden
   1.8 m).
6. **Reglas automáticas para toda figura nueva** (acordadas con el dueño: no se revisa figura por figura): **orientación**
   (miran a la pantalla en reposo, defensa, aparición y entrada de carta clave; solo al atacar se voltean al objetivo y
   vuelven: `orientacion_figura.h`, medido por `CU-FT-FIGURA-ORIENTACION*`), **alas** (aleteo en reposo y alas recogidas
   en defensa), **armas** (la espada va a la mano del esqueleto: `empunarFigura`), **aliento y rayo** desde la boca o
   punta (por tabla), **muerte** (clip de muerte con esqueleto; de espaldas, al suelo o aplastado sin él) y **pies**
   (suelo = `SUELO_FIGURA`). Las herramientas `herramientas/atributos.py`, `escaneo_glb.py` y `orientar_auto.py` escanean
   un `.glb` para sacar alas, boca y orientación (nacieron en el proyecto hermano; **no verificado** que su salida siga
   alimentando directamente la tabla de `figuras.h`, que hoy está escrita a mano).
7. **Verificación visual.** `captura_figuras.sh` en la vista previa de PC con `YUGI_FIGURAS_MESA`/`YUGI_FIGURAS_ATAQUE`
   (ver la cabecera de `figuras.h`). La **prueba cartesiana** (barrido de ángulos y poses) era del proyecto hermano
   (`prueba_cartesiana.sh`); **no existe en este repo**: la comprobación equivalente aquí son las capturas de
   `captura_figuras.sh` y los casos `CU-FT-FIGURA-*`.
8. Pruebas: `CU-FT-FIGURA-*` (aparece solo boca arriba, desaparece al morir, respeta la opción "Figuras 3D").

### 6.3 Añadir un personaje a la historia

1. `personajes_datos.py`: `id`, nombre ES/EN, saga (`reino ciudad noah dragones campeonato faraon`), nivel 1-8,
   personalidad (agresivo, defensivo, paciente, tramposo, calculador, equilibrado...), entorno, lema ES/EN, foto y
   **variantes de mazo** (la 0 es "la suya"; nombres TCG en inglés, `"Nombre*2"` = 2 copias).
2. `python -I personajes_gen.py --firma` genera `datos/cartas_firma.txt` (cartas que no están entre las 1690: **la carta
   firma se desbloquea al vencer a su dueño**) y luego `python -I personajes_gen.py` escribe `datos/personajes.txt` y
   `datos/personajes_rivales.txt`. Después `preparar_classic.py` para las caras de las cartas firma nuevas.
3. `datos/dialogos.txt`: líneas `intro` (2-4), `pierde` y `gana` con el id del personaje, ES y EN.
4. **Fondo/entorno**: una fila en `fondos_datos.py` (escena en inglés **sin nombres de la franquicia**, luz, tema) ->
   `python -I fondos_datos.py` -> `cola_fondos.py` -> `fondos_post.py` (6.4). Sin fondo propio se usa el de su tema.
5. **Retrato**: `cola_personajes.py` (Gemini, fondo de croma) + `retratos_post.py` ->
   `classic-assets/personajes/retrato_<id>.png`.
6. Si la carta firma es un monstruo importante, hacer su figura 3D (6.2).
7. Pruebas: `CU-FE-RIVAL-*`, `CU-FE-DIALOGO-*`, `prueba_ia`/`prueba_variedad` (mazos legales). Los mazos de personajes
   cumplen la banlist Goat salvo las cartas firma.

### 6.4 Añadir un fondo, campo o entorno

- **Fondo por personaje o tema**: `fondos_datos.py` define `id`, tema, lugar ES/EN y la escena para Gemini (foto de cine
  21:9, sujeto centrado en el 15-75 % de la altura, cuarto inferior liso, sin personas ni texto). Flujo:
  `python -I fondos_datos.py` (escribe `datos/fondos.txt`) -> `python -I cola_fondos.py [--solo a,b]` -> `fondos_post.py <id>`
  (recorte, Real-ESRGAN x4, profundidad). Gemini rechaza prompts con nombres de franquicia ("derechos de terceros"): se
  describe el lugar.
- **Entorno (luz, partículas, siluetas)**: claves en `src/entornos.h` (`entornos::claveDe`, `porTema`); una clave
  desconocida da `neutro`. Contrato en `docs/entornos.md`. Texturas CC0: `entornos_insumos.py`.
- **Carta de campo**: fila `campo_<carta>` en `datos/fondos.txt`, texturas con `campos_insumos.py` y la entrada de la
  carta en `src/campos.h` (color de onda, partículas, tema). `docs/campos.md` lista las 25.

### 6.5 Añadir un mazo de torneo

Editar la lista en `mazos_torneo.py` (nombres TCG; `*N` copias), **solo con cartas del pool** (1690 Goat + extras, sin
firma), respetando la banlist Goat del juego; correr `python -I mazos_torneo.py` (valida: 40 cartas, máx. 3 copias y
límites, fusiones con sus materiales y Polymerization/Metamorphosis, rituales con su magia, Toons con Toon World) que
escribe `datos/mazos_torneo.txt`. La línea `A|rival|claves` decide a qué rivales va. Si falta una carta: `cartas_extra.txt`
+ efecto + cara (así se hicieron las 10 de torneo). Prueba: `prueba_torneo` (mazos legales, sin jugadas ilegales, % de
victorias). Detalle: `docs/mazos_torneo.md`. Cuidado: los combos con campo `torneo` en `combos.txt` no arman mazos
dinámicos de los demás rivales.

### 6.6 Añadir sonidos o música

Colocar el `.wav` (PCM 16 bit, 48 kHz) o `.ogg` en `classic-assets/audio/` y añadir la línea a `audio.txt`
(`clave|archivo|volumen|bucle`; música con puntos de bucle `1:<inicio>:<fin>`). El juego pide la **clave**
(`sfx_invocar`, `musica_duelo`...); cambiar qué suena es editar esa línea. Para sacar sonidos de una fuente propia:
`herramientas/audio/extraer.py` + `convertir.py` + `seleccion.json`. Prueba: `prueba_audio`.

### 6.7 Cambiar el ritmo

Las constantes están nombradas: `control::DURACION_ROBO`, `DURACION_ROBO_RIVAL`, `DURACION_ANIM`,
`ritmo::T_ATAQUE_BAJO/ALTO`, `T_ROMPE`, `T_POST_GOLPE`, `ritmoturno::LECTURA_MIN`, `ROBO_TURNO_RIVAL`, la espera del rival al
elegir (`cpu_muestra.h`), `Interfaz::ELEV_ALTO`. Las curvas vienen de `md_ritmo/movimientos.txt` (no editar a mano:
reexportar con `exportar_movimientos.py`). Al cambiar un tiempo: actualizar la tabla de `docs/md_ritmo.md` y los límites
de las pruebas (`CU-FE-ATAQUE-DURACION-JOEY` falla si el ataque en Normal no dura 2.8-3.3 s; los topes del jugador
humano en `cu_humano.h`). La opción de velocidad (x1.75, x2.75) escala todo sin tocar nada más.

### 6.8 Cambiar la dificultad y la IA

> Documento completo de la IA (capas, niveles, planes, mazos, combate, red, pruebas y palancas): [`docs/IA.md`](docs/IA.md).

- **Niveles 1-8** de los rivales: `rivales_gen.py` (techo de ATK por nivel, tributo, temas) y `Cerebro::ruido()` (520
  puntos de ruido a nivel 1, 86 a nivel 8). Tabla en `docs/rivales.md`.
- **Personalidad**: agresivo, defensivo, farolero, paciente, temerario, calculador, equilibrado (pesos del `Cerebro`).
- **Planes** (`ia_plan.h`): agresivo, control, bloqueo, enjambre, combo, castigo, molino, tempo, alternativa; roles de
  cartas en `roles_cartas.txt`; combos en `combos.txt`.
- **Dificultad adaptable** (Opciones): racha de victorias del jugador (`Memoria::racha`, de -1 a +4) que sube la fuerza de
  los planes y baja el ruido (`factorRuido` 0.45-1.1) **sin tocar el nivel del rival ni su pool**.
- **IA Clásica / Humana** (Opciones): la humana añade búsqueda Monte Carlo, faroles, lectura del jugador y errores
  creíbles; presupuesto de ~150 ms por decisión (medido: 14 ms medios, 101 ms máximo).
- **IA que aprende**: la **red de valor** `ia_red.h` mezcla su logit con la evaluación manual
  (`valor = (1-mezcla)*evaluarHumana + mezcla*escala*logit(red)`, `mezcla` 0.5). Se **entrena por autojuego** con
  `src/pruebas/entrenar_ia_red.cpp`:

  ```bash
  g++ -std=c++17 -O3 -march=native -I remake-ps5/src remake-ps5/src/pruebas/entrenar_ia_red.cpp -o /tmp/entrenar_ia_red
  HILOS=10 DUELOS_CLASICA=20000 DUELOS_HUMANA=1500 PARES=150 \
    /tmp/entrenar_ia_red /compartido/classic-assets/ui/ datos/ datos/ia_red.txt 3 > entrenar.log 2>&1
  ```
  Procesos con `fork` (el motor tiene banderas estáticas), Adam + entropía cruzada + parada temprana, búfer de hasta 900
  mil posiciones; una generación nueva solo se queda si le gana a la campeona >= 55 % en pares de duelos (la 0, si no es
  más débil que la humana sin red). Para medir sin entrenar: `PARTIDO_SOLO=<red.txt> CONTRA=ninguna|clasica|<red.txt>
  PARES=100`. Cambiar los rasgos o la forma de la red **invalida** `datos/ia_red.txt` (hay que reentrenar). En la
  consola, entre duelos, se ajusta solo la **última capa** (33 pesos) con los duelos grabados del jugador
  (`/data/yugioh_classic/ia_duelos.bin` -> `ia_red_jugador.txt`); "Olvidar lo aprendido" lo borra. Detalle:
  `docs/ia_red.md`.
- Como la IA corre en el motor, **cambiarla cambia las simulaciones del modo en línea**: subir `VERSION_JUEGO`.

### 6.9 Añadir un efecto visual traducido de Master Duel

1. Localizar el efecto en los datos exportados de MD (índice de bundles de `LocalData`; `herramientas/fx_md/indice.py`,
   `volcar_fx.py`, `volcar_ef.py`).
2. Efectos de partículas: hornear con `herramientas/fx_md/sim_fx.py` / `hornear_md.sh` a `fx_master_duel/horneado`
   (flipbooks).
3. Efectos `CardRunEffect` por carta (pistas 3D de sus `ParticleSystem`): `herramientas/md_ritmo/exportar_pistas.py` genera
   `md_ritmo/efectos/<ef>.mdp` + `.png`; el juego los reproduce en `md_cardrun_ui.h` (cámara de duelo de MD en `VISTA_MD`).
4. Sprites de efectos: `exportar_efectos.py` -> `md_efectos.png` (`md_efectos_ui.h`).
5. Registrar la carta en `datos/efectos_cartas.txt` (o el efecto nuevo en `efectos_cartas.h`) y el sonido en `audio.txt`
   (`herramientas/audio/seleccion.json`).
6. Prueba: `prueba_sprites_md.py` (los sprites de MD que pide la interfaz están en el título y ninguno es una máscara) y
   escenas de `md_ritmo_pc.h` con `captura_ritmo.sh`. Detalle: `docs/md_efectos.md`.

Los datos de Konami **nunca** se versionan; solo los scripts y los valores traducidos.

### 6.10 Traducir textos

`src/idioma.h`: `tr("texto en español")` busca en la tabla `{ "español", "English" }`; la interfaz la aplica sola en
`texto()`/`medir()`/`parrafo()`. Las partes variables (números, nombres de cartas) se unen **después** de traducir cada
pieza. Los datos de cartas van por `CartaUI::vNombre()/vAtributo()/vTipo()/vDesc()`; los archivos `_en` salen de
`preparar_classic.py` (`cartas_ui_en.txt`, `cara_en_*`). El idioma se guarda en `idioma.txt` (PC: `YUGI_IDIOMA=en|es`).
Ningún texto visible debe quedar fuera. Para añadir otro idioma habría que generalizar el par ES/EN de `idioma.h`
(**no hecho**).

### 6.11 Portar a otra plataforma

La plataforma se aísla en `platform.h` (`ps5/src`): `klog`, mando (`pad_player` con bits `PAD_*`), sonido (`audio_start`:
48 kHz, estéreo, 16 bits, bloques de 256 cuadros) y hora. El juego es Vulkan 1.3 puro sobre la base de Sascha Willems, que
ya trae Windows y Linux. Pasos (así se hizo Windows): (1) implementar `platform.h` en la nueva plataforma
(`pc/plataforma_windows.c`), (2) un arranque propio (`pc/main_windows.cpp`), (3) un `CMakeLists` que compile las mismas
fuentes, (4) la carpeta de guardado en **una sola función** (`guardado::carpetaDatos()`), y (5) parches de base detrás de
`#if` en `parche_base.py`. Regla: **no cambiar el motor, el duelo ni la IA** (lockstep).

---

## 7. Pruebas

### 7.1 Principios

- Todo se prueba **sin pantalla ni consola** (motor, interfaz real sin GPU, jugador humano simulado, red en loopback).
- **0 errores** significa: `ciclo_completo.sh` termina con código 0, **0 FALLA** en el resumen, y toda hoja de
  `docs/casos_de_uso.md` tiene al menos una prueba (una hoja sin prueba cuenta como FALLA). Las fallas que necesitan una
  decisión del dueño cuentan como **DECISIÓN** (`cu::decision`), no como falla (`docs/decisiones_pendientes.md`).
- Un ciclo se corre **completo desde cero** (recompila cada prueba, nada diferencial) dentro de `arch-ps5`.
- **Worktrees en paralelo**: cada rama de trabajo vive en `/compartido/wt/<rama>/` (un *worktree* de git propio); cada
  agente o persona edita solo en el suyo; compilar y probar toman el candado (`/compartido/.compilar.lock`); nada se sube a
  la consola ni se hace `push` sin pedirlo; cada rama terminada se une (merge) a la rama de integración.
- La validación en la PS5 la hace el dueño.

### 7.2 Cómo correr

```bash
bash /compartido/wt/<rama>/remake-ps5/src/pruebas/ciclo_completo.sh [carpeta de salida]
# deja un .log por prueba, casos_uso.tsv, resumen.txt y resultados.tsv; devuelve 0 solo con 0 fallas
bash .../src/pruebas/certificar.sh [solo_id]        # certificación de efectos
bash .../src/pruebas/humano.sh <salida> <modo> ...  # jugador humano (modos: fijos, duelos, general, cartas, todo, volcar, ritmo)
bash .../src/pruebas/humano_todo.sh                 # todo el jugador humano en lotes paralelos
bash .../src/pruebas/front_total.sh                 # batería desde el front
```

`ciclo_completo.sh`: (1) compila el juego (`probar_rama.sh`); (2) `prueba_sprites_md.py`; (3) compila y corre cada prueba;
(4) certificación; (5) casos de uso, front total, red desde el front, jugador humano y píxeles; (6) resumen.

### 7.3 Suites (`src/pruebas/`)

| Suite | Qué prueba |
|---|---|
| `prueba_mando.cpp` | el control con el mando (`control.h`) |
| `prueba_constructor.cpp` | constructor de mazos |
| `prueba_juego.cpp`, `prueba_duelo.cpp` | la ruta real del juego y duelos completos con el mando |
| `prueba_efectos.cpp` | ~881 casos a mano de efectos |
| `prueba_certificacion.cpp` | cada carta con efecto: plantilla + caso + presentación en el front (1395 OK) |
| `prueba_ia.cpp`, `prueba_variedad.cpp`, `prueba_ia_humana.cpp`, `prueba_ia_red.cpp` | rival, variedad de mazos y planes, IA humana vs clásica, red (`CU-BE-IA-RED-*`) |
| `prueba_torneo.cpp` | 21 mazos legales y % de victorias |
| `prueba_audio.cpp` | mezclador y archivos de audio |
| `prueba_red.cpp`, `prueba_red_nat.cpp`, `prueba_sala.cpp`, `prueba_red_front.cpp` | red, NAT, sala, DUELO EN LÍNEA desde el front (loopback) |
| `prueba_casos_uso.cpp` (+ `cu_front.h`, `cu_back.h`, `cu_cartas.h`, `cu_juego.h`) | árbol de casos de uso de `docs/casos_de_uso.md` (CU-FE front, CU-BE back, CU-CARTA carta por carta, CU-RED) |
| `prueba_front_total.cpp` (+ `cu_front_total.h`, `cu_ritmo.h`, `cu_ataque_directo.h`) | batería **desde el front**: duelos completos con el mando contra cada rival y carta por carta; píxeles de la mesa en la vista previa |
| `prueba_humano.cpp` (+ `cu_humano.h`, `pantalla_sin_gpu.h`) | **jugador humano**: decide solo con lo que la pantalla dibuja, con ritmo humano y travesuras; oráculos `CU-HU-*` (se entera de lo que pasa, nunca queda trancado, mesa coherente, ritmo) |
| `prueba_duelo_log.cpp`, `prueba_leer_log.py` | bitácora del duelo |
| `entrenar_ia_red.cpp` | entrenamiento (no es una prueba) |

Resultados documentados (`docs/ciclos_pruebas.md`, `control_final.md`, `md_ritmo.md`): ciclos de **7 008 a 7 088
casos** con 0 FALLA; front total 4 724 OK, jugador humano 1 824 OK, casos de uso 7 025 OK, red desde el front 58 OK;
certificación 1 395/1 395. Estos son los números **registrados en los docs**: **no se volvieron a correr en esta pasada**
(el trabajo fue de documentación).

---

## 8. Versión PC (Windows)

- **Capa Windows** (`remake-ps5/pc/`): `plataforma_windows.c` implementa `platform.h` (mando, teclado, ratón, audio),
  `main_windows.cpp` (arranque, ventana, Alt+Enter/F11, mensajes del ratón), `raton_plataforma.h`, `compat_windows.h`,
  `CMakeLists.txt` y `toolchain_mingw.cmake` (compilación cruzada con `mingw-w64`), `yugioh.rc`/`yugioh.manifest` (icono y
  manifiesto), `SDL3.sha256`.
- **Mando**: XInput (cargando `xinput1_4.dll`/`xinput9_1_0.dll` en tiempo de ejecución) y **SDL3** (DualSense, DualShock 4,
  Switch) cargada con `LoadLibrary` si `SDL3.dll` está junto al `.exe`. Mapeo a los mismos bits `PAD_*` de la consola.
- **Audio**: el mismo mezclador (`audio.h`) sobre **waveOut** (hilo que llena bloques de 256 cuadros a 48 kHz, 6 búferes).
- **Guardado**: `%APPDATA%\YugiohClassic\` (mazos, progreso, opciones, idioma, registro `yugioh_classic.log`); con
  `portable.txt` junto al `.exe`, en `guardado\`. El guardado es atómico (`MoveFileEx`).
- **Datos**: `datos\assets\` y `datos\shaders\` junto al `.exe`.
- **Video**: ventana de 1920x1080 (o 90 % del escritorio), `--completa`, `--ventana`, `--ancho N --alto N`, `--sin-vsync`,
  `--consola`; las mismas opciones, una por línea, en `opciones_pc.txt`. V-sync por defecto (a la frecuencia del monitor).
  Alt+Enter / F11: pantalla completa en caliente. Requiere GPU con Vulkan 1.3.
- **Recompilar**: `armar_pc.sh` (5.3). Vista previa del mismo código en Linux: `ninja -C build/ps5-host ps5-samples-host`.
- **Sin juego en línea en PC** (solo con `_WIN32`): el botón dice "DUELO EN LÍNEA: NO DISPONIBLE EN PC".

### Controles

| Acción | Mando (DualSense / Xbox) | Teclado | Ratón |
|---|---|---|---|
| Mover el foco | cruceta / stick izq | flechas / WASD | mover el puntero sobre una zona |
| Confirmar / abrir menú de acciones | X / A | Enter, Espacio | clic izquierdo |
| Cancelar / atrás | círculo / B | Esc, Retroceso | clic derecho |
| Detalle grande | triángulo / Y | T | clic central |
| Fases (selector de Master Duel) | cuadrado u OPTIONS corto (en tu turno) | F u O/Tab | placa FASES |
| Terminar turno (desde la fase de batalla) | círculo | Esc | - |
| Cámara | L1/R1 (modo), stick der (orbitar), L2/R2 (zoom), R3 (centrar) | Q/E, IJKL, 1/3, Z/C | rueda = L1/R1 en la mesa |
| Menú del duelo (engranaje) | panel táctil / Create / Back | G | clic en el engranaje |
| Opciones / pausa | OPTIONS | Tab, O | - |
| Pantalla completa | - | Alt+Enter, F11 | - |

Los iconos de la ayuda cambian según el último dispositivo usado (PlayStation, teclado y ratón, Xbox).

### Instalador y sus límites

Asistente de Inno Setup 6.7 (ver 5.5). Límites: pesa ~4 GB repartidos en `.exe` + `.bin` que deben ir juntos; no incluye
`en_linea.txt`; **contiene material de Konami**, por eso nunca va al repo; el instalador y el `.exe` **no están firmados**
(SmartScreen puede avisar; no verificado en una máquina limpia). Sin verificar (documentado en `docs/pc_jugable.md`):
mando real (XInput/DualSense) y el cambio automático de iconos, el puntero que se oculta a los 3 s, clic en algunas
pantallas (Personalización, Galería, teclado en pantalla, tutorial, resultado), pantalla completa en 4K a 120 Hz y otros
DPI.

---

## 9. Bitácora del duelo y herramientas de diagnóstico

- **Bitácora** (`duelo_log.txt`, `src/duelo_log.h`): una línea por suceso y por decisión de la IA, para juzgar si el rival
  juega con lógica. Opciones > "Bitácora del duelo" (Sí por defecto; `bitacora_duelo=0/1`). Se **añade** (append) al
  empezar cada duelo; pasados ~2 MB renombra a `duelo_log.anterior.txt`. No cambia el juego (ganchos opcionales de `ia.h`,
  no gasta azar; `prueba_duelo_log.cpp` comprueba que el duelo es idéntico con y sin bitácora). En la PS5:
  `/data/yugioh_classic/duelo_log.txt`; en PC: la carpeta de guardado.
- **Formato**: `mm:ss [T<turno> <fase> J|CPU] texto` (J = jugador, lado 0; CPU = rival, lado 1; fases `Robo, Principal1,
  Batalla, Principal2, Final`; zonas `z1..z5`). Líneas: `===== Turno N ... =====`, `roba`, `Invocacion Normal`, `coloca`,
  `ACTIVA X [eslabon n] | objetivo | coste | en cadena tras`, `resuelve`, `ATAQUE: A -> B | resultado esperado`,
  `combate real`, destrucciones y su causa, y la razón de la IA.
- **Leer desde el PC**: `python -I remake-ps5/leer_log_duelo.py una [-n 80]` (descarga y muestra) o `... seguir` (como
  `tail -f`). **Solo lectura** (SIZE y RETR por FTP).
- **Fotos y videos de la consola**: `fotos_ps5.py [PPSA77002]` baja las capturas de `/data/yugioh_classic/capturas/`, arma
  mosaicos y hojas por ataque y **borra las de la consola**; `esperar_video_ps5.py`. Pedido de fotos: subir `capturas.txt`
  a `/data/yugioh_classic/` (formato en `yugireload.cpp`).
- **Capturas en la vista previa de PC**: `captura_figuras.sh`, `captura_ritmo.sh`, `captura_pantallas.sh`,
  `captura_bosque.sh`, `captura_fx.sh`, `captura_constructor.sh`, `captura_entornos.sh`, `captura_front_total.sh`,
  `pixeles_ft.py`.
- **Variables de entorno de la vista previa (PC)**: `YUGI_GUION` (botones por guion), `YUGI_FIGURAS`,
  `YUGI_FIGURAS_MESA`, `YUGI_FIGURAS_ATAQUE`, `YUGI_FIGURAS_MEDIR`, `YUGI_PRUEBA_RITMO`, `YUGI_IDIOMA`,
  `YUGI_DISPOSITIVO=0|1|2`, `YUGI_ANCHO/ALTO`. En Windows: `YUGI_GUION_PC="3:X 7:X 11:C ..."` aprieta botones dentro del
  juego.
- **`yugictl`**: agente residente temporal de desarrollo en la consola, **solo para `PPSA77002`**, sin cerrar ni matar nada
  (el juego se cierra solo con `cerrar.txt`).

---

## 10. Decisiones de diseño, lecciones y pendientes

### 10.1 Decisiones de diseño (por qué)

- **Motor puro + presentación separada**: permite probar todo sin pantalla (miles de duelos), entrenar la red, jugar en
  línea por lockstep y cambiar la presentación sin tocar reglas. El motor vive en un solo archivo grande (`ia.h`) a
  propósito: una sola unidad de traducción, sin dependencias.
- **Formato Goat** (abril de 2005, 1690 cartas): una época bien definida, sin Sincronía/Xyz/Péndulo/Link; mazos de torneo
  reales de la época. Regla oficial elegida en las dos decisiones abiertas (repetición del ataque con elección de
  objetivo; ventana justo después del daño `V_DANNO` para Inferno Tempest y compañía).
- **Look nuestro, tiempos de Master Duel**: se copian curvas, tiempos y orden desde datos exportados (no el código C#, que
  depende de Unity). El rival "enseña la carta" como Joey.
- **IA impredecible**: el rival cambia de estrategia y de mazo entre duelos (3 variantes por rival, planes, ruido); prima
  la diversión sobre la predecibilidad. En una prueba, 800 duelos dieron 363 resultados distintos.
- **Fondos fotorrealistas** (matte painting de Gemini + profundidad para paralaje) en vez de polígonos: los fondos hechos
  con geometría por código se rechazaron ("un castillo real, como de una película"); un fondo distinto por personaje.
- **Un solo archivo exFAT** para el título: el FTP por carpeta se colgaba cada 250-300 archivos.
- **Guardado atómico** y todo lo escrito en `/data/yugioh_classic/` (la imagen es de solo lectura).
- **Candado de compilación** y *worktrees* por rama: la plantilla es compartida; evita compilaciones cruzadas.
- **Lockstep en línea**: solo viajan decisiones + hash del estado; compromiso criptográfico del barajado; reloj de turno de
  3 min por jugador; la desconexión de más de 30 s termina el duelo sin contar.
- **Sin juego en línea en PC**: decisión del dueño para la versión de Windows.

### 10.2 Lecciones y trampas conocidas

- **ShadowMount+**: un título = una fuente (si hay carpeta e imagen con el mismo ID, gana la que ya estaba); necesita
  cluster de 64 KiB e imagen en el disco interno para la vía rápida; no toma archivos `.part`; la imagen es de solo
  lectura. `ftpsrv` responde `226` a `DELE` (usar `sendcmd`) y se cuelga cada 250-300 archivos. La caché de shaders de RADV
  debe ir en `/data/yugioh_classic/`.
- **Al subir**: cerrar el juego antes; no tocar títulos ajenos (las herramientas se niegan a tocar otro ID).
- **Robo repetido**: el motor roba en el mismo paso en que termina el turno anterior; cualquier sincronización de la mesa
  antes de presentar `S_ROBO` ponía la carta en la mano antes de tiempo (arreglo: `ritmoTurnoOcultarRobos`). Lección:
  toda sincronización de la mesa pasa por `sincronizarMesaEnOrden`.
- **Pruebas con semilla por reloj** pueden fallar por azar: fijar la semilla o relajar la condición con criterio.
- **Gemini inventa** (Exodia en piezas, nombres): comparar cada imagen con su fuente; rechaza prompts con nombres de
  franquicia ("derechos de terceros"); no correr dos colas en el mismo Chrome.
- **Unión de ramas**: dos ramas que tocan `sincronizar.sh` pueden perder líneas al unir (pasó con la copia de los headers
  de MD, commit `69f77b0`): revisar la unión.
- **Ratón en Windows**: la entrada sintética con `PostMessage` no sirve del todo (Windows manda `WM_MOUSELEAVE`); usar
  `SetCursorPos`/`mouse_event` con la ventana arriba, y no mientras se usa el PC.
- **Enlaces duros**: `armar_pc.sh` enlaza los assets del título (NTFS por el montaje de Docker); si el título cambia a
  mitad (rsync en curso), el siguiente armado lo corrige (código 24 de rsync tolerado).

### 10.3 Límites conocidos y pendientes

- **Sin verificar en consola real** (lo valida el dueño): lo marcado "Para validar en la PS5" en
  `docs/decisiones_pendientes.md` (p. ej. el atasco de "no pude pasar a M2"; no se reprodujo en el PC).
- **Esconder la mano y el mazo del rival en el duelo en línea (compromiso versión 2)**: obligatorio **antes de abrir la sala
  al público**; hoy el orden comprometido viaja al empezar (lockstep con todo conocido): garantiza justicia pero una
  máquina modificada puede mirar la mano ajena. Entre amigos (LAN, código) se puede jugar ya. Falta validar un duelo real
  PS5 <-> PC (las pruebas juegan dos juegos completos en loopback).
- **Magic Cylinder** (efecto traducido de MD): las piezas van con la cámara de duelo de MD, no con la nuestra (si se
  cambia de cámara con L1/R1, no la siguen); la pista 3D completa quedó pendiente (`docs/md_efectos.md`).
- **Rayo fino del Cráneo Convocado**: el rayo eléctrico de las figuras tipo "Rayo" (Cráneo, Jinzo, Dragón de Trueno) se
  consideró demasiado fino frente al aliento; los últimos commits de la rama de ataque lo engrosaron (chispas y orbes que
  corren: `1f7d603`, `c273f34`). **No verificado**: no hay un registro escrito en `docs/` de que siga pendiente; se
  anota por indicación del dueño.
- Otras aproximaciones de efectos de MD: Mirror Force sin `flare04` ni `Sonic_01/02`; Swords es un sprite de pie, no la
  malla clavada; el contador `DuelTurnCount` no sale bajo la carta mostrada; el brillo amarillo sale en toda
  magia/trampa activa boca arriba (en MD solo en la que se puede activar); `CardIndividualData.mrkPowerTable` y los tweens
  de elevación del atacante se estiman.
- Aproximaciones de efectos de cartas: Fairy Guardian, Pitch-Black Power Stone y Level Conversion Lab (ver
  `docs/control_final.md` y `src/pruebas/certificacion_notas.md`).
- **Nombre de duelista** en línea: hoy "Duelista XXXX"; no se puede escribir. El mapeo UPnP no se libera si el juego se
  cierra de otra forma que "Salir".
- El instalador no está firmado; sin verificar en una máquina limpia.
- Nombres heredados de otro proyecto en el código (`yugireload.cpp`, carpeta de shaders `yugireload`) por exigencia de la
  plantilla: no es un fallo.

---

