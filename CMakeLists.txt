cmake_minimum_required(VERSION 3.20)

project(PrtSc
    VERSION 0.4.0
    LANGUAGES CXX
)

add_executable(PrtSc WIN32
    Annotation.cpp
    Annotation.h
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
    TextAnnotation.cpp
    TextAnnotation.h
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

add_library(PrtScStartupMsix SHARED
    PrtScStartupMsix.cpp
)

target_compile_features(PrtScStartupMsix PRIVATE cxx_std_23)

target_link_libraries(PrtScStartupMsix PRIVATE
    WindowsApp
    RuntimeObject
)

if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(PRTSC_WINDOWS_SDK_INCLUDE_ROOT
        "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    if(EXISTS "${PRTSC_WINDOWS_SDK_INCLUDE_ROOT}/cppwinrt")
        target_include_directories(PrtScStartupMsix PRIVATE
            "${PRTSC_WINDOWS_SDK_INCLUDE_ROOT}/cppwinrt")
    endif()
endif()

target_compile_definitions(PrtScStartupMsix PRIVATE
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
