add_executable(hareflow_integration)
include(GoogleTest)
gtest_discover_tests(hareflow_integration)

find_package(GTest REQUIRED)
target_link_libraries(hareflow_integration
	PRIVATE
		GTest::GTest
		GTest::Main

		hareflow
)

target_sources(hareflow_integration
	PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/bench.cpp
		${CMAKE_CURRENT_LIST_DIR}/producer.cpp
)
