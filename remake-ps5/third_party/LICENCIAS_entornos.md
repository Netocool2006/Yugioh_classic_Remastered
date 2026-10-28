# Entornos por rival (src/entornos.h): insumos de terceros

Todas las texturas son **CC0 1.0** de [Poly Haven](https://polyhaven.com/) (1k: color, normal GL, rugosidad y
oclusion), bajadas de su API oficial por `entornos_insumos.py` y preparadas como capas de 1024x1024. No van al repo
(quedan en `<classic-assets>/entornos/`).

castle_brick_07, cobblestone_floor_04, sandstone_blocks_08, aerial_sand, metal_plate, marble_01, red_brick_03,
concrete_floor_02, rock_face_03, wood_floor_deck, aerial_grass_rock, bark_brown_02, forest_leaves_02, granite_tile,
large_sandstone_blocks.

Todo lo demas (siluetas, cielos, agua, llamas, particulas) se hace por codigo.

## Fondos pre-renderizados (rama classic-escenarios, `escenarios/*.py`, docs/escenarios.md)

Todo **CC0 1.0** de [Poly Haven](https://polyhaven.com/license), bajado por `escenarios/polyhaven.py` a
`classic-assets/entornos_src/` (no va al repo). En el juego solo entra el render (`classic-assets/entornos/pano/`).

- HDRIs: belfast_sunset_puresky, canary_wharf, shanghai_bund, stadium_01, museum_of_ethnography, urban_street_04,
  mossy_forest, kloppenheim_04, umhlanga_sunrise, the_sky_is_on_fire, goegap, rogland_moonlit_night,
  kloppenheim_06_puresky, kloppenheim_02.
- Texturas: stone_wall_05, medieval_blocks_03, stone_brick_wall_001, castle_wall_slates, castle_wall_varriation,
  floor_pattern_01, grey_roof_tiles_02, medieval_wood, concrete_pavers, concrete_floor_worn_001, grass_path_2,
  marble_tiles, marble_01, sandstone_blocks_08, sandstone_cracks, rock_wall_10, rock_ground, sand_01, metal_plate,
  rusty_metal_02, wood_floor_deck, forest_ground_04, asphalt_02.
- Modelos: modular_fort_01, gothic_statue, wooden_barrels_01, wooden_crate_02, wine_barrel_01, street_lamp_01,
  boulder_01, rock_moss_set_02, fern_02, dead_tree_trunk, namaqualand_boulder_04.

Descartado: Megascans/Fab (la biblioteca del usuario no los tiene y bajarlos requiere el launcher con sesion).
