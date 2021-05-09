cmake_minimum_required(VERSION 3.13)
project(ThreadPool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ThreadPool main.cpp)
find_package(Threads REQUIRED)
target_link_libraries(ThreadPool Threads::Threads)