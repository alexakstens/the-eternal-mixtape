cmake_minimum_required(VERSION 3.28)
project(localJukebox C)

set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(AUBIO REQUIRED aubio)
pkg_check_modules(FFTW3 REQUIRED fftw3f)

add_executable(localJukebox
        main.c
        song_analyzer.c
        song_analyzer.h
        warp.c
        warp.h
        dr_wav.h
)

target_include_directories(localJukebox PRIVATE ${AUBIO_INCLUDE_DIRS} ${FFTW3_INCLUDE_DIRS})
target_link_libraries(localJukebox PRIVATE ${AUBIO_LIBRARIES} ${FFTW3_LIBRARIES} m)