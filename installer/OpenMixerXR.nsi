; OpenMixer XR — NSIS Installer Script
; ─────────────────────────────────────────────────────────────────────────────
; Build with:
;   makensis installer\OpenMixerXR.nsi
;
; Prerequisites:
;   - NSIS 3.x   (https://nsis.sourceforge.io/)
;   - The Release build must be complete:
;       cmake --build build --config Release
;   - Optional: copy the VC++ 2022 Redistributable installer to
;       installer\VC_redist.x64.exe
;     to bundle it; otherwise the script skips the redist step.
;
; The installer places files under:
;   $PROGRAMFILES64\OpenMixer XR\
; and registers the VRManifest with SteamVR via the command-line tool.

; ── Metadata ──────────────────────────────────────────────────────────────────
Name              "OpenMixer XR"
OutFile           "OpenMixerXR-Installer.exe"
InstallDir        "$PROGRAMFILES64\OpenMixer XR"
InstallDirRegKey  HKLM "Software\OpenMixerXR" "InstallDir"
RequestExecutionLevel admin
SetCompressor     /SOLID lzma
Unicode           True

; ── Version info ──────────────────────────────────────────────────────────────
VIProductVersion  "1.0.0.0"
VIAddVersionKey   "ProductName"      "OpenMixer XR"
VIAddVersionKey   "FileVersion"      "1.0.0"
VIAddVersionKey   "ProductVersion"   "1.0.0"
VIAddVersionKey   "LegalCopyright"   "2024-2026 OpenMixer XR Contributors"
VIAddVersionKey   "FileDescription"  "SteamVR chroma passthrough overlay"

; ── Installer pages ───────────────────────────────────────────────────────────
!include "MUI2.nsh"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; ── Install section ───────────────────────────────────────────────────────────
Section "OpenMixer XR" SecMain
    SectionIn RO   ; required

    SetOutPath "$INSTDIR"

    ; Main executable
    File "..\build\Release\OpenMixerXR.exe"

    ; Resources (must live next to the executable — see CMakeLists.txt POST_BUILD)
    File "..\build\Release\manifest.vrmanifest"
    File /nonfatal "..\build\Release\dashboard_icon.png"

    ; ── VC++ Runtime (optional — bundle installer\VC_redist.x64.exe) ──────────
    IfFileExists "$EXEDIR\VC_redist.x64.exe" 0 skip_redist
        DetailPrint "Installing Visual C++ 2022 Redistributable..."
        ExecWait '"$EXEDIR\VC_redist.x64.exe" /install /quiet /norestart'
    skip_redist:

    ; ── Registry: store install dir ───────────────────────────────────────────
    WriteRegStr HKLM "Software\OpenMixerXR" "InstallDir" "$INSTDIR"

    ; ── Start Menu shortcut ───────────────────────────────────────────────────
    CreateDirectory "$SMPROGRAMS\OpenMixer XR"
    CreateShortcut  "$SMPROGRAMS\OpenMixer XR\OpenMixer XR.lnk" \
                    "$INSTDIR\OpenMixerXR.exe"
    CreateShortcut  "$SMPROGRAMS\OpenMixer XR\Uninstall.lnk" \
                    "$INSTDIR\Uninstall.exe"

    ; ── Write uninstaller ─────────────────────────────────────────────────────
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Register in Add/Remove Programs
    WriteRegStr   HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenMixerXR" \
        "DisplayName"          "OpenMixer XR"
    WriteRegStr   HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenMixerXR" \
        "UninstallString"      '"$INSTDIR\Uninstall.exe"'
    WriteRegStr   HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenMixerXR" \
        "DisplayVersion"       "1.0.0"
    WriteRegStr   HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenMixerXR" \
        "Publisher"            "OpenMixer XR Contributors"
    WriteRegDWORD HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenMixerXR" \
        "NoModify" 1
    WriteRegDWORD HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenMixerXR" \
        "NoRepair" 1

SectionEnd

; ── Uninstall section ─────────────────────────────────────────────────────────
Section "Uninstall"

    ; Remove files
    Delete "$INSTDIR\OpenMixerXR.exe"
    Delete "$INSTDIR\manifest.vrmanifest"
    Delete "$INSTDIR\dashboard_icon.png"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir  "$INSTDIR"

    ; Remove Start Menu entries
    Delete "$SMPROGRAMS\OpenMixer XR\OpenMixer XR.lnk"
    Delete "$SMPROGRAMS\OpenMixer XR\Uninstall.lnk"
    RMDir  "$SMPROGRAMS\OpenMixer XR"

    ; Remove registry entries
    DeleteRegKey HKLM "Software\OpenMixerXR"
    DeleteRegKey HKLM \
        "Software\Microsoft\Windows\CurrentVersion\Uninstall\OpenMixerXR"

SectionEnd
