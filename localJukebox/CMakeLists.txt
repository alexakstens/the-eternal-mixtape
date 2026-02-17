cmake_minimum_required(VERSION 3.28)
project(localJukebox)

set(CMAKE_CXX_STANDARD 20)

add_executable(localJukebox main.cpp
        beat_tracker.h
        beat_tracker.c
        main.c
        dr_wav.h)
