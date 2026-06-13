cmake_minimum_required(VERSION 3.10)
project(libmcp9808 VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mcp9808 SHARED src/mcp9808.cpp)

target_include_directories(mcp9808
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include/libmcp9808>
)

set(_mcp9808_deps "")
if(TARGET i2chal)
    # モノレポビルド: i2chal を PUBLIC リンク（mcp9808.hpp が ii2c_driver.hpp を include）
    target_link_libraries(mcp9808 PUBLIC i2chal)
    set(_mcp9808_deps i2chal)
else()
    # スタンドアロンビルド（cmake -S libmcp9808/）: I2C HAL のソースをそのままコンパイル
    target_sources(mcp9808 PRIVATE ../i2c-hal/src/i2c_driver.cpp)
    target_include_directories(mcp9808 PRIVATE ../common/include)
    target_include_directories(mcp9808
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../i2c-hal/include>)
endif()

set_target_properties(mcp9808 PROPERTIES
    VERSION   1.1.0
    SOVERSION 1
)

if(COMMAND eds_install_package)
    eds_install_package(PACKAGE mcp9808 TARGET mcp9808
        VERSION ${PROJECT_VERSION} INCLUDE_DIR libmcp9808
        DEPENDENCIES ${_mcp9808_deps})
else()
    install(TARGETS mcp9808 LIBRARY DESTINATION lib)
endif()
install(DIRECTORY include/ DESTINATION include/libmcp9808)
