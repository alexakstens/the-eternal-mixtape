cmake_minimum_required(VERSION 3.28)
project(localJukebox CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AUBIO REQUIRED aubio)
pkg_check_modules(FFTW3 REQUIRED fftw3f)

add_executable(localJukebox
        main.cpp
        song_analyzer.cpp
        song_analyzer.hpp
        warp.cpp
        warp.hpp
        dr_wav.h
)

target_include_directories(localJukebox PRIVATE ${AUBIO_INCLUDE_DIRS} ${FFTW3_INCLUDE_DIRS})
target_link_libraries(localJukebox PRIVATE ${AUBIO_LIBRARIES} ${FFTW3_LIBRARIES} m)