# Mapa de casos de uso (Yugioh Classic Remaster, PPSA77002)

Rama `classic-ciclos`. Este es el árbol de lo que el juego tiene que hacer, del front (lo que el jugador ve y toca) al
back (las reglas, la IA, el guardado) y carta por carta. Tiene tres niveles: **caso** (`##`), **subcaso** (`###`) y
**sub-subcaso**. Las hojas son las líneas que empiezan con un ID entre comillas invertidas (`` `CU-...` ``).

Cada hoja tiene al menos una prueba automática:

- `src/pruebas/prueba_casos_uso.cpp`, con `cu_juego.h` (el juego real sin GPU: las pantallas de verdad y maquetas
  solo de Vulkan, sonido y figuras), `cu_front.h`, `cu_back.h` y `cu_cartas.h`;
- o una prueba de siempre (`src/pruebas/casos_suites.txt` dice cuál: la hoja vale si esa prueba terminó sin fallas
  en el mismo ciclo).

`src/pruebas/ciclo_completo.sh` corre todo desde cero y revisa que cada hoja de este archivo tenga su resultado. Si
una hoja no tiene prueba, cuenta como FALLA. Los resultados de cada ciclo están en `docs/ciclos_pruebas.md`. Las
fallas que necesitan una decisión del usuario están en `docs/decisiones_pendientes.md`.

---

## CU-FE: front end

### CU-FE-ARRANQUE: el juego arranca

- `CU-FE-COMPILA-PS5`: compila para la PS5 y la vista previa de PC (`probar_rama.sh`)
- `CU-FE-ARRANQUE`: carga las cartas, los rivales, la historia y la colección
- `CU-FE-GUARDADO-RUTAS`: progreso y opciones van en la carpeta de datos del jugador
- `CU-FE-CONS-MAZOS-INICIO`: la primera vez se arman los mazos de inicio y uno queda activo
- `CU-FE-AUDIO-MEZCLA`: el mezclador toca la música y los efectos

### CU-FE-PORTADA: la portada

- `CU-FE-PORTADA-ARRANQUE`: sale al arrancar, desde negro y con la música del menú
- `CU-FE-PORTADA-ESPERA`: no acepta botones durante el fundido de entrada
- `CU-FE-PORTADA-X`: X suena, funde a negro y va al menú principal
- `CU-FE-PORTADA-OPTIONS`: OPTIONS también va al menú
- `CU-FE-PORTADA-SIN-PAUSA`: en la portada OPTIONS no abre la pausa

### CU-FE-MENU: el menú principal

- `CU-FE-MENU-BOTONES-5`: los botones en orden (MODO DUELO, DUELO EN LÍNEA, CONSTRUCCIÓN DE DECK, LISTA DE CARTAS, OPCIONES, SALIR; el sexto, de la rama classic-en-linea, va debajo de los 5 de la puerta)
- `CU-FE-MENU-MUSICA`: suena la música del menú
- `CU-FE-MENU-NAVEGAR`: arriba y abajo dan la vuelta; la cruz mantenida repite
- `CU-FE-MENU-DUELO`: MODO DUELO va a elegir rival
- `CU-FE-MENU-CONSTRUIR`: CONSTRUCCIÓN DE DECK abre el constructor con su música
- `CU-FE-MENU-LISTA`: LISTA DE CARTAS abre la colección en modo solo ver
- `CU-FE-MENU-OPCIONES`: OPCIONES abre las 11 opciones y círculo vuelve
- `CU-FE-MENU-SALIR`: SALIR pide confirmar; X otra vez cierra el juego
- `CU-FE-MENU-SALIR-CANCELA`: otro botón cancela la salida
- `CU-FE-MENU-SIN-PAUSA`: en el menú OPTIONS no abre la pausa

### CU-FE-OPC: Opciones (cada opción, su efecto, guardado y carga)

- `CU-FE-OPC-LISTA`: las 14 opciones en orden
- `CU-FE-OPC-RUTA`: `opciones.txt` está junto a `progreso.txt`
- `CU-FE-OPC-MUSICA`: volumen de la música de 0 a 10, al instante y guardado
- `CU-FE-OPC-EFECTOS`: volumen de los efectos, suena de prueba y se guarda
- `CU-FE-OPC-PROHIBIDAS`: permitir cartas prohibidas cambia el límite de la colección (3 o banlist Goat)
- `CU-FE-OPC-PREGUNTAR`: preguntar antes de activar recorre Auto, Siempre y Nunca y se guarda
- `CU-FE-OPC-PREGUNTAR-EFECTO`: el duelo obedece (Nunca: no se detiene; Auto: pregunta si hay algo que activar)
- `CU-BE-PREGUNTAR-SIEMPRE`: Siempre: el motor pregunta al jugador en cada ventana, aunque no tenga nada
- `CU-FE-OPC-VELOCIDAD`: Normal, Rápida y Muy rápida; el duelo la lee y el factor sube
- `CU-FE-OPC-ADAPTABLE`: dificultad adaptable en `variedad.txt`; apagada, la dificultad es 0
- `CU-FE-OPC-IA`: IA Clásica o Humana en `variedad.txt`
- `CU-FE-OPC-APRENDE`: IA que aprende Sí o No en `variedad.txt`; en No la IA juega sin red (rama classic-ia-red)
- `CU-FE-OPC-OLVIDAR`: Olvidar lo aprendido borra los duelos grabados y la red del jugador (vuelve la base)
- `CU-FE-OPC-FIGURAS`: figuras 3D sí o no, guardado
- `CU-FE-OPC-IDIOMA`: español o inglés al instante y en `idioma.txt`
- `CU-FE-OPC-PERSONALIZACION`: abre Personalización
- `CU-FE-OPC-VOLVER`: Volver (X) o círculo guardan y vuelven
- `CU-FE-OPC-GUARDAR-CARGAR`: todo lo guardado vuelve igual al leer `opciones.txt`
- `CU-FE-OPC-CLAVES-AJENAS`: guardar no borra las claves de otras partes del juego
- `CU-FE-OPC-GUARDADO-ATOMICO`: no queda el `.tmp`

### CU-FE-PERSO: Personalización

- `CU-FE-PERSO-MENU`: su menú (Funda, Tapete, Controles, Estadísticas, Volver); círculo vuelve
- `CU-FE-PERSO-FUNDA`: elegir una funda libre la equipa y la guarda; la rejilla se recorre con la cruz
- `CU-FE-PERSO-FUNDA-BLOQUEADA`: una bloqueada no se equipa y avisa a quién hay que vencer
- `CU-FE-PERSO-TAPETE`: elegir un tapete libre; cada tapete tiene el escenario de su carta de campo
- `CU-FE-PERSO-TAPETE-BLOQUEADO`: uno bloqueado no se equipa
- `CU-FE-PERSO-CONTROLES`: las 3 pestañas (duelo, constructor, menús) con R1/L1 y sus filas
- `CU-FE-PERSO-ESTADISTICAS`: los duelos por rival, las cartas más jugadas y las firma desbloqueadas
- `CU-FE-PERSO-CAMARA`: la cámara elegida se guarda y se usa al arrancar
- `CU-FE-PERSO-GUARDAR-CARGAR`: funda y tapete vuelven al leer `opciones.txt`

