cmake_minimum_required(VERSION 3.10)
project(spi-driver VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(spi_driver STATIC
    src/spi_driver.cpp
)
target_include_directories(spi_driver PUBLIC include)

install(TARGETS spi_driver ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/spi_driver)
