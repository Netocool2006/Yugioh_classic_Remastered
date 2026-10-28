# Imagen de disco del titulo (PPSA77002.exfat)

Subir la carpeta del titulo (≈11.000 archivos, 3,6 GB) por FTP tarda ~3 h porque ftpsrv se cuelga cada
250-300 archivos. En su lugar el titulo se distribuye como **un solo archivo exFAT** que ShadowMount+ monta.

## Que hace ShadowMount+ con el homebrew (evidencia, oct 2026)

Consola: PS5 Pro FW 13.00, `ShadowMountPlus_1.7-beta5fix1.elf` (lo lanza el Payload Manager al arrancar).
Archivos: `/data/shadowmount/config.ini`, `debug.log`, `manual.lst`; API web en `http://<ps5>:10101`.

- **Escanea** cada 15 s `/data/homebrew` y `/mnt/usbN/homebrew` (y `/mnt/usbN`), con `scan_depth=2` en este
  config (lo puso BlackBox). Un archivo/carpeta debe llevar 10 s sin cambiar antes de tomarse (`stability_wait`).
- **Monta imagenes** `.exfat` (exFAT), `.ffpkg` (UFS) y `.ffpfsc` (PFS comprimido con un exFAT dentro) en
  `/mnt/shadowmnt/<nombre>_<hash>` por LVD, de solo lectura (`mount_read_only=1`). Con
  `persistent_image_mounts=1` quedan montadas siempre. Del log:
  `[IMG] Mounting image (exfatfs, optimized-1.7): /data/homebrew/PPSA04609.exfat -> /mnt/shadowmnt/PPSA04609_57048760`.
  El perfil `optimized-1.7` (via rapida LVD img_type=5) exige imagen en el disco interno y **cluster de 64 KiB**;
  si no, usa `legacy-1.6` (p. ej. la misma imagen en USB).
- **Registra homebrew**, no solo juegos: lo que necesita es `eboot.bin` + `sce_sys/param.json`.
  - Carpeta: nuestro `/data/homebrew/PPSA77002` ya esta instalado asi:
    `[ACTION] Installing: Yugioh Classic Remaster (PPSA77002)` /
    `[LINK] nullfs mounted: /data/homebrew/PPSA77002 -> /system_ex/app/PPSA77002` / `NOTIFY: installed game PPSA77002`.
  - Imagen: `Overdrive-PPSA99058.exfat` (un homebrew con eboot.bin, sce_module, sce_sys y .elf) esta en
    `/mnt/usb0/Homebrew` y se monta como exFAT en `/mnt/shadowmnt/Overdrive-PPSA99058_744153ad`.
- **Un title id = una fuente.** Si dos fuentes traen el mismo title id gana la que ya estaba y la otra se ignora:
  `NOTIFY: Duplicado PPSA99058 ignorado: /mnt/shadowmnt/... existente: /mnt/shadowmnt/...`.
- Al lanzar el juego monta en su sandbox `/data`, `/mnt/usb0` y las fuentes, y aplica el fakelib en modo
  `emulators` (`fakelib_default_mode=emulators`).

## Formato elegido

`.exfat` de cluster 64 KiB (perfil `optimized-1.7`), sin comprimir: es lo mas simple, monta como las otras imagenes
que ya funcionan y no necesita herramientas de Sony. El `.ffpfsc` solo ahorraria espacio (los assets ya son
PNG/OGG/GLB) y el `.ffpkg` (UFS) no tiene escritor libre en Windows.

Con 11.126 archivos y 4,13 GB de datos (19-oct, con los 63 fondos) la imagen pesa **4,50 GB** (el resto es relleno de cluster).

## Como se genera

```
python -I remake-ps5\imagen_titulo.py --verificar
```

- Lee `D:\Docker\compartido\ps5\subida\PPSA77002` (o `...\ps5\PPSA77002`), o la carpeta que se le pase, y escribe
  `D:\Docker\compartido\ps5\PPSA77002.exfat` (primero como `.tmp`, se renombra al terminar).
- Escritor exFAT propio en Python (sin montar, sin admin), misma disposicion que el `PkgExfat.cs` que ya produjo
  imagenes que montan en la consola: FAT en el sector 128, archivos contiguos, tabla upcase estandar.
  Fechas fijas: misma carpeta = misma imagen.
- Omite `radv-shader-cache`, `capturas` y archivos ocultos/`.upload`.
- `--verificar` relee la imagen con un lector propio (arranque, FAT, checksums de entradas) y compara cada archivo.
- Comprobado tambien con `fsck.exfat -n` (exfatprogs 1.4.3, contenedor arch-ps5): `clean`.
  `docker exec arch-ps5 fsck.exfat -n /compartido/ps5/PPSA77002.exfat`
- Tarda ~7-10 min en este PC (o dentro del contenedor, con `flock /compartido/.compilar.lock` para que otra compilacion no cambie el dist a medias: `python3 -I imagen_titulo.py /compartido/ps5/PPSA77002 --salida /compartido/ps5/PPSA77002.exfat --verificar`) (lo limita leer 11.000 archivos de D:). Si la carpeta cambia mientras se lee (p. ej. un rsync en curso) se detiene: esperar a que termine la copia y repetir.

## Como se sube

```
python -I remake-ps5\subir_imagen.py            # D:\Docker\compartido\ps5\PPSA77002.exfat
```

