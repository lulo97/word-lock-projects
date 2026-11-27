cmake_minimum_required(VERSION 3.20)

project(Win32HelloWorld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(unofficial-webview2 CONFIG REQUIRED)

add_executable(${PROJECT_NAME} WIN32 src/main.cpp)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
target_link_libraries(${PROJECT_NAME} PRIVATE
    shell32.lib
    unofficial::webview2::webview2
)

# The settings screens are plain HTML/CSS hosted by WebView2.
foreach(asset settings.html settings.js ignoreWords.html ignoreWordsUI.js)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/settings/${asset}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${asset}"
    )
endforeach()

# WebView2Loader.dll must sit next to the executable.
set(WEBVIEW2_LOADER_RELEASE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/WebView2Loader.dll")
set(WEBVIEW2_LOADER_DEBUG "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin/WebView2Loader.dll")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${WEBVIEW2_LOADER_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${WEBVIEW2_LOADER_RELEASE}>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/WebView2Loader.dll"
)
