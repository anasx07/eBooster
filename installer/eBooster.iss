; Script generated for Inno Setup 6
; eBooster Pro - Professional Gaming Network Optimizer & Anti-Lag Shield

#ifndef MyAppVersion
#define MyAppVersion "1.0.4"
#endif

#define MyAppName "eBooster Pro"
#define MyAppPublisher "anasx07"
#define MyAppURL "https://github.com/anasx07/eBooster"
#define MyAppExeName "eBooster.exe"

[Setup]
; App Metadata
AppId={{9F7B3E42-6B81-4C53-8C4E-7C189622A5E4}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases

; Install Directory & Group
DefaultDirName={autopf}\eBooster
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\dist
OutputBaseFilename=eBooster-Setup
SetupIconFile=..\assets\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}

; Compression & Engine
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
DisableDirPage=no
DisableProgramGroupPage=yes

; Architecture & Permissions
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog

; Close application before installing/uninstalling
CloseApplications=yes
CloseApplicationsFilter=*{#MyAppExeName}*

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce
Name: "startmenuicon"; Description: "Create a Start Menu shortcut"; GroupDescription: "{cm:AdditionalIcons}"; Flags: checkedonce
Name: "startupicon"; Description: "Run eBooster automatically when Windows starts"; GroupDescription: "Automation:"; Flags: unchecked

[Files]
Source: "..\dist\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\app.ico"; DestDir: "{app}\assets"; Flags: ignoreversion
Source: "..\assets\logo.png"; DestDir: "{app}\assets"; Flags: ignoreversion
Source: "..\configs\*"; DestDir: "{app}\configs"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\ui\dist\*"; DestDir: "{app}\ui\dist"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\assets\app.ico"; Tasks: startmenuicon
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\assets\app.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
Filename: "schtasks.exe"; Parameters: "/create /tn ""eBooster"" /tr ""\""{app}\{#MyAppExeName}\"" --minimized"" /sc onlogon /rl highest /f"; Flags: runhidden; Tasks: startupicon

[UninstallRun]
Filename: "schtasks.exe"; Parameters: "/delete /tn ""eBooster"" /f"; Flags: runhidden; RunOnceId: "RemoveStartupTask"

[UninstallDelete]
Type: filesandordirs; Name: "{app}\cache"
Type: files; Name: "{app}\eBooster_update.exe"
Type: files; Name: "{app}\eBooster.old.exe"
