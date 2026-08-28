!include MUI2.nsh
!include nsDialogs.nsh

RequestExecutionLevel admin
CRCCheck force

!define PRODUCT_PUBLISHER "huawei"

Outfile "MindStudio-Insight_{plugins_version}_win.exe"
InstallDir "$PROGRAMFILES\MindStudio Insight"
Caption "MindStudio Insight Setup"
Name "MindStudio Insight"

!define CURRENT_VERSION "{plugins_version}"
!define PE_NUMERIC_VERSION "{plugins_numeric_version}"

VIProductVersion "${PE_NUMERIC_VERSION}"
VIAddVersionKey /LANG=1033 "ProductName" "MindStudio Insight"
VIAddVersionKey /LANG=1033 "CompanyName" "Huawei Technologies CO.,Ltd."
VIAddVersionKey /LANG=1033 "FileDescription" "MindStudio Insight Setup"
VIAddVersionKey /LANG=1033 "FileVersion" "${PE_NUMERIC_VERSION}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "Copyright (c) 2026 Huawei Technologies Co.,Ltd."
VIAddVersionKey /LANG=1033 "ProductVersion" "${CURRENT_VERSION}"

!define REGKEY "Software\huawei\MindStudio Insight"

!define MUI_FINISHPAGE_RUN  "$INSTDIR\MindStudio-Insight.exe"

; Modern UI settings
!define MUI_ICON "resources\images\icons\mindstudio.ico"
; Set the uninstaller icon
!define MUI_UNICON "resources\images\icons\mindstudio.ico"

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\nsis.bmp"
!define MUI_HEADERIMAGE_RIGHT

; Pages
!define MUI_UNPAGE_CUSTOMFUNCTION_PRE un.onCustomUninstallPage
!define MUI_UNPAGE_CUSTOMFUNCTION_LEAVE un.onCustomUninstallPageLeave

!insertmacro MUI_PAGE_WELCOME

!insertmacro MUI_PAGE_LICENSE "resources\license\hwEulaCommunity.txt"

!define MUI_PAGE_HEADER_TEXT "Choose Install Location"
!insertmacro MUI_PAGE_DIRECTORY

!insertmacro MUI_PAGE_COMPONENTS

!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Merge custom uninstall page with confirm uninstall page
UninstPage custom un.onCustomUninstallPage un.onCustomUninstallPageLeave
!define MUI_UNPAGE_CONFIRM_TEXT "Your MindStudio Insight uninstall text here"
!insertmacro MUI_UNPAGE_INSTFILES

; Language
!insertmacro MUI_LANGUAGE "English"

Var RemoveCacheData
Function un.onCustomUninstallPage
    !insertmacro MUI_HEADER_TEXT "Whether To Delete Cache Files" "eg log,setting..."
    
    nsDialogs::Create 1018
    Pop $0
    
    ${NSD_CreateCheckbox} 0 30u 100% 10u "Remove cache data"
    Pop $1
    
    nsDialogs::Show
FunctionEnd

Function un.onCustomUninstallPageLeave
    ${NSD_GetState} $1 $0
    StrCpy $RemoveCacheData $0
FunctionEnd


Function .onInit
  nsProcessW::_FindProcess "ascend_insight.exe" $R0
  Pop $0
  ${If} $0 = "0"
    MessageBox MB_OK "ascend_insight.exe is running. Please close it first."
    Abort
  ${EndIf}

  nsProcessW::_FindProcess "MindStudio-Insight.exe" $R0
  Pop $0
  ${If} $0 = "0"
    MessageBox MB_OK "MindStudio-Insight.exe is running. Please close it first."
    Abort
  ${EndIf}

  ReadRegStr $0 HKLM "${REGKEY}" "Version"

  StrCmp $0 "" NoPreviousInstallation
    MessageBox MB_YESNO|MB_ICONQUESTION "A previous version of MindStudio Insight ($0) is already installed. Do you want to update to version ${CURRENT_VERSION}?" IDYES NoPreviousInstallation
    Abort

  NoPreviousInstallation:

  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Install\MindStudio Insight" "InstallPath"
  StrCmp $0 "" +2 0
  StrCpy $INSTDIR $0
FunctionEnd

