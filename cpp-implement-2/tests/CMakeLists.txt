file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(test_source IN LISTS TEST_SOURCES)
    get_filename_component(test_name "${test_source}" NAME_WE)
    file(READ "${test_source}" test_source_text)

    add_executable(${test_name} "${test_source}")
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
    )
    target_include_directories(${test_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/support")
    target_compile_definitions(${test_name} PRIVATE APP_EXE="$<TARGET_FILE:PomodoroLockCpp>")
    target_link_libraries(${test_name} PRIVATE
        Catch2::Catch2WithMain
        wordlock_core
        user32.lib
        shell32.lib
    )
    # Building a test rebuilds the app it launches, avoiding stale binaries.
    add_dependencies(${test_name} PomodoroLockCpp)

    add_test(NAME ${test_name} COMMAND ${test_name})
    if(test_source_text MATCHES "\\[runtime\\]")
        # Any test tagged [runtime] shares global window/focus state, so CTest
        # must never run two of them at the same time.
        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            TIMEOUT 30
            RESOURCE_LOCK desktop
        )
    else()
        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            TIMEOUT 10
        )
    endif()
endforeach()
