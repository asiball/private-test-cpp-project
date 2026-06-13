cmake_minimum_required(VERSION 3.10)
project(gpio VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(gpio STATIC
    src/gpio_line.cpp
)
set_target_properties(gpio PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(gpio
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include/gpio>
    PRIVATE ../common/include   # 共有ログマクロ（logger.hpp）
)

if(COMMAND eds_install_package)
    eds_install_package(PACKAGE gpio TARGET gpio
        VERSION ${PROJECT_VERSION} INCLUDE_DIR gpio)
else()
    install(TARGETS gpio ARCHIVE DESTINATION lib)
endif()
install(DIRECTORY include/ DESTINATION include/gpio)
