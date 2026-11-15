// ----------------------------------------------------------------------------
//
// Based on modpath.iss, Script Version 1.4.2
// Author:     Jared Breland <jbreland@legroom.net>
// Homepage:   http://www.legroom.net/software
// License:    GNU Lesser General Public License (LGPL), version 3
//             http://www.gnu.org/licenses/lgpl.html
//
// Modified for Inno Setup 6.3+ (Win9x/AUTOEXEC.BAT support removed):
//  - the system PATH is modified in admin install mode, the PATH of the current
//    user otherwise (IsAdminInstallMode), so ModPathType is no longer needed and
//    the uninstaller removes the entry from the same PATH it was added to
//  - PATH is written as REG_EXPAND_SZ so that entries like %SystemRoot% keep working
//  - directories are compared case-insensitive and without trailing backslash
//
// Script Function:
//   Allow modification of the environmental path directly from Inno Setup installers
//
// Instructions:
//   Add this statement to your [Setup] section
//     ChangesEnvironment=true
//
//   Add a task with the name defined in ModPathName to your [Tasks] section
//     Name: modifypath; Description: "Add application directory to PATH"; Flags: unchecked
//
//   Add the following to your [Code] section before including this file
//     const
//       ModPathName = 'modifypath';
//       ModPathPath = '{app}\bin';   // may be a constant like {app}\bin
//     #include "modpath.iss"
//
//   Call ModPath() in CurStepChanged(ssPostInstall) if the task is selected.
//   The removal on uninstall is handled by this file (CurUninstallStepChanged).
// ----------------------------------------------------------------------------

function ModPathDir(): TArrayOfString;
begin
  SetArrayLength(Result, 1);
  Result[0] := ExpandConstant(ModPathPath);
end;

// Add (install) or remove (uninstall) the directories of ModPathDir() to/from PATH
procedure ModPath();
var
  oldpath:  String;
  newpath:  String;
  entry:    String;
  pathArr:  TArrayOfString;
  pathdir:  TArrayOfString;
  regroot:  Integer;
  regpath:  String;
  i, d:     Integer;
  found:    Boolean;
begin
  if IsAdminInstallMode then begin
    regroot := HKEY_LOCAL_MACHINE;
    regpath := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';
  end else begin
    regroot := HKEY_CURRENT_USER;
    regpath := 'Environment';
  end;

  pathdir := ModPathDir();
  for d := 0 to GetArrayLength(pathdir) - 1 do begin
    // RegQueryStringValue does not expand REG_EXPAND_SZ values, which is what we want
    oldpath := '';
    RegQueryStringValue(regroot, regpath, 'Path', oldpath);
    pathArr := StringSplit(oldpath, [';'], stExcludeEmpty);

    newpath := '';
    found := False;
    for i := 0 to GetArrayLength(pathArr) - 1 do begin
      entry := pathArr[i];
      if CompareText(RemoveBackslashUnlessRoot(entry), RemoveBackslashUnlessRoot(pathdir[d])) = 0 then begin
        found := True;
        // when uninstalling drop the entry, when installing keep it (once)
        if IsUninstaller() then
          Continue;
      end;
      if newpath = '' then
        newpath := entry
      else
        newpath := newpath + ';' + entry;
    end;

    if not IsUninstaller() then begin
      if not found then begin
        if newpath = '' then
          newpath := pathdir[d]
        else
          newpath := newpath + ';' + pathdir[d];
        RegWriteExpandStringValue(regroot, regpath, 'Path', newpath);
      end;
    end else begin
      if found then
        RegWriteExpandStringValue(regroot, regpath, 'Path', newpath);
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  aSelectedTasks: TArrayOfString;
  i:              Integer;
  regpath:        String;
  regstring:      String;
  appid:          String;
begin
  // only run during actual uninstall
  if CurUninstallStep = usUninstall then begin
    // get the list of selected tasks saved in the registry at install time
    appid := '{#emit SetupSetting("AppId")}';
    if appid = '' then appid := '{#emit SetupSetting("AppName")}';
    regpath := ExpandConstant('Software\Microsoft\Windows\CurrentVersion\Uninstall\' + appid + '_is1');
    regstring := '';
    // HKA is HKLM in admin install mode and HKCU otherwise
    RegQueryStringValue(HKA, regpath, 'Inno Setup: Selected Tasks', regstring);

    // if the modpath task was selected, remove the directory from PATH
    aSelectedTasks := StringSplit(regstring, [','], stExcludeEmpty);
    for i := 0 to GetArrayLength(aSelectedTasks) - 1 do begin
      if CompareText(aSelectedTasks[i], ModPathName) = 0 then begin
        ModPath();
        Break;
      end;
    end;
  end;
end;
