cmake_minimum_required(VERSION 3.22)
project(OpenMPTMac VERSION 0.1 LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    src/App.cpp
    src/Toolbar.cpp
    src/AudioEngine.cpp
    src/TrackerView.cpp
    src/PatternEditor.cpp
    src/CocoaHelpers.mm
    src/Logo.mm
    assets/logo.png
    assets/AppIcon.icns
)

add_executable(OpenMPTMac MACOSX_BUNDLE
    ${SOURCES}
)

# Mark the Objective-C++ sources explicitly
set_source_files_properties(src/CocoaHelpers.mm src/Logo.mm PROPERTIES
    LANGUAGE OBJCXX
)

# Bundle the logo into Resources/
set_source_files_properties(assets/logo.png assets/AppIcon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources
)

target_include_directories(OpenMPTMac PRIVATE
    src
    ${GLFW_INCLUDE_DIR}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${OPENMPT_ROOT}
    ${MPG123_INCLUDE_DIRS}
)

target_link_libraries(OpenMPTMac PRIVATE
    OpenGL::GL
    ${GLFW_LIBRARY}
    imgui
    libopenmpt
    ${PORTAUDIO_LINK_LIBRARIES}
    ${VORBISFILE_LINK_LIBRARIES}
    ${VORBIS_LINK_LIBRARIES}
    ${OGG_LINK_LIBRARIES}
    ${MPG123_LIBRARIES}
    "-lz"
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreAudio"
    "-framework AudioUnit"
    "-framework CoreFoundation"
    "-framework CoreMIDI"
)

target_compile_options(OpenMPTMac PRIVATE ${PORTAUDIO_CFLAGS_OTHER})

set_target_properties(OpenMPTMac PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/assets/Info.plist"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.openmpt.mac"
    MACOSX_BUNDLE_BUNDLE_NAME "OpenMPT"
    MACOSX_BUNDLE_ICON_FILE "AppIcon"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
)