; Components
Section "MindStudio Insight" Secascend_insight

  ; Only the fixed installed RAG root is replaced; NSIS owns payload integrity and extraction errors.
  ClearErrors
  RMDir /r "$INSTDIR\resources\profiler\server\insight_web_agent\rag-data"
  ${If} ${Errors}
    Abort
  ${EndIf}

  ; Remove Start Menu shortcut
  Delete "$DESKTOP\MindStudio Insight.lnk"
  Delete "$DESKTOP\Ascend Insight.lnk"
  RMDir /r "$SMPROGRAMS\MindStudio Insight"

  SetOutPath $INSTDIR
  ClearErrors
  File /r "MindStudio-Insight.exe"
  SetOutPath $INSTDIR\resources
  File /r "resources\*"
  SetOutPath $INSTDIR\config
  File /r "config\*"
  ${If} ${Errors}
    Abort
  ${EndIf}

  SetOutPath $INSTDIR

  ; Create Start Menu shortcut
  CreateDirectory "$SMPROGRAMS\MindStudio Insight"
  CreateShortCut "$SMPROGRAMS\MindStudio Insight\MindStudio Insight.lnk" "$INSTDIR\MindStudio-Insight.exe"
  ; Create Desktop shortcut
  CreateShortCut "$DESKTOP\MindStudio Insight.lnk" "$INSTDIR\MindStudio-Insight.exe"

  ; Add to control panel
  ; 如果旧版本已安装，则删除旧版本的启动菜单文件以及注册表项
  ReadRegStr $R0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ascend_insight" "UninstallString"
  ${If} $R0 != ""
    ; Remove ascend_insight Start Menu shortcut
    Delete $SMPROGRAMS\ascend_insight\ascend_insight.lnk
    RMDir /r "$SMPROGRAMS\ascend_insight"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ascend_insight"
    DeleteRegKey HKLM "Software\huawei\ascend_insight"
  ${EndIf}
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight" "DisplayName" "MindStudio Insight"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight" "DisplayVersion" "${CURRENT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight" "DisplayIcon" "$INSTDIR\MindStudio-Insight.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight" "Publisher" "Huawei Technologies CO.,Ltd."

  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight" "NoRepair" 1
  WriteUninstaller "Uninstall.exe"

  StrCpy $1 $INSTDIR
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Install\MindStudio Insight" "InstallPath" $1
  
  ; 
  WriteRegStr HKLM "${REGKEY}" "Version" "${CURRENT_VERSION}"
 
SectionEnd

; Uninstaller
Section "Uninstall"
  nsProcessW::_FindProcess "MindStudio-Insight.exe" $R0
  Pop $0
  ${If} $0 = "0" 
    MessageBox MB_OK "MindStudio-Insight.exe is running. Please close it first."
    Abort
  ${EndIf}

  nsProcessW::_FindProcess "profiler_server.exe" $R0
    Pop $0
    ${If} $0 = "0"
      MessageBox MB_OK "profiler_server.exe is running. Please close it first."
      Abort
    ${EndIf}

  ; Remove files
  Delete "$INSTDIR\MindStudio-Insight.exe"
  RMDir /r $INSTDIR\config
  ; remove all except logs
  RMDir /r $INSTDIR\.mindstudio_insight\admin
  RMDir /r $INSTDIR\.mindstudio_insight\EBWebView
  RMDir /r $INSTDIR\resources
    ${If} $RemoveCacheData == 1
        StrCpy $1 $APPDATA
        ; remove all logs and install dir
        RMDir /r $INSTDIR\.mindstudio_insight
        RMDir /r $PROFILE\.mindstudio_insight
        RMDir /r $INSTDIR
    ${EndIf}

  ; Remove Start Menu shortcut
  Delete "$SMPROGRAMS\MindStudio Insight\MindStudio Insight.lnk"
  RMDir /r "$SMPROGRAMS\MindStudio Insight"

  ; Remove Desktop shortcut
  Delete "$DESKTOP\Ascend Insight.lnk"
  Delete "$DESKTOP\MindStudio Insight.lnk"

  ; Remove control panel entry
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\MindStudio Insight"
  
  

  ; Remove registry key
  DeleteRegKey HKLM "${REGKEY}"

  ; Remove installation directory
  Delete "$INSTDIR\uninstall.exe"
  StrCpy $2 $INSTDIR 3
  StrCmp $2 "C:\" 0 +4
  RMDir /r $INSTDIR
  RMDir /r $PROFILE\.mindstudio_insight\admin
  RMDir /r $PROFILE\.mindstudio_insight\EBWebView
SectionEnd
