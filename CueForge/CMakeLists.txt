# CMakeLists.txt - Qt6 + JUCE + Tracktion Engine Integration for CueForge 2.0
cmake_minimum_required(VERSION 3.20)
project(CueForge VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Enable position independent code for all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find Qt6 components we need
find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Gui
    Multimedia MultimediaWidgets
    Network SerialPort
)

# Add JUCE framework
add_subdirectory(third_party/juce)

# Add Tracktion Engine using FetchContent
include(FetchContent)
FetchContent_Declare(
    TracktionEngine
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG        develop
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/tracktion
)

# Only fetch if tracktion directory doesn't exist
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tracktion)
    FetchContent_MakeAvailable(TracktionEngine)
endif()

# Add tracktion engine if the directory exists
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tracktion)
    add_subdirectory(tracktion)
endif()

# Create our source file list
set(CUEFORGE_SOURCES
    # Main entry point
    src/main.cpp
    
    # Core application classes
    src/core/Application.cpp
    src/core/Application.h
    src/core/CueManager.cpp
    src/core/CueManager.h
    src/core/Workspace.cpp
    src/core/Workspace.h
    
    # Cue system classes  
    src/core/Cue.cpp
    src/core/Cue.h
    src/core/AudioCue.cpp
    src/core/AudioCue.h
    src/core/VideoCue.cpp
    src/core/VideoCue.h
    src/core/MIDICue.cpp
    src/core/MIDICue.h
    src/core/FadeCue.cpp
    src/core/FadeCue.h
    src/core/GroupCue.cpp
    src/core/GroupCue.h
    src/core/ControlCue.cpp
    src/core/ControlCue.h
    
    # UI components
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/CueListWidget.cpp
    src/ui/CueListWidget.h
    src/ui/InspectorWidget.cpp
    src/ui/InspectorWidget.h
    src/ui/TransportWidget.cpp
    src/ui/TransportWidget.h
    src/ui/MatrixMixerWidget.cpp
    src/ui/MatrixMixerWidget.h
    src/ui/PreferencesDialog.cpp
    src/ui/PreferencesDialog.h
    
    # Data models
    src/models/CueModel.cpp
    src/models/CueModel.h
    src/models/CueListModel.cpp
    src/models/CueListModel.h
    
    # Audio engine integration
    src/audio/TracktionAudioEngine.cpp
    src/audio/TracktionAudioEngine.h
    src/audio/AudioEngineManager.cpp
    src/audio/AudioEngineManager.h
    src/audio/JuceAudioBridge.cpp
    src/audio/JuceAudioBridge.h
    
    # Utilities
    src/utils/Settings.cpp
    src/utils/Settings.h
    src/utils/FileUtils.cpp
    src/utils/FileUtils.h
)

# Define our application
qt6_add_executable(CueForge ${CUEFORGE_SOURCES})

# Add resources
qt6_add_resources(CueForge "cueforge_resources"
    PREFIX "/"
    FILES
        resources/icons/cueforge.ico
        resources/icons/play.svg
        resources/icons/stop.svg
        resources/icons/pause.svg
        resources/icons/go.svg
        resources/styles/cueforge-dark.qss
        resources/styles/cueforge-light.qss
)

# Set target properties
set_target_properties(CueForge PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "CueForge"
)

# Include directories
target_include_directories(CueForge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils
)

# Link Qt6 libraries
target_link_libraries(CueForge PRIVATE
    # Qt6 components
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::SerialPort
)

# Link JUCE components for audio
target_link_libraries(CueForge PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_data_structures
    juce::juce_events
)

# Link Tracktion Engine if available
if(TARGET tracktion_engine)
    target_link_libraries(CueForge PRIVATE
        tracktion_engine
    )
    target_compile_definitions(CueForge PRIVATE
        CUEFORGE_USE_TRACKTION_ENGINE=1
    )
    message(STATUS "Tracktion Engine integration enabled")
else()
    message(WARNING "Tracktion Engine not found - audio features will be limited")
    target_compile_definitions(CueForge PRIVATE
        CUEFORGE_USE_TRACKTION_ENGINE=0
    )
endif()

