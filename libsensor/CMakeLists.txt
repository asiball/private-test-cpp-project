cmake_minimum_required(VERSION 3.10)
project(libsensor VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(sensor SHARED src/device.cpp)

if(TARGET spihal)
    # モノレポビルド: spi-hal サブディレクトリが先に処理されている場合はリンク
    target_link_libraries(sensor PRIVATE spihal pthread)
else()
    # スタンドアロンビルド（cmake -S libsensor/）: SPI HAL のソースをそのままコンパイル
    target_sources(sensor PRIVATE ../spi-hal/src/spi_driver.cpp)
    target_link_libraries(sensor pthread)
endif()

target_include_directories(sensor
    PUBLIC  include
            ../spi-hal/include
)

set_target_properties(sensor PROPERTIES
    VERSION   1.1.0
    SOVERSION 1
)

install(TARGETS sensor LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include/libsensor)
