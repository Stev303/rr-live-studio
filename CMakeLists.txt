cmake_minimum_required(VERSION 3.22)

project(RrLiveStudio VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(RRLIVE_BUILD_TESTS "Build the Rr Live Studio unit tests" ON)
set(RRLIVE_ASIO_SDK_PATH "" CACHE PATH "Optional path to the Steinberg ASIO SDK (contains asio.h)")
set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/external/webview2pkg" CACHE PATH "Folder containing Microsoft.Web.WebView2 NuGet packages")

# ------------------------------------------------------------------------------
# JUCE
# ------------------------------------------------------------------------------
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ------------------------------------------------------------------------------
# Core audio / DSP library (no GUI)
# ------------------------------------------------------------------------------
if (WIN32)
    set(RRLIVE_WEBVIEW2_CMAKE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/extras/Build/CMake")
    if (NOT EXISTS "${RRLIVE_WEBVIEW2_CMAKE_DIR}/FindWebView2.cmake" AND DEFINED juce_SOURCE_DIR)
        set(RRLIVE_WEBVIEW2_CMAKE_DIR "${juce_SOURCE_DIR}/extras/Build/CMake")
    endif()
    list(APPEND CMAKE_MODULE_PATH "${RRLIVE_WEBVIEW2_CMAKE_DIR}")
    find_package(WebView2 REQUIRED)
endif()

set(RRLIVE_CORE_SOURCES
    Source/Audio/ParamStore.cpp
    Source/Audio/BusRouter.cpp
    Source/Audio/LoopbackCapture.cpp
    Source/Audio/AudioEngine.cpp
    Source/Audio/SmartBalance.cpp
    Source/DSP/Biquad.cpp
    Source/DSP/PitchShifter.cpp
    Source/DSP/PitchDetector.cpp
    Source/DSP/Compressor.cpp
    Source/DSP/Limiter.cpp
    Source/DSP/VocalChain.cpp
    Source/DSP/MusicBooster.cpp
    Source/DSP/Harmonizer.cpp
    Source/DSP/SmartEQ.cpp
    Source/Sampler/SamplerEngine.cpp
    Source/VST3Host/PluginHost.cpp
    Source/Presets/PresetManager.cpp
    Source/AI/HarmonyAssistant.cpp
    Source/Utilities/DiagnosticsLog.cpp
)

add_library(RrLiveCore STATIC ${RRLIVE_CORE_SOURCES})
target_include_directories(RrLiveCore PUBLIC Source)
target_compile_definitions(RrLiveCore PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_ASIO=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_MODAL_LOOPS_PERMITTED=0
    NOMINMAX
    _USE_MATH_DEFINES)

target_link_libraries(RrLiveCore
    PUBLIC
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    PRIVATE
        ole32
        oleaut32
        propsys
        uuid
        avrt
        winmm
        shlwapi
        version)

if (WIN32 AND TARGET juce::juce_webview2)
    target_link_libraries(RrLiveCore PUBLIC juce::juce_webview2)
endif()

if (WIN32 AND RRLIVE_ASIO_SDK_PATH AND EXISTS "${RRLIVE_ASIO_SDK_PATH}/asio.h")
    message(STATUS "RrLiveStudio: ASIO SDK found at ${RRLIVE_ASIO_SDK_PATH} - enabling ASIO")
    target_compile_definitions(RrLiveCore PUBLIC JUCE_ASIO=1)
    target_include_directories(RrLiveCore PUBLIC "${RRLIVE_ASIO_SDK_PATH}")
    target_link_libraries(RrLiveCore PRIVATE setupapi)
else()
    message(STATUS "RrLiveStudio: ASIO SDK not configured - building without ASIO (WASAPI only)")
    target_compile_definitions(RrLiveCore PUBLIC JUCE_ASIO=0)
endif()

# ------------------------------------------------------------------------------
# Application (GUI)
# ------------------------------------------------------------------------------
set(RRLIVE_APP_SOURCES
    Source/App/Main.cpp
    Source/App/AppState.cpp
    Source/App/MainComponent.cpp
    Source/UI/Theme.cpp
    Source/UI/Widgets.cpp
    Source/UI/Sidebar.cpp
    Source/UI/TopBar.cpp
    Source/UI/Pages/DashboardPage.cpp
    Source/UI/Pages/VocalChainPage.cpp
    Source/UI/Pages/EffectsPage.cpp
    Source/UI/Pages/YouTubePage.cpp
    Source/UI/Pages/VST3Page.cpp
    Source/UI/Pages/SamplerPage.cpp
    Source/UI/Pages/MixerPage.cpp
    Source/UI/Pages/PresetsPage.cpp
    Source/UI/Pages/SettingsPage.cpp
    Source/YouTube/YouTubeBrowser.cpp
    Source/UI/GoLiveDialog.cpp
    Source/UI/RoutingDiagram.cpp
)

juce_add_gui_app(RrLiveStudio
    PRODUCT_NAME "Rr Live Studio"
    COMPANY_NAME "Rr Studio"
    BUNDLE_ID "com.rrstudio.rrlivestudio"
    VERSION "1.0.0"
    NEEDS_WEBVIEW2 TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Rr Live Studio membutuhkan akses mikrofon untuk vocal chain.")

target_sources(RrLiveStudio PRIVATE ${RRLIVE_APP_SOURCES})
target_compile_definitions(RrLiveStudio PRIVATE
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:RrLiveStudio,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:RrLiveStudio,JUCE_VERSION>")

target_link_libraries(RrLiveStudio PRIVATE
    RrLiveCore
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    winhttp)

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------
if (RRLIVE_BUILD_TESTS)
    juce_add_console_app(RrLiveStudioTests PRODUCT_NAME "RrLiveStudioTests")
    target_sources(RrLiveStudioTests PRIVATE
        Tests/TestMain.cpp
        Tests/DSPTests.cpp
        Tests/PresetRoutingTests.cpp)
    target_include_directories(RrLiveStudioTests PRIVATE Source Tests)
    target_link_libraries(RrLiveStudioTests PRIVATE
        RrLiveCore
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    enable_testing()
    add_test(NAME RrLiveStudioTests COMMAND RrLiveStudioTests)
endif()
