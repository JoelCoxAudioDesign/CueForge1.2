
# CMakeLists.txt - Qt6 + JUCE Integration for CueForge 2.0
cmake_minimum_required(VERSION 3.20)
project(CueForge VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find Qt6 components we need
find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Gui
    Multimedia MultimediaWidgets
    Network SerialPort
)

# Add JUCE for audio engine (we'll use FetchContent for Tracktion Engine)
include(FetchContent)

# Fetch JUCE
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.8
)
FetchContent_MakeAvailable(JUCE)

# Fetch Tracktion Engine (recommended audio engine)
FetchContent_Declare(
    TracktionEngine
    GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
    GIT_TAG        develop
)
FetchContent_MakeAvailable(TracktionEngine)

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

# Add JUCE
add_subdirectory(third_party/juce)

# Add native JUCE engine sources
set(JUCE_ENGINE_SOURCES
    native/src/AudioEngine.cpp
    native/src/AudioCue.cpp
    native/src/MatrixMixer.cpp
    native/src/OutputPatch.cpp
)

# Add to main executable
target_sources(CueForge PRIVATE ${JUCE_ENGINE_SOURCES})

# Include native headers
target_include_directories(CueForge PRIVATE native/include)

# Link JUCE modules (from your existing engine)
target_link_libraries(CueForge PRIVATE
    # Qt6 components (already defined)
    Qt6::Core Qt6::Widgets Qt6::Gui
    Qt6::Multimedia Qt6::MultimediaWidgets
    Qt6::Network Qt6::SerialPort
    
    # JUCE modules (matching your native engine)
    juce::juce_core
    juce::juce_events
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
)

# Link libraries
target_link_libraries(CueForge PRIVATE
    # Qt6 components
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::SerialPort
    
    # JUCE components for audio
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    
    # Tracktion Engine
    tracktion_engine
)

# Compiler-specific settings
if(MSVC)
    target_compile_options(CueForge PRIVATE /W4)
    # Set Windows subsystem for release builds
    set_target_properties(CueForge PROPERTIES
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
    )
else()
    target_compile_options(CueForge PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Platform-specific configurations
if(WIN32)
    # Windows-specific settings
    set_target_properties(CueForge PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
elseif(APPLE)
    # macOS-specific settings
    set_target_properties(CueForge PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "CueForge"
        MACOSX_BUNDLE_GUI_IDENTIFIER "app.cueforge.CueForge"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
endif()

# Install configuration
install(TARGETS CueForge
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Package configuration (for CPack)
set(CPACK_PACKAGE_NAME "CueForge")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "Professional cue-based show control application")
set(CPACK_PACKAGE_VENDOR "CueForge")

include(CPack)