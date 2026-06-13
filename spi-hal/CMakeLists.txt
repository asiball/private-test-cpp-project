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
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include/spihal>
    PRIVATE ../kernel/include   # my_spi_dev.h（カーネルと共有する ioctl 定義）
)

# find_package 対応（モノレポビルド時）。単体ビルドでは素の install にフォールバック。
if(COMMAND eds_install_package)
    eds_install_package(PACKAGE spihal TARGET spihal
        VERSION ${PROJECT_VERSION} INCLUDE_DIR spihal)
else()
    install(TARGETS spihal ARCHIVE DESTINATION lib)
endif()
# version.hpp.in はテンプレートのため install しない。生成物 version.hpp は
# ビルドツリー側に出力されるため（トップ CMakeLists 参照）ここには現れない。
install(DIRECTORY include/ DESTINATION include/spihal
        PATTERN "*.in" EXCLUDE)
