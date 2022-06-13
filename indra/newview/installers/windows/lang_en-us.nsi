; First is default
!insertmacro MUI_LANGUAGE "English"

; Language string
LangString LanguageCode ${LANG_ENGLISH}  "en"

; Language selection dialog
LangString SelectInstallerLanguage  ${LANG_ENGLISH} "Please select the language of the installer"

; installation directory text
LangString DirectoryChooseTitle ${LANG_ENGLISH} "Installation Directory" 
LangString DirectoryChooseUpdate ${LANG_ENGLISH} "Select the ${APPNAME} directory to update to version ${VERSION_LONG}.(XXX):"
LangString DirectoryChooseSetup ${LANG_ENGLISH} "Select the directory to install ${APPNAME} in:"

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_ENGLISH} "Could not find the program '$INSTPROG'. Silent update failed."

; check windows version
LangString CheckWindowsVersionDP ${LANG_ENGLISH} "Checking Windows version..."
LangString CheckWindowsVersionMB ${LANG_ENGLISH} "${APPNAME} only supports Windows Vista with Service Pack 2 and later.$\nInstallation on this Operating System is not supported. Quitting."
LangString CheckWindowsServPackMB ${LANG_ENGLISH} "It is recomended to run ${APPNAME} on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_ENGLISH} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_ENGLISH} "Checking for permission to install..."
LangString CheckAdministratorInstMB ${LANG_ENGLISH} 'You appear to be using a "limited" account.$\nYou must be an "administrator" to install ${APPNAME}.'

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_ENGLISH} "Checking for permission to uninstall..."
LangString CheckAdministratorUnInstMB ${LANG_ENGLISH} 'You appear to be using a "limited" account.$\nYou must be an "administrator" to uninstall ${APPNAME}.'

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_ENGLISH} "It appears that ${APPNAME} ${VERSION_LONG} is already installed.$\n$\nWould you like to install it again?"

; checkcpuflags
LangString MissingSSE2 ${LANG_ENGLISH} "This machine may not have a CPU with SSE2 support, which is required to run ${APPNAME} ${VERSION_LONG}. Do you want to continue?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_ENGLISH} "Waiting for ${APPNAME} to shut down..."
LangString CloseSecondLifeInstMB ${LANG_ENGLISH} "${APPNAME} can't be installed while it is already running.$\n$\nFinish what you're doing then select OK to close ${APPNAME} and continue.$\nSelect NO to proceed without closing the viewer, Caution: this can prevent a proper install, use with care (like when installing a different viewer than the one you are running).$\nSelect CANCEL to cancel installation."

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_ENGLISH} "Waiting for ${APPNAME} to shut down..."
LangString CloseSecondLifeUnInstMB ${LANG_ENGLISH} "${APPNAME} can't be uninstalled while it is already running.$\n$\nFinish what you're doing then select OK to close ${APPNAME} and continue.$\nSelect NO to proceed without closing the viewer, Caution: this can prevent a proper uninstall, use with care (like when uninstalling a different viewer than the one you are running).$\nSelect CANCEL to cancel."

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_ENGLISH} "Checking network connection..."

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_ENGLISH} "Deleting cache files in Documents and Settings folder"

; delete program files
LangString DeleteProgramFilesMB ${LANG_ENGLISH} "There are still files in your ${APPNAME} program directory.$\n$\nThese are possibly files you created or moved to:$\n$INSTDIR$\n$\nDo you want to remove them?"

; uninstall text
LangString UninstallTextMsg ${LANG_ENGLISH} "This will uninstall ${APPNAME} ${VERSION_LONG} from your system. Click Uninstall to start the uninstallation."