### CU-FE-CONS: constructor de mazos y lista de cartas

- `CU-FE-CONS-MIS-MAZOS`: abre en MIS MAZOS
- `CU-FE-CONS-CERRAR`: círculo en MIS MAZOS vuelve al menú con su música
- `CU-FE-CONS-CREAR`: + Nuevo mazo con el teclado en pantalla
- `CU-FE-CONS-TECLADO`: el teclado escribe, borra, mayúsculas y acepta
- `CU-FE-CONS-BUSQUEDA`: buscar por nombre
- `CU-FE-CONS-FILTROS`: clase, subtipo, atributo, nivel, tipo, ATK, DEF, orden y quitar filtros
- `CU-FE-CONS-AGREGAR`: X agrega una copia
- `CU-FE-CONS-QUITAR`: cuadrado quita una copia
- `CU-FE-CONS-LIMITE-3`: no más de 3 copias y explica por qué
- `CU-FE-CONS-FUSION-EXTRA`: las fusiones van al extra
- `CU-FE-CONS-CLASE-RAPIDA`: L2/R2 cambian la clase rápida
- `CU-FE-CONS-STICK-DERECHO`: el stick derecho salta entre el mazo y la colección
- `CU-FE-CONS-VALIDAR`: un mazo inválido no se guarda
- `CU-FE-CONS-GUARDAR-SALIR`: círculo con cambios pregunta; Guardar y salir vuelve a la lista
- `CU-FE-CONS-USAR`: cuadrado lo hace el mazo de los duelos
- `CU-FE-CONS-COPIAR`: Copiar
- `CU-FE-CONS-RENOMBRAR`: Renombrar con el teclado
- `CU-FE-CONS-BORRAR`: Borrar pregunta antes
- `CU-FE-CONS-PROHIBIDAS`: la banlist Goat (prohibida, limitada, semilimitada) y la opción de permitirlas
- `CU-FE-CONS-FIRMA-BLOQUEADA`: una carta firma bloqueada no se agrega y dice a quién vencer
- `CU-FE-CONS-INICIO-VALIDOS`: los mazos de inicio son válidos y traen su carta de campo
- `CU-FE-CONS-LISTA-SOLO-VER`: la lista de cartas no agrega ni guarda
- `CU-FE-CONS-CARGAR`: los mazos vuelven igual al leerlos
- `CU-FE-CONS-MAZO-EN-DUELO`: el duelo roba del mazo activo, barajado
- `CU-FE-CONS-TORNEO-ABRIR`: Triángulo en MIS MAZOS abre MAZOS DE TORNEO con los 21 mazos (rama classic-torneo-extras)
- `CU-FE-CONS-TORNEO-VALIDOS`: los 21 mazos de torneo son válidos en la colección del jugador
- `CU-FE-CONS-TORNEO-VOLVER`: círculo en MAZOS DE TORNEO vuelve a MIS MAZOS
- `CU-FE-CONS-TORNEO-COPIAR`: X > Copiar a mis mazos (o Cuadrado) agrega el mazo a MIS MAZOS con las mismas cartas y su nombre
- `CU-FE-CONS-TORNEO-EDITAR`: Copiar y editar abre el editor sobre la copia y se edita como cualquier mazo
- `CU-FE-LISTA-CARTAS`: la lista de cartas muestra la colección y círculo la cierra

### CU-FE-GALERIA: galería de figuras 3D

- `CU-FE-GALERIA-ABRIR`: una entrada por figura, abierta desde la lista
- `CU-FE-GALERIA-ORDEN`: ordenadas por nombre
- `CU-FE-GALERIA-NAVEGAR`: la cruz mueve el cursor por la rejilla
- `CU-FE-GALERIA-ATACAR`: X hace atacar a la figura
- `CU-FE-GALERIA-BLOQUEADA`: una figura de carta firma bloqueada avisa
- `CU-FE-GALERIA-DETALLE`: triángulo abre el detalle
- `CU-FE-GALERIA-CERRAR`: círculo cierra

### CU-FE-RIVAL: elegir rival

- `CU-FE-RIVAL-HISTORIA`: modo historia: la saga actual y el primero sin vencer, con su mazo
- `CU-FE-RIVAL-HISTORIA-NAVEGAR`: izquierda/derecha cambian de personaje; arriba/abajo, de saga
- `CU-FE-RIVAL-SAGA-CERRADA`: una saga cerrada no deja empezar y avisa
- `CU-FE-RIVAL-VARIANTE`: cuadrado arma otra variante del mazo
- `CU-FE-RIVAL-REGLAS`: R1 abre las reglas; la cruz pasa las páginas; círculo cierra
- `CU-FE-RIVAL-DESAFIO`: L1 enciende el Modo Desafío solo con la historia terminada
- `CU-FE-RIVAL-CAMBIO-MODO`: triángulo cambia entre Historia y Duelo libre
- `CU-FE-RIVAL-LIBRE-NIVELES`: duelo libre: arriba/abajo cambian el nivel hasta el desbloqueado
- `CU-FE-RIVAL-LIBRE-RIVAL`: izquierda/derecha cambian el rival del nivel
- `CU-FE-RIVAL-LIBRE-AZAR`: cuadrado elige un rival al azar
- `CU-FE-RIVAL-VOLVER`: círculo vuelve al menú

### CU-FE-DIALOGO: diálogos de la historia

- `CU-FE-DIALOGO-CONTENIDO`: cada personaje tiene intro, victoria y derrota en ES y EN; el final tiene texto y créditos
- `CU-FE-DIALOGO-INTRO`: X sobre el rival abre su diálogo; X completa la línea o pasa a la siguiente; al final empieza el duelo
- `CU-FE-DIALOGO-SALTAR`: círculo salta el diálogo
- `CU-FE-DIALOGO-CIERRE`: el personaje tiene su línea de cierre

### CU-FE-DUELO: el duelo

