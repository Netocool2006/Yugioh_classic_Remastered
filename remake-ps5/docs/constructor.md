# Constructor de mazos (rama classic-constructor2)

Yugioh Classic Remaster tenía un "Construir mazo" que en la práctica era un listado: un solo mazo, sin búsqueda
y sin forma cómoda de probar cartas como las mágicas de campo. Esta rama lo rehace siguiendo el editor de mazos de
**Yu-Gi-Oh! Master Duel**, adaptado al DualSense y a las reglas del Formato Goat (abril de 2005).

## 1. Cómo es el editor de Master Duel

Fuentes: la guía del deckbuilder de outof.games
(<https://outof.games/yu-gi-oh/guides/138-how-to-use-yu-gi-oh-master-duels-deckbuilder-to-create-decks/>), las notas
de la actualización de febrero de 2025 en ygorganization.com (filtros de ATK/DEF por rango) y masterduelmeta.com.
Master Duel no estaba instalado en `D:\SteamLibrary\steamapps\common\Yu-Gi-Oh!  Master Duel` ni en otras
bibliotecas de Steam de esta PC, así que no se miraron sus recursos de UI. Tampoco se copió ningún recurso.

- **Lista de mazos (Deck).** Muestra todos los mazos guardados (hasta 20) y un botón **+**. Con **+** se crea un mazo
  vacío o se copia uno público o un Structure Deck. Al elegir un mazo salen las acciones: ver, editar, copiar,
  borrar, renombrar y fijarlo como mazo principal para los duelos.
- **Pantalla de edición, con tres vistas:**
  - **Card (detalle):** la carta elegida grande, con su texto completo, atributo, tipo, nivel y ATK/DEF, y botones
    +/- para cambiar las copias sin moverse.
  - **Deck:** el mazo principal (40-60) y el extra (hasta 15), cada copia por separado y con contadores. El nombre se
    edita ahí mismo. Una carta va sola al principal o al extra según su tipo.
  - **Collection:** la colección en rejilla, con búsqueda por nombre o texto, un botón de **filtros** (tipo de
    carta, atributo, nivel o rango, tipo de monstruo, rareza, ATK/DEF por rango y otros), un botón de **orden** y
    una papelera que borra filtros y búsqueda. Cada carta muestra cuántas copias tienes.
- **Validez:** al guardar, avisa si el mazo no es legal (cantidad, límites de la banlist). Uno ilegal no se puede usar.
- **Con mando:** la cruz mueve el foco entre la colección, el mazo y los botones; un botón agrega y otro quita; los
  gatillos y bumpers cambian de página o de pestaña; un botón abre los filtros, y la búsqueda usa el teclado en
  pantalla del sistema.

## 2. El constructor del juego

Código: `src/constructor.h` (lógica), `src/constructor_ui.h` (dibujo), `src/mazo.h` (colección, mazos, biblioteca,
mazos de inicio), `src/mazo_duelo.h` (ganchos en el juego) y textos ES/EN en `src/idioma.h`.

### Pantalla MIS MAZOS

Muestra la lista de mazos (hasta 20) con su nombre, principal + extra y si es válido. El mazo de los duelos lleva un
orbe dorado y la etiqueta EN USO. A la derecha, una vista previa con los contadores y las cartas distintas del mazo.
Al final de la lista está **+ Nuevo mazo**.

| Botón | Acción |
|---|---|
| Cruz arriba/abajo | Elegir un mazo (con repetición, como el resto del menú) |
| X | Abre las acciones: Editar, Usar en los duelos, Renombrar (teclado en pantalla), Copiar, Borrar (pide confirmación) |
| X en "+ Nuevo mazo" | Pide el nombre con el teclado en pantalla y abre el editor vacío |
| Cuadrado | Usar ese mazo en los duelos (solo si es válido; si no, dice por qué) |
| Círculo | Volver al menú principal |

### MAZOS DE TORNEO (rama classic-torneo-extras)

Como los Structure Deck de Master Duel: **Triángulo** en MIS MAZOS abre los 21 mazos de torneo (los de los rivales:
`datos/mazos_torneo.txt`, ver `docs/mazos_torneo.md`), con la misma lista y la vista previa (contadores y cartas). El
nombre sale en el idioma activo.

| Botón | Acción |
|---|---|
| Cruz arriba/abajo | Elegir un mazo de torneo |
| X | Acciones: Copiar a mis mazos, Copiar y editar (abre el editor sobre la copia), Volver |
| Cuadrado | Copiar a mis mazos |
| Círculo | Volver a MIS MAZOS |

La copia es un mazo más (cuenta para los 20, se renombra, se usa en los duelos y se edita como cualquiera); el mazo de
torneo original no cambia. Los carga `mazo::leerMazosTorneo` (en `mazo_duelo.h::prepararMazos`). Textos ES/EN en
`idioma.h`. Pruebas: `prueba_constructor.cpp` (sección 5) y `CU-FE-CONS-TORNEO-*`.

### Editor (pantalla dividida)

- **Izquierda, el detalle:** la carta grande (HD si existe) y debajo el nombre, la clase y el subtipo ("Magia de campo",
  "Monstruo de efecto", etc.), el atributo, el tipo, el nivel, el ATK/DEF y el texto completo en el idioma activo.
  Abajo, las copias en el mazo y el límite Goat. En una carta firma bloqueada salen un candado y
  "Vence a X para desbloquear".
- **Medio, el mazo:** el principal en 10×6 casilleros (40 marcados), los contadores (principal x / 40-60, monstruos,
  magias, trampas) y el **MAZO EXTRA x / 15** en 8×2. Si hay cambios sin guardar, aparece "SIN GUARDAR".
- **Derecha, la colección:** 6×4 por página, con el resumen de la búsqueda y los filtros activos, las copias que
  tienes en el mazo de cada carta, las etiquetas PROHIBIDA, LIMITADA, SEMI y BLOQUEADA, y las agotadas apagadas.
- **Abajo:** la validez en vivo ("MAZO VÁLIDO" o el motivo exacto) y la ayuda de botones.

| Botón | Acción |
|---|---|
| Cruz / stick izquierdo | Mover. Por el borde se pasa del mazo a la colección y al revés; abajo del principal, al extra |
| Stick derecho izq/der | Saltar al mazo o a la colección |
| Stick derecho arr/ab | Desplazar el texto de la carta |
| X | Agregar una copia (las fusiones van solas al extra) |
| Cuadrado | Quitar una copia |
| L1 / R1 | Página anterior / siguiente de la colección |
| L2 / R2 | Clase rápida: Todas, Normal, Efecto, Ritual, Fusión, Magia, Trampa |
| L3 | Quitar los filtros |
| Triángulo | Buscar y filtrar |
| OPTIONS | Guardar (solo si el mazo es válido; si no, explica por qué) |
| Círculo | Volver a MIS MAZOS. Con cambios pregunta: Guardar y salir / Salir sin guardar / Seguir editando |

**Buscar y filtrar (Triángulo):** Nombre (X abre el teclado y filtra mientras se escribe), Clase, Subtipo (Normal,
Campo, Equipo, Continua, Rápida, Contraataque, Ritual, Volteo, Toon, Unión, Espíritu), Atributo, Nivel (1-12), Tipo de
monstruo, ATK y DEF por rango (0-999 … 2500+), Orden (clase y nombre, nombre, ATK, DEF, nivel), Quitar filtros y Listo.
Con la cruz se elige la fila y se cambia el valor. Cuadrado pone esa fila en Todos y Círculo cierra.

**Teclado en pantalla:** números, QWERTY con Ñ, guion, apóstrofo y punto, y además MAYÚS, ESPACIO, BORRAR y ACEPTAR.
X escribe, Cuadrado borra, Triángulo pone un espacio, L1/R1 cambia a mayúsculas, OPTIONS acepta y Círculo cancela.
La búsqueda no distingue tildes ni mayúsculas, y cada palabra tiene que estar en el nombre.

### LISTA DE CARTAS (solo ver)

Es la misma rejilla con búsqueda, filtros y panel de detalle, pero ocupa también el lugar del mazo (10 columnas).
No tiene panel de mazo y no deja agregar, quitar ni guardar. Se maneja con Cruz, Triángulo (buscar y filtrar),
L1/R1, L2/R2 y L3, y Círculo vuelve al menú.

Entradas para el menú principal, en `src/constructor.h` (`namespace constructor`, funciones libres). Las llama el
menú de la rama classic-portada, y el juego abre la pantalla al empezar el cuadro siguiente
(`mazo_duelo.h::avanzarConstructor`, con `constructor::tomarPedido()`):
- `constructor::abrirConstructorMazo()`: CONSTRUCCIÓN DE DECK (MIS MAZOS y el editor).
- `constructor::abrirListaCartas()`: LISTA DE CARTAS.

En el PC: tecla M (constructor), tecla L (lista), `YUGI_CONSTRUCTOR=1` o `YUGI_LISTA_CARTAS=1`.

### Reglas que se validan en vivo

- Principal entre 40 y 60 cartas; extra hasta 15, y en él solo fusiones (van solas).
- Hasta 3 copias de cada carta; la banlist Goat (`cartas_classic.txt`): prohibida 0, limitada 1, semilimitada 2.
- Cartas firma bloqueadas hasta vencer a su dueño en el modo historia (candado y "Vence a X para desbloquear").
- Un mazo inválido no se guarda ni se puede usar en los duelos, y siempre se dice el primer motivo.
- **Opciones > "Permitir cartas prohibidas"** (la pantalla es de la rama classic-portada). Se guarda en
  `/data/yugioh_classic/opciones.txt` con `permitir_prohibidas=1`. Para no depender de esa rama, `mazo.h` tiene
  una lectura propia en `constructor.h` (`constructor::aplicarOpcionProhibidas()`), que se relee al abrir el constructor y al repartir el duelo.
  - **Sí:** se pueden poner prohibidas y no cuentan limitada ni semilimitada (hasta 3 copias de todo; el mazo sigue
    siendo de 40 a 60). La colección muestra "PROHIBIDAS PERMITIDAS".
  - **No** (por defecto): se aplica la banlist Goat entera, y un mazo armado con Sí queda inválido y dice por qué.
  - La colección y el detalle muestran siempre el estado Goat de cada carta (PROHIBIDA, LIMITADA, SEMI). Con Sí, el
    detalle agrega "(Opciones: sin límite)".
  - Los mazos de inicio respetan siempre la banlist Goat.

### Mazos de inicio

La primera vez se arman cuatro mazos temáticos y válidos, sin cartas firma, para probar las mágicas de campo:

| Mazo | Campo | Extra |
|---|---|---|
| Dragones de la Montaña | Mountain ×3 | Gaia the Dragon Champion, Fiend Skull Dragon, Black Skull Dragon, Thousand Dragon |
| Océano Legendario | Umi ×2, A Legendary Ocean ×2 | Deepsea Shark, Humanoid Worm Drake, Roaring Ocean Snake |
| Sombras de Yami | Yami ×3 | Bickuribox, Dark Balter, Sanwitch, Skull Knight, Reaper on the Nightmare |
| Guerreros de Sogen | Sogen ×3 | Flame Swordsman, Karbonala Warrior, Giltia, Warrior of Tradition, Dark Blade the Dragon Knight |

Si a una lista le falta alguna carta (porque no está en la colección, es firma o pasaría el límite), se completa
hasta 40 con monstruos del mismo tema.

## 3. Guardado

La imagen del juego es de solo lectura, así que todo va a `/data/yugioh_classic/`. En el PC va a la carpeta de
`YUGI_MAZO`, o a `./`.

```
/data/yugioh_classic/mazos.txt          activo <n> y una línea "mazo <archivo>" por mazo
/data/yugioh_classic/mazos/mazo_<n>.txt cada mazo
/data/yugioh_classic/mazo.txt           copia del mazo activo (el formato de siempre)
```

Cada mazo usa el formato de `mazo.txt`, con una línea más para el nombre, que las versiones viejas ignoran:

```
# Yugioh Classic Remaster - mazo del jugador (principal 40, fusion 3)
nombre Océano Legendario
principal umi 2
principal a_legendary_ocean 2
fusion deepsea_shark 1
```

La primera vez, si existe un `mazo.txt`, se importa como "Mi mazo", y queda activo si es válido.

## 4. El duelo usa el mazo activo

`mazo_duelo.h::prepararMazos` carga la biblioteca y `tomarMazoActivo()` copia el mazo activo a `mazoJugador`, también
al cerrar el constructor. `repartir()` lo pasa a `mazoDuelo` si es válido; si no, usa el mazo inicial y lo anota en el
log. En `duelo_juego.h::arrancarDueloIA`, `dueloIA.empezar(mazoDuelo.principal, mazoDuelo.fusion, …)` copia el
principal a `l[0].mazo` y lo **baraja**, roba la mano de ahí, cada robo saca la carta de arriba, y `l[0].extra` son las
fusiones del mazo. No hizo falta tocar `duelo_juego.h`.

## 5. Pruebas

- `src/pruebas/prueba_constructor.cpp` hace lo siguiente sin pantalla:
  - la primera vez importa `mazo.txt` y arma los mazos de inicio, y comprueba que sean válidos, que tengan su campo y
    que el extra sean fusiones;
  - con los pulsos del mando, crea un mazo nombrado con el teclado, busca "umi", aplica los filtros Magia/Campo y
    prueba los límites (3 copias, prohibida, limitada, firma);
  - manda una fusión al extra, completa 40, quita una copia, sale con "Guardar y salir", lo usa en los duelos, lo
    copia, lo renombra y lo borra;
  - comprueba que un mazo inválido no se puede usar;
  - lo vuelve a cargar del disco (y desde `mazo.txt`);
  - con `ia::Duelo::empezar`, igual que en el juego, comprueba que mano + mazo = el mazo activo, que va barajado y que
    el extra son sus fusiones.

  ```
  g++ -std=c++17 -O2 -I remake-ps5/src remake-ps5/src/pruebas/prueba_constructor.cpp -o /tmp/prueba_constructor
  /tmp/prueba_constructor /work/PS5_ClassicRemaster/ps5/assets/ui/
  ```
- `captura_constructor.sh` saca capturas de la vista previa de PC. Arranca en el constructor con
  `YUGI_CONSTRUCTOR=1` y `YUGI_GUION`, que ahora también maneja el constructor.
