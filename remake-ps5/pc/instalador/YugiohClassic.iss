; Yugioh Classic Remaster (Windows): instalador con asistente (Inno Setup 6.7).
; Se compila con armar_instalador.ps1 (define Origen, Salida y Version con /D). NO subir al repo la salida.
#ifndef Origen
  #define Origen "D:\Docker\compartido\pc\YugiohClassic"
#endif
#ifndef Salida
  #define Salida "D:\Docker\compartido\pc\instalador"
#endif
#ifndef Version
  #define Version "1.0"
#endif
#ifndef Compresion
  #define Compresion "lzma2/max"
#endif
#ifndef Solido
  #define Solido "yes"
#endif
#ifndef Corte
  #define Corte "2000000000"
#endif

[Setup]
AppId={{6B0F2C1E-5D3A-4C7B-9A41-3D5E7F8A9B0C}
AppName=Yugioh Classic Remaster
AppVersion={#Version}
AppPublisher=Proyecto personal (sin fines de lucro)
DefaultDirName={code:ElegirCarpeta}
DefaultGroupName=Yugioh Classic Remaster
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir={#Salida}
OutputBaseFilename=YugiohClassic_Instalador
Compression={#Compresion}
SolidCompression={#Solido}
DiskSpanning=yes
DiskSliceSize={#Corte}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
UninstallDisplayIcon={app}\YugiohClassic.exe
UninstallDisplayName=Yugioh Classic Remaster
CloseApplications=no
#ifdef Icono
SetupIconFile={#Icono}
#endif

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"; LicenseFile: "aviso_es.txt"
Name: "english"; MessagesFile: "compiler:Default.isl"; LicenseFile: "aviso_en.txt"

[Tasks]
Name: "escritorio"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#Origen}\YugiohClassic.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Origen}\SDL3.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#Origen}\LEEME.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Origen}\opciones_pc.txt"; DestDir: "{app}"; Flags: onlyifdoesntexist skipifsourcedoesntexist
Source: "{#Origen}\licencias\*"; DestDir: "{app}\licencias"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
; assets ya comprimidos (png/jpg/ogg/glb): sin compresion; el resto, con la compresion elegida
Source: "{#Origen}\datos\*"; Excludes: "*.png,*.jpg,*.ogg"; DestDir: "{app}\datos"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Origen}\datos\*.png"; DestDir: "{app}\datos"; Flags: ignoreversion recursesubdirs createallsubdirs nocompression
Source: "{#Origen}\datos\*.jpg"; DestDir: "{app}\datos"; Flags: ignoreversion recursesubdirs createallsubdirs nocompression
Source: "{#Origen}\datos\*.ogg"; DestDir: "{app}\datos"; Flags: ignoreversion recursesubdirs createallsubdirs nocompression

[Dirs]
Name: "{userappdata}\YugiohClassic"; Flags: uninsneveruninstall

[Icons]
Name: "{group}\Yugioh Classic Remaster"; Filename: "{app}\YugiohClassic.exe"; WorkingDir: "{app}"
Name: "{group}\Leeme"; Filename: "{app}\LEEME.txt"
Name: "{group}\Desinstalar Yugioh Classic Remaster"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Yugioh Classic Remaster"; Filename: "{app}\YugiohClassic.exe"; WorkingDir: "{app}"; Tasks: escritorio

[Run]
Filename: "{app}\YugiohClassic.exe"; Description: "{cm:LaunchProgram,Yugioh Classic Remaster}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[Code]
// Carpeta por defecto: D:\Yugioh Classic Remaster; si no hay D:, la unidad fija distinta de C: con mas espacio libre;
// si no hay ninguna, Archivos de programa del usuario.
function ElegirCarpeta(Param: String): String;
var
  c: Integer;
  libre, total, mejor: Int64;
  unidad: String;
begin
  Result := ExpandConstant('{autopf}\Yugioh Classic Remaster');
  if GetSpaceOnDisk64('D:\', libre, total) and (libre > 0) then
  begin
    Result := 'D:\Yugioh Classic Remaster';
    exit;
  end;
  mejor := 0;
  for c := 69 to 90 do
  begin
    unidad := Chr(c) + ':' + Chr(92);
    if GetSpaceOnDisk64(unidad, libre, total) and (libre > mejor) then
    begin
      mejor := libre;
      Result := unidad + 'Yugioh Classic Remaster';
    end;
  end;
end;

// El desinstalador conserva las partidas (%APPDATA%\YugiohClassic) salvo que el usuario lo pida (en modo silencioso: se conservan).
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  datos: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    datos := ExpandConstant('{userappdata}\YugiohClassic');
    if DirExists(datos) and (not UninstallSilent) then
      if MsgBox('Desea borrar tambien las partidas guardadas y la configuracion?' + #13#10 + datos + #13#10#13#10 +
                'Pulse No para conservarlas.', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(datos, True, True, True);
  end;
end;
