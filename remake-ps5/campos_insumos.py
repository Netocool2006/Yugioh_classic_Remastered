# Yugioh Classic Remaster (rama classic-campos): texturas CC0 de Poly Haven para la superficie del tablero de cada carta
# de campo (src/campos.h). Mismo formato que entornos_insumos.py pero a 512x512 (el tablero se ve de lejos y se repite
# cada ~2.5 m: sobra resolucion y cuesta 4 veces menos memoria): <assets>/campos/<nombre>_color.png (color + oclusion)
# y <nombre>_normal.png (normal GL + rugosidad). Las descargas (1k jpg) quedan en D:\Yugi-Reload\terceros\entornos\
# descargas (compartidas con entornos). Nada va al repo. Licencias: third_party/LICENCIAS_campos.md.
# Uso: python -I campos_insumos.py [--descargas DIR] [--assets DIR]
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import entornos_insumos as ei  # noqa: E402

ei.TAM = 512

# capa del juego -> textura de Poly Haven (el ORDEN es el de campos::Capa en src/campos.h)
MATERIALES = [
    ("cesped", "leafy_grass"),               # Sogen, Poder de Gaia
    ("roca", "rocky_terrain_02"),            # Montana, Poder de Gaia
    ("grieta", "mud_cracked_dry_03"),        # Paramo
    ("pizarra", "dark_rock"),                # Yami, Santuario oscuro, Pandemonium, Oricalco, plasma
    ("arena", "aerial_sand"),                # Necrovalle (arena que cubre las losas)
    ("losa", "large_sandstone_blocks"),      # Necrovalle (losas con relieves)
    ("marmol", "marble_tiles"),              # santuarios, Chispa luminosa, Formacion de luz
    ("quemado", "burned_ground_01"),         # Destruccion fundida (entre las grietas de lava)
    ("metal", "metal_plate"),                # Puerta de fusion, Campo centrifugo, Castillo dorado
    ("piedramar", "seaworn_stone_tiles"),    # Oceano legendario (ruinas bajo el agua)
    ("selva", "leaves_forest_ground"),       # Mundo jurasico
    ("acantilado", "rock_face_03"),          # Coto de caza de las arpias
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--descargas", default=r"D:\Yugi-Reload\terceros\entornos\descargas")
    ap.add_argument("--assets", default=r"D:\Docker\compartido\classic-assets")
    a = ap.parse_args()
    ei.MATERIALES = MATERIALES
    ei.bajar_polyhaven(a.descargas)
    ei.preparar(a.descargas, os.path.join(a.assets, "campos"))
    print("listo:", len(MATERIALES), "materiales en", os.path.join(a.assets, "campos"))


if __name__ == "__main__":
    sys.exit(main())