#### Inicio
- `CU-FE-DUELO-MONEDA`: lanzamiento de moneda al estilo Master Duel: VS de los dos duelistas, la moneda gira y cae
- `CU-FE-DUELO-MONEDA-REGLA`: cara = gana el jugador, cruz = gana el rival (el jugador no elige)
- `CU-FE-DUELO-MONEDA-ANIMACION`: la moneda sube, cae, rebota y queda mostrando el lado que salió
- `CU-FE-DUELO-MONEDA-SONIDO`: suenan el VS, el lanzamiento, la caída y el brillo
- `CU-FE-DUELO-MONEDA-GANA`: si gana el jugador, se le pregunta ir primero o segundo
- `CU-FE-DUELO-MONEDA-GANA-PRIMERO`: elige ir primero y empieza él
- `CU-FE-DUELO-MONEDA-GANA-SEGUNDO`: elige ir segundo y empieza el rival
- `CU-FE-DUELO-MONEDA-PIERDE`: si gana el rival, decide la IA y se respeta
- `CU-FE-DUELO-MONEDA-JUSTA`: en 200 lanzamientos salen cara y cruz (y en duelos al azar, los dos)
- `CU-FE-DUELO-MONEDA-MANDO`: izquierda/derecha y X; no se salta sin elegir ni con la moneda en el aire
- `CU-FE-DUELO-MONEDA-VELOCIDAD`: con velocidad de animaciones rápida el lanzamiento se acorta
- `CU-FE-DUELO-MONEDA-TEXTOS`: textos ES/EN; los de piedra, papel o tijera ya no están
- `CU-FE-DUELO-INICIO`: 8000 LP y el mazo activo del jugador
- `CU-FE-DUELO-REPARTO`: el reparto anima 10 u 11 cartas y la mano queda igual a la del duelo
- `CU-FE-DUELO-MUSICA`: suena la música del duelo

#### Turno y fases
- `CU-FE-DUELO-FASE-ESPERA`: la fase de espera se ve antes de la principal 1
- `CU-FE-DUELO-TURNO-JUGADOR`: el jugador queda libre en su principal 1 y la fase en pantalla es esa
- `CU-FE-DUELO-FASES-CUADRADO`: cuadrado pasa de fase y termina el turno
- `CU-FE-DUELO-ROBO-ANIMADO`: cada robo se anima
- `CU-FE-DUELO-MESA-IGUAL-DUELO`: la mesa siempre muestra lo mismo que el duelo
- `CU-FE-DUELO-CONTADORES`: los contadores (mazo, extra, cementerio, desterradas) son los del duelo

#### La mano y la mesa
- `CU-FE-DUELO-MANO-MENU`: X sobre una carta abre su menú
- `CU-FE-DUELO-INVOCAR`: invocar con el mando pone la carta en el duelo y en la mesa
- `CU-FE-DUELO-COLOCAR-MONSTRUO`: colocar boca abajo
- `CU-FE-DUELO-VOLTEAR`: voltear
- `CU-FE-DUELO-POSICION`: cambiar de posición
- `CU-FE-DUELO-ATACAR`: atacar eligiendo el objetivo
- `CU-FE-DUELO-ACTIVAR-MAGIA`: activar una magia con sus costes y objetivos
- `CU-FE-DUELO-ACTIVAR-TRAMPA`: activar una carta colocada
- `CU-FE-DUELO-CAMPO-ACTIVAR`: activar la magia de campo
- `CU-FE-DUELO-ELEGIR-OBJETIVO`: elegir objetivo o carta con el foco o la lista
- `CU-FE-DUELO-PREGUNTAS`: las preguntas del duelo (responder con una trampa o no; elegir al resolver)
- `CU-FE-DUELO-REPETICION`: la repetición del ataque llega al jugador: elegir con el foco (círculo: no atacar) o con la
  lista ("Ataque directo", "No atacar")
- `CU-FE-DUELO-VENTANA-DANNO`: tras recibir daño, la pregunta "Recibiste daño" ofrece Attack and Receive; con
  "Preguntar antes de activar" = Nunca el duelo no se detiene
- `CU-FE-DUELO-JUGABLES`: lo que se puede jugar brilla, y nada más
- `CU-FE-DUELO-COMPLETO-MANDO`: duelos enteros solo con el mando, sin cuelgues

#### Lo que se ve
- `CU-FE-DUELO-DETALLE`: triángulo abre el detalle de la carta; círculo vuelve
- `CU-FE-DUELO-DETALLE-SCROLL`: el stick derecho desplaza el texto
- `CU-FE-DUELO-CADENA-VISIBLE`: la pila de la cadena: entran, se marcan negados, salen al resolverse
- `CU-FE-DUELO-REGISTRO`: L3 abre el registro del duelo y anota las jugadas
- `CU-FE-DUELO-VISORES`: los visores de cementerio, desterradas y extra coinciden con el duelo
- `CU-FE-DUELO-VISORES-ZONAS`: X sobre el cementerio o las desterradas abre su visor
- `CU-FE-DUELO-CEMENTERIO-VER`: ver el cementerio desde la mesa
- `CU-FE-DUELO-ENTRADA-CLAVE`: las cartas clave (firma o con figura) hacen su entrada con su clase y duración
- `CU-FE-DUELO-FIGURAS`: cada carta de `figuras.txt` existe y tiene su modelo
- `CU-CARTA-<id>-CAMPO` (por carta): cada magia de campo cambia el escenario (campos.h)

#### Primer turno en la PS5 (rama classic-ps5-arreglos1)
- `CU-FE-PS5-INVOCAR-ATAQUE`: contra Téa (plan bloqueo, el mazo del usuario, empieza el jugador) invocar con el mando
  deja la carta boca arriba en ataque; nada la pone en defensa sola y la mesa muestra cada monstruo como el duelo
- `CU-FE-PS5-CORTE-TRAMPA`: la carta que activa el rival en tu turno (Trap Hole) sale en el cartel grande con su nombre;
  tu monstruo sigue en la zona mientras se lee y al irse avisa "Al cementerio"
- `CU-FE-PS5-TABLERO-LIMPIO`: en el duelo real el tablero no dibuja las cartas horneadas del duelo de prueba ni la casilla
  de vidrio opaca que las tapaba (recuadro negro con pestañas naranjas en la PS5)
- `CU-FE-PS5-FASES`: con el cuadrado se pasa de fase hasta terminar el turno (también con la pregunta "¿activar?" o el
  detalle abiertos), sin quedar trabado

### CU-FE-PAUSA: la pausa

- `CU-FE-PAUSA-ABRE`: OPTIONS corto la abre con sus 5 opciones
- `CU-FE-PAUSA-DETIENE`: con la pausa abierta el duelo no avanza
- `CU-FE-PAUSA-CONTINUAR`: Continuar vuelve al duelo
- `CU-FE-PAUSA-OPCIONES`: Opciones abre las Opciones y círculo vuelve a la pausa
- `CU-FE-PAUSA-CERRAR`: OPTIONS corto la cierra
- `CU-FE-PAUSA-CONSTRUIR`: Construir mazo abre el constructor y vuelve al mismo duelo
- `CU-FE-PAUSA-LARGO`: mantener OPTIONS 2 s no abre la pausa
- `CU-FE-PAUSA-ABANDONAR`: Abandonar cuenta la derrota y muestra el resultado
- `CU-FE-PAUSA-MENU`: Salir al menú principal no cuenta derrota
- `CU-FE-PAUSA-ELEGIR-RIVAL`: en elegir rival, la pausa cambia de modo

