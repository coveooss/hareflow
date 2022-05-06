# Based on https://github.com/google/googletest/blob/main/docs/quickstart-cmake.md
set(CMAKE_CXX_STANDARD 17) # Hareflow requires string_view in its tests

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)
