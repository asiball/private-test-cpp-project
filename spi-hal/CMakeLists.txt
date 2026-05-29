cmake_minimum_required(VERSION 3.10)
project(spi-hal VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(spihal STATIC
    src/spi_driver.cpp
    src/kernel_spi_driver.cpp
)
set_target_properties(spihal PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(spihal
    PUBLIC  include
    PRIVATE ../kernel/include   # my_spi_dev.h（カーネルと共有する ioctl 定義）
)

install(TARGETS spihal ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/spihal)
