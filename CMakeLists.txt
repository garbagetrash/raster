cmake_minimum_required(VERSION 3.11)
project(plots)

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
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(raylib)

find_package(Threads REQUIRED)

# Resources path
set(RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)

set(COMMON_SRC
    src/common.c
    src/filetypes.c
)

# Build our examples
add_executable(plot src/plot.c ${COMMON_SRC})
target_include_directories(plot PRIVATE include)
target_link_libraries(plot PRIVATE raylib Threads::Threads)
target_compile_definitions(plot PRIVATE RESOURCES_DIR="${RESOURCES_DIR}")
target_compile_options(plot PRIVATE $<$<CONFIG:Debug>:-fno-omit-frame-pointer -fsanitize=address>)
target_link_options(plot PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)

add_executable(waterfall src/waterfall.c ${COMMON_SRC})
target_include_directories(waterfall PRIVATE include)
target_link_libraries(waterfall PRIVATE raylib Threads::Threads)
target_compile_definitions(waterfall PRIVATE RESOURCES_DIR="${RESOURCES_DIR}")
target_compile_options(waterfall PRIVATE $<$<CONFIG:Debug>:-fno-omit-frame-pointer -fsanitize=address>)
target_link_options(waterfall PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)

add_executable(raster1d src/raster1d.c ${COMMON_SRC})
target_include_directories(raster1d PRIVATE include)
target_link_libraries(raster1d PRIVATE raylib Threads::Threads)
target_compile_definitions(raster1d PRIVATE RESOURCES_DIR="${RESOURCES_DIR}")
target_compile_options(raster1d PRIVATE $<$<CONFIG:Debug>:-fno-omit-frame-pointer -fsanitize=address>)
target_link_options(raster1d PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)

