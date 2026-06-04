#define MyAppName "LUFS Meter Plus"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "LUFS Meter Plus"
#define BuildDir "C:\LocalBuild\LufsMeterPlus\LufsMeterPlus_artefacts\Release"

[Setup]
AppId={{B3A2F1C0-4D5E-4F6A-8B7C-9D0E1F2A3B4C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=.
OutputBaseFilename=LUFS_Meter_Plus_Setup_{#MyAppVersion}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "전체 설치 (VST3 + 독립 실행형)"
Name: "vst3only"; Description: "VST3 플러그인만"
Name: "standaloneonly"; Description: "독립 실행형(Standalone)만"
Name: "custom"; Description: "사용자 지정"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 플러그인"; Types: full vst3only custom; Flags: fixed
Name: "standalone"; Description: "독립 실행형 앱 (Standalone)"; Types: full standaloneonly custom

[Files]
; VST3 plugin (directory structure must be preserved)
Source: "{#BuildDir}\VST3\LUFS Meter Plus.vst3\*"; DestDir: "{commoncf64}\VST3\LUFS Meter Plus.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

; Standalone executable
Source: "{#BuildDir}\Standalone\LUFS Meter Plus.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\LUFS Meter Plus.exe"; Components: standalone
Name: "{group}\제거"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\LUFS Meter Plus.exe"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Components: standalone
