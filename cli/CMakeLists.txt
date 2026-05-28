cmake_minimum_required(VERSION 3.10)
project(device-ctl VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(device-ctl src/main.cpp)
target_link_libraries(device-ctl sensor)

install(TARGETS device-ctl RUNTIME DESTINATION bin)
