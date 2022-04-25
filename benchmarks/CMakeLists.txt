add_executable(hareflow_bench)
include(GoogleTest)
gtest_discover_tests(hareflow_bench)

find_package(GTest REQUIRED)
target_link_libraries(hareflow_bench
	PRIVATE
		GTest::GTest
		GTest::Main

		hareflow
)

target_sources(hareflow_bench
	PRIVATE
		${CMAKE_CURRENT_LIST_DIR}/end_to_end.cpp
)
