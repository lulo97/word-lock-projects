file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(test_source IN LISTS TEST_SOURCES)
    get_filename_component(test_name "${test_source}" NAME_WE)

    add_executable(${test_name} "${test_source}")
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/$<CONFIG>"
    )
    target_link_libraries(${test_name} PRIVATE
        Catch2::Catch2WithMain
        user32.lib
        shell32.lib
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
    # Hard limit: CTest kills the test and logs "***Timeout" if it exceeds 10s.
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        TIMEOUT 10
    )
endforeach()

# These tests rely on global mouse/keyboard focus state, so they must not run
# alongside other runtime tests.
set_tests_properties(countdown_draggable PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(input_autofocus PROPERTIES RUN_SERIAL TRUE)
