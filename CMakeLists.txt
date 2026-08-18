cmake_minimum_required(VERSION 3.20)
project(brickstack CXX)

# ---- Standard --------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---- Warnings-as-errors (applied to OUR code only, not fetched deps) --------
add_library(brickstack_warnings INTERFACE)
target_compile_options(brickstack_warnings INTERFACE
    -Wall -Wextra -Werror)

# ---- Core library ----------------------------------------------------------
add_library(brickstack_core STATIC
    src/world.cpp
    src/stability.cpp
    src/collapse.cpp
    src/sim.cpp
    src/scenarios.cpp)
target_include_directories(brickstack_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(brickstack_core PRIVATE brickstack_warnings)

# ---- Benchmark driver (no test-framework dependency) -----------------------
add_executable(brickstack_bench bench/bench_main.cpp)
target_link_libraries(brickstack_bench PRIVATE brickstack_core brickstack_warnings)

# ---- Tests (GoogleTest via FetchContent) -----------------------------------
option(BRICKSTACK_BUILD_TESTS "Build the unit tests" ON)
if(BRICKSTACK_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(brickstack_tests
        tests/test_placement.cpp
        tests/test_stability.cpp
        tests/test_collapse.cpp
        tests/test_determinism.cpp)
    # Note: brickstack_warnings is intentionally NOT applied here so GoogleTest
    # headers do not trip -Werror; our own library is already built with it.
    target_link_libraries(brickstack_tests PRIVATE brickstack_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(brickstack_tests)
endif()
