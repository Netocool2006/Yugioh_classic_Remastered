# Escenarios fotorrealistas (rama classic-escenarios)

El usuario vio los 16 entornos por rival hechos con siluetas por codigo y pidio "un castillo de verdad, como en
Demon's Souls", para TODOS los escenarios. Esta rama cambia como se hace el fondo: deja de ser geometria low-poly
dibujada en tiempo real y pasa a ser un **fondo pre-renderizado** (path tracing) que el motor reproyecta.

## Por que asi (y no Unreal ni mas poligonos en tiempo real)

- **Calidad**: lo que hace "real" un castillo es piedra fotoescaneada con relieve, luz global, sombras suaves,
  niebla con dispersion y luces calidas de antorchas. Eso, a 4K/120 Hz en la PS5 con el tablero, los monstruos y los
  efectos, no entra en el presupuesto si se calcula en vivo. Pre-renderizado cuesta lo mismo que un cielo: una
  muestra de textura por pixel.
- **Unreal 5.8** quedo descartado como renderizador: la biblioteca de Fab del usuario no tiene Megascans (solo el
  Niagara Examples Pack) y bajar assets de Fab requiere el launcher con sesion (intervencion del usuario); sin assets
  propios Unreal no aporta sobre Cycles, y en la GTX 1050 de 2 GB Lumen/MRQ es lento e inestable.
- **Blender 5.2 + Cycles** (D:\tools\blender-5.2.2, sin interfaz, OptiX en la 1050) con insumos **CC0 de Poly Haven**
  (HDRIs fotograficos, texturas PBR escaneadas de 2k y modelos): licencia sin restricciones para usarlos fuera de
  cualquier motor. Todo se arma por script (repetible) en `escenarios/<lugar>.py`.

## Como funciona

1. `escenarios/polyhaven.py` baja los insumos a `D:\Docker\compartido\classic-assets\entornos_src`.
2. `escenarios/<lugar>.py` arma la escena (coordenadas del juego: mesa de 26.9 x 20.2 m en el origen, suelo en
   y = -0.3) y `comun.renderizar()` deja en `classic-assets/entornos/pano/`:
   - `<clave>.jpg`: panorama equirectangular (8192 x 4096) visto desde la camara de juego (OJO), con tono AgX;
   - `<clave>_prof.png`: distancia desde ese punto por direccion (16 bits, log2(m)/12);
   - `<clave>.txt`: el origen y la lista de llamas (antorchas y braseros) en coordenadas del juego.
   `--modo vista` hace un solo cuadro con la proyeccion exacta del juego (columna de la interfaz incluida) para
   revisar la composicion en segundos; `ESC_CAMARA=x,y,z,mx,my,mz` mira desde otro lado.
3. El motor (`src/entornos.h`) lee los `.txt` al arrancar; si un entorno tiene fondo, no arma su obra por codigo
   (sin siluetas) y solo agrega las **llamas vivas** (las aditivas animadas de siempre) donde el render tiene
   antorchas. El panorama del entorno actual se carga al cambiar de entorno (uno a la vez en memoria, ~180 MB con
   mipmaps a 8K; `YUGI_PANO_MAX=4096` lo baja en el PC).
4. `shaders/entornos_cielo.frag` reproyecta: por cada pixel del fondo toma el rayo de la camara real y busca (5
   iteraciones de punto fijo) el punto del rayo que esta a la distancia guardada en su propia direccion del
   panorama. Desde la camara de juego es exacto; al moverse (CINE, lateral, libre) cada cosa tiene su paralaje real
   (el suelo y las paredes quedan quietos bajo la mesa). El cielo (sin distancia) queda en el infinito.
5. Las particulas y la luz del tablero (`Estilo`) siguen siendo del motor, acordes al render.

## Rendimiento

- GPU: el fondo es el mismo triangulo de pantalla de antes: 5 lecturas puntuales de la textura de distancias + 1
  lectura filtrada del color. Sin las siluetas por codigo hay menos triangulos que antes. Estimado en PS5 a 4K:
  < 0.3 ms (8.3 M pixeles, la mayoria tapados por la mesa).
- Memoria: un panorama cargado a la vez (8192 x 4096 RGBA8 + mipmaps ~ 180 MB, distancias 2048 x 1024 x 2 B = 4 MB).
- Disco: ~10-15 MB por entorno (jpg q92 + png16) -> ~250 MB los 16.
- Carga: ~0.3-1 s al cambiar de rival (stb_image + mipmaps por CPU), con el fondo ya fundido a negro.

## Licencias

Ver `third_party/LICENCIAS_entornos.md` (seccion "Fondos pre-renderizados"): todo CC0.