### CU-FE-RESULTADO: el resultado del duelo

- `CU-FE-RESULTADO-PANTALLA`: ganar muestra la pantalla de resultado
- `CU-FE-RESULTADO-MOTIVO`: el motivo (LP a 0, Exodia, rendición...)
- `CU-FE-RESULTADO-DERROTA`: perder muestra su resultado
- `CU-FE-RESULTADO-MUSICA`: después del sfx, la música de victoria o de derrota
- `CU-FE-RESULTADO-NAVEGAR`: la cruz recorre las opciones
- `CU-FE-RESULTADO-REVANCHA`: Revancha repite el rival con otro sorteo
- `CU-FE-RESULTADO-OTRO-RIVAL`: Elegir otro rival
- `CU-FE-RESULTADO-MENU`: Menú principal
- `CU-FE-RESULTADO-CIRCULO`: círculo vuelve al menú
- `CU-FE-RESULTADO-SUBE-NIVEL`: ganar en el nivel máximo desbloquea el siguiente
- `CU-FE-RESULTADO-ESTADISTICAS`: cada duelo se cuenta en las estadísticas
- `CU-FE-RESULTADO-HISTORIA`: vencer a un personaje lo marca vencido
- `CU-FE-RESULTADO-FIRMA`: y desbloquea sus cartas firma
- `CU-FE-RESULTADO-SIGUIENTE`: dice quién es el siguiente rival de la historia
- `CU-FE-FINAL`: vencer al último abre el final de la historia
- `CU-FE-FINAL-CREDITOS`: el final pasa por los créditos
- `CU-FE-FINAL-DESAFIO`: el final desbloquea el Modo Desafío
- `CU-FE-DESAFIO-DUELO`: el duelo de desafío empieza con 4000 LP

### CU-FE-IDIOMA: español e inglés en todo

- `CU-FE-IDIOMA-MENU`: los botones del menú
- `CU-FE-IDIOMA-OPCIONES`: las opciones y sus valores
- `CU-FE-IDIOMA-PAUSA`: la pausa
- `CU-FE-IDIOMA-AVISOS`: los avisos del duelo
- `CU-FE-IDIOMA-FASES`: los nombres de las fases
- `CU-FE-IDIOMA-MENU-CARTA`: el menú de cada carta y sus motivos
- `CU-FE-IDIOMA-RESULTADO`: los motivos y las opciones del resultado
- `CU-FE-IDIOMA-REGLAS`: las reglas y la ayuda de botones
- `CU-FE-IDIOMA-CONTROLES`: los controles de Personalización
- `CU-FE-IDIOMA-PERSO`: fundas y tapetes
- `CU-FE-IDIOMA-SAGAS`: las sagas
- `CU-FE-IDIOMA-PERSONAJES`: nombre y lema de cada personaje
- `CU-FE-IDIOMA-RIVALES`: nombre y lema de cada rival
- `CU-FE-IDIOMA-CARTAS`: la colección en inglés (los filtros siguen igual)
- `CU-FE-IDIOMA-CONSTRUCTOR`: los avisos del constructor
- `CU-FE-IDIOMA-TORNEO`: MAZOS DE TORNEO, sus acciones y el nombre de cada mazo en español e inglés
- `CU-CARTA-<id>-DATOS` (por carta): nombre, texto, atributo y tipo en ES y EN

### CU-FE-MANDO: el mando

- `CU-FE-MANDO-CRUZ-TOQUE`: un toque es un paso
- `CU-FE-MANDO-CRUZ-REPETIR`: mantener la cruz repite (0,40 s y luego cada 0,09 s)
- `CU-FE-MANDO-STICK`: el stick izquierdo hace de cruz
- `CU-FE-MANDO-R2-ACELERA`: mantener R2 acelera las animaciones (sin animación es zoom)
- `CU-FE-MANDO-R2-SALTA`: un toque de R2 salta la animación
- `CU-FE-MANDO-VELOCIDAD`: los factores de velocidad suben

### CU-FE-GUARDADO: guardado atómico y avisos

- `CU-FE-GUARDADO-ATOMICO`: se escribe entero (temporal, fsync y renombrar) y reemplaza el viejo
- `CU-FE-GUARDADO-AVISO`: si no se puede guardar, avisa con el nombre del archivo
- `CU-FE-GUARDADO-SIN-CARPETA`: sin carpeta de datos, avisa una sola vez
- `CU-FE-GUARDADO-PROGRESO`: el nivel desbloqueado queda en `progreso.txt`
- `CU-FE-GUARDADO-HISTORIA`: los vencidos y el final quedan en `historia.txt`
- `CU-FE-GUARDADO-ESTADISTICAS`: `estadisticas.txt` vuelve igual al leerlo

---

## CU-BE: back end

### CU-BE-REGLAS: las reglas del duelo

#### Inicio, robo y fases
- `CU-BE-INICIO`: 8000 LP, 5 cartas, el extra son las fusiones, empieza en la principal 1
- `CU-BE-INICIO-QUIEN`: sin sorteo, empieza cualquiera de los dos
- `CU-BE-ROBO-PRIMER-TURNO`: el que empieza roba en su primer turno
- `CU-BE-ROBO`: cada turno se roba 1 (con su suceso)
- `CU-BE-FASES-ORDEN`: principal 1, batalla, principal 2, final
- `CU-BE-FASES-PRIMER-TURNO`: en el primer turno no hay batalla
- `CU-BE-FASES-PASA-TURNO`: la fase final pasa el turno
- `CU-BE-FASES-ESPERA`: robo, espera y principal 1 del turno nuevo

#### Invocaciones
- `CU-BE-INVOCAR-NORMAL`: invocación normal boca arriba en ataque
- `CU-BE-INVOCAR-UNA-POR-TURNO`: una sola normal por turno
- `CU-BE-INVOCAR-FASE`: solo en las fases principales
- `CU-BE-COLOCAR`: colocar boca abajo en defensa
- `CU-BE-TRIBUTO-1`: nivel 5 y 6 con 1 sacrificio, que va al cementerio
- `CU-BE-TRIBUTO-2`: nivel 7 o más con 2
- `CU-BE-TRIBUTO-COLOCAR`: colocar también pide sacrificios
- `CU-BE-TRIBUTO-DE-SOBRA`: no se sacrifica de más
- `CU-BE-TRIBUTO-DIOS`: los Dioses con 3 y no se colocan
- `CU-BE-VOLTEO`: voltear, no el turno en que se colocó
- `CU-BE-POSICION`: cambiar de posición una vez por turno, no el turno en que llegó
- `CU-BE-ESPECIAL`: invocación especial (Monster Reborn)
- `CU-BE-ESPECIAL-NO-GASTA-NORMAL`: la especial no gasta la normal
- `CU-BE-FUSION`: Polymerization con los materiales de la mano
- `CU-BE-FUSION-CAMPO`: o del campo
- `CU-BE-FUSION-SIN-MATERIALES`: sin los materiales, no
- `CU-BE-RITUAL`: magia ritual, monstruo ritual y sacrificios con nivel suficiente
- `CU-BE-RITUAL-NIVEL`: con nivel de menos, no
- `CU-BE-UNION`: la unión se equipa y da su bono
- `CU-BE-ESPIRITU`: el espíritu vuelve a la mano en la fase final
- `CU-BE-TOON`: el toon necesita Toon World y no se invoca normal

