cmake_minimum_required(VERSION 3.21)
project(Log_file)

set(CMAKE_CXX_STANDARD 11)

add_executable(Log_file main.cpp src/log_format.h src/log_write.h src/slice.h src/status.h src/log_write.cpp src/log_reader.h src/log_reader.cpp)
