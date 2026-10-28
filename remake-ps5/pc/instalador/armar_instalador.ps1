# Yugioh Classic Remaster: arma el instalador de Windows (Inno Setup 6.7) a partir de una carpeta de salida del juego.
# Uso:  powershell -File armar_instalador.ps1 [-Origen <carpeta>] [-Salida <carpeta>] [-Version 1.0]
#   Origen por defecto: D:\Docker\compartido\pc\YugiohClassic  (con el .exe nuevo con raton: -Origen D:\Docker\compartido\pc\YugiohClassic_dev)
#   Salida por defecto: D:\Docker\compartido\pc\instalador  (YugiohClassic_Instalador.exe + .bin)
# Los datos\ del origen pueden ser enlaces duros: Inno lee los archivos reales. La salida NO va al repo (arte de Konami).
param(
	[string]$Origen = 'D:\Docker\compartido\pc\YugiohClassic',
	[string]$Salida = 'D:\Docker\compartido\pc\instalador',
	[string]$Version = '1.0',
	[string]$Icono = 'D:\Docker\compartido\pc\yugioh.ico',  # el .ico que genera armar_pc.sh (docker cp arch-ps5:/work/PC_ClassicRemaster/icono/yugioh.ico); opcional
	[string]$Compresion = 'lzma2/max',
	[string]$Solido = 'yes',
	[string]$ISCC = 'D:\Herramientas\InnoSetup\ISCC.exe'
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path "$Origen\YugiohClassic.exe")) { throw "falta $Origen\YugiohClassic.exe" }
if (-not (Test-Path "$Origen\datos\assets")) { throw "falta $Origen\datos\assets" }
if (-not (Test-Path $ISCC)) { throw "falta $ISCC" }
if ($Salida -like 'C:*') { throw 'la salida no debe ir en C: (regla del usuario)' }
New-Item -ItemType Directory -Force $Salida | Out-Null
Get-ChildItem $Salida -Filter 'YugiohClassic_Instalador*' -ErrorAction SilentlyContinue | Remove-Item -Force
$iss = Join-Path $PSScriptRoot 'YugiohClassic.iss'
$t = Get-Date
$defIcono = @(); if ($Icono -and (Test-Path $Icono)) { $defIcono = @("/DIcono=$Icono") }
& $ISCC @defIcono "/DOrigen=$Origen" "/DSalida=$Salida" "/DVersion=$Version" "/DCompresion=$Compresion" "/DSolido=$Solido" $iss
if ($LASTEXITCODE -ne 0) { throw "ISCC fallo ($LASTEXITCODE)" }
"Armado en {0:N1} min" -f ((Get-Date) - $t).TotalMinutes
Get-ChildItem $Salida -Filter 'YugiohClassic_Instalador*' | ForEach-Object {
	"{0}  {1:N1} MB  SHA-256 {2}" -f $_.FullName, ($_.Length / 1MB), (Get-FileHash $_.FullName -Algorithm SHA256).Hash
}