#### Límites
- `CU-BE-LIMITE-MANO`: con más de 6 en la fase final hay que descartar
- `CU-BE-LIMITE-ZONAS`: 5 monstruos y 5 magias/trampas; la de campo va a su zona

#### Cadena y ventanas
- `CU-BE-VENTANA-INVOCACION`: tras una invocación el rival puede responder (Trap Hole)
- `CU-BE-TRAMPA-MISMO-TURNO`: una trampa no se activa el turno en que se colocó
- `CU-BE-CADENA-VENTANA`: tras una activación el rival puede responder
- `CU-BE-CADENA-VELOCIDAD`: solo responde lo de velocidad suficiente
- `CU-BE-CADENA-LIFO`: la cadena se resuelve de arriba hacia abajo
- `CU-BE-EFECTOS-CASOS`: los casos a mano del motor de efectos
- `CU-BE-EFECTOS-ELECCIONES`: las elecciones al resolver

#### Batalla y daño
- `CU-BE-BATALLA-ATK`: ATK contra ATK: se destruye el más débil y su dueño recibe la diferencia
- `CU-BE-BATALLA-EMPATE`: ATK iguales: se destruyen los dos sin daño
- `CU-BE-BATALLA-DEF`: ATK contra DEF: el defensor no recibe daño; con DEF mayor, el atacante sí
- `CU-BE-BATALLA-BOCA-ABAJO`: el atacado boca abajo se voltea
- `CU-BE-BATALLA-DIRECTO`: ataque directo solo con el campo rival vacío
- `CU-BE-BATALLA-DEFENSA-NO-ATACA`: en defensa no se ataca
- `CU-BE-BATALLA-UN-ATAQUE`: un ataque por monstruo
- `CU-BE-REPETICION-ATAQUE`: si cambia el número de monstruos del rival, la repetición del ataque: el atacante elige
  (pregunta del motor); "no atacar" deja al monstruo volver a atacar en esa fase; sin cambio (Waboku) el ataque sigue
- `CU-BE-REPETICION-ATAQUE-ELEGIR`: en la repetición se elige otro objetivo o el mismo (el de antes va primero)
- `CU-BE-REPETICION-ATAQUE-DIRECTO`: ante un ataque directo aparecen monstruos: hay repetición sin ataque directo; si
  el objetivo se va y el campo queda vacío, la repetición ofrece el ataque directo
- `CU-BE-REPETICION-ATAQUE-IA`: la IA clásica y la humana responden la repetición con su política de ataque (letal:
  ataca; suicida: no ataca)
- `CU-BE-VENTANA-DANNO`: justo después del daño se abre una ventana solo para el que lo recibió y solo si tiene una
  carta "cuando recibes daño"; sin esa carta no se abre nada
- `CU-BE-VENTANA-DANNO-ATTACK-RECEIVE`: Attack and Receive en esa ventana (daño de batalla); el rival recibe 700 y tiene
  su propia ventana; después ya no se puede activar
- `CU-BE-VENTANA-DANNO-NUMINOUS`: Numinous Healer en esa ventana (daño de efecto, Hinotama); después ya no
- `CU-BE-VENTANA-DANNO-IA`: la IA clásica y la humana usan su carta en esa ventana

#### Condiciones de victoria
- `CU-BE-VICTORIA-LP`: LP a 0
- `CU-BE-VICTORIA-SIN-MAZO`: no poder robar
- `CU-BE-VICTORIA-EXODIA`: las 5 piezas de Exodia
- `CU-BE-VICTORIA-FIN`: con el duelo terminado no se juega

#### Atajos conocidos (de `control_final.md`; todos resueltos con la regla oficial)
- `CU-BE-ATAJO-FAIRY-GUARDIAN`: devuelve la magia que mandó el rival al cementerio este turno
- `CU-BE-ATAJO-PITCH-BLACK`: el contador va a cualquier carta que pueda tenerlo (también del rival) y se elige
- `CU-BE-ATAJO-LEVEL-LAB`: el monstruo de la mano lo elige quien la activa
- `CU-BE-ATAJO-INFERNO-TEMPEST`: se activa en la ventana justo después de recibir 3000+ de daño de batalla (con 1700, no)
- `CU-BE-ATAJO-INFERNO-TEMPEST-MOMENTO`: solo en esa ventana: después (batalla, fase final) ya no se puede activar

#### Primer turno en la PS5 (rama classic-ps5-arreglos1)
- `CU-BE-PS5-TRAP-HOLE`: invocación normal y Trap Hole del rival: cada carta a su cementerio, el duelo libre y el turno
  pasa a la Principal 2 y a la Final
- `CU-BE-PS5-SIN-CAMBIO-POSICION`: invocado en ataque sigue en ataque (sin cambios de posición que el jugador no pidió)
- `CU-BE-PS5-NUVIA`: Nuvia the Wicked invocada normal se destruye sola y el turno sigue

### CU-BE-IA: el rival

- `CU-BE-IA-MAZOS-RIVALES`: los 120 mazos de los rivales son válidos (40-60, copias, banlist)
- `CU-BE-IA-LEGAL`: la IA nunca intenta una jugada ilegal
- `CU-BE-IA-TERMINA`: los duelos IA contra IA terminan
- `CU-BE-IA-RESULTADOS-VARIAN`: los resultados varían
- `CU-BE-IA-NIVEL`: el nivel alto gana más que el bajo
- `CU-BE-IA-PLANES`: planes de juego que cambian entre duelos y a mitad de duelo
- `CU-BE-IA-MAZOS-DINAMICOS`: mazos dinámicos válidos y distintos entre duelos
- `CU-BE-IA-LECTURA-JUGADOR`: lee al jugador (trampero o farolero)
- `CU-BE-IA-ADAPTABLE`: dificultad adaptable tras rachas y se puede apagar
- `CU-BE-IA-MEMORIA`: la memoria del rival se guarda y se lee igual
- `CU-BE-IA-HUMANA-FUERZA`: la IA humana le gana a la clásica con los mismos mazos
- `CU-BE-IA-HUMANA-PRESUPUESTO`: piensa dentro de su presupuesto de tiempo
- `CU-BE-IA-HUMANA-DIVERSIDAD`: juega distinto entre duelos
- `CU-BE-IA-HUMANA-FAROLES`: faroles y trampas guardadas
- `CU-BE-IA-HUMANA-ERRORES`: errores creíbles por nivel
- `CU-BE-IA-RED-CARGA`: la red de valor base (`ia_red.txt`) se carga, da probabilidades válidas y evalúa en menos de 1 ms (rama classic-ia-red)
- `CU-BE-IA-RED-CORRUPTO`: un archivo de pesos o de duelos corrupto (o de otra forma) se descarta y se usa la red base
- `CU-BE-IA-RED-GRABACION`: los duelos del jugador se graban con tope (últimos 40) y se leen igual
- `CU-BE-IA-RED-AJUSTE`: el ajuste fino por rebanadas mejora la predicción sobre duelos de ese jugador y no traba (rebanada corta)
- `CU-BE-IA-RED-APAGADA`: con la opción en No la IA juega exactamente como antes (sin red)
- `CU-BE-IA-RED-FUERZA`: la IA con red no es más débil que sin red y le sigue ganando a la clásica
- `CU-BE-IA-RED-TIEMPO`: con red sigue dentro del presupuesto de tiempo por decisión
- `CU-BE-IA-RED-DIVERSIDAD`: con red sigue jugando distinto entre duelos

