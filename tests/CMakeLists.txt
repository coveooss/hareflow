add_executable(hareflow_tests)
include(GoogleTest)
gtest_discover_tests(hareflow_tests)

target_link_libraries(hareflow_tests
    PRIVATE
        gmock_main # when you use gmock, _only_ link to gmock_main! https://github.com/google/googletest/issues/2157#issuecomment-674361850

        hareflow
)

target_compile_definitions(hareflow_tests PRIVATE $<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x603>)

target_sources(hareflow_tests
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/consumer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/producer.cpp
)
