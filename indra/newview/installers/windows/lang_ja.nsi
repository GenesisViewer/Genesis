; First is default
!insertmacro MUI_LANGUAGE "Japanese"

; Language string
LangString LanguageCode ${LANG_JAPANESE} "ja"

; Language selection dialog
LangString SelectInstallerLanguage  ${LANG_JAPANESE} "インストーラの言語を選択してください"

; installation directory text
LangString DirectoryChooseTitle ${LANG_JAPANESE} "インストール・ディレクトリ" 
LangString DirectoryChooseUpdate ${LANG_JAPANESE} "アップデートするセカンドライフのディレクトリを選択してください。:" 
LangString DirectoryChooseSetup ${LANG_JAPANESE} "セカンドライフをインストールするディレクトリを選択してください。: " 

; CheckStartupParams message box
LangString CheckStartupParamsMB ${LANG_JAPANESE} "プログラム名'$INSTPROG'が見つかりません。サイレント・アップデートに失敗しました。" 

; check windows version
LangString CheckWindowsVersionDP ${LANG_JAPANESE} "ウィンドウズのバージョン情報をチェック中です..." 
LangString CheckWindowsVersionMB ${LANG_JAPANESE} "Second Life はWindows XPのみをサポートしています。Windows $R0をインストールする事は、データの消失やクラッシュの原因になる可能性があります。インストールを続けますか？" 
LangString CheckWindowsServPackMB ${LANG_JAPANESE} "It is recomended to run Firestorm on the latest service pack for your operating system.$\nThis will help with performance and stability of the program."
LangString UseLatestServPackDP ${LANG_JAPANESE} "Please use Windows Update to install the latest Service Pack."

; checkifadministrator function (install)
LangString CheckAdministratorInstDP ${LANG_JAPANESE} "インストールのための権限をチェック中です..." 
LangString CheckAdministratorInstMB ${LANG_JAPANESE} "セカンドライフをインストールするには管理者権限が必要です。"

; checkifadministrator function (uninstall)
LangString CheckAdministratorUnInstDP ${LANG_JAPANESE} "アンインストールのための権限をチェック中です..." 
LangString CheckAdministratorUnInstMB ${LANG_JAPANESE} "セカンドライフをアンインストールするには管理者権限が必要です。" 

; checkifalreadycurrent
LangString CheckIfCurrentMB ${LANG_JAPANESE} "セカンドライフ${VERSION_LONG} はインストール済みです。再度インストールしますか？ " 

; checkcpuflags
LangString MissingSSE2 ${LANG_JAPANESE} "This machine may not have a CPU with SSE2 support, which is required to run ${APPNAME} ${VERSION_LONG}. Do you want to continue?"

; closesecondlife function (install)
LangString CloseSecondLifeInstDP ${LANG_JAPANESE} "セカンドライフを終了中です..." 
LangString CloseSecondLifeInstMB ${LANG_JAPANESE} "セカンドライフの起動中にインストールは出来ません。直ちにセカンドライフを終了してインストールを開始する場合はOKボタンを押してください。CANCELを押すと中止します。"

; closesecondlife function (uninstall)
LangString CloseSecondLifeUnInstDP ${LANG_JAPANESE} "セカンドライフを終了中です..." 
LangString CloseSecondLifeUnInstMB ${LANG_JAPANESE} "セカンドライフの起動中にアンインストールは出来ません。直ちにセカンドライフを終了してアンインストールを開始する場合はOKボタンを押してください。CANCELを押すと中止します。" 

; CheckNetworkConnection
LangString CheckNetworkConnectionDP ${LANG_JAPANESE} "ネットワークの接続を確認中..." 

; removecachefiles
LangString RemoveCacheFilesDP ${LANG_JAPANESE} " Documents and Settings フォルダのキャッシュファイルをデリート中です。" 

; delete program files
LangString DeleteProgramFilesMB ${LANG_JAPANESE} "セカンドライフのディレクトリには、まだファイルが残されています。$\n$INSTDIR$\nにあなたが作成、または移動させたファイルがある可能性があります。全て削除しますか？ " 

; uninstall text
LangString UninstallTextMsg ${LANG_JAPANESE} "セカンドライフ${VERSION_LONG}をアンインストールします。"
