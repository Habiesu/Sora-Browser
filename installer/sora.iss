; Instalador de Sora (Inno Setup 6). Instalación por usuario, sin permisos de administrador.
; Se compila con: ISCC /DAppVersion=0.1.0 /DSourceDir=<carpeta con sora.exe> /DOutputDir=<salida> sora.iss
; La autoactualización llega en el Hito 7.

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
AppId={{6F1C3B8E-5A2D-4C7E-9B41-2E8D0F3A7C55}
AppName=Sora
AppVersion={#AppVersion}
AppPublisher=Sora
DefaultDirName={localappdata}\Programs\Sora
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
WizardStyle=modern

[Languages]
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "Crear un acceso directo en el escritorio"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Sora"; Filename: "{app}\sora.exe"
Name: "{autodesktop}\Sora"; Filename: "{app}\sora.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\sora.exe"; Description: "Abrir Sora"; Flags: nowait postinstall skipifsilent
