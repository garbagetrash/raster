cmake_minimum_required(VERSION 3.11)
project(raster)

set(CMAKE_C_VERSION 99)

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# Get Raylib 5.5
set(RAYLIB_VERSION 5.5)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
set(BUILD_GAMES OFF CACHE BOOL "" FORCE) # don't build the supplied games
FetchContent_Declare(
    raylib
    GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
    GIT_TAG "5.5"
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(raylib)

# Resources path
set(RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)

# Build our examples
add_executable(raster src/raster.c)
target_link_libraries(raster PRIVATE raylib)
target_compile_definitions(raster PRIVATE RESOURCES_DIR="${RESOURCES_DIR}")
