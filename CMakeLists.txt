cmake_minimum_required(VERSION 3.10)
project(embedded-device-suite VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(lib)
add_subdirectory(cli)
# driver は別途 kernel Makefile でビルドする
