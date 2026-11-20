; Instalador de Sora (Inno Setup 6.3 o posterior). Instalación por usuario, sin permisos de administrador.
; Se compila con: ISCC /DAppVersion=x.y.z /DSourceDir=<carpeta con sora.exe> /DOutputDir=<salida> sora.iss
; Actualizar: borra la versión anterior del programa y copia la nueva. No instala encima de una
; versión más nueva. La autoactualización llega en el Hito 7.

#if Ver < EncodeVer(6, 3, 0)
  #error Hace falta Inno Setup 6.3 o posterior
#endif

#ifndef AppVersion
  #error Falta /DAppVersion=x.y.z (scripts\build-windows.ps1 la toma de CMakeLists.txt)
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\Sora"
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif

[Setup]
AppId={{6F1C3B8E-5A2D-4C7E-9B41-2E8D0F3A7C55}
AppName=Sora
AppVersion={#AppVersion}
AppPublisher=Sora
DefaultDirName={localappdata}\Programs\Sora
; Carpeta fija: [InstallDelete] la vacía al actualizar, así que nunca puede ser una que elija el usuario.
DisableDirPage=yes
UsePreviousAppDir=no
DefaultGroupName=Sora
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename=Sora-{#AppVersion}-instalador
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\sora.exe
SetupIconFile=..\assets\sora.ico
WizardStyle=modern

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; Flags: unchecked

[InstallDelete]
; Quita la versión anterior antes de copiar la nueva: no quedan archivos que ya no se usan.
; Los datos del usuario (historial, cookies, listas) están en otra carpeta y no se tocan.
Type: filesandordirs; Name: "{app}\*"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Sora"; Filename: "{app}\sora.exe"
Name: "{autodesktop}\Sora"; Filename: "{app}\sora.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\sora.exe"; Description: "Abrir Sora"; Flags: nowait postinstall skipifsilent

[Code]
const
  UninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{6F1C3B8E-5A2D-4C7E-9B41-2E8D0F3A7C55}_is1';

// No deja instalar una versión anterior encima de una más nueva (la misma versión sí: reinstalar).
function InitializeSetup(): Boolean;
var
  Installed: String;
  InstalledPacked, NewPacked: Int64;
begin
  Result := True;
  if not RegQueryStringValue(HKCU, UninstallKey, 'DisplayVersion', Installed) then
    Exit;
  if not StrToVersion(Installed, InstalledPacked) or not StrToVersion('{#AppVersion}', NewPacked) then
    Exit;
  if ComparePackedVersion(InstalledPacked, NewPacked) > 0 then
  begin
    SuppressibleMsgBox('Ya tienes instalada una versión más nueva de Sora (' + Installed + ').' + #13#10 +
      'Este instalador es de la versión {#AppVersion}.' + #13#10#13#10 +
      'Si de verdad quieres volver a la anterior, desinstala Sora primero. Tus datos se conservan.',
      mbError, MB_OK, IDOK);
    Result := False;
  end;
end;
