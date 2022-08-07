#define MyAppName "WebServer"
#define MyAppVersion "1.0"
#define MyAppURL "https://github.com/ianfun/Win32-static-file-Server"

[Setup]
AppId={{A5BC8B92-7569-4471-89E1-80444DA2C135}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonpf64}\{#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=HTTPServer_setup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
DisableDirPage=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "armenian"; MessagesFile: "compiler:Languages\Armenian.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "bulgarian"; MessagesFile: "compiler:Languages\Bulgarian.isl"
Name: "catalan"; MessagesFile: "compiler:Languages\Catalan.isl"
Name: "corsican"; MessagesFile: "compiler:Languages\Corsican.isl"
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "danish"; MessagesFile: "compiler:Languages\Danish.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "finnish"; MessagesFile: "compiler:Languages\Finnish.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "hebrew"; MessagesFile: "compiler:Languages\Hebrew.isl"
Name: "icelandic"; MessagesFile: "compiler:Languages\Icelandic.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "norwegian"; MessagesFile: "compiler:Languages\Norwegian.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "slovak"; MessagesFile: "compiler:Languages\Slovak.isl"
Name: "slovenian"; MessagesFile: "compiler:Languages\Slovenian.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\..\static\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs
Source: "./x64/Release/installer.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "./x64/Release/service.exe"; DestDir: "{app}"; Flags: ignoreversion

[Run]
Filename: "{app}\installer.exe"; Flags: 64bit skipifsilent runascurrentuser; Parameters: "{code:serviceMode}"

[UninstallRun]
Filename: "sc delete HTTPServer"; Flags: runascurrentuser; RunOnceId: "DeleteService"

[Code]
const
  NTText =
    'Run as "nt authority/system" service.\nThis is use the system account when creating child process.No login is required in this option, and child process will use system environments.';
  UserText =
    'Login via UI.Run as user service.\nThis requires login you account, and use your acount when creating child process.';
  CmdText =
    'Login via cmd(command line).Same as login via UI, but use windows command prompt for login.';
var
  UserBtn: TNewRadioButton;
  NTBtn: TNewRadioButton;
  CmdBtn: TNewRadioButton;

procedure InitializeWizard;
var
  myPage: TWizardPage;
  NTLable: TLabel;
  userLable: TLabel;
  CmdLable: TLabel;
begin
  myPage := CreateCustomPage(wpSelectTasks, 'Service account', 'The service account will effect when the service creating process.We recommend login via dialog to creating process in your account.');
  
  UserBtn := TNewRadioButton.Create(WizardForm);
  UserBtn.Parent := myPage.Surface;
  UserBtn.Checked := True;
  UserBtn.Top := 20;
  UserBtn.Width := myPage.SurfaceWidth;
  UserBtn.Font.Style := [fsBold];
  UserBtn.Font.Size := 9;
  UserBtn.Caption := 'Login via UI Dialog(Recommend)'
  userLable := TLabel.Create(WizardForm);
  userLable.Parent := myPage.Surface;
  userLable.Left := 8;
  userLable.Top := UserBtn.Top + UserBtn.Height + 20;
  userLable.Width := myPage.SurfaceWidth; 
  userLable.Height := 40;
  userLable.AutoSize := False;
  userLable.Wordwrap := True;
  userLable.Caption := UserText;

  NTBtn := TNewRadioButton.Create(WizardForm);
  NTBtn.Parent := myPage.Surface;
  NTBtn.Top := userLable.Top + userLable.Height + 20;
  NTBtn.Width := myPage.SurfaceWidth;
  NTBtn.Font.Style := [fsBold];
  NTBtn.Font.Size := 9
  NTBtn.Caption := 'Use NT AUTHORITY\system account.'
  NTLable := TLabel.Create(WizardForm);
  NTLable.Parent := myPage.Surface;
  NTLable.Left := 8;
  NTLable.Top := NTBtn.Top + NTBtn.Height + 20;
  NTLable.Width := myPage.SurfaceWidth;
  NTLable.Height := 60;
  NTLable.AutoSize := False;
  NTLable.Wordwrap := True;
  NTLable.Caption := NTText;

  CmdBtn := TNewRadioButton.Create(WizardForm);
  CmdBtn.Parent := myPage.Surface;
  CmdBtn.Top := NTLable.Top + NTLable.Height + 20;
  CmdBtn.Width := myPage.SurfaceWidth;
  CmdBtn.Font.Style := [fsBold];
  CmdBtn.Font.Size := 9;
  CmdBtn.Caption := 'Login via cmd.'
  CmdLable := TLabel.Create(WizardForm);
  CmdLable.Parent := myPage.Surface;
  CmdLable.Left := 8;
  CmdLable.Top := CmdBtn.Top + CmdBtn.Height + 20 ;
  CmdLable.Width := myPage.SurfaceWidth;
  CmdLable.Height := 40;
  CmdLable.AutoSize := False;
  CmdLable.Wordwrap := True;
  CmdLable.Caption := CmdText;
end;

function serviceMode(Param: String): String;
  begin
  if UserBtn.Checked then
      Result := 'user'
  else
  if CmdBtn.Checked then
      Result := 'cmd'
  else
      Result := 'admin'
end;
