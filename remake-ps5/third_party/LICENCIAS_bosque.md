# Bosque realista (carta de campo Bosque): fuentes, licencias e ideas

Los insumos binarios NO van al repo. `remake-ps5/bosque_insumos.py` los descarga a
`D:\Yugi-Reload\terceros\bosque\descargas` y deja lo preparado en `<assets>/bosque`
(`D:\Docker\compartido\yugi-src\assets\bosque`), que `sincronizar.sh` copia al titulo.
Para rehacerlos: `python -I remake-ps5/bosque_insumos.py` (Pillow, numpy y Blender 5.2.2 en D:\tools).

## Insumos usados (todos CC0 1.0, dominio publico: uso comercial libre, sin atribucion obligatoria)

| Insumo | Uso en el juego | Fuente | Licencia |
|---|---|---|---|
| bark_brown_02 (color, normal GL, rugosidad, AO, 1k) | corteza de las hayas, arbustos y raices | https://polyhaven.com/a/bark_brown_02 | CC0 |
| pine_bark (1k) | corteza de los abetos | https://polyhaven.com/a/pine_bark | CC0 |
| forrest_ground_01 (1k) | suelo base (tierra, pasto ralo, ramitas) | https://polyhaven.com/a/forrest_ground_01 | CC0 |
| forest_leaves_02 (1k) | manchas de hojarasca del suelo | https://polyhaven.com/a/forest_leaves_02 | CC0 |
| Moss002 (1K-PNG) | musgo del suelo y encima de rocas/troncos/raices | https://ambientcg.com/view?id=Moss002 | CC0 |
| LeafSet024 (1K-PNG) | hojas de haya: se recortan y se arman ramilletes 2x2 con alfa y normal | https://ambientcg.com/view?id=LeafSet024 | CC0 |
| LeafSet019 (1K-PNG) | ramitas de abeto: ramilletes de las ramas de los abetos | https://ambientcg.com/view?id=LeafSet019 | CC0 |
| LeafSet017 (1K-PNG) | hojas de hiedra para los arbustos | https://ambientcg.com/view?id=LeafSet017 | CC0 |
| fern_02 (gltf 1k) | helecho (2 variantes, reducido a ~500 triangulos) | https://polyhaven.com/a/fern_02 | CC0 |
| rock_moss_set_01 (gltf 1k) | rocas con musgo (2 variantes, ~400 triangulos) | https://polyhaven.com/a/rock_moss_set_01 | CC0 |
| tree_stump_01 (gltf 1k) | tocon (~1600 triangulos) | https://polyhaven.com/a/tree_stump_01 | CC0 |
| dead_tree_trunk (gltf 1k) | tronco caido (~2400 triangulos) | https://polyhaven.com/a/dead_tree_trunk | CC0 |

Poly Haven: https://polyhaven.com/license (CC0). ambientCG: https://docs.ambientcg.com/license/ (CC0).

## Revisado y descartado

- Arboles completos de Poly Haven (fir_tree_01, pine_tree_01, island_tree_0x, tree_small_02): CC0 pero de
  0.3 a 17 millones de triangulos; reducirlos rompe el follaje. Se usan sus mismas tecnicas con tarjetas.
- Quaternius / Kenney (CC0): estilizados (low poly), justo lo que se queria dejar atras.
- Blender Sapling Tree Gen (GPL, extension de Blender), tree-gen, proctree.js (MIT): generadores de ramas;
  se tomo la idea, no el codigo.

## Ideas tomadas (sin codigo copiado)

- ez-tree de Dan Greenheck (MIT, https://github.com/dgreenheck/ez-tree): ramas por niveles con secciones
  que se tuercen (nudoso) y buscan la luz, hijos repartidos con angulo de oro, follaje en tarjetas.
- Tarjetas de ramilletes con alfa y normales "de copa" (esfericas) para iluminar la copa como volumen
  (tecnica habitual de SpeedTree y de los juegos de mundo abierto).
- Mipmaps de alfa que conservan la cobertura (I. Castano, "Computing Alpha Mipmaps"), para que el follaje
  no se adelgace de lejos con prueba de alfa (sin MSAA no hay alpha-to-coverage).
- Normal mapping sin tangentes guardadas (C. Schuler, "Followup: Normal Mapping Without Precomputed
  Tangents", 2013).
- Viento por vertice en tres capas (arbol, rama, temblor de hoja) como en GPU Gems 3 cap. 16 (Crytek).
- Niebla con la distancia tenida del sol al mirar hacia el; luz moteada bajo las copas; translucidez de hojas.
- Anillo lejano de arboles livianos que se pierde en la niebla en lugar de impostores (barato con pocos arboles).