### CU-BE-TORNEO: mazos de torneo curados (rama classic-mazos-torneo; `datos/mazos_torneo.txt`, `docs/mazos_torneo.md`)

- `CU-BE-TORNEO-LEGAL`: los 21 mazos de torneo son legales (40, copias, banlist Goat, fusiones con materiales y
  Polymerization o con Metamorphosis, rituales con su magia, especiales con lo que los saca)
- `CU-BE-TORNEO-ASIGNACION`: cada mazo va a algún rival; solo genéricos de nivel 7-8, personajes de nivel 5+ y Joey
- `CU-BE-TORNEO-CARGADOR`: salen tal cual, con la probabilidad esperada y nunca a quien no tiene
- `CU-BE-TORNEO-IA-COMBOS`: la IA juega sus cartas clave y combos en duelos IA contra IA, sin jugadas ilegales
- `CU-BE-TORNEO-GANA-N8`: los mazos de torneo le ganan en mayoría a los temáticos de nivel 8
- `CU-BE-TORNEO-HUMANA`: la IA humana también los juega sin jugadas ilegales
- `CU-BE-TORNEO-JUEGO-CARGA`: el juego (`Base::leer`) carga los mazos de torneo y sus asignaciones
- `CU-BE-TORNEO-NIVEL-BAJO`: los niveles 1-4 nunca sacan un mazo de torneo (salvo Joey: decisión del usuario)
- `CU-BE-TORNEO-TAL-CUAL`: el mazo de torneo del duelo es el de la lista, con su plan, y el duelo empieza con él
- `CU-BE-TORNEO-NO-REPITE`: el mismo mazo de torneo no sale dos duelos seguidos
- `CU-BE-TORNEO-METAMORFOSIS`: Metamorphosis trae a Thousand-Eyes Restrict (Goat Control)
- `CU-BE-TORNEO-EXODIA-IA`: la IA junta las piezas de Exodia (no las invoca ni las descarta)
- `CU-BE-TORNEO-EXACTAS`: las listas usan las 10 cartas que faltaban (no los sustitutos) y todas tienen datos y efecto
- `CU-BE-TORNEO-JOEY`: Joey (nivel 4) lleva Gearfried y Equipos y le sale tal cual

### CU-BE-EXTRA: las 10 cartas extra de los mazos de torneo (rama classic-torneo-extras; `datos/cartas_extra.txt`)

Cada una tiene además su caso largo en `prueba_efectos.cpp` (`tandaTorneoExtras`), su certificación y sus hojas
`CU-CARTA-<id>-*`.

- `CU-BE-EXTRA-COMMAND-KNIGHT`: tus Guerreros +400 ATK; con otro monstruo no se le ataca
- `CU-BE-EXTRA-CYBER-DRAGON`: invocación especial desde la mano si solo el rival controla monstruos
- `CU-BE-EXTRA-FOOLISH-BURIAL`: un monstruo de tu mazo al cementerio
- `CU-BE-EXTRA-GOBLIN-ZOMBIE`: del campo al cementerio, busca un Zombi de 1200 DEF o menos
- `CU-BE-EXTRA-IL-BLUD`: invocado de nuevo, invoca un Zombi de la mano o de un cementerio; si se va, se destruye
- `CU-BE-GEMINIS-REGLA`: un Géminis boca arriba se invoca de nuevo con la invocación normal del turno y gana su efecto
- `CU-BE-EXTRA-MAGICAL-MALLET`: las cartas elegidas de la mano al mazo y roba las mismas
- `CU-BE-EXTRA-IZANAGI`: invocación especial desterrando un Spirit de la mano; los Spirit se quedan si quieres
- `CU-BE-EXTRA-YAMATO`: no se invoca normal; desterrando un Spirit del cementerio; vuelve a la mano al final del turno
- `CU-BE-EXTRA-CELTIC-GUARD`: no lo destruye en batalla un monstruo de 1900 ATK o más
- `CU-BE-EXTRA-DRILLROID`: destruye al que está en defensa antes del cálculo de daño

### CU-BE-MAZO: validación de mazos

- `CU-BE-MAZO-INICIAL`: el mazo inicial es válido
- `CU-BE-MAZO-MIN-40`: menos de 40, no
- `CU-BE-MAZO-MAX-60`: más de 60, no
- `CU-BE-MAZO-MAX-3`: más de 3 copias, no
- `CU-BE-MAZO-EXTRA-15`: más de 15 en el extra, no
- `CU-BE-MAZO-BANLIST`: la banlist Goat (prohibidas, limitadas, semilimitadas)

---

## CU-RED-NAT: conexión directa del modo en línea

Rama classic-red-nat (`src/red/nat.h`, `stun.h`, `upnp.h`, `resolver.h`), probada por `prueba_red_nat.cpp`: STUN con
los vectores del RFC 5769, DNS propio, SSDP/SOAP con respuestas de routers, UPnP contra un router de mentira, código
manual, clasificación del NAT y perforación en loopback y con NAT simulados en el proceso (matriz de 15 combinaciones).

