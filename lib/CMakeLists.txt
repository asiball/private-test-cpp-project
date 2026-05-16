cmake_minimum_required(VERSION 3.10)
project(libdevice VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(device SHARED
    src/device.cpp
    ../driver/src/spi_driver.cpp
)
target_include_directories(device
    PUBLIC  include
            ../driver/include   # ISpiDriver を PUBLIC にして利用者も参照できる
)
target_link_libraries(device pthread)

set_target_properties(device PROPERTIES
    VERSION   1.1.0
    SOVERSION 1
)

install(TARGETS device LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/device)
