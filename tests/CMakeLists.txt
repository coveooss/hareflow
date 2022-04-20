find_package(GTest REQUIRED)

add_executable(hareflow_tests)

target_link_libraries(hareflow_tests
	PRIVATE
		GTest::GTest
		GTest::Main

		hareflow
)

target_sources(hareflow_tests
	PRIVATE
		src/producer.cpp
)