- `CU-RED-NAT-STUN-RFC5769-PETICION`
- `CU-RED-NAT-STUN-RFC5769-IPV4`
- `CU-RED-NAT-STUN-RFC5769-IPV6`
- `CU-RED-NAT-STUN-ERRORES`
- `CU-RED-NAT-STUN-ARMAR`
- `CU-RED-NAT-STUN-MAPPED-VIEJO`
- `CU-RED-NAT-CRC32`
- `CU-RED-NAT-DNS-CONSULTA`
- `CU-RED-NAT-DNS-RESPUESTA`
- `CU-RED-NAT-DNS-ERRORES`
- `CU-RED-NAT-STUN-LISTA`
- `CU-RED-NAT-SSDP-ROUTERS`
- `CU-RED-NAT-SSDP-BUSQUEDA`
- `CU-RED-NAT-UPNP-DESC-MINIUPNPD`
- `CU-RED-NAT-UPNP-DESC-FRITZ`
- `CU-RED-NAT-UPNP-DESC-PPPOE`
- `CU-RED-NAT-SOAP-ARMAR`
- `CU-RED-NAT-SOAP-LEER`
- `CU-RED-NAT-HTTP-CHUNKED`
- `CU-RED-NAT-NATPMP`
- `CU-RED-NAT-UPNP-FLUJO`
- `CU-RED-NAT-UPNP-LIBERAR`
- `CU-RED-NAT-UPNP-CGNAT`
- `CU-RED-NAT-CODIGO-IDA-VUELTA`
- `CU-RED-NAT-CODIGO-CORTO`
- `CU-RED-NAT-CODIGO-IPV6-MINUSCULAS`
- `CU-RED-NAT-CODIGO-ERRORES`
- `CU-RED-NAT-CODIGO-MANUAL`
- `CU-RED-NAT-RANGOS`
- `CU-RED-NAT-CLASIFICA-CGNAT`
- `CU-RED-NAT-CLASIFICA-SIMETRICO`
- `CU-RED-NAT-CLASIFICA-CONO`
- `CU-RED-NAT-CLASIFICA-OTROS`
- `CU-RED-NAT-AVISOS`
- `CU-RED-NAT-TEXTOS-ES-EN`
- `CU-RED-NAT-PERFORAR-LOOPBACK`
- `CU-RED-NAT-PERFORAR-OTRA-SESION`
- `CU-RED-NAT-PERFORAR-LOOPBACK-V6`
- `CU-RED-NAT-SIM-MATRIZ`
- `CU-RED-NAT-SIM-MISMA-LAN`
- `CU-RED-NAT-SIM-MISMA-IP-PRIVADA`
- `CU-RED-NAT-SIM-CGNAT-CONO`
- `CU-RED-NAT-SIM-CGNAT-SIMETRICO`
- `CU-RED-NAT-SIM-MISMO-CGNAT`
- `CU-RED-NAT-SIM-PERDIDA`

## CU-CARTA: carta por carta

Para cada una de las 1784 cartas (`<id>` es su id). Estas hojas no se listan una por una aquí: salen de los datos de
las cartas y están todas en `resultados.tsv` de cada ciclo.

- `CU-CARTA-<id>-DATOS`: la carta en ES y EN con nombre y texto, la misma clase, nivel, ATK y DEF, el atributo y el
  tipo traducidos como en el resto de la colección y su línea de `cartas_classic.txt` (passcode, límite Goat, subtipo
  que corresponde a la clase)
- `CU-CARTA-<id>-CARAS`: sus 6 caras (`cara_`, `cara_en_`, `mini_`, `mini_en_`, `cara_hd_`, `cara_hd_en_`)
- `CU-CARTA-<id>-INVOCAR` (monstruos): se invoca como dice su texto (normal con sus sacrificios y colocada, Dioses con
  3, fusión con Polymerization y su receta o por contacto, ritual con su magia, toon con Toon World, guardianes con su
  arma); las que el texto dice que no se invocan de modo normal no se invocan; las de condición (Cave Dragon,
  Terrorking Archfiend), solo cuando se cumple
- `CU-CARTA-<id>-COLOCAR` (magias y trampas): se coloca boca abajo (la de campo en su zona)
- `CU-CARTA-<id>-EFECTO` (las 1385 con efecto): la certificación del mismo ciclo (casos a mano y plantilla, en el motor
  y en el front: `prueba_certificacion.cpp`)
- `CU-CARTA-<id>-CAMPO` (las 25 de campo): tiene su escenario en `campos.h`

### CU-RED-SALA: buscar rival en línea (rama classic-sala; `src/red/sala.h`, `dht.h`, `servidor/sala`, `docs/en_linea.md`)

Los prueba `prueba_sala.cpp` (con el Worker local de `servidor/sala/probar.sh`, que corre también `prueba_sala.mjs`).

- `CU-RED-SALA-BENCODE`: bencode de ida y vuelta (claves ordenadas) y rechazo de lo mal formado
- `CU-RED-SALA-KRPC`: ping, find_node, get_peers, announce_peer (token) y error 204 en una mini-red DHT en loopback
- `CU-RED-SALA-DHT-EMPAREJA`: dos que buscan por la DHT se encuentran y se emparejan (misma sesión, un solo anfitrión,
  candidatos del otro); cuatro a la vez forman dos parejas coherentes
- `CU-RED-SALA-VERSION-DISTINTA`: con otra versión del juego (o catálogo) no hay pareja, ni en la sala ni en la DHT
- `CU-RED-SALA-ELO`: la ventana de ELO se ensancha con la espera; con pocos jugadores, el primero que aparezca
- `CU-RED-SALA-CANCELAR`: cancelar no espera a los hilos y la sala deja de contarlo
- `CU-RED-SALA-CADUCIDAD`: una búsqueda sin consultas caduca; las sesiones viejas se borran
- `CU-RED-SALA-RESULTADO`: el resultado cuenta solo si los dos lados coinciden; la sala confirma el ELO y manda
- `CU-RED-SALA-ELO-LOCAL`: el ELO y el id anónimo se guardan en `en_linea.txt` con el guardado atómico
- `CU-RED-SALA-CLIENTE`: el cliente C++ contra el Worker local (HTTP sin TLS, long-polling) empareja PS5 con PC
- `CU-RED-SALA-NO-BLOQUEA`: `actualizar()`/`estado()` no bloquean el cuadro mientras el hilo espera a la sala
- `CU-RED-SALA-RESPALDO`: si la sala no responde, arranca la DHT sola y empareja igual; sin nada, «sin conexión»
- `CU-RED-SALA-LIMITE-IP`: la sala limita los pedidos por IP

---

## CU-PRUEBA: las pruebas enteras

Cada prueba de `ciclo_completo.sh` también es un caso (`CU-PRUEBA-<nombre>`): COMPILAR-JUEGO, prueba_mando,
prueba_constructor, prueba_juego, prueba_duelo, prueba_efectos, prueba_ia, prueba_variedad, prueba_ia_humana,
prueba_torneo, prueba_audio, prueba_red, prueba_red_nat, prueba_sala, prueba_red_front, CERTIFICACION y CASOS-DE-USO.

### CU-RED: modo en línea, base de red (rama classic-red-base; `src/red/`, `docs/red.md`)

