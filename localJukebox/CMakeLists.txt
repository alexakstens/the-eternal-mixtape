cmake_minimum_required(VERSION 3.28)
project(localJukebox C) # Changed to C project

set(CMAKE_C_STANDARD 11)

# 1. Look for aubio (requires aubio to be installed on system)
find_package(PkgConfig REQUIRED)
pkg_check_modules(AUBIO REQUIRED aubio)

add_executable(localJukebox
        main.c
        beat_tracker.c
        beat_tracker.h
        dr_wav.h
)

# 2. Include the aubio directories
target_include_directories(localJukebox PRIVATE ${AUBIO_INCLUDE_DIRS})

# 3. Link aubio and the math library
target_link_libraries(localJukebox PRIVATE ${AUBIO_LIBRARIES} m)