- Sube a `/data/homebrew/PPSA77002.exfat.part` y renombra al terminar (ShadowMount no toma un `.part`, asi no
  intenta montar una imagen a medias si ftpsrv se cuelga mas de 10 s).
- Reanudable con REST: si se corta, se vuelve a ejecutar y sigue. Probado en la consola: STOR con REST continua el
  archivo y el contenido queda identico.
- Velocidad medida (300 MB, un archivo): **10-11 MB/s → ~7 min la imagen completa de ~4,5 GB** (frente a ~3 h por carpeta).
- Si ya hay una `PPSA77002.exfat`, la renombra a `PPSA77002.exfat.anterior` (no borra nada).
- Ojo: ftpsrv responde `226` a `DELE`, y `ftplib.delete` lo toma por error aunque borra; usar `sendcmd("DELE ...")`.

## Primera vez: pasar de carpeta a imagen (lo hace el usuario, con la consola delante)

Hoy PPSA77002 esta registrado desde la **carpeta** `/data/homebrew/PPSA77002`. Mientras exista, la imagen se
montaria pero se ignoraria como duplicado. Plan:

1. Cerrar el juego.
2. Sacar la carpeta de las rutas que escanea ShadowMount (con `scan_depth=2` renombrarla dentro de
   `/data/homebrew` no basta). Por FTP: renombrar `/data/homebrew/PPSA77002` a `/data/yugioh_classic_carpeta_vieja`
   (RNFR/RNTO, instantaneo) o borrarla. Si tenia `idioma.txt`, copiarlo antes a `/data/yugioh_classic/`.
3. `python -I remake-ps5\subir_imagen.py`.
4. Esperar ~30 s y mirar `/data/shadowmount/debug.log` (o el klog del puerto 3232). Debe salir:
   `[IMG] Mounting image (exfatfs, optimized-1.7): /data/homebrew/PPSA77002.exfat -> /mnt/shadowmnt/PPSA77002_xxxxxxxx`
   y, al registrar o abrir, `[LINK] nullfs mounted: /mnt/shadowmnt/PPSA77002_xxxxxxxx -> /system_ex/app/PPSA77002`.
   Por FTP se puede listar `/mnt/shadowmnt/PPSA77002_*` y ver eboot.bin, sce_sys, assets...
5. Abrir el juego desde el menu. Si el icono no aparece o queda apuntando a la carpeta vieja: en
   `http://<ps5>:10101` pedir un escaneo, o reiniciar la consola (ShadowMount re-sincroniza al arrancar).

## Como se actualiza

- Cambio de codigo o de assets: recompilar, `imagen_titulo.py`, `subir_imagen.py` (~6-7 min), cerrar el juego
  antes de subir. La imagen vieja queda como `.anterior` hasta que se borre a mano.
- Partirla en dos (codigo+ui pequeno / assets pesados) no compensa: ShadowMount registra una fuente por title id, y
  la imagen completa ya sube en minutos. Si algun dia los assets crecen mucho, la opcion seria una imagen de assets
  sin title id que el juego lea de `/mnt/shadowmnt/...`, pero hoy no hace falta.

## La imagen es de solo lectura: el juego escribe en /data/yugioh_classic/

ShadowMount monta la imagen RO (`mount_read_only=1`). Desde la rama `classic-imagen` todo lo que el juego escribe va a
`/data/yugioh_classic/` (la crea al arrancar, permisos 0777; `/data` esta montado en el sandbox del juego):

| archivo | antes (carpeta del juego) | ahora |
|---|---|---|
| mazo, progreso, historia | ya estaban en `/data/yugioh_classic/` | igual |
| `idioma.txt` | `/data/homebrew/PPSA77002/idioma.txt` | `/data/yugioh_classic/idioma.txt` (si no existe, lee una vez el viejo) |
| `capturas.txt` (pedido de fotos) | carpeta del juego | `/data/yugioh_classic/capturas.txt` (tambien acepta el viejo) |
| `capturas/` (fotos, video) | carpeta del juego | `/data/yugioh_classic/capturas/` |
| `cerrar.txt` | carpeta del juego | cualquiera de las dos |
| cache de shaders RADV | `/app0/radv-shader-cache` | `/data/yugioh_classic/radv-shader-cache` |
| registro de yugictl/classicctl | `/data/homebrew/PPSA77002/yugictl.log` | `/data/yugioh_classic/yugictl.log` (al recompilar classicctl) |

La cache de RADV: `radv_ps5_platform.c` hace `setenv("MESA_SHADER_CACHE_DIR", "/app0/radv-shader-cache", 0)` (sin
pisar una ya puesta); `yugireload.cpp` la fija antes, en un inicializador estatico, a la carpeta de datos (y
`PS5VK_SHADER_CACHE_DIR` por si se usa ps5vk). La primera vez tras el cambio se recompilan los shaders (la cache
vieja quedo en la carpeta del juego).

`fotos_ps5.py` y `esperar_video_ps5.py` leen de `/data/yugioh_classic/capturas` (y, si no existe, de la carpeta del
juego, para builds viejos). Para pedir fotos: subir `capturas.txt` a `/data/yugioh_classic/`.
`ciclo_ps5.sh` sigue subiendo por carpeta (vaciar + subir_ps5.py): con la imagen, el ciclo seria
`imagen_titulo.py` + `subir_imagen.py`.
