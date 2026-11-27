cmake_minimum_required(VERSION 3.20)

project(PomodoroLockCpp LANGUAGES CXX)

if(MSVC)
    add_compile_options(/MP)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(unofficial-webview2 CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc")

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
target_link_libraries(${PROJECT_NAME} PRIVATE
    shell32.lib
    winhttp.lib
    unofficial::sqlite3::sqlite3
    unofficial::webview2::webview2
)

# WebView2 screens are plain HTML/CSS copied next to the executable.
function(copy_webview_asset source_file asset_name)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${source_file}"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/${asset_name}"
    )
endfunction()

foreach(asset settings.html settings.js ignoreWords.html ignoreWordsUI.js)
    copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/src/settings/${asset}" "${asset}")
endforeach()

foreach(asset ui.css themes.css info-render.js)
    copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/src/ui/${asset}" "${asset}")
endforeach()

copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/src/tts/speakUI.js" "speakUI.js")
copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/favicon.ico" "favicon.ico")

foreach(asset english.html englishInfo.html english.js englishInfo.js)
    copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/src/english/${asset}" "${asset}")
endforeach()

foreach(asset japanese.html japaneseInfo.html japanese.js japaneseInfo.js)
    copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/src/japanese/${asset}" "${asset}")
endforeach()

foreach(asset mcq.html mcqResult.html mcq.js mcqResult.js)
    copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/src/mcq/${asset}" "${asset}")
endforeach()

foreach(asset dumbbell.html rep-counter.js overlay.js pose-tracker.js dumbbell.js)
    copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/src/dumbbell/${asset}" "${asset}")
endforeach()

copy_webview_asset("${CMAKE_CURRENT_SOURCE_DIR}/pose_landmarker_heavy.task"
                   "pose_landmarker_heavy.task")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mediapipe/wasm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/mediapipe/vision_bundle.mjs"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mediapipe/vision_bundle.mjs"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/vendor/mediapipe/wasm"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mediapipe/wasm"
)

# WebView2Loader.dll must sit next to the executable.
set(WEBVIEW2_LOADER_RELEASE "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/WebView2Loader.dll")
set(WEBVIEW2_LOADER_DEBUG "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin/WebView2Loader.dll")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<$<CONFIG:Debug>:${WEBVIEW2_LOADER_DEBUG}>$<$<NOT:$<CONFIG:Debug>>:${WEBVIEW2_LOADER_RELEASE}>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/WebView2Loader.dll"
)

include(CTest)
if(BUILD_TESTING)
    find_package(Catch2 CONFIG REQUIRED)
    add_subdirectory(tests)
endif()
