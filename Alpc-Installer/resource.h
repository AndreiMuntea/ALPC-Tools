//{{NO_DEPENDENCIES}}
// Microsoft Visual C++ generated include file.
// Used by Alpc-Installer.rc

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        101
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1001
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif

//
// Default to debug builds - useful for debugging.
//
#define INSTALLFILE_DEFAULT_TO_DEBUG_BUILDS

//
// Win32 data - default to debug dir.
//
#ifdef  INSTALLFILE_DEFAULT_TO_DEBUG_BUILDS
    #define INSTALL_FILE_WIN32_ALPCMONSYS               L"..\\out\\Win32\\Debug\\AlpcMon_Sys\\AlpcMon_Sys\\AlpcMon_Sys.sys"
    #define INSTALL_FILE_WIN32_ALPCMONDLL               L"..\\out\\Win32\\Debug\\AlpcMon_Dll\\AlpcMon_DllWin32.dll"
#else
    #define INSTALL_FILE_WIN32_ALPCMONSYS               L"..\\out\\Win32\\Release\\AlpcMon_Sys\\AlpcMon_Sys\\AlpcMon_Sys.sys"
    #define INSTALL_FILE_WIN32_ALPCMONDLL               L"..\\out\\Win32\\Release\\AlpcMon_Dll\\AlpcMon_DllWin32.dll"
#endif  // INSTALLFILE_DEFAULT_TO_DEBUG_BUILDS

//
// x64 driver data - default to debug dir.
//
#ifdef  INSTALLFILE_DEFAULT_TO_DEBUG_BUILDS
    #define INSTALL_FILE_x64_ALPCMONSYS                 L"..\\out\\x64\\Debug\\AlpcMon_Sys\\AlpcMon_Sys\\AlpcMon_Sys.sys"
    #define INSTALL_FILE_x64_ALPCMONDLL                 L"..\\out\\x64\\Debug\\AlpcMon_Dll\\AlpcMon_Dllx64.dll"
#else
    #define INSTALL_FILE_x64_ALPCMONSYS                 L"..\\out\\x64\\Release\\AlpcMon_Sys\\AlpcMon_Sys\\AlpcMon_Sys.sys"
    #define INSTALL_FILE_x64_ALPCMONDLL                 L"..\\out\\x64\\Release\\AlpcMon_Dll\\AlpcMon_Dllx64.dll"
#endif // INSTALLFILE_DEFAULT_TO_DEBUG_BUILDS

//
// Install data ids.
//
#define IDR_INSTALLFILE1                            200     // INSTALL_FILE_WIN32_ALPCMONSYS
#define IDR_INSTALLFILE2                            201     // INSTALL_FILE_WIN32_ALPCMONDLL
#define IDR_INSTALLFILE3                            202     // INSTALL_FILE_x64_ALPCMONSYS
#define IDR_INSTALLFILE4                            203     // INSTALL_FILE_x64_ALPCMONDLL
