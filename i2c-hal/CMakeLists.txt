cmake_minimum_required(VERSION 3.10)
project(i2c-hal VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(i2chal STATIC
    src/i2c_driver.cpp
)
set_target_properties(i2chal PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(i2chal
    PUBLIC  include
    PRIVATE ../common/include   # 共有ログマクロ（logger.hpp）
)

install(TARGETS i2chal ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/i2chal)