- `CU-RED-SHA256`: SHA-256 propio contra los vectores de FIPS 180-2
- `CU-RED-SOCKET`: socket UDP en loopback IPv4, IPv6 y dual-stack; `Direccion` desde y hacia texto; direcciones locales
- `CU-RED-CANAL-PERDIDA`: canal confiable: 1200 mensajes (algunos de 4 KB, en pedazos) llegan una vez y en orden con 25 % de pérdida, duplicados y desorden
- `CU-RED-CAIDA-RECONEXION`: 10 s sin nada: caído; vuelve a los 20 s (desde otra dirección y puerto) y sigue con lo pendiente
- `CU-RED-PERDIDA-30S`: 30 s sin volver: perdido
- `CU-RED-VERSION`: versión del juego o catálogo de cartas distintos: el anfitrión rechaza y el invitado sabe por qué
- `CU-RED-CATALOGO`: el duelo en red con otro catálogo de cartas no empieza (rechazado)
- `CU-RED-LOCKSTEP`: 200 duelos completos IA contra IA por la red simulada (pérdida, duplicado, desorden), mazos variados: mismo resultado, mismos pasos y mismo hash de estado en las dos máquinas
- `CU-RED-PREGUNTAR-SIEMPRE`: «Preguntar antes de activar» distinto en cada máquina no desincroniza (viaja en el INICIO)
- `CU-RED-AJENOS`: paquetes ajenos (basura, otra sesión) se ignoran sin cortar el duelo
- `CU-RED-MONEDA`: la moneda sale de la semilla común (compromiso + revelación): justa y la misma en las dos máquinas
- `CU-RED-COMPROMISO`: cada robo de arriba del mazo se revela con su sal y se verifica; verificación final de los dos mazos OK
- `CU-RED-DESINCRONIZACION`: estado roto a propósito: se detecta en la jugada siguiente, termina sin ganador en las dos y cada una guarda su registro
- `CU-RED-ABANDONO`: el rival se corta: «esperando reconexión» y, a los 30 s, termina sin ganador (no cuenta para ninguno; rama classic-en-linea)
- `CU-RED-RENDICION`: el rival se rinde: gana el otro y se verifica su mazo
- `CU-RED-SALIR`: el rival cierra (ADIOS): abandono enseguida, sin ganador
- `CU-RED-TRAMPA-MAZO`: un mazo arreglado (el orden no sale de las semillas) se detecta al final
- `CU-RED-TRAMPA-REVELACION`: una carta revelada falsa se detecta en el momento
- `CU-RED-COMPROMISO-UNIDAD`: sal o carta equivocada no verifican; semilla privada falsa detectada; el orden no depende de cómo venía el mazo
- `CU-RED-UDP-LOOPBACK`: duelos completos por UDP de verdad (`iniciarDueloRed`) en IPv4, solo IPv4 e IPv6
- `CU-RED-RELOJ-AGOTADO`: (rama classic-en-linea) el invitado agota su reloj 3 veces: lo manda él, los dos lo cuentan y gana el anfitrión (FIN_TIEMPO)
- `CU-RED-REVANCHA`: (rama classic-en-linea) los dos piden revancha: el duelo vuelve a empezar en el mismo canal y se juega entero, igual en las dos
- `CU-RED-ESPEJO`: (rama classic-en-linea) la vista en espejo dos veces es el mismo duelo; manos, mazos, activo, sucesos y quien decide cambian de lado
- `CU-RED-ESPEJO-DUELOS`: (rama classic-en-linea) 12 duelos en los que el invitado decide mirando la vista en espejo (como en el juego): iguales en las dos máquinas
- `CU-RED-LAN`: red local: el anuncio se ve con nombre y puerto, otra versión se marca incompatible y el retiro lo saca de la lista

### CU-RED-FE: el DUELO EN LÍNEA desde el front (rama classic-en-linea; `prueba_red_front.cpp`)

Dos juegos completos sin GPU (`cu_juego.h`), cada uno con su bot de mando (`cuft::Bot`) en su hilo, por UDP en loopback; se
turnan de a un cuadro y la red usa el reloj de los cuadros (las caídas y el reloj del turno pasan en cuadros).

- `CU-RED-FE-AVISO-IP` / `-RECORDADO`: el aviso de privacidad sale la primera vez; cancelar vuelve al menú; aceptar se guarda (opciones.txt `aviso_ip=1`) y no vuelve a salir
- `CU-RED-FE-LAN-CREAR` / `-LISTA` / `-CONECTA-A` / `-CONECTA-B`: red local: A crea el duelo, B lo ve en la lista con el nombre de A, se une y los dos van a la moneda contra el otro
- `CU-RED-FE-MONEDA`: la moneda (la de siempre, sin elegir resultado) sale de la semilla común: el mismo ganador y el mismo que empieza en los dos
- `CU-RED-FE-INVITADO-LADO-1`: A es el anfitrión (lado 0) y B el invitado (lado 1 del motor)
- `CU-RED-FE-DUELO-COMPLETO`: un duelo entero jugado con el mando en los dos: el mismo final y el mismo estado; uno ve victoria y el otro derrota
- `CU-RED-FE-ESPEJO`: en cada cuadro al día, el invitado ve abajo SUS cartas (mano, LP, quién decide) y la mano de la mesa es la suya
- `CU-RED-FE-ESTADISTICAS`: los duelos en línea se cuentan aparte; los de la IA no cambian
- `CU-RED-FE-SIN-ELO-LAN`: la red local no muestra ni cambia el ELO
- `CU-RED-FE-REVANCHA-A` / `-B`: los dos piden la revancha: otra moneda y otro duelo en el mismo canal
- `CU-RED-FE-RENDICION`: rendirse cuenta como derrota (y victoria del otro, «El rival se rindió»)
- `CU-RED-FE-RELOJ-ALERTA`: con 30 s o menos el reloj propio avisa (sonido y parpadeo)
- `CU-RED-FE-RELOJ-1`: el reloj de A se agota: su máquina manda AGOTADO y pasa sola; B lo cuenta sin decidir por su reloj
- `CU-RED-FE-RELOJ-3`: 3 agotamientos: A pierde («Se agotó tu tiempo 3 veces»), igual en los dos
- `CU-RED-FE-RECONEXION`: B se corta 12 s: A ve «Esperando al rival… (reconectando N s)»; B vuelve y el duelo sigue
- `CU-RED-FE-DESCONEXION-NO-CUENTA` / `-OTRO-LADO`: B no vuelve en 30 s: «Duelo interrumpido: no cuenta» en los dos; ni estadísticas ni ELO
- `CU-RED-FE-CODIGO` / `-DUELO`: A muestra su código; B lo escribe con el teclado en pantalla; conectan y juegan
- `CU-RED-FE-IA-ESPERA`: buscando rival, «Jugar contra la IA mientras espero» lleva a un duelo contra la IA y la búsqueda sigue
- `CU-RED-FE-RIVAL-ENCONTRADO`: aparece el rival (DHT de una mini red en el proceso): cartel «¡Rival encontrado!» en pleno duelo contra la IA
- `CU-RED-FE-BUSCAR-A` / `-B`: X: los dos se conectan por la búsqueda y van a la moneda
- `CU-RED-FE-IA-NO-CUENTA`: el duelo contra la IA abandonado no cuenta; el duelo en línea sí