# Compiler-specific settings
if(MSVC)
    target_compile_options(CueForge PRIVATE 
        /W4 
        /wd4458  # Disable "declaration hides class member" warning common in Qt
        /wd4127  # Disable "conditional expression is constant" for Qt macros
    )
    # Set Windows subsystem for release builds
    set_target_properties(CueForge PROPERTIES
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
    
    # Enable multi-processor compilation
    target_compile_options(CueForge PRIVATE /MP)
    
else()
    target_compile_options(CueForge PRIVATE 
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter  # Common in Qt signal/slot connections
    )
    
    # Enable debug symbols in release builds
    target_compile_options(CueForge PRIVATE "$<$<CONFIG:RELEASE>:-g>")
endif()

# Platform-specific configurations
if(WIN32)
    # Windows-specific settings
    set_target_properties(CueForge PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Set application manifest for modern Windows features
    if(MSVC)
        target_sources(CueForge PRIVATE resources/windows/cueforge.rc)
    endif()
    
elseif(APPLE)
    # macOS-specific settings
    set_target_properties(CueForge PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "CueForge"
        MACOSX_BUNDLE_GUI_IDENTIFIER "app.cueforge.CueForge"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/Info.plist.in
    )
    
    # macOS frameworks
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(COREMIDI_FRAMEWORK CoreMIDI)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    
    target_link_libraries(CueForge PRIVATE
        ${COREAUDIO_FRAMEWORK}
        ${COREMIDI_FRAMEWORK}
        ${AUDIOTOOLBOX_FRAMEWORK}
        ${ACCELERATE_FRAMEWORK}
    )
    
elseif(UNIX)
    # Linux-specific settings
    find_package(PkgConfig REQUIRED)
    
    # ALSA for audio
    pkg_check_modules(ALSA REQUIRED alsa)
    target_link_libraries(CueForge PRIVATE ${ALSA_LIBRARIES})
    target_include_directories(CueForge PRIVATE ${ALSA_INCLUDE_DIRS})
    
    # Additional Linux audio libraries
    target_link_libraries(CueForge PRIVATE pthread dl)
endif()

# Enable JUCE-specific settings
target_compile_definitions(CueForge PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CueForge,OUTPUT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
)

# Development vs Release settings
target_compile_definitions(CueForge PRIVATE
    $<$<CONFIG:DEBUG>:CUEFORGE_DEBUG=1>
    $<$<CONFIG:RELEASE>:CUEFORGE_DEBUG=0>
)

# Install configuration
install(TARGETS CueForge
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
    COMPONENT Application
)

# Install Qt6 libraries (for deployment)
if(WIN32)
    # Windows deployment
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(WINDEPLOYQT_EXECUTABLE)
        install(CODE "execute_process(COMMAND ${WINDEPLOYQT_EXECUTABLE} \${CMAKE_INSTALL_PREFIX}/bin/CueForge.exe)")
    endif()
elseif(APPLE)
    # macOS deployment
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt HINTS ${Qt6_DIR}/../../../bin)
    if(MACDEPLOYQT_EXECUTABLE)
        install(CODE "execute_process(COMMAND ${MACDEPLOYQT_EXECUTABLE} \${CMAKE_INSTALL_PREFIX}/CueForge.app)")
    endif()
endif()

# Package configuration (for CPack)
set(CPACK_PACKAGE_NAME "CueForge")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "Professional cue-based show control application")
set(CPACK_PACKAGE_VENDOR "CueForge")
set(CPACK_PACKAGE_CONTACT "support@cueforge.app")

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "CueForge 2.0")
    set(CPACK_NSIS_PACKAGE_NAME "CueForge")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://cueforge.app")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    set(CPACK_DMG_VOLUME_NAME "CueForge")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "qt6-base-dev, libasound2-dev")
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase-devel, alsa-lib-devel")
endif()

include(CPack)

# Build information
message(STATUS "CueForge ${PROJECT_VERSION} configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Qt6 version: ${Qt6_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
if(TARGET tracktion_engine)
    message(STATUS "  Tracktion Engine: ENABLED")
else()
    message(STATUS "  Tracktion Engine: DISABLED")
endif()