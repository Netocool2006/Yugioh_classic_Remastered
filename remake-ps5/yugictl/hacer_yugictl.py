# Arma yugictl a partir del agente ps5vkctl de PS5_Vulkan (GPL-3.0, Mihawk-99), recortado para Yugioh Classic Remaster
# (autorizado por el usuario como programa residente temporal durante el desarrollo):
#  - solo actua sobre PPSA77002 (Yugioh Classic Remaster); cualquier otro titulo se rechaza
#  - SIN kill ni restart: nunca cierra ni mata nada (el juego se cierra solo con cerrar.txt)
#  - puerto 9113 (no choca con ps5vkctl, 9111, ni con el yugictl de Yugi Reload, 9112) y registro en /data/yugioh_classic (la carpeta del juego puede ser una imagen de solo lectura)
# Uso (en el contenedor arch-ps5): python3 hacer_yugictl.py /work/PS5_Vulkan/payload/ps5vkctl /work/classicctl  (el ELF: classicctl.elf, al USB)
# Compilar (contenedor): cd /work/classicctl && make PS5_PAYLOAD_SDK=/work/PS5_ClassicRemaster/.deps/native/ps5-payload-sdk \n#   OUTPUT=/work/classicctl/build ELF=/work/classicctl/build/classicctl.elf   -> copia en /compartido/classicctl/classicctl.elf
import shutil, sys
from pathlib import Path
origen, destino = Path(sys.argv[1]), Path(sys.argv[2])
if destino.exists():
    shutil.rmtree(destino)
shutil.copytree(origen, destino)
p = destino / "main.c"
s = p.read_text(encoding="utf-8")


def cambiar(a, b):
    global s
    assert a in s, a[:80]
    s = s.replace(a, b, 1)


cambiar("#define PS5VKCTL_PORT 9111", "#define PS5VKCTL_PORT 9113   /* yugictl: no choca con ps5vkctl */")
cambiar('#define PS5VKCTL_LOG "/data/ps5vkctl.log"', '#define PS5VKCTL_LOG "/data/yugioh_classic/yugictl.log"')
cambiar('''    if (strcmp(command, "launch") == 0)
    {''', '''    /* yugictl: solo Yugi Reload, y nunca cierra ni mata nada */
    if (strcmp(title, "PPSA77002") != 0)
    {
        reply("err yugictl solo abre PPSA77002");
        return;
    }
    if (strcmp(command, "kill") == 0 || strcmp(command, "restart") == 0)
    {
        reply("err yugictl no cierra titulos (el juego se cierra solo con cerrar.txt)");
        return;
    }
    if (strcmp(command, "launch") == 0)
    {''')
cambiar(" * PS5 Vulkan compatibility probe - resident control payload.",
        " * yugictl (Yugioh Classic Remaster): ps5vkctl recortado -- solo abre PPSA77002, sin kill ni restart, puerto 9113.\n"
        " * PS5 Vulkan compatibility probe - resident control payload.")
p.write_text(s, encoding="utf-8")
print("yugictl armado en", destino)
