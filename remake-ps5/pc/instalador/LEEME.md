# Instalador de Windows (Inno Setup 6.7)

Archivos: `YugiohClassic.iss` (script), `armar_instalador.ps1` (armado), `aviso_es.txt` / `aviso_en.txt` (pantalla de licencia).
La salida (`.exe` + `.bin`) contiene arte de Konami y audio de Master Duel: **nunca al repo**; va a `D:\Docker\compartido\pc\instalador\`.

## Regenerar
```
powershell -NoProfile -ExecutionPolicy Bypass -File remake-ps5\pc\instalador\armar_instalador.ps1
```
- Por defecto empaca `D:\Docker\compartido\pc\YugiohClassic` (los enlaces duros de `datos\` se leen como archivos reales).
- Con el .exe nuevo (raton): `... -File remake-ps5\pc\instalador\armar_instalador.ps1 -Origen D:\Docker\compartido\pc\YugiohClassic_dev`
- Otros parametros: `-Salida`, `-Version`, `-Compresion` (por defecto `lzma2/max`), `-Solido`, `-ISCC`.
- Entrega: `YugiohClassic_Instalador.exe` + `YugiohClassic_Instalador-1.bin ... -3.bin` (DiskSpanning, cortes de 2 GB). **Hay que mantener todos juntos en la misma carpeta.**
- Se empacan: .exe, SDL3.dll, LEEME.txt, opciones_pc.txt, licencias\, datos\ (assets y shaders). No se empaca `en_linea.txt` (id anonimo personal).

## Comportamiento
- Carpeta por defecto `D:\Yugioh Classic Remaster` (si no hay D:, la unidad no-C con mas espacio libre; si no, Archivos de programa del usuario). Editable en el asistente.
- Sin administrador (`PrivilegesRequired=lowest`, con dialogo para elegir admin).
- Menu Inicio (juego, Leeme, Desinstalar) y escritorio opcional. Espanol e ingles.
- El desinstalador conserva `%APPDATA%\YugiohClassic` (partidas); solo lo borra si el usuario responde Si a la pregunta (en silencioso, siempre se conserva).
- Silencioso: `YugiohClassic_Instalador.exe /VERYSILENT /SUPPRESSMSGBOXES /DIR="D:\ruta" /TASKS=escritorio /LOG=inst.log`; `unins000.exe /VERYSILENT /SUPPRESSMSGBOXES`.

## Compresion
Los png/jpg/ogg (4.0 GB, ya comprimidos) van `nocompression`; el resto (glb, wav, shaders, exe) con lzma2/max solido. Armado ~2-3.5 min, instalacion ~2 min.
