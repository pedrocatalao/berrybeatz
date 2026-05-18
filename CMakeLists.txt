cmake_minimum_required(VERSION 3.22)
project(OpenMPTMac VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)

# OpenGL
find_package(OpenGL REQUIRED)

# PortAudio + libopenmpt runtime deps
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
pkg_check_modules(VORBIS REQUIRED vorbis)
pkg_check_modules(OGG REQUIRED ogg)

# mpg123 may not have a .pc file — fall back to Homebrew path
pkg_check_modules(MPG123 mpg123)
if(NOT MPG123_FOUND)
    set(MPG123_LIBRARIES "-L${HOMEBREW_PREFIX}/opt/mpg123/lib -lmpg123")
    set(MPG123_INCLUDE_DIRS "${HOMEBREW_PREFIX}/opt/mpg123/include")
endif()

# libopenmpt (built from sibling repo)
set(OPENMPT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../openmpt")
set(OPENMPT_LIB "${OPENMPT_ROOT}/bin/libopenmpt.a")

if(NOT EXISTS "${OPENMPT_LIB}")
    message(FATAL_ERROR
        "libopenmpt.a not found at ${OPENMPT_LIB}\n"
        "Build it first:\n"
        "  cd ${OPENMPT_ROOT} && make CONFIG=clang DYNLINK=0 NO_OPENMPT123=1 NO_EXAMPLES=1 NO_TESTS=1"
    )
endif()

add_library(libopenmpt STATIC IMPORTED)
set_target_properties(libopenmpt PROPERTIES
    IMPORTED_LOCATION "${OPENMPT_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${OPENMPT_ROOT}"
)

# Dear ImGui
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# Main app
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/AudioEngine.cpp
    src/TrackerView.cpp
    src/PatternEditor.cpp
)

set(HEADERS
    src/MainWindow.h
    src/AudioEngine.h
    src/ModuleState.h
    src/TrackerView.h
)

add_executable(OpenMPTMac MACOSX_BUNDLE
    ${SOURCES}
    src/MainWindow.h
    src/AudioEngine.h
    src/TrackerView.h
)

target_include_directories(OpenMPTMac PRIVATE
    src
    ${PORTAUDIO_INCLUDE_DIRS}
    ${OPENMPT_ROOT}
)

target_link_libraries(OpenMPTMac PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    OpenGL::GL
    imgui
    libopenmpt
    ${PORTAUDIO_LINK_LIBRARIES}
    ${VORBISFILE_LINK_LIBRARIES}
    ${VORBIS_LINK_LIBRARIES}
    ${OGG_LINK_LIBRARIES}
    ${MPG123_LIBRARIES}
    "-lz"
    "-framework CoreAudio"
    "-framework AudioUnit"
    "-framework CoreFoundation"
    "-framework CoreMIDI"
)

target_compile_options(OpenMPTMac PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

set_target_properties(OpenMPTMac PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.openmpt.mac"
    MACOSX_BUNDLE_BUNDLE_NAME "OpenMPT"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
)
