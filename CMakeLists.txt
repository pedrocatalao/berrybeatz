cmake_minimum_required(VERSION 3.22)
project(BerryBeatz VERSION 0.1 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# OpenGL
find_package(OpenGL REQUIRED)

# GLFW (Homebrew arm64 prefix)
set(GLFW_ROOT "/opt/homebrew/opt/glfw")
find_library(GLFW_LIBRARY glfw HINTS "${GLFW_ROOT}/lib" REQUIRED)
set(GLFW_INCLUDE_DIR "${GLFW_ROOT}/include")

# PortAudio + libopenmpt runtime deps
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
pkg_check_modules(VORBIS REQUIRED vorbis)
pkg_check_modules(OGG REQUIRED ogg)

pkg_check_modules(MPG123 REQUIRED libmpg123)

# libopenmpt (built from sibling repo)
set(OPENMPT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/vendor/openmpt")
set(OPENMPT_LIB "${OPENMPT_ROOT}/bin/libopenmpt.a")

if(NOT EXISTS "${OPENMPT_LIB}")
    message(FATAL_ERROR
        "libopenmpt.a not found at ${OPENMPT_LIB}\n"
        "Build it first:\n"
        "  cd ${OPENMPT_ROOT} && make CONFIG=clang SHARED_LIB=0 NO_SHARED_LINKER_FLAG=1 bin/libopenmpt.a"
    )
endif()

add_library(libopenmpt STATIC IMPORTED)
set_target_properties(libopenmpt PROPERTIES
    IMPORTED_LOCATION "${OPENMPT_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${OPENMPT_ROOT}"
)

# Dear ImGui (with GLFW backend)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GLFW_INCLUDE_DIR}
)

# Main app sources
set(SOURCES
    src/main.cpp
    # core — platform-independent logic
    src/core/AudioEngine.cpp
    src/platform/AudioDeviceMonitor.cpp
    src/core/PatternEditor.cpp
    src/core/Settings.cpp
    src/core/UndoManager.cpp
    src/core/OpenMPTInternal.cpp
    src/core/OpenMPTInternalGet.cpp
    # ui — ImGui views and widgets
    src/ui/App.cpp
    src/ui/SidebarView.cpp
    src/ui/GeneralView.cpp
    src/ui/InstrumentView.cpp
    src/ui/SampleView.cpp
    src/ui/SettingsView.cpp
    src/ui/Toolbar.cpp
    src/ui/LogoView.cpp
    src/ui/TrackerView.cpp
    src/ui/PatternInput.cpp
    # platform — macOS-specific
    src/platform/MidiDevices.mm
    src/platform/MidiEngine.mm
    src/platform/WindowChrome.cpp
    src/platform/CocoaHelpers.mm
    src/platform/Logo.mm
    # assets bundled into Resources/
    assets/img/banner.png
    assets/img/speaker.png
    assets/AppIcon.icns
    assets/ttf/Michroma.ttf
    assets/ttf/35px2.ttf
    assets/ttf/terminal-6-mono.ttf
)

add_executable(BerryBeatz MACOSX_BUNDLE
    ${SOURCES}
)

# Mark the Objective-C++ sources explicitly
set_source_files_properties(
    src/platform/MidiDevices.mm
    src/platform/MidiEngine.mm
    src/platform/CocoaHelpers.mm
    src/platform/Logo.mm
    PROPERTIES LANGUAGE OBJCXX
)

# Bundle the logo into Resources/
set_source_files_properties(
    assets/img/banner.png
    assets/img/speaker.png
    assets/AppIcon.icns
    assets/ttf/Michroma.ttf
    assets/ttf/35px2.ttf
    assets/ttf/terminal-6-mono.ttf
    PROPERTIES MACOSX_PACKAGE_LOCATION Resources
)

target_include_directories(BerryBeatz PRIVATE
    src
    src/core
    src/ui
    src/platform
    ${GLFW_INCLUDE_DIR}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${OPENMPT_ROOT}
    ${OPENMPT_ROOT}/src
    ${MPG123_INCLUDE_DIRS}
)

# OpenMPTInternal.cpp: soundlib side — needs full common/ include chain.
# LIBOPENMPT_BUILD_TEST is defined inside the .cpp to remove MODPLUG_NO_FILESAVE.
set_source_files_properties(src/core/OpenMPTInternal.cpp PROPERTIES
    COMPILE_DEFINITIONS "LIBOPENMPT_BUILD"
    INCLUDE_DIRECTORIES "${OPENMPT_ROOT}/common;${OPENMPT_ROOT}/src;${OPENMPT_ROOT}"
)

# OpenMPTInternalGet.cpp: libopenmpt side — extracts CSoundFile* via internal cast.
# Must NOT include soundlib headers (namespace conflict with libopenmpt fwd-decls).
set_source_files_properties(src/core/OpenMPTInternalGet.cpp PROPERTIES
    COMPILE_DEFINITIONS "LIBOPENMPT_BUILD"
    INCLUDE_DIRECTORIES "${OPENMPT_ROOT}/common;${OPENMPT_ROOT}/src;${OPENMPT_ROOT}"
)

target_link_libraries(BerryBeatz PRIVATE
    OpenGL::GL
    ${GLFW_LIBRARY}
    imgui
    libopenmpt
    ${PORTAUDIO_LINK_LIBRARIES}
    ${VORBISFILE_LINK_LIBRARIES}
    ${VORBIS_LINK_LIBRARIES}
    ${OGG_LINK_LIBRARIES}
    ${MPG123_LINK_LIBRARIES}
    "-lz"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreAudio"
    "-framework AudioUnit"
    "-framework CoreFoundation"
    "-framework CoreMIDI"
)

target_compile_options(BerryBeatz PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

set_target_properties(BerryBeatz PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/assets/Info.plist"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.berrybeatz.app"
    MACOSX_BUNDLE_BUNDLE_NAME "BerryBeatz"
    MACOSX_BUNDLE_ICON_FILE "AppIcon"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
)
