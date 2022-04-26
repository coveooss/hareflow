add_executable(hareflow_tests)
include(GoogleTest)
gtest_discover_tests(hareflow_tests)

find_package(GTest REQUIRED)
target_link_libraries(hareflow_tests
	PRIVATE
		GTest::GTest
		GTest::Main

		hareflow
)

target_sources(hareflow_tests
	PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/producer.cpp
)
