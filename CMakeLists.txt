cmake_minimum_required(VERSION 3.20)

project(PrtSc
    VERSION 0.3.0
    LANGUAGES CXX
)

add_executable(PrtSc WIN32
    AppMessages.h
    ArrowAnnotation.cpp
    ArrowAnnotation.h
    CaptureToolbar.cpp
    CaptureToolbar.h
    HotkeyManager.cpp
    HotkeyManager.h
    main.cpp
    OcrEngine.cpp
    OcrEngine.h
    PrtSc.rc
    resource.h
    ScreenOverlay.cpp
    ScreenOverlay.h
    Settings.cpp
    Settings.h
    SettingsDialog.cpp
    SettingsDialog.h
    StartupManager.cpp
    StartupManager.h
    SysTrayIcon.cpp
    SysTrayIcon.h
)

target_compile_features(PrtSc PRIVATE cxx_std_23)

target_link_libraries(PrtSc PRIVATE
    Advapi32
    Comdlg32
    Comctl32
    Gdiplus
    Shell32
)

target_compile_definitions(PrtSc PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

add_library(PrtScOcr SHARED
    PrtScOcr.cpp
)

target_compile_features(PrtScOcr PRIVATE cxx_std_23)

target_link_libraries(PrtScOcr PRIVATE
    Gdiplus
    Ole32
    WindowsApp
)

target_compile_definitions(PrtScOcr PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
