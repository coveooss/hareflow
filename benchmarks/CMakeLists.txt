find_package(GTest REQUIRED)

add_executable(hareflow_bench)

target_link_libraries(hareflow_bench
	PRIVATE
		GTest::GTest
		GTest::Main

		hareflow
)

target_sources(hareflow_bench
	PRIVATE
		src/end_to_end.cpp
)