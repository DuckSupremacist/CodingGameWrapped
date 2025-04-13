include(FetchContent)

# Enable testing
FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/release-1.11.0.zip)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# ######################################################################################################################

set(CMAKE_CXX_STANDARD 23)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${gtest_SOURCE_DIR}/include
    ${gtest_SOURCE_DIR}
)

set(LIBRARIES stdc++fs)

file(
    GLOB_RECURSE
    TESTED_SOURCES
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

file(
    GLOB_RECURSE
    TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.test.cpp"
)

# A bit hacky, to build the
set(TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/coding_game.test.cpp")

# Compile common sources to object files
add_library(
    common_objects
    OBJECT
    ${TESTED_SOURCES}
)
target_link_libraries(
    common_objects
    ${LIBRARIES}
)
link_libraries(
    gtest
    gtest_main
    ${LIBRARIES}
)

add_executable(
    test-all
    $<TARGET_OBJECTS:common_objects>
    ${TEST_SOURCES}
)
add_compile_definitions(
    PRIVATE
    BUILD_TESTS=1
)

# Add the tests to be run
gtest_discover_tests(test-all)

# ######################################################################################################################
add_executable(test-matrix-transformations ${CMAKE_CURRENT_SOURCE_DIR}/src/matrix_transformations.test.cpp)
add_compile_definitions(
    PRIVATE
    BUILD_TESTS=1
)
gtest_discover_tests(test-matrix-transformations)
