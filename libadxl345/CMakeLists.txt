cmake_minimum_required(VERSION 3.10)
project(libadxl345 VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(adxl345 SHARED src/adxl345.cpp)

if(TARGET spihal)
    # モノレポビルド: spi-hal サブディレクトリが先に処理されている場合はリンク
    target_link_libraries(adxl345 PRIVATE spihal)
else()
    # スタンドアロンビルド（cmake -S libadxl345/）: SPI HAL のソースをそのままコンパイル
    target_sources(adxl345 PRIVATE ../spi-hal/src/spi_driver.cpp)
endif()

target_include_directories(adxl345
    PUBLIC  include
            ../spi-hal/include
)

set_target_properties(adxl345 PROPERTIES
    VERSION   1.1.0
    SOVERSION 1
)

install(TARGETS adxl345 LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/libadxl345)
