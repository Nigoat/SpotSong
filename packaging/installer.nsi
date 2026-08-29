; Copyright (C) 2026 SpotSong Contributors
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <https://www.gnu.org/licenses/>.

!include "MUI2.nsh"

Name "SpotSong"
OutFile "SpotSong-Setup.exe"
InstallDir "$PROGRAMFILES64\SpotSong"
InstallDirRegKey HKLM "Software\SpotSong" "Install_Dir"
RequestExecutionLevel admin

!define MUI_ABORTWARNING
!define MUI_ICON "..\assets\icons\app_icon.ico"
!define MUI_UNICON "..\assets\icons\app_icon.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "SpotSong Core" SecCore
  SetOutPath "$INSTDIR"
  File /r "build-windows\bin\*"

  WriteRegStr HKLM "Software\SpotSong" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpotSong" "DisplayName" "SpotSong Music Player"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpotSong" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpotSong" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpotSong" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\SpotSong"
  CreateShortcut "$SMPROGRAMS\SpotSong\SpotSong.lnk" "$INSTDIR\spotsong.exe"
  CreateShortcut "$SMPROGRAMS\SpotSong\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\SpotSong.lnk" "$INSTDIR\spotsong.exe"
SectionEnd

Section "Uninstall"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SpotSong"
  DeleteRegKey HKLM "Software\SpotSong"

  RMDir /r "$INSTDIR"
  Delete "$SMPROGRAMS\SpotSong\SpotSong.lnk"
  Delete "$SMPROGRAMS\SpotSong\Uninstall.lnk"
  RMDir "$SMPROGRAMS\SpotSong"
  Delete "$DESKTOP\SpotSong.lnk"
SectionEnd
