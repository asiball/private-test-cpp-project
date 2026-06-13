cmake_minimum_required(VERSION 3.10)
project(libmcp9808 VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mcp9808 SHARED src/mcp9808.cpp)

if(TARGET i2chal)
    # モノレポビルド: i2c-hal サブディレクトリが先に処理済みならリンク
    target_link_libraries(mcp9808 PRIVATE i2chal)
else()
    # スタンドアロンビルド（cmake -S libmcp9808/）: I2C HAL のソースをそのままコンパイル
    target_sources(mcp9808 PRIVATE ../i2c-hal/src/i2c_driver.cpp)
    target_include_directories(mcp9808 PRIVATE ../common/include)
endif()

target_include_directories(mcp9808
    PUBLIC  include
            ../i2c-hal/include
)

set_target_properties(mcp9808 PROPERTIES
    VERSION   1.1.0
    SOVERSION 1
)

install(TARGETS mcp9808 LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/libmcp9808)
