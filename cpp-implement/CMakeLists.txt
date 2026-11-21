cmake_minimum_required(VERSION 3.16)
project(MyApp LANGUAGES CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC options equivalent to /EHsc /MP and Release /O2
if (MSVC)
    add_compile_options(/EHsc /MP)
    add_compile_options($<$<CONFIG:Release>:/O2>)
endif()

# Main source, default app.cpp
set(APP_MAIN_SOURCE "app.cpp" CACHE FILEPATH "Main source file")
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${APP_MAIN_SOURCE}")
    message(FATAL_ERROR "Input file not found: ${APP_MAIN_SOURCE}")
endif()

set(SOURCES "${APP_MAIN_SOURCE}")

# Collect framework sources.
# Option A: explicitly list files (recommended for stable projects).
# list(APPEND SOURCES
#     ui/foo.cpp
#     utils/bar.cpp
#     ui/window.cpp
# )

# Option B: glob recursively with CONFIGURE_DEPENDS.
file(GLOB_RECURSE EXTRA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp"
)
list(APPEND SOURCES ${EXTRA_SOURCES})

# Resource file
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
    list(APPEND SOURCES "app.rc")
endif()

include(cmake/EmbedAssets.cmake)

add_executable(MyApp ${SOURCES})

# Embed HTML assets into the executable.
target_embed_assets(MyApp
    NAMESPACE assets
    "${CMAKE_CURRENT_SOURCE_DIR}/htmls/countdown.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/htmls/info.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/htmls/input.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/htmls/settings.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/htmls/tray_menu.html"
)

# Equivalent to /I"ui"
target_include_directories(MyApp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

# Equivalent to user32.lib gdi32.lib shell32.lib winmm.lib gdiplus.lib
target_link_libraries(MyApp PRIVATE
    user32
    gdi32
    shell32
    winmm
    gdiplus
)

# Equivalent to /SUBSYSTEM:WINDOWS
set_target_properties(MyApp PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# Output executable.
# For Visual Studio multi-config generators, set per-config output directories
# if you want build/app.exe instead of build/Release/app.exe.
set_target_properties(MyApp PROPERTIES
    OUTPUT_NAME "app"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)